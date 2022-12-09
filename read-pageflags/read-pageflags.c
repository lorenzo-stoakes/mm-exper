#include "read-pageflags.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// Obtain a mask with `_bit` set.
#define BIT_MASK(_bit) (1UL << _bit)
// Obtain a mask for lower bits below `_bit`.
#define BIT_MASK_LOWER(_bit) (BIT_MASK(_bit) - 1)
// Dance to allow us to get a string of a macro value.
#define STRINGIFY(_x) STRINGIFY2(_x)
#define STRINGIFY2(_x) #_x

// As per https://www.kernel.org/doc/Documentation/vm/pagemap.txt:

// Indicates if mapping is soft-dirty.
#define PAGEMAP_SOFT_DIRTY_BIT (55)
// Indicates if mapping is exclusively mapped.
#define PAGEMAP_EXCLUSIVELY_MAPPED_BIT (56)
// Indicates file-page or shared-anon.
#define PAGEMAP_FILE_PAGE_SH_ANON_BIT (61)
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

uint64_t extract_pfn(uint64_t val)
{
	if (val == INVALID_VALUE)
		return INVALID_VALUE;

	if (CHECK_BIT(val, PAGEMAP_SWAPPED_BIT))
		return INVALID_VALUE;

	if (!CHECK_BIT(val, PAGEMAP_PRESENT_BIT))
		return INVALID_VALUE;

	return val & PAGEMAP_PFN_MASK;
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

// Prints pagemap flags.
static void print_pagemap_flags(uint64_t val)
{
	printf("[%s%s%s%s%s] ",
	       CHECK_BIT(val, PAGEMAP_SOFT_DIRTY_BIT) ? "s" : " ",
	       CHECK_BIT(val, PAGEMAP_EXCLUSIVELY_MAPPED_BIT) ? "x" : " ",
	       CHECK_BIT(val, PAGEMAP_FILE_PAGE_SH_ANON_BIT) ? "f" : " ",
	       CHECK_BIT(val, PAGEMAP_SWAPPED_BIT) ? "S" : " ",
	       CHECK_BIT(val, PAGEMAP_PRESENT_BIT) ? "p" : " ");
}

uint64_t read_pagemap(const void *ptr)
{
	const uint64_t virt_page_num = (uint64_t)ptr / getpagesize();
	// There is 'one 64-bit value for each virtual page'.
	const uint64_t offset = virt_page_num * sizeof(uint64_t);

	return read_u64("/proc/self/pagemap", offset);
}

uint64_t read_pfn(const void *ptr)
{
	return extract_pfn(read_pagemap(ptr));
}

uint64_t read_kpageflags(uint64_t pfn)
{
	return read_u64("/proc/kpageflags", pfn * sizeof(uint64_t));
}

uint64_t read_mapcount(uint64_t pfn)
{
	return read_u64("/proc/kpagecount", pfn * sizeof(uint64_t));
}

static uint64_t parse_hex(const char *str)
{
	uint64_t ret = 0;

	for (; *str != '\0'; str++) {
		const char chr = *str;

		ret <<= 4;

		if (chr >= 'a' && chr <= 'f') {
			ret += 10 + chr - 'a';
		} else if (chr >= 'A' && chr <= 'F') {
			ret += 10 + chr - 'A';
		} else if (chr >= '0' && chr <= '9') {
			ret += chr - '0';
		} else {
			fprintf(stderr, "ERROR: Invalid char '%c'",
				chr);
			return INVALID_VALUE;
		}
	}

	return ret;
}

static bool extract_address_range(char *str, uint64_t *from_ptr, uint64_t *to_ptr)
{
	uint64_t from, to;
	char *ptr = str;

	for (; *ptr != '\0'; ptr++) {
		const char chr = *ptr;

		if (chr == '-')
			break;
	}

	if (*ptr == '\0')
		return false;

	*ptr = '\0';
	from = parse_hex(str);
	if (from == INVALID_VALUE)
		return false;

	to = parse_hex(ptr + 1);
	if (to == INVALID_VALUE)
		return false;

	*from_ptr = from;
	*to_ptr = to;

	return true;
}

static bool extract_mapdata(const char *line,
			    struct map_data *out)
{
	char range[255], offset[64], dev_num[64];
	int res;

	// Clear ready.
	memset(out, 0, sizeof(struct map_data));

	res = sscanf(line, "%s %s %s %s %lu %s", range, out->perms, offset,
		     dev_num, &out->inode, out->name);

	if (res < 5) {
		fprintf(stderr, "Cannot parse /proc/self/maps line [%s]\n", line);
		return false;
	}

	out->offset = parse_hex(offset);
	if (out->offset == INVALID_VALUE) {
		out->offset = 0;
		return false;
	}

	return true;
}

bool read_mapdata(const void *ptr,
		  struct map_data *out)
{
	uint64_t from, to;
	char *line = NULL;
	size_t len = 0;
	FILE *fp = fopen("/proc/self/maps", "r");
	uint64_t vaddr = (uint64_t)ptr;
	bool ret = true;
	bool found = false;

	if (fp == NULL) {
		fprintf(stderr, "Cannot open /proc/self/maps");
		return false;
	}

	// Find the applicable map line.
	while (getline(&line, &len, fp) >= 0) {
		char buf[255];

		sscanf(line, "%s", buf);
		len = strnlen(buf, sizeof(buf));

		if (!extract_address_range(buf, &from, &to)) {
			ret = false;

			goto out;
		}

		if (vaddr >= from && vaddr < to) {
			found = true;
			break;
		}
	}

	if (!found) {
		fprintf(stderr, "ERROR: Couldn't find maps data for [%p]\n", ptr);
		ret = false;
		goto out;
	}

	ret = extract_mapdata(line, out);

out:
	fclose(fp);
	if (line != NULL)
		free(line);

	return ret;
}

bool print_flags_virt_precalc(const void *ptr,
			      uint64_t pagemap, uint64_t pfn,
			      uint64_t kpageflags, uint64_t mapcount,
			      const struct map_data* mapfields,
			      const char *descr)
{
	// TODO
	(void)mapfields;

	printf("%p: ", ptr);
	print_pagemap_flags(pagemap);

	if (pfn == INVALID_VALUE) {
		printf("(not present) [%s]\n", descr);
		return false;
	} else if (pfn == 0) {
		printf("(cannot retrieve PFN) [%s]\n", descr);
		return false;
	}

	printf("pfn=%lu: ", pfn);

	if (kpageflags == INVALID_VALUE) {
		printf("(cannot retrieve kpageflags) [%s]\n", descr);
		return false;
	}

	const int refcount = get_refcount(pfn);
	if (refcount != -1)
		printf("refcount=%d ", refcount);

	if (mapcount == INVALID_VALUE)
		printf("mapcount=(failed) ");
	else
		printf("mapcount=%lu ", mapcount);

	print_kpageflags(kpageflags);

	printf(" [%s]\n", descr);

	return true;
}

bool print_flags_virt(const void *ptr, const char *descr)
{
	struct map_data mapfields;
	const bool gotfields = read_mapdata(ptr, &mapfields);
	const uint64_t pagemap = read_pagemap(ptr);

	const uint64_t pfn = extract_pfn(pagemap);
	if (pfn == INVALID_VALUE || 0)
		return print_flags_virt_precalc(ptr,
						pagemap,
						pfn,
						INVALID_VALUE,
						INVALID_VALUE,
						gotfields ? &mapfields : NULL,
						descr);

	const uint64_t kpageflags = read_kpageflags(pfn);
	if (kpageflags == INVALID_VALUE)
		return print_flags_virt_precalc(ptr,
						pagemap,
						pfn,
						INVALID_VALUE,
						INVALID_VALUE,
						gotfields ? &mapfields : NULL,
						descr);

	const uint64_t mapcount = read_mapcount(pfn);
	if (mapcount == INVALID_VALUE)
		return print_flags_virt_precalc(ptr,
						pagemap,
						pfn,
						kpageflags,
						INVALID_VALUE,
						gotfields ? &mapfields : NULL,
						descr);

	return print_flags_virt_precalc(ptr,
					pagemap,
					pfn,
					kpageflags,
					mapcount,
					gotfields ? &mapfields : NULL,
					descr);
}
