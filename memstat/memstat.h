#pragma once

#include <stdbool.h>
#include <stdint.h>

// VMA and underlying page statistics taken from:-
//   * /proc/$$/smaps
//   * /proc/$$/pagemap
//   * /proc/$$/kpageflag
struct memstat {
	// Subset of VMA information.
	uint64_t vma_start, vma_end;
	const char perms[5];
	uint64_t offset;
	const char *name;

	// Subset of smaps information.
	uint64_t vm_size;
	uint64_t rss;
	bool rss_counted; // Did we manually count entries? (VM stats being slow)
	uint64_t referenced;
	uint64_t anon;
	uint64_t anon_huge;
	uint64_t swap;
	uint64_t locked;
	const char *vm_flags;

	// Page table/phys page information.
	uint64_t *pagemaps;
	uint64_t *kpagecounts;
	uint64_t *kpageflags;
};

// Detailed information to stdout.
void memstat_print(struct memstat *mstat);

// Grab snapshot for VMA containing specified virtual address. Returns NULL if
// VMA cannot be found.
struct memstat *memstat_snapshot(uint64_t vaddr);

// Diff between two mstats to stdout.
void memstat_print_diff(struct memstat *mstat_a, struct memstat *mstat_b);

// Free previously allocated mstat object.
void memstat_free(struct memstat *mstat);
