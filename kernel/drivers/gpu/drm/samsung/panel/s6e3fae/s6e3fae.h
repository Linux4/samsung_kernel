/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fae/s6e3fae.h
 *
 * Header file for S6E3FAE Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAE_H__
#define __S6E3FAE_H__

#include <linux/types.h>
#include <linux/kernel.h>
#include "../panel.h"
#include "../maptbl.h"
#include "../panel_function.h"
#include "oled_function.h"

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => S6E3FAE_XXX_OFS (0)
 * XXX 2nd param => S6E3FAE_XXX_OFS (1)
 * XXX 36th param => S6E3FAE_XXX_OFS (35)
 */

#define AID_INTERPOLATION
#define S6E3FAE_GAMMA_CMD_CNT (35)

#define S6E3FAE_IRC_ONOFF_OFS	(0)
#define S6E3FAE_IRC_VALUE_OFS	(10)
#define S6E3FAE_IRC_VALUE_LEN	(33)

#define S6E3FAE_ADDR_OFS	(0)
#define S6E3FAE_ADDR_LEN	(1)
#define S6E3FAE_DATA_OFS	(S6E3FAE_ADDR_OFS + S6E3FAE_ADDR_LEN)

#define S6E3FAE_DATE_REG			0xA1
#define S6E3FAE_DATE_OFS			4
#define S6E3FAE_DATE_LEN			(PANEL_DATE_LEN)

#define S6E3FAE_COORDINATE_REG		0xA1
#define S6E3FAE_COORDINATE_OFS		0
#define S6E3FAE_COORDINATE_LEN		(PANEL_COORD_LEN)

#define S6E3FAE_ID_REG				0x04
#define S6E3FAE_ID_OFS				0
#define S6E3FAE_ID_LEN				(PANEL_ID_LEN)

#define S6E3FAE_CODE_REG			0xD6
#define S6E3FAE_CODE_OFS			0
#define S6E3FAE_CODE_LEN			(PANEL_CODE_LEN)

#define S6E3FAE_ELVSS_REG			0xB5
#define S6E3FAE_ELVSS_OFS			0
#define S6E3FAE_ELVSS_LEN			23

#define S6E3FAE_ELVSS_CAL_OFFSET_REG			0x94
#define S6E3FAE_ELVSS_CAL_OFFSET_OFS			0x18
#define S6E3FAE_ELVSS_CAL_OFFSET_LEN			1

#define S6E3FAE_ELVSS_TEMP_0_REG		0xB5
#define S6E3FAE_ELVSS_TEMP_0_OFS		74
#define S6E3FAE_ELVSS_TEMP_0_LEN		1

#define S6E3FAE_ELVSS_TEMP_1_REG		0xB5
#define S6E3FAE_ELVSS_TEMP_1_OFS		75
#define S6E3FAE_ELVSS_TEMP_1_LEN		1

#define S6E3FAE_GAMMA_INTER_REG			0xB1
#define S6E3FAE_GAMMA_INTER_OFS			0x5D
#define S6E3FAE_GAMMA_INTER_LEN			6

#define S6E3FAE_OCTA_ID_REG			0xA1
#define S6E3FAE_OCTA_ID_OFS			11
#define S6E3FAE_OCTA_ID_LEN			(PANEL_OCTA_ID_LEN)

#define S6E3FAE_COPR_SPI_REG			0x5A
#define S6E3FAE_COPR_SPI_OFS			0
#define S6E3FAE_COPR_SPI_LEN			(41)

#define S6E3FAE_COPR_DSI_REG			0x5A
#define S6E3FAE_COPR_DSI_OFS			0
#define S6E3FAE_COPR_DSI_LEN			(41)

#define S6E3FAE_COPR_CTRL_REG			0xE1
#define S6E3FAE_COPR_CTRL_OFS			0
#define S6E3FAE_COPR_CTRL_LEN			(COPR_V6_CTRL_REG_SIZE)

#define S6E3FAE_CHIP_ID_REG			0xD6
#define S6E3FAE_CHIP_ID_OFS			0
#define S6E3FAE_CHIP_ID_LEN			5

/* for brightness debugging */
#define S6E3FAE_GAMMA_REG			0xCA
#define S6E3FAE_GAMMA_OFS			0
#define S6E3FAE_GAMMA_LEN			34

#define S6E3FAE_AOR_REG			0xB1
#define S6E3FAE_AOR_OFS			0
#define S6E3FAE_AOR_LEN			2

#define S6E3FAE_VINT_REG			0xF4
#define S6E3FAE_VINT_OFS			4
#define S6E3FAE_VINT_LEN			1

#define S6E3FAE_ELVSS_T_REG			0xB5
#define S6E3FAE_ELVSS_T_OFS			2
#define S6E3FAE_ELVSS_T_LEN			1

#define S6E3FAE_FLASH_LOADED_1_REG	0xAB
#define S6E3FAE_FLASH_LOADED_1_OFS	0x11
#define S6E3FAE_FLASH_LOADED_1_LEN	1

#define S6E3FAE_FLASH_LOADED_2_REG	0x63
#define S6E3FAE_FLASH_LOADED_2_OFS	0xFF
#define S6E3FAE_FLASH_LOADED_2_LEN	1

#define S6E3FAE_FLASH_LOADED_LEN	(S6E3FAE_FLASH_LOADED_1_LEN + S6E3FAE_FLASH_LOADED_2_LEN)

#define S6E3FAE_IRC_REG			(0x92)
#define S6E3FAE_IRC_OFS			(S6E3FAE_IRC_VALUE_OFS)
#define S6E3FAE_IRC_LEN			(S6E3FAE_IRC_VALUE_LEN)

/* for panel dump */
#define S6E3FAE_RDDPM_REG			0x0A
#define S6E3FAE_RDDPM_OFS			0
#define S6E3FAE_RDDPM_LEN			(PANEL_RDDPM_LEN)

#define S6E3FAE_RDDPM_MASK (0x9C)
#define S6E3FAE_RDDPM_AFTER_DISPLAY_ON_VALUE (0x9C)
#define S6E3FAE_RDDPM_BEFORE_SLEEP_IN_VALUE (0x98)

#define S6E3FAE_RDDSM_REG			0x0E
#define S6E3FAE_RDDSM_OFS			0
#define S6E3FAE_RDDSM_LEN			(PANEL_RDDSM_LEN)

#define S6E3FAE_RDDSM_MASK (0x80)
#define S6E3FAE_RDDSM_VALUE (0x80)

#define S6E3FAE_ERR_REG				0xE9
#define S6E3FAE_ERR_OFS				0
#define S6E3FAE_ERR_LEN				5

#define S6E3FAE_ERR_FG_REG			0xEE
#define S6E3FAE_ERR_FG_OFS			0
#define S6E3FAE_ERR_FG_LEN			1

#define S6E3FAE_ERR_FG_MASK (0x4C)
#define S6E3FAE_ERR_FG_VALUE (0x00)

#define S6E3FAE_DSI_ERR_REG			0x05
#define S6E3FAE_DSI_ERR_OFS			0
#define S6E3FAE_DSI_ERR_LEN			1

#define S6E3FAE_SELF_DIAG_REG			0x0F
#define S6E3FAE_SELF_DIAG_OFS			0
#define S6E3FAE_SELF_DIAG_LEN			1

#define S6E3FAE_SELF_MASK_CRC_REG		0x14
#define S6E3FAE_SELF_MASK_CRC_OFS		0
#define S6E3FAE_SELF_MASK_CRC_LEN		2

