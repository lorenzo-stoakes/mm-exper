#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main()
{
	const long page_size = sysconf(_SC_PAGESIZE);

	int fd = open("test.txt", O_RDWR);
	if (fd == -1) {
		perror("open");
		return EXIT_FAILURE;
	}

	printf("fd=%d\n", fd);

	// No worries about spurious VMA attachments as with anon memory.
	char *ptr = mmap(NULL, 3 * page_size, PROT_READ | PROT_WRITE,
			 MAP_SHARED | MAP_POPULATE, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	int fd2 = open("test.txt", O_RDWR);
	if (fd2 == -1) {
		perror("open");
		return EXIT_FAILURE;
	}

	printf("fd2=%d\n", fd2);

	char *ptr2 = mmap(NULL, 3 * page_size, PROT_READ | PROT_WRITE,
			  MAP_SHARED | MAP_POPULATE, fd2, 0);
	if (ptr2 == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
	// (at madvise_walk_vmas()).

	// Take 1
	madvise(ptr, 3 * page_size, MADV_NORMAL);
	// Take 2
	madvise(ptr2, 3 * page_size, MADV_NORMAL);

	return EXIT_SUCCESS;
}
