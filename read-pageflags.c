#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "linux/kernel-page-flags.h"

#define INVALID_VALUE (~(uint64_t)0)

// Obtain a mask for lower bits below `_n`.
#define BIT_MASK(_n) ((1UL << _n) - 1)
// Dance to allow us to get a string of a macro value.
#define STRINGIFY(_x) STRINGIFY2(_x)
#define STRINGIFY2(_x) #_x

// Determine whether `bit` is set in `val`.
static inline bool check_bit(uint64_t val, uint64_t bit)
{
	const uint64_t mask = 1UL << bit;
	return (val & mask) == mask;
}

// Read a single uint64 from the specified path at the specified offset.
static uint64_t read_u64(const char *path, uint64_t offset)
{
	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		const int err = errno;
		fprintf(stderr, "Can't open %s: %s\n", path, strerror(err));

		return INVALID_VALUE;
	}

	uint64_t ret = 0;

	if (fseek(fp, offset, SEEK_SET) != 0) {
		fprintf(stderr, "Can't seek to pointer offset in %s\n", path);
		goto error_close;
	}

	if (fread(&ret, sizeof(ret), 1, fp) != 1) {
		if (feof(fp))
			fprintf(stderr, "EOF in %s?\n", path);
		else if (ferror(fp))
			fprintf(stderr, "Unable to read %s", path);
		else
			fprintf(stderr, "Unknown error on %s read", path);

		goto error_close;
	}

	goto done_close;

error_close:
	ret = INVALID_VALUE;
done_close:
	fclose(fp);
	return ret;
}

// Reads from /proc/self/pagemap to retrieve information on virtual mapping.
// See https://www.kernel.org/doc/Documentation/vm/pagemap.txt for details.
// If unable to retrieve, returns INVALID_VALUE.
static uint64_t read_pagemap(const void *ptr)
{
	// There is 'one 64-bit value for each virtual page'.
	const uint64_t page_num = (uint64_t)ptr / getpagesize();
	const uint64_t offset = page_num * sizeof(uint64_t);

	return read_u64("/proc/self/pagemap", offset);
}

// Invokes read_pagemap() and decodes physical Page Frame Number (PFN) from the
// obtained value.
// If unable to retrieve, returns INVALID_VALUE.
static uint64_t get_pfn(const void *ptr)
{
	uint64_t val = read_pagemap(ptr);
	if (val == INVALID_VALUE)
		return INVALID_VALUE;

	// As per https://www.kernel.org/doc/Documentation/vm/pagemap.txt
	// 'Bits 0-54  page frame number (PFN) if present'
	return val & BIT_MASK(54);
}

// Retrieves kpageflags as described at
// https://www.kernel.org/doc/html/latest/admin-guide/mm/pagemap.html for the
// specified physical page at PFN `pfn`.
// If unable to retrieve, returns INVALID_VALUE.
static uint64_t get_kpageflags(uint64_t pfn)
{
	return read_u64("/proc/kpageflags", pfn * sizeof(uint64_t));
}

// Output all set flags from the specified kpageflags value.
static void print_kpageflags(uint64_t flags)
{
#define CHECK_FLAG(flag) \
	if ((flags & (1UL << KPF_##flag)) == (1UL << KPF_##flag))	\
		printf(STRINGIFY(flag) " ");

	CHECK_FLAG(ACTIVE);
	CHECK_FLAG(ANON);
	CHECK_FLAG(BUDDY);
	CHECK_FLAG(COMPOUND_HEAD);
	CHECK_FLAG(COMPOUND_TAIL);
	CHECK_FLAG(DIRTY);
	CHECK_FLAG(ERROR);
	CHECK_FLAG(HUGE);
	CHECK_FLAG(HWPOISON);
	CHECK_FLAG(IDLE);
	CHECK_FLAG(KSM);
	CHECK_FLAG(LOCKED);
	CHECK_FLAG(LRU);
	CHECK_FLAG(MMAP);
	CHECK_FLAG(NOPAGE);
	CHECK_FLAG(OFFLINE);
	CHECK_FLAG(PGTABLE);
	CHECK_FLAG(RECLAIM);
	CHECK_FLAG(REFERENCED);
	CHECK_FLAG(SLAB);
	CHECK_FLAG(SWAPBACKED);
	CHECK_FLAG(SWAPCACHE);
	CHECK_FLAG(THP);
	CHECK_FLAG(UNEVICTABLE);
	CHECK_FLAG(UPTODATE);
	CHECK_FLAG(WRITEBACK);
	CHECK_FLAG(ZERO_PAGE);

	printf("\n");
#undef CHECK_FLAG
}

// Print kpageflags for the page containing the specified pointer.
// Return value indicates whether succeeded.
static bool print_kpageflags_ptr(const void *ptr)
{
	const uint64_t pfn = get_pfn(ptr);
	if (pfn == INVALID_VALUE) {
		return false;
	} else if (pfn == 0) {
		fprintf(stderr, "Cannot retrieve PFN\n");
		return false;
	}

	const uint64_t kpf = get_kpageflags(pfn);
	if (kpf == INVALID_VALUE) {
		fprintf(stderr, "Cannot retrieve kpageflags\n");
		return false;
	}

	print_kpageflags(kpf);

	return true;
}

int main(void)
{
	// First allocate a page of memory from the kernel, force _actual_
	// allocation via MAP_POPULATE.
	const void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
			       MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	if (!print_kpageflags_ptr(ptr))
		return EXIT_FAILURE;

	munmap((void *)ptr, 4096);

	return EXIT_SUCCESS;
}
