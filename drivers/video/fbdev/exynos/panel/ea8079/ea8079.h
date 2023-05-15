/*
 * linux/drivers/video/fbdev/exynos/panel/ea8079/ea8079.h
 *
 * Header file for EA8079 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8079_H__
#define __EA8079_H__

#include <linux/types.h>
#include <linux/kernel.h>
#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "../panel_poc.h"
#endif

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => EA8079_XXX_OFS (0)
 * XXX 2nd param => EA8079_XXX_OFS (1)
 * XXX 36th param => EA8079_XXX_OFS (35)
 */

#define EA8079_GAMMA_CMD_CNT (35)

#define EA8079_ADDR_OFS	(0)
#define EA8079_ADDR_LEN	(1)
#define EA8079_DATA_OFS	(EA8079_ADDR_OFS + EA8079_ADDR_LEN)

#define EA8079_MTP_REG				0xC8
#define EA8079_MTP_OFS				0
#define EA8079_MTP_LEN				34

#define EA8079_DATE_REG			0xEA
#define EA8079_DATE_OFS			7
#define EA8079_DATE_LEN			(PANEL_DATE_LEN)

#define EA8079_COORDINATE_REG		0xEA
#define EA8079_COORDINATE_OFS		3
#define EA8079_COORDINATE_LEN		(PANEL_COORD_LEN)

#define EA8079_ID_REG				0x04
#define EA8079_ID_OFS				0
#define EA8079_ID_LEN				(PANEL_ID_LEN)

#define EA8079_CODE_REG			0xD1
#define EA8079_CODE_OFS			95
#define EA8079_CODE_LEN			6

#define EA8079_ELVSS_REG			0xB7
#define EA8079_ELVSS_OFS			7
#define EA8079_ELVSS_LEN			2

#define EA8079_OCTA_ID_REG			0xEA
#define EA8079_OCTA_ID_OFS			14
#define EA8079_OCTA_ID_LEN			(PANEL_OCTA_ID_LEN)

/* for brightness debugging */
#define EA8079_GAMMA_REG			0xCA
#define EA8079_GAMMA_OFS			0
#define EA8079_GAMMA_LEN			34

#define EA8079_AOR_REG			0xB1
#define EA8079_AOR_OFS			0
#define EA8079_AOR_LEN			2

#define EA8079_VINT_REG			0xF4
#define EA8079_VINT_OFS			4
#define EA8079_VINT_LEN			1

#define EA8079_ELVSS_T_REG			0xB5
#define EA8079_ELVSS_T_OFS			2
#define EA8079_ELVSS_T_LEN			1

/* for panel dump */
#define EA8079_RDDPM_REG			0x0A
#define EA8079_RDDPM_OFS			0
#define EA8079_RDDPM_LEN			(PANEL_RDDPM_LEN)

#define EA8079_RDDSM_REG			0x0E
#define EA8079_RDDSM_OFS			0
#define EA8079_RDDSM_LEN			(PANEL_RDDSM_LEN)

#define EA8079_ERR_REG				0xEA
#define EA8079_ERR_OFS				0
#define EA8079_ERR_LEN				5

#define EA8079_ERR_FG_REG			0xEE
#define EA8079_ERR_FG_OFS			0
#define EA8079_ERR_FG_LEN			1

#define EA8079_DSI_ERR_REG			0x05
#define EA8079_DSI_ERR_OFS			0
#define EA8079_DSI_ERR_LEN			1

#define EA8079_SELF_DIAG_REG			0x0F
#define EA8079_SELF_DIAG_OFS			0
#define EA8079_SELF_DIAG_LEN			1

#define EA8079_SELF_MASK_CHECKSUM_REG		0xFB
#define EA8079_SELF_MASK_CHECKSUM_OFS		15
#define EA8079_SELF_MASK_CHECKSUM_LEN		2

#define EA8079_SELF_MASK_CRC_REG		0x7F
#define EA8079_SELF_MASK_CRC_OFS	6
#define EA8079_SELF_MASK_CRC_LEN		4


#define EA8079_GAMMA_MTP_1SET_LEN			43
#define EA8079_GAMMA_MTP_SET_NUM			6

#define EA8079_GAMMA_MTP_ALL_LEN		(EA8079_GAMMA_MTP_1SET_LEN * EA8079_GAMMA_MTP_SET_NUM)

