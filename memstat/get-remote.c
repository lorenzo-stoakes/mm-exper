#include "memstat.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	const char *pid;
	struct memstat **mstats;
	int i;

	if (argc < 2) {
		fprintf(stderr, "usage: %s [pid to trace]\n", argv[0]);
		return EXIT_FAILURE;
	}

	pid = argv[1];
	mstats = memstat_snapshot_all(pid);

	// Should have already reported error.
	if (mstats == NULL)
		return EXIT_FAILURE;

	for (i = 0; i < MAX_MAPS; i++) {
		struct memstat *mstat = mstats[i];
		if (mstat == NULL)
			break;

		printf("\n");
		memstat_print(mstat);
	}

	memstat_free_all(mstats);

	return EXIT_SUCCESS;
}
