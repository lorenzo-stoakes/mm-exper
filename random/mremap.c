#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define TARGET_PTR ((void *)0x700000000000UL)
#define TARGET_PTR2 ((void *)0x7f0000000000UL)
#define TARGET_PTR3 ((void *)0x6f0000000000UL)

static void check_vma(void *ptr, unsigned long size)
{
	// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
	// (at madvise_walk_vmas()).
	madvise(ptr, size, MADV_NORMAL);
}

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	char *ptr = mmap(TARGET_PTR, page_size, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE | MAP_FIXED | MAP_POPULATE,
			 -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 1");
		return EXIT_FAILURE;
	}

	printf("orig mmap=%p\n", ptr);

	ptr[0] = 'x';

	check_vma(ptr, page_size);

	char *ptr2 = mremap(ptr, page_size, page_size,
			    MREMAP_MAYMOVE | MREMAP_DONTUNMAP);
	if (ptr2 == MAP_FAILED) {
		perror("mremap 2");
		return EXIT_FAILURE;
	}

	ptr2[0] = 'y';

	printf("after mremap2=%p\n", ptr2);
	check_vma(ptr2, page_size);

	char *ptr3 = mremap(ptr2, page_size, page_size,
			    MREMAP_MAYMOVE | MREMAP_DONTUNMAP);
	if (ptr3 == MAP_FAILED) {
		perror("mremap 3");
		return EXIT_FAILURE;
	}

	ptr3[0] = 'z';

	printf("after mremap3=%p\n", ptr3);
	check_vma(ptr3, page_size);

	printf("%c %c %c\n", ptr[0], ptr2[0], ptr3[0]);

	return EXIT_SUCCESS;
}
