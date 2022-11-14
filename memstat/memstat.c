#include "memstat.h"

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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INVALID_VALUE ((uint64_t)-1)

// Obtain a mask with `_bit` set.
#define BIT_MASK(_bit) (1UL << _bit)
// Obtain a mask for lower bits below `_bit`.
#define BIT_MASK_LOWER(_bit) (BIT_MASK(_bit) - 1)
// Dance to allow us to get a string of a macro value.
#define STRINGIFY(_x) STRINGIFY2(_x)
#define STRINGIFY2(_x) #_x

#define CHECK_BIT(_val, _bit) ((_val & BIT_MASK(_bit)) == BIT_MASK(_bit))

// Page flags
#define PAGEMAP_SOFT_DIRTY_BIT (55)
#define PAGEMAP_EXCLUSIVE_MAPPED_BIT (56)
#define PAGEMAP_UFFD_WP_BIT (57)
#define PAGEMAP_IS_FILE_BIT (61)
// Indicates page swapped out.
#define PAGEMAP_SWAPPED_BIT (62)
// Indicates page is present.
#define PAGEMAP_PRESENT_BIT (63)

// 'Bits 0-54  page frame number (PFN) if present'
#define PAGEMAP_PFN_NUM_BITS (55)
#define PAGEMAP_PFN_MASK BIT_MASK_LOWER(PAGEMAP_PFN_NUM_BITS)

#define PAGEMAP_SWAP_TYPE_NUM_BITS (5)
#define PAGEMAP_SWAP_TYPE_MASK BIT_MASK_LOWER(PAGEMAP_SWAP_TYPE_NUM_BITS)
#define PAGEMAP_SWAP_OFFSET_NUM_BITS (50)
#define PAGEMAP_SWAP_OFFSET_MASK BIT_MASK_LOWER(PAGEMAP_SWAP_OFFSET_NUM_BITS)

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

static bool is_smap_header_field(const char *line, size_t len)
{
	return line[len-1] != ':';
}

static bool get_smap_header_fields(struct memstat *ms, char *line)
{
	char range[255], perms[5], dev_num[64], offset[64], name[255];
	uint64_t inode;
	int res;

	res = sscanf(line, "%s %s %s %s %lu %s", range, perms, offset, dev_num, &inode, name);
	if (res < 5) {
		fprintf(stderr, "ERROR: Can't parse smap header line [%s]", line);
		return false;
	}

	ms->offset = parse_hex(offset);
	strncpy((char *)ms->perms, perms, sizeof(perms));

	if (res <= 5)
		return true;

	// If it has a name, assign it.
	size_t len = strnlen(name, sizeof(name));
	ms->name = malloc(len + 1);
	strncpy((char *)ms->name, name, len + 1);

	return true;
}

static bool get_smap_other_fields(struct memstat *ms, char *line, FILE *smaps_fp)
{
	size_t len = 0;

	while (getline(&line, &len, smaps_fp) >= 0) {
		char key[255], unit[16];
		uint64_t size;

		sscanf(line, "%s %lu %s", key, &size, unit);
		if (strncmp(unit, "kB", sizeof(key)) != 0) {
			fprintf(stderr, "ERROR: Unrecognised unit '%s'\n", unit);
			return false;
		}

		if (is_smap_header_field(key, strnlen(key, sizeof(key))))
			break;

		if (strncmp(key, "Size:", sizeof(key)) == 0)
			ms->vm_size = size;
		else if (strncmp(key, "Rss:", sizeof(key)) == 0)
			ms->rss = size;
		else if (strncmp(key, "Referenced:", sizeof(key)) == 0)
			ms->referenced = size;
		else if (strncmp(key, "Anonymous:", sizeof(key)) == 0)
			ms->anon = size;
		else if (strncmp(key, "AnonHugePages:", sizeof(key)) == 0)
			ms->anon_huge = size;
		else if (strncmp(key, "Swap:", sizeof(key)) == 0)
			ms->swap = size;
		else if (strncmp(key, "Locked:", sizeof(key)) == 0)
			ms->locked = size;
		else if (strncmp(key, "VmFlags:", sizeof(key)) == 0) {
			ssize_t count;
			const ssize_t offset = sizeof("VmFlags: ");

			len = strnlen(line, len);
			for (count = len - offset; count > 0; count--) {
				const char chr = line[offset + count - 1];

				if (chr != '\n' && chr != ' ')
					break;
			}
			if (count <= 0)
				return false;

			count++;

			ms->vm_flags = malloc(count + 1);

			memcpy((char *)ms->vm_flags, &line[offset - 1], count);
			((char *)ms->vm_flags)[count] = '\0';
		}
	}

	return true;
}

