#include <mram.h>
#include "snappy_compress.h" // for mram layout
#include "alloc_static.h"
#include <stdio.h>
#include <string.h>

#ifdef STATISTICS
void print_statistics(void)
{
	printf("STATS allocs: %llu frees: %llu blocks: %u  bytes: %u pages: %llu\n",
		stats.num_allocs,
		stats.num_frees,
		stats.current_blocks_allocated,
		stats.current_bytes_used,
		stats.current_pages);
}
#endif // STATISTICS

void dump_l1_bmp(uint8_t *l1_entries)
{
	char entry[8];
	char temp[256];
	int i;

	temp[0] = 0;

	strcat(temp, "L1: ");
	for (i = ALLOC_TABLE_L1_ENTRIES-1; i >= 0; i++) {
		snprintf(entry, 8, "%02x ", l1_entries[i]);
		strncat(temp, entry, 256);
	}
	printf(temp);
}

void dump_l2_section_bmp(uint32_t section, uint8_t *l2)
{
	uint8_t l2_entries[ALLOC_TABLE_L2_ENTRIES_PER_SECTION];
	uint64_t *ptr;
	__mram_ptr uint8_t *section_addr;

	if (l2) {
		ptr = (uint64_t*)l2;
		printf("section %u: %08llx %08llx %08llx %08llx %08llx %08llx %08llx %08llx", section, ptr[7], ptr[6], ptr[5], ptr[4], ptr[3], ptr[2], ptr[1], ptr[0]);
		return;
	}

	for (section=0; section < NUM_L2_SECTIONS; section++) {
		// read the l2 section into wram

        // JR: Casting correct? getting:
        //  error: assigning '__attribute__((address_space(255))) uint8_t *' (aka '__attribute__((address_space(255)))
        // unsigned char *') to 'uint8_t *' (aka 'unsigned char *') changes address space of pointer
		section_addr = directory + DIR_LEVEL2_OFFSET + (section << ALLOC_TABLE_L2_ENTRIES_PER_SECTION_LOG2);
		mram_read(section_addr, l2_entries, ALLOC_TABLE_L2_ENTRIES_PER_SECTION);
		ptr = (uint64_t*)l2_entries;
		printf("section %u: %08llx %08llx %08llx %08llx %08llx %08llx %08llx %08llx", section, ptr[7], ptr[6], ptr[5], ptr[4], ptr[3], ptr[2], ptr[1], ptr[0]);
	}
}

#ifdef DIRECTORY_BTREE
static void print_node(struct btree_node *n)
{
	unsigned int i;
	for (i=0; i < n->num_children; i++) {
		printf("%u | ", n->keys[i]);
	}
}

static void btree_insert(btree_node *parent, uint16_t value)
{
	parent->num_children++;
}

void pimswap_insert_index_btree(uint32_t id, __mram_ptr uint8_t *addr, uint32_t length)
{
	btree_node root;

	printf("Total space in directory partition: %lu\n", DIRECTORY_SIZE);
	printf("Space available for btree: %lu\n", INDEX_SIZE);
	printf("btree node size: %lu\n", BTREE_NODE_SIZE);
	printf("Number of allocatable nodes: %lu\n", INDEX_SIZE / BTREE_NODE_SIZE);

	// find the root of the b-tree
	mram_read(directory + offsetof(struct static_directory_btree, block_tbl), &root, DPU_ALIGN(sizeof(struct btree_node), 8));

	if (root.num_children < MAX_CHILDREN) {
		btree_insert(&root, (uint16_t)id);
	} else {
		printf("ERROR: root node is full!\n");
	}

	print_node(&root);
}
#endif // DIRECTORY_BTREE

