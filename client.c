#include "API.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

int main (int argc, char* argv[]){
    ENABLE_PRINTS = 0;
    char* sock_name = NULL;
    struct timespec temp;
    int i;
    for(i=1; i<argc; i++){
        if (!strncmp(argv[i],"-h",_POSIX_PATH_MAX)){
            printf("h received\n");
            printf("commands you can use blah blah blah\n");
            exit(0);
        }
        else if (!strncmp(argv[i],"-p",3)){
            ENABLE_PRINTS = 1;
        }
        else if (!strncmp(argv[i],"-f",3)){
            i++;
            sock_name = argv[i];
            openConnection(sock_name,50,temp);
        }
    }
    for(i=1; i<argc; i++){
        if (!strncmp(argv[i],"-r",333)){
            printf("r received\n");
            i++;
            readFile(argv[i],NULL,0);
        }
        else if (!strncmp(argv[i],"-R",3)){
            i++;
            int N = atoi(argv[i]);
            printf("calling ReadN of %d files\n",N);
            i++;
            char* dest = NULL;
            if (i < argc){
                if (!strncmp(argv[i],"-d",3)){
                    if (i < argc)
                        dest = argv[++i];
                } 
            }
            readNFiles(N,dest);
        }
        else if (!strncmp(argv[i],"-o",3)){
            printf("o received\n");
            i++;
            openFile(argv[i],0);
        }
        else if (!strncmp(argv[i],"-w",3)){
            printf("w received\n");
            i++;
            openFile(argv[i],O_CREATE);
            writeFile(argv[i],NULL);
            
        }
        else if (!strncmp(argv[i],"-a",3)){
            i++;
            printf("a received\n");
            //openFile(argv[i],0);
            char* append = "test\n";
            appendToFile(argv[i], append, strlen(append)+1, NULL);
        }
        else if (!strncmp(argv[i],"-c",3)){
            i++;
            printf("c received\n");
            removeFile(argv[i]);
        }
        else{
            //command not recognized
            //printf("garbage value, type -H to get a list of commands or -h at startup\n");
        }
    }
    fflush(stdout);
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
