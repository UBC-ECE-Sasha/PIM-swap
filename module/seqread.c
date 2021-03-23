/* Copyright 2020 UPMEM. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "seqread.h"
#include "mram.h"
//#include <dpuconst.h>
//#include <dpuruntime.h>
//#include <atomic_bit.h>
//#include <stddef.h>
#include "dpu.h"

#define PAGE_SIZE (SEQREAD_CACHE_SIZE)
#define PAGE_ALLOC_SIZE (2 * PAGE_SIZE)
#define PAGE_OFF_MASK (PAGE_SIZE - 1)
#define PAGE_IDX_MASK (~PAGE_OFF_MASK)

#define MRAM_READ_PAGE(from, to) mram_read((__mram_ptr void*)(from), (void *)(to), PAGE_ALLOC_SIZE)

extern void *
mem_alloc_nolock(size_t size);

seqreader_buffer_t
__SEQREAD_ALLOC(void)
{
    uintptr_t heap_pointer = __WRAM_HEAP_POINTER;
    seqreader_buffer_t pointer = (seqreader_buffer_t)((heap_pointer + PAGE_OFF_MASK) & PAGE_IDX_MASK);
    size_t size = pointer + PAGE_ALLOC_SIZE - heap_pointer;
    /* We already compute the return pointer
     * mem_alloc_nolock is only used to really reserve the memory area in the heap */
    mem_alloc_nolock(size);

    return pointer;
}

void *
__SEQREAD_INIT(seqreader_buffer_t cache, __mram_ptr void *mram_addr, seqreader_t *reader)
{
	printk("%s: mram_addr = %p\n", __func__, mram_addr);

    reader->wram_cache = cache;
    reader->mram_addr = (uintptr_t)(1 << __DPU_MRAM_SIZE_LOG2);
	
	//printk("%s: reader wram_cache = 0x%lx mram_addr = 0x%lx\n", __func__, reader->wram_cache, reader->mram_addr);

    return __SEQREAD_SEEK(mram_addr, reader);
}

void *
__SEQREAD_GET(void *ptr, uint32_t inc, seqreader_t *reader)
{
	printk("%s: %u bytes\n", __func__, inc);
	// read 'inc' bytes
	printk("%s not implemented!\n", __func__);
	return NULL;
}

void *
__SEQREAD_SEEK(__mram_ptr void *mram_addr, seqreader_t *reader)
{
    uint64_t target_addr = (uint64_t)mram_addr;
    uint64_t current_addr = (uint64_t)reader->mram_addr;
    uint64_t wram_cache = (uint64_t)reader->wram_cache;
    uint64_t mram_offset = target_addr - current_addr;
	printk("%s: target_addr = 0x%llx\n", __func__, target_addr);
	printk("%s: current_addr = 0x%llx\n", __func__, current_addr);
	printk("%s: wram_cache = 0x%llx\n", __func__, wram_cache);
	printk("%s: mram_offset = 0x%llx\n", __func__, mram_offset);
    if ((mram_offset & PAGE_IDX_MASK) != 0) {
        uint64_t target_addr_idx_page = target_addr & PAGE_IDX_MASK;
			//printk("%s: target_addr_idx_page = 0x%lx\n", __func__, target_addr_idx_page);
        MRAM_READ_PAGE(target_addr_idx_page, wram_cache);
        mram_offset = target_addr & PAGE_OFF_MASK;
        reader->mram_addr = target_addr_idx_page;
    }
    return (void *)(mram_offset + wram_cache);
}

__mram_ptr void *
__SEQREAD_TELL(void *ptr, seqreader_t *reader)
{
    return (__mram_ptr void *)((uintptr_t)reader->mram_addr + ((uintptr_t)ptr & PAGE_OFF_MASK));
}
