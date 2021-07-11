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
int sendFilesDirectory(char* dir_name,int n);
int validFileParam(char** argv,int i, int argc);
int validNumParam(char** argv,int i, int argc);
int validOpParam(char** argv, int i, int argc, char c);
void printHelp();

int main (int argc, char* argv[]){
    ENABLE_PRINTS = 0;
    char* sock_name = NULL;
    //client attempts to connect until 10 seconds have passed
    struct timespec maxwait;
    clock_gettime(CLOCK_REALTIME,&maxwait);
    maxwait.tv_sec += 10;
    struct timespec request_delay = {0,0};
    int i;
    int not_connected = 1;
    //check argv first for flags h,p,f
    for(i=1; i<argc; i++){
        char c;
        if (*(argv[i]) == '-'){
            c = *(argv[i]+1);
        }
        else
            continue;
        switch (c) {
        case 'h':
            printHelp();
            exit(0);
            break;
        case 'p':
            ENABLE_PRINTS = 1;
            break;
        case 'f':
            if (validFileParam(argv,i+1,argc)){
                i++;
                sock_name = argv[i];
                not_connected = 0;
            }
            else {
                fprintf(stderr,"Failed to pass valid socket name! Client exiting\n");
                exit(1);
            }
            if (openConnection(sock_name,50,maxwait) == -1){
                fprintf(stderr,"open connection timed out!\n");
                return 1;
            }
            break;
        default:
            break;
        }
    }
    if (not_connected){
        fprintf(stderr,"Connection not specified, program closing\n");
        return 1;
    }
    for(i=1; i<argc; i++){
        char c;
        int n;
        if (*(argv[i]) == '-'){
            c = *(argv[i]+1);
        }
        else{
            continue;
        }
        switch (c) {
        case 'r':
            //check if the next parameter is a valid name (does not start with -)
            if (validFileParam(argv,i+1,argc)){
                //if a parameter is found i is incremented, as argv[i+1] is part of the command
                i++;
                //check if the next parameter is an optional command (d in this case)
                if (validOpParam(argv,i+1,argc,'d')){
                    //if there is an optional command, check that it has a valid argument (valid name)
                    i++;
                    if (validFileParam(argv,i+1,argc)){
                        i++;
                        //don't call readFileList if the destination is not a valid directory
                        if (checkDir(argv[i]))
                            readFileList(argv[i-2],argv[i],",");
                    }
                    else{
                        if (ENABLE_PRINTS)
                        printf("command -r not launched: -d requires a valid directory\n");
                    }
                }
                else{
                    readFileList(argv[i], NULL,",");
                }
                        
            }
            else{
                if (ENABLE_PRINTS)
                    printf("command -r not launched: specified file missing \n");
            }
            break;
        //all other cases use the same procedure to determine the command and optional arguments
        case 'R':
            n = validNumParam(argv,i+1,argc);
            if (n == -1){
                //if n was not passed its default value is 0
                n=0;
            }
            else{
                //n was passed so argv[i+1] is part of the command
                //to read what comes after it we increment i
                i++;
            }
            if (validOpParam(argv,i+1,argc,'d')){
                i++;
                if (validFileParam(argv,i+1,argc) && checkDir(argv[i+1])){
                    i++;
                    readNFiles(n,argv[i]);
                }
                else{
                    if (ENABLE_PRINTS)
                        printf("command -R not launch: invalid destination directory\n");
                }
            }
            else{
                readNFiles(n,NULL);
            }
            break;
        case 'W':
            if (validFileParam(argv,i+1,argc)){
                i++;
                writeFileList(argv[i],NULL,",");
            }
            else{
                if (ENABLE_PRINTS)
                    printf("command -W not launched: specified file missing \n");
            }
            break;
        case 'w':
            n = validNumParam(argv,i+1,argc);
            if (n != -1)
                i++;
            if (n==0)
                n = -1;
            if (validFileParam(argv,i+1,argc)){
                i++;
                sendFilesDirectory(argv[i],n);
            }
            else{
                if (ENABLE_PRINTS)
                    printf("command -w not launched: specified file missing \n");
            }
            break;
        case 'c':
            if (validFileParam(argv,i+1,argc)){
                i++;
                removeFileList(argv[i], ",");
            }
            else{
                if (ENABLE_PRINTS)
                    printf("command -c not launched: specified file missing \n");
            }
            break;
        case 't':
            if (i+1 < argc && *argv[i+1] != '-'){
                i++;
                int time_add= atoi(argv[i]);
                request_delay.tv_sec = time_add/1000;
                request_delay.tv_nsec = (time_add % 1000)*1000*1000;
            }
            else{
                if (ENABLE_PRINTS)
                    printf("-t not set: time specifier missing \n");
            }
            break;
        default:
            if (c != 'p' && c != 'f' && c != 'h'){
                if (ENABLE_PRINTS){
                    printf(" -%c invalid command!\n",c);
                }
            }
            break;
        }
        nanosleep(&request_delay,NULL);
    }
    fflush(stdout);
    if (closeConnection(sock_name) != -1){
        if (ENABLE_PRINTS)
            printf("closed connection!\n");
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
            char* short_name = fileShortenName(file_name);
            char* write_dest = malloc(_POSIX_PATH_MAX);
            strncpy(write_dest,dir_name,_POSIX_PATH_MAX);
            strncat(write_dest,"/",_POSIX_PATH_MAX);
            strncat(write_dest,short_name,_POSIX_PATH_MAX);
            int fd_file = open( write_dest, O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);
            if (fd_file == -1){
                perror("file open");
                return -1;
            }
            if (write(fd_file,file_contents,file_size) == -1){
                perror("file write");
                return -1;
            }
            else{
                if (ENABLE_PRINTS)
                    printf("file -%s- of size %zu sent to /%s successfully\n",short_name,file_size,dir_name);
            }
            free(short_name);
        }
        return 0;
}

