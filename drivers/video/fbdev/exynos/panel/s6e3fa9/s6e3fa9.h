/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fa9/s6e3fa9.h
 *
 * Header file for S6E3HAB Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FA9_H__
#define __S6E3FA9_H__

#include <linux/types.h>
#include <linux/kernel.h>

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => S6E3FA9_XXX_OFS (0)
 * XXX 2nd param => S6E3FA9_XXX_OFS (1)
 * XXX 36th param => S6E3FA9_XXX_OFS (35)
 */

#define S6E3FA9_GAMMA_CMD_CNT (35)

#define S6E3FA9_ADDR_OFS	(0)
#define S6E3FA9_ADDR_LEN	(1)
#define S6E3FA9_DATA_OFS	(S6E3FA9_ADDR_OFS + S6E3FA9_ADDR_LEN)

#define S6E3FA9_DATE_REG			0xA1
#define S6E3FA9_DATE_OFS			4
#define S6E3FA9_DATE_LEN			(PANEL_DATE_LEN)

#define S6E3FA9_COORDINATE_REG		0xA1
#define S6E3FA9_COORDINATE_OFS		0
#define S6E3FA9_COORDINATE_LEN		(PANEL_COORD_LEN)

#define S6E3FA9_ID_REG				0x04
#define S6E3FA9_ID_OFS				0
#define S6E3FA9_ID_LEN				(PANEL_ID_LEN)

#define S6E3FA9_CODE_REG			0xD6
#define S6E3FA9_CODE_OFS			0
#define S6E3FA9_CODE_LEN			(PANEL_CODE_LEN)

#define S6E3FA9_CELL_ID_REG			0xA1
#define S6E3FA9_CELL_ID_OFS			15
#define S6E3FA9_CELL_ID_LEN			(PANEL_CELL_ID_LEN)

#define S6E3FA9_MANUFACTURE_INFO_REG			0xA1
#define S6E3FA9_MANUFACTURE_INFO_OFS			11
#define S6E3FA9_MANUFACTURE_INFO_LEN			(PANEL_MANUFACTURE_INFO_LEN)

#define S6E3FA9_OCTA_ID_REG			0xA1
#define S6E3FA9_OCTA_ID_OFS			11
#define S6E3FA9_OCTA_ID_LEN			(PANEL_OCTA_ID_LEN)

#define S6E3FA9_ELVSS_REG			0xB7
#define S6E3FA9_ELVSS_OFS			7
#define S6E3FA9_ELVSS_LEN			2

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
#define S6E3FA9_GRAYSPOT_REG			0xB5
#define S6E3FA9_GRAYSPOT_OFS			2
#define S6E3FA9_GRAYSPOT_LEN			2
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
#define S6E3FA9_CCD_STATE_REG				0xCD
#define S6E3FA9_CCD_STATE_OFS				0
#define S6E3FA9_CCD_STATE_LEN				1
//#define INVALID_CCD_STATE				0x04
#endif

#ifdef CONFIG_EXTEND_LIVE_CLOCK
#define S6E3FA9_SELF_MASK_CHECKSUM_REG				0xFB
#define S6E3FA9_SELF_MASK_CHECKSUM_OFS				15	/*	16th, 17th	*/
#define S6E3FA9_SELF_MASK_CHECKSUM_LEN				2
#endif

#if 0
#define S6E3FA9_ELVSS_TEMP_0_REG		0xB5
#define S6E3FA9_ELVSS_TEMP_0_OFS		45
#define S6E3FA9_ELVSS_TEMP_0_LEN		1

#define S6E3FA9_ELVSS_TEMP_1_REG		0xB5
#define S6E3FA9_ELVSS_TEMP_1_OFS		75
#define S6E3FA9_ELVSS_TEMP_1_LEN		1

/* for brightness debugging */
#define S6E3FA9_GAMMA_REG			0xCA
#define S6E3FA9_GAMMA_OFS			0
#define S6E3FA9_GAMMA_LEN			34
#endif

/* for panel dump */
#define S6E3FA9_RDDPM_REG			0x0A
#define S6E3FA9_RDDPM_OFS			0
#define S6E3FA9_RDDPM_LEN			(PANEL_RDDPM_LEN)

#define S6E3FA9_RDDSM_REG			0x0E
#define S6E3FA9_RDDSM_OFS			0
#define S6E3FA9_RDDSM_LEN			(PANEL_RDDSM_LEN)

