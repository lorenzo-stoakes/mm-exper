#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	FILE *file = fopen("test.txt", "w");

	if (file == NULL) {
		perror("fopen");
		return EXIT_FAILURE;
	}

	fprintf(file, "hello!\n");

	fclose(file);

	return EXIT_SUCCESS;
}
