#define _GNU_SOURCE

#include "bitwise.h"
#include "malloc_impl.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#if defined(__GNUC__) && defined(__PIC__)
#define inline inline __attribute__((always_inline))
#endif

#ifdef DEBUG_OUTPUT
#define pr_dbg(fmt, ...) printf("debug: " __FILE__ ": %d: " fmt "\n", __LINE__, ## __VA_ARGS__)
#define pr_dbg_chunk(preface, chunk) pr_dbg("%s: chunk[ptr=%p, psize=%lu [%lu]%s, csize=%lu [%lu]%s]", preface, chunk, \
					    chunk->psize & -2, CHUNK_PSIZE(chunk) / SIZE_ALIGN, \
					    chunk->psize & 1 ? " (used)" : "", \
					    chunk->csize & -2, CHUNK_SIZE(chunk) / SIZE_ALIGN, \
					    chunk->csize & 1 ? " (used)" : "")
#else
#define pr_dbg(...)
#endif

static struct {
	volatile uint64_t binmap;
	struct bin bins[64];
	volatile int split_merge_lock[2];
} mal;

static struct {
	uint64_t allocated_bytes;
	uint64_t free_bytes;

	uint64_t heap_bytes;
	uint64_t free_block_bytes;
	uint64_t mmap_bytes;
	uint64_t reclaimed_bytes;
} stats;

void musl_dump_bins(void)
{
	puts("\n=== STATS ===\n");

	printf("allocated = %lu, free = %lu, heap size = %lu, free blocks = %lu, mmaps = %lu, reclaimed = %lu\n",
	       stats.allocated_bytes, stats.free_bytes, stats.heap_bytes, stats.free_block_bytes, stats.mmap_bytes,
		stats.reclaimed_bytes);

	for (int i = 0; i < 64; i++) {
		if (!is_bit_set(mal.binmap, i) || mal.bins[i].head == BIN_TO_CHUNK(i))
			continue;

		printf("%02d: ", i);

		for (struct chunk *curr = mal.bins[i].head; curr != BIN_TO_CHUNK(i); curr = curr->next) {
			// I mean, this shouldn't be possible should it?
			if (curr->csize & C_INUSE)
				continue;

			const size_t num_aligns = curr->csize / SIZE_ALIGN;
			printf("%lu ", num_aligns);
		}
		printf("\n");
	}

	puts("\n=============\n");
}

static void crash(void)
{
	fprintf(stderr, "FATAL ERROR\n");
	abort();
}

static inline void lock_bin(int i)
{
	lock(mal.bins[i].lock);
	if (!mal.bins[i].head)
		mal.bins[i].head = mal.bins[i].tail = BIN_TO_CHUNK(i);
}

static inline void unlock_bin(int i)
{
	unlock(mal.bins[i].lock);
}

static int first_set(uint64_t x)
{
	return __builtin_ctzl(x);
}

