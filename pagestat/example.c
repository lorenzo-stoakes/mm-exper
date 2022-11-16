#include "pagestat.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

int main(void)
{
	struct pagestat *ps_before, *ps_after;
        char *ptr = mmap(NULL, 20000, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (ptr == MAP_FAILED) {
                perror("mmap");
                return EXIT_FAILURE;
        }

	ps_before = pagestat_snapshot((uint64_t)ptr);

	printf("=== BEFORE ===\n\n");
	pagestat_print(ps_before);

	ptr[0] = 'x';

	ps_after = pagestat_snapshot((uint64_t)ptr);

	printf("\n=== AFTER ===\n\n");
	pagestat_print_diff(ps_before, ps_after);
	pagestat_free(ps_before);
	pagestat_free(ps_after);

	if (munmap(ptr, 20000) != 0) {
		perror("munmap");

		return EXIT_FAILURE;
	}

        return EXIT_SUCCESS;
}
