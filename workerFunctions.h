#if !defined(WORKER_FUNCTIONS_H)
#define WORKER_FUNCTIONS_H

#include "API.h"
#include "FileMemory.h"
#include "ThreadPool.h"
#include "config.h"

void* fileRead(void* args);
void* fileNRead(void* args);
void* fileWrite(void* args);
void* fileAppend(void* args);
void* fileOpen(void* args);
void* fileClose(void* args);
void* fileDelete(void* args);
void* fileLock(void* args);
void* fileUnlock(void* args);
void* fileSearch(void* args);
int  fileOpenCheck(int fd, MemFile** filePtr);
void* fileInit(void* args);

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

int  fileOpenCheck(int fd, MemFile** filePtr);
int  fileSearchSilent(int fd,MemFile** filePtr);
int sendFile(int fd, MemFile* file);
char* fileShortenName(char* file_name);
int clientPidDelete(MemFile* file, pid_t client_pid);
char** arrayRandomPermutation(char**,int n);
#endif