#ifdef DIRECTORY_HASH
void pimswap_insert_index_hash(uint32_t id, uint32_t block, uint32_t length)
{
	uint16_t entry_index;
	struct pim_hash_entry entry;
	__mram_ptr uint8_t *entry_addr;

	// hash the id
	entry_index = id % NUM_HASH_ENTRIES;
	entry_addr = HASH_ADDR_FROM_ENTRY(entry_index);

	// load the entry
	mram_read(entry_addr, &entry, DPU_ALIGN(sizeof(struct pim_hash_entry), 8));

	// unless this is a removal, check the length to see if this is already used
	// block=0 length=0 is an indicator that we are removing the entry
	if (block && length && entry.length) {
		printf("Hash entry %u already in use by tag %u\n", entry_index, entry.tag);
		return;
	}

	entry.tag = TAG_FROM_ID(id);
	entry.block = block;
	entry.length = length;
	//printf("[%u] Storing id %u as tag: %u block %u length %u\n", dpu_id, id, entry.tag, entry.block, entry.length);

	// update the entry with the new values
	//printf("[%u] writing entry %u to 0x%llx\n", dpu_id, entry_index, (uint64_t)entry_addr);
	mram_write(&entry, entry_addr, DPU_ALIGN(sizeof(struct pim_hash_entry), 8));
}

/**
	Look up a storage location based on id
*/
__mram_ptr uint8_t *pimswap_lookup_index_hash(unsigned int id, unsigned int *length)
{
	uint16_t entry_index;
	struct pim_hash_entry entry;
	__mram_ptr uint8_t *entry_addr;

	// hash the id - hopefully we get the same value as when we stored the item
	entry_index = id % NUM_HASH_ENTRIES;
	entry_addr = HASH_ADDR_FROM_ENTRY(entry_index);

	// load the entry
	mram_read(entry_addr, &entry, DPU_ALIGN(sizeof(struct pim_hash_entry), 8));

	// check the length to see if this is already used
	if (entry.length && entry.tag == TAG_FROM_ID(id)) {
		*length = entry.length;
		//printf("Found tag: %u block %u length %u\n", entry.tag, entry.block, entry.length);
		return storage + (entry.block << STORAGE_BLOCK_SIZE_LOG2);
	}

	//printf("[%u] length %u wrong or tag %u doesn't match %u\n", dpu_id, entry.length, entry.tag, TAG_FROM_ID(id));
	*length = 0;
	return 0;
}
#endif // DIRECTORY_HASH

static void free_blocks_l2(uint32_t first_block, uint8_t num_blocks)
{
	uint8_t l2_entry;
	uint8_t bit;
	__mram_ptr uint8_t *section_addr;
	uint8_t l2_entries[DPU_ALIGN(ALLOC_TABLE_L2_ENTRIES_PER_SECTION, 8)];
	uint32_t section;
	uint8_t block;

	section = L2_SECTION_FROM_BLOCK(first_block);
	l2_entry = L2_ENTRY_FROM_BLOCK(first_block);
	bit = L2_BIT_FROM_BLOCK(first_block);

	// read the l2 section into wram
	section_addr = directory + DIR_LEVEL2_OFFSET + (section << ALLOC_TABLE_L2_ENTRIES_PER_SECTION_LOG2);
	//printf("Reading %u L2 entries from %pS for section %u\n", ALLOC_TABLE_L2_ENTRIES_PER_SECTION, section_addr, section);
	mram_read(section_addr, l2_entries, ALLOC_TABLE_L2_ENTRIES_PER_SECTION);

	for (block = 0; block < num_blocks; block++) {
		if (l2_entry >= ALLOC_TABLE_L2_ENTRIES_PER_SECTION) {
			printf("l2_entry is too big for this section! (%u)\n", l2_entry);
			printf("Freeing %u blocks, starting at %u (section %u, entry %u, bit %u)\n", num_blocks, first_block, section, l2_entry, bit);
			break;
		}

		l2_entries[l2_entry] &= ~(1 << bit);

		bit++;
		if (bit >= L2_BITS_PER_ENTRY) {
			bit = 0;
			l2_entry++;
		}
	}
	//dump_l2_section_bmp(section, l2_entries);

	mram_write(l2_entries, section_addr, ALLOC_TABLE_L2_ENTRIES_PER_SECTION);
}

