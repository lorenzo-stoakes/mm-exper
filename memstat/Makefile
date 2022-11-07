all: example-mstat

SHARED_OPTIONS=-g -Wall -Werror --std=gnu99 -I.

example-mstat: memstat.h memstat.c example.c
	gcc $(SHARED_OPTIONS) memstat.c example.c -o example-mstat

clean:
	rm example-mstat

.PHONY: all clean
