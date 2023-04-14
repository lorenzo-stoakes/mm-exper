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

       write(fd, "test", 4);

       if (fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE) == -1) {
	       perror("fcntl");
	       return EXIT_FAILURE;
       }

       void *ret = mmap(NULL, 4, PROT_READ, MAP_SHARED, fd, 0);
       if (ret == MAP_FAILED) {
	       perror("mmap");
	       return EXIT_FAILURE;
       }

       return EXIT_SUCCESS;
}
