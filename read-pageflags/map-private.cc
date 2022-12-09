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

namespace
{

static constexpr const char* test_path="test2.txt";

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

int main()
{
	setup_file();

	if (getuid() != 0) {
		std::cerr << "ERROR: Must be run as root.\n";

		return 1;
	}

	using namespace std::chrono_literals;

	static const auto delay = 100ms;

	std::cout << "[start updating via MAP_SHARED...]\n";
	std::thread t([] {
		decltype(auto) strptr = map_shared(true);
		if (strptr == nullptr)
			throw std::runtime_error("can't map shared");

		print_kpageflags_virt((char *)strptr, "shared ptr");

		char curr = 'a';

		while (true) {
			strptr[0] = curr;
			std::this_thread::sleep_for(delay);

			curr = curr == 'z' ? 'a' : curr + 1;
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

		std::string prev = (char *)strptr;

		while (true) {
			// We only show if the file has changed.
			if (prev != (char *)strptr) {
				print_kpageflags_virt((char *)strptr, "private read");
				std::cout << (char *)strptr;
				prev = (char *)strptr;
			}

			std::this_thread::sleep_for(delay);
		}
	});

	t.join();

	return 0;
}
