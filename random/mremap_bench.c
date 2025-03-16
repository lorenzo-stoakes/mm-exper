#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <time.h>

#define MREMAP_RELOCATE_ANON 8
#define MREMAP_MUST_RELOCATE_ANON 16

#define PG (1UL << 12)
#define MB (1UL << 20)
#define GB (1UL << 30)

#define NS_PER_SEC 1000000000ULL

#define MAX(x, y) ((x) > (y) ? (x) : (y))

struct relocate_struct {
	/* Input. */
	int additional_flags;
	bool nohuge;
	unsigned long old_len;
	unsigned long new_len;
	unsigned long pop_len;
	unsigned long align;

	/* Output */
	unsigned long time_ns;
};

static void *sys_mremap(void *old_address, unsigned long old_size,
			unsigned long new_size, int flags, void *new_address)
{
	return (void *)syscall(__NR_mremap, (unsigned long)old_address,
			       old_size, new_size, flags,
			       (unsigned long)new_address);
}

static char *mmap_aligned(unsigned long len, unsigned long align, int prot)
{
	/* Provide space for alignment. */
	unsigned long new_len = len + 2 * align;
	unsigned long orig_addr, new_addr;
	char *ptr;

	ptr = mmap(NULL, new_len, prot, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap_aligned() mmap");
		return NULL;
	}

	orig_addr = (unsigned long)ptr;

	if (!align)
		goto out;

	munmap(ptr, new_len);

	new_addr = (orig_addr + align) & ~(align - 1);
	if (new_addr > orig_addr) {
		ptr = mmap((void *)new_addr, len, prot,
			   MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
		if (ptr == MAP_FAILED) {
			perror("mmap_aligned() mmap 2");
			return NULL;
		}
	}

out:
	return ptr;
}

static char *map_and_populate_region(struct relocate_struct *reloc)
{
	unsigned long len = reloc->old_len;
	char *ptr;

	ptr = mmap_aligned(len, reloc->align, PROT_READ | PROT_WRITE);
	if (!ptr)
		return NULL;

	if (reloc->nohuge && madvise(ptr, len, MADV_NOHUGEPAGE)) {
		perror("map_and_populate_region() madvise");

		munmap(ptr, len);
		return NULL;
	}

	memset(ptr, 'x', reloc->pop_len);
	return ptr;
}

static void *reserve_region(struct relocate_struct *reloc)
{
	unsigned long len = reloc->new_len;
	char *ptr;

	ptr = mmap_aligned(len, reloc->align, PROT_NONE);
	if (!ptr)
		return NULL;

	if (munmap(ptr, len)) {
		perror("reserve_region() munmap");
		return NULL;
	}

	return ptr;
}

static bool check_move(struct relocate_struct *reloc, char *dst)
{
	unsigned long i;

	for (i = 0; i < reloc->pop_len; i++) {
		if (dst[i] != 'x') {
			fprintf(stderr, "check_move fail at %lu\n", i);
			return false;
		}
	}

	return true;
}

static bool time_relocate(struct relocate_struct *reloc)
{
	char *src;
	void *dst;
	struct timespec t_start = {0, 0}, t_end = {0, 0};
	unsigned long start_ns, end_ns;

	src = map_and_populate_region(reloc);
	if (!src)
		return false;

	dst = reserve_region(reloc);
	if (!dst) {
		munmap(src, reloc->old_len);
		return false;
	}

	clock_gettime(CLOCK_MONOTONIC, &t_start);
	if (sys_mremap(src, reloc->old_len, reloc->new_len,
		       MREMAP_FIXED | MREMAP_MAYMOVE | reloc->additional_flags,
		       dst) != dst) {
		perror("time_relocate() mremap");
		munmap(src, reloc->old_len);
		return false;
	}
	clock_gettime(CLOCK_MONOTONIC, &t_end);

	if (!check_move(reloc, dst))
		return false;

	start_ns = t_start.tv_sec * NS_PER_SEC + t_start.tv_nsec;
	end_ns = t_end.tv_sec * NS_PER_SEC + t_end.tv_nsec;

	reloc->time_ns = end_ns - start_ns;

	/* Force reclaim to assert rmap state correct. */
	if (madvise(dst, reloc->new_len, MADV_PAGEOUT)) {
		perror("reserve_region() madvise");
		return false;
	}

	munmap(dst, reloc->new_len);

	return true;
}

static unsigned long remap(unsigned long len, bool relocate,
			   unsigned long pop_len, bool nohuge,
			   unsigned long align)
{
	struct relocate_struct reloc = {
		.additional_flags = relocate ? MREMAP_MUST_RELOCATE_ANON : 0,
		.nohuge = nohuge,
		.align = align,
		.old_len = len,
		.new_len = len,
		.pop_len = pop_len,
	};

	if (!time_relocate(&reloc)) {
		fprintf(stderr, "FAILED :(\n");
		exit(EXIT_FAILURE);
	}

	return reloc.time_ns;
}

static unsigned long remap_normal(unsigned long len, unsigned long pop_len,
		bool nohuge, bool align)
{
	unsigned long pmd_size = 1UL << 21;

	return remap(len, /* relocate= */false, pop_len, nohuge,
		     align && len >= pmd_size ? pmd_size : 0);
}

static unsigned long remap_relocate(unsigned long len, unsigned long pop_len,
	bool nohuge, bool align)
{
	unsigned long pmd_size = 1UL << 21;

	return remap(len, /* relocate= */true, pop_len, nohuge,
		     align && len >= pmd_size ? pmd_size : 0);
}

#define MB_INTERVAL 100

static void time_move(double pop, bool nohuge, bool align)
{
	unsigned long count, normal, remap;
	unsigned long max_remap = 0, max_normal = 0;
	double av_normal, av_remap;

	normal = 0;
	remap = 0;
	for (count = 1; (count * PG) < 2 * MB; count++) {
		unsigned long len = count * PG;
		unsigned long pop_len = (unsigned long)(pop * (double)len);
		unsigned long normal_ns, remap_ns;

		normal_ns = remap_normal(len, pop_len, nohuge, align);
		remap_ns = remap_relocate(len, pop_len, nohuge, align);
		max_remap = MAX(max_remap, remap_ns);
		max_normal = MAX(max_normal, normal_ns);

		normal += normal_ns;
		remap += remap_ns;
	}

	for (count = 1; count <= 1000; count += MB_INTERVAL) {
		unsigned long len = count * MB;
		unsigned long pop_len = (unsigned long)(pop * (double)len);
		unsigned long normal_ns, remap_ns;

		normal_ns = remap_normal(len, pop_len, nohuge, align);
		remap_ns = remap_relocate(len, pop_len, nohuge, align);
		max_remap = MAX(max_remap, remap_ns);
		max_normal = MAX(max_normal, normal_ns);

		normal += normal_ns;
		remap += remap_ns;
	}

	av_normal = (double)normal / (double)count;
	av_remap = (double)remap / (double)count;

	printf("[4KB, 1GB] [%.1f populated] Took %.2fns vs. %.2fns (%.2fx [%.2fus] slower, max normal=%.2fms,remap=%.2fms)%s%s\n",
	       pop, av_normal, av_remap, av_remap / av_normal, (av_remap - av_normal) / 1000.,
	       (double)max_normal / 1000000., (double)max_remap / 1000000.,
	       nohuge ? " [nohuge]" : "", align ? " [align]" : "");
}

int main(void)
{
	int i = 0;

	for (i = 1; i <= 10; i++) {
		double pop = (double)i/10.f;

		time_move(pop, /* nohuge= */false, /* align= */false);
	}

	for (i = 1; i <= 10; i++) {
		double pop = (double)i/10.f;

		time_move(pop, /* nohuge= */false, /* align= */true);
	}

	for (i = 1; i <= 10; i++) {
		double pop = (double)i/10.f;

		time_move(pop, /* nohuge= */true, /* align= */false);
	}

	for (i = 1; i <= 10; i++) {
		double pop = (double)i/10.f;

		time_move(pop, /* nohuge= */true, /* align= */true);
	}


	return EXIT_SUCCESS;
}
