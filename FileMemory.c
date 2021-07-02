#include "FileMemory.h"
#include "config.h"


size_t page_num;
pthread_mutex_t hashTB_mutex    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pages_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t filestack_mutex = PTHREAD_MUTEX_INITIALIZER;
HashTable* FileHashTb = NULL;
FileStack* FStack;
    
// Adds an integer representing a free memory page to pageList,
// locks pages_mutex while operating 
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
// Takes a page from page list, returns -1 if none is found (they're all taken)
// locks pages_mutex while operating
int pageTake(PageList** list){
    pthread_mutex_lock(&pages_mutex);
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
        pthread_mutex_unlock(&pages_mutex);
    return ret_val;
}


PageList* pageListCreate(){
    page_num = server_memory_size / page_size;
    PageList* new_list = NULL;
    int i;
    for(i=0; i<page_num; i++){
        pageAdd(i, &new_list);
    }
    return new_list;
}

// writes contents from input of given size into requested file's page starting from offset
// pag_ind is the number of the page (0,1,2,...)
// returns -1 if the requested write would have written out of bounds (refused), caused by an excessive size or offset
int addPageToMem(const void* input, const MemFile* FilePtr, const  int page_ind, const size_t size, const  int offset){
    if(size > page_size-offset){
       fprintf(stderr,"error, size is larger than a full page\n");
        return -1;
    }
    int index = FilePtr->pages[page_ind];
    memcpy( (FileMemory.memPtr[index])+offset, input, size);
    return 0;
}

