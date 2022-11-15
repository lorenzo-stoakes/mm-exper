all: example-mstat memstat memstat-watch

SHARED_OPTIONS=-g -Wall -Werror --std=gnu99 -I. -O2

example-mstat: memstat.h memstat.c example.c
	gcc $(SHARED_OPTIONS) memstat.c example.c -o example-mstat

memstat: memstat.h memstat.c memstat-cmd.c
	gcc $(SHARED_OPTIONS) memstat.c memstat-cmd.c -o memstat

memstat-watch: memstat.h memstat.c watch-cmd.c
	gcc $(SHARED_OPTIONS) memstat.c watch-cmd.c -o memstat-watch

clean:
	rm example-mstat memstat memstat-watch

.PHONY: all clean
