#include <stdio.h>
#include <stdlib.h>

#include "musl/oldmalloc.h"

#define MIN_ORDER (1)
#define MAX_ORDER (24)
#define NUM_ALLOCS (MAX_ORDER - MIN_ORDER + 1)

int main(void)
{
	void *ptrs[NUM_ALLOCS];
	size_t sizes[NUM_ALLOCS];

	for (size_t i = 0; i < NUM_ALLOCS; i++) {
		const size_t order = MIN_ORDER + i;
		size_t size = 1UL << order;
		size += rand() % size;

		sizes[i] = size;

		printf("malloc %lu bytes...\n", size);
		ptrs[i] = musl_malloc(size);
	}

	for (size_t i = 0; i < NUM_ALLOCS; i++) {
		const size_t size = sizes[i];

		printf("free %lu bytes...\n", size);
		musl_free(ptrs[i]);
	}

	return EXIT_SUCCESS;
}
