#include "read-pageflags.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define NUM_PAGES (256)

int main(void)
{
	char buf[4096];

	int fd = open("readahead.dat", O_RDWR);
	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	if (read(fd, buf, 4096) < 0) {
		perror("read");
		return EXIT_FAILURE;
	}

	// mmap to get a VA.
	const void *ptr = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	uint64_t pfn = read_pfn(ptr);
	if (pfn == INVALID_VALUE) {
		fprintf(stderr, "Cannot extract PFN for VA %p\n", ptr);
		return EXIT_FAILURE;
	}

	printf("pfn=%lu\n", pfn);

	close(fd);

	return EXIT_SUCCESS;
}
