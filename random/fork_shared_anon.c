#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
	long page_size = sysconf(_SC_PAGESIZE);

	char *ptr_shared = mmap(NULL, page_size * 10, PROT_READ | PROT_WRITE,
				MAP_ANON | MAP_SHARED, -1, 0);
	if (ptr_shared == MAP_FAILED) {
		perror("mmap 1");
		return EXIT_FAILURE;
	}

	char *ptr_priv = mmap(NULL, page_size * 10, PROT_READ | PROT_WRITE,
			      MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr_priv == MAP_FAILED) {
		perror("mmap 2");
		return EXIT_FAILURE;
	}

	ptr_priv[0] = 'x';

	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		return EXIT_FAILURE;
	}

	// Child.
	if (pid == 0) {
		// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
		// (at madvise_walk_vmas()).
		madvise(ptr_priv, page_size, MADV_NORMAL);

		ptr_shared[0] = 'x';
		ptr_priv[0] = 'x'; // CoW.

		sleep(2);
		return EXIT_SUCCESS;
	}

	// Parent.
	sleep(1);

	printf("shared=[%c], priv=[%c]\n", ptr_shared[0], ptr_priv[0]);

	waitpid(pid, NULL, 0);

	return EXIT_SUCCESS;
}
