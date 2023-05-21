#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#define TARGET_PTR ((void *)0x700000000000UL)

static void print_smaps(void)
{
	char cmd[255] = {};
	pid_t pid = getpid();

	sprintf(cmd, "cat /proc/%lu/maps | grep -E '^70'", pid);
	system(cmd);

	sprintf(cmd, "cat /proc/%lu/maps | grep -E '^6f'", pid);
	system(cmd);
}

int main(void)
{
	long page_size = sysconf(_SC_PAGESIZE);
	char *ptr = mmap(TARGET_PTR, page_size * 10, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_GROWSDOWN | MAP_PRIVATE | MAP_FIXED |
			 MAP_POPULATE, -1, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	print_smaps();

	struct rlimit lim;

	if (getrlimit(RLIMIT_STACK, &lim)) {
		perror("getrlimit");
		return EXIT_FAILURE;
	}

	if (lim.rlim_cur == -1) {
		// Assuming overcommit mode OVERCOMMIT_GUESS, we can map up to
		// the amount of physical memory + swap. Grab amount of physical
		// memory and expand stack to that.
		long pages = sysconf(_SC_PHYS_PAGES);
		// Offset by the 10 pages in the stack.
		unsigned long addr = (unsigned long)TARGET_PTR - pages * page_size + 10;

		*(char *)addr = 'x';
	} else {
		// The limit is for the _total size_ of the stack. So we offset
		// by the size of the allocation, i.e. 10 pages.
		ptr[-lim.rlim_cur + page_size * 10] = 'x';
	}


	printf("---\n");
	print_smaps();

	return EXIT_SUCCESS;
}
