#include "dpu.h"
#include "mram.h"
#include "alloc_static.h"
#include "../common.h"

static __mram_ptr uint8_t *pimswap_lookup_index(unsigned int id, unsigned int *length)
{
	//printk("%s: id=%u\n", __func__, id);

	return pimswap_lookup_index_hash(id, length);
}

int main(void)
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
