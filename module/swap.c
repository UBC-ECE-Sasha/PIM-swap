#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/frontswap.h>
#include <linux/crypto.h>
// #include "snappy_compress.h"
#include <uapi/dpu_memory.h>
#include <uapi/dpu_runner.h>
#include <uapi/dpu_management.h>

#define USE_COMPRESSION
#define NUM_RANKS 1
// offset is divided like this: rank (1 bit) | id (31 bits) | dpu index (6 bits) |
// TODO just changed DPU_PER_RANK_LOG2 to a constant for now.
#define DPU_INDEX_FROM_OFFSET(_offset) ((uint8_t)(_offset & ((1 << 2) - 1)))
#define ID_FROM_OFFSET(_offset) ((uint32_t)(_offset >> 2) & 0x7FFFFFFF)
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

struct allocated_dpu_page_node {
    int offset_in_mram;
    int sz; 
    int id;
    struct allocated_dpu_page_node* next; 
};

static struct allocated_dpu_page_node *head, *curr;
struct dpu_transfer_mram *xfer; 
struct allocated_dpu_page_node *rank_0;
static int mram_offset = 0;
typedef struct page_descriptor {
	uint8_t data[PAGE_SIZE];
	uint32_t id;	// page identifier - high bit is used as 'page valid'
	uint32_t status;
} page_descriptor;

// Kernel datastructures for dpus.
static struct dpu_rank_t *rank;
static uint64_t out_buffer_bmp;
static struct dpu_t *dpu;
static page_descriptor inbuffer[MAX_NR_DPUS_PER_RANK]; // coming from DPU.
static page_descriptor *outbuffer; // going to DPU. 

/*int lzo_decompress_main(void)
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
} */

/**
	Entry point for DPU compression
*/
/*int lzo_compress_main(void)
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
	 //   32, 1, (vint8_t*)((uint64_t)out_page_m + get_dpu(get_current_dpu())->mram), 32, false);
#endif // USE_COMPRESSION

	//printk("%s done\n", __func__);

	status = STATUS_OK;
	mram_write(&status, MRAM_VAR(trans_page) + PAGE_SIZE, sizeof(uint32_t));

	return 0;
}*/

