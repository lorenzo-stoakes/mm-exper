#include "shared.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	init();

	long uffd = make_handler(false);
	(void)uffd;

	return EXIT_SUCCESS;
}
