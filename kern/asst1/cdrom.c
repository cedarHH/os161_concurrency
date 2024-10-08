/* This file will contain your solution. Modify it as you wish. */
#include <types.h>
#include <lib.h>
#include <synch.h>
#include "cdrom.h"

/* Some variables shared between threads */

typedef struct {
    int block_num;
    int next;
    int set;
    unsigned int value;
    struct semaphore *cdrom_sem;
} StaticLinkListNode;

typedef struct {
    int length,head,tail,reserveListHead;
    StaticLinkListNode *node;
} StaticLinkListRepr,*StaticLinkList;

struct lock *cdrom_lock_1, *cdrom_lock_2;
struct cv *cdrom_full, *cdrom_empty;
int count = 0;
StaticLinkList staticlist;

StaticLinkList list_init(int size);
void list_destroy(StaticLinkList list);
StaticLinkListNode *list_append(StaticLinkList list, int block_num);
StaticLinkListNode *list_get(StaticLinkList list, int block_num);
StaticLinkListNode *list_pop(StaticLinkList list, StaticLinkListNode *node);

StaticLinkList list_init(int size) {
    StaticLinkList list = (StaticLinkList) kmalloc(sizeof(StaticLinkListRepr));
    list->length = size + 1;
    list->head = 0;
    list->tail = 0;
    list->reserveListHead = 1;
    list->node = (StaticLinkListNode*) kmalloc((size + 1)*sizeof(StaticLinkListNode));
    list->node[list->head].block_num = -1;
    list->node[list->head].set = -1;
    list->node[list->head].next = -1;
    list->node[list->head].value = (unsigned int) -1;
    list->node[list->head].cdrom_sem = sem_create("",0);
    for(int i = 1; i < list->length-1; ++i){
        list->node[i].next = i+1;
        list->node[i].block_num = -1;
        list->node[i].set = -1;
        list->node[i].value = (unsigned int) -1;
        list->node[i].cdrom_sem = sem_create("",0);
    }
    list->node[list->length-1].next = -1;
    list->node[list->length-1].block_num = -1;
    list->node[list->length-1].set = -1;
    list->node[list->length-1].value = (unsigned int) -1;
    list->node[list->length-1].cdrom_sem = sem_create("",0);
    return list;
}


void list_destroy(StaticLinkList list) {
    for(int i = 0; i < list->length; ++i){
        sem_destroy(list->node[i].cdrom_sem);
    }
    kfree(list->node);
    kfree(list);
}

StaticLinkListNode *list_append(StaticLinkList list, int block_num) {
    if(list->reserveListHead == -1){
        return NULL;                       //Static list is full
    }
    int nextNode = list->reserveListHead;
    list->reserveListHead = list->node[list->reserveListHead].next;
    list->node[nextNode].next = -1;
    list->node[nextNode].block_num = block_num;
    list->node[list->tail].next = nextNode;
    list->tail = nextNode;
    return &list->node[nextNode];
}

StaticLinkListNode *list_get(StaticLinkList list, int block_num) {
    int curNode = list->head;
    for(;list->node[curNode].next != -1;curNode = list->node[curNode].next){
        if(list->node[list->node[curNode].next].block_num == block_num){
            if(list->node[list->node[curNode].next].set == 1){
                continue;
            }
            return &list->node[list->node[curNode].next];
        }
    }
    return NULL;
}

StaticLinkListNode *list_pop(StaticLinkList list, StaticLinkListNode *node) {
    int curNode = list->head;
    for(;list->node[curNode].next != -1;curNode = list->node[curNode].next){
        if(&list->node[list->node[curNode].next] == node){
            int target = list->node[curNode].next;
            if(target == list->tail){
                list->tail = curNode;
            }
            list->node[curNode].next = list->node[target].next;
            list->node[target].next = list->reserveListHead;
            list->reserveListHead = target;
            return &list->node[target];
        }
    }
    return NULL;
}

/*
* cdrom_read: This is the client facing interface to the cdrom that makes
* requests to the cdrom system. 
* 
* Args :-
* block_num: The block number of the cdrom that the client is requesting the
* content from.
*
* This function makes a request to the cdrom system using the cdrom_block_read()
* function. cdrom_block_read() returns immediately before the request has completed.
* cdrom_read() then puts the current thread to sleep until the request completes,
* upon which, this thread needs to the woken up, the value from the specific block
* obtained, and the value is return to the requesting client.
*/

unsigned int cdrom_read(int block_num)
{
        //kprintf("Received request read block %d\n",block_num);
        StaticLinkListNode *curReq;
        lock_acquire(cdrom_lock_1);
        while (count == MAX_CONCURRENT_REQ){
            cv_wait(cdrom_full,cdrom_lock_1);
        }
        ++count;
        curReq = list_append(staticlist,block_num);
        cdrom_block_request(block_num);
        cv_signal(cdrom_empty,cdrom_lock_1);
        lock_release(cdrom_lock_1);
        
        P(curReq->cdrom_sem);

        lock_acquire(cdrom_lock_2);
        unsigned int result = curReq->value;
        curReq->block_num = -1;
        curReq->set = -1;
        list_pop(staticlist,curReq);
        lock_release(cdrom_lock_2);
        return result;
}

/*
* cdrom_handler: This function is called by the system each time a cdrom block request
* has completed.
* 
* Args:-
* block_num: the number of the block originally requested from the cdrom.
* value: the content of the block, i.e. the data value retrieved by the request.
* 
* The cdrom_handler runs in its own thread. The handler needs to deliver the value 
* to the original requestor and wake the requestor waiting on the value.
*/

void cdrom_handler(int block_num, unsigned int value)
{
    lock_acquire(cdrom_lock_1);
    while (count == 0){
        cv_wait(cdrom_empty,cdrom_lock_1);
    }
    --count;
    cv_signal(cdrom_full,cdrom_lock_1);
    lock_release(cdrom_lock_1);

    lock_acquire(cdrom_lock_2);
    StaticLinkListNode *node  = list_get(staticlist,block_num);
    if(node){
        node->value = value;
        node->set = 1;
        V(node->cdrom_sem);
    }
    // while(node){
    //     node->value = value;
    //     node->set = 1;
    //     V(node->cdrom_sem);
    //     node = list_get(staticlist,block_num);
    // }
    lock_release(cdrom_lock_2);
}

/*
* cdrom_startup: This is called at system initilisation time, 
* before the various threads are started. Use it to initialise 
* or allocate anything you need before the system starts.
*/

void cdrom_startup(void)
{
    cdrom_lock_1 = lock_create("cdrom_lock_1");
    cdrom_lock_2 = lock_create("cdrom_lock_2");
    cdrom_full = cv_create("cdrom_full");
    cdrom_empty = cv_create("cdrom_empty");
    staticlist = list_init(MAX_CONCURRENT_REQ*MAX_CLIENTS);
}   

/*
* cdrom_shutdown: This is called after all the threads in the system
* have completed. Use this function to clean up and de-allocate anything
* you set up in cdrom_startup()
*/
void cdrom_shutdown(void)
{
    lock_destroy(cdrom_lock_1);
    lock_destroy(cdrom_lock_2);
    cv_destroy(cdrom_full);
    cv_destroy(cdrom_empty);
    list_destroy(staticlist);
}


/* Just a sanity check to warn about including the internal header file 
   It contains nothing relevant to a correct solution and any use of 
   what is contained is likely to result in failure in our testing 
   */

#if defined(CDROM_TESTER_H) 
#error Do not include the cdrom_tester header file
#endif