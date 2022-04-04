/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 UPMEM. All rights reserved. */
#include <linux/kernel.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE == KERNEL_VERSION(3, 10, 0)
#include <asm/i387.h>
#else
#include <asm/fpu/api.h>
#endif
#include <asm/io.h>
#include <asm/special_insns.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/sizes.h>
#include <linux/slab.h>

#include "dpu_management.h"
#include "dpu_power_management.h"
#include "dpu_rank.h"
#include "dpu_region_address_translation.h"
#include "dpu_region.h"
#include "dpu_utils.h"

#define NB_ELEM_MATRIX 8

void byte_interleave(uint64_t *input, uint64_t *output)
{
	int i, j;
	int sleep = 0;

	for (i = 0; i < NB_ELEM_MATRIX; ++i) {
		for (j = 0; j < sizeof(uint64_t); ++j) {
			// printk("Address of input is %p and output is %p\n", &input[j], &output[i]);
			((uint8_t *)&output[i])[j] = ((uint8_t *)&input[j])[i];
		}
	}
}

void xeon_sp_write_to_cis(struct dpu_region_address_translation *tr,
			  void *base_region_addr, uint8_t channel_id,
			  void *block_data)
{
	uint64_t output[NB_ELEM_MATRIX] __attribute((aligned(64)));
	uint64_t *ci_address;

	/* 0/ Find out CI address */
	// To discover address translation, base_region_addr will point to
	// whatever address
	ci_address = (uint64_t *)((uint8_t *)base_region_addr + 0x20000);

	pr_debug("command: %16llx\n", ((uint64_t *)block_data)[0]);

	/* 1/ Byte interleave the command */
	byte_interleave(block_data, output);

	/* 2/ Write the command */
	/*
	 * Write to the CI all at once using a non-temporal store.
	 * Note that it works because all 64 bytes of the WC buffer are transmitted
	 * on the data bus in a single bus transaction.
	 * cf. Intel Volume 3A: 11.3.1 Buffering of Write Combining Memory Locations
	 */
	kernel_fpu_begin();
	asm volatile("vmovdqa64 %0, %%zmm0\n\t"
		     "vmovntdq %%zmm0, %1"
		     :
		     : "m"(*output), "m"(*ci_address));
	asm volatile("sfence" : : : "memory");
	kernel_fpu_end();
}

void xeon_sp_read_from_cis(struct dpu_region_address_translation *tr,
			   void *base_region_addr, uint8_t channel_id,
			   void *block_data)
{
	uint64_t input[NB_ELEM_MATRIX];
	uint64_t *ci_address;
	int i;

	/* 0/ Find out CI address */
	// To discover address translation, base_region_addr will point to
	// whatever address
	ci_address = (uint64_t *)((uint8_t *)base_region_addr + 0x20000 +
				  32 * 1024); // + 0x80 * (count % 16);

	/* 1/ Read the result */
	// Write back only DIRTY cache lines and invalidates all.
	for (i = 0; i < 4; ++i) {
		mb();
		clflushopt(ci_address);
		mb();

		((volatile uint64_t *)input)[0] = __raw_readq(ci_address + 0);
		((volatile uint64_t *)input)[1] = __raw_readq(ci_address + 1);
		((volatile uint64_t *)input)[2] = __raw_readq(ci_address + 2);
		((volatile uint64_t *)input)[3] = __raw_readq(ci_address + 3);
		((volatile uint64_t *)input)[4] = __raw_readq(ci_address + 4);
		((volatile uint64_t *)input)[5] = __raw_readq(ci_address + 5);
		((volatile uint64_t *)input)[6] = __raw_readq(ci_address + 6);
		((volatile uint64_t *)input)[7] = __raw_readq(ci_address + 7);
	}

	/* 2/ Byte de-interleave the result */
	byte_interleave(input, block_data);

	pr_debug("result:  %16llx\n", ((uint64_t *)block_data)[0]);
}

#define BANK_START(dpu_id)                                                     \
	(0x40000 * ((dpu_id) % 4) + ((dpu_id >= 4) ? 0x40 : 0))
