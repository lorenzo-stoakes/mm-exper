#define _GNU_SOURCE
#include <fcntl.h>
#include <stdbool.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static void trigger_gdb(char *ptr, size_t size)
{
	// Can hook madvise() to easily break into kernel code in qemu/gdb.
	madvise(ptr, size, MADV_NORMAL);
}

int main()
{
	// memfd method.

	int fd = memfd_create("hugepage-mmap", MFD_HUGETLB);
	if (fd < 0) {
		perror("memfd");
		return EXIT_FAILURE;
	}
	size_t size = 2 * 1024 * 1024;

	char *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
			  MAP_SHARED | MAP_POPULATE | MAP_HUGETLB, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	trigger_gdb(ptr, size);

	munmap(mmap, size);

	// anon MAP_HUGETLB method.

	ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
		   MAP_ANON | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	trigger_gdb(ptr, size);

	// file system method

	int fd2 = open("/mnt/hugepage", O_CREAT | O_RDWR, 0777);
	if (fd2 < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	if (ftruncate(fd2, size)) {
		perror("ftruncate");
		return EXIT_FAILURE;
	}

	ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd2, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
