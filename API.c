#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <search.h>
#include <string.h>
#include <errno.h>

#include "API.h"

#define MAX_PATH 108


int fd_st =-1; // variabile globale per comunicare con il server 
char* client_socket_name; //nome del socket passato con -f

int openConnection(const char* sock_name, int msec, const struct timespec abstime){ // TODO: implement abstime
  int fd_socket = socket(AF_UNIX,SOCK_STREAM, 0);
  struct sockaddr_un sock_addr;
  int str_l = strnlen(sock_name,MAX_PATH);
  strncpy(sock_addr.sun_path, sock_name, str_l+1);
  sock_addr.sun_family = AF_UNIX;
  while ( connect(fd_socket, (struct sockaddr*)&sock_addr, SUN_LEN(&sock_addr)) != 0) // connection to socket is attempted every msec until it succeeds
      sleep(msec/1000);
  printf("Connection success!\n");
  fd_st = fd_socket;                        //salvataggio del descrittore del socket aperto e il nome associato ad esso per comunicazioni future 
  str_l = strnlen(sock_name,MAX_PATH);
  client_socket_name = malloc(sizeof(str_l)+1);
  strncpy(client_socket_name,sock_name,str_l+1);
  return 0;
}

int closeConnection(const char* sock_name){
    if(strncmp(client_socket_name,sock_name,MAX_PATH) == 0){ //controllo che il socket passato sia attualmente in uso
        close(fd_st);
        printf(" Connection %s closed succesfully.\n",sock_name); 
        free(client_socket_name);
        return 0;
    }
    else {
        printf("ERROR: no such socket exists/ socket not opened\n");
        return -1;
    }
}


int sendToSocket(const void* msg,const int len){ //se  una connessione e' aperta al server viene passato msg, altrimenti restituisce un errore
    //int msg_len = strnlen(msg,1024)+1;
    if (fd_st == -1){
        printf("Error: connection not established\n");
        return -1;
    }
    int sent_bytes = write(fd_st,msg,len);
    return sent_bytes;
}

int writeFile(const char* path_name,const char* dir_name){
    char* abs_path = realpath(path_name, NULL);
    printf("write file called\n");
    int inp_file;
    inp_file = open(abs_path,O_RDONLY);
    if (inp_file == -1){
        perror("open file");
        return -1;
    }
    if (abs_path == NULL){
        perror("realpath");
    }
    double len;
    int path_len = strnlen(abs_path,MAX_PATH)+1;
    struct stat f_stat;
    fstat(inp_file,&f_stat);
    len = f_stat.st_size;
        SockMsg send_msg;
        printf("len is %f\n",len);
        send_msg.func = 1;
        send_msg.path_size= path_len;
        send_msg.file_size = len;
        printf("path_size is %d\n",send_msg.path_size);
        //salvataggio contenuto del file in out_buff
        printf("sending to socket!\n");
        fflush(stdin);
        //sendToSocket("req file write\n");
        sendToSocket(&(send_msg.func), sizeof(int));
        sendToSocket(&(send_msg.path_size), sizeof(int));
        sendToSocket(abs_path, send_msg.path_size);
        sendToSocket(&(send_msg.file_size),sizeof(double));
        char out_buff[CHUNK_SIZE];
        int total_sent_bytes= 0;
        int sent_bytes;
        while(total_sent_bytes < len){
            int rd_bytes;
            rd_bytes = read(inp_file,out_buff,CHUNK_SIZE);
            sent_bytes = sendToSocket(out_buff,rd_bytes);
            total_sent_bytes += sent_bytes;
            if (sent_bytes ==0){
                break;
            }
            if (sent_bytes < 0){
                perror("client write");
                break;
            }
        }
        free(abs_path);
        return 0;
}

