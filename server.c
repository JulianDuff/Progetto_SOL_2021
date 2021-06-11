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
#include "API.h"
#include <stdarg.h>
#include "DataStr.c"

#define MEMORY_SIZE (1024 * 1024 * 1024)
#define PAGE_SIZE (1024*8)
#define WORKER_NUMBER 4

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

int CheckForFdRequest(FdStruct*);
int serverStartup(void**,double);
int makeWorkerThreads(pthread_t**,const int,threadPool* );
void* workerStartup(void*);
void clientReadReq(void*);
int checkFdSets(fd_set*);
int fileAdd(void *,char*, double, int);
FdStruct* fdSetMake(int* fd,int n); 
int fdSetFree(FdStruct* );

int main (int argc, char* argv[]){

    void* file_memory;                                      // Allocazione memoria file server
    
    serverStartup(&file_memory,MEMORY_SIZE);
    // pipe usata dalla threadpool per comunicare 
    // gli fd che devono essere riascoltati dal server
    int pipefd[2];
    if (pipe(pipefd) == -1){
        perror("pipe");
        return -1;
    }
    pthread_t*  worker_threads; // array che contiene i thread id dei workers
    //Pool per distribuire le richieste dei client ai workers
    threadPool worker_pool;
    threadPoolInit(&worker_pool,pipefd);
    makeWorkerThreads(&worker_threads,WORKER_NUMBER, &worker_pool);

    //TODO: use config.txt 
    //Setup del socket di comunicazione server-client
    char* socket_name = argv[1];   
    //Preparazione socket di ascolto
    int listen_socket = socket(AF_UNIX, SOCK_STREAM, 0);        
    struct sockaddr_un sock_addr;
    strncpy(sock_addr.sun_path,socket_name,108);
    sock_addr.sun_family =AF_UNIX;
    bind(listen_socket, (struct sockaddr*) &sock_addr, SUN_LEN(&sock_addr)); 
    listen(listen_socket, 5);
    // massimo fd aperto
    // fd_set da usare con select()
    int fdArr[3];
    fdArr[0] = pipefd[0];
    fdArr[1] = pipefd[1];
    fdArr[2] = listen_socket;
    FdStruct* fd_struct = fdSetMake(fdArr,3);
    // MAIN LOOP
    while(1){
        //CheckForFdRequest(fd_struct,)
        fd_set tmp_fd_set = *(fd_struct->set);
        select(fd_struct->max+1, &tmp_fd_set,NULL,NULL,NULL);//add err check
        int i;
        for(i=0; i<=fd_struct->max; i++){
            if (FD_ISSET(i,&tmp_fd_set)){
                // Accettata connessione a nuovo client
                if (i==listen_socket){
                    int acpt_socket;
                    acpt_socket = accept(listen_socket, NULL,0); // il nuovo socket viene passato ad una lista contenente gli fd di tutti i client
                    FD_SET(acpt_socket, fd_struct->set);
                    if (acpt_socket > fd_struct->max){
                        fd_struct->max = acpt_socket;
                    }
                    printf("accepted!\n");
                    fflush(stdin);
                }
                else if(i == pipefd[0]){
                    char* rd_inp = malloc(sizeof(char));
                    if (read(i, rd_inp,sizeof(char*)) > 0){
                        int fd_received = *((int*)rd_inp);
                        printf(" fd sent to pipe was %d!\n",fd_received);
                        FD_SET(fd_received, fd_struct->set);
                        free(rd_inp);
                    }
                }
                // Un socket lettura e' pronto ad essere usato
                else{
                    // gli argomenti delle func nel thread pool
                    // vengono passati tramite un puntatore a struct (castato a void*)
                    // viene passato il pipe per comunicare al dispatcher thread
                    // quando riascoltare il client
                    ReqReadStruct* workargs = malloc(sizeof(ReqReadStruct));
                    //worker calls free when it is done with the task
                    workargs->fd = i;
                    workargs->pipe = pipefd[1];
                    workargs->mem = file_memory;
                    workargs->set = fd_struct->set;
                    FD_CLR(i,fd_struct->set);
                    threadPoolAdd(&worker_pool,&clientReadReq,(void*)workargs);
                }
            }
        } 
    }
        char buff[20];
    /*read(acpt_socket, buff,25); // ricezione messaggio dal client
    printf(" I just got:\n %s\n",buff);
    int temp;           // temporary block to test client func
    scanf("%d",&temp);
    */
    fdSetFree(fd_struct);
    free (file_memory);     //cleanup finale
    close(listen_socket);
    unlink(socket_name);
}