#define S6E3FA9_ERR_REG				0xEA
#define S6E3FA9_ERR_OFS				0
#define S6E3FA9_ERR_LEN				5

#define S6E3FA9_ERR_FG_REG			0xEE
#define S6E3FA9_ERR_FG_OFS			0
#define S6E3FA9_ERR_FG_LEN			1

#define S6E3FA9_DSI_ERR_REG			0x05
#define S6E3FA9_DSI_ERR_OFS			0
#define S6E3FA9_DSI_ERR_LEN			1

#ifdef CONFIG_EXYNOS_DECON_MDNIE
#define NR_S6E3FA9_MDNIE_REG	(3)

#define S6E3FA9_MDNIE_0_REG		(0xDF)
#define S6E3FA9_MDNIE_0_OFS		(0)
#define S6E3FA9_MDNIE_0_LEN		(55)

#define S6E3FA9_MDNIE_1_REG		(0xDE)
#define S6E3FA9_MDNIE_1_OFS		(S6E3FA9_MDNIE_0_OFS + S6E3FA9_MDNIE_0_LEN)
#define S6E3FA9_MDNIE_1_LEN		(182)

#define S6E3FA9_MDNIE_2_REG		(0xDD)
#define S6E3FA9_MDNIE_2_OFS		(S6E3FA9_MDNIE_1_OFS + S6E3FA9_MDNIE_1_LEN)
#define S6E3FA9_MDNIE_2_LEN		(19)
#define S6E3FA9_MDNIE_LEN		(S6E3FA9_MDNIE_0_LEN + S6E3FA9_MDNIE_1_LEN + S6E3FA9_MDNIE_2_LEN)

#ifdef CONFIG_SUPPORT_AFC
#define S6E3FA9_AFC_REG			(0xE2)
#define S6E3FA9_AFC_OFS			(0)
#define S6E3FA9_AFC_LEN			(70)
#define S6E3FA9_AFC_ROI_OFS		(55)
#define S6E3FA9_AFC_ROI_LEN		(12)
#endif

#define S6E3FA9_SCR_CR_OFS	(31)
#define S6E3FA9_SCR_WR_OFS	(49)
#define S6E3FA9_SCR_WG_OFS	(51)
#define S6E3FA9_SCR_WB_OFS	(53)
#define S6E3FA9_NIGHT_MODE_OFS	(S6E3FA9_SCR_CR_OFS)
#define S6E3FA9_NIGHT_MODE_LEN	(24)
#define S6E3FA9_COLOR_LENS_OFS	(S6E3FA9_SCR_CR_OFS)
#define S6E3FA9_COLOR_LENS_LEN	(24)

#define S6E3FA9_TRANS_MODE_OFS	(16)
#define S6E3FA9_TRANS_MODE_LEN	(1)
#endif /* CONFIG_EXYNOS_DECON_MDNIE */

#define S6E3FA9_NR_LUMINANCE		(256)
#define S6E3FA9_NR_HBM_LUMINANCE	(170)

#define S6E3FA9_TOTAL_NR_LUMINANCE (S6E3FA9_NR_LUMINANCE + S6E3FA9_NR_HBM_LUMINANCE)

#ifdef CONFIG_SUPPORT_POC_FLASH
#define S6E3FA9_POC_MCA_CHKSUM_REG		(0x91)
#define S6E3FA9_POC_MCA_CHKSUM_OFS		(0)
#define S6E3FA9_POC_MCA_CHKSUM_LEN		(94)

#define S6E3FA9_POC_DATA_REG		(0x6E)
#define S6E3FA9_POC_DATA_OFS		(0)
#define S6E3FA9_POC_DATA_LEN		(128)
#endif

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
#ifdef CONFIG_SUPPORT_HMD
	/* HMD MAPTBL */
	HMD_BRT_MAPTBL,
#endif /* CONFIG_SUPPORT_HMD */
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	GRAYSPOT_OTP_MAPTBL,
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	DYN_FFC_MAPTBL,
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
	POC_ON_MAPTBL,
	POC_WR_ADDR_MAPTBL,
	POC_RD_ADDR_MAPTBL,
	POC_WR_DATA_MAPTBL,
	POC_ER_ADDR_MAPTBL,
#endif

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
	READ_OCTA_ID,
	READ_RDDPM,
	READ_RDDSM,
	READ_ERR,
	READ_ERR_FG,
	READ_DSI_ERR,
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	READ_GRAYSPOT,
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	READ_CCD_STATE,
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	READ_SELF_MASK_CHECKSUM,
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
	READ_POC_DATA,
	READ_POC_MCA_CHKSUM,
