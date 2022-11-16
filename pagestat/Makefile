all: example pagestat pagestat-watch

SHARED_OPTIONS=-g -Wall -Werror --std=gnu99 -I. -O2

example: pagestat.h pagestat.c example.c
	gcc $(SHARED_OPTIONS) pagestat.c example.c -o example

pagestat: pagestat.h pagestat.c pagestat-cmd.c
	gcc $(SHARED_OPTIONS) pagestat.c pagestat-cmd.c -o pagestat

pagestat-watch: pagestat.h pagestat.c watch-cmd.c
	gcc $(SHARED_OPTIONS) pagestat.c watch-cmd.c -o pagestat-watch

clean:
	rm -f example pagestat pagestat-watch

.PHONY: all clean
