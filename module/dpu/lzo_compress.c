#include <mram.h>
#include "alloc_static.h"
#include "../common.h"
#include <stdio.h>

#define STATUS_OK 0
#define STATUS_ERROR_STORE_NO_WRAM 1
#define STATUS_ERROR_STORE_NO_SPACE 2

uint8_t __mram_noinit trans_page[PAGE_SIZE];
uint32_t __mram_noinit id;
uint32_t __mram_noinit status;
uint8_t __mram_noinit directory[DIRECTORY_SIZE];
uint8_t __mram_noinit storage[STORAGE_SIZE];

static __mram_ptr uint8_t *pimswap_alloc_page(unsigned int id, unsigned int length)
{
	__mram_ptr uint8_t *page;

	//printf("%s: id=%u length=%u\n", __func__, id, length);

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

	//printf("%s\n", __func__);

	// read page ID
	mram_read(trans_page + PAGE_SIZE, &id, DPU_ALIGN(sizeof(uint32_t), 8));
	if (!(id & PAGE_VALID)) {
		//printf("[%u] Ignoring invalid page\n", dpu_id);
		status = STATUS_OK;
		mram_write(&status, trans_page + PAGE_SIZE, DPU_ALIGN(sizeof(uint32_t), 8));
		return -1;
	}
	id &= ~PAGE_VALID; // remove valid flag
	//printf("[%u] compress got id %u\n", dpu_id, id);

	//printf("in_page: 0x%llx\n", in_page);

	//d_page = vmalloc(PAGE_SIZE);

	out_page_w = mem_alloc(PAGE_SIZE);
	if (!out_page_w) {
		printf("error allocating output page in wram\n");
		status = STATUS_ERROR_STORE_NO_WRAM;
		mram_write(&status, trans_page + PAGE_SIZE, DPU_ALIGN(sizeof(uint32_t), 8));
		return -1;
	}
	in_page_w = mem_alloc(PAGE_SIZE);
	if (!in_page_w) {
		printf("error allocating input page in wram\n");
		status = STATUS_ERROR_STORE_NO_WRAM;
		mram_write(&status, trans_page + PAGE_SIZE, DPU_ALIGN(sizeof(uint32_t), 8));
		return -1;
	}

	// copy the page from MRAM to WRAM
    uint32_t remaining = PAGE_SIZE;
    uint8_t *in_page_w_loc = in_page_w;
    uint8_t __mram_ptr *trans_page_loc = trans_page;
    while (remaining > 0) {
        size_t read_sz = DPU_ALIGN(remaining > MAX_MRAM_RW ? MAX_MRAM_RW : remaining, 8);
        mram_read(trans_page_loc, in_page_w_loc, read_sz);
        in_page_w_loc += read_sz;
        trans_page_loc += read_sz;
        remaining -= MAX_MRAM_RW;
    }

	//in_page_m = dpu_id->mram + in_page;


//	printf("Compressing from %p to %p (length=%u)\n", in_page_w, out_page, dlen);

#ifdef USE_COMPRESSION
	//print_hex_dump(KERN_ERR, "before compress (w): ", DUMP_PREFIX_ADDRESS,
	 //   32, 1, in_page_w, 32, false);

	// TODO: JR: Replace kernel function
	ret = 0; // crypto_comp_compress(tfm, in_page_w, PAGE_SIZE, out_page_w, &compressed_length);
	if (ret < 0) {
		printf("Compression failed\n");
		compressed_length = PAGE_SIZE;
		out_page_w = in_page_w;
	}

	//printf("LZO: %u\n", compressed_length);

#endif // USE_COMPRESSION

	// don't store pages that are bigger than n-1, where n is the number of blocks needed to store a page.
	// It doesn't save any storage, and it would take time to decompress the results.
	if (compressed_length > (PAGE_SIZE - STORAGE_BLOCK_SIZE)) {
		//printf("Compressed page is too big! (%u)\n", compressed_length);
#ifdef STATISTICS
		stats.uncompressable++;
#endif // STATISTICS
		compressed_length = PAGE_SIZE;
		out_page_w = in_page_w;
	}

	// allocate a spot for it in MRAM
	out_page_m = pimswap_alloc_page(id, compressed_length);

	if (!out_page_m) {
		printf("Can't allocate %u bytes\n", compressed_length);
		status = STATUS_ERROR_STORE_NO_SPACE;
		mram_write(&status, trans_page + PAGE_SIZE, DPU_ALIGN(sizeof(uint32_t), 8));
		return -1;
	}

	// copy the page from WRAM to its new location in MRAM
    remaining = compressed_length;
    uint8_t *out_page_w_loc = out_page_w;
    uint8_t __mram_ptr *out_page_m_loc = out_page_m;
    while (remaining > 0) {
        size_t write_sz = DPU_ALIGN(remaining > MAX_MRAM_RW ? MAX_MRAM_RW : remaining, 8);
        mram_write(out_page_w_loc, out_page_m_loc, write_sz);
        out_page_w_loc += write_sz;
        out_page_m_loc += write_sz;
        remaining -= MAX_MRAM_RW;
    }


#ifdef USE_COMPRESSION
	//printf("Writing page to 0x%llx (length 0x%x)\n", (uint64_t)out_page_m, compressed_length);
	//print_hex_dump(KERN_ERR, "after compress (m): ", DUMP_PREFIX_ADDRESS,
	 //   32, 1, (uint8_t*)((uint64_t)out_page_m + get_dpu(dpu_id)->mram), 32, false);
#endif // USE_COMPRESSION

	//printf("%s done\n", __func__);

	status = STATUS_OK;
	mram_write(&status, trans_page + PAGE_SIZE, DPU_ALIGN(sizeof(uint32_t), 8));

	return 0;
}
