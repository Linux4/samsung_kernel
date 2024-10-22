// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include "scpsys.h"
#include "mtk-scpsys.h"

#include <dt-bindings/power/mt6989-power.h>

#define SCPSYS_BRINGUP			(0)
#if SCPSYS_BRINGUP
#define default_cap			(MTK_SCPD_BYPASS_OFF)
#else
#define default_cap			(0)
#endif

#define MT6989_TOP_AXI_PROT_EN_INFRASYS1_MD	(BIT(12))
#define MT6989_TOP_AXI_PROT_EN_INFRASYS0_MD	(BIT(11))
#define MT6989_TOP_AXI_PROT_EN_EMISYS0_MD	(BIT(17) | BIT(18))
#define MT6989_TOP_AXI_PROT_EN_EMISYS1_MD	(BIT(17) | BIT(18))
#define MT6989_TOP_AXI_PROT_EN_INFRASYS0_CONN	(BIT(25))
#define MT6989_TOP_AXI_PROT_EN_CONNSYS0_CONN	(BIT(1))
#define MT6989_TOP_AXI_PROT_EN_INFRASYS0_CONN_2ND	(BIT(26))
#define MT6989_TOP_AXI_PROT_EN_CONNSYS0_CONN_2ND	(BIT(0))
#define MT6989_TOP_AXI_PROT_EN_PERISYS0_PERI_USB0	(BIT(8))
#define MT6989_VLP_AXI_PROT_EN1_PEXTP_MAC0	(BIT(11))
#define MT6989_VLP_AXI_PROT_EN1_PEXTP_MAC1	(BIT(10))
#define MT6989_VLP_AXI_PROT_EN1_PEXTP_PHY0	(BIT(13))
#define MT6989_VLP_AXI_PROT_EN1_PEXTP_PHY1	(BIT(12))
#define MT6989_TOP_AXI_PROT_EN_PERISYS0_AUDIO	(BIT(5))
#define MT6989_VLP_AXI_PROT_EN_ADSP_TOP	(BIT(14) | BIT(15) |  \
			BIT(16) | BIT(17) |  \
			BIT(18) | BIT(21) |  \
			BIT(22) | BIT(23) |  \
			BIT(25))
#define MT6989_VLP_AXI_PROT_EN1_ADSP_TOP	(BIT(14) | BIT(15) |  \
			BIT(16) | BIT(17) |  \
			BIT(18) | BIT(19) |  \
			BIT(20) | BIT(21) |  \
			BIT(22) | BIT(23) |  \
			BIT(25))
#define MT6989_TOP_AXI_PROT_EN_INFRASYS1_ADSP_TOP	(BIT(14))
#define MT6989_VLP_AXI_PROT_EN1_ADSP_AO	(BIT(23) | BIT(25))
#define MT6989_TOP_AXI_PROT_EN_INFRASYS1_ADSP_AO	(BIT(14))
#define MT6989_VLP_AXI_PROT_EN1_ADSP_AO_2ND	(BIT(24))
#define MT6989_TOP_AXI_PROT_EN_INFRASYS0_ADSP_AO	(BIT(29) | BIT(30))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_ISP_DIP1	(BIT(2))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_ISP_DIP1_2ND	(BIT(3))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_ISP_MAIN	(BIT(3))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_ISP_MAIN_2ND	(BIT(5))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_ISP_MAIN	(BIT(0))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_ISP_MAIN_2ND	(BIT(1))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_ISP_VCORE	(BIT(14))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_ISP_VCORE_2ND	(BIT(15) | BIT(25))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_VDE0	(BIT(10))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_VDE0	(BIT(30))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_VDE0_2ND	(BIT(28))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_VDE0_2ND	(BIT(11))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_VDE1	(BIT(20))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_VDE1	(BIT(26))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_VDE1_2ND	(BIT(24))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_VDE1_2ND	(BIT(21))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_VEN0	(BIT(12))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_VEN0	(BIT(14))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_VEN0_2ND	(BIT(13))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_VEN0_2ND	(BIT(15))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_VEN1	(BIT(18) | BIT(19))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_VEN1	(BIT(22))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_VEN1	(BIT(16))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_VEN1_2ND	(BIT(23))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_VEN1_2ND	(BIT(17))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_VEN2	(BIT(18) | BIT(19))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_CAM_MRAW	(BIT(7))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_CAM_MRAW_2ND	(BIT(26))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBA	(BIT(9))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBA_2ND	(BIT(6))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_CAM_SUBB	(BIT(9))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBB	(BIT(8))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBC	(BIT(11))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBC_2ND	(BIT(10))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_CAM_MAIN	(BIT(16))
#define MT6989_TOP_AXI_PROT_EN_INFRASYS0_CAM_MAIN	(BIT(27))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_MAIN	(BIT(4))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_VCORE	(BIT(5))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_CAM_VCORE	(BIT(17) | BIT(27) |  \
			BIT(31))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_CAM_VCORE	(BIT(19))
#define MT6989_TOP_AXI_PROT_EN_CCUSYS0_CAM_VCORE	(BIT(12))
#define MT6989_TOP_AXI_PROT_EN_CCUSYS0_CAM_CCU	(BIT(9) | BIT(10))
#define MT6989_TOP_AXI_PROT_EN_CCUSYS0_CAM_CCU_2ND	(BIT(8) | BIT(11))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_DISP_VCORE	(BIT(23) | BIT(25) |  \
			BIT(27) | BIT(29))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_DISP_VCORE	(BIT(16) | BIT(17))
