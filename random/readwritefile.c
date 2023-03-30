#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	int fd = open("test.txt", O_RDWR);
	if (fd == -1) {
		perror("open");
		return EXIT_FAILURE;
	}

	// Map shared the file.
	char *ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	ptr[0] = 'x';

	// _Read_ fault this in.
	char buf[3];
	buf[0] = ptr[0];

	// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
	// (at madvise_walk_vmas()).
	madvise(ptr, page_size, MADV_NORMAL);

	// _Write_ fault this in.
	ptr[0] = 'x';

	madvise(ptr, page_size, MADV_NORMAL);

	return EXIT_SUCCESS;
}
