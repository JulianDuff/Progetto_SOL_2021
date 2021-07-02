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

#include "API.h"
#include "FileMemory.h"
#include "config.h"

typedef void* (*thread_func) (void* args);


struct _queue{
    thread_func func;
    void* args;
    struct _queue* next;
};
typedef struct _queue queue;


typedef struct{
    thread_func func;
    void* args;
} pool_request;

typedef struct {
    queue* queue_tail;
    queue* queue;
    pthread_mutex_t mutex;
    pthread_cond_t queueHasWork;
} threadPool;

extern thread_func ReqFunArr[numberOfFunctions];
int threadPoolInit(threadPool*, int*);
int threadPoolAdd(threadPool*, thread_func, void* );
int threadPoolDestroy(threadPool*);
int makeWorkerThreads(pthread_t**,const int,threadPool* );
void* workerStartup(void*);
int workersDestroy(pthread_t*,int);
ReqReadStruct* makeWorkArgs(int,int,void*, FdStruct*);
int PoolTakeTask(pool_request* input_req, threadPool* pool);
void* ThreadRequestExit(void* args);

void queueAdd(queue**, queue**, thread_func,void*);
int queueTakeHead(pool_request*,queue**,queue**);
#endif