#endif
};

enum {
	RES_ID,
	RES_COORDINATE,
	RES_CODE,
	RES_ELVSS,
	RES_DATE,
	RES_CELL_ID,
	RES_MANUFACTURE_INFO,
	RES_OCTA_ID,
	RES_RDDPM,
	RES_RDDSM,
	RES_ERR,
	RES_ERR_FG,
	RES_DSI_ERR,
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	RES_GRAYSPOT,
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	RES_CCD_STATE,
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	RES_SELF_MASK_CHECKSUM,
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
	RES_POC_CHKSUM,
	RES_POC_CTRL,
	RES_POC_DATA,
	RES_FLASH_MCD,
#endif
};

static u8 S6E3FA9_ID[S6E3FA9_ID_LEN];
static u8 S6E3FA9_COORDINATE[S6E3FA9_COORDINATE_LEN];
static u8 S6E3FA9_CODE[S6E3FA9_CODE_LEN];
static u8 S6E3FA9_ELVSS[S6E3FA9_ELVSS_LEN];
static u8 S6E3FA9_DATE[S6E3FA9_DATE_LEN];
static u8 S6E3FA9_CELL_ID[S6E3FA9_CELL_ID_LEN];
static u8 S6E3FA9_MANUFACTURE_INFO[S6E3FA9_MANUFACTURE_INFO_LEN];
static u8 S6E3FA9_OCTA_ID[S6E3FA9_OCTA_ID_LEN];
static u8 S6E3FA9_RDDPM[S6E3FA9_RDDPM_LEN];
static u8 S6E3FA9_RDDSM[S6E3FA9_RDDSM_LEN];
static u8 S6E3FA9_ERR[S6E3FA9_ERR_LEN];
static u8 S6E3FA9_ERR_FG[S6E3FA9_ERR_FG_LEN];
static u8 S6E3FA9_DSI_ERR[S6E3FA9_DSI_ERR_LEN];
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static u8 S6E3FA9_GRAYSPOT[S6E3FA9_GRAYSPOT_LEN];
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
static u8 S6E3FA9_CCD_STATE[S6E3FA9_CCD_STATE_LEN];
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
static u8 S6E3FA9_SELF_MASK_CHECKSUM[S6E3FA9_SELF_MASK_CHECKSUM_LEN];
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
static u8 S6E3FA9_POC_MCA_CHKSUM[S6E3FA9_POC_MCA_CHKSUM_LEN];
static u8 S6E3FA9_POC_DATA[S6E3FA9_POC_DATA_LEN];
#endif

