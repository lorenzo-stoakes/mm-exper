#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#define NUM_BYTES (128 * 1024 * 1024 * 1024UL)

int main(void)
{
	//const long page_size = sysconf(_SC_PAGESIZE);

	char *ptr = mmap(NULL, NUM_BYTES, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE | MAP_POPULATE | MAP_NORESERVE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap()");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
