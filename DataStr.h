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


#endif
