#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
//#include <linux/userfaultfd.h>
#include "userfaultfd.h"

// Fatal error has occurred with errno set to
// the error code.
#define pfatal(prefix)				\
	do {					\
		perror(prefix);			\
		exit(EXIT_FAILURE);		\
	} while(0)

// Fatal error has occurred.
#define fatal(msg, ...)							\
	do {								\
		fprintf(stderr, "error: " msg __VA_OPT__(,) __VA_ARGS__); \
		exit(EXIT_FAILURE);					\
	} while(0)

// Perform required application initialisation.
void init(void);

// Map a single page of anonymous memory.
void *map_page(bool populate);

// Unmap a single page of anonymous memory.
void unmap_page(void *ptr);

// Handle a userfaultfd event. This is the heart of the userfaultfd
// implementation and should be called as soon as a message is ready.
//
// Returns the new uffd in the case of a fork event.
unsigned handle_event(long uffd, struct uffd_msg* msg);

// Register userfaultfd handler in current process.
long make_handler(bool block);

// Register a range of pages. ptr will be page-aligned.
void register_page_range(long uffd, void *ptr, int num_pages);

// Register address range to be handled by userfaultfd.
void register_range(long uffd, void *ptr, unsigned long len);
