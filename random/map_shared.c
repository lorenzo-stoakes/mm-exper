#include "shared.h"
#include "../read-pageflags/read-pageflags.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static void analyse(char *ptr)
{
	printf("ptr=%p\n", ptr);

	lookup_vma_flags(ptr);
	print_flags_virt(ptr, "anon shared before fault");

	// Read fault in...
	volatile char chr = ((volatile char *)ptr)[0];
	(void)chr;

	puts("after read fault");

	lookup_vma_flags(ptr);
	print_flags_virt(ptr, "anon shared read");

	// Write fault in...
	ptr[0] = 'x';

	puts("after write fault");

	lookup_vma_flags(ptr);
	print_flags_virt(ptr, "anon shared write");
}

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	char *ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_SHARED, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	analyse(ptr);

	char *ptr2 = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_SHARED, -1, 0);
	if (ptr2 == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	puts("--- again ---");
	analyse(ptr2);

	getchar();

	return EXIT_SUCCESS;
}
