#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MAX_MAPS 8192

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

// Grab snapshot for VMA containing specified virtual address. Returns NULL if
// VMA cannot be found.
struct memstat *memstat_snapshot(uint64_t vaddr);

// Same as memstat_snapshot() only can specify remote PID.
struct memstat *memstat_snapshot_remote(const char *pid, uint64_t vaddr);

// Grab snapshot of all memory mappings.
struct memstat **memstat_snapshot_all(const char *pid);

// Detailed information to stdout.
void memstat_print(struct memstat *mstat);

// Print all memstat entries.
void memstat_print_all(struct memstat **mstats);

// Print diff between two mstats to stdout.
void memstat_print_diff(struct memstat *mstat_a, struct memstat *mstat_b);

// Free previously allocated mstat object.
void memstat_free(struct memstat *mstat);

// Free bulk-allocated mstat objects.
void memstat_free_all(struct memstat **mstats);
