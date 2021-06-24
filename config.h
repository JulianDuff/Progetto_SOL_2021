#if !defined(CONFIG_H)
#define CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_LINE_MAX 32


extern size_t server_memory_size;
extern size_t page_size;
extern int file_max;
extern size_t file_hash_tb_size;
extern int worker_threads_n;
extern char* c_socket_name;

int configGetInt(char* value_str,  FILE* file);
char* configGetToken(char* value_str,  FILE* file);
int configGetAll(char*filename, char* mode);
#endif
