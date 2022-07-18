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

// Obtain a mask with `_bit` set.
#define BIT_MASK(_bit) (1UL << _bit)
// Obtain a mask for lower bits below `_bit`.
#define BIT_MASK_LOWER(_bit) (BIT_MASK(_bit) - 1)
// Dance to allow us to get a string of a macro value.
#define STRINGIFY(_x) STRINGIFY2(_x)
#define STRINGIFY2(_x) #_x


// As per https://www.kernel.org/doc/Documentation/vm/pagemap.txt:

// Indicates page swapped out.
#define PAGEMAP_SWAPPED_BIT (62)
// Indicates page is present.
#define PAGEMAP_PRESENT_BIT (63)
// 'Bits 0-54  page frame number (PFN) if present'
#define PAGEMAP_PFN_NUM_BITS (55)
#define PAGEMAP_PFN_MASK BIT_MASK_LOWER(PAGEMAP_PFN_NUM_BITS)

#define CHECK_BIT(_val, _bit) ((_val & BIT_MASK(_bit)) == BIT_MASK(_bit))

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
	const uint64_t virt_page_num = (uint64_t)ptr / getpagesize();
	// There is 'one 64-bit value for each virtual page'.
	const uint64_t offset = virt_page_num * sizeof(uint64_t);

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

	if (CHECK_BIT(val, PAGEMAP_SWAPPED_BIT)) {
		fprintf(stderr, "%p: physical page swapped out!", ptr);
		return INVALID_VALUE;
	}

	if (!CHECK_BIT(val, PAGEMAP_PRESENT_BIT)) {
		fprintf(stderr, "%p: physical page not present.", ptr);
		return INVALID_VALUE;
	}

	return val & PAGEMAP_PFN_MASK;
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
#define CHECK_FLAG(flag)			\
	if (CHECK_BIT(flags, KPF_##flag))	\
		printf(STRINGIFY(flag) " ");

	// Alphabetical order.
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

#undef CHECK_FLAG
}

// Print kpageflags for the page containing the specified pointer.
// Return value indicates whether succeeded.
static bool print_kpageflags_ptr(const void *ptr, const char *descr)
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

	printf("%p: ", ptr);
	print_kpageflags(kpf);
	printf(" [%s]\n", descr);

	return true;
}

int main(void)
{
	// First allocate a page of memory from the kernel, force _actual_
	// allocation via MAP_POPULATE.
	void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	if (!print_kpageflags_ptr(ptr, "initial mmap"))
		return EXIT_FAILURE;

	// Do something with the page.
	memset(ptr, 123, 4096);

	if (!print_kpageflags_ptr(ptr, "modified page"))
		return EXIT_FAILURE;

	munmap((void *)ptr, 4096);

	void *ptr2 = malloc(4096);
	if (!print_kpageflags_ptr(ptr2, "initial malloc"))
		return EXIT_FAILURE;

	memset(ptr2, 123, 4096);

	if (!print_kpageflags_ptr(ptr2, "modified malloc"))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
