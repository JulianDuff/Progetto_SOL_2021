#include <stdio.h>
#include <stdlib.h>

struct _queue_arg{
        
};
typedef struct _queue_arg queue_arg;

struct _queue{
    queue_arg element;
    struct _queue* next;
};
typedef struct _queue queue;

struct _ClientSock{
    int fd;
    struct _ClientSock* next;
};
typedef struct _ClientSock ClientSockets;

typedef struct {
    queue* q;
} thread_parameters;

void clientAdd(ClientSockets**,int);
void clientRead(ClientSockets*);

void queueAdd(queue**,queue_arg element);
int queueCheck(queue**,queue_arg element);
int queueDeleteHead(queue**);

void queueAdd(queue** q,queue_arg element){
    queue* new = malloc(sizeof(queue));
    new->next = NULL;
    new->element= element;
    if (*q == NULL){
        *q = new;
    }else{
    queue* q_aux = *q;
    while(q_aux->next != NULL){
        q_aux = q_aux->next;
        }
    q_aux->next = new;
    }
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
void clientRead(ClientSockets* Clients){
    int i=0;
    while(Clients != NULL){
        fprintf(stdout,"client %d fd is %d\n",i,Clients->fd);
        Clients = Clients->next;
        i++;
    }
}
int queueCheck(queue **q,queue_arg element){
    //TODO: implement
    return 0;
}

int queueDeleteHead(queue** q){
    queue* q_aux = *q;
    *q = (*q)->next;
    free(q_aux);
    return 0;
}
