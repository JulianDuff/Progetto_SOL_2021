#if !defined(FILE_MEMORY_H)
#define FILE_MEMORY_H

//#define MEMORY_SIZE (1024 * 1024 * 64 )
//#define PAGE_SIZE (1024*4)
//#define PAGE_NUM (MEMORY_SIZE/PAGE_SIZE)
//#define FILE_MAX 100
//#define HASHTB_SIZE 401

#include <stdio.h>
#include <stdlib.h>
#include <search.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <search.h>
#include <limits.h>

#include "DataStr.h"

#define SRC 0
#define DEL 1

extern size_t page_num;
//linked listed to store pages available for file storage
// pages are taken from the list and given to a file when it is written/something is appended
// and they are put back into the list when a file is deleted
struct page_list_s{
    int page;
    struct page_list_s* next;
};
typedef struct page_list_s PageList;

struct gen_list{
    void* data;
    struct gen_list* next;
    struct gen_list* prev;
};
typedef struct gen_list DL_List;
//Struct that holds file metadata: how many pages and which are allocated for it in memory,
//its absolute path name (used to identify it), its size and a mutex used to prevent race condition
//from multiple threads accessing the same file at the same time
typedef struct{
    int* pages;
    int pages_n;
    char* abspath;
    size_t size;
    pthread_mutex_t mutex;
    DL_List* clients_opened;
}MemFile;

//This struct represents the memory in which files are stored.
//It's defined as an array holding page_n pages of memory.
//page_n does not change after it is read from config.
typedef struct{
    void** memPtr;
    size_t page_n;
    int file_n;
    PageList* pages;   
} FileMemoryStruct; 

// double linked list used as bookkeeping for files entered into the server,
// it allows to free memory and access any number of (non specific) files more efficiently

// HashTable cointans an array of size...size, of which every element is a doubly linked list
// which can hold any void* data
typedef struct {
    DL_List** table;
    size_t size;
}HashTable;

typedef struct {
    void* data;
    unsigned long key;
}HashEntry;

typedef struct{
    int max;
    int top;
    unsigned long* stack;
}FileStack;
FileStack* fileStackInit(int size);
int fileStackDefrag(FileStack* stack);
int fileStackAdd(FileStack* stack,unsigned long file_key);
MemFile* fileStackGetTop(FileStack* stack,double key);
int fileStackDelete(FileStack* stack);
int hashTbDestroy(HashTable*);
HashTable* hashTbMake(size_t size);
int hashTbAdd(HashTable*, void*, unsigned long);
void* hashTbSearch(HashTable* HTable, unsigned long key,int flag);
MemFile* hashGetFile(HashTable*, char*,int flag);
int hashRemoveFile(HashTable* FileHashTb,char* file_path);
//adds a file pointer to the FileList to the top of the list 
//todo: use int ret and check for inv input
void DL_ListAdd(DL_List**, void* data);
//returns The file pointer cointaned in a specific cell of the file list.
//if flag is set to del it also removes it from the list.
void* DL_ListTake(DL_List**, int flag);
int DL_ListDeleteCell(DL_List* list_cell);
int clientOpenSearch(DL_List*,pid_t client_pid);
void pageAdd(int,PageList**);
int  memorySetup(); 
int  memoryClean(); 
int pageTake(PageList** list);
int pageGet(PageList** list,double key);
PageList* pageListCreate();
int fileAddToHash(MemFile*);
int addPageToMem(const void* input, const MemFile* FilePtr, const  int page_ind, const size_t size, const  int offset);
unsigned long hashKey(char*);
int fileFree(MemFile* filePtr);
int  filePagesInitialize(MemFile* file);
int filePagesRenew(MemFile* file,int pages);
int fileDeleteFIFO(PageList** list, double key);

void FileNDecrease();
int FileNUpdate(int n_max);

extern pthread_mutex_t hashTB_mutex;
extern pthread_mutex_t pages_mutex;
extern pthread_mutex_t filestack_mutex;
extern pthread_mutex_t file_n_mutex;
FileMemoryStruct FileMemory;
extern HashTable* FileHashTb;
extern FileStack* FStack;
#endif
