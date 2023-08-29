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
MODULE_DESCRIPTION("Memory fragment test");
MODULE_LICENSE("GPL");

static LIST_HEAD(pages);

static int __init fragment2_init(void)
{
	struct page *page;
	int /*i,*/ count = 0;

	/*
	 * Allocate as many order-9 pages as we can... We aren't using GFP_COMP
	 * so only the 1st page in each 1 << 9 block are initialised.
	 */

	while ((page = alloc_pages(GFP_KERNEL, 9))) {
		list_add(&page->lru, &pages);
		count++;
	}

	pr_info("IVG: %d order-9 pages allocated, or %d MiB\n", count, count * 2);

	return 0;
}

static void __exit fragment2_exit(void)
{
	struct page *page, *tmp;
	int count = 0;

	/* Now free everything. */
	list_for_each_entry_safe(page, tmp, &pages, lru) {
		__free_pages(page, 9);
		count++;
	}

	pr_info("IVG: %d order-9 pages freed\n", count);
}

module_init(fragment2_init);
module_exit(fragment2_exit);
