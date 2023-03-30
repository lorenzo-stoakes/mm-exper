#define _GNU_SOURCE
#include <sys/mman.h>
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

int main(void)
{
	const char *file = "mremap_repro.c";
	char *addr, *addr2;
	int fd;
	char c;

	fd = open (file, O_RDONLY);
	if (fd == -1) {
		perror("open");
		return EXIT_FAILURE;
	}
	addr = mmap (NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	close(fd);
	if (addr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}
	c = addr[0];

	if (!addr[0])
		fprintf(stderr, "error 1\n");

	addr[0] = 0;

	if (addr[0])
		fprintf(stderr, "error 2\n");

	addr2 = mremap (addr, PAGE_SIZE, PAGE_SIZE,
			MREMAP_MAYMOVE | MREMAP_DONTUNMAP);
	if (addr2 == MAP_FAILED) {
		perror("mremap");
		return EXIT_FAILURE;
	}

	printf("%p\n%p\n", addr, addr2);

	if (addr2[0])
		fprintf(stderr, "error 3\n");

	if (addr[0] == c)
		printf("Test FAILED\n");
	else
		printf("Test PASSED\n");
	return 0;
}