#define MT6989_TOP_AXI_PROT_EN_MMSYS3_DISP_VCORE	(BIT(22) | BIT(23) |  \
			BIT(24) | BIT(25) |  \
			BIT(26) | BIT(27))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_DISP_VCORE_2ND	(BIT(2) | BIT(10) |  \
			BIT(12) | BIT(13) |  \
			BIT(20) | BIT(22))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_DISP_VCORE	(BIT(1))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_DISP_VCORE_3RD	(BIT(1) | BIT(3) |  \
			BIT(11))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_DISP_VCORE_2ND	(BIT(13) | BIT(27))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_MML0	(BIT(0))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_MML0	(BIT(31))
#define MT6989_TOP_AXI_PROT_EN_MMSYS3_MML0	(BIT(16) | BIT(17))
#define MT6989_TOP_AXI_PROT_EN_EMISYS0_MML0	(BIT(11))
#define MT6989_TOP_AXI_PROT_EN_EMISYS1_MML0	(BIT(11))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_MML1	(BIT(29) | BIT(30))
#define MT6989_TOP_AXI_PROT_EN_MMSYS3_MML1	(BIT(18) | BIT(19))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_DIS0	(BIT(18) | BIT(21))
#define MT6989_TOP_AXI_PROT_EN_MMSYS3_DIS0	(BIT(20) | BIT(21))
#define MT6989_TOP_AXI_PROT_EN_EMISYS0_DIS0	(BIT(12))
#define MT6989_TOP_AXI_PROT_EN_EMISYS1_DIS0	(BIT(12))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_DIS1	(BIT(8) | BIT(18) |  \
			BIT(19) | BIT(24) |  \
			BIT(28) | BIT(29) |  \
			BIT(30))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_DIS1	(BIT(0) | BIT(8) |  \
			BIT(18) | BIT(21))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_DIS1	(BIT(20) | BIT(21) |  \
			BIT(22) | BIT(23) |  \
			BIT(24) | BIT(25) |  \
			BIT(26) | BIT(28) |  \
			BIT(29) | BIT(30) |  \
			BIT(31))
#define MT6989_TOP_AXI_PROT_EN_MMSYS3_DIS1	((BIT(0) | BIT(1) |  \
			BIT(2) | BIT(3) |  \
			BIT(4) | BIT(5) |  \
			BIT(6) | BIT(7) |  \
			BIT(8) | BIT(9) |  \
			BIT(10) | BIT(11) |  \
			BIT(12) | BIT(13) |  \
			BIT(14) | BIT(15) |  \
			BIT(16) | BIT(17) |  \
			BIT(18) | BIT(19) |  \
			BIT(20) | BIT(21)))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_DIS1_2ND	(BIT(23) | BIT(25) |  \
			BIT(27) | BIT(29))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_DIS1_2ND	(BIT(16) | BIT(17))
#define MT6989_TOP_AXI_PROT_EN_MMSYS3_DIS1_2ND	(BIT(22) | BIT(23) |  \
			BIT(24) | BIT(25) |  \
			BIT(26) | BIT(27))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_OVL0	(BIT(20) | BIT(21) |  \
			BIT(22) | BIT(23) |  \
			BIT(24) | BIT(25) |  \
			BIT(26) | BIT(28))
#define MT6989_TOP_AXI_PROT_EN_MMSYS3_OVL0	(BIT(0) | BIT(1) |  \
			BIT(2) | BIT(3) |  \
			BIT(4) | BIT(5) |  \
			BIT(6) | BIT(7))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_OVL1	(BIT(8) | BIT(18) |  \
			BIT(19) | BIT(24) |  \
			BIT(28) | BIT(29) |  \
			BIT(30))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_OVL1	(BIT(8))
#define MT6989_TOP_AXI_PROT_EN_MMSYS3_OVL1	(BIT(8) | BIT(9) |  \
			BIT(10) | BIT(11) |  \
			BIT(12) | BIT(13) |  \
			BIT(14) | BIT(15))
#define MT6989_TOP_AXI_PROT_EN_INFRASYS1_MM_INFRA	(BIT(10))
#define MT6989_TOP_AXI_PROT_EN_MMSYS0_MM_INFRA	(BIT(0) | BIT(2) |  \
			BIT(4) | BIT(6))
#define MT6989_TOP_AXI_PROT_EN_MMSYS1_MM_INFRA	(BIT(4) | BIT(6) |  \
			BIT(7))
#define MT6989_TOP_AXI_PROT_EN_MMSYS2_MM_INFRA	(BIT(12))
#define MT6989_TOP_AXI_PROT_EN_MMSYS3_MM_INFRA	(BIT(28) | BIT(29))
#define MT6989_BCRM_INFRA1_PROT_EN_BCRM_INFRA_AO10_MM_INFRA	(BIT(1))
#define MT6989_TOP_AXI_PROT_EN_INFRASYS0_MM_INFRA	(BIT(8) | BIT(9))
#define MT6989_TOP_AXI_PROT_EN_MMSYS3_MM_INFRA_2ND	(BIT(30) | BIT(31))
#define MT6989_TOP_AXI_PROT_EN_EMISYS0_MM_INFRA	(BIT(21) | BIT(22))
#define MT6989_TOP_AXI_PROT_EN_EMISYS1_MM_INFRA	(BIT(21) | BIT(22))
#define MT6989_NEMICFG_AO_MEM_PROT_REG_PROT_EN_GLITCH_MM_INFRA	(BIT(4) | BIT(5) |  \
			BIT(9))
