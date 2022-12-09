#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace fs = std::filesystem;

static constexpr const char* test_path = "test2.txt";

namespace
{
void overwrite_file()
{
	std::ofstream oss;
	oss.open(test_path);
	oss << "foo\n";
	oss.close();
}

void append_file()
{
	std::ofstream oss;
	oss.open(test_path, std::ios_base::app);
	oss << "foo\n";
	oss.close();
}

void seek_update_file()
{
	FILE *fp = fopen(test_path, "r+");
	fseek(fp, 1, SEEK_CUR);
	fputc('y', fp);
	fclose(fp);
}

void shared_mmap()
{
	const int fd = open(test_path, O_RDWR);

	auto* chr = (char *)mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
				 MAP_SHARED | MAP_POPULATE, fd, 0);
	close(fd);

	chr[2] = chr[2] == 'z' ? 'a' : chr[2] + 1;

	// unmap should handle this, but for the sake of absolutely making the
	// point, sync it too.
	msync((void *)chr, 4096, MS_INVALIDATE);
	munmap((void *)chr, 4096);
}
} // namespace

int main(int argc, char **argv)
{
	if (argc < 2) {
		std::cerr << "usage: " << argv[0] << "[overwrite, mmap, append, seek]\n";
		return 1;
	}

	const std::string choice = argv[1];
	if (choice == "overwrite") {
		overwrite_file();
	} else if (choice == "mmap") {
		shared_mmap();
	} else if (choice == "append") {
		append_file();
	} else if (choice == "seek") {
		seek_update_file();
	} else {
		std::cerr << "Unrecognised choice [" << choice << "]\n";
		return 1;
	}

	return 0;
}
