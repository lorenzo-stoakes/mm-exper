#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/kernel-page-flags.h>

#define PAGEMAP_SOFTDIRTY (1UL << 55)
#define PAGEMAP_EXCLUSIVE (1UL << 56)
#define PAGEMAP_FILE      (1UL << 61)
#define PAGEMAP_SWAPPED   (1UL << 62)
#define PAGEMAP_PRESENT   (1UL << 63)

#define PAGEMAP_SWAP_OFFSET_SHIFT (5)
#define PAGEMAP_SWAP_TYPE_MASK ((1UL << 5) - 1)
/* 'Bits 0-54  page frame number (PFN) if present */
#define PAGEMAP_PFN_MASK ((1UL << 55) - 1)

static uint64_t read_u64(const char *path, uint64_t offset)
{
	uint64_t ret;
	int fd = open(path, O_RDWR);

	if (fd < 0) {
		perror("open");
		exit(1);
	}

	if (lseek(fd, offset, SEEK_SET) != offset) {
		perror("lseek");
		exit(1);
	}

	if (read(fd, &ret, sizeof(ret)) != sizeof(ret)) {
		perror("read");
		exit(1);
	}

	return ret;
}

static uint64_t read_pagemap(const void *ptr)
{
	const uint64_t virt_page_num = (uint64_t)ptr / getpagesize();
	/* There is 'one 64-bit value for each virtual page'. */
	const uint64_t offset = virt_page_num * sizeof(uint64_t);

	return read_u64("/proc/self/pagemap", offset);
}

static uint64_t read_kpageflags(uint64_t pfn)
{
	return read_u64("/proc/kpageflags", pfn * sizeof(uint64_t));
}

uint64_t read_mapcount(uint64_t pfn)
{
	return read_u64("/proc/kpagecount", pfn * sizeof(uint64_t));
}

static void describe_swapped(uint64_t pagemap)
{
	/* Swap type and offset encoded in same location as PFN would be. */
	const uint64_t type = pagemap & PAGEMAP_SWAP_TYPE_MASK;
	const uint64_t offset =
		(pagemap & PAGEMAP_PFN_MASK) >> PAGEMAP_SWAP_OFFSET_SHIFT;

	printf("swapped: type=%lu, offset=%lu\n", type, offset);
}

static void describe_kpageflags(uint64_t pfn)
{
	uint64_t kpageflags = read_kpageflags(pfn);

	/*
	 * There are a huge number of flags declared in
	 * uapi/include/linux/kernel-page-flags.h, we examine a subset.
	 */

#define PRINT_FLAGS(flag) \
	if (kpageflags & (1UL << KPF_##flag))	\
		printf(" [" #flag "]");

	PRINT_FLAGS(ACTIVE);
	PRINT_FLAGS(DIRTY);
	PRINT_FLAGS(LOCKED);
	PRINT_FLAGS(LRU);
	PRINT_FLAGS(MMAP);
	PRINT_FLAGS(NOPAGE);
	PRINT_FLAGS(REFERENCED);
	PRINT_FLAGS(SWAPBACKED);
	PRINT_FLAGS(UPTODATE);
	PRINT_FLAGS(WRITEBACK);

#undef PRINT_FLAGS
}

static void describe_addr(const void *ptr)
{
	uint64_t pfn, mapcount;
	const uint64_t pagemap = read_pagemap(ptr);

	printf("%p: ", ptr);

	if (!(pagemap & PAGEMAP_PRESENT)) {
		if (pagemap & PAGEMAP_SWAPPED) {
			describe_swapped(pagemap);
			return;
		}

		printf("<not present>\n");
		return;
	}

	printf("%s: ", (pagemap & PAGEMAP_FILE) ? "file" : "anon");

	pfn = pagemap & PAGEMAP_PFN_MASK;
	printf("pfn=%lu", pfn);

	if (pagemap & PAGEMAP_SOFTDIRTY)
		printf(" [SD]");

	if (pagemap & PAGEMAP_EXCLUSIVE)
		printf(" [EX]");

	mapcount = read_mapcount(pfn);
	printf(" mapped=%lu", mapcount);

	describe_kpageflags(pfn);

	printf("\n");
}

int main(void)
{
	int fd;
	char *ptr;
	const long page_size = sysconf(_SC_PAGESIZE);

	ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
		   MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	/* Won't be present yet. */
	describe_addr(ptr);
	ptr[0] = 'x';
	/* Anonymous page description. */
	describe_addr(ptr);

	fd = open("test.txt", O_RDWR);
	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
		   MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	close(fd);

	/* Won't be present yet. */
	describe_addr(ptr);
	printf("test contents: %s", ptr);
	/* Now we have read from it, will be present. */
	describe_addr(ptr);
	ptr[0] = 'x';
	/* Now we have updated, should see as dirty. */
	describe_addr(ptr);

	return EXIT_SUCCESS;
}