#define MT6989_SEMICFG_AO_MEM_PROT_REG_PROT_EN_GLITCH_MM_INFRA	(BIT(4) | BIT(5))
#define MT6989_VLP_AXI_PROT_EN_MM_INFRA	(BIT(11) | BIT(12))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_ISP_DIP1	(BIT(2))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_ISP_DIP1_2ND	(BIT(3))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_ISP_MAIN	(BIT(3))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_ISP_MAIN_2ND	(BIT(5))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_ISP_MAIN	(BIT(0))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_ISP_MAIN_2ND	(BIT(1))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_ISP_VCORE	(BIT(14))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_ISP_VCORE_2ND	(BIT(15) | BIT(25))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_VDE0	(BIT(10))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_VDE0	(BIT(30))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_VDE0_2ND	(BIT(28))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_VDE0_2ND	(BIT(11))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_VDE1	(BIT(20))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_VDE1	(BIT(26))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_VDE1_2ND	(BIT(24))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_VDE1_2ND	(BIT(21))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_VEN0	(BIT(12))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_VEN0	(BIT(14))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_VEN0_2ND	(BIT(13))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_VEN0_2ND	(BIT(15))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_VEN1	(BIT(18) | BIT(19))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_VEN1	(BIT(22))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_VEN1	(BIT(16))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_VEN1_2ND	(BIT(23))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_VEN1_2ND	(BIT(17))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_VEN2	(BIT(18) | BIT(19))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_CAM_MRAW	(BIT(7))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_CAM_MRAW_2ND	(BIT(26))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_CAM_SUBA	(BIT(9))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_CAM_SUBA_2ND	(BIT(6))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_CAM_SUBB	(BIT(9))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_CAM_SUBB	(BIT(8))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_CAM_SUBC	(BIT(11))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_CAM_SUBC_2ND	(BIT(10))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_CAM_MAIN	(BIT(16))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_CAM_MAIN	(BIT(4))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_CAM_VCORE	(BIT(5))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_CAM_VCORE	(BIT(17) | BIT(27) |  \
			BIT(31))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_CAM_VCORE	(BIT(19))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS4_CAM_VCORE	(BIT(12))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS4_CAM_CCU	(BIT(10) | BIT(15))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS4_CAM_CCU_2ND	(BIT(11) | BIT(14))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_DISP_VCORE	(BIT(23) | BIT(25) |  \
			BIT(27) | BIT(29))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_DISP_VCORE	(BIT(16) | BIT(17))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS3_DISP_VCORE	(BIT(22) | BIT(23) |  \
			BIT(24) | BIT(25) |  \
			BIT(26) | BIT(27))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_DISP_VCORE_2ND	(BIT(2) | BIT(10) |  \
			BIT(12) | BIT(13) |  \
			BIT(20) | BIT(22))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_DISP_VCORE	(BIT(1))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_DISP_VCORE_3RD	(BIT(1) | BIT(3) |  \
			BIT(11))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_DISP_VCORE_2ND	(BIT(13) | BIT(27))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_MML0	(BIT(0))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_MML0	(BIT(31))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS3_MML0	(BIT(16) | BIT(17))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_MML1	(BIT(29) | BIT(30))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS3_MML1	(BIT(18) | BIT(19))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_DIS0	(BIT(18) | BIT(21))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS3_DIS0	(BIT(20) | BIT(21))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_DIS1	(BIT(8) | BIT(18) |  \
			BIT(19) | BIT(24) |  \
			BIT(28) | BIT(29) |  \
			BIT(30))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_DIS1	(BIT(0) | BIT(8) |  \
			BIT(18) | BIT(21))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_DIS1	(BIT(20) | BIT(21) |  \
			BIT(22) | BIT(23) |  \
			BIT(24) | BIT(25) |  \
			BIT(26) | BIT(28) |  \
			BIT(29) | BIT(30) |  \
			BIT(31))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS3_DIS1	(BIT(0) | BIT(1) |  \
			BIT(2) | BIT(3) |  \
			BIT(4) | BIT(5) |  \
			BIT(6) | BIT(7) |  \
			BIT(8) | BIT(9) |  \
			BIT(10) | BIT(11) |  \
			BIT(12) | BIT(13) |  \
			BIT(14) | BIT(15) |  \
			BIT(16) | BIT(17) |  \
			BIT(18) | BIT(19) |  \
			BIT(20) | BIT(21))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_DIS1_2ND	(BIT(23) | BIT(25) |  \
			BIT(27) | BIT(29))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_DIS1_2ND	(BIT(16) | BIT(17))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS3_DIS1_2ND	(BIT(22) | BIT(23) |  \
			BIT(24) | BIT(25) |  \
			BIT(26) | BIT(27))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS2_OVL0	(BIT(20) | BIT(21) |  \
			BIT(22) | BIT(23) |  \
			BIT(24) | BIT(25) |  \
			BIT(26) | BIT(28))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS3_OVL0	(BIT(0) | BIT(1) |  \
			BIT(2) | BIT(3) |  \
			BIT(4) | BIT(5) |  \
			BIT(6) | BIT(7))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS0_OVL1	(BIT(8) | BIT(18) |  \
			BIT(19) | BIT(24) |  \
			BIT(28) | BIT(29) |  \
			BIT(30))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS1_OVL1	(BIT(8))
#define MT6989_HFRP_1_PROT_EN_HFRP_MMSYS3_OVL1	(BIT(8) | BIT(9) |  \
			BIT(10) | BIT(11) |  \
			BIT(12) | BIT(13) |  \
			BIT(14) | BIT(15))

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE = 1,
	VLP_TYPE = 2,
	BCRM_INFRA1_TYPE = 3,
	NEMICFG_AO_MEM_PROT_REG_TYPE = 4,
	SEMICFG_AO_MEM_PROT_REG_TYPE = 5,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[IFR_TYPE] = "ifr-bus",
	[VLP_TYPE] = "vlpcfg",
	[BCRM_INFRA1_TYPE] = "bcrm-infra1-bus",
	[NEMICFG_AO_MEM_PROT_REG_TYPE] = "nemicfg-ao-mem-prot-reg-bus",
	[SEMICFG_AO_MEM_PROT_REG_TYPE] = "semicfg-ao-mem-prot-reg-bus",
};

