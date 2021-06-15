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
#include <fcntl.h>
#include <signal.h>

#include "API.h"
#include "DataStr.h" 
#include "ThreadPool.h" 
#include "FileMemory.h"


typedef struct{
    sigset_t* set;
    int pipe;
} SignalThreadArgs;


size_t readNBytes(int fd, void* buff, size_t n);
int acceptConnection(int, FdStruct*);
int serverStartup(void**,size_t );
int checkFdSets(fd_set*);
int checkPipeForFd(int ,FdStruct*);
int CheckForFdRequest(FdStruct*);
int fileAdd(void *,char*,size_t , int);
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

    memorySetup(MEMORY_SIZE);
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
                    //and its value sent to main thread
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
                    ReqReadStruct* workargs = makeWorkArgs(i,pipefd[1],FileMemory.memPtr,fd_struct);
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
    pthread_mutex_lock(&worker_pool.mutex);
    worker_pool.stop = 1;
    pthread_mutex_unlock(&worker_pool.mutex);
    pthread_cond_broadcast(&(worker_pool.queueHasWork));
    workersDestroy(worker_threads, WORKER_NUMBER);
    pthread_join(sig_thread,NULL);
    fdSetFree(fd_struct);
    memoryClean();
    close(listen_socket);
    unlink(socket_name);
    //threadPoolDestroy();
    
    printf("Server closed successfully!\n");
}

int serverStartup(void** server, size_t size){
    if ( (*server = malloc(size)) == NULL)
        return -1;
    return 0;
}

// temporary simplified implementation
int fileAdd(void* mem ,char* inp_file, size_t len, int flag){
    printf("len is : %zu\n",len);
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
    size_t file_size;
    char* file_path = NULL;
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
        read(fd,&file_size,sizeof(size_t ));
        printf(" file size is %zu\n",file_size);
        void* MM_file = malloc(file_size);
        //char read_buff[CHUNK_SIZE]; 
        char* MM_file_ptr = MM_file;
        readNBytes(fd,MM_file,file_size);
        fprintf(stdout,"client from %d attempting to call fileAdd\n",fd);
        MemFile* new_file = malloc(sizeof(MemFile));
        int already_exists = fileInit(new_file,file_size, file_path);
        if(already_exists == 1){
            free(new_file); 
        }else{
            pthread_mutex_lock(&(new_file->mutex));
            int to_write;
            MM_file_ptr = MM_file;
            //File is written into memory one page at a time
            for(to_write=new_file->pages_n; to_write>1; to_write--){
                addPageToMem(MM_file_ptr, new_file, to_write-1, PAGE_SIZE);
                MM_file_ptr += PAGE_SIZE;
            }
            //the last page will be smaller than PAGE_SIZE, so we calculate how big it is
            addPageToMem(MM_file_ptr, new_file, to_write-1, file_size % PAGE_SIZE); 
            pthread_mutex_unlock(&(new_file->mutex));
        }
        write(pipe, &fd, sizeof(int));
        fflush(stdout);
        free(MM_file);
        free(file_path);
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

size_t readNBytes(int fd, void* buff, size_t n){
    size_t n_left;
    size_t n_read;
    n_left = n;
    while (n_left > 0){
        if ((n_read = read(fd, buff, n_left)) < 0){
            if (n_left == n){
                return -1;
            }
            else{
                break;
            }
        }
        else if (n_read == 0){
            break;
        }
        n_left -= n_read;
        buff += n_read;
    }
    return (n - n_left);
}
