#define _GNU_SOURCE
#include "shared.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static void examine_vma(void *ptr)
{
	if (ptr == MAP_FAILED) {
		puts("(invalid mapping!)");
		return;
	}

	lookup_vma_flags(ptr);
	const long page_size = sysconf(_SC_PAGESIZE);

	// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
	// (at madvise_walk_vmas()).
	madvise(ptr, page_size, MADV_NORMAL);
}

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	void *ptr;

	// We intentionally leak mappings all over the shop.

	printf("1: ");

	ptr = mmap(NULL, page_size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 1");
		return EXIT_FAILURE;
	}

	examine_vma(ptr);

	printf("2: ");

	ptr = mmap(NULL, page_size, PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 2");
		return EXIT_FAILURE;
	}

	examine_vma(ptr);

	printf("3: ");

	ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 3");
		return EXIT_FAILURE;
	}

	examine_vma(ptr);

	printf("4: ");

	ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 4");
		return EXIT_FAILURE;
	}

	examine_vma(ptr);

	printf("5: ");

	ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE | PROT_EXEC,
		   MAP_ANON | MAP_PRIVATE | MAP_POPULATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 5");
		return EXIT_FAILURE;
	}

	examine_vma(ptr);


	printf("6: ");

	ptr = mmap(NULL, page_size,
		   PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_SHARED, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 6");
		return EXIT_FAILURE;
	}

	examine_vma(ptr);

	printf("7: ");

	int fd = open("vma.c", O_RDWR);
	if (fd == -1) {
		perror("open 7");
		return EXIT_FAILURE;
	}

	ptr = mmap(NULL, page_size,
		   PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 7");
		return EXIT_FAILURE;
	}

	examine_vma(ptr);

	printf("8: ");

	ptr = mmap(NULL, 3 * page_size,
		   PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_POPULATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 8");
		return EXIT_FAILURE;
	}

	if (mlock2(ptr, page_size, MLOCK_ONFAULT) < 0) {
		perror("mlock 8");
		return EXIT_FAILURE;
	}

	examine_vma(ptr);

	printf("9: ");

	ptr = mmap(NULL, 3 * page_size,
		   PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 9");
		return EXIT_FAILURE;
	}

	if (mlock(ptr, page_size) < 0) {
		perror("mlock 9a");
		return EXIT_FAILURE;
	}

	if (mlock(ptr + 2*page_size, page_size) < 0) {
		perror("mlock 9b");
		return EXIT_FAILURE;
	}

	if (munlock(ptr, page_size) < 0) {
		perror("munlock 9");
		return EXIT_FAILURE;
	}

	examine_vma(ptr);

	return EXIT_SUCCESS;
}