// For each 64bit word, you must jump 16 * 64bit (2 cache lines)
#define BANK_OFFSET_NEXT_DATA(i) (i * 16)
#define BANK_CHUNK_SIZE 0x20000
#define BANK_NEXT_CHUNK_OFFSET 0x100000

#define XFER_BLOCK_SIZE 8

static u32 apply_address_translation_on_mram_offset(u32 byte_offset)
{
	/* We have observed that, within the 26 address bits of the MRAM address, we need to apply an address translation:
	 *
	 * virtual[13: 0] = physical[13: 0]
	 * virtual[20:14] = physical[21:15]
	 * virtual[   21] = physical[   14]
	 * virtual[25:22] = physical[25:22]
	 *
	 * This function computes the "virtual" mram address based on the given "physical" mram address.
	 */

	u32 mask_21_to_15 = ((1 << (21 - 15 + 1)) - 1) << 15;
	u32 mask_21_to_14 = ((1 << (21 - 14 + 1)) - 1) << 14;
	u32 bits_21_to_15 = (byte_offset & mask_21_to_15) >> 15;
	u32 bit_14 = (byte_offset >> 14) & 1;
	u32 unchanged_bits = byte_offset & ~mask_21_to_14;

	return unchanged_bits | (bits_21_to_15 << 14) | (bit_14 << 21);
}

void xeon_sp_write_to_rank(struct dpu_region_address_translation *tr,
			   void *base_region_addr, uint8_t channel_id,
			   struct dpu_transfer_mram *xfer_matrix)
{
	uint8_t idx, ci_id, dpu_id, nb_cis, nb_dpus_per_ci;

	nb_cis = tr->desc.topology.nr_of_control_interfaces;
	nb_dpus_per_ci = tr->desc.topology.nr_of_dpus_per_control_interface;

