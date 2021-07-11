#include "workerFunctions.h"

void* fileRead(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req ;
    int req_err = fileOpenCheck(fd,&file_req);
    writeNB(fd,&req_err,sizeof(int));
    if (!req_err)
        sendFile(fd,file_req);
    return NULL;
}

void* fileNRead(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    int file_num = 0;
    readNB(fd,&file_num,sizeof(file_num));
    printf("Received request to read %d files\n",file_num);
    // clear the stack of deleted files or empty space
    fileStackDefrag(FStack);
    int i;
    pthread_mutex_lock(&filestack_mutex);
    //to read files in random order, a copy of the filestack with its elements in random order is made
    int arr_s = FStack->top;
    char** stackRandOrd = arrayRandomPermutation(FStack->stack,arr_s);
    pthread_mutex_unlock(&filestack_mutex);
    if (file_num == 0)
        file_num = FStack->top;
    // read from the stack copy until file_num files are read or the stack has no more files
    for(i=0; i<file_num && i < FStack->top; i++){
        MemFile* rnd_file = hashGetFile(FileHashTb,stackRandOrd[i],SRC);
        // the file might have been deleted since the stack copy was made
        if (rnd_file != NULL){
            pthread_mutex_lock(&rnd_file->mutex);
            char* name_sent = fileShortenName(rnd_file->abspath);
            int file_name_size = strlen(name_sent)+1;
            writeNB(fd,&file_name_size,sizeof(int));
            writeNB(fd,name_sent,file_name_size);
            printf("Sent file -%s- of size %zu\n",rnd_file->abspath, rnd_file->size);
            pthread_mutex_unlock(&rnd_file->mutex);
            sendFile(fd,rnd_file);
            free(name_sent);
        }
    }
    //free memory used for the stack copy
    for (i=0; i<arr_s;i++)
        free(stackRandOrd[i]);
    free(stackRandOrd);
    int end = 0;
    writeNB(fd,&end,sizeof(int));
    return NULL;
}

void* fileWrite(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req = NULL;
    int req_err = fileOpenCheck(fd,&file_req);
    size_t file_size;
    readNB(fd,&file_size,sizeof(size_t));
    pthread_mutex_lock(&(file_req->mutex));
    if(file_req->pages_n != 0){
        req_err= EEXIST;
    }
    size_t serv_mem_size = configReadSizeT(&c_server_memory_size,&c_server_memory_size_mtx);
    if (file_size > serv_mem_size){
        req_err= EFBIG;
        fprintf(stdout,"file -%s- was too big, refused write request\n",file_req->abspath);
    }
    writeNB(fd,&req_err,sizeof(req_err));
    if (req_err){
        pthread_mutex_unlock(&file_req->mutex);
        return NULL;
    }
    file_req->size = file_size;
    //check whether adding a file exceeds server capacity
    int file_max = configReadInt(&c_file_max,&c_file_max_mtx);
    FileNUpdate(file_max);
    filePagesInitialize(file_req);
    void* MM_file = malloc(file_size);
    char* MM_file_ptr = MM_file;
    readNB(fd,MM_file,file_size);
    int to_write;
    MM_file_ptr = MM_file;
    //File is written into memory one page at a time
    size_t page_size = configReadSizeT(&c_page_size,&c_page_size_mtx);
    for(to_write=0; to_write<file_req->pages_n-1; to_write++){
        addPageToMem(MM_file_ptr, file_req, to_write, page_size, 0);
        MM_file_ptr += page_size;
    }
    //the last page may be smaller than page_size, so we calculate how big it is
    size_t last_write = file_size % page_size;
    if (last_write == 0)
        last_write = page_size;
    addPageToMem(MM_file_ptr, file_req, to_write, last_write, 0); 
    fileStackAdd(FStack, file_req->abspath);
    fprintf(stdout,"Written file -%s- of size %zu\n",file_req->abspath, file_req->size);
    pthread_mutex_unlock(&file_req->mutex);
    free(MM_file);
    int response = 0;
    writeNB(fd,&response,sizeof(response));
    return NULL;
}

