#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "linux/kernel-page-flags.h"

#include "read-pageflags.h"

// Set to MAP_POPULATE the private mapping.
//#define POPULATE_PRIVATE

// Set to periodically write to private mapping.
#define WRITE_PRIVATE_MAPPING

// Set to observe changes to private mapping buffer.
//#define SHOW_PRIVATE_CHANGES

// Set to mask uninteresting folio flags for monitoring purposes, e.g. we
// probably don't care for the purposes of this investigation whether a folio
// moves from the inactive to the active list or vice-versa.
#define MASK_FLAGS

// Set to explicitly read from the private buffer to fault in.
#define READ_FAULT_PRIVATE

/*
 * -- MAP_PRIVATE experiment --
 *
 * The mmap() man page states:-
 *
 *     MAP_PRIVATE
 *         Create a private copy-on-write mapping.  Updates to the mapping are
 *         not visible to other processes mapping the same file, and are not
 *         carried through to the underlying file.  It is unspecified whether
 *         changes made to the file after the mmap() call are visible in the
 *         mapped region.
 *
 * This code is part of an investigation into under precisely what circumstances
 * an mmap() of a FILE with MAP_PRIVATE set gets updates.
 *
 * A test file is opened and mmap()'d in 2 separate threads - the first a shared
 * memory thread which
 */

using namespace std::chrono_literals;

static constexpr const char* test_path = "test2.txt";
static constexpr auto delay = 500ms;

namespace
{
char* map_shared(bool populate)
{
	const int fd = open(test_path, O_RDWR);

	void *ret = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
			 MAP_SHARED | (populate ? MAP_POPULATE : 0), fd, 0);
	close(fd);

	return ret == MAP_FAILED ? nullptr : ( char*)ret;
}

char* map_private(bool populate)
{
	const int fd = open(test_path, O_RDWR);

	void *ret = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | (populate ? MAP_POPULATE : 0), fd, 0);
	close(fd);

	return ret == MAP_FAILED ? nullptr : ( char*)ret;
}

void setup_file()
{
	namespace fs = std::filesystem;

	if (fs::exists(test_path))
		return;

	std::ofstream oss;
	oss.open(test_path);
	oss << "foo\n";
	oss.close();

	fs::permissions(test_path, fs::perms::all);
}

char next_char(char chr)
{
	if (chr < 'a' || chr > 'z')
		return 'a';

	return chr == 'z' ? 'a' : chr + 1;
}
} // namespace

struct page_state {
	explicit page_state(char* ptr)
		: ptr{ptr}
		, pagemap{read_pagemap((const void *)ptr)}
		, pfn{extract_pfn(pagemap)}
	{
		if (pfn == INVALID_VALUE) {
			kpageflags = 0;
			mapcount = 0;
			return;
		}

		kpageflags = read_kpageflags(pfn);
		mapcount = read_mapcount(pfn);
		if (!read_mapdata((const void*)ptr, &mapfields))
			memset(&mapfields, 0, sizeof(map_data));
	}

	bool operator==(const page_state& that)
	{
		return pagemap == that.pagemap &&
			pfn == that.pfn &&
			kpageflags == that.kpageflags &&
			mapcount == that.mapcount;
	}

	bool operator!=(const page_state& that)
	{
		return !operator==(that);
	}

	char* ptr;
	uint64_t pagemap;
	uint64_t pfn;
	uint64_t kpageflags;
	uint64_t mapcount;
	map_data mapfields;

#ifdef MASK_FLAGS
	// Mask out flags that simply add noise e.g. dirty page flag.
	page_state masked() const
	{
		if (kpageflags == INVALID_VALUE)
			return *this;

		page_state ret = *this;

		ret.kpageflags &= ~(1UL << KPF_DIRTY);
		ret.kpageflags &= ~(1UL << KPF_ACTIVE);
		ret.kpageflags &= ~(1UL << KPF_REFERENCED);
		ret.kpageflags &= ~(1UL << KPF_WRITEBACK);
		ret.kpageflags &= ~(1UL << KPF_LRU);

		return ret;
	}
#else
	page_state masked() const
	{
		return *this;
	}
#endif

	void print(const char *descr) const
	{
		print_flags_virt_precalc((const void*)ptr, pagemap, pfn,
					 kpageflags, mapcount, &mapfields,
					 descr);
	}
};

int main()
{
	setup_file();

	if (getuid() != 0) {
		std::cerr << "ERROR: Must be run as root.\n";

		return 1;
	}

	std::thread t([] {
		decltype(auto) strptr = map_shared(true);
		if (strptr == nullptr)
			throw std::runtime_error("can't map shared");

		print_flags_virt((char *)strptr, "SHARED first ptr");

		page_state prev(strptr);

		char curr_chr = 'a';
		int count = 0;

		while (true) {
			strptr[0] = curr_chr;
			std::this_thread::sleep_for(delay);

			page_state curr(strptr);
			// We mask out some common flag changes to reduce noise.
			if (prev.masked() != curr.masked()) {
				curr.print("SHARED changed ptr");
				prev = curr;
			}

			if (msync((void *)strptr, 4096, MS_INVALIDATE) != 0)
				perror("msync");

			curr_chr = next_char(curr_chr);
			count++;
		}
	});

	// Allow time for first mmap() to occur.
	std::this_thread::sleep_for(100ms);

	std::thread t2([] {
		const bool populate =
#ifdef POPULATE_PRIVATE
			true
#else
			false
#endif
			;

		decltype(auto) strptr = map_private(populate);

		if (strptr == nullptr)
			throw std::runtime_error("can't map private");

		print_flags_virt((char *)strptr, "PRIVATE first ptr");

		page_state prev(strptr);

		int count = 0;

#ifdef SHOW_PRIVATE_CHANGES
		std::string prev_buf;
#endif

		while (true) {
			page_state pre_read_fault(strptr);

#ifdef READ_FAULT_PRIVATE
			// We may need to fault the page back in, so access it.
			((volatile char *)strptr)[0];
#endif

			page_state curr(strptr);

			if (prev.masked() != curr.masked()) {
#ifdef WRITE_PRIVATE_MAPPING
				// Output what was written given we triggered a change.
				if (count != 0 && count % 10 == 0)
					std::cout << "PRIVATE write\n";
#endif

#ifdef READ_FAULT_PRIVATE
				if (pre_read_fault.masked() != curr.masked())
					std::cout << "PRIVATE read\n";
#endif

				curr.print("PRIVATE changed ptr");
				prev = curr;
			}

#ifdef SHOW_PRIVATE_CHANGES
			if (prev_buf != (char *)strptr) {
				std::cout << "Buffer changed: " << (char *)strptr;
				std::cout.flush(); // buffer should have newline, flush in case not.
				prev_buf = (char *)strptr;
			}
#endif

			std::this_thread::sleep_for(delay);

			count++;
#ifdef WRITE_PRIVATE_MAPPING
			// Write every 10 * interval (5s by default).
			if (count % 10 == 0) {
// We only want to be reading if we have already read-faulted in.
#ifdef READ_FAULT_PRIVATE
				const char chr = strptr[1];
				const char next = next_char(chr);

				strptr[1] = next;
#ifdef SHOW_PRIVATE_CHANGES
				prev_buf[1] = next;
#endif
#else
				strptr[1] = 'y';
#ifdef SHOW_PRIVATE_CHANGES
				prev_buf[1] = 'y';
#endif
#endif
			}
#endif
		}
	});

	t.join();

	return 0;
}
