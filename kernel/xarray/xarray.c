#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/page_ref.h>
#include <linux/page-flags.h>
#include <linux/pageblock-flags.h>
#include <linux/percpu-defs.h>
#include <linux/xarray.h>

MODULE_AUTHOR("Lorenzo Stoakes <lstoakes@gmail.com>");
MODULE_DESCRIPTION("Experiments");
MODULE_LICENSE("GPL");

DEFINE_XARRAY(xa);

#define NUM_ENTRIES (4096 + 64 + 1)

#define MAX_DEPTH (5)

static char buf[MAX_DEPTH][NUM_ENTRIES + 1];

static void examine_node(int depth, struct xa_node *node)
{
	int i;
	int offset = 0;

	for (i = 0; i < 64; i++) {
		void *entry = node->slots[i];
		if (!entry) {
			buf[depth][offset++] = '.';
		} else if (xa_is_value(entry)) {
			buf[depth][offset++] = '*';
		} else if (xa_is_node(entry)) {
			buf[depth][offset++] = 'N';
			examine_node(depth + 1, xa_to_node(entry));
		}
	}

	pr_err("IVG: %d: %02d: %s\n", depth, node->shift, buf[depth]);
}

static void examine_xa(void)
{
	struct xa_node *node = xa_to_node(xa.xa_head);

	pr_err("IVG: shift=%d\n", node->shift);

	examine_node(0, node);
	pr_err("\n");
}

static int __init xarray_init(void)
{
	int i;
	XA_STATE(xas, &xa, 0);

	for (i = 0; i < NUM_ENTRIES; i++) {
		void *entry;
		int err;

		xas_next(&xas);

		entry = xas_store(&xas, xa_mk_value(i + 1));
		err = xa_err(entry);

		if (entry && !err) {
			pr_err("IVG: Value found at %d, aborting.\n", i);
			return -EINVAL;
		}

		if (err) {
			pr_err("IVG: Error %d at %d\n", err, i);
			return -EINVAL;
		}
	}

	for (i = 0; i < NUM_ENTRIES; i++) {
		unsigned long idx = i;
		void *entry;

		entry = xa_find(&xa, &idx, idx, XA_PRESENT);
		if (!entry) {
			pr_err("IVG: Can't find %d\n", i);
			return -EINVAL;
		}

		if (!xa_is_value(entry)) {
			pr_err("IVG: Entry not a value?\n");
			return -EINVAL;
		}

		if (xa_to_value(entry) != i + 1) {
			pr_err("IVG: %d -> %lu??\n", i, xa_to_value(entry));
			return -EINVAL;
		}
	}

	examine_xa();

	return 0;
}

static void __exit xarray_exit(void)
{

}

module_init(xarray_init);
module_exit(xarray_exit);
