#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
	int fd = open("test8192.txt", O_RDONLY);
	char *ptr;
	int bytes;

	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
		   MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	/* Now try to read past the end of the buffer... */
	bytes = read(fd, ptr, 8192);
	if (bytes < 0) {
		perror("read");
		return EXIT_FAILURE;
	}

	printf("bytes=%d\n", bytes);

	return EXIT_SUCCESS;
}