/*
 * MT6989 power domain support
 */

static const struct scp_domain_data scp_domain_mt6989_spm_data[] = {
	[MT6989_POWER_DOMAIN_MD] = {
		.name = "md",
		.ctl_offs = 0xE00,
		.extb_iso_offs = 0xF60,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x024, 0x028, 0x020, 0x02c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS1_MD),
			BUS_PROT(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS0_MD),
			BUS_PROT(IFR_TYPE, 0x124, 0x128, 0x120, 0x12c,
				MT6989_TOP_AXI_PROT_EN_EMISYS0_MD),
			BUS_PROT(IFR_TYPE, 0x104, 0x108, 0x100, 0x10c,
				MT6989_TOP_AXI_PROT_EN_EMISYS1_MD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_MD_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6989_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x1c4, 0x1c8, 0x1c0, 0x1cc,
				MT6989_TOP_AXI_PROT_EN_CONNSYS0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS0_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x1c4, 0x1c8, 0x1c0, 0x1cc,
				MT6989_TOP_AXI_PROT_EN_CONNSYS0_CONN_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6989_POWER_DOMAIN_PERI_USB0] = {
		.name = "peri-usb0",
		.ctl_offs = 0xE10,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0e4, 0x0e8, 0x0e0, 0x0ec,
				MT6989_TOP_AXI_PROT_EN_PERISYS0_PERI_USB0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_PEXTP_MAC0] = {
		.name = "pextp-mac0",
		.ctl_offs = 0xE1C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_PEXTP_MAC0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_PEXTP_MAC1] = {
		.name = "pextp-mac1",
		.ctl_offs = 0xE20,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_PEXTP_MAC1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_PEXTP_PHY0] = {
		.name = "pextp-phy0",
		.ctl_offs = 0xE24,
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_PEXTP_PHY0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_PEXTP_PHY1] = {
		.name = "pextp-phy1",
		.ctl_offs = 0xE28,
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_PEXTP_PHY1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.ctl_offs = 0xE2C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"aud_bus"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0e4, 0x0e8, 0x0e0, 0x0ec,
				MT6989_TOP_AXI_PROT_EN_PERISYS0_AUDIO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_ADSP_TOP_DORMANT] = {
		.name = "adsp-top-dormant",
		.ctl_offs = 0xE34,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"adsp"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6989_VLP_AXI_PROT_EN_ADSP_TOP),
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_ADSP_TOP),
			BUS_PROT_IGN(IFR_TYPE, 0x024, 0x028, 0x020, 0x02c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS1_ADSP_TOP),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON
				| default_cap,
	},
	[MT6989_POWER_DOMAIN_ADSP_AO] = {
		.name = "adsp-ao",
		.ctl_offs = 0xE3C,
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_ADSP_AO),
			BUS_PROT_IGN(IFR_TYPE, 0x024, 0x028, 0x020, 0x02c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS1_ADSP_AO),
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_ADSP_AO_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS0_ADSP_AO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_ISP_TRAW] = {
		.name = "isp-traw",
		.ctl_offs = 0xE40,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_ISP_DIP1] = {
		.name = "isp-dip1",
		.ctl_offs = 0xE44,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_ISP_DIP1),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_ISP_DIP1_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp-main",
		.ctl_offs = 0xE48,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"img1", "ipe"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_ISP_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_ISP_MAIN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_ISP_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_ISP_MAIN_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_ISP_VCORE] = {
		.name = "isp-vcore",
		.ctl_offs = 0xE4C,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_ISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_ISP_VCORE_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_VDE0] = {
		.name = "vde0",
		.ctl_offs = 0xE50,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vde"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_VDE0),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_VDE0),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_VDE0_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_VDE0_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_VDE1] = {
		.name = "vde1",
		.ctl_offs = 0xE54,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_VDE1),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_VDE1),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_VDE1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_VDE1_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_VDE_VCORE0] = {
		.name = "vde-vcore0",
		.ctl_offs = 0xE58,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_VDE_VCORE1] = {
		.name = "vde-vcore1",
		.ctl_offs = 0xE5C,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_VEN0] = {
		.name = "ven0",
		.ctl_offs = 0xE60,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_VEN0_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_VEN0_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_VEN1] = {
		.name = "ven1",
		.ctl_offs = 0xE64,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_VEN1),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_VEN1),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_VEN1),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_VEN1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_VEN1_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_VEN2] = {
		.name = "ven2",
		.ctl_offs = 0xE68,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_VEN2),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_MRAW] = {
		.name = "cam-mraw",
		.ctl_offs = 0xE6C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_mraw",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_CAM_MRAW),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_CAM_MRAW_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_SUBA] = {
		.name = "cam-suba",
		.ctl_offs = 0xE70,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_suba",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBA),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBA_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_SUBB] = {
		.name = "cam-subb",
		.ctl_offs = 0xE74,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_subb",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_CAM_SUBB),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBB),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_SUBC] = {
		.name = "cam-subc",
		.ctl_offs = 0xE78,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_subc",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBC),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_SUBC_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam-main",
		.ctl_offs = 0xE7C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam", "dpe"},
		.subsys_clk_prefix = "cam",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_CAM_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS0_CAM_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_MAIN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_VCORE] = {
		.name = "cam-vcore",
		.ctl_offs = 0xE80,
		.basic_clk_name = {"ccu_ahb"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x264, 0x268, 0x260, 0x26c,
				MT6989_TOP_AXI_PROT_EN_CCUSYS0_CAM_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_CCU] = {
		.name = "cam-ccu",
		.ctl_offs = 0xE84,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ccusys"},
		.subsys_clk_prefix = "ccu",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x264, 0x268, 0x260, 0x26c,
				MT6989_TOP_AXI_PROT_EN_CCUSYS0_CAM_CCU),
			BUS_PROT_IGN(IFR_TYPE, 0x264, 0x268, 0x260, 0x26c,
				MT6989_TOP_AXI_PROT_EN_CCUSYS0_CAM_CCU_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_CCU_AO] = {
		.name = "cam-ccu-ao",
		.ctl_offs = 0xE88,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_DISP_VCORE] = {
		.name = "disp-vcore",
		.ctl_offs = 0xE8C,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_DISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_DISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2a0, 0x2ac,
				MT6989_TOP_AXI_PROT_EN_MMSYS3_DISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_DISP_VCORE_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_DISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_DISP_VCORE_3RD),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_DISP_VCORE_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_MML0_SHUTDOWN] = {
		.name = "mml0-shutdown",
		.ctl_offs = 0xE90,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mdp"},
		.subsys_clk_prefix = "mdp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_MML0),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_MML0),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2a0, 0x2ac,
				MT6989_TOP_AXI_PROT_EN_MMSYS3_MML0),
			BUS_PROT_IGN(IFR_TYPE, 0x124, 0x128, 0x120, 0x12c,
				MT6989_TOP_AXI_PROT_EN_EMISYS0_MML0),
			BUS_PROT_IGN(IFR_TYPE, 0x104, 0x108, 0x100, 0x10c,
				MT6989_TOP_AXI_PROT_EN_EMISYS1_MML0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
			| default_cap,
	},
	[MT6989_POWER_DOMAIN_MML1_SHUTDOWN] = {
		.name = "mml1-shutdown",
		.ctl_offs = 0xE94,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mdp"},
		.subsys_clk_prefix = "mdp1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_MML1),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2a0, 0x2ac,
				MT6989_TOP_AXI_PROT_EN_MMSYS3_MML1),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
			| default_cap,
	},
	[MT6989_POWER_DOMAIN_DIS0_SHUTDOWN] = {
		.name = "dis0-shutdown",
		.ctl_offs = 0xE98,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "disp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_DIS0),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2a0, 0x2ac,
				MT6989_TOP_AXI_PROT_EN_MMSYS3_DIS0),
			BUS_PROT_IGN(IFR_TYPE, 0x124, 0x128, 0x120, 0x12c,
				MT6989_TOP_AXI_PROT_EN_EMISYS0_DIS0),
			BUS_PROT_IGN(IFR_TYPE, 0x104, 0x108, 0x100, 0x10c,
				MT6989_TOP_AXI_PROT_EN_EMISYS1_DIS0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6989_POWER_DOMAIN_DIS1_SHUTDOWN] = {
		.name = "dis1-shutdown",
		.ctl_offs = 0xE9C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "disp1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_DIS1),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_DIS1),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_DIS1),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2a0, 0x2ac,
				MT6989_TOP_AXI_PROT_EN_MMSYS3_DIS1),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_DIS1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_DIS1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2a0, 0x2ac,
				MT6989_TOP_AXI_PROT_EN_MMSYS3_DIS1_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6989_POWER_DOMAIN_OVL0_SHUTDOWN] = {
		.name = "ovl0-shutdown",
		.ctl_offs = 0xEA0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "ovl",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_OVL0),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2a0, 0x2ac,
				MT6989_TOP_AXI_PROT_EN_MMSYS3_OVL0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6989_POWER_DOMAIN_OVL1_SHUTDOWN] = {
		.name = "ovl1-shutdown",
		.ctl_offs = 0xEA4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "ovl1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_OVL1),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_OVL1),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2a0, 0x2ac,
				MT6989_TOP_AXI_PROT_EN_MMSYS3_OVL1),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF
				| default_cap,
	},
	[MT6989_POWER_DOMAIN_MM_INFRA] = {
		.name = "mm-infra",
		.ctl_offs = 0xEA8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm_infra"},
		.subsys_lp_clk_prefix = "mm_infra_lp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x024, 0x028, 0x020, 0x02c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS1_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6989_TOP_AXI_PROT_EN_MMSYS0_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6989_TOP_AXI_PROT_EN_MMSYS1_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6989_TOP_AXI_PROT_EN_MMSYS2_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2A0, 0x2ac,
				MT6989_TOP_AXI_PROT_EN_MMSYS3_MM_INFRA),
			BUS_PROT_IGN(BCRM_INFRA1_TYPE, 0x000, 0x004, 0x008, 0x00c,
				MT6989_BCRM_INFRA1_PROT_EN_BCRM_INFRA_AO10_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS0_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x2a4, 0x2a8, 0x2A0, 0x2ac,
				MT6989_TOP_AXI_PROT_EN_MMSYS3_MM_INFRA_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x124, 0x128, 0x120, 0x12c,
				MT6989_TOP_AXI_PROT_EN_EMISYS0_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x104, 0x108, 0x100, 0x10c,
				MT6989_TOP_AXI_PROT_EN_EMISYS1_MM_INFRA),
			BUS_PROT_IGN(NEMICFG_AO_MEM_PROT_REG_TYPE, 0x84, 0x88, 0x40, 0x8c,
				MT6989_NEMICFG_AO_MEM_PROT_REG_PROT_EN_GLITCH_MM_INFRA),
			BUS_PROT_IGN(SEMICFG_AO_MEM_PROT_REG_TYPE, 0x84, 0x88, 0x40, 0x8c,
				MT6989_SEMICFG_AO_MEM_PROT_REG_PROT_EN_GLITCH_MM_INFRA),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6989_VLP_AXI_PROT_EN_MM_INFRA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | MTK_SCPD_BYPASS_CLK
				| default_cap,
	},
	[MT6989_POWER_DOMAIN_DP_TX] = {
		.name = "dp-tx",
		.ctl_offs = 0xEB0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_CSI_RX] = {
		.name = "csi-rx",
		.ctl_offs = 0xEF4,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_NON_CPU_RTFF | default_cap,
	},
	[MT6989_POWER_DOMAIN_SSR] = {
		.name = "ssrsys",
		.hwv_comp = "hw-voter-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 0,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6989_POWER_DOMAIN_APU] = {
		.name = "apu",
		.caps = MTK_SCPD_APU_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
};