#define S6E3FAE_SELF_MASK_CHECKSUM_REG		0x7F
#define S6E3FAE_SELF_MASK_CHECKSUM_OFS		0
#define S6E3FAE_SELF_MASK_CHECKSUM_LEN		4

#ifdef CONFIG_USDM_PANEL_MAFPC
#define S6E3FAE_MAFPC_REG				0x87
#define S6E3FAE_MAFPC_OFS				0
#define S6E3FAE_MAFPC_LEN				1

#define S6E3FAE_MAFPC_FLASH_REG				0xFE
#define S6E3FAE_MAFPC_FLASH_OFS				0x09
#define S6E3FAE_MAFPC_FLASH_LEN				1

#define S6E3FAE_MAFPC_CRC_REG				0x14
#define S6E3FAE_MAFPC_CRC_OFS				0
#define S6E3FAE_MAFPC_CRC_LEN				2
#endif

#ifdef CONFIG_USDM_FACTORY_GCT_TEST
#define S6E3FAE_GRAM_CHECKSUM_REG	0xBC
#define S6E3FAE_GRAM_CHECKSUM_OFS	0
#define S6E3FAE_GRAM_CHECKSUM_LEN	1
#define S6E3FAE_GRAM_CHECKSUM_VALID_1	0x00
#define S6E3FAE_GRAM_CHECKSUM_VALID_2	0x00
#endif

#ifdef CONFIG_USDM_DDI_CMDLOG
#define S6E3FAE_CMDLOG_REG			0x9C
#define S6E3FAE_CMDLOG_OFS			0
#define S6E3FAE_CMDLOG_LEN			0x80
#endif

#ifdef CONFIG_USDM_FACTORY_CCD_TEST
#define S6E3FAE_CCD_STATE_REG				0xCD
#define S6E3FAE_CCD_STATE_OFS				0
#define S6E3FAE_CCD_STATE_LEN				1
#endif

#define S6E3FAE_SSR_STATE_REG				0xF6
#define S6E3FAE_SSR_STATE_OFS				0x156
#define S6E3FAE_SSR_STATE_LEN				1

#define S6E3FAE_SSR_TEST_LEN				(S6E3FAE_SSR_STATE_LEN)

#define S6E3FAE_ECC_RECOVERED_REG				0xF8
#define S6E3FAE_ECC_RECOVERED_OFS				0x04
#define S6E3FAE_ECC_RECOVERED_LEN				1

#define S6E3FAE_ECC_NOT_RECOVERED_REG				0xF8
#define S6E3FAE_ECC_NOT_RECOVERED_OFS				0x0A
#define S6E3FAE_ECC_NOT_RECOVERED_LEN				1

#define S6E3FAE_ECC_TEST_LEN				(S6E3FAE_ECC_RECOVERED_LEN + S6E3FAE_ECC_NOT_RECOVERED_LEN)

#define S6E3FAE_DECODER_TEST_REG			0x14
#define S6E3FAE_DECODER_TEST_OFS			0
#define S6E3FAE_DECODER_TEST_LEN			2

#define S6E3FAE_DSC_CRC_VALUE_1 0xB1
#define S6E3FAE_DSC_CRC_VALUE_2 0x8F

#define S6E3FAE_NR_GLUT_POINT (32)
#define S6E3FAE_GLUT_LEN (144)

#define S6E3FAE_SSR_STATE_VALUE		0x00
#define S6E3FAE_SSR_STATE_MASK		0x0F

#define S6E3FAE_ECC_VALUE	0x00
#define S6E3FAE_ECC_MASK	0xFF

#define S6E3FAE_FLASH_LOADED_1_VALUE		0x0C
#define S6E3FAE_FLASH_LOADED_2_LSB_MASK		0x1F
#define S6E3FAE_FLASH_LOADED_2_LSB_VALUE	0x00
#define S6E3FAE_FLASH_LOADED_2_MSB_MASK		0x80
#define S6E3FAE_FLASH_LOADED_2_MSB_VALUE	0x00

enum s6e3fae_function {
	S6E3FAE_MAPTBL_INIT_GAMMA_MODE2_BRT,
	S6E3FAE_MAPTBL_INIT_LPM_BRT,
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	S6E3FAE_MAPTBL_INIT_GRAM_IMG_PATTERN,
#endif
#if defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
	S6E3FAE_MAPTBL_GETIDX_FAST_DISCHARGE,
#endif
#ifdef CONFIG_USDM_PANEL_MAFPC
	S6E3FAE_MAPTBL_COPY_MAFPC_CTRL,
	S6E3FAE_MAPTBL_COPY_MAFPC_SCALE,
	S6E3FAE_MAPTBL_COPY_MAFPC_IMG,
	S6E3FAE_MAPTBL_COPY_MAFPC_CRC_IMG,
#endif
	S6E3FAE_MAPTBL_COPY_ELVSS_CAL,
	S6E3FAE_MAPTBL_GETIDX_HBM_VRR,
	S6E3FAE_MAPTBL_GETIDX_VRR_MODE,
	S6E3FAE_MAPTBL_GETIDX_RESOLUTION,
	S6E3FAE_DO_GAMMA_FLASH_CHECKSUM,
#ifdef CONFIG_USDM_PANEL_HMD
	S6E3FAE_MAPTBL_INIT_GAMMA_MODE2_HMD_BRT,
#endif
	S6E3FAE_MAPTBL_GETIDX_FFC,
	MAX_S6E3FAE_FUNCTION,
};

extern struct pnobj_func s6e3fae_function_table[MAX_S6E3FAE_FUNCTION];

#undef DDI_FUNC
#define DDI_FUNC(_index) (s6e3fae_function_table[_index])

enum {
	S6E3FAE_VRR_TYPE_OSC_HIGH_SYNC_HIGH,
	S6E3FAE_VRR_TYPE_OSC_HIGH_SYNC_LOW,
	S6E3FAE_VRR_TYPE_OSC_LOW_SYNC_LOW,
	MAX_S6E3FAE_VRR_TYPE,
};

enum {
	GAMMA_MAPTBL,
	GAMMA_MODE2_MAPTBL,
	VBIAS_MAPTBL,
	AOR_MAPTBL,
	MPS_MAPTBL,
	TSET_MAPTBL,
	ELVSS_CAL_OFFSET_MAPTBL,
	ELVSS_TEMP_MAPTBL,
	SMOOTH_TRANSITION_FRAME_MAPTBL,
#ifdef CONFIG_SUPPORT_XTALK_MODE
	VGH_MAPTBL,
#endif
	VINT_MAPTBL,
	VINT_VRR_120HZ_MAPTBL,
	ACL_OPR_MAPTBL,
	IRC_MODE_MAPTBL,
#ifdef CONFIG_USDM_PANEL_HMD
	/* HMD MAPTBL */
	HMD_GAMMA_MAPTBL,
	HMD_AOR_MAPTBL,
	HMD_ELVSS_MAPTBL,
#endif /* CONFIG_USDM_PANEL_HMD */
	DSC_MAPTBL,
	PPS_MAPTBL,
	SCALER_MAPTBL,
	CASET_MAPTBL,
	PASET_MAPTBL,
	OSC_MAPTBL,
	TSP_SYNC_MAPTBL,
	TE_SET_1_MAPTBL,
	TE_SET_2_MAPTBL,
	VFP_TH_MAPTBL,
	LFD_MIN_MAPTBL,
	LFD_MAX_MAPTBL,
	LFD_FI_FREQ_MAPTBL,
	LFD_FI_NUMBER_MAPTBL,
	LPM_ON_MAPTBL,
	LPM_WRDISBV_MAPTBL,
	LPM_OFF_MAPTBL,
	LPM_AOR_OFF_MAPTBL,
	LPM_LFD_MIN_MAPTBL,
	LPM_LFD_MAX_MAPTBL,
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	VDDM_MAPTBL,
	GRAM_IMG_MAPTBL,
	GRAM_INV_IMG_MAPTBL,
#endif
	FFC_MAPTBL,
	SMOOTH_DIM_MAPTBL,
	SMOOTH_SYNC_MAPTBL,
#ifdef CONFIG_USDM_PANEL_MAFPC
	MAFPC_ENA_MAPTBL,
	MAFPC_CTRL_MAPTBL,
	MAFPC_SCALE_MAPTBL,
	MAFPC_IMG_MAPTBL,
	MAFPC_CRC_IMG_MAPTBL,
#endif
	DIA_ONOFF_MAPTBL,
#if defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
	FAST_DISCHARGE_MAPTBL,
#endif
	MAX_MAPTBL,
};

