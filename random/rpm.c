#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

void say_hi()
{
	printf("hi!\n");
	while (true)
		;
}

int main()
{
	void *ptr = &say_hi;

	// Now madvise MADV_NORMAL to make it easy to put a breakpoint in gdb
	// (at madvise_walk_vmas()).

	if (madvise((void *)((unsigned long)ptr & ~4095), 1, MADV_NORMAL)) {
		perror("madvise");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
