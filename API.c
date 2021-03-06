#include "API.h"

#define MAX_PATH 108
const int O_CREATE = 0x0001;
const int O_LOCK   = 0x0002;
int ENABLE_PRINTS;

int fd_st = NOT_SET; // global variable that holds the fd for communicating with the server
char* client_socket_name; // name of the socket file

int openConnection(const char* sock_name, int msec, const struct timespec abstime){ 
    //struct that holds how often client will attempt to reconnect
    struct timespec retry_t;
    retry_t.tv_sec = msec / 1000;
    retry_t.tv_nsec = (msec % 1000)*1000*1000;
    //if connection has already been established do nothing and return 1
    if (fd_st == NOT_SET){
        int fd_socket = socket(AF_UNIX,SOCK_STREAM, 0);
        struct sockaddr_un sock_addr;
        int str_l = strnlen(sock_name,MAX_PATH);
        strncpy(sock_addr.sun_path, sock_name, str_l+1);
        sock_addr.sun_family = AF_UNIX;
        while ( connect(fd_socket, (struct sockaddr*)&sock_addr, SUN_LEN(&sock_addr)) != 0){ 
          // connection to socket is attempted every given msec until it succeeds
          struct timespec curr_time;
          clock_gettime(CLOCK_REALTIME,&curr_time);
          //if abstime has passed, abort and return -1
          if (curr_time.tv_sec > abstime.tv_sec || ( curr_time.tv_sec == abstime.tv_sec && curr_time.tv_nsec > abstime.tv_nsec)){
              errno = ETIME;
              return -1;
          }
          nanosleep(&retry_t,NULL);
      }
      if (ENABLE_PRINTS)
          printf("Connection success!\n");
      //saving the socket fd and the socket name for future use
      fd_st = fd_socket;                        
      str_l = strnlen(sock_name,MAX_PATH);
      client_socket_name = malloc(sizeof(str_l)+1);
      strncpy(client_socket_name,sock_name,str_l+1);
      return 0;
    }
    else{
      return 1;
    }
}

int closeConnection(const char* sock_name){
    //check that the connection has been established
    if(strncmp(client_socket_name,sock_name,MAX_PATH) == 0){ 
        close(fd_st);
        if (ENABLE_PRINTS)
            printf(" Connection %s closed succesfully.\n",sock_name); 
        free(client_socket_name);
        return 0;
    }
    else {
        errno = ENOTCONN;
        if (ENABLE_PRINTS)
            fprintf(stderr,"ERROR: no such socket exists / socket not opened\n");
        return -1;
    }
}


int openFile(const char* path_name, int flags){
    if (flags & O_LOCK){

    }
    int response;
    if (flags & O_CREATE){
        //if file does not exists in server request to initialize it, otherwise abort open and return -1
        response = sendRequest(e_fileSearch,path_name);
        // response ENOENT == file does not exist in server
        if (response == ENOENT){
            sendRequest(e_fileInit,path_name);
        }
        else{
            errno = EEXIST;
            return -1;
        }
    }
    if ((response = sendRequest(e_fileOpen,path_name)) != -1){
        pid_t pid_self = getpid();
        sendToSocket(&pid_self,sizeof(pid_t));
        if (response > 0){
            errno = response;
            return -1;
        }
        //a file opened is determined by the client pid, so it has to be sent
        //hear from server whether file exists in server
    }
    else{
        errno = EIO;
        return -1;
    }
    return 0;
}


int closeFile(const char* path_name){
    int response;
    if (( response = sendRequest(e_fileClose,path_name)) != -1){
        pid_t pid_self = getpid();
        sendToSocket(&pid_self,sizeof(pid_t));
        if (response >0){
            errno = response;
            return -1;
        }
        return 0;
    }
    else{
        errno = EIO;
        return -1;
    }
}

//send a message to the server if the connection is established
int sendToSocket(void* buff, const int len){ 
    if (fd_st == -1){
        printf("Error: connection not established\n");
        return -1;
    }
    int sent_bytes = writeNB(fd_st, buff, len);
    return sent_bytes;
}

