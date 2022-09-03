#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/page-flags.h>
#include <linux/pageblock-flags.h>
#include <linux/percpu-defs.h>

MODULE_AUTHOR("Lorenzo Stoakes <lstoakes@gmail.com>");
MODULE_DESCRIPTION("Experiments");
MODULE_LICENSE("GPL");

static int __init exper_init(void)
{
	struct page *pg;
	char *chrs_raw;
	char *chrs_kmalloc;

	pg = alloc_page(GFP_KERNEL);

	chrs_raw = page_to_virt(pg);
	chrs_raw[0] = 'x';

	pr_err("flags = %lu\n", pg->flags & PAGEFLAGS_MASK);

	__free_page(pg);

	chrs_kmalloc = kmalloc(4096, GFP_KERNEL);
	chrs_kmalloc[0] = 'x';

	pr_err("kmalloc flags = %lu\n", virt_to_page(chrs_kmalloc)->flags & PAGEFLAGS_MASK);

	kfree(chrs_kmalloc);

	pr_err("HUGETLB_PAGE_ORDER = %u, MAX_ORDER = %u, pageblock_order = %u\n",
	       HUGETLB_PAGE_ORDER, MAX_ORDER, pageblock_order);

	return 0;
}

static void __exit exper_exit(void)
{
}

module_init(exper_init);
module_exit(exper_exit);
