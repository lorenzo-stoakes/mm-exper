#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/syscall.h>

#define MREMAP_RELOCATE_ANON 8
#define MREMAP_MUST_RELOCATE_ANON 16

struct self_procmap {
	int fd;
	struct procmap_query query;
	char vma_name[256];
};

enum query_result {
	MERGE_UNKNOWN,
	NO_MERGE,
	MERGE_0_1,
	MERGE_1_2,
	MERGE_ALL,
};

/* The state of each VMA undergoing testing. */
enum vma_state {
	NO_VMA,
	EMPTY_ANON_VMA,
	SAME_ANON_VMA,
	DIFFERENT_ANON_VMA,
	DIFFERENT_ANON_VMA2,
	NUM_VMA_STATES,
};

static char state_to_char(enum vma_state state)
{
	switch (state) {
	case NO_VMA:
		return '_';
	case EMPTY_ANON_VMA:
		return '.';
	case SAME_ANON_VMA:
		return 'P';
	case DIFFERENT_ANON_VMA:
		return 'x';
	case DIFFERENT_ANON_VMA2:
		return 'y';
	}

	return '?';
}

/*
 * Test different configurations of VMAs adjacent to one another, with the
 * middle VMA being the one mremap()'d into place.
 */
struct mremap_merge_config {
	struct self_procmap *map;

	/* State of left, right VMAs. */
	enum vma_state states[2];
	bool primary_populated;
	bool relocate_anon;
};

static void print_state(struct mremap_merge_config *conf)
{
	printf("%c%c%c",
	       state_to_char(conf->states[0]),
	       conf->primary_populated ? 'P' : '.',
	       state_to_char(conf->states[1]));
}

static enum vma_state state(struct mremap_merge_config *conf, int ind)
{
	return conf->states[ind];
}

static bool is_none(struct mremap_merge_config *conf, int ind)
{
	return state(conf, ind) == NO_VMA;
}

static bool is_empty(struct mremap_merge_config *conf, int ind)
{
	return state(conf, ind) == EMPTY_ANON_VMA;
}

static bool is_same(struct mremap_merge_config *conf, int ind)
{
	return state(conf, ind) == SAME_ANON_VMA;
}

static bool is_diff(struct mremap_merge_config *conf, int ind)
{
	return state(conf, ind) == DIFFERENT_ANON_VMA ||
		state(conf, ind) == DIFFERENT_ANON_VMA2;
}

static bool is_primary_pop(struct mremap_merge_config *conf)
{
	return conf->primary_populated;
}

static void *sys_mremap(void *old_address, unsigned long old_size,
			unsigned long new_size, int flags, void *new_address)
{
	return (void *)syscall(__NR_mremap, (unsigned long)old_address,
			       old_size, new_size, flags,
			       (unsigned long)new_address);
}

static void mremap_move(char *ptr, int src_offset, int tgt_offset, int num_pages,
		       bool relocate_anon)
{
	const unsigned long page_size =
		(const unsigned long)sysconf(_SC_PAGESIZE);
	unsigned long flags = MREMAP_FIXED | MREMAP_MAYMOVE;
	void *res;

	if (relocate_anon)
		flags |= MREMAP_MUST_RELOCATE_ANON;

	res = sys_mremap(&ptr[src_offset * page_size], num_pages * page_size,
			 num_pages * page_size, flags,
			 &ptr[tgt_offset * page_size]);
	if (res == MAP_FAILED) {
		perror("mremap()");
		exit(EXIT_FAILURE);
	}
}

