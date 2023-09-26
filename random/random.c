#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
	long page_size = sysconf(_SC_PAGESIZE);
	int fd;
	char *buf;

	fd = open("test.txt", O_RDWR);
	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	buf = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	if (madvise(buf, page_size, MADV_RANDOM)) {
		perror("madvise");
		return EXIT_FAILURE;
	}

	printf("1st char ='%c'\n", buf[0]);

	return EXIT_SUCCESS;
}
