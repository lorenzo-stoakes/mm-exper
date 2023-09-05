#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define NAME "test"
#define SIZE (4096)
#define NUM_LOOPS (10)

int main(void)
{
	int i, fd;
	pid_t pid;
	char *ptr;

	fd = memfd_create(NAME, MFD_ALLOW_SEALING);
	if (fd < 0) {
		perror("memfd_create");
		return EXIT_FAILURE;
	}

	if (ftruncate(fd, SIZE)) {
		perror("ftruncate");
		return EXIT_FAILURE;
	}

	/* Do not permit the size to change henceforth. */
	if (fcntl(fd, F_ADD_SEALS,
		  F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL) < 0) {
		perror("fcntl");
		return EXIT_FAILURE;
	}

	ptr = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	memset(ptr, 0, SIZE);

	pid = getpid();
	printf("Running at /proc/%d/fd/%d\n", pid, fd);

	/* Wait for client... */
	while (ptr[1] < 'a') {
		sleep(1);
	}

	for (i = 0; i < NUM_LOOPS; i++) {
		ptr[0] = 'a' + i;
		printf("server: %s\n", ptr);
		sleep(1);
	}

	return EXIT_SUCCESS;
}
