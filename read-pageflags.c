#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "linux/kernel-page-flags.h"

// Imported from include/linux/kernel-page-flags.h
#define KPF_RESERVED		32
#define KPF_MLOCKED		33
#define KPF_MAPPEDTODISK	34
#define KPF_PRIVATE		35
#define KPF_PRIVATE_2		36
#define KPF_OWNER_PRIVATE	37
#define KPF_ARCH		38
#define KPF_UNCACHED		39
#define KPF_SOFTDIRTY		40
#define KPF_ARCH_2		41

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
		fprintf(stderr, "%p: physical page swapped out!\n", ptr);
		return INVALID_VALUE;
	}

	if (!CHECK_BIT(val, PAGEMAP_PRESENT_BIT)) {
		fprintf(stderr, "%p: physical page not present\n", ptr);
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

// Retrieves page map count as described at
// https://www.kernel.org/doc/html/latest/admin-guide/mm/pagemap.html for the
// specified physical page at PFN `pfn`.
// If unable to retrieve, returns INVALID_VALUE.
static uint64_t get_mapcount(uint64_t pfn)
{
	return read_u64("/proc/kpagecount", pfn * sizeof(uint64_t));
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
	CHECK_FLAG(RESERVED);
	CHECK_FLAG(MLOCKED);

	// Handle overloaded flag.
	if (CHECK_BIT(flags, KPF_MAPPEDTODISK)) {
		if (CHECK_BIT(flags, KPF_ANON))
			printf("ANON_EXCLUSIVE ");
		else
			printf("MAPPEDTODISK ");
	}

	CHECK_FLAG(PRIVATE);
	CHECK_FLAG(PRIVATE_2);
	CHECK_FLAG(OWNER_PRIVATE);
	CHECK_FLAG(ARCH);
	CHECK_FLAG(UNCACHED);
	CHECK_FLAG(SOFTDIRTY);
	CHECK_FLAG(ARCH_2);
#undef CHECK_FLAG
}

// If kernel module has provided a refcount reader, use it.
static int get_refcount(uint64_t pfn)
{
	FILE *fp = fopen("/dev/refcount", "r+");
	if (fp == NULL)
		return -1;

	if (fprintf(fp, "%lu\n", pfn) < 0) {
		perror("writing to /dev/refcount");
		exit(1);
	}

	fflush(fp);
	fclose(fp);

	fp = fopen("/dev/refcount", "r");

	int ret = 0;
	if (fscanf(fp, "%d", &ret) < 0) {
		perror("reading from /dev/refcount");
		exit(1);
	}
	fclose(fp);

	return ret;
}

// Print kpageflags for the page containing the specified pointer.
// Return value indicates whether succeeded.
static bool print_kpageflags_virt(const void *ptr, const char *descr)
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
	const uint64_t mapcount = get_mapcount(pfn);
	if (mapcount == INVALID_VALUE) {
		fprintf(stderr, "Cannot retrieve kpagecount\n");
		return false;
	}

	printf("%p: ", ptr);
	printf("pfn=%lu: ", pfn);

	const int refcount = get_refcount(pfn);
	if (refcount != -1)
		printf("refcount=%d ", refcount);

	printf("mapcount=%lu ", mapcount);

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

	if (!print_kpageflags_virt(ptr, "initial mmap"))
		return EXIT_FAILURE;

	// Do something with the page.
	memset(ptr, 123, 4096);

	if (!print_kpageflags_virt(ptr, "modified page"))
		return EXIT_FAILURE;

	munmap(ptr, 4096);

	void *ptr2 = malloc(4096);
	if (!print_kpageflags_virt(ptr2, "initial malloc"))
		return EXIT_FAILURE;

	memset(ptr2, 123, 4096);

	if (!print_kpageflags_virt(ptr2, "modified malloc"))
		return EXIT_FAILURE;

	int fd = open("test.txt", O_RDWR);
	if (fd == -1) {
		perror("open test.txt");
		return EXIT_FAILURE;
	}

	char *ptr3 = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
			  MAP_SHARED | MAP_POPULATE, fd, 0);
	// We can discard the fd now we've mmap'd it.
	close(fd);

	if (ptr3 == MAP_FAILED) {
		perror("mmap test.txt");
		return EXIT_FAILURE;
	}

	if (!print_kpageflags_virt(ptr3, "mmap file"))
		return EXIT_FAILURE;

	ptr3[0] = 'Z';

	if (!print_kpageflags_virt(ptr3, "mmap modified file"))
		return EXIT_FAILURE;

	madvise(ptr3, 4096, MADV_COLD);

	if (!print_kpageflags_virt(ptr3, "mmap modified, cold file"))
		return EXIT_FAILURE;

	munmap(ptr3, 4096);

	char *ptr4 = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
	if (ptr4 == MAP_FAILED) {
		perror("mmap (4)");
		return EXIT_FAILURE;
	}

	ptr4[0] = 'x';

	print_kpageflags_virt(ptr4, "mmap anon, pre-fork");

	pid_t p = fork();
	if (p == 0) {
		print_kpageflags_virt(ptr4, "mmap anon, forked");

		ptr4[0] = 'y';
		print_kpageflags_virt(ptr4, "mmap anon, forked, modified (pre-sleep)");

		sleep(1);
		print_kpageflags_virt(ptr4, "mmap anon, forked, modified (after sleep)");

		if (ptr4[0] == 'y')
			ptr4[0] = 'x';

		sleep(1);

		print_kpageflags_virt(ptr4, "IVG: mmap anon, forked, modified (after sleep, modify)");

		return EXIT_SUCCESS;
	}
	wait(NULL);

	fd = open("test.txt", O_RDWR);
	if (fd == -1) {
		perror("open test.txt");
		return EXIT_FAILURE;
	}

	char *ptr5 = mmap(NULL, 4097, PROT_READ | PROT_WRITE,
			  MAP_SHARED | MAP_POPULATE, fd, 0);

	close(fd);

	print_kpageflags_virt(ptr5, "mmap file page 1, all bytes");
	print_kpageflags_virt(ptr5 + 4096, "mmap file page 2, all bytes");

	char *ptr6 = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
			  MAP_SHARED | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
	if (ptr6 == MAP_FAILED) {
		perror("mmap (6)");
		return EXIT_FAILURE;
	}
	print_kpageflags_virt(ptr6, "mmap anon, shared");

	pid_t p2 = fork();
	if (p2 == 0) {
		ptr6[0] = 'x';

		print_kpageflags_virt(ptr6, "mmap anon, shared, post fork");

		return EXIT_SUCCESS;
	}
	wait(NULL);
	print_kpageflags_virt(ptr6, "mmap anon, shared, post fork done");

	munmap(ptr6, 4096);

	// Must have set up hugetlb pages in /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
	char *ptr7 = mmap(NULL, 2 * 1024 * 1024, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_HUGETLB, -1, 0);
	if (ptr7 == MAP_FAILED) {
		perror("mmap (7)");
		return EXIT_FAILURE;
	}
	print_kpageflags_virt(ptr7, "mmap anon, hugetlb");
	ptr7[0] = 'x';
	ptr7[4096] = 'y';
	ptr7[2 * 1024 *1024 - 1] = 'z';
	print_kpageflags_virt(ptr7, "mmap anon, hugetlb, post sleep, modification");
	sleep(1);
	print_kpageflags_virt(ptr7, "mmap anon, hugetlb, post sleep, modification");
	return EXIT_SUCCESS;
}
