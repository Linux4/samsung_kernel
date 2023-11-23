/*
 * linux/drivers/video/fbdev/exynos/panel/ea8076/ea8076.h
 *
 * Header file for S6E3HAB Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8076_H__
#define __EA8076_H__

#include <linux/types.h>
#include <linux/kernel.h>

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => EA8076_XXX_OFS (0)
 * XXX 2nd param => EA8076_XXX_OFS (1)
 * XXX 36th param => EA8076_XXX_OFS (35)
 */

#define EA8076_GAMMA_CMD_CNT (35)

#define EA8076_ADDR_OFS	(0)
#define EA8076_ADDR_LEN	(1)
#define EA8076_DATA_OFS	(EA8076_ADDR_OFS + EA8076_ADDR_LEN)

#define EA8076_DATE_REG			0xEA
#define EA8076_DATE_OFS			7
#define EA8076_DATE_LEN			(PANEL_DATE_LEN)

#define EA8076_COORDINATE_REG		0xEA
#define EA8076_COORDINATE_OFS		3
#define EA8076_COORDINATE_LEN		(PANEL_COORD_LEN)

#define EA8076_ID_REG				0x04
#define EA8076_ID_OFS				0
#define EA8076_ID_LEN				(PANEL_ID_LEN)

#define EA8076_CODE_REG			0xD1
#define EA8076_CODE_OFS			55
#define EA8076_CODE_LEN			PANEL_CODE_LEN		// will modify

#define EA8076_MANUFACTURE_INFO_REG			0xEA
#define EA8076_MANUFACTURE_INFO_OFS			15
#define EA8076_MANUFACTURE_INFO_LEN			(PANEL_MANUFACTURE_INFO_LEN)

#define EA8076_CELL_ID_REG			0xEF
#define EA8076_CELL_ID_OFS			2
#define EA8076_CELL_ID_LEN			(PANEL_CELL_ID_LEN)

#define EA8076_ELVSS_REG			0xB7
#define EA8076_ELVSS_OFS			7
#define EA8076_ELVSS_LEN			2

#if 0
#define EA8076_ELVSS_TEMP_0_REG		0xB5
#define EA8076_ELVSS_TEMP_0_OFS		45
#define EA8076_ELVSS_TEMP_0_LEN		1

#define EA8076_ELVSS_TEMP_1_REG		0xB5
#define EA8076_ELVSS_TEMP_1_OFS		75
#define EA8076_ELVSS_TEMP_1_LEN		1

/* for brightness debugging */
#define EA8076_GAMMA_REG			0xCA
#define EA8076_GAMMA_OFS			0
#define EA8076_GAMMA_LEN			34
#endif

/* for panel dump */
#define EA8076_RDDPM_REG			0x0A
#define EA8076_RDDPM_OFS			0
#define EA8076_RDDPM_LEN			(PANEL_RDDPM_LEN)

#define EA8076_RDDSM_REG			0x0E
#define EA8076_RDDSM_OFS			0
#define EA8076_RDDSM_LEN			(PANEL_RDDSM_LEN)

#define EA8076_ERR_REG				0xEA
#define EA8076_ERR_OFS				0
#define EA8076_ERR_LEN				5

#define EA8076_ERR_FG_REG			0xEE
#define EA8076_ERR_FG_OFS			0
#define EA8076_ERR_FG_LEN			1

#define EA8076_DSI_ERR_REG			0x05
#define EA8076_DSI_ERR_OFS			0
#define EA8076_DSI_ERR_LEN			1

#ifdef CONFIG_EXYNOS_DECON_MDNIE
#define NR_EA8076_MDNIE_REG	(3)

#define EA8076_MDNIE_0_REG		(0xDF)
#define EA8076_MDNIE_0_OFS		(0)
#define EA8076_MDNIE_0_LEN		(124)

#define EA8076_MDNIE_1_REG		(0xDE)
#define EA8076_MDNIE_1_OFS		(EA8076_MDNIE_0_OFS + EA8076_MDNIE_0_LEN)
#define EA8076_MDNIE_1_LEN		(196)

#define EA8076_MDNIE_2_REG		(0xDD)
#define EA8076_MDNIE_2_OFS		(EA8076_MDNIE_1_OFS + EA8076_MDNIE_1_LEN)
#define EA8076_MDNIE_2_LEN		(19)
#define EA8076_MDNIE_LEN		(EA8076_MDNIE_0_LEN + EA8076_MDNIE_1_LEN + EA8076_MDNIE_2_LEN)

#ifdef CONFIG_SUPPORT_AFC
#define EA8076_AFC_REG			(0xE2)
#define EA8076_AFC_OFS			(0)
#define EA8076_AFC_LEN			(70)
#define EA8076_AFC_ROI_OFS		(55)
#define EA8076_AFC_ROI_LEN		(12)
#endif

