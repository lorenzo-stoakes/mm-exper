#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TARGET_PTR ((void *)0x700000000000UL)

int main(int argc, char **argv)
{
	long pid;
	int fd;
	char path[256];
	const char *hello = "hello!";

	if (argc < 2) {
		fprintf(stderr, "usage: %s [pid]\n", argv[0]);
		return EXIT_FAILURE;
	}

	sprintf(path, "/proc/%s/mem", argv[1]);

	fd = open(path, O_RDWR);
	if (fd == -1) {
		perror("open");
		return EXIT_FAILURE;
	}

	if (lseek(fd, (off_t)TARGET_PTR, SEEK_SET) != (off_t)TARGET_PTR) {
		perror("lseek");
		return EXIT_FAILURE;
	}

	if (write(fd, hello, 7) != 7) {
		perror("write");
		return EXIT_FAILURE;
	}

	close(fd);

	return EXIT_SUCCESS;
}
