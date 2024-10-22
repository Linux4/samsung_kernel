/*
 * linux/drivers/video/fbdev/exynos/panel/ana38407/ana38407.h
 *
 * Header file for ANA38407 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ANA38407_H__
#define __ANA38407_H__

#include <linux/types.h>
#include <linux/kernel.h>
#ifdef CONFIG_USDM_PANEL_DDI_FLASH
#include "../panel_poc.h"
#endif
#include "../panel_drv.h"
#include "../panel.h"
#include "../maptbl.h"
#include "../panel_function.h"
#include "oled_function.h"

/*
 * OFFSET ==> OFS means N-param - 1
 * <example>
 * XXX 1st param => ANA38407_XXX_OFS (0)
 * XXX 2nd param => ANA38407_XXX_OFS (1)
 * XXX 36th param => ANA38407_XXX_OFS (35)
 */

#define ANA38407_ADDR_OFS	(0)
#define ANA38407_ADDR_LEN	(1)
#define ANA38407_DATA_OFS	(ANA38407_ADDR_OFS + ANA38407_ADDR_LEN)

#define ANA38407_DATE_REG			0xA1
#define ANA38407_DATE_OFS			4
#define ANA38407_DATE_LEN			(PANEL_DATE_LEN)

#define ANA38407_COORDINATE_REG		0xA1
#define ANA38407_COORDINATE_OFS		0
#define ANA38407_COORDINATE_LEN		(PANEL_COORD_LEN)

#define ANA38407_ID_REG				0x04
#define ANA38407_ID_OFS				0
#define ANA38407_ID_LEN				(PANEL_ID_LEN)

/* ANA38407 CHIP ID ? not written in op code */
#define ANA38407_CODE_REG			0xD6
#define ANA38407_CODE_OFS			0
#define ANA38407_CODE_LEN			5

#define ANA38407_OCTA_ID_0_REG			0xA1
#define ANA38407_OCTA_ID_0_OFS			11
#define ANA38407_OCTA_ID_0_LEN			10

#define ANA38407_OCTA_ID_1_REG			0xA1
#define ANA38407_OCTA_ID_1_OFS			21
#define ANA38407_OCTA_ID_1_LEN			10

#define ANA38407_OCTA_ID_LEN			(ANA38407_OCTA_ID_0_LEN + ANA38407_OCTA_ID_1_LEN)

/* for panel dump */
#define ANA38407_RDDPM_REG			0x0A
#define ANA38407_RDDPM_OFS			0
#define ANA38407_RDDPM_LEN			(PANEL_RDDPM_LEN)

#define ANA38407_RDDSM_REG			0x0E
#define ANA38407_RDDSM_OFS			0
#define ANA38407_RDDSM_LEN			(PANEL_RDDSM_LEN)

#define ANA38407_ERR_REG				0xE9
#define ANA38407_ERR_OFS				0
#define ANA38407_ERR_LEN				5

#define ANA38407_ERR_FG_REG			0xEE
#define ANA38407_ERR_FG_OFS			0
#define ANA38407_ERR_FG_LEN			1

#define ANA38407_DSI_ERR_REG			0x05
#define ANA38407_DSI_ERR_OFS			0
#define ANA38407_DSI_ERR_LEN			1

#define ANA38407_SELF_DIAG_REG			0x0F
#define ANA38407_SELF_DIAG_OFS			0
#define ANA38407_SELF_DIAG_LEN			1

#define ANA38407_SELF_MASK_CHECKSUM_REG		0x14
#define ANA38407_SELF_MASK_CHECKSUM_OFS		0
#define ANA38407_SELF_MASK_CHECKSUM_LEN		2

#define ANA38407_SELF_MASK_CRC_REG		0x7F
#define ANA38407_SELF_MASK_CRC_OFS	6
#define ANA38407_SELF_MASK_CRC_LEN		4

