#include "API.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

int main (int argc, char* argv[]){
    char* sock_name = NULL;
    struct timespec temp;
    int i;
    for(i=1; i<argc; i++){
        if (!strncmp(argv[i],"-h",_POSIX_PATH_MAX)){
            printf("h received\n");
            printf("commands you can use blah blah blah\n");
            exit(0);
        }
        else if (!strncmp(argv[i],"-f",_POSIX_PATH_MAX)){
            i++;
            sock_name = argv[i];
            openConnection(sock_name,50,temp);
            printf("f received, socket name is now %s\n", sock_name);
        }
        else if (!strncmp(argv[i],"-r",_POSIX_PATH_MAX)){
            printf("r received\n");
            i++;
            openConnection(sock_name,50,temp);
            readFile(argv[i],NULL,0);
            
        }
        else if (!strncmp(argv[i],"-w",_POSIX_PATH_MAX)){
            printf("w received\n");
            i++;
            openConnection(sock_name,50,temp);
            writeFile(argv[i],NULL);
            
        }
        else if (!strncmp(argv[i],"-c",_POSIX_PATH_MAX)){
            i++;
            printf("c received\n");
            removeFile(argv[i]);
        }
        else{
            //command not recognized
            printf("garbage value, type -H to get a list of commands or -h at startup\n");
        }
    }
    fflush(stdout);
    openConnection(argv[1],50,temp);
    char* filep = malloc(sizeof(char) * 124);
    while(1){
        printf("what file do you want to send?\n");
        scanf("%s",filep);
        fflush(stdin);
        if (strcmp(filep,"exit") == 0){
            closeConnection(argv[1]);
            fflush(stdout);
            free(filep);
            exit(0);
        }
        writeFile(filep,"NA");
    }
    free(filep);
    if (( closeConnection(argv[1])) ==0)   //
        printf("success!\n");              //
    else
        printf("failure\n");

    exit(0);
}