static const struct scp_domain_data scp_domain_mt6989_hwv_data[] = {
	[MT6989_POWER_DOMAIN_MD] = {
		.name = "md",
		.ctl_offs = 0xE00,
		.extb_iso_offs = 0xF60,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x024, 0x028, 0x020, 0x02c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS1_MD),
			BUS_PROT(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS0_MD),
			BUS_PROT(IFR_TYPE, 0x124, 0x128, 0x120, 0x12c,
				MT6989_TOP_AXI_PROT_EN_EMISYS0_MD),
			BUS_PROT(IFR_TYPE, 0x104, 0x108, 0x100, 0x10c,
				MT6989_TOP_AXI_PROT_EN_EMISYS1_MD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_MD_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6989_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x1c4, 0x1c8, 0x1c0, 0x1cc,
				MT6989_TOP_AXI_PROT_EN_CONNSYS0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS0_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x1c4, 0x1c8, 0x1c0, 0x1cc,
				MT6989_TOP_AXI_PROT_EN_CONNSYS0_CONN_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6989_POWER_DOMAIN_PERI_USB0] = {
		.name = "peri-usb0",
		.ctl_offs = 0xE10,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0e4, 0x0e8, 0x0e0, 0x0ec,
				MT6989_TOP_AXI_PROT_EN_PERISYS0_PERI_USB0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_PEXTP_MAC0] = {
		.name = "pextp-mac0",
		.ctl_offs = 0xE1C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_PEXTP_MAC0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_PEXTP_MAC1] = {
		.name = "pextp-mac1",
		.ctl_offs = 0xE20,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_PEXTP_MAC1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_PEXTP_PHY0] = {
		.name = "pextp-phy0",
		.ctl_offs = 0xE24,
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_PEXTP_PHY0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_PEXTP_PHY1] = {
		.name = "pextp-phy1",
		.ctl_offs = 0xE28,
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_PEXTP_PHY1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.ctl_offs = 0xE2C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"aud_bus"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0e4, 0x0e8, 0x0e0, 0x0ec,
				MT6989_TOP_AXI_PROT_EN_PERISYS0_AUDIO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_ADSP_TOP_DORMANT] = {
		.name = "adsp-top-dormant",
		.ctl_offs = 0xE34,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"adsp"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6989_VLP_AXI_PROT_EN_ADSP_TOP),
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_ADSP_TOP),
			BUS_PROT_IGN(IFR_TYPE, 0x024, 0x028, 0x020, 0x02c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS1_ADSP_TOP),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON
				| default_cap,
	},
	[MT6989_POWER_DOMAIN_ADSP_AO] = {
		.name = "adsp-ao",
		.ctl_offs = 0xE3C,
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_ADSP_AO),
			BUS_PROT_IGN(IFR_TYPE, 0x024, 0x028, 0x020, 0x02c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS1_ADSP_AO),
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6989_VLP_AXI_PROT_EN1_ADSP_AO_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6989_TOP_AXI_PROT_EN_INFRASYS0_ADSP_AO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | default_cap,
	},
	[MT6989_POWER_DOMAIN_MM_INFRA] = {
		.name = "mm-infra",
		.hwv_comp = "mminfra-hwv-regmap",
		.hwv_ofs = 0x400,
		.hwv_set_ofs = 0x0404,
		.hwv_clr_ofs = 0x0408,
		.hwv_done_ofs = 0x091C,
		.hwv_shift = 0,
		.caps = MTK_SCPD_MMINFRA_HWV_OPS | MTK_SCPD_IRQ_SAVE |
			MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_ISP_TRAW] = {
		.name = "isp-traw",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 27,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_ISP_DIP1] = {
		.name = "isp-dip1",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 28,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp-main",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 7,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_ISP_VCORE] = {
		.name = "isp-vcore",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 2,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_VDE0] = {
		.name = "vde0",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 8,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_VDE1] = {
		.name = "vde1",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 26,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_VDE_VCORE0] = {
		.name = "vde-vcore0",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 3,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_VDE_VCORE1] = {
		.name = "vde-vcore1",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 13,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_VEN0] = {
		.name = "ven0",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 4,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_VEN1] = {
		.name = "ven1",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 9,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_VEN2] = {
		.name = "ven2",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 14,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_MRAW] = {
		.name = "cam-mraw",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 16,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_SUBA] = {
		.name = "cam-suba",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 17,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_SUBB] = {
		.name = "cam-subb",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 18,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_SUBC] = {
		.name = "cam-subc",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 19,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam-main",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 11,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_VCORE] = {
		.name = "cam-vcore",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 5,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_CCU] = {
		.name = "cam-ccu",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 10,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_CAM_CCU_AO] = {
		.name = "cam-ccu-ao",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 15,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_DISP_VCORE] = {
		.name = "disp-vcore",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 6,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_MML0_SHUTDOWN] = {
		.name = "mml0-shutdown",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 23,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_MML1_SHUTDOWN] = {
		.name = "mml1-shutdown",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 24,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_DIS0_SHUTDOWN] = {
		.name = "dis0-shutdown",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 25,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_DIS1_SHUTDOWN] = {
		.name = "dis1-shutdown",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 12,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_OVL0_SHUTDOWN] = {
		.name = "ovl0-shutdown",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 20,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_OVL1_SHUTDOWN] = {
		.name = "ovl1-shutdown",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 21,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_DP_TX] = {
		.name = "dp-tx",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 22,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_CSI_RX] = {
		.name = "csi-rx",
		.hwv_comp = "mm-hw-ccf-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 1,
		.caps = MTK_SCPD_HWV_OPS | MTK_SCPD_WAIT_VCP | default_cap,
	},
	[MT6989_POWER_DOMAIN_SSR] = {
		.name = "ssrsys",
		.hwv_comp = "hw-voter-regmap",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 0,
		.caps = MTK_SCPD_HWV_OPS | default_cap,
	},
	[MT6989_POWER_DOMAIN_APU] = {
		.name = "apu",
		.caps = MTK_SCPD_APU_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
};

