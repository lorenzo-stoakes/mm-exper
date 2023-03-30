#define _GNU_SOURCE
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
	int fd = open("test.txt", O_RDWR);
	if (fd == -1) {
		perror("open");
		return EXIT_FAILURE;
	}

	char *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
			 MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	while (true) {
		const char chr = ptr[0];
		ptr[0] = chr == 'z' ? 'a' : chr + 1;

		sleep(1);
	}

	return EXIT_SUCCESS;
}
