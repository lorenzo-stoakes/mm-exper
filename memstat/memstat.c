#include "memstat.h"

#include <errno.h>
#include <stdbool.h>
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

#define SWAP_TYPE_NUM_BITS (5)
#define SWAP_TYPE_MASK BIT_MASK_LOWER(SWAP_TYPE_NUM_BITS)
#define SWAP_OFFSET_NUM_BITS (50)


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
	char range[255], perms[5], dev_num[64], name[255];
	uint64_t offset, inode;
	int res;

	res = sscanf(line, "%s %s %lu %s %lu %s", range, perms, &offset, dev_num, &inode, name);
	if (res < 5) {
		fprintf(stderr, "ERROR: Can't parse smap header line [%s]", line);
		return false;
	}

	ms->offset = offset;
	strncpy((char *)ms->perms, perms, sizeof(perms));

	// If it has a name, assign it.
	if (res > 5) {
		size_t len = strnlen(name, sizeof(name));
		ms->name = malloc(len);
		strncpy((char *)ms->name, name, len);
	}

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

// Read a single uint64 from the specified path at the specified offset (both
// count and offset expressed in uint64_t's)
static bool read_u64s(uint64_t *ptr, const char *path, uint64_t offset, uint64_t count,
		      bool report_errors)
{
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

	if (fread(ptr, sizeof(uint64_t), count, fp) != count) {
		if (!report_errors)
			goto error_close;

		if (feof(fp))
			fprintf(stderr, "ERROR: EOF in %s?\n", path);
		else if (ferror(fp))
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

static uint64_t get_pfn(uint64_t val)
{
	if (CHECK_BIT(val, PAGEMAP_SWAPPED_BIT) ||
	    !CHECK_BIT(val, PAGEMAP_PRESENT_BIT))
		return INVALID_VALUE;

	return val & PAGEMAP_PFN_MASK;
}

static uint64_t count_virt_pages(const struct memstat *mstat)
{
	return mstat->vm_size * 1024 / getpagesize();
}

static bool get_pagetable_fields(struct memstat *mstat)
{
	const uint64_t count = count_virt_pages(mstat);
	const uint64_t offset = mstat->vma_start / getpagesize();

	mstat->pagemaps = malloc(count * sizeof(uint64_t));
	// These may not be populated depending on whether physical pages are mapped/
	// we have permission to access these.
	mstat->kpagecounts = calloc(count, sizeof(uint64_t));
	mstat->kpageflags = calloc(count, sizeof(uint64_t));

	if (!read_u64s(mstat->pagemaps, "/proc/self/pagemap", offset, count, true))
		return false; // We will free pagemaps elsewhere.

	// Get page flags and counts if they exist.
	for (uint64_t i = 0; i < count; i++) {
		const uint64_t entry = mstat->pagemaps[i];
		const uint64_t pfn = get_pfn(entry);

		if (pfn == INVALID_VALUE)
			continue;

		mstat->kpagecounts[i] = read_u64("/proc/kpagecount", pfn, false);
		mstat->kpageflags[i] = read_u64("/proc/kpageflags", pfn, false);
	}

	return true;
}

static void print_mapping(struct memstat *mstat, uint64_t index)
{
	const uint64_t val = mstat->pagemaps[index];
	const uint64_t pfn = get_pfn(val);

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

	if (CHECK_BIT(val, PAGEMAP_SWAPPED_BIT))
		printf("Sw ");

	if (CHECK_BIT(val, PAGEMAP_PRESENT_BIT))
		printf("p  ");

	if (pfn != INVALID_VALUE)
		printf("pfn=[%lx] ", pfn);

/*


// Page flags
#define
#define  (57)
#define PAGEMAP_IS_FILE (61)
// Indicates page swapped out.
#define PAGEMAP_SWAPPED_BIT (62)
// Indicates page is present.
#define PAGEMAP_PRESENT_BIT (63)

// 'Bits 0-54  page frame number (PFN) if present'
#define PAGEMAP_PFN_NUM_BITS (55)
#define PAGEMAP_PFN_MASK BIT_MASK_LOWER(PAGEMAP_PFN_NUM_BITS)

#define SWAP_TYPE_NUM_BITS (5)
#define SWAP_TYPE_MASK BIT_MASK_LOWER(SWAP_TYPE_NUM_BITS)
#define SWAP_OFFSET_NUM_BITS (50)

 */

	printf("\n");
}

void memstat_print(struct memstat *mstat)
{
	uint64_t i;
	uint64_t addr = mstat->vma_start;
	uint64_t num_pages = count_virt_pages(mstat);

	printf("0x%lx [vma_start]\n", mstat->vma_start);
	printf("0x%lx [vma_end]\n\n", mstat->vma_end);
	printf("vm_size=[%lu] rss=[%lu] ref=[%lu] anon=[%lu] anon_huge=[%lu] swap=[%lu] "
	       "locked=[%lu]\nvm_flags=[%s] perms=[%s] offset=[%lu] name=[%s]\n\n",
	       mstat->vm_size, mstat->rss, mstat->referenced, mstat->anon,
	       mstat->anon_huge, mstat->swap, mstat->locked, mstat->vm_flags, mstat->perms,
	       mstat->offset, mstat->name);

	for (i = 0; i < num_pages; i++, addr += getpagesize()) {
		printf("%lx: ", addr);
		print_mapping(mstat, i);
	}
}

struct memstat *memstat_snapshot(uint64_t vaddr)
{
	uint64_t from, to;
	struct memstat *ret = NULL;
	bool found = false;
	char *line = NULL;
	size_t len = 0;
	const char *path = "/proc/self/smaps";
	FILE *fp = fopen(path, "r");

	if (fp == NULL) {
		fprintf(stderr, "ERROR: Can't open %s\n", path);
		return NULL;
	}

	// Find the start of the smap block.
	while (getline(&line, &len, fp) >= 0) {
		char buf[255];

		sscanf(line, "%s", buf);
		len = strnlen(buf, sizeof(buf));

		if (!is_smap_header_field(buf, len))
			continue;

		if (!extract_address_range(buf, &from, &to))
			goto out;

		if (vaddr >= from && vaddr < to) {
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
	if (!get_pagetable_fields(ret)) {
		memstat_free(ret);
		ret = NULL;
		goto out;
	}

out:
	fclose(fp);
	if (line != NULL)
		free(line);

	return ret;
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
