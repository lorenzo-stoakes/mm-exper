#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Output the ultimate locations of sections and the program break. Hook the
 * libc main to ensure we get the original break.
 *
 * This is intended to be executed under `setarch -R` and to be built with
 * `-no-pie` in order to give more sensible addresses (avoiding the 2/3 address
 * space offset discussed at https://stackoverflow.com/a/51343797) and to avoid
 * ASLR mudding the waters.
 */

// These symbols are exported by the default linker configuration.
extern char __bss_start;
extern char data_start;

// Store the _original_ program break.
static void *orig_brk;

// Hook the libc main so we get a clean unaltered break value.
int __libc_start_main(int (*main)(int, char **, char **),
		      int argc,
		      char **argv,
		      int (*init)(int, char **, char **),
		      void (*fini)(void),
		      void (*rtld_fini)(void),
		      void *stack_end)
{
	// Hope & pray this doesn't need libc to actually be initialised by the
	// original __libc_start_main().
	orig_brk = sbrk(0);

	// Load the actual libc start.
	typeof(&__libc_start_main) orig = dlsym(RTLD_NEXT, "__libc_start_main");

	// Now invoke it.
	return orig(main, argc, argv, init, fini, rtld_fini, stack_end);
}

int main(void)
{
	static int foo = 3;
	int *ptr = malloc(1);
	void *brk_after_ptr = sbrk(0);

	printf("%p\t[&main]\n", &main);
	printf("%p\t[data_start]\n", &data_start);
	printf("%p\t[static variable]\n", &foo);
	printf("%p\t[__bss_start]\n", &__bss_start);
	printf("%p\t[orig brk]\n", orig_brk);
	printf("%p\t[malloc'd val]\n", ptr);
	printf("%p\t[brk]\n", brk_after_ptr);
	printf("%p\t[stack]\n", __builtin_frame_address(0));

	return 0;
}
