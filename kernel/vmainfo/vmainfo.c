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
MODULE_DESCRIPTION("VMA info");
MODULE_LICENSE("GPL");

static char output_buf[4096] = "";
static int output_buf_size = 1;

static ssize_t vmainfo_read_(struct file *file, char __user *out,
			      size_t size, loff_t *off)
{
	return simple_read_from_buffer(out, size, off, output_buf,
				       output_buf_size);
}

static ssize_t vmainfo_write_(struct file *file, const char __user *in,
			       size_t size, loff_t *off)
{
	char start_buf[255];
	unsigned long start;
	ssize_t count;
	struct vm_area_struct *vma;

	// We always reset the output buffer.
	output_buf[0] = '\0';
	output_buf_size = 1;

	count = simple_write_to_buffer(start_buf, sizeof(start_buf) - 1, off, in, size);
	if (count <= 0)
		return -EINVAL;
	start_buf[count] = '\0';

	if (sscanf(start_buf, "%lu", &start) != 1)
		return -EINVAL;

	vma = find_vma(current->mm, start);
	if (vma == NULL || start < vma->vm_start)
		return -ENXIO;

	output_buf_size += snprintf(output_buf, 4096 - output_buf_size,
				    "start=%lx, end=%lx, flags=%lx",
				    vma->vm_start, vma->vm_end, vma->vm_flags);

	*off = size;
	return size;
}

static const struct file_operations vmainfo_fops = {
	.owner = THIS_MODULE,
	.read = vmainfo_read_,
	.write = vmainfo_write_
};

static struct miscdevice vmainfo_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "vmainfo",
	.fops = &vmainfo_fops
};

static int __init vmainfo_init(void)
{
	return misc_register(&vmainfo_dev);
}

static void __exit vmainfo_exit(void)
{
	misc_deregister(&vmainfo_dev);
}

module_init(vmainfo_init);
module_exit(vmainfo_exit);
