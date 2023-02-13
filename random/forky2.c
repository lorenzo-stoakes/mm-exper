#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TARGET_PTR ((void *)0x700000000000UL)
#define NUM_PAGES (1)

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

	void *ptr = mmap(TARGET_PTR, NUM_PAGES * page_size, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	pid_t p = fork();
	if (p == -1) {
		perror("fork");
		return EXIT_FAILURE;
	}

	if (p != 0) {
		// Parent.

		// Wait for CoW.
		sleep(1);

		// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
		// (at madvise_walk_vmas()).
		madvise(ptr, NUM_PAGES * page_size, MADV_NORMAL);

		return EXIT_SUCCESS;
	}

	// Child.

	// Now CoW the page.
	((char *)ptr)[0] = 'x';
	while (1)
		;

	return EXIT_SUCCESS;
}
