#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include "ealloc.h"
// To implement dynamic memory manager, we use a page structure and each page has its own freeList and takenList. Also the memory it takes from OS through system call:-
// freeList : This linked list maintain which free memory location chunks are available in a page. It also stores the chunk size in each node. The next pointer points to the next chunk of free memory available in the page.
// takenList : This linked list maintains the record of the memory given to the user. It stores the address as well as the size of memory allocated of each chunk
// The concept is simple. We do not ask for any memory from OS at the start. But we ask for memory on DEMAND. If the memory demand cannot be satisfied by current number of pages, then we create a new page and give it memory from OS. Each page is of 4KB.
// WE USE WORST FIT TECHNIQUE FOR ALLOCATING MEMORY


// List node of freeList
struct LinkedListNode{
	int size;
	char* memory;
	struct LinkedListNode* next;
};

// List node of takenList
struct ListNode{
	char* memory;
	int size;
	struct ListNode* next;
};

// Each page  has the following structure
struct page{
	char* memory;
	struct LinkedListNode* freeList;	// freeList manages freechunks of memory. Initially its just one big chunk
	struct ListNode* takenList;         // takenList manages record of memory given to user during alloc()

};

// ***IMPORTANT*** 
struct page* pages[4];          // The array of pages we will maintain. Initially all of the are NULL as we create pages on demand   	
int pagesCreated;				// Pages created till now.

//-------------------------------------------------------------------------------------------------------------------------------


// initially all pages are NULL and pagesCreated are 0
void init_alloc()
{
	for(int i=0; i<4; i++) pages[i] = NULL;
	pagesCreated = 0;
}

//--------------------------------------------------------------------------
// Helper functions to check freeList and takenList of each page

// void printFreeList()
// {
// 	for(int i=0; i<pagesCreated; i++)
// 	{
// 		struct LinkedListNode* trav =pages[i]->freeList;
// 		int j=1;
// 		while(trav != NULL)
// 		{
// 			printf("page %d, chunk: %d, size: %d\n", i+1, j, trav->size);
// 			j++;
// 			trav = trav->next;
// 		}
// 	}
// }

// void printTakenList()
// {
// 	for(int i=0; i<pagesCreated; i++)
// 	{
// 		struct ListNode* trav =pages[i]->takenList;
// 		int j=1;
// 		while(trav != NULL)
// 		{
// 			printf("page %d, chunk: %d, size: %d\n", i+1, j, trav->size);
// 			j++;
// 			trav = trav->next;
// 		}
// 	}
// }
//----------------------------------------------------------------------------


