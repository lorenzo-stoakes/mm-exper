#include "memstat.h"

#include <stdio.h>

void memstat_print_short(struct memstat *mstat)
{
	printf("%lx-%lx: %s ", mstat->vma_start, mstat->vma_end, mstat->perms);

	if (mstat->offset > 0)
		printf("offset=[%lu] ", mstat->offset);

	if (mstat->name != NULL)
		printf("name=[%s] ", mstat->name);

	printf("vm=[%lu] rss=[%lu]\n", mstat->vm_size, mstat->rss);
}

void memstat_print(struct memstat *mstat)
{
	(void)mstat;
}
