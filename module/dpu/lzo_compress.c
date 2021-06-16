#include "dpu.h"
#include "mram.h"
#include "alloc_static.h"
#include "../common.h"

static __mram_ptr uint8_t *pimswap_alloc_page(unsigned int id, unsigned int length)
{
	__mram_ptr uint8_t *page;

	//printk("%s: id=%u length=%u\n", __func__, id, length);

	page = pimswap_alloc_page_static(id, length);

	return page;
}

/**
	Entry point for DPU compression
*/
int main(void)
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