int serverStartup(void** server, double size){
    if ( (*server = malloc(size)) == NULL)
        return -1;
    return 0;
}

int makeWorkerThreads(pthread_t** workers,const int n, threadPool* pool){
    if ((*workers = malloc(sizeof(pthread_t) * n)) == NULL){
        printf("Thread malloc error!\n");
        return -1;
    }
    int i;
    for (i=0; i<n; i++){
        if (pthread_create(( &(*workers)[i]), NULL, &workerStartup, pool) != 0){
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
        queueTakeHead(&request, th_pool);
        if (request.func != NULL){
            (request.func)(request.args);
        }
        if (request.args != NULL){
            free(request.args);
        }
    }
    return 0;
}
// temporary simplified implementation
int fileAdd(void* mem ,char* inp_file, double len, int flag){
    printf("len is : %f\n",len);
    memcpy(mem,inp_file,len);
    printf("File from memory is:\n");
    write(1,mem,len);
    return 0;
}


void clientReadReq(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int pipe = req->pipe;
    void* mem_ptr = req->mem;
    int fd = req->fd;
    fd_set* fset = req->set;
    // first part of socket message(fixed size) is parsed
    SockMsg msg_rec;
    int func;
    int path_size;
    double file_size;
    char* file_path;
    int read_n = read(fd,&func,sizeof(int));
    if (read_n == 0){
        printf("client %d closed the connection!\n",fd);
        close(fd);
    }
    else{
        printf("func is %d\n",func);
        read(fd,&path_size,sizeof(int));
        printf("path_size is %d \n",path_size);
        file_path = malloc( (path_size) * sizeof(char));
        read(fd,file_path,path_size);
        printf("file path is %s\n",file_path);
        read(fd,&file_size,sizeof(double));
        printf(" file size is %f\n",file_size);
        char* MM_file = malloc(file_size);
        char read_buff[CHUNK_SIZE]; 
        int rd_bytes;
        double total_rd_bytes = 0;
        char* MM_file_ptr = MM_file;
        while (total_rd_bytes < file_size){
            rd_bytes = read(fd, read_buff, sizeof(read_buff));
            total_rd_bytes += rd_bytes;
            if (rd_bytes == 0){
                break;
            }
            if (rd_bytes < 0){
                perror("file read");
            }
            memcpy(MM_file_ptr,read_buff,rd_bytes);
            MM_file_ptr += rd_bytes;
        }
        fprintf(stdout,"client from %d attempting to call fileAdd\n",fd);
        fileAdd((char*)mem_ptr, MM_file, file_size, 0);
        char* message = (char*)&fd;
        write(pipe, message, sizeof(char*));
        fflush(stdout);
        free(file_path);
        free(MM_file);
    }
}



FdStruct* fdSetMake(int* fdArr,int n){
    int max = 0;
    fd_set* new_set = malloc(sizeof(fd_set));
    FD_ZERO(new_set);
    int i;
    for(i=0; i<n; i++){
        FD_SET( fdArr[i],new_set);
        if (max < fdArr[i]){
            max = fdArr[i];
        }
    }
    FdStruct* set_made = malloc(sizeof(FdStruct));
    set_made->max = max;
    set_made->set = new_set;
    return set_made;
}
int fdSetFree(FdStruct* setStruct){
        free(setStruct->set);
        free(setStruct);
        return 0;
}
int CheckForFdRequest(FdStruct* fdS){

    return 0;
}
