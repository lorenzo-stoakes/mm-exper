#pragma once

#include <stdbool.h>
#include <stdint.h>

#define INVALID_VALUE (~(uint64_t)0)

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

// Print kpageflags for the page containing the specified pointer.
// Return value indicates whether succeeded.
bool print_kpageflags_virt(const void *ptr, const char *descr);

// Same as print_kpageflags_virt(), only we have already obtained values from
// procfs.
bool print_kpageflags_virt_precalc(const void *ptr, uint64_t pagemap, uint64_t pfn,
				   uint64_t kpageflags, uint64_t mapcount,
				   const char *descr);

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
