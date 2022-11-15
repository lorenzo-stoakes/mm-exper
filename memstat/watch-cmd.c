#include "memstat.h"

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
	struct memstat **mstats;
	bool silent = false;
	useconds_t interval = INTERVAL;
	uint64_t counter = 1;

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

	mstats = memstat_snapshot_all(pid);

	// Should have already reported error.
	if (mstats == NULL)
		return EXIT_FAILURE;

	// We don't free because we never exit.
	while (true) {
		struct memstat **mstats_curr = memstat_snapshot_all(pid);
		bool updated = memstat_print_diff_all(mstats, mstats_curr);

		if (!updated && !silent)
			printf("(no updates)\n");

		if (updated)
			printf("^^-- COUNTER=%lu --^^\n", counter++);

		memstat_free_all(mstats);
		mstats = mstats_curr;

		usleep(interval);
	}
}
