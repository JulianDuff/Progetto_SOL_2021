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


#define SRC 0
#define DEL 1

extern size_t page_num;
//linked list to store pages available for file storage
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
//its absolute path name (used to identify it), its size, list of clients that have file in an opened state
// and a mutex used to prevent race conditions from multiple threads accessing the same file at the same time
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
//page_n is fixed while the server is running.
typedef struct{
    void** memPtr;
    size_t page_n;
    int file_n;
    PageList* pages;   
} FileMemoryStruct; 

// HashTable cointans an array of which every element is a doubly linked list which can hold any void* data
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
    char** stack;
}FileStack;

FileStack* fileStackInit(int size);
int fileStackDefrag(FileStack* stack);
int fileStackAdd(FileStack* stack,char* file_path);
MemFile* fileStackGetTop(FileStack* stack,char* key);
int fileStackDelete(FileStack* stack);

HashTable* hashTbMake(size_t size);
int hashTbDestroy(HashTable*);
int hashTbAdd(HashTable*, void*, unsigned long);
void* hashTbSearch(HashTable* HTable,char* path_name,int flag);
unsigned long hashKey(char*);
MemFile* hashGetFile(HashTable*, char*,int flag);
int hashRemoveFile(HashTable* FileHashTb,char* file_path);

void DL_ListAdd(DL_List**, void* data);
void* DL_ListTake(DL_List**, int flag);
int DL_ListDeleteCell(DL_List* list_cell);


void pageAdd(int,PageList**);
int pageTake(PageList** list);
int pageGet(PageList** list,char* file_str);
PageList* pageListCreate();


int addPageToMem(const void* input, const MemFile* FilePtr, const  int page_ind, const size_t size, const  int offset);
int filePagesInitialize(MemFile* file);
int fileAddToHash(MemFile*);
int filePagesRenew(MemFile* file,int pages);
int fileFree(MemFile* filePtr);
int clientOpenSearch(DL_List*,pid_t client_pid);
int fileDeleteFIFO(PageList** list,char* file_str);

void FileNDecrease();
int FileNUpdate(int n_max);

int  memorySetup(); 
int  memoryClean(); 

extern pthread_mutex_t hashTB_mutex;
extern pthread_mutex_t pages_mutex;
extern pthread_mutex_t filestack_mutex;
extern pthread_mutex_t file_n_mutex;
FileMemoryStruct FileMemory;
extern HashTable* FileHashTb;
extern FileStack* FStack;
#endif
