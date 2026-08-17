// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>

#include "osmem.h"
#include "block_meta.h"

struct block_meta *head = NULL;

void split_block(struct block_meta *block, size_t size, size_t dif)
{
	struct block_meta *new = (struct block_meta *) ((char*) (block + 1) + size); 
	new->next = block->next;
	new->prev = block;
	new->size = dif - sizeof(struct block_meta);
	new->status = STATUS_FREE;
	block->next = new;
	block->size = size;
	if (new->next != NULL) {
		new->next->prev = new;
	}
}

static void *alloc_heap(size_t size)
{
	struct block_meta *return_ptr = NULL;

	// heap preallocation
	if (head == NULL) {
		void *ptr = sbrk(128 * 1024);
		if (ptr == (void *) -1)	{
			return NULL;
		} else {
				
			head = (struct block_meta *) ptr;
			head->next = NULL;
			head->prev = NULL;
			head->status = STATUS_FREE;
			head->size = 128 * 1024 - sizeof(struct block_meta);
		}
	}

	// coalesce blocks
	struct block_meta *curr = head;
	while (curr != NULL && curr->next != NULL) {
		
		if (curr->status == STATUS_FREE) {
			struct block_meta *valid = curr;
			size_t total_size = curr->size;
			curr = curr->next;
				
			while (curr != NULL && curr->status == STATUS_FREE) {
				total_size += sizeof(struct block_meta) + curr->size;
				curr = curr->next;
			}
				
			valid->size = total_size;
			valid->next = curr;
			if (curr != NULL) {
				curr->prev = valid;
			}
		}
		if (curr != NULL) {
			curr = curr->next;
		}
	}

	// find best fit
	struct block_meta *best_fit = NULL, *last = NULL;
	curr = head;
	size_t min_val = (size_t) -1;
	while (curr != NULL) {
			
		if (curr->status == STATUS_FREE && curr->size >= size && min_val >= curr->size - size) {
			min_val = curr->size - size;
			best_fit = curr;
		}
		last = curr;
		curr = curr->next;
	}	

	// create new block
	if (best_fit == NULL) {
		if (last->status == STATUS_FREE) {
			
			size_t dif = size - last->size;
			void *ptr = sbrk(dif);
			if (ptr == (void *) -1) {
				return NULL;
			}
			last->status = STATUS_ALLOC;
			last->size = size;
			return_ptr = last;
		} else {
				
			size_t total_size = sizeof(struct block_meta) + size;
			void *ptr = sbrk(total_size);
			if (ptr == (void *) -1) {
				return NULL;
			} 
				
			last->next = (struct block_meta*) ptr;
			struct block_meta* new = last->next;
			new->next = NULL;
			new->prev = last;
			new->size = total_size - sizeof(struct block_meta);
			new->status = STATUS_ALLOC;
			return_ptr = new;
		}
	} else {

		// alloc the memory
        best_fit->status = STATUS_ALLOC;
        return_ptr = best_fit;

        // split block
        size_t left_size = best_fit->size - size;
        if (left_size >= sizeof(struct block_meta) + 8) {
                
            split_block(best_fit, size, left_size);
        }
	}

	return (void *) (return_ptr + 1);
}

static void *alloc_map(size_t size)
{
	struct block_meta *return_ptr = NULL;

	void *ptr = mmap(NULL, size + sizeof(struct block_meta), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (ptr == MAP_FAILED) {
		return NULL;
	}
	
	struct block_meta *mmap_ptr = (struct block_meta *) ptr;
	mmap_ptr->status = STATUS_MAPPED;
	mmap_ptr->prev = NULL;
	mmap_ptr->next = NULL;
	mmap_ptr->size = size;
	return_ptr = mmap_ptr;

	return (void *) (return_ptr + 1);
}

void *os_malloc(size_t size)
{	
	if (size == 0) {
		return NULL;
	}

	// memory alignment
	size = ALIGN(size);
	
	if (size + sizeof(struct block_meta) < MMAP_THRESHOLD) {

		return alloc_heap(size);

	} else {
		
		return alloc_map(size);
	}

}

void os_free(void *ptr)
{
	if (ptr == NULL) {
		return;
	}

	struct block_meta *curr = (struct block_meta *) ptr - 1;
	if (curr->status == STATUS_ALLOC) {
		curr->status = STATUS_FREE;
	} else if (curr->status == STATUS_MAPPED) {
		size_t total_size = sizeof(struct block_meta) + curr->size;
		int ret = munmap(curr, total_size);
		DIE(ret == -1, "munmap failed");
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (size == 0 || nmemb == 0) {
		return NULL;
	}

	void *ptr = NULL;

	// memory alignment
	size_t total_size = ALIGN(size * nmemb);

	if (total_size + sizeof(struct block_meta) < (size_t)getpagesize()) {

		ptr = alloc_heap(total_size);

	} else {

		ptr = alloc_map(total_size);
	}

	memset(ptr, 0, ((struct block_meta *)ptr - 1)->size);

	return ptr;
}

void *os_realloc(void *ptr, size_t size)
{
	if (ptr == NULL) {
		return os_malloc(size);
	}

	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	struct block_meta *curr = (struct block_meta *) ptr - 1;
	if (curr->status == STATUS_FREE) {
		return NULL;
	}

	size = ALIGN(size);

	if (curr->status == STATUS_MAPPED) {
		void *new_ptr = os_malloc(size);
		size_t min_size = (curr->size < size) ? curr->size : size;
		memcpy(new_ptr, ptr, min_size);
		os_free(ptr);
		return new_ptr;
	} else {
		
		if (size <= curr->size) {
			
			// split block
			size_t dif = curr->size - size;
			if (dif >= sizeof(struct block_meta) + 8) {
				split_block(curr, size, dif);
			}
			return ptr;
		} else {
			
			// coalesce blocks
			size_t curr_size = curr->size;
			struct block_meta *block = curr;
			curr = curr->next;
			while (curr != NULL && curr->status == STATUS_FREE) {
				curr_size += curr->size + sizeof(struct block_meta);
				curr = curr->next;
			}

			block->next = curr;
			block->size = curr_size;
			if (curr != NULL) {
				curr->prev = block;
			}

			// verify if the size is enough
			if (block->size >= size) {
				
				// split block
				size_t dif = block->size - size;
				if (dif >= sizeof(struct block_meta) + 8) {
					split_block(block, size, dif);
				}

				return ptr;
			} else if (curr == NULL) {
				
				// end of the heap
				size_t dif = size - block->size;
				void *new_ptr = sbrk(dif);
				if (new_ptr == (void*) -1) {
					return NULL;
				}
				block->size = size;
				return ptr;
			} else {
				
				// alloc more space
				void *new_ptr = os_malloc(size);
				DIE(new_ptr == NULL, "malloc failed");
				memcpy(new_ptr, ptr, block->size);
				os_free(ptr);
				return new_ptr;
			}
		}
	}
}
