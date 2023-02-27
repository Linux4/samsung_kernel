/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __SPRD_SPI_R4P0_H__
#define __SPRD_SPI_R4P0_H__

#include <linux/bitops.h>

#define SPI_TXD			0x0000
#define SPI_CLKD		0x0004
#define SPI_CTL0		0x0008
#define SPI_CTL1		0x000c
#define SPI_CTL2		0x0010
#define SPI_CTL3		0x0014
#define SPI_CTL4		0x0018
#define SPI_CTL5		0x001c
#define SPI_INT_EN		0x0020
#define SPI_INT_CLR		0x0024
#define SPI_INT_RAW_STS		0x0028
#define SPI_INT_MASK_STS	0x002c
#define SPI_STS1		0x0030
#define SPI_STS2		0x0034
#define SPI_DSP_WAIT		0x0038
#define SPI_STS3		0x003c
#define SPI_CTL6		0x0040
#define SPI_STS4		0x0044
#define SPI_FIFO_RST		0x0048
#define SPI_CTL7		0x004c
#define SPI_STS5		0x0050
#define SPI_CTL8		0x0054
#define SPI_CTL9		0x0058
#define SPI_CTL10		0x005c
#define SPI_CTL11		0x0060
#define SPI_CTL12		0x0064
#define SPI_STS6		0x0068
#define SPI_STS7		0x006c
#define SPI_STS8		0x0070
#define SPI_STS9		0x0074

/* Bit define for register STS2 */
#define SPI_RX_FIFO_FULL            BIT(0)
#define SPI_RX_FIFO_EMPTY           BIT(1)
#define SPI_TX_FIFO_FULL            BIT(2)
#define SPI_TX_FIFO_EMPTY           BIT(3)
#define SPI_RX_FIFO_REALLY_FULL     BIT(4)
#define SPI_RX_FIFO_REALLY_EMPTY    BIT(5)
#define SPI_TX_FIFO_REALLY_FULL     BIT(6)
#define SPI_TX_FIFO_REALLY_EMPTY    BIT(7)
#define SPI_TX_BUSY                 BIT(8)
/* Bit define for register ctr1 */
#define SPI_RX_MODE                 BIT(12)
#define SPI_TX_MODE                 BIT(13)
/* Bit define for register ctr2 */
#define SPI_DMA_EN                  BIT(6)
/* Bit define for register ctr4 */
#define SPI_START_RX                BIT(9)

#define SPRD_SPI_DMA_MODE 1
#define SPRD_SPI_ONLY_RX_AND_TXRX_BUG_FIX 1
#define SPRD_SPI_ONLY_RX_AND_TXRX_BUG_FIX_IGNORE_ADDR ((void*)1)
#define SPRD_SPI_RX_WATERMARK_BUG_FIX 1
#define SPRD_SPI_RX_WATERMARK_MAX  0x1f

#define SPRD_SPI_FIFO_SIZE 32
#define SPRD_SPI_MAX_RECV_BLK	0x1ff
#define SPRD_SPI_CHIP_CS_NUM 0x4
#define MAX_BITS_PER_WORD 32

enum {
	sprd_spi_0 =0x0,
	sprd_spi_1,
	sprd_spi_2,
};

#define SPI_TIME_OUT 0xff0000

#endif
