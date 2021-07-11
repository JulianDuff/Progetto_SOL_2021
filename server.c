#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>
#include <search.h>
#include <string.h>
#include <sys/select.h>
#include <signal.h>
#include <limits.h>

#include "API.h" 
#include "ThreadPool.h" 
#include "FileMemory.h"
#include "config.h"


typedef struct{
    sigset_t* set;
    int pipe;
} SignalThreadArgs;


int acceptConnection(int, FdStruct*);
int checkFdSets(fd_set*);
int checkPipeForFd(int ,fd_set* fd_MT_set, fd_set* fd_work_set);
int CheckForFdRequest(FdStruct*);
FdStruct* fdSetMake(int* fd,int n); 
int fdSetFree(FdStruct* );
void* signal_h(void*);
void* clientReadReq(void* args);
int FdSetUpdate(FdStruct* fd_struct, fd_set* fd_work_set);

int main (int argc, char* argv[]){
    if (argc < 2){
        printf("Error, you must pass config file as command line argument\n");
        return 1;
    }
    if (configGetAll(argv[1],"r") == -1){
        printf("error loading config\n");
        return 1;
                
    }
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask,SIGQUIT);
    sigaddset(&mask,SIGPIPE);
    sigaddset(&mask,SIGINT);
    sigaddset(&mask,SIGHUP);
    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0){
        printf("sigmask error!\n");
        return 1;
    }
    sigdelset(&mask,SIGPIPE);
    // storage memory and memory for saving file data
    memorySetup();
    // this pipe will be used by the threadpool workers to comunicate
    // to the main thread that they completed a request from a client
    int pipefd[2];
    if (pipe(pipefd) == -1){
        perror("pipe");
        return -1;
    }
    // this pipe will be used by a signal handler thread to alert the main thread
    // if a request to stop the server was heard
    int signalpipe[2];
    if (pipe(signalpipe) == -1){
        perror("pipe");
        return -1;
    }
    SignalThreadArgs SigArgs = { &mask, signalpipe[1] };
    pthread_t sig_thread;
    pthread_create(&sig_thread, NULL, signal_h, &SigArgs);
    //array which holds the workers's thread ids
    pthread_t*  worker_threads; 
    threadPool worker_pool;
    threadPoolInit(&worker_pool,pipefd);
    //create and assign WORKER_NUMBER threads to worker pool
    makeWorkerThreads(&worker_threads, c_worker_threads_n, &worker_pool);
    //TODO: use config.txt 
    //Setup of the socket used to accept client connections
    //Preparazione socket di ascolto
    int listen_socket = socket(AF_UNIX, SOCK_STREAM, 0);        
    struct sockaddr_un sock_addr;
    strncpy(sock_addr.sun_path, c_socket_name, 108);
    sock_addr.sun_family =AF_UNIX;
    bind(listen_socket, (struct sockaddr*) &sock_addr, SUN_LEN(&sock_addr)); 
    listen(listen_socket, 5);

    // all the file descriptors that the server will read from are put into 
    // an array which is used to prepare a fd_set
    int fdArr[3] = {pipefd[0], listen_socket, signalpipe[0]};
    //fd_struct holds an fd_set and its highest fd
    FdStruct* fd_struct = fdSetMake(fdArr,3);
    int last_non_client_fd = fd_struct->max;
    int signal = 0;
    // work set used to keep track of fds which are being handled by workers
    fd_set fd_work_set;
    FD_ZERO(&fd_work_set);
    while( (signal != SIGINT) && (signal != SIGQUIT) ){
        // select modifies the fd_set, so we pass a  copy of it to preserve it
        fd_set tmp_fd_set = *(fd_struct->set);
        //wait for a fd to be readable 
        if (select(fd_struct->max+1, &tmp_fd_set,NULL,NULL,NULL) == -1){
            perror("select");
        }
        int i;
        //a fd is ready for a read operation, but the entire fd_set
        //must be searched to find it 
        for(i=0; i<=fd_struct->max; i++){
            if (FD_ISSET(i,&tmp_fd_set)){
                if (i == signalpipe[0]){
                    //a signal was intercepted from the signal handler thread
                    //read it
                    read(signalpipe[0], &signal, sizeof(int));
                    printf("\n received signal %d\n",signal);
                }
                // if SIGHUP was heard, the server does not want to accept any new client connections
                else if ((i == listen_socket && signal != SIGHUP)){
                    //server received a connection request from a new client
                    acceptConnection(listen_socket,fd_struct);
                }
                else if (i == pipefd[0]) {
                    // the pipe will update the MT set and the working set based on whether the client is still connected or not
                    checkPipeForFd(pipefd[0],fd_struct->set,&fd_work_set);
                }
                else if (i != listen_socket){
                    //client requested something
                    // struct containing data needed by the worker
                    ReqReadStruct* workargs = makeWorkArgs(i,pipefd[1],FileMemory.memPtr,fd_struct);
                    //request to read from the client socket is added to the thread pool task queue
                    threadPoolAdd(&worker_pool,&clientReadReq,(void*)workargs);
                    // a worker will be using client fd to fulfill its request,
                    // so it's cleared from the fd set until worker is done
                    FD_CLR(i,fd_struct->set);
                    FD_SET(i,&fd_work_set);
                    // once worker is done,it'll send back client fd to the main thread by shared pipe
                    // and will free memory allocated for its arguments
                }
            }
        } 
        // sets max to the highest set fd between fd_struct and fd_work_set
        FdSetUpdate(fd_struct,&fd_work_set);
        //if fd_struct max is lower than any client_fd then no clients are connected to the server anymore
        if( signal == SIGHUP && fd_struct->max == last_non_client_fd)
            break;
    }
    printf("Server is closing!\n");
    threadPoolClear(&worker_pool);
    // add "exit thread" task to threadpool
    threadPoolAdd(&worker_pool, ThreadRequestExit, &worker_pool);
    // join with every thread and free array used to store their IDs
    workersDestroy(worker_threads, c_worker_threads_n);
    printf("All workers quit!\n");
    threadPoolDestroy(&worker_pool);
    pthread_join(sig_thread,NULL);
    printf("sig thread quit!\n");
    fdSetFree(fd_struct);
    memoryClean();
    close(listen_socket);
    unlink(c_socket_name);
    free(c_socket_name);
    
    printf("Server closed successfully!\n");
}

