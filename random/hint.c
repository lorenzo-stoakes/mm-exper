#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

int reserve(void* addr, size_t size) {
	void* const res = mmap(addr, size, PROT_NONE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_NORESERVE, -1, 0);
	if (res == MAP_FAILED) {
		// Failed to reserve memory
		return 0;
	}

	if (res != addr) {
		// Failed to reserve memory at the requested address
		munmap(res, size);
		return 0;
	}

	// Success
	return 1;
}

void reserve_or_exit(void* addr, size_t size) {
	if (!reserve(addr, size)) {
		fprintf(stderr, "Failed to reserve %p %p\n", addr, (char*)addr + size);
		exit(-1);
	}
}

const size_t K = 1024;
const size_t M = 1024 * K;

char* base = (char*) 0x40000000000ull;

int main() {
	// This order succeeds
	// reserve_or_exit(base,         4 * M);
	// reserve_or_exit(base + 4 * M, 2 * M);
	// reserve_or_exit(base + 6 * M, 2 * M);


	// But this fails
	reserve_or_exit(base,         4 * M);
	reserve_or_exit(base + 6 * M, 2 * M);
	reserve_or_exit(base + 4 * M, 2 * M); // Fails here

	return 0;
}
