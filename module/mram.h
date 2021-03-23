/* Copyright 2020 UPMEM. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DPUSYSCORE_MRAM_H
#define DPUSYSCORE_MRAM_H

//JKN #include <stdint.h>
//JKN #include <attributes.h>
#include <linux/string.h>
#include "common.h"
#include "dpu.h"

/**
 * @file mram.h
 * @brief MRAM Transfer Management.
 */

//#define DPU_MRAM_HEAP_POINTER ((__mram_ptr void *)(&__sys_used_mram_end))
//extern __mram_ptr __dma_aligned uint8_t __sys_used_mram_end[0];

/**
 * @fn mram_read
 * @brief Stores the specified number of bytes from MRAM to WRAM.
 * The number of bytes must be:
 *  - at least 8
 *  - at most 2048
 *  - a multiple of 8
 *
 * @param from source address in MRAM
 * @param to destination address in WRAM
 * @param nb_of_bytes number of bytes to transfer
 */
static inline void
mram_read(__mram_ptr void* from, void* to, unsigned int nb_of_bytes)
{
	uint8_t *mram_src = (uint8_t*)((uint64_t)from + get_dpu(get_current_dpu())->mram);
	uint8_t *wram_dest = to; //(void*)((uintptr_t)to + get_current_dpu()->wram);
	//printk("%s: src 0x%llx to 0x%llx\n", __func__, (uint64_t)mram_src, (uint64_t)wram_dest);
	memcpy(wram_dest, mram_src, nb_of_bytes);
}

/**
 * @fn mram_write
 * @brief Stores the specified number of bytes from WRAM to MRAM.
 * The number of bytes must be:
 *  - at least 8
 *  - at most 2048
 *  - a multiple of 8
 *
 * @param from source address in WRAM
 * @param to destination address in MRAM
 * @param nb_of_bytes number of bytes to transfer
 */
static inline void
mram_write(const void *from, __mram_ptr void* to, unsigned int nb_of_bytes)
{
	const uint8_t *wram_src = from;
	uint8_t *mram_dest = (uint8_t*)((uint64_t)to + get_dpu(get_current_dpu())->mram);
	//printk("%s: src 0x%llx to 0x%llx\n", __func__, (uint64_t)wram_src, (uint64_t)mram_dest);
	memcpy(mram_dest, wram_src, nb_of_bytes);
}

#endif /* DPUSYSCORE_MRAM_H */
