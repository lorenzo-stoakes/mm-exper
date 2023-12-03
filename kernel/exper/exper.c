#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/list.h>
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

// TODO: HORRENDOUS duplication.

static char pagebuffers_buf[255] = "[]";
static int pagebuffers_buf_size = 3;

static ssize_t pagebuffers_read_(struct file *file, char __user *out,
				 size_t size, loff_t *off)
{
	return simple_read_from_buffer(out, size, off, pagebuffers_buf,
				       pagebuffers_buf_size);
}

static void read_buffer_head(struct buffer_head *bh) {
	int size = sizeof(pagebuffers_buf) - 1;

	pagebuffers_buf[0] = '[';
	pagebuffers_buf[1] = '\0';

#define CHECK_FLAG(flag_, str_) \
	if (bh->b_state & (1U << flag_))		\
		strncat(pagebuffers_buf, str_, size);

	CHECK_FLAG(BH_Uptodate, "U");
	CHECK_FLAG(BH_Dirty, "D");
	CHECK_FLAG(BH_Lock, "L");
	CHECK_FLAG(BH_Req, "R");
	CHECK_FLAG(BH_Mapped, "M");
	CHECK_FLAG(BH_New, "N");
	CHECK_FLAG(BH_Async_Read, "Ar");
	CHECK_FLAG(BH_Async_Write, "Aw");
	CHECK_FLAG(BH_Delay, "De");
	CHECK_FLAG(BH_Boundary, "B");
	CHECK_FLAG(BH_Write_EIO, "Ew");
	CHECK_FLAG(BH_Unwritten, "Un");
	CHECK_FLAG(BH_Quiet, "Q");
	CHECK_FLAG(BH_Meta, "Me");
	CHECK_FLAG(BH_Prio, "!");
	CHECK_FLAG(BH_Defer_Completion, "@");

#undef CHECK_FLAG

	strncat(pagebuffers_buf, "]", size);

	pagebuffers_buf_size = strlen(pagebuffers_buf);
}

static ssize_t pagebuffers_write_(struct file *file, const char __user *in,
				  size_t size, loff_t *off)
{
	char pfn_buf[255];
	uint64_t pfn;
	ssize_t count;
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

	if (page_has_buffers(pg)) {
		pr_err("IVG: Page has buffers\n");
		read_buffer_head(page_buffers(pg));
	} else {
		pagebuffers_buf[0] = '[';
		pagebuffers_buf[1] = ']';
		pagebuffers_buf[2] = '\0';
		pagebuffers_buf_size = 3;
	}

	*off = size;
	return size;
}

static const struct file_operations pagebuffers_fops = {
	.owner = THIS_MODULE,
	.read = pagebuffers_read_,
	.write = pagebuffers_write_
};

static struct miscdevice pagebuffers_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "pagebuffers",
	.fops = &pagebuffers_fops
};

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

static void list_exper(void)
{
	struct obj {
		int val;
		struct list_head node;
	};
	struct obj *curr;

	struct obj obj1 = {
		.val = 1,
	};

	struct obj obj2 = {
		.val = 2,
	};

	struct obj obj3 = {
		.val = 3,
	};

	struct obj obj4 = {
		.val = 4,
	};

	LIST_HEAD(a);
	LIST_HEAD(b);

	list_add_tail(&obj1.node, &a);
	list_add_tail(&obj2.node, &a);

	list_add_tail(&obj3.node, &b);
	list_add_tail(&obj4.node, &b);

	pr_info("List A:\n");
	list_for_each_entry(curr, &a, node) {
		pr_info("entry: %d\n", curr->val);
	}

	pr_info("List B:\n");
	list_for_each_entry(curr, &b, node) {
		pr_info("entry: %d\n", curr->val);
	}

	list_splice_init(&b, &a);

	pr_info("Post splice b-> a\n");

	pr_info("List A:\n");
	list_for_each_entry(curr, &a, node) {
		pr_info("entry: %d\n", curr->val);
	}

	pr_info("List B:\n");
	list_for_each_entry(curr, &b, node) {
		pr_info("entry: %d\n", curr->val);
	}

	pr_info("Reverse through list:\n");

	while (!list_empty(&a)) {
		curr = list_entry(a.prev, struct obj, node);
		pr_info("entry: %d\n", curr->val);
		list_del_init(&curr->node);
	}
}

static int __init exper_init(void)
{
	struct page *pg;
	char *chrs_raw;
	char *chrs_kmalloc;
	int err;

	list_exper();

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

	err = misc_register(&refcount_dev);
	if (err)
		return err;

	err = misc_register(&pagebuffers_dev);
	if (err)
		return err;

	return 0;
}

static void __exit exper_exit(void)
{
	misc_deregister(&refcount_dev);
	misc_deregister(&pagebuffers_dev);
}

module_init(exper_init);
module_exit(exper_exit);
