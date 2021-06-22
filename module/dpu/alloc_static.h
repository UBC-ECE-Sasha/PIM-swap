#ifndef _ALLOC_STATIC__H
#define _ALLOC_STATIC__H

#include "../common.h"
#include <mram.h>
#include <defs.h>
#include <stdint.h>

#define STATISTICS
#define DIRECTORY_HASH

// The directory is used to organize space in the storage area, and is arranged in 2 parts: the allocation table and the file table

// The 60MB storage area is arranged as 61440 x 1KB blocks. Each compressed page may use between 1-4 consecutive blocks. In the worst case,
// each compressed page will use only a single 1KB block, which means the maximum possible number of allocations is 61440. The usage of the
// blocks is tracked in a 2 level bitmap called the "allocation table".

// The first level is 120 bits (15 bytes), in which each bit represents a 64-byte section in the second level (each section covers 512 blocks).
// That size is chosen because the minimum read from MRAM is 8 bytes, which means we know exactly which section to read from level 2 -- doesn't make sense now!
// The second level is 61440 bits (7680 bytes) in which each bit represents a 1KB block (each byte covers 8KB)
// The allocator bitmap uses a total of 7696 bytes (including padding).

// The file table also uses a bitmap allocator to keep track of btree nodes and file nodes. The maximum number of file nodes (and therefore
// btree children) is 61440. The remaining space (3014656 bytes) is divided into 47104 x 64B blocks.
// The first level is 184 bits (23 bytes) where each bit represents 32 bytes in second level (covering 256 blocks).
// The second level uses 1 bit per 64B block for a total of 47104 bits (5888 bytes).

/*
n is the number of entries in the tree (61440)
m is the maximum number of children (28)
d is the minimum number of children (28/2 = 14)
worst case height = logd (N + 1)/2 = logd (61440 + 1) / 2 = 4
(Comer79 and Cormen2001)

worse case leaf and file nodes (61440/d) = 4388
total space required for file nodes = 4388 * 64 = 280832
total space available for nodes = 3133932
total number of nodes = 3133932/64 = 48967 nodes
maximum number of internal nodes = 48967 - 15360 = 33607

maximum number of nodes = 28^(h+1)-1 = 262144 !Cannot fit!
*/

// Each entry requires 16 bits for the block index and 32 bits for the id

// finding a free block
// Check each byte in the first level until 0 is found (l1index)
// Check each byte in the second level until 0 is found (l2index)
// free block = l2index << 8

// The file table holds a B-tree sorted by page id. The key is the id, the data is the
// block index holding the data.

// finding an entry in the file table
// hash the id into a 16-bit index

// maybe a B-tree?

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define RESERVED_SIZE MEGABYTE(1)
#define DIRECTORY_SIZE (MEGABYTE(3) - KILOBYTE(4) - sizeof(unsigned int))
#define STORAGE_SIZE (MRAM_SIZE - MEGABYTE(4))

#define ALLOC_TABLE_L1_ENTRIES (ALLOC_TABLE_L2_ENTRIES >> (6 + 3)) //15
#define ALLOC_TABLE_L2_ENTRIES (NUM_STORAGE_BLOCKS >> L2_BITS_PER_ENTRY_LOG2) //7680
#define ALLOC_TABLE_L2_ENTRIES_PER_SECTION_LOG2 6 // 64-byte section
#define ALLOC_TABLE_L2_ENTRIES_PER_SECTION (1 << ALLOC_TABLE_L2_ENTRIES_PER_SECTION_LOG2)
#define NUM_L2_SECTIONS (ALLOC_TABLE_L2_ENTRIES >> ALLOC_TABLE_L2_ENTRIES_PER_SECTION_LOG2)

#define STORAGE_BLOCK_SIZE_LOG2 6
#define STORAGE_BLOCK_SIZE (1 << STORAGE_BLOCK_SIZE_LOG2)
#define NUM_STORAGE_BLOCKS (STORAGE_SIZE >> STORAGE_BLOCK_SIZE_LOG2)
#define MAX_CONSECUTIVE_BLOCKS (PAGE_SIZE >> STORAGE_BLOCK_SIZE_LOG2)

#define ALLOC_TABLE_SIZE (ALLOC_TABLE_L1_SIZE + ALLOC_TABLE_L2_SIZE)
#define INDEX_SIZE (DIRECTORY_SIZE - ALLOC_TABLE_SIZE)

#define L1_BITS_PER_ENTRY_LOG2 3
#define L1_BITS_PER_ENTRY (1 << L1_BITS_PER_ENTRY_LOG2)

#define L1_ENTRY_FROM_SECTION(_s) (_s >> L1_BITS_PER_ENTRY_LOG2)
#define L1_BIT_FROM_SECTION(_s) (_s & (L1_BITS_PER_ENTRY-1))
#define L1_MAKE_SECTION(_entry, _bit) (_entry << L1_BITS_PER_ENTRY_LOG2 | _bit)

#define L2_BITS_PER_ENTRY_LOG2 3
#define L2_BITS_PER_ENTRY (1 << L2_BITS_PER_ENTRY_LOG2)

#define MAKE_BLOCK_FROM_L2(_section, _l2entry, _bit) \
	((_section << (ALLOC_TABLE_L2_ENTRIES_PER_SECTION_LOG2 + L2_BITS_PER_ENTRY_LOG2)) | (_l2entry << L2_BITS_PER_ENTRY_LOG2) | _bit)

