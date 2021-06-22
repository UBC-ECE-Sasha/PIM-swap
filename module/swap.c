#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/frontswap.h>
#include <linux/crypto.h>

#define USE_COMPRESSION
#define NUM_RANKS 1
// offset is divided like this: rank (1 bit) | id (31 bits) | dpu index (6 bits) |
#define DPU_INDEX_FROM_OFFSET(_offset) ((uint8_t)(_offset & ((1 << __DPUS_PER_RANK_LOG2) - 1)))
#define ID_FROM_OFFSET(_offset) ((uint32_t)(_offset >> __DPUS_PER_RANK_LOG2) & 0x7FFFFFFF)
#define RANK_INDEX_FROM_OFFSET(_offset) ((uint8_t)(_offset >> 24))

// command to select the DPU function
enum {
	COMMAND_STORE,
	COMMAND_LOAD
};

enum {
	STATUS_OK,
	STATUS_ERROR_STORE_NO_WRAM,
	STATUS_ERROR_STORE_NO_SPACE
};

typedef struct page_descriptor {
	uint8_t data[PAGE_SIZE];
	uint32_t id;	// page identifier - high bit is used as 'page valid'
	uint32_t status;
} page_descriptor;

static struct dpu_set_t dpus; // all allocated dpus
static page_descriptor in_buffer[DPUS_PER_RANK]; // coming from the DPU
static page_descriptor out_buffer[DPUS_PER_RANK]; // going to the DPU
static uint64_t out_buffer_bmp;
struct crypto_comp *tfm;
static char *pimswap_compressor = "lzo";

static void pimswap_frontswap_init(unsigned type)
{
}

/**
	compress and store a single page

	This is called by the mm subsystem when a page needs to be swapped out
*/
static int pimswap_frontswap_store(unsigned type, pgoff_t offset,
				struct page *page)
{
	uint8_t dpu_index, rank_index;
	struct dpu_set_t dpu_rank;
	void *src;

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
	//pr_err("dpu: %u rank: %u\n", dpu_index, rank_index);
	if (rank_index != 0) {
		printk("ERROR: rank %u doesn't exist!\n", rank_index);
		return -ENOSPC;
	}

	// TEMPORARY - ignore anything that isn't for DPU 0
	if (dpu_index != 0)
		return -ENOSPC;

	//pr_err("store: offset=%lu\n", offset);

	dpu_rank = dpus;

	// if that buffer slot is already taken - submit the batch
	if (out_buffer_bmp & (1ULL << dpu_index)) {
		uint8_t dpu_id;
		struct dpu_set_t dpu;

		DPU_FOREACH(dpu_rank, dpu, dpu_id) {
			//printk("Preparing xfer from %p for dpu %u\n", out_buffer[dpu_id].data, dpu_id);
			dpu_prepare_xfer(dpu, out_buffer[dpu_id].data);
			//print_hex_dump(KERN_ERR, "page: ", DUMP_PREFIX_ADDRESS,
			//	32, 1, out_buffer[dpu_id].data, 32, false);
		}

		// copy the data
		dpu_push_xfer(dpu_rank, DPU_XFER_TO_DPU, trans_page, 0, sizeof(page_descriptor), DPU_XFER_DEFAULT);

		// load and execute the program
		dpu_load(dpu_rank, lzo_compress_main, NULL);
		dpu_launch(dpu_rank, DPU_SYNCHRONOUS);
		out_buffer_bmp = 0;

		dpu_push_xfer(dpu_rank, DPU_XFER_FROM_DPU, trans_page, 0, sizeof(page_descriptor), DPU_XFER_DEFAULT);

		// check return status
		DPU_FOREACH(dpu_rank, dpu, dpu_id) {
			out_buffer[dpu_id].id = 0;
		}
	}

	// store the page in the buffer
	out_buffer_bmp |= (1ULL << dpu_index);
	src = kmap_atomic(page);
	memcpy(out_buffer[dpu_index].data, src, PAGE_SIZE);
	kunmap_atomic(src);

	out_buffer[dpu_index].id = ID_FROM_OFFSET(offset) | PAGE_VALID;
	//printk("Storing id %u\n", ID_FROM_OFFSET(offset));

