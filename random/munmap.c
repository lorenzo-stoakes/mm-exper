#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main()
{
	const long page_size = sysconf(_SC_PAGESIZE);
/*
	char *ptr = mmap((void *)0x5000000, page_size, PROT_READ,
			 MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");

		return EXIT_FAILURE;
	}

	char *ptr2 = mmap((void *)0x5003000, page_size, PROT_READ,
			  MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0);

	if (ptr2 == MAP_FAILED) {
		perror("mmap");

		return EXIT_FAILURE;
	}
*/

	if (munmap((void *)0x5000000, 4 * page_size)) {
		perror("munmap");

		return EXIT_FAILURE;
	}

	if (munmap((void *)0x5000000, 4 * page_size)) {
		perror("munmap");

		return EXIT_FAILURE;
	}


	return EXIT_SUCCESS;
}
