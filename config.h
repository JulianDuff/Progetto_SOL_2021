#if !defined(CONFIG_H)
#define CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define CONFIG_LINE_MAX 128


extern size_t c_server_memory_size;
extern pthread_mutex_t c_server_memory_size_mtx;
extern size_t c_page_size;
extern pthread_mutex_t c_page_size_mtx;
extern int c_file_max;
extern pthread_mutex_t c_file_max_mtx;
extern size_t c_file_hash_tb_size;
extern pthread_mutex_t c_file_hash_tb_size_mtx;
extern int c_worker_threads_n;
extern pthread_mutex_t c_worker_threads_n_mtx;
extern char* c_socket_name;

int configGetInt(char* value_str,  FILE* file);
char* configGetToken(char* value_str,  FILE* file);
int configGetAll(char*filename, char* mode);


int configReadInt(int* var,pthread_mutex_t* lock);
size_t configReadSizeT(size_t* var,pthread_mutex_t* lock);
#endif