int writeFile(const char* path_name,const char* dir_name){
    char* abspath;
    int inp_file;
    size_t file_len;
    int path_len;
    int response = -1;
    if ( setFileData(path_name, &inp_file, &file_len, &abspath, &path_len) == -1){
        errno = EIO;
        if (ENABLE_PRINTS)
            fprintf(stderr,"failed to send write request for -%s-\n",path_name);
        return -1;
    }
    //don't send a write request for an empty file
    if (file_len == 0){
        errno = EIO;
        return -1;
    }
    int func = e_fileWrite;
    //information to send to server
    sendMetadata(&func,&path_len, abspath);
    pid_t pid_self = getpid();
    sendToSocket(&pid_self,sizeof(pid_t));
    sendToSocket(&file_len,sizeof(size_t));

    int req_err;
    readNB(fd_st,&req_err,sizeof(int));
    if (req_err == 0){
        //server can complete the write request (no error occurred)
        char* out_buff = NULL;
        if ((out_buff = malloc(file_len)) == NULL){
            printf("receiving file malloc error!");
            free(abspath);
            return -1;
        }
        if (ENABLE_PRINTS)
            printf("writing file -%s- of size %zu bytes \n",path_name,file_len);
        //get file contents
        readNB(inp_file, out_buff, file_len);
        sendToSocket(out_buff, file_len);
        readNB(fd_st,&response,sizeof(response));
        free(out_buff);
    }
    else{
        errno = req_err;
        if (ENABLE_PRINTS)
        printf("file write of -%s- refused, error code = %d \n", path_name,errno);
    }
    free(abspath);
    close(inp_file);
    return response;
}

//tells the server the desired function requested and file name if set (path not NULL)
int sendMetadata(const int* func, const int* path_len, const char* path){
    sendToSocket((void*)func, sizeof(int));
    if (path != NULL){
        sendToSocket((void*)(path_len), sizeof(int));
        sendToSocket((void*)path, *path_len);
    }
    return 0;
}

int writeNB(const int fd, void* data, const size_t size){
        int total_sent_bytes = 0;
        int sent_bytes;
        while(total_sent_bytes < size){
            sent_bytes = write(fd, data, size - total_sent_bytes);
            total_sent_bytes += sent_bytes;
            data += sent_bytes;
            if (sent_bytes == 0){
                break;
            }
            if (sent_bytes < 0){
                perror("client write");
                break;
            }
        }
    return 0;
}

size_t readNB(const int fd, void* buff, const size_t n){
    size_t n_left;
    size_t n_read;
    n_left = n;
    while (n_left > 0){
        if ((n_read = read(fd, buff, n_left)) < 0){
            if (n_left == n){
                return -1;
            }
            else{
                break;
            }
        }
        else if (n_read == 0){
            break;
        }
        n_left -= n_read;
        buff += n_read;
    }
    return (n - n_left);
}

size_t fileGetSize(int  file){
    size_t size = 0;
    struct stat f_stat;
    fstat(file,&f_stat);
    size = f_stat.st_size;
    return size;
}

int readFile(const char* path_name, void **buf, size_t* size){
    int ret_val = 0;
    char* abspath;
    size_t file_len;
    int path_len;
    if ( setFileData(path_name, NULL, NULL, &abspath, &path_len) == -1){
        errno = EIO;
        return -1;
    }
    int func = e_fileRead;
    //information to send the server
    sendMetadata(&func,&path_len, abspath);
    pid_t pid_self = getpid();
    sendToSocket(&pid_self,sizeof(pid_t));
    int req_err;
    readNB(fd_st,&req_err,sizeof(int));
    if (req_err == 0){
        // file was found, its size and contents were sent to the socket
        char* out_buff = NULL;
        readNB(fd_st, &file_len, sizeof(size_t));
        if ((out_buff = malloc(file_len) )== NULL){
            fprintf(stderr,"Malloc file read error!\n");
            ret_val = -1;
        }
        else{
            //place file contents into out_buff
            readNB(fd_st, out_buff, file_len);
          if (ENABLE_PRINTS)
                printf(" received file -%s- of size %zu \n",path_name, file_len);
        }
        *buf = out_buff;
        *size = file_len;
    }
    else {
        // file was not found
        errno = req_err;
        if (ENABLE_PRINTS){
            printf("File %s not found or not opened!\n",path_name);
        }
        ret_val = -1;
    }
    free(abspath);
    return ret_val;
}

int readNFiles(int N, const char* dir_name){
    if (ENABLE_PRINTS)
        printf("called readNFiles of %d files\n",N);
    int buff = N;
    //if dir_name is not a valid directory cancel operation
    if (dir_name != NULL && checkDir(dir_name) != 1){
        errno = ENOTDIR;
        return -1;
    }
    if (sendRequest(e_fileNRead,NULL) == -1){
        errno = EIO;
        return -1;
    }
    //tell the server how many files the client wants to read
    sendToSocket( &buff, sizeof(int));
    size_t file_s = 0;
    int file_name_size;
    //Read from socket until file_name_size sent is 0 (no more files)
    while(readNB(fd_st,&file_name_size,sizeof(file_name_size)) > 0){
        if (file_name_size == 0)
            break;
        // get file name,size and contents from socket
        char* file_name = malloc(file_name_size);
        readNB(fd_st,file_name,file_name_size);
        readNB(fd_st,&file_s,sizeof(size_t));
        char* file_contents= malloc(file_s);
        readNB(fd_st, file_contents, file_s);
        
        if (ENABLE_PRINTS)
            printf("received file -%s- of size %zu bytes\n",file_name, file_s);
        int ret_val = 0;
        // if a directory was passed, write the file received into it
        if (checkDir(dir_name)==1){
            char* write_dest = malloc(_POSIX_PATH_MAX);
            strncpy(write_dest,dir_name,_POSIX_PATH_MAX);
            strncat(write_dest,"/",_POSIX_PATH_MAX);
            strncat(write_dest,file_name,_POSIX_PATH_MAX);
            int e_file = open( write_dest, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);
            //errno is set by open and write if they fail
            if (e_file == -1){
                perror("file open");
                return -1;
            }
            if (write(e_file,file_contents,file_s) == -1){
                perror("file write");
                return -1;
            }
            free(write_dest);
        }
        free(file_name);
        free(file_contents);
    }
    return 0;
}

