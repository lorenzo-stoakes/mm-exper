#define _GNU_SOURCE

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#define DO_FORCE

#ifdef DO_FORCE
#define MREMAP_RELOCATE_ANON 8
#else
#define MREMAP_RELOCATE_ANON 0
#endif

static void *sys_mremap(void *old_address, unsigned long old_size,
			unsigned long new_size, int flags, void *new_address)
{
	return (void *)syscall(__NR_mremap, (unsigned long)old_address,
			       old_size, new_size, flags,
			       (unsigned long)new_address);
}

static void do_move(bool touch_a_first, bool touch_a_afterwards, bool touch_b)
{
	const unsigned long page_size = (const unsigned long)sysconf(_SC_PAGESIZE);
	char *ptr, *ptr2;
	pid_t pid = getpid();
	char cmd[512];

	printf("do_move: %c%c%c\n",
	       touch_a_first ? 'Y' : 'N',
	       touch_a_afterwards ? 'Y' : 'N',
	       touch_b ? 'Y' : 'N');

	/* Hackily establish a PROT_NONE region we can operate in. */
	ptr = mmap(NULL, 100 * page_size, PROT_NONE, MAP_ANON | MAP_PRIVATE,
		   -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	printf("%lx-%lx\n", (unsigned long)ptr, (unsigned long)ptr + 10 * page_size);

	/*
	 * we munmap so we can carry on moving things around in this space where
	 * _most likely_ we cannot be touching any other mappings.
	 *
	 * The proper way of doing this would be to keep the PROT_NONE mapping
	 * around, however this complicates things kernel-side as we then have
	 * aggregate unmapping/mapping operations.
	 */
	munmap(ptr, 100 * page_size);

	/* Now map 10 pages. */
	ptr = mmap(ptr, 10 * page_size, PROT_READ | PROT_WRITE,
		   MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	if (touch_a_first)
		ptr[0] = 'x';

	/*
	 * At this point the mapping is:
	 *
	 * 01234567890123456789
	 * AAAAAAAAAA
	 */

	/* Now move half of this mapping... */
	ptr2 = sys_mremap(&ptr[5 * page_size], 5 * page_size, 5 * page_size,
			  MREMAP_MAYMOVE | MREMAP_FIXED | MREMAP_RELOCATE_ANON,
			  &ptr[15 * page_size]);
	if (ptr2 == MAP_FAILED) {
		perror("mremap");
		exit(EXIT_FAILURE);
	}

	if (touch_a_afterwards)
		ptr[0] = 'x';

	/*
	 * At this point the mapping is:
	 *
	 * 01234567890123456789
	 * AAAAA     BBBBB
	 */

	if (touch_b)
		ptr2[0] = 'x';

	/* Now move it back... */
	ptr2 = sys_mremap(&ptr[15 * page_size], 5 * page_size, 5 * page_size,
			  MREMAP_MAYMOVE | MREMAP_FIXED | MREMAP_RELOCATE_ANON,
			  &ptr[5 * page_size]);
	if (ptr2 == MAP_FAILED) {
		perror("mremap");
		exit(EXIT_FAILURE);
	}

	/*
	 * If anon_vma incompatibility causes merge fail, then we end up with:
	 *
	 * 01234567890123456789
	 * AAAAABBBBB
	 */

	sprintf(cmd, "cat /proc/%d/maps | grep -E \"%lx|%lx\"\n", pid,
		ptr, ptr2);
	system(cmd);

	munmap(ptr, page_size * 5);
	munmap(ptr2, page_size * 5);
}

static void do_mremap_mprotect(void)
{
	const unsigned long page_size = (const unsigned long)sysconf(_SC_PAGESIZE);
	char *ptr, *ptr2, *ptr3;
	pid_t pid = getpid();
	char cmd[512];

	printf("\ndo_mremap_mprotect\n");

	/* Hackily establish a PROT_NONE region we can operate in. */
	ptr = mmap(NULL, 100 * page_size, PROT_NONE, MAP_ANON | MAP_PRIVATE,
		   -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	printf("%lx-%lx\n", (unsigned long)ptr, (unsigned long)ptr + 10 * page_size);
	munmap(ptr, 100 * page_size);

	/* Now map 10 pages. */
	ptr = mmap(ptr, 10 * page_size, PROT_READ | PROT_WRITE,
		   MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	/* Map another 10 at a distance away. */
	ptr2 = mmap(&ptr[50 * page_size], 10 * page_size, PROT_READ | PROT_WRITE,
		    MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
	if (ptr2 == MAP_FAILED) {
		perror("mmap 2");
		exit(EXIT_FAILURE);
	}

	/* Fault in. */
	ptr2[0] = 'x';

	/* Now mprotect read-only. */
	if (mprotect(ptr2, 10 * page_size, PROT_READ)) {
		perror("mprotect");
		exit(EXIT_FAILURE);
	}

	/* Fault in half way through... */
	ptr[5 * page_size] = 'x';

	/*
	 * Now move half of the original mapping adjacent to the read-only
	 * one.
	 */
	ptr3 = sys_mremap(&ptr[5 * page_size], 5 * page_size, 5 * page_size,
			  MREMAP_MAYMOVE | MREMAP_FIXED | MREMAP_RELOCATE_ANON,
			  &ptr[60 * page_size]);
	if (ptr3 == MAP_FAILED) {
		perror("mremap");
		exit(EXIT_FAILURE);
	}

	/* Now mprotect the read-only mapping read-write. */
	if (mprotect(ptr2, 10 * page_size, PROT_READ | PROT_WRITE)) {
		perror("mprotect 2");
		exit(EXIT_FAILURE);
	}

	sprintf(cmd, "cat /proc/%d/maps | grep -E \"%lx|%lx\"\n", pid,
		ptr2, ptr3);
	system(cmd);

	munmap(ptr, page_size * 5);
	munmap(ptr2, page_size * 10);
	munmap(ptr3, page_size * 10);
}

int main(void)
{
	int i;

	/*
	 * Boolean permutations:
	 *
	 * 000
	 * 001
	 * 010
	 * 011
	 * 100
	 * 101
	 * 110
	 * 111
	 */
	for (i = 0; i < 8; i++) {
		do_move(i & 1, (i >> 1) & 1, (i >> 2) & 1);
	}

	do_mremap_mprotect();

	return EXIT_SUCCESS;
}