/*
 *
 * How bin_index() and bin_index_up() SIZE_ALIGN counts map to bin indexes:
 *
 * i  bin_index    bin_index_up count
 *  0 | 1          | 1          | 1
 *  1 | 2          | 2          | 1
 *  2 | 3          | 3          | 1
 *  3 | 4          | 4          | 1
 *  4 | 5          | 5          | 1
 *  5 | 6          | 6          | 1
 *  6 | 7          | 7          | 1
 *  7 | 8          | 8          | 1
 *  8 | 9          | 9          | 1
 *  9 | 10         | 10         | 1
 * 10 | 11         | 11         | 1
 * 11 | 12         | 12         | 1
 * 12 | 13         | 13         | 1
 * 13 | 14         | 14         | 1
 * 14 | 15         | 15         | 1
 * 15 | 16         | 16         | 1
 * 16 | 17         | 17         | 1
 * 17 | 18         | 18         | 1
 * 18 | 19         | 19         | 1
 * 19 | 20         | 20         | 1
 * 20 | 21         | 21         | 1
 * 21 | 22         | 22         | 1
 * 22 | 23         | 23         | 1
 * 23 | 24         | 24         | 1
 * 24 | 25         | 25         | 1
 * 25 | 26         | 26         | 1
 * 26 | 27         | 27         | 1
 * 27 | 28         | 28         | 1
 * 28 | 29         | 29         | 1
 * 29 | 30         | 30         | 1
 * 30 | 31         | 31         | 1
 * 31 | 32         | 32         | 1
 * 32 | 33  ..40   | 33         | 8, 1
 * 33 | 41  ..48   | 34  ..41   | 8, 8
 * 34 | 49  ..56   | 42  ..49   | 8, 8
 * 35 | 57  ..64   | 50  ..57   | 8, 8
 * 36 | 65  ..80   | 58  ..65   | 16, 8
 * 37 | 81  ..96   | 66  ..81   | 16, 16
 * 38 | 97  ..112  | 82  ..97   | 16, 16
 * 39 | 113 ..128  | 98  ..113  | 16, 16
 * 40 | 129 ..160  | 114 ..129  | 32, 16
 * 41 | 161 ..192  | 130 ..161  | 32, 32
 * 42 | 193 ..224  | 162 ..193  | 32, 32
 * 43 | 225 ..256  | 194 ..225  | 32, 32
 * 44 | 257 ..320  | 226 ..257  | 64, 32
 * 45 | 321 ..384  | 258 ..321  | 64, 64
 * 46 | 385 ..448  | 322 ..385  | 64, 64
 * 47 | 449 ..512  | 386 ..449  | 64, 64
 * 48 | 513 ..640  | 450 ..513  | 128, 64
 * 49 | 641 ..768  | 514 ..641  | 128, 128
 * 50 | 769 ..896  | 642 ..769  | 128, 128
 * 51 | 897 ..1024 | 770 ..897  | 128, 128
 * 52 | 1025..1280 | 898 ..1025 | 256, 128
 * 53 | 1281..1536 | 1026..1281 | 256, 256
 * 54 | 1537..1792 | 1282..1537 | 256, 256
 * 55 | 1793..2048 | 1538..1793 | 256, 256
 * 56 | 2049..2560 | 1794..2049 | 512, 256
 * 57 | 2561..3072 | 2050..2561 | 512, 512
 * 58 | 3073..3584 | 2562..3073 | 512, 512
 * 59 | 3585..4096 | 3074..3585 | 512, 512
 * 60 | 4097..5120 | 3586..4097 | 1024, 512
 * 61 | 5121..6144 | 4098..5121 | 1024, 1024
 * 62 | 6145..7168 | 5122..6145 | 1024, 1024
 * 63 | 7169..     | 6146..7169 | ..., 1024
 */
static const unsigned char bin_tab[60] = {
	            32,33,34,35,36,36,37,37,38,38,39,39,
	40,40,40,40,41,41,41,41,42,42,42,42,43,43,43,43,
	44,44,44,44,44,44,44,44,45,45,45,45,45,45,45,45,
	46,46,46,46,46,46,46,46,47,47,47,47,47,47,47,47,
};

// Convert an (assumed aligned to SIZE_ALIGN) size to its 'bin' index. See
// comment above bin_tab[].
static int bin_index(size_t x)
{
	x = (x / SIZE_ALIGN) - 1;

	if (x <= 32)
		return x;
	if (x < 512)
		return bin_tab[x/8-4];
	if (x > 0x1c00)
		return 63;

	return bin_tab[x/128-4] + 16;
}

// Convert an (assumed aligned to SIZE_ALIGN) size to its 'bin' index rounding
// upwards... See comment above bin_tab[].
static int bin_index_up(size_t x)
{
	x = (x / SIZE_ALIGN) - 1;

	if (x <= 32)
		return x;

	x--;

	if (x < 512)
		return bin_tab[x/8-4] + 1;

	return bin_tab[x/128-4] + 17;
}

/*
 * Expand the heap in-place if brk can be used, or otherwise via mmap, using an
 * exponential lower bound on growth by mmap to make fragmentation
 * asymptotically irrelevant. The size argument is both an input and an output,
 * since the caller needs to know the size allocated, which will be larger than
 * requested due to page alignment and mmap minimum size rules. The caller is
 * responsible for locking to prevent concurrent calls.
 */
