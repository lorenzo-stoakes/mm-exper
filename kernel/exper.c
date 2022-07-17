#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/page-flags.h>

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

	return 0;
}

static void __exit exper_exit(void)
{
}

module_init(exper_init);
module_exit(exper_exit);
