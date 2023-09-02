#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_ALLOCS (100)
#define ALLOC_SIZE (1 << 20) /* 1 MiB */

int main(void)
{
	int i;
	void *span;
	const unsigned long page_size = (unsigned long)sysconf(_SC_PAGESIZE);
	/* 4 TiB should be sufficient. */
	const unsigned long size = page_size << 30;

	span = mmap(NULL, size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (span == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	for (i = 0; i < MAX_ALLOCS; i++) {
		void *ptr = mmap(span + i * ALLOC_SIZE, ALLOC_SIZE,
				 PROT_READ | PROT_WRITE,
				 MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0);

		if (ptr == MAP_FAILED) {
			perror("internal mmap");
			return EXIT_FAILURE;
		}
	}

	/* Do something with mappings... */

	if (munmap(span, size)) {
		perror("munmap");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
