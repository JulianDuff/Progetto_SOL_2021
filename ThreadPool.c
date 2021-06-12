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


// inizializzazione della coda contenente lavoro(inizialmente vuota), 
// del mutex e delle condizioni per l'accesso ad essa
int threadPoolInit(threadPool* pool,int* pipe){
    pool->queue = NULL;
    pool->stop = 0;
    pool->queue_tail = NULL;
    if (pthread_mutex_init(&(pool->mutex),NULL) != 0){
        fprintf(stderr,"Error initializing pool mutex!\n");
        return 1;
    }
    if (pthread_cond_init(&(pool->queueHasWork),NULL)){
        fprintf(stderr,"Error initializing pool mutex condition!\n");
        return 2;
    }
    if (pthread_cond_init(&(pool->queueIsEmpty),NULL)){
        fprintf(stderr,"Error initializing pool mutex condition!\n");
        return 2;
    }
    printf("threadPool init successful\n");
    return 0;
}

int threadPoolAdd(threadPool* pool, thread_func func, void* args){
    printf("threadPool add called\n");
    pthread_mutex_lock(&(pool->mutex));
    queueAdd(&(pool->queue), &(pool->queue_tail), func,args);
    pthread_cond_signal(&(pool->queueHasWork));
    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}

int threadPoolDestroy(threadPool* pool ){
    //Add function to destroy queue later
    queue* clnup_ptr = pool->queue;
    while (pool->queue != NULL){
        pool->queue = pool->queue->next;    
        free(clnup_ptr);
        clnup_ptr = pool->queue;
    }
    pthread_mutex_destroy(&(pool->mutex));
    pthread_cond_destroy(&(pool->queueHasWork));
    pthread_cond_destroy(&(pool->queueIsEmpty));
    free(pool);
    return 0;
}

// la funzione memorizza in input_req la richiesta in cima alla coda di lavoro, 
// usa mutex per prevenire la corruzzione dei dati, restituisce func
// NULL se la coda e' vuota 
int PoolTakeTask(pool_request* input_req, threadPool* pool){
    printf("queue head attempt take\n");
    pthread_mutex_lock(&(pool->mutex));
    pthread_cond_wait(&(pool->queueHasWork),&(pool->mutex));
    //is the server closing the threadpool
    if (pool->stop){
        pthread_mutex_unlock(&(pool->mutex));
        pthread_exit(NULL);
    }
    //take a task from the task queue, 
    //receive func NULL if it was empty (spurious wakeup)
    queueTakeHead(input_req, &(pool->queue), &(pool->queue_tail));
    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}

int makeWorkerThreads(pthread_t** workers,const int n, threadPool* pool){
    if ((*workers = malloc(sizeof(pthread_t) * n)) == NULL){
        printf("Thread malloc error!\n");
        return -1;
    }
    int i;
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

