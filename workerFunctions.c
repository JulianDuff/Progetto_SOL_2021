#include "workerFunctions.h"

void* fileRead(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    //fileOpenCheck will read from the socket what file the client wants to read and return
    //its record if it exists in memory and the client has access to it
    MemFile* file_req ;
    int validReq = fileOpenCheck(fd,&file_req);
    writeNB(fd,&validReq,sizeof(int));
    if (validReq)
        sendFile(fd,file_req);
    return NULL;
}

void* fileNRead(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    int file_num = 0;
    readNB(fd,&file_num,sizeof(file_num));
    printf("I have received request to read %d files",file_num);
    fileStackDefrag(FStack);
    int i;
    pthread_mutex_lock(&filestack_mutex);
    //to read files in random order, a copy of the filestack with its elements in random order is made
    unsigned long* stackRandOrd = arrayRandomPermutation(FStack->stack,FStack->top);
    pthread_mutex_unlock(&filestack_mutex);
    for(i=0; i<file_num && i < FStack->top; i++){
        pthread_mutex_lock(&hashTB_mutex);
        MemFile* rnd_file = (MemFile*) hashTbSearch(FileHashTb, stackRandOrd[i],SRC);        
        pthread_mutex_unlock(&hashTB_mutex);
        if (rnd_file != NULL){
            pthread_mutex_lock(&rnd_file->mutex);
            char* name_sent = fileShortenName(rnd_file->abspath);
            int file_name_size = strlen(name_sent)+1;
            writeNB(fd,&file_name_size,sizeof(int));
            writeNB(fd,name_sent,file_name_size);
            pthread_mutex_unlock(&rnd_file->mutex);
            sendFile(fd,rnd_file);
            free(name_sent);
        }
    }
    free(stackRandOrd);
    int end = 0;
    writeNB(fd,&end,sizeof(int));
    return NULL;
}
void* fileWrite(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req = NULL;
    int validRead = fileOpenCheck(fd,&file_req);
    size_t file_size;
    readNB(fd,&file_size,sizeof(size_t));
    pthread_mutex_lock(&(file_req->mutex));
    if(file_req->pages_n != 0){
        validRead= 0;
    }
    if (file_size > server_memory_size){
        validRead= 0;
        printf("file write was too big, refused write request\n");
    }
    writeNB(fd,&validRead,sizeof(validRead));
    if (!validRead){
        pthread_mutex_unlock(&file_req->mutex);
        return NULL;
    }
    file_req->size = file_size;
    printf(" file size is %zu\n",file_size);
    FileNUpdate(file_max);
    filePagesInitialize(file_req);
    void* MM_file = malloc(file_size);
    char* MM_file_ptr = MM_file;
    readNB(fd,MM_file,file_size);
    int to_write;
    MM_file_ptr = MM_file;
    //File is written into memory one page at a time
    for(to_write=0; to_write<file_req->pages_n-1; to_write++){
        addPageToMem(MM_file_ptr, file_req, to_write, page_size, 0);
        MM_file_ptr += page_size;
    }
    //the last page will be smaller than page_size, so we calculate how big it is
    addPageToMem(MM_file_ptr, file_req, to_write, file_size % page_size, 0); 
    fileStackAdd(FStack, hashKey(file_req->abspath));
    pthread_mutex_unlock(&file_req->mutex);
    free(MM_file);
    int response = 1;
    writeNB(fd,&response,sizeof(response));
    return NULL;
}

void* fileAppend(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req;
    int validRead = fileOpenCheck(fd,&file_req);
    size_t append_size;
    readNB(fd,&append_size,sizeof(append_size));
    pthread_mutex_lock(&file_req->mutex);
    if (append_size+file_req->size > server_memory_size){
        //file size was larger than the server's memory capacity, alert the client and abort write
        validRead = 0;
        printf("file write was too big, refused write request\n");
    }
    writeNB(fd,&validRead,sizeof(validRead));
    if (!validRead){
        pthread_mutex_unlock(&file_req->mutex);
        return NULL;
    }
    int response = -1;
    int old_pages = file_req->pages_n;
    int new_page_n;  
    //how many bytes are left in the last file page
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
    //fileSearch tells the socket whether file exists (1) or not(0)
    MemFile* file_req = fileSearch(args);
    pid_t* client_pid = malloc(sizeof(pid_t));
    readNB(fd,client_pid,sizeof(pid_t));
    if (file_req == NULL){
        //file was not found, abort open
        free(client_pid);
        return NULL;
    }
    else{
        pthread_mutex_lock(&file_req->mutex);
        DL_ListAdd(&(file_req->clients_opened), client_pid);
        pthread_mutex_unlock(&file_req->mutex);
    }
    return NULL;
}

