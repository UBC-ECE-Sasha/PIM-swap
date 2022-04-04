/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 UPMEM. All rights reserved. */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/sizes.h>
#include <asm/cacheflush.h>

#include "dpu_rank.h"
#include "dpu_region_address_translation.h"
#include "dpu_region.h"
#include "dpu_utils.h"

#define CIID 6

#define NB_ELEM_MATRIX 8
#define NB_WRQ_FIFO_ENTRIES 17
#define WRQ_FIFO_ENTRY_SIZE 128

static DEFINE_SPINLOCK(ci_lock);

static void byte_interleave(uint64_t *input, uint64_t *output)
{
	int i, j;

	for (i = 0; i < NB_ELEM_MATRIX; ++i)
		for (j = 0; j < sizeof(uint64_t); ++j)
			((uint8_t *)&output[i])[j] = ((uint8_t *)&input[j])[i];
}

/* Write NB_WRQ_FIFO_ENTRIES of 0 right after the CI */
/* static void flush_mc_fifo(void *base_ci_address)
{
        uint64_t *next_ci_address = (uint64_t *)base_ci_address + 0x800;
	int i, j;

        for (i = 0; i < NB_WRQ_FIFO_ENTRIES; ++i) {
                for (j = 0; j < WRQ_FIFO_ENTRY_SIZE / sizeof(uint64_t); ++j)
                        next_ci_address[j] = 0xdeadbeefULL;
                __asm__ __volatile__ ("isync; sync; dcbf %y0; sync; isync" : : "Z"(*(uint8_t *)next_ci_address) : "memory");

                next_ci_address += 0x4000 / sizeof(uint64_t);
        }
} */

void power9_write_to_rank(struct dpu_region_address_translation *tr,
			  void *base_region_addr, uint8_t channel_id,
			  struct dpu_transfer_mram *xfer_matrix)
{
	pr_err("%s: not implemented.\n", __func__);
}

void power9_read_from_rank(struct dpu_region_address_translation *tr,
			   void *base_region_addr, uint8_t channel_id,
			   struct dpu_transfer_mram *xfer_matrix)
{
	pr_err("%s: not implemented.\n", __func__);
}

void power9_write_to_cis(struct dpu_region_address_translation *tr,
			 void *base_region_addr, uint8_t channel_id,
			 void *block_data)
{
	struct dpu_region *region = tr->private;
	struct dpu_rank_t *rank = &region->rank;
	uint64_t output[2 * NB_ELEM_MATRIX];
	uint64_t *ci_address, command_msbs = 0;
	uint8_t nb_cis;
	void *ptr_block_data = block_data;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&ci_lock, flags);

	nb_cis = tr->desc.topology.nr_of_control_interfaces;

	ci_address = base_region_addr + 0x80 + 0x4000;

	byte_interleave(ptr_block_data, output);

	__asm__ __volatile__("isync; sync; dclz %y0; sync; isync"
			     :
			     : "Z"(*(uint8_t *)ci_address)
			     : "memory");

	/* Nopify only commands != 0 */
	for (i = 0; i < nb_cis; ++i) {
		uint64_t command = ((uint64_t *)ptr_block_data)[i];
		if ((command >> 56) & 0xFF)
			command_msbs |= (0xFFULL << (i * 8));
	}

	*((volatile uint64_t *)ci_address + 7) = command_msbs;
	*((volatile uint64_t *)ci_address + 15) = command_msbs;
	/*
	 * "Power9 user manual" states that dcb* instructions granularity
	 * are 128B.
	 */
	__asm__ __volatile__("isync; sync; dcbf %y0; sync; isync"
			     :
			     : "Z"(*((uint8_t *)ci_address))
			     : "memory");

	*((volatile uint64_t *)ci_address + 0) = output[0];
	*((volatile uint64_t *)ci_address + 1) = output[1];
	*((volatile uint64_t *)ci_address + 2) = output[2];
	*((volatile uint64_t *)ci_address + 3) = output[3];
	*((volatile uint64_t *)ci_address + 4) = output[4];
	*((volatile uint64_t *)ci_address + 5) = output[5];
	*((volatile uint64_t *)ci_address + 6) = output[6];

	*((volatile uint64_t *)ci_address + 8) = output[0];
	*((volatile uint64_t *)ci_address + 9) = output[1];
	*((volatile uint64_t *)ci_address + 10) = output[2];
	*((volatile uint64_t *)ci_address + 11) = output[3];
	*((volatile uint64_t *)ci_address + 12) = output[4];
	*((volatile uint64_t *)ci_address + 13) = output[5];
	*((volatile uint64_t *)ci_address + 14) = output[6];

	__asm__ __volatile__("isync; sync" : : : "memory");
	*((volatile uint64_t *)ci_address + 7) = output[7];
	*((volatile uint64_t *)ci_address + 15) = 0ULL;

	__asm__ __volatile__("isync; sync; dcbf %y0; sync; isync"
			     :
			     : "Z"(*(uint8_t *)ci_address)
			     : "memory");

	spin_unlock_irqrestore(&ci_lock, flags);
}