#define EA8076_SCR_CR_OFS	(31)
#define EA8076_SCR_WR_OFS	(49)
#define EA8076_SCR_WG_OFS	(51)
#define EA8076_SCR_WB_OFS	(53)
#define EA8076_NIGHT_MODE_OFS	(EA8076_SCR_CR_OFS)
#define EA8076_NIGHT_MODE_LEN	(24)
#define EA8076_COLOR_LENS_OFS	(EA8076_SCR_CR_OFS)
#define EA8076_COLOR_LENS_LEN	(24)

#define EA8076_TRANS_MODE_OFS	(16)
#define EA8076_TRANS_MODE_LEN	(1)
#endif /* CONFIG_EXYNOS_DECON_MDNIE */

#define EA8076_NR_LUMINANCE		(256)
#define EA8076_NR_HBM_LUMINANCE	(110)

#define EA8076_TOTAL_NR_LUMINANCE (EA8076_NR_LUMINANCE + EA8076_NR_HBM_LUMINANCE)

enum {
	GAMMA_MAPTBL,
	MPS_MAPTBL,
	TSET_MAPTBL,
	ELVSS_MAPTBL,
	ELVSS_OTP_MAPTBL,
	ELVSS_TEMP_MAPTBL,
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
	READ_CODE,
	READ_ELVSS,
	READ_DATE,
	READ_CELL_ID,
	READ_MANUFACTURE_INFO,
	READ_RDDPM,
	READ_RDDSM,
	READ_ERR,
	READ_ERR_FG,
	READ_DSI_ERR,
};

enum {
	RES_ID,
	RES_COORDINATE,
	RES_CODE,
	RES_ELVSS,
	RES_DATE,
	RES_CELL_ID,
	RES_MANUFACTURE_INFO,
	RES_RDDPM,
	RES_RDDSM,
	RES_ERR,
	RES_ERR_FG,
	RES_DSI_ERR,
};

static u8 EA8076_ID[EA8076_ID_LEN];
static u8 EA8076_COORDINATE[EA8076_COORDINATE_LEN];
static u8 EA8076_CODE[EA8076_CODE_LEN];
static u8 EA8076_ELVSS[EA8076_ELVSS_LEN];
static u8 EA8076_DATE[EA8076_DATE_LEN];
static u8 EA8076_CELL_ID[EA8076_CELL_ID_LEN];
static u8 EA8076_MANUFACTURE_INFO[EA8076_MANUFACTURE_INFO_LEN];
static u8 EA8076_RDDPM[EA8076_RDDPM_LEN];
static u8 EA8076_RDDSM[EA8076_RDDSM_LEN];
static u8 EA8076_ERR[EA8076_ERR_LEN];
static u8 EA8076_ERR_FG[EA8076_ERR_FG_LEN];
static u8 EA8076_DSI_ERR[EA8076_DSI_ERR_LEN];

static struct rdinfo ea8076_rditbl[] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, EA8076_ID_REG, EA8076_ID_OFS, EA8076_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, EA8076_COORDINATE_REG, EA8076_COORDINATE_OFS, EA8076_COORDINATE_LEN),
	[READ_CODE] = RDINFO_INIT(code, DSI_PKT_TYPE_RD, EA8076_CODE_REG, EA8076_CODE_OFS, EA8076_CODE_LEN),
	[READ_ELVSS] = RDINFO_INIT(elvss, DSI_PKT_TYPE_RD, EA8076_ELVSS_REG, EA8076_ELVSS_OFS, EA8076_ELVSS_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, EA8076_DATE_REG, EA8076_DATE_OFS, EA8076_DATE_LEN),
	[READ_CELL_ID] = RDINFO_INIT(cell_id, DSI_PKT_TYPE_RD, EA8076_CELL_ID_REG, EA8076_CELL_ID_OFS, EA8076_CELL_ID_LEN),
	[READ_MANUFACTURE_INFO] = RDINFO_INIT(manufacture_info, DSI_PKT_TYPE_RD, EA8076_MANUFACTURE_INFO_REG, EA8076_MANUFACTURE_INFO_OFS, EA8076_MANUFACTURE_INFO_LEN),
	[READ_RDDPM] = RDINFO_INIT(rddpm, DSI_PKT_TYPE_RD, EA8076_RDDPM_REG, EA8076_RDDPM_OFS, EA8076_RDDPM_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, EA8076_RDDSM_REG, EA8076_RDDSM_OFS, EA8076_RDDSM_LEN),
	[READ_ERR] = RDINFO_INIT(err, DSI_PKT_TYPE_RD, EA8076_ERR_REG, EA8076_ERR_OFS, EA8076_ERR_LEN),
	[READ_ERR_FG] = RDINFO_INIT(err_fg, DSI_PKT_TYPE_RD, EA8076_ERR_FG_REG, EA8076_ERR_FG_OFS, EA8076_ERR_FG_LEN),
	[READ_DSI_ERR] = RDINFO_INIT(dsi_err, DSI_PKT_TYPE_RD, EA8076_DSI_ERR_REG, EA8076_DSI_ERR_OFS, EA8076_DSI_ERR_LEN),
};

