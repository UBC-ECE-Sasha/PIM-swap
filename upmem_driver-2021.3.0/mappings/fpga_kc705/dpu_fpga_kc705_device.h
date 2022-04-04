/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 UPMEM. All rights reserved. */
#ifndef DPU_FPGA_KC705_DEVICE_H
#define DPU_FPGA_KC705_DEVICE_H

#include <linux/cdev.h>
#include <linux/kref.h>

#define CONTROL_INTERFACE_BANK_NUMBER 1
#define CONTROL_INTERFACE_BAR_OFFSET 0x40000000

#define DPU_ANALYZER_OFFSET 0x1000
#define ILA_TRIGGER_OFFSET 0x0
#define ILA_EMPTY_FLAGS_OFFSET 0x8
#define ILA_FULL_FLAGS_OFFSET 0x10
#define ILA_RESET_OFFSET 0x18
#define ILA_VALUE_COUNTER_OFFSET 0x20
#define ILA_FILTER_OFFSET 0x28

#define ILA_POP_GROUP_0_OFFSET 0x40
#define ILA_POP_GROUP_1_OFFSET 0x50
#define ILA_POP_GROUP_2_OFFSET 0x60

#define MRAM_BYPASS_OFFSET 0x70
#define MRAM_EMUL_REFRESH_OFFSET 0x78

#define ILA_FIFO_DEPTH (1 << 16)
#define ILA_OUTPUT_ENTRY_LENGTH 79

#define TRIGGER_ENABLE_SHIFT 63
#define TRIGGER_SELECTION_SHIFT 56
#define TRIGGER_VALUE_SHIFT 0

#define ISTATE_TRIGGER_SELECTION 5ULL
#define ISTATE_BOOT_STATE_VALUE 3ULL

/* Add-ons */
#define MRAM_DEBUG_ADDON 0x0001
#define SPI_ADDON 0x0002
#define ILA_ADDON 0x0004

#define DESCRIPTOR_SIZE 64 // 64-byte aligned Transfer Descriptor

// Scatter Gather Transfer descriptor
typedef struct {
	u32 nextDesc; /* 0x00 */
	u32 na1; /* 0x04 */
	u32 srcAddr; /* 0x08 */
	u32 na2; /* 0x0C */
	u32 destAddr; /* 0x10 */
	u32 na3; /* 0x14 */
	u32 control; /* 0x18 */
	u32 status; /* 0x1C */
} __aligned(DESCRIPTOR_SIZE) sg_desc_t;

/**
 * struct bank_map - Device mapping of the used register banks
 * @bar:    BAR containing the register bank
 * @offs:   offset of this bank within its BAR
 * @len:    bank size, in bytes
 * @addr:   set to the base virtual address of this bank
 */
struct bank_map {
	char name[9];
	unsigned char bar;
	unsigned int offs;
	unsigned int len;
	void *addr;
	unsigned long phys;
};

// TODO not pretty
// #define BANKS_NUM       (ARRAY_SIZE(init_banks))
#define BANKS_NUM 2

/* List head of all fpga pci devices */
struct pci_device_fpga_dma {
	unsigned long gBaseHdwr, gBaseLen;
	void *gBaseVirt;
	char *gReadBuffer, *gWriteBuffer;
	dma_addr_t gReadHWAddr, gWriteHWAddr, gDescChainHWAddr;
	sg_desc_t *gDescChain;
	size_t gDescChainLength;
};

struct pci_device_fpga {
	struct pci_dev *dev;
	struct bank_map banks[BANKS_NUM];
	struct pci_device_fpga_dma dma;
};

#endif /* DPU_FPGA_KC705_DEVICE_H */
