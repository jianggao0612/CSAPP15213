/*
 * mm.c
 * 
 * Gao Jiang
 * Andrew ID: gaoj
 *
 * Design idea - Use segregated list
 * 1. 10 segregated list using the size class of power 2
 * 2. Free blocks in seg list is ordered by size
 * 3. Malloc - find the needed space in seg list; if not, sbrk needed space. 
 * All reminder space returned back to seg list if larger than min
 * 4. Free - return back to seg list and keep order
 * 
 * CHUNKSIZE is tricky
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"
#include "contracts.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
//#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

/* Basic constants and macros */
#define WSIZE       4       		/* Word and header/footer size (bytes) */ 
#define DSIZE       8       		/* Double word size (bytes) */
#define CHUNKSIZE  (196)    		/* Extend heap by this amount (bytes) */ 
#define SEG_LIST_NUM 10     		/* Number of segregated lists */
#define SEG_LIST_START_SHIFT 2 		/* The index shift of seg_list */

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) 

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   
#define GET_ALLOC(p) (GET(p) & 0x1)                    

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given block ptr bp, compute the address of the next and previous free blocks */
#define NEXT_FREE_BLKP(bp)  (*(uint64_t **)bp)
#define PREV_FREE_BLKP(bp)  (*(uint64_t **)((char*)bp + DSIZE))

/* Given a new poiter, update the old pointer */
#define UPDATE_PRT(bp, p)   (bp = (uint64_t *)p)

/* Global variables */
static void **seg_list; 		// segregated list
static char *heap_listp = 0; 	// pointer to the first block in the heap 

/* Static function prototypes for checking memory blocks */
static int in_heap(const void *p);
static int aligned(const void *p);
static void mm_checkblock(void *p);

/* Static helper functions prototypes for seg list and heap operations */
static int seg_list_index(size_t size);
static void seg_list_insert(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void* bp, size_t asize);
static void *coalesce(void *bp);
static void remove_fb(void *bp);


/*
* Initialize: return -1 on error, 0 on success.
*/
int mm_init (void) {
	
	dbg_printf("Enter mm_init.\n");
	void* bp;

	/* Initialize the segregated list */
	seg_list = mem_sbrk(SEG_LIST_NUM * DSIZE);

	for (int i = 0; i <  SEG_LIST_NUM; i++) {
		seg_list[i] = (void *) NULL;
	}

	/* Create the initial empty heap */
	heap_listp = mem_sbrk(4 * WSIZE);

	if (heap_listp == NULL)
		return -1;

	PUT(heap_listp, 0);                          	/* Alignment padding */
	PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); 	/* Prologue header */ 
	PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); 	/* Prologue footer */ 
	PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     	/* Epilogue header */
	heap_listp += (2 * WSIZE);
	
	dbg_printf("head start:%p\n", heap_listp);

	if ((bp = extend_heap(CHUNKSIZE/WSIZE)) == NULL) {
		return -1;
	}

	seg_list_insert(bp);

	dbg_printf("mm_init finished.\n");
	return 0;
}

/*
* malloc - return pointer if malloc successfully, otherwise return NULL
*/
void *malloc (size_t size) {

	dbg_printf("Enter malloc. Size:%zu\n", size);

	/* Adjusted block size */
	size_t asize;

	/* Amount to extend heap if not fit */
	size_t extendsize;
	char *bp;

	/* Ignore spurious requests */
	if (size == 0)
		return NULL;

	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else 
		asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

	dbg_printf("Malloc asize: %zu.\n", asize);

	/* Search in seg list for a fit */
	if ((bp = find_fit(asize)) != NULL) {

		dbg_printf("Get fit %p. Size: %u.\n", bp, GET_SIZE(HDRP(bp)));
		
		place(bp, asize);
		dbg_printf("malloc finished. Find fit: %p.\n", bp);

		return bp;

	}

	/* No fit found. Get more memory from OS and place the block */
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {

		dbg_printf("Enter return NULL.\n");
		return NULL;

	}
	place(bp, asize);

	dbg_printf("malloc finished ptr:%p.\n", bp);

	return bp;
}

