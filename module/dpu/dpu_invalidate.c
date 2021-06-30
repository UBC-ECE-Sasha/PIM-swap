#include <mram.h>
#include "alloc_static.h"
#include "../common.h"
#include <stdio.h>

// TODO, set from host
__host uint32_t dpu_id;

uint8_t __mram_noinit trans_page[PAGE_SIZE];
uint32_t __mram_noinit id;
uint32_t __mram_noinit status;
uint8_t __mram_noinit directory[DIRECTORY_SIZE];
uint8_t __mram_noinit storage[STORAGE_SIZE];

static void pimswap_free_page(unsigned int id)
{
	//printf("%s: id=%u\n", __func__, id);

	pimswap_free_page_static(id);
}

int main(void)
{
	uint32_t id;
	uint32_t length;

	// read page ID
	mram_read(trans_page + PAGE_SIZE, &id, DPU_ALIGN(sizeof(uint32_t), 8));

	if (!(id & PAGE_VALID)) {
		return 0;
	}
	id &= ~PAGE_VALID; // remove valid flag

	printf("[%u] Invalidating id %u\n", dpu_id, id);
	pimswap_free_page(id);

	// the hash entry is not unique, so only remove it if it is the one we are looking for
	if (pimswap_lookup_index_hash(id, &length))
		pimswap_insert_index_hash(id, 0, 0);

	return 0;
}
