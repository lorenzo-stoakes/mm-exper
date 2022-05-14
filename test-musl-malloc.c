#include <stdio.h>
#include <stdlib.h>

#include "musl/oldmalloc.h"

int main(void)
{
	void *ptr = musl_malloc(17);
	musl_free(ptr);

	return EXIT_SUCCESS;
}
