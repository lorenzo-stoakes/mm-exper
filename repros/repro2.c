#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/userfaultfd.h>

/*
 * r0 = userfaultfd(0x0)
 * ioctl$UFFDIO_API(r0, 0xc018aa3f, &(0x7f0000000080))
 * ioctl$UFFDIO_REGISTER(r0, 0xc020aa00, &(0x7f00000002c0)={{&(0x7f0000400000/0xc00000)=nil, 0xc00000}, 0x2})
 * ioctl$UFFDIO_UNREGISTER(r0, 0x8010aa01, &(0x7f0000000100)={&(0x7f00008aa000/0x4000)=nil, 0x4000})

 */

int main(void)
{
	long uffd = syscall(__NR_userfaultfd, 0);
	struct uffdio_api api = {
		.api = UFFD_API,
		.features = 0,
	};
	struct uffdio_register reg = {
		.range = {
			.start = 0x20400000,
			.len = 0xc00000,
		},
		.mode = 2,
	};
	struct uffdio_range unreg_range = {
		.start = 0x208aa000,
		.len = 0x4000,
	};

	if (mmap((void *)0x1ffff000ul, 0x1000ul, PROT_NONE,
		 MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0) == MAP_FAILED) {
		perror("mmap 1");
		return EXIT_FAILURE;
	}

	if (mmap((void *)0x20000000ul, 0x1000000ul, PROT_WRITE|PROT_READ|PROT_EXEC,
		 MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0) == MAP_FAILED) {
		perror("mmap 2");
		return EXIT_FAILURE;
	}

	if (mmap((void *)0x21000000ul, 0x1000ul, PROT_NONE,
		 MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0) == MAP_FAILED) {
		perror("mmap 3");
		return EXIT_FAILURE;
	}

	if (uffd == -1) {
		perror("uffd");
		return EXIT_FAILURE;
	}

	if (ioctl(uffd, UFFDIO_API, &api) == -1) {
		perror("ioctl 1");
		return EXIT_FAILURE;
	}

	if (ioctl(uffd, UFFDIO_REGISTER, &reg) == -1) {
		perror("ioctl 2");
		return EXIT_FAILURE;
	}

	if (ioctl(uffd, UFFDIO_UNREGISTER, &unreg_range) == -1) {
		perror("ioctl 3");
		return EXIT_FAILURE;
	}

	close(uffd);

	return EXIT_SUCCESS;
}
