// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 * Pablo v9.1 specific functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#if IS_ENABLED(CONFIG_EXYNOS_SCI)
#include <soc/samsung/exynos-sci.h>
#endif

#include "is-config.h"
#include "is-param.h"
#include "is-type.h"
#include "is-core.h"
#include "is-hw-chain.h"
#include "is-hw-settle-5nm-lpp.h"
#include "is-device-sensor.h"
#include "is-device-csi.h"
#include "is-device-ischain.h"

#include "../../interface/is-interface-ischain.h"
#include "../../hardware/is-hw-control.h"
#include "../../hardware/is-hw-mcscaler-v3.h"

/* SYSREG register description */
static const struct is_reg sysreg_csis_regs[SYSREG_CSIS_REG_CNT] = {
	{0x0108, "MEMCLK"},
	{0x0180, "POWER_FAIL_DETECT"},
	{0x031C, "CSIS_EMA_STATUS"},
	{0x0408, "CSIS_FRAME_ID_EN"},
	{0x040c, "CSISX6_SC_PDP_IN_EN"},
	{0x0410, "CSISX6_SC_CON0"},
	{0x0414, "CSISX6_SC_CON1"},
	{0x0418, "CSISX6_SC_CON2"},
	{0x041c, "CSISX6_SC_CON3"},
	{0x0420, "CSISX6_SC_CON4"},
	{0x0424, "CSISX6_SC_CON5"},
	{0x0428, "CSISX6_SC_CON6"},
	{0x042C, "CSISX6_SC_CON7"},
	{0x0430, "PDP_VC_CON0"},
	{0x0434, "PDP_VC_CON1"},
	{0x0438, "PDP_VC_CON2"},
	{0x043C, "PDP_VC_CON3"},
	{0x0440, "LH_GLUE_CON"},
	{0x0444, "LH_GLUE_INT_CON"},
	{0x0500, "MIPI_PHY_CON"},
};

static const struct is_reg sysreg_taa_regs[SYSREG_TAA_REG_CNT] = {
	{0X0108, "MEMCLK"},
	{0X0400, "TAA_USER_CON0"},
	{0X0404, "TAA_USER_CON1"},
	{0X0408, "LH_QACTIVE_CON"},
};

static const struct is_reg sysreg_mcfp0_regs[SYSREG_MCFP0_REG_CNT] = {
	{0x0108, "MEMCLK"},
};

static const struct is_reg sysreg_mcfp1_regs[SYSREG_MCFP1_REG_CNT] = {
	{0x0108, "MEMCLK"},
};

static const struct is_reg sysreg_dns_regs[SYSREG_DNS_REG_CNT] = {
	{0x0108, "MEMCLK"},
	{0x0400, "DNS_USER_CON0"},
	{0x0404, "DNS_USER_CON1"},
	{0x0428, "DNS_USER_CON5"},
};

static const struct is_reg sysreg_itp_regs[SYSREG_ITP_REG_CNT] = {
	{0x0108, "MEMCLK"},
	{0x0400, "ITP_USER_CON"},
};

static const struct is_reg sysreg_yuvpp_regs[SYSREG_YUVPP_REG_CNT] = {
	{0x0108, "MEMCLK"},
	{0x040C, "YUVPP_USER_CON3"},
};

static const struct is_reg sysreg_mcsc_regs[SYSREG_MCSC_REG_CNT] = {
	{0x0108, "MEMCLK"},
	{0x0408, "MCSC_USER_CON2"},
};

static const struct is_reg sysreg_lme_regs[SYSREG_LME_REG_CNT] = {
	{0x0108, "MEMCLK"},
	{0x0400, "LME_USER_CON0"},
};

static const struct is_field sysreg_csis_fields[SYSREG_CSIS_REG_FIELD_CNT] = {
	{"EN", 0, 1, RW, 0x1},
	{"PFD_AVDD085_MIPI_CPHY_STAT", 4, 1, RO, 0x0},
	{"PFD_AVDD085_MIPI_CPHY", 0, 1, RW, 0x0},
	{"SFR_ENABLE", 8, 1, RW, 0x0},
	{"WIDTH_DATA2REQ", 4, 2, RW, 0x3},
	{"EMA_BUSY", 0, 1, RO, 0x0},
	{"FRAME_ID_EN_CSIS0", 0, 1, RW, 0x0},
	{"PDP3_IN_CSIS5_EN", 23, 1, RW, 0x0},
	{"PDP3_IN_CSIS4_EN", 22, 1, RW, 0x0},
	{"PDP3_IN_CSIS3_EN", 21, 1, RW, 0x0},
	{"PDP3_IN_CSIS2_EN", 20, 1, RW, 0x0},
	{"PDP3_IN_CSIS1_EN", 19, 1, RW, 0x0},
	{"PDP3_IN_CSIS0_EN", 18, 1, RW, 0x0},
	{"PDP2_IN_CSIS5_EN", 17, 1, RW, 0x0},
	{"PDP2_IN_CSIS4_EN", 16, 1, RW, 0x0},
	{"PDP2_IN_CSIS3_EN", 15, 1, RW, 0x0},
	{"PDP2_IN_CSIS2_EN", 14, 1, RW, 0x0},
	{"PDP2_IN_CSIS1_EN", 13, 1, RW, 0x0},
	{"PDP2_IN_CSIS0_EN", 12, 1, RW, 0x0},
	{"PDP1_IN_CSIS5_EN", 11, 1, RW, 0x0},
	{"PDP1_IN_CSIS4_EN", 10, 1, RW, 0x0},
	{"PDP1_IN_CSIS3_EN", 9, 1, RW, 0x0},
	{"PDP1_IN_CSIS2_EN", 8, 1, RW, 0x0},
	{"PDP1_IN_CSIS1_EN", 7, 1, RW, 0x0},
	{"PDP1_IN_CSIS0_EN", 6, 1, RW, 0x0},
	{"PDP0_IN_CSIS5_EN", 5, 1, RW, 0x0},
	{"PDP0_IN_CSIS4_EN", 4, 1, RW, 0x0},
	{"PDP0_IN_CSIS3_EN", 3, 1, RW, 0x0},
	{"PDP0_IN_CSIS2_EN", 2, 1, RW, 0x0},
	{"PDP0_IN_CSIS1_EN", 1, 1, RW, 0x0},
	{"PDP0_IN_CSIS0_EN", 0, 1, RW, 0x0},
	{"GLUEMUX_PDP0_VAL", 0, 3, RW, 0x0},
	{"GLUEMUX_PDP1_VAL", 0, 3, RW, 0x0},
	{"GLUEMUX_PDP2_VAL", 0, 3, RW, 0x0},
	{"GLUEMUX_PDP3_VAL", 0, 3, RW, 0x0},
	{"GLUEMUX_CSIS_DMA0_OTF_SEL", 0, 4, RW, 0x0},
	{"GLUEMUX_CSIS_DMA1_OTF_SEL", 0, 4, RW, 0x0},
	{"GLUEMUX_CSIS_DMA2_OTF_SEL", 0, 4, RW, 0x0},
	{"GLUEMUX_CSIS_DMA3_OTF_SEL", 0, 4, RW, 0x0},
	{"MUX_IMG_VC_PDP0", 16, 2, RW, 0x0},
	{"MUX_AF_VC_PDP0", 0, 2, RW, 0x1},
	{"MUX_IMG_VC_PDP1", 16, 2, RW, 0x0},
	{"MUX_AF_VC_PDP1", 0, 2, RW, 0x1},
	{"MUX_IMG_VC_PDP2", 16, 2, RW, 0x0},
	{"MUX_AF_VC_PDP2", 0, 2, RW, 0x1},
	{"MUX_IMG_VC_PDP3", 16, 2, RW, 0x0},
	{"MUX_AF_VC_PDP3", 0, 2, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_ZOTF3_TAACSIS", 23, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_ZOTF2_TAACSIS", 22, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_ZOTF1_TAACSIS", 21, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_ZOTF0_TAACSIS", 20, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_SOTF3_TAACSIS", 19, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_SOTF2_TAACSIS", 18, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_SOTF1_TAACSIS", 17, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_SOTF0_TAACSIS", 16, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_OTF3_CSISTAA", 15, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_OTF2_CSISTAA", 14, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_OTF1_CSISTAA", 13, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_OTF0_CSISTAA", 12, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_INT_OTF3_PDPCSIS", 31, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_INT_OTF2_PDPCSIS", 30, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_INT_OTF1_PDPCSIS", 29, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_INT_OTF0_PDPCSIS", 28, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_INT_OTF3_PDPCSIS", 27, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_INT_OTF2_PDPCSIS", 26, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_INT_OTF1_PDPCSIS", 25, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_INT_OTF0_PDPCSIS", 24, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_INT_OTF3_CSISPDP", 23, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_INT_OTF2_CSISPDP", 22, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_INT_OTF1_CSISPDP", 21, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_INT_OTF0_CSISPDP", 20, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_INT_OTF3_CSISPDP", 19, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_INT_OTF2_CSISPDP", 18, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_INT_OTF1_CSISPDP", 17, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_INT_OTF0_CSISPDP", 16, 1, RW, 0x1},
	{"MIPI_RESETN_DPHY_S1", 5, 1, RW, 0x0},
	{"MIPI_RESETN_DPHY_S", 4, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S3", 3, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S2", 2, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S1", 1, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S", 0, 1, RW, 0x0},
};

