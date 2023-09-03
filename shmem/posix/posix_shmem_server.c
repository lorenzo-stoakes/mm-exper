#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <unistd.h>

#define SHM_NAME "test"
#define SIZE (4096)
#define NUM_LOOPS (10)

int main(void)
{
	int i, fd;
	char *ptr;

	fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
	if (fd < 0) {
		perror("shm_open");
		return EXIT_FAILURE;
	}

	if (ftruncate(fd, SIZE)) {
		perror("ftruncate");
		return EXIT_FAILURE;
	}

	ptr = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	memset(ptr, 0, SIZE);

	for (i = 0; i < NUM_LOOPS; i++) {
		ptr[0] = 'a' + i;
		printf("server: %s\n", ptr);
		sleep(1);
	}

	/* Wait until client is done. */
	while (ptr[1] != 'a' + (NUM_LOOPS - 1)) {
		sleep(1);
	}

	if (shm_unlink(SHM_NAME)) {
		perror("shm_unlink");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