/*
* free - Free the given space
*/
void free (void *ptr) {
	
	// if the given pointer is null
	if (ptr == 0)
		return;

	dbg_printf("Enter free. Pointer:%p.\n", ptr);
	
	size_t size = GET_SIZE(HDRP(ptr));	// get size of the block

	dbg_printf("Free size:%zu.\n", size);

	/* Check whether the heap is empty */
	if (heap_listp == NULL)
		mm_init();

	// if free size is less than minimum size 
	if(size < 3 * DSIZE) {
		return;
	}
	
	PUT(HDRP(ptr), PACK(size, 0));			// update the header of the block
	PUT(FTRP(ptr), PACK(size, 0));			// update the footer of the block
	seg_list_insert(coalesce(ptr));			// return the block back to seg list

	dbg_printf("Header: Size:%u Alloc:%d.\n", GET_SIZE(HDRP(ptr)), GET_ALLOC(HDRP(ptr)));
	dbg_printf("Footer: Size:%u Alloc:%d.\n", GET_SIZE(FTRP(ptr)), GET_ALLOC(FTRP(ptr)));
	
	dbg_printf("Free finished.\n");
	return;

}

/*
* realloc - realloc a space based on the given pointer and size.
*/
void *realloc (void *oldptr, size_t size) {

	dbg_printf("Enter realloc. Pointer:%p. Size:%zu\n", oldptr, size);
	
	size_t old_size;
	size_t asize;
	size_t new_size;
	void *newptr;

	// if oldptr is NULL, call malloc to assign memory
	if (oldptr == NULL) 
		return malloc(size);

	// if size is 0, call free to free memory
	if (size == 0) {
		free(oldptr);
		return NULL;
	}

	old_size = GET_SIZE(HDRP(oldptr)); // get the old size of the space

	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else 
		asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

	if (asize == old_size) { // size after adjusted is the same as the old size
		
		dbg_printf("CASE 0\n");
		return oldptr;

	} else if (asize < old_size) { // realloc size is smaller than the old size, return the remainder size to seg list if larger than min
		
		dbg_printf("CASE 1\n");
		dbg_printf("oldsize - asize:%zu\n", old_size - asize);

		/* when the remainder block size larger than minimum block size, return it to the seg list */
		if ((old_size - asize) >= (3 * DSIZE)) {
			
			// change the header and footer of the old block
			PUT(HDRP(oldptr), PACK(asize, 1));
			PUT(FTRP(oldptr), PACK(asize, 1));

			// build up a new free block
			PUT(HDRP(NEXT_BLKP(oldptr)), PACK(old_size - asize, 0));
			PUT(FTRP(NEXT_BLKP(oldptr)), PACK(old_size - asize, 0));

			// insert the new free block to the seg list
			seg_list_insert(NEXT_BLKP(oldptr)); 

		}

		return oldptr;

	} else { // if the old size is less than the realloc size

		// check whether their free block after the original block, if so, use it with the original block
		if (!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && ((old_size + GET_SIZE(HDRP(NEXT_BLKP(oldptr)))) >= asize )) {
			
			dbg_printf("CASE 2\n");
			
			new_size = old_size + GET_SIZE(HDRP(NEXT_BLKP(oldptr))); // get the total size of the original block and next free block
			
			remove_fb(NEXT_BLKP(oldptr)); // remove the next block from seg lit

			// if the newly coalesced block is larger than the realloc size, decide whether to return the remainder to the seg list
			if ((new_size - asize) >= (3 * DSIZE)) {
			
				// change the header and footer of the original block
				PUT(HDRP(oldptr), PACK(asize, 1));
				PUT(FTRP(oldptr), PACK(asize, 1));

				// build up a new free block 
				PUT(HDRP(NEXT_BLKP(oldptr)), PACK(new_size - asize, 0));
				PUT(FTRP(NEXT_BLKP(oldptr)), PACK(new_size - asize, 0));

				// insert the new free block to seg list
				seg_list_insert(NEXT_BLKP(oldptr));

			} else {
				
					PUT(HDRP(oldptr), PACK(new_size, 1));
					PUT(FTRP(oldptr), PACK(new_size, 1));

			}

			return oldptr;

		} else { // if the next block is allocated or the total size is still less than the realloc size, malloc a new place for it
			
			dbg_printf("CASE 3\n");
			
			newptr = malloc(asize); // do malloc

			if (newptr != NULL) {

				old_size = (size < old_size ? size : old_size);
				memcpy(newptr, oldptr, old_size); // copy the data in the original block into the new block
				free(oldptr); // free the old block

			}

			return newptr;

		}

	}

	dbg_printf("realloc finished.\n");
}

