/*
 * linux/drivers/video/fbdev/exynos/panel/ana6705/ana6705.h
 *
 * Header file for S6E3HAB Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ANA6705_H__
#define __ANA6705_H__

#include <linux/types.h>
#include <linux/kernel.h>

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => ANA6705_XXX_OFS (0)
 * XXX 2nd param => ANA6705_XXX_OFS (1)
 * XXX 36th param => ANA6705_XXX_OFS (35)
 */

#define ANA6705_GAMMA_CMD_CNT (35)

#define ANA6705_ADDR_OFS	(0)
#define ANA6705_ADDR_LEN	(1)
#define ANA6705_DATA_OFS	(ANA6705_ADDR_OFS + ANA6705_ADDR_LEN)

#define ANA6705_DATE_REG			0xA1
#define ANA6705_DATE_OFS			4
#define ANA6705_DATE_LEN			(PANEL_DATE_LEN)

#define ANA6705_COORDINATE_REG		0xA1
#define ANA6705_COORDINATE_OFS		0
#define ANA6705_COORDINATE_LEN		(PANEL_COORD_LEN)

#define ANA6705_ID_REG				0x04
#define ANA6705_ID_OFS				0
#define ANA6705_ID_LEN				(PANEL_ID_LEN)

#define ANA6705_CELL_ID_REG			0xA1
#define ANA6705_CELL_ID_OFS			18
#define ANA6705_CELL_ID_LEN			(PANEL_CELL_ID_LEN)

#define ANA6705_ELVSS_REG			0xB7
#define ANA6705_ELVSS_OFS			7
#define ANA6705_ELVSS_LEN			2

#if 0
#define ANA6705_ELVSS_TEMP_0_REG		0xB5
#define ANA6705_ELVSS_TEMP_0_OFS		45
#define ANA6705_ELVSS_TEMP_0_LEN		1

#define ANA6705_ELVSS_TEMP_1_REG		0xB5
#define ANA6705_ELVSS_TEMP_1_OFS		75
#define ANA6705_ELVSS_TEMP_1_LEN		1

#define ANA6705_OCTA_ID_REG			0xC9
#define ANA6705_OCTA_ID_OFS			1
#define ANA6705_OCTA_ID_LEN			(PANEL_OCTA_ID_LEN)

/* for brightness debugging */
#define ANA6705_GAMMA_REG			0xCA
#define ANA6705_GAMMA_OFS			0
#define ANA6705_GAMMA_LEN			34
#endif

/* for panel dump */
#define ANA6705_RDDPM_REG			0x0A
#define ANA6705_RDDPM_OFS			0
#define ANA6705_RDDPM_LEN			(PANEL_RDDPM_LEN)

#define ANA6705_RDDSM_REG			0x0E
#define ANA6705_RDDSM_OFS			0
#define ANA6705_RDDSM_LEN			(PANEL_RDDSM_LEN)

#define ANA6705_ERR_REG				0xEA
#define ANA6705_ERR_OFS				0
#define ANA6705_ERR_LEN				5

#define ANA6705_ERR_FG_REG			0xEE
#define ANA6705_ERR_FG_OFS			0
#define ANA6705_ERR_FG_LEN			1

#define ANA6705_DSI_ERR_REG			0x05
#define ANA6705_DSI_ERR_OFS			0
#define ANA6705_DSI_ERR_LEN			1

#ifdef CONFIG_EXYNOS_DECON_MDNIE
#define NR_ANA6705_MDNIE_REG	(3)

#define ANA6705_MDNIE_0_REG		(0xDF)
#define ANA6705_MDNIE_0_OFS		(0)
#define ANA6705_MDNIE_0_LEN		(124)

#define ANA6705_MDNIE_1_REG		(0xDE)
#define ANA6705_MDNIE_1_OFS		(ANA6705_MDNIE_0_OFS + ANA6705_MDNIE_0_LEN)
#define ANA6705_MDNIE_1_LEN		(196)

#define ANA6705_MDNIE_2_REG		(0xDD)
#define ANA6705_MDNIE_2_OFS		(ANA6705_MDNIE_1_OFS + ANA6705_MDNIE_1_LEN)
#define ANA6705_MDNIE_2_LEN		(19)
#define ANA6705_MDNIE_LEN		(ANA6705_MDNIE_0_LEN + ANA6705_MDNIE_1_LEN + ANA6705_MDNIE_2_LEN)

