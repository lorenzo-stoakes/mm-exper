#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define TARGET_PTR ((void *)0x700000000000UL)

static void print_maps(char *ptr)
{
	const pid_t pid = getpid();
	char cmd[255];
	char ptr_str[255];

	// Terrible stuff.
	sprintf(ptr_str, "%p", ptr);
	ptr_str[strlen(ptr_str) - 4] = '\0';

	sprintf(cmd, "cat /proc/%lu/maps | grep %s", pid, &ptr_str[2]);
	system(cmd);
}

static void breakpoint(void *ptr)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
	// (at madvise_walk_vmas()).
	madvise(ptr, page_size, MADV_NORMAL);
}

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	char *ptr = mmap(TARGET_PTR, page_size * 4, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
	const pid_t pid = getpid();

	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	print_maps(ptr);
	puts("---");

	if (mprotect(ptr + page_size, page_size, PROT_READ | PROT_WRITE)) {
		perror("mprotect 1");
		return EXIT_FAILURE;
	}

	print_maps(ptr);
	puts("---");

	// Break before so we can insert breakpoint in mprotect/merge code...
	breakpoint(ptr + page_size);

	if (mprotect(ptr + page_size, page_size, PROT_READ)) {
		perror("mprotect 2");
		return EXIT_FAILURE;
	}

	print_maps(ptr);

	return EXIT_SUCCESS;
}
