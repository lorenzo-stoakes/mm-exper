#include <linux/errname.h>
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

//#define LONGTERM

MODULE_AUTHOR("Lorenzo Stoakes <lstoakes@gmail.com>");
MODULE_DESCRIPTION("mad idea");
MODULE_LICENSE("GPL");

/*
 * 1. memory map file r/w
 * 2. echo uaddr > /dev/gup_uaddr
 * 3. write to file -> dirty
 * 4. msync -> clean
 * 5. echo 1 > /dev/gup_event # attempt write, dirty.
 *
 * To unpin gup mapping, echo 0 > /dev/gup_event.
 */

static struct page *curr_page;

static ssize_t gup_uaddr_write(struct file *file, const char __user *in,
			       size_t size, loff_t *off)
{
	char addr_buf[255];
	ssize_t count;
	unsigned long uaddr;
	long ret;

	if (curr_page) {
		pr_err("Page pfn=[%lu] already GUP'd\n",
		       page_to_pfn(curr_page));
		return -EINVAL;
	}

	count = simple_write_to_buffer(addr_buf, sizeof(addr_buf) - 1, off, in,
				       size);
	if (count <= 0)
		return -EINVAL;

	addr_buf[count] = '\0';

	if (sscanf(addr_buf, "%lu", &uaddr) != 1)
		return -EINVAL;

	if (uaddr & ~PAGE_MASK) {
		pr_err("uaddr=[%lx] is not page-aligned\n", uaddr);
		return -EINVAL;
	}

	pr_info("Attempting to GUP page pfn=[%lu]\n", page_to_pfn(curr_page));

	ret = pin_user_pages_unlocked(uaddr, 1, &curr_page,
				      FOLL_WRITE
#ifdef LONGTERM
				      | FOLL_LONGTERM | FOLL_UNSAFE_FILE_WRITE
#endif
		);
	if (IS_ERR_VALUE(ret)) {
		pr_err("Error [%ld] wheen pinning uaddr=[%lx]\n",
		       ret, uaddr);
		return ret;
	}

	if (ret != 1) {
		pr_err("Failed to pin page for uaddr=[%lx]\n", uaddr);
		return -EINVAL;
	}

	return size;
}

static void unpin_curr_page(void)
{
	pr_info("Unpinning page pfn=[%lu]\n", page_to_pfn(curr_page));

	unpin_user_pages_dirty_lock(&curr_page, 1, true);

	curr_page = NULL;
}

static void write_dirty(void)
{
	unsigned long pfn = page_to_pfn(curr_page);
	char *ptr = page_to_virt(curr_page);

	pr_info("Writing to page pfn=[%lu]\n", pfn);
	ptr[0] = 'x';

	pr_info("Setting page pfn=[%lu] dirty\n", pfn);
	set_page_dirty(curr_page);
}

static const struct file_operations gup_uaddr_fops = {
	.owner = THIS_MODULE,
	.write = gup_uaddr_write
};

static struct miscdevice gup_uaddr_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gup_uaddr",
	.fops = &gup_uaddr_fops
};

static ssize_t gup_event_write(struct file *file, const char __user *in,
			       size_t size, loff_t *off)
{
	char event_buf[10];
	ssize_t count;
	int event;

	if (!curr_page) {
		pr_err("no page to operate on?\n");
		return -EINVAL;
	}
	count = simple_write_to_buffer(event_buf, sizeof(event_buf) - 1, off,
				       in, size);
	if (count <= 0)
		return -EINVAL;
	event_buf[count] = '\0';

	if (sscanf(event_buf, "%d", &event) != 1)
		return -EINVAL;

	switch (event) {
	case 0:
		unpin_curr_page();
		break;
	case 1:
		write_dirty();
		break;
	default:
		pr_err("Unrecognised event %d\n", event);
		return -EINVAL;
	}

	return size;
}

static const struct file_operations gup_event_fops = {
	.owner = THIS_MODULE,
	.write = gup_event_write
};

static struct miscdevice gup_event_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gup_event",
	.fops = &gup_event_fops
};

static int __init mad_init(void)
{
	int err;

	err = misc_register(&gup_uaddr_dev);
	if (err)
		return err;

	return misc_register(&gup_event_dev);
}

static void __exit mad_exit(void)
{
	misc_deregister(&gup_uaddr_dev);
	misc_deregister(&gup_event_dev);

	if (curr_page)
		unpin_curr_page();
}

module_init(mad_init);
module_exit(mad_exit);
