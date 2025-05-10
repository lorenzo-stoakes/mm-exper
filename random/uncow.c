#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
	pid_t parent_pid = getpid();
	char *ptr;
	pid_t pid;
	char cmd[4096];

	ptr = mmap(NULL, 12 * 4096, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);


	ptr = mmap(&ptr[4096], 10 * 4096, PROT_READ | PROT_WRITE,
		   MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	ptr[0] = 'x';

	sprintf(cmd, "cat /proc/%d/maps | grep -E ^%lx\n", parent_pid, ptr);
	system(cmd);

	pid = fork();

	if (pid == 0) {
		int i;
		pid_t child_pid = getpid();

		sprintf(cmd, "cat /proc/%d/maps | grep -E ^%lx\n", child_pid, ptr);
		system(cmd);

		for (i = 0; i < 10 * 4096; i += 4096)
			ptr[i] = 'y';

		madvise(ptr, 10 * 4096, MADV_DONTNEED);

		sprintf(cmd, "cat /proc/%d/maps | grep -E ^%lx\n", child_pid, ptr);
		system(cmd);
	} else if (pid < 0) {
		perror("fork");
	} else {
		wait(NULL);
	}

	return EXIT_SUCCESS;
}
