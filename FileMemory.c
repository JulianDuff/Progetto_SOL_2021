#include "FileMemory.h"


pthread_mutex_t hashTB_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pages_mutex= PTHREAD_MUTEX_INITIALIZER;
DL_List* file_list = NULL;
HashTable* FileHashTb = NULL;
    
void pageAdd(int page, PageList** list){
    pthread_mutex_lock(&pages_mutex);
    PageList* new = malloc(sizeof(PageList));
    new->page = page;
    new->next = NULL;
    if (*list == NULL){
        *list= new;
    }
    else{
        new->next = (*list);
        (*list) = new;
    }
    pthread_mutex_unlock(&pages_mutex);
    return;
}

int pageTake(PageList** list){
    int ret_val;
    if( *list == NULL){
        ret_val = -1;
    }
    else{
        ret_val = (*list)->page;
        PageList* q_aux = (*list);
        *list = (*list)->next;
        free(q_aux);
    }
    return ret_val;
}


PageList* pageListCreate(int page_num){
    PageList* new_list = NULL;
    int i;
    for(i=0; i<page_num; i++){
        pageAdd(i, &new_list);
    }
    return new_list;
}
//a pointer

MemFile* fileSearch(char* abspath){
    MemFile* FilePtr = NULL;
    //hash mutex is locked to prevent race conditions
    pthread_mutex_lock(&hashTB_mutex);
    //looking into hash table for where FilePtr is
    //found its position in the FilePtr list
    pthread_mutex_unlock(&hashTB_mutex);
    //done adding a pointer to file in the hash table
    return FilePtr;
}

// sets values of FilePtr based on its size and path name,
// if file already exists it returns 1, otherwise it is added
// to file_list, a record is entered into the file hash table,
// and it returns 1
int fileInit(MemFile* FilePtr, size_t size, char* abspath){
    int ret_val = 0;
    if(hashGetFile(FileHashTb,abspath) != NULL){
        printf("file found!\n");
        ret_val =1;
    }
    else{
        pthread_mutex_unlock(&hashTB_mutex);
        printf("heyo!\n");
        pthread_mutex_init(&(FilePtr->mutex), NULL);
        FilePtr->size = size;
        printf("FilePtr size is %zu\n",FilePtr->size);
        FilePtr->abspath = malloc(strnlen(abspath,_POSIX_PATH_MAX)+1);
        strncpy(FilePtr->abspath,abspath,strlen(abspath)+1);
        // calculates how many pages are needed to store a file
        FilePtr->pages_n = FilePtr->size / PAGE_SIZE + 1;
        printf("FilePtr page num is %d\n",FilePtr->pages_n);
        if ((FilePtr->pages = malloc(sizeof(int) * (FilePtr->pages_n)) ) == NULL)
            printf("error file page smalloc!\n");
        int i;
        //TODO: add server page mutex
        for(i=0; i<(FilePtr->pages_n); i++){
            (FilePtr->pages)[i] = pageTake((&(FileMemory.pages)));
            printf("file took page: %d \n",FilePtr->pages[i]);
        }
        pthread_mutex_lock(&hashTB_mutex);
        //check if file is not already in memory
        //add file to list of files
        DL_ListAdd(&file_list, FilePtr);
        // add position in list of file to hashtable
        hashTbAdd(FileHashTb, &file_list);
        //file already exists, set return value to 1
        pthread_mutex_unlock(&hashTB_mutex);
    }
   return ret_val; 
}

// writes a full file page into memory
// MORE THAN A FULL PAGE IS UNDEFINED BEHAVIOUR
int addPageToMem(void* input,MemFile* FilePtr, int page_ind,size_t size){
    if(size > PAGE_SIZE){
        printf("error, size is larger than a full page\n");
        return -1;
    }
    int index = FilePtr->pages[page_ind];
    memcpy(FileMemory.memPtr[index], input, size);
    //temp stuff
    printf("Page read from memory:\n");
    write(1,FileMemory.memPtr[index],size);
    //delete soon
    return 0;

}

int memorySetup(size_t size){
    FileMemory.page_n= PAGE_NUM;
    FileMemory.pages = pageListCreate(PAGE_NUM);
    FileMemory.file_n = 0;

    if ((FileMemory.memPtr = malloc(sizeof(void*)* FileMemory.page_n)) == NULL){
        printf("Fatal Error! Server memory malloc failed.\n");
        return -1;
    }
    int i;
    for (i=0; i<FileMemory.page_n; i++){
        (FileMemory.memPtr[i]) = malloc(PAGE_SIZE);
        memset(FileMemory.memPtr[i], 0, PAGE_SIZE);
    }
    FileHashTb = hashTbMake(HASHTB_SIZE);
    return 0;
}

