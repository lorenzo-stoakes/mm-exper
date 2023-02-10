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

	// No worries about spurious VMA attachments as with anon memory.
	char *ptr = mmap(NULL, 3 * page_size, PROT_READ | PROT_WRITE,
			 MAP_SHARED | MAP_POPULATE, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	// Unmap middle page.
	if (munmap(ptr + page_size, page_size) == -1) {
		perror("munmap");
		return EXIT_FAILURE;
	}

	// We should now have 2 split VMAs.

	// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
	// (at madvise_walk_vmas()).
	madvise(ptr, 3 * page_size, MADV_NORMAL);

	return EXIT_SUCCESS;
}
