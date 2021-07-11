#include "ThreadPool.h"

int threadPoolInit(threadPool* pool,int* pipe){
    pool->queue = NULL;
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

//function used to add a function (with a single void arg and void* return value) to the threadpool task queue
int threadPoolAdd(threadPool* pool, thread_func func, void* args){
    // locking the threadpool to prevent conflict with thread workers
    pthread_mutex_lock(&(pool->mutex));
    queueAdd(&(pool->queue), &(pool->queue_tail), func,args);
    pthread_mutex_unlock(&(pool->mutex));
    // workers must be signaled that the queue now has a task
    pthread_cond_signal(&(pool->queueHasWork));
    return 0;
}


void threadPoolClear(threadPool* worker_pool){
    pool_request to_del;
    pthread_mutex_lock(&(worker_pool->mutex));
    while (queueTakeHead(&to_del,&worker_pool->queue,&worker_pool->queue_tail) != 1){
        if (to_del.args != NULL)
            free(to_del.args);
    }
    pthread_mutex_unlock(&(worker_pool->mutex));
}

// function is only called after the worker threads have exited, so no mutex is used
int threadPoolDestroy(threadPool* pool ){
    //clears the queue in case tasks were left
    queue* clnup_ptr = pool->queue;
    while (pool->queue != NULL){
        pool->queue = pool->queue->next;    
        free(clnup_ptr);
        clnup_ptr = pool->queue;
    }
    pthread_mutex_destroy(&(pool->mutex));
    pthread_cond_destroy(&(pool->queueHasWork));
    return 0;
}

//function used by worker threads to receive a task
int PoolTakeTask(pool_request* input_req, threadPool* pool){
    //how much time a worker waits before checking the pool contents
    //(if queueHasWork was signaled it checks it immediately)
    struct timespec wait_max;
    clock_gettime(CLOCK_REALTIME, &wait_max);
    wait_max.tv_nsec += 50000;
    //only one thread can use the pool at once
    pthread_mutex_lock(&(pool->mutex));
    //if there is no task, wait until wait_max has passed
    pthread_cond_timedwait(&(pool->queueHasWork),&(pool->mutex),&wait_max);
    //take a task from the task queue, 
    //receive NULL if it was empty
    queueTakeHead(input_req, &(pool->queue), &(pool->queue_tail));
    pthread_mutex_unlock(&(pool->mutex));
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
        if (pthread_create(( &(*workers)[i]), NULL, poolWorker, pool) != 0){
            printf("Error occurred while initializing thread %d\n",i);
            return -1;
        }
    }
    return 0;
}

void* poolWorker(void* pool){
    threadPool* th_pool = (threadPool*) pool;
    pool_request request;
    request.args = NULL;
    request.func = NULL;
    while(1){
        //take a task from the pool, execute function if not NULL and free args if not NULL
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

// function is added to the pool queue to signal to threads that they need to exit
void* ThreadRequestExit(void* args){
    printf("Thread has  quit!\n");
    threadPool* pool = (threadPool*) args;
    threadPoolAdd(pool, ThreadRequestExit, pool);
    pthread_exit(NULL);
}


void queueAdd(queue** head, queue** tail, thread_func n_func, void* n_args){
    queue* new = malloc(sizeof(queue));
    new->next = NULL;
    new->args= n_args;
    new->func = n_func;
    if (*head == NULL){
        *head = new;
        *tail = new;
    }else{
        (*tail)->next = new; 
        (*tail) = (*tail)->next;
    }
}


// take the first request from the queue and place it in input_req (NULL if nothing was found)
int queueTakeHead(pool_request* input_req,queue** head, queue** tail){
    if( *head == NULL){
        // no request was in the queue
        input_req->func = NULL;
        input_req->args = NULL;
        return 1;
    }
    else{
        //there is a request, pass it to the calling thread through input_req
        input_req->func = (*head)->func;
        input_req->args = (*head)->args; 
        //advance the queue and free the previous head pointer
        queue* q_aux = (*head);
        *head = (*head)->next;
        free(q_aux);
        if (*head == NULL){
            //if head is null then the queue is empty
            //and tail is pointing to freed memory,
            //it needs to be updated
            *tail = *head;
        }
    }
    return 0;
}

