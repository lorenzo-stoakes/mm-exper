#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
	unsigned long size = 2 * 1024 * 1024 * 1000;
	char *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	if (madvise(ptr, size, MADV_POPULATE_WRITE)) {
		perror("madvise 1");
		return EXIT_FAILURE;
	}

	if (madvise(ptr, size, MADV_COLLAPSE)) {
		perror("madvise 2");
		return EXIT_FAILURE;
	}

	while (1) {
		unsigned long i;

		for (i = 0; i < size; i++) {
			ptr[i] = 'a' + (i % 26);
		}
	}

	return EXIT_SUCCESS;
}
