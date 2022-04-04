/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 UPMEM. All rights reserved. */
#ifndef DPU_FPGA_KC705_SPI_H
#define DPU_FPGA_KC705_SPI_H

#define DPU_SPI_BANK_NUMBER 1
#define DPU_SPI_OFFSET 0x40002000

/* SPI Register Space, at bar2 + 0x40002000 */
#define BAR_SWRESET 0x40 //SRR
#define BAR_SPI_CONTROL 0x60 //SPICR
#define BAR_SPI_STATUS 0x64 //SPISR
#define BAR_SPI_TX_DATA 0x68 //SPIDTR
#define BAR_SPI_RX_DATA 0x6C //SPIDRR
#define BAR_SPI_SLAVE_SELECT 0x70 //SPISSR
#define BAR_SPI_TX_FIFO_UW 0x74 //SPITXFIFO Used Word
#define BAR_SPU_RX_FIFO_UW 0x78 //SPIRXFIFO Used Word
#define BAR_INT_GLOBAL_EN 0x1C //DGIER
#define BAR_INT_STATUS 0x20 //IPISR
#define BAR_INT_CONTROL_EN 0x28 //IPIER

#define SPI_BCKP_WR_ACCESS 0xC0
#define SPI_BCKP_RD_ACCESS 0x80

extern uint8_t spi_mode_enabled;

void dpu_spi_reset(void *base_addr);
uint8_t dpu_spi_write_read_byte(void *base_addr, uint8_t tx_data);
int dpu_spi_read(void *base_addr, void *buf, uint8_t len);
int dpu_spi_write(void *base_addr, void *buf, uint8_t len);

#endif /* DPU_FPGA_KC705_SPI_H */
