#include <mram.h>
#include "alloc_static.h"
#include "../common.h"
#include <stdio.h>

uint8_t __mram_noinit trans_page[PAGE_SIZE];
uint32_t __mram_noinit id;
uint32_t __mram_noinit status;
uint8_t __mram_noinit directory[DIRECTORY_SIZE];
uint8_t __mram_noinit storage[STORAGE_SIZE];

static __mram_ptr uint8_t *pimswap_lookup_index(unsigned int id, unsigned int *length)
{
	//printf("%s: id=%u\n", __func__, id);

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

	//printf("%s\n", __func__);

	// read page ID
	mram_read(trans_page + PAGE_SIZE, &id, DPU_ALIGN(sizeof(uint32_t), 8));

	//printf("[%u] Looking for id %u\n", dpu_id, id);

	// look up page location in MRAM
	in_page_m = pimswap_lookup_index(id, &compressed_length);
	if (!in_page_m) {
		printf("Page %u not found in MRAM!\n", id);
		return -1;
	}

	out_page_w = mem_alloc(PAGE_SIZE);
	if (!out_page_w) {
		printf("ERROR: allocating output page in wram\n");
		return -1;
	}
	in_page_w = mem_alloc(PAGE_SIZE);
	if (!in_page_w) {
		printf("ERROR: allocating input page in wram\n");
		return -1;
	}

	//printf("Reading page from 0x%llx\n", (uint64_t)in_page_m);
	if (compressed_length == PAGE_SIZE) {
		// if it's not compressed, then we are done!
		// TODO: JR: multiple reads, PAGE_SIZE too large
		// mram_read(in_page_m, out_page_w, DPU_ALIGN(compressed_length, 8));
	} else {
		mram_read(in_page_m, in_page_w, DPU_ALIGN(compressed_length, 8));

	//print_hex_dump(KERN_ERR, "before decompress (w): ", DUMP_PREFIX_ADDRESS,
	 //   32, 1, in_page_w, 32, false);

		// TODO: JR: Replace kernel function
		ret = 0; // crypto_comp_decompress(tfm, in_page_w, compressed_length, out_page_w, &new_len);
		if (ret != 0) {
			printf("ERROR: Decompression failed\n");
			return -1;
		}

		if (new_len != PAGE_SIZE) {
			printf("ERROR: decompressed size is wrong\n");
			return -1;
		}
	}

	//print_hex_dump(KERN_ERR, "after decompress (w): ", DUMP_PREFIX_ADDRESS,
	 //   32, 1, out_page_w, 32, false);

	// copy results to transfer page in MRAM
	// TODO: JR: multiple writes, PAGE_SIZE too large
	// mram_write(out_page_w, trans_page, PAGE_SIZE);

	return 0;
}