void* clientReadReq(void* args){
    ReqReadStruct* req = (ReqReadStruct*) args;
    int pipe = req->pipe;
    int fd = req->fd;
    int func;
    int fdOpen;
    int read_n = readNB(fd,&func,sizeof(int));
    if (read_n == 0){
        fdOpen = 0;
        write(pipe, &fdOpen, sizeof(int));
        write(pipe, &fd, sizeof(int));
        printf("client %d closed the connection!\n",fd);
    }
    else{
        (*ReqFunArr[func])(args);
        fdOpen = 1;
        write(pipe, &fdOpen, sizeof(int));
        write(pipe, &fd, sizeof(int));
        fflush(stdout);
    }
    return NULL;
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

int acceptConnection(int socket, FdStruct* fd_struct){
   int new_conn = accept(socket, NULL, 0);
    FD_SET(new_conn, fd_struct->set);
    if (new_conn > fd_struct->max){
        fd_struct->max = new_conn;
    }
    printf("connection %d  accepted!\n",new_conn);
    fflush(stdin);
    return new_conn;
}


int checkPipeForFd(int pipe,fd_set* fd_MT_set ,fd_set* fd_work_set){
    int fdOpen;
    int fd_received;
    readNB(pipe, &fdOpen, sizeof(int));
    readNB(pipe, &fd_received, sizeof(int));
    FD_CLR(fd_received,fd_work_set);
    if (fdOpen){
        FD_SET(fd_received,fd_MT_set);
    }
    else{
        close (fd_received);
        return 1;
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

int FdSetUpdate(FdStruct* fd_struct, fd_set* fd_work_set){
    int i;
    int n_max = 0;
    for (i=0; i<= fd_struct->max ;i++){
        //check if fd is being handled by a worker or if its being listened to by the main thread
        if (FD_ISSET(i,fd_struct->set) || FD_ISSET(i,fd_work_set))
                n_max = i;
    }
    fd_struct->max = n_max;
    return 0;
}