void* fileAppend(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req;
    int req_err = fileOpenCheck(fd,&file_req);
    size_t append_size;
    readNB(fd,&append_size,sizeof(append_size));
    pthread_mutex_lock(&file_req->mutex);
    size_t serv_mem_size = configReadSizeT(&c_server_memory_size,&c_server_memory_size_mtx);
    if (append_size+file_req->size > serv_mem_size){
        //file size was larger than the server's memory capacity, alert the client and abort write
        req_err = 1;
        fprintf(stdout,"file -%s- was too big, refused write request\n",file_req->abspath);
    }
    writeNB(fd,&req_err,sizeof(req_err));
    if (req_err){
        pthread_mutex_unlock(&file_req->mutex);
        return NULL;
    }
    int response = -1;
    int old_pages = file_req->pages_n;
    int new_page_n;  
    //how many bytes are left in the last file page
    size_t page_size = configReadSizeT(&c_page_size,&c_page_size_mtx);
    int bytes_available = page_size - file_req->size % page_size;
    int bytes_overflow = append_size - bytes_available;
    // are there bytes to write tha can't fit in the last page
    if (bytes_overflow > 0){
        //if so add as many pages as are needed
        new_page_n = file_req->pages_n + ( bytes_overflow / page_size)+1;
    }
    else{
        new_page_n = old_pages;
    }
    filePagesRenew(file_req, new_page_n);
    void* MM_file = malloc(append_size);
    readNB(fd,MM_file,append_size);
    char* MM_file_ptr = MM_file;
    int to_write = 0;
    int first_add;
    if (append_size > bytes_available){
        first_add = bytes_available;
    }
    else{
        first_add = append_size;
    }
    addPageToMem(MM_file_ptr, file_req, old_pages-1, first_add, (file_req->size % page_size));
    MM_file_ptr+= first_add;

    //if append needs to write full pages they are written in this loop
    for(to_write=old_pages; to_write<file_req->pages_n-1; to_write++){
        addPageToMem(MM_file_ptr, file_req, to_write, page_size,0);
        MM_file_ptr += page_size;
    }
    //the last page will be smaller than page_size, so we calculate how big it is
    if (old_pages != file_req->pages_n){
        // this step is skipped if append didn't add any pages
        addPageToMem(MM_file_ptr, file_req, to_write, bytes_overflow % page_size,0); 
    }
    response = 0;
    file_req->size += append_size;
    pthread_mutex_unlock(&file_req->mutex);
    free(MM_file);
    writeNB(fd,&response,sizeof(response));
    return NULL;
}

//reads from client socket and opens file requested if it exists
void* fileOpen(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req;
    int req_err = fileSearchSilent(fd,&file_req);
    writeNB(fd,&req_err,sizeof(req_err));
    pid_t* client_pid = malloc(sizeof(pid_t));
    readNB(fd,client_pid,sizeof(pid_t));
    if (req_err){
        //file was not found, abort open
        free(client_pid);
        return NULL;
    }
    else{
        pthread_mutex_lock(&file_req->mutex);
        //add client pid to list of clients that opened file
        DL_ListAdd(&(file_req->clients_opened), client_pid);
        pthread_mutex_unlock(&file_req->mutex);
    }
    return NULL;
}

//sets file requested by client in filePtr (NULL if request not valid)
//returns 0 if file request is valid, >0 if an error occurred (not opened or does not exist )
int  fileOpenCheck(int fd, MemFile** filePtr){
    int fileNotExists = fileSearchSilent(fd,filePtr);
    pid_t client_pid;
    readNB(fd,&client_pid,sizeof(pid_t));
    if (fileNotExists)
        return (fileNotExists);
    int isNotOpen= EPERM;
    if (*filePtr!= NULL){
        pthread_mutex_lock(&(*filePtr)->mutex);
        if (clientOpenSearch((*filePtr)->clients_opened,client_pid) != 0){
            isNotOpen = 0;
        }
        pthread_mutex_unlock(&(*filePtr)->mutex);
    }
    return isNotOpen;
}

void* fileClose(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    pid_t client_pid;
    MemFile* file_req;
    int req_err = fileSearchSilent(fd,&file_req);
    writeNB(fd,&req_err,sizeof(req_err));
    readNB(fd,&client_pid,sizeof(pid_t));
    if (file_req == NULL || req_err){
        return NULL;
    }
    else{
        pthread_mutex_lock(&file_req->mutex);
        clientPidDelete(file_req, client_pid);
        pthread_mutex_unlock(&file_req->mutex);
    }
    return NULL;
}

void* fileDelete(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req ;
    int req_err = fileOpenCheck(fd,&file_req);
    writeNB(fd,&req_err,sizeof(int));
    if (req_err){
        return NULL;
    }
    //file exists, lock its mutex then delete its record in the hashtable
    pthread_mutex_lock(&file_req->mutex);
    printf("Deleting  file -%s-\n",file_req->abspath);
    hashGetFile(FileHashTb, file_req->abspath,DEL);
    //file has no record in the hashtable, unlock mutex and delete it
    pthread_mutex_unlock(&file_req->mutex);
    fileFree(file_req);
    return NULL;
}

void* fileLock(void* args){
    return NULL;
}
void* fileUnlock(void* args){
    return NULL;
}

//tell the client whether a file is in the server by sending 0 if found, >0 if not
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
    int found = ENOENT;
    file_req = hashGetFile(FileHashTb,file_path, SRC);
    if (file_req != NULL){
        found = 0;
    }
    writeNB(fd,&found,sizeof(found));
    free(file_path);
    return file_req;
}

