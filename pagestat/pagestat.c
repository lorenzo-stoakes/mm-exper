#include "pagestat.h"

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

static bool get_smap_header_fields(struct pagestat *ms, char *line)
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

static bool get_smap_other_fields(struct pagestat *ms, char *line, FILE *smaps_fp)
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

static uint64_t count_virt_pages(const struct pagestat *ps)
{
	return ps->vm_size * 1024 / getpagesize();
}

// Stats aren't always updated quickly so also do our own counting.
static void tweak_counts(struct pagestat *ps, uint64_t count)
{
	uint64_t i;
	uint64_t rss_kib;
	uint64_t pagecount = 0;

	// For now we just tweak RSS.

	for (i = 0; i < count; i++) {
		const uint64_t entry = ps->pagemaps[i];

		if (has_pfn(entry))
			pagecount++;
	}

	rss_kib = pagecount * getpagesize() / 1024;

	if (ps->rss == rss_kib)
		return;

	ps->rss = rss_kib;
	ps->rss_counted = true;
}

static bool get_pagetable_fields(const char *pid, struct pagestat *ps)
{
	uint64_t i;
	const uint64_t count = count_virt_pages(ps);
	const uint64_t offset = ps->vma_start / getpagesize();

	char path[512] = "/proc/";

	strncat(path, pid, sizeof(path) - 1);
	strncat(path, "/pagemap", sizeof(path) - 1);

	ps->pagemaps = malloc(count * sizeof(uint64_t));
	// These may not be populated depending on whether physical pages are mapped/
	// we have permission to access these.
	ps->kpagecounts = calloc(count, sizeof(uint64_t));
	ps->kpageflags = calloc(count, sizeof(uint64_t));

	if (!read_u64s(ps->pagemaps, path, offset, count, true))
		return false; // We will free pagemaps elsewhere.

	// Get page flags and counts if they exist.
	for (i = 0; i < count; i++) {
		const uint64_t entry = ps->pagemaps[i];
		const uint64_t pfn = get_pfn(entry);

		if (pfn == INVALID_VALUE)
			continue;

		// These can be set to INVALID_VALUE which we will check later.
		ps->kpagecounts[i] = read_u64("/proc/kpagecount", pfn, false);
		ps->kpageflags[i] = read_u64("/proc/kpageflags", pfn, false);
	}

	tweak_counts(ps, count);

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

static void do_print_mapping(uint64_t addr, struct pagestat *ps, uint64_t index,
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
		const uint64_t flags = ps->kpageflags[index];
		const uint64_t count = ps->kpagecounts[index];

		if (flags != INVALID_VALUE)
			print_kpageflags(flags);

		printf("/ %lx ", pfn);

		printf("/ ");
		if (CHECK_BIT(flags, KPF_LRU))
			printf("LRU ");
		else
			printf("NON-LRU ");


		if (CHECK_BIT(flags, KPF_ACTIVE))
			printf("ACTIVE ");
		else
			printf("INACTIVE ");

		if (CHECK_BIT(flags, KPF_REFERENCED))
			printf("REF ");

		if (count != INVALID_VALUE)
			printf("/ %lu", count);
	}

	printf("\n");
}

static void print_abbrev(struct pagestat *ps)
{
	// Should never happen.
	if (last_seen_map == INVALID_VALUE)
		return;

	if (seen_map_count > 2) {
		printf("%016lx: (%lu more repetitions of above)\n", last_seen_addr, seen_map_count - 1);
	} else if (seen_map_count == 2) {
		do_print_mapping(last_seen_addr, ps, last_seen_index,
				 last_seen_pfn == INVALID_VALUE
				 ? last_seen_map
				 : last_seen_map | last_seen_pfn);
	}
}

static void print_mapping(uint64_t addr, struct pagestat *ps, uint64_t index,
			  bool abbrev)
{
	const uint64_t val = ps->pagemaps[index];
	const uint64_t pfn = get_pfn(val);
	const bool new_val = (val & ~PAGEMAP_PFN_MASK) != last_seen_map ||
		(pfn != last_seen_pfn && pfn != last_seen_pfn + 1);

	if (abbrev && new_val) {
		print_abbrev(ps);
		seen_map_count = 0;
	}

	last_seen_map = val & ~PAGEMAP_PFN_MASK;
	last_seen_pfn = pfn;
	last_seen_addr = addr;
	last_seen_index = index;
	seen_map_count++;

	if (abbrev && !new_val)
		return;

	do_print_mapping(addr, ps, index, val);
}

static void print_mapping_terminate(struct pagestat *ps)
{
	print_abbrev(ps);

	last_seen_map = INVALID_VALUE;
	last_seen_index = 0;
	last_seen_pfn = 0;
	seen_map_count = 0;
}

