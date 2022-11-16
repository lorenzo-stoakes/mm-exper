#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MAX_MAPS 8192

// VMA and underlying page statistics taken from:-
//   * /proc/$$/smaps
//   * /proc/$$/pagemap
//   * /proc/$$/kpageflag
struct pagestat {
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
struct pagestat *pagestat_snapshot(uint64_t vaddr);

// Same as pagestat_snapshot() only can specify remote PID.
struct pagestat *pagestat_snapshot_remote(const char *pid, uint64_t vaddr);

// Grab snapshot of all memory mappings.
struct pagestat **pagestat_snapshot_all(const char *pid);

// Detailed information to stdout.
bool pagestat_print(struct pagestat *ps);

// Print all pagestat entries.
void pagestat_print_all(struct pagestat **pss);

// Print diff between two pagestats to stdout.
bool pagestat_print_diff(struct pagestat *ps_a, struct pagestat *ps_b);

// Print all pagestat diffs. Return indicates if diff detected.
bool pagestat_print_diff_all(struct pagestat **pss_a, struct pagestat **pss_b);

// Free previously allocated pstat object.
void pagestat_free(struct pagestat *ps);

// Free bulk-allocated pstat objects.
void pagestat_free_all(struct pagestat **pss);
