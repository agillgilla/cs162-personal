/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

/* First block at the bottom of the heap */
struct mem_block *first_block = NULL;

/* Last block at the top of the heap */
struct mem_block *last_block = NULL;

/**
 * Pushes a new memory block to the list
 *
 * Return the pointer to the newly created mem_block
 */
struct mem_block *push_mem_block(size_t size) {
	/* Get the pointer to where the new block will reside */
	struct mem_block *new_block = (struct mem_block *) sbrk(0);
	/* Increment the data segment using sbrk().  If error on sbrk(), return NULL */
	void *status = sbrk(sizeof(struct mem_block) + size);
	if ((int) status < 0) return NULL;
	/* Initialize new block members */
	new_block->size = size;
	new_block->extra = 0;
	new_block->used = true;
	new_block->prev = last_block;
	new_block->next = NULL;

	/* Check if this is the first block we are adding */
	if (first_block == NULL && last_block == NULL) {
		/* Set this block as the first and last block */
		first_block = new_block;
		last_block = new_block;
		/* Set the previous and next blocks both to NULL */
		new_block->prev = NULL;
		new_block->next = NULL;
	} else {
		/* Set the last block's next to the block we are adding */
		last_block->next = new_block;
		/* Set the last block to the block we are creating */
		last_block = new_block;
	}
	return new_block;
}

/**
 * Splits the memory block passed as an arg
 *
 */
struct mem_block *split_mem_block(struct mem_block *block, size_t size) {
	size_t leftover = block->size + block->extra - size;
	
	struct mem_block *new_block = (void *) (block->memory + size);
	new_block->size = leftover - sizeof(struct mem_block);
	new_block->extra = 0;
	new_block->used = false;
	new_block->prev = block;
	new_block->next = block->next;

	block->size = size;
	block->extra = 0;
	block->used = true;
	block->next = new_block;

	if (block == last_block) {
		/* Set the last block's next to the block we are adding */
		block->next = new_block;
		/* Set the last block to the block we are creating */
		last_block = new_block;
	}
	
    return block;
}

/**
 * Combines the memory block passed as an arg
 * with the next memory block in the list
 */
void combine_mem_block(struct mem_block *block) {
	/* Get the block after the consuming block */
	struct mem_block *block_to_remove = block->next;

	/* Set the consuming block to the last block if the one being consumed is last */
	if (block_to_remove == last_block) {
		last_block = block;
	}

	/* Set the next pointer of this block to the block after the one we are removing */
	block->next = block_to_remove->next;
	/* Increase the size of the block that just consumed the block in front of it */
	block->size = block->size + sizeof(struct mem_block) + block_to_remove->size;

	/* Set the previous pointer of the block after the consuming block to point to the consuming block */
	if (block->next != NULL) {
		block->next->prev = block;
	}
}

/**
 * Utility function to mm_malloc without zeroing old data
 * Used only in mm_realloc
 */
void *mm_malloc_without_zero(size_t size) {
	/* Return null if requested size is 0 */
    if (size == 0) return NULL;
    
    if (first_block == NULL) { 
    	/* We are allocating for the first time */
    	struct mem_block *new_block = push_mem_block(size);
		
    	/* Return NULL if the block creation was unsuccessful */
		if (new_block == NULL) {
			return NULL;
		}

		return new_block->memory;
    } else {
    	struct mem_block *curr_block = first_block;
    	/* Keep iterating through list until we find an unused block with enough room */
    	while (true) {
    		/* Check if we found and unused block with enough room */
    		if (curr_block->used == false && curr_block->size + curr_block->extra >= size) {
    			size_t leftover = curr_block->size + curr_block->extra - size;
    			/* Check if we have enough left over to split */
    			if (leftover > sizeof(struct mem_block)) {
    				/* We can split into two */
    				return split_mem_block(curr_block, size)->memory;
    			} else {
    				/* We can use this block, but not enough left over to split */
    				curr_block->size = size;
    				curr_block->extra = leftover;
    				curr_block->used = true;

    				return curr_block->memory;
    			}
    		} 

    		if (curr_block->next == NULL) {
    			break;
    		}
			curr_block = curr_block->next;
		}

		if (curr_block == last_block) {
			/* We need to append a new block to the list */
	    	struct mem_block *new_block = push_mem_block(size);
			
	    	/* Return NULL if the block creation was unsuccessful */
			if (new_block == NULL) {
				return NULL;
			}
			return new_block->memory;
		}

		/* Execution should never reach here, so return NULL */
		return NULL;
    }
}

/**
 * Zero-fills the memory of a memory block and returns
 * the memory pointer
 */
void *zero_fill(struct mem_block *block) {
	/* Return null if the pointer is null */
	if (block == NULL) return NULL;

	/* Set the memory to all zeroes */
	memset(block->memory, 0, block->size);

	return (void *) block->memory;
}

