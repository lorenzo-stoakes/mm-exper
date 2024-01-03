#include "read-pageflags.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define NUM_PAGES (256)

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s [interval size in pages]\n", argv[0]);
		return EXIT_FAILURE;
	}

	const int count = atoi(argv[1]);

	if (count < 1 || count > NUM_PAGES) {
		fprintf(stderr, "invalid count %d\n", count);
		return EXIT_FAILURE;
	}

	const size_t size = count * sysconf(_SC_PAGESIZE);

	char *buf = malloc(size);

	int fd = open("readahead.dat", O_RDWR);
	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	if (read(fd, buf, count * 4096) < 0) {
		perror("read");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
