#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main()
{
	const long page_size = sysconf(_SC_PAGESIZE);
	int fd = memfd_create("test", MFD_ALLOW_SEALING);
	char *buf;

	if (fd == -1) {
		perror("memfd_create");
		return EXIT_FAILURE;
	}

	write(fd, "test", sizeof("test"));

	if (fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE) == -1) {
		perror("fcntl");
		return EXIT_FAILURE;
	}

	buf = mmap(NULL, page_size, PROT_READ, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	close(fd);

	printf("%s\n", buf);

	return EXIT_SUCCESS;
}