static void do_print_name(const char *name)
{
	printf("----==== %s ====---- \n\n", name);
}

static void print_name(struct pagestat *ps)
{
	if (ps->name == NULL)
		do_print_name("(anon)");
	else
		do_print_name(ps->name);
}

static void print_header(struct pagestat *ps)
{
	print_name(ps);

	printf("0x%lx [vma_start]\n", ps->vma_start);
	printf("0x%lx [vma_end]\n\n", ps->vma_end);
	printf("vm_size=[%lu] rss=[%lu%s] ref=[%lu] anon=[%lu] anon_huge=[%lu] swap=[%lu] "
	       "locked=[%lu]\nvm_flags=[%s] perms=[%s] offset=[%lu]\n\n",
	       ps->vm_size, ps->rss, ps->rss_counted ? "*" : "", ps->referenced, ps->anon,
	       ps->anon_huge, ps->swap, ps->locked, ps->vm_flags, ps->perms,
	       ps->offset);
}

static bool ignored_ps(struct pagestat *ps)
{
	return
		strcmp(ps->name, "[vvar]") == 0 ||
		strcmp(ps->name, "[vdso]") == 0 ||
		strcmp(ps->name, "[vsyscall]") == 0 ||
		strcmp(ps->name, "[heap]") == 0 ||
		strcmp(ps->name, "[stack]") == 0 ||
		strncmp(ps->name, "/usr/bin", sizeof("/usr/bin") - 1) == 0 ||
		strncmp(ps->name, "/usr/lib", sizeof("/usr/lib") - 1) == 0 ||
		strncmp(ps->name, "/var/cache", sizeof("/var/cache") - 1) == 0 ||
		strncmp(ps->name, "/usr/share", sizeof("/usr/share") - 1) == 0;
}

bool pagestat_print(struct pagestat *ps)
{
	uint64_t i;
	uint64_t addr;
	uint64_t num_pages;

	if (ps == NULL || (ps->name != NULL && ignored_ps(ps)))
		return false;

	print_header(ps);

	addr = ps->vma_start;
	num_pages = count_virt_pages(ps);

	for (i = 0; i < num_pages; i++, addr += getpagesize()) {
		print_mapping(addr, ps, i, true);
	}
	print_mapping_terminate(ps);

	return true;
}

static void print_separator(void)
{
	printf("\n");
}

void pagestat_print_all(struct pagestat **pss)
{
	int i;

	for (i = 0; i < MAX_MAPS; i++) {
		struct pagestat *ps = pss[i];
		if (ps == NULL)
			break;

		if (pagestat_print(ps))
			print_separator();
	}
}

bool pagestat_print_diff(struct pagestat *ps_a, struct pagestat *ps_b)
{
	uint64_t i;
	uint64_t addr;
	uint64_t num_pages;
	bool seen_first = false;

	if (ps_a == NULL && ps_b == NULL)
		return false;

	if (ps_a == NULL) {
		const bool printed = pagestat_print(ps_b);
		return printed;
	}

	if (ps_b == NULL) {
		const bool printed = pagestat_print(ps_a);
		return printed;
	}

	addr = ps_a->vma_start;
	num_pages = count_virt_pages(ps_a);

	// This will be too fiddly to deal with manually so just output both.
	if (ps_a->vma_start != ps_b->vma_start ||
	    ps_a->vma_end != ps_b->vma_end) {
	       const bool printed = pagestat_print(ps_a);

	       if (!printed)
		       return false;

	       printf("^^^^---- VMA RANGE CHANGE. Outputting both for manual diff ----vvvv\n");
	       pagestat_print(ps_b);

	       return true;
	}

	// We don't need to check vm_size because of above check.

#define COMPARE(field, fmt, ...)		\
	if (ps_a->field != ps_b->field) {	\
		seen_first = true;		\
		printf(fmt, __VA_ARGS__);	\
	}

#define COMPARE_STR(field, fmt, ...)			\
	{						\
		const char *from = ps_a->field == NULL	\
			? "" : ps_a->field;		\
		const char *to = ps_b->field == NULL	\
			? "" : ps_b->field;		\
		if (strncmp(from, to, 255) != 0) {	\
			seen_first = true;		\
			printf(fmt, __VA_ARGS__);	\
		}					\
	}

#define COMPARE_STR_UNSAFE(field, fmt, ...)		\
	if (strcmp(ps_a->field, ps_b->field) != 0) {	\
		seen_first = true;			\
		printf(fmt, __VA_ARGS__);		\
	}

	COMPARE(rss, "rss=[%lu%s]->[%lu%s] ", ps_a->rss,
		ps_a->rss_counted ? "*" : "",
		ps_b->rss, ps_b->rss_counted ? "*" : "");
	COMPARE(referenced, "ref=[%lu]->[%lu] ", ps_a->referenced, ps_b->referenced);
	COMPARE(anon, "anon=[%lu]->[%lu] ", ps_a->anon, ps_b->anon);
	COMPARE(anon_huge, "anon_huge=[%lu]->[%lu] ", ps_a->anon_huge, ps_b->anon_huge);
	COMPARE(swap, "swap=[%lu]->[%lu] ", ps_a->swap, ps_b->swap);
	COMPARE(locked, "locked=[%lu]->[%lu] ", ps_a->locked, ps_b->locked);

	if (seen_first) {
		printf("\n");
		seen_first = false;
	}

	COMPARE_STR(vm_flags, "vm_flags=[%s]->[%s] ", ps_a->vm_flags, ps_b->vm_flags);
	COMPARE_STR_UNSAFE(perms, "perms=[%s]->[%s] ", ps_a->perms, ps_b->perms);
	COMPARE(offset, "offset=[%lu]->[%lu] ", ps_a->offset, ps_b->offset);
	COMPARE_STR(name, "name=[%s]->[%s] ", ps_a->name, ps_b->name);

#undef COMPARE
#undef COMPARE_STR
#undef COMPARE_STR_UNSAFE

	if (seen_first) {
		printf("\n");
		seen_first = false;
	}

	for (i = 0; i < num_pages; i++, addr += getpagesize()) {
		if (ps_a->pagemaps[i] == ps_b->pagemaps[i] &&
		    ps_a->kpagecounts[i] == ps_b->kpagecounts[i] &&
		    ps_a->kpageflags[i] == ps_b->kpageflags[i])
			continue;

		if (!seen_first) {
			printf("\n");
			seen_first = true;
		}

		print_mapping(addr, ps_a, i, false);
		printf("               -> ");
		print_mapping(INVALID_VALUE, ps_b, i, false);
	}

	if (!seen_first)
		return false;

	printf("\n");
	print_header(ps_a);

	return true;
}

