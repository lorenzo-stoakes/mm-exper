all: section-pointers test-musl-malloc

section-pointers: section-pointers.c
	gcc -g -no-pie -Wall --std=gnu99 section-pointers.c -o section-pointers

test-musl-malloc: test-musl-malloc.c musl/oldmalloc.c
	gcc -g -I. -Imusl/ -Wall --std=gnu99 -Wno-parentheses -Wno-int-conversion \
		test-musl-malloc.c musl/oldmalloc.c -o test-musl-malloc

clean:
	rm section-pointers test-musl-malloc

.PHONY: all clean