static const struct scp_subdomain scp_subdomain_mt6989_spm[] = {
	{MT6989_POWER_DOMAIN_ADSP_AO, MT6989_POWER_DOMAIN_ADSP_TOP_DORMANT},
	{MT6989_POWER_DOMAIN_ISP_MAIN, MT6989_POWER_DOMAIN_ISP_TRAW},
	{MT6989_POWER_DOMAIN_ISP_MAIN, MT6989_POWER_DOMAIN_ISP_DIP1},
	{MT6989_POWER_DOMAIN_ISP_VCORE, MT6989_POWER_DOMAIN_ISP_MAIN},
	{MT6989_POWER_DOMAIN_MM_INFRA, MT6989_POWER_DOMAIN_ISP_VCORE},
	{MT6989_POWER_DOMAIN_VDE_VCORE0, MT6989_POWER_DOMAIN_VDE0},
	{MT6989_POWER_DOMAIN_VDE_VCORE1, MT6989_POWER_DOMAIN_VDE1},
	{MT6989_POWER_DOMAIN_MM_INFRA, MT6989_POWER_DOMAIN_VDE_VCORE0},
	{MT6989_POWER_DOMAIN_VDE0, MT6989_POWER_DOMAIN_VDE_VCORE1},
	{MT6989_POWER_DOMAIN_MM_INFRA, MT6989_POWER_DOMAIN_VEN0},
	{MT6989_POWER_DOMAIN_VEN0, MT6989_POWER_DOMAIN_VEN1},
	{MT6989_POWER_DOMAIN_VEN1, MT6989_POWER_DOMAIN_VEN2},
	{MT6989_POWER_DOMAIN_CAM_MAIN, MT6989_POWER_DOMAIN_CAM_MRAW},
	{MT6989_POWER_DOMAIN_CAM_MAIN, MT6989_POWER_DOMAIN_CAM_SUBA},
	{MT6989_POWER_DOMAIN_CAM_MAIN, MT6989_POWER_DOMAIN_CAM_SUBB},
	{MT6989_POWER_DOMAIN_CAM_MAIN, MT6989_POWER_DOMAIN_CAM_SUBC},
	{MT6989_POWER_DOMAIN_CAM_VCORE, MT6989_POWER_DOMAIN_CAM_MAIN},
	{MT6989_POWER_DOMAIN_MM_INFRA, MT6989_POWER_DOMAIN_CAM_VCORE},
	{MT6989_POWER_DOMAIN_CAM_VCORE, MT6989_POWER_DOMAIN_CAM_CCU},
	{MT6989_POWER_DOMAIN_CAM_CCU, MT6989_POWER_DOMAIN_CAM_CCU_AO},
	{MT6989_POWER_DOMAIN_MM_INFRA, MT6989_POWER_DOMAIN_DISP_VCORE},
	{MT6989_POWER_DOMAIN_DIS1_SHUTDOWN, MT6989_POWER_DOMAIN_MML0_SHUTDOWN},
	{MT6989_POWER_DOMAIN_DIS1_SHUTDOWN, MT6989_POWER_DOMAIN_MML1_SHUTDOWN},
	{MT6989_POWER_DOMAIN_DIS1_SHUTDOWN, MT6989_POWER_DOMAIN_DIS0_SHUTDOWN},
	{MT6989_POWER_DOMAIN_DISP_VCORE, MT6989_POWER_DOMAIN_DIS1_SHUTDOWN},
	{MT6989_POWER_DOMAIN_DIS1_SHUTDOWN, MT6989_POWER_DOMAIN_OVL0_SHUTDOWN},
	{MT6989_POWER_DOMAIN_DIS1_SHUTDOWN, MT6989_POWER_DOMAIN_OVL1_SHUTDOWN},
	{MT6989_POWER_DOMAIN_DIS1_SHUTDOWN, MT6989_POWER_DOMAIN_DP_TX},
	{MT6989_POWER_DOMAIN_MM_INFRA, MT6989_POWER_DOMAIN_CSI_RX},
};

