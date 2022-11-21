/*
 * linux/drivers/video/fbdev/exynos/panel/ea8076/ea8076.h
 *
 * Header file for EA8076 Dimming Driver
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
#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "../panel_poc.h"
#endif

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

#define EA8076_MTP_REG				0xC8
#define EA8076_MTP_OFS				0
#define EA8076_MTP_LEN				34

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
#define EA8076_CODE_LEN			(PANEL_CODE_LEN)

#define EA8076_ELVSS_REG			0xB7
#define EA8076_ELVSS_OFS			7
#define EA8076_ELVSS_LEN			2

#define EA8076_OCTA_ID_REG			0xA1
#define EA8076_OCTA_ID_OFS			11
#define EA8076_OCTA_ID_LEN			(PANEL_OCTA_ID_LEN)

/* for brightness debugging */
#define EA8076_GAMMA_REG			0xCA
#define EA8076_GAMMA_OFS			0
#define EA8076_GAMMA_LEN			34

#define EA8076_AOR_REG			0xB1
#define EA8076_AOR_OFS			0
#define EA8076_AOR_LEN			2

#define EA8076_VINT_REG			0xF4
#define EA8076_VINT_OFS			4
#define EA8076_VINT_LEN			1

#define EA8076_ELVSS_T_REG			0xB5
#define EA8076_ELVSS_T_OFS			2
#define EA8076_ELVSS_T_LEN			1

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

#define EA8076_SELF_DIAG_REG			0x0F
#define EA8076_SELF_DIAG_OFS			0
#define EA8076_SELF_DIAG_LEN			1

#define EA8076_SELF_MASK_CHECKSUM_REG		0xFB
#define EA8076_SELF_MASK_CHECKSUM_OFS		15
#define EA8076_SELF_MASK_CHECKSUM_LEN		2

#define EA8076_SELF_MASK_CRC_REG		0x7F
#define EA8076_SELF_MASK_CRC_OFS	6
#define EA8076_SELF_MASK_CRC_LEN		4

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
#define EA8076_CMDLOG_REG			0x9C
#define EA8076_CMDLOG_OFS			0
#define EA8076_CMDLOG_LEN			0x80
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#define NR_EA8076_MDNIE_REG	(6)

#define EA8076_MDNIE_0_REG		(0xB1)
#define EA8076_MDNIE_0_OFS		(0)
#define EA8076_MDNIE_0_LEN		(132)

#define EA8076_MDNIE_1_REG		(0x81)
#define EA8076_MDNIE_1_OFS		(EA8076_MDNIE_0_OFS + EA8076_MDNIE_0_LEN)
#define EA8076_MDNIE_1_LEN		(1)

#define EA8076_MDNIE_2_REG		(0xB3)
#define EA8076_MDNIE_2_OFS		(EA8076_MDNIE_1_OFS + EA8076_MDNIE_1_LEN)
#define EA8076_MDNIE_2_LEN		(80)

#define EA8076_MDNIE_3_REG		(0x87)
#define EA8076_MDNIE_3_OFS		(EA8076_MDNIE_2_OFS + EA8076_MDNIE_2_LEN)
#define EA8076_MDNIE_3_LEN		(2)

#define EA8076_MDNIE_4_REG		(0x85)
#define EA8076_MDNIE_4_OFS		(EA8076_MDNIE_3_OFS + EA8076_MDNIE_3_LEN)
#define EA8076_MDNIE_4_LEN		(1)

#define EA8076_MDNIE_5_REG		(0x83)
#define EA8076_MDNIE_5_OFS		(EA8076_MDNIE_4_OFS + EA8076_MDNIE_4_LEN)
#define EA8076_MDNIE_5_LEN		(1)
#define EA8076_MDNIE_LEN		(EA8076_MDNIE_0_LEN + EA8076_MDNIE_1_LEN + EA8076_MDNIE_2_LEN + EA8076_MDNIE_3_LEN + EA8076_MDNIE_4_LEN + EA8076_MDNIE_5_LEN)

#define EA8076_SCR_CR_OFS	(1)
#define EA8076_SCR_WR_OFS	(19)
#define EA8076_SCR_WG_OFS	(20)
#define EA8076_SCR_WB_OFS	(21)
#define EA8076_NIGHT_MODE_OFS	(EA8076_SCR_CR_OFS)
#define EA8076_NIGHT_MODE_LEN	(21)
#define EA8076_COLOR_LENS_OFS	(EA8076_SCR_CR_OFS)
#define EA8076_COLOR_LENS_LEN	(21)
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */


#ifdef CONFIG_DYNAMIC_FREQ
#define EA8076_MAX_MIPI_FREQ			4
#define EA8076_DEFAULT_MIPI_FREQ		3
enum {
	EA8076_OSC_DEFAULT,
	MAX_EA8076_OSC,
};
#endif


enum {
	GAMMA_MAPTBL,
	GAMMA_MODE2_MAPTBL,
	VBIAS_MAPTBL,
	AOR_MAPTBL,
	MPS_MAPTBL,
	TSET_MAPTBL,
	ELVSS_MAPTBL,
	ELVSS_OTP_MAPTBL,
	ELVSS_TEMP_MAPTBL,
#ifdef CONFIG_SUPPORT_XTALK_MODE
	VGH_MAPTBL,
#endif
	VINT_MAPTBL,
	VINT_VRR_120HZ_MAPTBL,
	ACL_ONOFF_MAPTBL,
	ACL_FRAME_AVG_MAPTBL,
	ACL_START_POINT_MAPTBL,
	ACL_OPR_MAPTBL,
	IRC_MAPTBL,
	IRC_MODE_MAPTBL,
#ifdef CONFIG_SUPPORT_HMD
	/* HMD MAPTBL */
	HMD_GAMMA_MAPTBL,
	HMD_AOR_MAPTBL,
	HMD_ELVSS_MAPTBL,
#endif /* CONFIG_SUPPORT_HMD */
	DSC_MAPTBL,
	PPS_MAPTBL,
	SCALER_MAPTBL,
	CASET_MAPTBL,
	PASET_MAPTBL,
	SSD_IMPROVE_MAPTBL,
	AID_MAPTBL,
	OSC_MAPTBL,
	VFP_NM_MAPTBL,
	VFP_HS_MAPTBL,
	PWR_GEN_MAPTBL,
	SRC_AMP_MAPTBL,
	MTP_MAPTBL,
	FPS_MAPTBL,
	LPM_NIT_MAPTBL,
	LPM_MODE_MAPTBL,
	LPM_DYN_VLIN_MAPTBL,
	LPM_ON_MAPTBL,
	LPM_AOR_OFF_MAPTBL,
#ifdef CONFIG_SUPPORT_DDI_FLASH
	POC_ON_MAPTBL,
	POC_WR_ADDR_MAPTBL,
	POC_RD_ADDR_MAPTBL,
	POC_WR_DATA_MAPTBL,
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
	POC_ER_ADDR_MAPTBL,
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	POC_SPI_READ_ADDR_MAPTBL,
	POC_SPI_WRITE_ADDR_MAPTBL,
	POC_SPI_WRITE_DATA_MAPTBL,
	POC_SPI_ERASE_ADDR_MAPTBL,
#endif
	POC_ONOFF_MAPTBL,
#ifdef CONFIG_SUPPORT_TDMB_TUNE
	TDMB_TUNE_MAPTBL,
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	DYN_FFC_MAPTBL,
#endif

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	GRAYSPOT_CAL_MAPTBL,
#endif
	GAMMA_INTER_CONTROL_MAPTBL,
	POC_COMP_MAPTBL,
	DBV_MAPTBL,
	HBM_CYCLE_MAPTBL,
	HBM_ONOFF_MAPTBL,
	DIA_ONOFF_MAPTBL,
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	FAST_DISCHARGE_MAPTBL,
#endif
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
	READ_ELVSS,
	READ_ELVSS_TEMP_0,
	READ_ELVSS_TEMP_1,
	READ_MTP,
	READ_DATE,
	READ_OCTA_ID,
	READ_CHIP_ID,
	/* for brightness debugging */
	READ_AOR,
	READ_VINT,
	READ_ELVSS_T,
	READ_IRC,

	READ_RDDPM,
	READ_RDDSM,
	READ_ERR,
	READ_ERR_FG,
	READ_DSI_ERR,
	READ_SELF_DIAG,
	READ_SELF_MASK_CHECKSUM,
	READ_SELF_MASK_CRC,
#ifdef CONFIG_SUPPORT_POC_SPI
	READ_POC_SPI_READ,
	READ_POC_SPI_STATUS1,
	READ_POC_SPI_STATUS2,
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
	READ_POC_MCA_CHKSUM,
#endif
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	READ_CMDLOG,
#endif
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
	RES_ELVSS,
	RES_ELVSS_TEMP_0,
	RES_ELVSS_TEMP_1,
	RES_MTP,
	RES_DATE,
	RES_OCTA_ID,
	RES_CHIP_ID,
	/* for brightness debugging */
	RES_AOR,
	RES_VINT,
	RES_ELVSS_T,
	RES_IRC,
	RES_RDDPM,
	RES_RDDSM,
	RES_ERR,
	RES_ERR_FG,
	RES_DSI_ERR,
	RES_SELF_DIAG,
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	RES_CMDLOG,
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	RES_POC_SPI_READ,
	RES_POC_SPI_STATUS1,
	RES_POC_SPI_STATUS2,
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
	RES_POC_MCA_CHKSUM,
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	RES_CCD_STATE,
	RES_CCD_CHKSUM_FAIL,
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	RES_GRAYSPOT_CAL,
#endif
	RES_SELF_MASK_CHECKSUM,
	RES_SELF_MASK_CRC,
	MAX_RESTBL
};