#define EA8079_GAMMA_MTP_120_READ_0_REG		0xB8
#define EA8079_GAMMA_MTP_120_READ_0_OFS		0xE6
#define EA8079_GAMMA_MTP_120_READ_0_LEN		1

#define EA8079_GAMMA_MTP_120_READ_1_REG		0xB9
#define EA8079_GAMMA_MTP_120_READ_1_OFS		0x0
#define EA8079_GAMMA_MTP_120_READ_1_LEN		237

#define EA8079_GAMMA_MTP_120_READ_2_REG		0xBA
#define EA8079_GAMMA_MTP_120_READ_2_OFS		0x0
#define EA8079_GAMMA_MTP_120_READ_2_LEN		20

#define EA8079_GAMMA_MTP_60_READ_0_REG		0xB7
#define EA8079_GAMMA_MTP_60_READ_0_OFS		0x81
#define EA8079_GAMMA_MTP_60_READ_0_LEN		114

#define EA8079_GAMMA_MTP_60_READ_1_REG		0xB8
#define EA8079_GAMMA_MTP_60_READ_1_OFS		0x0
#define EA8079_GAMMA_MTP_60_READ_1_LEN		144

#define EA8079_GAMMA_MTP_0_REG		0xB7
#define EA8079_GAMMA_MTP_0_OFS		0x81
#define EA8079_GAMMA_MTP_0_LEN		114

#define EA8079_GAMMA_MTP_1_REG		0xB8
#define EA8079_GAMMA_MTP_1_OFS		0x0
#define EA8079_GAMMA_MTP_1_LEN		145

#define EA8079_GAMMA_MTP_2_REG		0xB9
#define EA8079_GAMMA_MTP_2_OFS		0x0
#define EA8079_GAMMA_MTP_2_LEN		237

#define EA8079_GAMMA_MTP_3_REG		0xBA
#define EA8079_GAMMA_MTP_3_OFS		0x0
#define EA8079_GAMMA_MTP_3_LEN		20

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
#define EA8079_CMDLOG_REG			0x9C
#define EA8079_CMDLOG_OFS			0
#define EA8079_CMDLOG_LEN			0x80
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#define NR_EA8079_MDNIE_REG	(6)

#define EA8079_MDNIE_0_REG		(0xB1)
#define EA8079_MDNIE_0_OFS		(0)
#define EA8079_MDNIE_0_LEN		(132)

#define EA8079_MDNIE_1_REG		(0x81)
#define EA8079_MDNIE_1_OFS		(EA8079_MDNIE_0_OFS + EA8079_MDNIE_0_LEN)
#define EA8079_MDNIE_1_LEN		(1)

#define EA8079_MDNIE_2_REG		(0xB2)
#define EA8079_MDNIE_2_OFS		(EA8079_MDNIE_1_OFS + EA8079_MDNIE_1_LEN)
#define EA8079_MDNIE_2_LEN		(130)

#define EA8079_MDNIE_3_REG		(0x87)
#define EA8079_MDNIE_3_OFS		(EA8079_MDNIE_2_OFS + EA8079_MDNIE_2_LEN)
#define EA8079_MDNIE_3_LEN		(2)

#define EA8079_MDNIE_4_REG		(0x85)
#define EA8079_MDNIE_4_OFS		(EA8079_MDNIE_3_OFS + EA8079_MDNIE_3_LEN)
#define EA8079_MDNIE_4_LEN		(1)

#define EA8079_MDNIE_5_REG		(0x83)
#define EA8079_MDNIE_5_OFS		(EA8079_MDNIE_4_OFS + EA8079_MDNIE_4_LEN)
#define EA8079_MDNIE_5_LEN		(1)
#define EA8079_MDNIE_LEN		(EA8079_MDNIE_0_LEN + EA8079_MDNIE_1_LEN + EA8079_MDNIE_2_LEN + EA8079_MDNIE_3_LEN + EA8079_MDNIE_4_LEN + EA8079_MDNIE_5_LEN)

#ifdef CONFIG_SUPPORT_AFC
#define EA8079_AFC_REG			(0xE2)
#define EA8079_AFC_OFS			(0)
#define EA8079_AFC_LEN			(70)
#define EA8079_AFC_ROI_OFS		(55)
#define EA8079_AFC_ROI_LEN		(12)
#endif

