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

// Print kpageflags for the page containing the specified pointer.
// Return value indicates whether succeeded.
bool print_kpageflags_virt(const void *ptr, const char *descr);
