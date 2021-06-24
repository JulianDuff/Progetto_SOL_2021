#if !defined(DATA_STR_H)
#define DATA_STR_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>


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

typedef struct{
    fd_set* set;
    int max;
} FdStruct;

typedef struct{
    int pipe;
    int fd;
    void* mem;
    fd_set* set;
}ReqReadStruct;

void queueAdd(queue**, queue**, thread_func,void*);
int queueTakeHead(pool_request*,queue**,queue**);

#endif
