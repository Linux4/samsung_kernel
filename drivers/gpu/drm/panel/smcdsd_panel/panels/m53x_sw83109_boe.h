/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SW83109_BOE_00_H__
#define __SW83109_BOE_00_H__

#include <linux/types.h>
#include <linux/kernel.h>
#include <drm/drm_mipi_dsi.h>

#if defined(CONFIG_SMCDSD_DPUI)
#include "dpui.h"
#endif

#if defined(CONFIG_SMCDSD_MDNIE)
#include "mdnie.h"
#include "m53x_sw83109_boe_00_mdnie.h"
#endif

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
#include "m53x_sw83109_boe_00_freq.h"
#endif

#include "m53x_sw83109_boe_00_param.h"

struct bit_info {
	unsigned int reg;
	unsigned int len;
	char **print;
	unsigned int expect;
	unsigned int offset;
	unsigned int g_para;
	unsigned int invert;
	unsigned int mask;
	unsigned int result;
	union {
		unsigned int reserved;
		unsigned int dpui_key;
	};
};

enum {
	LDI_BIT_ENUM_05,	LDI_BIT_ENUM_RDNUMPE = LDI_BIT_ENUM_05,
	LDI_BIT_ENUM_0A,	LDI_BIT_ENUM_RDDPM = LDI_BIT_ENUM_0A,
	LDI_BIT_ENUM_0E,	LDI_BIT_ENUM_RDDSM = LDI_BIT_ENUM_0E,
	LDI_BIT_ENUM_0F,	LDI_BIT_ENUM_RDDSDR = LDI_BIT_ENUM_0F,
	LDI_BIT_ENUM_EE,	LDI_BIT_ENUM_ESDERR = LDI_BIT_ENUM_EE,
	LDI_BIT_ENUM_EA,	LDI_BIT_ENUM_DSIERR = LDI_BIT_ENUM_EA,
	LDI_BIT_ENUM_9F,	LDI_BIT_ENUM_RDERRFLAG = LDI_BIT_ENUM_9F,
	LDI_BIT_ENUM_MAX
};

static char *LDI_BIT_DESC_05[BITS_PER_BYTE] = {
	[0 ... 6] = "number of corrupted packets",
	[7] = "overflow on number of corrupted packets",
};

static char *LDI_BIT_DESC_0A[BITS_PER_BYTE] = {
	[2] = "Display is Off",		/* 0b : Display is Off */
	//[7] = "Booster Off or has a fault",	/* 0b : Booster Off or has a fault. */
};

static char *LDI_BIT_DESC_0E[BITS_PER_BYTE] = {
	[0] = "Error on DSI",
};

static char *LDI_BIT_DESC_0F[BITS_PER_BYTE] = {
	[7] = "Register Loading Detection",	/* 0b : EEPROM and Register values NOT Same */
};

static char *LDI_BIT_DESC_EE[BITS_PER_BYTE] = {
	[2] = "VLIN3 error",
	[3] = "ELVDD error",
	[6] = "VLIN1 error",
};

static char *LDI_BIT_DESC_EA[BITS_PER_BYTE * 2] = {
	[0] = "SoT Error",
	[1] = "SoT Sync Error",
	[2] = "EoT Sync Error",
	[3] = "Escape Mode Entry Command Error",
	[4] = "Low-Power Transmit Sync Error",
	[5] = "HS RX Timeout",
	[6] = "False Control Error",
	[7] = "Data Lane Contention Detection",
	[8] = "ECC Error, single-bit (detected and corrected)",
	[9] = "ECC Error, multi-bit (detected, not corrected)",
	[10] = "Checksum Error",
	[11] = "DSI Data Type Not Recognized",
	[12] = "DSI VC ID Invalid",
	[13] = "Data P Lane Contention Detetion",
	[14] = "Data Lane Contention Detection",
	[15] = "DSI Protocol Violation",
};

static char *LDI_BIT_DESC_9F[BITS_PER_BYTE * 2] = {
//	[0] = "UCS_CHK",	/* UCS checksum Error flag */
//	[1] = "VGHL_CHK",	/* VGH, VGL Level Error flag */
	[2] = "INPWR_CHK",	/* Input level error flag */
//	[3] = "PS_CHK",		/* Power sequence Error flag */
//	[4] = "HS_CHK",		/* Hsync Time out flag */
//	[5] = "VS_CHK",		/* Vsync Time out flag */
//	[8] = "PCD_OUT",	/* PCD output value */
};