#ifdef CONFIG_SUPPORT_AFC
#define ANA6705_AFC_REG			(0xE2)
#define ANA6705_AFC_OFS			(0)
#define ANA6705_AFC_LEN			(70)
#define ANA6705_AFC_ROI_OFS		(55)
#define ANA6705_AFC_ROI_LEN		(12)
#endif

#define ANA6705_SCR_CR_OFS	(31)
#define ANA6705_SCR_WR_OFS	(49)
#define ANA6705_SCR_WG_OFS	(51)
#define ANA6705_SCR_WB_OFS	(53)
#define ANA6705_NIGHT_MODE_OFS	(ANA6705_SCR_CR_OFS)
#define ANA6705_NIGHT_MODE_LEN	(24)
#define ANA6705_COLOR_LENS_OFS	(ANA6705_SCR_CR_OFS)
#define ANA6705_COLOR_LENS_LEN	(24)

#define ANA6705_TRANS_MODE_OFS	(16)
#define ANA6705_TRANS_MODE_LEN	(1)
#endif /* CONFIG_EXYNOS_DECON_MDNIE */

#define ANA6705_NR_LUMINANCE		(256)
#define ANA6705_NR_HBM_LUMINANCE	(110)

#define ANA6705_TOTAL_NR_LUMINANCE (ANA6705_NR_LUMINANCE + ANA6705_NR_HBM_LUMINANCE)

enum {
	GAMMA_MAPTBL,
	MPS_MAPTBL,
	TSET_MAPTBL,
	ELVSS_MAPTBL,
	ELVSS_OTP_MAPTBL,
	ELVSS_TEMP_MAPTBL,
#ifdef CONFIG_SUPPORT_XTALK_MODE
	VGH_MAPTBL,
#endif
	ACL_OPR_MAPTBL,
	CASET_MAPTBL,
	PASET_MAPTBL,
	FPS_MAPTBL,
	BRT_MAPTBL,
	BRT_MODE_MAPTBL,
	LPM_NIT_MAPTBL,
	LPM_MODE_MAPTBL,
	LPM_ON_MAPTBL,
	LPM_OFF_MAPTBL,
	MAX_MAPTBL,
};


enum {
	READ_ID,
	READ_COORDINATE,
	READ_ELVSS,
	READ_DATE,
	READ_CELL_ID,
	READ_RDDPM,
	READ_RDDSM,
	READ_ERR,
	READ_ERR_FG,
	READ_DSI_ERR,
};

enum {
	RES_ID,
	RES_COORDINATE,
	RES_ELVSS,
	RES_DATE,
	RES_CELL_ID,
	RES_RDDPM,
	RES_RDDSM,
	RES_ERR,
	RES_ERR_FG,
	RES_DSI_ERR,
};

static u8 ANA6705_ID[ANA6705_ID_LEN];
static u8 ANA6705_COORDINATE[ANA6705_COORDINATE_LEN];
static u8 ANA6705_ELVSS[ANA6705_ELVSS_LEN];
static u8 ANA6705_DATE[ANA6705_DATE_LEN];
static u8 ANA6705_CELL_ID[ANA6705_CELL_ID_LEN];
static u8 ANA6705_RDDPM[ANA6705_RDDPM_LEN];
static u8 ANA6705_RDDSM[ANA6705_RDDSM_LEN];
static u8 ANA6705_ERR[ANA6705_ERR_LEN];
static u8 ANA6705_ERR_FG[ANA6705_ERR_FG_LEN];
static u8 ANA6705_DSI_ERR[ANA6705_DSI_ERR_LEN];

