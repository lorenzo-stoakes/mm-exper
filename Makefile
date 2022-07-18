all: section-pointers test-musl-malloc read-pageflags

SHARED_HEADERS=include/bitwise.h

SHARED_OPTIONS=-g -Wall -Werror --std=gnu99 -I. -Iinclude/

section-pointers: section-pointers.c $(SHARED_HEADERS) Makefile
	gcc $(SHARED_OPTIONS) -no-pie section-pointers.c -o section-pointers

test-musl-malloc: test-musl-malloc.c musl/oldmalloc.c $(SHARED_HEADERS) Makefile
	gcc $(SHARED_OPTIONS) -Imusl/ -Wno-int-conversion -DDEBUG_OUTPUT \
		test-musl-malloc.c musl/oldmalloc.c -o test-musl-malloc
read-pageflags: read-pageflags.c Makefile
	gcc $(SHARED_OPTIONS) read-pageflags.c -o read-pageflags

clean:
	rm section-pointers test-musl-malloc

.PHONY: all clean