static const struct scp_subdomain scp_subdomain_mt6989_hfrp[] = {
	{MT6989_POWER_DOMAIN_ADSP_AO, MT6989_POWER_DOMAIN_ADSP_TOP_DORMANT},
	{MT6989_POWER_DOMAIN_ISP_MAIN, MT6989_POWER_DOMAIN_ISP_TRAW},
	{MT6989_POWER_DOMAIN_ISP_MAIN, MT6989_POWER_DOMAIN_ISP_DIP1},
	{MT6989_POWER_DOMAIN_ISP_VCORE, MT6989_POWER_DOMAIN_ISP_MAIN},
	{MT6989_POWER_DOMAIN_MM_INFRA, MT6989_POWER_DOMAIN_ISP_VCORE},
	{MT6989_POWER_DOMAIN_VDE_VCORE0, MT6989_POWER_DOMAIN_VDE0},
	{MT6989_POWER_DOMAIN_VDE_VCORE1, MT6989_POWER_DOMAIN_VDE1},
	{MT6989_POWER_DOMAIN_MM_INFRA, MT6989_POWER_DOMAIN_VDE_VCORE0},
	{MT6989_POWER_DOMAIN_VDE0, MT6989_POWER_DOMAIN_VDE_VCORE1},
	{MT6989_POWER_DOMAIN_MM_INFRA, MT6989_POWER_DOMAIN_VEN0},
	{MT6989_POWER_DOMAIN_VEN0, MT6989_POWER_DOMAIN_VEN1},
	{MT6989_POWER_DOMAIN_VEN1, MT6989_POWER_DOMAIN_VEN2},
	{MT6989_POWER_DOMAIN_CAM_MAIN, MT6989_POWER_DOMAIN_CAM_MRAW},
	{MT6989_POWER_DOMAIN_CAM_MAIN, MT6989_POWER_DOMAIN_CAM_SUBA},
	{MT6989_POWER_DOMAIN_CAM_MAIN, MT6989_POWER_DOMAIN_CAM_SUBB},
	{MT6989_POWER_DOMAIN_CAM_MAIN, MT6989_POWER_DOMAIN_CAM_SUBC},
	{MT6989_POWER_DOMAIN_CAM_VCORE, MT6989_POWER_DOMAIN_CAM_MAIN},
	{MT6989_POWER_DOMAIN_MM_INFRA, MT6989_POWER_DOMAIN_CAM_VCORE},
	{MT6989_POWER_DOMAIN_CAM_VCORE, MT6989_POWER_DOMAIN_CAM_CCU},
	{MT6989_POWER_DOMAIN_CAM_CCU, MT6989_POWER_DOMAIN_CAM_CCU_AO},
	{MT6989_POWER_DOMAIN_DIS1_SHUTDOWN, MT6989_POWER_DOMAIN_MML0_SHUTDOWN},
	{MT6989_POWER_DOMAIN_DIS1_SHUTDOWN, MT6989_POWER_DOMAIN_MML1_SHUTDOWN},
	{MT6989_POWER_DOMAIN_DIS1_SHUTDOWN, MT6989_POWER_DOMAIN_DIS0_SHUTDOWN},
	{MT6989_POWER_DOMAIN_DISP_VCORE, MT6989_POWER_DOMAIN_DIS1_SHUTDOWN},
	{MT6989_POWER_DOMAIN_DIS1_SHUTDOWN, MT6989_POWER_DOMAIN_OVL0_SHUTDOWN},
	{MT6989_POWER_DOMAIN_DIS1_SHUTDOWN, MT6989_POWER_DOMAIN_OVL1_SHUTDOWN},
	{MT6989_POWER_DOMAIN_DIS1_SHUTDOWN, MT6989_POWER_DOMAIN_DP_TX},
	{MT6989_POWER_DOMAIN_MM_INFRA, MT6989_POWER_DOMAIN_CSI_RX},
};

