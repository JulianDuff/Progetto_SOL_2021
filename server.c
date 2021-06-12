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

#include "API.h"
#include "DataStr.h" 
#include "ThreadPool.h" 

#define MEMORY_SIZE (1024 * 1024 * 50)
#define PAGE_SIZE (1024*8)


typedef struct{
    sigset_t* set;
    int pipe;
} SignalThreadArgs;


int acceptConnection(int, FdStruct*);
int serverStartup(void**,double);
int checkFdSets(fd_set*);
int checkPipeForFd(int ,FdStruct*);
int CheckForFdRequest(FdStruct*);
int fileAdd(void *,char*, double, int);
FdStruct* fdSetMake(int* fd,int n); 
int fdSetFree(FdStruct* );
void* signal_h(void*);
void clientReadReq(void* args);

int main (int argc, char* argv[]){

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask,SIGQUIT);
    sigaddset(&mask,SIGINT);
    sigaddset(&mask,SIGHUP);
    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0){
        printf("sigmask error!\n");
        return 1;
    }

    void* file_memory;                                      // Allocazione memoria file server

    serverStartup(&file_memory,MEMORY_SIZE);
    // pipe usata dalla threadpool per comunicare 
    // gli fd che devono essere riascoltati dal server
    int pipefd[2];
    if (pipe(pipefd) == -1){
        perror("pipe");
        return -1;
    }
    int signalpipe[2];
    if (pipe(signalpipe) == -1){
        perror("pipe");
        return -1;
    }
    SignalThreadArgs SigArgs = { &mask, signalpipe[1] };
    pthread_t sig_thread;
    pthread_create(&sig_thread, NULL, signal_h, &SigArgs);
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

    // fd_set da usare con select()
    int fdArr[3] = {pipefd[0], listen_socket, signalpipe[0]};
    FdStruct* fd_struct = fdSetMake(fdArr,3);
    // MAIN LOOP
    int signal = 0;
    //TODO: update fd_set when a client closes the connection
    while( (signal != SIGINT) && (signal != SIGQUIT) ){
        fd_set tmp_fd_set = *(fd_struct->set);
        select(fd_struct->max+1, &tmp_fd_set,NULL,NULL,NULL);//add err check
        int i;
        for(i=0; i<=fd_struct->max; i++){
            if (FD_ISSET(i,&tmp_fd_set)){
                if (i == signalpipe[0]){
                    //a signal was intercepted from the signal handler thread
                    //and sent to main thread
                    read(signalpipe[0], &signal, sizeof(int));
                    printf("\n received signal %d\n",signal);
                    if ( (signal == SIGINT) || (signal == SIGQUIT)){
                        //server received a request to quit as soon as possible,
                        //so no more client requests will be read
                        break;
                    }
                }
                // if SIGHUP was heard, the server does not want to accept any new client connections
                else if ((i == listen_socket && signal != SIGHUP)){
                    //server received a connection request from a new client
                    acceptConnection(listen_socket,fd_struct);
                }
                else if (i == pipefd[0]) {
                    // a worker finished serving a client request and is
                    // telling the server to resume listening to the client
                    checkPipeForFd(pipefd[0],fd_struct);
                }
                else{
                    //client requested something
                    // struct containing data needed by the worker
                    ReqReadStruct* workargs = makeWorkArgs(i,pipefd[1],file_memory,fd_struct);
                    //request to read from the client socket is added to the thread pool task queue
                    threadPoolAdd(&worker_pool,&clientReadReq,(void*)workargs);
                    // a worker will be using client fd to fulfill its request,
                    // so it's cleared from the fd set until worker is done
                    FD_CLR(i,fd_struct->set);
                    // once worker is done,it'll send back client fd to the main thread by shared pipe
                    // and will free memory allocated for its arguments
                }
            }
        } 
    }
    printf("Server is closing!\n");
    worker_pool.stop = 1;
    pthread_cond_broadcast(&(worker_pool.queueHasWork));
    workersDestroy(worker_threads, WORKER_NUMBER);
    pthread_join(sig_thread,NULL);
    fdSetFree(fd_struct);
    free(file_memory);     //cleanup finale
    close(listen_socket);
    unlink(socket_name);
    //threadPoolDestroy();
    printf("Server closed successfully!\n");
}

int serverStartup(void** server, double size){
    if ( (*server = malloc(size)) == NULL)
        return -1;
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
        write(pipe, &fd, sizeof(int));
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

int acceptConnection(int socket, FdStruct* fd_struct){
   int new_conn = accept(socket, NULL, 0);
    FD_SET(new_conn, fd_struct->set);
    if (new_conn> fd_struct->max){
        fd_struct->max = new_conn;
    }
    printf("connection %d  accepted!\n",new_conn);
    fflush(stdin);
    return new_conn;
}
int checkPipeForFd(int pipe,FdStruct* fd_struct){
    int fd_received;
    if (read(pipe, &fd_received, sizeof(int)) > 0){
        printf(" fd sent to pipe was %d!\n",fd_received);
        FD_SET(fd_received, fd_struct->set);
    }
    return 0;
}

void* signal_h(void* Args){
    SignalThreadArgs* ArgsCast= (SignalThreadArgs*) Args;
    sigset_t* set = ArgsCast->set;
    while (1){
        int pipe = ArgsCast->pipe;
        int signal;
        if( sigwait(set, &signal) != 0){
            perror("signal handler");
            return NULL;
        }
        write(pipe, &signal, sizeof(int));
        pthread_exit(NULL);
        return NULL;
    } 
}

