#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
	for (int i = 0; i < 10; i++) {
		pid_t p = fork();
		if (p == -1) {
			puts("failed?");
			return EXIT_FAILURE;
		}

		if (p != 0) {
			printf("forked %d\n", p);

			while (1)
				;

			return EXIT_SUCCESS;
		}

		// Otherwise we spawn another fork.
	}

	return EXIT_SUCCESS;
}
