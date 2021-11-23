#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/frontswap.h>
#include <linux/crypto.h>
#include "dpu.h"
#include "snappy_compress.h"
#include "mram.h"
#include "alloc_static.h"
#include <uapi/dpu/dpu_memory.h>
#include <uapi/dpu/dpu_runner.h>
#include <uapi/dpu/dpu_management.h>

#define USE_COMPRESSION
#define NUM_RANKS 1
// offset is divided like this: rank (1 bit) | id (31 bits) | dpu index (6 bits) |
#define DPU_INDEX_FROM_OFFSET(_offset) ((uint8_t)(_offset & ((1 << __DPUS_PER_RANK_LOG2) - 1)))
#define ID_FROM_OFFSET(_offset) ((uint32_t)(_offset >> __DPUS_PER_RANK_LOG2) & 0x7FFFFFFF)
#define RANK_INDEX_FROM_OFFSET(_offset) ((uint8_t)(_offset >> 24))
#define PAGE_VALID 0x80000000

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

// Kernel datastructures for dpus.
static struct dpu_rank_t rank; 
static struct dpu_t dpu;


static __mram_ptr uint8_t *pimswap_alloc_page(unsigned int id, unsigned int length)
{
	__mram_ptr uint8_t *page;

	//printk("%s: id=%u length=%u\n", __func__, id, length);

	page = pimswap_alloc_page_static(id, length);

	return page;
}

static void pimswap_free_page(unsigned int id)
{
	//printk("%s: id=%u\n", __func__, id);

	pimswap_free_page_static(id);
}

static __mram_ptr uint8_t *pimswap_lookup_index(unsigned int id, unsigned int *length)
{
	//printk("%s: id=%u\n", __func__, id);

	return pimswap_lookup_index_hash(id, length);
}

int dpu_invalidate_main(void)
{
	uint32_t id;
	uint32_t length;

	// read page ID
	mram_read(MRAM_VAR(trans_page) + PAGE_SIZE, &id, sizeof(uint32_t));

	if (!(id & PAGE_VALID)) {
		return 0;
	}
	id &= ~PAGE_VALID; // remove valid flag

	printk("[%u] Invalidating id %u\n", get_current_dpu(), id);
	pimswap_free_page(id);

	// the hash entry is not unique, so only remove it if it is the one we are looking for
	if (pimswap_lookup_index_hash(id, &length))
		pimswap_insert_index_hash(id, 0, 0);

	return 0;
}

int lzo_decompress_main(void)
{
	int ret;
	uint32_t new_len = PAGE_SIZE;
	uint32_t compressed_length;
	uint8_t *out_page_w = NULL;
	uint8_t *in_page_w = NULL;
	__mram_ptr uint8_t *in_page_m = NULL;
	uint32_t id;

	//printk("%s\n", __func__);

	// read page ID
	mram_read(MRAM_VAR(trans_page) + PAGE_SIZE, &id, sizeof(uint32_t));

	//printk("[%u] Looking for id %u\n", get_current_dpu(), id);

	// look up page location in MRAM
	in_page_m = pimswap_lookup_index(id, &compressed_length);
	if (!in_page_m) {
		printk("Page %u not found in MRAM!\n", id);
		return -1;
	}

	out_page_w = mem_alloc(PAGE_SIZE);
	if (!out_page_w) {
		printk("ERROR: allocating output page in wram\n");
		return -1;
	}
	in_page_w = mem_alloc(PAGE_SIZE);
	if (!in_page_w) {
		printk("ERROR: allocating input page in wram\n");
		return -1;
	}

	//printk("Reading page from 0x%llx\n", (uint64_t)in_page_m);
	if (compressed_length == PAGE_SIZE) {
		// if it's not compressed, then we are done!
		mram_read(in_page_m, out_page_w, compressed_length);
	} else {
		mram_read(in_page_m, in_page_w, compressed_length);

	//print_hex_dump(KERN_ERR, "before decompress (w): ", DUMP_PREFIX_ADDRESS,
	 //   32, 1, in_page_w, 32, false);

		ret = crypto_comp_decompress(tfm, in_page_w, compressed_length, out_page_w, &new_len);
		if (ret != 0) {
			printk("ERROR: Decompression failed\n");
			return -1;
		}

		if (new_len != PAGE_SIZE) {
			printk("ERROR: decompressed size is wrong\n");
			return -1;
		}
	}

	//print_hex_dump(KERN_ERR, "after decompress (w): ", DUMP_PREFIX_ADDRESS,
	 //   32, 1, out_page_w, 32, false);

	// copy results to transfer page in MRAM
	mram_write(out_page_w, MRAM_VAR(trans_page), PAGE_SIZE);

	return 0;
}

