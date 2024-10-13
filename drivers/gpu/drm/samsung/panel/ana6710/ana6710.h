/*
 * linux/drivers/video/fbdev/exynos/panel/ana6710/ana6710.h
 *
 * Header file for ANA6710 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ANA6710_H__
#define __ANA6710_H__

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
 * XXX 1st param => ANA6710_XXX_OFS (0)
 * XXX 2nd param => ANA6710_XXX_OFS (1)
 * XXX 36th param => ANA6710_XXX_OFS (35)
 */

#define ANA6710_ADDR_OFS	(0)
#define ANA6710_ADDR_LEN	(1)
#define ANA6710_DATA_OFS	(ANA6710_ADDR_OFS + ANA6710_ADDR_LEN)

#define ANA6710_DATE_REG			0xA1
#define ANA6710_DATE_OFS			4
#define ANA6710_DATE_LEN			(PANEL_DATE_LEN)

#define ANA6710_COORDINATE_REG		0xA1
#define ANA6710_COORDINATE_OFS		0
#define ANA6710_COORDINATE_LEN		(PANEL_COORD_LEN)

#define ANA6710_ID_REG				0x04
#define ANA6710_ID_OFS				0
#define ANA6710_ID_LEN				(PANEL_ID_LEN)

#define ANA6710_CODE_REG			0xD6
#define ANA6710_CODE_OFS			0
#define ANA6710_CODE_LEN			5

#define ANA6710_OCTA_ID_0_REG			0xA1
#define ANA6710_OCTA_ID_0_OFS			11
#define ANA6710_OCTA_ID_0_LEN			4

#define ANA6710_OCTA_ID_1_REG			0xE2
#define ANA6710_OCTA_ID_1_OFS			22
#define ANA6710_OCTA_ID_1_LEN			16

#define ANA6710_OCTA_ID_LEN			(ANA6710_OCTA_ID_0_LEN + ANA6710_OCTA_ID_1_LEN)

/* for panel dump */
#define ANA6710_RDDPM_REG			0x0A
#define ANA6710_RDDPM_OFS			0
#define ANA6710_RDDPM_LEN			(PANEL_RDDPM_LEN)

#define ANA6710_RDDSM_REG			0x0E
#define ANA6710_RDDSM_OFS			0
#define ANA6710_RDDSM_LEN			(PANEL_RDDSM_LEN)

#define ANA6710_ERR_REG				0xE9
#define ANA6710_ERR_OFS				0
#define ANA6710_ERR_LEN				5

#define ANA6710_ERR_FG_REG			0xEE
#define ANA6710_ERR_FG_OFS			0
#define ANA6710_ERR_FG_LEN			1

#define ANA6710_DSI_ERR_REG			0x05
#define ANA6710_DSI_ERR_OFS			0
#define ANA6710_DSI_ERR_LEN			1

#define ANA6710_SELF_DIAG_REG			0x0F
#define ANA6710_SELF_DIAG_OFS			0
#define ANA6710_SELF_DIAG_LEN			1

#define ANA6710_SELF_MASK_CHECKSUM_REG		0x14
#define ANA6710_SELF_MASK_CHECKSUM_OFS		0
#define ANA6710_SELF_MASK_CHECKSUM_LEN		2

#define ANA6710_SELF_MASK_CRC_REG		0x7F
#define ANA6710_SELF_MASK_CRC_OFS	6
#define ANA6710_SELF_MASK_CRC_LEN		4

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
#define ANA6710_DECODER_TEST1_REG			0xF2
#define ANA6710_DECODER_TEST1_OFS			0x21
#define ANA6710_DECODER_TEST1_LEN			8

#define ANA6710_DECODER_TEST2_REG			0xF2
#define ANA6710_DECODER_TEST2_OFS			0x21
#define ANA6710_DECODER_TEST2_LEN			8

#define ANA6710_DECODER_TEST3_REG			0xF6
#define ANA6710_DECODER_TEST3_OFS			0xD6
#define ANA6710_DECODER_TEST3_LEN			2
#endif

#ifdef CONFIG_USDM_FACTORY_CCD_TEST
#define ANA6710_CCD_STATE_REG				0xCC
#define ANA6710_CCD_STATE_OFS				2
#define ANA6710_CCD_STATE_LEN				1
#define ANA6710_CCD_STATE_PASS_LIST_LEN		1
#endif

