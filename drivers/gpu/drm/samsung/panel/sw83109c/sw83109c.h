/*
 * linux/drivers/video/fbdev/exynos/panel/sw83109c/sw83109c.h
 *
 * Header file for SW83109C Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SW83109C_H__
#define __SW83109C_H__

#include <linux/types.h>
#include <linux/kernel.h>
#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "../panel_poc.h"
#endif
#include "../panel_drv.h"
#include "../panel.h"
#include "../maptbl.h"
#include "oled_common_dump.h"
#include "sw83109c_mdnie.h"

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => SW83109C_XXX_OFS (0)
 * XXX 2nd param => SW83109C_XXX_OFS (1)
 * XXX 36th param => SW83109C_XXX_OFS (35)
 */

#define SW83109C_ADDR_OFS	(0)
#define SW83109C_ADDR_LEN	(1)
#define SW83109C_DATA_OFS	(SW83109C_ADDR_OFS + SW83109C_ADDR_LEN)

#define SW83109C_DATE_REG			0xA9
#define SW83109C_DATE_OFS			0
#define SW83109C_DATE_LEN			(11)

#define SW83109C_COORDINATE_REG		0xA1
#define SW83109C_COORDINATE_OFS		0
#define SW83109C_COORDINATE_LEN		(PANEL_COORD_LEN)

#define SW83109C_ID_REG				0x04
#define SW83109C_ID_OFS				0
#define SW83109C_ID_LEN				(PANEL_ID_LEN)

#define SW83109C_CODE_REG			0xEA
#define SW83109C_CODE_OFS			0
#define SW83109C_CODE_LEN			10

#define SW83109C_OCTA_ID_0_REG			0xA1
#define SW83109C_OCTA_ID_0_OFS			11
#define SW83109C_OCTA_ID_0_LEN			4

#define SW83109C_OCTA_ID_1_REG			0x92
#define SW83109C_OCTA_ID_1_OFS			2
#define SW83109C_OCTA_ID_1_LEN			16

#define SW83109C_OCTA_ID_LEN			(SW83109C_OCTA_ID_0_LEN + SW83109C_OCTA_ID_1_LEN)

/* for panel dump */
#define SW83109C_RDDSM_REG			0x0E
#define SW83109C_RDDSM_OFS			0
#define SW83109C_RDDSM_LEN			(PANEL_RDDSM_LEN)

#define SW83109C_ERR_FG_REG			0x9F
#define SW83109C_ERR_FG_OFS			0
#define SW83109C_ERR_FG_LEN			2

#define SW83109C_DSI_ERR_REG			0x05
#define SW83109C_DSI_ERR_OFS			0
#define SW83109C_DSI_ERR_LEN			1

#define SW83109C_SELF_MASK_CHECKSUM_REG			0xE0
#define SW83109C_SELF_MASK_CHECKSUM_OFS			0
#define SW83109C_SELF_MASK_CHECKSUM_LEN			12

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
#define SW83109C_CMDLOG_REG			0x9C
#define SW83109C_CMDLOG_OFS			0
#define SW83109C_CMDLOG_LEN			0x80
#endif

#define SW83109C_TRIM_REG		(0xEB)
#define SW83109C_TRIM_OFS		(0)
#define SW83109C_TRIM_LEN		(9)

#define SW83109C_ERR_FLAG_REG		(0x9F)
#define SW83109C_ERR_FLAG_OFS		(0)
#define SW83109C_ERR_FLAG_LEN		(2)

enum {
	GAMMA_MODE2_MAPTBL,
	AOD_TO_GAMMA_MODE2_MAPTBL,
	TSET_MAPTBL,
	FMEM_MAPTBL,
	ACL_ONOFF_MAPTBL,
	ACL_OPR_MAPTBL,
	FPS_1_MAPTBL,
	FPS_2_MAPTBL,
	LPM_NIT_MAPTBL,
	LPM_MODE_MAPTBL,
	FFC_MAPTBL,
	MAX_MAPTBL,
};