int removeFile(const char* file_name){
    char* abspath;
    size_t file_len;
    int path_len;
    if ( setFileData(file_name, NULL, NULL, &abspath, &path_len) == -1){
        errno = EIO;
        return -1;
    }
    int func = e_fileDelete;
    sendMetadata(&func,&path_len, abspath);
    pid_t pid_self = getpid();
    sendToSocket(&pid_self,sizeof(pid_t));
    int req_err;
    readNB(fd_st,&req_err,sizeof(req_err));
    free(abspath);
    if (req_err){
        if (ENABLE_PRINTS)
            printf("deletion of file -%s- failed, error code = %d\n",file_name, req_err);
        errno = req_err;
        return -1;
    }
    return 0;
}
//sets abspath based on the relative file_path input and calculates its path_len,
//if inp_file and file_len are not NULL (write request) a file descriptor is opened for file_path and placed into inp_file
//and its size is placed into file_len
int setFileData(const char* file_path, int* inp_file,size_t* file_len,char** abspath,int* path_len){
    // finds the absolute path thath corresponds to the relative path given as argument
    char* getpath = NULL;
    getpath = realpath(file_path, NULL);
    if (getpath == NULL){
        printf("error getting full path of -%s-\n",file_path);
        return -1;
    }
    else{
        *abspath = getpath;
    }
    // writes the length of absolute path into path_len ( terminating \0 included)
    *path_len = strnlen(getpath,_POSIX_PATH_MAX)+1;
    if (*path_len >= _POSIX_PATH_MAX)
        return -1;
    //A write request will also need to open a file descriptor to read what it needs to send
    //and the size of the file has to be calculated
    if (inp_file != NULL && file_len != NULL){
        *inp_file = open(*abspath,O_RDONLY);
        if (*inp_file == -1){
            return -1;
        }
        *file_len = fileGetSize(*inp_file);
    }
    return 0;
}


int appendToFile(const char* path_name, void* buf, size_t size, const char* dir_name){
    char* abspath;
    int path_len;
    if ( setFileData(path_name, NULL , NULL , &abspath, &path_len) == -1){
        errno = EIO;
        return -1;
    }
    int func = e_fileAppend;
    pid_t pid_self = getpid();
    sendMetadata(&func,&path_len, abspath);
    sendToSocket(&pid_self,sizeof(pid_self));
    sendToSocket(&size,sizeof(size_t));
    int req_err;
    readNB(fd_st,&req_err,sizeof(req_err));
    int response;
    if (req_err == 0){
        sendToSocket(buf, size);
        readNB(fd_st,&response,sizeof(response));
    }
    else{
        errno = req_err;
        return -1;
    }
    free(abspath);
    return response;
}

//sends func and path and associated path_length to server
// if the request cannot be sent (invalid values) the function returns -1
int sendRequest(int func, const char* path){
    int response = 0;
    char* abspath = NULL;
    size_t file_len;
    int path_len;
    if (path != NULL){
        if ( setFileData(path, NULL, NULL, &abspath, &path_len) == -1){
            return -1;
        }
    }
    // tell the server the file and associated request the client needs
    sendMetadata(&func,&path_len, abspath);
    //in case of a request tied to a file, hear from the server if it could not be found
    if (path != NULL){
        readNB(fd_st, &response, sizeof(response));
    }
    free(abspath);
    return response;
}



//returns 1 if dir_name is a valid existing directory
int  checkDir(const char* dir_name){
    if (dir_name == NULL)
        return 0;
    struct stat info;
    if (stat(dir_name,&info) == -1){
        fprintf(stderr,"error loading dir %s\n",dir_name);
        return 0;
    }
    if (S_ISDIR(info.st_mode)){
        return 1;
    }
    else{
        if (ENABLE_PRINTS)
            printf(" %s is not a valid directory!\n",dir_name);
    }
    return 0;
}
//takes a relative path of a file and returns only its name as a dynamically allocated string
//example: /doc/somefold/file --> file
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
