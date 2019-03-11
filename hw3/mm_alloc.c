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
	if (sbrk(sizeof(struct mem_block) + size) < 0) return NULL;
	
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
    	while (curr_block != last_block) {
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
			curr_block = curr_block->next;
		}

		if (curr_block == last_block || curr_block == NULL) {
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
    /* YOUR CODE HERE */
    return NULL;
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
