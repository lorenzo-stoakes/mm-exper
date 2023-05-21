#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static void print_vmstat(void)
{
	char cmd[255];
	pid_t pid = getpid();

	sprintf(cmd, "grep -E \"VmSize|VmRSS\" /proc/%lu/status", pid);
	system(cmd);
}

int main(void)
{
	long page_size = sysconf(_SC_PAGESIZE);
	long size = 1024 * 1024 * 1024 * page_size;

	int fd = open("test.txt", O_RDWR);

	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	print_vmstat();

	char *ptr_file = mmap(NULL, size, PROT_READ | PROT_WRITE,
			      MAP_SHARED | MAP_NORESERVE, fd, 0);

	if (ptr_file == MAP_FAILED) {
		perror("mmap file");
		return EXIT_FAILURE;
	}

	char *ptr_anon = mmap(NULL, size, PROT_READ | PROT_WRITE,
			      MAP_ANON | MAP_PRIVATE | MAP_NORESERVE, -1, 0);

	if (ptr_anon == MAP_FAILED) {
		perror("mmap file");
		return EXIT_FAILURE;
	}

	printf("---\n");
	print_vmstat();

	// Shared mappings are not accountable.

	ptr_file = mmap(NULL, size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);
	if (ptr_file == MAP_FAILED) {
		fprintf(stderr, "file mapping failed check?\n");
	}

	ptr_anon = mmap(NULL, size, PROT_READ | PROT_WRITE,
			MAP_ANON | MAP_PRIVATE, -1, 0);
	if (ptr_anon != MAP_FAILED) {
		fprintf(stderr, "file mapping worked for massive size?\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