#ifdef CONFIG_USDM_FACTORY_GCT_TEST
#define ANA38407_GRAM_CHECKSUM_REG	0xC1
#define ANA38407_GRAM_CHECKSUM_OFS	0
#define ANA38407_GRAM_CHECKSUM_LEN	1
#define ANA38407_GRAM_CHECKSUM_VALID_1	0x03
#define ANA38407_GRAM_CHECKSUM_VALID_2	0x03
#endif

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
#define ANA38407_DECODER_TEST1_REG			0xF2
#define ANA38407_DECODER_TEST1_OFS			0x21
#define ANA38407_DECODER_TEST1_LEN			8

#define ANA38407_DECODER_TEST2_REG			0xF2
#define ANA38407_DECODER_TEST2_OFS			0x21
#define ANA38407_DECODER_TEST2_LEN			8

#define ANA38407_DECODER_TEST3_REG			0xF6
#define ANA38407_DECODER_TEST3_OFS			0xD6
#define ANA38407_DECODER_TEST3_LEN			2
#endif

#ifdef CONFIG_USDM_FACTORY_CCD_TEST
#define ANA38407_CCD_STATE_REG				0xCC
#define ANA38407_CCD_STATE_OFS				2
#define ANA38407_CCD_STATE_LEN				1
#define ANA38407_CCD_STATE_PASS_LIST_LEN		1
#endif

#ifdef CONFIG_USDM_FACTORY_ECC_TEST
#define ANA38407_ECC_TEST_REG				0xF2
#define ANA38407_ECC_TEST_OFS				0x21
#define ANA38407_ECC_TEST_LEN				8
#endif

#define ANA38407_FW_VER_MAJOR_LEN				1
#define ANA38407_FW_VER_MINOR_LEN				1

#define ANA38407_ANALOG_GAMMA_REG		(0x67)
#define ANA38407_ANALOG_GAMMA_OFS		(0xA6)
#define ANA38407_ANALOG_GAMMA_LEN		(78)

#ifdef CONFIG_USDM_DDI_CMDLOG
#define ANA38407_CMDLOG_REG			0x9C
#define ANA38407_CMDLOG_OFS			0
#define ANA38407_CMDLOG_LEN			0x80
#endif

enum ana38407_function {
	ANA38407_MAPTBL_INIT_GAMMA_MODE2_BRT,
	ANA38407_MAPTBL_GETIDX_FIRMWARE_UPDATE,
	ANA38407_MAPTBL_GETIDX_GM2_ORIG_BRT,
	ANA38407_MAPTBL_GETIDX_HBM_TRANSITION,
	ANA38407_MAPTBL_GETIDX_ACL_OPR,
#if defined(CONFIG_USDM_FACTORY) && defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
	ANA38407_MAPTBL_GETIDX_FAST_DISCHARGE,
#endif
	ANA38407_MAPTBL_GETIDX_VRR_FPS,
	ANA38407_MAPTBL_GETIDX_VRR,
	ANA38407_MAPTBL_GETIDX_FFC,
	ANA38407_MAPTBL_INIT_ANALOG_GAMMA,
	ANA38407_MAPTBL_GETIDX_IRC_MODE,
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	ANA38407_MAPTBL_INIT_GRAM_IMG_PATTERN,
#endif
	MAX_ANA38407_FUNCTION,
};

extern struct pnobj_func ana38407_function_table[MAX_ANA38407_FUNCTION];

#undef DDI_FUNC
#define DDI_FUNC(_index) (ana38407_function_table[_index])

