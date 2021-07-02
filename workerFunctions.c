#include "workerFunctions.h"

void* fileRead(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    //fileOpenCheck will read from the socket what file the client wants to read and return
    //its record if it exists in memory and the client has access to it
    MemFile* file_req = fileOpenCheck(args);
    if (file_req != NULL){
        sendFile(fd,file_req);
    }
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
    for(i=0; i<file_num && i <FStack->top; i++){
        pthread_mutex_lock(&hashTB_mutex);
        MemFile* rnd_file = (MemFile*) hashTbSearch(FileHashTb, FStack->stack[i],SRC);        
        pthread_mutex_unlock(&hashTB_mutex);
        if (rnd_file != NULL){
            pthread_mutex_lock(&rnd_file->mutex);
            char* name_sent = fileShortenName(rnd_file->abspath);
            printf("ret token is %s!\n",name_sent);
            int file_name_size = strlen(name_sent)+1;
            writeNB(fd,&file_name_size,sizeof(int));
            writeNB(fd,name_sent,file_name_size);
            pthread_mutex_unlock(&rnd_file->mutex);
            sendFile(fd,rnd_file);
            free(name_sent);
        }
    }
    int end = 0;
    writeNB(fd,&end,sizeof(int));
    return NULL;
}
void* fileWrite(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req = fileOpenCheck(args);
    if (file_req == NULL){
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
        addPageToMem(MM_file_ptr, file_req, to_write, page_size, 0);
        MM_file_ptr += page_size;
    }
    //the last page will be smaller than page_size, so we calculate how big it is
    addPageToMem(MM_file_ptr, file_req, to_write, file_size % page_size, 0); 
    fileStackAdd(FStack, hashKey(file_req->abspath));
    pthread_mutex_unlock(&file_req->mutex);
    free(MM_file);
    writeNB(fd,&response,sizeof(response));
    return NULL;
}

void* fileAppend(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int fd = req->fd;
    MemFile* file_req = fileOpenCheck(args);
    if (file_req == NULL){
        return NULL;
    }
    else{
        pthread_mutex_lock(&file_req->mutex);
        int response = 0; size_t append_size;
        //get file size from socket
        readNB(fd,&append_size,sizeof(size_t ));
        if (append_size+file_req->size > server_memory_size){
        //file size was larger than the server's memory capacity, alert the client and abort write
        response = 1;
        printf("file write was too big, refused write request\n");
        }
        writeNB(fd,&response,sizeof(response));
        if (response){
            pthread_mutex_unlock(&file_req->mutex);
            return NULL;
        }
        int old_pages = file_req->pages_n;
        int new_page_n;  
        int bytes_available = page_size - file_req->size % page_size;
        int bytes_overflow = append_size - bytes_available;
        if (bytes_overflow > 0){
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
        //File is written into memory one page at a time
        for(to_write=old_pages; to_write<file_req->pages_n-1; to_write++){
            addPageToMem(MM_file_ptr, file_req, to_write, page_size,0);
            MM_file_ptr += page_size;
        }
        //the last page will be smaller than page_size, so we calculate how big it is
        if (old_pages != file_req->pages_n){
            // this step is skipped if append didn't add any pages
            addPageToMem(MM_file_ptr, file_req, to_write, bytes_overflow % page_size,0); 
        }
        file_req->size += append_size;
        pthread_mutex_unlock(&file_req->mutex);
        free(MM_file);
        writeNB(fd,&response,sizeof(response));
        return NULL;
    }
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
    pthread_mutex_unlock(&hashTB_mutex);
    new_file->clients_opened = NULL;
    new_file->pages_n = 0;
    pthread_mutex_unlock(&new_file->mutex);
    int success = 1;
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

char* fileShortenName(char* file_name){
    char* save = NULL;
    char* token;
    token= strtok_r(file_name,"/",&save);
    char* last_token = token;
    while ( (token = strtok_r(NULL, "/",&save)) != NULL){
        last_token = token;
    }
    char* ret_str = malloc(strlen(last_token)+1);
    strncpy(ret_str,last_token,strlen(last_token)+1);
    return ret_str;
}
