#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <numaif.h>

int main()
{
	const long page_size = sysconf(_SC_PAGESIZE);

	char *ptr = mmap(NULL, 5 * page_size,PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE | MAP_POPULATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	// Split into 5.
	if (mlock(ptr + page_size, page_size)) {
		perror("mlock 1");
		return EXIT_FAILURE;
	}
	if (mlock(ptr + 3 * page_size, page_size)) {
		perror("mlock 2");
		return EXIT_FAILURE;
	}

	const unsigned long nodemask = 0;

	// Initialise ranges to be the same for first two pages.
	if (mbind(ptr, page_size * 2, MPOL_PREFERRED, &nodemask, 1, 0)) {
		perror("mbind 1");
		return EXIT_FAILURE;
	}

	// Now, try changing for full range.
	if (mbind(ptr/* + page_size*/, page_size * 4, MPOL_PREFERRED, &nodemask, 1, 0)) {
		perror("mbind 2");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
