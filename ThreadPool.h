#if !defined(THREADPOOL_H)
#define THREADPOOL_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>
#include <search.h>
#include <string.h>
#include <pthread.h>
#include <sys/select.h>
#include <signal.h>
#include <time.h>

#include "DataStr.h"
#include "API.h"

#define WORKER_NUMBER 4

typedef struct {
    queue* queue_tail;
    queue* queue;
    pthread_mutex_t mutex;
    pthread_cond_t queueHasWork;
} threadPool;

extern thread_func ReqFunArr[numberOfFunctions];
void** FuncArrFill();
int threadPoolInit(threadPool*, int*);
int threadPoolAdd(threadPool*, thread_func, void* );
int threadPoolDestroy(threadPool*);
int makeWorkerThreads(pthread_t**,const int,threadPool* );
void* workerStartup(void*);
int workersDestroy(pthread_t*,int);
ReqReadStruct* makeWorkArgs(int,int,void*, FdStruct*);
int PoolTakeTask(pool_request* input_req, threadPool* pool);
void ThreadRequestExit(void* args);

void fileRead(void* args);
void fileNRead(void* args);
void fileWrite(void* args);
void fileAppend(void* args);
void fileOpen(void* args);
void fileClose(void* args);
void fileDelete(void* args);
void fileLock(void* args);
void fileUnlock(void* args);

#endif