static struct rdinfo s6e3fa9_rditbl[] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, S6E3FA9_ID_REG, S6E3FA9_ID_OFS, S6E3FA9_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, S6E3FA9_COORDINATE_REG, S6E3FA9_COORDINATE_OFS, S6E3FA9_COORDINATE_LEN),
	[READ_CODE] = RDINFO_INIT(code, DSI_PKT_TYPE_RD, S6E3FA9_CODE_REG, S6E3FA9_CODE_OFS, S6E3FA9_CODE_LEN),
	[READ_ELVSS] = RDINFO_INIT(elvss, DSI_PKT_TYPE_RD, S6E3FA9_ELVSS_REG, S6E3FA9_ELVSS_OFS, S6E3FA9_ELVSS_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, S6E3FA9_DATE_REG, S6E3FA9_DATE_OFS, S6E3FA9_DATE_LEN),
	[READ_CELL_ID] = RDINFO_INIT(cell_id, DSI_PKT_TYPE_RD, S6E3FA9_CELL_ID_REG, S6E3FA9_CELL_ID_OFS, S6E3FA9_CELL_ID_LEN),
	[READ_MANUFACTURE_INFO] = RDINFO_INIT(manufacture_info, DSI_PKT_TYPE_RD, S6E3FA9_MANUFACTURE_INFO_REG, S6E3FA9_MANUFACTURE_INFO_OFS, S6E3FA9_MANUFACTURE_INFO_LEN),
	[READ_OCTA_ID] = RDINFO_INIT(octa_id, DSI_PKT_TYPE_RD, S6E3FA9_OCTA_ID_REG, S6E3FA9_OCTA_ID_OFS, S6E3FA9_OCTA_ID_LEN),
	[READ_RDDPM] = RDINFO_INIT(rddpm, DSI_PKT_TYPE_RD, S6E3FA9_RDDPM_REG, S6E3FA9_RDDPM_OFS, S6E3FA9_RDDPM_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, S6E3FA9_RDDSM_REG, S6E3FA9_RDDSM_OFS, S6E3FA9_RDDSM_LEN),
	[READ_ERR] = RDINFO_INIT(err, DSI_PKT_TYPE_RD, S6E3FA9_ERR_REG, S6E3FA9_ERR_OFS, S6E3FA9_ERR_LEN),
	[READ_ERR_FG] = RDINFO_INIT(err_fg, DSI_PKT_TYPE_RD, S6E3FA9_ERR_FG_REG, S6E3FA9_ERR_FG_OFS, S6E3FA9_ERR_FG_LEN),
	[READ_DSI_ERR] = RDINFO_INIT(dsi_err, DSI_PKT_TYPE_RD, S6E3FA9_DSI_ERR_REG, S6E3FA9_DSI_ERR_OFS, S6E3FA9_DSI_ERR_LEN),
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[READ_GRAYSPOT] = RDINFO_INIT(grayspot, DSI_PKT_TYPE_RD, S6E3FA9_GRAYSPOT_REG, S6E3FA9_GRAYSPOT_OFS, S6E3FA9_GRAYSPOT_LEN),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[READ_CCD_STATE] = RDINFO_INIT(ccd_state, DSI_PKT_TYPE_RD, S6E3FA9_CCD_STATE_REG, S6E3FA9_CCD_STATE_OFS, S6E3FA9_CCD_STATE_LEN),
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	[READ_SELF_MASK_CHECKSUM] = RDINFO_INIT(self_mask_checksum, DSI_PKT_TYPE_RD, S6E3FA9_SELF_MASK_CHECKSUM_REG, S6E3FA9_SELF_MASK_CHECKSUM_OFS, S6E3FA9_SELF_MASK_CHECKSUM_LEN),
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
	[READ_POC_MCA_CHKSUM] = RDINFO_INIT(poc_mca_chksum, DSI_PKT_TYPE_RD, S6E3FA9_POC_MCA_CHKSUM_REG, S6E3FA9_POC_MCA_CHKSUM_OFS, S6E3FA9_POC_MCA_CHKSUM_LEN),
	[READ_POC_DATA] = RDINFO_INIT(poc_data, DSI_PKT_TYPE_RD, S6E3FA9_POC_DATA_REG, S6E3FA9_POC_DATA_OFS, S6E3FA9_POC_DATA_LEN),
#endif
};

static DEFINE_RESUI(id, &s6e3fa9_rditbl[READ_ID], 0);
static DEFINE_RESUI(coordinate, &s6e3fa9_rditbl[READ_COORDINATE], 0);
static DEFINE_RESUI(code, &s6e3fa9_rditbl[READ_CODE], 0);
static DEFINE_RESUI(elvss, &s6e3fa9_rditbl[READ_ELVSS], 0);
static DEFINE_RESUI(date, &s6e3fa9_rditbl[READ_DATE], 0);
static DEFINE_RESUI(cell_id, &s6e3fa9_rditbl[READ_CELL_ID], 0);
static DEFINE_RESUI(manufacture_info, &s6e3fa9_rditbl[READ_MANUFACTURE_INFO], 0);
static DEFINE_RESUI(octa_id, &s6e3fa9_rditbl[READ_OCTA_ID], 0);
static DEFINE_RESUI(rddpm, &s6e3fa9_rditbl[READ_RDDPM], 0);
static DEFINE_RESUI(rddsm, &s6e3fa9_rditbl[READ_RDDSM], 0);
static DEFINE_RESUI(err, &s6e3fa9_rditbl[READ_ERR], 0);
static DEFINE_RESUI(err_fg, &s6e3fa9_rditbl[READ_ERR_FG], 0);
static DEFINE_RESUI(dsi_err, &s6e3fa9_rditbl[READ_DSI_ERR], 0);
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static DEFINE_RESUI(grayspot, &s6e3fa9_rditbl[READ_GRAYSPOT], 0);
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
static DEFINE_RESUI(ccd_state, &s6e3fa9_rditbl[READ_CCD_STATE], 0);
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
static DEFINE_RESUI(self_mask_checksum, &s6e3fa9_rditbl[READ_SELF_MASK_CHECKSUM], 0);
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
static DEFINE_RESUI(poc_mca_chksum, &s6e3fa9_rditbl[READ_POC_MCA_CHKSUM], 0);
static DEFINE_RESUI(poc_data, &s6e3fa9_rditbl[READ_POC_DATA], 0);
#endif