enum {
	HLPM_ON_LOW,
	HLPM_ON,
	MAX_HLPM_ON
};

static u8 EA8076_ID[EA8076_ID_LEN];
static u8 EA8076_COORDINATE[EA8076_COORDINATE_LEN];
static u8 EA8076_CODE[EA8076_CODE_LEN];
static u8 EA8076_ELVSS[EA8076_ELVSS_LEN];
static u8 EA8076_MTP[EA8076_MTP_LEN];
static u8 EA8076_DATE[EA8076_DATE_LEN];
static u8 EA8076_OCTA_ID[EA8076_OCTA_ID_LEN];
/* for brightness debugging */
static u8 EA8076_AOR[EA8076_AOR_LEN];
static u8 EA8076_VINT[EA8076_VINT_LEN];
static u8 EA8076_ELVSS_T[EA8076_ELVSS_T_LEN];
static u8 EA8076_RDDPM[EA8076_RDDPM_LEN];
static u8 EA8076_RDDSM[EA8076_RDDSM_LEN];
static u8 EA8076_ERR[EA8076_ERR_LEN];
static u8 EA8076_ERR_FG[EA8076_ERR_FG_LEN];
static u8 EA8076_DSI_ERR[EA8076_DSI_ERR_LEN];
static u8 EA8076_SELF_DIAG[EA8076_SELF_DIAG_LEN];


static struct rdinfo ea8076_rditbl[MAX_READTBL] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, EA8076_ID_REG, EA8076_ID_OFS, EA8076_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, EA8076_COORDINATE_REG, EA8076_COORDINATE_OFS, EA8076_COORDINATE_LEN),
	[READ_CODE] = RDINFO_INIT(code, DSI_PKT_TYPE_RD, EA8076_CODE_REG, EA8076_CODE_OFS, EA8076_CODE_LEN),
	[READ_ELVSS] = RDINFO_INIT(elvss, DSI_PKT_TYPE_RD, EA8076_ELVSS_REG, EA8076_ELVSS_OFS, EA8076_ELVSS_LEN),
	[READ_MTP] = RDINFO_INIT(mtp, DSI_PKT_TYPE_RD, EA8076_MTP_REG, EA8076_MTP_OFS, EA8076_MTP_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, EA8076_DATE_REG, EA8076_DATE_OFS, EA8076_DATE_LEN),
	[READ_OCTA_ID] = RDINFO_INIT(octa_id, DSI_PKT_TYPE_RD, EA8076_OCTA_ID_REG, EA8076_OCTA_ID_OFS, EA8076_OCTA_ID_LEN),
	/* for brightness debugging */
	[READ_AOR] = RDINFO_INIT(aor, DSI_PKT_TYPE_RD, EA8076_AOR_REG, EA8076_AOR_OFS, EA8076_AOR_LEN),
	[READ_VINT] = RDINFO_INIT(vint, DSI_PKT_TYPE_RD, EA8076_VINT_REG, EA8076_VINT_OFS, EA8076_VINT_LEN),
	[READ_ELVSS_T] = RDINFO_INIT(elvss_t, DSI_PKT_TYPE_RD, EA8076_ELVSS_T_REG, EA8076_ELVSS_T_OFS, EA8076_ELVSS_T_LEN),
	[READ_RDDPM] = RDINFO_INIT(rddpm, DSI_PKT_TYPE_RD, EA8076_RDDPM_REG, EA8076_RDDPM_OFS, EA8076_RDDPM_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, EA8076_RDDSM_REG, EA8076_RDDSM_OFS, EA8076_RDDSM_LEN),
	[READ_ERR] = RDINFO_INIT(err, DSI_PKT_TYPE_RD, EA8076_ERR_REG, EA8076_ERR_OFS, EA8076_ERR_LEN),
	[READ_ERR_FG] = RDINFO_INIT(err_fg, DSI_PKT_TYPE_RD, EA8076_ERR_FG_REG, EA8076_ERR_FG_OFS, EA8076_ERR_FG_LEN),
	[READ_DSI_ERR] = RDINFO_INIT(dsi_err, DSI_PKT_TYPE_RD, EA8076_DSI_ERR_REG, EA8076_DSI_ERR_OFS, EA8076_DSI_ERR_LEN),
	[READ_SELF_DIAG] = RDINFO_INIT(self_diag, DSI_PKT_TYPE_RD, EA8076_SELF_DIAG_REG, EA8076_SELF_DIAG_OFS, EA8076_SELF_DIAG_LEN),
};

