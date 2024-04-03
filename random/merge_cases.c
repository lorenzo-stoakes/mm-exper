#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// First valid address in reserved memory range.
static void *base;
// First valid address in SECOND reserved memory range,
static void *rebase;

static void fatal(const char *msg)
{
	const int err = errno;

	if (err > 0)
		fprintf(stderr, "FATAL: %s [%s]\n", msg,
			strerror(err));
	else
		fprintf(stderr, "FATAL: %s\n", msg);

	exit(EXIT_FAILURE);
}

static void kmsg(const char *msg)
{
	FILE *file = fopen("/dev/kmsg", "w");
	if (file == NULL)
		fatal("open");

	fprintf(file, "<userland> %d: %s\n", getpid(), msg);

	fclose(file);
}

static void *reserve(size_t size)
{
	void *ret = mmap(NULL, size, PROT_NONE,
			 MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ret == MAP_FAILED)
		fatal("mmap() in reserve()");

	return ret;
}

static void map(size_t pg_offset, size_t pgs)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	void *ptr = mmap(base + pg_offset * page_size,
			 pgs * page_size, PROT_READ | PROT_WRITE,
			 MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0);

	if (ptr == MAP_FAILED)
		fatal("mmap() in map()");
}

static void remap(size_t pg_offset, size_t pgs, size_t remap_pg_offset)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	void *ptr = mremap(base + pg_offset * page_size, pgs * page_size, pgs * page_size,
			   MREMAP_MAYMOVE | MREMAP_FIXED | MREMAP_DONTUNMAP,
			   rebase + remap_pg_offset * page_size);

	if (ptr == MAP_FAILED)
		fatal("mremap() in remap()");
}

// Remaps within the base area.
static void remap_expand(size_t pg_offset, size_t pgs, size_t new_pgs)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	void *ptr = mremap(base + pg_offset * page_size, pgs * page_size,
			   new_pgs * page_size, 0);

	if (ptr == MAP_FAILED)
		fatal("mremap() in remap_expand()");
}

static bool do_unmap(void *ptr, size_t pg_offset, size_t pgs)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	return !munmap(ptr + pg_offset * page_size,
		       pgs * page_size);
}

static void unmap(size_t pg_offset, size_t pgs)
{
	if (!do_unmap(base, pg_offset, pgs))
		fatal("munmap() in unmap()");
}

static void reunmap(size_t remap_pg_offset, size_t pgs)
{
	if (!do_unmap(rebase, remap_pg_offset, pgs))
		fatal("munmap() in unmap2()");
}

static void mmap_cases(void)
{
	// x..
	map(0, 1);
	// x.x
	map(2, 1);
	// xxx # case 1
	kmsg("mmap: case 1");
	map(1, 1);

	// x.x
	unmap(1, 1);
	// x..
	unmap(2, 1);
	// xx.
	kmsg("mmap: case 2");
	map(1, 1);

	// x..
	unmap(1, 1);
	// ...
	unmap(0, 1);
	// ..x
	map(2, 1);
	// .xx
	kmsg("mmap: case 3");
	map(1, 1);

	// ..x
	unmap(1, 1);
	// ...
	unmap(2, 1);
}

static void mremap_cases(void)
{
	// x....
	map(0, 1);
	// x.x..
	map(2, 1);
	// x.x.x
	map(4, 1);

	// x.x.x / y..
	remap(0, 1, 0);
	// x.x.x / y.y
	remap(2, 1, 2);
	// x.x.x / yyy
	kmsg("mremap: case 1");
	remap(4, 1, 1);
	// x.x.x / y.y
	reunmap(1, 1);
	// x.x.x / y..
	reunmap(2, 1);
	// x.x.x / yy.
	kmsg("mremap: case 2");
	remap(2, 1, 1);
	// x.x.x / .y.
	reunmap(0, 1);
	// x.x.x / ...
	reunmap(1, 1);
	// x.x.x / ..y
	remap(2, 1, 2);
	// x.x.x / .yy
	kmsg("mremap: case 3");
	remap(4, 1, 1);

	// x.x.x / ..y
	reunmap(1, 1);
	// x.x.x / ...
	reunmap(2, 1);
	// ..x.x / ...
	unmap(0, 1);
	// ....x / ...
	unmap(2, 1);
	// ..... / ...
	unmap(4, 1);
}

static void mremap_expand_cases(void)
{
	// x....
	map(0, 1);
	// x.x..
	map(2, 1);

	// xxx..
	kmsg("mremap [expand]: case 1");
	remap_expand(0, 1, 2);

	// .....
	unmap(0, 3);
	// x....
	map(0, 1);
	// xx...
	kmsg("mremap [expand]: case 2");
	remap_expand(0, 1, 2);

	// Since it is only ever merging the old VMA with the newly expanded
	// one, I don't think case 3 can be hit.
}

// TODO: vma_modify() cases.

static void init(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	base = reserve(10 * page_size);
	rebase = reserve(10 * page_size);
}

int main(void)
{
	init();

	mmap_cases();
	mremap_cases();
	mremap_expand_cases();

	return EXIT_SUCCESS;
}
