/*
 * linux/drivers/video/fbdev/exynos/panel/dummy_lcd/dummy_lcd.h
 *
 * Header file for S6E3HAB Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DUMMY_H__
#define __DUMMY_H__

#include <linux/types.h>
#include <linux/kernel.h>

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => DUMMY_XXX_OFS (0)
 * XXX 2nd param => DUMMY_XXX_OFS (1)
 * XXX 36th param => DUMMY_XXX_OFS (35)
 */

#define DUMMY_ID_REG				0x04 /* no use */
#define DUMMY_ID_OFS				0
#define DUMMY_ID_LEN				(PANEL_ID_LEN)

#define DUMMY_ID_DA_REG				0xDA
#define DUMMY_ID_DA_OFS				0
#define DUMMY_ID_DA_LEN				1

#define DUMMY_ID_DB_REG				0xDB
#define DUMMY_ID_DB_OFS				0
#define DUMMY_ID_DB_LEN				1

#define DUMMY_ID_DC_REG				0xDC
#define DUMMY_ID_DC_OFS				0
#define DUMMY_ID_DC_LEN				1

#define DUMMY_NR_LUMINANCE		(256)
#define DUMMY_NR_HBM_LUMINANCE	(51)

#define DUMMY_TOTAL_NR_LUMINANCE (DUMMY_NR_LUMINANCE + DUMMY_NR_HBM_LUMINANCE)

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

static u8 DUMMY_ID[DUMMY_ID_LEN];

static struct rdinfo dummy_lcd_rditbl[] = {
	[READ_ID_DA] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, DUMMY_ID_DA_REG, DUMMY_ID_DA_OFS, DUMMY_ID_DA_LEN),
	[READ_ID_DB] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, DUMMY_ID_DB_REG, DUMMY_ID_DB_OFS, DUMMY_ID_DB_LEN),
	[READ_ID_DC] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, DUMMY_ID_DC_REG, DUMMY_ID_DC_OFS, DUMMY_ID_DC_LEN),

};

static DECLARE_RESUI(id) = {
	{.rditbl = &dummy_lcd_rditbl[READ_ID_DA], .offset = 0},
	{.rditbl = &dummy_lcd_rditbl[READ_ID_DB], .offset = 1},
	{.rditbl = &dummy_lcd_rditbl[READ_ID_DC], .offset = 2},
};

static struct resinfo dummy_lcd_restbl[] = {
	[RES_ID] = RESINFO_INIT(id, DUMMY_ID, RESUI(id)),
};

static int init_brightness_table(struct maptbl *tbl);
static int getidx_brt_table(struct maptbl *);
static void copy_common_maptbl(struct maptbl *, u8 *);

#endif /* __DUMMY_H__ */