static DEFINE_RESUI(id, &ea8076_rditbl[READ_ID], 0);
static DEFINE_RESUI(coordinate, &ea8076_rditbl[READ_COORDINATE], 0);
static DEFINE_RESUI(code, &ea8076_rditbl[READ_CODE], 0);
static DEFINE_RESUI(elvss, &ea8076_rditbl[READ_ELVSS], 0);
static DEFINE_RESUI(mtp, &ea8076_rditbl[READ_MTP], 0);
static DEFINE_RESUI(date, &ea8076_rditbl[READ_DATE], 0);
static DEFINE_RESUI(octa_id, &ea8076_rditbl[READ_OCTA_ID], 0);
/* for brightness debugging */
static DEFINE_RESUI(aor, &ea8076_rditbl[READ_AOR], 0);
static DEFINE_RESUI(vint, &ea8076_rditbl[READ_VINT], 0);
static DEFINE_RESUI(elvss_t, &ea8076_rditbl[READ_ELVSS_T], 0);
static DEFINE_RESUI(rddpm, &ea8076_rditbl[READ_RDDPM], 0);
static DEFINE_RESUI(rddsm, &ea8076_rditbl[READ_RDDSM], 0);
static DEFINE_RESUI(err, &ea8076_rditbl[READ_ERR], 0);
static DEFINE_RESUI(err_fg, &ea8076_rditbl[READ_ERR_FG], 0);
static DEFINE_RESUI(dsi_err, &ea8076_rditbl[READ_DSI_ERR], 0);
static DEFINE_RESUI(self_diag, &ea8076_rditbl[READ_SELF_DIAG], 0);


static struct resinfo ea8076_restbl[MAX_RESTBL] = {
	[RES_ID] = RESINFO_INIT(id, EA8076_ID, RESUI(id)),
	[RES_COORDINATE] = RESINFO_INIT(coordinate, EA8076_COORDINATE, RESUI(coordinate)),
	[RES_CODE] = RESINFO_INIT(code, EA8076_CODE, RESUI(code)),
	[RES_ELVSS] = RESINFO_INIT(elvss, EA8076_ELVSS, RESUI(elvss)),
	[RES_MTP] = RESINFO_INIT(mtp, EA8076_MTP, RESUI(mtp)),
	[RES_DATE] = RESINFO_INIT(date, EA8076_DATE, RESUI(date)),
	[RES_OCTA_ID] = RESINFO_INIT(octa_id, EA8076_OCTA_ID, RESUI(octa_id)),
	[RES_AOR] = RESINFO_INIT(aor, EA8076_AOR, RESUI(aor)),
	[RES_VINT] = RESINFO_INIT(vint, EA8076_VINT, RESUI(vint)),
	[RES_ELVSS_T] = RESINFO_INIT(elvss_t, EA8076_ELVSS_T, RESUI(elvss_t)),
	[RES_RDDPM] = RESINFO_INIT(rddpm, EA8076_RDDPM, RESUI(rddpm)),
	[RES_RDDSM] = RESINFO_INIT(rddsm, EA8076_RDDSM, RESUI(rddsm)),
	[RES_ERR] = RESINFO_INIT(err, EA8076_ERR, RESUI(err)),
	[RES_ERR_FG] = RESINFO_INIT(err_fg, EA8076_ERR_FG, RESUI(err_fg)),
	[RES_DSI_ERR] = RESINFO_INIT(dsi_err, EA8076_DSI_ERR, RESUI(dsi_err)),
	[RES_SELF_DIAG] = RESINFO_INIT(self_diag, EA8076_SELF_DIAG, RESUI(self_diag)),
};

enum {
	DUMP_RDDPM = 0,
	DUMP_RDDSM,
	DUMP_ERR,
	DUMP_ERR_FG,
	DUMP_DSI_ERR,
	DUMP_SELF_DIAG,
	DUMP_SELF_MASK_CRC,
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	DUMP_CMDLOG,
#endif
};

