/*
 * mm.c
 * 
 * Gao Jiang
 * Andrew ID: gaoj
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
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
#define WSIZE       4       /* Word and header/footer size (bytes) */ 
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */ 
#define SEG_LIST_NUM 10     /* Number of segeragated lists */

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
#define NEXT_FREE_BLKP(bp)  ((uint32_t *)(*bp))
#define PREV_FREE_BLKP(bp)  ((uint32_t *)(*(bp + DSIZE)))

/* Global variables */
static void **seg_list; // segeragated list
static char *heap_listp = 0; // pointer to the first block in the heap 

/* Static function prototypes for checking memory blocks */
static int in_heap(const void *p);
static int aligned(const void *p);
static void mm_checkblock(void *p);

static void *coalesce(void *bp);

/* Remove free blocks from segrageted free lists */
static void remove_fb(void *bp);


/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {

	void *bp;

    /* Create the initial empty heap */
	heap_list_p = mem_sbrk(4 * WSIZE);
	if (heap_list_p == -1)
		return -1;
	PUT(heap_listp, 0);                          	/* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); 	/* Prologue header */ 
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); 	/* Prologue footer */ 
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     	/* Epilogue header */
    heap_listp += (2 * WSIZE);

    /* Initialize the segregated list */
    seg_list = mem_sbrk(SEG_LIST_NUM * DSIZE);

    for (int i = 0; i <  SEG_LIST_NUM; i++) {
    	seg_list[i] = (void *) NULL;
    }

    return 0;
}

/*
 * seg_list_index - Return the index of the list in segregated list according to the size of the block
 *					Least size of the free block in the seg list should be 16b (use 4 bytes addr since heap size is less than 2 to 32)
 */
static int seg_list_index(size_t size) {

	int index = 0;

	size = size / (WSIZE * DSIZE);

	// index for size of (2 to 4) to (2 to 5) 
	if (size < 0) 
		return 0;

	/*
	 * Find index for the given size, put all size larger than (2 to 13) in the last list
	 */
	while ((index < SEG_LIST_NUM - 1) && (size != 0)) {

		index++;
		size >>= 1; // Get the power of 2

	}

	return index - 2;

}
/*
 * seg_list_insert - Insert free block into the segregated list, sorted by size in acsending order.
 *
 */
static void seg_list_insert(void *bp) {

	size_t size = GET_SIZE(HDRP(bp));
	int seg_index = seg_list_index(size);
	void* search_node = seg_list[seg_index];
	void* insert_pos = NULL;

	/*
	 * Go through the list to find the position to insert
	 */
	while ((search_node != NULL) && (GET_SIZE(HDRP(search_node)) < size) {
		insert_pos = search_node;
		search_node = NEXT_FREE_BLKP(search_node);
	}

	// Insert to the end of the list
	if ((search_node == NULL) && (insert_pos != NULL)) {

		NEXT_FREE_BLKP(insert_pos) = bp;
		PREV_FREE_BLKP(bp) = insert_pos;
		NEXT_FREE_BLKP(bp) = NULL;

	}
	// Insert to the front of the list
	if ((search_node != NULL) && (insert_pos == NULL)) {

		NEXT_FREE_BLKP(bp) = search_node;
		PREV_FREE_BLKP(search_node) = bp;
		PREV_FREE_BLKP(search_node) = NULL;
		seg_list[seg_index] = bp;

	}
	// Insert to the found postion in the list
	if ((search_node != NULL) && (insert_pos != NULL)) {

		NEXT_FREE_BLKP(bp) = search_node;
		PREV_FREE_BLKP(bp) = insert_pos;
		NEXT_FREE_BLKP(insert_pos) = bp;
		PREV_FREE_BLKP(search_node) = bp;

	}
	// Insert to the front when the list is empty
	if ((search_node == NULL) && (insert_pos == NULL)) {

		PREV_FREE_BLKP(bp) = NULL;
		NEXT_FREE_BLKP(bp) = NULL;
		seg_list[seg_index] = bp;

	}

	return;

}

/*
 * malloc
 */
void *malloc (size_t size) {

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

	/* Search in seg list for a fit */
	if ((bp = find_fit(asize)) != NULL) {
		
		place(bp, asize);
		return bp;

	}

	/* No fit found. Get more memory from OS and place the block */
	enxtendsize = MAX(asize, CHUNKSIZE);
	if (bp = extend_heap(extendsize / WSIZE) == NULL)
		return NULL;
	place(bp, asize);

	return bp;
}

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words) {

    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; 
    if ((long)(bp = mem_sbrk(size)) == -1)  
        return NULL;                                        

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ 

    /* Coalesce if the previous block was free */
    return coalesce(bp);

}

/*
 * find_fit - Find a fit free block from the seg list for the given adjusted size
 *			  return the ptr of the free block if found
 			  return NULL if not
 */
static void *find_fit(size_t asize) {

	int seg_index = seg_list_index(asize);
	void* listp = NULL;

	while (seg_index < SEG_LIST_NUM) {

		listp = seg_list[seg_index];

		while ((listp != NULL) && GET_SIZE(HDRP(listp) < asize)) 
			listp = NEXT_FREE_BLKP(listp);

		if (listp != NULL) {
			remove_fb(listp);
			break;
		}

		seg_index++;

	}

	return listp;
}

