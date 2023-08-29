#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define TARGET_PTR ((void *)0x700000000000UL)

static void print_status()
{
	char cmd[255] = {};
	pid_t pid = getpid();

	sprintf(cmd, "cat /proc/%lu/status | grep -E 'VmHWM|VmRSS'", pid);
	system(cmd);
}

static void print_rollup()
{
	char cmd[255] = {};
	pid_t pid = getpid();

	sprintf(cmd, "cat /proc/%lu/smaps_rollup | grep 'Rss:' | sed 's/Rss/smaps_rollup.Rss/'", pid);
	system(cmd);
}

static void print_smaps(void)
{
	char cmd[255] = {};
	pid_t pid = getpid();

	sprintf(cmd, "cat /proc/%lu/maps | grep -E '^70'", pid);
	system(cmd);
}

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	puts("-- before anything --");

	print_status();
	print_rollup();

	char *ptr = mmap(TARGET_PTR, page_size, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE | MAP_FIXED | MAP_POPULATE,
			 -1, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	ptr[0] = 'x';

	puts("-- after mmap --");

	sleep(1);

	print_status();
	print_rollup();

	if (munmap(ptr, page_size)) {
		perror("munmap");
		return EXIT_FAILURE;
	}

	puts("-- after munmap --");

	print_status();
	print_rollup();

	return EXIT_SUCCESS;
}
