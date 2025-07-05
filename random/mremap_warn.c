#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>

static void *sys_mremap(void *old_address, unsigned long old_size,
			unsigned long new_size, int flags, void *new_address)
{
	return (void *)syscall(__NR_mremap, (unsigned long)old_address,
			       old_size, new_size, flags,
			       (unsigned long)new_address);
}

static bool do_test(int map_flags)
{
	bool ret;
	char *ptr, *ptr2;

	ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, map_flags, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap()");
		return EXIT_FAILURE;
	}

	ptr2 = sys_mremap(ptr, 0, 4096, MREMAP_MAYMOVE, 0);

	munmap(ptr2 == MAP_FAILED ? ptr : ptr2, 4096);

	return ptr2 != MAP_FAILED;
}

int main(void)
{
	if (do_test(MAP_ANON | MAP_PRIVATE))
		fprintf(stderr, "Unexpected success of private 0-len mapping remap\n");

	if (!do_test(MAP_ANON | MAP_SHARED))
		fprintf(stderr, "Unexpected failed of shared 0-len mapping remap\n");

	return EXIT_SUCCESS;
}
