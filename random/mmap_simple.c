#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	char *ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE, -1, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	ptr[0] = 'x';

	if (munmap(ptr, page_size)) {
		perror("munmap");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
