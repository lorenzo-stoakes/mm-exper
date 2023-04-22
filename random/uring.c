#include <liburing.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int normal()
{
	struct io_uring_params params = {
		.flags = 0
	};

	int ring = io_uring_setup(2, &params);
	if (ring < 0) {
		fprintf(stderr, "io_uring_setup: %s\n", strerror(-ring));
		return EXIT_FAILURE;
	}


	void *buf = mmap(NULL, 4096 * 10, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE | MAP_POPULATE, -1, 0);
	if (buf == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	if (mlock(buf, 4096 * 3)) {
		perror("mlock");
		return EXIT_FAILURE;
	}

	struct iovec iov = {
		.iov_base = buf,
		.iov_len = 4096 * 10,
	};

	int ret = io_uring_register(ring, IORING_REGISTER_BUFFERS, &iov, 1);
	if (ret < 0) {
		fprintf(stderr, "io_uring_register: %s\n", strerror(ret));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int file_broken()
{
	int fd = open("test.txt", O_RDWR);
	if (fd == -1) {
		perror("open");
		return EXIT_FAILURE;
	}

	struct io_uring_params params = {
		.flags = 0
	};

	int ring = io_uring_setup(2, &params);

	void *buf2 = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
			  MAP_SHARED, fd, 0);
	if (buf2 == MAP_FAILED) {
		perror("mmap file");
		return EXIT_FAILURE;
	}

	struct iovec iov2 = {
		.iov_base = buf2,
		.iov_len = 4096,
	};

	int ret2 = io_uring_register(ring, IORING_REGISTER_BUFFERS, &iov2, 1);
	if (ret2 < 0) {
		fprintf(stderr, "io_uring_register 2: %s\n", strerror(-ret2));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int main()
{
	return normal();
}
