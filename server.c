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
#include "API.h"
#include "DataStr.c"

#define MEMORY_SIZE (1024 * 1024)
#define PAGE_SIZE (1024*8)
#define WORKER_NUMBER 10
#define START_ARRAY_N 10

int serverStartup(void**,int);
int makeWorkerThreads(pthread_t**,const int, const queue*);
void* workerStartup(void*);

int main (int argc, char* argv[]){

    void* file_memory;                                      // Allocazione memoria file server
    serverStartup(&file_memory,MEMORY_SIZE);
    pthread_t*  worker_threads; // array che contiene i thread id dei workers
    queue* worker_queue = NULL;
    makeWorkerThreads(&worker_threads,WORKER_NUMBER, worker_queue);
    char* socket_name = argv[1];                            //TODO: use config.txt Setup del socket di comunicazione server-client
    int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);        // Preparazione socket
    struct sockaddr_un sock_addr;
    strncpy(sock_addr.sun_path,socket_name,108);
    sock_addr.sun_family =AF_UNIX;
    bind(socket_fd, (struct sockaddr*) &sock_addr, SUN_LEN(&sock_addr)); 
    listen(socket_fd, 5);
    ClientSockets* fd_Clients = NULL; // lista di fd
    while(1){
        int acpt_socket;
        acpt_socket = accept(socket_fd, NULL,0); // il nuovo socket viene passato ad una lista contenente gli fd di tutti i client
        printf("accepted!\n");
        clientAdd(&fd_Clients,acpt_socket);
        clientRead(fd_Clients);
    }
        char buff[20];
    /*read(acpt_socket, buff,25); // ricezione messaggio dal client
    printf(" I just got:\n %s\n",buff);
    int temp;           // temporary block to test client func
    scanf("%d",&temp);
    */
    free (file_memory);     //cleanup finale
    close(socket_fd);
    unlink(socket_name);
}

int serverStartup(void** server, int size){
    if ( (*server = malloc(size)) == NULL)
        return -1;
    return 0;
}

int makeWorkerThreads(pthread_t** workers,const int n, const queue* worker_queue){
    if ((*workers = malloc(sizeof(pthread_t) * n)) == NULL){
        printf("Thread malloc error!\n");
        return -1;
    }
    int i;
    for (i=0; i<n; i++){
        if (pthread_create(( &(*workers)[i]), NULL, &workerStartup, &worker_queue) != 0){
            printf("Error occurred while initializing thread %d\n",i);
            return -1;
        }
    }
    return 0;
}

void* workerStartup(void* queue){
    return 0;
}










