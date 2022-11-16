#include "pagestat.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *bin)
{
	fprintf(stderr, "usage: %s [pid to trace] <-s>\n", bin);
}

int main(int argc, char **argv)
{
	const char *pid;
	struct pagestat **pss;
	bool silent = false;

	if (argc < 2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	pid = argv[1];
	if (argc >= 3) {
		if (strncmp(argv[2], "-s", sizeof("-s")) != 0) {
			usage(argv[0]);
			return EXIT_FAILURE;
		}

		silent = true;
	}

	pss = pagestat_snapshot_all(pid);

	// Should have already reported error.
	if (pss == NULL)
		return EXIT_FAILURE;

	if (!silent)
		pagestat_print_all(pss);

	pagestat_free_all(pss);

	return EXIT_SUCCESS;
}
