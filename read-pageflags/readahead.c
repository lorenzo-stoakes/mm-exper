#include "read-pageflags.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define NUM_PAGES (256)

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: %s [offset] [interval]\n", argv[0]);
		return EXIT_FAILURE;
	}

	const int offset = atoi(argv[1]);
	const int count = atoi(argv[2]);

	if (count < 1 || count > NUM_PAGES) {
		fprintf(stderr, "invalid count %d\n", count);
		return EXIT_FAILURE;
	}

	if (offset < 0 || offset >= NUM_PAGES) {
		fprintf(stderr, "invalid offset %d\n", offset);
		return EXIT_FAILURE;
	}

	const size_t page_size = sysconf(_SC_PAGESIZE);
	const size_t size = count * page_size;

	char *buf = malloc(size);

	int fd = open("readahead.dat", O_RDWR);
	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	if (lseek(fd, offset * page_size, SEEK_SET) < 0) {
		perror("lseek");
		return EXIT_FAILURE;
	}

	if (read(fd, buf, count * 4096) < 0) {
		perror("read");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