enum {
	GAMMA_MAPTBL,
	GAMMA_MODE2_MAPTBL,
	ANALOG_GAMMA_MAPTBL,
	TE_SETTING_MAPTBL,
	TE_FRAME_SEL_MAPTBL,
	AOR_MAPTBL,
	TSET_MAPTBL,
	ACL_ONOFF_MAPTBL,
	ACL_FRAME_AVG_MAPTBL,
	ACL_START_POINT_MAPTBL,
	ACL_DIM_SPEED_MAPTBL,
	ACL_OPR_MAPTBL,
	FPS_MAPTBL,
	FPS_TSP_VSYNC_MAPTBL,
	DIMMING_SPEED,
	LPM_NIT_MAPTBL,
	LPM_MODE_MAPTBL,
	HBM_ONOFF_MAPTBL,
#if defined(CONFIG_USDM_FACTORY) && defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
	FAST_DISCHARGE_MAPTBL,
#endif
	SET_FFC_MAPTBL,
	IRC_MODE_MAPTBL,
	GLUT_OFFSET_1_MAPTBL,
	GLUT_OFFSET_2_MAPTBL,
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
	READ_ELVSS_TEMP_0,
	READ_ELVSS_TEMP_1,
	READ_MTP,
	READ_DATE,
	READ_OCTA_ID_0,
	READ_OCTA_ID_1,
	/* READ_CHIP_ID, */
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
	READ_ANALOG_GAMMA_120HS,
#ifdef CONFIG_USDM_POC_SPI
	READ_POC_SPI_READ,
	READ_POC_SPI_STATUS1,
	READ_POC_SPI_STATUS2,
#endif
#ifdef CONFIG_USDM_PANEL_POC_FLASH
	READ_POC_MCA_CHKSUM,
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	READ_GRAM_CHECKSUM,
#endif
#ifdef CONFIG_USDM_DDI_CMDLOG
	READ_CMDLOG,
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	READ_CCD_STATE,
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
	READ_ECC_TEST,
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	READ_GRAYSPOT_CAL,
#endif
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	READ_DECODER_TEST1,
	READ_DECODER_TEST2,
	READ_DECODER_TEST3,
#endif
	MAX_READTBL,
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
	RES_ELVSS_TEMP_0,
	RES_ELVSS_TEMP_1,
	RES_MTP,
	RES_DATE,
	RES_OCTA_ID,
	/* RES_CHIP_ID, */
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
#ifdef CONFIG_USDM_DDI_CMDLOG
	RES_CMDLOG,
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	RES_GRAM_CHECKSUM,
#endif
#ifdef CONFIG_USDM_POC_SPI
	RES_POC_SPI_READ,
	RES_POC_SPI_STATUS1,
	RES_POC_SPI_STATUS2,
#endif
#ifdef CONFIG_USDM_PANEL_POC_FLASH
	RES_POC_MCA_CHKSUM,
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	RES_CCD_STATE,
	RES_CCD_CHKSUM_PASS_LIST,
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
	RES_ECC_TEST,
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	RES_GRAYSPOT_CAL,
#endif
	RES_SELF_MASK_CHECKSUM,
	RES_SELF_MASK_CRC,
	RES_ANALOG_GAMMA_120HS,
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	RES_DECODER_TEST1,
	RES_DECODER_TEST2,
	RES_DECODER_TEST3,
#endif
	MAX_RESTBL
};

enum {
	HLPM_ON_LOW,
	HLPM_ON,
	MAX_HLPM_ON
};


enum {
	ANA38407_VRR_FPS_120,
	ANA38407_VRR_FPS_60,
	MAX_ANA38407_VRR_FPS,
};


enum {
	ANA38407_SMOOTH_DIMMING_OFF,
	ANA38407_SMOOTH_DIMMING_ON,
	MAX_ANA38407_SMOOTH_DIMMING,
};

