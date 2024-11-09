#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#define MLOCK_ONFAULT	0x01

int mlock2(void *start, size_t len, int flags)
{
	return syscall(__NR_mlock2, start, len, flags);
}

static void do_mlock_page(char *ptr)
{
	if (mlock(ptr, 4096)) {
		perror("mlock");
		abort();
	}
}

int main(void)
{
	char *ptr = mmap(NULL, 3 * 4096, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANON, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	do_mlock_page(ptr);
	do_mlock_page(ptr + 4096);

	return EXIT_SUCCESS;
}
