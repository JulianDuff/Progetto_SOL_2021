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
  int connectionSuccess = 0;
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
        return 0;
    }
    else {
        printf("ERROR: no such socket exists/ socket not opened\n");
        return -1;
    }
}


int sendToSocket(const char* msg){ //se  una connessione e' aperta al server viene passato msg, altrimenti restituisce un errore
    int msg_len = strnlen(msg,1024)+1;
    if (fd_st == -1){
        printf("Error: connection not established\n");
        return -1;
    }
    write(fd_st,msg,msg_len);
    return 0;
}

int writeFile(const char* path_name,const char* dir_name){
    printf("write file called\n");
    FILE* inp_file;
    inp_file = fopen(path_name,"r");
    char * out_buff = NULL;
    long len;
    if (inp_file != NULL) {
        //calcolo lunghezza del file
        fseek(inp_file, 0, SEEK_END);
        len = ftell(inp_file);
        fseek(inp_file, 0, SEEK_SET);
        if ((out_buff = malloc (len)) == NULL){
            printf("writeFile Malloc Error!\n");
            return -2;
        }
        //salvataggio contenuto del file in out_buff
        fread(out_buff, 1, len, inp_file);
        fclose(inp_file);
        printf("sending to socket!\n");
        fflush(stdin);
        //sendToSocket("req file write\n");
        sendToSocket(out_buff);
        return 0;
    }
    else
        return -1;
}