static u8 ANA38407_ID[ANA38407_ID_LEN];
static u8 ANA38407_COORDINATE[ANA38407_COORDINATE_LEN];
static u8 ANA38407_CODE[ANA38407_CODE_LEN];
static u8 ANA38407_DATE[ANA38407_DATE_LEN];
static u8 ANA38407_OCTA_ID[ANA38407_OCTA_ID_LEN];
/* for brightness debugging */
static u8 ANA38407_RDDPM[ANA38407_RDDPM_LEN];
static u8 ANA38407_RDDSM[ANA38407_RDDSM_LEN];
static u8 ANA38407_ERR[ANA38407_ERR_LEN];
static u8 ANA38407_ERR_FG[ANA38407_ERR_FG_LEN];
static u8 ANA38407_DSI_ERR[ANA38407_DSI_ERR_LEN];
static u8 ANA38407_SELF_DIAG[ANA38407_SELF_DIAG_LEN];
static u8 ANA38407_SELF_MASK_CHECKSUM[ANA38407_SELF_MASK_CHECKSUM_LEN];
static u8 ANA38407_SELF_MASK_CRC[ANA38407_SELF_MASK_CRC_LEN];
static u8 ANA38407_ANALOG_GAMMA_120HS[ANA38407_ANALOG_GAMMA_LEN];

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static u8 ANA38407_DECODER_TEST1[ANA38407_DECODER_TEST1_LEN];
static u8 ANA38407_DECODER_TEST2[ANA38407_DECODER_TEST2_LEN];
static u8 ANA38407_DECODER_TEST3[ANA38407_DECODER_TEST3_LEN];
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static u8 ANA38407_CCD_STATE[ANA38407_CCD_STATE_LEN];
static u8 ANA38407_CCD_CHKSUM_PASS_LIST[ANA38407_CCD_STATE_PASS_LIST_LEN] = { 0x00, };
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
static u8 ANA38407_ECC_TEST[ANA38407_ECC_TEST_LEN];
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
static u8 ANA38407_GRAM_CHECKSUM[ANA38407_GRAM_CHECKSUM_LEN];
#endif

static struct rdinfo ana38407_rditbl[MAX_READTBL] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, ANA38407_ID_REG, ANA38407_ID_OFS, ANA38407_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, ANA38407_COORDINATE_REG, ANA38407_COORDINATE_OFS, ANA38407_COORDINATE_LEN),
	[READ_CODE] = RDINFO_INIT(code, DSI_PKT_TYPE_RD, ANA38407_CODE_REG, ANA38407_CODE_OFS, ANA38407_CODE_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, ANA38407_DATE_REG, ANA38407_DATE_OFS, ANA38407_DATE_LEN),
	[READ_OCTA_ID_0] = RDINFO_INIT(octa_id_0, DSI_PKT_TYPE_RD, ANA38407_OCTA_ID_0_REG, ANA38407_OCTA_ID_0_OFS, ANA38407_OCTA_ID_0_LEN),
	[READ_OCTA_ID_1] = RDINFO_INIT(octa_id_1, DSI_PKT_TYPE_RD, ANA38407_OCTA_ID_1_REG, ANA38407_OCTA_ID_1_OFS, ANA38407_OCTA_ID_1_LEN),
	[READ_RDDPM] = RDINFO_INIT(rddpm, DSI_PKT_TYPE_RD, ANA38407_RDDPM_REG, ANA38407_RDDPM_OFS, ANA38407_RDDPM_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, ANA38407_RDDSM_REG, ANA38407_RDDSM_OFS, ANA38407_RDDSM_LEN),
	[READ_ERR] = RDINFO_INIT(err, DSI_PKT_TYPE_RD, ANA38407_ERR_REG, ANA38407_ERR_OFS, ANA38407_ERR_LEN),
	[READ_ERR_FG] = RDINFO_INIT(err_fg, DSI_PKT_TYPE_RD, ANA38407_ERR_FG_REG, ANA38407_ERR_FG_OFS, ANA38407_ERR_FG_LEN),
	[READ_DSI_ERR] = RDINFO_INIT(dsi_err, DSI_PKT_TYPE_RD, ANA38407_DSI_ERR_REG, ANA38407_DSI_ERR_OFS, ANA38407_DSI_ERR_LEN),
	[READ_SELF_DIAG] = RDINFO_INIT(self_diag, DSI_PKT_TYPE_RD, ANA38407_SELF_DIAG_REG, ANA38407_SELF_DIAG_OFS, ANA38407_SELF_DIAG_LEN),
	[READ_SELF_MASK_CHECKSUM] = RDINFO_INIT(self_mask_checksum, DSI_PKT_TYPE_RD, ANA38407_SELF_MASK_CHECKSUM_REG, ANA38407_SELF_MASK_CHECKSUM_OFS, ANA38407_SELF_MASK_CHECKSUM_LEN),
	[READ_SELF_MASK_CRC] = RDINFO_INIT(self_mask_crc, DSI_PKT_TYPE_RD, ANA38407_SELF_MASK_CRC_REG, ANA38407_SELF_MASK_CRC_OFS, ANA38407_SELF_MASK_CRC_LEN),
	[READ_ANALOG_GAMMA_120HS] = RDINFO_INIT(analog_gamma_120hs, DSI_PKT_TYPE_RD, ANA38407_ANALOG_GAMMA_REG, ANA38407_ANALOG_GAMMA_OFS, ANA38407_ANALOG_GAMMA_LEN),
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	[READ_DECODER_TEST1] = RDINFO_INIT(decoder_test1, DSI_PKT_TYPE_RD, ANA38407_DECODER_TEST1_REG, ANA38407_DECODER_TEST1_OFS, ANA38407_DECODER_TEST1_LEN),
	[READ_DECODER_TEST2] = RDINFO_INIT(decoder_test2, DSI_PKT_TYPE_RD, ANA38407_DECODER_TEST2_REG, ANA38407_DECODER_TEST2_OFS, ANA38407_DECODER_TEST2_LEN),
	[READ_DECODER_TEST3] = RDINFO_INIT(decoder_test3, DSI_PKT_TYPE_RD, ANA38407_DECODER_TEST3_REG, ANA38407_DECODER_TEST3_OFS, ANA38407_DECODER_TEST3_LEN),