#ifdef CONFIG_USDM_FACTORY_ECC_TEST
#define ANA6710_ECC_TEST_REG				0xF2
#define ANA6710_ECC_TEST_OFS				0x21
#define ANA6710_ECC_TEST_LEN				8
#endif

#define ANA6710_ANALOG_GAMMA_REG		(0x67)
#define ANA6710_ANALOG_GAMMA_OFS		(0xA6)
#define ANA6710_ANALOG_GAMMA_LEN		(78)

#ifdef CONFIG_USDM_DDI_CMDLOG
#define ANA6710_CMDLOG_REG			0x9C
#define ANA6710_CMDLOG_OFS			0
#define ANA6710_CMDLOG_LEN			0x80
#endif

enum ana6710_function {
	ANA6710_MAPTBL_INIT_GAMMA_MODE2_BRT,
	ANA6710_MAPTBL_GETIDX_HBM_TRANSITION,
	ANA6710_MAPTBL_GETIDX_ACL_OPR,
	ANA6710_MAPTBL_INIT_LPM_BRT,
	ANA6710_MAPTBL_GETIDX_LPM_BRT,
#if defined(CONFIG_USDM_FACTORY) && defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
	ANA6710_MAPTBL_GETIDX_FAST_DISCHARGE,
#endif
	ANA6710_MAPTBL_GETIDX_VRR_FPS,
	ANA6710_MAPTBL_GETIDX_VRR,
	ANA6710_MAPTBL_GETIDX_FFC,
	ANA6710_MAPTBL_INIT_ANALOG_GAMMA,
	ANA6710_MAPTBL_GETIDX_IRC_MODE,
	MAX_ANA6710_FUNCTION,
};

extern struct pnobj_func ana6710_function_table[MAX_ANA6710_FUNCTION];

#undef DDI_FUNC
#define DDI_FUNC(_index) (ana6710_function_table[_index])

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
	DIMMING_SPEED,
	LPM_NIT_MAPTBL,
	LPM_MODE_MAPTBL,
	HBM_ONOFF_MAPTBL,
#if defined(CONFIG_USDM_FACTORY) && defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
	FAST_DISCHARGE_MAPTBL,
#endif
	SET_FFC_MAPTBL,
	IRC_MODE_MAPTBL,
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
	ANA6710_VRR_FPS_120,
	ANA6710_VRR_FPS_60,
	MAX_ANA6710_VRR_FPS,
};


enum {
	ANA6710_SMOOTH_DIMMING_OFF,
	ANA6710_SMOOTH_DIMMING_ON,
	MAX_ANA6710_SMOOTH_DIMMING,
};

static u8 ANA6710_ID[ANA6710_ID_LEN];
static u8 ANA6710_COORDINATE[ANA6710_COORDINATE_LEN];
static u8 ANA6710_CODE[ANA6710_CODE_LEN];
static u8 ANA6710_DATE[ANA6710_DATE_LEN];
static u8 ANA6710_OCTA_ID[ANA6710_OCTA_ID_LEN];
/* for brightness debugging */
static u8 ANA6710_RDDPM[ANA6710_RDDPM_LEN];
static u8 ANA6710_RDDSM[ANA6710_RDDSM_LEN];
static u8 ANA6710_ERR[ANA6710_ERR_LEN];
static u8 ANA6710_ERR_FG[ANA6710_ERR_FG_LEN];
static u8 ANA6710_DSI_ERR[ANA6710_DSI_ERR_LEN];
static u8 ANA6710_SELF_DIAG[ANA6710_SELF_DIAG_LEN];
static u8 ANA6710_SELF_MASK_CHECKSUM[ANA6710_SELF_MASK_CHECKSUM_LEN];
static u8 ANA6710_SELF_MASK_CRC[ANA6710_SELF_MASK_CRC_LEN];
static u8 ANA6710_ANALOG_GAMMA_120HS[ANA6710_ANALOG_GAMMA_LEN];

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static u8 ANA6710_DECODER_TEST1[ANA6710_DECODER_TEST1_LEN];
static u8 ANA6710_DECODER_TEST2[ANA6710_DECODER_TEST2_LEN];
static u8 ANA6710_DECODER_TEST3[ANA6710_DECODER_TEST3_LEN];
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static u8 ANA6710_CCD_STATE[ANA6710_CCD_STATE_LEN];
static u8 ANA6710_CCD_CHKSUM_PASS_LIST[ANA6710_CCD_STATE_PASS_LIST_LEN] = { 0x00, };
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
static u8 ANA6710_ECC_TEST[ANA6710_ECC_TEST_LEN];
#endif

