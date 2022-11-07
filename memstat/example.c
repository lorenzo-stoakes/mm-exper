#include "memstat.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

int main(void)
{
        char *ptr = mmap(NULL, 20000, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (ptr == MAP_FAILED) {
                perror("mmap");
                return EXIT_FAILURE;
        }

	ptr[0] = 'x';

	struct memstat *mstat = memstat_snapshot((uint64_t)ptr);
	if (mstat != NULL)
	    memstat_print(mstat);

	if (munmap(ptr, 20000) != 0) {
		perror("munmap");
		return EXIT_FAILURE;
	}

        return EXIT_SUCCESS;
}