enum {
#ifdef CONFIG_USDM_PANEL_COPR
	READ_COPR_SPI,
	READ_COPR_DSI,
#endif
	READ_ID,
	READ_COORDINATE,
	READ_CODE,
	READ_ELVSS,
	READ_ELVSS_CAL_OFFSET,
	READ_ELVSS_TEMP_0,
	READ_ELVSS_TEMP_1,
	READ_DATE,
	READ_OCTA_ID,
	READ_CHIP_ID,
	/* for brightness debugging */
	READ_AOR,
	READ_VINT,
	READ_ELVSS_T,
	READ_FLASH_LOADED_1,
	READ_FLASH_LOADED_2,
	READ_IRC,

	READ_RDDPM,
	READ_RDDSM,
	READ_ERR,
	READ_ERR_FG,
	READ_DSI_ERR,
	READ_SELF_DIAG,
	READ_SELF_MASK_CRC,
	READ_SELF_MASK_CHECKSUM,
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	READ_GRAM_CHECKSUM,
#endif
#ifdef CONFIG_USDM_DDI_CMDLOG
	READ_CMDLOG,
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	READ_CCD_STATE,
#endif
	READ_SSR_STATE,
	READ_ECC_RECOVERED,
	READ_ECC_NOT_RECOVERED,
	READ_DECODER_TEST,
#ifdef CONFIG_USDM_PANEL_MAFPC
	READ_MAFPC,
	READ_MAFPC_FLASH,
	READ_MAFPC_CRC,
#endif
};

enum {
#ifdef CONFIG_USDM_PANEL_COPR
	RES_COPR_SPI,
	RES_COPR_DSI,
#endif
	RES_ID,
	RES_COORDINATE,
	RES_CODE,
	RES_ELVSS,
	RES_ELVSS_CAL_OFFSET,
	RES_ELVSS_TEMP_0,
	RES_ELVSS_TEMP_1,
	RES_DATE,
	RES_OCTA_ID,
	RES_CHIP_ID,
	/* for brightness debugging */
	RES_AOR,
	RES_VINT,
	RES_ELVSS_T,
	RES_FLASH_LOADED,
	RES_IRC,
	RES_RDDPM,
	RES_RDDSM,
	RES_ERR,
	RES_ERR_FG,
	RES_DSI_ERR,
	RES_SELF_DIAG,
#ifdef CONFIG_USDM_DDI_CMDLOG
	RES_CMDLOG,
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	RES_GRAM_CHECKSUM,
#endif
#ifdef CONFIG_USDM_PANEL_MAFPC
	RES_MAFPC,
	RES_MAFPC_FLASH,
	RES_MAFPC_CRC,
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	RES_CCD_STATE,
	RES_CCD_CHKSUM_PASS,
#endif
	RES_SSR_TEST,
	RES_ECC_TEST,
	RES_DECODER_TEST,
	RES_SELF_MASK_CRC,
	RES_SELF_MASK_CHECKSUM,
	MAX_S6E3FAE_RES,
};

static u8 S6E3FAE_ID[S6E3FAE_ID_LEN];
static u8 S6E3FAE_COORDINATE[S6E3FAE_COORDINATE_LEN];
static u8 S6E3FAE_CODE[S6E3FAE_CODE_LEN];
static u8 S6E3FAE_ELVSS[S6E3FAE_ELVSS_LEN];
static u8 S6E3FAE_ELVSS_CAL_OFFSET[S6E3FAE_ELVSS_CAL_OFFSET_LEN];
static u8 S6E3FAE_ELVSS_TEMP_0[S6E3FAE_ELVSS_TEMP_0_LEN];
static u8 S6E3FAE_ELVSS_TEMP_1[S6E3FAE_ELVSS_TEMP_1_LEN];
static u8 S6E3FAE_DATE[S6E3FAE_DATE_LEN];
static u8 S6E3FAE_OCTA_ID[S6E3FAE_OCTA_ID_LEN];
/* for brightness debugging */
static u8 S6E3FAE_AOR[S6E3FAE_AOR_LEN];
static u8 S6E3FAE_VINT[S6E3FAE_VINT_LEN];
static u8 S6E3FAE_ELVSS_T[S6E3FAE_ELVSS_T_LEN];
static u8 S6E3FAE_FLASH_LOADED[S6E3FAE_FLASH_LOADED_LEN];
static u8 S6E3FAE_IRC[S6E3FAE_IRC_LEN];

#ifdef CONFIG_USDM_PANEL_COPR
static u8 S6E3FAE_COPR_SPI[S6E3FAE_COPR_SPI_LEN];
static u8 S6E3FAE_COPR_DSI[S6E3FAE_COPR_DSI_LEN];
#endif

static u8 S6E3FAE_CHIP_ID[S6E3FAE_CHIP_ID_LEN];
static u8 S6E3FAE_RDDPM[S6E3FAE_RDDPM_LEN];
static u8 S6E3FAE_RDDSM[S6E3FAE_RDDSM_LEN];
static u8 S6E3FAE_ERR[S6E3FAE_ERR_LEN];
static u8 S6E3FAE_ERR_FG[S6E3FAE_ERR_FG_LEN];
static u8 S6E3FAE_DSI_ERR[S6E3FAE_DSI_ERR_LEN];
static u8 S6E3FAE_SELF_DIAG[S6E3FAE_SELF_DIAG_LEN];
#ifdef CONFIG_USDM_DDI_CMDLOG
static u8 S6E3FAE_CMDLOG[S6E3FAE_CMDLOG_LEN];
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
static u8 S6E3FAE_GRAM_CHECKSUM[S6E3FAE_GRAM_CHECKSUM_LEN];
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static u8 S6E3FAE_CCD_STATE[S6E3FAE_CCD_STATE_LEN];
static u8 S6E3FAE_CCD_CHKSUM_PASS[S6E3FAE_CCD_STATE_LEN] = { 0x00 };
#endif
static u8 S6E3FAE_SSR_TEST[S6E3FAE_SSR_TEST_LEN];
static u8 S6E3FAE_ECC_TEST[S6E3FAE_ECC_TEST_LEN];
static u8 S6E3FAE_DECODER_TEST[S6E3FAE_DECODER_TEST_LEN];
#ifdef CONFIG_USDM_PANEL_MAFPC
static u8 S6E3FAE_MAFPC[S6E3FAE_MAFPC_LEN];
static u8 S6E3FAE_MAFPC_FLASH[S6E3FAE_MAFPC_FLASH_LEN];
static u8 S6E3FAE_MAFPC_CRC[S6E3FAE_MAFPC_CRC_LEN];
#endif
static u8 S6E3FAE_SELF_MASK_CRC[S6E3FAE_SELF_MASK_CRC_LEN];
static u8 S6E3FAE_SELF_MASK_CHECKSUM[S6E3FAE_SELF_MASK_CHECKSUM_LEN];

