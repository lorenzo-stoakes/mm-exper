#include "memstat.h"

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
	struct memstat **mstats;
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

	mstats = memstat_snapshot_all(pid);

	// Should have already reported error.
	if (mstats == NULL)
		return EXIT_FAILURE;

	if (!silent)
		memstat_print_all(mstats);

	memstat_free_all(mstats);

	return EXIT_SUCCESS;
}
