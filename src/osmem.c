// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include "block_meta.h"

struct block_meta *head = NULL;

void *os_malloc(size_t size)
{	
	if (size == 0) {
		return NULL;
	}

	// memory alignment
	while (size % 8 != 0) {
		size++;
	}
	
	if (size < MMAP_THRESHOLD) {

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
		for (; curr->next; curr = curr->next) {
			if (curr->status == STATUS_FREE && curr->next->status == STATUS_FREE) {
				curr->size = curr->size + curr->next->size + sizeof(struct block_meta);
				curr->next = curr->next->next;
				curr->next->prev = curr;
			}
		}

		// find best fit
		struct block_meta *best_fit = NULL;
		curr = head;
		long long min_val = 1e18;
		for (; curr; curr = curr->next) {
			if (curr->status == STATUS_FREE && curr->size >= size) {
				if (size - curr->size < min_val) {
					min_val = size - curr->size;
					best_fit = curr;
				}
			}
		}
		if (best_fit == NULL) {
			
		} else {
			if ()
		}

	} else {

	}

	return NULL;
}

void os_free(void *ptr)
{
	/* TODO: Implement os_free */
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