// sets values of FilePtr based on its size and path name,
// if file already exists a response is sent to the client to signify refusal to complete the request
// file is added to the hashtable but not to the stack, as it is currently empty
void* fileInit(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    int path_size;
    char* file_path;
    int success = 0;
    readNB(fd,&path_size,sizeof(int));
    if ((file_path = malloc(path_size)) == NULL){
        printf(" file write file_path malloc error!\n");
        return NULL;
    }
    readNB(fd,file_path,path_size);
    MemFile* new_file = malloc(sizeof(MemFile));
    //Received request to initialize file, lock hash table until file is placed into it
    pthread_mutex_lock(&hashTB_mutex);
    pthread_mutex_init(&(new_file->mutex), NULL);
    pthread_mutex_lock(&new_file->mutex);
    new_file->abspath = file_path;
    new_file->pages = NULL;
    new_file->clients_opened = NULL;
    new_file->pages_n = 0;
    new_file->size = 0;
    unsigned long key = hashKey(new_file->abspath);
    hashTbAdd(FileHashTb, new_file,key);
    //added file record to table, it can now be unlocked,
    pthread_mutex_unlock(&hashTB_mutex);
    pthread_mutex_unlock(&new_file->mutex);
    success = 1;
    writeNB(fd,&success,sizeof(int));
   return NULL; 
}

//send a file's contents to fd socket
int sendFile(int fd,MemFile* file){
        pthread_mutex_lock(&file->mutex);
        size_t file_size = file->size;
        writeNB(fd, &file_size,sizeof(size_t));
        int i;
        //write every page (except last one) to the client fd
        if (file_size != 0){
            size_t page_size = configReadSizeT(&c_page_size,&c_page_size_mtx);
            for(i=0; i<file->pages_n-1; i++){
                writeNB(fd, FileMemory.memPtr[file->pages[i]],page_size);
            }
            //write last page (its contents may be less than a full page, so only write up to end of file)
            size_t last_page_size = file_size % page_size;
            if (last_page_size == 0)
                last_page_size = page_size;
            writeNB(fd, FileMemory.memPtr[file->pages[i]], last_page_size);
        }
        pthread_mutex_unlock(&file->mutex);
    return 0;
}

//removes client_pid from file's list of clients who have it currently opened
int clientPidDelete(MemFile* file ,pid_t client_pid){
    int isHead = 1 ;
    if (file->clients_opened == NULL)
        return 0;
    DL_List* list_ptr = file->clients_opened;
    while (list_ptr != NULL){
        pid_t* found_pid = DL_ListTake(&list_ptr,SRC);
        if(found_pid == NULL){
            break;
        }
        if (client_pid == *found_pid){
            if (isHead){
                DL_List* aux = file->clients_opened; 
                file->clients_opened = file->clients_opened->next;
                if (file->clients_opened != NULL)
                    file->clients_opened->prev = NULL;
                else
                free(aux);
            }
            else{
                DL_ListDeleteCell(list_ptr);
            }
            free(found_pid);
            return 1;
        }
        list_ptr = list_ptr->next;
        isHead = 0;
    }
    return 0;
}

//look for file specified by client fd,update filePtr to its record if it exists
//(otherwise NULL) and return 0 if file was found
//does not tell the client if file was found or not 
int  fileSearchSilent(int fd,MemFile** filePtr){
    int path_size;
    char* file_path;
    readNB(fd,&path_size,sizeof(int));
    if ((file_path = malloc(path_size)) == NULL){
        printf(" file write file_path malloc error!\n");
        return -1;
    }
    readNB(fd,file_path,path_size);
    MemFile* file_req = NULL;
    int Notfound = ENOENT;
    file_req = hashGetFile(FileHashTb,file_path, SRC);
    if (file_req != NULL){
        Notfound = 0;
    }
    *filePtr = file_req;
    free(file_path);
    return Notfound;
}

//creates a copy of array (of size n) with its elements set in random order
char** arrayRandomPermutation(char** array ,int n){
    int i;
    char** array_copy;
    if ((array_copy = malloc(sizeof(char*)*n)) == NULL){
        fprintf(stderr,"error allocating ReadN array\n");
        return NULL;
    }
    for (i=0; i<n; i++){
        array_copy[i] = malloc(_POSIX_PATH_MAX);
        strncpy(array_copy[i],array[i],_POSIX_PATH_MAX);
    }
    srand(time(0));
    for (i=n-1; i>0; i--){
        int indx = rand() % (i);
        char* tmp = array_copy[i];
        array_copy[i] = array_copy[indx];
        array_copy[indx] = tmp;
    }
    return array_copy;
}

