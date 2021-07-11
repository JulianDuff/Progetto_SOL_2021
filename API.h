#if !defined(API_H)
#define API_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <search.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>

#define CHUNK_SIZE (1024 * 5)
#define NOT_SET -1

extern const int O_CREATE;
extern const int O_LOCK  ;
extern int ENABLE_PRINTS;

typedef struct{
    int func;
    int path_size;
    size_t file_size;
}SockMsg;

enum ReqFunctions{
    e_fileRead,
    e_fileNRead,
    e_fileWrite,
    e_fileAppend,
    e_fileOpen,
    e_fileClose,
    e_fileDelete,
    e_fileLock,
    e_fileUnlock,
    e_fileSearch,
    e_fileInit,
    numberOfFunctions
};

int openConnection(const char* sock_name, int msec, const struct timespec abstime);
int closeConnection(const char* sock_name);
int openFile(const char* path_name, int flags);
int readFile(const char* path_name, void **buf, size_t* size);
int readNFiles(int N, const char* dir_name); //OPTIONAL
int writeFile(const char* path_name, const char* dir_name);
int appendToFile(const char* path_name, void* buf, size_t size, const char* dir_name);
int lockFile(const char* path_name);
int unlockFile(const char* path_name);
int closeFile(const char* path_name);
int removeFile(const char* file_name);
int sendToSocket(void* msg,const int len);
int appendToFile(const char* path_name, void* buf, size_t size, const char* dir_name);
int sendRequest(int func, const char* path);


size_t fileGetSize(int file );
int sendMetadata(const int* func, const  int* path_len, const char* path);
int writeNB(const int fd, void* data,const size_t size);
size_t readNB(const int fd, void* buff,const size_t n);
int setFileData(const char* file_path, int* inp_file,size_t* file_len,char** abspath,int* path_len);
int checkDir(const char* dir_name);
char* fileShortenName(char* file_name);
#endif
