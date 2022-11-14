all: example-mstat get-remote

SHARED_OPTIONS=-g -Wall -Werror --std=gnu99 -I.

example-mstat: memstat.h memstat.c example.c
	gcc $(SHARED_OPTIONS) memstat.c example.c -o example-mstat

get-remote: memstat.h memstat.c get-remote.c
	gcc $(SHARED_OPTIONS) memstat.c get-remote.c -o get-remote

clean:
	rm example-mstat get-remote

.PHONY: all clean
