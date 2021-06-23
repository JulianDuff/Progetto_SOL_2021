
#include "DataStr.h"
#include "ThreadPool.h"
#include "FileMemory.h"


thread_func ReqFunArr[numberOfFunctions] = {
    fileRead,
    fileNRead,
    fileWrite,
    fileAppend,
    fileOpen,
    fileClose,
    *fileDelete,
    fileLock,
    fileUnlock,
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
void fileRead(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int path_size;
    int fd = req->fd;
    size_t file_size;
    char* file_path;
    readNB(fd,&path_size,sizeof(int));
    if ((file_path = malloc(path_size)) == NULL){
        printf(" file write file_path malloc error!\n");
        return;
    }
    readNB(fd,file_path,path_size);
    MemFile* file_req = NULL;
    file_req = hashGetFile(FileHashTb,file_path, SRC);
    int file_exists = 0;
    if (file_req != NULL){
        file_exists = 1;
    }
    writeNB(fd, &(file_exists),sizeof(int));
    if (file_exists){
        pthread_mutex_lock(&file_req->mutex);
        file_size = file_req->size;
        writeNB(fd, &file_size,sizeof(size_t));
        printf("path_size is %d \n",path_size);
        printf("file path is %s\n",file_path);
        int i;
        //write every page (except last one) to the client fd
        for(i=0; i<file_req->pages_n-1; i++){
            writeNB(fd, FileMemory.memPtr[file_req->pages[i]],PAGE_SIZE);
        }
        //write last page (its contents may be less than a full page, so only write up to end of file)
        writeNB(fd, FileMemory.memPtr[file_req->pages[i]],file_size % (PAGE_SIZE+1));
        pthread_mutex_unlock(&file_req->mutex);
    }
    free(file_path);
    return;
}

void fileNRead(void* args){
    printf("trying to call func from funcArr!\n");
    return;
}
void fileWrite(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int path_size;
    int fd = req->fd;
    size_t file_size;
    char* file_path;
    readNB(fd,&path_size,sizeof(int));
    if ((file_path = malloc(path_size)) == NULL){
        printf(" file write file_path malloc error!\n");
        return;
    }
    readNB(fd,file_path,path_size);
    readNB(fd,&file_size,sizeof(size_t ));
    printf("path_size is %d \n",path_size);
    printf("file path is %s\n",file_path);
    printf(" file size is %zu\n",file_size);
    void* MM_file = malloc(file_size);
    char* MM_file_ptr = MM_file;
    readNB(fd,MM_file,file_size);
    MemFile* new_file = malloc(sizeof(MemFile));
    int already_exists = fileInit(new_file,file_size, file_path);
    if(already_exists == 1){
        free(new_file); 
    }else{
        pthread_mutex_lock(&(new_file->mutex));
        int to_write;
        MM_file_ptr = MM_file;
        //File is written into memory one page at a time
        for(to_write=new_file->pages_n; to_write>1; to_write--){
            addPageToMem(MM_file_ptr, new_file, to_write-1, PAGE_SIZE);
            MM_file_ptr += PAGE_SIZE;
        }
        //the last page will be smaller than PAGE_SIZE, so we calculate how big it is
        addPageToMem(MM_file_ptr, new_file, to_write-1, file_size % PAGE_SIZE); 
        pthread_mutex_unlock(&(new_file->mutex));
    }
    free(MM_file);
    free(file_path);
    return;
}

void fileAppend(void* args){
    printf("trying to call func from funcArr!\n");
    return;
}
void fileOpen(void* args){
    printf("trying to call func from funcArr!\n");
    return;
}
void fileClose(void* args){
    printf("trying to call func from funcArr!\n");
    return;
}
void fileDelete(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int path_size;
    int fd = req->fd;
    char* file_path;
    readNB(fd,&path_size,sizeof(int));
    if ((file_path = malloc(path_size)) == NULL){
        printf(" file delete file_path malloc error!\n");
        return;
    }
    readNB(fd,file_path,path_size);
    MemFile* file_to_del = hashGetFile(FileHashTb, file_path, SRC);
    if (file_to_del == NULL){
        printf("Error,file not found\n");
        free(file_path);
        return;
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
    return;
}
void fileLock(void* args){
    printf("trying to call func from funcArr!\n");
    return;
}
void fileUnlock(void* args){
    printf("trying to call func from funcArr!\n");
    return;
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
void ThreadRequestExit(void* args){
    printf("Thread has  quit!\n");
    threadPool* pool = (threadPool*) args;
    threadPoolAdd(pool, ThreadRequestExit, pool);
    pthread_exit(NULL);
}
