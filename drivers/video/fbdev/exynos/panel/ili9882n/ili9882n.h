/*
 * linux/drivers/video/fbdev/exynos/panel/ili9882n/ili9882n.h
 *
 * Header file for S6E3HAB Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ILI9882N_H__
#define __ILI9882N_H__

#include <linux/types.h>
#include <linux/kernel.h>

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => ILI9882N_XXX_OFS (0)
 * XXX 2nd param => ILI9882N_XXX_OFS (1)
 * XXX 36th param => ILI9882N_XXX_OFS (35)
 */

#define ILI9882N_ID_REG				0x04 /* no use */
#define ILI9882N_ID_OFS				0
#define ILI9882N_ID_LEN				(PANEL_ID_LEN)

#define ILI9882N_ID_DA_REG				0xDA
#define ILI9882N_ID_DA_OFS				0
#define ILI9882N_ID_DA_LEN				1

#define ILI9882N_ID_DB_REG				0xDB
#define ILI9882N_ID_DB_OFS				0
#define ILI9882N_ID_DB_LEN				1

#define ILI9882N_ID_DC_REG				0xDC
#define ILI9882N_ID_DC_OFS				0
#define ILI9882N_ID_DC_LEN				1

#define ILI9882N_NR_LUMINANCE		(256)
#define ILI9882N_NR_HBM_LUMINANCE	(51)

#define ILI9882N_TOTAL_NR_LUMINANCE (ILI9882N_NR_LUMINANCE + ILI9882N_NR_HBM_LUMINANCE)

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

static u8 ILI9882N_ID[ILI9882N_ID_LEN];

static struct rdinfo ili9882n_rditbl[] = {
	[READ_ID_DA] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, ILI9882N_ID_DA_REG, ILI9882N_ID_DA_OFS, ILI9882N_ID_DA_LEN),
	[READ_ID_DB] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, ILI9882N_ID_DB_REG, ILI9882N_ID_DB_OFS, ILI9882N_ID_DB_LEN),
	[READ_ID_DC] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, ILI9882N_ID_DC_REG, ILI9882N_ID_DC_OFS, ILI9882N_ID_DC_LEN),

};

static DECLARE_RESUI(id) = {
	{.rditbl = &ili9882n_rditbl[READ_ID_DA], .offset = 0},
	{.rditbl = &ili9882n_rditbl[READ_ID_DB], .offset = 1},
	{.rditbl = &ili9882n_rditbl[READ_ID_DC], .offset = 2},
};

static struct resinfo ili9882n_restbl[] = {
	[RES_ID] = RESINFO_INIT(id, ILI9882N_ID, RESUI(id)),
};

static int init_brightness_table(struct maptbl *tbl);
static int getidx_brt_table(struct maptbl *);
static void copy_common_maptbl(struct maptbl *, u8 *);

#endif /* __ILI9882N_H__ */
