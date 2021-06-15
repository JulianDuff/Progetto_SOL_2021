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
#include <fcntl.h>
#include <signal.h>

#include "DataStr.h"

#define WORKER_NUMBER 4

typedef struct {
    queue* queue_tail;
    queue* queue;
    pthread_mutex_t mutex;
    pthread_cond_t queueHasWork;
    int queueIsEmpty;
    int stop;
} threadPool;

int threadPoolInit(threadPool*, int*);
int threadPoolAdd(threadPool*, thread_func, void* );
int threadPoolDestroy(threadPool*);
int makeWorkerThreads(pthread_t**,const int,threadPool* );
void* workerStartup(void*);
int workersDestroy(pthread_t*,int);
ReqReadStruct* makeWorkArgs(int,int,void*, FdStruct*);
int PoolTakeTask(pool_request* input_req, threadPool* pool);

#endif
