#include "API.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <dirent.h>

int writeFileList(char* list,char* dir_name,char* delim);
int readFileList(char* string, char* dir_name, char* delim);
int removeFileList(char* string, char* delim);
int fileToFolder(char* file_name, char* dir_name,void* file_contents, size_t file_size);
int sendFilesDirectory(char* dir_name);

int main (int argc, char* argv[]){
    ENABLE_PRINTS = 0;
    char* sock_name = NULL;
    //client attempts to connect until 10 seconds have passed
    struct timespec maxwait;
    clock_gettime(CLOCK_REALTIME,&maxwait);
    maxwait.tv_sec += 10;
    struct timespec request_delay = {0,0};
    int i;
    //check argv first for flags (h,p,f)
    for(i=1; i<argc; i++){
        char c;
        if (*(argv[i]) == '-'){
            c = *(argv[i]+1);
        }
        else
            continue;
        switch (c) {
        case 'h':
            printf("h received\n");
            printf("commands you can use blah blah blah\n");
            exit(0);
            break;
        case 'p':
            ENABLE_PRINTS = 1;
            break;
        case 'f':
            i++;
            sock_name = argv[i];
            if (openConnection(sock_name,50,maxwait) == -1){
                fprintf(stderr,"open connection timed out!\n");
                return 1;
            }
            break;
        default:
            break;
        }
    }
    for(i=1; i<argc; i++){
        char c;
        if (*(argv[i]) == '-'){
            c = *(argv[i]+1);
        }
        else
            continue;

        switch (c) {
        case 'r':
            i++;
            if ( (*argv[i+1] == '-') && (i+1 < argc) ){
                if ( *(argv[i+1]+1) == 'd' && i+2 < argc && *argv[i+2] != '-'){
                    if (checkDir(argv[i+2])){
                        readFileList(argv[i],argv[i+2],",");
                    }
                    i+= 2;
                    }
                else{
                    printf("wrong -r value\n");
                    break;
                }
            }
            else{
                readFileList(argv[i], NULL, ",");
            }
            break;
        case 'R':
            i++;
            int N = atoi(argv[i]);
            if (ENABLE_PRINTS)
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
            break;
        case 'W':
            i++;
            writeFileList(argv[i],NULL,",");
            break;
        case 'w':
            i++;
            sendFilesDirectory(argv[i]);
            break;
        case 'a':
            i++;
            //openFile(argv[i],0);
            char* append = "test\n";
            openFile(argv[i],0);
            appendToFile(argv[i], append, strlen(append)+1, NULL);
            closeFile(argv[i]);
            break;
        case 'c':
            i++;
            removeFileList(argv[i], ",");
            break;
        case 't':
            i++;
            if (*argv[i] != '-'){
                int time_add= atoi(argv[i]);
                request_delay.tv_sec = time_add/1000;
                request_delay.tv_nsec = (time_add % 1000)*1000*1000;
            }
        default:
            break;
        }
        nanosleep(&request_delay,NULL);
    }
    fflush(stdout);
    if (closeConnection(sock_name) != -1){
        if (ENABLE_PRINTS)
            printf("closed connection!");
    }
    else{
        if (ENABLE_PRINTS)
            printf("error closing connection!\n");
    }
    fflush(stdout);
    exit(0);
}

// sends an open and write request for every file in string
// (separated by the characters in delim)
// returns -1 if a write failed, 0 otherwise
int writeFileList(char* string, char* dir_name, char* delim){
    char* save = NULL;
    char* token;
    token= strtok_r(string,delim,&save);
    openFile(token,O_CREATE);
    writeFile(token,dir_name);
    closeFile(token);
    while ( (token = strtok_r(NULL, delim, &save)) != NULL){
        openFile(token, O_CREATE);
        writeFile(token, dir_name);
        closeFile(token);
    }
    return 0;
}

int readFileList(char* string, char* dir_name, char* delim){
    char* save = NULL;
    char* token;
    token= strtok_r(string,delim,&save);
    size_t file_size;
    void* file_contents;
    openFile(token,0);
    if (readFile(token,&file_contents,&file_size) != -1){
        fileToFolder(token,dir_name,file_contents,file_size);
        free(file_contents);
        closeFile(token);
    }
    while ( (token = strtok_r(NULL, delim, &save)) != NULL){
        openFile(token,0);
        if (readFile(token,&file_contents,&file_size) != -1){
            fileToFolder(token,dir_name,file_contents,file_size);
            free(file_contents);
            closeFile(token);
        }
    }
    return 0;
}

int removeFileList(char* string, char* delim){
    char* save = NULL;
    char* token;
    token= strtok_r(string,delim,&save);
    openFile(token,0);
    removeFile(token);
    while ( (token = strtok_r(NULL, delim, &save)) != NULL){
        openFile(token,0);
        removeFile(token);
    }
    return 0;
}

int fileToFolder(char* file_name, char* dir_name,void* file_contents, size_t file_size){
        if (dir_name != NULL){
            char* write_dest = malloc(_POSIX_PATH_MAX);
            strncpy(write_dest,dir_name,_POSIX_PATH_MAX);
            strncat(write_dest,"/",_POSIX_PATH_MAX);
            strncat(write_dest,file_name,_POSIX_PATH_MAX);
            int fd_file = open( write_dest, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);
            if (fd_file == -1){
                perror("file open");
                return -1;
            }
            if (write(fd_file,file_contents,file_size) == -1){
                perror("file write");
                return -1;
            }
        }
        return 0;
}
// returns dir_name if it's a valid directory, NULL otherwise


int sendFilesDirectory(char* dir_name){
    DIR* dir;
    if ((dir = opendir(dir_name))== NULL){
            perror("opendir");
            fprintf(stderr,"error opening dir in %s -w\n",dir_name);
        return -1;
    }
    char file_name[_POSIX_PATH_MAX];
    struct dirent* file;
    while (errno = 0, (file = readdir(dir))!= NULL){

        strncpy(file_name,dir_name,_POSIX_PATH_MAX);
        strncat(file_name,"/",_POSIX_PATH_MAX-strlen(file_name));
        strncat(file_name,file->d_name,_POSIX_PATH_MAX-strlen(file_name));

        struct stat info;
        stat(file_name,&info);
        if (S_ISDIR(info.st_mode)){
            if (strncmp(file->d_name,".",1) != 0){
                sendFilesDirectory(file_name);
            }
        }
        else if(S_ISREG(info.st_mode)){
            openFile(file_name,O_CREATE);
            writeFile(file_name,NULL);
            closeFile(file_name);
        }
    }
    if (closedir(dir) == -1){
        fprintf(stderr,"error closing dir %s \n",dir_name);
        perror("closedir");
    }
    return 0;
}
