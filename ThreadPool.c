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
#include "ThreadPool.h"


int threadPoolInit(threadPool* pool,int* pipe){
    pool->queue = NULL;
    pool->stop = 0;
    pool->queueIsEmpty = 0;
    pool->queue_tail = NULL;
    if (pthread_mutex_init(&(pool->mutex),NULL) != 0){
        fprintf(stderr,"Error initializing pool mutex!\n");
        return -1;
    }
    if (pthread_cond_init(&(pool->queueHasWork),NULL)){
        fprintf(stderr,"Error initializing pool mutex condition!\n");
        return -1;
    }
    printf("threadPool init successful\n");
    return 0;
}
//function to add a function and its arguments to the threadpool task queue
//(used by the server's main thread)
int threadPoolAdd(threadPool* pool, thread_func func, void* args){
    printf("threadPool add called\n");
    // locking the threadpool to prevent conflict with thread workers
    pthread_mutex_lock(&(pool->mutex));
    queueAdd(&(pool->queue), &(pool->queue_tail), func,args);
    // workers must be signaled that the queue now has a task
    pthread_cond_signal(&(pool->queueHasWork));
    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}

int threadPoolDestroy(threadPool* pool ){
    queue* clnup_ptr = pool->queue;
    while (pool->queue != NULL){
        pool->queue = pool->queue->next;    
        free(clnup_ptr);
        clnup_ptr = pool->queue;
    }
    pthread_mutex_destroy(&(pool->mutex));
    pthread_cond_destroy(&(pool->queueHasWork));
    free(pool);
    return 0;
}

//function used by worker threads to receive a task
int PoolTakeTask(pool_request* input_req, threadPool* pool){
    //only one thread can use the pool at once
    pthread_mutex_lock(&(pool->mutex));
    if(pool->stop && pool->queueIsEmpty){
        pthread_mutex_unlock(&(pool->mutex));
        pthread_exit(NULL);
    }
    else{
        printf("queue head attempt take\n");
        //if there is no task, wait
        pthread_cond_wait(&(pool->queueHasWork),&(pool->mutex));
        //take a task from the task queue, 
        //receive func NULL if it was empty (spurious wakeup)
        queueTakeHead(input_req, &(pool->queue), &(pool->queue_tail));
        //pool had no request, 
        if (input_req->func == NULL)
            pool->queueIsEmpty = 1;
        else
            pool->queueIsEmpty = 0;
        pthread_mutex_unlock(&(pool->mutex));
    }
    return 0;
}

int makeWorkerThreads(pthread_t** workers,const int n, threadPool* pool){
    //allocate memory to hold n pthread_t in *workers
    if ((*workers = malloc(sizeof(pthread_t) * n)) == NULL){
        printf("Thread malloc error!\n");
        return -1;
    }
    int i;
    //create n worker threads
    for (i=0; i<n; i++){
        if (pthread_create(( &(*workers)[i]), NULL, workerStartup, pool) != 0){
            printf("Error occurred while initializing thread %d\n",i);
            return -1;
        }
    }
    return 0;
}

void* workerStartup(void* pool){
    threadPool* th_pool = (threadPool*) pool;
    pool_request request;
    request.args = NULL;
    request.func = NULL;
    while(1){
        PoolTakeTask(&request, th_pool);
        if (request.func != NULL){
            (request.func)(request.args);
        }
        if (request.args != NULL){
            free(request.args);
        }
    }
    return 0;
}

ReqReadStruct* makeWorkArgs(int fd, int pipe, void* mem, FdStruct* fd_struct){
    ReqReadStruct* new_struct = malloc(sizeof(ReqReadStruct));
    new_struct->fd = fd;
    new_struct->pipe= pipe;
    new_struct->mem = mem ;
    new_struct->set = fd_struct->set;
    return new_struct;
}

int workersDestroy(pthread_t* wrkArr , int size){
    int i;
    for(i=0; i<size; i++){
        pthread_join(wrkArr[i], NULL);
    }
    free(wrkArr);
    return 0;
}

