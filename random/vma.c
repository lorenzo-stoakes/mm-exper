#include "shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define USE_VMAFLAGS

static void examine_vma(void *ptr)
{
#ifdef USE_VMAFLAGS
	lookup_vma_flags(ptr);
#else
	const long page_size = sysconf(_SC_PAGESIZE);

	// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
	// (at madvise_walk_vmas()).
	madvise(ptr, page_size, MADV_NORMAL);
#endif
}


int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	void *ptr;

	// We intentionally leak mappings all over the shop.

	printf("anon, prot_none: ");

	ptr = mmap(NULL, page_size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 1");
		return EXIT_FAILURE;
	}

	examine_vma(ptr);

	printf("anon, prot_read: ");

	ptr = mmap(NULL, page_size, PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 1");
		return EXIT_FAILURE;
	}

	examine_vma(ptr);

	return EXIT_SUCCESS;
}
