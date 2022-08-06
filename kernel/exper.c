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
	void *ptr;

	pg = alloc_page(GFP_KERNEL);

	if (PageAnon(pg))
		pr_err("Anon!\n");
	else
		pr_err("Not Anon!\n");

	pr_err("flags = %lu\n", pg->flags & PAGEFLAGS_MASK);

	__free_page(pg);

	ptr = kmalloc(4096, GFP_KERNEL);

	pr_err("kmalloc flags = %lu\n", virt_to_page(ptr)->flags & PAGEFLAGS_MASK);

	kfree(ptr);

	pr_err("HUGETLB_PAGE_ORDER = %u, MAX_ORDER = %u, pageblock_order = %u\n",
	       HUGETLB_PAGE_ORDER, MAX_ORDER, pageblock_order);

	pr_err("per_cpu_offset(0) = %lx\n", (*(unsigned long *)per_cpu_offset(1)));
	pr_err("per_cpu_offset(1) = %lx\n", per_cpu_offset(1));
	pr_err("per_cpu_offset(2) = %lx\n", per_cpu_offset(2));
	pr_err("processor id =%d\n", smp_processor_id());

	pr_err("%p\n", per_cpu_ptr((unsigned char *)0, 1));

	return 0;
}

static void __exit exper_exit(void)
{
}

module_init(exper_init);
module_exit(exper_exit);
