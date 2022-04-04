/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 UPMEM. All rights reserved.
 * Includes portions of codes from https://github.com/strezh/XPDMA,
 * with the authorization of the author Iurii Strezhik <strezhik@gmail.com>.
 */

#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */
#include <linux/fs.h>       /* Needed for files operations */
#include <linux/pci.h>      /* Needed for PCI */
#include <linux/mutex.h>    /* Exclusive scatter/gather operations */
#include <linux/uaccess.h>    /* Needed for copy_to_user & copy_from_user */
#include <linux/delay.h>    /* udelay, mdelay */
#include <linux/dma-mapping.h>

#include <dpu_fpga_kc705_dma_op.h>

// Max CDMA buffer size
#define MAX_BTT             0x007FFFFF   // 8 MBytes maximum for DMA Transfer */
#define BUF_SIZE            (4<<20)      // 4 MBytes read/write buffer size
#define TRANSFER_SIZE       (4<<20)      // 4 MBytes transfer size for scatter gather

#define BRAM_OFFSET         0x00000000   // Translation BRAM offset
#define PCIE_CTL_OFFSET     0x00008000   // AXI PCIe control offset
#define CDMA_OFFSET         0x0000c000   // AXI CDMA LITE control offset

// AXI CDMA Register Offsets
#define CDMA_CONTROL_OFFSET	0x00         // Control Register
#define CDMA_STATUS_OFFSET	0x04         // Status Register
#define CDMA_CDESC_OFFSET	0x08         // Current descriptor Register
#define CDMA_TDESC_OFFSET	0x10         // Tail descriptor Register
#define CDMA_SRCADDR_OFFSET	0x18         // Source Address Register
#define CDMA_DSTADDR_OFFSET	0x20         // Dest Address Register
#define CDMA_BTT_OFFSET		0x28         // Bytes to transfer Register

#define AXI_PCIE_DM_ADDR    0x80000000   // AXI:BAR1 Address
#define AXI_PCIE_SG_ADDR    0x80800000   // AXI:BAR0 Address
#define AXI_BRAM_ADDR       0x81000000   // AXI Translation BRAM Address
#define AXI_DDR3_ADDR       0x00000000   // AXI DDR3 Address

#define SG_COMPLETE_MASK    0xF0000000   // Scatter Gather Operation Complete status flag mask
#define SG_DEC_ERR_MASK     0x40000000   // Scatter Gather Operation Decode Error flag mask
#define SG_SLAVE_ERR_MASK   0x20000000   // Scatter Gather Operation Slave Error flag mask
#define SG_INT_ERR_MASK     0x10000000   // Scatter Gather Operation Internal Error flag mask

#define BRAM_STEP           0x8          // Translation Vector Length
#define ADDR_BTT            0x00000008   // 64 bit address translation descriptor control length

#define CDMA_CR_SG_EN       0x00000008   // Scatter gather mode enable
#define CDMA_CR_IDLE_MASK   0x00000002   // CDMA Idle mask
#define CDMA_CR_RESET_MASK  0x00000004   // CDMA Reset mask
#define AXIBAR2PCIEBAR_0U   0x208        // AXI:BAR0 Upper Address Translation (bits [63:32])
#define AXIBAR2PCIEBAR_0L   0x20C        // AXI:BAR0 Lower Address Translation (bits [31:0])
#define AXIBAR2PCIEBAR_1U   0x210        // AXI:BAR1 Upper Address Translation (bits [63:32])
#define AXIBAR2PCIEBAR_1L   0x214        // AXI:BAR1 Lower Address Translation (bits [31:0])

#define CDMA_RESET_LOOP	    1000000      // Reset timeout counter limit
#define SG_TRANSFER_LOOP	1000000      // Scatter Gather Transfer timeout counter limit

// TODO by pdev
static DEFINE_MUTEX(sg_mutex);

unsigned int gStatFlags = 0x00;     // Status flags used for cleanup