static struct rdinfo s6e3fae_rditbl[] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, S6E3FAE_ID_REG, S6E3FAE_ID_OFS, S6E3FAE_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, S6E3FAE_COORDINATE_REG, S6E3FAE_COORDINATE_OFS, S6E3FAE_COORDINATE_LEN),
	[READ_CODE] = RDINFO_INIT(code, DSI_PKT_TYPE_RD, S6E3FAE_CODE_REG, S6E3FAE_CODE_OFS, S6E3FAE_CODE_LEN),
	[READ_ELVSS] = RDINFO_INIT(elvss, DSI_PKT_TYPE_RD, S6E3FAE_ELVSS_REG, S6E3FAE_ELVSS_OFS, S6E3FAE_ELVSS_LEN),
	[READ_ELVSS_CAL_OFFSET] = RDINFO_INIT(elvss_cal_offset, DSI_PKT_TYPE_RD, S6E3FAE_ELVSS_CAL_OFFSET_REG, S6E3FAE_ELVSS_CAL_OFFSET_OFS, S6E3FAE_ELVSS_CAL_OFFSET_LEN),
	[READ_ELVSS_TEMP_0] = RDINFO_INIT(elvss_temp_0, DSI_PKT_TYPE_RD, S6E3FAE_ELVSS_TEMP_0_REG, S6E3FAE_ELVSS_TEMP_0_OFS, S6E3FAE_ELVSS_TEMP_0_LEN),
	[READ_ELVSS_TEMP_1] = RDINFO_INIT(elvss_temp_1, DSI_PKT_TYPE_RD, S6E3FAE_ELVSS_TEMP_1_REG, S6E3FAE_ELVSS_TEMP_1_OFS, S6E3FAE_ELVSS_TEMP_1_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, S6E3FAE_DATE_REG, S6E3FAE_DATE_OFS, S6E3FAE_DATE_LEN),
	[READ_OCTA_ID] = RDINFO_INIT(octa_id, DSI_PKT_TYPE_RD, S6E3FAE_OCTA_ID_REG, S6E3FAE_OCTA_ID_OFS, S6E3FAE_OCTA_ID_LEN),
	/* for brightness debugging */
	[READ_AOR] = RDINFO_INIT(aor, DSI_PKT_TYPE_RD, S6E3FAE_AOR_REG, S6E3FAE_AOR_OFS, S6E3FAE_AOR_LEN),
	[READ_VINT] = RDINFO_INIT(vint, DSI_PKT_TYPE_RD, S6E3FAE_VINT_REG, S6E3FAE_VINT_OFS, S6E3FAE_VINT_LEN),
	[READ_ELVSS_T] = RDINFO_INIT(elvss_t, DSI_PKT_TYPE_RD, S6E3FAE_ELVSS_T_REG, S6E3FAE_ELVSS_T_OFS, S6E3FAE_ELVSS_T_LEN),
	[READ_FLASH_LOADED_1] = RDINFO_INIT(flash_loaded_1, DSI_PKT_TYPE_RD, S6E3FAE_FLASH_LOADED_1_REG, S6E3FAE_FLASH_LOADED_1_OFS, S6E3FAE_FLASH_LOADED_1_LEN),
	[READ_FLASH_LOADED_2] = RDINFO_INIT(flash_loaded_2, DSI_PKT_TYPE_RD, S6E3FAE_FLASH_LOADED_2_REG, S6E3FAE_FLASH_LOADED_2_OFS, S6E3FAE_FLASH_LOADED_2_LEN),
	[READ_IRC] = RDINFO_INIT(irc, DSI_PKT_TYPE_RD, S6E3FAE_IRC_REG, S6E3FAE_IRC_OFS, S6E3FAE_IRC_LEN),
#ifdef CONFIG_USDM_PANEL_COPR
	[READ_COPR_SPI] = RDINFO_INIT(copr_spi, SPI_PKT_TYPE_RD, S6E3FAE_COPR_SPI_REG, S6E3FAE_COPR_SPI_OFS, S6E3FAE_COPR_SPI_LEN),
	[READ_COPR_DSI] = RDINFO_INIT(copr_dsi, DSI_PKT_TYPE_RD, S6E3FAE_COPR_DSI_REG, S6E3FAE_COPR_DSI_OFS, S6E3FAE_COPR_DSI_LEN),
#endif
	[READ_CHIP_ID] = RDINFO_INIT(chip_id, DSI_PKT_TYPE_RD, S6E3FAE_CHIP_ID_REG, S6E3FAE_CHIP_ID_OFS, S6E3FAE_CHIP_ID_LEN),
	[READ_RDDPM] = RDINFO_INIT(rddpm, DSI_PKT_TYPE_RD, S6E3FAE_RDDPM_REG, S6E3FAE_RDDPM_OFS, S6E3FAE_RDDPM_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, S6E3FAE_RDDSM_REG, S6E3FAE_RDDSM_OFS, S6E3FAE_RDDSM_LEN),
	[READ_ERR] = RDINFO_INIT(err, DSI_PKT_TYPE_RD, S6E3FAE_ERR_REG, S6E3FAE_ERR_OFS, S6E3FAE_ERR_LEN),
	[READ_ERR_FG] = RDINFO_INIT(err_fg, DSI_PKT_TYPE_RD, S6E3FAE_ERR_FG_REG, S6E3FAE_ERR_FG_OFS, S6E3FAE_ERR_FG_LEN),
	[READ_DSI_ERR] = RDINFO_INIT(dsi_err, DSI_PKT_TYPE_RD, S6E3FAE_DSI_ERR_REG, S6E3FAE_DSI_ERR_OFS, S6E3FAE_DSI_ERR_LEN),
	[READ_SELF_DIAG] = RDINFO_INIT(self_diag, DSI_PKT_TYPE_RD, S6E3FAE_SELF_DIAG_REG, S6E3FAE_SELF_DIAG_OFS, S6E3FAE_SELF_DIAG_LEN),
#ifdef CONFIG_USDM_DDI_CMDLOG
	[READ_CMDLOG] = RDINFO_INIT(cmdlog, DSI_PKT_TYPE_RD, S6E3FAE_CMDLOG_REG, S6E3FAE_CMDLOG_OFS, S6E3FAE_CMDLOG_LEN),
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	[READ_GRAM_CHECKSUM] = RDINFO_INIT(gram_checksum, DSI_PKT_TYPE_RD, S6E3FAE_GRAM_CHECKSUM_REG, S6E3FAE_GRAM_CHECKSUM_OFS, S6E3FAE_GRAM_CHECKSUM_LEN),
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	[READ_CCD_STATE] = RDINFO_INIT(ccd_state, DSI_PKT_TYPE_RD, S6E3FAE_CCD_STATE_REG, S6E3FAE_CCD_STATE_OFS, S6E3FAE_CCD_STATE_LEN),
#endif
	[READ_SSR_STATE] = RDINFO_INIT(ssr_state, DSI_PKT_TYPE_RD, S6E3FAE_SSR_STATE_REG, S6E3FAE_SSR_STATE_OFS, S6E3FAE_SSR_STATE_LEN),
	[READ_ECC_RECOVERED] = RDINFO_INIT(ecc_recovered, DSI_PKT_TYPE_RD, S6E3FAE_ECC_RECOVERED_REG, S6E3FAE_ECC_RECOVERED_OFS, S6E3FAE_ECC_RECOVERED_LEN),
	[READ_ECC_NOT_RECOVERED] = RDINFO_INIT(ecc_not_recovered, DSI_PKT_TYPE_RD, S6E3FAE_ECC_NOT_RECOVERED_REG, S6E3FAE_ECC_NOT_RECOVERED_OFS, S6E3FAE_ECC_NOT_RECOVERED_LEN),
	[READ_DECODER_TEST] = RDINFO_INIT(decoder_test, DSI_PKT_TYPE_RD, S6E3FAE_DECODER_TEST_REG, S6E3FAE_DECODER_TEST_OFS, S6E3FAE_DECODER_TEST_LEN),