static uint32_t find_free_blocks_l2(uint32_t section, uint8_t num_blocks, uint8_t *update_l1)
{
	uint8_t l2_entry;
	uint8_t bit;
	uint8_t found=0;
	uint8_t check_l2=0;
	__mram_ptr uint8_t *section_addr;
	uint32_t first_block;
	uint8_t l2_entries[ALLOC_TABLE_L2_ENTRIES_PER_SECTION];

	// read the l2 section into wram
	section_addr = directory + DIR_LEVEL2_OFFSET + (section << ALLOC_TABLE_L2_ENTRIES_PER_SECTION_LOG2);
	//printf("Reading %u L2 entries from %pS for section %u\n", ALLOC_TABLE_L2_ENTRIES_PER_SECTION, section_addr, section);
	mram_read(section_addr, l2_entries, ALLOC_TABLE_L2_ENTRIES_PER_SECTION);

	// look for 'num_blocks' consecutive bits in this section
	//dump_l2_section_bmp(section, l2_entries);
	for (l2_entry=0; l2_entry < ALLOC_TABLE_L2_ENTRIES_PER_SECTION; l2_entry++) {

		if (l2_entries[l2_entry] == 0xFF)
			continue;

		// look at each bit in this entry
		//printf("Looking at entry %u\n", l2_entry);
		for (bit=0; bit < L2_BITS_PER_ENTRY; bit++) {
			if (!(l2_entries[l2_entry] & (1 << bit))) {
				if (!found) {
					first_block = MAKE_BLOCK_FROM_L2(section, l2_entry, bit); // remember the first block in the set
					//printf("Num blocks %u First block %u section %u, entry %u, bit %u\n", num_blocks, first_block, section, l2_entry, bit);
				}
				found++;
				if (found == num_blocks) {
					//printf("Found %u blocks\n", found);
					break;
				}
			} else {
				// broke the sequence
				if (found)
					found = 0;
			}
		}

		if (found == num_blocks)
			break;
	}

	// we failed - try again later
	if (found != num_blocks)
		return UINT_MAX;

	while (found--)
	{
		//printf("Setting entry %u bit %u\n", l2_entry, bit);
		l2_entries[l2_entry] |= (1 << bit);
		if (l2_entries[l2_entry] == 0xFF)
			check_l2 = 1;

		if (bit == 0) {
			l2_entry--;
			bit = L2_BITS_PER_ENTRY - 1;
		} else {
			bit--;
		}
	}
	//dump_l2_section_bmp(section, l2_entries);

	// mark the l2 entries as used
	//print_hex_dump(KERN_ERR, "l2_entries ", DUMP_PREFIX_NONE, ALLOC_TABLE_L2_ENTRIES_PER_SECTION, 1, l2_entries, ALLOC_TABLE_L2_ENTRIES_PER_SECTION, false);
	mram_write(l2_entries, section_addr, ALLOC_TABLE_L2_ENTRIES_PER_SECTION);

	// if the whole section is full, remember to update the bit in the L1
	*update_l1 = 0;
	if (check_l2) {
		for (l2_entry=0; l2_entry < ALLOC_TABLE_L2_ENTRIES_PER_SECTION; l2_entry++) {
			if (l2_entries[l2_entry] != 0xFF)
				break;
		}
		if (l2_entry == ALLOC_TABLE_L2_ENTRIES_PER_SECTION)
			*update_l1 = 1;
	}

	return first_block;
}

