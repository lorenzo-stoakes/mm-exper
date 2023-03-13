#include "shared.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

// Align to a page, rounding down.
#define PAGE_ALIGN(addr) ((typeof(addr))(((unsigned long)addr) & ~(page_size - 1)))

#define REQUIRED_FEATURES (			\
		UFFD_FEATURE_THREAD_ID |	\
		UFFD_FEATURE_EVENT_FORK |	\
		UFFD_FEATURE_EVENT_REMAP |	\
		UFFD_FEATURE_EVENT_REMOVE)

static int page_size;

void init(void)
{
	page_size = sysconf(_SC_PAGE_SIZE);
}

void *map_page(bool populate)
{
	void *ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			 MAP_ANON | MAP_PRIVATE |
			 (populate ? MAP_POPULATE : 0), -1, 0);
	if (ptr == MAP_FAILED)
		pfatal("mmap");

	return ptr;
}

void unmap_page(void *ptr)
{
	if (munmap(ptr, page_size))
		pfatal("munmap");
}

static void handle_fault(long uffd, struct uffd_msg* msg)
{
	printf("-- page fault uffd=[%ld] --\n", uffd);

	printf("flags=%llx addr=%llx pid=%u\n",
	       msg->arg.pagefault.flags, msg->arg.pagefault.address, msg->arg.pagefault.feat.ptid);

	void *ptr = map_page(true);

	struct uffdio_copy copy = {
		.mode = 0,
		.copy = 0,
		.src = (unsigned long)ptr,
		.dst = PAGE_ALIGN(msg->arg.pagefault.address),
		.len = page_size,
	};

	if (ioctl(uffd, UFFDIO_COPY, &copy))
		pfatal("fault ioctl");
}

static unsigned handle_fork(long uffd, struct uffd_msg* msg)
{
	printf("-- fork uffd=[%ld] --\n", uffd);

	printf("Child uffd=[%u]\n", msg->arg.fork.ufd);

	return msg->arg.fork.ufd;
}

static void handle_remap(long uffd, struct uffd_msg* msg)
{
	printf("-- remap uffd=[%ld] --\n", uffd);

	(void)msg;
}

static void handle_remove(long uffd, struct uffd_msg* msg)
{
	printf("-- remove uffd=[%ld] --\n", uffd);

	(void)msg;
}

static void handle_unmap(long uffd, struct uffd_msg* msg)
{
	printf("-- unmap uffd=[%lx] --\n", uffd);

	(void)msg;
}

unsigned handle_event(long uffd, struct uffd_msg* msg)
{
	if (msg == NULL)
		return 0;

	switch (msg->event) {
	case UFFD_EVENT_PAGEFAULT:
		handle_fault(uffd, msg);
		break;
	case UFFD_EVENT_FORK:
		return handle_fork(uffd, msg);
	case UFFD_EVENT_REMAP:
		handle_remap(uffd, msg);
		break;
	case UFFD_EVENT_REMOVE:
		handle_remove(uffd, msg);
		break;
	case UFFD_EVENT_UNMAP:
		handle_unmap(uffd, msg);
		break;
	default:
		fatal("Unrecognised event %u", msg->event);
		return 0; // Should be unreachable.
	}

	return 0;
}

long make_handler(bool block)
{
	long uffd = syscall(__NR_userfaultfd, block ? 0 : O_NONBLOCK);
	if (uffd == -1)
		pfatal("syscall");

	struct uffdio_api api = {
		.api = UFFD_API,
		.features = REQUIRED_FEATURES,
	};

	if (ioctl(uffd, UFFDIO_API, &api) == -1)
		pfatal("UFFDIO_API");

	return uffd;
}

void register_range(long uffd, void *ptr, unsigned long len)
{
	struct uffdio_register reg = {
		.range = {
			.start = (unsigned long)ptr,
			.len = len,
		},
		.mode = UFFDIO_REGISTER_MODE_MISSING | UFFDIO_REGISTER_MODE_WP,
	};

	if (ioctl(uffd, UFFDIO_REGISTER, &reg) == -1)
		pfatal("UFFDIO_REGISTER");
}

void register_page_range(long uffd, void *ptr, int num_pages)
{
	register_range(uffd, PAGE_ALIGN(ptr), num_pages * page_size);
}
