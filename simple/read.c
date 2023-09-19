#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
	int fd = open("test.txt", O_RDONLY);
	ssize_t num_bytes = 0;
	int err = EXIT_FAILURE;
	struct stat statbuf;
	char *buf;
	size_t size;

	if (fd == -1) {
		perror("open");
		goto exit;
	}

	if (fstat(fd, &statbuf)) {
		perror("fstat");
		goto exit_close;
	}

	size = statbuf.st_size;
	if (size == 0) {
		err = EXIT_SUCCESS;
		goto exit_close;
	}

	buf = malloc(size + 1);
	if (!buf) {
		perror("malloc");
		goto exit_close;
	}

	do {
		ssize_t ret = read(fd, &buf[num_bytes], size - num_bytes);

		if (ret == -1) {
			perror("read");
			goto exit_free;
		}

		num_bytes += ret;
	} while (num_bytes < size);

	buf[size] = '\0';
	printf("%s", buf);

	err = EXIT_SUCCESS;

exit_free:
	free(buf);
exit_close:
	close(fd);
exit:
	return err;
}
