#include "pagestat.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INTERVAL (1000000) // Default to 1s.

static void usage(const char *bin)
{
	fprintf(stderr, "usage: %s [pid to trace]\n", bin);
}

int main(int argc, char **argv)
{
	const char *pid;
	struct pagestat **pss;
	bool silent = false;
	useconds_t interval = INTERVAL;

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
		// If we are silent we up the interval rate.
		interval /= 10;
	}

	pss = pagestat_snapshot_all(pid);

	// Should have already reported error.
	if (pss == NULL)
		return EXIT_FAILURE;

	// We don't free because we never exit.
	while (true) {
		struct pagestat **pss_curr = pagestat_snapshot_all(pid);
		bool updated = pagestat_print_diff_all(pss, pss_curr);

		if (!updated && !silent)
			printf("(no updates)\n");

		pagestat_free_all(pss);
		pss = pss_curr;

		usleep(interval);
	}
}
