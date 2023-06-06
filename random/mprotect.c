#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main()
{
	const long page_size = sysconf(_SC_PAGESIZE);

	char *ptr = mmap((void *)0x5000000, page_size, PROT_READ,
			 MAP_ANON | MAP_PRIVATE, -1, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");

		return EXIT_FAILURE;
	}

	if (mprotect(ptr, 4 * page_size,
		     PROT_READ | PROT_WRITE)) {
		perror("mprotect");

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