static const struct is_field sysreg_taa_fields[SYSREG_TAA_REG_FIELD_CNT] = {
	{"EN", 0, 1, RW, 0x1},
	{"SW_RESETN_C2R_TAA1", 27, 1, RW, 0x1},
	{"SW_RESETN_C2R_TAA0", 26, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_OTF_TAADNS", 25, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_SOTF3_TAACSIS", 24, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_SOTF2_TAACSIS", 23, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_SOTF1_TAACSIS", 22, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_SOTF0_TAACSIS", 21, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_ZOTF3_TAACSIS", 20, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_ZOTF2_TAACSIS", 19, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_ZOTF1_TAACSIS", 18, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_ZOTF0_TAACSIS", 17, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_OTF3_CSISTAA", 16, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_OTF2_CSISTAA", 15, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_OTF1_CSISTAA", 14, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_OTF0_CSISTAA", 13, 1, RW, 0x1},
	{"TYPE_LHS_AST_GLUE_OTF_TAADNS", 12, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_SOTF3_TAACSIS", 11, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_SOTF2_TAACSIS", 10, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_SOTF1_TAACSIS", 9, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_SOTF0_TAACSIS", 8, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_ZOTF3_TAACSIS", 7, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_ZOTF2_TAACSIS", 6, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_ZOTF1_TAACSIS", 5, 1, RW, 0x0},
	{"TYPE_LHS_AST_GLUE_ZOTF0_TAACSIS", 4, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF3_CSISTAA", 3, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF2_CSISTAA", 2, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF1_CSISTAA", 1, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF0_CSISTAA", 0, 1, RW, 0x0},
	{"GLUEMUX_OTFOUT_SEL_0", 0, 2, RW, 0x0},
	{"LHS_AST_OTF_TAADNS", 12, 1, RW, 0x1},
	{"LHS_AST_ZOTF3_TAACSIS", 11, 1, RW, 0x1},
	{"LHS_AST_ZOTF2_TAACSIS", 10, 1, RW, 0x1},
	{"LHS_AST_ZOTF1_TAACSIS", 9, 1, RW, 0x1},
	{"LHS_AST_ZOTF0_TAACSIS", 8, 1, RW, 0x1},
	{"LHS_AST_SOTF3_TAACSIS", 7, 1, RW, 0x1},
	{"LHS_AST_SOTF2_TAACSIS", 6, 1, RW, 0x1},
	{"LHS_AST_SOTF1_TAACSIS", 5, 1, RW, 0x1},
	{"LHS_AST_SOTF0_TAACSIS", 4, 1, RW, 0x1},
	{"LHM_AST_OTF3_CSISTAA", 3, 1, RW, 0x1},
	{"LHM_AST_OTF2_CSISTAA", 2, 1, RW, 0x1},
	{"LHM_AST_OTF1_CSISTAA", 1, 1, RW, 0x1},
	{"LHM_AST_OTF0_CSISTAA", 0, 1, RW, 0x1},
};

static const struct is_field sysreg_mcfp0_fields[SYSREG_MCFP0_REG_FIELD_CNT] = {
	{"EN", 0, 1, RW, 0x1},
};

static const struct is_field sysreg_mcfp1_fields[SYSREG_MCFP1_REG_FIELD_CNT] = {
	{"EN", 0, 1, RW, 0x1},
};

static const struct is_field sysreg_dns_fields[SYSREG_DNS_REG_FIELD_CNT] = {
	{"EN", 0, 1, RW, 0x1},
	{"GLUEMUX_DNS0_VAL", 8, 1, RW, 0x0},
	{"AXCACHE_DNS1", 4, 4, RW, 0x2},
	{"AXCACHE_DNS0", 0, 4, RW, 0x2},
	{"C2R_DNS_SW_RESET", 23, 1, RW, 0x1},
	{"ENABLE_OTF4_IN_ITPDNS", 22, 1, RW, 0x1},
	{"ENABLE_OTF3_IN_ITPDNS", 21, 1, RW, 0x1},
	{"ENABLE_OTF2_IN_ITPDNS", 20, 1, RW, 0x1},
	{"ENABLE_OTF1_IN_ITPDNS", 19, 1, RW, 0x1},
	{"ENABLE_OTF0_IN_ITPDNS", 18, 1, RW, 0x1},
	{"ENABLE_OTF_OUT_CTL_DNSITP", 17, 1, RW, 0x1},
	{"ENABLE_OTF_IN_CTL_ITPDNS", 16, 1, RW, 0x1},
	{"ENABLE_OTF9_OUT_DNSITP", 15, 1, RW, 0x1},
	{"ENABLE_OTF8_OUT_DNSITP", 14, 1, RW, 0x1},
	{"ENABLE_OTF7_OUT_DNSITP", 13, 1, RW, 0x1},
	{"ENABLE_OTF6_OUT_DNSITP", 12, 1, RW, 0x1},
	{"ENABLE_OTF5_OUT_DNSITP", 11, 1, RW, 0x1},
	{"ENABLE_OTF4_OUT_DNSITP", 10, 1, RW, 0x1},
	{"ENABLE_OTF3_OUT_DNSITP", 9, 1, RW, 0x1},
	{"ENABLE_OTF2_OUT_DNSITP", 8, 1, RW, 0x1},
	{"ENABLE_OTF1_OUT_DNSITP", 7, 1, RW, 0x1},
	{"ENABLE_OTF0_OUT_DNSITP", 6, 1, RW, 0x1},
	{"ENABLE_OTF_IN_MCFP1DNS", 5, 1, RW, 0x1},
	{"ENABLE_OTF_IN_TAADNS", 4, 1, RW, 0x1},
	{"TYPE_LHM_AST_GLUE_OTF_MCFP1DNS", 3, 1, RW, 0x0},
	{"TYPE_LHM_AST_GLUE_OTF_TAADNS", 2, 1, RW, 0x0},
	{"SW_RESETN_LHM_AST_GLUE_OTF_MCFP1DNS", 1, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_OTF_TAADNS", 0, 1, RW, 0x1},
	{"C2R_DNS_TOPBYPASS_EN", 1, 1, RW, 0x1},
	{"C2R_DNS_TOPBYPASS_SEL", 0, 1, RW, 0x0},
};

static const struct is_field sysreg_itp_fields[SYSREG_ITP_REG_FIELD_CNT] = {
	{"EN", 0, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_OTF_MCFP1ITP", 19, 1, RW, 0x1},
	{"TYPE_LHM_AST_GLUE_OTF_MCFP1ITP", 18, 1, RW, 0x0},
	{"ENABLE_OTF_IN_MCFP1ITP", 17, 1, RW, 0x1},
	{"ENABLE_OTF4_OUT_ITPDNS", 16, 1, RW, 0x1},
	{"ENABLE_OTF3_OUT_ITPDNS", 15, 1, RW, 0x1},
	{"ENABLE_OTF2_OUT_ITPDNS", 14, 1, RW, 0x1},
	{"ENABLE_OTF1_OUT_ITPDNS", 13, 1, RW, 0x1},
	{"ENABLE_OTF0_OUT_ITPDNS", 12, 1, RW, 0x1},
	{"ENABLE_OTF_IN_CTL_DNSITP", 11, 1, RW, 0x1},
	{"ENABLE_OTF_OUT_CTL_ITPDNS", 10, 1, RW, 0x1},
	{"ENABLE_OTF9_IN_DNSITP", 9, 1, RW, 0x1},
	{"ENABLE_OTF8_IN_DNSITP", 8, 1, RW, 0x1},
	{"ENABLE_OTF7_IN_DNSITP", 7, 1, RW, 0x1},
	{"ENABLE_OTF6_IN_DNSITP", 6, 1, RW, 0x1},
	{"ENABLE_OTF5_IN_DNSITP", 5, 1, RW, 0x1},
	{"ENABLE_OTF4_IN_DNSITP", 4, 1, RW, 0x1},
	{"ENABLE_OTF3_IN_DNSITP", 3, 1, RW, 0x1},
	{"ENABLE_OTF2_IN_DNSITP", 2, 1, RW, 0x1},
	{"ENABLE_OTF1_IN_DNSITP", 1, 1, RW, 0x1},
	{"ENABLE_OTF0_IN_DNSITP", 0, 1, RW, 0x1},
};

static const struct is_field sysreg_yuvpp_fields[SYSREG_YUVPP_REG_FIELD_CNT] = {
	{"EN", 0, 1, RW, 0x1},
	{"SW_RESETN_LHS_AST_GLUE_OTF_YUVPPMCSC", 1, 1, RW, 0x1},
	{"TYPE_LHS_AST_GLUE_OTF_YUVPPMCSC", 0, 1, RW, 0x0},
};

static const struct is_field sysreg_mcsc_fields[SYSREG_MCSC_REG_FIELD_CNT] = {
	{"EN", 0, 1, RW, 0x1},
	{"SW_RESETN_LHM_AST_GLUE_OTF_YUVPPMCSC", 14, 1, RW, 0x1},
	{"TYPE_LHM_AST_GLUE_OTF_YUVPPMCSC", 13, 1, RW, 0x0},
	{"C2R_NPUS_MCSC_TOPBYPASS_SEL", 12, 1, RW, 0x0},
	{"C2R_VPC_MCSC_TOPBYPASS_SEL", 11, 1, RW, 0x0},
	{"C2R_DPUF_MCSC_TOPBYPASS_SEL", 10, 1, RW, 0x0},
	{"C2R_NPUS_MCSC_DISABLE_SEL", 9, 1, RW, 0x1},
	{"C2R_VPC_MCSC_DISABLE_SEL", 8, 1, RW, 0x1},
	{"C2R_DPUF_MCSC_DISABLE_SEL", 7, 1, RW, 0x1},
	{"C2R_NPUS_MCSC_SW_RESET", 6, 1, RW, 0x1},
	{"C2R_VPC_MCSC_SW_RESET", 5, 1, RW, 0x1},
	{"C2R_DPUF_MCSC_SW_RESET", 4, 1, RW, 0x1},
	{"C2AGENT_D2_MCSC_M6S4_C2AGENT_SW_RESET", 3, 1, RW, 0x1},
	{"C2AGENT_D1_MCSC_M6S4_C2AGENT_SW_RESET", 2, 1, RW, 0x1},
	{"C2AGENT_D0_MCSC_M6S4_C2AGENT_SW_RESET", 1, 1, RW, 0x1},
	{"ENABLE_OTF_IN_LHM_AST_OTF_YUVPPMCSC", 0, 1, RW, 0x1},
};

static const struct is_field sysreg_lme_fields[SYSREG_LME_REG_FIELD_CNT] = {
	{"EN", 0, 1, RW, 0x1},
	{"LME_SW_RESET", 4, 1, RW, 0x1},
	{"AXCACHE_LME", 0, 4, RW, 0x2},
};

void __iomem *hwfc_rst;

/*
 * ISR definitions
 */
void is_enter_lib_isr(void)
{
#ifdef ENABLE_FPSIMD_FOR_USER
	is_fpsimd_get_isr();
#endif
}

void is_exit_lib_isr(void)
{
#ifdef ENABLE_FPSIMD_FOR_USER
	is_fpsimd_put_isr();
#endif
}

static inline void __nocfi __is_isr_ddk(void *data, int handler_id)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[handler_id];

	if (intr_hw->valid) {
		is_enter_lib_isr();
		intr_hw->handler(intr_hw->id, intr_hw->ctx);
		is_exit_lib_isr();
	} else {
		err_itfc("[ID:%d](%d)- chain(%d) empty handler!!",
			itf_hw->id, handler_id, intr_hw->chain_id);
	}
}

static inline void __is_isr_host(void *data, int handler_id)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[handler_id];

	if (intr_hw->valid)
		intr_hw->handler(intr_hw->id, (void *)itf_hw->hw_ip);
	else
		err_itfc("[ID:%d](1) empty handler!!", itf_hw->id);
}

/*
 * Interrupt handler definitions
 */
/* 3AA0 */
static irqreturn_t __is_isr1_3aa0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_3aa0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr3_3aa0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP3);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr4_3aa0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP4);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr5_3aa0(int irq, void *data)
{
	/* for ORBMCH0 IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP5];

	if (intr_hw->chain_id != ID_3AA_0)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP5);
	return IRQ_HANDLED;
}

/* TODO: ORBMCH1 at EVT1
static irqreturn_t __is_isr6_3aa0(int irq, void *data)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP6];

	if (intr_hw->chain_id != ID_3AA_0)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP6);
	return IRQ_HANDLED;
}
*/

static irqreturn_t __is_isr7_3aa0(int irq, void *data)
{
	/* for LME IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP7];

	if (intr_hw->chain_id != ID_3AA_0)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP7);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr8_3aa0(int irq, void *data)
{
	/* for LME IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP8];

	if (intr_hw->chain_id != ID_3AA_0)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP8);
	return IRQ_HANDLED;
}

/* 3AA1 */
static irqreturn_t __is_isr1_3aa1(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_3aa1(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr3_3aa1(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP3);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr4_3aa1(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP4);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr5_3aa1(int irq, void *data)
{
	/* for ORBMCH0 IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP5];

	if (intr_hw->chain_id != ID_3AA_1)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP5);
	return IRQ_HANDLED;
}

/* TODO: ORBMCH1 at EVT1
static irqreturn_t __is_isr6_3aa1(int irq, void *data)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP6];

	if (intr_hw->chain_id != ID_3AA_1)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP6);
	return IRQ_HANDLED;
}
*/

static irqreturn_t __is_isr7_3aa1(int irq, void *data)
{
	/* for LME IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP7];

	if (intr_hw->chain_id != ID_3AA_1)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP7);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr8_3aa1(int irq, void *data)
{
	/* for LME IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP8];

	if (intr_hw->chain_id != ID_3AA_1)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP8);
	return IRQ_HANDLED;
}
/* 3AA2 */
static irqreturn_t __is_isr1_3aa2(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_3aa2(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr3_3aa2(int irq, void *data)
{
	/* FOR DMA2/3 IRQ shared */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP3];

	if (intr_hw->chain_id != ID_3AA_2)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP3);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr4_3aa2(int irq, void *data)
{
	/* FOR DMA2/3 IRQ shared */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP4];

	if (intr_hw->chain_id != ID_3AA_2)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP4);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr5_3aa2(int irq, void *data)
{
	/* for ORBMCH0 IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP5];

	if (intr_hw->chain_id != ID_3AA_2)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP5);
	return IRQ_HANDLED;
}

/* TODO: ORBMCH1 at EVT1
static irqreturn_t __is_isr6_3aa2(int irq, void *data)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP6];

	if (intr_hw->chain_id != ID_3AA_2)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP6);
	return IRQ_HANDLED;
}
*/

static irqreturn_t __is_isr7_3aa2(int irq, void *data)
{
	/* for LME IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP7];

	if (intr_hw->chain_id != ID_3AA_2)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP7);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr8_3aa2(int irq, void *data)
{
	/* for LME IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP8];

	if (intr_hw->chain_id != ID_3AA_2)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP8);
	return IRQ_HANDLED;
}
/* 3AA3 */
static irqreturn_t __is_isr1_3aa3(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_3aa3(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr3_3aa3(int irq, void *data)
{
	/* FOR DMA2/3 IRQ shared */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP3];

	if (intr_hw->chain_id != ID_3AA_3)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP3);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr4_3aa3(int irq, void *data)
{
	/* FOR DMA2/3 IRQ shared */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP4];

	if (intr_hw->chain_id != ID_3AA_3)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP4);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr5_3aa3(int irq, void *data)
{
	/* for ORBMCH0 IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP5];

	if (intr_hw->chain_id != ID_3AA_3)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP5);
	return IRQ_HANDLED;
}

/* TODO: ORBMCH1 at EVT1
static irqreturn_t __is_isr6_3aa3(int irq, void *data)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP6];

	if (intr_hw->chain_id != ID_3AA_3)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP6);
	return IRQ_HANDLED;
}
*/

static irqreturn_t __is_isr7_3aa3(int irq, void *data)
{
	/* for LME IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP7];

	if (intr_hw->chain_id != ID_3AA_3)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP7);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr8_3aa3(int irq, void *data)
{
	/* for LME IRQ SHARED */
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[INTR_HWIP8];

	if (intr_hw->chain_id != ID_3AA_3)
		return IRQ_NONE;

	__is_isr_ddk(data, INTR_HWIP8);
	return IRQ_HANDLED;
}

/* ITP0 */
static irqreturn_t __is_isr1_itp0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_itp0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr3_itp0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP3);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr4_itp0(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP4);
	return IRQ_HANDLED;
}

/* LME */
static irqreturn_t __is_isr1_lme(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

/* YUVPP */
static irqreturn_t __is_isr1_yuvpp(int irq, void *data)
{
	__is_isr_ddk(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

/* MCSC */
static irqreturn_t __is_isr1_mcs0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

/*
 * HW group related functions
 */
void __is_hw_group_init(struct is_group *group)
{
	int i;

	for (i = ENTRY_SENSOR; i < ENTRY_END; i++)
		group->subdev[i] = NULL;

	INIT_LIST_HEAD(&group->subdev_list);
}

int is_hw_group_cfg(void *group_data)
{
	int ret = 0;
	struct is_group *group;
	struct is_device_sensor *sensor;
	struct is_device_ischain *device;

	FIMC_BUG(!group_data);

	group = (struct is_group *)group_data;

#ifdef CONFIG_USE_SENSOR_GROUP
	if (group->slot == GROUP_SLOT_SENSOR) {
		sensor = group->sensor;
		if (!sensor) {
			err("device is NULL");
			BUG();
		}

		__is_hw_group_init(group);
		group->subdev[ENTRY_SENSOR] = &sensor->group_sensor.leader;
		group->subdev[ENTRY_SSVC0] = &sensor->ssvc0;
		group->subdev[ENTRY_SSVC1] = &sensor->ssvc1;
		group->subdev[ENTRY_SSVC2] = &sensor->ssvc2;
		group->subdev[ENTRY_SSVC3] = &sensor->ssvc3;

		list_add_tail(&sensor->group_sensor.leader.list, &group->subdev_list);
		list_add_tail(&sensor->ssvc0.list, &group->subdev_list);
		list_add_tail(&sensor->ssvc1.list, &group->subdev_list);
		list_add_tail(&sensor->ssvc2.list, &group->subdev_list);
		list_add_tail(&sensor->ssvc3.list, &group->subdev_list);
		return ret;
	}
#endif

	device = group->device;
	if (!device) {
		err("device is NULL");
		BUG();
	}

	switch (group->slot) {
	case GROUP_SLOT_PAF:
		__is_hw_group_init(group);
		group->subdev[ENTRY_PAF] = &device->group_paf.leader;
		group->subdev[ENTRY_PDAF] = &device->pdaf;
		group->subdev[ENTRY_PDST] = &device->pdst;

		list_add_tail(&device->group_paf.leader.list, &group->subdev_list);
		list_add_tail(&device->pdaf.list, &group->subdev_list);
		list_add_tail(&device->pdst.list, &group->subdev_list);
		break;
	case GROUP_SLOT_3AA:
		__is_hw_group_init(group);
		group->subdev[ENTRY_3AA] = &device->group_3aa.leader;
		group->subdev[ENTRY_3AC] = &device->txc;
		group->subdev[ENTRY_3AP] = &device->txp;
		group->subdev[ENTRY_3AF] = &device->txf;
		group->subdev[ENTRY_3AG] = &device->txg;
		group->subdev[ENTRY_3AO] = &device->txo;
		group->subdev[ENTRY_3AL] = &device->txl;
		group->subdev[ENTRY_MEXC] = &device->mexc;
		group->subdev[ENTRY_ORBXC] = &device->orbxc;

		list_add_tail(&device->group_3aa.leader.list, &group->subdev_list);
		list_add_tail(&device->txc.list, &group->subdev_list);
		list_add_tail(&device->txp.list, &group->subdev_list);
		list_add_tail(&device->txf.list, &group->subdev_list);
		list_add_tail(&device->txg.list, &group->subdev_list);
		list_add_tail(&device->txo.list, &group->subdev_list);
		list_add_tail(&device->txl.list, &group->subdev_list);
		list_add_tail(&device->mexc.list, &group->subdev_list);
		list_add_tail(&device->orbxc.list, &group->subdev_list);

		device->txc.param_dma_ot = PARAM_3AA_VDMA4_OUTPUT;
		device->txp.param_dma_ot = PARAM_3AA_VDMA2_OUTPUT;
		device->txf.param_dma_ot = PARAM_3AA_FDDMA_OUTPUT;
		device->txg.param_dma_ot = PARAM_3AA_MRGDMA_OUTPUT;
		device->txo.param_dma_ot = PARAM_3AA_ORBDS_OUTPUT;
		device->txl.param_dma_ot = PARAM_3AA_LMEDS_OUTPUT;
		break;
	case GROUP_SLOT_ISP:
		__is_hw_group_init(group);
		group->subdev[ENTRY_ISP] = &device->group_isp.leader;
		group->subdev[ENTRY_IXC] = &device->ixc;
		group->subdev[ENTRY_IXP] = &device->ixp;
		group->subdev[ENTRY_IXT] = &device->ixt;
		group->subdev[ENTRY_IXG] = &device->ixg;
		group->subdev[ENTRY_IXV] = &device->ixv;
		group->subdev[ENTRY_IXW] = &device->ixw;

		list_add_tail(&device->group_isp.leader.list, &group->subdev_list);
		list_add_tail(&device->ixc.list, &group->subdev_list);
		list_add_tail(&device->ixp.list, &group->subdev_list);
		list_add_tail(&device->ixt.list, &group->subdev_list);
		list_add_tail(&device->ixg.list, &group->subdev_list);
		list_add_tail(&device->ixv.list, &group->subdev_list);
		list_add_tail(&device->ixw.list, &group->subdev_list);
		break;
	case GROUP_SLOT_MCS:
		__is_hw_group_init(group);
		group->subdev[ENTRY_MCS] = &device->group_mcs.leader;
		group->subdev[ENTRY_M0P] = &device->m0p;
		group->subdev[ENTRY_M1P] = &device->m1p;
		group->subdev[ENTRY_M2P] = &device->m2p;
		group->subdev[ENTRY_M3P] = &device->m3p;
		group->subdev[ENTRY_M4P] = &device->m4p;
		group->subdev[ENTRY_M5P] = &device->m5p;

		list_add_tail(&device->group_mcs.leader.list, &group->subdev_list);
		list_add_tail(&device->m0p.list, &group->subdev_list);
		list_add_tail(&device->m1p.list, &group->subdev_list);
		list_add_tail(&device->m2p.list, &group->subdev_list);
		list_add_tail(&device->m3p.list, &group->subdev_list);
		list_add_tail(&device->m4p.list, &group->subdev_list);
		list_add_tail(&device->m5p.list, &group->subdev_list);

		device->m0p.param_dma_ot = PARAM_MCS_OUTPUT0;
		device->m1p.param_dma_ot = PARAM_MCS_OUTPUT1;
		device->m2p.param_dma_ot = PARAM_MCS_OUTPUT2;
		device->m3p.param_dma_ot = PARAM_MCS_OUTPUT3;
		device->m4p.param_dma_ot = PARAM_MCS_OUTPUT4;
		device->m5p.param_dma_ot = PARAM_MCS_OUTPUT5;
		break;
	case GROUP_SLOT_YPP:
		__is_hw_group_init(group);
		group->subdev[ENTRY_YPP] = &device->group_ypp.leader;

		list_add_tail(&device->group_ypp.leader.list, &group->subdev_list);
		break;
	case GROUP_SLOT_VRA:
	case GROUP_SLOT_CLH:
		break;
	default:
		probe_err("group slot(%d) is invalid", group->slot);
		BUG();
		break;
	}

	/* for hwfc: reset all REGION_IDX registers and outputs */
	hwfc_rst = ioremap(HWFC_INDEX_RESET_ADDR, SZ_4);

	return ret;
}

int is_hw_group_open(void *group_data)
{
	int ret = 0;
	u32 group_id;
	struct is_subdev *leader;
	struct is_group *group;
	struct is_device_ischain *device;

	FIMC_BUG(!group_data);

	group = group_data;
	leader = &group->leader;
	device = group->device;
	group_id = group->id;

	switch (group_id) {
#ifdef CONFIG_USE_SENSOR_GROUP
	case GROUP_ID_SS0:
	case GROUP_ID_SS1:
	case GROUP_ID_SS2:
	case GROUP_ID_SS3:
	case GROUP_ID_SS4:
	case GROUP_ID_SS5:
		leader->constraints_width = GROUP_SENSOR_MAX_WIDTH;
		leader->constraints_height = GROUP_SENSOR_MAX_HEIGHT;
		break;
#endif
	case GROUP_ID_PAF0:
	case GROUP_ID_PAF1:
	case GROUP_ID_PAF2:
	case GROUP_ID_PAF3:
		leader->constraints_width = GROUP_PDP_MAX_WIDTH;
		leader->constraints_height = GROUP_PDP_MAX_HEIGHT;
		break;
	case GROUP_ID_3AA0:
	case GROUP_ID_3AA1:
	case GROUP_ID_3AA2:
	case GROUP_ID_3AA3:
		leader->constraints_width = GROUP_3AA_MAX_WIDTH;
		leader->constraints_height = GROUP_3AA_MAX_HEIGHT;
		break;
	case GROUP_ID_ISP0:
	case GROUP_ID_YPP:
	case GROUP_ID_MCS0:
	case GROUP_ID_MCS1:
		leader->constraints_width = GROUP_ITP_MAX_WIDTH;
		leader->constraints_height = GROUP_ITP_MAX_HEIGHT;
		break;
	case GROUP_ID_LME:
		leader->constraints_width = GROUP_LME_MAX_WIDTH;
		leader->constraints_height = GROUP_LME_MAX_HEIGHT;
		break;
	default:
		merr("(%s) is invalid", group, group_id_name[group_id]);
		break;
	}

	return ret;
}

inline int is_hw_slot_id(int hw_id)
{
	int slot_id = -1;

	switch (hw_id) {
	case DEV_HW_PAF0:
		slot_id = 0;
		break;
	case DEV_HW_PAF1:
		slot_id = 1;
		break;
	case DEV_HW_PAF2:
		slot_id = 2;
		break;
	case DEV_HW_PAF3:
		slot_id = 3;
		break;
	case DEV_HW_3AA0:
		slot_id = 4;
		break;
	case DEV_HW_3AA1:
		slot_id = 5;
		break;
	case DEV_HW_3AA2:
		slot_id = 6;
		break;
	case DEV_HW_3AA3:
		slot_id = 7;
		break;
	case DEV_HW_LME:
		slot_id = 8;
		break;
	case DEV_HW_ISP0:
		slot_id = 9;
		break;
	case DEV_HW_YPP:
		slot_id = 10;
		break;
	case DEV_HW_MCSC0:
		slot_id = 11;
		break;
	case DEV_HW_ISP1:
	case DEV_HW_MCSC1:
	case DEV_HW_VRA:
	case DEV_HW_CLH0:
		break;
	default:
		err("Invalid hw id(%d)", hw_id);
		break;
	}

	return slot_id;
}

int is_get_hw_list(int group_id, int *hw_list)
{
	int i;
	int hw_index = 0;

	/* initialization */
	for (i = 0; i < GROUP_HW_MAX; i++)
		hw_list[i] = -1;

	switch (group_id) {
	case GROUP_ID_PAF0:
		hw_list[hw_index] = DEV_HW_PAF0; hw_index++;
		break;
	case GROUP_ID_PAF1:
		hw_list[hw_index] = DEV_HW_PAF1; hw_index++;
		break;
	case GROUP_ID_PAF2:
		hw_list[hw_index] = DEV_HW_PAF2; hw_index++;
		break;
	case GROUP_ID_PAF3:
		hw_list[hw_index] = DEV_HW_PAF3; hw_index++;
		break;
	case GROUP_ID_3AA0:
		hw_list[hw_index] = DEV_HW_3AA0; hw_index++;
		break;
	case GROUP_ID_3AA1:
		hw_list[hw_index] = DEV_HW_3AA1; hw_index++;
		break;
	case GROUP_ID_3AA2:
		hw_list[hw_index] = DEV_HW_3AA2; hw_index++;
		break;
	case GROUP_ID_3AA3:
		hw_list[hw_index] = DEV_HW_3AA3; hw_index++;
		break;
	case GROUP_ID_LME:
		hw_list[hw_index] = DEV_HW_LME; hw_index++;
		break;
	case GROUP_ID_ISP0:
		hw_list[hw_index] = DEV_HW_ISP0; hw_index++;
		break;
	case GROUP_ID_YPP:
		hw_list[hw_index] = DEV_HW_YPP; hw_index++;
		break;
	case GROUP_ID_MCS0:
		hw_list[hw_index] = DEV_HW_MCSC0; hw_index++;
		break;
	case GROUP_ID_MAX:
		break;
	default:
		err("Invalid group%d(%s)", group_id, group_id_name[group_id]);
		break;
	}

	return hw_index;
}
/*
 * System registers configurations
 */
static int is_hw_get_clk_gate(struct is_hw_ip *hw_ip, int hw_id)
{
	if (!hw_ip) {
		probe_err("hw_id(%d) hw_ip(NULL)", hw_id);
		return -EINVAL;
	}

	hw_ip->clk_gate_idx = 0;
	hw_ip->clk_gate = NULL;

	return 0;
}

int is_hw_get_address(void *itfc_data, void *pdev_data, int hw_id)
{
	int ret = 0;
	struct resource *mem_res = NULL;
	struct platform_device *pdev = NULL;
	struct is_interface_hwip *itf_hwip = NULL;
	int idx;

	FIMC_BUG(!itfc_data);
	FIMC_BUG(!pdev_data);

	itf_hwip = (struct is_interface_hwip *)itfc_data;
	pdev = (struct platform_device *)pdev_data;

	switch (hw_id) {
	case DEV_HW_3AA0:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA0);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}
		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}
		/* TODO: need check if exist dump_region */
#if 0
		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x0;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0x1FB3;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x1FB8;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0x9FB3;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x9FB8;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0xFFFF;
#endif
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA_DMA_TOP);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT2] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT2] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT2] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT2]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_ORBMCH0);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT1] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT1] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT1] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT1]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_LME);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT3] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT3] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT3] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT3]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		/* TODO: add ORBMCH1 (REG_EXT4) at EVT1 */

		info_itfc("[ID:%2d] 3AA0 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		info_itfc("[ID:%2d] 3AA DMA VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT2]);
		info_itfc("[ID:%2d] ORBMCH0 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT1]);
		info_itfc("[ID:%2d] LME VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT3]);
		break;
	case DEV_HW_3AA1:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA1);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}
		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start + LIC_CHAIN_OFFSET;
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}
		itf_hwip->hw_ip->regs[REG_SETA] += LIC_CHAIN_OFFSET;
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA_DMA_TOP);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT2] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT2] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT2] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT2]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_ORBMCH0);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT1] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT1] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT1] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT1]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_LME);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT3] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT3] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT3] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT3]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		/* TODO: add ORBMCH1 (REG_EXT4) at EVT1 */

		info_itfc("[ID:%2d] 3AA1 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		info_itfc("[ID:%2d] 3AA DMA VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT2]);
		info_itfc("[ID:%2d] ORBMCH VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT1]);
		info_itfc("[ID:%2d] LME VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT3]);
		break;
	case DEV_HW_3AA2:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA2);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}
		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start + (2 * LIC_CHAIN_OFFSET);
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}
		itf_hwip->hw_ip->regs[REG_SETA] += (2 * LIC_CHAIN_OFFSET);
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA_DMA_TOP);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT2] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT2] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT2] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT2]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_ORBMCH0);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT1] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT1] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT1] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT1]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_LME);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT3] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT3] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT3] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT3]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		/* TODO: add ORBMCH1 (REG_EXT4) at EVT1 */

		info_itfc("[ID:%2d] 3AA2 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		info_itfc("[ID:%2d] 3AA DMA VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT2]);
		info_itfc("[ID:%2d] ORBMCH VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT1]);
		info_itfc("[ID:%2d] LME VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT3]);
		break;
	case DEV_HW_3AA3:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA3);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}
		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start + (3 * LIC_CHAIN_OFFSET);
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}
		itf_hwip->hw_ip->regs[REG_SETA] += (3 * LIC_CHAIN_OFFSET);
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_3AA_DMA_TOP);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT2] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT2] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT2] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT2]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_ORBMCH0);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT1] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT1] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT1] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT1]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_LME);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT3] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT3] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT3] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT3]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		/* TODO: add ORBMCH1 (REG_EXT4) at EVT1 */

		info_itfc("[ID:%2d] 3AA3 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		info_itfc("[ID:%2d] 3AA DMA VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT2]);
		info_itfc("[ID:%2d] ORBMCH VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT1]);
		info_itfc("[ID:%2d] LME VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT3]);
		break;
	case DEV_HW_LME:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_LME);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		info_itfc("[ID:%2d] LME VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		break;
	case DEV_HW_ISP0:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_ITP);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_MCFP0);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT1] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT1] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT1] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT1]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx].start = 0x0;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx++].end = 0x03FF;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx].start = 0x0500;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx++].end = 0x05FF;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx].start = 0x0800;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx++].end = 0x09FF;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx].start = 0x1800;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx++].end = 0x1AFF;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx].start = 0x2000;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx++].end = 0x23FF;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx].start = 0x2600;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx++].end = 0x2BFF;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx].start = 0x3800;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx++].end = 0x39FF;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx].start = 0x4000;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx++].end = 0x4DFF;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx].start = 0x5200;
		itf_hwip->hw_ip->dump_region[REG_EXT1][idx++].end = 0x55FF;

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_DNS);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT2] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT2] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT2] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT2]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_MCFP1);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_EXT3] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_EXT3] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_EXT3] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_EXT3]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_EXT3][idx].start = 0x0;
		itf_hwip->hw_ip->dump_region[REG_EXT3][idx++].end = 0x08FF;
		itf_hwip->hw_ip->dump_region[REG_EXT3][idx].start = 0x3000;
		itf_hwip->hw_ip->dump_region[REG_EXT3][idx++].end = 0x3FFF;

		info_itfc("[ID:%2d] ITP VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		info_itfc("[ID:%2d] MCFP0 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT1]);
		info_itfc("[ID:%2d] DNS VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT2]);
		info_itfc("[ID:%2d] MCFP1 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_EXT3]);
		break;
	case DEV_HW_YPP:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_YUVPP);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		info_itfc("[ID:%2d] YUVPP VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		break;
	case DEV_HW_MCSC0:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, IORESOURCE_MCSC);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			return -EINVAL;
		}

		itf_hwip->hw_ip->regs_start[REG_SETA] = mem_res->start;
		itf_hwip->hw_ip->regs_end[REG_SETA] = mem_res->end;
		itf_hwip->hw_ip->regs[REG_SETA] = ioremap_nocache(mem_res->start, resource_size(mem_res));
		if (!itf_hwip->hw_ip->regs[REG_SETA]) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			return -EINVAL;
		}

		info_itfc("[ID:%2d] MCSC0 VA(0x%lx)\n", hw_id, (ulong)itf_hwip->hw_ip->regs[REG_SETA]);
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	ret = is_hw_get_clk_gate(itf_hwip->hw_ip, hw_id);
	if (ret)
		dev_err(&pdev->dev, "is_hw_get_clk_gate is fail\n");

	return ret;
}

