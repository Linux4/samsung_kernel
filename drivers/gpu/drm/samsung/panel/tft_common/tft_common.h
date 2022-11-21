/*
 * linux/drivers/video/fbdev/exynos/panel/tft_common/tft_common.h
 *
 * Header file for TFT_COMMON Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __TFT_COMMON_H__
#define __TFT_COMMON_H__

#include <linux/types.h>
#include <linux/kernel.h>
#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "../panel_poc.h"
#endif

#if IS_ENABLED(CONFIG_TFT_COMMON_TEST)
static inline void register_common_panel_list(void) { return; }
static inline void unregister_common_panel_list(void) { return; }
#endif

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => TFT_COMMON_XXX_OFS (0)
 * XXX 2nd param => TFT_COMMON_XXX_OFS (1)
 * XXX 36th param => TFT_COMMON_XXX_OFS (35)
 */

#define TFT_COMMON_ADDR_OFS	(0)
#define TFT_COMMON_ADDR_LEN	(1)
#define TFT_COMMON_DATA_OFS	(TFT_COMMON_ADDR_OFS + TFT_COMMON_ADDR_LEN)

#define TFT_COMMON_ID_LEN				(PANEL_ID_LEN)

#define TFT_COMMON_ID_DA_REG				0xDA
#define TFT_COMMON_ID_DA_OFS				0
#define TFT_COMMON_ID_DA_LEN				1

#define TFT_COMMON_ID_DB_REG				0xDB
#define TFT_COMMON_ID_DB_OFS				0
#define TFT_COMMON_ID_DB_LEN				1

#define TFT_COMMON_ID_DC_REG				0xDC
#define TFT_COMMON_ID_DC_OFS				0
#define TFT_COMMON_ID_DC_LEN				1

enum {
	BRT_MAPTBL,
	MAX_MAPTBL,
};

enum {
	READ_ID,
	READ_ID_DA,
	READ_ID_DB,
	READ_ID_DC,
};

enum {
	RES_ID,
};

static u8 TFT_COMMON_ID[TFT_COMMON_ID_LEN];

static struct rdinfo tft_common_rditbl[] = {
	[READ_ID_DA] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DA_OFS, TFT_COMMON_ID_DA_LEN),
	[READ_ID_DB] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DB_REG, TFT_COMMON_ID_DB_OFS, TFT_COMMON_ID_DB_LEN),
	[READ_ID_DC] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DC_REG, TFT_COMMON_ID_DC_OFS, TFT_COMMON_ID_DC_LEN),

};

static DECLARE_RESUI(id) = {
	{.rditbl = &tft_common_rditbl[READ_ID_DA], .offset = 0},
	{.rditbl = &tft_common_rditbl[READ_ID_DB], .offset = 1},
	{.rditbl = &tft_common_rditbl[READ_ID_DC], .offset = 2},
};

static struct resinfo tft_common_restbl[] = {
	[RES_ID] = RESINFO_INIT(id, TFT_COMMON_ID, RESUI(id)),
};

static int init_brt_table(struct maptbl *tbl);
static int getidx_brt_table(struct maptbl *);
static void copy_common_maptbl(struct maptbl *, u8 *);

#endif /* __TFT_COMMON_H__ */
