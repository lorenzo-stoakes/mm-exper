#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define USE_VMAINFO

static void examine_vma(void *ptr)
{
#ifdef USE_VMAINFO
	unsigned long addr = (unsigned long)ptr;
	FILE *fp = fopen("/dev/vmainfo", "r+");

	if (fp == NULL) {
		fprintf(stderr, "Cannot open /dev/vmainfo");
		exit(1);
	}

	if (fprintf(fp, "%lu\n", addr) < 0) {
		perror("writing to /dev/vmainfo");
		exit(1);
	}

	fflush(fp);
	fclose(fp);

	fp = fopen("/dev/vmainfo", "r");

	printf("%lx: ", addr);

	for (char chr = fgetc(fp); chr != EOF; chr = fgetc(fp)) {
		printf("%c", chr);
	}
	fclose(fp);

	printf("\n");

#else
	const long page_size = sysconf(_SC_PAGESIZE);

	// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
	// (at madvise_walk_vmas()).
	madvise(ptr, page_size, MADV_NORMAL);
#endif
}


int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	void *ptr;

	// We intentionally leak mappings all over the shop.

	puts("MAP_ANON | MAP_PRIVATE, PROT_NONE");

	ptr = mmap(NULL, page_size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 1");
		return EXIT_FAILURE;
	}

	examine_vma(ptr);

	puts("MAP_ANON | MAP_PRIVATE, PROT_READ");

	ptr = mmap(NULL, page_size, PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 1");
		return EXIT_FAILURE;
	}

	examine_vma(ptr);

	return EXIT_SUCCESS;
}