enum {
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	READ_COPR_SPI,
	READ_COPR_DSI,
#endif
	READ_ID,
	READ_COORDINATE,
	READ_CODE,
	READ_DATE,
	READ_OCTA_ID_0,
	READ_OCTA_ID_1,
	READ_CHIP_ID,
	/* for brightness debugging */
	READ_RDDPM,
	READ_RDDSM,
	READ_ERR,
	READ_ERR_FG,
	READ_DSI_ERR,
	READ_SELF_DIAG,
	READ_SELF_MASK_CHECKSUM,
	READ_TRIM,
	READ_ERR_FLAG,
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	READ_CMDLOG,
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	READ_CCD_STATE,
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	READ_GRAYSPOT_CAL,
#endif
	MAX_READTBL,
};

enum {
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	RES_COPR_SPI,
	RES_COPR_DSI,
#endif
	RES_ID,
	RES_COORDINATE,
	RES_CODE,
	RES_DATE,
	RES_OCTA_ID,
	RES_CHIP_ID,
	/* for brightness debugging */
	RES_RDDPM,
	RES_RDDSM,
	RES_ERR,
	RES_ERR_FG,
	RES_DSI_ERR,
	RES_SELF_DIAG,
	RES_TRIM,
	RES_ERR_FLAG,
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	RES_CMDLOG,
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	RES_CCD_STATE,
	RES_CCD_CHKSUM_FAIL,
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	RES_GRAYSPOT_CAL,
#endif
	RES_SELF_MASK_CHECKSUM,
	MAX_RESTBL
};

enum {
	HLPM_ON_LOW,
	HLPM_ON,
	MAX_HLPM_ON
};


enum {
	SW83109C_VRR_FPS_120,
	SW83109C_VRR_FPS_90,
	SW83109C_VRR_FPS_60,
	MAX_SW83109C_VRR_FPS,
};


enum {
	SW83109C_SMOOTH_DIMMING_OFF,
	SW83109C_SMOOTH_DIMMING_ON,
	MAX_SW83109C_SMOOTH_DIMMING,
};

static u8 SW83109C_ID[SW83109C_ID_LEN];
static u8 SW83109C_COORDINATE[SW83109C_COORDINATE_LEN];
static u8 SW83109C_CODE[SW83109C_CODE_LEN];		// manufacture code
static u8 SW83109C_DATE[SW83109C_DATE_LEN];		// cell id
/* for brightness debugging */
static u8 SW83109C_RDDSM[SW83109C_RDDSM_LEN];
static u8 SW83109C_ERR_FG[SW83109C_ERR_FG_LEN];
static u8 SW83109C_DSI_ERR[SW83109C_DSI_ERR_LEN];
static u8 SW83109C_SELF_MASK_CHECKSUM[SW83109C_SELF_MASK_CHECKSUM_LEN];
static u8 SW83109C_TRIM[SW83109C_TRIM_LEN];
static u8 SW83109C_ERR_FLAG[SW83109C_ERR_FLAG_LEN];

static struct rdinfo sw83109c_rditbl[MAX_READTBL] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, SW83109C_ID_REG, SW83109C_ID_OFS, SW83109C_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, SW83109C_COORDINATE_REG, SW83109C_COORDINATE_OFS, SW83109C_COORDINATE_LEN),
	[READ_CODE] = RDINFO_INIT(code, DSI_PKT_TYPE_RD, SW83109C_CODE_REG, SW83109C_CODE_OFS, SW83109C_CODE_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, SW83109C_DATE_REG, SW83109C_DATE_OFS, SW83109C_DATE_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, SW83109C_RDDSM_REG, SW83109C_RDDSM_OFS, SW83109C_RDDSM_LEN),
	[READ_ERR_FG] = RDINFO_INIT(err_fg, DSI_PKT_TYPE_RD, SW83109C_ERR_FG_REG, SW83109C_ERR_FG_OFS, SW83109C_ERR_FG_LEN),
	[READ_DSI_ERR] = RDINFO_INIT(dsi_err, DSI_PKT_TYPE_RD, SW83109C_DSI_ERR_REG, SW83109C_DSI_ERR_OFS, SW83109C_DSI_ERR_LEN),
	[READ_SELF_MASK_CHECKSUM] = RDINFO_INIT(self_mask_checksum, DSI_PKT_TYPE_RD, SW83109C_SELF_MASK_CHECKSUM_REG, SW83109C_SELF_MASK_CHECKSUM_OFS, SW83109C_SELF_MASK_CHECKSUM_LEN),
	[READ_TRIM] = RDINFO_INIT(trim, DSI_PKT_TYPE_RD, SW83109C_TRIM_REG, SW83109C_TRIM_OFS, SW83109C_TRIM_LEN),
	[READ_ERR_FLAG] = RDINFO_INIT(err_flag, DSI_PKT_TYPE_RD, SW83109C_ERR_FLAG_REG, SW83109C_ERR_FLAG_OFS, SW83109C_ERR_FLAG_LEN),
};

