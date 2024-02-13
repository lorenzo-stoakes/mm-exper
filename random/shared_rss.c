#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define SIZE (512 * 1024 * 1024)

static void print_rollup()
{
	char cmd[255] = {};
	pid_t pid = getpid();

	sprintf(cmd, "cat /proc/%lu/smaps_rollup | grep 'Rss:' | sed 's/Rss/smaps_rollup.Rss/'", pid);
	system(cmd);
}

int main(void)
{
	printf("before:\n");
	print_rollup();

	int fd = memfd_create("loz1", 0);
	if (fd < 0) {
		perror("memfd_create");
		return EXIT_FAILURE;
	}

	if (ftruncate(fd, SIZE)) {
		perror("ftruncate");
		return EXIT_FAILURE;
	}

	printf("after memfd_create() / ftruncate():\n");
	print_rollup();

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		return EXIT_FAILURE;
	}

	char *buf = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
			 fd, 0);
	if (buf == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	if (pid == 0) {
		// Child.

		// Fill.
		memset(buf, 'x', SIZE);

		printf("CHILD: after fill:\n");
		print_rollup();
		return EXIT_SUCCESS;
	}

	// Parent.

	// Wait for the child to fill the RSS.
	wait(NULL);

	printf("PARENT: after child exits:\n");
	print_rollup();

	const long page_size = sysconf(_SC_PAGESIZE);

	for (size_t i = 0; i < SIZE / page_size; i++) {
		volatile char chr = buf[i * page_size];
		(void)chr;
	}

	printf("PARENT: after touch:\n");
	print_rollup();

	return EXIT_SUCCESS;
}