/*
* calloc - you may want to look at mm-naive.c
* This function is not tested by mdriver, but it is
* needed to run the traces.
*/
void *calloc (size_t nmemb, size_t size) {

	size_t bytes = nmemb * size;
	void *newptr;

	newptr = malloc(bytes);
	memset(newptr, 0, bytes);

	return newptr;
}

/*
* seg_list_index - Return the index of the list in segregated list according to the size of the block
*					Least size of the free block in the seg list should be 24B
*/
static int seg_list_index(size_t size) {

	int index = 0;
	size = size / DSIZE; 

	// index for size of (2 to 4) to (2 to 5) 
	if (size < 4) 
		return 0;

	/*
	* Find index for the given size, put all size larger than (2 to 13) in the last list
	*/
	while ((index < SEG_LIST_NUM + SEG_LIST_START_SHIFT) && (size != 0)) {

		index++;
		size >>= 1; // Get the power of 2

	}

	if( index == SEG_LIST_START_SHIFT + SEG_LIST_NUM ) {
		return SEG_LIST_NUM - 1;
	}

	return index - SEG_LIST_START_SHIFT;

}

/*
* seg_list_insert - Insert free block into the segregated list, sorted by size in acsending order.
*
*/
static void seg_list_insert (void *bp) {

	dbg_printf("Enter insert %p.\n", bp);

	size_t size = GET_SIZE(HDRP(bp));
	dbg_printf("%p size: %u.\n", bp, GET_SIZE(HDRP(bp)));

	int seg_index = seg_list_index(size);
	dbg_printf("Insert index:%d.\n", seg_index);

	void *search_node = seg_list[seg_index];
	void *insert_pos = NULL;

	/* Go through the list to find the position to insert */
	while ((search_node != NULL) && (GET_SIZE(HDRP(search_node)) < size)) {

		insert_pos = search_node;
		search_node = (uint64_t *)NEXT_FREE_BLKP(search_node);

	}

	// Insert to the end of the list
	if ((search_node == NULL) && (insert_pos != NULL)) {
		
		dbg_printf("Enter 1.\n");
		
		UPDATE_PRT(NEXT_FREE_BLKP(insert_pos), bp);
		UPDATE_PRT(PREV_FREE_BLKP(bp), insert_pos);
		UPDATE_PRT(NEXT_FREE_BLKP(bp), NULL);	
		
		dbg_printf("bp:%p PREV(bp):%p NEXT(bp):%p.\n",bp, PREV_FREE_BLKP(bp), NEXT_FREE_BLKP(bp));
		dbg_printf("PREV(bp):%p PREV(PREV(bp)):%p NEXT(PREV(bp)):%p.\n",PREV_FREE_BLKP(bp), PREV_FREE_BLKP(PREV_FREE_BLKP(bp)), NEXT_FREE_BLKP(PREV_FREE_BLKP(bp)));

	}

	// Insert to the front of the list
	if ((search_node != NULL) && (insert_pos == NULL)) {
		
		dbg_printf("Enter 2.\n");
		dbg_printf("Search Node: %p.\n", search_node);

		UPDATE_PRT(NEXT_FREE_BLKP(bp), search_node);
		UPDATE_PRT(PREV_FREE_BLKP(search_node), bp);
		UPDATE_PRT(PREV_FREE_BLKP(bp), NULL);
		
		dbg_printf("bp:%p PREV(bp):%p NEXT(bp):%p.\n",bp, PREV_FREE_BLKP(bp), NEXT_FREE_BLKP(bp));

		seg_list[seg_index] = bp;

	}
	// Insert to the found postion in the list
	if ((search_node != NULL) && (insert_pos != NULL)) {
		
		dbg_printf("Enter 3.\n");

		UPDATE_PRT(NEXT_FREE_BLKP(bp), search_node);
		UPDATE_PRT(PREV_FREE_BLKP(bp), insert_pos);
		UPDATE_PRT(NEXT_FREE_BLKP(insert_pos), bp);
		UPDATE_PRT(PREV_FREE_BLKP(search_node), bp);

	}
	// Insert to the front when the list is empty
	if ((search_node == NULL) && (insert_pos == NULL)) {
		
		dbg_printf("Enter 4.\n");
		
		UPDATE_PRT(PREV_FREE_BLKP(bp), NULL);
		UPDATE_PRT(NEXT_FREE_BLKP(bp), NULL);
		
		dbg_printf("bp:%p PREV(bp):%p NEXT(bp):%p.\n",bp, PREV_FREE_BLKP(bp), NEXT_FREE_BLKP(bp));

		seg_list[seg_index] = bp;

	}

	dbg_printf("Finished insert.\n");
	return;

}

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words) {
	
	dbg_printf("Enter extend.\n");

    char *bp;
    size_t size;
	
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; 

    if ((bp = mem_sbrk(size)) == NULL)  
        return NULL;                                        

    /* Initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   
	PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ 
    
    dbg_printf("Finished extend.\n");

    /* Coalesce if the previous block was free */
    return coalesce(bp);

}

