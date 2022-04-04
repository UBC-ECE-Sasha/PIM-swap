/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 UPMEM. All rights reserved. */
#include <linux/kernel.h>
#include <linux/version.h>
#include <asm/cacheflush.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/sizes.h>
#include <asm/page.h>
#include <asm/io.h>

#include <dpu_rank.h>
#include <dpu_region_address_translation.h>
#include <dpu_region.h>
#include <dpu_utils.h>

#include <dpu_fpga_aws_libxdma.h>
#include <dpu_fpga_aws_libxdma_api.h>
#include <dpu_fpga_aws_xdma_mod.h>

void fpga_aws_write_to_rank(struct dpu_region_address_translation *tr,
			    void *base_region_addr, uint8_t channel_id,
			    struct dpu_transfer_mram *xfer_matrix)
{
	struct dpu_region *region = tr->private;
	struct xdma_dev *xdev = region->dpu_fpga_aws_dev;
	struct xdma_io_cb cb;
	struct xfer_page *xferp;
	uint64_t ptr_dpu;
	uint64_t len_xfer_remaining, len_xfer_done;
	int ret;
	int idx;
	uint8_t nb_cis, nb_dpus_per_ci;
	uint8_t dpu_id, ci_id;

	nb_cis = tr->desc.topology.nr_of_control_interfaces;
	nb_dpus_per_ci = tr->desc.topology.nr_of_dpus_per_control_interface;

	for_each_dpu_in_rank(idx, ci_id, dpu_id, nb_cis, nb_dpus_per_ci)
	{
		struct scatterlist *sg;
		uint32_t page;
		uint32_t len_xfer_in_page;
		uint32_t off_in_page;
		uint8_t physical_channel_id = ci_id % 4;

		xferp = xfer_matrix->ptr[idx];
		if (!xferp)
			continue;

		pr_debug("dpu_rank%d: xfer from (%hhu, %hhu)\n",
			 region->rank.id, ci_id, dpu_id);

		len_xfer_remaining = xfer_matrix->size;
		len_xfer_done = 0;

		ptr_dpu = physical_channel_id * 0x400000000ULL +
			  dpu_id * tr->desc.memories.mram_size * 2 +
			  xfer_matrix->offset_in_mram;

		ret = sg_alloc_table(&cb.sgt, xferp->nb_pages, GFP_KERNEL);
		if (ret) {
			pr_err("Can't allocate sg table\n");
			return;
		}

		sg = cb.sgt.sgl;

		for (page = 0; page < xferp->nb_pages;
		     ++page, sg = sg_next(sg)) {
			off_in_page = !page ? xferp->off_first_page : 0;

			len_xfer_in_page =
				min((uint32_t)(PAGE_SIZE - off_in_page),
				    (uint32_t)len_xfer_remaining);

			sg_set_page(sg, xferp->pages[page], len_xfer_in_page,
				    off_in_page);

			len_xfer_remaining -= len_xfer_in_page;
			len_xfer_done += len_xfer_in_page;
		}

		ret = xdma_xfer_submit(xdev, physical_channel_id, 1, ptr_dpu,
				       &cb.sgt, 0, 10000);

		sg_free_table(&cb.sgt);

		if (ret != xfer_matrix->size) {
			printk(KERN_ERR "Error while transmitting data,"
					" stopping transfer at ci %d "
					"and dpu %d\n",
			       ci_id, dpu_id);
			return;
		}
	}
}

void fpga_aws_read_from_rank(struct dpu_region_address_translation *tr,
			     void *base_region_addr, uint8_t channel_id,
			     struct dpu_transfer_mram *xfer_matrix)
{
	struct dpu_region *region = tr->private;
	struct xdma_dev *xdev = region->dpu_fpga_aws_dev;
	struct xdma_io_cb cb;
	struct xfer_page *xferp;
	uint64_t len_xfer_remaining, len_xfer_done;
	uint64_t ptr_dpu;
	int ret;
	int idx;
	uint8_t nb_cis, nb_dpus_per_ci;
	uint8_t dpu_id, ci_id;

	nb_cis = tr->desc.topology.nr_of_control_interfaces;
	nb_dpus_per_ci = tr->desc.topology.nr_of_dpus_per_control_interface;