static struct rdinfo ana6710_rditbl[MAX_READTBL] = {
	[READ_ID] = RDINFO_INIT(id, DSI_PKT_TYPE_RD, ANA6710_ID_REG, ANA6710_ID_OFS, ANA6710_ID_LEN),
	[READ_COORDINATE] = RDINFO_INIT(coordinate, DSI_PKT_TYPE_RD, ANA6710_COORDINATE_REG, ANA6710_COORDINATE_OFS, ANA6710_COORDINATE_LEN),
	[READ_CODE] = RDINFO_INIT(code, DSI_PKT_TYPE_RD, ANA6710_CODE_REG, ANA6710_CODE_OFS, ANA6710_CODE_LEN),
	[READ_DATE] = RDINFO_INIT(date, DSI_PKT_TYPE_RD, ANA6710_DATE_REG, ANA6710_DATE_OFS, ANA6710_DATE_LEN),
	[READ_OCTA_ID_0] = RDINFO_INIT(octa_id_0, DSI_PKT_TYPE_RD, ANA6710_OCTA_ID_0_REG, ANA6710_OCTA_ID_0_OFS, ANA6710_OCTA_ID_0_LEN),
	[READ_OCTA_ID_1] = RDINFO_INIT(octa_id_1, DSI_PKT_TYPE_RD, ANA6710_OCTA_ID_1_REG, ANA6710_OCTA_ID_1_OFS, ANA6710_OCTA_ID_1_LEN),
	[READ_RDDPM] = RDINFO_INIT(rddpm, DSI_PKT_TYPE_RD, ANA6710_RDDPM_REG, ANA6710_RDDPM_OFS, ANA6710_RDDPM_LEN),
	[READ_RDDSM] = RDINFO_INIT(rddsm, DSI_PKT_TYPE_RD, ANA6710_RDDSM_REG, ANA6710_RDDSM_OFS, ANA6710_RDDSM_LEN),
	[READ_ERR] = RDINFO_INIT(err, DSI_PKT_TYPE_RD, ANA6710_ERR_REG, ANA6710_ERR_OFS, ANA6710_ERR_LEN),
	[READ_ERR_FG] = RDINFO_INIT(err_fg, DSI_PKT_TYPE_RD, ANA6710_ERR_FG_REG, ANA6710_ERR_FG_OFS, ANA6710_ERR_FG_LEN),
	[READ_DSI_ERR] = RDINFO_INIT(dsi_err, DSI_PKT_TYPE_RD, ANA6710_DSI_ERR_REG, ANA6710_DSI_ERR_OFS, ANA6710_DSI_ERR_LEN),
	[READ_SELF_DIAG] = RDINFO_INIT(self_diag, DSI_PKT_TYPE_RD, ANA6710_SELF_DIAG_REG, ANA6710_SELF_DIAG_OFS, ANA6710_SELF_DIAG_LEN),
	[READ_SELF_MASK_CHECKSUM] = RDINFO_INIT(self_mask_checksum, DSI_PKT_TYPE_RD, ANA6710_SELF_MASK_CHECKSUM_REG, ANA6710_SELF_MASK_CHECKSUM_OFS, ANA6710_SELF_MASK_CHECKSUM_LEN),
	[READ_SELF_MASK_CRC] = RDINFO_INIT(self_mask_crc, DSI_PKT_TYPE_RD, ANA6710_SELF_MASK_CRC_REG, ANA6710_SELF_MASK_CRC_OFS, ANA6710_SELF_MASK_CRC_LEN),
	[READ_ANALOG_GAMMA_120HS] = RDINFO_INIT(analog_gamma_120hs, DSI_PKT_TYPE_RD, ANA6710_ANALOG_GAMMA_REG, ANA6710_ANALOG_GAMMA_OFS, ANA6710_ANALOG_GAMMA_LEN),
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	[READ_DECODER_TEST1] = RDINFO_INIT(decoder_test1, DSI_PKT_TYPE_RD, ANA6710_DECODER_TEST1_REG, ANA6710_DECODER_TEST1_OFS, ANA6710_DECODER_TEST1_LEN),
	[READ_DECODER_TEST2] = RDINFO_INIT(decoder_test2, DSI_PKT_TYPE_RD, ANA6710_DECODER_TEST2_REG, ANA6710_DECODER_TEST2_OFS, ANA6710_DECODER_TEST2_LEN),
	[READ_DECODER_TEST3] = RDINFO_INIT(decoder_test3, DSI_PKT_TYPE_RD, ANA6710_DECODER_TEST3_REG, ANA6710_DECODER_TEST3_OFS, ANA6710_DECODER_TEST3_LEN),
#endif

#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	[READ_CCD_STATE] = RDINFO_INIT(ccd_state, DSI_PKT_TYPE_RD, ANA6710_CCD_STATE_REG, ANA6710_CCD_STATE_OFS, ANA6710_CCD_STATE_LEN),
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
	[READ_ECC_TEST] = RDINFO_INIT(ecc_test, DSI_PKT_TYPE_RD, ANA6710_ECC_TEST_REG, ANA6710_ECC_TEST_OFS, ANA6710_ECC_TEST_LEN),
#endif
};

