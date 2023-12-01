/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2022 Samsung Electronics Co., Ltd.
 */
#ifndef _UFS_CAL_TABLE_
#define _UFS_CAL_TABLE_

#define UFS_CAL_VER 0
#include "ufs-cal-if.h"

struct ufs_cal_phy_cfg {
	u32 mib;
	u32 addr;
	u32 val;
	u32 flg;
	u32 lyr;
	u8 board;
};

enum {
	PHY_CFG_NONE = 0,
	PHY_PCS_COMN,
	PHY_PCS_RXTX,
	PHY_PMA_COMN,
	PHY_PMA_TRSV,
	PHY_PLL_WAIT,
	PHY_CDR_WAIT,
	PHY_CDR_AFC_WAIT,
	UNIPRO_STD_MIB,
	UNIPRO_DBG_MIB,
	UNIPRO_DBG_APB,

	PHY_PCS_RX,
	PHY_PCS_TX,
	PHY_PCS_RX_PRD,
	PHY_PCS_TX_PRD,
	UNIPRO_DBG_PRD,
	PHY_PMA_TRSV_LANE1_SQ_OFF,
	PHY_PMA_TRSV_SQ,
	COMMON_WAIT,

	PHY_PCS_RX_LR_PRD,
	PHY_PCS_TX_LR_PRD,
	PHY_PCS_RX_PRD_ROUND_OFF,
	PHY_PCS_TX_PRD_ROUND_OFF,
	UNIPRO_ADAPT_LENGTH,
	PHY_EMB_CDR_WAIT,
	PHY_EMB_CAL_WAIT,

	HCI_AH8_THIBERN,
	HCI_AH8_REFCLKGATINGTING,
	HCI_AH8_ACTIVE_LANE,
	HCI_AH8_CMN,
	HCI_AH8_TRSV,
	HCI_STD,
};

enum {
	PMD_PWM_G1 = 0,
	PMD_PWM_G2,
	PMD_PWM_G3,
	PMD_PWM_G4,
	PMD_PWM_G5,
	PMD_PWM,

	PMD_HS_G1,
	PMD_HS_G2,
	PMD_HS_G3,
	PMD_HS_G4,
	PMD_HS,

	PMD_ALL,
};

