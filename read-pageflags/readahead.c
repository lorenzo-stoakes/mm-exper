#include "read-pageflags.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define NUM_PAGES (256)

static void read_pages(int fd, char *buf, int count)
{
	if (read(fd, buf, count * 4096) < 0) {
		perror("read");
		exit(EXIT_FAILURE);
	}
}

static void iterate_minor(int fd, char *buf, int count)
{
	for (int i = 0; i < NUM_PAGES; i += count) {
		read_pages(fd, buf, count);
	}
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s [interval] / [offset] [interval]\n", argv[0]);
		return EXIT_FAILURE;
	}

	const bool do_iterate = argc == 2;

	const int offset = do_iterate ? -1 : atoi(argv[1]);
	const int count = do_iterate ? atoi(argv[1]) : atoi(argv[2]);

	if (count < 1 || count > NUM_PAGES) {
		fprintf(stderr, "invalid count %d\n", count);
		return EXIT_FAILURE;
	}

	if (!do_iterate && (offset < 0 || offset >= NUM_PAGES)) {
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

	if (do_iterate) {
		iterate_minor(fd, buf, count);
		return EXIT_SUCCESS;
	}

	if (lseek(fd, offset * page_size, SEEK_SET) < 0) {
		perror("lseek");
		return EXIT_FAILURE;
	}

	read_pages(fd, buf, count);
	return EXIT_SUCCESS;
}
