/*
 * mm_alloc.h
 *
 * A clone of the interface documented in "man 3 malloc".
 */

#pragma once

#include <stdlib.h>
#include <stdbool.h>

struct mem_block {
	size_t size;			/* Size of memory allocated for this block (not including struct) */
	size_t extra;			/* Amount of extra memory after this block and before the next that can't be used */
    bool used;				/* Boolean that stores whether memory is in use or not */
    struct mem_block* prev;	/* A pointer to previous memory block in list */
    struct mem_block* next;	/* A pointer to next memory block in list */
    //void *mem_ptr;			/* A pointer that points to the start of the allocated memory */
    char memory[0];			/* Zero length array for where memory goes */
};

void *mm_malloc(size_t size);
void *mm_realloc(void *ptr, size_t size);
void mm_free(void *ptr);