static struct ufs_cal_phy_cfg init_cfg_evt0[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x44, 0x00, PMD_ALL, UNIPRO_DBG_PRD, BRD_ALL},

	{0x200, 0x2800, 0x40, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x12, 0x2048, 0x00, PMD_ALL, PHY_PCS_RX_PRD_ROUND_OFF, BRD_ALL},
	{0xAA, 0x22A8, 0x00, PMD_ALL, PHY_PCS_TX_PRD_ROUND_OFF, BRD_ALL},
	{0xA9, 0x22A4, 0x02, PMD_ALL, PHY_PCS_TX, BRD_ALL},
	{0xAB, 0x22AC, 0x00, PMD_ALL, PHY_PCS_TX_LR_PRD, BRD_ALL},
	{0x11, 0x2044, 0x00, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x1B, 0x206C, 0x00, PMD_ALL, PHY_PCS_RX_LR_PRD, BRD_ALL},
	{0x2F, 0x20BC, 0x69, PMD_ALL, PHY_PCS_RX, BRD_ALL},

	{0x84, 0x2210, 0x01, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x04, 0x2010, 0x01, PMD_ALL, PHY_PCS_TX, BRD_ALL},
	{0x25, 0x2094, 0xF6, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x7F, 0x21FC, 0x00, PMD_ALL, PHY_PCS_TX, BRD_ALL},
	{0x200, 0x2800, 0x0, PMD_ALL, PHY_PCS_COMN, BRD_ALL},

	{0x155E, 0x3178, 0x0, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},
	{0x3000, 0x5000, 0x0, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},
	{0x3001, 0x5004, 0x1, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},
	{0x4021, 0x6084, 0x1, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},
	{0x4020, 0x6080, 0x1, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},

	{0x0000, 0x10C, 0x10, PMD_ALL, PHY_PMA_COMN, BRD_ALL},

	{0x0000, 0x0F0, 0x14, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x118, 0x48, PMD_ALL, PHY_PMA_COMN, BRD_ALL},

	{0x0000, 0x800, 0x00, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x804, 0x06, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x808, 0x06, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x80C, 0x0A, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x810, 0x00, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x814, 0x01, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x81C, 0x0C, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xB84, 0xC0, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x8B4, 0xB8, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x8D0, 0x60, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x8E0, 0x13, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x8E4, 0x48, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x8E8, 0x01, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x8EC, 0x25, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x8F0, 0x2A, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x8F4, 0x01, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x8F8, 0x13, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x8FC, 0x13, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x900, 0x4A, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x90C, 0x40, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x910, 0x02, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x974, 0x00, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x978, 0x3F, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x97C, 0xFF, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x9CC, 0x33, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x9D0, 0x50, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0xA10, 0x02, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xA14, 0x02, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0xA88, 0x04, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x9F4, 0x01, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xBE8, 0x01, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0xA18, 0x03, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xA1C, 0x03, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xA20, 0x03, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xA24, 0x03, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0xACC, 0x04, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xAD8, 0x0B, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xADC, 0x0B, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xAE0, 0x0B, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xAE4, 0x0B, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xAE8, 0x0B, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xAEC, 0x06, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xAF0, 0x06, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xAF4, 0x06, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xAF8, 0x06, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0xB90, 0x1A, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xBB4, 0x25, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x9A4, 0x1A, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xBD0, 0x2F, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0xD2C, 0x01, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xD30, 0x23, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xD34, 0x23, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xD38, 0x45, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xD3C, 0x00, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xD40, 0x31, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xD44, 0x00, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xD48, 0x02, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xD4C, 0x00, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xD50, 0x01, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x9BC, 0xF0, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xAB0, 0x33, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x10C, 0x18, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x10C, 0x00, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0xCE0, 0x08, PMD_ALL, PHY_EMB_CAL_WAIT, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg init_cfg_evt1[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_init_cfg_evt0[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x15D2, 0x3348, 0x0, PMD_ALL, UNIPRO_ADAPT_LENGTH, BRD_ALL},
	{0x15D3, 0x334C, 0x0, PMD_ALL, UNIPRO_ADAPT_LENGTH, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_init_cfg_evt1[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_pwm[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x2041, 0x4104, 8064, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x2042, 0x4108, 28224, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x2043, 0x410C, 20160, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B0, 0x32C0, 12000, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B1, 0x32C4, 32000, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B2, 0x32C8, 16000, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},

	{0x0000, 0x7888, 8064, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x788C, 28224, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x7890, 20160, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78B8, 12000, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78BC, 32000, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78C0, 16000, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_pwm[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x20, 0x60, PMD_PWM, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x888, 0x08, PMD_PWM, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x918, 0x01, PMD_PWM, PHY_PMA_TRSV, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_a[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x15D4, 0x3350, 0x1, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},

	{0x2041, 0x4104, 8064, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x2042, 0x4108, 28224, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x2043, 0x410C, 20160, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B0, 0x32C0, 12000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B1, 0x32C4, 32000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B2, 0x32C8, 16000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},

	{0x0000, 0x7888, 8064, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x788C, 28224, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x7890, 20160, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78B8, 12000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78BC, 32000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78C0, 16000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},

	{0x0000, 0xDA4, 0x11, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x918, 0x03, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_a[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0xCE4, 0x08, PMD_HS, PHY_EMB_CDR_WAIT, BRD_ALL},
	{0x0000, 0x918, 0x01, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_b[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x15D4, 0x3350, 0x1, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},

	{0x2041, 0x4104, 8064, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x2042, 0x4108, 28224, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x2043, 0x410C, 20160, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B0, 0x32C0, 12000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B1, 0x32C4, 32000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B2, 0x32C8, 16000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},

	{0x0000, 0x7888, 8064, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x788C, 28224, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x7890, 20160, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78B8, 12000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78BC, 32000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78C0, 16000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},

	{0x0000, 0xDA4, 0x11, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x918, 0x03, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_b[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0xCE4, 0x08, PMD_HS, PHY_EMB_CDR_WAIT, BRD_ALL},
	{0x0000, 0x918, 0x01, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_ah8_cfg_evt0[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x504, 0x00, PMD_HS, HCI_AH8_THIBERN, BRD_ALL},
	{0x0000, 0x508, 0x00, PMD_HS, HCI_AH8_REFCLKGATINGTING, BRD_ALL},
	{0x0000, 0x51C, 0x00, PMD_HS, HCI_AH8_ACTIVE_LANE, BRD_ALL},
	{0x0000, 0x524, 0x35B60, PMD_HS, HCI_AH8_CMN, BRD_ALL},

	// Retry Number & Access Number
	{0x0000, 0x520, 0x16001B, PMD_HS, HCI_AH8_CMN, BRD_ALL},

	// need to lane# check
	{0x0000, 0x610, 0x00000988, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x650, 0x00000008, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x614, 0x00000994, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x654, 0x0000000A, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x618, 0x00000004, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x658, 0x00000008, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x61C, 0x00000000, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x65C, 0x00000086, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x620, 0x00000020, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x660, 0x00000060, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x624, 0x00000888, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x664, 0x00000008, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x628, 0x00000918, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x668, 0x00000001, PMD_HS, HCI_AH8_CMN, BRD_ALL},

	{0x0000, 0x690, 0x00000000, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6D0, 0x000000C6, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x694, 0x401E0000, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6D4, 0x00000000, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x698, 0x00000004, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6D8, 0x0000000C, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x69C, 0x00000988, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x6DC, 0x00000000, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6A0, 0x00000994, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x6E0, 0x00000000, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6A4, 0x00000020, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6E4, 0x000000E0, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6A8, 0x00000918, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x6E8, 0x00000003, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6AC, 0x00000888, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x6EC, 0x00000010, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6B0, 0x40020000, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6F0, 0x00000000, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6B4, 0x00000888, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x6F4, 0x00000018, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6B8, 0x40140000, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6F8, 0x00000000, PMD_HS, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x6BC, 0x30000CE4, PMD_HS, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x6FC, 0x00080008, PMD_HS, HCI_AH8_CMN, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_ah8_cfg_evt1[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg lane1_sq_off[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x988, 0x08, PMD_ALL, PHY_PMA_TRSV_LANE1_SQ_OFF, BRD_ALL},
	{0x0000, 0x994, 0x0A, PMD_ALL, PHY_PMA_TRSV_LANE1_SQ_OFF, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_h8_enter[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x988, 0x08, PMD_ALL, PHY_PMA_TRSV_SQ, BRD_ALL},
	{0x0000, 0x994, 0x0A, PMD_ALL, PHY_PMA_TRSV_SQ, BRD_ALL},
	{0x0000, 0x04, 0x08, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x00, 0x86, PMD_ALL, PHY_PMA_COMN, BRD_ALL},

	{0x0000, 0x20, 0x60, PMD_HS, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x888, 0x08, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x918, 0x01, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg pre_h8_exit[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x00, 0xC6, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x04, 0x0C, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x988, 0x00, PMD_ALL, PHY_PMA_TRSV_SQ, BRD_ALL},
	{0x0000, 0x994, 0x00, PMD_ALL, PHY_PMA_TRSV_SQ, BRD_ALL},

	{0x0000, 0x20, 0xE0, PMD_HS, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x918, 0x03, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x888, 0x18, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0xCE4, 0x08, PMD_HS, PHY_EMB_CDR_WAIT, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_init[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0xBB4, 0x23, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x868, 0x02, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x9A8, 0xA1, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x9AC, 0x40, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_set_1[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0xBB4, 0x2B, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x888, 0x06, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_set_2[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x9BC, 0x52, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x9A8, 0xA7, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x8AC, 0xC3, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg eom_prepare[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0xBC0, 0x00, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xA88, 0x05, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x93C, 0x0F, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x940, 0x4F, PMD_HS_G4, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x940, 0x2F, PMD_HS_G3, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x940, 0x1F, PMD_HS_G2, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x940, 0x0F, PMD_HS_G1, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0xB64, 0xE3, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xB68, 0x04, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0xB6C, 0x00, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg init_cfg_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_init_cfg_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_pwm_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_pwm_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_a_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_a_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_b_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_b_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg lane1_sq_off_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_h8_enter_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg pre_h8_exit_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_init_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_set_1_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_set_2_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};
#endif	/* _UFS_CAL_TABLE_ */