static DEFINE_RESUI(id, &ana6710_rditbl[READ_ID], 0);
static DEFINE_RESUI(coordinate, &ana6710_rditbl[READ_COORDINATE], 0);
static DEFINE_RESUI(code, &ana6710_rditbl[READ_CODE], 0);
static DEFINE_RESUI(elvss, &ana6710_rditbl[READ_ELVSS], 0);
static DEFINE_RESUI(mtp, &ana6710_rditbl[READ_MTP], 0);
static DEFINE_RESUI(date, &ana6710_rditbl[READ_DATE], 0);
static DECLARE_RESUI(octa_id) = {
	{ .rditbl = &ana6710_rditbl[READ_OCTA_ID_0], .offset = 0 },
	{ .rditbl = &ana6710_rditbl[READ_OCTA_ID_1], .offset = 4 },
};

/* for brightness debugging */
static DEFINE_RESUI(aor, &ana6710_rditbl[READ_AOR], 0);
static DEFINE_RESUI(vint, &ana6710_rditbl[READ_VINT], 0);
static DEFINE_RESUI(elvss_t, &ana6710_rditbl[READ_ELVSS_T], 0);
static DEFINE_RESUI(rddpm, &ana6710_rditbl[READ_RDDPM], 0);
static DEFINE_RESUI(rddsm, &ana6710_rditbl[READ_RDDSM], 0);
static DEFINE_RESUI(err, &ana6710_rditbl[READ_ERR], 0);
static DEFINE_RESUI(err_fg, &ana6710_rditbl[READ_ERR_FG], 0);
static DEFINE_RESUI(dsi_err, &ana6710_rditbl[READ_DSI_ERR], 0);
static DEFINE_RESUI(self_diag, &ana6710_rditbl[READ_SELF_DIAG], 0);
static DEFINE_RESUI(self_mask_checksum, &ana6710_rditbl[READ_SELF_MASK_CHECKSUM], 0);
static DEFINE_RESUI(self_mask_crc, &ana6710_rditbl[READ_SELF_MASK_CRC], 0);
static DEFINE_RESUI(analog_gamma_120hs, &ana6710_rditbl[READ_ANALOG_GAMMA_120HS], 0);
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static DEFINE_RESUI(decoder_test1, &ana6710_rditbl[READ_DECODER_TEST1], 0);
static DEFINE_RESUI(decoder_test2, &ana6710_rditbl[READ_DECODER_TEST2], 0);
static DEFINE_RESUI(decoder_test3, &ana6710_rditbl[READ_DECODER_TEST3], 0);
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static DEFINE_RESUI(ccd_state, &ana6710_rditbl[READ_CCD_STATE], 0);
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
static DEFINE_RESUI(ecc_test, &ana6710_rditbl[READ_ECC_TEST], 0);
#endif