static void pimswap_frontswap_init(unsigned type)
{
    rank_0 = vmalloc(sizeof(struct allocated_dpu_page_node));
    rank_0->offset_in_mram = 0; 
    rank_0->sz = 64*1024*1024;
    rank_0->page = NULL;
    rank_0->next = NULL;
  int status = dpu_rank_alloc(&rank);
    if(status != 0) {
        printk("Unable to get rank!\n");
        return; 
    } 
  
  status = dpu_reset_rank(rank); 
    
    if(status != 0) {
	printk("DPU reset rank failed, err code: %d\n", status);
	return;
    } 
    
    out_buffer_bmp = 0; 
    int i = 0;
    outbuffer = vmalloc(sizeof(struct page_descriptor) * MAX_NR_DPUS_PER_RANK); 
    for(i = 0; i < MAX_NR_DPUS_PER_RANK; i++) {
        outbuffer[i].id = 0;
    }
   
    printk("Got rank!\n");

   xfer = vmalloc(sizeof(struct dpu_transfer_mram)); 
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
static int pimswap_frontswap_store(unsigned type, pgoff_t offset,
				struct page *page) {
    
    uint8_t dpu_index, rank_index; 
    void * src;
    uint32_t page_id; 
    struct allocated_dpu_page_node *current_node = rank_0; 
    int status;
    struct dpu_t *dpu;

    if(mram_offset >= 64*1024*1024) 
        return -1;

    // Get the rank using the hash function.
    rank_index = RANK_INDEX_FROM_OFFSET(offset);

    // Get the dpu number using the hash function.
    dpu_index = DPU_INDEX_FROM_OFFSET(offset); 

    page_id = ID_FROM_OFFSET(offset);
     
    if(page == NULL) 
	   return -1;  
    // TODO - for now stick to rank 0.
    if(rank_index != 0) {
        printk("ERROR: rank %u doesn't exist!\n", rank_index);
        return -1; 
    }

    if(dpu_index != 1) {
        printk("ERROR: dpu %u doesn't exist!\n", dpu_index);
        return -1; 
    }

    printk("Found page to support\n");

    if(out_buffer_bmp & (1ULL<<dpu_index)) {
        printk("Pushing the pages\n");
        int nb_dpus = dpu_get_number_of_dpus_for_rank(rank);
	    int i = 0;
        // TOOD(shaurp): This might be inefficient, change this.
        for(i = 0; i < nb_dpus; i++) {
            int ci_id = i / 8; 
            int dpu_id = i % 8;

            if(outbuffer[i].id != 0) {
                struct dpu_t *dpu = dpu_get(rank, ci_id, dpu_id);
                dpu_transfer_matrix_add_dpu(dpu, xfer, outbuffer[i].data);
                struct allocated_dpu_page_node *new = vmalloc(sizeof(struct allocated_dpu_page_node));
                new->id = outbuffer[i].id; 
                new->next = NULL; 
                new->offset_in_mram = mram_offset;
                new->sz = 4096;
                if(head == NULL) {
                    head = new;
                    curr = new; 
                } else {
                    curr->next = new;
                    curr = new; 
                }
                outbuffer[i].id = 0;
                mram_offset += 4096; 
            }
        }
	    printk("Transferring to dpus\n");
	    status = dpu_copy_to_mrams(rank, xfer, PAGE_SIZE, 0);
        
        if(status != 0) {
            printk("Failed to copy to MRAM\n");
            return -1;
        } 
        struct allocated_dpu_page_node *new = vmalloc(sizeof(struct allocated_dpu_page_node));

        
        printk("Successfully copied to MRAM\n");

        out_buffer_bmp = 0;
        
    }
   // Page doesn't exist.
    out_buffer_bmp |= (1ULL << dpu_index);
    src = kmap_atomic(page);

    memcpy(outbuffer[dpu_index].data, src, PAGE_SIZE);

    outbuffer[dpu_index].id = ID_FROM_OFFSET(offset) | PAGE_VALID;
    // Naive impl, store the offset into a list.
    kunmap_atomic(src);
    return 0; 
}

/** Load a compressed page (swap in)
 * returns 0 if the page was successfully decompressed
 * return -1 on entry not found or error
*/
static int pimswap_frontswap_load(unsigned type, pgoff_t offset,
				struct page *page)
{
    // Find the page offset and do mram read. 
    uint8_t dpu_index, rank_index; 
    void * src;
    uint32_t page_id; 
    struct allocated_dpu_page_node *current_node = rank_0; 
    int status;
    struct dpu_t *dpu;
    int offset_in_mram = 0;
    // Get the rank using the hash function.
    rank_index = RANK_INDEX_FROM_OFFSET(offset);

    // Get the dpu number using the hash function.
    dpu_index = DPU_INDEX_FROM_OFFSET(offset); 

    page_id = ID_FROM_OFFSET(offset);
    
    if(rank_index != 0) 
        return -1; 
    
    if(dpu_index != 1) 
        return -1; 

    printk("Loading a page from pimswap\n");
    struct allocated_dpu_page_node *list = head; 
    while(list != NULL) {
        if(list->id == page_id) {
            offset_in_mram = list->offset_in_mram;
            break;
        }
        list = list->next;
    }

    if(list == NULL) 
        return -1; 


    // DPU_index is same as i here.
    dpu = dpu_get(rank, dpu_index/8, dpu_index%8);

    src = kmap_atomic(page);
    status = dpu_copy_from_mram(dpu, src, offset_in_mram, PAGE_SIZE);
    if(status != 0) {
        printk("Read didn't work\n");
        kunmap_atomic(src);
        return -1; 
    }
    printk("Load successful!\n");
    kunmap_atomic(src);
	return 0;
}

/* frees an entry in zswap */
static void pimswap_frontswap_invalidate_page(unsigned type, pgoff_t offset)
{
    return; 
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
	.invalidate_area = NULL,
	.init = pimswap_frontswap_init
};

static int __init init_pim_swap(void)
{
	pr_err("%s: started\n", __func__);

    // TODO change with kernel API 
    // functions - dpu_rank_alloc
    // dpu_get_number_of_dpus_for_rank 
	/* if (dpu_alloc(DPU_ALLOCATE_ALL, NULL, &dpus) != DPU_OK) {
		pr_err("error in dpu_alloc\n");
		return -1;
	} */

    // TODO - do we need this? I would assume that's a no.
    /*tfm = crypto_alloc_comp(pimswap_compressor, 0, 0);
    if (IS_ERR(tfm)) {
      printk("could not alloc crypto comp %s : %ld\n",
             pimswap_compressor, PTR_ERR(tfm));
      return PTR_ERR(tfm);
   } */

	// make sure 'write through' is off - we don't want to wait for slow
	// swap devices.
	frontswap_writethrough(false);

	frontswap_register_ops(&pimswap_frontswap_ops);

	// printk("DPUs per rank: %u\n", DPUS_PER_RANK);
	// printk("MRAM size: %d\n", MRAM_PER_DPU);
	printk("MRAM layout:\n");
	// printk("0x%04lx - 0x%4lx: reserved\n", offsetof(struct mram_layout, reserved), offsetofend(struct mram_layout, reserved));
	// printk("0x%04lx - 0x%4lx: transfer page\n", offsetof(struct mram_layout, trans_page), offsetofend(struct mram_layout, trans_page));
	// printk("0x%04lx - 0x%4lx: directory\n", offsetof(struct mram_layout, directory), offsetof(struct mram_layout, directory) + DIRECTORY_SIZE);
	// printk("0x%04lx - 0x%4lx: storage\n", offsetof(struct mram_layout, storage), offsetof(struct mram_layout, storage) + STORAGE_SIZE);

	// printk("Total storage space: %d bytes\n", STORAGE_SIZE);
	// printk("Allocation table size: %lu\n", ALLOC_TABLE_SIZE);
	// printk("Storage block size: %d\n", STORAGE_BLOCK_SIZE);
	// printk("Total storage blocks: %d\n", NUM_STORAGE_BLOCKS);
	// printk("Number of L1 entries: %d\n", ALLOC_TABLE_L1_ENTRIES);
	// printk("Number of L2 entries: %d\n", ALLOC_TABLE_L2_ENTRIES);
	// printk("Number of L2 sections: %d\n", NUM_L2_SECTIONS);
#ifdef DIRECTORY_HASH
	printk("Using a hash table with %lu entries\n", NUM_HASH_ENTRIES);
#endif // DIRECTORY_HASH

	pr_err("%s: done\n", __func__);
	return 0;
}

static void __exit exit_pim_swap(void)
{
	pr_err("%s: exit\n", __func__);

	// dpu_free(dpus);

    // crypto_free_comp(tfm);
}

module_init(init_pim_swap);
module_exit(exit_pim_swap);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Joel Nider <joel@nider.org>");
MODULE_DESCRIPTION("Compressed cache for swap pages using UPMEM processing-in-memory");
