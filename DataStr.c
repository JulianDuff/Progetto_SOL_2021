#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "DataStr.h"


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


// la funzione memorizza in input_req la richiesta in cima alla coda di lavoro, 
// NULL se la coda e' vuota 
int queueTakeHead(pool_request* input_req,queue** head, queue** tail){
    printf("queue head attempt take\n");
    if( *head == NULL){
        // no request was in the queue
        input_req->func = NULL;
        input_req->args = NULL;
        printf("queue was empty!\n"); 
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