void *mm_malloc(size_t size) {
	/* Return null if requested size is 0 */
    if (size == 0) return NULL;
    
    if (first_block == NULL) { 
    	/* We are allocating for the first time */
    	struct mem_block *new_block = push_mem_block(size);
		
    	/* Return NULL if the block creation was unsuccessful */
		if (new_block == NULL) {
			return NULL;
		}

		return zero_fill(new_block);
    } else {
    	struct mem_block *curr_block = first_block;
    	/* Keep iterating through list until we find an unused block with enough room */
    	while (true) {
    		/* Check if we found and unused block with enough room */
    		if (curr_block->used == false && curr_block->size + curr_block->extra >= size) {
    			size_t leftover = curr_block->size + curr_block->extra - size;
    			/* Check if we have enough left over to split */
    			if (leftover > sizeof(struct mem_block)) {
    				/* We can split into two */
    				return zero_fill(split_mem_block(curr_block, size));
    			} else {
    				/* We can use this block, but not enough left over to split */
    				curr_block->size = size;
    				curr_block->extra = leftover;
    				curr_block->used = true;

    				return zero_fill(curr_block);
    			}
    		} 

    		if (curr_block->next == NULL) {
    			break;
    		}
			curr_block = curr_block->next;
		}

		if (curr_block == last_block) {
			/* We need to append a new block to the list */
	    	struct mem_block *new_block = push_mem_block(size);
			
	    	/* Return NULL if the block creation was unsuccessful */
			if (new_block == NULL) {
				return NULL;
			}
			return zero_fill(new_block);
		}

		/* Execution should never reach here, so return NULL */
		return NULL;
    }
}

void *mm_realloc(void *ptr, size_t size) {
    if (ptr != NULL && size == 0) {
    	/* Operate like mm_free if pointer is null and size is 0 */
    	mm_free(ptr);
    	return NULL;
    } else if (ptr == NULL) {
    	/* Operate just like mm_malloc if pointer isn't null */
    	return mm_malloc(size);
    } else {
    	void *test_memory = mm_malloc(size);
    	/* Return null if we cannot allocate the specified amount */
    	if (test_memory == NULL) {
    		return NULL;
    	}

    	/* Free the test memory we mm_malloc'd */
    	mm_free(test_memory);

    	/* Get the mem_block to realloc */
    	struct mem_block *block_to_realloc = (struct mem_block *) (ptr - sizeof(struct mem_block));

    	/* Get the old size of the block to realloc */
    	size_t old_size = block_to_realloc->size;

    	/* Free the memory passed in to function */
    	mm_free(ptr);

    	/* Get the pointer to where the new block will reside */
		void *realloc_mem = mm_malloc_without_zero(size);

		if (size <= old_size) {
			/* Copy in the old memory */
			memcpy(realloc_mem, ptr, size);
		} else {
			/* Copy in the old memory */
			memcpy(realloc_mem, ptr, old_size);
			/* Get the bytes of extra memory that we need to zero-fill */
			size_t extra_mem = size - old_size;
			/* Zero fill empty memory */
			memset(realloc_mem + old_size, 0, extra_mem);
		}

		return realloc_mem;
    }   
}

void mm_free(void *ptr) {
	/* Do nothing if passed a null pointer */
    if (ptr == NULL) return;

    /* Cast the pointer to a mem_block pointer */
    struct mem_block *block_to_free = (struct mem_block *) (ptr - sizeof(struct mem_block));

    block_to_free->used = false;
    block_to_free->size = block_to_free->size + block_to_free->extra;
    block_to_free->extra = 0;

    if (block_to_free->next != NULL && block_to_free->next->used == false) {
    	combine_mem_block(block_to_free);
    }
    if (block_to_free->prev != NULL && block_to_free->prev->used == false) {
    	combine_mem_block(block_to_free->prev);
    }
}

void print_mem_structure() {
	if (first_block == NULL) {
		printf("No memory to print\n");
		return;
	}

	struct mem_block *curr_block = first_block;

	while (true) {
		printf("Block members:\n");
		printf("--------------\n");
    	printf("Block size: %zd\n", curr_block->size);
    	printf("Block extra: %zd\n", curr_block->extra);
    	printf("Block used: %s\n", curr_block->used ? "true" : "false");
    	if (curr_block->prev == NULL) {
    		printf("Block prev is null\n");
    	} else {
    		printf("Block prev is not null\n");
    	}
    	if (curr_block->next == NULL) {
    		printf("Block next is null\n");
    	} else {
    		printf("Block next is not null\n");
    	}
    	
    	printf("Block raw memory:\n");
    	printf("%s\n", curr_block->memory);
    	printf("=========================\n");

    	if (curr_block->next == NULL) {
    		break;
    	}
    	curr_block = curr_block->next;
	}

	if (curr_block == last_block) {
		printf("The block we ended on was the last block.  Good.\n");
	} else {
		printf("Oops! The block we ended on was NOT the last block.\n");
	}

}
