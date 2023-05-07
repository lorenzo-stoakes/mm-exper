#define _GNU_SOURCE
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static bool gup_page(void *ptr)
{
	unsigned long addr = (unsigned long)ptr;

	FILE *file = fopen("/dev/gup_uaddr", "w");
	if (!file) {
		perror("fopen /dev/gup_uaddr");
		return false;
	}

	int val = fprintf(file, "%lu\n", addr);

	if (val < 0) {
		perror("gup_page fprintf");

		fclose(file);
		return false;
	} else if (val == 0) {
		fprintf(stderr, "Couldn't write %lu to /dev/gup_uaddr\n", addr);

		fclose(file);
		return false;
	}

	fclose(file);
	return true;
}

static bool gup_write_event(int event)
{
	FILE *file = fopen("/dev/gup_event", "w");
	if (!file) {
		perror("fopen /dev/gup_event");
		return false;
	}

	int val = fprintf(file, "%d\n", event);

	if (val < 0) {
		perror("gup_write_event fprintf");

		fclose(file);
		return false;
	} else if (val == 0) {
		fprintf(stderr, "Couldn't write %d to /dev/gup_event\n", event);

		fclose(file);
		return false;
	}

	fclose(file);
	return true;
}

static bool gup_unpin(void)
{
	return gup_write_event(0);
}

static bool gup_write_dirty(void)
{
	return gup_write_event(1);
}

static bool do_sync(void *ptr)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	if (msync(ptr, page_size, MS_SYNC)) {
		perror("msync");
		return false;
	}

	return true;
}

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	int fd = open("test.txt", O_RDWR);
	if (fd == -1) {
		perror("open test.txt");
		return EXIT_FAILURE;
	}

	printf("mapping...\n");

	char *ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			 MAP_SHARED | MAP_POPULATE, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	printf("sync...\n");

	// Clear any dirty flags.
	if (!do_sync(ptr))
		return EXIT_FAILURE;

	printf("gup pin %lu...\n", (unsigned long)ptr);

	// Now we're clean, gup it.
	if (!gup_page(ptr))
		return EXIT_FAILURE;

	// Now let's write to the page and sync it so it ends up clean.
	printf("write...\n");
	ptr[0] = 'x';
	printf("clean...\n");
	if (!do_sync(ptr)) {
		gup_unpin();
		return EXIT_FAILURE;
	}

	printf("gup write/dirty...");
	if (!gup_write_dirty()) {
		gup_unpin();
		return EXIT_FAILURE;
	}

	return gup_unpin() ? EXIT_SUCCESS : EXIT_FAILURE;
}
