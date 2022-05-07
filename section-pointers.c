#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <unistd.h>

/*
 * Output the ultimate locations of sections and the program break. Hook the
 * libc main to ensure we get the original break.
 *
 * The code disables ASLR and is intende dto be built with `-no-pie` in order to
 * give more sensible addresses (avoiding the 2/3 address space offset discussed
 * at https://stackoverflow.com/a/51343797) and to avoid the random offsets of
 * ASLR muddying the waters.
 */

// These symbols are exported by the default linker configuration.
extern char __bss_start;
extern char data_start;

// Remove ASLR as we're interested in the layout of 'unslid' virtual addresses.
static void remove_aslr(char **argv)
{
	const int curr_personality = personality(ADDR_NO_RANDOMIZE);

	// Nothing to do, ASLR is already disabled.
	if ((curr_personality & ADDR_NO_RANDOMIZE) == ADDR_NO_RANDOMIZE)
		return;

	if (curr_personality == -1) {
		perror("Unable to change personality");
		exit(EXIT_FAILURE);
	}

	const int persona = personality(0xffffffff);
	if ((persona & ADDR_NO_RANDOMIZE) != ADDR_NO_RANDOMIZE) {
		const char *err_msg = "Could not obtain current personality";

		if (persona == -1) {
			perror(err_msg);
		} else {
			fprintf(stderr, err_msg);
			fprintf(stderr, "\n");
		}

		exit(EXIT_FAILURE);
	}

	// Now overwrite the process image with an ASLR-disabled personality.
	execv(argv[0], argv);
}

int main(int argc, char **argv)
{
	remove_aslr(argv);

	static int foo = 3; // Intentionally not const so it lives in .data not .rodata
	const void *orig_brk = sbrk(0);
	const int *ptr = malloc(1);
	const void *brk_after_ptr = sbrk(0);
	const void *mmap_ptr = mmap(NULL, 1024 * 1024, PROT_READ | PROT_WRITE,
				    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	const void *stack_ptr = __builtin_frame_address(0);
	const uint64_t stack_mmap_delta_mb =
		((uint64_t)stack_ptr - (uint64_t)mmap_ptr) >> 20;

	printf("%p\t[&main]\n", &main);
	printf("%p\t[data_start]\n", &data_start);
	printf("%p\t[static variable]\n", &foo);
	printf("%p\t[__bss_start]\n", &__bss_start);
	printf("%p\t[orig brk]\n", orig_brk);
	printf("%p\t[malloc'd val]\n", ptr);
	printf("%p\t[brk]\n", brk_after_ptr);
	printf("%p\t[stack]\n", stack_ptr);
	printf("%p\t[mmap] (%lu MiB below stack)\n", mmap_ptr, stack_mmap_delta_mb);

	return EXIT_SUCCESS;
}
