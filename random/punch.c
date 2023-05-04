#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main()
{
	const long page_size = sysconf(_SC_PAGESIZE);

	int fd = open("test2.txt", O_RDWR);
	if (fd == -1) {
		perror("open");
		return EXIT_FAILURE;
	}

	char *ptr = mmap(NULL, 10 * page_size, PROT_READ | PROT_WRITE,
			 MAP_SHARED | MAP_POPULATE, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	printf("--- before ---\n");

	printf("--- begin ---\n");
	for (int i = 0; i < page_size * 2; i++) {
		printf("%c", ptr[3 * page_size + i]);
	}
	printf("--- done ---\n");

	if (madvise(ptr, page_size * 10, MADV_REMOVE)) {
		perror("madvise");
		return EXIT_FAILURE;
	}

	munmap(ptr, 10 * page_size);
	close(fd);

	fd = open("test2.txt", O_RDWR);
	if (fd == -1) {
		perror("open");
		return EXIT_FAILURE;
	}

	char *ptr2 = mmap(NULL, 10 * page_size, PROT_READ | PROT_WRITE,
			  MAP_SHARED | MAP_POPULATE, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 2");
		return EXIT_FAILURE;
	}

	printf("--- after ---\n");

	printf("--- begin ---\n");
	for (int i = 0; i < page_size * 2; i++) {
		printf("%c", ptr[3 * page_size + i]);
	}
	printf("--- done ---\n");


	return EXIT_SUCCESS;
}