	printk("Xeon transfer\n");
	/* Works only for transfers of same size and same offset on the
	 * same line
	 */
	  for (dpu_id = 0, idx = 0; dpu_id < nb_dpus_per_ci;
	     ++dpu_id, idx += nb_cis) {
		uint8_t *ptr_dest =
			(uint8_t *)base_region_addr + BANK_START(dpu_id);
		uint32_t size_transfer = xfer_matrix->size;
		uint32_t offset_in_mram = xfer_matrix->offset_in_mram;
		uint64_t cache_line[8], cache_line_interleave[8];
		uint32_t page[8];
		uint32_t off_in_page[8];
		uint32_t len_xfer_in_page[8];
		uint32_t len_xfer_done_in_page[8];
		struct xfer_page *xferp;
		struct page *cur_page[8];
		uint64_t len_xfer_done, len_xfer_remaining;
		bool do_dpu_transfer = false;

		for (ci_id = 0; ci_id < nb_cis; ++ci_id) {
			xferp = xfer_matrix->ptr[idx + ci_id];
			if (!xferp)
				continue;

			do_dpu_transfer = true;
			page[ci_id] = 0;
			cur_page[ci_id] = xferp->pages[0];
			len_xfer_in_page[ci_id] =
				min((uint32_t)PAGE_SIZE - xferp->off_first_page,
				    (uint32_t)size_transfer);
			off_in_page[ci_id] = xferp->off_first_page;
			len_xfer_done_in_page[ci_id] = 0;
		}

		if (!do_dpu_transfer)
			continue; 

		/* Split transfer into 8B blocks */
		 for (len_xfer_done = 0, len_xfer_remaining = size_transfer;
		     len_xfer_done < size_transfer;
		     len_xfer_done += XFER_BLOCK_SIZE) {
			uint32_t mram_64_bit_word_offset =
				apply_address_translation_on_mram_offset(
					len_xfer_done + offset_in_mram) /
				8;
			uint64_t next_data = BANK_OFFSET_NEXT_DATA(
				mram_64_bit_word_offset * sizeof(uint64_t));
			uint64_t offset = (next_data % BANK_CHUNK_SIZE) +
					  (next_data / BANK_CHUNK_SIZE) *
						  BANK_NEXT_CHUNK_OFFSET;

			for (ci_id = 0; ci_id < nb_cis; ++ci_id) {
				if (xfer_matrix->ptr[idx + ci_id])
					cache_line[ci_id] = *(
						uint64_t
							*)((uint8_t *)page_to_virt(
								   cur_page[ci_id]) +
							   off_in_page[ci_id] +
							   len_xfer_done_in_page
								   [ci_id]);
			}
	
			
			int i, j;	
			/* for (i = 0; i < NB_ELEM_MATRIX; ++i) { 
				for (j = 0; j < sizeof(uint64_t); ++j) {
					if(i % 8 == 0 && j % (sizeof(uint64_t)/2) == 0)
				} }*/
			}
			printk("Address of input is %p\n", (uint8_t *)&cache_line[0]);
		 	
			/* byte_interleave(cache_line, cache_line_interleave); */ 

			/* __raw_writeq(cache_line_interleave[0],
				     ptr_dest + offset + 0 * sizeof(uint64_t)); 
			__raw_writeq(cache_line_interleave[1],
				     ptr_dest + offset + 1 * sizeof(uint64_t));
			__raw_writeq(cache_line_interleave[2],
				     ptr_dest + offset + 2 * sizeof(uint64_t));
			__raw_writeq(cache_line_interleave[3],
				     ptr_dest + offset + 3 * sizeof(uint64_t));
			__raw_writeq(cache_line_interleave[4],
				     ptr_dest + offset + 4 * sizeof(uint64_t));
			__raw_writeq(cache_line_interleave[5],
				     ptr_dest + offset + 5 * sizeof(uint64_t));
			__raw_writeq(cache_line_interleave[6],
				     ptr_dest + offset + 6 * sizeof(uint64_t));
			__raw_writeq(cache_line_interleave[7],
				     ptr_dest + offset + 7 * sizeof(uint64_t));

			len_xfer_remaining -= XFER_BLOCK_SIZE; */

			/* Check if we should switch to next page */
			/* for (ci_id = 0; ci_id < nb_cis; ++ci_id) {
				xferp = xfer_matrix->ptr[idx + ci_id];
				if (!xferp)
					continue;

				len_xfer_done_in_page[ci_id] += XFER_BLOCK_SIZE;

				if ((page[ci_id] < xferp->nb_pages - 1) &&
				    (len_xfer_done_in_page[ci_id] >=
				     len_xfer_in_page[ci_id])) {
					page[ci_id]++;
					cur_page[ci_id] =
						xferp->pages[page[ci_id]];
					len_xfer_in_page[ci_id] = min(
						(uint32_t)PAGE_SIZE,
						(uint32_t)len_xfer_remaining);
					off_in_page[ci_id] = 0;
					len_xfer_done_in_page[ci_id] = 0;
				}
			}
		}

		mb();

		for (len_xfer_done = 0; len_xfer_done < size_transfer;
		     len_xfer_done += XFER_BLOCK_SIZE) {
			uint32_t mram_64_bit_word_offset =
				apply_address_translation_on_mram_offset(
					len_xfer_done + offset_in_mram) /
				8;
			uint64_t next_data = BANK_OFFSET_NEXT_DATA(
				mram_64_bit_word_offset * sizeof(uint64_t));
			uint64_t offset = (next_data % BANK_CHUNK_SIZE) +
					  (next_data / BANK_CHUNK_SIZE) *
						  BANK_NEXT_CHUNK_OFFSET;

			clflushopt(ptr_dest + offset);
		}

		mb(); */
	} 
}

void xeon_sp_read_from_rank(struct dpu_region_address_translation *tr,
			    void *base_region_addr, uint8_t channel_id,
			    struct dpu_transfer_mram *xfer_matrix)
{
	uint8_t idx, ci_id, dpu_id, nb_cis, nb_dpus_per_ci;

	nb_cis = tr->desc.topology.nr_of_control_interfaces;
	nb_dpus_per_ci = tr->desc.topology.nr_of_dpus_per_control_interface;

