#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/userfaultfd.h>
#include <unistd.h>

#define TARGET_PTR ((void *)0x700000000000UL)

#define pfatal(prefix)				\
	do {					\
		perror(prefix);			\
		exit(EXIT_FAILURE);		\
	} while(0)

static long make_uffd(void)
{
	long uffd = syscall(__NR_userfaultfd, 0);
	struct uffdio_api api = {
		.api = UFFD_API,
		.features = 0,
	};

	if (uffd == -1)
		pfatal("syscall");

	if (ioctl(uffd, UFFDIO_API, &api) == -1)
		pfatal("UFFDIO_API");

	return uffd;
}

static void print_smaps(void)
{
	char cmd[255] = {};
	pid_t pid = getpid();

	sprintf(cmd, "cat /proc/%lu/maps | grep -E '^70'", pid);
	system(cmd);
}

static void register_range(long uffd, unsigned long addr, unsigned long len)
{
	struct uffdio_register reg = {
		.range = {
			.start = addr,
			.len = len,
		},
		.mode = UFFDIO_REGISTER_MODE_MISSING,
	};

	if (ioctl(uffd, UFFDIO_REGISTER, &reg) == -1)
		pfatal("UFFDIO_REGISTER");
}

static void unregister_range(long uffd, unsigned long addr, unsigned long len)
{
	struct uffdio_register reg = {
		.range = {
			.start = addr,
			.len = len,
		},
	};

	if (ioctl(uffd, UFFDIO_UNREGISTER, &reg) == -1)
		pfatal("UFFDIO_UNREGISTER");
}

int main(void)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	void *buf;
	long uffd;

	buf = mmap(TARGET_PTR, page_size * 20, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE | MAP_FIXED | MAP_POPULATE,
			 -1, 0);
	if (buf == MAP_FAILED)
		pfatal("mmap");

	uffd = make_uffd();

	register_range(uffd, (unsigned long)buf, page_size * 10);
	/* We should have 2 equally sized separate VMAs. */
	print_smaps();

	printf("---\n");
	unregister_range(uffd, (unsigned long)buf + page_size * 5, page_size * 5);
	/* First VMA should be 5 pages, the second 15. */
	print_smaps();


	return EXIT_SUCCESS;
}
