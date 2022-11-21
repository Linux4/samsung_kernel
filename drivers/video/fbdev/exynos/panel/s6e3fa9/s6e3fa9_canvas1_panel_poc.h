/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fa9/s6e3fa9_canvas1_panel_poc.h
 *
 * Header file for POC Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FA9_CANVAS1_PANEL_POC_H__
#define __S6E3FA9_CANVAS1_PANEL_POC_H__

#include "../panel.h"
#include "../panel_poc.h"

#define CANVAS1_POC_IMG_ADDR	(0)
#define CANVAS1_POC_IMG_SIZE	(390528)

/* partition consists of DATA, CHECKSUM and MAGICNUM */
static struct poc_partition canvas1_poc_partition[] = {
	[POC_IMG_PARTITION] = {
		.name = "image",
		.addr = CANVAS1_POC_IMG_ADDR,
		.size = CANVAS1_POC_IMG_SIZE,
		.need_preload = false
	},
};

static struct panel_poc_data s6e3fa9_canvas1_poc_data = {
	.version = 4,
	.partition = canvas1_poc_partition,
	.nr_partition = ARRAY_SIZE(canvas1_poc_partition),
#ifdef CONFIG_SUPPORT_POC_SPI
	.conn_src = POC_CONN_SRC_SPI,
#endif
};
#endif /* __S6E3FA9_CANVAS1_PANEL_POC_H__ */
