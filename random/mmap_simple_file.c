#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
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
		   MAP_SHARED_VALIDATE, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	close(fd);

	ptr[0] = ptr[0] == 'z' ? 'a' : ptr[0] + 1;

	if (msync(ptr, st.st_size, MS_SYNC)) {
		perror("msync");
		return EXIT_FAILURE;
	}

	if (munmap(ptr, st.st_size)) {
		perror("munmap");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
