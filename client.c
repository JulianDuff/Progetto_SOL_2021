#include "API.h"
#include <stdio.h>
#include <stdlib.h>


int main (int argc, char* argv[]){
    /*char* socket_name = argv[1];
    int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un sock_addr;
    strncpy(sock_addr.sun_path,socket_name,108);
    sock_addr.sun_family =AF_UNIX;
    connect(socket_fd,(struct sockaddr*) &sock_addr, SUN_LEN(&sock_addr));
    */
    /* write(socket_fd, "hey there kill all humans :) \n",20);
    close(socket_fd);
    */
    //test
    struct timespec temp;
    openConnection(argv[1],50,temp);
    sendToSocket("orange banana banana");
    printf("I am connected, socket fd\n"); // printf temporanei
    if (( closeConnection(argv[1])) ==0)   //
        printf("success!\n");              //
    else
        printf("failure\n");

    exit(0);
}