// This is a function which allocates memory in a given page/
char* allocateMemoryInPage(struct page* p, int memoryNeeded)
{
	// If not a valid memory allocation or no free memory available, return null
	if(memoryNeeded % MINALLOC != 0 || p->freeList == NULL) return NULL;
	int maxAvailableMemory = 0;                  // Max available memory in freeList.
	char* maxMemoryLocation = NULL;              // Location of that max memory
	char* result = NULL;                 		 // To be returned to user as response to allocation request
	struct LinkedListNode* trav = p->freeList;
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
			if(memoryTakenFromNode == p->freeList) p->freeList = p->freeList->next;           // If memory is to be taken from head of freeList, then change head of the list
			else      // else it is somewhere in the freeList, So search for previous node of it, then free the memoryTakenFromNode
			{
				struct LinkedListNode* trav = p->freeList;
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
	if(p->takenList == NULL) p->takenList = node;    // If takenList is empty
	else 
	{	
		// else Find the end of takenList and add it to the end of list.
		struct ListNode* trav = p->takenList;
		while(trav->next != NULL) trav = trav->next;
		trav->next = node;
	}

	// Finally return the allocated memory address.
	return result;
}


// This is the function that the user will call. It uses helper function allocateMemoryInPage()
char *alloc(int memoryNeeded)
{
	char* result;  // To be returned to user as response to allocation request 

	// First we try to allocate memory in the pages already created
	for(int i=0; i<pagesCreated; i++)
	{
		result = allocateMemoryInPage(pages[i], memoryNeeded);
		if(result != NULL) return result;	// If success, then stop and return the address it allocated
	}
	 

	if(pagesCreated == 4) return NULL;	 	// Dont maintain more than 4 pages
	// Else we come here and create a new page and allocate it memory using system call and set its freeList and takenList appropriately. 
	struct page* newPage = (struct page*)malloc(sizeof(struct page));
	char* memory = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
	newPage->memory = memory;
	newPage->freeList = (struct LinkedListNode*)malloc(sizeof(struct LinkedListNode));
	newPage->freeList->size = PAGESIZE;
	newPage->freeList->memory = memory;
	newPage->freeList->next = NULL;
	newPage->takenList = NULL;
	pages[pagesCreated] = newPage;
	
	// Now we allocate memory in the newly created page. This time it has to be success or failure depending upon memory needed.
	result = allocateMemoryInPage(pages[pagesCreated], memoryNeeded);
	pagesCreated++;
	return result;
}


// This merge the  freeList nodes if they are adjacent to each other to form a bigger chunk
void mergeFreeList(struct page* p)
{
	if(p->freeList == NULL) return;

	// Just some traversal pointers
	struct LinkedListNode* trav1 = p->freeList;
	struct LinkedListNode* trav2 = p->freeList->next;
	while(trav2 != NULL)
	{
		// If two memory chunks are next to each other  i.e.
		// If first chunk end address is same as second chunk starting address, Then merge them
		if(trav1->memory + trav1->size == trav2->memory)
		{
			trav1->size = trav1->size + trav2->size;
			trav1->next = trav2->next;
			free(trav2);   // Free that node after merging
			trav2 = trav1->next;
		}
		else   // They are not adjacent
		{
			trav1 = trav2;
			trav2 = trav2->next;
		}
	}
}


void dealloc(char* ptr)
{
	// First we try to find the page number from which we allocated ptr
	int pageNo;
	for(int i=0;i<pagesCreated; i++)
	{
		if(pages[i]->takenList == NULL) continue;
		struct ListNode* trav = pages[i]->takenList;
		while(trav != NULL) 
		{
			if(trav->memory == ptr) break;  		// Found the page
			trav = trav->next;
		}


		// fif search was success, then break loop
		if(trav != NULL)
		{
			pageNo = i;          
			break;
		}
	}

	// First lets find out how much memory we allocated at this address by traversing the takenList of that page. 
	struct ListNode* trav = pages[pageNo]->takenList;
	struct ListNode* prev = NULL;
	while(trav->memory != ptr)
	{
		prev = trav;
		trav = trav->next;
	}

	int memoryFreed = trav->size;  // This much memory will be freed

	// Lets  remove that entry from takenList
	if(prev == NULL) pages[pageNo]->takenList = pages[pageNo]->takenList->next;  		// If at the head of the takenList, then remove head
	else
	{
		prev->next = trav->next;         // Else remove that node(trav) from takenList
	}
	free(trav);    // Free takenList node

	// Now we are done with takenList.
	// Lets start with freeList
	//First we create a node and add it to freeList at its appropriate position by comparing pointers of same type.
	// ******The head pointer of freeList always has smallest address on that page always. ********
	struct LinkedListNode* node = malloc(sizeof(struct LinkedListNode));
	node->memory = ptr;
	node->size = memoryFreed;
	node->next = NULL;
	if(pages[pageNo]->freeList == NULL)		// If freeList is empty
	{
		pages[pageNo]->freeList = node;
	}
	else if(pages[pageNo]->freeList->memory > ptr)  	// If memory freed is before the freeList first entry, then change freeList head
	{
		node->next = pages[pageNo]->freeList;
		pages[pageNo]->freeList = node;
	}
	else
	{
		// we need to find appropriate position for that node in freeList
		struct LinkedListNode* trav2 = pages[pageNo]->freeList->next;
		struct LinkedListNode* prev2 = pages[pageNo]->freeList;
		while(trav2 != NULL)
		{
			if(trav2->memory > ptr)
			{
				// If that node is not at the end of the list, then just add that node to freeList
				node->next = trav2;
				prev2->next = node;
				break;
			}
			prev2 = trav2;
			trav2 = trav2->next;
		}

		// But if at the end, then add at end.
		if(trav2 == NULL) prev2->next = node;
	}

	// This merge the adjacent free chunks in freeList of that page
	mergeFreeList(pages[pageNo]);
}

void cleanup()
{
	// For each page created
	for(int i=0; i<pagesCreated; i++)
	{
		struct LinkedListNode* trav1 = pages[i]->freeList;
		struct ListNode* trav2 = pages[i]->takenList;
		
		// Free its freeList
		while(trav1 != NULL)
		{
			struct LinkedListNode* temp = trav1;
			trav1 = trav1->next;
			free(temp);
		}

		// Then free its takenList
		while(trav2 != NULL)
		{
			struct ListNode* temp = trav2;
			trav2 = trav2->next;
			free(temp);
		}

		// Then free Page itself
		free(pages[i]);
	}		
}