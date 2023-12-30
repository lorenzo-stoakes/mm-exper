#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define SIZE (16106127360UL)

int main(void) {
	int fd = open("test.dat", O_RDWR | O_CREAT);

	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	if (ftruncate(fd, SIZE)) {
		perror("ftruncate");
		return EXIT_FAILURE;
	}

	char *buf = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	for (unsigned long i = 0; i < SIZE; i += 4096) {
		buf[i] = 'x';
	}

	return EXIT_SUCCESS;
}
