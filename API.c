#include "API.h"

#define MAX_PATH 108
const int O_CREATE = 0x0001;
const int O_LOCK   = 0x0002;
int ENABLE_PRINTS;

int fd_st = NOT_SET; // variabile globale per comunicare con il server 
char* client_socket_name; //nome del socket passato con -f

int openConnection(const char* sock_name, int msec, const struct timespec abstime){ // TODO: implement abstime
    //if connection has already been established do nothing and return 1
    if (fd_st == NOT_SET){
      int fd_socket = socket(AF_UNIX,SOCK_STREAM, 0);
      struct sockaddr_un sock_addr;
      int str_l = strnlen(sock_name,MAX_PATH);
      strncpy(sock_addr.sun_path, sock_name, str_l+1);
      sock_addr.sun_family = AF_UNIX;
      while ( connect(fd_socket, (struct sockaddr*)&sock_addr, SUN_LEN(&sock_addr)) != 0){ // connection to socket is attempted every msec until it succeeds
          sleep(msec/1000);
      }
      if (ENABLE_PRINTS)
          printf("Connection success!\n");
      //saving the socket fd for later use into fd_st variable 
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
    if(strncmp(client_socket_name,sock_name,MAX_PATH) == 0){ //controllo che il socket passato sia attualmente in uso
        close(fd_st);
      if (ENABLE_PRINTS)
        printf(" Connection %s closed succesfully.\n",sock_name); 
        free(client_socket_name);
        return 0;
    }
    else {
        printf("ERROR: no such socket exists/ socket not opened\n");
        return -1;
    }
}


int openFile(const char* path_name, int flags){
    if (flags & O_LOCK){
        //nothing for now
    }
    if (flags & O_CREATE){
        //if file does not exists in server request to initialize it, otherwise abort open and return -1
        if (!sendRequest(e_fileSearch,path_name)){
            sendRequest(e_fileInit,path_name);
        }
        else{
            return -1;
        }
    }
    sendRequest(e_fileOpen,path_name);
    return 0;
}
int closeFile(const char* pathname){

    return 0;
}
int sendToSocket(void* buff, const int len){ //se  una connessione e' aperta al server viene passato msg, altrimenti restituisce un errore
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
    if ( setFileData(path_name, &inp_file, &file_len, &abspath, &path_len) == -1){
        return -1;
    }
    int func = e_fileWrite;
    sendMetadata(&func,&path_len, abspath);
    char* out_buff = NULL;
    if ((out_buff = malloc(file_len)) == NULL){
        printf("receiving file malloc error!");
        free(abspath);
        return -1;
    }
    int response = 0;
    int fileExists;
    int fileIsOpened;
    int fileAlreadyWritten;
    readNB(fd_st,&fileExists,sizeof(fileExists));
    readNB(fd_st,&fileIsOpened,sizeof(fileIsOpened));
    readNB(fd_st,&fileAlreadyWritten,sizeof(fileAlreadyWritten));
    if (fileExists && fileIsOpened && !fileAlreadyWritten){
        sendToSocket(&file_len,sizeof(size_t));
        int fileTooLarge;
        readNB(fd_st,&fileTooLarge,sizeof(fileTooLarge));
        if (!fileTooLarge){
          if (ENABLE_PRINTS)
                printf("writing file %s of size %zu \n",path_name,file_len);
            readNB(inp_file, out_buff, file_len);
            sendToSocket(out_buff, file_len);
            readNB(fd_st,&response,sizeof(response));
        }
    }
    else{
      if (ENABLE_PRINTS)
        printf("file write refused\n");
    }
    free(abspath);
    free(out_buff);
    close(inp_file);
    return response;
}

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
        return -1;
    }
    // what service should I ask the server to complete
    int func = e_fileRead;
    // tell the server the file and associated request the client needs
    sendMetadata(&func,&path_len, abspath);

    int file_exists;
    int file_opened;
    //The server will respond with 1 if it found the file, 0 otherwise
    readNB(fd_st, &file_exists, sizeof(file_exists));
    readNB(fd_st, &file_opened, sizeof(file_exists));
    if (file_exists && file_opened){
        // file was found
        char* out_buff = NULL;
        readNB(fd_st, &file_len, sizeof(size_t));
        if ((out_buff = malloc(file_len) )== NULL){
            fprintf(stderr,"Malloc file read error!\n");
            ret_val = -1;
        }
        else{
            readNB(fd_st, out_buff, file_len);
          if (ENABLE_PRINTS)
                printf("I received file:\n");
            fflush(stdout);
            write(1, out_buff, file_len);
            free(out_buff);
        }
    }
    else {
        // file was not found
      if (ENABLE_PRINTS)
            printf("File not found or not opened!\n");
        ret_val = -1;
    }
    free(abspath);
    return ret_val;
}