#endif

#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	[READ_CCD_STATE] = RDINFO_INIT(ccd_state, DSI_PKT_TYPE_RD, ANA38407_CCD_STATE_REG, ANA38407_CCD_STATE_OFS, ANA38407_CCD_STATE_LEN),
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
	[READ_ECC_TEST] = RDINFO_INIT(ecc_test, DSI_PKT_TYPE_RD, ANA38407_ECC_TEST_REG, ANA38407_ECC_TEST_OFS, ANA38407_ECC_TEST_LEN),
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	[READ_GRAM_CHECKSUM] = RDINFO_INIT(gram_checksum, DSI_PKT_TYPE_RD, ANA38407_GRAM_CHECKSUM_REG, ANA38407_GRAM_CHECKSUM_OFS, ANA38407_GRAM_CHECKSUM_LEN),
#endif
};

static DEFINE_RESUI(id, &ana38407_rditbl[READ_ID], 0);
static DEFINE_RESUI(coordinate, &ana38407_rditbl[READ_COORDINATE], 0);
static DEFINE_RESUI(code, &ana38407_rditbl[READ_CODE], 0);
static DEFINE_RESUI(elvss, &ana38407_rditbl[READ_ELVSS], 0);
static DEFINE_RESUI(mtp, &ana38407_rditbl[READ_MTP], 0);
static DEFINE_RESUI(date, &ana38407_rditbl[READ_DATE], 0);
static DECLARE_RESUI(octa_id) = {
	{ .rditbl = &ana38407_rditbl[READ_OCTA_ID_0], .offset = 0 },
	{ .rditbl = &ana38407_rditbl[READ_OCTA_ID_1], .offset = 10 },
};