#ifdef CONFIG_USDM_PANEL_MAFPC
	[READ_MAFPC] = RDINFO_INIT(mafpc, DSI_PKT_TYPE_RD, S6E3FAE_MAFPC_REG, S6E3FAE_MAFPC_OFS, S6E3FAE_MAFPC_LEN),
	[READ_MAFPC_FLASH] = RDINFO_INIT(mafpc_flash, DSI_PKT_TYPE_RD, S6E3FAE_MAFPC_FLASH_REG, S6E3FAE_MAFPC_FLASH_OFS, S6E3FAE_MAFPC_FLASH_LEN),
	[READ_MAFPC_CRC] = RDINFO_INIT(mafpc_crc, DSI_PKT_TYPE_RD, S6E3FAE_MAFPC_CRC_REG, S6E3FAE_MAFPC_CRC_OFS, S6E3FAE_MAFPC_CRC_LEN),
#endif
	[READ_SELF_MASK_CRC] = RDINFO_INIT(self_mask_crc, DSI_PKT_TYPE_RD, S6E3FAE_SELF_MASK_CRC_REG, S6E3FAE_SELF_MASK_CRC_OFS, S6E3FAE_SELF_MASK_CRC_LEN),
	[READ_SELF_MASK_CHECKSUM] = RDINFO_INIT(self_mask_checksum, DSI_PKT_TYPE_RD, S6E3FAE_SELF_MASK_CHECKSUM_REG, S6E3FAE_SELF_MASK_CHECKSUM_OFS, S6E3FAE_SELF_MASK_CHECKSUM_LEN),
};

static DEFINE_RESUI(id, &s6e3fae_rditbl[READ_ID], 0);
static DEFINE_RESUI(coordinate, &s6e3fae_rditbl[READ_COORDINATE], 0);
static DEFINE_RESUI(code, &s6e3fae_rditbl[READ_CODE], 0);
static DEFINE_RESUI(elvss, &s6e3fae_rditbl[READ_ELVSS], 0);
static DEFINE_RESUI(elvss_cal_offset, &s6e3fae_rditbl[READ_ELVSS_CAL_OFFSET], 0);
static DEFINE_RESUI(elvss_temp_0, &s6e3fae_rditbl[READ_ELVSS_TEMP_0], 0);
static DEFINE_RESUI(elvss_temp_1, &s6e3fae_rditbl[READ_ELVSS_TEMP_1], 0);
static DEFINE_RESUI(date, &s6e3fae_rditbl[READ_DATE], 0);
static DEFINE_RESUI(octa_id, &s6e3fae_rditbl[READ_OCTA_ID], 0);
/* for brightness debugging */
static DEFINE_RESUI(aor, &s6e3fae_rditbl[READ_AOR], 0);
static DEFINE_RESUI(vint, &s6e3fae_rditbl[READ_VINT], 0);
static DEFINE_RESUI(elvss_t, &s6e3fae_rditbl[READ_ELVSS_T], 0);
static struct res_update_info RESUI(flash_loaded)[] = {
	{
		.offset = 0,
		.rditbl = &s6e3fae_rditbl[READ_FLASH_LOADED_1],
	}, {
		.offset = S6E3FAE_FLASH_LOADED_1_LEN,
		.rditbl = &s6e3fae_rditbl[READ_FLASH_LOADED_2],
	},
};
static DEFINE_RESUI(irc, &s6e3fae_rditbl[READ_IRC], 0);
#ifdef CONFIG_USDM_PANEL_COPR
static DEFINE_RESUI(copr_spi, &s6e3fae_rditbl[READ_COPR_SPI], 0);
static DEFINE_RESUI(copr_dsi, &s6e3fae_rditbl[READ_COPR_DSI], 0);
#endif
static DEFINE_RESUI(chip_id, &s6e3fae_rditbl[READ_CHIP_ID], 0);
static DEFINE_RESUI(rddpm, &s6e3fae_rditbl[READ_RDDPM], 0);
static DEFINE_RESUI(rddsm, &s6e3fae_rditbl[READ_RDDSM], 0);
static DEFINE_RESUI(err, &s6e3fae_rditbl[READ_ERR], 0);
static DEFINE_RESUI(err_fg, &s6e3fae_rditbl[READ_ERR_FG], 0);
static DEFINE_RESUI(dsi_err, &s6e3fae_rditbl[READ_DSI_ERR], 0);
static DEFINE_RESUI(self_diag, &s6e3fae_rditbl[READ_SELF_DIAG], 0);

#ifdef CONFIG_USDM_DDI_CMDLOG
static DEFINE_RESUI(cmdlog, &s6e3fae_rditbl[READ_CMDLOG], 0);
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
static DEFINE_RESUI(gram_checksum, &s6e3fae_rditbl[READ_GRAM_CHECKSUM], 0);
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static DEFINE_RESUI(ccd_state, &s6e3fae_rditbl[READ_CCD_STATE], 0);
#endif
static DEFINE_RESUI(ssr_test, &s6e3fae_rditbl[READ_SSR_STATE], 0);
static struct res_update_info RESUI(ecc_test)[] = {
	{
		.offset = 0,
		.rditbl = &s6e3fae_rditbl[READ_ECC_RECOVERED],
	}, {
		.offset = 1,
		.rditbl = &s6e3fae_rditbl[READ_ECC_NOT_RECOVERED],
	},
};
static DEFINE_RESUI(decoder_test, &s6e3fae_rditbl[READ_DECODER_TEST], 0);
static DEFINE_RESUI(self_mask_crc, &s6e3fae_rditbl[READ_SELF_MASK_CRC], 0);
static DEFINE_RESUI(self_mask_checksum, &s6e3fae_rditbl[READ_SELF_MASK_CHECKSUM], 0);
#ifdef CONFIG_USDM_PANEL_MAFPC
static DEFINE_RESUI(mafpc, &s6e3fae_rditbl[READ_MAFPC], 0);
static DEFINE_RESUI(mafpc_flash, &s6e3fae_rditbl[READ_MAFPC_FLASH], 0);
static DEFINE_RESUI(mafpc_crc, &s6e3fae_rditbl[READ_MAFPC_CRC], 0);
#endif

