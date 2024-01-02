#pragma once

#include <stdbool.h>
#include <stdint.h>

#define INVALID_VALUE (~(uint64_t)0)

// Represents data retrieved from /proc/$pid/maps.
struct map_data {
	// VMA permissions.
	char perms[5];

	uint64_t offset;
	uint64_t inode;
	char name[255];
};

// Reads from /proc/self/pagemap to retrieve information on virtual mapping.
// See https://www.kernel.org/doc/Documentation/vm/pagemap.txt for details.
// If unable to retrieve, returns INVALID_VALUE.
uint64_t read_pagemap(const void *ptr);

// Reads PFN from pagemap if present.
// If unable to retrieve, returns INVALID_VALUE.
uint64_t read_pfn(const void *ptr);

// Convert pagemap value to PFN.
// If unable to retrieve, returns INVALID_VALUE.
uint64_t extract_pfn(uint64_t val);

// Print flags for the page containing the specified pointer.
// Return value indicates whether succeeded.
bool print_flags_virt(const void *ptr, const char *descr);

// Same as print_flags_virt(), only we have already obtained values from
// procfs.
bool print_flags_virt_precalc(const void *ptr, uint64_t pagemap, uint64_t pfn,
			      uint64_t kpageflags, uint64_t mapcount,
			      const struct map_data* mapfields,
			      const char *descr);

// Prints flags for a physical PFN.
void print_flags_phys(uint64_t pfn, const char *descr);

// Retrieves kpageflags as described at
// https://www.kernel.org/doc/html/latest/admin-guide/mm/pagemap.html for the
// specified physical page at PFN `pfn`.
// If unable to retrieve, returns INVALID_VALUE.
uint64_t read_kpageflags(uint64_t pfn);

// Retrieves page map count as described at
// https://www.kernel.org/doc/html/latest/admin-guide/mm/pagemap.html for the
// specified physical page at PFN `pfn`.
// If unable to retrieve, returns INVALID_VALUE.
uint64_t read_mapcount(uint64_t pfn);

// Read data from /proc/$pid/maps and place in out. Returns true if succeeded,
// false otherwise.
bool read_mapdata(const void *ptr,
		  struct map_data *out);
