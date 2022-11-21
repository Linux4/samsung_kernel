/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hac/s6e3hac_canvas2_panel_poc.h
 *
 * Header file for POC Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAC_CANVAS2_PANEL_POC_H__
#define __S6E3HAC_CANVAS2_PANEL_POC_H__

#include "../panel.h"
#include "../panel_poc.h"

#ifdef CONFIG_SUPPORT_GM2_FLASH
#define CANVAS2_POC_GM2_VBIAS_ADDR		(0xF0000)
#define CANVAS2_POC_GM2_VBIAS1_DATA_ADDR	(0xF0000)
#define CANVAS2_POC_GM2_VBIAS1_DATA_SIZE 	(S6E3HAC_GM2_FLASH_VBIAS1_LEN)
#define CANVAS2_POC_GM2_VBIAS_CHECKSUM_ADDR	(CANVAS2_POC_GM2_VBIAS1_DATA_ADDR + S6E3HAC_GM2_FLASH_VBIAS_CHECKSUM_OFS)
#define CANVAS2_POC_GM2_VBIAS_CHECKSUM_SIZE (S6E3HAC_GM2_FLASH_VBIAS_CHECKSUM_LEN)
#define CANVAS2_POC_GM2_VBIAS_MAGICNUM_ADDR	(CANVAS2_POC_GM2_VBIAS1_DATA_ADDR + S6E3HAC_GM2_FLASH_VBIAS_MAGICNUM_OFS)
#define CANVAS2_POC_GM2_VBIAS_MAGICNUM_SIZE (S6E3HAC_GM2_FLASH_VBIAS_MAGICNUM_LEN)
#define CANVAS2_POC_GM2_VBIAS_TOTAL_SIZE (S6E3HAC_GM2_FLASH_VBIAS_TOTAL_SIZE)
#define CANVAS2_POC_GM2_VBIAS2_DATA_ADDR	(0xF0057)
#define CANVAS2_POC_GM2_VBIAS2_DATA_SIZE 	(2)
#endif

/* partition consists of DATA, CHECKSUM and MAGICNUM */
static struct poc_partition canvas2_poc_partition[] = {
#ifdef CONFIG_SUPPORT_GM2_FLASH
	[POC_GM2_VBIAS_PARTITION] = {
		.name = "gamma_mode2_vbias",
		.addr = CANVAS2_POC_GM2_VBIAS_ADDR,
		.size = CANVAS2_POC_GM2_VBIAS_TOTAL_SIZE,
		.data = {
			{
				.data_addr = CANVAS2_POC_GM2_VBIAS1_DATA_ADDR,
				.data_size = CANVAS2_POC_GM2_VBIAS1_DATA_SIZE,
			},
			{
				.data_addr = CANVAS2_POC_GM2_VBIAS2_DATA_ADDR,
				.data_size = CANVAS2_POC_GM2_VBIAS2_DATA_SIZE,
			},
		},
		.checksum_addr = CANVAS2_POC_GM2_VBIAS_CHECKSUM_ADDR,
		.checksum_size = CANVAS2_POC_GM2_VBIAS_CHECKSUM_SIZE,
		.magicnum_addr = CANVAS2_POC_GM2_VBIAS_MAGICNUM_ADDR,
		.magicnum_size = CANVAS2_POC_GM2_VBIAS_MAGICNUM_SIZE,
		.magicnum = 0x01,
		.need_preload = true,
	},
#endif
};

static struct panel_poc_data s6e3hac_canvas2_poc_data = {
	.version = 3,
	.partition = canvas2_poc_partition,
	.nr_partition = ARRAY_SIZE(canvas2_poc_partition),
#ifdef CONFIG_SUPPORT_POC_SPI
	.conn_src = POC_CONN_SRC_SPI,
#endif
};
#endif /* __S6E3HAC_CANVAS2_PANEL_POC_H__ */