	/* Works only for transfers of same size and same offset on the
	 * same line
	 */
	for (dpu_id = 0, idx = 0; dpu_id < nb_dpus_per_ci;
	     ++dpu_id, idx += nb_cis) {
		uint8_t *ptr_dest =
			(uint8_t *)base_region_addr + BANK_START(dpu_id);
		uint32_t size_transfer = xfer_matrix->size;
		uint32_t offset_in_mram = xfer_matrix->offset_in_mram;
		uint64_t cache_line[8], cache_line_interleave[8];
		uint32_t page[8];
		uint32_t off_in_page[8];
		uint32_t len_xfer_in_page[8];
		uint32_t len_xfer_done_in_page[8];
		struct xfer_page *xferp;
		struct page *cur_page[8];
		uint64_t len_xfer_done, len_xfer_remaining;
		bool do_dpu_transfer = false;

		for (ci_id = 0; ci_id < nb_cis; ++ci_id) {
			xferp = xfer_matrix->ptr[idx + ci_id];
			if (!xferp)
				continue;

			do_dpu_transfer = true;
			page[ci_id] = 0;
			cur_page[ci_id] = xferp->pages[0];
			len_xfer_in_page[ci_id] =
				min((uint32_t)PAGE_SIZE - xferp->off_first_page,
				    (uint32_t)size_transfer);
			off_in_page[ci_id] = xferp->off_first_page;
			len_xfer_done_in_page[ci_id] = 0;
		}

		if (!do_dpu_transfer)
			continue;

		mb();

		for (len_xfer_done = 0; len_xfer_done < size_transfer;
		     len_xfer_done += XFER_BLOCK_SIZE) {
			uint32_t mram_64_bit_word_offset =
				apply_address_translation_on_mram_offset(
					len_xfer_done + offset_in_mram) /
				8;
			uint64_t next_data = BANK_OFFSET_NEXT_DATA(
				mram_64_bit_word_offset * sizeof(uint64_t));
			uint64_t offset = (next_data % BANK_CHUNK_SIZE) +
					  (next_data / BANK_CHUNK_SIZE) *
						  BANK_NEXT_CHUNK_OFFSET;
			clflushopt(ptr_dest + offset);
		}

		mb();

		/* Split transfer into 8B blocks */
		for (len_xfer_done = 0, len_xfer_remaining = size_transfer;
		     len_xfer_done < size_transfer;
		     len_xfer_done += XFER_BLOCK_SIZE) {
			uint32_t mram_64_bit_word_offset =
				apply_address_translation_on_mram_offset(
					len_xfer_done + offset_in_mram) /
				8;
			uint64_t next_data = BANK_OFFSET_NEXT_DATA(
				mram_64_bit_word_offset * sizeof(uint64_t));
			uint64_t offset = (next_data % BANK_CHUNK_SIZE) +
					  (next_data / BANK_CHUNK_SIZE) *
						  BANK_NEXT_CHUNK_OFFSET;

			cache_line[0] = __raw_readq(ptr_dest + offset +
						    0 * sizeof(uint64_t));
			cache_line[1] = __raw_readq(ptr_dest + offset +
						    1 * sizeof(uint64_t));
			cache_line[2] = __raw_readq(ptr_dest + offset +
						    2 * sizeof(uint64_t));
			cache_line[3] = __raw_readq(ptr_dest + offset +
						    3 * sizeof(uint64_t));
			cache_line[4] = __raw_readq(ptr_dest + offset +
						    4 * sizeof(uint64_t));
			cache_line[5] = __raw_readq(ptr_dest + offset +
						    5 * sizeof(uint64_t));
			cache_line[6] = __raw_readq(ptr_dest + offset +
						    6 * sizeof(uint64_t));
			cache_line[7] = __raw_readq(ptr_dest + offset +
						    7 * sizeof(uint64_t));

			byte_interleave(cache_line, cache_line_interleave);

			for (ci_id = 0; ci_id < nb_cis; ++ci_id) {
				if (xfer_matrix->ptr[idx + ci_id])
					*(uint64_t *)((uint8_t *)page_to_virt(
							      cur_page[ci_id]) +
						      off_in_page[ci_id] +
						      len_xfer_done_in_page
							      [ci_id]) =
						cache_line_interleave[ci_id];
			}

			len_xfer_remaining -= XFER_BLOCK_SIZE;

			/* Check if we should switch to next page */
			for (ci_id = 0; ci_id < nb_cis; ++ci_id) {
				xferp = xfer_matrix->ptr[idx + ci_id];
				if (!xferp)
					continue;

				len_xfer_done_in_page[ci_id] += XFER_BLOCK_SIZE;

				if ((page[ci_id] < xferp->nb_pages - 1) &&
				    (len_xfer_done_in_page[ci_id] >=
				     len_xfer_in_page[ci_id])) {
					page[ci_id]++;
					cur_page[ci_id] =
						xferp->pages[page[ci_id]];
					len_xfer_in_page[ci_id] = min(
						(uint32_t)PAGE_SIZE,
						(uint32_t)len_xfer_remaining);
					off_in_page[ci_id] = 0;
					len_xfer_done_in_page[ci_id] = 0;
				}
			}
		}
	}
}

