#include <dirent.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <linux/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "../read-pageflags/read-pageflags.h"

/*

  start
630,466,560 = sectors
 78,808,320 = pages/blocks

  offset
1,266,201,608 = sectors
  158,275,201 = pages/blocks

 */

// Derived from https://github.com/Distrotech/hdparm/blob/master/fibmap.c

struct local_hd_geometry {
      unsigned char	heads;
      unsigned char	sectors;
      unsigned short	cylinders;
      unsigned long	start;
};

#define ROUND_DOWN(val, multiple) \
	((val) & ~((multiple) - 1))

#define ROUND_UP(val, multiple) \
	(((val) + (multiple) - 1) & ~((multiple) - 1))

#define DIV_UP(val, multiple) \
	(ROUND_UP(val, multiple) / (multiple))

static char next_char(char chr)
{
	if (chr < 'a' || chr > 'z')
		return 'a';

	return chr == 'z' ? 'a' : (chr + 1);
}

static void print_buffers(char *file_buf, char *dev_buf)
{
	printf("file=%s", file_buf);
	printf("---\n");
	printf("dev =%s", dev_buf);
	printf("---\n");
	print_flags_virt(file_buf, "file");
	print_flags_virt(dev_buf, "dev");
	printf("---\n");
}

static unsigned long *get_block_nums(int fd, unsigned long num_blocks,
				     unsigned int blksize)
{
	// Linux hardcodes 'logical' sector size to 512 bytes.
	const unsigned long sectors_per_block = blksize / 512;
	unsigned long *ret = calloc(num_blocks, sizeof(unsigned long));
	int i;

	if (ret == NULL) {
		fprintf(stderr, "Unable to allocate\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < num_blocks; i++) {
		unsigned long blknum_sects = i * sectors_per_block;

		if (ioctl(fd, FIBMAP, &blknum_sects)) {
			perror("ioctl FIBMAP");
			exit(EXIT_FAILURE);
		}

		ret[i] = blknum_sects;
	}

	return ret;
}

int main(int argc, char **argv)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	unsigned long sectors_per_block;
	int fd, dev_fd;
	struct stat st;
	unsigned long num_blocks;
	unsigned int blksize;
	const char *path;
	const char *dev_path;
	unsigned long *block_nums;
	char *file_buf, *dev_buf;
	unsigned long size_in_pages;
	unsigned long offset, offset_page_start, start_block;
	struct local_hd_geometry geometry;
	char next_chr, next_next_chr;
#ifdef PRINT_BLOCK_NUMS
	unsigned long i;
#endif

	if (argc < 3) {
		fprintf(stderr, "usage: %s [path to file] [/dev/...]\n", argv[0]);
		return EXIT_FAILURE;
	}

	path = argv[1];
	dev_path = argv[2];

	fd  = open(path, O_RDWR);
	if (fd < 0) {
		perror("open fd");
		return EXIT_FAILURE;
	}

	dev_fd = open(dev_path, O_RDWR);
	if (dev_fd < 0) {
		perror("open dev fd");
		return EXIT_FAILURE;
	}

	if (fstat(fd, &st) < 0) {
		perror("fstat");
		return EXIT_FAILURE;
	}

	blksize = st.st_blksize;
	sectors_per_block = blksize / 512;
	num_blocks = DIV_UP(st.st_size, blksize);
	size_in_pages = DIV_UP(st.st_size, page_size);

	if (ioctl(dev_fd, HDIO_GETGEO, &geometry)) {
		perror("ioctl HDIO_GETGEO");
		return EXIT_FAILURE;
	}
	start_block = geometry.start / sectors_per_block;

	printf("%s has %lu block(s) of size %u, totalling %lu pages. Disk starts at %lu\n", path, num_blocks, blksize,
	       size_in_pages, start_block);

	if (num_blocks == 0) {
		fprintf(stderr, "Zero blocks?\n");
		return EXIT_FAILURE;
	}

	block_nums = get_block_nums(fd, num_blocks, blksize);
#ifdef PRINT_BLOCK_NUMS
	for (i = 0; i < num_blocks; i++) {
		printf("%lu ", block_nums[i]);
	}
	printf("\n");
#endif
	if (block_nums[0] == 0) {
		fprintf(stderr, "First block a hole\n");
		return EXIT_FAILURE;
	}

	printf("First LBA is %lu\n", block_nums[0]);

	// Map first page.

	file_buf = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);
	if (file_buf == MAP_FAILED) {
		perror("mmap file");
		return EXIT_FAILURE;
	}

	offset = block_nums[0] * page_size;
	offset_page_start = ROUND_DOWN(offset, page_size);

	printf("Offset=%lu, offset_page_start=%lu, delta=%lu\n", offset,
	       offset_page_start, offset - offset_page_start);
	printf("---\n");

	dev_buf = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
		       MAP_SHARED, dev_fd, offset_page_start);
	if (dev_buf == MAP_FAILED) {
		perror("mmap dev");
		return EXIT_FAILURE;
	}
	close(fd);
	close(dev_fd);

	dev_buf = &dev_buf[offset - offset_page_start];

	printf("-- read --\n");
	print_buffers(file_buf, dev_buf);

	next_chr = next_char(file_buf[0]);
	file_buf[0] = next_chr;

	printf("-- after write to file '%c' --\n", next_chr);
	print_buffers(file_buf, dev_buf);

	next_next_chr = dev_buf[0];
	do {
		next_next_chr = next_char(next_next_chr);
	} while (next_next_chr == next_chr);

	dev_buf[0] = next_next_chr;

	printf("-- after write to dev '%c' --\n", next_next_chr);
	print_buffers(file_buf, dev_buf);

	msync(dev_buf, page_size, MS_SYNC);

	printf("-- after msync DEV --\n");
	print_buffers(file_buf, dev_buf);

	// Sync file first.
	msync(file_buf, page_size, MS_SYNC);

	do {
		next_next_chr = next_char(next_next_chr);
	} while (next_next_chr == next_chr);
	dev_buf[0] = next_next_chr;

	printf("-- after write to dev '%c' --\n", next_next_chr);
	print_buffers(file_buf, dev_buf);

	msync(dev_buf, page_size, MS_SYNC);

	printf("-- after msync DEV --\n");
	print_buffers(file_buf, dev_buf);

	do {
		next_chr = next_char(next_chr);
	} while (next_chr == next_next_chr);

	file_buf[0] = next_chr;

	printf("-- after set file_buf to '%c' --\n", next_chr);
	print_buffers(file_buf, dev_buf);

	// Sync file first.
	msync(file_buf, page_size, MS_SYNC);

	do {
		next_next_chr = next_char(next_next_chr);
	} while (next_next_chr == next_chr);

	dev_buf[0] = next_next_chr;

	printf("-- after msync FILE, set dev_buf to '%c' --\n", next_next_chr);
	print_buffers(file_buf, dev_buf);

	// Sync dev...
	//msync(dev_buf, page_size, MS_SYNC);

	return EXIT_SUCCESS;
}
