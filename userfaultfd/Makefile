all: server client simple

SHARED_HEADERS=shared.h userfaultfd.h
SHARED_SOURCE=shared.c
SHARED=$(SHARED_HEADERS) $(SHARED_SOURCE) Makefile

SHARED_OPTIONS=-g -Wall -Werror --std=gnu99 -I. -Iinclude/ -fcommon

server: server.c $(SHARED)
	gcc $(SHARED_OPTIONS) $(SHARED_SOURCE) server.c -o server

client: client.c $(SHARED)
	gcc $(SHARED_OPTIONS) $(SHARED_SOURCE) client.c -o client

simple: simple.c $(SHARED)
	gcc $(SHARED_OPTIONS) $(SHARED_SOURCE) -lpthread simple.c -o simple

clean:
	rm -f server client simple

.PHONY: all clean
