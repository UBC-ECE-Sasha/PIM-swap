#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/frontswap.h>

static char *dpu_mem;

static void pimswap_frontswap_init(unsigned type)
{
	pr_err("%s: \n", __func__);
}

/* attempts to compress and store an single page */
static int pimswap_frontswap_store(unsigned type, pgoff_t offset,
				struct page *page)
{
	pr_err("%s: type=%u offset=%u\n", __func__, type, offset);

	/* THP isn't supported */
	if (PageTransHuge(page)) {
		pr_err("%s: can't cache hugepages\n", __func__);
		return -EINVAL;
	}
	return 0;
}

/** Load a compressed page (swap in)
 * returns 0 if the page was successfully decompressed
 * return -1 on entry not found or error
*/
static int pimswap_frontswap_load(unsigned type, pgoff_t offset,
				struct page *page)
{
	pr_err("%s: \n", __func__);
	return -1;
}

/* frees an entry in zswap */
static void pimswap_frontswap_invalidate_page(unsigned type, pgoff_t offset)
{
	pr_err("%s: \n", __func__);
}

/* frees all zswap entries for the given swap type */
static void pimswap_frontswap_invalidate_area(unsigned type)
{
	pr_err("%s: \n", __func__);
}

static struct frontswap_ops pimswap_frontswap_ops = {
	.store = pimswap_frontswap_store,
	.load = pimswap_frontswap_load,
	.invalidate_page = pimswap_frontswap_invalidate_page,
	.invalidate_area = pimswap_frontswap_invalidate_area,
	.init = pimswap_frontswap_init
};

static int __init init_pim_swap(void)
{
	pr_err("%s: started\n", __func__);

	// grab some memory to simulate the DPUs
	dpu_mem = kmalloc(1024 * 1024, GFP_KERNEL);
	if (!dpu_mem) {
		pr_err("%s: can't alloc DPU memory\n", __func__);
		return -1;
	}

	frontswap_register_ops(&pimswap_frontswap_ops);

	pr_err("%s: done\n", __func__);
	return 0;
}

static void __exit exit_pim_swap(void)
{
	pr_info("%s: exit\n", __func__);

	kfree(dpu_mem);
}

module_init(init_pim_swap);
module_exit(exit_pim_swap);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Joel Nider <joel@nider.org>");
MODULE_DESCRIPTION("Compressed cache for swap pages using UPMEM processing-in-memory");
