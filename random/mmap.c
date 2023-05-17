#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	char *ptr = mmap(NULL, 4 * page_size, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE | MAP_POPULATE,
			 -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");

		return EXIT_FAILURE;
	}

	memset(ptr, 'x', 4 * page_size);

	printf("chr=[%c]\n", ptr[0]);

	/* We can just map over an existing mapping... :) */
	ptr = mmap(ptr, page_size, PROT_READ | PROT_WRITE,
		   MAP_ANON | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 2");

		return EXIT_FAILURE;
	}

	printf("chr=[%c]\n", ptr[0]);
	printf("chr=[%c]\n", ptr[4096]);

	return EXIT_SUCCESS;
}
