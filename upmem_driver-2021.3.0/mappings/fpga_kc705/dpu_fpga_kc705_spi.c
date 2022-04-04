/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 UPMEM. All rights reserved. */
#include <linux/sched.h>
#include <linux/version.h>
#include <asm/io.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
#include <linux/sched/signal.h>
#endif

#include <dpu_fpga_kc705_spi.h>

#define WAIT_TX(base_addr)                                                     \
	do {                                                                   \
		if (dpu_spi_wait_for_tx_completion(base_addr))                 \
			return -EBUSY;                                         \
	} while (0);

#define WAIT_RX(base_addr)                                                     \
	do {                                                                   \
		if (dpu_spi_wait_for_rx_completion(base_addr))                 \
			return -EBUSY;                                         \
	} while (0);

void dpu_spi_reset(void *base_addr)
{
	uint32_t cfg_int_global_en = 0; //DGIER - Disable the interrupt output
	uint32_t cfg_int_control_en = 0;
	uint32_t cfg_spi_control = 0;
	uint32_t cfg_spi_slave_select = 0;

	pr_debug("Resetting spi...\n");

	//1- Reset the IP to start from a known state
	writew(0xA, base_addr + DPU_SPI_OFFSET + BAR_SWRESET);

	//2- configure the DGIER and IPIER
	cfg_int_control_en |= 1 << 4; //Data Rx Full
	cfg_int_control_en |= 1 << 2; //Data Tx empty

	writew(cfg_int_global_en,
	       base_addr + DPU_SPI_OFFSET + BAR_INT_GLOBAL_EN);
	writew(cfg_int_control_en,
	       base_addr + DPU_SPI_OFFSET + BAR_INT_CONTROL_EN);

	//3- configure the SPICR
	cfg_spi_control |= 1 << 9; //LSB First
	cfg_spi_control |= 0 << 8; //Master transaction enabled
	cfg_spi_control |= 0 << 7; //Manual Slave select assertion
	cfg_spi_control |= 0 << 6; //Clear Rx FIFO Content
	cfg_spi_control |= 0 << 5; //Clear Tx FIFO Content
	cfg_spi_control |=
		0
		<< 4; //Clock Phase - Data is sampled on the rising edge and shifted on falling edge
	cfg_spi_control |= 0 << 3; //Clock Polarity Low (sclk idles low)
	cfg_spi_control |= 1 << 2; //SPI Master Mode
	cfg_spi_control |= 1 << 1; //SPI System Enable
	//cfg_spi_control |= 1 << 0; //SPI loopback

	writew(cfg_spi_control, base_addr + DPU_SPI_OFFSET + BAR_SPI_CONTROL);

	//4- Not applied for Master
	//5- configure the SPISSR
	writew(cfg_spi_slave_select,
	       base_addr + DPU_SPI_OFFSET + BAR_SPI_SLAVE_SELECT);
}

static uint32_t dpu_spi_wait_for_tx_completion(void *base_addr)
{
	uint8_t spi_status = 0;
	uint8_t counter = 0;

	do {
		spi_status = readb(base_addr + DPU_SPI_OFFSET + BAR_SPI_STATUS);
		pr_debug("spi waiting tx: %d\n", spi_status);
		if (!(counter++ % 1000))
			schedule();
		if (fatal_signal_pending(current))
			return 1;
	} while (!(spi_status & 0x4));

	return 0;
}

static uint32_t dpu_spi_wait_for_rx_completion(void *base_addr)
{
	uint8_t spi_status = 0;
	uint8_t counter = 0;

	do {
		spi_status = readb(base_addr + DPU_SPI_OFFSET + BAR_SPI_STATUS);
		pr_debug("spi waiting rx: %d\n", spi_status);
		if (!(counter++ % 1000))
			schedule();
		if (fatal_signal_pending(current))
			return 1;
	} while (spi_status & 0x1);

	return 0;
}

uint8_t dpu_spi_write_read_byte(void *base_addr, uint8_t tx_data)
{
	writeb(tx_data, base_addr + DPU_SPI_OFFSET + BAR_SPI_TX_DATA);
	WAIT_TX(base_addr);
	WAIT_RX(base_addr);
	return readb(base_addr + DPU_SPI_OFFSET + BAR_SPI_RX_DATA);
}

int dpu_spi_write(void *base_addr, void *buf, uint8_t len)
{
	uint8_t cur_byte;
	uint32_t byte;

	pr_debug("spi sending cmd: 0x%x\n", SPI_BCKP_WR_ACCESS);

	dpu_spi_write_read_byte(base_addr, SPI_BCKP_WR_ACCESS);

	for (cur_byte = 0; cur_byte < len; ++cur_byte) {
		byte = ((uint8_t *)buf)[cur_byte];
		pr_debug("spi sending %d: 0x%x\n", cur_byte, byte);

		dpu_spi_write_read_byte(base_addr, byte);
	}

	return len;
}

int dpu_spi_read(void *base_addr, void *buf, uint8_t len)
{
	uint8_t cur_byte;
	uint32_t byte;

	pr_debug("spi sending cmd: 0x%x\n", SPI_BCKP_RD_ACCESS);

	dpu_spi_write_read_byte(base_addr, SPI_BCKP_RD_ACCESS);

	for (cur_byte = 0; cur_byte < len; ++cur_byte) {
		byte = dpu_spi_write_read_byte(base_addr, 0);

		((uint8_t *)buf)[cur_byte] = (uint8_t)byte;
		pr_debug("spi reading %d: 0x%x\n", cur_byte, byte);
	}

	return len;
}