	for_each_dpu_in_rank(idx, ci_id, dpu_id, nb_cis, nb_dpus_per_ci)
	{
		struct scatterlist *sg;
		uint32_t page;
		uint32_t len_xfer_in_page;
		uint32_t off_in_page;
		uint8_t physical_channel_id = ci_id % 4;

		xferp = xfer_matrix->ptr[idx];
		if (!xferp)
			continue;

		pr_debug("dpu_rank%d: xfer from (%hhu, %hhu)\n",
			 region->rank.id, ci_id, dpu_id);

		len_xfer_remaining = xfer_matrix->size;
		len_xfer_done = 0;

		ptr_dpu = physical_channel_id * 0x400000000ULL +
			  dpu_id * tr->desc.memories.mram_size * 2 +
			  xfer_matrix->offset_in_mram;

		ret = sg_alloc_table(&cb.sgt, xferp->nb_pages, GFP_KERNEL);
		if (ret) {
			pr_err("Can't allocate sg table\n");
			return;
		}

		sg = cb.sgt.sgl;

		for (page = 0; page < xferp->nb_pages;
		     ++page, sg = sg_next(sg)) {
			off_in_page = !page ? xferp->off_first_page : 0;

			len_xfer_in_page =
				min((uint32_t)(PAGE_SIZE - off_in_page),
				    (uint32_t)len_xfer_remaining);

			sg_set_page(sg, xferp->pages[page], len_xfer_in_page,
				    off_in_page);

			len_xfer_remaining -= len_xfer_in_page;
			len_xfer_done += len_xfer_in_page;
		}

		ret = xdma_xfer_submit(xdev, physical_channel_id, 0, ptr_dpu,
				       &cb.sgt, 0, 10000);

		sg_free_table(&cb.sgt);

		if (ret != xfer_matrix->size) {
			printk(KERN_ERR "Error while transmitting data,"
					" stopping transfer at ci %d "
					"and dpu %d\n",
			       ci_id, dpu_id);
			return;
		}
	}
}

void fpga_aws_write_to_cis(struct dpu_region_address_translation *tr,
			   void *base_region_addr, uint8_t channel_id,
			   void *block_data)
{
	struct dpu_region *region = tr->private;
	uint64_t *command;
	void *ptr_block_data = block_data;
	uint8_t nb_cis;
	int i;

	nb_cis = tr->desc.topology.nr_of_control_interfaces;

	for (i = 0; i < nb_cis; ++i) {
		uint64_t off_in_bar = i * 0x1000 + 0x1000000000ULL;

		command = &((uint64_t *)ptr_block_data)[i];

		pr_debug("dpu_rank%d: Writing %llx to %d %llx\n",
			 region->rank.id, *command, i, off_in_bar);

		if (*command == 0ULL)
			continue;

		writeq(*command,
		       (volatile uint8_t *)base_region_addr + off_in_bar);
	}
}

void fpga_aws_read_from_cis(struct dpu_region_address_translation *tr,
			    void *base_region_addr, uint8_t channel_id,
			    void *block_data)
{
	struct dpu_region *region = tr->private;
	uint64_t *result;
	void *ptr_block_data = block_data;
	uint8_t nb_cis;
	int i;

	nb_cis = tr->desc.topology.nr_of_control_interfaces;

	for (i = 0; i < nb_cis; ++i) {
		uint64_t result_tmp;
		uint64_t off_in_bar = i * 0x1000 + 0x1000000000ULL;

		result = &((uint64_t *)ptr_block_data)[i];

		result_tmp = readq((volatile uint8_t *)base_region_addr +
				   off_in_bar);

		if (result_tmp == 0ULL)
			continue;

		*result = result_tmp;

		pr_debug("dpu_rank%d: Reading %llx from %d %llx\n",
			 region->rank.id, *(uint64_t *)result, i,
			 (uint64_t)base_region_addr);
	}
}

int fpga_aws_mmap_hybrid(struct dpu_region_address_translation *tr,
			 struct file *filp, struct vm_area_struct *vma)
{
	struct dpu_region *region = tr->private;
	struct xdma_dev *xdev = region->dpu_fpga_aws_dev;
	uint64_t vm_size;
	int ret;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vm_size = vma->vm_end - vma->vm_start;

