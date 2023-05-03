#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main()
{
       int fd = memfd_create("test", MFD_ALLOW_SEALING);
       if (fd == -1) {
	       perror("memfd_create");
	       return EXIT_FAILURE;
       }

       if (ftruncate(fd, 4096)) {
	       perror("ftruncate");
	       return EXIT_FAILURE;
       }

       char *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_POPULATE, fd, 0);
       if (ptr == MAP_FAILED) {
	       perror("mmap");
	       return EXIT_FAILURE;
       }

       ptr[3] = 'x';

       if (madvise(ptr, 4096, MADV_REMOVE)) {
	       perror("madvise");
	       return EXIT_FAILURE;
       }

	printf("%c\n", ptr[3]);

       return EXIT_SUCCESS;
}
