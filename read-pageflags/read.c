#include "read-pageflags.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static bool check_hugetlb(void)
{
	static const char *path = "/sys/kernel/mm/hugepages/hugepages-2048kB/free_hugepages";
	int val = 0;
	FILE *fp = fopen(path, "r");

	if (fp == NULL)
		return false;

	// If this fails we default to 0 anyway and assume false.
	fscanf(fp, "%d", &val);
	fclose(fp);

	return val > 0;
}

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	if (getuid() != 0) {
		fprintf(stderr, "ERROR: Must be run as root.\n");
		return EXIT_FAILURE;
	}

	// First allocate a page of memory from the kernel.
	void *ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	((char *)ptr)[0] = 'x';

	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	if (!print_flags_virt(ptr, "initial mmap"))
		return EXIT_FAILURE;

	// Do something with the page.
	memset(ptr, 123, page_size);

	if (!print_flags_virt(ptr, "modified page"))
		return EXIT_FAILURE;

	munmap(ptr, page_size);

	void *ptr2 = malloc(page_size);
	if (!print_flags_virt(ptr2, "initial malloc"))
		return EXIT_FAILURE;

	memset(ptr2, 123, page_size);

	if (!print_flags_virt(ptr2, "modified malloc"))
		return EXIT_FAILURE;

	int fd = open("test.txt", O_RDWR);
	if (fd == -1) {
		perror("open test.txt");
		return EXIT_FAILURE;
	}

	char *ptr3 = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			  MAP_SHARED, fd, 0);
	// We can discard the fd now we've mmap'd it.
	close(fd);

	if (ptr3 == MAP_FAILED) {
		perror("mmap test.txt");
		return EXIT_FAILURE;
	}

	static volatile char dummy;
	dummy = ptr3[0];
	(void)dummy;

	if (!print_flags_virt(ptr3, "mmap file"))
		return EXIT_FAILURE;

	ptr3[0] = 'Z';

	if (!print_flags_virt(ptr3, "mmap modified file"))
		return EXIT_FAILURE;

	madvise(ptr3, page_size, MADV_COLD);

	if (!print_flags_virt(ptr3, "mmap modified, cold file"))
		return EXIT_FAILURE;

	munmap(ptr3, page_size);

	char *ptr4 = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS , -1, 0);

	if (ptr4 == MAP_FAILED) {
		perror("mmap (4)");
		return EXIT_FAILURE;
	}

	ptr4[0] = 'x';

	print_flags_virt(ptr4, "mmap anon, pre-fork");

	fd = open("test.txt", O_RDWR);
	if (fd == -1) {
		perror("open test.txt");
		return EXIT_FAILURE;
	}
	char *ptr4b = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE, fd, 0);
	if (ptr4b == MAP_FAILED) {
		perror("mmap (4b)");
		return EXIT_FAILURE;
	}

	ptr4b[0] = 'x';

	char *ptr4c = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE, fd, 0);
	if (ptr4c == MAP_FAILED) {
		perror("mmap (4c)");
		return EXIT_FAILURE;
	}

	ptr4c[0] = 'x';

	close(fd);

	print_flags_virt(ptr4b, "mmap private file, pre-fork");

	pid_t p = fork();
	if (p == 0) {
		print_flags_virt(ptr4, "mmap anon, forked");

		ptr4[0] = 'y';
		print_flags_virt(ptr4, "mmap anon, forked, modified (pre-sleep)");

		sleep(1);
		print_flags_virt(ptr4, "mmap anon, forked, modified (after sleep)");

		print_flags_virt(ptr4b, "mmap private file, post-fork");
		ptr4b[3] = 'x';
		print_flags_virt(ptr4b, "mmap private file, post-fork (modify)");

		if (ptr4[0] == 'y')
			ptr4[0] = 'x';

		ptr4c[3] = 'x'; // Trigger CoW
		print_flags_virt(ptr4c, "mmap private file, post-fork, not populated, CoW (modify)");

		sleep(1);

		print_flags_virt(ptr4, "mmap anon, forked, modified (after sleep, modify)");

		print_flags_virt(ptr4b, "mmap private file, post-fork (after sleep, modify)");

		return EXIT_SUCCESS;
	}
	wait(NULL);

	fd = open("test.txt", O_RDWR);
	if (fd == -1) {
		perror("open test.txt");
		return EXIT_FAILURE;
	}

	char *ptr5 = mmap(NULL, page_size + 1, PROT_READ | PROT_WRITE,
			  MAP_SHARED, fd, 0);
	ptr5[0] = 'x';

	close(fd);
	if (ptr5 == MAP_FAILED) {
		perror("mmap (5)");
		return EXIT_FAILURE;
	}

	print_flags_virt(ptr5, "mmap file page 1, all bytes");
	print_flags_virt(ptr5 + page_size, "mmap file page 2, all bytes");

	char *ptr6 = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	ptr6[0] = 'x';
	if (ptr6 == MAP_FAILED) {
		perror("mmap (6)");
		return EXIT_FAILURE;
	}
	print_flags_virt(ptr6, "mmap anon, shared");

	pid_t p2 = fork();
	if (p2 == 0) {
		ptr6[0] = 'x';

		print_flags_virt(ptr6, "mmap anon, shared, post fork");

		return EXIT_SUCCESS;
	}
	wait(NULL);
	print_flags_virt(ptr6, "mmap anon, shared, post fork done");

	munmap(ptr6, page_size);

	char *ptr7 = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
	ptr7[0] = 'x';
	if (ptr7 == MAP_FAILED) {
		perror("mmap (7)");
		return EXIT_FAILURE;
	}

	print_flags_virt(ptr7, "mmap anon, noreserve");

	int ro_fd = open("test_ro.txt", O_RDONLY);
	if (ro_fd < 0) {
		perror("open test_ro.txt");
		return EXIT_FAILURE;
	}

	char *ptr8 = mmap(NULL, page_size, PROT_READ, MAP_SHARED, ro_fd, 0);
	if (ptr8 == MAP_FAILED) {
		perror("mmap (8)");
		return EXIT_FAILURE;
	}

	dummy = ptr8[0];

	print_flags_virt(ptr8, "mmap file, readonly");

	/*** huge-only below. ***/

	// Must have set up hugetlb pages in /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
	if (!check_hugetlb()) {
		puts("[hugetlb pages not availble, skipping tests]");
		return EXIT_SUCCESS;
	}

	char *ptr_huge = mmap(NULL, 2 * 1024 * 1024, PROT_READ | PROT_WRITE,
			      MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
	ptr_huge[0] = 'x';
	if (ptr_huge == MAP_FAILED) {
		perror("mmap (7)");
		return EXIT_FAILURE;
	}
	print_flags_virt(ptr_huge, "mmap anon, hugetlb");
	ptr_huge[0] = 'x';
	ptr_huge[page_size] = 'y';
	ptr_huge[2 * 1024 *1024 - 1] = 'z';
	print_flags_virt(ptr_huge, "mmap anon, hugetlb, pre sleep, modification");
	sleep(1);
	print_flags_virt(ptr_huge, "mmap anon, hugetlb, post sleep, modification");

	return EXIT_SUCCESS;
}