static struct resinfo ana6710_restbl[MAX_RESTBL] = {
	[RES_ID] = RESINFO_INIT(id, ANA6710_ID, RESUI(id)),
	[RES_COORDINATE] = RESINFO_INIT(coordinate, ANA6710_COORDINATE, RESUI(coordinate)),
	[RES_CODE] = RESINFO_INIT(code, ANA6710_CODE, RESUI(code)),
	[RES_DATE] = RESINFO_INIT(date, ANA6710_DATE, RESUI(date)),
	[RES_OCTA_ID] = RESINFO_INIT(octa_id, ANA6710_OCTA_ID, RESUI(octa_id)),
	[RES_RDDPM] = RESINFO_INIT(rddpm, ANA6710_RDDPM, RESUI(rddpm)),
	[RES_RDDSM] = RESINFO_INIT(rddsm, ANA6710_RDDSM, RESUI(rddsm)),
	[RES_ERR] = RESINFO_INIT(err, ANA6710_ERR, RESUI(err)),
	[RES_ERR_FG] = RESINFO_INIT(err_fg, ANA6710_ERR_FG, RESUI(err_fg)),
	[RES_DSI_ERR] = RESINFO_INIT(dsi_err, ANA6710_DSI_ERR, RESUI(dsi_err)),
	[RES_SELF_DIAG] = RESINFO_INIT(self_diag, ANA6710_SELF_DIAG, RESUI(self_diag)),
	[RES_SELF_MASK_CHECKSUM] = RESINFO_INIT(self_mask_checksum, ANA6710_SELF_MASK_CHECKSUM, RESUI(self_mask_checksum)),
	[RES_SELF_MASK_CRC] = RESINFO_INIT(self_mask_crc, ANA6710_SELF_MASK_CRC, RESUI(self_mask_crc)),
	[RES_ANALOG_GAMMA_120HS] = RESINFO_INIT(analog_gamma_120hs, ANA6710_ANALOG_GAMMA_120HS, RESUI(analog_gamma_120hs)),
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	[RES_DECODER_TEST1] = RESINFO_INIT(decoder_test1, ANA6710_DECODER_TEST1, RESUI(decoder_test1)),
	[RES_DECODER_TEST2] = RESINFO_INIT(decoder_test2, ANA6710_DECODER_TEST2, RESUI(decoder_test2)),
	[RES_DECODER_TEST3] = RESINFO_INIT(decoder_test3, ANA6710_DECODER_TEST3, RESUI(decoder_test3)),
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	[RES_CCD_STATE] = RESINFO_INIT(ccd_state, ANA6710_CCD_STATE, RESUI(ccd_state)),
	[RES_CCD_CHKSUM_PASS_LIST] = RESINFO_IMMUTABLE_INIT(ccd_chksum_pass_list, ANA6710_CCD_CHKSUM_PASS_LIST),
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
	[RES_ECC_TEST] = RESINFO_INIT(ecc_test, ANA6710_ECC_TEST, RESUI(ecc_test)),
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
int ana6710_decoder_test(struct panel_device *panel, void *data, u32 len);
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
int ana6710_ecc_test(struct panel_device *panel, void *data, u32 len);
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

static struct dump_expect self_mask_crc_expects[] = {
};

static struct dump_expect pcd_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = 0x00, .msg = "PCD Occur" },
};

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
int ana6710_decoder_test(struct panel_device *panel, void *data, u32 len);
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
int ana6710_ecc_test(struct panel_device *panel, void *data, u32 len);
#endif

