#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include "alloc.h"

// To implement memory manager, we use a freeList and takenList to maintain the  following:-
// freeList : This linked list maintain which free memory location chunks are available in our page. It also stores the chunk size in each node. The next pointer points to the next chunk of free memory available in the page.
// takenList : This linked list maintains the record of the memory given to the user. It stores the address as well as the size of memory allocated of each chunk
// The concept is simple. When alloc() is called, we give memory from freelist and enter an entry into takenList. Similarly when dealloc() is called, we remove that  entry from takenList and add another entry into freeList
// WE USE WORST FIT TECHNIQUE FOR ALLOCATING MEMORY


// List type of freeList
struct LinkedListNode{
	int size;
	char* memory;
	struct LinkedListNode* next;
};

// List type of takenList
struct ListNode{
	char* memory;
	int size;
	struct ListNode* next;
};

// ***IMPORTANT*** 
char* memory;   					// Pointer to whole memory we will manage
struct LinkedListNode* freeList;	// freeList manages freechunks of memory. Initially its just one big chunk
struct ListNode* takenList;         // takenList manages record of memory given to user during alloc()



//------------------------------------------------------------------------------------------------//


// This merge the  freeList nodes if they are adjacent to each other to form a bigger chunk
void mergeFreeList()
{
	if(freeList == NULL) return;
	// Just some traversal pointers
	struct LinkedListNode* trav1 = freeList;
	struct LinkedListNode* trav2 = freeList->next;
	while(trav2 != NULL)
	{
		// If two memory chunks are next to each other
		if(trav1->memory + trav1->size == trav2->memory)
		{
			trav1->size = trav1->size + trav2->size;
			trav1->next = trav2->next;
			free(trav2);   // Free that node after merging
			trav2 = trav1->next;
		}
		else
		{
			trav1 = trav2;
			trav2 = trav2->next;
		}
	}
}

int init_alloc()
{
	// Getting memory from OS using system call
	memory = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0 );
	if(memory == MAP_FAILED) return -1;

	freeList = (struct LinkedListNode*)malloc(sizeof(struct LinkedListNode));
	freeList->size = PAGESIZE;
	freeList->memory = memory;
	freeList->next = NULL;
	takenList = NULL;
	return 0;
}


int cleanup()
{
	munmap(memory, PAGESIZE);
	struct LinkedListNode* trav1 = freeList;
	struct ListNode* trav2 = takenList;
	
	while(trav1 != NULL)
	{
		struct LinkedListNode* temp = trav1;
		trav1 = trav1->next;
		free(temp);
	}

	while(trav2 != NULL)
	{
		struct ListNode* temp = trav2;
		trav2 = trav2->next;
		free(temp);
	}
	return 0;		
}
char *alloc(int memoryNeeded)
{
	// If not a valid memory allocation or no free memory available, return null
	if(memoryNeeded % MINALLOC != 0 || freeList == NULL) return NULL;
	int maxAvailableMemory = 0;                  // Max available memory in freeList.
	char* maxMemoryLocation = NULL;              // Location of that max memory
	char* result = NULL;                 		 // To be returned to user as response to allocation request
	struct LinkedListNode* trav = freeList;
	struct LinkedListNode* memoryTakenFromNode;
	// Traverse the freeList to find the max available memory. MemoryTakenFromNode points to that node of freelist which contains the max available memory.
	// Trav is just a pointer for traversing

	while(trav != NULL)
	{
		if(trav->size > maxAvailableMemory)   // If size of that chunk is greater than last max available memory
		{
			memoryTakenFromNode = trav;
			maxMemoryLocation = trav->memory;
			maxAvailableMemory = trav->size;
		}  
		trav = trav->next;
	}

	// If we can't fullfill the request as there is no chunk in freeList of required memoryNeeded size.
	if(maxAvailableMemory < memoryNeeded) return NULL;  
	else
	{	
		result = maxMemoryLocation;        // To be returned to user

		// What if the size of that chunk was same as memoryNeeded, Then we must remove that chunk from freeList
		if(maxAvailableMemory == memoryNeeded)
		{
			if(memoryTakenFromNode == freeList) freeList = freeList->next;           // If memory is to be taken from head of freeList, then change head of the list
			else      // else it is somewhere in the freeList, So search for previous node of it, then free the memoryTakenFromNode
			{
				struct LinkedListNode* trav = freeList;
				while(trav->next != memoryTakenFromNode) trav = trav->next;
				trav->next = memoryTakenFromNode->next;  
			}
			free(memoryTakenFromNode); 
		}
		else  // We need to update memoryTakenFromNode size and memory pointer
		{
			memoryTakenFromNode->size = memoryTakenFromNode->size - memoryNeeded;
			memoryTakenFromNode->memory = (memoryTakenFromNode->memory + memoryNeeded);
		}
	}

	// We also need to add an entry to TakenList to keep record of what we gave. Create a node first, then add it to takenList. 
	struct ListNode* node = (struct ListNode*)malloc(sizeof(struct ListNode));
	node->memory = result;			// This memory has been given
	node->size = memoryNeeded;		// size of allocated memory
	node->next = NULL;
	if(takenList == NULL) takenList = node;    // If takenList is empty
	else 
	{	
		// else Find the end of takenList and add it to the end of list.
		struct ListNode* trav = takenList;
		while(trav->next != NULL) trav = trav->next;
		trav->next = node;
	}

	// Finally return the allocated memory address.
	return result;
}


void dealloc(char* ptr)
{
	// First lets find out how much memory we allocated at this address by traversing the takenList. 
	struct ListNode* trav = takenList;
	struct ListNode* prev = NULL;
	while(trav->memory != ptr)
	{
		prev = trav;
		trav = trav->next;
	}

	int memoryFreed = trav->size;  // This much memory will be freed
	if(prev == NULL) takenList = takenList->next;  		// If at the head of the takenList, then remove head
	else
	{
		prev->next = trav->next;         // Else remove that node(trav) from takenList
	}
	free(trav);    // Free takenList node

	// Now we are done with takenList.
	// Lets start with freeList
	//First we create a node and add it to freeList at its appropriate position. 
	struct LinkedListNode* node = malloc(sizeof(struct LinkedListNode));
	node->memory = ptr;
	node->size = memoryFreed;
	node->next = NULL;
	if(freeList == NULL)		// If freeList is empty
	{
		freeList = node;
		return;
	}
	else if(freeList->memory > ptr)  	// If memory freed is before the freeList first entry, then change freeList head
	{
		node->next = freeList;
		freeList = node;
	}
	else
	{
		// we need to find appropriate position for that node in freeList
		struct LinkedListNode* trav2 = freeList->next;
		struct LinkedListNode* prev2 = freeList;
		while(trav2 != NULL)
		{
			if(trav2->memory > ptr)
			{
				node->next = trav2;
				prev2->next = node;
				break;
			}
			prev2 = trav2;
			trav2 = trav2->next;
		}
		if(trav2 == NULL) prev2->next = node;
	}

	// This merge the adjacent free chunks in freeList
	mergeFreeList();
}

