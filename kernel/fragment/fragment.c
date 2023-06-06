#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/page_ref.h>
#include <linux/page-flags.h>
#include <linux/pageblock-flags.h>
#include <linux/percpu-defs.h>
#include <linux/vmalloc.h>

MODULE_AUTHOR("Lorenzo Stoakes <lstoakes@gmail.com>");
MODULE_DESCRIPTION("Memory fragment and vmalloc test");
MODULE_LICENSE("GPL");

#define NUM_VMALLOCS (100)
#define VMALLOC_SIZE (1UL << (9 + PAGE_SHIFT))

static unsigned long vaddrs[NUM_VMALLOCS];

static LIST_HEAD(pages);

static int __init fragment_init(void)
{
	struct page *page;
	int i, count = 0;

	/*
	 * Allocate as many order-9 pages as we can... We aren't using GFP_COMP
	 * so only the 1st page in each 1 << 9 block are initialised.
	 */
	while ((page = alloc_pages(GFP_KERNEL | __GFP_NOWARN | __GFP_HIGH, 9))) {
		list_add(&page->lru, &pages);
		count++;
	}

	pr_info("IVG: %d order-9 pages allocated, or %d MiB\n", count, count * 2);

	/* Split each page to order 8, and free the second. */
	list_for_each_entry(page, &pages, lru) {
		struct page *buddy = &page[1 << 8];

		split_page(page, 9);
		/*
		 * Free the 2nd page in each batch, so we can keep the list the
		 * same, and free the first page in each pair on module exit.
		 */
		__free_pages(buddy, 8);
	}

	pr_info("IVG: %d order-8 pages freed, or %d MiB\n", count, count);

	/*
	 * Now we have half our memory available to us, so vmalloc()-ing 1<<9
	 * pages (a few times for good measure :) should be fine, right?
	 */
	count = 0;
	for (i = 0; i < NUM_VMALLOCS; i++) {
		/*
		 * vmalloc_huge() behaves the same as the problematic
		 *kvmalloc_node() code.
		 */
		vaddrs[i] = (unsigned long)vmalloc_huge(VMALLOC_SIZE, GFP_KERNEL);
		if (!vaddrs[i]) {
			/* Well knock me down with a feather... */
			pr_err("IVG: Unable to vmalloc!\n");
			break;
		}
		count++;
	}

	pr_info("IVG: vmalloc()'d %d pages of size %lu\n", count, VMALLOC_SIZE);

	return 0;
}

static void __exit fragment_exit(void)
{
	struct page *page, *tmp;
	int i, count = 0;

	/* Now free everything. */
	list_for_each_entry_safe(page, tmp, &pages, lru) {
		__free_pages(page, 8);
		count++;
	}

	pr_info("IVG: %d order-8 pages freed, or %d MiB\n", count, count);

	for (i = 0 ; i < NUM_VMALLOCS; i++) {
		if (!vaddrs[i])
			break;

		vfree((void *)vaddrs[i]);
	}

	pr_info("IVG: vfreed()'d %d pages of size %lu\n", i, VMALLOC_SIZE);
}

module_init(fragment_init);
module_exit(fragment_exit);
