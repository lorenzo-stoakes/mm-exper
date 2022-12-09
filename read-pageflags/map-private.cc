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

static constexpr const char* test_path="test2.txt";
static constexpr auto delay = 100ms;

namespace
{
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

	page_state strip_dirty_flag() const
	{
		page_state ret = *this;

		ret.kpageflags &= ~(1UL << KPF_DIRTY);
		return ret;
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
			if (prev.strip_dirty_flag() != curr.strip_dirty_flag()) {
				print_kpageflags_virt((char *)strptr, "CHANGED shared ptr");
				prev = page_state(strptr);
			}

			curr_chr = curr_chr == 'z' ? 'a' : curr_chr + 1;
		}
	});

	// Allow time for first mmap() to occur.
	std::this_thread::sleep_for(100ms);

	std::cout << "[start updating via MAP_PRIVATE...]\n";

	std::thread t2([] {
		// While 'volatile' is often misused we explicitly use it here
		// to indicate to the compiler not to get 'smart' and try to
		// elide code but rather to take into account that what is
		// mapped might get updated by the OS (the very thing we are
		// investigating).
		decltype(auto) strptr = map_private(true);
		if (strptr == nullptr)
			throw std::runtime_error("can't map private");

		print_kpageflags_virt((char *)strptr, "1st private ptr");

		page_state prev(strptr);

		while (true) {
			// We only show if the file has changed.
			if (prev != page_state(strptr)) {
				print_kpageflags_virt((char *)strptr, "private read");
				prev = page_state(strptr);
			}

			std::this_thread::sleep_for(delay);
		}
	});

	t.join();

	return 0;
}
