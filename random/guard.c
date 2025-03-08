#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define MADV_GUARD_INSTALL 102
#define MADV_GUARD_REMOVE 103

#define SIZE (1UL << 23)

int main(void)
{
	char *ptr;
	char cmd[4096];
	int i;
	pid_t pid = getpid();

	ptr = mmap(NULL, SIZE, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANON, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	for (i = 0; i < SIZE; i += 4096) {
		ptr[i] = 'x';
	}

	/* Guard region every 10th page. */
	for (i = 0; i < SIZE; i += 4096 * 10) {
		if (madvise(&ptr[i], 4096, MADV_GUARD_INSTALL)) {
			perror("MADV_GUARD_INSTALL");
			return EXIT_FAILURE;
		}
	}

	/* Collapse THP. */
	if (madvise(ptr, SIZE, MADV_COLLAPSE)) {
		perror("MADV_COLLAPSE");
		return EXIT_FAILURE;
	}

//	sprintf(cmd, "cat /proc/%d/smaps | grep %lx -A 100", pid, ptr);
//	system(cmd);

	return EXIT_SUCCESS;
}
