#define _GNU_SOURCE
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define NR_PAGES (10)
#define ALIGN_UP(from, to) ((from + to - 1) & ~(to - 1))

#define LOOP_PRINT
//#define USE_MEMFD

static unsigned long page_size, size;

static int get_fd(void)
{
#ifdef USE_MEMFD
	int fd = memfd_create("test", MFD_ALLOW_SEALING);
#else
	int fd = open("test.txt", O_RDWR | O_CREAT, 0666);
#endif

	if (fd < 0) {
		perror("open");
		exit(1);
	}

	if (ftruncate(fd, size)) {
		perror("ftruncate");
		exit(1);
	}

	return fd;
}

static void reset_buffer(char *ptr)
{
	int i;

	for (i = 0; i < size; i++) {
		ptr[i] = 'x';
	}
}

static void copy_first_half(char *ptr)
{
	int i;

	/* Copy 2nd half of private pages. */
	for (i = 0; i < NR_PAGES / 2; i++) {
		ptr[i * page_size] = 'y';
	}
}

static void print_buf(int fd, bool is_shared, const char *ptr)
{
	int i;
	struct stat statbuf;
	int num_pages;

	if (fstat(fd, &statbuf)) {
		perror("fstat");
		exit(1);
	}

	num_pages = ALIGN_UP(statbuf.st_size, page_size) / page_size;

	if (is_shared)
		printf("shared:  ");
	else
		printf("private: ");

	for (i = 0; i < num_pages; i++) {
		printf("%c", ptr[i * page_size]);
	}

	printf("\n");
}

static void do_overwrite(int fd)
{
	char *buf = malloc(size);

	memset(buf, 'z', size);

	if (pwrite(fd, buf, size, 0) != size) {
		perror("pwrite");
		exit(1);
	}

	free(buf);
}

int main(void)
{
	int fd;
	char *ptr_shared, *ptr_private;

#define PRINT(prefix)					\
	{						\
		printf("--- %s ---\n", prefix);		\
		print_buf(fd, true, ptr_shared);	\
		print_buf(fd, false, ptr_private);	\
	}

	page_size = sysconf(_SC_PAGESIZE);
	size = NR_PAGES * page_size;

	fd = get_fd();

	ptr_shared = mmap(NULL, size, PROT_READ | PROT_WRITE,
			  MAP_SHARED, fd, 0);
	if (ptr_shared == MAP_FAILED) {
		perror("mmap shared");
		return EXIT_FAILURE;
	}

	reset_buffer(ptr_shared);

	ptr_private = mmap(NULL, size, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE, fd, 0);
	if (ptr_private == MAP_FAILED) {
		perror("mmap private");
		return EXIT_FAILURE;
	}

	PRINT("init");

	copy_first_half(ptr_private);

	PRINT("copied private pages");

	ftruncate(fd, 11 * page_size);

	PRINT("truncated UP");

	ftruncate(fd, 5 * page_size);
	ftruncate(fd, 10 * page_size);

	PRINT("truncated DOWN then back up again");

	/* Reset again. */
	reset_buffer(ptr_shared);
	copy_first_half(ptr_private);

	do_overwrite(fd);

	PRINT("after overwrite");

#ifdef LOOP_PRINT
	while (true) {
		PRINT("loop");
		sleep(1);
	}
#endif

	close(fd);
	return EXIT_SUCCESS;

#undef PRINT_SHARED
#undef PRINT_PRIVATE
}
