#include "config.h"


size_t server_memory_size;
size_t page_size;
int    file_max;
size_t file_hash_tb_size;
int    worker_threads_n;
char* c_socket_name = NULL;

//look into file (must already be opened) for a value associated
//to value_str and return it as dynamically allocated string.
char* configGetToken(char* value_str,  FILE* file){
    char line[CONFIG_LINE_MAX];
    //get one line from config until EOF
    while (fgets(line,CONFIG_LINE_MAX,file) != NULL){
        char* save = NULL;
        char* token;
        //look for first string in a line before a = or  " "
        token= strtok_r(line,"= ",&save);
        if (strncmp(token,value_str,CONFIG_LINE_MAX) == 0){
            //string was equal to value_str, save the value
            //associated with value_str into token
            token = strtok_r(NULL, "= \n",&save);
            int len = strnlen(token,CONFIG_LINE_MAX-1);
            char* ret_token = malloc(len+1);
            strncpy(ret_token,token,len+1);
            return ret_token;
        }
    }
    return NULL;
}

//set every config variable to its value stored in filename opened with given mode
int configGetAll(char*filename, char* mode){
    FILE* file;
    if ((file = fopen(filename,mode)) == NULL){
        perror("config.txt open");
        return -1;
    }
    server_memory_size = configGetInt("memory_size",file)*1024*1024;
    if (server_memory_size == 0){ 
        return -1;
    }
    page_size = configGetInt("page_size",file)*1024;
    if (page_size== 0) 
        return -1;
    file_max= configGetInt("file_max",file);
    if (file_max== 0) 
        return -1;
    file_hash_tb_size= configGetInt("file_hash_tb_size",file);
    if (file_hash_tb_size== 0) 
        return -1;
    worker_threads_n = configGetInt("worker_threads",file);
    if (worker_threads_n == 0) 
        return -1;
    c_socket_name = configGetToken("socket_name",file);
    fclose(file);
    return 0;
    
}
//look into file (must already be opened) for a value associated
//to value_str and return it as integer 
int configGetInt(char* value_str, FILE* file){
    char* token = configGetToken(value_str,file);
    int num = atoi(token);
    free(token);
    return num;
}
