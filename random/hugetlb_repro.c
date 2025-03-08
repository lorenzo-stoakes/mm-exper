#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define MMAP_FLAGS_COMMON (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB)

int main()
{
        size_t len = 2ULL << 30;
	int i;

        void *a = mmap(
                (void *)0x7c8000000000, len, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANON |  MAP_FIXED_NOREPLACE, -1, 0);
        printf("a=%p errno %d %m\n", a, errno);
        errno = 0;

	for (i = 0; i < 2048; i++) {
		char *ptr = &((char *)a)[i * 4096];

		*ptr = 'x';
	}

        char buf[128];
        sprintf(buf, "cp /proc/%d/smaps smaps1", getpid());
        assert(system(buf) == 0);

        len = 4096;
        void *b = mmap(
                a, len, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANON | MAP_FIXED | MAP_HUGETLB, -1, 0);
        printf("b=%p errno %d %m\n", b, errno);
        errno = 0;

        sprintf(buf, "cp /proc/%d/smaps smaps2", getpid());
        assert(system(buf) == 0);

        return 0;
}
