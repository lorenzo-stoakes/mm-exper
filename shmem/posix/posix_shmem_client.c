#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
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

	fd = shm_open(SHM_NAME, O_RDWR, 0666);
	if (fd < 0) {
		perror("shm_open");
		return EXIT_FAILURE;
	}

	ptr = mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	for (i = 0; i < NUM_LOOPS; i++) {
		ptr[1] = 'a' + i;
		printf("client: %s\n", ptr);
		sleep(1);
	}

	return EXIT_SUCCESS;
}
