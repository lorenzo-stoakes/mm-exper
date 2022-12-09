all: read

read: read-pageflags.c Makefile
	gcc $(SHARED_OPTIONS) read-pageflags.c read.c -o read

clean:
	rm -f read

.PHONY: all clean