#define HAVE_KERNEL_REG     0x01    // Kernel registration
#define HAVE_MEM_REGION     0x02    // I/O Memory region

#define SUCCESS      0
#define CRIT_ERR    -1
#define AGAIN       -2

#define DEVICE_NAME "dpu"

#define HANDLE_ALIGNMENT_IN_SW 1

static inline u32 xpdma_readReg(struct pci_device_fpga *pdev, u32 reg);
static inline void xpdma_writeReg(struct pci_device_fpga *pdev,
				  u32 reg, u32 val);
static int xpdma_reset(struct pci_device_fpga *pdev);
static inline int xpdma_isIdle(struct pci_device_fpga *pdev);
ssize_t create_desc_chain(struct pci_device_fpga *pdev, int direction,
			  u32 size, u32 addr);
static void show_descriptors(struct pci_device_fpga *pdev);
static int sg_operation(struct pci_device_fpga *pdev, int direction,
			size_t count, u32 addr);

// IO access (with byte addressing)
static inline u32 xpdma_readReg (struct pci_device_fpga *pdev, u32 reg)
{
    dev_dbg(&pdev->dev->dev,  "%s: xpdma_readReg: address:0x%08X\t\n", DEVICE_NAME, reg);
    return readl(pdev->dma.gBaseVirt + reg);
}

static inline void xpdma_writeReg (struct pci_device_fpga *pdev, u32 reg, u32 val)
{
    dev_dbg(&pdev->dev->dev,  "%s: xpdma_writeReg: address:0x%08X\t data: 0x%08X\n", DEVICE_NAME, reg, val);
    writel(val, (pdev->dma.gBaseVirt + reg));
}

static int xpdma_reset(struct pci_device_fpga *pdev)
{
    int loop = CDMA_RESET_LOOP;
    u32 tmp;

    dev_dbg(&pdev->dev->dev, "%s: RESET CDMA\n", DEVICE_NAME);

    xpdma_writeReg(pdev, (CDMA_OFFSET + CDMA_CONTROL_OFFSET),
                   xpdma_readReg(pdev, CDMA_OFFSET + CDMA_CONTROL_OFFSET) | CDMA_CR_RESET_MASK);

    tmp = xpdma_readReg(pdev, CDMA_OFFSET + CDMA_CONTROL_OFFSET) & CDMA_CR_RESET_MASK;

    /* Wait for the hardware to finish reset */
    while (loop && tmp) {
        tmp = xpdma_readReg(pdev, CDMA_OFFSET + CDMA_CONTROL_OFFSET) & CDMA_CR_RESET_MASK;
        loop--;
    }

    if (!loop) {
        printk(KERN_ERR"%s: reset timeout, CONTROL_REG: 0x%08X, STATUS_REG 0x%08X\n",
                DEVICE_NAME,
                xpdma_readReg(pdev, CDMA_OFFSET + CDMA_CONTROL_OFFSET),
                xpdma_readReg(pdev, CDMA_OFFSET + CDMA_STATUS_OFFSET));
        return (CRIT_ERR);
    }

    // For Axi CDMA, always do sg transfers if sg mode is built in
    xpdma_writeReg(pdev, CDMA_OFFSET + CDMA_CONTROL_OFFSET, tmp | CDMA_CR_SG_EN);

    dev_dbg(&pdev->dev->dev, "%s: SUCCESSFULLY RESET CDMA!\n", DEVICE_NAME);

    return (SUCCESS);
}

static inline int xpdma_isIdle(struct pci_device_fpga *pdev)
{
    return xpdma_readReg(pdev, CDMA_OFFSET + CDMA_STATUS_OFFSET) &
           CDMA_CR_IDLE_MASK;
}

