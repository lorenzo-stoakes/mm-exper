#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define TARGET_PTR ((void *)0x700000000000UL)
#define NUM_PAGES (100)

// Allocate memory around the target region then free it to ensure it is
// isolated.
static void ensure_space(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	void *before = mmap(TARGET_PTR - page_size, page_size, PROT_READ | PROT_WRITE,
			    MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
	if (before == MAP_FAILED) {
		perror("ensure_space() [before] mmap()");
		exit(EXIT_FAILURE);
	}

	void *after = mmap(TARGET_PTR + NUM_PAGES * page_size, page_size, PROT_READ | PROT_WRITE,
			    MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);

	if (munmap(before, page_size) != 0) {
		perror("ensure_space() munmap()");
		exit(EXIT_FAILURE);
	}

	if (munmap(after, page_size) != 0) {
		perror("ensure_space() munmap()");
		exit(EXIT_FAILURE);
	}
}

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	// First, make sure we don't get auto-merged with surrounding VMAs.
	ensure_space();

	void *ptr = mmap(TARGET_PTR, NUM_PAGES * page_size, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	printf("ptr = %p\n", ptr);
	getchar();

	// Now free a page in the middle, which should split the
	// VMA. MAP_POPULATE means folios are already populated.

	if (munmap(ptr + (NUM_PAGES / 2) * page_size, page_size) != 0) {
		perror("munmap");
		return EXIT_FAILURE;
	}

	puts("split done");
	getchar();

	void *join = mmap(ptr + (NUM_PAGES / 2) * page_size, page_size, PROT_READ | PROT_WRITE,
			  MAP_ANON | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED, -1, 0);
	if (join == MAP_FAILED) {
		perror("join mmap");
		return EXIT_FAILURE;
	}

	puts("merge done");

	// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
	// (at madvise_walk_vmas()).
	madvise(ptr, NUM_PAGES * page_size, MADV_NORMAL);

	return EXIT_SUCCESS;
}