int sendFilesDirectory(char* dir_name,int n){
    int sent_f = 0;
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
                sent_f += sendFilesDirectory(file_name,n-sent_f);
            }
        }
        else if(S_ISREG(info.st_mode)){
            if (n != -1)
                sent_f++;
            if (sent_f > n && n != -1){
                return 0;
            }
            else{
                openFile(file_name,O_CREATE);
                writeFile(file_name,NULL);
                closeFile(file_name);
            }
        }
    }
    if (closedir(dir) == -1){
        fprintf(stderr,"error closing dir %s \n",dir_name);
        perror("closedir");
    }
    return sent_f;
}

//returns 1 if string i in argv is a name and not a command (does not start with -)
int validFileParam(char** argv,int i, int argc){
    if ( i< argc && *(argv[i]) != '-')
        return 1;
    else 
        return 0;
}

// checks if string i in argv matches n=* and returns the numerical value of *, returns -1 if match is not found (n was not passed)
int validNumParam(char** argv,int i, int argc){
    if (i <argc && *(argv[i])== 'n' && *(argv[i]+1)=='=')
        return atoi(argv[i]+2);
    else
        return -1;
}
//checks if string i in argv is a command (starts with -) and if the character after - matches c
int validOpParam(char** argv, int i, int argc, char c){
    if (i< argc && *(argv[i]) == '-'){
        if (*(argv[i]+1) == c)
            return 1;
        else
            return 0;
    }
    else{
        return 0;
    }
}


void printHelp(){
    printf("~~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~--~-~-\n");
    printf("Commands supported by client:\n");
    printf("parameters enclosed in [ ] brackets are optional\n");
    printf("-f filename: name of socket to connect to\n");
    printf("-w dirname [n=0] : send up to n files from dirname or subdirectories of it\n");
    printf("-W file1,file2,..., : list of files to write to server\n");
    printf("-r file1,file2,..., : list of files to read from server\n");
    printf("-R [n=0] : read up to n random files from server (n=0 => no limit)\n");
    printf("-d dirname : optional parameter to pass to -r and -R \n   to specifiy a folder in which to save received files\n");
    printf("-c file1,file2,file..., : list of files to remove from the server\n");
    printf("-t N set a delay of N milliseconds for any future request\n   (can be set multiple times)\n");
    printf("-p allow the client process to print results from calls to stdout\n");
    printf("warning: only filenames from existing files are allowed\n");
    printf("calls to directories which do not exist are canceled\n");
    printf("~~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~--~-~-\n");
    return;
}