bool pagestat_print_diff_all(struct pagestat **pss_a, struct pagestat **pss_b)
{
	int i;
	bool seen = false;

	for (i = 0; i < MAX_MAPS; i++) {
		bool updated;
		struct pagestat *ps_a = pss_a[i];
		struct pagestat *ps_b = pss_b[i];

		updated = pagestat_print_diff(ps_a, ps_b);
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

static struct pagestat *get_pagestat_snapshot(FILE *fp, const char *pid, uint64_t vaddr)
{
	uint64_t from, to;
	struct pagestat *ret = NULL;
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
		pagestat_free(ret);
		ret = NULL;
		goto out;
	}

	// Now get the other fields.
	if (!get_smap_other_fields(ret, line, fp)) {
		pagestat_free(ret);
		ret = NULL;
		goto out;
	}

	// Finally, get page table fields.
	if (!get_pagetable_fields(pid, ret)) {
		pagestat_free(ret);
		ret = NULL;
		goto out;
	}

out:
	if (line != NULL)
		free(line);

	return ret;
}

struct pagestat *pagestat_snapshot(uint64_t vaddr)
{
	FILE *fp = open_smaps("self");
	struct pagestat *ret = get_pagestat_snapshot(fp, "self", vaddr);

	fclose(fp);

	return ret;
}

struct pagestat *pagestat_snapshot_remote(const char *pid, uint64_t vaddr)
{
	FILE *fp = open_smaps(pid);
	struct pagestat *ret = get_pagestat_snapshot(fp, pid, vaddr);

	fclose(fp);

	return ret;
}

struct pagestat **pagestat_snapshot_all(const char *pid)
{
	int i;
	FILE *fp = open_smaps(pid);
	struct pagestat **ret = calloc(MAX_MAPS, sizeof(struct pagestat*));

	for (i = 0; i < MAX_MAPS; i++) {
		// Get next snapshot.
		ret[i] = get_pagestat_snapshot(fp, pid, INVALID_VALUE);
		if (ret[i] == NULL)
			return ret;
	}

	fprintf(stderr, "ERROR: More than %d maps!", MAX_MAPS);
	pagestat_free_all(ret);

	return NULL;
}

void pagestat_free(struct pagestat* ps)
{
	if (ps == NULL)
		return;

	if (ps->name != NULL)
		free((void *)ps->name);

	if (ps->vm_flags != NULL)
		free((void *)ps->vm_flags);

	if (ps->pagemaps != NULL)
		free(ps->pagemaps);

	if (ps->kpagecounts != NULL)
		free(ps->kpagecounts);

	if (ps->kpageflags != NULL)
		free(ps->kpageflags);

	free(ps);
}

void pagestat_free_all(struct pagestat **pss)
{
	for (int i = 0; i < MAX_MAPS; i++) {
		struct pagestat *ps = pss[i];
		if (ps == NULL)
			return;

		pagestat_free(ps);
	}
}