static struct resinfo s6e3fae_restbl[] = {
	[RES_ID] = RESINFO_INIT(id, S6E3FAE_ID, RESUI(id)),
	[RES_COORDINATE] = RESINFO_INIT(coordinate, S6E3FAE_COORDINATE, RESUI(coordinate)),
	[RES_CODE] = RESINFO_INIT(code, S6E3FAE_CODE, RESUI(code)),
	[RES_ELVSS] = RESINFO_INIT(elvss, S6E3FAE_ELVSS, RESUI(elvss)),
	[RES_ELVSS_CAL_OFFSET] = RESINFO_INIT(elvss_cal_offset, S6E3FAE_ELVSS_CAL_OFFSET, RESUI(elvss_cal_offset)),
	[RES_ELVSS_TEMP_0] = RESINFO_INIT(elvss_temp_0, S6E3FAE_ELVSS_TEMP_0, RESUI(elvss_temp_0)),
	[RES_ELVSS_TEMP_1] = RESINFO_INIT(elvss_temp_1, S6E3FAE_ELVSS_TEMP_1, RESUI(elvss_temp_1)),
	[RES_DATE] = RESINFO_INIT(date, S6E3FAE_DATE, RESUI(date)),
	[RES_OCTA_ID] = RESINFO_INIT(octa_id, S6E3FAE_OCTA_ID, RESUI(octa_id)),
	[RES_AOR] = RESINFO_INIT(aor, S6E3FAE_AOR, RESUI(aor)),
	[RES_VINT] = RESINFO_INIT(vint, S6E3FAE_VINT, RESUI(vint)),
	[RES_ELVSS_T] = RESINFO_INIT(elvss_t, S6E3FAE_ELVSS_T, RESUI(elvss_t)),
	[RES_FLASH_LOADED] = RESINFO_INIT(flash_loaded, S6E3FAE_FLASH_LOADED, RESUI(flash_loaded)),
	[RES_IRC] = RESINFO_INIT(irc, S6E3FAE_IRC, RESUI(irc)),
#ifdef CONFIG_USDM_PANEL_COPR
	[RES_COPR_SPI] = RESINFO_INIT(copr_spi, S6E3FAE_COPR_SPI, RESUI(copr_spi)),
	[RES_COPR_DSI] = RESINFO_INIT(copr_dsi, S6E3FAE_COPR_DSI, RESUI(copr_dsi)),
#endif
	[RES_CHIP_ID] = RESINFO_INIT(chip_id, S6E3FAE_CHIP_ID, RESUI(chip_id)),
	[RES_RDDPM] = RESINFO_INIT(rddpm, S6E3FAE_RDDPM, RESUI(rddpm)),
	[RES_RDDSM] = RESINFO_INIT(rddsm, S6E3FAE_RDDSM, RESUI(rddsm)),
	[RES_ERR] = RESINFO_INIT(err, S6E3FAE_ERR, RESUI(err)),
	[RES_ERR_FG] = RESINFO_INIT(err_fg, S6E3FAE_ERR_FG, RESUI(err_fg)),
	[RES_DSI_ERR] = RESINFO_INIT(dsi_err, S6E3FAE_DSI_ERR, RESUI(dsi_err)),
	[RES_SELF_DIAG] = RESINFO_INIT(self_diag, S6E3FAE_SELF_DIAG, RESUI(self_diag)),
#ifdef CONFIG_USDM_DDI_CMDLOG
	[RES_CMDLOG] = RESINFO_INIT(cmdlog, S6E3FAE_CMDLOG, RESUI(cmdlog)),
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	[RES_GRAM_CHECKSUM] = RESINFO_INIT(gram_checksum, S6E3FAE_GRAM_CHECKSUM, RESUI(gram_checksum)),
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	[RES_CCD_STATE] = RESINFO_INIT(ccd_state, S6E3FAE_CCD_STATE, RESUI(ccd_state)),
	[RES_CCD_CHKSUM_PASS] = RESINFO_IMMUTABLE_INIT(ccd_chksum_pass, S6E3FAE_CCD_CHKSUM_PASS),
#endif
	[RES_SSR_TEST] = RESINFO_INIT(ssr_test, S6E3FAE_SSR_TEST, RESUI(ssr_test)),
	[RES_ECC_TEST] = RESINFO_INIT(ecc_test, S6E3FAE_ECC_TEST, RESUI(ecc_test)),
	[RES_DECODER_TEST] = RESINFO_INIT(decoder_test, S6E3FAE_DECODER_TEST, RESUI(decoder_test)),
	[RES_SELF_MASK_CRC] = RESINFO_INIT(self_mask_crc, S6E3FAE_SELF_MASK_CRC, RESUI(self_mask_crc)),
	[RES_SELF_MASK_CHECKSUM] = RESINFO_INIT(self_mask_checksum, S6E3FAE_SELF_MASK_CHECKSUM, RESUI(self_mask_checksum)),
#ifdef CONFIG_USDM_PANEL_MAFPC
	[RES_MAFPC] = RESINFO_INIT(mafpc, S6E3FAE_MAFPC, RESUI(mafpc)),
	[RES_MAFPC_FLASH] = RESINFO_INIT(mafpc_flash, S6E3FAE_MAFPC_FLASH, RESUI(mafpc_flash)),
	[RES_MAFPC_CRC] = RESINFO_INIT(mafpc_crc, S6E3FAE_MAFPC_CRC, RESUI(mafpc_crc)),
#endif
};

enum {
	DUMP_RDDPM = 0,
	DUMP_RDDPM_SLEEP_IN,
	DUMP_RDDSM,
	DUMP_ERR,
	DUMP_ERR_FG,
	DUMP_DSI_ERR,
	DUMP_SELF_DIAG,
	DUMP_SELF_MASK_CRC,
	DUMP_SELF_MASK_CHECKSUM,
	DUMP_SELF_MASK_FACTORY_CHECKSUM,
#ifdef CONFIG_USDM_DDI_CMDLOG
	DUMP_CMDLOG,
#endif
#ifdef CONFIG_USDM_PANEL_MAFPC
	DUMP_MAFPC,
	DUMP_MAFPC_FLASH,
	DUMP_ABC_CRC,
#endif
	DUMP_SSR,
	DUMP_ECC,
	DUMP_DSC_CRC,
	DUMP_FLASH_LOADED,
};

#ifdef CONFIG_USDM_PANEL_MAFPC
#define S6E3FAE_MAFPC_ENABLE (0x71)
#define S6E3FAE_MAFPC_CTRL_CMD_OFFSET (6)
#endif

static struct dump_expect rddpm_after_display_on_expects[] = {
	{ .offset = 0, .mask = 0x80, .value = 0x80, .msg = "Booster Mode : OFF(NG)" },
	{ .offset = 0, .mask = 0x40, .value = 0x00, .msg = "Idle Mode : ON(NG)" },
	{ .offset = 0, .mask = 0x10, .value = 0x10, .msg = "Sleep Mode : IN(NG)" },
	{ .offset = 0, .mask = 0x08, .value = 0x08, .msg = "Normal Mode : SLEEP(NG)" },
	{ .offset = 0, .mask = 0x04, .value = 0x04, .msg = "Display Mode : OFF(NG)" },
};

static struct dump_expect rddpm_before_sleep_in_expects[] = {
	{ .offset = 0, .mask = 0x80, .value = 0x80, .msg = "Booster Mode : OFF(NG)" },
	{ .offset = 0, .mask = 0x40, .value = 0x00, .msg = "Idle Mode : ON(NG)" },
	{ .offset = 0, .mask = 0x10, .value = 0x10, .msg = "Sleep Mode : IN(NG)" },
	{ .offset = 0, .mask = 0x08, .value = 0x08, .msg = "Normal Mode : SLEEP(NG)" },
	{ .offset = 0, .mask = 0x04, .value = 0x00, .msg = "Display Mode : ON(NG)" },
};

static struct dump_expect rddsm_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = 0x80, .msg = "TE Mode : OFF(NG)" },
};