	if (vm_size < tr->hybrid_mmap_size)
		return -EINVAL;

	vma->vm_flags |= VM_IO;
	vma->vm_pgoff +=
		(pci_resource_start(xdev->pdev, 4) + 0x1000000000ULL) >>
		PAGE_SHIFT;

	ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vm_size,
			      vma->vm_page_prot);
	if (ret) {
		pr_debug("dpu_rank%d: remap_pfn_range failed.\n.",
			 region->rank.id);
		return ret;
	}

	return 0;
}

void fpga_aws_destroy_rank(struct dpu_region_address_translation *tr,
			   uint8_t channel_id)
{
}

int fpga_aws_init_rank(struct dpu_region_address_translation *tr,
		       uint8_t channel_id)
{
	return 0;
}

struct dpu_region_address_translation fpga_aws_translate = {
	.desc = {
		.topology.nr_of_control_interfaces = 4,
		.topology.nr_of_dpus_per_control_interface = 8,
		.memories.mram_size = 64 * SZ_1M,
		.memories.wram_size = 64 * SZ_1K,
		.memories.iram_size = SZ_4K,
		.timings.fck_frequency_in_mhz = 600,
		.timings.carousel.cmd_duration = 2,
		.timings.carousel.cmd_sampling = 1,
		.timings.carousel.res_duration = 2,
		.timings.carousel.res_sampling = 1,
		.timings.reset_wait_duration = 20,
		.timings.clock_division = 3,
		.timings.std_temperature = 110,
		.dpu.nr_of_threads = 24,
		.dpu.nr_of_atomic_bits = 256,
		.dpu.nr_of_notify_bits = 40,
		.dpu.nr_of_work_registers_per_thread = 24,
	},
	.backend_id = DPU_BACKEND_FPGA_AWS,
	.capabilities = CAP_HYBRID_CONTROL_INTERFACE | CAP_SAFE,
	.hybrid_mmap_size = 4 /* nb_cis */ * 4 * 1024,
	.init_rank = fpga_aws_init_rank,
	.destroy_rank = fpga_aws_destroy_rank,
	.write_to_rank = fpga_aws_write_to_rank,
	.read_from_rank = fpga_aws_read_from_rank,
	.write_to_cis = fpga_aws_write_to_cis,
	.read_from_cis = fpga_aws_read_from_cis,
	.mmap_hybrid = fpga_aws_mmap_hybrid
};

struct dpu_region_address_translation fpga_bittware_translate = {
	.desc = {
		.topology.nr_of_control_interfaces = 4,
		.topology.nr_of_dpus_per_control_interface = 8,
		.memories.mram_size = 64 * SZ_1M,
		.memories.wram_size = 64 * SZ_1K,
		.memories.iram_size = SZ_4K,
		.timings.fck_frequency_in_mhz = 750,
		.timings.carousel.cmd_duration = 2,
		.timings.carousel.cmd_sampling = 1,
		.timings.carousel.res_duration = 2,
		.timings.carousel.res_sampling = 1,
		.timings.reset_wait_duration = 20,
		.timings.clock_division = 3,
		.timings.std_temperature = 110,
		.dpu.nr_of_threads = 24,
		.dpu.nr_of_atomic_bits = 256,
		.dpu.nr_of_notify_bits = 40,
		.dpu.nr_of_work_registers_per_thread = 24,
	},
	.backend_id = DPU_BACKEND_FPGA_AWS,
	.capabilities = CAP_HYBRID_CONTROL_INTERFACE | CAP_SAFE,
	.hybrid_mmap_size = 4 /* nb_cis */ * 4 * 1024,
	.init_rank = fpga_aws_init_rank,
	.destroy_rank = fpga_aws_destroy_rank,
	.write_to_rank = fpga_aws_write_to_rank,
	.read_from_rank = fpga_aws_read_from_rank,
	.write_to_cis = fpga_aws_write_to_cis,
	.read_from_cis = fpga_aws_read_from_cis,
	.mmap_hybrid = fpga_aws_mmap_hybrid
};