/*
 * place - update the block header and footer for an allocated block
 		   split the block and return the reminder space to the seg list if it is larger than the min free block size
 */
static void place(void* bp, size_t asize) {

	size_t csize = GET_SIZE(HDRP(bp));

	if ((csize - asize) > 3 * DSIZE) {

		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		PUT(HDRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));

		seg_list_insert(NEXT_BLKP(bp));

	} else {

		PUT(HDRP(bp), PACK(csize, 0));
		PUT(FTRP(bp), PACK(csize, 0));

	}

}

/*
 * free
 */
void free (void *ptr) {

	size_t size;

	/* 
	 * Check whether the heap is empty
	 * if so, illegal free operation
	 */
	if (heap_listp == NULL) {

		printf("Segmentation Fault.\n");
		return;

	}


    if(!ptr) {

    	size = GET_SIZE(HDRP(ptr));		// get size of the block
    	PUT(HDRP(ptr), PACK(size, 0));	// update the header of the block
    	PUT(FTRP(ptr), PACK(size, 0));	// update the footer of the block
    	seg_list_insert(coalesce(ptr));			// return the block back to seg list

    } else {

    	printf("Segmentation Fault.\n");	// illegal free operation
    }

    return;

}

static void *coalesce(void *bp) {

	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	//The previous and next blocks are both allocated
	if (prev_alloc && next_alloc) {

		return bp;

	} 

	// The previous block is free and the next block is allocated
	if (!prev_alloc && next_alloc) {

		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));

		/*
		 * Set the block as its previous block
		 * Remove itself from the seg list
		 */
		remove_fb(bp);
		bp = PREV_BLKP(bp);

	}

	// The previou block is allocated and the next block is free
	if (prev_alloc && !next_alloc) {

		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));

		// Remove the next block from the seg list
		remove_fb(NEXT_BLKP(bp));

	}

	// The previous and next blocks are both free
	if (!prev_alloc && !next_alloc) {

		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));

		/* 
		 * Set the free block as its previous block
		 * Remove itself and its next from the seg list
		 */
		remove_fb(bp); 
		remove_fb(NEXT_BLKP(bp));
		bp = PREV_BLKP(bp);

	}

	return bp;
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {

	size_t old_size;
	size_t asize;
	size_t new_size;
	void* searchptr;
	void* newptr;

	// if oldptr is NULL, call malloc to assign memory
	if (oldptr == NULL) 
		return malloc(size);
	// if size is 0, call free to free memory
	if (size == 0) {
		free(oldptr);
		return NULL;
	}

	old_size = GET_SIZE(HDRP(oldptr));

	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else 
		asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

	if (asize == old_size) {

		return oldptr;

	} else if (asize < old_size) {

		if (old_size - asize > 3 * DSIZE) {

			PUT(HDRP(NEXT_BLKP(oldptr)), PACK(old_size - asize, 0));
			PUT(FTRP(NEXT_BLKP(oldptr)), PACK(old_size - asize, 0));
			PUT(HDRP(oldptr), PACK(asize, 1));
			PUT(FTRP(oldptr), PACK(asize, 1));

			seg_list_insert(NEXT_BLKP(oldptr));

		} else {

			PUT(HDRP(oldptr), PACK(asize, 1));
			PUT(FTRP(oldptr), PACK(asize, 1));

		}

		return oldptr;

	} else {

		searchptr = oldptr;
		new_size = old_size;

		while(!GET_ALLOC(NEXT_BLKP(searchptr)) && ((new_size + GET_SIZE(NEXT_BLKP(searchptr))) < asize )) {
			new_size += GET_SIZE(NEXT_BLKP(searchptr));
			searchptr = NEXT_BLKP(searchptr);
		}

		if (new_size >= asize) {

			if ((new_size - asize) > 3 * DSIZE) {

				PUT(HDRP(NEXT_BLKP(searchptr)), PACK(new_size - asize, 0));
				PUT(FTRP(NEXT_BLKP(searchptr)), PACK(new_size - asize, 0));
				PUT(HDRP(oldptr), PACK(asize, 1));
				PUT(FTRP(oldptr), PACK(asize, 1));

				seg_list_insert(NEXT_BLKP(searchptr));

			} else {

				PUT(HDRP(oldptr), PACK(asize, 1));
				PUT(FTRP(oldptr), PACK(asize, 1));

			}

			return oldptr;

		} else {

			newptr = malloc(asize);

			if (newptr != NULL) {
				memcpy(newptr, oldptr, old_size);
				free(oldptr);
			}

			return newptr;

		}

	}


}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {
    return NULL;
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
 * Call functions to check the block
 * Check whether the block is in heap
 * Check whether the block is aligned
 * Check whether footer is the same as header
 */
 static void mm_checkblock(void *p) {

 	// check in heap
 	if (in_heap == 0) {

 		printf("Error: %p is not in heap.\n" + p);

 	}

 	// check alignment
 	if (aligned == 0) {

 		printf("Error: %p is not aligned.\n" + p);

 	}

 	// check footer and header
 	if (GET(HDRP(p)) != GET(FTRP(p))) {

 		printf("Error: Block footer doesn't match block footer.\n");

 	}
 }

/*
 * mm_checkheap
 */
void mm_checkheap(int lineno) {
}

