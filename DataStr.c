#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef void (*thread_func) (void* args);

struct _queue{
    thread_func func;
    void* args;
    struct _queue* next;
};
typedef struct _queue queue;

typedef struct {
    queue* queue;
    queue* queue_tail;
    pthread_mutex_t mutex;
    pthread_cond_t queueIsEmpty;
    pthread_cond_t queueHasWork;
    int stop;
} threadPool;
typedef struct{
    thread_func func;
    void* args;
} pool_request;

struct _ClientSock{
    int fd;
    struct _ClientSock* next; 
};
typedef struct _ClientSock ClientSockets;

typedef struct {
    queue* q;
} thread_parameters;


void queueAdd(threadPool*, thread_func,void*);
int queueCheck(queue**,void* element);
int queueTakeHead(pool_request*, threadPool*);
int threadPoolInit(threadPool*);
int threadPoolAdd(threadPool* , thread_func , void* );
void clientAdd(ClientSockets**,int);
void clientRead(ClientSockets*);
void testFunc(void*);
void testFunc2(void*);

void queueAdd(threadPool* pool,thread_func n_func, void* n_args){
    printf("queue attempt add\n");
    queue* new = malloc(sizeof(queue));
    new->next = NULL;
    new->args= n_args;
    new->func = n_func;
    if (pool->queue == NULL){
        pool->queue = new;
        pool->queue_tail = new;
    }else{
    pool->queue_tail->next = new; 
    pool->queue_tail = (pool->queue_tail)->next;
    }
}

// inizializzazione della coda contenente lavoro(inizialmente vuota), 
// del mutex e delle condizioni per l'accesso ad essa
int threadPoolInit(threadPool* pool){
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
    printf("thread Pool add called\n");
    pthread_mutex_lock(&(pool->mutex));
    queueAdd(pool,func,args);
    pthread_cond_signal(&(pool->queueHasWork));
    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}

void clientAdd(ClientSockets** Clients,int new_fd){
    ClientSockets* new = malloc(sizeof(ClientSockets));
    new->next = NULL;
    new->fd= new_fd;
    if (*Clients == NULL){
        *Clients = new;
    }else{
    ClientSockets* Clients_aux = *Clients;
    while(Clients_aux->next != NULL){
        Clients_aux = Clients_aux->next;
        }
    Clients_aux->next = new;
    }
}
//temp func
void clientRead(ClientSockets* Clients){
    int i=0;
    while(Clients != NULL){
        fprintf(stdout,"client %d fd is %d\n",i,Clients->fd);
        Clients = Clients->next;
        i++;
    }
}
int queueCheck(queue **q,void* element){
    //TODO: implement
    return 0;
}
// la funzione memorizza in input_req la richiesta in cima alla coda di lavoro, 
// usa mutex per prevenire la corruzzione dei dati, restituisce func
// NULL se la coda e' vuota 
int queueTakeHead(pool_request* input_req, threadPool* pool){
    printf("queue head attempt take\n");
    pthread_mutex_lock(&(pool->mutex));
    while(pool->queue == NULL){
        pthread_cond_wait(&(pool->queueHasWork),&(pool->mutex));
    }
    if(pool->queue == NULL){
        input_req->func = NULL;
        input_req->args = NULL;
        printf("queue was empty!\n"); // temp debug
        pthread_mutex_unlock(&(pool->mutex));
        return 0;
    }
    //(pool->queue->func)(pool->queue->args);
    input_req->func = ((pool->queue)->func);
    input_req->args = ((pool->queue)->args);
    queue* q_aux = (pool->queue);
    // works so far
    pool->queue = (pool->queue)->next;
    if (q_aux != NULL){
        free(q_aux);
    }
    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}

void testFunc(void* arg){
    printf(" I am a function called from the threadpool!\n");
}
void testFunc2(void* arg){
    int x = *(int*) arg;
    printf(" the square of %d is %d",x,x*x);
}