static void show_rddpm(struct dumpinfo *info);
static void show_rddsm(struct dumpinfo *info);
static void show_err(struct dumpinfo *info);
static void show_err_fg(struct dumpinfo *info);
static void show_dsi_err(struct dumpinfo *info);
static void show_self_diag(struct dumpinfo *info);
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static void show_cmdlog(struct dumpinfo *info);
#endif
static void show_self_mask_crc(struct dumpinfo *info);

static struct dumpinfo ea8076_dmptbl[] = {
	[DUMP_RDDPM] = DUMPINFO_INIT(rddpm, &ea8076_restbl[RES_RDDPM], show_rddpm),
	[DUMP_RDDSM] = DUMPINFO_INIT(rddsm, &ea8076_restbl[RES_RDDSM], show_rddsm),
	[DUMP_ERR] = DUMPINFO_INIT(err, &ea8076_restbl[RES_ERR], show_err),
	[DUMP_ERR_FG] = DUMPINFO_INIT(err_fg, &ea8076_restbl[RES_ERR_FG], show_err_fg),
	[DUMP_DSI_ERR] = DUMPINFO_INIT(dsi_err, &ea8076_restbl[RES_DSI_ERR], show_dsi_err),
	[DUMP_SELF_DIAG] = DUMPINFO_INIT(self_diag, &ea8076_restbl[RES_SELF_DIAG], show_self_diag),
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[DUMP_CMDLOG] = DUMPINFO_INIT(cmdlog, &ea8076_restbl[RES_CMDLOG], show_cmdlog),
#endif
	[DUMP_SELF_MASK_CRC] = DUMPINFO_INIT(self_mask_crc, &ea8076_restbl[RES_SELF_MASK_CRC], show_self_mask_crc),
};

static int init_common_table(struct maptbl *tbl);
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static int getidx_common_maptbl(struct maptbl *tbl);
#endif
static int init_gamma_mode2_brt_table(struct maptbl *tbl);
static int getidx_gamma_mode2_brt_table(struct maptbl *tbl);
static int getidx_gm2_elvss_table(struct maptbl *tbl);
static int getidx_hbm_transition_table(struct maptbl *tbl);
static int getidx_acl_opr_table(struct maptbl *tbl);

static int init_lpm_brt_table(struct maptbl *tbl);
static int getidx_lpm_brt_table(struct maptbl *tbl);

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst);
#endif
static void copy_common_maptbl(struct maptbl *tbl, u8 *dst);
static void copy_elvss_otp_maptbl(struct maptbl *, u8 *);
static void copy_tset_maptbl(struct maptbl *tbl, u8 *dst);
#ifdef CONFIG_SUPPORT_XTALK_MODE
static int getidx_vgh_table(struct maptbl *tbl);
#endif
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static int getidx_fast_discharge_table(struct maptbl *tbl);
#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static int init_color_blind_table(struct maptbl *tbl);
static int getidx_mdnie_scenario_maptbl(struct maptbl *tbl);
static int getidx_mdnie_hdr_maptbl(struct maptbl *tbl);
static int init_mdnie_night_mode_table(struct maptbl *tbl);
static int getidx_mdnie_night_mode_maptbl(struct maptbl *tbl);
static int init_mdnie_color_lens_table(struct maptbl *tbl);
static int getidx_color_lens_maptbl(struct maptbl *tbl);
static int init_color_coordinate_table(struct maptbl *tbl);
static int init_sensor_rgb_table(struct maptbl *tbl);
static int getidx_adjust_ldu_maptbl(struct maptbl *tbl);
static int getidx_color_coordinate_maptbl(struct maptbl *tbl);
static void copy_color_coordinate_maptbl(struct maptbl *tbl, u8 *dst);
static void copy_scr_white_maptbl(struct maptbl *tbl, u8 *dst);
static void copy_adjust_ldu_maptbl(struct maptbl *tbl, u8 *dst);
static int getidx_mdnie_0_maptbl(struct pkt_update_info *pktui);
static int getidx_mdnie_1_maptbl(struct pkt_update_info *pktui);
static int getidx_mdnie_2_maptbl(struct pkt_update_info *pktui);
static int getidx_mdnie_3_maptbl(struct pkt_update_info *pktui);
static int getidx_mdnie_4_maptbl(struct pkt_update_info *pktui);
static int getidx_mdnie_5_maptbl(struct pkt_update_info *pktui);

static int getidx_mdnie_scr_white_maptbl(struct pkt_update_info *pktui);
static void update_current_scr_white(struct maptbl *tbl, u8 *dst);
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */
#endif /* __EA8076_H__ */