static struct resinfo s6e3fa9_restbl[] = {
	[RES_ID] = RESINFO_INIT(id, S6E3FA9_ID, RESUI(id)),
	[RES_COORDINATE] = RESINFO_INIT(coordinate, S6E3FA9_COORDINATE, RESUI(coordinate)),
	[RES_CODE] = RESINFO_INIT(code, S6E3FA9_CODE, RESUI(code)),
	[RES_ELVSS] = RESINFO_INIT(elvss, S6E3FA9_ELVSS, RESUI(elvss)),
	[RES_DATE] = RESINFO_INIT(date, S6E3FA9_DATE, RESUI(date)),
	[RES_CELL_ID] = RESINFO_INIT(cell_id, S6E3FA9_CELL_ID, RESUI(cell_id)),
	[RES_MANUFACTURE_INFO] = RESINFO_INIT(manufacture_info, S6E3FA9_MANUFACTURE_INFO, RESUI(manufacture_info)),
	[RES_OCTA_ID] = RESINFO_INIT(octa_id, S6E3FA9_OCTA_ID, RESUI(octa_id)),
	[RES_RDDPM] = RESINFO_INIT(rddpm, S6E3FA9_RDDPM, RESUI(rddpm)),
	[RES_RDDSM] = RESINFO_INIT(rddsm, S6E3FA9_RDDSM, RESUI(rddsm)),
	[RES_ERR] = RESINFO_INIT(err, S6E3FA9_ERR, RESUI(err)),
	[RES_ERR_FG] = RESINFO_INIT(err_fg, S6E3FA9_ERR_FG, RESUI(err_fg)),
	[RES_DSI_ERR] = RESINFO_INIT(dsi_err, S6E3FA9_DSI_ERR, RESUI(dsi_err)),
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[RES_GRAYSPOT] = RESINFO_INIT(grayspot, S6E3FA9_GRAYSPOT, RESUI(grayspot)),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[RES_CCD_STATE] = RESINFO_INIT(ccd_state, S6E3FA9_CCD_STATE, RESUI(ccd_state)),
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	[RES_SELF_MASK_CHECKSUM] = RESINFO_INIT(self_mask_checksum, S6E3FA9_SELF_MASK_CHECKSUM, RESUI(self_mask_checksum)),
#endif
#ifdef CONFIG_SUPPORT_DDI_FLASH
	[RES_POC_CHKSUM] = RESINFO_INIT(poc_mca_chksum, S6E3FA9_POC_MCA_CHKSUM, RESUI(poc_mca_chksum)),
	[RES_POC_DATA] = RESINFO_INIT(poc_data, S6E3FA9_POC_DATA, RESUI(poc_data)),
#endif
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

static struct dumpinfo s6e3fa9_dmptbl[] = {
	[DUMP_RDDPM] = DUMPINFO_INIT(rddpm, &s6e3fa9_restbl[RES_RDDPM], show_rddpm),
	[DUMP_RDDSM] = DUMPINFO_INIT(rddsm, &s6e3fa9_restbl[RES_RDDSM], show_rddsm),
	[DUMP_ERR] = DUMPINFO_INIT(err, &s6e3fa9_restbl[RES_ERR], show_err),
	[DUMP_ERR_FG] = DUMPINFO_INIT(err_fg, &s6e3fa9_restbl[RES_ERR_FG], show_err_fg),
	[DUMP_DSI_ERR] = DUMPINFO_INIT(dsi_err, &s6e3fa9_restbl[RES_DSI_ERR], show_dsi_err),
};

/* Variable Refresh Rate */
enum {
	S6E3FA9_VRR_FPS_60,
	MAX_S6E3FA9_VRR_FPS,
};

static u32 S6E3FA9_VRR_FPS[] = {
	[S6E3FA9_VRR_FPS_60] = 60,
};

static int init_common_table(struct maptbl *);
static int init_brightness_table(struct maptbl *tbl);
#ifdef CONFIG_SUPPORT_HMD
static int init_hmd_brightness_table(struct maptbl *tbl);
static int getidx_hmd_brt_table(struct maptbl *);
#endif
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
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static void copy_grayspot_otp_maptbl(struct maptbl *, u8 *);
#endif

#ifdef CONFIG_DYNAMIC_FREQ
static int getidx_dyn_ffc_table(struct maptbl *tbl);
#endif

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
#endif /* __S6E3FA9_H__ */
