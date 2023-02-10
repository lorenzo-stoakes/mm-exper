#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define TARGET_PTR ((void *)0x700000000000UL)

// We will allocate 50 pages and 49 pages, then join them.
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
	if (after == MAP_FAILED) {
		perror("ensure_space() [after] mmap()");
		exit(EXIT_FAILURE);
	}

	if (munmap(before, page_size) != 0) {
		perror("ensure_space() [before] munmap()");
		exit(EXIT_FAILURE);
	}

	if (munmap(after, page_size) != 0) {
		perror("ensure_space() [after] munmap()");
		exit(EXIT_FAILURE);
	}
}

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	// First, make sure we don't get auto-merged with surrounding VMAs.
	ensure_space();

	const unsigned long half = NUM_PAGES / 2;

	// First block is 50 pages.
	void *ptr1 = mmap(TARGET_PTR, half * page_size,
			  PROT_READ | PROT_WRITE,
			  MAP_ANON | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED, -1, 0);
	if (ptr1 == MAP_FAILED) {
		perror("mmap ptr1");
		return EXIT_FAILURE;
	}

	// First block is 49 pages.
	void *ptr2 = mmap(TARGET_PTR + (half + 1) * page_size, (half  - 1) * page_size,
			  PROT_READ | PROT_WRITE,
			  MAP_ANON | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED, -1, 0);
	if (ptr2 == MAP_FAILED) {
		perror("mmap ptr2");
		return EXIT_FAILURE;
	}

	printf("ptr = %p [press enter to continue...]\n", ptr1);
	getchar();

	// Now map a page in the middle, which should merge the VMA.
	// MAP_POPULATE means folios are already populated.
	void *ptr3 = mmap(TARGET_PTR + half * page_size, page_size,
			  PROT_READ | PROT_WRITE,
			  MAP_ANON | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED, -1, 0);
	if (ptr3 == MAP_FAILED) {
		perror("mmap ptr3");
		return EXIT_FAILURE;
	}
	printf("ptr1=%p, ptr2=%p, ptr3=%p\n", ptr1, ptr2, ptr3);

	puts("merge done [press enter to continue...]");
	getchar();

	// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
	// (at madvise_walk_vmas()).
	madvise(ptr1, NUM_PAGES * page_size, MADV_NORMAL);

	return EXIT_SUCCESS;
}