	//printk("Writing page to %p for dpu %u\n", out_buffer[dpu_index].data, dpu_index);
	//	print_hex_dump(KERN_ERR, "page: ", DUMP_PREFIX_ADDRESS,
	//	    32, 1, out_buffer[dpu_index].data, 32, false);
	//pr_err("bitmap on rank %u: 0x%08llx\n", rank_index, out_buffer_bmp);

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
	struct dpu_set_t dpu_rank;
	uint8_t dpu_id;
	struct dpu_set_t dpu, selected_dpu;
	uint32_t page_id;

	dpu_index = DPU_INDEX_FROM_OFFSET(offset);
	rank_index = RANK_INDEX_FROM_OFFSET(offset);
	page_id = ID_FROM_OFFSET(offset);
	if (rank_index != 0) {
		printk("ERROR: rank %u doesn't exist!\n", rank_index);
		return -1;
	}

	// TEMPORARY - ignore anything that isn't for DPU 0
	if (dpu_index != 0)
		return -ENOSPC;

	//pr_err("load: offset=%lu\n", offset);
	//printk("%s: offset: %lu dpu: %u rank: %u id: %u\n", __func__, offset, dpu_index, rank_index, page_id);

	// select which rank to address
	dpu_rank = dpus;

	// check to see if its in the outgoing or incoming rank buffer (although not likely)
	//pr_err("bitmap on rank %u: 0x%08llx\n", rank_index, out_buffer_bmp);
	if (out_buffer_bmp & (1ULL << dpu_index) && out_buffer[dpu_index].id == page_id) {
		printk("using out buffer for rank %u\n", rank_index);
		retrieved = &out_buffer[dpu_index];
	//} else if (in_buffer_bmp & (1ULL << dpu_index) && in_buffer[dpu_index].id == page_id) {
	} else if (in_buffer[dpu_index].id == page_id) {
		printk("[%u] found id %u using in buffer\n", get_current_dpu(), page_id);
		retrieved = &in_buffer[dpu_index];
	} else {
		// set the page id that we are looking for, only in the correct DPU
		DPU_FOREACH(dpu_rank, dpu, dpu_id) {
			if (dpu_id == dpu_index) {
				in_buffer[dpu_id].id = page_id;
				selected_dpu = dpu;
			}
			else
				in_buffer[dpu_id].id = 0;
			dpu_prepare_xfer(dpu, &in_buffer[dpu_id].id);
		}
		dpu_push_xfer(dpu_rank, DPU_XFER_TO_DPU, trans_page, PAGE_SIZE, sizeof(in_buffer[dpu_id].id), DPU_XFER_DEFAULT);

		dpu_load(dpu_rank, lzo_decompress_main, NULL);

		//dpu_launch(dpu_rank, DPU_SYNCHRONOUS);
		dpu_launch(selected_dpu, DPU_SYNCHRONOUS);

		// gather results into 'in' buffer
		DPU_FOREACH(dpu_rank, dpu, dpu_id) {
			dpu_prepare_xfer(dpu, in_buffer[dpu_id].data);
		}
		dpu_push_xfer(dpu_rank, DPU_XFER_FROM_DPU, trans_page, 0, sizeof(page_descriptor), DPU_XFER_DEFAULT);

		if (in_buffer[dpu_index].id != ID_FROM_OFFSET(offset)) {
			printk("Error retrieving buffer for offset %lu\n", offset);
			return -1;
		}
		retrieved = &in_buffer[dpu_index];
	}

//		print_hex_dump(KERN_ERR, "page read: ", DUMP_PREFIX_ADDRESS,
//		    32, 1, retrieved, 32, false);

	contents = kmap_atomic(page);
	memcpy(contents, retrieved, PAGE_SIZE);
	kunmap_atomic(contents);

	return 0;
}

