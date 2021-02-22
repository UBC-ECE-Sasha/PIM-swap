#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/frontswap.h>
#include "dpu.h"

#define NUM_RANKS 1
#define DPU_INDEX_FROM_OFFSET(_offset) ((uint8_t)(_offset & ((1 << 6) - 1)))
#define RANK_INDEX_FROM_OFFSET(_offset) ((uint8_t)(0)) // always rank 0 - for now

typedef struct page_descriptor {
	uint8_t data[PAGE_SIZE];
	uint8_t flags;
} page_descriptor;

static struct dpu_set_t dpus; // all allocated dpus
static page_descriptor buffer[DPUS_PER_RANK];
static uint64_t buffer_bmp;

static void pimswap_frontswap_init(unsigned type)
{
	pr_err("%s: \n", __func__);
}

/* attempts to compress and store a single page */
static int pimswap_frontswap_store(unsigned type, pgoff_t offset,
				struct page *page)
{
	uint8_t dpu_index, rank_index;
	struct dpu_set_t dpu_rank;

	pr_err("store: type=%u offset=%lu\n", type, offset);

	/* THP isn't supported */
	if (PageTransHuge(page)) {
		pr_err("%s: can't cache hugepages\n", __func__);
		return -EINVAL;
	}

	// we batch up writes to the DPUs as much as possible. Since we must
	// write to an entire rank, we buffer (up to) one page per rank.
	// When a page comes in for a DPU that already has a buffered page,
	// we write the whole buffered batch to the rank before starting
	// a new batch with the incoming page.

	// which rank and DPU will store this page
	dpu_index = DPU_INDEX_FROM_OFFSET(offset);
	rank_index = RANK_INDEX_FROM_OFFSET(offset);
	pr_err("dpu: %u rank: %u\n", dpu_index, rank_index);

	// check if that buffer slot is empty
	if (buffer_bmp & (1 << dpu_index)) {
		// already taken - submit the batch
		int dpu_id;
		struct dpu_set_t dpu;
		DPU_FOREACH(dpu_rank, dpu, dpu_id) {
			dpu_prepare_xfer(dpu, &buffer[dpu_id].data);
		}
		dpu_push_xfer(dpu_rank, DPU_XFER_TO_DPU, "in_page", 0, PAGE_SIZE, DPU_XFER_DEFAULT);
		pr_err("launching %u\n", rank_index);
		dpu_launch(dpu_rank, DPU_ASYNCHRONOUS);
		buffer_bmp = 0;
	}

	// store the incoming page
	buffer_bmp |= (1 << dpu_index);
	memcpy(&buffer[dpu_index], page, PAGE_SIZE);
	pr_err("bitmap on rank %u: 0x%x\n", rank_index, buffer_bmp);

	// success!
	return 0;
}

/** Load a compressed page (swap in)
 * returns 0 if the page was successfully decompressed
 * return -1 on entry not found or error
*/
static int pimswap_frontswap_load(unsigned type, pgoff_t offset,
				struct page *page)
{
	dpu_index = DPU_INDEX_FROM_OFFSET(offset);
	rank_index = RANK_INDEX_FROM_OFFSET(offset);
	pr_err("dpu: %u rank: %u\n", dpu_index, rank_index);

	// first check to see if its the rank buffer
	pr_err("bitmap on rank %u: 0x%x\n", rank_index, buffer_bmp);
	if (buffer_bmp & (1 << dpu_index)) {
		pr_err("in buffer for rank %u\n", rank_index);
	}

	pr_err("%s: don't know how to load!\n", __func__);
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

	dpu_alloc(DPU_ALLOCATE_ALL, NULL, &dpus);

	// make sure 'write through' is off - we don't want to wait for slow
	// swap devices.
	frontswap_writethrough(false);

	frontswap_register_ops(&pimswap_frontswap_ops);

	pr_err("%s: done\n", __func__);
	return 0;
}

static void __exit exit_pim_swap(void)
{
	pr_info("%s: exit\n", __func__);

	dpu_free(dpus);
}

module_init(init_pim_swap);
module_exit(exit_pim_swap);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Joel Nider <joel@nider.org>");
MODULE_DESCRIPTION("Compressed cache for swap pages using UPMEM processing-in-memory");