// adds void* data to the head of the doubly linked list specified
void DL_ListAdd(DL_List** list, void* data){
    DL_List* new = NULL;
    if (( new = malloc(sizeof(DL_List))) == NULL){
        printf("error file list add malloc\n"); 
    }
    new->data= data;
    new->next = NULL;
    new->prev = NULL;
    if (*list == NULL){
        *list = new;
    }
    else{
        new->next = *list;
        (*list)->prev = new;
        (*list)= new;
    }
    return;
}
//Returns the data found in the first node of doubly linked list,
//if list is empty it return NULL,
//if flag is set to DEL the node is removed from the list
//DEL on an empty list does nothing
void* DL_ListTake(DL_List** list, int flag){
    //empty list -> NULL return value
    MemFile* ret_val = NULL;
    if( *list == NULL){
        ret_val = NULL;
    }
    else{
        ret_val = (*list)->data;
        // if flag is set to DEL, remove the node from the list
        if (flag == DEL){
            //used to deallocate memory once done modifying the list 
            DL_List* q_aux =(*list);
            if ((*list)->prev != NULL){
                //list member was not the head, 
                //link the nodes before and after it to eachother
                (*list)=(*list)->prev;
                (*list)->next =(*list)->next->next;
                ((*list)->next)->prev =(*list);
            }
            else{
                //list member was the head 
                (*list)=(*list)->next;
                if ((*list)!= NULL){
                    //list had more than one member, 
                    //so the new head can't point to prev anymore (freed memory) 
                    (*list)->prev = NULL;
                }
            }
            free(q_aux);
        }
    }
    return ret_val;
}

int memoryClean(){
    MemFile* to_clean = NULL;
    //to free memory used for every file and its metadata we take every file from
    //the file list one by one, free its metadata that's allocated dynamically
    //and then the file structure itself
    while ((to_clean = DL_ListTake(&file_list, DEL)) != NULL){
        pthread_mutex_destroy(&(to_clean->mutex));
        free(to_clean->abspath);
        free(to_clean->pages);
        free(to_clean);
    }
    // take every page from the page list to free its memory
    while( pageTake(&(FileMemory.pages)) != -1){
        ;
    }
    //deallocate every page one by one
    int i;
    for(i=0; i<FileMemory.page_n; i++){
        free(FileMemory.memPtr[i]);
    }
    //deallocate the page array
    free(FileMemory.memPtr);
    hashTbDestroy(FileHashTb);
    return 0;
}

HashTable* hashTbMake(size_t size){
    HashTable* new_tb = malloc(sizeof(HashTable));
    new_tb->size = size;
    new_tb->table = malloc(sizeof(DL_List)*size);
    size_t i;
    for (i=0; i<size; i++){
        new_tb->table[i] = NULL;
    }
    return new_tb;
}
//Adds node file_list to a hash table
int hashTbAdd(HashTable* HTable, DL_List ** file_pos){
    MemFile* FilePtr = (*file_pos)->data;
    unsigned long key = hashKey(FilePtr->abspath);
    size_t pos = key % HASHTB_SIZE; 
    HashEntry* entry = malloc(sizeof(HashEntry));
    entry->data = file_pos;
    entry->key = key;
    DL_ListAdd(&(HTable->table[pos]),entry);
    return 0;
}
// Looks for entry in hashTb with corresponding key,
// returns its data field if found, NULL otherwise
void* hashTbSearch(HashTable* HTable, unsigned long key){
    size_t pos = key % HASHTB_SIZE; 
    DL_List* hlist = HTable->table[pos];
    while (hlist != NULL){
        HashEntry* found = DL_ListTake(&hlist,SRC);
        if(found == NULL)
            break;
        else if (key == found->key){
            void* file_pos = found->data;
            return file_pos;
        }
        hlist = hlist->next;
    }
    return NULL;
}
// calculates associated hash table key for given string
unsigned long hashKey(char* str){
    char* indx = str;
    unsigned long key = 0;
    while(*indx != '\0'){
       key += (unsigned long)*indx; 
       indx++;
    }
    return key;
}
MemFile* hashGetFile(HashTable* HashTb, char* abspath){
    MemFile* ret_file = NULL;
    unsigned long key = hashKey(abspath);
    printf("key is %zu\n",key);
    pthread_mutex_lock(&hashTB_mutex);
    DL_List** list_pos = hashTbSearch(HashTb,key);
    if (list_pos != NULL){
        ret_file = (*list_pos)->data;
        printf("file from hash is %s \n",((MemFile*)(ret_file))->abspath);
    }
    pthread_mutex_unlock(&hashTB_mutex);
    return ret_file;
}
int  hashTbDestroy(HashTable* TB){
    size_t i;
    DL_List* entry = NULL;
    for (i=0; i<TB->size; i++){
        while((entry = DL_ListTake(&(TB->table[i]),DEL)) != NULL){
            free(entry);
        }
    }
    free(TB->table);
    free(TB);
    return 0;
}