#define EA8079_SCR_CR_OFS	(1)
#define EA8079_SCR_WR_OFS	(19)
#define EA8079_SCR_WG_OFS	(20)
#define EA8079_SCR_WB_OFS	(21)
#define EA8079_NIGHT_MODE_OFS	(EA8079_SCR_CR_OFS)
#define EA8079_NIGHT_MODE_LEN	(21)
#define EA8079_COLOR_LENS_OFS	(EA8079_SCR_CR_OFS)
#define EA8079_COLOR_LENS_LEN	(21)
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */


#ifdef CONFIG_DYNAMIC_FREQ
#define EA8079_MAX_MIPI_FREQ			3
#define EA8079_DEFAULT_MIPI_FREQ		0
enum {
	EA8079_OSC_DEFAULT,
	MAX_EA8079_OSC,
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
	ACL_DIM_SPEED_MAPTBL,
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
	FPS_PORCH_MAPTBL,
	FPS_PORCH_SETTING_MAPTBL,
	FPS_FREQ_MAPTBL,
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
	DYN_FFC_1_MAPTBL,
	DYN_FFC_2_MAPTBL,
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
	GAMMA_MTP_0_HS_MAPTBL,
	GAMMA_MTP_1_HS_MAPTBL,
	GAMMA_MTP_2_HS_MAPTBL,
	GAMMA_MTP_3_HS_MAPTBL,
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
	READ_GAMMA_MTP_120_0,
	READ_GAMMA_MTP_120_1,
	READ_GAMMA_MTP_120_2,
	READ_GAMMA_MTP_60_0,
	READ_GAMMA_MTP_60_1,
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
	RES_GAMMA_MTP_120,
	RES_GAMMA_MTP_60,
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


enum {
	EA8079_VRR_FPS_120,
	EA8079_VRR_FPS_96,
	EA8079_VRR_FPS_60,
	MAX_EA8079_VRR_FPS,
};

static u8 EA8079_ID[EA8079_ID_LEN];
static u8 EA8079_COORDINATE[EA8079_COORDINATE_LEN];
static u8 EA8079_CODE[EA8079_CODE_LEN];
static u8 EA8079_ELVSS[EA8079_ELVSS_LEN];
static u8 EA8079_MTP[EA8079_MTP_LEN];
static u8 EA8079_DATE[EA8079_DATE_LEN];
static u8 EA8079_OCTA_ID[EA8079_OCTA_ID_LEN];
/* for brightness debugging */
static u8 EA8079_AOR[EA8079_AOR_LEN];
static u8 EA8079_VINT[EA8079_VINT_LEN];
static u8 EA8079_ELVSS_T[EA8079_ELVSS_T_LEN];
static u8 EA8079_RDDPM[EA8079_RDDPM_LEN];
static u8 EA8079_RDDSM[EA8079_RDDSM_LEN];
static u8 EA8079_ERR[EA8079_ERR_LEN];
static u8 EA8079_ERR_FG[EA8079_ERR_FG_LEN];
static u8 EA8079_DSI_ERR[EA8079_DSI_ERR_LEN];
static u8 EA8079_SELF_DIAG[EA8079_SELF_DIAG_LEN];
static u8 EA8079_GAMMA_MTP_120[EA8079_GAMMA_MTP_ALL_LEN];
static u8 EA8079_GAMMA_MTP_60[EA8079_GAMMA_MTP_ALL_LEN];


static struct rdinfo ea8079_rditbl[MAX_READTBL] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, EA8079_ID_REG, EA8079_ID_OFS, EA8079_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, EA8079_COORDINATE_REG, EA8079_COORDINATE_OFS, EA8079_COORDINATE_LEN),
	[READ_CODE] = RDINFO_INIT(code, DSI_PKT_TYPE_RD, EA8079_CODE_REG, EA8079_CODE_OFS, EA8079_CODE_LEN),
	[READ_ELVSS] = RDINFO_INIT(elvss, DSI_PKT_TYPE_RD, EA8079_ELVSS_REG, EA8079_ELVSS_OFS, EA8079_ELVSS_LEN),
	[READ_MTP] = RDINFO_INIT(mtp, DSI_PKT_TYPE_RD, EA8079_MTP_REG, EA8079_MTP_OFS, EA8079_MTP_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, EA8079_DATE_REG, EA8079_DATE_OFS, EA8079_DATE_LEN),
	[READ_OCTA_ID] = RDINFO_INIT(octa_id, DSI_PKT_TYPE_RD, EA8079_OCTA_ID_REG, EA8079_OCTA_ID_OFS, EA8079_OCTA_ID_LEN),
	/* for brightness debugging */
	[READ_AOR] = RDINFO_INIT(aor, DSI_PKT_TYPE_RD, EA8079_AOR_REG, EA8079_AOR_OFS, EA8079_AOR_LEN),
	[READ_VINT] = RDINFO_INIT(vint, DSI_PKT_TYPE_RD, EA8079_VINT_REG, EA8079_VINT_OFS, EA8079_VINT_LEN),
	[READ_ELVSS_T] = RDINFO_INIT(elvss_t, DSI_PKT_TYPE_RD, EA8079_ELVSS_T_REG, EA8079_ELVSS_T_OFS, EA8079_ELVSS_T_LEN),
	[READ_RDDPM] = RDINFO_INIT(rddpm, DSI_PKT_TYPE_RD, EA8079_RDDPM_REG, EA8079_RDDPM_OFS, EA8079_RDDPM_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, EA8079_RDDSM_REG, EA8079_RDDSM_OFS, EA8079_RDDSM_LEN),
	[READ_ERR] = RDINFO_INIT(err, DSI_PKT_TYPE_RD, EA8079_ERR_REG, EA8079_ERR_OFS, EA8079_ERR_LEN),
	[READ_ERR_FG] = RDINFO_INIT(err_fg, DSI_PKT_TYPE_RD, EA8079_ERR_FG_REG, EA8079_ERR_FG_OFS, EA8079_ERR_FG_LEN),
	[READ_DSI_ERR] = RDINFO_INIT(dsi_err, DSI_PKT_TYPE_RD, EA8079_DSI_ERR_REG, EA8079_DSI_ERR_OFS, EA8079_DSI_ERR_LEN),
	[READ_SELF_DIAG] = RDINFO_INIT(self_diag, DSI_PKT_TYPE_RD, EA8079_SELF_DIAG_REG, EA8079_SELF_DIAG_OFS, EA8079_SELF_DIAG_LEN),
	[READ_GAMMA_MTP_120_0] = RDINFO_INIT(gamma_mtp_120, DSI_PKT_TYPE_RD, EA8079_GAMMA_MTP_120_READ_0_REG,
		EA8079_GAMMA_MTP_120_READ_0_OFS, EA8079_GAMMA_MTP_120_READ_0_LEN),
	[READ_GAMMA_MTP_120_1] = RDINFO_INIT(gamma_mtp_120, DSI_PKT_TYPE_RD, EA8079_GAMMA_MTP_120_READ_1_REG,
		EA8079_GAMMA_MTP_120_READ_1_OFS, EA8079_GAMMA_MTP_120_READ_1_LEN),
	[READ_GAMMA_MTP_120_2] = RDINFO_INIT(gamma_mtp_120, DSI_PKT_TYPE_RD, EA8079_GAMMA_MTP_120_READ_2_REG,
		EA8079_GAMMA_MTP_120_READ_2_OFS, EA8079_GAMMA_MTP_120_READ_2_LEN),
	[READ_GAMMA_MTP_60_0] = RDINFO_INIT(gamma_mtp_60, DSI_PKT_TYPE_RD, EA8079_GAMMA_MTP_60_READ_0_REG,
		EA8079_GAMMA_MTP_60_READ_0_OFS, EA8079_GAMMA_MTP_60_READ_0_LEN),
	[READ_GAMMA_MTP_60_1] = RDINFO_INIT(gamma_mtp_60, DSI_PKT_TYPE_RD, EA8079_GAMMA_MTP_60_READ_1_REG,
		EA8079_GAMMA_MTP_60_READ_1_OFS, EA8079_GAMMA_MTP_60_READ_1_LEN),
};

