all: forky forky2 file_rmap file_rmap2 merge_split split_vma vma

SHARED_OPTIONS=-g -Wall -Werror --std=gnu99 -I.

forky: forky.c
	gcc $(SHARED_OPTIONS) -o forky forky.c

forky2: forky2.c
	gcc $(SHARED_OPTIONS) -o forky2 forky2.c

file_rmap: file_rmap.c
	gcc $(SHARED_OPTIONS) -o file_rmap file_rmap.c

file_rmap2: file_rmap2.c
	gcc $(SHARED_OPTIONS) -o file_rmap2 file_rmap2.c

merge_split: merge_split.c
	gcc $(SHARED_OPTIONS) -o merge_split merge_split.c

split_vma: split_vma.c
	gcc $(SHARED_OPTIONS) -o split_vma split_vma.c

vma: vma.c shared.c
	gcc $(SHARED_OPTIONS) -o vma vma.c shared.c

clean:
	rm -f forky forky2 file_rmap file_rmap2 merge_split split_vma vma

.PHONY: all clean
