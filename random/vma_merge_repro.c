#include <linux/userfaultfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	int uffd;
	void *addr;

	struct uffdio_api uffdio_api;
	struct uffdio_register uffdio_register;

	uffd = syscall(__NR_userfaultfd, 0x801ul);

	uffdio_api.api = UFFD_API;
	uffdio_api.features = 0;
	ioctl(uffd, UFFDIO_API, &uffdio_api);

	addr = mmap(NULL, 0x1000000ul, PROT_READ | PROT_WRITE,
		    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

	uffdio_register.range.start = (unsigned long)addr + 0x10000;
	uffdio_register.range.len = 0x2000;
	uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
	ioctl(uffd, UFFDIO_REGISTER, &uffdio_register);

	return 0;
}
