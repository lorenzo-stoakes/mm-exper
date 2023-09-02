#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
	int fd;
	char *ptr;
	struct stat st;

	fd = open("test.txt", O_RDWR);
	if (fd == -1) {
		perror("open");
		return EXIT_FAILURE;
	}

	if (fstat(fd, &st)) {
		perror("fstat");
		return EXIT_FAILURE;
	}

	ptr = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE,
		   MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	close(fd);

	while (1) {
		char chr;

		if (strncmp(ptr, "exit", 4) == 0)
			break;

		chr = ptr[0];
		if (chr < 'a' || chr == 'z') {
			ptr[0] = 'a';
		} else {
			ptr[0]++;
		}

		if (msync(ptr, st.st_size, MS_SYNC)) {
			perror("msync");
			return EXIT_FAILURE;
		}


		printf("%s", ptr);
		sleep(1);
	}

	if (munmap(ptr, st.st_size)) {
		perror("munmap");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
