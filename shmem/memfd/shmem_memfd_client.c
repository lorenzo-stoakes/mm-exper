#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define SIZE (4096)
#define NUM_LOOPS (10)

int main(int argc, char **argv)
{
	int i, fd;
	char *ptr;

	if (argc < 2) {
		fprintf(stderr, "usage: %s [path to fd]\n", argv[0]);
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_RDWR);
	if (fd == -1) {
		perror("open");
		return EXIT_FAILURE;
	}

	ptr = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	close(fd);

	for (i = 0; i < NUM_LOOPS; i++) {
		ptr[1] = 'a' + i;
		printf("client: %s\n", ptr);
		sleep(1);
	}

	return EXIT_SUCCESS;
}
