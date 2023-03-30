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

       pid_t pid = fork();
       if (pid == -1) {
	       perror("fork");
	       return EXIT_FAILURE;
       }

       if (pid == 0) {
	       void *ret = mmap(NULL, 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	       if (ret == MAP_FAILED) {
		       perror("mmap");
		       return EXIT_FAILURE;
	       }
       } else {
	       while (1)
		       ;
       }

       return EXIT_SUCCESS;
}
