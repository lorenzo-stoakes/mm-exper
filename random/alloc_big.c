#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define SIZE (1UL << 32)

int main(void)
{
	char *ptr = mmap(NULL, SIZE, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE | MAP_POPULATE, -1, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	if (madvise(ptr, SIZE, MADV_POPULATE_WRITE)) {
		perror("madvise");
		return EXIT_FAILURE;
	}

	while (1)
		;

	return EXIT_SUCCESS;
}
