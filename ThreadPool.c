
#include "DataStr.h"
#include "ThreadPool.h"
#include "FileMemory.h"
#include "config.h"


thread_func ReqFunArr[numberOfFunctions] = {
    fileRead,
    fileNRead,
    fileWrite,
    fileAppend,
    fileOpen,
    fileClose,
    fileDelete,
    fileLock,
    fileUnlock,
    fileSearch,
    fileInit,
};

int threadPoolInit(threadPool* pool,int* pipe){
    pool->queue = NULL;
    pool->queue_tail = NULL;
    if (pthread_mutex_init(&(pool->mutex),NULL) != 0){
        fprintf(stderr,"Error initializing pool mutex!\n");
        return -1;
    }
    if (pthread_cond_init(&(pool->queueHasWork),NULL)){
        fprintf(stderr,"Error initializing pool mutex condition!\n");
        return -1;
    }
    printf("threadPool init successful\n");
    return 0;
}
//function used to add a function (with a single void arg) to the threadpool task queue
//by the server's main thread
int threadPoolAdd(threadPool* pool, thread_func func, void* args){
    printf("threadPool add called\n");
    // locking the threadpool to prevent conflict with thread workers
    pthread_mutex_lock(&(pool->mutex));
    queueAdd(&(pool->queue), &(pool->queue_tail), func,args);
    // workers must be signaled that the queue now has a task
    pthread_mutex_unlock(&(pool->mutex));
    pthread_cond_signal(&(pool->queueHasWork));
    return 0;
}

// function is only called after the worker threads have exited, so no mutex is used
int threadPoolDestroy(threadPool* pool ){
    //clears the queue in case tasks were left
    queue* clnup_ptr = pool->queue;
    //todo: clean dyn allocated args
    while (pool->queue != NULL){
        pool->queue = pool->queue->next;    
        free(clnup_ptr);
        clnup_ptr = pool->queue;
    }
    pthread_mutex_destroy(&(pool->mutex));
    pthread_cond_destroy(&(pool->queueHasWork));
    return 0;
}

//function used by worker threads to receive a task
int PoolTakeTask(pool_request* input_req, threadPool* pool){
    //how much time a worker waits before checking the pool contents
    //(if queueHasWork was signaled it checks it immediately)
    struct timespec wait_max;
    clock_gettime(CLOCK_REALTIME, &wait_max);
    wait_max.tv_sec += 5;
    //only one thread can use the pool at once
    pthread_mutex_lock(&(pool->mutex));
    printf("queue head attempt take\n");
    //if there is no task, wait until wait_max has passed
    pthread_cond_timedwait(&(pool->queueHasWork),&(pool->mutex),&wait_max);
    //take a task from the task queue, 
    //receive func NULL if it was empty (spurious wakeup)
    queueTakeHead(input_req, &(pool->queue), &(pool->queue_tail));
    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}

int makeWorkerThreads(pthread_t** workers,const int n, threadPool* pool){
    //allocate memory to hold n pthread_t in *workers
    if ((*workers = malloc(sizeof(pthread_t) * n)) == NULL){
        printf("Thread malloc error!\n");
        return -1;
    }
    int i;
    //create n worker threads
    for (i=0; i<n; i++){
        if (pthread_create(( &(*workers)[i]), NULL, workerStartup, pool) != 0){
            printf("Error occurred while initializing thread %d\n",i);
            return -1;
        }
    }
    return 0;
}

void* workerStartup(void* pool){
    threadPool* th_pool = (threadPool*) pool;
    pool_request request;
    request.args = NULL;
    request.func = NULL;
    while(1){
        PoolTakeTask(&request, th_pool);
        if (request.func != NULL){
            (request.func)(request.args);
        }
        if (request.args != NULL){
            free(request.args);
        }
    }
    return 0;
}