/*
 * find_fit - Find a fit free block from the seg list for the given adjusted size
 *			  return the ptr of the free block if found
 *			  return NULL if not
 */
static void *find_fit(size_t asize) {
	
	dbg_printf("Enter find_fit.\n");

	int seg_index = seg_list_index(asize);	// get the index in the seg list
	void *listp = NULL;

	/* 
	 * go through the seg list to find a fit
	 * if found a fit in the suitable list, return 
	 * if not, go to the next seg list until reaching the end of the list
	 */
	while (seg_index < SEG_LIST_NUM) {

		listp = seg_list[seg_index];

		while ((listp != NULL) && (GET_SIZE(HDRP(listp)) < asize)) 
			listp = (uint64_t *)NEXT_FREE_BLKP(listp);

		if (listp != NULL) {

			remove_fb(listp); // remove the block from seg list
			break;

		}

		seg_index++; // go to the next seg list

	}
	
	dbg_printf("Finished find_fit.\n");

	return listp;
}

/*
 * place - update the block header and footer for an allocated block
 *		   split the block and return the reminder space to the seg list if it is larger than the min free block size
 */
static void place (void* bp, size_t asize) {

	dbg_printf("Enter place. bp:%p size:%zu.\n", bp, asize);

	size_t csize = GET_SIZE(HDRP(bp));
	dbg_printf("Size of bp in place: %zu.\n", csize);
	
	// if the new size to be placed is larger than needed size by the min block size, return to seg list
	if ((csize - asize) > (3 * DSIZE)) {

		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		dbg_printf("******In place. bp: %p. size:%u.\n", bp, GET_SIZE(HDRP(bp)));
		
		PUT(HDRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
		dbg_printf("******In place. NEXT(bp): %p. size:%u.\n", NEXT_BLKP(bp), GET_SIZE(HDRP(NEXT_BLKP(bp))));

		seg_list_insert(NEXT_BLKP(bp));

	} else {
	
			PUT(HDRP(bp), PACK(csize, 1));
			PUT(FTRP(bp), PACK(csize, 1));

	}

	dbg_printf("Finished place.\n");
	return;

}

/*
 * coalesce - coalesce free blocks and insert the newly coalesced block into seg list
 */
static void *coalesce(void *bp) {

	dbg_printf("Enter coalesce %p.\n", bp);
	dbg_printf("PREV(bp):%p PREV(PREV(bp)):%p NEXT(PREV(bp)):%p.\n",PREV_BLKP(bp), PREV_FREE_BLKP(PREV_BLKP(bp)), NEXT_FREE_BLKP(PREV_BLKP(bp)));

	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));  // get the status of the previous block
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));	 // get the status of the next block
	size_t size = GET_SIZE(HDRP(bp));                    // get size of the given block

	//The previous and next blocks are both allocated
	if (prev_alloc && next_alloc) {
		
		dbg_printf("Enter 1.\n");
		dbg_printf("Finished coalesce.\n");
		return bp;

	} 

	// The previous block is free and the next block is allocated
	if (!prev_alloc && next_alloc) {
		
		dbg_printf("Enter 2.\n");

		// Remove the previous block from the seg list 
		remove_fb(PREV_BLKP(bp));

		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		
		if (size >= 3 * DSIZE) {

			PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
			PUT(FTRP(bp), PACK(size, 0));

			// Set the block as its previous block 
			bp = PREV_BLKP(bp);
		}
	}

	// The previou block is allocated and the next block is free
	if (prev_alloc && !next_alloc) {
		
		dbg_printf("Enter 3.\n");
		
		// Remove the next block from the seg list
		remove_fb(NEXT_BLKP(bp));

		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

		if(size >= 3 * DSIZE) {

			PUT(HDRP(bp), PACK(size, 0));
			PUT(FTRP(bp), PACK(size, 0));

		}
	}

	// The previous and next blocks are both free
	if (!prev_alloc && !next_alloc) {
		
		dbg_printf("Enter 4.\n");
		
		// Remove its previous and next blocks from the seg list
		remove_fb(PREV_BLKP(bp)); 
		remove_fb(NEXT_BLKP(bp));

		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
		
		if(size >= 3 * DSIZE) {	

			PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
			PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));

			// Set the free block as its previous block 
			bp = PREV_BLKP(bp);

		}
	}
	
	dbg_printf("Finished coalesce.\n");

	return bp;
}

