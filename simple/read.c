#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
	int fd = open("test.txt", O_RDONLY);
	int err = EXIT_FAILURE;
	struct stat statbuf;
	char *buf;
	off_t size;
	ssize_t bytes_;

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

	bytes_read = read(fd, buf, size);

	if (bytes_read == -1) {
		perror("read");
		goto exit_free;
	} else if (bytes_read < size) {
		fprintf(stderr, "Read %ld bytes, expected %ld",
			bytes_read, size);
		goto exit_free;
	}

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