/* for brightness debugging */
static DEFINE_RESUI(aor, &ana38407_rditbl[READ_AOR], 0);
static DEFINE_RESUI(vint, &ana38407_rditbl[READ_VINT], 0);
static DEFINE_RESUI(elvss_t, &ana38407_rditbl[READ_ELVSS_T], 0);
static DEFINE_RESUI(rddpm, &ana38407_rditbl[READ_RDDPM], 0);
static DEFINE_RESUI(rddsm, &ana38407_rditbl[READ_RDDSM], 0);
static DEFINE_RESUI(err, &ana38407_rditbl[READ_ERR], 0);
static DEFINE_RESUI(err_fg, &ana38407_rditbl[READ_ERR_FG], 0);
static DEFINE_RESUI(dsi_err, &ana38407_rditbl[READ_DSI_ERR], 0);
static DEFINE_RESUI(self_diag, &ana38407_rditbl[READ_SELF_DIAG], 0);
static DEFINE_RESUI(self_mask_checksum, &ana38407_rditbl[READ_SELF_MASK_CHECKSUM], 0);
static DEFINE_RESUI(self_mask_crc, &ana38407_rditbl[READ_SELF_MASK_CRC], 0);
static DEFINE_RESUI(analog_gamma_120hs, &ana38407_rditbl[READ_ANALOG_GAMMA_120HS], 0);
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static DEFINE_RESUI(decoder_test1, &ana38407_rditbl[READ_DECODER_TEST1], 0);
static DEFINE_RESUI(decoder_test2, &ana38407_rditbl[READ_DECODER_TEST2], 0);
static DEFINE_RESUI(decoder_test3, &ana38407_rditbl[READ_DECODER_TEST3], 0);
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static DEFINE_RESUI(ccd_state, &ana38407_rditbl[READ_CCD_STATE], 0);
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
static DEFINE_RESUI(ecc_test, &ana38407_rditbl[READ_ECC_TEST], 0);
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
static DEFINE_RESUI(gram_checksum, &ana38407_rditbl[READ_GRAM_CHECKSUM], 0);
#endif

static struct resinfo ana38407_restbl[MAX_RESTBL] = {
	[RES_ID] = RESINFO_INIT(id, ANA38407_ID, RESUI(id)),
	[RES_COORDINATE] = RESINFO_INIT(coordinate, ANA38407_COORDINATE, RESUI(coordinate)),
	[RES_CODE] = RESINFO_INIT(code, ANA38407_CODE, RESUI(code)),
	[RES_DATE] = RESINFO_INIT(date, ANA38407_DATE, RESUI(date)),
	[RES_OCTA_ID] = RESINFO_INIT(octa_id, ANA38407_OCTA_ID, RESUI(octa_id)),
	[RES_RDDPM] = RESINFO_INIT(rddpm, ANA38407_RDDPM, RESUI(rddpm)),
	[RES_RDDSM] = RESINFO_INIT(rddsm, ANA38407_RDDSM, RESUI(rddsm)),
	[RES_ERR] = RESINFO_INIT(err, ANA38407_ERR, RESUI(err)),
	[RES_ERR_FG] = RESINFO_INIT(err_fg, ANA38407_ERR_FG, RESUI(err_fg)),
	[RES_DSI_ERR] = RESINFO_INIT(dsi_err, ANA38407_DSI_ERR, RESUI(dsi_err)),
	[RES_SELF_DIAG] = RESINFO_INIT(self_diag, ANA38407_SELF_DIAG, RESUI(self_diag)),
	[RES_SELF_MASK_CHECKSUM] = RESINFO_INIT(self_mask_checksum, ANA38407_SELF_MASK_CHECKSUM, RESUI(self_mask_checksum)),
	[RES_SELF_MASK_CRC] = RESINFO_INIT(self_mask_crc, ANA38407_SELF_MASK_CRC, RESUI(self_mask_crc)),
	[RES_ANALOG_GAMMA_120HS] = RESINFO_INIT(analog_gamma_120hs, ANA38407_ANALOG_GAMMA_120HS, RESUI(analog_gamma_120hs)),
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	[RES_DECODER_TEST1] = RESINFO_INIT(decoder_test1, ANA38407_DECODER_TEST1, RESUI(decoder_test1)),
	[RES_DECODER_TEST2] = RESINFO_INIT(decoder_test2, ANA38407_DECODER_TEST2, RESUI(decoder_test2)),
	[RES_DECODER_TEST3] = RESINFO_INIT(decoder_test3, ANA38407_DECODER_TEST3, RESUI(decoder_test3)),
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	[RES_CCD_STATE] = RESINFO_INIT(ccd_state, ANA38407_CCD_STATE, RESUI(ccd_state)),
	[RES_CCD_CHKSUM_PASS_LIST] = RESINFO_IMMUTABLE_INIT(ccd_chksum_pass_list, ANA38407_CCD_CHKSUM_PASS_LIST),
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
	[RES_ECC_TEST] = RESINFO_INIT(ecc_test, ANA38407_ECC_TEST, RESUI(ecc_test)),
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	[RES_GRAM_CHECKSUM] = RESINFO_INIT(gram_checksum, ANA38407_GRAM_CHECKSUM, RESUI(gram_checksum)),
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
#ifdef CONFIG_USDM_DDI_CMDLOG
	DUMP_CMDLOG,
#endif
};

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
int ana38407_decoder_test(struct panel_device *panel, void *data, u32 len);
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
int ana38407_ecc_test(struct panel_device *panel, void *data, u32 len);
#endif

