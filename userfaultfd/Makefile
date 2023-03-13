all: server client simple

SHARED_HEADERS=shared.h
SHARED_SOURCE=shared.c
SHARED=$(SHARED_HEADERS) $(SHARED_SOURCE)

SHARED_OPTIONS=-g -Wall -Werror --std=gnu99 -I. -Iinclude/

server: server.c $(SHARED) Makefile
	gcc $(SHARED_OPTIONS) $(SHARED_SOURCE) server.c -o server

client: client.c $(SHARED) Makefile
	gcc $(SHARED_OPTIONS) $(SHARED_SOURCE) client.c -o client

simple: simple.c $(SHARED) Makefile
	gcc $(SHARED_OPTIONS) $(SHARED_SOURCE) -lpthread simple.c -o simple

clean:
	rm -f server client simple

.PHONY: all clean
