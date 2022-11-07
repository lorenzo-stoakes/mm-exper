#include "memstat.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

int main(void)
{
	struct memstat *mstat_before, *mstat_after;

        char *ptr = mmap(NULL, 20000, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (ptr == MAP_FAILED) {
                perror("mmap");
                return EXIT_FAILURE;
        }

	mstat_before = memstat_snapshot((uint64_t)ptr);

	ptr[0] = 'x';

	mstat_after = memstat_snapshot((uint64_t)ptr);

	memstat_print_diff(mstat_before, mstat_after);


	if (munmap(ptr, 20000) != 0) {
		perror("munmap");
		return EXIT_FAILURE;
	}

        return EXIT_SUCCESS;
}