static struct dump_expect rddpm_after_display_on_expects[] = {
	{ .offset = 0, .mask = 0x40, .value = 0x00, .msg = "Idle Mode : ON(NG)" },
	{ .offset = 0, .mask = 0x10, .value = 0x10, .msg = "Sleep Mode : IN(NG)" },
	{ .offset = 0, .mask = 0x08, .value = 0x08, .msg = "Normal Mode : SLEEP(NG)" },
	{ .offset = 0, .mask = 0x04, .value = 0x04, .msg = "Display Mode : OFF(NG)" },
};

static struct dump_expect rddpm_before_sleep_in_expects[] = {
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

static struct dump_expect self_mask_crc_expects[] = {
};

static struct dump_expect pcd_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = 0x00, .msg = "PCD Occur" },
};

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
int ana38407_decoder_test(struct panel_device *panel, void *data, u32 len);
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
int ana38407_ecc_test(struct panel_device *panel, void *data, u32 len);
#endif

static struct dumpinfo ana38407_dmptbl[] = {
	[DUMP_RDDPM] = DUMPINFO_INIT_V2(rddpm, &ana38407_restbl[RES_RDDPM], &OLED_FUNC(OLED_DUMP_SHOW_RDDPM), rddpm_after_display_on_expects),
	[DUMP_RDDPM_SLEEP_IN] = DUMPINFO_INIT_V2(rddpm_sleep_in, &ana38407_restbl[RES_RDDPM], &OLED_FUNC(OLED_DUMP_SHOW_RDDPM_BEFORE_SLEEP_IN), rddpm_before_sleep_in_expects),
	[DUMP_RDDSM] = DUMPINFO_INIT_V2(rddsm, &ana38407_restbl[RES_RDDSM], &OLED_FUNC(OLED_DUMP_SHOW_RDDSM), rddsm_expects),
	[DUMP_ERR] = DUMPINFO_INIT_V2(err, &ana38407_restbl[RES_ERR], &OLED_FUNC(OLED_DUMP_SHOW_ERR), dsi_err_expects),
	[DUMP_ERR_FG] = DUMPINFO_INIT_V2(err_fg, &ana38407_restbl[RES_ERR_FG], &OLED_FUNC(OLED_DUMP_SHOW_ERR_FG), err_fg_expects),
	[DUMP_DSI_ERR] = DUMPINFO_INIT_V2(dsi_err, &ana38407_restbl[RES_DSI_ERR], &OLED_FUNC(OLED_DUMP_SHOW_DSI_ERR), dsie_cnt_expects),
	[DUMP_SELF_DIAG] = DUMPINFO_INIT_V2(self_diag, &ana38407_restbl[RES_SELF_DIAG], &OLED_FUNC(OLED_DUMP_SHOW_SELF_DIAG), self_diag_expects),
#ifdef CONFIG_USDM_DDI_CMDLOG
	[DUMP_CMDLOG] = DUMPINFO_INIT_V2(cmdlog, &ana38407_restbl[RES_CMDLOG], &OLED_FUNC(OLED_DUMP_SHOW_CMDLOG), cmdlog_expects),
#endif
	[DUMP_SELF_MASK_CRC] = DUMPINFO_INIT_V2(self_mask_crc, &ana38407_restbl[RES_SELF_MASK_CRC], &OLED_FUNC(OLED_DUMP_SHOW_SELF_MASK_CRC), self_mask_crc_expects),
};