int is_hw_get_irq(void *itfc_data, void *pdev_data, int hw_id)
{
	struct is_interface_hwip *itf_hwip = NULL;
	struct platform_device *pdev = NULL;
	int ret = 0;

	FIMC_BUG(!itfc_data);

	itf_hwip = (struct is_interface_hwip *)itfc_data;
	pdev = (struct platform_device *)pdev_data;

	switch (hw_id) {
	case DEV_HW_3AA0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 0);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa0-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 1);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa0-2\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP3] = platform_get_irq(pdev, 2);
		if (itf_hwip->irq[INTR_HWIP3] < 0) {
			err("Failed to get irq 3aa0 zsl dma\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP4] = platform_get_irq(pdev, 3);
		if (itf_hwip->irq[INTR_HWIP4] < 0) {
			err("Failed to get irq 3aa0 strp dma\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP5] = platform_get_irq(pdev, 4);
		if (itf_hwip->irq[INTR_HWIP5] < 0) {
			err("Failed to get irq orbmch0\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_3AA1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 5);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa1-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 6);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa1-2\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP3] = platform_get_irq(pdev, 7);
		if (itf_hwip->irq[INTR_HWIP3] < 0) {
			err("Failed to get irq 3aa1 zsl dma\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP4] = platform_get_irq(pdev, 8);
		if (itf_hwip->irq[INTR_HWIP4] < 0) {
			err("Failed to get irq 3aa1 strp dma\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP5] = platform_get_irq(pdev, 9);
		if (itf_hwip->irq[INTR_HWIP5] < 0) {
			err("Failed to get irq orbmch0\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_3AA2:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 10);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa2-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 11);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa2-2\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP3] = platform_get_irq(pdev, 12);
		if (itf_hwip->irq[INTR_HWIP3] < 0) {
			err("Failed to get irq 3aa2 zsl dma\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP4] = platform_get_irq(pdev, 13);
		if (itf_hwip->irq[INTR_HWIP4] < 0) {
			err("Failed to get irq 3aa2 strp dma\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP5] = platform_get_irq(pdev, 14);
		if (itf_hwip->irq[INTR_HWIP5] < 0) {
			err("Failed to get irq orbmch0\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_3AA3:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 15);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa3-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 16);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa3-2\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP3] = platform_get_irq(pdev, 17);
		if (itf_hwip->irq[INTR_HWIP3] < 0) {
			err("Failed to get irq 3aa3 zsl dma\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP4] = platform_get_irq(pdev, 18);
		if (itf_hwip->irq[INTR_HWIP4] < 0) {
			err("Failed to get irq 3aa3 strp dma\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP5] = platform_get_irq(pdev, 19);
		if (itf_hwip->irq[INTR_HWIP5] < 0) {
			err("Failed to get irq orbmch0\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_LME:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 20);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq lme\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_ISP0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 21);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq isp0-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 22);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq isp0-2\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP3] = platform_get_irq(pdev, 23);
		if (itf_hwip->irq[INTR_HWIP3] < 0) {
			err("Failed to get irq mcfp0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP4] = platform_get_irq(pdev, 24);
		if (itf_hwip->irq[INTR_HWIP4] < 0) {
			err("Failed to get irq mcfp1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_YPP:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 25);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq yuvpp\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MCSC0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 26);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mcsc0\n");
			return -EINVAL;
		}
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	return ret;
}

static inline int __is_hw_request_irq(struct is_interface_hwip *itf_hwip,
	const char *name, int isr_num,
	unsigned int added_irq_flags,
	irqreturn_t (*func)(int, void *))
{
	size_t name_len = 0;
	int ret = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)) && defined(ENABLE_IRQ_MULTI_TARGET_CL0)
	const char *buf = "0-3";
	struct cpumask cpumask;
#endif
	name_len = sizeof(itf_hwip->irq_name[isr_num]);
	snprintf(itf_hwip->irq_name[isr_num], name_len, "%s-%d", name, isr_num);
	ret = request_irq(itf_hwip->irq[isr_num], func,
		IS_HW_IRQ_FLAG | added_irq_flags,
		itf_hwip->irq_name[isr_num],
		itf_hwip);
	if (ret) {
		err_itfc("[HW:%s] request_irq [%d] fail", name, isr_num);
		return -EINVAL;
	}
#if defined(ENABLE_IRQ_MULTI_TARGET_CL0)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
	cpulist_parse(buf, &cpumask);
	irq_set_affinity_hint(itf_hwip->irq[isr_num], &cpumask);
#else
	irq_force_affinity(itf_hwip->irq[isr_num], cpumask_of(0));
#endif
#endif
	itf_hwip->handler[isr_num].id = isr_num;
	itf_hwip->handler[isr_num].valid = true;

	return ret;
}

int is_hw_request_irq(void *itfc_data, int hw_id)
{
	struct is_interface_hwip *itf_hwip = NULL;
	int ret = 0;

	FIMC_BUG(!itfc_data);


	itf_hwip = (struct is_interface_hwip *)itfc_data;

	switch (hw_id) {
	case DEV_HW_3AA0:
		ret = __is_hw_request_irq(itf_hwip, "3a0-0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_3aa0);
		ret = __is_hw_request_irq(itf_hwip, "3a0-1", INTR_HWIP2, IRQF_TRIGGER_NONE, __is_isr2_3aa0);
		ret = __is_hw_request_irq(itf_hwip, "3a0-zsl", INTR_HWIP3, IRQF_TRIGGER_NONE, __is_isr3_3aa0);
		ret = __is_hw_request_irq(itf_hwip, "3a0-strp", INTR_HWIP4, IRQF_TRIGGER_NONE, __is_isr4_3aa0);
		ret = __is_hw_request_irq(itf_hwip, "orbmch0", INTR_HWIP5, IRQF_SHARED, __is_isr5_3aa0);
		/* ret = __is_hw_request_irq(itf_hwip, "orbmch1", INTR_HWIP6, IRQF_SHARED, __is_isr6_3aa0); */
		break;
	case DEV_HW_3AA1:
		ret = __is_hw_request_irq(itf_hwip, "3a1-0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_3aa1);
		ret = __is_hw_request_irq(itf_hwip, "3a1-1", INTR_HWIP2, IRQF_TRIGGER_NONE, __is_isr2_3aa1);
		ret = __is_hw_request_irq(itf_hwip, "3a1-zsl", INTR_HWIP3, IRQF_TRIGGER_NONE, __is_isr3_3aa1);
		ret = __is_hw_request_irq(itf_hwip, "3a1-strp", INTR_HWIP4, IRQF_TRIGGER_NONE, __is_isr4_3aa1);
		ret = __is_hw_request_irq(itf_hwip, "orbmch0", INTR_HWIP5, IRQF_SHARED, __is_isr5_3aa1);
		/* ret = __is_hw_request_irq(itf_hwip, "orbmch1", INTR_HWIP6, IRQF_SHARED, __is_isr6_3aa1); */
		break;
	case DEV_HW_3AA2:
		ret = __is_hw_request_irq(itf_hwip, "3a2-0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_3aa2);
		ret = __is_hw_request_irq(itf_hwip, "3a2-1", INTR_HWIP2, IRQF_TRIGGER_NONE, __is_isr2_3aa2);
		ret = __is_hw_request_irq(itf_hwip, "3a2-zsl", INTR_HWIP3, IRQF_SHARED, __is_isr3_3aa2);
		ret = __is_hw_request_irq(itf_hwip, "3a2-strp", INTR_HWIP4, IRQF_SHARED, __is_isr4_3aa2);
		ret = __is_hw_request_irq(itf_hwip, "orbmch0", INTR_HWIP5, IRQF_SHARED, __is_isr5_3aa2);
		/* ret = __is_hw_request_irq(itf_hwip, "orbmch1", INTR_HWIP6, IRQF_SHARED, __is_isr6_3aa2); */
		break;
	case DEV_HW_3AA3:
		ret = __is_hw_request_irq(itf_hwip, "3a3-0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_3aa3);
		ret = __is_hw_request_irq(itf_hwip, "3a3-1", INTR_HWIP2, IRQF_TRIGGER_NONE, __is_isr2_3aa3);
		ret = __is_hw_request_irq(itf_hwip, "3a3-zsl", INTR_HWIP3, IRQF_SHARED, __is_isr3_3aa3);
		ret = __is_hw_request_irq(itf_hwip, "3a3-strp", INTR_HWIP4, IRQF_SHARED, __is_isr4_3aa3);
		ret = __is_hw_request_irq(itf_hwip, "orbmch0", INTR_HWIP5, IRQF_SHARED, __is_isr5_3aa3);
		/* ret = __is_hw_request_irq(itf_hwip, "orbmch1", INTR_HWIP6, IRQF_SHARED, __is_isr6_3aa3); */
		break;
	case DEV_HW_LME:
		ret = __is_hw_request_irq(itf_hwip, "lme", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_lme);
		break;
	case DEV_HW_ISP0:
		ret = __is_hw_request_irq(itf_hwip, "itp-0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_itp0);
		ret = __is_hw_request_irq(itf_hwip, "itp-1", INTR_HWIP2, IRQF_TRIGGER_NONE, __is_isr2_itp0);
		ret = __is_hw_request_irq(itf_hwip, "mcfp0", INTR_HWIP3, IRQF_TRIGGER_NONE, __is_isr3_itp0);
		ret = __is_hw_request_irq(itf_hwip, "mcfp1", INTR_HWIP4, IRQF_TRIGGER_NONE, __is_isr4_itp0);
		break;
	case DEV_HW_YPP:
		ret = __is_hw_request_irq(itf_hwip, "yuvpp", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_yuvpp);
		break;
	case DEV_HW_MCSC0:
		ret = __is_hw_request_irq(itf_hwip, "mcs0", INTR_HWIP1, IRQF_TRIGGER_NONE, __is_isr1_mcs0);
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	return ret;
}

int is_hw_s_ctrl(void *itfc_data, int hw_id, enum hw_s_ctrl_id id, void *val)
{
	int ret = 0;

	switch (id) {
	case HW_S_CTRL_FULL_BYPASS:
		break;
	case HW_S_CTRL_CHAIN_IRQ:
		break;
	case HW_S_CTRL_HWFC_IDX_RESET:
		if (hw_id == IS_VIDEO_M3P_NUM) {
			struct is_video_ctx *vctx = (struct is_video_ctx *)itfc_data;
			struct is_device_ischain *device;
			unsigned long data = (unsigned long)val;

			FIMC_BUG(!vctx);
			FIMC_BUG(!GET_DEVICE(vctx));

			device = GET_DEVICE(vctx);

			/* reset if this instance is reprocessing */
			if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
				writel(data, hwfc_rst);
		}
		break;
	case HW_S_CTRL_MCSC_SET_INPUT:
		{
			unsigned long mode = (unsigned long)val;

			info_itfc("%s: mode(%lu)\n", __func__, mode);
		}
		break;
	default:
		break;
	}

	return ret;
}

int is_hw_g_ctrl(void *itfc_data, int hw_id, enum hw_g_ctrl_id id, void *val)
{
	int ret = 0;

	switch (id) {
	case HW_G_CTRL_FRM_DONE_WITH_DMA:
		if (hw_id == DEV_HW_YPP)
			*(bool *)val = false;
		else
			*(bool *)val = true;
		break;
	case HW_G_CTRL_HAS_MCSC:
		*(bool *)val = true;
		break;
	case HW_G_CTRL_HAS_VRA_CH1_ONLY:
		*(bool *)val = true;
		break;
	}

	return ret;
}

int is_hw_query_cap(void *cap_data, int hw_id)
{
	int ret = 0;

	FIMC_BUG(!cap_data);

	switch (hw_id) {
	case DEV_HW_MCSC0:
		{
			struct is_hw_mcsc_cap *cap = (struct is_hw_mcsc_cap *)cap_data;

			cap->hw_ver = HW_SET_VERSION(8, 0, 1, 0);
			cap->max_output = 5;
			cap->max_djag = 1;
			cap->max_cac = 1;
			cap->max_uvsp = 2;
			cap->hwfc = MCSC_CAP_SUPPORT;
			cap->in_otf = MCSC_CAP_SUPPORT;
			cap->in_dma = MCSC_CAP_NOT_SUPPORT;
			cap->out_dma[0] = MCSC_CAP_SUPPORT;
			cap->out_dma[1] = MCSC_CAP_SUPPORT;
			cap->out_dma[2] = MCSC_CAP_SUPPORT;
			cap->out_dma[3] = MCSC_CAP_SUPPORT;
			cap->out_dma[4] = MCSC_CAP_SUPPORT;
			cap->out_dma[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[0] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[1] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[2] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[3] = MCSC_CAP_SUPPORT;
			cap->out_hwfc[4] = MCSC_CAP_SUPPORT;
			cap->out_post[0] = MCSC_CAP_SUPPORT;
			cap->out_post[1] = MCSC_CAP_SUPPORT;
			cap->out_post[2] = MCSC_CAP_SUPPORT;
			cap->out_post[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[5] = MCSC_CAP_NOT_SUPPORT;
			cap->enable_shared_output = false;
			cap->tdnr = MCSC_CAP_NOT_SUPPORT;
			cap->djag = MCSC_CAP_SUPPORT;
			cap->cac = MCSC_CAP_SUPPORT;
			cap->uvsp = MCSC_CAP_NOT_SUPPORT;
			cap->ysum = MCSC_CAP_NOT_SUPPORT;
			cap->ds_vra = MCSC_CAP_NOT_SUPPORT;
		}
		break;
	case DEV_HW_MCSC1:
		break;
	default:
		break;
	}

	return ret;
}

void __iomem *is_hw_get_sysreg(ulong core_regs)
{
	if (core_regs)
		err_itfc("%s: core_regs(%p)\n", __func__, (void *)core_regs);

	/* deprecated */

	return NULL;
}

u32 is_hw_find_settle(u32 mipi_speed, u32 use_cphy)
{
	u32 align_mipi_speed;
	u32 find_mipi_speed;
	const u32 *settle_table;
	size_t max;
	int s, e, m;

	if (use_cphy) {
		settle_table = is_csi_settle_table_cphy;
		max = sizeof(is_csi_settle_table_cphy) / sizeof(u32);
	} else {
		settle_table = is_csi_settle_table;
		max = sizeof(is_csi_settle_table) / sizeof(u32);
	}
	align_mipi_speed = ALIGN(mipi_speed, 10);

	s = 0;
	e = max - 2;

	if (settle_table[s] < align_mipi_speed)
		return settle_table[s + 1];

	if (settle_table[e] > align_mipi_speed)
		return settle_table[e + 1];

	/* Binary search */
	while (s <= e) {
		m = ALIGN((s + e) / 2, 2);
		find_mipi_speed = settle_table[m];

		if (find_mipi_speed == align_mipi_speed)
			break;
		else if (find_mipi_speed > align_mipi_speed)
			s = m + 2;
		else
			e = m - 2;
	}

	return settle_table[m + 1];
}

unsigned int get_dma(struct is_device_sensor *device, u32 *dma_ch)
{
	*dma_ch = 0;

	return 0;
}

void is_hw_camif_init(void)
{
	/* TODO */
}

#ifdef USE_CAMIF_FIX_UP
int is_hw_camif_fix_up(struct is_device_sensor *sensor)
{
	int ret = 0;

	/* TODO */

	return ret;
}
#endif

int is_hw_camif_cfg(void *sensor_data)
{
	int ret = 0;

	/* TODO */

	return ret;
}

int is_hw_camif_open(void *sensor_data)
{
	struct is_device_sensor *sensor;
	struct is_device_csi *csi;

	FIMC_BUG(!sensor_data);

	sensor = sensor_data;
	csi = (struct is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);

	if (csi->ch >= CSI_ID_MAX) {
		merr("CSI channel is invalid(%d)\n", sensor, csi->ch);
		return -EINVAL;
	}

	set_bit(CSIS_DMA_ENABLE, &csi->state);

#ifdef SOC_SSVC0
	csi->dma_subdev[CSI_VIRTUAL_CH_0] = &sensor->ssvc0;
#else
	csi->dma_subdev[CSI_VIRTUAL_CH_0] = NULL;
#endif
#ifdef SOC_SSVC1
	csi->dma_subdev[CSI_VIRTUAL_CH_1] = &sensor->ssvc1;
#else
	csi->dma_subdev[CSI_VIRTUAL_CH_1] = NULL;
#endif
#ifdef SOC_SSVC2
	csi->dma_subdev[CSI_VIRTUAL_CH_2] = &sensor->ssvc2;
#else
	csi->dma_subdev[CSI_VIRTUAL_CH_2] = NULL;
#endif
#ifdef SOC_SSVC3
	csi->dma_subdev[CSI_VIRTUAL_CH_3] = &sensor->ssvc3;
#else
	csi->dma_subdev[CSI_VIRTUAL_CH_3] = NULL;
#endif

	return 0;
}

void is_hw_ischain_qe_cfg(void)
{
	dbg_hw(2, "%s()\n", __func__);
}

int blk_dns_mux_control(struct is_device_ischain *device, u32 value)
{
	void __iomem *dns_sys_regs;
	u32 dns_val = 0;
	int ret = 0;

	FIMC_BUG(!device);
	dns_sys_regs = ioremap_nocache(SYSREG_DNS_BASE_ADDR, 0x1000);

	/* DNS input mux setting */
	/* DNS0 <- TNR */
	dns_val = is_hw_get_reg(dns_sys_regs,
				&sysreg_dns_regs[SYSREG_R_DNS_DNS_USER_CON0]);
	dns_val = is_hw_set_field_value(dns_val,
				&sysreg_dns_fields[SYSREG_F_DNS_GLUEMUX_DNS0_VAL], value);

	info("SYSREG_R_DNS_USER_CON0:(0x%08X)\n", dns_val);
	is_hw_set_reg(dns_sys_regs, &sysreg_dns_regs[SYSREG_R_DNS_DNS_USER_CON0], dns_val);

	iounmap(dns_sys_regs);
	return ret;
}

int is_hw_ischain_cfg(void *ischain_data)
{
	int ret = 0;
	struct is_device_ischain *device;

	FIMC_BUG(!ischain_data);

	device = (struct is_device_ischain *)ischain_data;
	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		return ret;

	return ret;
}

int is_hw_ischain_enable(struct is_device_ischain *device)
{
	int ret = 0;

	FIMC_BUG(!device);

	ret = blk_dns_mux_control(device, 1);
	if (!ret)
		merr("blk_dns_mux_control is failed (%d)\n", device, ret);

	info("%s: complete\n", __func__);

	return ret;
}

int is_hw_ischain_disable(struct is_device_ischain *device)
{
	int ret = 0;

	FIMC_BUG(!device);

	info("%s: complete\n", __func__);

	return ret;
}

/* TODO: remove this, compile check only */
#ifdef ENABLE_HWACG_CONTROL
void is_hw_csi_qchannel_enable_all(bool enable)
{
	void __iomem *csi0_regs;
	void __iomem *csi1_regs;
	void __iomem *csi2_regs;
	void __iomem *csi3_regs;
	void __iomem *csi3_1_regs;

	u32 reg_val;

	csi0_regs = ioremap_nocache(CSIS0_QCH_EN_ADDR, SZ_4);
	csi1_regs = ioremap_nocache(CSIS1_QCH_EN_ADDR, SZ_4);
	csi2_regs = ioremap_nocache(CSIS2_QCH_EN_ADDR, SZ_4);
	csi3_regs = ioremap_nocache(CSIS3_QCH_EN_ADDR, SZ_4);
	csi3_1_regs = ioremap_nocache(CSIS3_1_QCH_EN_ADDR, SZ_4);

	reg_val = readl(csi0_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi0_regs);

	reg_val = readl(csi1_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi1_regs);

	reg_val = readl(csi2_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi2_regs);

	reg_val = readl(csi3_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi3_regs);

	reg_val = readl(csi3_1_regs);
	reg_val &= ~(1 << 20);
	writel(enable << 20 | reg_val, csi3_1_regs);

	iounmap(csi0_regs);
	iounmap(csi1_regs);
	iounmap(csi2_regs);
	iounmap(csi3_regs);
	iounmap(csi3_1_regs);
}
#endif

void is_hw_djag_adjust_out_size(struct is_device_ischain *ischain,
					u32 in_width, u32 in_height,
					u32 *out_width, u32 *out_height)
{
	struct is_global_param *g_param;
	int bratio;
	bool is_down_scale;

	if (!ischain) {
		err_hw("device is NULL");
		return;
	}

	g_param = &ischain->resourcemgr->global_param;
	is_down_scale = (*out_width < in_width) || (*out_height < in_height);
	bratio = is_sensor_g_bratio(ischain->sensor);
	if (bratio < 0) {
		err_hw("failed to get sensor_bratio");
		return;
	}

	dbg_hw(2, "%s:video_mode %d is_down_scale %d bratio %d\n", __func__,
			g_param->video_mode, is_down_scale, bratio);

	if (g_param->video_mode
		&& is_down_scale
		&& bratio >= MCSC_DJAG_ENABLE_SENSOR_BRATIO) {
		dbg_hw(2, "%s:%dx%d -> %dx%d\n", __func__,
				*out_width, *out_height, in_width, in_height);

		*out_width = in_width;
		*out_height = in_height;
	}
}

void __nocfi is_hw_interrupt_relay(struct is_group *group, void *hw_ip_data)
{
	struct is_group *child;
	struct is_hw_ip *hw_ip = (struct is_hw_ip *)hw_ip_data;

	child = group->child;
	if (child) {
		int hw_list[GROUP_HW_MAX], hw_slot;
		enum is_hardware_id hw_id;

		is_get_hw_list(child->id, hw_list);
		hw_id = hw_list[0];
		hw_slot = is_hw_slot_id(hw_id);
		if (!valid_hw_slot_id(hw_slot)) {
			serr_hw("invalid slot (%d,%d)", hw_ip, hw_id, hw_slot);
		} else {
			struct is_hardware *hardware;
			struct is_hw_ip *hw_ip_child;
			struct hwip_intr_handler *intr_handler;

			hardware = hw_ip->hardware;
			if (!hardware) {
				serr_hw("hardware is NILL", hw_ip);
			} else {
				hw_ip_child = &hardware->hw_ip[hw_slot];
				intr_handler = hw_ip_child->intr_handler[INTR_HWIP3];
				if (intr_handler && intr_handler->handler) {
					is_enter_lib_isr();
					intr_handler->handler(intr_handler->id, intr_handler->ctx);
					is_exit_lib_isr();
				}
			}
		}
	}
}

int is_hw_check_gframe_skip(void *group_data)
{
	int ret = 0;
	struct is_group *group;

	FIMC_BUG(!group_data);

	group = (struct is_group *)group_data;

	switch (group->id) {
	case GROUP_ID_VRA0:
	case GROUP_ID_CLH0:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

void is_hw_configure_llc(bool on, int scenario_id)
{
#if IS_ENABLED(CONFIC_EXYNOS_SCI)
	/* way 1 means alloc 512K LLC */
	int way_csis = 0, way_mcfp = 0;

	if (on) {
		switch (scenario_id) {
		default:
			/* CSIS: 1MB, MCFP: 4MB */
			way_csis = 2;
			way_mcfp = 8;
			break;
		}

		llc_region_alloc(LLC_REGION_CAM_CSIS, 1, way_csis);
		llc_region_alloc(LLC_REGION_CAM_MCFP, 1, way_mcfp);

		info("LLC alloc in scenario(%d), CSIS:%d.%dMB, MCFP:%d.%dMB", scenario_id,
				way_csis / 2, (way_csis % 2) ? 5 : 0,
				way_mcfp / 2, (way_mcfp % 2) ? 5 : 0);
	} else {
		llc_region_alloc(LLC_REGION_CAM_CSIS, 0, 0);
		llc_region_alloc(LLC_REGION_CAM_MCFP, 0, 0);

		info("LLC is released");
	}
#endif
}

void is_hw_configure_bts_scen(struct is_resourcemgr *resourcemgr, int scenario_id)
{
	int bts_index = 0;

	switch (scenario_id) {
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K24:
	case IS_DVFS_SN_REAR_SINGLE_TELE_VIDEO_8K30:
		bts_index = 1;
		break;
	default:
		bts_index = 0;
		break;
	}

	/* If default scenario & specific scenario selected,
	 * off specific scenario first.
	 */
	if (resourcemgr->cur_bts_scen_idx && bts_index == 0)
		is_bts_scen(resourcemgr, resourcemgr->cur_bts_scen_idx, false);

	if (bts_index && bts_index != resourcemgr->cur_bts_scen_idx)
		is_bts_scen(resourcemgr, bts_index, true);
	resourcemgr->cur_bts_scen_idx = bts_index;
}
