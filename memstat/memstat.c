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

#define CHECK_FLAG(flag, name)			\
	if (CHECK_BIT(flags, KPF_##flag))	\
		printf(name " ");

	// We print active/referenced elsewhere.

	// Alphabetical order.
	CHECK_FLAG(ANON, "Ano");
	if (mapped_to_disk && anon) // Handle overloaded flag.
		printf("AnE ");
	CHECK_FLAG(BUDDY, "Bud");
	CHECK_FLAG(COMPOUND_HEAD, "CmH");
	CHECK_FLAG(COMPOUND_TAIL, "CmT");
	CHECK_FLAG(DIRTY, "Drt");
	CHECK_FLAG(ERROR, "Err");
	CHECK_FLAG(HUGE, "Hug");
	CHECK_FLAG(HWPOISON, "xxH");
	CHECK_FLAG(IDLE, "Idl");
	CHECK_FLAG(KSM, "KSM");
	CHECK_FLAG(LOCKED, "Lok");
	CHECK_FLAG(LRU, "LRU");
	if (mapped_to_disk && !anon)
		printf("Mdk ");
	CHECK_FLAG(MMAP, "MMp");
	CHECK_FLAG(NOPAGE, "NoP");
	CHECK_FLAG(OFFLINE, "Off");
	CHECK_FLAG(PGTABLE, "Tbl");
	CHECK_FLAG(RECLAIM, "Rcm");
	CHECK_FLAG(SLAB, "Slb");
	CHECK_FLAG(SWAPBACKED, "SwB");
	CHECK_FLAG(SWAPCACHE, "SwC");
	CHECK_FLAG(THP, "THP");
	CHECK_FLAG(UNEVICTABLE, "Une");
	CHECK_FLAG(UPTODATE, "Upd");
	CHECK_FLAG(WRITEBACK, "WrB");
	CHECK_FLAG(ZERO_PAGE, "Zpg");
	CHECK_FLAG(RESERVED, "Rsv");
	CHECK_FLAG(MLOCKED, "Mlk");
	CHECK_FLAG(PRIVATE, "Prv");
	CHECK_FLAG(PRIVATE_2, "Pv2");
	CHECK_FLAG(OWNER_PRIVATE, "OwP");
	CHECK_FLAG(ARCH, "Ach");
	CHECK_FLAG(UNCACHED, "Unc");
	CHECK_FLAG(SOFTDIRTY, "DtS");
	CHECK_FLAG(ARCH_2, "Ar2");
#undef CHECK_FLAG
}

static uint64_t last_seen_map = INVALID_VALUE;
static uint64_t last_seen_pfn;
static uint64_t last_seen_addr;
static uint64_t last_seen_index;
static uint64_t seen_map_count;

static void do_print_mapping(uint64_t addr, struct memstat *mstat, uint64_t index,
			     uint64_t val)
{
	const uint64_t pfn = get_pfn(val);
	const bool have_pfn = pfn != INVALID_VALUE;
	const bool swapped = CHECK_BIT(val, PAGEMAP_SWAPPED_BIT);

	if (addr != INVALID_VALUE)
		printf("%016lx: ", addr);

	// sD = soft-dirty
	// xM = exclusive-mapped
	// uW = uffd-wp write-protected
	// fl = file
	// Sw = swapped
	// pr = present

	if (CHECK_BIT(val, PAGEMAP_SOFT_DIRTY_BIT))
		printf("Ds ");
	else
		printf("   ");

	if (CHECK_BIT(val, PAGEMAP_EXCLUSIVE_MAPPED_BIT))
		printf("Xm ");
	else
		printf("   ");

	if (CHECK_BIT(val, PAGEMAP_UFFD_WP_BIT))
		printf("Uw ");
	else
		printf("   ");

	if (CHECK_BIT(val, PAGEMAP_IS_FILE_BIT))
		printf("Fl ");
	else
		printf("   ");

	if (swapped)
		printf("Sw ");
	else
		printf("   ");

	if (CHECK_BIT(val, PAGEMAP_PRESENT_BIT))
		printf("Pr ");
	else
		printf("   ");

	if (swapped || have_pfn)
		printf("/ ");

	if (swapped) {
		const uint64_t offset = val >> PAGEMAP_SWAP_TYPE_NUM_BITS;

		printf("swap_type=[%lx] ", val & PAGEMAP_SWAP_TYPE_MASK);
		printf("swap_offset=[%lx] ", offset & PAGEMAP_SWAP_OFFSET_MASK);
	}  else if (have_pfn) {
		const uint64_t flags = mstat->kpageflags[index];
		const uint64_t count = mstat->kpagecounts[index];

		if (flags != INVALID_VALUE)
			print_kpageflags(flags);

		printf("/ %lx ", pfn);

		printf("/ ");
		if (CHECK_BIT(flags, KPF_LRU))
			printf("    LRU ");
		else
			printf("NON-LRU ");


		if (CHECK_BIT(flags, KPF_ACTIVE))
			printf("  ACTIVE ");
		else
			printf("INACTIVE ");

		if (CHECK_BIT(flags, KPF_REFERENCED))
			printf("REF ");

		if (count != INVALID_VALUE)
			printf("/ %lu", count);
	}

	printf("\n");
}

static void print_abbrev(struct memstat *mstat)
{
	// Should never happen.
	if (last_seen_map == INVALID_VALUE)
		return;

	if (seen_map_count > 2) {
		printf("%016lx: (%lu more repetitions of above)\n", last_seen_addr, seen_map_count - 1);
	} else if (seen_map_count == 2) {
		do_print_mapping(last_seen_addr, mstat, last_seen_index,
				 last_seen_pfn == INVALID_VALUE
				 ? last_seen_map
				 : last_seen_map | last_seen_pfn);
	}
}

static void print_mapping(uint64_t addr, struct memstat *mstat, uint64_t index,
			  bool abbrev)
{
	const uint64_t val = mstat->pagemaps[index];
	const uint64_t pfn = get_pfn(val);
	const bool new_val = (val & ~PAGEMAP_PFN_MASK) != last_seen_map ||
		(pfn != last_seen_pfn && pfn != last_seen_pfn + 1);

	if (abbrev && new_val) {
		print_abbrev(mstat);
		seen_map_count = 0;
	}

	last_seen_map = val & ~PAGEMAP_PFN_MASK;
	last_seen_pfn = pfn;
	last_seen_addr = addr;
	last_seen_index = index;
	seen_map_count++;

	if (abbrev && !new_val)
		return;

	do_print_mapping(addr, mstat, index, val);
}

static void print_mapping_terminate(struct memstat *mstat)
{
	print_abbrev(mstat);

	last_seen_map = INVALID_VALUE;
	last_seen_index = 0;
	last_seen_pfn = 0;
	seen_map_count = 0;
}

static void do_print_name(const char *name)
{
	printf("----==== %s ====---- \n\n", name);
}

static void print_name(struct memstat *mstat)
{
	if (mstat->name == NULL)
		do_print_name("(anon)");
	else
		do_print_name(mstat->name);
}

static void print_header(struct memstat *mstat)
{
	print_name(mstat);

	printf("0x%lx [vma_start]\n", mstat->vma_start);
	printf("0x%lx [vma_end]\n\n", mstat->vma_end);
	printf("vm_size=[%lu] rss=[%lu%s] ref=[%lu] anon=[%lu] anon_huge=[%lu] swap=[%lu] "
	       "locked=[%lu]\nvm_flags=[%s] perms=[%s] offset=[%lu]\n\n",
	       mstat->vm_size, mstat->rss, mstat->rss_counted ? "*" : "", mstat->referenced, mstat->anon,
	       mstat->anon_huge, mstat->swap, mstat->locked, mstat->vm_flags, mstat->perms,
	       mstat->offset);
}

static bool ignored_mstat(struct memstat *mstat)
{
	return
		strcmp(mstat->name, "[vvar]") == 0 ||
		strcmp(mstat->name, "[vdso]") == 0 ||
		strcmp(mstat->name, "[vsyscall]") == 0 ||
		strcmp(mstat->name, "[heap]") == 0 ||
		strcmp(mstat->name, "[stack]") == 0 ||
		strncmp(mstat->name, "/usr/bin", sizeof("/usr/bin") - 1) == 0 ||
		strncmp(mstat->name, "/usr/lib", sizeof("/usr/lib") - 1) == 0;
}

bool memstat_print(struct memstat *mstat)
{
	uint64_t i;
	uint64_t addr;
	uint64_t num_pages;

	if (mstat == NULL || (mstat->name != NULL && ignored_mstat(mstat)))
		return false;

	print_header(mstat);

	addr = mstat->vma_start;
	num_pages = count_virt_pages(mstat);

	for (i = 0; i < num_pages; i++, addr += getpagesize()) {
		print_mapping(addr, mstat, i, true);
	}
	print_mapping_terminate(mstat);

	return true;
}

static void print_separator(void)
{
	printf("\n");
}

void memstat_print_all(struct memstat **mstats)
{
	int i;

	for (i = 0; i < MAX_MAPS; i++) {
		struct memstat *mstat = mstats[i];
		if (mstat == NULL)
			break;

		if (memstat_print(mstat))
			print_separator();
	}
}

bool memstat_print_diff(struct memstat *mstat_a, struct memstat *mstat_b)
{
	uint64_t i;
	uint64_t addr;
	uint64_t num_pages;
	bool seen_first = false;

	if (mstat_a == NULL && mstat_b == NULL)
		return false;

	if (mstat_a == NULL) {
		const bool printed = memstat_print(mstat_b);
		return printed;
	}

	if (mstat_b == NULL) {
		const bool printed = memstat_print(mstat_a);
		return printed;
	}

	addr = mstat_a->vma_start;
	num_pages = count_virt_pages(mstat_a);

	// This will be too fiddly to deal with manually so just output both.
	if (mstat_a->vma_start != mstat_b->vma_start ||
	    mstat_a->vma_end != mstat_b->vma_end) {
	       const bool printed = memstat_print(mstat_a);

	       if (!printed)
		       return false;

	       printf("^^^^---- VMA RANGE CHANGE. Outputting both for manual diff ----vvvv\n");
	       memstat_print(mstat_b);

	       return true;
	}

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

		print_mapping(addr, mstat_a, i, false);
		printf("               -> ");
		print_mapping(INVALID_VALUE, mstat_b, i, false);
	}

	if (!seen_first)
		return false;

	printf("\n");
	print_header(mstat_a);

	return true;
}

bool memstat_print_diff_all(struct memstat **mstats_a, struct memstat **mstats_b)
{
	int i;
	bool seen = false;

	for (i = 0; i < MAX_MAPS; i++) {
		bool updated;
		struct memstat *mstat_a = mstats_a[i];
		struct memstat *mstat_b = mstats_b[i];

		updated = memstat_print_diff(mstat_a, mstat_b);
		if (!updated)
			continue;

		seen = true;

		printf("\n");
		print_separator();
		printf("\n");
	}

	return seen;
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