/*
 * remove_fb - remove the given block from the seg list.
 */
static void remove_fb (void* bp) {
	
	dbg_printf("Enter remove_fb. %p\n", bp);

	size_t size = GET_SIZE(HDRP(bp));	// get size of the block
	int pos = seg_list_index(size);		// get the index of seg list of the block

	/* remove element from linkedList, considering the previous and next element */
	//if previous and next are both not NULL
	if ((PREV_FREE_BLKP(bp) != NULL) && (NEXT_FREE_BLKP(bp) != NULL)) { 
		
		dbg_printf("Enter 1.\n");
		dbg_printf("PREV_FREE_BLKP(bp):%p.\n", PREV_FREE_BLKP(bp));
		dbg_printf("NEXT_FREE_BLKP(bp):%p.\n", NEXT_FREE_BLKP(bp));
		
		UPDATE_PRT(NEXT_FREE_BLKP(PREV_FREE_BLKP(bp)), NEXT_FREE_BLKP(bp));
		UPDATE_PRT(PREV_FREE_BLKP(NEXT_FREE_BLKP(bp)), PREV_FREE_BLKP(bp));

	} else if ((PREV_FREE_BLKP(bp) == NULL) && (NEXT_FREE_BLKP(bp) != NULL)) { // if previous is NULL, and next is not
		
		dbg_printf("Enter 2.\n");
		
		// set NEXT to be the first
		UPDATE_PRT(PREV_FREE_BLKP(NEXT_FREE_BLKP(bp)), NULL);
		seg_list[pos] = (uint64_t *)NEXT_FREE_BLKP(bp);
 
	} else if ((PREV_FREE_BLKP(bp) != NULL) && (NEXT_FREE_BLKP(bp) == NULL)) { // if next is NULL, and previous is not
		
		dbg_printf("Enter 3.\n");
		
		UPDATE_PRT(NEXT_FREE_BLKP(PREV_FREE_BLKP(bp)), NULL);

	} else { // if both are NULL
		
		dbg_printf("Enter 4.\n");
		dbg_printf("bp header: Alloc %d Size:%u.\n", GET_ALLOC(HDRP(bp)), GET_SIZE(HDRP(bp)));
		
		seg_list[pos] = NULL;

	}
	
	dbg_printf("Finished remove_fb.\n");
	return;

}

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

/*
 * mm_checkblock - Check the block
 * 				   Check whether the block is in heap
 * 				   Check whether the block is aligned
 *				   Check whether the block size satisfied the minimum block size
 * 				   Check whether footer is the same as header
 */
 static void mm_checkblock(void *p) {

 	// Check in heap
 	if (in_heap(p) == 0) 
 		printf("Error: %p is not in heap.\n", p);

 	// Check alignment
 	if (aligned(p) == 0) 
 		printf("Error: %p is not aligned.\n", p);

 	// Check footer and header
 	if (GET(HDRP(p)) != GET(FTRP(p))) 
 		
 		printf("Error: Block footer doesn't match block footer. Ptr:%p.\n", p);
		dbg_printf("%p header size:%u head alloc:%d\n", p, GET_SIZE(HDRP(p)), GET_ALLOC(HDRP(p)));
		dbg_printf("%p footer size:%u footer alloc:%d\n", p, GET_SIZE(FTRP(p)), GET_ALLOC(FTRP(p)));


 }

