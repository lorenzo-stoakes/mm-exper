// gcc -W -Wall -O2 -g scan.c -o scan
/*
 * scan.c - Demonstrate costs of physical vs logical scanning
 * Copyright (c) 2023 Oracle Corporation
 * Author: Matthew Wilcox <willy@infradead.org>
 * Version: 1.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

unsigned int verbose;
#define printv(level, fmt, ...) \
        if (level <= verbose) { printf(fmt, ##__VA_ARGS__); }

/* time2 - time1 */
double diff_time(struct timespec *time1, struct timespec *time2)
{
	double result = time2->tv_nsec - time1->tv_nsec;

	return time2->tv_sec - time1->tv_sec + result / 1000 / 1000 / 1000;
}

int usage(char opt)
{
	fprintf(stderr, "Option %c not known\n", opt);
	return 1;
}

/* Number of pages in memmap */
unsigned long count = 1000 * 1000;

/*
 * struct page is about this big.  technically the linked list points
 * to the linked list, not to the struct page, but it's close enough for
 * cricket.
 */
struct page {
	unsigned long flags;
	struct page *next;
	struct page *prev;
	unsigned long age;
	unsigned long padding[4];
};

void shuffle(struct page **array, unsigned long seed)
{
	unsigned long i;

	printv(2, "random seed %lu\n", seed);
	srand48(seed);

	/* Knuth shuffle. O(n) time */
	for (i = 1; i < count; i++) {
		struct page *page;
		unsigned long j = (unsigned long)mrand48() % (i + 1);

		page = array[j];
		array[j] = array[i];
		array[i] = page;
	}

	for (i = 0; i < count; i++) {
		struct page *page = array[i];
		page->next = array[(i + 1) % count];
		page->prev = array[(i - 1) % count];
		page->age = i;
	}
}

int main(int argc, char **argv)
{
	int opt;
	unsigned long seed = time(NULL);
	struct page *memmap, **lru_array, *page;
	unsigned long total, i;
	struct timespec start, finish;
	double phys_scan, listlru, arraylru;

	while ((opt = getopt(argc, argv, "c:s:v")) != -1) {
		if (opt == 'c')
			count *= strtoul(optarg, NULL, 0);
		else if (opt == 's')
			seed = strtoul(optarg, NULL, 0);
		else if (opt == 'v')
			verbose++;
		else
			return usage(opt);
        }

	clock_gettime(CLOCK_MONOTONIC, &start);
	memmap = calloc(count, sizeof(struct page));
	if (!memmap) {
		perror("Allocating memmap");
		return 1;
	}
	clock_gettime(CLOCK_MONOTONIC, &finish);
	printv(3, "Allocated memmap for %lu pages in %fs\n", count,
			diff_time(&start, &finish));

	lru_array = malloc(count * sizeof(struct page *));
	for (i = 0; i < count; i++)
		lru_array[i] = memmap + i;

	clock_gettime(CLOCK_MONOTONIC, &start);
	shuffle(lru_array, seed);
	clock_gettime(CLOCK_MONOTONIC, &finish);
	printv(3, "Created shuffled list in %fs\n", diff_time(&start, &finish));

	total = 0;
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (i = 0; i < count; i++)
		total += memmap[i].age;
	clock_gettime(CLOCK_MONOTONIC, &finish);
	printv(3, "total = %lu\n", total);
	phys_scan = diff_time(&start, &finish);
	printv(1, "Physical scan in %fs\n", phys_scan);

	total = 0;
	page = memmap;
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (i = 0; i < count; i++)
		total += lru_array[i]->age;
	clock_gettime(CLOCK_MONOTONIC, &finish);
	printv(3, "total = %lu\n", total);
	arraylru = diff_time(&start, &finish);
	printv(1, "Array scan in %fs\n", arraylru);

	total = 0;
	page = memmap;
	clock_gettime(CLOCK_MONOTONIC, &start);
	do {
		total += page->age;
		page = page->next;
	} while (page != memmap);
	clock_gettime(CLOCK_MONOTONIC, &finish);
	printv(3, "total = %lu\n", total);
	listlru = diff_time(&start, &finish);
	printv(1, "ListLRU scan in %fs\n", listlru);

	printf("Physical scan is %f faster than ListLRU\n",
		listlru / phys_scan);
	printf("Physical scan is %f faster than ArrayLRU\n",
		arraylru / phys_scan);

	return 0;
}
