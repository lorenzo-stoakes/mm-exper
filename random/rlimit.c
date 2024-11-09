#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>

int main() {
	struct rlimit lim;
	void *prev_brk;

	prev_brk = sbrk(0);
	if (prev_brk == (void *)-1) {
		perror("sbrk()");
		return EXIT_FAILURE;
	}

	prev_brk = sbrk(

	if (prev_brk == (void *)-1) {
		perror("sbrk()");
		return EXIT_FAILURE;
	}

	printf("prev brk = %p\n", prev_brk);
	prev_brk = sbrk(32);

	if (prev_brk == (void *)-1) {
		perror("sbrk()");
		return EXIT_FAILURE;
	}

	printf("prev brk = %p\n", prev_brk);

 	// set RLIMIT_AS for the processe's address space to 1 byte
	// This causes all future calls to sbrk to fail

	getrlimit(RLIMIT_AS, &lim);
	lim.rlim_cur = 1;
	printf("lim.rlim_max: %ld\n", lim.rlim_max);
	setrlimit(RLIMIT_AS, &lim);

	printf("Mallocing an additional 8 bytes, which requires more"
 	       "memory from sbrk, but sbrk SHOULD fail\n");
	void * ptr = sbrk(8);
	printf("sbrk result: %p\n", ptr);
	if (ptr != -1) {
		printf("sbrk unexpectedly passed\n");
	} else {
		printf("sbrk expectedly failed\n");
	}

	return EXIT_SUCCESS;
}
