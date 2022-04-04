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
#include <linux/sizes.h>

#include <dpu_rank.h>
#include <dpu_region_address_translation.h>
#include <dpu_region.h>
#include <dpu_utils.h>

#include <dpu_fpga_kc705_dma_op.h>
#include <dpu_fpga_kc705_spi.h>
#include <dpu_fpga_kc705_register.h>

void fpga_kc705_write_to_rank(struct dpu_region_address_translation *tr,
			      void *base_region_addr, uint8_t channel_id,
			      struct dpu_transfer_mram *xfer_matrix)
{
	struct pci_device_fpga *pdev = tr->private;
	struct pci_dev *pci_dev = pdev->dev;
	struct xfer_page *xferp;
	uint64_t len_xfer_remaining, len_xfer_done;
	uint32_t ptr_dpu;
	uint8_t nb_mrams;
	uint8_t nb_cis, nb_dpus_per_ci;
	uint8_t dpu_id, ci_id;
	int idx;
	int ret;

	nb_cis = tr->desc.topology.nr_of_control_interfaces;
	nb_dpus_per_ci = tr->desc.topology.nr_of_dpus_per_control_interface;

	if (pci_dev->subsystem_device & MRAM_DEBUG_ADDON)
		nb_mrams = 2;
	else
		nb_mrams = 1;

	for_each_dpu_in_rank(idx, ci_id, dpu_id, nb_cis, nb_dpus_per_ci)
	{
		uint32_t page;
		uint32_t len_xfer_in_page;
		uint32_t off_in_page;

		xferp = xfer_matrix->ptr[idx];
		if (!xferp)
			continue;

		len_xfer_remaining = xfer_matrix->size;
		len_xfer_done = 0;

		ptr_dpu = dpu_id * tr->desc.memories.mram_size * nb_mrams +
			  xfer_matrix->offset_in_mram;

		for (page = 0; page < xferp->nb_pages; ++page) {
			off_in_page = !page ? xferp->off_first_page : 0;

			len_xfer_in_page =
				min((uint32_t)(PAGE_SIZE - off_in_page),
				    (uint32_t)len_xfer_remaining);

			ret = sg_block(
				pdev, PCI_DMA_TODEVICE,
				page_to_virt(xferp->pages[page]) + off_in_page,
				len_xfer_in_page, ptr_dpu + len_xfer_done);
			if (ret) {
				pr_warn("Error while transmitting data, stopping transfer at ci"
					" %u and dpu %u\n",
					ci_id, dpu_id);
				return;
			}

			len_xfer_remaining -= len_xfer_in_page;
			len_xfer_done += len_xfer_in_page;
		}
	}
}

void fpga_kc705_read_from_rank(struct dpu_region_address_translation *tr,
			       void *base_region_addr, uint8_t channel_id,
			       struct dpu_transfer_mram *xfer_matrix)
{
	struct pci_device_fpga *pdev = tr->private;
	struct pci_dev *pci_dev = pdev->dev;
	struct xfer_page *xferp;
	uint64_t len_xfer_remaining, len_xfer_done;
	uint32_t ptr_dpu;
	uint8_t nb_mrams;
	uint8_t nb_cis, nb_dpus_per_ci;
	uint8_t dpu_id, ci_id;
	int idx;
	int ret;

	nb_cis = tr->desc.topology.nr_of_control_interfaces;
	nb_dpus_per_ci = tr->desc.topology.nr_of_dpus_per_control_interface;

	if (pci_dev->subsystem_device & MRAM_DEBUG_ADDON)
		nb_mrams = 2;
	else
		nb_mrams = 1;

	for_each_dpu_in_rank(idx, ci_id, dpu_id, nb_cis, nb_dpus_per_ci)
	{
		uint32_t page;
		uint32_t len_xfer_in_page;
		uint32_t off_in_page;

		xferp = xfer_matrix->ptr[idx];
		if (!xferp)
			continue;

		len_xfer_remaining = xfer_matrix->size;
		len_xfer_done = 0;

		ptr_dpu = dpu_id * tr->desc.memories.mram_size * nb_mrams +
			  xfer_matrix->offset_in_mram;

		for (page = 0; page < xferp->nb_pages; ++page) {
			off_in_page = !page ? xferp->off_first_page : 0;

			len_xfer_in_page =
				min((uint32_t)(PAGE_SIZE - off_in_page),
				    (uint32_t)len_xfer_remaining);

			ret = sg_block(
				pdev, PCI_DMA_FROMDEVICE,
				page_to_virt(xferp->pages[page]) + off_in_page,
				len_xfer_in_page, ptr_dpu + len_xfer_done);
			if (ret) {
				pr_warn("Error while transmitting data, stopping transfer at ci"
					" %u and dpu %u\n",
					ci_id, dpu_id);
				return;
			}

			len_xfer_remaining -= len_xfer_in_page;
			len_xfer_done += len_xfer_in_page;
		}
	}
}

void fpga_kc705_write_to_cis(struct dpu_region_address_translation *tr,
			     void *base_region_addr, uint8_t channel_id,
			     void *block_data)
{
	struct pci_device_fpga *pdev = tr->private;
	struct dpu_region *region =
		container_of(pdev, struct dpu_region, dpu_fpga_kc705_dev);

	dpu_device_write_register(block_data, CONTROL_INTERFACE_BAR_OFFSET,
				  pdev, region);
}

void fpga_kc705_read_from_cis(struct dpu_region_address_translation *tr,
			      void *base_region_addr, uint8_t channel_id,
			      void *block_data)
{
	struct pci_device_fpga *pdev = tr->private;
	struct dpu_region *region =
		container_of(pdev, struct dpu_region, dpu_fpga_kc705_dev);

	dpu_device_read_register(block_data, CONTROL_INTERFACE_BAR_OFFSET, pdev,
				 region);
}

struct dpu_region_address_translation fpga_kc705_translate_1dpu = {
	.desc = {
		.topology.nr_of_control_interfaces = 1,
		.topology.nr_of_dpus_per_control_interface = 1,
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
	.backend_id = DPU_BACKEND_FPGA_KC705,
	.capabilities = CAP_SAFE,
	.write_to_rank = fpga_kc705_write_to_rank,
	.read_from_rank = fpga_kc705_read_from_rank,
	.write_to_cis = fpga_kc705_write_to_cis,
	.read_from_cis = fpga_kc705_read_from_cis,
};

struct dpu_region_address_translation fpga_kc705_translate_8dpu = {
	.desc = {
		.topology.nr_of_control_interfaces = 1,
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
	.backend_id = DPU_BACKEND_FPGA_KC705,
	.capabilities = CAP_SAFE,
	.write_to_rank = fpga_kc705_write_to_rank,
	.read_from_rank = fpga_kc705_read_from_rank,
	.write_to_cis = fpga_kc705_write_to_cis,
	.read_from_cis = fpga_kc705_read_from_cis,
};
