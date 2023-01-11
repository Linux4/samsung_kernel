/*
 *  linux/arch/arm/mach-mmp/regdump-mmp.c
 *
 *  Copyright (C) 2014 Marvell Technology Group Ltd.
 *  Author: Neil Zhang<zhangwm@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/regdump_ops.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/cputype.h>

#include "regs-addr.h"

static struct regdump_ops pmua_regdump_ops = {
	.dev_name = "pxa1x88-common-pmua",
};

static struct regdump_region pmua_dump_region[] = {
	{"PMUA_CC_CP",			0x000, 4, regdump_cond_true},
	{"PMUA_CC_AP",			0x004, 4, regdump_cond_true},
	{"PMUA_DM_CC_CP",		0x008, 4, regdump_cond_true},
	{"PMUA_DM_CC_AP",		0x00c, 4, regdump_cond_true},
	{"PMUA_FC_TIMER",		0x010, 4, regdump_cond_true},
	{"PMUA_CP_IDLE_CFG",		0x014, 4, regdump_cond_true},
	{"PMUA_AP_IDLE_CFG",		0x018, 4, regdump_cond_true},
	{"PMUA_SQU_CLK_GATE_CTRL",	0x01c, 4, regdump_cond_true},
	{"PMUA_CCIC_CLK_GATE_CTRL",	0x028, 4, regdump_cond_true},
	{"PMUA_FBRC0_CLK_GATE_CTRL",	0x02c, 4, regdump_cond_true},
	{"PMUA_FBRC1_CLK_GATE_CTRL",	0x030, 4, regdump_cond_true},
	{"PMUA_USB_CLK_GATE_CTRL",	0x034, 4, regdump_cond_true},
	{"PMUA_ISP_CLK_RES_CTRL",	0x038, 4, regdump_cond_true},
	{"PMUA_PMU_CLK_GATE_CTRL",	0x040, 4, regdump_cond_true},
	{"PMUA_DSI_CLK_RES_CTRL",	0x044, 4, regdump_cond_true},
	{"PMUA_LCD_DSI_CLK_RES_CTRL",	0x04c, 4, regdump_cond_true},
	{"PMUA_CCIC_CLK_RES_CTRL",	0x050, 4, regdump_cond_true},
	{"PMUA_SDH0_CLK_RES_CTRL",	0x054, 4, regdump_cond_true},
	{"PMUA_SDH1_CLK_RES_CTRL",	0x058, 4, regdump_cond_true},
	{"PMUA_USB_CLK_RES_CTRL",	0x05c, 4, regdump_cond_true},
	{"PMUA_NF_CLK_RES_CTRL",	0x060, 4, regdump_cond_true},
	{"PMUA_DMA_CLK_RES_CTRL",	0x064, 4, regdump_cond_true},
	{"PMUA_AES_CLK_RES_CTRL",	0x068, 4, regdump_cond_true},
	{"PMUA_MCB_CLK_RES_CTRL",	0x06c, 4, regdump_cond_true},
	{"PMUA_CP_IMR",			0x070, 4, regdump_cond_true},
	{"PMUA_CP_IRWC",		0x074, 4, regdump_cond_true},
	{"PMUA_CP_ISR",			0x078, 4, regdump_cond_true},
	{"PMUA_SD_ROT_WAKE_CLR",	0x07c, 4, regdump_cond_true},
	{"PMUA_PWR_STBL_TIMER",		0x084, 4, regdump_cond_true},
	{"PMUA_DEBUG_REG",		0x088, 4, regdump_cond_true},
	{"PMUA_SRAM_PWR_DWN",		0x08c, 4, regdump_cond_true},
	{"PMUA_CORE_STATUS",		0x090, 4, regdump_cond_true},
	{"PMUA_RES_FRM_SLP_CLR",	0x094, 4, regdump_cond_true},
	{"PMUA_AP_IMR",			0x098, 4, regdump_cond_true},
	{"PMUA_AP_IRWC",		0x09c, 4, regdump_cond_true},
	{"PMUA_AP_ISR",			0x0a0, 4, regdump_cond_true},
	{"PMUA_VPU_CLK_RES_CTRL",	0x0a4, 4, regdump_cond_true},
	{"PMUA_DTC_CLK_RES_CTRL",	0x0ac, 4, regdump_cond_true},
	{"PMUA_MC_HW_SLP_TYPE",		0x0b0, 4, regdump_cond_true},
	{"PMUA_MC_SLP_REQ_AP",		0x0b4, 4, regdump_cond_true},
	{"PMUA_MC_SLP_REQ_CP",		0x0b8, 4, regdump_cond_true},
	{"PMUA_MC_SLP_REQ_MSA",		0x0bc, 4, regdump_cond_true},
	{"PMUA_MC_SW_SLP_TYPE",		0x0c0, 4, regdump_cond_true},
	{"PMUA_PLL_SEL_STATUS",		0x0c4, 4, regdump_cond_true},
	{"PMUA_SYNC_MODE_BYPASS",	0x0c8, 4, regdump_cond_true},
	{"PMUA_GPU_3D_CLK_RES_CTRL",	0x0cc, 4, regdump_cond_true},
	{"PMUA_SMC_CLK_RES_CTRL",	0x0d4, 4, regdump_cond_true},
	{"PMUA_PWR_CTRL_REG",		0x0d8, 4, regdump_cond_true},
	{"PMUA_PWR_BLK_TMR_REG",	0x0dc, 4, regdump_cond_true},
	{"PMUA_SDH2_CLK_RES_CTRL",	0x0e0, 4, regdump_cond_true},
	{"PMUA_CA7MP_IDLE_CFG1",	0x0e4, 4, regdump_cond_true},
	{"PMUA_MC_CTRL",		0x0e8, 4, regdump_cond_true},
	{"PMUA_PWR_STATUS_REG",		0x0f0, 4, regdump_cond_true},
	{"PMUA_GPU_2D_CLK_RES_CTRL",	0x0f4, 4, regdump_cond_true},
	{"PMUA_CC2_AP",			0x100, 4, regdump_cond_true},
	{"PMUA_DM_CC2_AP",		0x104, 4, regdump_cond_true},
	{"PMUA_TRACE_CONFIG",		0x108, 4, regdump_cond_true},
	{"PMUA_CA7MP_IDLE_CFG0",	0x120, 4, regdump_cond_true},
	{"PMUA_CA7_CORE0_IDLE_CFG",	0x124, 4, regdump_cond_true},
	{"PMUA_CA7_CORE1_IDLE_CFG",	0x128, 4, regdump_cond_true},
	{"PMUA_CA7_CORE0_WAKEUP",	0x12c, 4, regdump_cond_true},
	{"PMUA_CA7_CORE1_WAKEUP",	0x130, 4, regdump_cond_true},
	{"PMUA_CA7_CORE2_WAKEUP",	0x134, 4, regdump_cond_true},
	{"PMUA_CA7_CORE3_WAKEUP",	0x138, 4, regdump_cond_true},
	{"PMUA_DVC_DEBUG",		0x140, 4, regdump_cond_true},
	{"PMUA_CA7MP_IDLE_CFG2",	0x150, 4, regdump_cond_true},
	{"PMUA_CA7MP_IDLE_CFG3",	0x154, 4, regdump_cond_true},
	{"PMUA_CA7_CORE2_IDLE_CFG",	0x160, 4, regdump_cond_true},
	{"PMUA_CA7_CORE3_IDLE_CFG",	0x164, 4, regdump_cond_true},
	{"PMUA_CA7_PWR_MISC",		0x170, 4, regdump_cond_true},
};

static struct regdump_ops pmua_regdump_ops_1L88 = {
	.dev_name = "pxa1L88-pmua",
};

static struct regdump_region pmua_dump_region_1L88[] = {
	{"PMUA_CCIC2_CLK_RES_CTRL",	0x024, 4, regdump_cond_true},
	{"PMUA_LTEDMA_CLK_RES_CTRL",	0x048, 4, regdump_cond_true},
	{"DFC_AP",			0x180, 4, regdump_cond_true},
	{"DFC_CP",			0x184, 4, regdump_cond_true},
	{"DFC_STATUS",			0x188, 4, regdump_cond_true},
	{"DFC_LEVEL0",			0x190, 4, regdump_cond_true},
	{"DFC_LEVEL1",			0x194, 4, regdump_cond_true},
	{"DFC_LEVEL2",			0x198, 4, regdump_cond_true},
	{"DFC_LEVEL3",			0x19c, 4, regdump_cond_true},
	{"DFC_LEVEL4",			0x1a0, 4, regdump_cond_true},
	{"DFC_LEVEL5",			0x1a4, 4, regdump_cond_true},
	{"DFC_LEVEL6",			0x1a8, 4, regdump_cond_true},
	{"DFC_LEVEL7",			0x1ac, 4, regdump_cond_true},
};

static void __init mmp_pmua_regdump_init(void)
{
	pmua_regdump_ops.base = regs_addr_get_va(REGS_ADDR_APMU);
	pmua_regdump_ops.phy_base = regs_addr_get_pa(REGS_ADDR_APMU);
	pmua_regdump_ops.regions = pmua_dump_region;
	pmua_regdump_ops.reg_nums = ARRAY_SIZE(pmua_dump_region);
	register_regdump_ops(&pmua_regdump_ops);
}

static void __init mmp_pmua_regdump_1L88_init(void)
{
	pmua_regdump_ops_1L88.base = regs_addr_get_va(REGS_ADDR_APMU);
	pmua_regdump_ops_1L88.phy_base = regs_addr_get_pa(REGS_ADDR_APMU);
	pmua_regdump_ops_1L88.regions = pmua_dump_region_1L88;
	pmua_regdump_ops_1L88.reg_nums = ARRAY_SIZE(pmua_dump_region_1L88);
	register_regdump_ops(&pmua_regdump_ops_1L88);
}

static struct regdump_ops gic_regdump_ops = {
	.dev_name = "pxa1x88-gic",
};

static struct regdump_region gic_dump_region[] = {
	{"GIC_GICD_CTLR",		0x000, 4, regdump_cond_true},
	{"GIC_GICD_TYPER",		0x004, 4, regdump_cond_true},
	{"GIC_GICD_IIDR",		0x008, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER0",		0x100, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER1",		0x104, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER2",		0x108, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER3",		0x10c, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER4",		0x110, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER5",		0x114, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER6",		0x118, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER7",		0x11c, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER8",		0x120, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER9",		0x124, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER10",	0x128, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER11",	0x12c, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER12",	0x130, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER13",	0x134, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER14",	0x138, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER15",	0x13c, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR0",		0x200, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR1",		0x204, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR2",		0x208, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR3",		0x20c, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR4",		0x210, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR5",		0x214, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR6",		0x218, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR7",		0x21c, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR8",		0x220, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR9",		0x224, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR10",		0x228, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR11",		0x22c, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR12",		0x230, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR13",		0x234, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR14",		0x238, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR15",		0x23c, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER0",		0x300, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER1",		0x304, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER2",		0x308, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER3",		0x30c, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER4",		0x310, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER5",		0x314, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER6",		0x318, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER7",		0x31c, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER8",		0x320, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER9",		0x324, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER10",	0x328, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER11",	0x32c, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER12",	0x330, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER13",	0x334, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER14",	0x338, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER15",	0x33c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR0",		0xc00, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR1",		0xc04, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR2",		0xc08, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR3",		0xc0c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR4",		0xc10, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR5",		0xc14, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR6",		0xc18, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR7",		0xc1c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR8",		0xc20, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR9",		0xc24, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR10",		0xc28, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR11",		0xc2c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR12",		0xc30, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR13",		0xc34, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR14",		0xc38, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR15",		0xc3c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR16",		0xc40, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR17",		0xc44, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR18",		0xc48, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR19",		0xc4c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR20",		0xc50, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR21",		0xc54, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR22",		0xc58, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR23",		0xc5c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR24",		0xc60, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR25",		0xc64, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR26",		0xc68, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR27",		0xc6c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR28",		0xc70, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR29",		0xc74, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR30",		0xc78, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR31",		0xc7c, 4, regdump_cond_true},
};

static void __init mmp_gic_regdump_init(void)
{
	gic_regdump_ops.base = gic_get_dist_base();
	gic_regdump_ops.phy_base = gic_dist_base_phys();
	gic_regdump_ops.regions = gic_dump_region;
	gic_regdump_ops.reg_nums = ARRAY_SIZE(gic_dump_region);
	register_regdump_ops(&gic_regdump_ops);
}


static int __init mmp_regdump_init(void)
{
	mmp_gic_regdump_init();

	if (cpu_is_pxa1L88()) {
		mmp_pmua_regdump_init();
		mmp_pmua_regdump_1L88_init();
	}
	return 0;
}
arch_initcall(mmp_regdump_init);