/* frees an entry in zswap */
static void pimswap_frontswap_invalidate_page(unsigned type, pgoff_t offset)
{
	uint8_t dpu_index;
	uint8_t rank_index;
	uint8_t dpu_id;
	struct dpu_set_t dpu_rank, dpu;
	uint32_t invalidate_id;
	uint32_t invalidate_id_dummy=0;

	dpu_index = DPU_INDEX_FROM_OFFSET(offset);
	rank_index = RANK_INDEX_FROM_OFFSET(offset);
	if (rank_index != 0) {
		printk("ERROR: rank %u doesn't exist!\n", rank_index);
	}

	// TEMPORARY - ignore anything that isn't for DPU 0
	if (dpu_index != 0)
		return;

	//pr_err("invalidate: offset=%lu\n", offset);

	// maybe this page is sitting in the out buffer?
	//printk("Out buffer: 0x%llx\n", out_buffer_bmp);
	//printk("id %u for dpu %u\n", out_buffer[dpu_index].id & ~PAGE_VALID, dpu_index);
	if (out_buffer_bmp & (1ULL << dpu_index) && (out_buffer[dpu_index].id & ~PAGE_VALID) == ID_FROM_OFFSET(offset)) {
		//printk("invalidating page in out buffer\n");
		out_buffer_bmp &= ~(1ULL << dpu_index);
		return;
	}

	//printk("%s: offset: %lu dpu: %u rank: %u id: %u\n", __func__, offset, dpu_index, rank_index, ID_FROM_OFFSET(offset));

	// select which rank to address
	dpu_rank = dpus;

	DPU_FOREACH(dpu_rank, dpu, dpu_id) {
		if (dpu_id == dpu_index) {
			invalidate_id = ID_FROM_OFFSET(offset) | PAGE_VALID;
			dpu_prepare_xfer(dpu, &invalidate_id);
		}
		else
			dpu_prepare_xfer(dpu, &invalidate_id_dummy);
	}
	// copy the page id's to invalidate
	dpu_push_xfer(dpu_rank, DPU_XFER_TO_DPU, trans_page, offsetof(struct page_descriptor, id), sizeof(uint32_t), DPU_XFER_DEFAULT);

	dpu_load(dpu_rank, dpu_invalidate_main, NULL);

	dpu_launch(dpu_rank, DPU_SYNCHRONOUS);
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

		// make sure it is zeroed
		//memset(dpus[i].mram, 0, MRAM_PER_DPU);

/* Load once if the program has compress + decompress combined
	Right now, they are separate entry points, so are loaded individually
	pr_err("loading\n");
	if (dpu_load(dpus, snappy_main, NULL) != DPU_OK) {
		pr_err("error in dpu_load\n");
		return -2;
	}
*/

   tfm = crypto_alloc_comp(pimswap_compressor, 0, 0);
   if (IS_ERR(tfm)) {
      printk("could not alloc crypto comp %s : %ld\n",
             pimswap_compressor, PTR_ERR(tfm));
      return PTR_ERR(tfm);
   }

	// make sure 'write through' is off - we don't want to wait for slow
	// swap devices.
	frontswap_writethrough(false);

	frontswap_register_ops(&pimswap_frontswap_ops);

	printk("DPUs per rank: %u\n", DPUS_PER_RANK);
	printk("MRAM size: %d\n", MRAM_PER_DPU);
	printk("MRAM layout:\n");
	printk("0x%04lx - 0x%4lx: reserved\n", offsetof(struct mram_layout, reserved), offsetofend(struct mram_layout, reserved));
	printk("0x%04lx - 0x%4lx: transfer page\n", offsetof(struct mram_layout, trans_page), offsetofend(struct mram_layout, trans_page));
	printk("0x%04lx - 0x%4lx: directory\n", offsetof(struct mram_layout, directory), offsetof(struct mram_layout, directory) + DIRECTORY_SIZE);
	printk("0x%04lx - 0x%4lx: storage\n", offsetof(struct mram_layout, storage), offsetof(struct mram_layout, storage) + STORAGE_SIZE);

	printk("Total storage space: %d bytes\n", STORAGE_SIZE);
	printk("Allocation table size: %lu\n", ALLOC_TABLE_SIZE);
	printk("Storage block size: %d\n", STORAGE_BLOCK_SIZE);
	printk("Total storage blocks: %d\n", NUM_STORAGE_BLOCKS);
	printk("Number of L1 entries: %d\n", ALLOC_TABLE_L1_ENTRIES);
	printk("Number of L2 entries: %d\n", ALLOC_TABLE_L2_ENTRIES);
	printk("Number of L2 sections: %d\n", NUM_L2_SECTIONS);
#ifdef DIRECTORY_HASH
	printk("Using a hash table with %lu entries\n", NUM_HASH_ENTRIES);
#endif // DIRECTORY_HASH

	pr_err("%s: done\n", __func__);
	return 0;
}

static void __exit exit_pim_swap(void)
{
	pr_err("%s: exit\n", __func__);

	dpu_free(dpus);

   crypto_free_comp(tfm);
}

module_init(init_pim_swap);
module_exit(exit_pim_swap);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Joel Nider <joel@nider.org>");
MODULE_DESCRIPTION("Compressed cache for swap pages using UPMEM processing-in-memory");