static const struct scp_soc_data mt6989_spm_data = {
	.domains = scp_domain_mt6989_hwv_data,
	.num_domains = MT6989_SPM_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6989_spm,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6989_spm),
	.regs = {
		.pwr_sta_offs = 0xF70,
		.pwr_sta2nd_offs = 0xF74,
	}
};

static const struct scp_soc_data mt6989_hwv_data = {
	.domains = scp_domain_mt6989_hwv_data,
	.num_domains = MT6989_SPM_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6989_hfrp,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6989_hfrp),
	.regs = {
		.pwr_sta_offs = 0xF70,
		.pwr_sta2nd_offs = 0xF74,
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6989-scpsys",
		.data = &mt6989_spm_data,
	}, {
		.compatible = "mediatek,mt6989-scpsys-hwv",
		.data = &mt6989_hwv_data,
	}, {
		/* sentinel */
	}
};

static int mt6989_scpsys_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	int i, ret;

	soc = of_device_get_match_data(&pdev->dev);
	if (!soc)
		return -EINVAL;

	scp = init_scp(pdev, soc->domains, soc->num_domains, &soc->regs, bus_list, BUS_TYPE_NUM);
	if (IS_ERR(scp))
		return PTR_ERR(scp);

	ret = mtk_register_power_domains(pdev, scp, soc->num_domains);
	if (ret)
		return ret;

	pd_data = &scp->pd_data;

	for (i = 0, sd = soc->subdomains; i < soc->num_subdomains; i++, sd++) {
		ret = pm_genpd_add_subdomain(pd_data->domains[sd->origin],
					     pd_data->domains[sd->subdomain]);
		if (ret && IS_ENABLED(CONFIG_PM)) {
			dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
				ret);
			return ret;
		}
	}

	return 0;
}

static struct platform_driver mt6989_scpsys_drv = {
	.probe = mt6989_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6989",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

module_platform_driver(mt6989_scpsys_drv);
MODULE_LICENSE("GPL");