static struct dumpinfo ana6710_dmptbl[] = {
	[DUMP_RDDPM] = DUMPINFO_INIT_V2(rddpm, &ana6710_restbl[RES_RDDPM], &OLED_FUNC(OLED_DUMP_SHOW_RDDPM), rddpm_after_display_on_expects),
	[DUMP_RDDPM_SLEEP_IN] = DUMPINFO_INIT_V2(rddpm_sleep_in, &ana6710_restbl[RES_RDDPM], &OLED_FUNC(OLED_DUMP_SHOW_RDDPM_BEFORE_SLEEP_IN), rddpm_before_sleep_in_expects),
	[DUMP_RDDSM] = DUMPINFO_INIT_V2(rddsm, &ana6710_restbl[RES_RDDSM], &OLED_FUNC(OLED_DUMP_SHOW_RDDSM), rddsm_expects),
	[DUMP_ERR] = DUMPINFO_INIT_V2(err, &ana6710_restbl[RES_ERR], &OLED_FUNC(OLED_DUMP_SHOW_ERR), dsi_err_expects),
	[DUMP_ERR_FG] = DUMPINFO_INIT_V2(err_fg, &ana6710_restbl[RES_ERR_FG], &OLED_FUNC(OLED_DUMP_SHOW_ERR_FG), err_fg_expects),
	[DUMP_DSI_ERR] = DUMPINFO_INIT_V2(dsi_err, &ana6710_restbl[RES_DSI_ERR], &OLED_FUNC(OLED_DUMP_SHOW_DSI_ERR), dsie_cnt_expects),
	[DUMP_SELF_DIAG] = DUMPINFO_INIT_V2(self_diag, &ana6710_restbl[RES_SELF_DIAG], &OLED_FUNC(OLED_DUMP_SHOW_SELF_DIAG), self_diag_expects),
#ifdef CONFIG_USDM_DDI_CMDLOG
	[DUMP_CMDLOG] = DUMPINFO_INIT_V2(cmdlog, &ana6710_restbl[RES_CMDLOG], &OLED_FUNC(OLED_DUMP_SHOW_CMDLOG), cmdlog_expects),
#endif
	[DUMP_SELF_MASK_CRC] = DUMPINFO_INIT_V2(self_mask_crc, &ana6710_restbl[RES_SELF_MASK_CRC], &OLED_FUNC(OLED_DUMP_SHOW_SELF_MASK_CRC), self_mask_crc_expects),
};

/* Variable Refresh Rate */
enum {
	ANA6710_VRR_MODE_NS,
	ANA6710_VRR_MODE_HS,
	MAX_ANA6710_VRR_MODE,
};

enum {
	ANA6710_VRR_120HS,
	ANA6710_VRR_60HS_120HS_TE_HW_SKIP_1,
	ANA6710_VRR_60HS,
	ANA6710_VRR_30NS,
	MAX_ANA6710_VRR,
};

enum {
	ANA6710_RESOL_1080x2340,
};

enum {
	ANA6710_VRR_KEY_REFRESH_RATE,
	ANA6710_VRR_KEY_REFRESH_MODE,
	ANA6710_VRR_KEY_TE_SW_SKIP_COUNT,
	ANA6710_VRR_KEY_TE_HW_SKIP_COUNT,
	MAX_ANA6710_VRR_KEY,
};

static u32 ANA6710_VRR_FPS[MAX_ANA6710_VRR][MAX_ANA6710_VRR_KEY] = {
	[ANA6710_VRR_120HS] = { 120, VRR_HS_MODE, 0, 0 },
	[ANA6710_VRR_60HS_120HS_TE_HW_SKIP_1] = { 120, VRR_HS_MODE, 0, 1 },
	[ANA6710_VRR_60HS] = { 60, VRR_HS_MODE, 0, 0 },
	[ANA6710_VRR_30NS] = { 30, VRR_NORMAL_MODE, 0, 0 },
};

enum {
	ANA6710_ACL_DIM_OFF,
	ANA6710_ACL_DIM_ON,
	MAX_ANA6710_ACL_DIM,
};

/* use according to adaptive_control sysfs */
enum {
	ANA6710_ACL_OPR_0,
	ANA6710_ACL_OPR_1,
	ANA6710_ACL_OPR_2,
	ANA6710_ACL_OPR_3,
	MAX_ANA6710_ACL_OPR
};

enum {
	ANA6710_HS_CLK_1108 = 0,
	ANA6710_HS_CLK_1124,
	ANA6710_HS_CLK_1125,
	MAX_ANA6710_HS_CLK
};

int ana6710_get_octa_id(struct panel_device *panel, void *buf);
int ana6710_get_cell_id(struct panel_device *panel, void *buf);
int ana6710_get_manufacture_code(struct panel_device *panel, void *buf);
int ana6710_get_manufacture_date(struct panel_device *panel, void *buf);
int ana6710_init(struct common_panel_info *cpi);

#endif /* __ANA6710_H__ */
