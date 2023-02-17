#include "shared.h"

#include <stdio.h>
#include <stdlib.h>

#define BIT(n) (1UL << n)

// Flags only used by other flags.
#define VM_ARCH_1	0x01000000
#define VM_HIGH_ARCH_BIT_0	32
#define VM_HIGH_ARCH_BIT_1	33
#define VM_HIGH_ARCH_BIT_2	34
#define VM_HIGH_ARCH_BIT_3	35
#define VM_HIGH_ARCH_BIT_4	36
#define VM_HIGH_ARCH_0	BIT(VM_HIGH_ARCH_BIT_0)
#define VM_HIGH_ARCH_1	BIT(VM_HIGH_ARCH_BIT_1)
#define VM_HIGH_ARCH_2	BIT(VM_HIGH_ARCH_BIT_2)
#define VM_HIGH_ARCH_3	BIT(VM_HIGH_ARCH_BIT_3)
#define VM_HIGH_ARCH_4	BIT(VM_HIGH_ARCH_BIT_4)
#define VM_UFFD_MINOR_BIT	37

// Named flags.
#define VM_NONE		0x00000000
#define VM_READ		0x00000001
#define VM_WRITE	0x00000002
#define VM_EXEC		0x00000004
#define VM_SHARED	0x00000008
#define VM_MAYREAD	0x00000010
#define VM_MAYWRITE	0x00000020
#define VM_MAYEXEC	0x00000040
#define VM_MAYSHARE	0x00000080
#define VM_GROWSDOWN	0x00000100
#define VM_UFFD_MISSING	0x00000200
#define VM_PFNMAP	0x00000400
#define VM_UFFD_WP	0x00001000
#define VM_LOCKED	0x00002000
#define VM_IO           0x00004000
#define VM_SEQ_READ	0x00008000
#define VM_RAND_READ	0x00010000
#define VM_DONTCOPY	0x00020000
#define VM_DONTEXPAND	0x00040000
#define VM_LOCKONFAULT	0x00080000
#define VM_ACCOUNT	0x00100000
#define VM_NORESERVE	0x00200000
#define VM_HUGETLB	0x00400000
#define VM_SYNC		0x00800000
#define VM_WIPEONFORK	0x02000000
#define VM_DONTDUMP	0x04000000
#define VM_SOFTDIRTY	0x08000000
#define VM_MIXEDMAP	0x10000000
#define VM_HUGEPAGE	0x20000000
#define VM_NOHUGEPAGE	0x40000000
#define VM_MERGEABLE	0x80000000
#define VM_PKEY_SHIFT	VM_HIGH_ARCH_BIT_0
#define VM_PKEY_BIT0	VM_HIGH_ARCH_0
#define VM_PKEY_BIT1	VM_HIGH_ARCH_1
#define VM_PKEY_BIT2	VM_HIGH_ARCH_2
#define VM_PKEY_BIT3	VM_HIGH_ARCH_3
#define VM_PKEY_BIT4	VM_HIGH_ARCH_4
#define VM_PAT		VM_ARCH_1
#define VM_UFFD_MINOR	BIT(VM_UFFD_MINOR_BIT)

struct flag_pair {
	unsigned long flag;
	const char *name;
};

static struct flag_pair flags_map[] = {
	// VM_NONE is handled separately.
	{ VM_READ, "VM_READ" },
	{ VM_WRITE, "VM_WRITE" },
	{ VM_EXEC, "VM_EXEC" },
	{ VM_SHARED, "VM_SHARED" },
	{ VM_MAYREAD, "VM_MAYREAD" },
	{ VM_MAYWRITE, "VM_MAYWRITE" },
	{ VM_MAYEXEC, "VM_MAYEXEC" },
	{ VM_MAYSHARE, "VM_MAYSHARE" },
	{ VM_GROWSDOWN, "VM_GROWSDOWN" },
	{ VM_UFFD_MISSING, "VM_UFFD_MISSING" },
	{ VM_PFNMAP, "VM_PFNMAP" },
	{ VM_UFFD_WP, "VM_UFFD_WP" },
	{ VM_LOCKED, "VM_LOCKED" },
	{ VM_IO, "VM_IO" },
	{ VM_SEQ_READ, "VM_SEQ_READ" },
	{ VM_RAND_READ, "VM_RAND_READ" },
	{ VM_DONTCOPY, "VM_DONTCOPY" },
	{ VM_DONTEXPAND, "VM_DONTEXPAND" },
	{ VM_LOCKONFAULT, "VM_LOCKONFAULT" },
	{ VM_ACCOUNT, "VM_ACCOUNT" },
	{ VM_NORESERVE, "VM_NORESERVE" },
	{ VM_HUGETLB, "VM_HUGETLB" },
	{ VM_SYNC, "VM_SYNC" },
	{ VM_WIPEONFORK, "VM_WIPEONFORK" },
	{ VM_DONTDUMP, "VM_DONTDUMP" },
	{ VM_SOFTDIRTY, "VM_SOFTDIRTY" },
	{ VM_MIXEDMAP, "VM_MIXEDMAP" },
	{ VM_HUGEPAGE, "VM_HUGEPAGE" },
	{ VM_NOHUGEPAGE, "VM_NOHUGEPAGE" },
	{ VM_MERGEABLE, "VM_MERGEABLE" },
	{ VM_PKEY_SHIFT, "VM_PKEY_SHIFT" },
	{ VM_PKEY_BIT0, "VM_PKEY_BIT0" },
	{ VM_PKEY_BIT1, "VM_PKEY_BIT1" },
	{ VM_PKEY_BIT2, "VM_PKEY_BIT2" },
	{ VM_PKEY_BIT3, "VM_PKEY_BIT3" },
	{ VM_PKEY_BIT4, "VM_PKEY_BIT4" },
	{ VM_PAT, "VM_PAT" },
	{ VM_UFFD_MINOR, "VM_UFFD_MINOR" },
};

#define FLAGS_COUNT (sizeof(flags_map) / sizeof(struct flag_pair))

void print_vma_flags(unsigned long flags)
{
	if (flags == 0) {
		printf("VM_NONE\n");
		return;
	}

	for (int i = 0; i < FLAGS_COUNT; i++) {
		struct flag_pair *pair = &flags_map[i];

		if ((flags & pair->flag) != pair->flag)
			continue;

		flags &= ~pair->flag;
		printf("%s ", pair->name);
	}

	if (flags != 0)
		printf("(invalid: %lu remain)", flags);
	printf("\n");
}

void lookup_vma_flags(void *ptr)
{
	unsigned long addr = (unsigned long)ptr;
	FILE *fp = fopen("/dev/vmaflags", "r+");
	unsigned long flags;

	if (fp == NULL) {
		fprintf(stderr, "Cannot open /dev/vmaflags");
		exit(1);
	}

	if (fprintf(fp, "%lu\n", addr) < 0) {
		perror("writing to /dev/vmaflags");
		exit(1);
	}

	fflush(fp);
	fclose(fp);

	fp = fopen("/dev/vmaflags", "r");

	if (fscanf(fp, "%lx", &flags)) {
		perror("reading from /dev/vmaflags");
		exit(1);
	}
	fclose(fp);

	print_vma_flags(flags);
}
