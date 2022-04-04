/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 UPMEM. All rights reserved. */
#ifndef DPU_FPGA_KC705_REGISTER_H
#define DPU_FPGA_KC705_REGISTER_H

#include <dpu_fpga_kc705_device.h>
#include <dpu_fpga_kc705_spi.h>

#include <dpu_region.h>

static inline void write_register64(const uint64_t *buff, uint32_t off,
				    struct pci_device_fpga *pdev)
{
	volatile void *ptr = pdev->banks[CONTROL_INTERFACE_BANK_NUMBER].addr;

	writeq(*buff, ptr + off);
}

static inline void read_register64(uint64_t *buff, uint32_t off,
				   struct pci_device_fpga *pdev)
{
	volatile void *ptr = pdev->banks[CONTROL_INTERFACE_BANK_NUMBER].addr;

	*buff = readq(ptr + off);
}

static inline int dpu_device_read_register(uint64_t *buf, uint32_t off,
					   struct pci_device_fpga *pdev,
					   struct dpu_region *region)
{
	if (region->spi_mode_enabled && (off == CONTROL_INTERFACE_BAR_OFFSET)) {
		dpu_spi_read(pdev->banks[DPU_SPI_BANK_NUMBER].addr, buf, 8);
	} else {
		read_register64(buf, off, pdev);
	}

	pr_debug("Control interface register read  : 0x%llx\n", *buf);

	return 0;
}

static inline int dpu_device_write_register(uint64_t *buf, uint32_t off,
					    struct pci_device_fpga *pdev,
					    struct dpu_region *region)
{
	if (region->spi_mode_enabled && (off == CONTROL_INTERFACE_BAR_OFFSET)) {
		dpu_spi_write(pdev->banks[DPU_SPI_BANK_NUMBER].addr, buf, 8);
	} else {
		write_register64(buf, off, pdev);
	}

	pr_debug("Control interface register write : 0x%llx\n", *buf);

	return 0;
}

#endif /* DPU_FPGA_KC705_REGISTER_H */