static struct rdinfo ana6705_rditbl[] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, ANA6705_ID_REG, ANA6705_ID_OFS, ANA6705_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, ANA6705_COORDINATE_REG, ANA6705_COORDINATE_OFS, ANA6705_COORDINATE_LEN),
	[READ_ELVSS] = RDINFO_INIT(elvss, DSI_PKT_TYPE_RD, ANA6705_ELVSS_REG, ANA6705_ELVSS_OFS, ANA6705_ELVSS_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, ANA6705_DATE_REG, ANA6705_DATE_OFS, ANA6705_DATE_LEN),
	[READ_CELL_ID] = RDINFO_INIT(cell_id, DSI_PKT_TYPE_RD, ANA6705_CELL_ID_REG, ANA6705_CELL_ID_OFS, ANA6705_CELL_ID_LEN),
	[READ_RDDPM] = RDINFO_INIT(rddpm, DSI_PKT_TYPE_RD, ANA6705_RDDPM_REG, ANA6705_RDDPM_OFS, ANA6705_RDDPM_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, ANA6705_RDDSM_REG, ANA6705_RDDSM_OFS, ANA6705_RDDSM_LEN),
	[READ_ERR] = RDINFO_INIT(err, DSI_PKT_TYPE_RD, ANA6705_ERR_REG, ANA6705_ERR_OFS, ANA6705_ERR_LEN),
	[READ_ERR_FG] = RDINFO_INIT(err_fg, DSI_PKT_TYPE_RD, ANA6705_ERR_FG_REG, ANA6705_ERR_FG_OFS, ANA6705_ERR_FG_LEN),
	[READ_DSI_ERR] = RDINFO_INIT(dsi_err, DSI_PKT_TYPE_RD, ANA6705_DSI_ERR_REG, ANA6705_DSI_ERR_OFS, ANA6705_DSI_ERR_LEN),
};

static DEFINE_RESUI(id, &ana6705_rditbl[READ_ID], 0);
static DEFINE_RESUI(coordinate, &ana6705_rditbl[READ_COORDINATE], 0);
static DEFINE_RESUI(elvss, &ana6705_rditbl[READ_ELVSS], 0);
static DEFINE_RESUI(date, &ana6705_rditbl[READ_DATE], 0);
static DEFINE_RESUI(cell_id, &ana6705_rditbl[READ_CELL_ID], 0);
static DEFINE_RESUI(rddpm, &ana6705_rditbl[READ_RDDPM], 0);
static DEFINE_RESUI(rddsm, &ana6705_rditbl[READ_RDDSM], 0);
static DEFINE_RESUI(err, &ana6705_rditbl[READ_ERR], 0);
static DEFINE_RESUI(err_fg, &ana6705_rditbl[READ_ERR_FG], 0);
static DEFINE_RESUI(dsi_err, &ana6705_rditbl[READ_DSI_ERR], 0);

static struct resinfo ana6705_restbl[] = {
	[RES_ID] = RESINFO_INIT(id, ANA6705_ID, RESUI(id)),
	[RES_COORDINATE] = RESINFO_INIT(coordinate, ANA6705_COORDINATE, RESUI(coordinate)),
	[RES_ELVSS] = RESINFO_INIT(elvss, ANA6705_ELVSS, RESUI(elvss)),
	[RES_DATE] = RESINFO_INIT(date, ANA6705_DATE, RESUI(date)),
	[RES_CELL_ID] = RESINFO_INIT(cell_id, ANA6705_CELL_ID, RESUI(cell_id)),
	[RES_RDDPM] = RESINFO_INIT(rddpm, ANA6705_RDDPM, RESUI(rddpm)),
	[RES_RDDSM] = RESINFO_INIT(rddsm, ANA6705_RDDSM, RESUI(rddsm)),
	[RES_ERR] = RESINFO_INIT(err, ANA6705_ERR, RESUI(err)),
	[RES_ERR_FG] = RESINFO_INIT(err_fg, ANA6705_ERR_FG, RESUI(err_fg)),
	[RES_DSI_ERR] = RESINFO_INIT(dsi_err, ANA6705_DSI_ERR, RESUI(dsi_err)),
};

enum {
	DUMP_RDDPM = 0,
	DUMP_RDDSM,
	DUMP_ERR,
	DUMP_ERR_FG,
	DUMP_DSI_ERR,
};

static void show_rddpm(struct dumpinfo *info);
static void show_rddsm(struct dumpinfo *info);
static void show_err(struct dumpinfo *info);
static void show_err_fg(struct dumpinfo *info);
static void show_dsi_err(struct dumpinfo *info);