static DEFINE_RESUI(id, &ea8076_rditbl[READ_ID], 0);
static DEFINE_RESUI(coordinate, &ea8076_rditbl[READ_COORDINATE], 0);
static DEFINE_RESUI(code, &ea8076_rditbl[READ_CODE], 0);
static DEFINE_RESUI(elvss, &ea8076_rditbl[READ_ELVSS], 0);
static DEFINE_RESUI(date, &ea8076_rditbl[READ_DATE], 0);
static DEFINE_RESUI(cell_id, &ea8076_rditbl[READ_CELL_ID], 0);
static DEFINE_RESUI(manufacture_info, &ea8076_rditbl[READ_MANUFACTURE_INFO], 0);
static DEFINE_RESUI(rddpm, &ea8076_rditbl[READ_RDDPM], 0);
static DEFINE_RESUI(rddsm, &ea8076_rditbl[READ_RDDSM], 0);
static DEFINE_RESUI(err, &ea8076_rditbl[READ_ERR], 0);
static DEFINE_RESUI(err_fg, &ea8076_rditbl[READ_ERR_FG], 0);
static DEFINE_RESUI(dsi_err, &ea8076_rditbl[READ_DSI_ERR], 0);

static struct resinfo ea8076_restbl[] = {
	[RES_ID] = RESINFO_INIT(id, EA8076_ID, RESUI(id)),
	[RES_COORDINATE] = RESINFO_INIT(coordinate, EA8076_COORDINATE, RESUI(coordinate)),
	[RES_CODE] = RESINFO_INIT(code, EA8076_CODE, RESUI(code)),
	[RES_ELVSS] = RESINFO_INIT(elvss, EA8076_ELVSS, RESUI(elvss)),
	[RES_DATE] = RESINFO_INIT(date, EA8076_DATE, RESUI(date)),
	[RES_CELL_ID] = RESINFO_INIT(cell_id, EA8076_CELL_ID, RESUI(cell_id)),
	[RES_MANUFACTURE_INFO] = RESINFO_INIT(manufacture_info, EA8076_MANUFACTURE_INFO, RESUI(manufacture_info)),
	[RES_RDDPM] = RESINFO_INIT(rddpm, EA8076_RDDPM, RESUI(rddpm)),
	[RES_RDDSM] = RESINFO_INIT(rddsm, EA8076_RDDSM, RESUI(rddsm)),
	[RES_ERR] = RESINFO_INIT(err, EA8076_ERR, RESUI(err)),
	[RES_ERR_FG] = RESINFO_INIT(err_fg, EA8076_ERR_FG, RESUI(err_fg)),
	[RES_DSI_ERR] = RESINFO_INIT(dsi_err, EA8076_DSI_ERR, RESUI(dsi_err)),
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

static struct dumpinfo ea8076_dmptbl[] = {
	[DUMP_RDDPM] = DUMPINFO_INIT(rddpm, &ea8076_restbl[RES_RDDPM], show_rddpm),
	[DUMP_RDDSM] = DUMPINFO_INIT(rddsm, &ea8076_restbl[RES_RDDSM], show_rddsm),
	[DUMP_ERR] = DUMPINFO_INIT(err, &ea8076_restbl[RES_ERR], show_err),
	[DUMP_ERR_FG] = DUMPINFO_INIT(err_fg, &ea8076_restbl[RES_ERR_FG], show_err_fg),
	[DUMP_DSI_ERR] = DUMPINFO_INIT(dsi_err, &ea8076_restbl[RES_DSI_ERR], show_dsi_err),
};

/* Variable Refresh Rate */
enum {
	EA8076_VRR_FPS_60,
	MAX_EA8076_VRR_FPS,
};

static u32 EA8076_VRR_FPS[] = {
	[EA8076_VRR_FPS_60] = 60,
};

static int init_common_table(struct maptbl *);
static int init_brightness_table(struct maptbl *tbl);
static int init_elvss_table(struct maptbl *tbl);
//static int init_elvss_temp_table(struct maptbl *);
//static int getidx_elvss_temp_table(struct maptbl *);
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
#endif /* __EA8076_H__ */
