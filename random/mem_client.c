#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define TARGET_PTR ((void *)0x700000000000UL)

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);

	volatile char *ptr = mmap(TARGET_PTR, page_size, PROT_READ | PROT_WRITE,
				  MAP_ANON | MAP_PRIVATE, -1, 0);

	if (ptr == MAP_FAILED) {
		perror("mmap");

		return EXIT_FAILURE;
	}

	while (true) {
		printf("%s\n", ptr);
		sleep(1);
	}

	return EXIT_SUCCESS;
}
