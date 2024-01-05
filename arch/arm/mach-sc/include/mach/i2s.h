/*
 * sound/soc/sprd/dai/i2s/i2s.h
 *
 * SPRD SoC CPU-DAI -- SpreadTrum SOC DAI i2s.
 *
 * Copyright (C) 2012 SpreadTrum Ltd.
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
#ifndef __I2S_H
#define __I2S_H

#include <mach/sprd-audio.h>
#define I2S_VERSION	"i2s.r0p0"

#define I2S_MAGIC_ID		(0x124)

/* bus_type */
#define I2S_BUS 0
#define PCM_BUS 1

/* byte_per_chan */
#define I2S_BPCH_8 0
#define I2S_BPCH_16 1
#define I2S_BPCH_32 2

/* mode */
#define I2S_MASTER 0
#define I2S_SLAVE 1

/* lsb */
#define I2S_MSB 0
#define I2S_LSB 1

/* rtx_mode */
#define I2S_RTX_DIS 0
#define I2S_RX_MODE 1
#define I2S_TX_MODE 2
#define I2S_RTX_MODE 3

/* sync_mode */
#define I2S_LRCK 0
#define I2S_SYNC 1

/* lrck_inv */
#define I2S_L_LEFT 0
#define I2S_L_RIGTH 1

/* clk_inv */
#define I2S_CLK_N 0
#define I2S_CLK_R 1

/* i2s_bus_mode */
#define I2S_MSBJUSTFIED 0
#define I2S_COMPATIBLE 1

/* pcm_bus_mode */
#define I2S_LONG_FRAME 0
#define I2S_SHORT_FRAME 1

struct i2s_config {
	u32 hw_port;
	u32 fs;
	u32 slave_timeout;
	u32 bus_type:1;
	u32 byte_per_chan:2;
	u32 mode:1;
	u32 lsb:1;
	u32 rtx_mode:2;
	u32 sync_mode:1;
	u32 lrck_inv:1;
	u32 clk_inv:2;
	u32 i2s_bus_mode:1;
	u32 pcm_bus_mode:1;
	u32 pcm_slot:3;
	u16 pcm_cycle;
	u16 tx_watermark;
	u16 rx_watermark;
};

/* -------------------------- */
#define I2S_FIFO_DEPTH 32

#endif /* __I2S_H */