//sets file requested by client in filePtr (NULL if request not valid)
//returns 1 if file request is valid, 0 if it's not opened or does not exist
int  fileOpenCheck(int fd, MemFile** filePtr){
    int fileExists = fileSearchSilent(fd,filePtr);
    pid_t client_pid;
    readNB(fd,&client_pid,sizeof(pid_t));
    int isOpen = 0;
    if (*filePtr!= NULL){
        pthread_mutex_lock(&(*filePtr)->mutex);
        if (clientOpenSearch((*filePtr)->clients_opened,client_pid) != 0){
            isOpen= 1;
        }
        pthread_mutex_unlock(&(*filePtr)->mutex);
    }
    return (isOpen*fileExists);
}
void* fileClose(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req = fileSearch(args);
    pid_t client_pid;
    readNB(fd,&client_pid,sizeof(pid_t));
    if (file_req == NULL){
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
    printf("Deleting  file %s\n",file_to_del->abspath);
    hashGetFile(FileHashTb, file_path,DEL);
    pthread_mutex_unlock(&file_to_del->mutex);
    fileFree(file_to_del);
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
// if file already exists a response is sent to the client to signify refusal to complete the request
//  otherwise it is added to file_list,
// and it notifies the client of the write if successful
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
    //Received request to write file, lock hash table until file is placed into it
    pthread_mutex_lock(&hashTB_mutex);
    pthread_mutex_init(&(new_file->mutex), NULL);
    pthread_mutex_lock(&new_file->mutex);
    new_file->abspath = file_path;
    new_file->pages = NULL;
    new_file->clients_opened = NULL;
    new_file->pages_n = 0;
    unsigned long key = hashKey(new_file->abspath);
    hashTbAdd(FileHashTb, new_file,key);
    //added file record to table, it can now be unlocked,
    pthread_mutex_unlock(&hashTB_mutex);
    pthread_mutex_unlock(&new_file->mutex);
    success = 1;
    writeNB(fd,&success,sizeof(int));
   return NULL; 
}


int sendFile(int fd,MemFile* file){
        pthread_mutex_lock(&file->mutex);
        size_t file_size = file->size;
        writeNB(fd, &file_size,sizeof(size_t));
        int i;
        //write every page (except last one) to the client fd
        for(i=0; i<file->pages_n-1; i++){
            writeNB(fd, FileMemory.memPtr[file->pages[i]],page_size);
        }
        //write last page (its contents may be less than a full page, so only write up to end of file)
        int last_page_size = page_size;
        if (file_size % page_size > 0)
            last_page_size = file_size % page_size;
        writeNB(fd, FileMemory.memPtr[file->pages[i]], last_page_size);
        pthread_mutex_unlock(&file->mutex);
    return 0;
}

//Gets the name of the file from the stored absolute path (ignores directories)
//if file is /home/somefolder/file5 return value is -> file5
//string returned is allocated dynamically
char* fileShortenName(char* file_name){
    int len = strlen(file_name)+1;
    char* name_copy = malloc(len);
    strncpy(name_copy,file_name,len);
    char* save = NULL;
    char* token;
    token= strtok_r(name_copy,"/",&save);
    char* last_token = token;
    while ( (token = strtok_r(NULL, "/",&save)) != NULL){
        last_token = token;
    }
    len = strlen(last_token)+1;
    char* ret_str = malloc(len);
    strncpy(ret_str,last_token,len);
    free(name_copy);
    return ret_str;
}

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

int  fileSearchSilent(int fd,MemFile** filePtr){
    int path_size;
    char* file_path;
    readNB(fd,&path_size,sizeof(int));
    if ((file_path = malloc(path_size)) == NULL){
        printf(" file write file_path malloc error!\n");
        return 0;
    }
    readNB(fd,file_path,path_size);
    MemFile* file_req = NULL;
    int found = 0;
    file_req = hashGetFile(FileHashTb,file_path, SRC);
    if (file_req != NULL){
        *filePtr = file_req;
        found = 1;
    }
    free(file_path);
    return found;
}


//creates a copy of array (size n) with its elements set in random order
unsigned long* arrayRandomPermutation(unsigned long* array ,int n){
    int i;
    unsigned long* array_copy;
    if ((array_copy = malloc(sizeof(unsigned long)*n)) == NULL){
        fprintf(stderr,"error allocating ReadN array\n");
        return NULL;
    }
    for (i=0; i<n; i++){
        array_copy[i] = array[i];
    }
    srand(time(0));
    for (i=n-1; i>0; i--){
        int indx = rand() % (i);
        unsigned long tmp = array_copy[i];
        array_copy[i] = array_copy[indx];
        array_copy[indx] = tmp;
    }
    return array_copy;
}
