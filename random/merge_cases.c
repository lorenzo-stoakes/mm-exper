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

#define NUM_PAGES (10)

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

	fprintf(file, "mmap: <userland> %d: %s\n", getpid(), msg);

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

static bool do_map(size_t pg_offset, size_t pgs, bool read, bool write)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	void *ptr = mmap(base + pg_offset * page_size,
			 pgs * page_size,
			 (read ? PROT_READ : 0) | (write ? PROT_WRITE : 0),
			 MAP_FIXED | MAP_ANON | MAP_PRIVATE, -1, 0);

	return ptr != MAP_FAILED;
}

static void map(size_t pg_offset, size_t pgs)
{
	if (!do_map(pg_offset, pgs, true, true))
		fatal("mmap() in map()");
}

static void map_ro(size_t pg_offset, size_t pgs)
{
	if (!do_map(pg_offset, pgs, true, false))
		fatal("mmap() in map_ro()");
}

static void map_none(size_t pg_offset, size_t pgs)
{
	if (!do_map(pg_offset, pgs, false, false))
		fatal("mmap() in map_none()");
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
		fatal("munmap() in reunmap()");
}

static bool do_mprotect(size_t pg_offset, size_t pgs, bool read, bool write)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	return !mprotect(base + pg_offset * page_size, pgs * page_size,
			 (read ? PROT_READ : 0) | (write ? PROT_WRITE : 0));
}

static void mprotect_none(size_t pg_offset, size_t pgs)
{
	if (!do_mprotect(pg_offset, pgs, false, false))
		fatal("mprotect in mprotect_none()");
}

static void mprotect_ro(size_t pg_offset, size_t pgs)
{
	if (!do_mprotect(pg_offset, pgs, true, false))
		fatal("mprotect in mprotect_ro()");
}

static void mprotect_rw(size_t pg_offset, size_t pgs)
{
	if (!do_mprotect(pg_offset, pgs, true, true))
		fatal("mprotect in mprotect_rw()");
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
	// x..
	map(0, 1);
	// x.x
	map(2, 1);

	// xxx
	kmsg("mremap [expand]: case 1");
	remap_expand(0, 1, 2);

	// ...
	unmap(0, 3);
	// x..
	map(0, 1);
	// xx.
	kmsg("mremap [expand]: case 2");
	remap_expand(0, 1, 2);

	// Since it is only ever merging the old VMA with the newly expanded
	// one, I don't think case 3 can be hit.
}

static void mprotect_cases(void)
{
	// ww.
	map(0, 2);
	// wwr
	map_ro(2, 1);
	// wrr
	kmsg("mprotect: case 4");
	mprotect_ro(1, 1);
	// wwr
	kmsg("mprotect: case 5");
	mprotect_rw(1, 1);

	// ...
	unmap(0, 3);
	// w..
	map(0, 1);
	// wr.
	map_ro(1, 1);
	// wrw
	map(2, 1);
	// www
	kmsg("mprotect: case 6");
	mprotect_rw(1, 1);

	// wnw
	mprotect_none(1, 1);
	// wn.
	unmap(2, 1);
	// ww.
	kmsg("mprotect: case 7");
	mprotect_rw(1, 1);

	// rw.
	mprotect_ro(0, 1);
	// ww.
	kmsg("mprotect: case 8");
	mprotect_rw(0, 1);
}

static void init(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	base = reserve(NUM_PAGES * page_size);
	rebase = reserve(NUM_PAGES * page_size);
	char buf[255];

	sprintf(buf, "  base: %lx - %lx", base, base + NUM_PAGES * page_size);
	kmsg(buf);

	sprintf(buf, "rebase: %lx - %lx", rebase, rebase + NUM_PAGES * page_size);
	kmsg(buf);
}

int main(void)
{
	init();

	mmap_cases();
	mremap_cases();
	mremap_expand_cases();
	mprotect_cases();

	return EXIT_SUCCESS;
}