// Read count uint64s from the specified path at the specified offset.
static bool read_u64s(uint64_t *ptr, const char *path, uint64_t offset, uint64_t count,
		      bool report_errors)
{
	uint64_t read;
	FILE *fp = fopen(path, "r");
	bool ret = true;

	if (fp == NULL) {
		const int err = errno;

		if (report_errors)
			fprintf(stderr, "ERROR: Can't open %s: %s\n", path, strerror(err));

		return false;
	}

	if (fseek(fp, offset * sizeof(uint64_t), SEEK_SET) != 0) {
		if (report_errors)
			fprintf(stderr, "ERROR: Can't seek to pointer offset in %s\n", path);

		goto error_close;
	}

	if ((read = fread(ptr, sizeof(uint64_t), count, fp)) != count) {
		if (feof(fp) || !report_errors)
			goto error_close;

		if (ferror(fp))
			fprintf(stderr, "ERROR: Unable to read %s\n", path);
		else
			fprintf(stderr, "ERROR: Unknown error on %s read\n", path);

		goto error_close;
	}

	goto done_close;

error_close:
	ret = false;
done_close:
	fclose(fp);
	return ret;
}

static uint64_t read_u64(const char *path, uint64_t offset, bool report_errors)
{
	uint64_t ret;

	if (!read_u64s(&ret, path, offset, 1, report_errors))
		return INVALID_VALUE;

	return ret;
}

static bool has_pfn(uint64_t val)
{
	return !CHECK_BIT(val, PAGEMAP_SWAPPED_BIT) &&
		CHECK_BIT(val, PAGEMAP_PRESENT_BIT);
}

static uint64_t get_pfn(uint64_t val)
{
	if (!has_pfn(val))
		return INVALID_VALUE;

	return val & PAGEMAP_PFN_MASK;
}

static uint64_t count_virt_pages(const struct memstat *mstat)
{
	return mstat->vm_size * 1024 / getpagesize();
}

// Stats aren't always updated quickly so also do our own counting.
static void tweak_counts(struct memstat *mstat, uint64_t count)
{
	uint64_t i;
	uint64_t rss_kib;
	uint64_t pagecount = 0;

	// For now we just tweak RSS.

	for (i = 0; i < count; i++) {
		const uint64_t entry = mstat->pagemaps[i];

		if (has_pfn(entry))
			pagecount++;
	}

	rss_kib = pagecount * getpagesize() / 1024;

	if (mstat->rss == rss_kib)
		return;

	mstat->rss = rss_kib;
	mstat->rss_counted = true;
}

static bool get_pagetable_fields(const char *pid, struct memstat *mstat)
{
	uint64_t i;
	const uint64_t count = count_virt_pages(mstat);
	const uint64_t offset = mstat->vma_start / getpagesize();

	char path[512] = "/proc/";

	strncat(path, pid, sizeof(path) - 1);
	strncat(path, "/pagemap", sizeof(path) - 1);

	mstat->pagemaps = malloc(count * sizeof(uint64_t));
	// These may not be populated depending on whether physical pages are mapped/
	// we have permission to access these.
	mstat->kpagecounts = calloc(count, sizeof(uint64_t));
	mstat->kpageflags = calloc(count, sizeof(uint64_t));

	if (!read_u64s(mstat->pagemaps, path, offset, count, true))
		return false; // We will free pagemaps elsewhere.

	// Get page flags and counts if they exist.
	for (i = 0; i < count; i++) {
		const uint64_t entry = mstat->pagemaps[i];
		const uint64_t pfn = get_pfn(entry);

		if (pfn == INVALID_VALUE)
			continue;

		// These can be set to INVALID_VALUE which we will check later.
		mstat->kpagecounts[i] = read_u64("/proc/kpagecount", pfn, false);
		mstat->kpageflags[i] = read_u64("/proc/kpageflags", pfn, false);
	}

	tweak_counts(mstat, count);

	return true;
}

