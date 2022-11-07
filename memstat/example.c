#include "memstat.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

int main(void)
{
        char *ptr = mmap(NULL, 20000, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);

        if (ptr == MAP_FAILED) {
                perror("mmap");
                return EXIT_FAILURE;
        }

	struct memstat *snap = memstat_snapshot((uint64_t)ptr);
	if (snap != NULL)
	    memstat_print(snap);

	if (munmap(ptr, 20000) != 0) {
		perror("munmap");
		return EXIT_FAILURE;
	}

        return EXIT_SUCCESS;
}
