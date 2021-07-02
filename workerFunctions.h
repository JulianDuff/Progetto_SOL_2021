#if !defined(WORKER_FUNCTIONS_H)
#define WORKER_FUNCTIONS_H

#include "DataStr.h"
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
void* fileOpenCheck(void* args);
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

int sendFile(int fd, MemFile* file);
char* fileShortenName(char* file_name);
#endif