static DEFINE_RESUI(id, &ea8079_rditbl[READ_ID], 0);
static DEFINE_RESUI(coordinate, &ea8079_rditbl[READ_COORDINATE], 0);
static DEFINE_RESUI(code, &ea8079_rditbl[READ_CODE], 0);
static DEFINE_RESUI(elvss, &ea8079_rditbl[READ_ELVSS], 0);
static DEFINE_RESUI(mtp, &ea8079_rditbl[READ_MTP], 0);
static DEFINE_RESUI(date, &ea8079_rditbl[READ_DATE], 0);
static DEFINE_RESUI(octa_id, &ea8079_rditbl[READ_OCTA_ID], 0);
/* for brightness debugging */
static DEFINE_RESUI(aor, &ea8079_rditbl[READ_AOR], 0);
static DEFINE_RESUI(vint, &ea8079_rditbl[READ_VINT], 0);
static DEFINE_RESUI(elvss_t, &ea8079_rditbl[READ_ELVSS_T], 0);
static DEFINE_RESUI(rddpm, &ea8079_rditbl[READ_RDDPM], 0);
static DEFINE_RESUI(rddsm, &ea8079_rditbl[READ_RDDSM], 0);
static DEFINE_RESUI(err, &ea8079_rditbl[READ_ERR], 0);
static DEFINE_RESUI(err_fg, &ea8079_rditbl[READ_ERR_FG], 0);
static DEFINE_RESUI(dsi_err, &ea8079_rditbl[READ_DSI_ERR], 0);
static DEFINE_RESUI(self_diag, &ea8079_rditbl[READ_SELF_DIAG], 0);


