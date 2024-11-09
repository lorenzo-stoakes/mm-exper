#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

int main(void)
{
	char *ptr = mmap(NULL, 2 * 1024 * 1024, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANON | MAP_HUGETLB, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	puts("Wait...");
	getchar();

	ptr[0] = 'x';

	puts("Fault Wait...");
	getchar();

	return EXIT_SUCCESS;
}
