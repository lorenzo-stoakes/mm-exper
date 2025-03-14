#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
	char *ptr;
	unsigned long page_size = 4096;
	char cmd[4096];
	pid_t pid = getpid();

	ptr = mmap(NULL, page_size * 12, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	ptr = mmap(&ptr[page_size], page_size * 10, PROT_READ | PROT_WRITE,
		   MAP_ANON | MAP_PRIVATE | MAP_FIXED | MAP_NORESERVE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 2");
		return EXIT_FAILURE;
	}

	/* Make first 5 pages readonly. */
	if (mprotect(ptr, page_size * 5, PROT_READ)) {
		perror("mprotect");
		return EXIT_FAILURE;
	}

	/* Fault in first of last 5 pages to get anon_vma. */
	ptr[5 * page_size] = 'x';

	/* Now mprotect() the succeeding bit back, we should merge. */
	if (mprotect(&ptr[page_size * 5], page_size * 5, PROT_READ)) {
		perror("mprotect 2");
		return EXIT_FAILURE;
	}

	sprintf(cmd, "cat /proc/%d/maps |  grep -E \"^%lx|^%lx\"\n",
		pid, ptr, &ptr[5 * page_size]);
	system(cmd);

	sprintf(cmd, "dmesg | grep -E \"%lx|%lx\"\n", ptr, &ptr[5 * page_size]);
	system(cmd);

	return EXIT_SUCCESS;
}