__mram_ptr uint8_t *pimswap_alloc_page_static(unsigned int id, unsigned int length)
{
	uint8_t l1_bit;
	uint8_t l1_entry;
	uint8_t l1_entries[DPU_ALIGN(ALLOC_TABLE_L1_ENTRIES, 8)];
	uint8_t num_blocks;
	uint32_t first_block;
	uint8_t update_l1=1;
	uint32_t section;

	num_blocks = (length + (STORAGE_BLOCK_SIZE-1)) >> STORAGE_BLOCK_SIZE_LOG2;

	//printf("%s: id %u\n", __func__, id);

	// read level 1 allocation table from MRAM to WRAM
	//printf("Reading %lu bytes for L1 alloc table\n", DPU_ALIGN(ALLOC_TABLE_L1_ENTRIES, 8));
	mram_read(directory + DIR_LEVEL1_OFFSET, l1_entries, DPU_ALIGN(ALLOC_TABLE_L1_ENTRIES, 8));
	//dump_L1(l1_entries);

	// look for a section that has at least one free block
	for (l1_entry=0; l1_entry < ALLOC_TABLE_L1_ENTRIES; l1_entry++) {
		if (l1_entries[l1_entry] != 0xFF) {
			for (l1_bit=0; l1_bit < 8; l1_bit++) {
				if (!(l1_entries[l1_entry] & (1 << l1_bit))) {
					section = L1_MAKE_SECTION(l1_entry, l1_bit);
					//printf("section %u = l1_entry=%u l1_bit=%u\n", section, l1_entry, l1_bit);
					first_block = find_free_blocks_l2(section, num_blocks, &update_l1);
					if (first_block != UINT_MAX)
						break;
				}
			}
		}

		if (first_block != UINT_MAX)
			break;
	}

	if (first_block == UINT_MAX) {
		// can't find %u consecutive blocks for allocation
		printf("FULL blocks: %u alloc: %u\n", stats.current_blocks_allocated, num_blocks);
		//dump_l1_bmp(l1_entries);
		//dump_l2_section_bmp(section, NULL);
		return NULL;
	}

	if (update_l1) {
		//printf("Section %u is full - updating L1\n", L1_MAKE_SECTION(l1_entry, l1_bit));
		l1_entries[l1_entry] |= (1 << l1_bit);
		mram_write(l1_entries, directory + DIR_LEVEL1_OFFSET, DPU_ALIGN(ALLOC_TABLE_L1_ENTRIES, 8));
	}

#ifdef STATISTICS
	// collect some interesting statistics
	stats.num_allocs++;
	stats.current_pages++;
	stats.current_blocks_allocated += num_blocks;
	stats.current_bytes_used += length;
	print_statistics();
#endif // STATISTICS

	pimswap_insert_index_hash(id, first_block, length);

	//printf("[%u] Allocating block %u @ %pS\n", dpu_id, block, storage + (block << STORAGE_BLOCK_SIZE_LOG2));
	return storage + (first_block << STORAGE_BLOCK_SIZE_LOG2);
}

void pimswap_free_page_static(unsigned int id)
{
	uint8_t l1_bit;
	uint8_t l1_entry;
	uint8_t l1_entries[DPU_ALIGN(ALLOC_TABLE_L1_ENTRIES, 8)];
	uint8_t num_blocks;
	uint32_t first_block;
	uint32_t length;
	uint32_t section;
	__mram_ptr uint8_t *storage;

	storage = pimswap_lookup_index_hash(id, &length);
	if (!storage) {
		printf("%s: Can't find entry for id %u\n", __func__, id);
		return;
	}

	first_block = STORAGE_BLOCK_FROM_ADDRESS(storage);
	num_blocks = (length + (STORAGE_BLOCK_SIZE-1)) >> STORAGE_BLOCK_SIZE_LOG2;

	//printf("%s length %u num blocks: %u\n", __func__, length, num_blocks);
	free_blocks_l2(first_block, num_blocks);

	// check to see if the l1 needs updating
	section = L2_SECTION_FROM_BLOCK(first_block);
	l1_entry = L1_ENTRY_FROM_SECTION(section);
	l1_bit = L1_BIT_FROM_SECTION(section);
	mram_read(directory + DIR_LEVEL1_OFFSET, l1_entries, DPU_ALIGN(ALLOC_TABLE_L1_ENTRIES, 8));
	if (l1_entries[l1_entry] & (1 << l1_bit)) {
		//printf("Section is no longer full - updating L1\n");
		l1_entries[l1_entry] &= ~(1 << l1_bit);
		mram_write(l1_entries, directory + DIR_LEVEL1_OFFSET, DPU_ALIGN(ALLOC_TABLE_L1_ENTRIES, 8));
	}

#ifdef STATISTICS
	stats.num_frees++;
	stats.current_pages--;
	stats.current_blocks_allocated -= num_blocks;
	stats.current_bytes_used -= length;
	print_statistics();
#endif // STATISTICS
}