static void init_mappings(struct mremap_merge_config *conf, char *ptr)
{
	const unsigned long page_size =
		(const unsigned long)sysconf(_SC_PAGESIZE);
	char *ptr1;
	int primary_ind = 1;

	/*
	 * Map all as one to start with, we will figure out what the do next.
	 *
	 * 012345
	 * xPy
	 */

	ptr1 = mmap(ptr, 3 * page_size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
	if (ptr1 == MAP_FAILED) {
		perror("mmap() 2");
		exit(EXIT_FAILURE);
	}

	/* If all the same, fault in now. */
	if (is_same(conf, 0) && is_same(conf, 1) && is_primary_pop(conf)) {
		ptr[0] = 'x';
		/*
		 * By convention, put the primary VMA into index 9.
		 *
		 * 012345   0123456789
		 * xPy   -> x y      P
		 */
		mremap_move(ptr, primary_ind, 9, 1, conf->relocate_anon);
		return;
	}

	/*
	 * 012    012345
	 * xPy -> x   Py
	 *
	 */
	mremap_move(ptr, 1, 4, 2, conf->relocate_anon);
	primary_ind = 4;

	/* Fault Py together. */
	if (is_same(conf, 1))
		ptr[4 * page_size] = 'x';

	/*
	 * 012345    01234567
	 * x   Py -> xP   y
	 */
	mremap_move(ptr, 4, 1, 1, conf->relocate_anon);
	primary_ind = 1;

	/* Fault xP together. */
	if (is_same(conf, 0))
		ptr[0] = 'x';

	/*
	 * 012345    012345
	 * xP   y -> x  P y
	 */
	mremap_move(ptr, 1, 3, 1, conf->relocate_anon);
	primary_ind = 3;

	/* If already otherwise faulted, no-op. */
	if (is_primary_pop(conf))
		ptr[3 * page_size] = 'x';

	/* If x, y differently faulted, fault in separately. */
	if (is_diff(conf, 0) && is_diff(conf, 1) &&
	    state(conf, 0) != state(conf, 1)) {
		ptr[0] = 'x';
		ptr[5 * page_size] = 'x';
	}

	/*
	 * 012345    012345
	 * x  P y -> xy P
	 */
	mremap_move(ptr, 5, 1, 1, conf->relocate_anon);

	/* If already otherwise faulted, no-op. */
	if (is_diff(conf, 0) && is_diff(conf, 1))
		ptr[0] = 'x';

	/*
	 * By convention, put the primary VMA into index 9.
	 *
	 * 012345    0123456789
	 * xy P   -> x y      P
	 */
	mremap_move(ptr, 3, 9, 1, conf->relocate_anon);
	mremap_move(ptr, 1, 2, 1, conf->relocate_anon);

	if (is_none(conf, 0))
		munmap(ptr, page_size);
	if (is_none(conf, 1))
		munmap(&ptr[2 * page_size], page_size);
}


static int query(struct self_procmap *map, void *addr,
		 enum procmap_query_flags flags)
{
	map->query.query_addr = (unsigned long)addr;
	map->query.query_flags = flags;

	return ioctl(map->fd, PROCMAP_QUERY, &map->query);
}

static enum query_result query_ptr(struct mremap_merge_config *conf,
				   char *ptr)
{
	const unsigned long page_size =
		(const unsigned long)sysconf(_SC_PAGESIZE);
	unsigned long start, end, len;

	if (query(conf->map, &ptr[page_size], 0)) {
		perror("query()");
		exit(EXIT_FAILURE);
	}

	start = conf->map->query.vma_start;
	end = conf->map->query.vma_end;
	len = end - start;
	if (len <= page_size)
		return NO_MERGE;
	if (len == 3 * page_size)
		return MERGE_ALL;

	if (start == (unsigned long)ptr)
		return MERGE_0_1;
	if (start == (unsigned long)(&ptr[page_size]))
		return MERGE_1_2;

	return MERGE_UNKNOWN;
}

static enum query_result __try_mremap(struct mremap_merge_config *conf)
{
	const unsigned long page_size =
		(const unsigned long)sysconf(_SC_PAGESIZE);
	char *ptr;
	int err;

	/* Reserve PROT_NONE mapping to put VMAs into. */
	ptr = mmap(NULL, 10 * page_size, PROT_NONE,
		   MAP_PRIVATE | MAP_ANON, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap()");
		exit(EXIT_FAILURE);
	}

	init_mappings(conf, ptr);

	/* Execute the move. */
	mremap_move(ptr, 9, 1, 1, conf->relocate_anon);

	return query_ptr(conf, ptr);
}

static enum query_result try_mremap(struct mremap_merge_config *conf, bool *bypass)
{
	*bypass = true;

	/* Trivial cases. */
	if (is_none(conf, 0) && is_none(conf, 1))
		return NO_MERGE;
	if ((!is_primary_pop(conf)) && (is_same(conf, 0) || is_same(conf, 1)))
		return NO_MERGE; /* Invalid case, filter out. */

	*bypass = false;
	return __try_mremap(conf);
}

static int init_self_procmap(struct self_procmap *map)
{
	pid_t pid = getpid();
	char path_buf[256];
	int err;

	snprintf(path_buf, sizeof(path_buf), "/proc/%u/maps", pid);
	memset(map, 0, sizeof(*map));

	map->fd = open(path_buf, O_RDONLY);
	if (map->fd < 0)
		return -EBADF;

	map->query.size = sizeof(struct procmap_query);

	return 0;
}

static void print_query_result(struct mremap_merge_config *conf,
			       enum query_result res)
{
	print_state(conf);
	printf(": ");
	switch (res) {
	case MERGE_UNKNOWN:
	default:
		printf("???\n");
		break;
	case NO_MERGE:
		printf("NO MERGE\n");
		break;
	case MERGE_ALL:
		printf("MERGE 0,1,2\n");
		break;
	case MERGE_0_1:
		printf("MERGE 0,1\n");
		break;
	case MERGE_1_2:
		printf("MERGE 1,2\n");
		break;
	}
}

static void do_mremap_mprotect(struct mremap_merge_config *conf)
{
	const unsigned long page_size = (const unsigned long)sysconf(_SC_PAGESIZE);
	char *ptr, *ptr2, *ptr3;
	pid_t pid = getpid();
	char cmd[512];
	unsigned long start, end, len;
	unsigned long mremap_flags = MREMAP_FIXED | MREMAP_MAYMOVE;

	if (conf->relocate_anon)
		mremap_flags |= MREMAP_MUST_RELOCATE_ANON;

	/* Hackily establish a PROT_NONE region we can operate in. */
	ptr = mmap(NULL, 100 * page_size, PROT_NONE, MAP_ANON | MAP_PRIVATE,
		   -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	munmap(ptr, 100 * page_size);

	/* Now map 10 pages. */
	ptr = mmap(ptr, 10 * page_size, PROT_READ | PROT_WRITE,
		   MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	/* Map another 10 at a distance away. */
	ptr2 = mmap(&ptr[50 * page_size], 10 * page_size, PROT_READ | PROT_WRITE,
		    MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
	if (ptr2 == MAP_FAILED) {
		perror("mmap 2");
		exit(EXIT_FAILURE);
	}

	/* Fault in. */
	ptr2[0] = 'x';

	/* Now mprotect read-only. */
	if (mprotect(ptr2, 10 * page_size, PROT_READ)) {
		perror("mprotect");
		exit(EXIT_FAILURE);
	}

	/* Fault in half way through... */
	ptr[5 * page_size] = 'x';

	/*
	 * Now move half of the original mapping adjacent to the read-only
	 * one.
	 */
	ptr3 = sys_mremap(&ptr[5 * page_size], 5 * page_size, 5 * page_size,
			  mremap_flags, &ptr[60 * page_size]);
	if (ptr3 == MAP_FAILED) {
		perror("mremap");
		exit(EXIT_FAILURE);
	}

	/* Now mprotect the read-only mapping read-write. */
	if (mprotect(ptr2, 10 * page_size, PROT_READ | PROT_WRITE)) {
		perror("mprotect 2");
		exit(EXIT_FAILURE);
	}

	if (query(conf->map, ptr2, 0)) {
		perror("query()");
		exit(EXIT_FAILURE);
	}

	start = conf->map->query.vma_start;
	end = conf->map->query.vma_end;
	len = end - start;

	if (len > 10 * page_size)
		printf("mprotect() MERGED\n");
	else
		printf("mprotect() UNMERGED\n");

	munmap(ptr, page_size * 5);
	munmap(ptr2, page_size * 10);
	munmap(ptr3, page_size * 10);
}

static void iterate_cases(struct mremap_merge_config *conf)
{
	int i, j;

	for (i = 0; i < NUM_VMA_STATES; i++) {
		for (j = 0; j < NUM_VMA_STATES; j++) {
			enum query_result res;
			bool bypass;

			conf->states[0] = (enum vma_state)i;
			conf->states[1] = (enum vma_state)j;

			res = try_mremap(conf, &bypass);
			if (bypass)
				goto next;
			print_query_result(conf, res);

next:
			conf->primary_populated = true;
			res = try_mremap(conf, &bypass);
			if (bypass)
				continue;
			print_query_result(conf, res);
		}
	}

	do_mremap_mprotect(conf);
}

int main(void)
{
	struct self_procmap map;
	struct mremap_merge_config conf = {};

	if (init_self_procmap(&map))
		return EXIT_FAILURE;

	conf.map = &map;

	iterate_cases(&conf);

	conf.relocate_anon = true;
	printf("-- WITH MREMAP_RELOCATE_ANON: --\n");
	iterate_cases(&conf);

	close(map.fd);
	return EXIT_SUCCESS;
}
