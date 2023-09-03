#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

#define KEY (1234)
#define SIZE (4096)
#define NUM_LOOPS (10)

int main(void)
{
	int i, shmid;
	char *shm;

	shmid = shmget(KEY, SIZE, 0666);
	if (shmid < 0) {
		perror("shmget");
		return EXIT_FAILURE;
	}

	shm = shmat(shmid, NULL, 0);
	if (shm == (void *)-1) {
		perror("shmat");
		return EXIT_FAILURE;
	}

	for (i = 0; i < NUM_LOOPS; i++) {
		shm[1] = 'a' + i;
		printf("client: %s\n", shm);
		sleep(1);
	}

	return EXIT_SUCCESS;
}