static struct dumpinfo ana6705_dmptbl[] = {
	[DUMP_RDDPM] = DUMPINFO_INIT(rddpm, &ana6705_restbl[RES_RDDPM], show_rddpm),
	[DUMP_RDDSM] = DUMPINFO_INIT(rddsm, &ana6705_restbl[RES_RDDSM], show_rddsm),
	[DUMP_ERR] = DUMPINFO_INIT(err, &ana6705_restbl[RES_ERR], show_err),
	[DUMP_ERR_FG] = DUMPINFO_INIT(err_fg, &ana6705_restbl[RES_ERR_FG], show_err_fg),
	[DUMP_DSI_ERR] = DUMPINFO_INIT(dsi_err, &ana6705_restbl[RES_DSI_ERR], show_dsi_err),
};

/* Variable Refresh Rate */
enum {
	ANA6705_VRR_FPS_60,
	MAX_ANA6705_VRR_FPS,
};

static u32 ANA6705_VRR_FPS[] = {
	[ANA6705_VRR_FPS_60] = 60,
};

static int init_common_table(struct maptbl *);
static int init_brightness_table(struct maptbl *tbl);
static int init_elvss_table(struct maptbl *tbl);
//static int init_elvss_temp_table(struct maptbl *);
//static int getidx_elvss_temp_table(struct maptbl *);

#ifdef CONFIG_SUPPORT_XTALK_MODE
static int getidx_vgh_table(struct maptbl *);
#endif
static int getidx_brt_mode_table(struct maptbl *);
//static int getidx_acl_onoff_table(struct maptbl *);
//static int getidx_mps_table(struct maptbl *);
static int getidx_acl_opr_table(struct maptbl *);
static int getidx_brt_table(struct maptbl *);
static int getidx_fps_table(struct maptbl *);
static int init_lpm_table(struct maptbl *tbl);
static int getidx_lpm_table(struct maptbl *);

static void copy_common_maptbl(struct maptbl *, u8 *);
static void copy_tset_maptbl(struct maptbl *, u8 *);
static void copy_elvss_otp_maptbl(struct maptbl *, u8 *);

#ifdef CONFIG_EXYNOS_DECON_MDNIE
static int getidx_common_maptbl(struct maptbl *);
static void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst);
static int init_color_blind_table(struct maptbl *tbl);
static int getidx_mdnie_scenario_maptbl(struct maptbl *tbl);
static int getidx_mdnie_hdr_maptbl(struct maptbl *tbl);
static int getidx_mdnie_trans_mode_maptbl(struct maptbl *tbl);
static int init_mdnie_night_mode_table(struct maptbl *tbl);
static int getidx_mdnie_night_mode_maptbl(struct maptbl *tbl);
static int init_mdnie_color_lens_table(struct maptbl *tbl);
static int getidx_color_lens_maptbl(struct maptbl *tbl);
static int init_color_coordinate_table(struct maptbl *);
static int init_sensor_rgb_table(struct maptbl *tbl);
static int getidx_adjust_ldu_maptbl(struct maptbl *tbl);
static int getidx_color_coordinate_maptbl(struct maptbl *tbl);
static void copy_color_coordinate_maptbl(struct maptbl *tbl, u8 *dst);
static void copy_scr_white_maptbl(struct maptbl *tbl, u8 *dst);
static void copy_adjust_ldu_maptbl(struct maptbl *tbl, u8 *dst);
static int getidx_trans_maptbl(struct pkt_update_info *pktui);
static int getidx_mdnie_0_maptbl(struct pkt_update_info *pktui);
static int getidx_mdnie_1_maptbl(struct pkt_update_info *pktui);
static int getidx_mdnie_2_maptbl(struct pkt_update_info *pktui);
static int getidx_mdnie_scr_white_maptbl(struct pkt_update_info *pktui);
#ifdef CONFIG_SUPPORT_AFC
static void copy_afc_maptbl(struct maptbl *tbl, u8 *dst);
#endif
static void update_current_scr_white(struct maptbl *tbl, u8 *dst);
#endif /* CONFIG_EXYNOS_DECON_MDNIE */
#endif /* __ANA6705_H__ */
