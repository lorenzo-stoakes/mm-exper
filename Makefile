all: section-pointers test-musl-malloc read-pageflags pagestat

SHARED_HEADERS=include/bitwise.h

SHARED_OPTIONS=-g -Wall -Werror --std=gnu99 -I. -Iinclude/

section-pointers: section-pointers.c $(SHARED_HEADERS) Makefile
	gcc $(SHARED_OPTIONS) -no-pie section-pointers.c -o section-pointers

test-musl-malloc: test-musl-malloc.c musl/oldmalloc.c $(SHARED_HEADERS) Makefile
	gcc $(SHARED_OPTIONS) -Imusl/ -Wno-int-conversion -DDEBUG_OUTPUT \
		test-musl-malloc.c musl/oldmalloc.c -o test-musl-malloc

read-pageflags:
	make -C read-pageflags

pagestat:
	make -C pagestat

clean:
	rm -f section-pointers test-musl-malloc
	make -C read-pageflags clean
	make -C pagestat clean

.PHONY: all clean read-pageflags pagestat