DEFINE_MUTEX(mutex_nb_ranks_allocated);
uint32_t nb_ranks_allocated = 0;
#define PREFETCH_MSR 0x1A4
#define PREFETCH_DISABLE 0xF
#define PREFETCH_ENABLE 0x0

int xeon_sp_init_rank(struct dpu_region_address_translation *tr,
		      uint8_t channel_id)
{
	struct dpu_region *region = (struct dpu_region *)tr->private;
	struct dpu_rank_t *rank = &region->rank;

	/* Disable HW prefetchers if first rank allocated */
	mutex_lock(&mutex_nb_ranks_allocated);

	if (nb_ranks_allocated == 0) {
		int cpu;

		for_each_possible_cpu (cpu) {
			// wrmsr_on_cpu(cpu, PREFETCH_MSR, PREFETCH_DISABLE, 0);
		}
	}
	nb_ranks_allocated++;

	mutex_unlock(&mutex_nb_ranks_allocated);

	dpu_power_rank_exit_saving_mode(rank);

	/* Exit DIMM power-saving mode if DIMM is about to be used */
	if (!dpu_is_dimm_used(rank))
		dpu_power_dimm_exit_saving_mode(rank);

	return 0;
}

void xeon_sp_destroy_rank(struct dpu_region_address_translation *tr,
			  uint8_t channel_id)
{
	struct dpu_region *region = (struct dpu_region *)tr->private;
	struct dpu_rank_t *rank = &region->rank;

	/*
	 * We do not invalidate the whole rank but then we have to invalidate
	 * before reading anything.
	 */

	/* Enable HW prefetchers back if last rank freed */
	mutex_lock(&mutex_nb_ranks_allocated);

	nb_ranks_allocated--;
	if (nb_ranks_allocated == 0) {
		int cpu;

		for_each_possible_cpu (cpu) {
			wrmsr_on_cpu(cpu, PREFETCH_MSR, PREFETCH_ENABLE, 0);
		}
	}

	mutex_unlock(&mutex_nb_ranks_allocated);

	dpu_power_rank_enter_saving_mode(rank);

	/* Enter DIMM power-saving mode if DIMM is no longer used */
	if (!dpu_is_dimm_used(rank))
		dpu_power_dimm_enter_saving_mode(rank);
}

struct dpu_region_address_translation xeon_sp_translate = {
	.desc = {
		.topology.nr_of_control_interfaces = 8,
		.topology.nr_of_dpus_per_control_interface = 8,
		.memories.mram_size = 64 * SZ_1M,
		.memories.wram_size = 64 * SZ_1K,
		.memories.iram_size = SZ_4K,
		.timings.fck_frequency_in_mhz = 800,
		.timings.carousel.cmd_duration = 2,
		.timings.carousel.cmd_sampling = 1,
		.timings.carousel.res_duration = 2,
		.timings.carousel.res_sampling = 1,
		.timings.reset_wait_duration = 20,
		.timings.clock_division = 4,
		.timings.std_temperature = 110,
		.dpu.nr_of_threads = 24,
		.dpu.nr_of_atomic_bits = 256,
		.dpu.nr_of_notify_bits = 40,
		.dpu.nr_of_work_registers_per_thread = 24,
	},
	.backend_id = DPU_BACKEND_XEON_SP,
	.capabilities = CAP_PERF | CAP_SAFE,
	.init_rank = xeon_sp_init_rank,
	.destroy_rank = xeon_sp_destroy_rank,
	.write_to_rank = xeon_sp_write_to_rank,
	.read_from_rank = xeon_sp_read_from_rank,
	.write_to_cis = xeon_sp_write_to_cis,
	.read_from_cis = xeon_sp_read_from_cis,
};
