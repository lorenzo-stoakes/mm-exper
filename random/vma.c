#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static void trigger_breakpoint(void *ptr)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
	// (at madvise_walk_vmas()).
	madvise(ptr, page_size, MADV_NORMAL);
}

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	void *ptr;

	// We intentionally leak mappings all over the shop.

	puts("MAP_ANON | MAP_PRIVATE, PROT_NONE");

	ptr = mmap(NULL, page_size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 1");
		return EXIT_FAILURE;
	}

	trigger_breakpoint(ptr);

	puts("MAP_ANON | MAP_PRIVATE, PROT_READ");

	ptr = mmap(NULL, page_size, PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 1");
		return EXIT_FAILURE;
	}

	trigger_breakpoint(ptr);

	return EXIT_SUCCESS;
}