static void *__expand_heap(size_t *pn)
{
	static uintptr_t stored_brk;
	static unsigned mmap_step;
	size_t n = *pn;

	// If the expansion request is unreasonable, abort.
	if (n > SIZE_MAX/2 - PAGE_SIZE) {
		pr_dbg("      | Cannot expand heap to accomodate %lu, too big", n);
		errno = ENOMEM;
		return NULL;
	}

	// We always align to pages when expanding the heap.

	pr_dbg("      | aligned %lu to page size = %lu", n, align64_up(n, PAGE_SIZE));
	n = align64_up(n, PAGE_SIZE);

	// If we don't already know current program break, go get it.
	if (stored_brk == 0) {
		stored_brk = align64_up(sbrk(0), PAGE_SIZE);

		pr_dbg("      | retrieved initial brk = %p", (void *)stored_brk);
	}

	// If we won't overflow, try extending the program break.
	const bool would_overflow = n >= SIZE_MAX - stored_brk;
	if (!would_overflow && sbrk(n) >= 0) {
		pr_dbg("      | brk successfully extended by %lu to %p",
		       n, (void *)(stored_brk + n));

		*pn = n;
		stored_brk += n;
		stats.heap_bytes += n;

		pr_dbg("      | heap allocated memory expanded from %p",
		       (void *)(stored_brk - n));

		return (void *)(stored_brk - n);
	}

	// OK we failed to extend program break, let's try an mmap.

	// We perform mmap() invocations in gradually increasing PAGE_SIZE
	// blocks.
	size_t min = (size_t)PAGE_SIZE << mmap_step/2;
	n = n < min ? min : n;

	pr_dbg("      | brk extension FAILED, trying mmap (min %lu, size maybe changed to %lu)",
	       min, n);

	void *area = mmap(0, n, PROT_READ|PROT_WRITE,
			  MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

	stats.mmap_bytes += n;

	if (area == MAP_FAILED) {
		pr_dbg("      | mmap FAILED!!");

		return NULL;
	}

	pr_dbg("      | heap expanded via MMAP at %p, mmap_step=%u", area, mmap_step);

	*pn = n;
	mmap_step++;
	return area;
}

static struct chunk *expand_heap(size_t n)
{
	static void *end;
	void *p;
	struct chunk *w;

	// The argument n already accounts for the caller's chunk overhead
	// needs, but if the heap can't be extended in-place, we need room for
	// an extra zero-sized sentinel chunk.
	pr_dbg("      | extending size %lu to %lu", n, n + SIZE_ALIGN);
	n += SIZE_ALIGN;

	p = __expand_heap(&n);
	if (p == NULL)
		return NULL;

	pr_dbg("      | heap expansion resulted in %p, previous end was %p",
	       p, end);
	// If not just expanding existing space, we need to make a
	// new sentinel chunk below the allocated space.
	if (p != end) {
		pr_dbg("      | p != end so adding sentinel chunk");

		// Valid/safe because of the prologue increment.
		n -= SIZE_ALIGN;
		p = (char *)p + SIZE_ALIGN;

		pr_dbg("      | offset new chunk to %p", p);

		w = MEM_TO_CHUNK(p);
		w->psize = 0 | C_INUSE;
	}

	// Record new heap end and fill in footer.
	end = (char *)p + n;
	w = MEM_TO_CHUNK(end);
	w->psize = n | C_INUSE;
	w->csize = 0 | C_INUSE;

	pr_dbg("      | new end = %p, added footer at %p", end, w);

	// Fill in header, which may be new or may be replacing a
	// zero-size sentinel header at the old end-of-heap.
	w = MEM_TO_CHUNK(p);
	w->csize = n | C_INUSE;

	pr_dbg("      | Using footer of previous chunk at %p + marking in use", w);

	return w;
}

static int adjust_size(size_t *n)
{
	// Result of pointer difference must fit in ptrdiff_t.
	if (*n-1 > PTRDIFF_MAX - SIZE_ALIGN - PAGE_SIZE) {
		if (*n) {
			errno = ENOMEM;
			return -1;
		} else {
			*n = SIZE_ALIGN;
			return 0;
		}
	}

	*n = align64_up(*n + OVERHEAD, SIZE_ALIGN);
	return 0;
}

static void unbin(struct chunk *c, int i)
{
	if (c->prev == c->next)
		mal.binmap &= ~(1UL << i);
	c->prev->next = c->next;
	c->next->prev = c->prev;
	c->csize |= C_INUSE;
	NEXT_CHUNK(c)->psize |= C_INUSE;

	stats.free_block_bytes -= CHUNK_SIZE(c);
	stats.free_bytes -= CHUNK_SIZE(c);
}

static void bin_chunk(struct chunk *self, int i)
{
	self->next = BIN_TO_CHUNK(i);
	self->prev = mal.bins[i].tail;

	/*
	 * This is seriously dodgy/clever.
	 *
	 * 1. BIN_TO_CHUNK() subtracts 16 bytes from &mal.bins[i].head which ==
	 *    &mal.bins[i - 1].tail
	 *
	 * 2. This means BIN_TO_CHUNK(i)->next == &mal.bins[i].head and
	 *    BIN_TO_CHUNK(i)->prev == &mal.bins[i].tail
	 *
	 * Any access to BIN_TO_CHUNK(i)->[psize/csize] would have you writing
	 * to the locks...
	 */

	self->next->prev = self;
	self->prev->next = self;
	if (self->prev == BIN_TO_CHUNK(i))
		mal.binmap |= (1UL << i);

	stats.free_block_bytes += CHUNK_SIZE(self);
	stats.free_bytes += CHUNK_SIZE(self);
}

// Trim a chunk down to the actaully used size.
static void trim(struct chunk *self, size_t n)
{
	size_t n1 = CHUNK_SIZE(self);
	struct chunk *next, *split;

	if (n1 - n <= DONTCARE) {
		pr_dbg("    | delta %lu so small we will not TRIM", n1 - n);
		return;
	}

	next = NEXT_CHUNK(self);
	split = (void *)((char *)self + n);

	split->psize = n | C_INUSE;
	split->csize = n1 - n;
	next->psize = n1 - n;
	self->csize = n | C_INUSE;

	int i = bin_index(n1 - n);
	lock_bin(i);

	pr_dbg("    | TRIMmed chunk (size %lu [%lu]) from retrieved chunk (size %lu [%lu], binindex %d) ",
	       n, n / SIZE_ALIGN, n1, n1 / SIZE_ALIGN, bin_index(n1));
	pr_dbg("    | TRIMmed free block (size %lu [%lu]) reinserted into binindex %d",
	       split->csize, split->csize / SIZE_ALIGN, i);

	bin_chunk(split, i);

	unlock_bin(i);
}

void *musl_malloc(size_t n)
{
	struct chunk *c;
	int i, j;
	uint64_t mask;

	pr_dbg("-- MALLOC %lu --", n);

	if (adjust_size(&n) < 0) {
		pr_dbg("adjust_size() failed");
		return NULL;
	}

	pr_dbg("  | adjusted size = %lu (%lu SIZE_ALIGNs)", n, n / SIZE_ALIGN);

	if (n > MMAP_THRESHOLD) {
		pr_dbg("  | MMAP because %lu > MMAP_THRESHOLD (= %lu)",
		       n, MMAP_THRESHOLD);

		size_t len = align64_up(n + OVERHEAD, PAGE_SIZE);
		pr_dbg("    | page-aligned overhead-extended mmap size is %lu", len);

		char *base = mmap(0, len, PROT_READ | PROT_WRITE,
				  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (base == (void *)-1) {
			pr_dbg("    | mmap() failed");
			return NULL;
		}
		c = (void *)(base + SIZE_ALIGN - OVERHEAD);
		c->csize = len - (SIZE_ALIGN - OVERHEAD);
		c->psize = SIZE_ALIGN - OVERHEAD;

		stats.mmap_bytes += len;
		stats.allocated_bytes += CHUNK_SIZE(c);

		pr_dbg_chunk("    | returning", c);

		void *ret = CHUNK_TO_MEM(c);
		pr_dbg("    | (returned ptr %p)", ret);
		return ret;
	}

	i = bin_index_up(n);
	pr_dbg("  | BRK bin_index_up() = %d", i);

	if (i < 63 && is_bit_set(mal.binmap, i)) {
		lock_bin(i);
		c = mal.bins[i].head;
		if (c != BIN_TO_CHUNK(i) && CHUNK_SIZE(c) - n <= DONTCARE) {
			unbin(c, i);
			unlock_bin(i);

			stats.allocated_bytes += CHUNK_SIZE(c);

			pr_dbg_chunk("    | FAST PATH got from free list", c);
			return CHUNK_TO_MEM(c);
		}
		unlock_bin(i);
	}

	lock(mal.split_merge_lock);
	for (mask = mask_high_bits(mal.binmap, i); mask != 0; clear_lowest_bit(mask)) {
		j = first_set(mask);
		lock_bin(j);
		c = mal.bins[j].head;
		if (c != BIN_TO_CHUNK(j)) {
			unbin(c, j);
			unlock_bin(j);
			pr_dbg_chunk("    | SLOW PATH got from free list", c);

			break;
		}
		unlock_bin(j);
	}

	if (mask == 0) {
		pr_dbg("    | SLOWEST PATH expanding heap");
		c = expand_heap(n);

		if (c != NULL) {
			pr_dbg_chunk("    | got", c);
		} else {
			pr_dbg("    | FAILURE could not expand heap");

			unlock(mal.split_merge_lock);
			return NULL;
		}
	}

	// We trim for non-mmap non-fastpath.
	trim(c, n);
	unlock(mal.split_merge_lock);

	stats.allocated_bytes += CHUNK_SIZE(c);

	pr_dbg_chunk("    | returning", c);
	void *ret = CHUNK_TO_MEM(c);
		pr_dbg("    | (returned ptr %p)", ret);
	return CHUNK_TO_MEM(c);
}

void __bin_chunk(struct chunk *self)
{
	struct chunk *next = NEXT_CHUNK(self);

	stats.allocated_bytes -= CHUNK_SIZE(self);

	pr_dbg_chunk("    | freeing chunk", self);

	// Crash on corrupted footer (likely from buffer overflow).
	if (next->psize != self->csize)
		crash();

	lock(mal.split_merge_lock);

	size_t osize = CHUNK_SIZE(self), size = osize;

	/* Since we hold split_merge_lock, only transition from free to
	 * in-use can race; in-use to free is impossible */
	size_t psize = self->psize & C_INUSE ? 0 : CHUNK_PSIZE(self);
	size_t nsize = next->csize & C_INUSE ? 0 : CHUNK_SIZE(next);

	if (psize > 0) {
		int i = bin_index(psize);
		pr_dbg("    | prev bin_index = %d, psize=%lu (%lu SIZE_ALIGNs)",
		       i, psize, psize / SIZE_ALIGN);

		lock_bin(i);

		if (self->psize & C_INUSE) {
			pr_dbg("      | in use");
		} else {
			struct chunk *prev = PREV_CHUNK(self);
			pr_dbg_chunk("      | merge", prev);

			unbin(prev, i);
			self = prev;

			pr_dbg("      | size += %lu = %lu", psize, size + psize);
			size += psize;
		}

		unlock_bin(i);
	}

	if (nsize > 0) {
		int i = bin_index(nsize);
		pr_dbg("    | next bin_index = %d, nsize=%lu (%lu SIZE_ALIGNs)",
		       i, nsize, nsize / SIZE_ALIGN);

		lock_bin(i);
		if (next->csize & C_INUSE) {
			pr_dbg("      | in use");
		} else {
			unbin(next, i);
			next = NEXT_CHUNK(next);
			pr_dbg_chunk("      | merge", next);

			pr_dbg("      | size += %lu = %lu", nsize, size + nsize);
			size += nsize;
		}
		unlock_bin(i);
	}

	int i = bin_index(size);
	lock_bin(i);

	self->csize = size;
	next->psize = size;

	pr_dbg_chunk("    | freed chunk", self);

	bin_chunk(self, i);
	unlock(mal.split_merge_lock);

	/* Replace middle of large chunks with fresh zero pages */
	if (size > RECLAIM && !share_highest_bit(size, size - osize)) {
		pr_dbg("    | RECLAIM: %lu [%lu]", size, size / SIZE_ALIGN);

		uintptr_t a = (uintptr_t)align64_up((uint64_t)self + SIZE_ALIGN, PAGE_SIZE);
		uintptr_t b = (uintptr_t)align64((uint64_t)next - SIZE_ALIGN, PAGE_SIZE);

		pr_dbg("      | ptr=%p reclaim from %p to %p", self, (void *)a, (void *)b);
		pr_dbg("      | reclaim %lu bytes (%lu pages)", b - a, (b - a) / PAGE_SIZE);

		stats.reclaimed_bytes += (uint64_t)(b - a);

		int e = errno;
		madvise((void *)a, b - a, MADV_DONTNEED);
		errno = e;
	}

	unlock_bin(i);
}

static void unmap_chunk(struct chunk *self)
{
	stats.allocated_bytes -= CHUNK_SIZE(self);

	size_t extra = self->psize;
	char *base = (char *)self - extra;
	size_t len = CHUNK_SIZE(self) + extra;
	// Crash on double free.
	if (extra & 1)
		crash();
	int e = errno;
	munmap(base, len);
	errno = e;

	stats.mmap_bytes -= len;
}

void musl_free(void *p)
{
	pr_dbg("-- FREE %p --", p);

	if (!p) {
		pr_dbg("  | NULL pointer so abort");
		return;
	}

	struct chunk *self = MEM_TO_CHUNK(p);

	if (IS_MMAPPED(self)) {
		pr_dbg("  | mmap()'d so munmap()'ing");
		unmap_chunk(self);
	} else {
		pr_dbg("  | from free list so __bin_chunk()'ing");
		__bin_chunk(self);
	}
}
