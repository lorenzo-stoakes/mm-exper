#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "linux/kernel-page-flags.h"

#include "read-pageflags.h"

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

#define MASK_FLAGS
static constexpr const char* test_path = "test2.txt";
static constexpr auto delay = 100ms;

namespace
{
// While 'volatile' is often misused we explicitly use it here to indicate to
// the compiler not to get 'smart' and try to elide code but rather to take into
// account that what is mapped might get updated by the OS (the very thing we
// are investigating).

volatile char* map_shared(bool populate)
{
	const int fd = open("test2.txt", O_RDWR);

	void *ret = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
			 MAP_SHARED | (populate ? MAP_POPULATE : 0), fd, 0);
	::close(fd);

	return ret == MAP_FAILED ? nullptr : (volatile char*)ret;
}

volatile char* map_private(bool populate)
{
	const int fd = open("test2.txt", O_RDWR);

	void *ret = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | (populate ? MAP_POPULATE : 0), fd, 0);
	close(fd);

	return ret == MAP_FAILED ? nullptr : (volatile char*)ret;
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
} // namespace

struct page_state {
	explicit page_state(const void* ptr)
		: ptr{ptr}
		, pagemap{read_pagemap(ptr)}
		, pfn{extract_pfn(pagemap)}
	{
		if (pfn == INVALID_VALUE) {
			kpageflags = 0;
			mapcount = 0;
			return;
		}

		kpageflags = read_kpageflags(pfn);
		mapcount = read_mapcount(pfn);
	}

	explicit page_state(volatile char* strptr)
		: page_state((const void *)strptr)
	{
	}

	bool operator==(const page_state& that)
	{
		return pagemap == that.pagemap &&
			pfn == that.pfn &&
			kpageflags == that.kpageflags &&
			mapcount == that.mapcount;
	}

	const void* ptr;
	uint64_t pagemap;
	uint64_t pfn;
	uint64_t kpageflags;
	uint64_t mapcount;

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
		print_kpageflags_virt_precalc(ptr, pagemap, pfn,
					      kpageflags, mapcount,
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

	std::cout << "[start updating via MAP_SHARED...]\n";
	std::thread t([] {
		decltype(auto) strptr = map_shared(true);
		if (strptr == nullptr)
			throw std::runtime_error("can't map shared");

		print_kpageflags_virt((char *)strptr, "shared ptr");

		page_state prev(strptr);

		char curr_chr = 'a';

		while (true) {
			strptr[0] = curr_chr;
			std::this_thread::sleep_for(delay);

			page_state curr(strptr);
			// We mask out some common flag changes to reduce noise.
			if (prev.masked() != curr.masked()) {
				curr.print("CHANGED shared ptr");
				prev = curr;
			}

			curr_chr = curr_chr == 'z' ? 'a' : curr_chr + 1;
		}
	});

	// Allow time for first mmap() to occur.
	std::this_thread::sleep_for(100ms);

	std::cout << "[start updating via MAP_PRIVATE...]\n";

	std::thread t2([] {
		decltype(auto) strptr = map_private(true);
		if (strptr == nullptr)
			throw std::runtime_error("can't map private");

		print_kpageflags_virt((char *)strptr, "1st private ptr");

		page_state prev(strptr);

		while (true) {
			page_state curr(strptr);
			if (prev != curr) {
				curr.print("CHANGED private ptr");
				prev = curr;
			}

			std::this_thread::sleep_for(delay);
		}
	});

	t.join();

	return 0;
}
