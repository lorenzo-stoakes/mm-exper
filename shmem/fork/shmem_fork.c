#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define NUM_LOOPS (10)

int main(void)
{
	int i;
	pid_t pid;
	const long page_size = sysconf(_SC_PAGESIZE);
	char *ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_SHARED, -1, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	pid = fork();
	if (pid == -1) {
		perror("fork");
		return EXIT_FAILURE;
	}

	/* Child. */
	if (pid == 0) {
		for (i = 0; i < NUM_LOOPS; i++) {
			ptr[1] = 'a' + i;
			printf("child: %s\n", ptr);
			sleep(1);
		}

		return EXIT_SUCCESS;
	}

	/* Parent. */
	for (i = 0; i < NUM_LOOPS; i++) {
		ptr[0] = 'a' + i;
		printf("parent: %s\n", ptr);
		sleep(1);
	}

	if (waitpid(pid, NULL, 0) == -1) {
		perror("waitpid");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