ReqReadStruct* makeWorkArgs(int fd, int pipe, void* mem, FdStruct* fd_struct){
    ReqReadStruct* new_struct = malloc(sizeof(ReqReadStruct));
    new_struct->fd = fd;
    new_struct->pipe= pipe;
    new_struct->mem = mem ;
    new_struct->set = fd_struct->set;
    return new_struct;
}

int workersDestroy(pthread_t* wrkArr , int size){
    int i;
    for(i=0; i<size; i++){
        pthread_join(wrkArr[i], NULL);
    }
    free(wrkArr);
    return 0;
}
void* fileRead(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req = fileOpenCheck(args);
    if (file_req != NULL){
        pthread_mutex_lock(&file_req->mutex);
        size_t file_size = file_req->size;
        writeNB(fd, &file_size,sizeof(size_t));
        int i;
        //write every page (except last one) to the client fd
        for(i=0; i<file_req->pages_n-1; i++){
            writeNB(fd, FileMemory.memPtr[file_req->pages[i]],page_size);
        }
        //write last page (its contents may be less than a full page, so only write up to end of file)
        int last_page_size = page_size;
        if (file_size % page_size > 0)
            last_page_size = file_size % page_size;
        writeNB(fd, FileMemory.memPtr[file_req->pages[i]], last_page_size);
        pthread_mutex_unlock(&file_req->mutex);
    }
    return NULL;
}

void* fileNRead(void* args){
    printf("trying to call func from funcArr!\n");
    return NULL;
}
void* fileWrite(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req = fileOpenCheck(args);
    if (file_req == NULL){
        printf("FUCK!\n\n");
        return NULL;
    }
    int response = 0;
    pthread_mutex_lock(&(file_req->mutex));
    if(file_req->pages_n != 0){
        response = 1;
    }
    writeNB(fd,&response,sizeof(response));
    if (response){
        pthread_mutex_unlock(&file_req->mutex);
        return NULL;
    }
    size_t file_size;
    readNB(fd,&file_size,sizeof(size_t ));
    if (file_size > server_memory_size){
        response = 1;
        printf("file write was too big, refused write request\n");
    }
    writeNB(fd,&response,sizeof(response));
    if (response){
        pthread_mutex_unlock(&file_req->mutex);
        return NULL;
    }
    file_req->size = file_size;
    printf(" file size is %zu\n",file_size);
    filePagesInitialize(file_req);
    void* MM_file = malloc(file_size);
    char* MM_file_ptr = MM_file;
    readNB(fd,MM_file,file_size);
    int to_write;
    MM_file_ptr = MM_file;
    //File is written into memory one page at a time
    for(to_write=0; to_write<file_req->pages_n-1; to_write++){
        addPageToMem(MM_file_ptr, file_req, to_write, page_size);
        MM_file_ptr += page_size;
    }
    //the last page will be smaller than page_size, so we calculate how big it is
    addPageToMem(MM_file_ptr, file_req, to_write, file_size % page_size); 
    pthread_mutex_unlock(&file_req->mutex);
    free(MM_file);
    writeNB(fd,&response,sizeof(response));
    return NULL;
}

