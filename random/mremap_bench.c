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

struct relocate_struct {
	/* Input. */
	int additional_flags;
	bool nohuge;
	unsigned long old_len;
	unsigned long new_len;

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

static char *map_and_populate_region(unsigned long len, bool nohuge)
{
	char *ptr = mmap(NULL, len, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE, -1, 0);

	if (ptr == MAP_FAILED)
		return NULL;

	if (nohuge && madvise(ptr, len, MADV_NOHUGEPAGE)) {
		munmap(ptr, len);
		return NULL;
	}

	memset(ptr, 'x', len);
	return ptr;
}

static void *reserve_region(unsigned long len)
{
	void *ptr = mmap(NULL, len, PROT_NONE,
			 MAP_ANON | MAP_PRIVATE, -1, 0);

	if (ptr == MAP_FAILED)
		return NULL;

	if (munmap(ptr, len))
		return NULL;

	return ptr;
}

static bool time_relocate(struct relocate_struct *reloc)
{
	char *src;
	void *dst;
	struct timespec t_start = {0, 0}, t_end = {0, 0};
	long long  start_ns, end_ns;

	src = map_and_populate_region(reloc->old_len, reloc->nohuge);
	if (!src)
		return false;

	dst = reserve_region(reloc->new_len);
	if (!dst) {
		munmap(src, reloc->old_len);
		return false;
	}

	clock_gettime(CLOCK_MONOTONIC, &t_start);
	if (sys_mremap(src, reloc->old_len, reloc->new_len,
		       MREMAP_FIXED | MREMAP_MAYMOVE | reloc->additional_flags,
		       dst) != dst) {
		munmap(src, reloc->old_len);
		return false;
	}
	clock_gettime(CLOCK_MONOTONIC, &t_end);

	start_ns = t_start.tv_sec * NS_PER_SEC + t_start.tv_nsec;
	end_ns = t_end.tv_sec * NS_PER_SEC + t_end.tv_nsec;

	reloc->time_ns = end_ns - start_ns;
	munmap(dst, reloc->new_len);

	return true;
}

static unsigned long remap(unsigned long len, bool relocate)
{
	struct relocate_struct reloc = {
		.additional_flags = relocate ? MREMAP_MUST_RELOCATE_ANON : 0,
		.nohuge = true,
		.old_len = len,
		.new_len = len,
	};

	if (!time_relocate(&reloc)) {
		fprintf(stderr, "FAILED :(\n");
		exit(EXIT_FAILURE);
	}

	return reloc.time_ns;
}

static unsigned long remap_normal(unsigned long len)
{
	return remap(len, /* relocate= */false);
}

static unsigned long remap_relocate_anon(unsigned long len)
{
	return remap(len, /* relocate= */true);
}

#define COUNT 10

int main(void)
{
	unsigned long count;

	for (count = 1; (count * PG) < MB; count++) {
		unsigned long normal = remap_normal(count * PG);
		unsigned long remap = remap_relocate_anon(count * PG);

		printf("%luKB Took %lu ns vs. %lu ns (%lux slower).\n", count,
		       normal, remap, remap / normal);
	}

	for (count = 1; count < 1000; count += 10) {
		unsigned long normal = remap_normal(count * MB);
		unsigned long remap = remap_relocate_anon(count * MB);

		printf("%luMB Took %lu ns vs. %lu ns (%lux slower).\n", count,
		       normal, remap, remap / normal);
	}

	for (count = 1; count < 2; count++) {
		unsigned long normal = remap_normal(count * GB);
		unsigned long remap = remap_relocate_anon(count * GB);

		printf("%luGB Took %lu ns vs. %lu ns (%lux slower).\n", count,
		       normal, remap, remap / normal);
	}

	return EXIT_SUCCESS;
}
