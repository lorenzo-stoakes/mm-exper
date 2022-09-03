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

MODULE_AUTHOR("Lorenzo Stoakes <lstoakes@gmail.com>");
MODULE_DESCRIPTION("Experiments");
MODULE_LICENSE("GPL");

static char refcount_buf[255] = "0";
static int refcount_buf_size = 2;

static ssize_t refcount_read_(struct file *file, char __user *out,
			      size_t size, loff_t *off)
{
	return simple_read_from_buffer(out, size, off, refcount_buf,
				       refcount_buf_size);
}

static ssize_t refcount_write_(struct file *file, const char __user *in,
			       size_t size, loff_t *off)
{
	char pfn_buf[255];
	uint64_t pfn;
	ssize_t count;
	int refcount;
	struct page *pg;

	count = simple_write_to_buffer(pfn_buf, 254, off, in, size);
	if (count <= 0)
		return -EINVAL;
	pfn_buf[count] = '\0';

	if (sscanf(pfn_buf, "%llu", &pfn) != 1)
		return -EINVAL;

	pg = pfn_to_page(pfn);
	if (pg == NULL)
		return -EINVAL;

	refcount = page_ref_count(pg);
	refcount_buf_size = snprintf(refcount_buf, 255, "%d", refcount) + 1;

	*off = size;
	return size;
}

static const struct file_operations refcount_fops = {
	.owner = THIS_MODULE,
	.read = refcount_read_,
	.write = refcount_write_
};

static struct miscdevice refcount_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "refcount",
	.fops = &refcount_fops
};

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

	return misc_register(&refcount_dev);
}

static void __exit exper_exit(void)
{
	misc_deregister(&refcount_dev);
}

module_init(exper_init);
module_exit(exper_exit);