static DEFINE_RESUI(id, &sw83109c_rditbl[READ_ID], 0);
static DEFINE_RESUI(coordinate, &sw83109c_rditbl[READ_COORDINATE], 0);
static DEFINE_RESUI(code, &sw83109c_rditbl[READ_CODE], 0);
static DEFINE_RESUI(date, &sw83109c_rditbl[READ_DATE], 0);

/* for brightness debugging */
static DEFINE_RESUI(rddsm, &sw83109c_rditbl[READ_RDDSM], 0);
static DEFINE_RESUI(err_fg, &sw83109c_rditbl[READ_ERR_FG], 0);
static DEFINE_RESUI(dsi_err, &sw83109c_rditbl[READ_DSI_ERR], 0);
static DEFINE_RESUI(self_mask_checksum, &sw83109c_rditbl[READ_SELF_MASK_CHECKSUM], 0);
static DEFINE_RESUI(trim, &sw83109c_rditbl[READ_TRIM], 0);
static DEFINE_RESUI(err_flag, &sw83109c_rditbl[READ_ERR_FLAG], 0);

static struct resinfo sw83109c_restbl[MAX_RESTBL] = {
	[RES_ID] = RESINFO_INIT(id, SW83109C_ID, RESUI(id)),
	[RES_COORDINATE] = RESINFO_INIT(coordinate, SW83109C_COORDINATE, RESUI(coordinate)),
	[RES_CODE] = RESINFO_INIT(code, SW83109C_CODE, RESUI(code)),
	[RES_DATE] = RESINFO_INIT(date, SW83109C_DATE, RESUI(date)),
	[RES_RDDSM] = RESINFO_INIT(rddsm, SW83109C_RDDSM, RESUI(rddsm)),
	[RES_ERR_FG] = RESINFO_INIT(err_fg, SW83109C_ERR_FG, RESUI(err_fg)),
	[RES_DSI_ERR] = RESINFO_INIT(dsi_err, SW83109C_DSI_ERR, RESUI(dsi_err)),
	[RES_SELF_MASK_CHECKSUM] = RESINFO_INIT(self_mask_checksum, SW83109C_SELF_MASK_CHECKSUM, RESUI(self_mask_checksum)),
	[RES_TRIM] = RESINFO_INIT(trim, SW83109C_TRIM, RESUI(trim)),
	[RES_ERR_FLAG] = RESINFO_INIT(err_flag, SW83109C_ERR_FLAG, RESUI(err_flag)),
};

enum {
	DUMP_RDDPM = 0,
	DUMP_RDDPM_SLEEP_IN,
	DUMP_RDDSM,
	DUMP_ERR,
	DUMP_ERR_FG,
	DUMP_DSI_ERR,
	DUMP_SELF_DIAG,
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	DUMP_CMDLOG,
#endif
};

static struct dump_expect rddsm_expects[] = {
	{ .offset = 0, .mask = 0x01, .value = 0x00, .msg = "DSI Error" },
};

static struct dump_expect err_fg_expects[] = {
	{ .offset = 0, .mask = 0x08, .value = 0x00, .msg = "Power Sequence Error" },
	{ .offset = 0, .mask = 0x04, .value = 0x00, .msg = "Input Level Error" },
	{ .offset = 0, .mask = 0x02, .value = 0x00, .msg = "VGH, VGL Error" },
	{ .offset = 0, .mask = 0x01, .value = 0x00, .msg = "UCS Checksum Error" },
	{ .offset = 1, .mask = 0x02, .value = 0x00, .msg = "Vsync Timeout" },
	{ .offset = 1, .mask = 0x01, .value = 0x00, .msg = "Hsync Timeout" },
};

