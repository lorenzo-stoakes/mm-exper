#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

int main(void)
{
	char *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_SHARED, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 1");
		return EXIT_FAILURE;
	}

	char *ptr2 = mmap(ptr, 4096, PROT_READ | PROT_WRITE,
			  MAP_ANON | MAP_SHARED | MAP_FIXED, -1, 0);
	if (ptr2 == MAP_FAILED) {
		perror("mmap 2");
		return EXIT_FAILURE;
	}

	char *tmp = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANON, -1, 0);
	if (tmp == MAP_FAILED) {
		perror("mmap tmp");
		return EXIT_FAILURE;
	}

	// We'll now replace tmp with ptr2.

	ptr2 = mremap(ptr2, 4096, 4096,
		      MREMAP_FIXED | MREMAP_MAYMOVE | MREMAP_DONTUNMAP,
		      tmp);
	if (ptr2 == MAP_FAILED) {
		perror("mremap 2");
		return EXIT_FAILURE;
	}

	strcpy(ptr, "foo");

	printf("ptr =[%p] [%s]\nptr2=[%p] [%s]\n", ptr, ptr,
	       ptr2, ptr2);

	return EXIT_SUCCESS;
}