int memorySetup(){
    FileMemory.pages = pageListCreate();
    FileMemory.page_n= page_num;
    FileMemory.file_n = 0;

    if ((FileMemory.memPtr = malloc(sizeof(void*)* FileMemory.page_n)) == NULL){
        printf("Fatal Error! Server memory malloc failed.\n");
        return -1;
    }
    int i;
    for (i=0; i<FileMemory.page_n; i++){
        (FileMemory.memPtr[i]) = malloc(page_size);
        memset(FileMemory.memPtr[i], 0, page_size);
    }
    FileHashTb = hashTbMake(file_hash_tb_size);
    FStack = fileStackInit(file_max);
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
    void* ret_val = NULL;
    if( *list == NULL){
        ret_val = NULL;
    }
    else{
        ret_val = ((*list)->data);
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
    // take every page from the page list to free its memory
    hashTbDestroy(FileHashTb);
    fileStackDelete(FStack);
    while( pageTake(&(FileMemory.pages)) != -1){
        ;
    }
    //deallocate every page one by one
    int i;
    for(i=0; i<FileMemory.page_n; i++){
        free(FileMemory.memPtr[i]);
    }
    free(FileMemory.memPtr);
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
//Adds data field to a hash table on position defined by key
int hashTbAdd(HashTable* HTable,void* data, unsigned long key){
    size_t pos = key % file_hash_tb_size; 
    HashEntry* entry = malloc(sizeof(HashEntry));
    entry->data = data;
    entry->key = key;
    DL_ListAdd(&(HTable->table[pos]),entry);
    return 0;
}

// Looks for entry in hashTb with corresponding key,
// returns its data field if found, NULL otherwise
// if flag is set to DEL (1) the hash entry is removed from the table
void* hashTbSearch(HashTable* HTable, unsigned long key, int flag){
    size_t pos = key % file_hash_tb_size; 
    DL_List* hlist = (HTable->table[pos]);
    int isHead = 1 ;
    while (hlist != NULL){
        HashEntry* found = DL_ListTake(&hlist,SRC);
        if(found == NULL){
            break;
        }
        else if (key == found->key){
            void* data = found->data;
            if(flag == DEL){
                if (isHead){
                    DL_List* aux = HTable->table[pos];
                    HTable->table[pos] = HTable->table[pos]->next;
                    if (HTable->table[pos] != NULL)
                        HTable->table[pos]->prev = NULL;
                    free(aux);
                    printf("tried deleteing with isHead\n");
                }
                else{
                    DL_ListDeleteCell(hlist);
                }
            free(found);
            }
            return data;
        }
        hlist = hlist->next;
        isHead = 0;
    }
    return NULL;
}
// calculates associated hash key for given string
unsigned long hashKey(char* str){
    int incr = 1;
    char* indx = str;
    unsigned long key = 0;
    while(*indx != '\0'){
        key += ((int)*indx * incr*8);
        indx++;
        incr++;
    }
    return key;
}
// Searches for a File Record in HashTb for a file with name abspath,
// returns it if found, NULL otherwise,
// if flag is set to DEL (1) the entry is removed,
// mutex hashTb_mutex is locked when operating
MemFile* hashGetFile(HashTable* HashTb, char* abspath, int flag){
    MemFile* ret_file = NULL;
    unsigned long key = hashKey(abspath);
    printf("key is %zu\n",key);
    pthread_mutex_lock(&hashTB_mutex);
    ret_file = (MemFile*) hashTbSearch(HashTb,key, flag);
    pthread_mutex_unlock(&hashTB_mutex);
    return ret_file;
}


//assumes data is a MemFile*
int  hashTbDestroy(HashTable* TB){
    size_t i;
    HashEntry* entry = NULL;
    for (i=0; i<TB->size; i++){
        while(1){
            entry = DL_ListTake(&(TB->table[i]), DEL);
            if (entry == NULL)
                break;
            fileFree((MemFile*)(entry->data));
            free(entry);
        }
    }
    free(TB->table);
    free(TB);
    pthread_mutex_destroy(&hashTB_mutex);
    return 0;
}

FileStack* fileStackInit(int size){
    FileStack* stack = malloc(sizeof(FileStack));
    stack->top = 0;
    stack->max= size * 2 -1;
    (stack->stack) = malloc(sizeof(MemFile*)*stack->max);
    int i;
    for(i=0; i<stack->max; i++){
        stack->stack[i] = 0; 
    }
    return stack;
}

// Adds a file key to the fileStack and refreshes the top position
// locks filestack_mutex
int fileStackAdd(FileStack* stack,unsigned long file_key){
    pthread_mutex_lock(&filestack_mutex);
    if (stack->top == stack->max){
        fileStackDefrag(stack);
    }
    int top = stack->top;
    stack->stack[top] = file_key;
    (stack->top)++;
    pthread_mutex_unlock(&filestack_mutex);
    return 0;
}

int fileStackRemove(FileStack* stack){
    stack->top--;
    stack->stack[stack->top] = 0;
    return 0;
}

// clears any empty cells from the filestack and updates its top position
// locks filestack_mutex
int fileStackDefrag(FileStack* stack){
    pthread_mutex_lock(&filestack_mutex);
    int i;
    int last = 0;
    for(i=0; i<stack->max; i++){
        if(stack->stack[i] != 0){
            int j;
            //check if stack contains duplicates
            for(j=i+1; j<stack->max; j++){
                if (stack->stack[i] == stack->stack[j])
                    stack->stack[j] = 0;
            }
            // has the file been deleted from the hash table
            pthread_mutex_lock(&hashTB_mutex);
            MemFile* found = hashTbSearch(FileHashTb, stack->stack[i], SRC);
            pthread_mutex_unlock(&hashTB_mutex);
            if (found != NULL){
                stack->stack[last] = stack->stack[i]; 
                if (last < i){
                    //found at least an empty space, so delete stack[i] (stack would contain duplicates)
                    stack->stack[i] = 0;
                }
                last++;
            }
            else{
                stack->stack[i] = 0;
            }
        }
    }
    stack->top = last;
    pthread_mutex_unlock(&filestack_mutex);
    return 0;
}


int fileStackDelete(FileStack* stack){
    free(stack->stack);
    free(stack);
    return 0;
}

// Look for the most recently added file in File Stack with hash not equal to key,
// remove it from stack and return it.
//top position changes if the first file found wasn't defined by key
//returns NULL if no file is found in the stack 
// mutex hashTB_mutex is locked while looking for file to delete
MemFile* fileStackGetTop(FileStack* stack,double key){
    int foundSelf = -1;
    int indx = stack->top;
    if (indx > 0){
        indx--;
    }
    else{
        fprintf(stderr,"No file found in stack!\n");
        return NULL;
    }
    pthread_mutex_lock(&hashTB_mutex);
    if (stack->stack[indx] == key){
       foundSelf = indx;
       indx--; 
    }
    MemFile* found = hashTbSearch(FileHashTb, stack->stack[indx], SRC);
    while (found == NULL && indx >= 0){
        indx--;
        found = hashTbSearch(FileHashTb,stack->stack[indx], SRC);
    } 
    if (foundSelf != -1){
        printf("found myself!\n");
        stack->top = foundSelf+1;
    }
    else{
        stack->top = indx;
    }
    pthread_mutex_unlock(&hashTB_mutex);
    return found;
}

//Removes file record from hashtable and deallocates memory used for it.
// File is not locked during deletion.
int fileFree(MemFile* FilePtr){
    free(FilePtr->abspath);
    int i;
    if (FilePtr->pages != NULL){
        for(i=0; i<FilePtr->pages_n; i++){
            pageAdd(FilePtr->pages[i], &(FileMemory.pages));
        }
        free(FilePtr->pages);
    }
    DL_List* to_del = NULL;
    to_del = FilePtr->clients_opened;
    while (to_del != NULL){
       free(to_del->data);
       DL_List* aux = to_del;
       to_del = to_del->next;
       free(aux);
    }
    pthread_mutex_destroy(&FilePtr->mutex);
    free(FilePtr);
    return 0;
}

int DL_ListDeleteCell(DL_List* list_cell){
    if (list_cell == NULL){
        return -1;
    }
    DL_List* aux = list_cell;
    list_cell = list_cell->prev;
    list_cell->next = list_cell->next->next;
    list_cell->next->prev = list_cell;
    free(aux);
    return 0;
}

int clientOpenSearch(DL_List* list,int fd){
    while( list != NULL){
        int list_fd = *((int*)list->data);
        if (list_fd  == fd){
            return 1;
        }
        list = list->next;
    }
    return 0;
}

int filePagesInitialize(MemFile* FilePtr){
    FilePtr->pages_n = FilePtr->size / page_size + 1;
    if ((FilePtr->pages = malloc(sizeof(int) * (FilePtr->pages_n)) ) == NULL){
       printf("error file pages malloc!\n");
       return -1;
    }
    int i;
    double file_key = hashKey(FilePtr->abspath);
    for(i=0; i<(FilePtr->pages_n); i++){
       (FilePtr->pages)[i] = pageGet((&(FileMemory.pages)),file_key);
    }
    return 0;
}
int filePagesRenew(MemFile* file, int dim){
    if (dim == file->pages_n)
        return 0;
    int* new_page_Arr = NULL;
    if ((new_page_Arr = malloc(sizeof(int)*dim) )== NULL){
        fprintf(stderr,"error allocating space for file page array\n");
        return -1;
    }
    int i;
    for (i=0; i<file->pages_n; i++){
        new_page_Arr[i] = file->pages[i];
    }
    double file_key = hashKey(file->abspath);
    for (i=(file->pages_n) ;i<dim; i++){
        new_page_Arr[i] = pageGet(&FileMemory.pages, file_key);
    }
    free(file->pages);
    file->pages_n = dim;
    file->pages = new_page_Arr;
    return 0;
}

// Look for a file to delete (FIFO order) with hash different than key,
// return 0 on successful deletion, -1 otherwise
// locks filestack_mutex
int fileDeleteFIFO(PageList** list,double key){
    pthread_mutex_lock(&filestack_mutex);
    MemFile* to_del = fileStackGetTop(FStack,key);
    if (to_del == NULL){
        fprintf(stderr,"No file was found to delete!\n");
        pthread_mutex_unlock(&filestack_mutex);
        return -1;
    }
    pthread_mutex_lock(&to_del->mutex);
    hashGetFile(FileHashTb, to_del->abspath, DEL);
    printf("Deleting file %s to free up memory!\n",to_del->abspath);
    pthread_mutex_unlock(&to_del->mutex);
    pthread_mutex_unlock(&filestack_mutex);
    fileFree(to_del);
    return 0;
}
// Get a page from page list, if list is empty request a file deletion
// to free up pages
int pageGet(PageList** list,double key){
    int ret_val = pageTake(list);
    while(ret_val == -1){
        fileDeleteFIFO(list,key);
        ret_val = pageTake(list);
    }
    return ret_val;
}

