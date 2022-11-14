all: example-mstat memstat

SHARED_OPTIONS=-g -Wall -Werror --std=gnu99 -I. -O2

example-mstat: memstat.h memstat.c example.c
	gcc $(SHARED_OPTIONS) memstat.c example.c -o example-mstat

memstat: memstat.h memstat.c memstat-cmd.c
	gcc $(SHARED_OPTIONS) memstat.c memstat-cmd.c -o memstat

clean:
	rm example-mstat memstat

.PHONY: all clean
