section-pointers: section-pointers.c
	gcc -no-pie -Wall --std=gnu99 section-pointers.c -o section-pointers

all: section-pointers

clean:
	rm section-pointers

.PHONY: all clean
