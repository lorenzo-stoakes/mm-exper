#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main()
{
	const long page_size = sysconf(_SC_PAGESIZE);

	char *ptr = mmap(NULL, 10 * page_size, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANON | MAP_POPULATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	if (madvise(ptr, page_size * 10, MADV_REMOVE)) {
		perror("madvise");
		return EXIT_FAILURE;
	}

	printf("chr=[%c]\n", ptr[0]);

	ptr[0] = 'x';
	ptr[4096] = 'y';

	printf("chr=[%c]\n", ptr[0]);
	printf("chr=[%c]\n", ptr[4096]);

	return EXIT_SUCCESS;
}