static struct bit_info ldi_bit_info_list[LDI_BIT_ENUM_MAX] = {
	[LDI_BIT_ENUM_05] = {0x05, 1, LDI_BIT_DESC_05, 0x00, },
	[LDI_BIT_ENUM_0A] = {0x0A, 1, LDI_BIT_DESC_0A, 0x1C, .invert = BIT(2), },
	[LDI_BIT_ENUM_0E] = {0x0E, 1, LDI_BIT_DESC_0E, 0x00, },
	[LDI_BIT_ENUM_0F] = {0x0F, 1, LDI_BIT_DESC_0F, 0xC0, .invert = BIT(7), },
	//[LDI_BIT_ENUM_EE] = {0xEE, 1, LDI_BIT_DESC_EE, 0xC0, },
	//[LDI_BIT_ENUM_EA] = {0xEA, 2, LDI_BIT_DESC_EA, 0x00, },
	[LDI_BIT_ENUM_9F] = {0x9F, 2, LDI_BIT_DESC_9F, 0x00, },
};

#if defined(CONFIG_SMCDSD_DPUI)
/*
 * ESD_ERROR[0] = MIPI DSI error is occurred by ESD.
 * ESD_ERROR[1] = HS CLK lane error is occurred by ESD.
 * ESD_ERROR[2] = VLIN3 error is occurred by ESD.
 * ESD_ERROR[3] = ELVDD error is occurred by ESD.
 * ESD_ERROR[4] = CHECK_SUM error is occurred by ESD.
 * ESD_ERROR[5] = Internal HSYNC error is occurred by ESD.
 * ESD_ERROR[6] = VLIN1 error is occurred by ESD
 */

static struct bit_info ldi_bit_dpui_list[] = {
	{0x05, .mask = GENMASK(7, 0), .dpui_key = DPUI_KEY_PNDSIE, },		/* panel dsi error count */
	{0x0F, .mask = BIT(7), .invert = BIT(7), .dpui_key = DPUI_KEY_PNSDRE, },	/* panel OTP loading error count */
//	{0xEE, .mask = BIT(2), .dpui_key = DPUI_KEY_PNVLO3E, },			/* panel VLOUT3 error count */
//	{0xEE, .mask = BIT(3), .dpui_key = DPUI_KEY_PNELVDE, },			/* panel ELVDD error count */
//	{0xEE, .mask = BIT(6), .dpui_key = DPUI_KEY_PNVLI1E, },			/* panel VLIN1 error count */
//	{0xEE, .mask = BIT(2)|BIT(3)|BIT(6), .dpui_key = DPUI_KEY_PNESDE, },	/* panel ESD error count */
};
#endif

enum {
//	MSG_IDX_ACL,
	MSG_IDX_BRIGHTNESS,
//	MSG_IDX_TEMPERATURE,
//	MSG_IDX_VRR,
//	MSG_IDX_AOD,
//	MSG_IDX_FFC,

	/* do not modify below line */
	MSG_IDX_LAST,
	MSG_IDX_BASE = MSG_IDX_LAST,
	MSG_IDX_MAX,
};

enum {
//	MSG_BIT_ACL = BIT(MSG_IDX_ACL),
	MSG_BIT_BRIGHTNESS = BIT(MSG_IDX_BRIGHTNESS),
//	MSG_BIT_TEMPERATURE = BIT(MSG_IDX_TEMPERATURE),
//	MSG_BIT_VRR = BIT(MSG_IDX_VRR),
//	MSG_BIT_AOD = BIT(MSG_IDX_AOD),
//	MSG_BIT_FFC = BIT(MSG_IDX_FFC),

	/* do not modify below line */
	MSG_BIT_LAST = BIT(MSG_IDX_LAST),
	MSG_BIT_BASE = MSG_BIT_LAST,
	MSG_BIT_MAX,
};

static struct msg_package *PACKAGE_SW83109_BOE_00[MSG_IDX_MAX] = {
	[MSG_IDX_BASE] = PACKAGE_SW83109_BOE,
	[MSG_IDX_BRIGHTNESS] = PACKAGE_SW83109_BOE_00_BRIGHTNESS,
};

static struct msg_package **PACKAGE_LIST = PACKAGE_SW83109_BOE_00;

#if 0
static struct msg_package PACKAGE_SW83109_BOE_00_DUMP[MSG_IDX_MAX] = {
	[MSG_IDX_BASE] = { __ADDRESS(PACKAGE_SW83109_BOE) },
	[MSG_IDX_BRIGHTNESS] = { __ADDRESS(PACKAGE_SW83109_BOE_00_BRIGHTNESS) },
};

static struct msg_package *PACKAGE_LIST_DUMP = PACKAGE_SW83109_BOE_00_DUMP;
#endif

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
enum {
	FFC_OFF,
	FFC_UPDATE,
};

enum {
	DYNAMIC_MIPI_INDEX_0,
	DYNAMIC_MIPI_INDEX_1,
	DYNAMIC_MIPI_INDEX_2,
	DYNAMIC_MIPI_INDEX_MAX,
};
#endif

#endif /* __SW83109_BOE_00_H__ */