/*
 * mm_checkheap - Check heap
 				  Check seg list
 				  Check free blocks
 */
void mm_checkheap (int lineno) {

	/* Heap check variables */
	char* heap_bp;
	int prev_alloc;
	int curr_alloc;
	int fb_count_heap = 0;

	/* Seg List check variables */
	void *prev_fp;
	void *curr_fp;
	int pos;
	int fb_count_list = 0;

	/* Check the heap */
	// Check prologue block
	if (!GET_ALLOC(HDRP(heap_listp)) || ((GET_SIZE(HDRP(heap_listp))) != DSIZE))
		printf("Error: Bad epilogue header.\n");
	mm_checkblock(heap_listp);

	// Check blocks in heap
	prev_alloc = 1;
	for (heap_bp = heap_listp; GET_SIZE(HDRP(heap_bp)) > 0; heap_bp = NEXT_BLKP(heap_bp)) {
		
		// Check current block
		mm_checkblock(heap_bp);

		// Check coalescing
		curr_alloc = GET_ALLOC(HDRP(heap_bp));
		if (!prev_alloc && !curr_alloc)
			printf("Error: Two consecutive free blocks in the heap.\n");

		// Count free blocks for checking seg list
		if (!curr_alloc)
			fb_count_heap++;

		// Move on to check
		prev_alloc = curr_alloc;

	}

	// Check epilogue block
	if (!GET_ALLOC(HDRP(heap_bp)) || (GET_SIZE(HDRP(heap_bp)) != 0)) 
		printf("Error: Bad prologue header. At pointer %p.\n", heap_bp);

	/* Check the seg list */
	for (pos = 0; pos < SEG_LIST_NUM; pos++) {

		prev_fp = NULL;
		curr_fp = seg_list[pos];

		while (curr_fp != NULL) {

			// Check consisdency
			if (prev_fp != NULL) {
				if(((uint64_t *)PREV_FREE_BLKP(curr_fp) != prev_fp) || ((uint64_t *)NEXT_FREE_BLKP(prev_fp) != curr_fp)) {
					
					dbg_printf("*****PREV(curr_fp):%p. Prev:%p\n", (uint64_t *)PREV_FREE_BLKP(curr_fp), prev_fp);
					dbg_printf("*****NEXT(prev_fp):%p. Curr:%p\n", (uint64_t *)NEXT_FREE_BLKP(prev_fp), curr_fp);
					printf("Error: Previous free block %p and free block %p is not consistent in seg list.\n", prev_fp, curr_fp);
				}
			}

			// Check free block in heap
			if (!in_heap(curr_fp))
				printf("Error: Free block %p is not in heap range.\n", curr_fp);
			
			// Check right bucket for post = SEG_LIST_NUM - 1
			if ((pos == SEG_LIST_NUM - 1) && (GET_SIZE(HDRP(curr_fp)) < (unsigned)(1 << (pos + WSIZE)))) {
				
				dbg_printf("Error: Free block size:%u\n", GET_SIZE(HDRP(curr_fp)));
				printf("Error: Free block %p is not in the correct bucket. Bucket:%d\n", curr_fp, pos);
			
			}

			// Check right bucket for pos < SEG_LIST_NUM - 1
			if ((pos < SEG_LIST_NUM - 1) && (!(GET_SIZE(HDRP(curr_fp)) >= (unsigned)(1 << (pos + WSIZE))) || !(GET_SIZE(HDRP(curr_fp)) < (unsigned)(1 << (pos + WSIZE + 1))))) {
				
				dbg_printf("Error: Free block size:%u\n", GET_SIZE(HDRP(curr_fp)));
				printf("Error: Free block %p is not in the correct bucket.Bucket:%d\n", curr_fp, pos);
		 	
		 	} 
			// Count free blocks
			fb_count_list++;

			// Move on to check
			prev_fp = curr_fp;
			curr_fp = (uint64_t *)NEXT_FREE_BLKP(curr_fp);

		}

	}

	/* Check the free block counts */
	if (fb_count_heap != fb_count_list)
		
		printf("Error: The count of free blocks in heap doesn't match that in seg list.\n");
		dbg_printf("heap count:%d seg count:%d.\n", fb_count_heap, fb_count_list);

}

