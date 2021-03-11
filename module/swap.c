#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/frontswap.h>
#include "dpu.h"
#include "snappy_compress.h"

#define NUM_RANKS 1
#define DPU_INDEX_FROM_OFFSET(_offset) ((uint8_t)(_offset & ((1 << 6) - 1)))
#define RANK_INDEX_FROM_OFFSET(_offset) ((uint8_t)(0)) // always rank 0 - for now

// declare DPU entry point
extern dpu_main snappy_main;

// command to select the DPU function
enum {
	COMMAND_STORE,
	COMMAND_LOAD
};

typedef struct page_descriptor {
	uint8_t data[PAGE_SIZE];
} page_descriptor;

static struct dpu_set_t dpus; // all allocated dpus
static page_descriptor in_buffer[DPUS_PER_RANK]; // coming from the DPU
static page_descriptor out_buffer[DPUS_PER_RANK]; // going to the DPU
static uint64_t in_buffer_bmp;
static uint64_t out_buffer_bmp;

static void pimswap_frontswap_init(unsigned type)
{
	pr_err("%s: \n", __func__);
}

/* compress and store a single page */
static int pimswap_frontswap_store(unsigned type, pgoff_t offset,
				struct page *page)
{
	uint8_t dpu_index, rank_index;
	struct dpu_set_t dpu_rank;
	void *src;
	uint32_t dpu_cmd;

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

	dpu_rank = dpus;

	// if that buffer slot is already taken - submit the batch
	if (out_buffer_bmp & (1ULL << dpu_index)) {
		int dpu_id;
		struct dpu_set_t dpu;
		pr_err("buffer is full - flushing\n");
		DPU_FOREACH(dpu_rank, dpu, dpu_id) {
			dpu_prepare_xfer(dpu, &out_buffer[dpu_id].data);
		}

		// copy the data
		dpu_push_xfer(dpu_rank, DPU_XFER_TO_DPU, in_page, 0, PAGE_SIZE, DPU_XFER_DEFAULT);

		// copy the command
		dpu_cmd = COMMAND_STORE;
		dpu_copy_to(dpu_rank, "command", 0, &dpu_cmd, sizeof(uint32_t));
		pr_err("launching %u\n", rank_index);
		dpu_launch(dpu_rank, DPU_SYNCHRONOUS);
		out_buffer_bmp = 0;
	}

	// store the page in the buffer
	out_buffer_bmp |= (1ULL << dpu_index);
	src = kmap_atomic(page);
	memcpy(&out_buffer[dpu_index], src, PAGE_SIZE);
	kunmap_atomic(src);
	pr_err("bitmap on rank %u: 0x%08llx\n", rank_index, out_buffer_bmp);

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
	uint8_t dpu_index, rank_index;
	void *contents;
	void *retrieved;
	uint32_t dpu_cmd;
	struct dpu_set_t dpu_rank;

	dpu_index = DPU_INDEX_FROM_OFFSET(offset);
	rank_index = RANK_INDEX_FROM_OFFSET(offset);
	pr_err("dpu: %u rank: %u\n", dpu_index, rank_index);

	dpu_rank = dpus;

	// check to see if its in the outgoing or incoming rank buffer
	pr_err("bitmap on rank %u: 0x%08llx\n", rank_index, out_buffer_bmp);
	if (out_buffer_bmp & (1ULL << dpu_index)) {
		pr_err("in out buffer for rank %u\n", rank_index);
		retrieved = &out_buffer[dpu_index];
	} else if (in_buffer_bmp & (1ULL << dpu_index)) {
		pr_err("in in buffer for rank %u\n", rank_index);
		retrieved = &in_buffer[dpu_index];
	}

	// start reading specutively from the DPU in case there is a hit
	dpu_cmd = COMMAND_LOAD;
	dpu_copy_to(dpu_rank, "command", 0, &dpu_cmd, sizeof(uint32_t));
	dpu_launch(dpu_rank, DPU_SYNCHRONOUS);

	// now check the stored pages in case there is a miss

	contents = kmap_atomic(page);
	memcpy(contents, retrieved, PAGE_SIZE);
	kunmap_atomic(contents);

	pr_err("%s: don't know how to load!\n", __func__);
	return -1;
}

/* frees an entry in zswap */
static void pimswap_frontswap_invalidate_page(unsigned type, pgoff_t offset)
{
	pr_err("%s: offset %lu\n", __func__, offset);
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

	if (dpu_alloc(DPU_ALLOCATE_ALL, NULL, &dpus) != DPU_OK) {
		pr_err("error in dpu_alloc\n");
		return -1;
	}

	pr_err("loading\n");
/*
	if (dpu_load(dpus, snappy_main, NULL) != DPU_OK) {
		pr_err("error in dpu_load\n");
		return -2;
	}
*/

	// make sure 'write through' is off - we don't want to wait for slow
	// swap devices.
	frontswap_writethrough(false);

	frontswap_register_ops(&pimswap_frontswap_ops);

	pr_err("%s: done\n", __func__);
	return 0;
}

static void __exit exit_pim_swap(void)
{
	pr_err("%s: exit\n", __func__);

	dpu_free(dpus);
}

module_init(init_pim_swap);
module_exit(exit_pim_swap);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Joel Nider <joel@nider.org>");
MODULE_DESCRIPTION("Compressed cache for swap pages using UPMEM processing-in-memory");