ssize_t create_desc_chain(struct pci_device_fpga *pdev, int direction,
			  u32 size, u32 addr)
{
    u32 count = 0;                 // length of descriptors chain
    u32 sgAddr = AXI_PCIE_SG_ADDR; // current descriptor address in chain
    u32 bramAddr = AXI_BRAM_ADDR ; // Translation BRAM Address
    u32 btt = 0;                   // current descriptor BTT
    u32 unmappedSize = size;       // unmapped data size
    u32 srcAddr = 0;               // source address (SG_DM of DDR3)
    u32 destAddr = 0;              // destination address (SG_DM of DDR3)

    pdev->dma.gDescChainLength = (size + (u32)(TRANSFER_SIZE) - 1) / (u32)(TRANSFER_SIZE);
    dev_dbg(&pdev->dev->dev, "%s: gDescChainLength = %lu\n", DEVICE_NAME,
	   pdev->dma.gDescChainLength);

    // TODO: future: add PCI_DMA_NONE as indicator of MEM 2 MEM transitions
    if (direction == PCI_DMA_FROMDEVICE) {
        srcAddr  = AXI_DDR3_ADDR + addr;
        destAddr = AXI_PCIE_DM_ADDR;
    } else if (direction == PCI_DMA_TODEVICE) {
        srcAddr  = AXI_PCIE_DM_ADDR;
        destAddr = AXI_DDR3_ADDR + addr;
    } else {
        printk(KERN_ERR"%s: Descriptors Chain create error: unknown direction\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    // fill descriptor chain
    dev_dbg(&pdev->dev->dev, "%s: fill descriptor chain\n", DEVICE_NAME);
    for (count = 0; count < pdev->dma.gDescChainLength; ++count) {
        sg_desc_t *addrDesc = pdev->dma.gDescChain + 2 * count; // address translation descriptor
        sg_desc_t *dataDesc = addrDesc + 1;                // target data transfer descriptor
        btt = (unmappedSize > TRANSFER_SIZE) ? TRANSFER_SIZE : unmappedSize;

        // fill address translation descriptor
        dev_dbg(&pdev->dev->dev, "%s: fill address translation descriptor\n", DEVICE_NAME);
        addrDesc->nextDesc  = sgAddr + DESCRIPTOR_SIZE;
        addrDesc->srcAddr   = bramAddr;
        addrDesc->destAddr  = AXI_BRAM_ADDR + PCIE_CTL_OFFSET + AXIBAR2PCIEBAR_1U;
        addrDesc->control   = ADDR_BTT;
        addrDesc->status    = 0x00000000;
        sgAddr += DESCRIPTOR_SIZE;

        // fill target data transfer descriptor
        dev_dbg(&pdev->dev->dev, "%s: fill address data transfer descriptor\n", DEVICE_NAME);
        dataDesc->nextDesc  = sgAddr + DESCRIPTOR_SIZE;
        dataDesc->srcAddr   = srcAddr;
        dataDesc->destAddr  = destAddr;
        dataDesc->control   = btt;
        dataDesc->status    = 0x00000000;
        sgAddr += DESCRIPTOR_SIZE;

        dev_dbg(&pdev->dev->dev, "%s: update counters\n", DEVICE_NAME);
        bramAddr += BRAM_STEP;
        unmappedSize -= btt;
        srcAddr += btt;
        destAddr += btt;
    }

    pdev->dma.gDescChain[2 * pdev->dma.gDescChainLength - 1].nextDesc = AXI_PCIE_SG_ADDR; // tail descriptor pointed to chain head

    return (SUCCESS);
}

static void show_descriptors(struct pci_device_fpga *pdev)
{
    int c = 0;
    sg_desc_t *desc = pdev->dma.gDescChain;

    dev_dbg(&pdev->dev->dev, "%s: Translation vectors:\n", DEVICE_NAME);
    dev_dbg(&pdev->dev->dev, "%s: Operation_1 Upper: %08X\n", DEVICE_NAME, xpdma_readReg(pdev, 0));
    dev_dbg(&pdev->dev->dev, "%s: Operation_1 Lower: %08X\n", DEVICE_NAME, xpdma_readReg(pdev, 4));
    dev_dbg(&pdev->dev->dev, "%s: Operation_2 Upper: %08X\n", DEVICE_NAME, xpdma_readReg(pdev, 8));
    dev_dbg(&pdev->dev->dev, "%s: Operation_2 Lower: %08X\n", DEVICE_NAME, xpdma_readReg(pdev, 12));

    for (c = 0; c < 4; ++c) {
        dev_dbg(&pdev->dev->dev, "%s: Descriptor %d\n", DEVICE_NAME, c);
        dev_dbg(&pdev->dev->dev, "%s: nextDesc 0x%08X\n", DEVICE_NAME, desc->nextDesc);
        dev_dbg(&pdev->dev->dev, "%s: srcAddr 0x%08X\n", DEVICE_NAME, desc->srcAddr);
        dev_dbg(&pdev->dev->dev, "%s: destAddr 0x%08X\n", DEVICE_NAME, desc->destAddr);
        dev_dbg(&pdev->dev->dev, "%s: control 0x%08X\n", DEVICE_NAME, desc->control);
        dev_dbg(&pdev->dev->dev, "%s: status 0x%08X\n", DEVICE_NAME, desc->status);
        desc++;        // target data transfer desc
    }
}

static int sg_operation(struct pci_device_fpga *pdev, int direction,
			size_t count, u32 addr)
{
    u32 status = 0;
    size_t pntr = 0;
    size_t delayTime = 0;
    u32 countBuf = count;
    size_t bramOffset = 0;

    if (!xpdma_isIdle(pdev)){
        printk(KERN_ERR"%s: CDMA is not idle\n", DEVICE_NAME);
        return (AGAIN);
    }

    // 1. Set DMA to Scatter Gather Mode
    dev_dbg(&pdev->dev->dev, "%s: 1. Set DMA to Scatter Gather Mode\n", DEVICE_NAME);
    xpdma_writeReg (pdev, CDMA_OFFSET + CDMA_CONTROL_OFFSET, CDMA_CR_SG_EN);

    // 2. Create Descriptors chain
    dev_dbg(&pdev->dev->dev, "%s: 2. Create Descriptors chain\n", DEVICE_NAME);
    create_desc_chain(pdev, direction, count, addr);

    // 3. Update PCIe Translation vector
    pntr =  (size_t) (pdev->dma.gDescChainHWAddr);
    dev_dbg(&pdev->dev->dev, "%s: 3. Update PCIe Translation vector\n", DEVICE_NAME);
    dev_dbg(&pdev->dev->dev, "%s: gDescChain 0x%016lX\n", DEVICE_NAME, pntr);
    xpdma_writeReg (pdev, (PCIE_CTL_OFFSET + AXIBAR2PCIEBAR_0L), (pntr >> 0)  & 0xFFFFFFFF); // Lower 32 bit
    xpdma_writeReg (pdev, (PCIE_CTL_OFFSET + AXIBAR2PCIEBAR_0U), (pntr >> 32) & 0xFFFFFFFF); // Upper 32 bit

    // 4. Write appropriate Translation Vectors
    dev_dbg(&pdev->dev->dev, "%s: 4. Write Translation Vectors to BRAM\n", DEVICE_NAME);
    if (PCI_DMA_FROMDEVICE == direction) {
        pntr = (size_t)(pdev->dma.gReadHWAddr);
    } else if (PCI_DMA_TODEVICE == direction) {
        pntr = (size_t)(pdev->dma.gWriteHWAddr);
    } else {
        printk(KERN_ERR"%s: Write Translation Vectors to BRAM error: unknown direction\n", DEVICE_NAME);
        return (CRIT_ERR);
    }

    countBuf = pdev->dma.gDescChainLength;
    while (countBuf) {
        dev_dbg(&pdev->dev->dev, "%s: pntr 0x%016lX\n", DEVICE_NAME, pntr);
        dev_dbg(&pdev->dev->dev, "%s: bramOffset 0x%016lX\n", DEVICE_NAME, bramOffset);
        dev_dbg(&pdev->dev->dev, "%s: countBuf 0x%08X\n", DEVICE_NAME, countBuf);
        xpdma_writeReg (pdev, (BRAM_OFFSET + bramOffset + 4), (pntr >> 0 ) & 0xFFFFFFFF); // Lower 32 bit
        xpdma_writeReg (pdev, (BRAM_OFFSET + bramOffset + 0), (pntr >> 32) & 0xFFFFFFFF); // Upper 32 bit

        pntr += TRANSFER_SIZE;
        bramOffset += BRAM_STEP;
        countBuf--;
    }

    // 5. Write a valid pointer to DMA CURDESC_PNTR
    dev_dbg(&pdev->dev->dev, "%s: 5. Write a valid pointer to DMA CURDESC_PNTR\n", DEVICE_NAME);
    xpdma_writeReg (pdev, (CDMA_OFFSET + CDMA_CDESC_OFFSET), (AXI_PCIE_SG_ADDR));

    // 6. Write a valid pointer to DMA TAILDESC_PNTR
    dev_dbg(&pdev->dev->dev, "%s: 6. Write a valid pointer to DMA TAILDESC_PNTR\n", DEVICE_NAME);
    xpdma_writeReg (pdev, (CDMA_OFFSET + CDMA_TDESC_OFFSET), (AXI_PCIE_SG_ADDR) + ((2 * pdev->dma.gDescChainLength - 1) * (DESCRIPTOR_SIZE)));

    // wait for Scatter Gather operation...
    dev_dbg(&pdev->dev->dev, "%s: Scatter Gather must be started!\n", DEVICE_NAME);

    delayTime = SG_TRANSFER_LOOP;
    while (delayTime) {
        delayTime--;
        udelay(10);// TODO: can it be less?

        status = (pdev->dma.gDescChain + 2 * pdev->dma.gDescChainLength - 1)->status;

        dev_dbg(&pdev->dev->dev,  "%s: Scatter Gather Operation: loop counter %08lX\n", DEVICE_NAME, SG_TRANSFER_LOOP - delayTime);

        dev_dbg(&pdev->dev->dev,  "%s: Scatter Gather Operation: status 0x%08X\n", DEVICE_NAME, status);

        if (status & SG_DEC_ERR_MASK) {
            printk(KERN_ERR
            "%s: Scatter Gather Operation: Decode Error\n", DEVICE_NAME);
            show_descriptors(pdev);
            return (CRIT_ERR);
        }

        if (status & SG_SLAVE_ERR_MASK) {
            printk(KERN_ERR
            "%s: Scatter Gather Operation: Slave Error\n", DEVICE_NAME);
            show_descriptors(pdev);
            return (CRIT_ERR);
        }

        if (status & SG_INT_ERR_MASK) {
            printk(KERN_ERR
            "%s: Scatter Gather Operation: Internal Error\n", DEVICE_NAME);
            show_descriptors(pdev);
            return (CRIT_ERR);
        }

        if (status & SG_COMPLETE_MASK) {
            dev_dbg(&pdev->dev->dev,
            "%s: Scatter Gather Operation: Completed successfully\n", DEVICE_NAME);
            return (SUCCESS);
        }
    }
    dev_dbg(&pdev->dev->dev,  "%s: gReadBuffer: %s\n", DEVICE_NAME, pdev->dma.gReadBuffer);
    dev_dbg(&pdev->dev->dev,  "%s: gWriteBuffer: %s\n", DEVICE_NAME, pdev->dma.gWriteBuffer);

    printk(KERN_ERR"%s: Scatter Gather Operation error: Timeout Error\n", DEVICE_NAME);
    show_descriptors(pdev);
    return (CRIT_ERR);
}

int sg_block(struct pci_device_fpga *pdev, int direction, void *data,
	     size_t count, u32 addr)
{
    size_t unsended = count;
    char *curData = data;
    u32 curAddr = addr;
    u32 btt = BUF_SIZE;

    mutex_lock(&sg_mutex);

#if HANDLE_ALIGNMENT_IN_SW
#define ALIGN_IN_BYTES_LOG2 4
#define MASK_OFFSET_ALIGN ((1 << (ALIGN_IN_BYTES_LOG2)) - 1)
    if (PCI_DMA_FROMDEVICE == direction) {
        u32 offset = addr & MASK_OFFSET_ALIGN;

        if (offset != 0) {
            unsended += offset;
            curAddr -= offset;

            btt = (unsended < BUF_SIZE) ? unsended : BUF_SIZE;
            dev_dbg(&pdev->dev->dev, "%s: SG Block: BTT=%u\tunsended=%lu \n", DEVICE_NAME, btt, unsended);

            if (SUCCESS != sg_operation(pdev, direction, btt, curAddr)) {
                mutex_unlock(&sg_mutex);
                return (CRIT_ERR);
            }

	    memcpy(curData, pdev->dma.gReadBuffer + offset, btt - offset);

            curData += BUF_SIZE - offset;
            curAddr += BUF_SIZE;
            unsended -= btt;
        }

        // divide block
        while (unsended) {
            btt = (unsended < BUF_SIZE) ? unsended : BUF_SIZE;
            dev_dbg(&pdev->dev->dev, "%s: SG Block: BTT=%u\tunsended=%lu \n", DEVICE_NAME, btt, unsended);

            if (SUCCESS != sg_operation(pdev, direction, btt, curAddr)) {
                mutex_unlock(&sg_mutex);
                return (CRIT_ERR);
            }

	    memcpy(curData, pdev->dma.gReadBuffer, btt);

            curData += BUF_SIZE;
            curAddr += BUF_SIZE;
            unsended -= btt;
        }
    } else if (PCI_DMA_TODEVICE == direction) {
        u32 offset = addr & MASK_OFFSET_ALIGN;

        if (offset != 0) {
            unsended += offset;
            curAddr -= offset;

	    // TODO ALEX weird that we pass FROMDEVICE, whereas above, we use
	    // direction
            if (SUCCESS != sg_operation(pdev, PCI_DMA_FROMDEVICE, offset, curAddr)) {
                mutex_unlock(&sg_mutex);
                return (CRIT_ERR);
            }

            memcpy(pdev->dma.gWriteBuffer, pdev->dma.gReadBuffer, offset);

            btt = (unsended < BUF_SIZE) ? unsended : BUF_SIZE;
            dev_dbg(&pdev->dev->dev, "%s: SG Block: BTT=%u\tunsended=%lu \n", DEVICE_NAME, btt, unsended);

	    memcpy(pdev->dma.gWriteBuffer + offset, curData, btt - offset);

            if (SUCCESS != sg_operation(pdev, direction, btt, curAddr)) {
                mutex_unlock(&sg_mutex);
                return (CRIT_ERR);
            }
            curData += BUF_SIZE - offset;
            curAddr += BUF_SIZE;
            unsended -= btt;
        }

        // divide block
        while (unsended) {
            btt = (unsended < BUF_SIZE) ? unsended : BUF_SIZE;
            dev_dbg(&pdev->dev->dev, "%s: SG Block: BTT=%u\tunsended=%lu \n", DEVICE_NAME, btt, unsended);

	    memcpy(pdev->dma.gWriteBuffer, curData, btt);

            if (SUCCESS != sg_operation(pdev, direction, btt, curAddr)) {
                mutex_unlock(&sg_mutex);
                return (CRIT_ERR);
            }
            curData += BUF_SIZE;
            curAddr += BUF_SIZE;
            unsended -= btt;
        }
    } else {
        printk(KERN_ERR"%s: SG Block: unknown direction\n", DEVICE_NAME);
        mutex_unlock(&sg_mutex);
        return (CRIT_ERR);
    }
#else
    // divide block
    while (unsended) {
        btt = (unsended < BUF_SIZE) ? unsended : BUF_SIZE;
        dev_dbg(&pdev->dev->dev, "%s: SG Block: BTT=%u\tunsended=%lu \n", DEVICE_NAME, btt, unsended);

        // TODO: remove this multiple checks
        if (PCI_DMA_TODEVICE == direction)
		memcpy(pdev->dma.gWriteBuffer, curData, btt);

        if (SUCCESS != sg_operation(pdev, direction, btt, curAddr)) {
            mutex_unlock(&sg_mutex);
            return (CRIT_ERR);
        }

        // TODO: remove this multiple checks
        if (PCI_DMA_FROMDEVICE == direction)
            	memcpy(curData, pdev->dma.gReadBuffer, btt);

        curData += BUF_SIZE;
        curAddr += BUF_SIZE;
        unsended -= btt;
    }
#endif
    mutex_unlock(&sg_mutex);
    return (SUCCESS);
}

int xpdma_init(struct pci_device_fpga *pdev)
{
	int ret;

	pdev->dma.gBaseHdwr = pci_resource_start(pdev->dev, 0);
	dev_dbg(&pdev->dev->dev,  "%s: Init: Base hw val %X\n", DEVICE_NAME,
	       (unsigned int) pdev->dma.gBaseHdwr);

	// Get the Base Address Length
	pdev->dma.gBaseLen = pci_resource_len(pdev->dev, 0);
	dev_dbg(&pdev->dev->dev, "%s: Init: Base hw len %d\n", DEVICE_NAME,
	       (unsigned int)pdev->dma.gBaseLen);

	// Get Virtual HW address
	pdev->dma.gBaseVirt = pdev->banks[0].addr;
	if (!pdev->dma.gBaseVirt) {
		printk(KERN_ERR"%s: Init: Could not remap memory.\n",
		       DEVICE_NAME);
		return (CRIT_ERR);
	}
	dev_dbg(&pdev->dev->dev, "%s: Init: Virt HW address %lX\n", DEVICE_NAME,
	       (size_t)pdev->dma.gBaseVirt);

	// TODO ALEX quid ??
	gStatFlags = gStatFlags | HAVE_MEM_REGION;
	dev_dbg(&pdev->dev->dev, "%s: Init: Initialize Hardware Done..\n",
	       DEVICE_NAME);

	pdev->dma.gReadBuffer = dma_alloc_coherent(&pdev->dev->dev, BUF_SIZE,
						   &pdev->dma.gReadHWAddr,
						   GFP_KERNEL);
	if (NULL == pdev->dma.gReadBuffer) {
		printk(KERN_ERR"%s: Init: Unable to allocate gReadBuffer\n",
		       DEVICE_NAME);
		return (CRIT_ERR);
	}
	dev_info(&pdev->dev->dev, "%s: Init: Read buffer allocated: 0x%016lX, Phy:0x%016lX\n",
	       DEVICE_NAME, (size_t)pdev->dma.gReadBuffer,
	       (size_t)pdev->dma.gReadHWAddr);

	pdev->dma.gWriteBuffer = dma_alloc_coherent(&pdev->dev->dev, BUF_SIZE,
						    &pdev->dma.gWriteHWAddr,
						    GFP_KERNEL);
	if (NULL == pdev->dma.gWriteBuffer) {
		printk(KERN_ERR"%s: Init: Unable to allocate gWriteBuffer\n",
		       DEVICE_NAME);
		ret = CRIT_ERR;
		goto free_read_buffer;
	}
	dev_info(&pdev->dev->dev, "%s: Init: Write buffer allocated: 0x%016lX, Phy:0x%016lX\n",
	       DEVICE_NAME, (size_t)pdev->dma.gWriteBuffer,
	       (size_t)pdev->dma.gWriteHWAddr);

	pdev->dma.gDescChain = dma_alloc_coherent(&pdev->dev->dev, BUF_SIZE,
						  &pdev->dma.gDescChainHWAddr,
						  GFP_KERNEL);
	if (NULL == pdev->dma.gDescChain) {
		printk(KERN_ERR"%s: Init: Unable to allocate gDescChain\n",
		       DEVICE_NAME);
		ret = CRIT_ERR;
		goto free_write_buffer;
	}
	dev_info(&pdev->dev->dev, "%s: Init: Descriptor chain buffer allocated: 0x%016lX, Phy:0x%016lX\n",
	       DEVICE_NAME, (size_t)pdev->dma.gDescChain,
	       (size_t)pdev->dma.gDescChainHWAddr);

	gStatFlags = gStatFlags | HAVE_KERNEL_REG;
	dev_dbg(&pdev->dev->dev, "%s: driver is loaded\n", DEVICE_NAME);

	// try to reset CDMA
	if (xpdma_reset(pdev)) {
		printk(KERN_ERR"%s: RESET timeout\n", DEVICE_NAME);
		ret = CRIT_ERR;
		goto free_desc_chain;
	}

	return (SUCCESS);

free_desc_chain:
	dev_info(&pdev->dev->dev, "%s: Init: erase gDescChain\n", DEVICE_NAME);
	dma_free_coherent( &pdev->dev->dev, BUF_SIZE, pdev->dma.gDescChain, pdev->dma.gDescChainHWAddr);
free_write_buffer:
	dev_info(&pdev->dev->dev, "%s: Init: erase gWriteBuffer\n", DEVICE_NAME);
	dma_free_coherent( &pdev->dev->dev, BUF_SIZE, pdev->dma.gWriteBuffer, pdev->dma.gWriteHWAddr);
free_read_buffer:
	dev_info(&pdev->dev->dev, "%s: Init: erase gReadBuffer\n", DEVICE_NAME);
	dma_free_coherent( &pdev->dev->dev, BUF_SIZE, pdev->dma.gReadBuffer, pdev->dma.gReadHWAddr);
	return ret;
}

void xpdma_exit(struct pci_device_fpga *pdev)
{
	dev_info(&pdev->dev->dev, "%s: xpdma_exit: erase gReadBuffer\n", DEVICE_NAME);
	// Free Write, Read and Descriptor buffers allocated to use
	if (NULL != pdev->dma.gReadBuffer)
		dma_free_coherent( &pdev->dev->dev, BUF_SIZE, pdev->dma.gReadBuffer, pdev->dma.gReadHWAddr);

	dev_info(&pdev->dev->dev, "%s: xpdma_exit: erase gWriteBuffer\n", DEVICE_NAME);
	if (NULL != pdev->dma.gWriteBuffer)
		dma_free_coherent( &pdev->dev->dev, BUF_SIZE, pdev->dma.gWriteBuffer, pdev->dma.gWriteHWAddr);

	dev_info(&pdev->dev->dev, "%s: xpdma_exit: erase gDescChain\n", DEVICE_NAME);
	if (NULL != pdev->dma.gDescChain)
		dma_free_coherent( &pdev->dev->dev, BUF_SIZE, pdev->dma.gDescChain, pdev->dma.gDescChainHWAddr);

	pdev->dma.gReadBuffer = NULL;
	pdev->dma.gWriteBuffer = NULL;
	pdev->dma.gDescChain = NULL;

	pdev->dma.gBaseVirt = NULL;

	gStatFlags = 0;
	dev_dbg(&pdev->dev->dev, "%s: DMA is unloaded\n", DEVICE_NAME);
}