int readNFiles(int N, const char* dir_name){
    int buff = N;
    sendRequest(e_fileNRead,NULL);
    //tell the server how many files the client wants to read
    sendToSocket( &buff, sizeof(int));
    size_t file_s = 0;
    int file_name_size;
    //Read from socket until file_name_size sent is 0 (no more files)
    while(readNB(fd_st,&file_name_size,sizeof(int))> 0){
        if (file_name_size == 0)
            break;
        // get file name,size and contents from socket
        char* file_name = malloc(file_name_size);
        readNB(fd_st,file_name,file_name_size);
        readNB(fd_st,&file_s,sizeof(size_t));
        char* file_contents= malloc(file_s);
        readNB(fd_st, file_contents, file_s);
        /////////////////////////////////////////////
        printf("received file of size %zu\n",file_s);

        if (dir_name != NULL){
            char* write_dest = malloc(_POSIX_PATH_MAX);
            strncpy(write_dest,dir_name,_POSIX_PATH_MAX);
            strncat(write_dest,"/",_POSIX_PATH_MAX);
            strncat(write_dest,file_name,_POSIX_PATH_MAX);
            int e_file = open( write_dest, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);
            if (e_file == -1){
                perror("file open");
                return -1;
            }
            if (write(e_file,file_contents,file_s) == -1){
                perror("file write");
                return -1;
            }
        }
        free(file_contents);
    }
    return 0;
}

int removeFile(const char* file_name){
    char* abspath;
    size_t file_len;
    int path_len;
    if ( setFileData(file_name, NULL, NULL, &abspath, &path_len) == -1){
        return -1;
    }
    int func = e_fileDelete;
    sendMetadata(&func,&path_len, abspath);
    free(abspath);
    return 0;
}
//sets abspath based on the relative file_path input and calculates its path_len,
//if inp_file and file_len are not NULL(write request) a file descriptor is opened for file_path
//and its size is placed into file_len
int setFileData(const char* file_path, int* inp_file,size_t* file_len,char** abspath,int* path_len){
    // finds the absolute path thath corresponds to the relative path given as argument
     *abspath = realpath(file_path, NULL);
    if (*abspath == NULL){
        perror("");
        return -1;
    }
    // writes the length of absolute path into path_len ( terminating \0 included)
    *path_len = strnlen(*abspath,MAX_PATH)+1;
    //A write request will also need to open a file descriptor to read what it needs to send
    //and the size of the file has to be calculated
    if (inp_file != NULL && file_len != NULL){
        *inp_file = open(*abspath,O_RDONLY);
        if (*inp_file == -1){
            perror("open file");
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
        return -1;
    }
    int func = e_fileAppend;
    sendMetadata(&func,&path_len, abspath);
    int response = 0;
    int fileExists;
    int fileIsOpened;
    int fileAlreadyWritten;
    readNB(fd_st,&fileExists,sizeof(fileExists));
    readNB(fd_st,&fileIsOpened,sizeof(fileIsOpened));
    //readNB(fd_st,&fileAlreadyWritten,sizeof(fileAlreadyWritten));
    if (fileExists && fileIsOpened){
        sendToSocket(&size,sizeof(size_t));
        int fileTooLarge;
        readNB(fd_st,&fileTooLarge,sizeof(fileTooLarge));
        if (!fileTooLarge){
            sendToSocket(buf, size);
            readNB(fd_st,&response,sizeof(response));
        }
    }
    free(abspath);
    return response;
}
int sendRequest(int func, const char* path){
    int response = 0;
    char* abspath = NULL;
    size_t file_len;
    int path_len;
    if (path != NULL){
        if ( setFileData(path, NULL, NULL, &abspath, &path_len) == -1){
            return response;
        }
    }
    // what service should I ask the server to complete
    // tell the server the file and associated request the client needs
    sendMetadata(&func,&path_len, abspath);
    //The server will respond with 1 if it found the file, 0 otherwise
    if (path != NULL){
        readNB(fd_st, &response, sizeof(response));
    }
    return response;
}