#define L2_SECTION_FROM_BLOCK(_b) (_b >> (ALLOC_TABLE_L2_ENTRIES_PER_SECTION_LOG2 + L2_BITS_PER_ENTRY_LOG2))
#define L2_ENTRY_FROM_BLOCK(_b) ((_b >> L2_BITS_PER_ENTRY_LOG2) & (ALLOC_TABLE_L2_ENTRIES_PER_SECTION-1))
#define L2_BIT_FROM_BLOCK(_b) (_b & (L2_BITS_PER_ENTRY-1))

#define STORAGE_BLOCK_FROM_ADDRESS(_address) (((uint64_t)_address - (uint64_t)storage) >> STORAGE_BLOCK_SIZE_LOG2)
#define TAG_FROM_ID(_x) (_x & 0xFFFF)

// JR: Adding (__mram_ptr uint8_t*)directory is correct?
#define HASH_ADDR_FROM_ENTRY(_entry) ((__mram_ptr uint8_t*)directory + offsetof(struct static_directory_hash, hash_table) + _entry * sizeof(struct pim_hash_entry))

// MRAM buffers
extern uint8_t __mram_noinit reserved[RESERVED_SIZE];
extern uint8_t __mram_noinit trans_page[PAGE_SIZE];
extern uint32_t __mram_noinit id;
extern uint32_t __mram_noinit status;
extern uint8_t __mram_noinit directory[DIRECTORY_SIZE];
extern uint8_t __mram_noinit storage[STORAGE_SIZE];

/***************/
#ifdef DIRECTORY_BTREE

#define ALLOC_TABLE_L1_SIZE DPU_ALIGN(ALLOC_TABLE_L1_ENTRIES, 8)
#define ALLOC_TABLE_L2_SIZE DPU_ALIGN(ALLOC_TABLE_L2_ENTRIES, 8)
#define DIR_LEVEL1_OFFSET offsetof(struct static_directory_btree, dir_level1)
#define DIR_LEVEL2_OFFSET offsetof(struct static_directory_btree, dir_level2)
#define BTREE_L1_ENTRIES	16
#define BTREE_L2_ENTRIES 7680
#define BTREE_NODE_SIZE (sizeof(struct btree_node))
#define MAX_CHILDREN 16

typedef struct btree_node {
	uint16_t num_children;
	uint16_t keys[MAX_CHILDREN-1];
	uint16_t children[MAX_CHILDREN]; // block numbers
} btree_node;

struct static_directory_btree {
	uint8_t dir_level1[DPU_ALIGN(ALLOC_TABLE_L1_ENTRIES, 8)];
	uint8_t dir_level2[DPU_ALIGN(ALLOC_TABLE_L2_ENTRIES, 8)];
	uint8_t node_level1[DPU_ALIGN(BTREE_L1_ENTRIES, 8)];
	uint8_t node_level2[DPU_ALIGN(BTREE_L2_ENTRIES, 8)];
	__mram_ptr struct btree_node block_tbl;
};

#elif defined DIRECTORY_HASH

#define ALLOC_TABLE_L1_SIZE DPU_ALIGN(ALLOC_TABLE_L1_ENTRIES, 8)
#define ALLOC_TABLE_L2_SIZE DPU_ALIGN(ALLOC_TABLE_L2_ENTRIES, 8)
#define DIR_LEVEL1_OFFSET offsetof(struct static_directory_hash, dir_level1)
#define DIR_LEVEL2_OFFSET offsetof(struct static_directory_hash, dir_level2)
#define HASH_ENTRY_SIZE (sizeof(struct pim_hash_entry))
#define NUM_HASH_ENTRIES (INDEX_SIZE / HASH_ENTRY_SIZE)

struct static_directory_hash {
	uint8_t dir_level1[DPU_ALIGN(ALLOC_TABLE_L1_ENTRIES, 8)];
	uint8_t dir_level2[DPU_ALIGN(ALLOC_TABLE_L2_ENTRIES, 8)];
	uint8_t hash_table[0];
};

struct pim_hash_entry {
	uint32_t block;
	uint16_t tag;
	uint16_t length;
};

#else
	#error You must define a DIRECTORY style
#endif // DIRECTORY_BTREE
/***************/

#ifdef STATISTICS
static struct statistics {
	uint64_t num_allocs;
	uint64_t num_frees;
	uint64_t current_pages;
	uint32_t current_blocks_allocated;
	uint32_t current_bytes_used;
	uint32_t uncompressable;
} stats;
#endif // STATISTICS

__mram_ptr uint8_t *pimswap_alloc_page_static(unsigned int id, unsigned int length);
void pimswap_free_page_static(unsigned int id);

#ifdef DIRECTORY_BTREE
void pimswap_insert_index_btree(uint32_t id, __mram_ptr uint8_t *addr, uint32_t length);
#elif defined DIRECTORY_HASH
void pimswap_insert_index_hash(uint32_t id, uint32_t block, uint32_t length);
__mram_ptr uint8_t *pimswap_lookup_index_hash(unsigned int id, unsigned int *length);
#endif // DIRECTORY

#endif // _ALLOC_STATIC__H