void* fileAppend(void* args){
    printf("trying to call func from funcArr!\n");
    return NULL;
}
void* fileOpen(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req = fileSearch(args);
    if (file_req == NULL){
        return NULL;
    }
    else{
        pthread_mutex_lock(&file_req->mutex);
        int* client_fd = NULL;
        client_fd = malloc(sizeof(int));
        (*client_fd) = fd;
        DL_ListAdd(&(file_req->clients_opened), client_fd);
        pthread_mutex_unlock(&file_req->mutex);
    }
    return NULL;
}
//sends two integers to the client: 1/0 if file is/isn't present
//and 1/0 if file is/isn't opened by client
void* fileOpenCheck(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req = fileSearch(args);
    void* ret_val = NULL;
    int isOpen = 0;
    if (file_req != NULL){
        pthread_mutex_lock(&file_req->mutex);
        if (clientOpenSearch(file_req->clients_opened,fd) != 0){
            ret_val = file_req;
            isOpen = 1;
        }
        pthread_mutex_unlock(&file_req->mutex);
    }
    writeNB(fd, &isOpen,sizeof(isOpen));
    return ret_val;
}
void* fileClose(void* args){
    printf("trying to call func from funcArr!\n");
    return NULL;
}
void* fileDelete(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int path_size;
    int fd = req->fd;
    char* file_path;
    readNB(fd,&path_size,sizeof(int));
    if ((file_path = malloc(path_size)) == NULL){
        printf(" file delete file_path malloc error!\n");
        return NULL;
    }
    readNB(fd,file_path,path_size);
    MemFile* file_to_del = hashGetFile(FileHashTb, file_path, SRC);
    if (file_to_del == NULL){
        printf("Error,file not found\n");
        free(file_path);
        return NULL;
    }
    pthread_mutex_lock(&file_to_del->mutex);
    printf("Trying to delete file %s\n",file_to_del->abspath);
    hashGetFile(FileHashTb, file_path,DEL);
    pthread_mutex_unlock(&file_to_del->mutex);
    fileFree(file_to_del);
    MemFile* test = hashGetFile(FileHashTb, file_path,SRC);
    if (test == NULL)
        printf("file was deleted successfully\n");
    free(file_path);
    return NULL;
}
void* fileLock(void* args){
    printf("trying to call func from funcArr!\n");
    return NULL;
}
void* fileUnlock(void* args){
    printf("trying to call func from funcArr!\n");
    return NULL;
}

void* fileSearch(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int path_size;
    int fd = req->fd;
    char* file_path;
    readNB(fd,&path_size,sizeof(int));
    if ((file_path = malloc(path_size)) == NULL){
        printf(" file write file_path malloc error!\n");
        return NULL;
    }
    readNB(fd,file_path,path_size);
    MemFile* file_req = NULL;
    int found = 0;
    file_req = hashGetFile(FileHashTb,file_path, SRC);
    if (file_req != NULL){
        found = 1;
    }
    writeNB(fd,&found,sizeof(found));
    free(file_path);
    return file_req;
}

// sets values of FilePtr based on its size and path name,
// if file already exists it returns 1, otherwise it is added
// to file_list, a record is entered into the file hash table,
// and it returns 1
void* fileInit(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    int path_size;
    char* file_path;
    readNB(fd,&path_size,sizeof(int));
    if ((file_path = malloc(path_size)) == NULL){
        printf(" file write file_path malloc error!\n");
        return NULL;
    }
    readNB(fd,file_path,path_size);
    MemFile* new_file = malloc(sizeof(MemFile));
    pthread_mutex_lock(&hashTB_mutex);
    pthread_mutex_init(&(new_file->mutex), NULL);
    pthread_mutex_lock(&new_file->mutex);
    new_file->abspath = file_path;
    new_file->pages = NULL;
    unsigned long key = hashKey(new_file->abspath);
    hashTbAdd(FileHashTb, new_file,key);
    fileStackAdd(FStack, key);
    pthread_mutex_unlock(&hashTB_mutex);
    new_file->clients_opened = NULL;
    new_file->pages_n = 0;
    pthread_mutex_unlock(&new_file->mutex);
    int success = 1;
    writeNB(fd,&success,sizeof(int));
   return NULL; 
}
/*void** FuncArrFill(){
        void* Arr[numberOfFunctions];
        Arr[e_fileRead]   = fileRead;
        Arr[e_fileNRead]  = fileNRead;
        Arr[e_fileWrite]  = fileWrite;
        Arr[e_fileDelete] = fileDelete;
        return 0;
}*/
// function is added to the pool queue to signal to threads that they need to exit
void* ThreadRequestExit(void* args){
    printf("Thread has  quit!\n");
    threadPool* pool = (threadPool*) args;
    threadPoolAdd(pool, ThreadRequestExit, pool);
    pthread_exit(NULL);
}