// Output all set flags from the specified kpageflags value.
static void print_kpageflags(uint64_t flags)
{
	const bool mapped_to_disk = CHECK_BIT(flags, KPF_MAPPEDTODISK);
	const bool anon = CHECK_BIT(flags, KPF_ANON);

#define CHECK_FLAG(flag)			\
	if (CHECK_BIT(flags, KPF_##flag))	\
		printf(STRINGIFY(flag) " ");

	// Alphabetical order.
	CHECK_FLAG(ACTIVE);
	CHECK_FLAG(ANON);
	if (mapped_to_disk && anon) // Handle overloaded flag.
		printf("ANON_EXCLUSIVE ");
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
	if (mapped_to_disk && !anon)
		printf("MAPPEDTODISK ");
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
	CHECK_FLAG(PRIVATE);
	CHECK_FLAG(PRIVATE_2);
	CHECK_FLAG(OWNER_PRIVATE);
	CHECK_FLAG(ARCH);
	CHECK_FLAG(UNCACHED);
	CHECK_FLAG(SOFTDIRTY);
	CHECK_FLAG(ARCH_2);
#undef CHECK_FLAG
}

static void print_mapping(struct memstat *mstat, uint64_t index)
{
	const uint64_t val = mstat->pagemaps[index];
	const uint64_t pfn = get_pfn(val);
	const bool have_pfn = pfn != INVALID_VALUE;
	const bool swapped = CHECK_BIT(val, PAGEMAP_SWAPPED_BIT);

	printf("%016lx: ", val); // raw

	// sD = soft-dirty
	// xM = exclusive-mapped
	// uW = uffd-wp write-protected
	// f  = file
	// Sw = swapped
	// p  = present

	if (CHECK_BIT(val, PAGEMAP_SOFT_DIRTY_BIT))
		printf("sD ");

	if (CHECK_BIT(val, PAGEMAP_EXCLUSIVE_MAPPED_BIT))
		printf("xM ");

	if (CHECK_BIT(val, PAGEMAP_UFFD_WP_BIT))
		printf("uW ");

	if (CHECK_BIT(val, PAGEMAP_IS_FILE_BIT))
		printf("f  ");

	if (swapped)
		printf("Sw ");

	if (CHECK_BIT(val, PAGEMAP_PRESENT_BIT))
		printf("p  ");

	if (swapped) {
		const uint64_t offset = val >> PAGEMAP_SWAP_TYPE_NUM_BITS;

		printf("swap_type=[%lx] ", val & PAGEMAP_SWAP_TYPE_MASK);
		printf("swap_offset=[%lx] ", offset & PAGEMAP_SWAP_OFFSET_MASK);
	}  else if (have_pfn) {
		const uint64_t flags = mstat->kpageflags[index];
		const uint64_t count = mstat->kpagecounts[index];

		printf("pfn=[%lx] ", pfn);

		if (flags != INVALID_VALUE)
			print_kpageflags(flags);

		if (count != INVALID_VALUE)
			printf("mapcount=[%lu]", count);
	}

	printf("\n");
}

void memstat_print(struct memstat *mstat)
{
	uint64_t i;
	uint64_t addr = mstat->vma_start;
	uint64_t num_pages = count_virt_pages(mstat);

	printf("0x%lx [vma_start]\n", mstat->vma_start);
	printf("0x%lx [vma_end]\n\n", mstat->vma_end);
	printf("vm_size=[%lu] rss=[%lu%s] ref=[%lu] anon=[%lu] anon_huge=[%lu] swap=[%lu] "
	       "locked=[%lu]\nvm_flags=[%s] perms=[%s] offset=[%lu] name=[%s]\n\n",
	       mstat->vm_size, mstat->rss, mstat->rss_counted ? "*" : "", mstat->referenced, mstat->anon,
	       mstat->anon_huge, mstat->swap, mstat->locked, mstat->vm_flags, mstat->perms,
	       mstat->offset, mstat->name == NULL ? "" : mstat->name);

	for (i = 0; i < num_pages; i++, addr += getpagesize()) {
		printf("%lx: ", addr);
		print_mapping(mstat, i);
	}
}

void memstat_print_diff(struct memstat *mstat_a, struct memstat *mstat_b)
{
	uint64_t i;
	uint64_t addr;
	uint64_t num_pages;
	bool seen_first = false;

	if (mstat_a == NULL || mstat_b == NULL) {
		fprintf(stderr, "NULL input?");
		return;
	}

	addr = mstat_a->vma_start;
	num_pages = count_virt_pages(mstat_a);

	// This will be too fiddly to deal with manually so just output both.
	if (mstat_a->vma_start != mstat_b->vma_start ||
	    mstat_a->vma_end != mstat_b->vma_end) {
		printf("VMA RANGE CHANGE. Outputting both for manual diff:-\\");
		memstat_print(mstat_a);
		printf("========\n");
		memstat_print(mstat_b);
		return;
	}

	printf("0x%lx [vma_start]\n", mstat_a->vma_start);
	printf("0x%lx [vma_end]\n\n", mstat_a->vma_end);

	// We don't need to check vm_size because of above check.

#define COMPARE(field, fmt, ...)		\
	if (mstat_a->field != mstat_b->field) {	\
		seen_first = true;		\
		printf(fmt, __VA_ARGS__);	\
	}

#define COMPARE_STR(field, fmt, ...)				\
	{							\
		const char *from = mstat_a->field == NULL	\
			? "" : mstat_a->field;			\
		const char *to = mstat_b->field == NULL		\
			? "" : mstat_b->field;			\
		if (strncmp(from, to, 255) != 0) {		\
			seen_first = true;			\
			printf(fmt, __VA_ARGS__);		\
		}						\
	}

#define COMPARE_STR_UNSAFE(field, fmt, ...)			\
	if (strcmp(mstat_a->field, mstat_b->field) != 0) {	\
		seen_first = true;				\
		printf(fmt, __VA_ARGS__);			\
	}

	COMPARE(rss, "rss=[%lu%s]->[%lu%s] ", mstat_a->rss,
		mstat_a->rss_counted ? "*" : "",
		mstat_b->rss, mstat_b->rss_counted ? "*" : "");
	COMPARE(referenced, "ref=[%lu]->[%lu] ", mstat_a->referenced, mstat_b->referenced);
	COMPARE(anon, "anon=[%lu]->[%lu] ", mstat_a->anon, mstat_b->anon);
	COMPARE(anon_huge, "anon_huge=[%lu]->[%lu] ", mstat_a->anon_huge, mstat_b->anon_huge);
	COMPARE(swap, "swap=[%lu]->[%lu] ", mstat_a->swap, mstat_b->swap);
	COMPARE(locked, "locked=[%lu]->[%lu] ", mstat_a->locked, mstat_b->locked);

	if (seen_first) {
		printf("\n");
		seen_first = false;
	}

	COMPARE_STR(vm_flags, "vm_flags=[%s]->[%s] ", mstat_a->vm_flags, mstat_b->vm_flags);
	COMPARE_STR_UNSAFE(perms, "perms=[%s]->[%s] ", mstat_a->perms, mstat_b->perms);
	COMPARE(offset, "offset=[%lu]->[%lu] ", mstat_a->offset, mstat_b->offset);
	COMPARE_STR(name, "name=[%s]->[%s] ", mstat_a->name, mstat_b->name);

#undef COMPARE
#undef COMPARE_STR
#undef COMPARE_STR_UNSAFE

	if (seen_first) {
		printf("\n");
		seen_first = false;
	}

	for (i = 0; i < num_pages; i++, addr += getpagesize()) {
		if (mstat_a->pagemaps[i] == mstat_b->pagemaps[i] &&
		    mstat_a->kpagecounts[i] == mstat_b->kpagecounts[i] &&
		    mstat_a->kpageflags[i] == mstat_b->kpageflags[i])
			continue;

		if (!seen_first) {
			printf("\n");
			seen_first = true;
		}

		printf("%016lx: ", addr);
		print_mapping(mstat_a, i);
		printf("               -> ");
		print_mapping(mstat_b, i);
	}
}

static FILE *open_smaps(const char *pid)
{
	FILE *fp;
	char path[512] = "/proc/";

	strncat(path, pid, sizeof(path) - 1);
	strncat(path, "/smaps", sizeof(path) - 1);

	fp = fopen(path, "r");

	if (fp == NULL) {
		fprintf(stderr, "ERROR: Can't open %s\n", path);
		return NULL;
	}

	return fp;
}

static struct memstat *get_memstat_snapshot(FILE *fp, const char *pid, uint64_t vaddr)
{
	uint64_t from, to;
	struct memstat *ret = NULL;
	bool found = false;
	char *line = NULL;
	size_t len = 0;

	if (fp == NULL)
		return NULL;

	// Find the start of the smap block.
	while (getline(&line, &len, fp) >= 0) {
		char buf[255];

		sscanf(line, "%s", buf);
		len = strnlen(buf, sizeof(buf));

		if (!is_smap_header_field(buf, len))
			continue;

		if (!extract_address_range(buf, &from, &to))
			goto out;

		// INVALID_VALUE implies get first.
		if (vaddr == INVALID_VALUE || (vaddr >= from && vaddr < to)) {
			found = true;
			break;
		}
	}

	if (!found)
		goto out;

	ret = calloc(1, sizeof(*ret));

	ret->vma_start = from;
	ret->vma_end = to;

	// Extract header line fields.
	if (!get_smap_header_fields(ret, line)) {
		memstat_free(ret);
		ret = NULL;
		goto out;
	}

	// Now get the other fields.
	if (!get_smap_other_fields(ret, line, fp)) {
		memstat_free(ret);
		ret = NULL;
		goto out;
	}

	// Finally, get page table fields.
	if (!get_pagetable_fields(pid, ret)) {
		memstat_free(ret);
		ret = NULL;
		goto out;
	}

out:
	if (line != NULL)
		free(line);

	return ret;
}

struct memstat *memstat_snapshot(uint64_t vaddr)
{
	FILE *fp = open_smaps("self");
	struct memstat *ret = get_memstat_snapshot(fp, "self", vaddr);

	fclose(fp);

	return ret;
}

struct memstat *memstat_snapshot_remote(const char *pid, uint64_t vaddr)
{
	FILE *fp = open_smaps(pid);
	struct memstat *ret = get_memstat_snapshot(fp, pid, vaddr);

	fclose(fp);

	return ret;
}

struct memstat **memstat_snapshot_all(const char *pid)
{
	int i;
	FILE *fp = open_smaps(pid);
	struct memstat **ret = calloc(MAX_MAPS, sizeof(struct memstat*));

	for (i = 0; i < MAX_MAPS; i++) {
		// Get next snapshot.
		ret[i] = get_memstat_snapshot(fp, pid, INVALID_VALUE);
		if (ret[i] == NULL)
			return ret;
	}

	fprintf(stderr, "ERROR: More than %d maps!", MAX_MAPS);
	memstat_free_all(ret);

	return NULL;
}

void memstat_free(struct memstat* mstat)
{
	if (mstat == NULL)
		return;

	if (mstat->name != NULL)
		free((void *)mstat->name);

	if (mstat->vm_flags != NULL)
		free((void *)mstat->vm_flags);

	if (mstat->pagemaps != NULL)
		free(mstat->pagemaps);

	if (mstat->kpagecounts != NULL)
		free(mstat->kpagecounts);

	if (mstat->kpageflags != NULL)
		free(mstat->kpageflags);

	free(mstat);
}

void memstat_free_all(struct memstat **mstats)
{
	for (int i = 0; i < MAX_MAPS; i++) {
		struct memstat *mstat = mstats[i];
		if (mstat == NULL)
			return;

		memstat_free(mstat);
	}
}
