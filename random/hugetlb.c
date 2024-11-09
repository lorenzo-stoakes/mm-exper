#define _GNU_SOURCE
#include <fcntl.h>
#include <stdbool.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int main()
{
	unsigned long size = 20971520000UL;

	int fd = memfd_create("hugepage-mmap", MFD_HUGETLB);
	if (fd < 0) {
		perror("memfd");
		return EXIT_FAILURE;
	}

//	char *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
//			  MAP_SHARED | MAP_POPULATE | MAP_HUGETLB, fd, 0);
	char *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
			 MAP_SHARED | MAP_ANON, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	for (unsigned long i = 0; i < size; i += 4096) {
		ptr[i] = 'x';
	}
	printf("\n");

	while (true)
		;

	// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
	// (at madvise_walk_vmas()).
	madvise(ptr, 1024 * 1024, MADV_NORMAL);

	return EXIT_SUCCESS;
}