static struct dump_expect dsi_err_expects[] = {
	{ .offset = 0, .mask = 0x80, .value = 0x00, .msg = "DSI Protocol Violation" },
	{ .offset = 0, .mask = 0x40, .value = 0x00, .msg = "Data P Lane Contention Detetion" },
	{ .offset = 0, .mask = 0x20, .value = 0x00, .msg = "Invalid Transmission Length" },
	{ .offset = 0, .mask = 0x10, .value = 0x00, .msg = "DSI VC ID Invalid" },
	{ .offset = 0, .mask = 0x08, .value = 0x00, .msg = "DSI Data Type Not Recognized" },
	{ .offset = 0, .mask = 0x04, .value = 0x00, .msg = "Checksum Error" },
	{ .offset = 0, .mask = 0x02, .value = 0x00, .msg = "ECC Error, multi-bit (detected, not corrected)" },
	{ .offset = 0, .mask = 0x01, .value = 0x00, .msg = "ECC Error, single-bit (detected and corrected)" },
	{ .offset = 1, .mask = 0x80, .value = 0x00, .msg = "Data Lane Contention Detection" },
	{ .offset = 1, .mask = 0x40, .value = 0x00, .msg = "False Control Error" },
	{ .offset = 1, .mask = 0x20, .value = 0x00, .msg = "HS RX Timeout" },
	{ .offset = 1, .mask = 0x10, .value = 0x00, .msg = "Low-Power Transmit Sync Error" },
	{ .offset = 1, .mask = 0x08, .value = 0x00, .msg = "Escape Mode Entry Command Error" },
	{ .offset = 1, .mask = 0x04, .value = 0x00, .msg = "EoT Sync Error" },
	{ .offset = 1, .mask = 0x02, .value = 0x00, .msg = "SoT Sync Error" },
	{ .offset = 1, .mask = 0x01, .value = 0x00, .msg = "SoT Error" },
	{ .offset = 2, .mask = 0xFF, .value = 0x00, .msg = "CRC Error Count" },
	{ .offset = 3, .mask = 0xFF, .value = 0x00, .msg = "CRC Error Count" },
	{ .offset = 4, .mask = 0xFF, .value = 0x00, .msg = "CRC Error Count" },
};

static struct dump_expect err_fg_expects[] = {
	{ .offset = 0, .mask = 0x40, .value = 0x00, .msg = "VLIN1 Error" },
	{ .offset = 0, .mask = 0x04, .value = 0x00, .msg = "VLOUT3 Error" },
	{ .offset = 0, .mask = 0x08, .value = 0x00, .msg = "ELVDD Error" },
};

static struct dump_expect dsie_cnt_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = 0x00, .msg = "DSI Error Count" },
};

static struct dump_expect self_diag_expects[] = {
	{ .offset = 0, .mask = 0x40, .value = 0x40, .msg = "Panel Boosting Error" },
};

#ifdef CONFIG_USDM_DDI_CMDLOG
static struct dump_expect cmdlog_expects[] = {
};
#endif

#ifdef CONFIG_USDM_PANEL_MAFPC
static struct dump_expect mafpc_en_expects[] = {
	{ .offset = 0, .mask = 0x01, .value = 0x01, .msg = "mAFPC Disabled" },
};

static struct dump_expect mafpc_flash_expects[] = {
	{ .offset = 0, .mask = 0x02, .value = 0x02, .msg = "mAFPC bypass(NG)" },
};

static struct dump_expect abc_crc_expects[] = {
};
#endif

static struct dump_expect self_mask_crc_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = 0xB7, .msg = "Self Mask CRC[0] Error(NG)" },
	{ .offset = 1, .mask = 0xFF, .value = 0xEB, .msg = "Self Mask CRC[1] Error(NG)" },
};

static struct dump_expect ecc_expects[] = {
	{ .offset = 0, .mask = S6E3FAE_ECC_MASK, .value = S6E3FAE_ECC_VALUE, .msg = "Error Bit Recovered Count" },
	{ .offset = 1, .mask = S6E3FAE_ECC_MASK, .value = S6E3FAE_ECC_VALUE, .msg = "Error Bit Not Recovered Count" },
};

static struct dump_expect ssr_expects[] = {
	{ .offset = 0, .mask = S6E3FAE_SSR_STATE_MASK, .value = S6E3FAE_SSR_STATE_VALUE, .msg = "SSR recovered" },
	{ .offset = 0, .mask = 0x01, .value = 0x00, .msg = "Left source recovered" },
	{ .offset = 0, .mask = 0x04, .value = 0x00, .msg = "Right source recovered" },
};

static struct dump_expect flash_loaded_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = S6E3FAE_FLASH_LOADED_1_VALUE, .msg = "Flash State 1 Error(NG)" },
	{ .offset = 1, .mask = S6E3FAE_FLASH_LOADED_2_LSB_MASK, .value = S6E3FAE_FLASH_LOADED_2_LSB_VALUE, .msg = "Flash State 2 LSB Error(NG)" },
	{ .offset = 1, .mask = S6E3FAE_FLASH_LOADED_2_MSB_MASK, .value = S6E3FAE_FLASH_LOADED_2_MSB_VALUE, .msg = "Flash State 2 MSB Error(NG)" },
};

static struct dump_expect dsc_crc_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = S6E3FAE_DSC_CRC_VALUE_1, .msg = "DDI DSC CRC 1 Error(NG)" },
	{ .offset = 1, .mask = 0xFF, .value = S6E3FAE_DSC_CRC_VALUE_2, .msg = "DDI DSC CRC 2 Error(NG)" },
};

static struct dumpinfo s6e3fae_dmptbl[] = {
	[DUMP_RDDPM] = DUMPINFO_INIT_V2(rddpm, &s6e3fae_restbl[RES_RDDPM], &OLED_FUNC(OLED_DUMP_SHOW_RDDPM), rddpm_after_display_on_expects),
	[DUMP_RDDPM_SLEEP_IN] = DUMPINFO_INIT_V2(rddpm_sleep_in, &s6e3fae_restbl[RES_RDDPM], &OLED_FUNC(OLED_DUMP_SHOW_RDDPM_BEFORE_SLEEP_IN), rddpm_before_sleep_in_expects),
	[DUMP_RDDSM] = DUMPINFO_INIT_V2(rddsm, &s6e3fae_restbl[RES_RDDSM], &OLED_FUNC(OLED_DUMP_SHOW_RDDSM), rddsm_expects),
	[DUMP_ERR] = DUMPINFO_INIT_V2(err, &s6e3fae_restbl[RES_ERR], &OLED_FUNC(OLED_DUMP_SHOW_ERR), dsi_err_expects),
	[DUMP_ERR_FG] = DUMPINFO_INIT_V2(err_fg, &s6e3fae_restbl[RES_ERR_FG], &OLED_FUNC(OLED_DUMP_SHOW_ERR_FG), err_fg_expects),
	[DUMP_DSI_ERR] = DUMPINFO_INIT_V2(dsi_err, &s6e3fae_restbl[RES_DSI_ERR], &OLED_FUNC(OLED_DUMP_SHOW_DSI_ERR), dsie_cnt_expects),
	[DUMP_SELF_DIAG] = DUMPINFO_INIT_V2(self_diag, &s6e3fae_restbl[RES_SELF_DIAG], &OLED_FUNC(OLED_DUMP_SHOW_SELF_DIAG), self_diag_expects),
#ifdef CONFIG_USDM_DDI_CMDLOG
	[DUMP_CMDLOG] = DUMPINFO_INIT_V2(cmdlog, &s6e3fae_restbl[RES_CMDLOG], &OLED_FUNC(OLED_DUMP_SHOW_CMDLOG), cmdlog_expects),
#endif
#ifdef CONFIG_USDM_PANEL_MAFPC
	[DUMP_MAFPC] = DUMPINFO_INIT_V2(mafpc, &s6e3fae_restbl[RES_MAFPC], &OLED_FUNC(OLED_DUMP_SHOW_MAFPC_LOG), mafpc_en_expects),
	[DUMP_MAFPC_FLASH] = DUMPINFO_INIT_V2(mafpc_flash, &s6e3fae_restbl[RES_MAFPC_FLASH], &OLED_FUNC(OLED_DUMP_SHOW_MAFPC_FLASH_LOG), mafpc_flash_expects),
	[DUMP_ABC_CRC] = DUMPINFO_INIT_V2(mafpc_crc, &s6e3fae_restbl[RES_MAFPC_CRC], &OLED_FUNC(OLED_DUMP_SHOW_ABC_CRC_LOG), abc_crc_expects),
#endif
	[DUMP_SELF_MASK_CRC] = DUMPINFO_INIT_V2(self_mask_crc, &s6e3fae_restbl[RES_SELF_MASK_CRC], &OLED_FUNC(OLED_DUMP_SHOW_EXPECTS), self_mask_crc_expects),
	[DUMP_SSR] = DUMPINFO_INIT_V2(ssr, &s6e3fae_restbl[RES_SSR_TEST], &OLED_FUNC(OLED_DUMP_SHOW_SSR_ERR), ssr_expects),
	[DUMP_ECC] = DUMPINFO_INIT_V2(ecc, &s6e3fae_restbl[RES_ECC_TEST], &OLED_FUNC(OLED_DUMP_SHOW_ECC_ERR), ecc_expects),
	[DUMP_DSC_CRC] = DUMPINFO_INIT_V2(dsc_crc, &s6e3fae_restbl[RES_DECODER_TEST], &OLED_FUNC(OLED_DUMP_SHOW_EXPECTS), dsc_crc_expects),
	[DUMP_FLASH_LOADED] = DUMPINFO_INIT_V2(flash_loaded, &s6e3fae_restbl[RES_FLASH_LOADED], &OLED_FUNC(OLED_DUMP_SHOW_FLASH_LOADED), flash_loaded_expects),
};