void power9_read_from_cis(struct dpu_region_address_translation *tr,
			  void *base_region_addr, uint8_t channel_id,
			  void *block_data)
{
	struct dpu_region *region = tr->private;
	struct dpu_rank_t *rank = &region->rank;
	uint64_t tmp[4][8];
	uint64_t input[NB_ELEM_MATRIX * 2];
	uint64_t *ci_address;
	uint8_t nb_cis;
	void *ptr_block_data = block_data;
	int i;
	unsigned long flags;

	spin_lock_irqsave(&ci_lock, flags);

	nb_cis = tr->desc.topology.nr_of_control_interfaces;

	__asm__ __volatile__("isync; sync; " : : : "memory");

	ci_address =
		base_region_addr +
		0x80; // + 0x10; // ARITHMETIC ON VOID* ARE FUCKING KIDDING ME ??!!!

	for (i = 0; i < 4; ++i) {
		input[0] = *((volatile uint64_t *)ci_address + 0);
		__asm__ __volatile__("isync; sync; eieio;" : : : "memory");

		input[1] = *((volatile uint64_t *)ci_address + 1);
		input[2] = *((volatile uint64_t *)ci_address + 2);
		input[3] = *((volatile uint64_t *)ci_address + 3);

		input[4] = *((volatile uint64_t *)ci_address + 4);
		input[5] = *((volatile uint64_t *)ci_address + 5);
		input[6] = *((volatile uint64_t *)ci_address + 6);
		input[7] = *((volatile uint64_t *)ci_address + 7);

		input[8] = *((volatile uint64_t *)ci_address + 8);
		input[9] = *((volatile uint64_t *)ci_address + 9);
		input[10] = *((volatile uint64_t *)ci_address + 10);
		input[11] = *((volatile uint64_t *)ci_address + 11);

		input[12] = *((volatile uint64_t *)ci_address + 12);
		input[13] = *((volatile uint64_t *)ci_address + 13);
		input[14] = *((volatile uint64_t *)ci_address + 14);
		input[15] = *((volatile uint64_t *)ci_address + 15);

		__asm__ __volatile__("isync; sync;" : : : "memory");

		byte_interleave(input, tmp[i]);
		byte_interleave(&input[8], &tmp[i][8]);

		__asm__ __volatile__("isync; sync; dcbz %y0; sync; isync"
				     :
				     : "Z"(*(uint8_t *)ci_address)
				     : "memory");
		__asm__ __volatile__("isync; sync; dcbf %y0; sync; isync"
				     :
				     : "Z"(*((uint8_t *)ci_address))
				     : "memory");
	}

	pr_debug("result: %016llx %016llx %016llx %016llx\n", tmp[0][CIID],
		 tmp[1][CIID], tmp[2][CIID], tmp[3][CIID]);

	memcpy(ptr_block_data, tmp[3], 64);

	spin_unlock_irqrestore(&ci_lock, flags);
}

void power9_destroy_rank(struct dpu_region_address_translation *tr,
			 uint8_t channel_id)
{
	pr_info("%s\n", __func__);
}

int power9_init_rank(struct dpu_region_address_translation *tr,
		     uint8_t channel_id)
{
	return 0;
}

struct dpu_region_address_translation power9_translate = {
	.desc = {
		.topology.nr_of_control_interfaces = 1,
		.topology.nr_of_dpus_per_control_interface = 8,
		.memories.mram_size = 64 * SZ_1M,
		.memories.wram_size = 64 * SZ_1K,
		.memories.iram_size = SZ_4K,
		.timings.carousel.cmd_duration = 2,
		.timings.carousel.cmd_sampling = 1,
		.timings.carousel.res_duration = 2,
		.timings.carousel.res_sampling = 1,
		.timings.reset_wait_duration = 20,
		.timings.std_temperature = 110,
		.dpu.nr_of_threads = 24,
		.dpu.nr_of_atomic_bits = 256,
		.dpu.nr_of_notify_bits = 40,
		.dpu.nr_of_work_registers_per_thread = 24,
	},
	.backend_id = DPU_BACKEND_POWER9,
	.capabilities = CAP_PERF | CAP_SAFE,
	.init_rank = power9_init_rank,
	.write_to_rank = power9_write_to_rank,
	.read_from_rank = power9_read_from_rank,
	.write_to_cis = power9_write_to_cis,
	.read_from_cis = power9_read_from_cis,
};
