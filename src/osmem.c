// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include "block_meta.h"

struct block_meta *head = NULL;

void *os_malloc(size_t size)
{	
	if (size == 0) {
		return NULL;
	}

	struct block_meta *return_ptr = NULL;

	// memory alignment
	size = ALIGN(size);
	
	// heap preallocation
	if (size + sizeof(struct block_meta) < MMAP_THRESHOLD) {

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
		long long min_val = 1e18;
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
				
				struct block_meta *new = (struct block_meta *)((char *)(best_fit + 1) + size);
				best_fit->size = size;
				new->size = left_size - sizeof(struct block_meta);
				new->next = best_fit->next;
				best_fit->next = new;
				new->prev = best_fit;
				new->status = STATUS_FREE;
			}
		}

	} else {
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
	}

	return (void *) (return_ptr + 1);
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
	/* TODO: Implement os_calloc */
	return NULL;
}

void *os_realloc(void *ptr, size_t size)
{
	/* TODO: Implement os_realloc */
	return NULL;
}
