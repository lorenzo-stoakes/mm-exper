#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define NUM (100000)

/* Retrieve random number up to INCLUSIVE maximum. */
int get_random(int max)
{
	const long rand_num = random();

	return (int)(rand_num % (long)(max + 1));
}

int main(void)
{
	int i;
	const unsigned long page_size = sysconf(_SC_PAGESIZE);
	const unsigned long num_pages = 1000;

	char *ptr = mmap(NULL, num_pages * page_size,
			 PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap 1");
		return EXIT_FAILURE;
	}

	for (i = 0; i < NUM; i++) {
		unsigned long offset = get_random(num_pages - 1);
		unsigned long size = get_random(num_pages - offset);

		if (size == 0)
			continue;

		offset *= page_size;
		size *= page_size;

		if (get_random(1)) {
			if (munmap(ptr + offset, size)) {
				perror("munmap");
				return EXIT_FAILURE;
			}
		} else {
			char *ptr2 = mmap(ptr + offset, size,
					   PROT_READ | PROT_WRITE,
					   MAP_ANON | MAP_PRIVATE | MAP_FIXED,
					   -1, 0);
			if (ptr2 == MAP_FAILED) {
				perror("mmap");
				return EXIT_FAILURE;
			}
		}
	}

	return EXIT_SUCCESS;
}
