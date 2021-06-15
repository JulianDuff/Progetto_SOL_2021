#define CHUNK_SIZE (1024 * 5)
#include <stdlib.h>

typedef struct{
    int func;
    int path_size;
    size_t file_size;
}SockMsg;

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
int removeFile(int socket,char* file_p,void* server);
int sendToSocket(const void* msg,const int len);
