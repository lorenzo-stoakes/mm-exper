#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define MADV_GUARD_INSTALL 102
#define MADV_GUARD_REMOVE 103

int main(void)
{
	char *ptr;
	pid_t pid = getpid();
	char cmd[4096];

	ptr = mmap(NULL, 10 * 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	if (madvise(ptr, 5 * 4096, MADV_GUARD_INSTALL)) {
		perror("madvise");
		return EXIT_FAILURE;
	}

	if (mlock(ptr, 4096)) {
		perror("mlock");
//		return EXIT_FAILURE;
	}

	sprintf(cmd, "cat /proc/%d/smaps | grep %lx -A 100", pid, ptr);
	system(cmd);

	return EXIT_SUCCESS;
}
