/* This file will contain your solution. Modify it as you wish. */
#include <types.h>
#include <lib.h>
#include <synch.h>
#include "cdrom.h"

/* Some variables shared between threads */



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
        
        kprintf("Received request read block %d\n",block_num);
        
        cdrom_block_request(block_num);
        
        return 0;
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
        (void) block_num;
        (void) value;

        
}

/*
* cdrom_startup: This is called at system initilisation time, 
* before the various threads are started. Use it to initialise 
* or allocate anything you need before the system starts.
*/

void cdrom_startup(void)
{

}   

/*
* cdrom_shutdown: This is called after all the threads in the system
* have completed. Use this function to clean up and de-allocate anything
* you set up in cdrom_startup()
*/
void cdrom_shutdown(void)
{
}


/* Just a sanity check to warn about including the internal header file 
   It contains nothing relevant to a correct solution and any use of 
   what is contained is likely to result in failure in our testing 
   */

#if defined(CDROM_TESTER_H) 
#error Do not include the cdrom_tester header file
#endif