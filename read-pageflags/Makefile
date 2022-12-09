all: read map-private

SHARED_OPTIONS=-g -Wall -Werror -I.


map-private: map-private.cc read-pageflags.c Makefile
	g++ --std=c++2b $(SHARED_OPTIONS) map-private.cc read-pageflags.c -o map-private

read: read-pageflags.c Makefile
	gcc --std=gnu99 $(SHARED_OPTIONS) read-pageflags.c read.c -o read

clean:
	rm -f read map-private

.PHONY: all clean