/**
	Entry point for DPU compression
*/
int lzo_compress_main(void)
{
	int ret;
	uint32_t compressed_length = PAGE_SIZE;
	uint8_t *out_page_w = NULL;
	uint8_t *in_page_w = NULL;
	__mram_ptr uint8_t *out_page_m = NULL;
	uint32_t id;
	uint32_t status;

	//printk("%s\n", __func__);

	// read page ID
	mram_read(MRAM_VAR(trans_page) + PAGE_SIZE, &id, sizeof(uint32_t));
	if (!(id & PAGE_VALID)) {
		//printk("[%u] Ignoring invalid page\n", get_current_dpu());
		status = STATUS_OK;
		mram_write(&status, MRAM_VAR(trans_page) + PAGE_SIZE, sizeof(uint32_t));
		return -1;
	}
	id &= ~PAGE_VALID; // remove valid flag
	//printk("[%u] compress got id %u\n", get_current_dpu(), id);

	//printk("in_page: 0x%llx\n", MRAM_VAR(in_page));

	//d_page = vmalloc(PAGE_SIZE);

	out_page_w = mem_alloc(PAGE_SIZE);
	if (!out_page_w) {
		printk("error allocating output page in wram\n");
		status = STATUS_ERROR_STORE_NO_WRAM;
		mram_write(&status, MRAM_VAR(trans_page) + PAGE_SIZE, sizeof(uint32_t));
		return -1;
	}
	in_page_w = mem_alloc(PAGE_SIZE);
	if (!in_page_w) {
		printk("error allocating input page in wram\n");
		status = STATUS_ERROR_STORE_NO_WRAM;
		mram_write(&status, MRAM_VAR(trans_page) + PAGE_SIZE, sizeof(uint32_t));
		return -1;
	}

	// copy the page from MRAM to WRAM
	mram_read(MRAM_VAR(trans_page), in_page_w, PAGE_SIZE);
	//in_page_m = get_current_dpu()->mram + MRAM_VAR(in_page);


//	printk("Compressing from %p to %p (length=%u)\n", in_page_w, out_page, dlen);

#ifdef USE_COMPRESSION
	//print_hex_dump(KERN_ERR, "before compress (w): ", DUMP_PREFIX_ADDRESS,
	 //   32, 1, in_page_w, 32, false);

	ret = crypto_comp_compress(tfm, in_page_w, PAGE_SIZE, out_page_w, &compressed_length);
	if (ret < 0) {
		printk("Compression failed\n");
		compressed_length = PAGE_SIZE;
		out_page_w = in_page_w;
	}

	//printk("LZO: %u\n", compressed_length);

#endif // USE_COMPRESSION

	// don't store pages that are bigger than n-1, where n is the number of blocks needed to store a page.
	// It doesn't save any storage, and it would take time to decompress the results.
	if (compressed_length > (PAGE_SIZE - STORAGE_BLOCK_SIZE)) {
		//printk("Compressed page is too big! (%u)\n", compressed_length);
#ifdef STATISTICS
		stats.uncompressable++;
#endif // STATISTICS
		compressed_length = PAGE_SIZE;
		out_page_w = in_page_w;
	}

	// allocate a spot for it in MRAM
	out_page_m = pimswap_alloc_page(id, compressed_length);

	if (!out_page_m) {
		printk("Can't allocate %u bytes\n", compressed_length);
		status = STATUS_ERROR_STORE_NO_SPACE;
		mram_write(&status, MRAM_VAR(trans_page) + PAGE_SIZE, sizeof(uint32_t));
		return -1;
	}
	
	// copy the page from WRAM to its new location in MRAM
	mram_write(out_page_w, out_page_m, DPU_ALIGN(compressed_length, 8));

#ifdef USE_COMPRESSION
	//printk("Writing page to 0x%llx (length 0x%x)\n", (uint64_t)out_page_m, compressed_length);
	//print_hex_dump(KERN_ERR, "after compress (m): ", DUMP_PREFIX_ADDRESS,
	 //   32, 1, (uint8_t*)((uint64_t)out_page_m + get_dpu(get_current_dpu())->mram), 32, false);
#endif // USE_COMPRESSION

	//printk("%s done\n", __func__);

	status = STATUS_OK;
	mram_write(&status, MRAM_VAR(trans_page) + PAGE_SIZE, sizeof(uint32_t));

	return 0;
}

static void pimswap_frontswap_init(unsigned type)
{
}

/**
	compress and store a single page

	This is called by the mm subsystem when a page needs to be swapped out
*/
 static int pimswap_frontswap_store_old(unsigned type, pgoff_t offset,
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

/*
 * Invoking pim from the kernel. 
 * Currently we get a page and store it to mram which
 * is supplied by the hash function. 
 * @Input:
 *  type - 
 *  offset - offset into the page. 
 *  page - struct that represents the physical page. 
 * @Return - integer representing a successful store or negative on failure.
 */
static int pimswap_frontswap_store_kernel(unsigned type, pgoff_t offset,
				struct page *page) {
    uint8_t dpu_index, rank_index; 
    struct dpu_rank_t; 
    voud * src;
    // Get the rank using the hash function.
    rank_index = RANK_INDEX_FROM_OFFSET(offset);

    // Get the dpu number using the hash function.
    dpu_index = DPU_INDEX_FROM_OFFSET(offset); 

    page_id = ID_FROM_OFFSET(offset);
    
    if(rank_index != 0) {
        printk("ERROR: rank %u doesn't exist!\n", rank_index);
        return -1; 
    }

    // We will try to support all dpus.
     dpu_rank_t = rank;

     src = kmap_atomic(page);
    // src contains the data to copy. 

    kunmap_atomic(src);
    printk("Kernel not implemented yet\n");
    return -1; 

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
	.store = pimswap_frontswap_store_kernel,
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
