#include <assert.h>
#include <setjmp.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define MADV_POISON	102		/* poison a page range */
#define MADV_REMEDY	103		/* rhythmical remedy */

#define NUM_PAGES 10

volatile bool sigbus_jmp_set;
sigjmp_buf sigbus_jmp_buf;

static void handle_sigbus(int c)
{
	if (!sigbus_jmp_set)
		return;

	siglongjmp(sigbus_jmp_buf, c);
}

static void setup_signal_handler(void)
{
	struct sigaction act = {
		.sa_handler = &handle_sigbus,
		.sa_flags = SA_NODEFER,
	};

	sigemptyset(&act.sa_mask);
	sigaction(SIGBUS, &act, NULL);
}

/*
 * Returns true if access succeeded, and false if there was a SIGUBS.
 */
static bool try_touch_buf(char *ptr, size_t offset)
{
	sigbus_jmp_set = true;

	if (sigsetjmp(sigbus_jmp_buf, 0) == 0) {
		ptr[offset] = 'x';

		sigbus_jmp_set = false;
	} else {
		sigbus_jmp_set = false;
		return false;
	}

	return true;
}

int main(void)
{
	long page_size = sysconf(_SC_PAGESIZE);
	char *ptr;
	int i;

	setup_signal_handler();

	ptr = mmap(NULL, NUM_PAGES * page_size, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANON, -1, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	/* Trivially assert we can touch the first page. */
	assert(try_touch_buf(ptr, 0));

	/* This is now backed, so MADV_POISON will refuse to touch it. */
	assert(madvise(ptr, page_size, MADV_POISON) == -EPERM);

	/* So drop it. */
	assert(!madvise(ptr, page_size, MADV_DONTNEED));

	/* Establish a guard page at the start of the mapping. */
	assert(!madvise(ptr, page_size, MADV_POISON));

	/* Establish that 1st page SIGBUS's. */
	assert(!try_touch_buf(ptr, 0));

	/* Ensure we can touch everything else.*/
	for (i = 1; i < NUM_PAGES; i++) {
		assert(try_touch_buf(ptr + i * page_size, 0));
	}

	/* Drop non-guard mappings again.  */
	assert(!madvise(ptr + page_size, (NUM_PAGES - 1) * page_size,
			MADV_DONTNEED));


	/* Establish a guard page at the end of the mapping. */
	assert(!madvise(ptr + (NUM_PAGES - 1) * page_size, page_size,
			MADV_POISON));

	/* Check that both guard pages result in SIGBUS. */
	assert(!try_touch_buf(ptr, 0));
	assert(!try_touch_buf(ptr + (NUM_PAGES - 1) * page_size, 0));

	/* Remedy the first. */
	assert(!madvise(ptr, page_size, MADV_REMEDY));

	/* Make sure we can touch it. */
	assert(try_touch_buf(ptr, 0));

	/* Remedy the last. */
	assert(!madvise(ptr + (NUM_PAGES - 1) * page_size, page_size, MADV_REMEDY));

	/* Make sure we can touch it. */
	assert(try_touch_buf(ptr + (NUM_PAGES - 1) * page_size, 0));

	/* We can't poison out of range. */
	assert(madvise(ptr - page_size, page_size, MADV_POISON) == -EPERM);
	assert(madvise(ptr + NUM_PAGES * page_size, page_size, MADV_POISON) == -EPERM);

	/* Clear everything down. */
	assert(!madvise(ptr, NUM_PAGES * page_size, MADV_DONTNEED));

	/* Test setting a _range_ of pages, namely the first 3. */
	assert(!madvise(ptr, 3 * page_size, MADV_POISON));

	/* Make sure they are all poisoned. */
	for (i = 0; i < 3; i++) {
		assert(!try_touch_buf(ptr, i * page_size));
	}

	/* Make sure the rest are not. */
	for (i = 3; i < NUM_PAGES; i++) {
		assert(try_touch_buf(ptr, i * page_size));
	}

	/* Remedy them. */
	assert(!madvise(ptr, 3 * page_size, MADV_REMEDY));

	/* Now make sure we can touch everything. */
	for (i = 0; i < NUM_PAGES; i++) {
		assert(try_touch_buf(ptr, i * page_size));
	}

	puts("All tests passed.");

	return EXIT_SUCCESS;
}
