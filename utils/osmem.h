/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include "printf.h"

#define MMAP_THRESHOLD	(128 * 1024)
#define ALIGN_SZ 8
#define ALIGN(size) (((size) + (ALIGN_SZ - 1)) & ~(ALIGN_SZ - 1))

void *os_malloc(size_t size);
void os_free(void *ptr);
void *os_calloc(size_t nmemb, size_t size);
void *os_realloc(void *ptr, size_t size);