/* Variable Refresh Rate */
enum {
	ANA38407_VRR_MODE_NS,
	ANA38407_VRR_MODE_HS,
	MAX_ANA38407_VRR_MODE,
};

enum {
	ANA38407_VRR_120HS,
	ANA38407_VRR_60HS_120HS_TE_HW_SKIP_1,
	ANA38407_VRR_60HS,
	ANA38407_VRR_30HS_60HS_TE_HW_SKIP_1,
	ANA38407_VRR_30HS_120HS_TE_HW_SKIP_3,
	MAX_ANA38407_VRR,
};

enum {
	ANA38407_RESOL_2800x1752,
};

enum {
	ANA38407_RESOL_2960x1848,
};

enum {
	ANA38407_VRR_KEY_REFRESH_RATE,
	ANA38407_VRR_KEY_REFRESH_MODE,
	ANA38407_VRR_KEY_TE_SW_SKIP_COUNT,
	ANA38407_VRR_KEY_TE_HW_SKIP_COUNT,
	MAX_ANA38407_VRR_KEY,
};

static u32 ANA38407_VRR_FPS[MAX_ANA38407_VRR][MAX_ANA38407_VRR_KEY] = {
	[ANA38407_VRR_120HS] = { 120, VRR_HS_MODE, 0, 0 },
	[ANA38407_VRR_60HS_120HS_TE_HW_SKIP_1] = { 120, VRR_HS_MODE, 0, 1 },
	[ANA38407_VRR_60HS] = { 60, VRR_HS_MODE, 0, 0 },
	[ANA38407_VRR_30HS_60HS_TE_HW_SKIP_1] = { 60, VRR_HS_MODE, 0, 1 },
	[ANA38407_VRR_30HS_120HS_TE_HW_SKIP_3] = { 120, VRR_HS_MODE, 0, 3 },
};

/* use according to adaptive_control sysfs */
enum {
	ANA38407_ACL_OPR_0,
	ANA38407_ACL_OPR_1,
	ANA38407_ACL_OPR_2,
	ANA38407_ACL_OPR_3,
	MAX_ANA38407_ACL_OPR
};

enum {
	ANA38407_HS_CLK_1524 = 0,
	ANA38407_HS_CLK_1536,
	ANA38407_HS_CLK_1587,
	MAX_ANA38407_HS_CLK
};

int ana38407_get_octa_id(struct panel_device *panel, void *buf);
int ana38407_get_cell_id(struct panel_device *panel, void *buf);
int ana38407_get_manufacture_code(struct panel_device *panel, void *buf);
int ana38407_get_manufacture_date(struct panel_device *panel, void *buf);
int ana38407_init(struct common_panel_info *cpi);
bool ana38407_gct_chksum_is_valid(struct panel_device *panel, void *data);
#ifdef CONFIG_USDM_PANEL_FIRMWARE_UPDATE
int ana38407_gts10u_tcon_firmware_update_fw_ver_read_only(struct panel_device *panel, void *data);
int ana38407_gts10u_tcon_firmware_info_print(struct panel_device *panel, void *data);
#endif
#endif /* __ANA38407_H__ */
