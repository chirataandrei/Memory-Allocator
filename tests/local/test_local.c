// SPDX-License-Identifier: BSD-3-Clause
// Local sanity tests for os_malloc/os_calloc/os_realloc/os_free.
// Not the official grader (ltrace snippets aren't in this repo) -
// just functional + alignment + zero-fill + edge-case checks.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "osmem.h"

static int failures;
static int checks;

#define CHECK(cond)                                                          \
	do {                                                                  \
		checks++;                                                     \
		if (!(cond)) {                                                 \
			failures++;                                            \
			printf("  FAIL (%s:%d): %s\n", __FILE__, __LINE__, #cond); \
		}                                                              \
	} while (0)

#define SECTION(name) printf("== %s ==\n", name)

static void test_malloc_zero(void)
{
	SECTION("malloc(0) returns NULL");
	CHECK(os_malloc(0) == NULL);
}

static void test_calloc_zero(void)
{
	SECTION("calloc(0, x) / calloc(x, 0) return NULL");
	CHECK(os_calloc(0, 16) == NULL);
	CHECK(os_calloc(16, 0) == NULL);
}

static void test_free_null(void)
{
	SECTION("free(NULL) does not crash");
	os_free(NULL);
	CHECK(1);
}

static void test_basic_rw(void)
{
	SECTION("basic malloc/write/read/free");
	char *p = os_malloc(100);

	CHECK(p != NULL);
	memset(p, 0xAB, 100);
	for (int i = 0; i < 100; i++)
		CHECK((unsigned char)p[i] == 0xAB);
	os_free(p);
}

static void test_alignment(void)
{
	SECTION("payload is 8-byte aligned");
	for (size_t sz = 1; sz <= 200; sz += 7) {
		void *p = os_malloc(sz);

		CHECK(p != NULL);
		CHECK(((size_t)p % 8) == 0);
		os_free(p);
	}
}

static void test_calloc_zero_fill(void)
{
	SECTION("calloc zero-fills memory (small + mmap-sized)");
	unsigned char *p = os_calloc(10, 20);

	CHECK(p != NULL);
	for (int i = 0; i < 200; i++)
		CHECK(p[i] == 0);
	os_free(p);

	size_t big_n = 300000;
	unsigned char *big = os_calloc(big_n, 1);

	CHECK(big != NULL);
	for (size_t i = 0; i < big_n; i += 4999)
		CHECK(big[i] == 0);
	os_free(big);
}

static void test_realloc_null_is_malloc(void)
{
	SECTION("realloc(NULL, size) behaves like malloc(size)");
	void *p = os_realloc(NULL, 64);

	CHECK(p != NULL);
	os_free(p);
}

static void test_realloc_zero_is_free(void)
{
	SECTION("realloc(ptr, 0) behaves like free(ptr) and returns NULL");
	void *p = os_malloc(64);

	CHECK(os_realloc(p, 0) == NULL);
}

static void test_realloc_on_free_block(void)
{
	SECTION("realloc on a freed block returns NULL");
	void *p = os_malloc(64);

	os_free(p);
	CHECK(os_realloc(p, 128) == NULL);
}

static void test_realloc_grow_shrink(void)
{
	SECTION("realloc grow preserves data, shrink truncates in place when possible");
	char *p = os_malloc(50);

	CHECK(p != NULL);
	memset(p, 'x', 50);

	char *grown = os_realloc(p, 200);

	CHECK(grown != NULL);
	for (int i = 0; i < 50; i++)
		CHECK(grown[i] == 'x');

	char *shrunk = os_realloc(grown, 10);

	CHECK(shrunk != NULL);
	for (int i = 0; i < 10; i++)
		CHECK(shrunk[i] == 'x');

	os_free(shrunk);
}

static void test_block_reuse(void)
{
	SECTION("freed block is reused by a later allocation of similar size");
	void *p1 = os_malloc(128);

	CHECK(p1 != NULL);
	os_free(p1);

	void *p2 = os_malloc(128);

	CHECK(p2 == p1);
	os_free(p2);
}

static void test_split(void)
{
	SECTION("large block gets split for smaller allocations");
	void *big = os_malloc(1000);

	CHECK(big != NULL);
	os_free(big);

	void *small = os_malloc(16);

	CHECK(small == big);

	void *next = os_malloc(16);

	CHECK(next != NULL);
	CHECK(next != small);

	os_free(small);
	os_free(next);
}

static void test_coalesce(void)
{
	SECTION("adjacent free blocks are coalesced to satisfy a bigger request");
	void *a = os_malloc(64);
	void *b = os_malloc(64);
	void *c = os_malloc(64);

	CHECK(a && b && c);
	os_free(a);
	os_free(b);

	/* a and b should coalesce into a block big enough for ~144 bytes */
	void *d = os_malloc(140);

	CHECK(d == a);
	os_free(c);
	os_free(d);
}

static void test_mmap_threshold(void)
{
	SECTION("allocations above MMAP_THRESHOLD are usable and freed cleanly");
	size_t big_size = MMAP_THRESHOLD + 4096;
	char *p = os_malloc(big_size);

	CHECK(p != NULL);
	memset(p, 0x5A, big_size);
	CHECK(p[0] == 0x5A);
	CHECK(p[big_size - 1] == 0x5A);
	os_free(p);
}

static void test_realloc_mmap_move(void)
{
	SECTION("realloc on a mapped block preserves data");
	size_t big_size = MMAP_THRESHOLD + 100;
	char *p = os_malloc(big_size);

	CHECK(p != NULL);
	memset(p, 0x7C, big_size);

	char *grown = os_realloc(p, big_size + 4096);

	CHECK(grown != NULL);
	CHECK(grown[0] == 0x7C);
	CHECK(grown[big_size - 1] == 0x7C);
	os_free(grown);
}

static void test_arrays_stress(void)
{
	SECTION("stress: many interleaved malloc/free of varying sizes");
	enum { N = 200 };
	void *ptrs[N];

	for (int i = 0; i < N; i++) {
		size_t sz = (size_t)((i * 37) % 4096) + 1;

		ptrs[i] = os_malloc(sz);
		CHECK(ptrs[i] != NULL);
		memset(ptrs[i], (int)(i & 0xFF), sz);
	}

	for (int i = 0; i < N; i += 2)
		os_free(ptrs[i]);

	for (int i = 0; i < N; i += 2) {
		size_t sz = (size_t)((i * 17) % 2048) + 1;

		ptrs[i] = os_malloc(sz);
		CHECK(ptrs[i] != NULL);
	}

	for (int i = 0; i < N; i++)
		os_free(ptrs[i]);

	CHECK(1);
}

int main(void)
{
	test_malloc_zero();
	test_calloc_zero();
	test_free_null();
	test_basic_rw();
	test_alignment();
	test_calloc_zero_fill();
	test_realloc_null_is_malloc();
	test_realloc_zero_is_free();
	test_realloc_on_free_block();
	test_realloc_grow_shrink();
	test_block_reuse();
	test_split();
	test_coalesce();
	test_mmap_threshold();
	test_realloc_mmap_move();
	test_arrays_stress();

	printf("\n%d/%d checks passed\n", checks - failures, checks);
	if (failures) {
		printf("%d FAILURES\n", failures);
		return 1;
	}
	printf("ALL PASSED\n");
	return 0;
}