static struct dump_expect dsie_cnt_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = 0x00, .msg = "DSI Error Count" },
};

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static struct dump_expect cmdlog_expects[] = {
};
#endif

static struct dumpinfo sw83109c_dmptbl[] = {
	[DUMP_RDDSM] = DUMPINFO_INIT_V2(rddsm, &sw83109c_restbl[RES_RDDSM], show_rddsm, rddsm_expects),
	[DUMP_ERR_FG] = DUMPINFO_INIT_V2(err_fg, &sw83109c_restbl[RES_ERR_FG], show_expects, err_fg_expects),
	[DUMP_DSI_ERR] = DUMPINFO_INIT_V2(dsi_err, &sw83109c_restbl[RES_DSI_ERR], show_dsi_err, dsie_cnt_expects),
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[DUMP_CMDLOG] = DUMPINFO_INIT_V2(cmdlog, &sw83109c_restbl[RES_CMDLOG], show_cmdlog, cmdlog_expects),
#endif
};

/* Variable Refresh Rate */
enum {
	SW83109C_VRR_MODE_NS,
	SW83109C_VRR_MODE_HS,
	MAX_SW83109C_VRR_MODE,
};

enum {
	SW83109C_VRR_120HS,
	SW83109C_VRR_60HS,
	SW83109C_VRR_30NS,
	MAX_SW83109C_VRR,
};

enum {
	SW83109C_RESOL_1080x2400,
};

enum {
	SW83109C_M54X_DISPLAY_MODE_1080x2400_120HS,
	SW83109C_M54X_DISPLAY_MODE_1080x2400_60HS,
	SW83109C_M54X_DISPLAY_MODE_1080x2400_30NS,
	MAX_SW83109C_M54X_DISPLAY_MODE,
};

enum {
	SW83109C_VRR_KEY_REFRESH_RATE,
	SW83109C_VRR_KEY_REFRESH_MODE,
	SW83109C_VRR_KEY_TE_SW_SKIP_COUNT,
	SW83109C_VRR_KEY_TE_HW_SKIP_COUNT,
	MAX_SW83109C_VRR_KEY,
};

enum {
	SW83109C_ACL_DIM_OFF,
	SW83109C_ACL_DIM_ON,
	MAX_SW83109C_ACL_DIM,
};

/* use according to adaptive_control sysfs */
enum {
	SW83109C_ACL_OPR_0,
	SW83109C_ACL_OPR_1,
	SW83109C_ACL_OPR_2,
	MAX_SW83109C_ACL_OPR
};

enum {
	SW83109C_HS_CLK_1443 = 0,
	SW83109C_HS_CLK_1462,
	SW83109C_HS_CLK_1471,
	MAX_SW83109C_HS_CLK
};
int init_gamma_mode2_brt_table(struct maptbl *tbl);
int getidx_gamma_mode2_brt_table(struct maptbl *tbl);

int init_lpm_brt_table(struct maptbl *tbl);
int getidx_lpm_brt_table(struct maptbl *tbl);

void copy_common_maptbl(struct maptbl *tbl, u8 *dst);
void copy_tset_maptbl(struct maptbl *tbl, u8 *dst);
void copy_fmem_maptbl(struct maptbl *tbl, u8 *dst);
int getidx_vrr_fps_table(struct maptbl *);
int sw83109c_getidx_ffc_table(struct maptbl *);
int sw83109c_get_cell_id(struct panel_device *panel, void *buf);
int sw83109c_get_manufacture_code(struct panel_device *panel, void *buf);
int sw83109c_get_manufacture_date(struct panel_device *panel, void *buf);
int sw83109c_getidx_acl_opr_table(struct maptbl *tbl);
int sw83109c_getidx_acl_onoff_table(struct maptbl *tbl);
bool is_panel_state_acl(struct panel_device *panel);
bool is_panel_state_not_lpm(struct panel_device *panel);

#endif /* __SW83109C_H__ */
