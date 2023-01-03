/*
 * linux/drivers/video/fbdev/exynos/panel/hx83102/hx83102.h
 *
 * Header file for S6E3HAB Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __HX83102_H__
#define __HX83102_H__

#include <linux/types.h>
#include <linux/kernel.h>

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => HX83102_XXX_OFS (0)
 * XXX 2nd param => HX83102_XXX_OFS (1)
 * XXX 36th param => HX83102_XXX_OFS (35)
 */

#define HX83102_ID_REG				0x04 /* no use */
#define HX83102_ID_OFS				0
#define HX83102_ID_LEN				(PANEL_ID_LEN)

#define HX83102_ID_DA_REG				0xDA
#define HX83102_ID_DA_OFS				0
#define HX83102_ID_DA_LEN				1

#define HX83102_ID_DB_REG				0xDB
#define HX83102_ID_DB_OFS				0
#define HX83102_ID_DB_LEN				1

#define HX83102_ID_DC_REG				0xDC
#define HX83102_ID_DC_OFS				0
#define HX83102_ID_DC_LEN				1

#define HX83102_NR_LUMINANCE		(256)
#define HX83102_NR_HBM_LUMINANCE	(51)

#define HX83102_TOTAL_NR_LUMINANCE (HX83102_NR_LUMINANCE + HX83102_NR_HBM_LUMINANCE)

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

static u8 HX83102_ID[HX83102_ID_LEN];

static struct rdinfo hx83102_rditbl[] = {
	[READ_ID_DA] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, HX83102_ID_DA_REG, HX83102_ID_DA_OFS, HX83102_ID_DA_LEN),
	[READ_ID_DB] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, HX83102_ID_DB_REG, HX83102_ID_DB_OFS, HX83102_ID_DB_LEN),
	[READ_ID_DC] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, HX83102_ID_DC_REG, HX83102_ID_DC_OFS, HX83102_ID_DC_LEN),

};

static DECLARE_RESUI(id) = {
	{.rditbl = &hx83102_rditbl[READ_ID_DA], .offset = 0},
	{.rditbl = &hx83102_rditbl[READ_ID_DB], .offset = 1},
	{.rditbl = &hx83102_rditbl[READ_ID_DC], .offset = 2},
};

static struct resinfo hx83102_restbl[] = {
	[RES_ID] = RESINFO_INIT(id, HX83102_ID, RESUI(id)),
};

static int init_brightness_table(struct maptbl *tbl);
static int getidx_brt_table(struct maptbl *);
static void copy_common_maptbl(struct maptbl *, u8 *);

#endif /* __HX83102_H__ */
