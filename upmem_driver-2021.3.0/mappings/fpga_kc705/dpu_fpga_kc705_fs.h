/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 UPMEM. All rights reserved. */
#ifndef DPU_FPGA_KC705_FS_H
#define DPU_FPGA_KC705_FS_H

#include <dpu_fpga_kc705_device.h>

struct dpu_region;

int dpu_debugfs_sysfs_fpga_kc705_create(struct pci_device_fpga *pci_dev,
					struct dpu_region *region);
int dpu_debugfs_sysfs_fpga_kc705_destroy(struct pci_device_fpga *pci_dev,
					 struct dpu_region *region);

#endif /* DPU_FPGA_KC705_FS_H */
