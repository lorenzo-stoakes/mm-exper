all: section-pointers test-musl-malloc

SHARED_HEADERS=include/bitwise.h

SHARED_OPTIONS=-g -Wall --std=gnu99 -I. -Iinclude/

section-pointers: section-pointers.c $(SHARED_HEADERS) Makefile
	gcc $(SHARED_OPTIONS) -no-pie section-pointers.c -o section-pointers

test-musl-malloc: test-musl-malloc.c musl/oldmalloc.c $(SHARED_HEADERS) Makefile
	gcc $(SHARED_OPTIONS) -Imusl/ -Wno-int-conversion -DDEBUG_OUTPUT \
		test-musl-malloc.c musl/oldmalloc.c -o test-musl-malloc

clean:
	rm section-pointers test-musl-malloc

.PHONY: all clean
