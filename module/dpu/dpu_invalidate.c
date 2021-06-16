#include "dpu.h"
#include "mram.h"
#include "alloc_static.h"
#include "../common.h"

static void pimswap_free_page(unsigned int id)
{
	//printk("%s: id=%u\n", __func__, id);

	pimswap_free_page_static(id);
}

int main(void)
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