struct res_update_info RESUI(gamma_mtp_120)[] = {
	RESUI_INIT(&ea8079_rditbl[READ_GAMMA_MTP_120_0], 0),
	RESUI_INIT(&ea8079_rditbl[READ_GAMMA_MTP_120_1], EA8079_GAMMA_MTP_120_READ_0_LEN),
	RESUI_INIT(&ea8079_rditbl[READ_GAMMA_MTP_120_2],
			EA8079_GAMMA_MTP_120_READ_0_LEN + EA8079_GAMMA_MTP_120_READ_1_LEN),
};

struct res_update_info RESUI(gamma_mtp_60)[] = {
	RESUI_INIT(&ea8079_rditbl[READ_GAMMA_MTP_60_0], 0),
	RESUI_INIT(&ea8079_rditbl[READ_GAMMA_MTP_60_1], EA8079_GAMMA_MTP_60_READ_0_LEN),
};

static struct resinfo ea8079_restbl[MAX_RESTBL] = {
	[RES_ID] = RESINFO_INIT(id, EA8079_ID, RESUI(id)),
	[RES_COORDINATE] = RESINFO_INIT(coordinate, EA8079_COORDINATE, RESUI(coordinate)),
	[RES_CODE] = RESINFO_INIT(code, EA8079_CODE, RESUI(code)),
	[RES_ELVSS] = RESINFO_INIT(elvss, EA8079_ELVSS, RESUI(elvss)),
	[RES_MTP] = RESINFO_INIT(mtp, EA8079_MTP, RESUI(mtp)),
	[RES_DATE] = RESINFO_INIT(date, EA8079_DATE, RESUI(date)),
	[RES_OCTA_ID] = RESINFO_INIT(octa_id, EA8079_OCTA_ID, RESUI(octa_id)),
	[RES_AOR] = RESINFO_INIT(aor, EA8079_AOR, RESUI(aor)),
	[RES_VINT] = RESINFO_INIT(vint, EA8079_VINT, RESUI(vint)),
	[RES_ELVSS_T] = RESINFO_INIT(elvss_t, EA8079_ELVSS_T, RESUI(elvss_t)),
	[RES_RDDPM] = RESINFO_INIT(rddpm, EA8079_RDDPM, RESUI(rddpm)),
	[RES_RDDSM] = RESINFO_INIT(rddsm, EA8079_RDDSM, RESUI(rddsm)),
	[RES_ERR] = RESINFO_INIT(err, EA8079_ERR, RESUI(err)),
	[RES_ERR_FG] = RESINFO_INIT(err_fg, EA8079_ERR_FG, RESUI(err_fg)),
	[RES_DSI_ERR] = RESINFO_INIT(dsi_err, EA8079_DSI_ERR, RESUI(dsi_err)),
	[RES_SELF_DIAG] = RESINFO_INIT(self_diag, EA8079_SELF_DIAG, RESUI(self_diag)),
	[RES_GAMMA_MTP_120] = RESINFO_INIT(gamma_mtp_120, EA8079_GAMMA_MTP_120, RESUI(gamma_mtp_120)),
	[RES_GAMMA_MTP_60] = RESINFO_INIT(gamma_mtp_60, EA8079_GAMMA_MTP_60, RESUI(gamma_mtp_60)),
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

static struct dumpinfo ea8079_dmptbl[] = {
	[DUMP_RDDPM] = DUMPINFO_INIT(rddpm, &ea8079_restbl[RES_RDDPM], show_rddpm),
	[DUMP_RDDSM] = DUMPINFO_INIT(rddsm, &ea8079_restbl[RES_RDDSM], show_rddsm),
	[DUMP_ERR] = DUMPINFO_INIT(err, &ea8079_restbl[RES_ERR], show_err),
	[DUMP_ERR_FG] = DUMPINFO_INIT(err_fg, &ea8079_restbl[RES_ERR_FG], show_err_fg),
	[DUMP_DSI_ERR] = DUMPINFO_INIT(dsi_err, &ea8079_restbl[RES_DSI_ERR], show_dsi_err),
	[DUMP_SELF_DIAG] = DUMPINFO_INIT(self_diag, &ea8079_restbl[RES_SELF_DIAG], show_self_diag),
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[DUMP_CMDLOG] = DUMPINFO_INIT(cmdlog, &ea8079_restbl[RES_CMDLOG], show_cmdlog),
#endif
	[DUMP_SELF_MASK_CRC] = DUMPINFO_INIT(self_mask_crc, &ea8079_restbl[RES_SELF_MASK_CRC], show_self_mask_crc),
};

/* Variable Refresh Rate */
enum {
	EA8079_VRR_MODE_NS,
	EA8079_VRR_MODE_HS,
	MAX_EA8079_VRR_MODE,
};

enum {
	EA8079_VRR_120HS,
	EA8079_VRR_96HS,
	EA8079_VRR_60HS,
	MAX_EA8079_VRR,
};

enum {
	EA8079_RESOL_1080x2400,
};

enum {
	EA8079_DISPLAY_MODE_1080x2400_120HS,
	EA8079_DISPLAY_MODE_1080x2400_96HS,
	EA8079_DISPLAY_MODE_1080x2400_60HS,
	MAX_EA8079_DISPLAY_MODE,
};

enum {
	EA8079_VRR_KEY_REFRESH_RATE,
	EA8079_VRR_KEY_REFRESH_MODE,
	EA8079_VRR_KEY_TE_SW_SKIP_COUNT,
	EA8079_VRR_KEY_TE_HW_SKIP_COUNT,
	MAX_EA8079_VRR_KEY,
};

enum {
	EA8079_GAMMA_MTP_0,
	EA8079_GAMMA_MTP_1,
	EA8079_GAMMA_MTP_2,
	EA8079_GAMMA_MTP_3,
	EA8079_GAMMA_MTP_4,
	EA8079_GAMMA_MTP_5,
	MAX_EA8079_GAMMA_MTP,
};

enum {
	EA8079_GAMMA_BR_INDEX_0,
	EA8079_GAMMA_BR_INDEX_1,
	EA8079_GAMMA_BR_INDEX_2,
	EA8079_GAMMA_BR_INDEX_3,
	MAX_EA8079_GAMMA_BR_INDEX,
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
static int getidx_hbm_onoff_table(struct maptbl *);
static int getidx_acl_onoff_table(struct maptbl *);
static int getidx_acl_dim_onoff_table(struct maptbl *tbl);

static int init_lpm_brt_table(struct maptbl *tbl);
static int getidx_lpm_brt_table(struct maptbl *tbl);

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst);
#endif
static void copy_common_maptbl(struct maptbl *tbl, u8 *dst);
static void copy_tset_maptbl(struct maptbl *tbl, u8 *dst);
#ifdef CONFIG_SUPPORT_XTALK_MODE
static int getidx_vgh_table(struct maptbl *tbl);
#endif
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static int getidx_fast_discharge_table(struct maptbl *tbl);
#endif
static int getidx_vrr_fps_table(struct maptbl *);
#if defined(__PANEL_NOT_USED_VARIABLE__)
static int getidx_vrr_gamma_table(struct maptbl *);
#endif
static int getidx_lpm_mode_table(struct maptbl *);
static bool ea8079_is_120hz(struct panel_device *panel);

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

#if defined(__PANEL_NOT_USED_VARIABLE__)
static int init_gamma_mtp_all_table(struct maptbl *tbl);
#endif

static int getidx_mdnie_scr_white_maptbl(struct pkt_update_info *pktui);
static void update_current_scr_white(struct maptbl *tbl, u8 *dst);
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */
#ifdef CONFIG_DYNAMIC_FREQ
static int getidx_dyn_ffc_table(struct maptbl *tbl);
#endif
static bool is_panel_state_not_lpm(struct panel_device *panel);
#endif /* __EA8079_H__ */