/* use according to adaptive_control sysfs */
enum {
	S6E3FAE_ACL_OPR_0,
	S6E3FAE_ACL_OPR_1,
	S6E3FAE_ACL_OPR_2,
	S6E3FAE_ACL_OPR_3,
	S6E3FAE_ACL_OPR_4,
	S6E3FAE_ACL_OPR_5,
	S6E3FAE_ACL_OPR_6,
	MAX_S6E3FAE_ACL_OPR
};

#define S6E3FAE_ACL_OPR_PROPERTY ("s6e3fae_acl_opr")

/* Variable Refresh Rate */
enum {
	S6E3FAE_VRR_MODE_NS,
	S6E3FAE_VRR_MODE_HS,
	MAX_S6E3FAE_VRR_MODE,
};

enum {
	S6E3FAE_VRR_120HS,
	S6E3FAE_VRR_80HS,
	S6E3FAE_VRR_60HS,
	S6E3FAE_VRR_48HS,
	S6E3FAE_VRR_30HS,
	S6E3FAE_VRR_24HS,
	S6E3FAE_VRR_10HS,
	MAX_S6E3FAE_VRR,
};

#define S6E3FAE_VRR_PROPERTY ("s6e3fae_vrr")

enum {
	S6E3FAE_LPM_LFD_1HZ,
	S6E3FAE_LPM_LFD_30HZ,
	MAX_S6E3FAE_LPM_LFD,
};

#define S6E3FAE_LPM_LFD_PROPERTY ("s6e3fae_lpm_lfd")

enum {
	S6E3FAE_VRR_LFD_SCALABILITY_NONE,
	S6E3FAE_VRR_LFD_SCALABILITY_1,
	S6E3FAE_VRR_LFD_SCALABILITY_2,
	S6E3FAE_VRR_LFD_SCALABILITY_3,
	S6E3FAE_VRR_LFD_SCALABILITY_4,
	S6E3FAE_VRR_LFD_SCALABILITY_5,
	S6E3FAE_VRR_LFD_SCALABILITY_6,
	MAX_S6E3FAE_VRR_LFD_SCALABILITY,
};

#define S6E3FAE_LFD_SCALABILITY_PROPERTY ("s6e3fae_lfd_scalability")

#define S6E3FAE_VRR_LFD_SCALABILITY_MIN (S6E3FAE_VRR_LFD_SCALABILITY_1)
#define S6E3FAE_VRR_LFD_SCALABILITY_DEF (S6E3FAE_VRR_LFD_SCALABILITY_5)
#define S6E3FAE_VRR_LFD_SCALABILITY_MAX (S6E3FAE_VRR_LFD_SCALABILITY_6)

#define S6E3FAE_VRR_LFD_SCALABILITY_MIN (S6E3FAE_VRR_LFD_SCALABILITY_1)
#define S6E3FAE_VRR_LFD_SCALABILITY_DEF (S6E3FAE_VRR_LFD_SCALABILITY_5)
#define S6E3FAE_VRR_LFD_SCALABILITY_MAX (S6E3FAE_VRR_LFD_SCALABILITY_6)

enum {
	S6E3FAE_LFD_120HZ,
	S6E3FAE_LFD_80HZ,
	S6E3FAE_LFD_60HZ,
	S6E3FAE_LFD_48HZ,
	S6E3FAE_LFD_30HZ,
	S6E3FAE_LFD_24HZ,
	S6E3FAE_LFD_10HZ,
	S6E3FAE_LFD_1HZ,
	S6E3FAE_LFD_0_5HZ,
	MAX_S6E3FAE_LFD,
};

#define S6E3FAE_PREV_LFD_MIN_PROPERTY ("s6e3fae_prev_lfd_min")
#define S6E3FAE_PREV_LFD_MAX_PROPERTY ("s6e3fae_prev_lfd_max")
#define S6E3FAE_LFD_MIN_PROPERTY ("s6e3fae_lfd_min")
#define S6E3FAE_LFD_MAX_PROPERTY ("s6e3fae_lfd_max")

enum {
	S6E3FAE_RESOL_1080x2340,
};

enum {
	S6E3FAE_DISPLAY_MODE_1080x2340_120HS,
	S6E3FAE_DISPLAY_MODE_1080x2340_80HS,
	S6E3FAE_DISPLAY_MODE_1080x2340_60HS,
	S6E3FAE_DISPLAY_MODE_1080x2340_48HS,
	S6E3FAE_DISPLAY_MODE_1080x2340_30HS,
	S6E3FAE_DISPLAY_MODE_1080x2340_24HS,
	S6E3FAE_DISPLAY_MODE_1080x2340_10HS,
	MAX_S6E3FAE_DISPLAY_MODE,
};

enum {
	S6E3FAE_SMOOTH_DIM_USE = 0,
	S6E3FAE_SMOOTH_DIM_NOT_USE,
	MAX_S6E3FAE_SMOOTH_DIM,
};

#define S6E3FAE_SMOOTH_DIM_PROPERTY ("s6e3fae_smooth_dim")
#define S6E3FAE_MAFPC_ENABLE_PROPERTY ("s6e3fae_mafpc_enable")
#define S6E3FAE_MDNIE_HBM_CE_LEVEL_PROPERTY ("s6e3fae_hbm_ce_level")

int s6e3fae_do_gamma_flash_checksum(struct panel_device *panel, void *data, u32 len);
static inline int s6e3fae_ddi_init(struct panel_device *panel, void *data, u32 len) { return 0; }
int s6e3fae_get_octa_id(struct panel_device *panel, void *buf);
int s6e3fae_get_cell_id(struct panel_device *panel, void *buf);
int s6e3fae_get_manufacture_code(struct panel_device *panel, void *buf);
int s6e3fae_get_manufacture_date(struct panel_device *panel, void *buf);
int s6e3fae_get_temperature_range(struct panel_device *panel, void *buf);

int s6e3fae_init(struct common_panel_info *cpi);

#endif /* __S6E3FAE_H__ */
