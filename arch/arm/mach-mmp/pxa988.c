/*
 * linux/arch/arm/mach-mmp/pxa988.c
 *
 * code name PXA988
 *
 * Copyright (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_data/mmp_audio.h>
#include <linux/notifier.h>
#include <linux/memblock.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/ion.h>
#include <linux/dma-mapping.h>
#include <linux/persistent_ram.h>

#include <asm/smp_twd.h>
#include <asm/mach/time.h>
#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/cacheflush.h>

#include <mach/addr-map.h>
#include <mach/regs-ciu.h>
#include <mach/regs-apbc.h>
#include <mach/regs-apmu.h>
#include <mach/regs-mpmu.h>
#include <mach/cputype.h>
#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/gpio-edge.h>
#include <mach/dma.h>
#include <mach/devices.h>
#include <mach/pxa988.h>
#include <mach/regs-timers.h>
#include <mach/regs-usb.h>
#include <mach/soc_coda7542.h>
#include <mach/isp_dev.h>
#include <mach/uio_isp.h>
#include <mach/reset-pxa988.h>
#include <mach/pxa168fb.h>
#include <plat/mfp.h>
#include <mach/gpu_mem.h>

#ifdef CONFIG_ARM_ARCH_TIMER
#include <asm/arch_timer.h>
#endif /* #ifdef CONFIG_ARM_ARCH_TIMER */

#include "common.h"

#include <linux/regdump_ops.h>

#define MFPR_VIRT_BASE	(APB_VIRT_BASE + 0x1e000)
#define RIPC3_VIRT_BASE	(APB_VIRT_BASE + 0x3D000)
#define GPIOE_VIRT_BASE	(APB_VIRT_BASE + 0x19800)
#define RIPC3_STATUS	(RIPC3_VIRT_BASE + 0x300)
#define APMU_PHY_BASE  0xd4282800
#define GIC_DIST_PHYS_BASE  (PERI_PHYS_BASE + 0x1000)

#ifdef CONFIG_MACH_WILCOX_CMCC
#define BOARD_ID_CMCC_REV05 (0x7)
#endif

static struct mfp_addr_map pxa988_addr_map[] __initdata = {

	MFP_ADDR_X(GPIO0, GPIO54, 0xdc),
	MFP_ADDR_X(GPIO67, GPIO98, 0x1b8),
	MFP_ADDR_X(GPIO100, GPIO109, 0x238),
	MFP_ADDR_X(GPIO110, GPIO116, 0x298),

	MFP_ADDR(DF_IO0, 0x40),
	MFP_ADDR(DF_IO1, 0x3c),
	MFP_ADDR(DF_IO2, 0x38),
	MFP_ADDR(DF_IO3, 0x34),
	MFP_ADDR(DF_IO4, 0x30),
	MFP_ADDR(DF_IO5, 0x2c),
	MFP_ADDR(DF_IO6, 0x28),
	MFP_ADDR(DF_IO7, 0x24),
	MFP_ADDR(DF_IO8, 0x20),
	MFP_ADDR(DF_IO9, 0x1c),
	MFP_ADDR(DF_IO10, 0x18),
	MFP_ADDR(DF_IO11, 0x14),
	MFP_ADDR(DF_IO12, 0x10),
	MFP_ADDR(DF_IO13, 0xc),
	MFP_ADDR(DF_IO14, 0x8),
	MFP_ADDR(DF_IO15, 0x4),

	MFP_ADDR(DF_nCS0_SM_nCS2, 0x44),
	MFP_ADDR(DF_nCS1_SM_nCS3, 0x48),
	MFP_ADDR(SM_nCS0, 0x4c),
	MFP_ADDR(SM_nCS1, 0x50),
	MFP_ADDR(DF_WEn, 0x54),
	MFP_ADDR(DF_REn, 0x58),
	MFP_ADDR(DF_CLE_SM_OEn, 0x5c),
	MFP_ADDR(DF_ALE_SM_WEn, 0x60),
	MFP_ADDR(SM_SCLK, 0x64),
	MFP_ADDR(DF_RDY0, 0x68),
	MFP_ADDR(SM_BE0, 0x6c),
	MFP_ADDR(SM_BE1, 0x70),
	MFP_ADDR(SM_ADV, 0x74),
	MFP_ADDR(DF_RDY1, 0x78),
	MFP_ADDR(SM_ADVMUX, 0x7c),
	MFP_ADDR(SM_RDY, 0x80),
	MFP_ADDR(ANT_SW4, 0x26c),

	MFP_ADDR_X(MMC1_DAT7, MMC1_WP, 0x84),

	MFP_ADDR(GPIO124, 0xd0),
	MFP_ADDR(VCXO_REQ, 0xd4),       
	MFP_ADDR(VCXO_OUT, 0xd8),

	MFP_ADDR(CLK_REQ, 0xcc),
#ifdef CONFIG_SEC_GPIO_DVS
	MFP_ADDR(PRI_TDI, 0xB4),
	MFP_ADDR(PRI_TMS, 0xB8),
	MFP_ADDR(PRI_TCK, 0xBC),
	MFP_ADDR(PRI_TDO, 0xC0),
	MFP_ADDR(SLAVE_RESET_OUT, 0xC8),
#endif
	MFP_ADDR_END,
};

#ifdef CONFIG_REGDUMP
static struct regdump_ops pmua_regdump_ops = {
	.dev_name = "PXA1088-PMUA",
};

static struct regdump_region pmua_dump_region[] = {
	{"PMUA_CC_CP",			0x000, 4, regdump_cond_true},
	{"PMUA_CC_AP",			0x004, 4, regdump_cond_true},
	{"PMUA_DM_CC_CP",		0x008, 4, regdump_cond_true},
	{"PMUA_DM_CC_AP",		0x00c, 4, regdump_cond_true},
	{"PMUA_FC_TIMER",		0x010, 4, regdump_cond_true},
	{"PMUA_CP_IDLE_CFG",		0x014, 4, regdump_cond_true},
	{"PMUA_AP_IDLE_CFG",		0x018, 4, regdump_cond_true},
	{"PMUA_SQU_CLK_GATE_CTRL",		0x01c, 4, regdump_cond_true},
#ifdef CONFIG_CPU_PXA1088
	{"PMUA_IRE_CLK_GATE_CTRL",		0x020, 4, regdump_cond_true},
#endif
	{"PMUA_CCIC_CLK_GATE_CTRL",	0x028, 4, regdump_cond_true},
	{"PMUA_FBRC0_CLK_GATE_CTRL",	0x02c, 4, regdump_cond_true},
	{"PMUA_FBRC1_CLK_GATE_CTRL",	0x030, 4, regdump_cond_true},
	{"PMUA_USB_CLK_GATE_CTRL",	0x034, 4, regdump_cond_true},
	{"PMUA_ISP_CLK_RES_CTRL",	0x038, 4, regdump_cond_true},
	{"PMUA_PMU_CLK_GATE_CTRL",	0x040, 4, regdump_cond_true},
	{"PMUA_DSI_CLK_RES_CTRL",	0x044, 4, regdump_cond_true},
#ifdef CONFIG_CPU_PXA1088
	{"PMUA_HSI_CLK_RES_CTRL",	0x048, 4, regdump_cond_true},
#endif
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
	{"PMUA_CP_IRWC",			0x074, 4, regdump_cond_true},
	{"PMUA_CP_ISR",			0x078, 4, regdump_cond_true},
	{"PMUA_SD_ROT_WAKE_CLR",		0x07c, 4, regdump_cond_true},
#ifdef CONFIG_CPU_PXA1088
	{"PMUA_FBRC_CLK",		0x080, 4, regdump_cond_true},
#endif
	{"PMUA_PWR_STBL_TIMER",		0x084, 4, regdump_cond_true},
	{"PMUA_DEBUG_REG",		0x088, 4, regdump_cond_true},
	{"PMUA_SRAM_PWR_DWN",		0x08c, 4, regdump_cond_true},
	{"PMUA_CORE_STATUS",		0x090, 4, regdump_cond_true},
	{"PMUA_RES_FRM_SLP_CLR",	0x094, 4, regdump_cond_true},
	{"PMUA_AP_IMR",			0x098, 4, regdump_cond_true},
	{"PMUA_AP_IRWC",		0x09c, 4, regdump_cond_true},
	{"PMUA_AP_ISR",			0x0a0, 4, regdump_cond_true},
	{"PMUA_VPU_CLK_RES_CTRL",	0x0a4, 4, regdump_cond_true},
#ifdef CONFIG_CPU_PXA1088
	{"PMUA_VPRO_PWRDWN",	0x0a8, 4, regdump_cond_true},
#endif
	{"PMUA_DTC_CLK_RES_CTRL",	0x0ac, 4, regdump_cond_true},
	{"PMUA_MC_HW_SLP_TYPE",		0x0b0, 4, regdump_cond_true},
	{"PMUA_MC_SLP_REQ_AP",		0x0b4, 4, regdump_cond_true},
	{"PMUA_MC_SLP_REQ_CP",		0x0b8, 4, regdump_cond_true},
	{"PMUA_MC_SLP_REQ_MSA",		0x0bc, 4, regdump_cond_true},
	{"PMUA_MC_SW_SLP_TYPE",		0x0c0, 4, regdump_cond_true},
	{"PMUA_PLL_SEL_STATUS",		0x0c4, 4, regdump_cond_true},
	{"PMUA_SYNC_MODE_BYPASS",	0x0c8, 4, regdump_cond_true},
	{"PMUA_GPU_3D_CLK_RES_CTRL",	0x0cc, 4, regdump_cond_true},
#ifdef CONFIG_CPU_PXA1088
	{"PMUA_GPU_3D_PWRDWN",	0x0d0, 4, regdump_cond_true},
#endif
	{"PMUA_SMC_CLK_RES_CTRL",	0x0d4, 4, regdump_cond_true},
	{"PMUA_PWR_CTRL_REG",	0x0d8, 4, regdump_cond_true},
	{"PMUA_PWR_BLK_TMR_REG",		0x0dc, 4, regdump_cond_true},
	{"PMUA_SDH2_CLK_RES_CTRL",	0x0e0, 4, regdump_cond_true},
	{"PMUA_CA7MP_IDLE_CFG1",		0x0e4, 4, regdump_cond_true},
	{"PMUA_MC_CTRL",	0x0e8, 4, regdump_cond_true},
	{"PMUA_PWR_STATUS_REG",	0x0f0, 4, regdump_cond_true},
	{"PMUA_GPU_2D_CLK_RES_CTRL",	0x0f4, 4, regdump_cond_true},
	{"PMUA_CC2_AP",	0x100, 4, regdump_cond_true},
	{"PMUA_DM_CC2_AP",	0x104, 4, regdump_cond_true},
	{"PMUA_TRACE_CONFIG",	0x108, 4, regdump_cond_true},
	{"PMUA_CA7MP_IDLE_CFG0",		0x120, 4, regdump_cond_true},
	{"PMUA_CA7_CORE0_IDLE_CFG",		0x124, 4, regdump_cond_true},
	{"PMUA_CA7_CORE1_IDLE_CFG",		0x128, 4, regdump_cond_true},
	{"PMUA_CA7_CORE0_WAKEUP",		0x12c, 4, regdump_cond_true},
	{"PMUA_CA7_CORE1_WAKEUP",		0x130, 4, regdump_cond_true},
	{"PMUA_CA7_CORE2_WAKEUP",		0x134, 4, regdump_cond_true},
	{"PMUA_CA7_CORE3_WAKEUP",		0x138, 4, regdump_cond_true},
	{"PMUA_DVC_DEBUG",		0x140, 4, regdump_cond_true},
	{"PMUA_CA7MP_IDLE_CFG2",		0x150, 4, regdump_cond_true},
	{"PMUA_CA7MP_IDLE_CFG3",		0x154, 4, regdump_cond_true},
	{"PMUA_CA7_CORE2_IDLE_CFG",		0x160, 4, regdump_cond_true},
	{"PMUA_CA7_CORE3_IDLE_CFG",		0x164, 4, regdump_cond_true},
	{"PMUA_CA7_PWR_MISC",		0x170, 4, regdump_cond_true},
};

static void __init pxa_init_pmua_regdump(void)
{
	pmua_regdump_ops.base = (unsigned long)(APMU_VIRT_BASE);
	pmua_regdump_ops.phy_base = (unsigned long)(APMU_PHY_BASE);
	pmua_regdump_ops.regions = pmua_dump_region;
	pmua_regdump_ops.reg_nums = ARRAY_SIZE(pmua_dump_region);
	register_regdump_ops(&pmua_regdump_ops);
}

static struct regdump_ops gic_regdump_ops = {
	.dev_name = "PXA1088-gic",
};

static struct regdump_region gic_dump_region[] = {
	{"GIC_GICD_CTLR",			0x000, 4, regdump_cond_true},
	{"GIC_GICD_TYPER",			0x004, 4, regdump_cond_true},
	{"GIC_GICD_IIDR",			0x008, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER0",			0x100, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER1",			0x104, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER2",			0x108, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER3",			0x10c, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER4",			0x110, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER5",			0x114, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER6",			0x118, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER7",			0x11c, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER8",			0x120, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER9",			0x124, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER10",		0x128, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER11",		0x12c, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER12",		0x130, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER13",		0x134, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER14",		0x138, 4, regdump_cond_true},
	{"GIC_GICD_ISENABLER15",		0x13c, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR0",			0x200, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR1",			0x204, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR2",			0x208, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR3",			0x20c, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR4",			0x210, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR5",			0x214, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR6",			0x218, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR7",			0x21c, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR8",			0x220, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR9",			0x224, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR10",			0x228, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR11",			0x22c, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR12",			0x230, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR13",			0x234, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR14",			0x238, 4, regdump_cond_true},
	{"GIC_GICD_ISPENDR15",			0x23c, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER0",			0x300, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER1",			0x304, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER2",			0x308, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER3",			0x30c, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER4",			0x310, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER5",			0x314, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER6",			0x318, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER7",			0x31c, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER8",			0x320, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER9",			0x324, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER10",		0x328, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER11",		0x32c, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER12",		0x330, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER13",		0x334, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER14",		0x338, 4, regdump_cond_true},
	{"GIC_GICD_ISACTIVER15",		0x33c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR0",			0xc00, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR1",			0xc04, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR2",			0xc08, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR3",			0xc0c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR4",			0xc10, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR5",			0xc14, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR6",			0xc18, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR7",			0xc1c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR8",			0xc20, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR9",			0xc24, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR10",			0xc28, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR11",			0xc2c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR12",			0xc30, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR13",			0xc34, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR14",			0xc38, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR15",			0xc3c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR16",			0xc40, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR17",			0xc44, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR18",			0xc48, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR19",			0xc4c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR20",			0xc50, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR21",			0xc54, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR22",			0xc58, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR23",			0xc5c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR24",			0xc60, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR25",			0xc64, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR26",			0xc68, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR27",			0xc6c, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR28",			0xc70, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR29",			0xc74, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR30",			0xc78, 4, regdump_cond_true},
	{"GIC_GICD_ICFGR31",			0xc7c, 4, regdump_cond_true},
};

static void __init pxa_init_gic_regdump(void)
{
	gic_regdump_ops.base = (void __iomem *)GIC_DIST_VIRT_BASE;
	gic_regdump_ops.phy_base = GIC_DIST_PHYS_BASE;
	gic_regdump_ops.regions = gic_dump_region;
	gic_regdump_ops.reg_nums = ARRAY_SIZE(gic_dump_region);
	register_regdump_ops(&gic_regdump_ops);
}

#else
static inline void  __init pxa_init_pmua_regdump(void) {}
static inline void __init pxa_init_gic_regdump(void) {}
#endif

/*
 * gc, vpu, isp will access the same regsiter to pwr on/off,
 * add spinlock to protect the sequence
 */
static DEFINE_SPINLOCK(gc_vpu_isp_pwr_lock);

/* used to protect GC power sequence */
static DEFINE_SPINLOCK(gc_pwr_lock);

/* used to protect ripc status */
DEFINE_SPINLOCK(ripc_lock);
EXPORT_SYMBOL(ripc_lock);
static int ripc_status;

/* GC power control */
#define GC_USE_HW_PWRCTRL	1
#define GC_AUTO_PWR_ON		(0x1 << 0)

#define GC_CLK_EN	\
	((0x1 << 3) | (0x1 << 4) | (0x1 << 5))

#define GC_ACLK_RST	(0x1 << 0)
#define GC_FCLK_RST	(0x1 << 1)
#define GC_HCLK_RST	(0x1 << 2)
#define GC_CLK_RST	\
	(GC_ACLK_RST | GC_FCLK_RST | GC_HCLK_RST)

#define GC_ISOB		(0x1 << 8)
#define GC_PWRON1	(0x1 << 9)
#define GC_PWRON2	(0x1 << 10)
#define GC_HWMODE	(0x1 << 11)

#define GC_FCLK_SEL_MASK	(0x3 << 6)
#define GC_ACLK_SEL_MASK	(0x3 << 20)
#define GC_FCLK_DIV_MASK	(0x7 << 12)
#define GC_ACLK_DIV_MASK	(0x7 << 17)
#define GC_FCLK_REQ		(0x1 << 15)
#define GC_ACLK_REQ		(0x1 << 16)

#define GC_CLK_SEL_WIDTH	(2)
#define GC_CLK_DIV_WIDTH	(3)
#define GC_FCLK_SEL_SHIFT	(6)
#define GC_ACLK_SEL_SHIFT	(20)
#define GC_FCLK_DIV_SHIFT	(12)
#define GC_ACLK_DIV_SHIFT	(17)

#define GC_REG_WRITE(val)	{	\
	__raw_writel(val, APMU_GC);	\
}

#define GC_2D_REG_WRITE(val)	{	\
	_raw_writel(val, APMU_GC_2D);	\
}

void gc_pwr(int power_on)
{
	unsigned int val = __raw_readl(APMU_GC);
	int timeout = 5000;
#ifdef	CONFIG_CPU_PXA1088
	unsigned int __attribute__ ((unused)) val_2d = __raw_readl(APMU_GC_2D);
#endif

	static struct clk *gc_clk = NULL;
	if(!gc_clk)
		gc_clk = clk_get(NULL, "GCCLK");
	if(power_on)
		clk_enable(gc_clk);

	spin_lock(&gc_pwr_lock);

	if (power_on) {
#ifdef GC_USE_HW_PWRCTRL
		/* enable hw mode */
		val |= GC_HWMODE;
		GC_REG_WRITE(val);

		spin_lock(&gc_vpu_isp_pwr_lock);
		/* set PWR_BLK_TMR_REG to recommend value */
		__raw_writel(0x20001FFF, APMU_PWR_BLK_TMR_REG);

		/* pwr on GC */
		val = __raw_readl(APMU_PWR_CTRL_REG);
		val |= GC_AUTO_PWR_ON;
		__raw_writel(val, APMU_PWR_CTRL_REG);
		spin_unlock(&gc_vpu_isp_pwr_lock);

		/* polling pwr status */
		while (!(__raw_readl(APMU_PWR_STATUS_REG) & GC_AUTO_PWR_ON)) {
			udelay(200);
			timeout -= 200;
			if (timeout < 0) {
				pr_err("%s: power on timeout\n", __func__);
				clk_disable(gc_clk);
				return;
			}
		}
#else
		/* enable bus and function clock  */
		val |= GC_CLK_EN;
		GC_REG_WRITE(val);

#ifdef	CONFIG_CPU_PXA1088
		val_2d |= GC_CLK_EN;
		GC_2D_REG_WRITE(val_2d);
#endif
		/* enable power_on1, wait at least 200us */
		val |= GC_PWRON1;
		GC_REG_WRITE(val);
		udelay(200);

		/* enable power_on2 */
		val |= GC_PWRON2;
		GC_REG_WRITE(val);

		/* fRst release */
		val |= GC_FCLK_RST;
		GC_REG_WRITE(val);
#ifdef	CONFIG_CPU_PXA1088
		val_2d |= GC_FCLK_RST;
		GC_2D_REG_WRITE(val_2d);
#endif
		udelay(100);

		/* aRst hRst release at least 48 cycles later than fRst */
		val |= (GC_ACLK_RST | GC_HCLK_RST);
		GC_REG_WRITE(val);
#ifdef	CONFIG_CPU_PXA1088
		val_2d |= (GC_ACLK_RST | GC_HCLK_RST);
		GC_2D_REG_WRITE(val_2d);
#endif
		/* disable isolation */
		val |= GC_ISOB;
		GC_REG_WRITE(val);
#endif
	} else {
#ifdef GC_USE_HW_PWRCTRL
		spin_lock(&gc_vpu_isp_pwr_lock);
		/* pwr on GC */
		val = __raw_readl(APMU_PWR_CTRL_REG);
		val &= ~GC_AUTO_PWR_ON;
		__raw_writel(val, APMU_PWR_CTRL_REG);
		spin_unlock(&gc_vpu_isp_pwr_lock);

		/* polling pwr status */
		while ((__raw_readl(APMU_PWR_STATUS_REG) & GC_AUTO_PWR_ON)) {
			udelay(200);
			timeout -= 200;
			if (timeout < 0) {
				pr_err("%s: power off timeout\n", __func__);
				return;
			}
		}
#else
		/* enable isolation */
		val &= ~GC_ISOB;
		GC_REG_WRITE(val);

		/* disable power_on2 */
		val &= ~GC_PWRON2;
		GC_REG_WRITE(val);

		/* disable power_on1 */
		val &= ~GC_PWRON1;
		GC_REG_WRITE(val);

		/* fRst aRst hRst */
		val &= ~(GC_CLK_RST | GC_CLK_EN);
		GC_REG_WRITE(val);
#ifdef	CONFIG_CPU_PXA1088
		val_2d &= ~(GC_CLK_RST | GC_CLK_EN);
		GC_2D_REG_WRITE(val_2d);
#endif
		udelay(100);

#endif
	}
	spin_unlock(&gc_pwr_lock);
	if(power_on)
		clk_disable(gc_clk);
}
EXPORT_SYMBOL(gc_pwr);

#define VPU_HW_MODE	(0x1 << 19)
#define VPU_AUTO_PWR_ON	(0x1 << 2)
#define VPU_PWR_STAT	(0x1 << 2)

void coda7542_power_switch(int on)
{
	unsigned int val;
	int timeout = 2000;

	/* HW mode power on */
	if (on) {
		/* set VPU HW on/off mode  */
		val = __raw_readl(APMU_VPU_CLK_RES_CTRL);
		val |= VPU_HW_MODE;
		__raw_writel(val, APMU_VPU_CLK_RES_CTRL);

		spin_lock(&gc_vpu_isp_pwr_lock);
		/* on1, on2, off timer */
		__raw_writel(0x20001fff, APMU_PWR_BLK_TMR_REG);

		/* VPU auto power on */
		val = __raw_readl(APMU_PWR_CTRL_REG);
		val |= VPU_AUTO_PWR_ON;
		__raw_writel(val, APMU_PWR_CTRL_REG);
		spin_unlock(&gc_vpu_isp_pwr_lock);
		/*
		 * VPU power on takes 316us, usleep_range(280,290) takes about
		 * 300~320us, so it can reduce the duty cycle.
		 */
		usleep_range(280, 290);

		/* polling VPU_PWR_STAT bit */
		while (!(__raw_readl(APMU_PWR_STATUS_REG) & VPU_PWR_STAT)) {
			udelay(1);
			timeout -= 1;
			if (timeout < 0) {
				pr_err("%s: VPU power on timeout\n", __func__);
				return;
			}
		}
	/* HW mode power off */
	} else {
		spin_lock(&gc_vpu_isp_pwr_lock);
		/* VPU auto power off */
		val = __raw_readl(APMU_PWR_CTRL_REG);
		val &= ~VPU_AUTO_PWR_ON;
		__raw_writel(val, APMU_PWR_CTRL_REG);
		spin_unlock(&gc_vpu_isp_pwr_lock);
		/*
		 * VPU power off takes 23us, add a pre-delay to reduce the
		 * number of polling
		 */
		udelay(20);

		/* polling VPU_PWR_STAT bit */
		while ((__raw_readl(APMU_PWR_STATUS_REG) & VPU_PWR_STAT)) {
			udelay(1);
			timeout -= 1;
			if (timeout < 0) {
				pr_err("%s: VPU power off timeout\n", __func__);
				return;
			}
		}
	}
}

#ifdef CONFIG_ANDROID_RAM_CONSOLE
static void pxa988_ram_console_init(void)
{
	static struct persistent_ram ram;
	static struct persistent_ram_descriptor desc;
	static char name[20] = "ram_console";

	/* reserver 256K memory from DDR address 0x8100000 */
	ram.start = 0x8100000;
	ram.size = 0x40000;
	ram.num_descs = 1;

	desc.size = 0x40000;
	desc.name = name;
	ram.descs = &desc;

	persistent_ram_early_init(&ram);
}
#endif

#ifdef CONFIG_ION
static struct ion_platform_data ion_data = {
	.nr	= 2,
	.heaps	= {
		[0] = {
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.id	= ION_HEAP_TYPE_CARVEOUT,
			.name	= "carveout_heap",
			.size   = 0x01000000,
			.base   = 0x09000000,
		},
		[1] = {
			.type	= ION_HEAP_TYPE_SYSTEM,
			.id	= ION_HEAP_TYPE_SYSTEM,
			.name	= "system_heap",
		},
	},
};

struct platform_device device_ion = {
	.name	= "pxa-ion",
	.id	= -1,
	.dev	= {
		.platform_data = &ion_data,
	},
};

static int __init ion_mem_carveout(char *p)
{
	unsigned long size;
	phys_addr_t start;
	char *endp;

	size  = memparse(p, &endp);

	if (size == 0) {
		ion_data.heaps[0].size = 0;
		ion_data.heaps[0].base = 0;
		return 0;
	}

	if (*endp == '@')
		start = memparse(endp + 1, NULL);
	else
		BUG_ON(1);

	/* set the carveout heap range */
	ion_data.heaps[0].size = size;
	ion_data.heaps[0].base = start;

	return 0;
}
early_param("ioncarv", ion_mem_carveout);
#endif

/* CP memeory reservation, 32MB by default */
static u32 cp_area_size = 0x02000000;
static u32 cp_area_addr = 0x06000000;

static int __init early_cpmem(char *p)
{
	char *endp;

	cp_area_size = memparse(p, &endp);
	if (*endp == '@')
		cp_area_addr = memparse(endp + 1, NULL);

	return 0;
}
early_param("cpmem", early_cpmem);

static void __init pxa988_reserve_cpmem(void)
{
	/* Reserve memory for CP */
	BUG_ON(memblock_reserve(cp_area_addr, cp_area_size) != 0);
	memblock_free(cp_area_addr, cp_area_size);
	memblock_remove(cp_area_addr, cp_area_size);
	pr_info("Reserved CP memory: 0x%x@0x%x\n", cp_area_size, cp_area_addr);
}

static void __init pxa988_reserve_obmmem(void)
{
	/* Reserve 1MB memory for obm */
	BUG_ON(memblock_reserve(PLAT_PHYS_OFFSET, 0x100000) != 0);
	memblock_free(PLAT_PHYS_OFFSET, 0x100000);
	memblock_remove(PLAT_PHYS_OFFSET, 0x100000);
	pr_info("Reserved OBM memory: 0x%x@0x%lx\n",
		0x100000, PLAT_PHYS_OFFSET);
}

/*
* We will arrange the reserved memory for the following usage.
* 0 ~ PAGE_SIZE -1:  For low power
* PAGE_SIZE ~ 2 * PAGE_SIZE -1: For reset handler
*/
static void __init pxa988_reserve_pmmem(void)
{
	u32 pm_area_addr = 0x08000000;
	u32 pm_area_size = 0x00100000;
	pm_reserve_pa = pm_area_addr;
	/* Reserve 1MB memory for power management use */
	BUG_ON(memblock_reserve(pm_area_addr, pm_area_size) != 0);
	BUG_ON(memblock_free(pm_area_addr, pm_area_size));
	BUG_ON(0 != memblock_remove(pm_area_addr, pm_area_size));
}

#ifdef CONFIG_ION
static void __init pxa988_reserve_ion(void)
{
	BUG_ON(memblock_reserve(ion_data.heaps[0].base,
		ion_data.heaps[0].size));

	pr_info("ION carveout memory: 0x%08x@0x%08x\n",
		(u32)ion_data.heaps[0].size, (u32)ion_data.heaps[0].base);
}
#endif
void __init pxa988_reserve(void)
{
	/*
	 * reserve the first 1MB physical ddr memory for obm. when use EMMD
	 * (Enhanced Marvell Memory Dump), kernel should not make use of this
	 * memory, since it'll be corrupted by next reboot by obm.
	 */
	pxa988_reserve_obmmem();

	pxa988_reserve_cpmem();

#ifdef CONFIG_ANDROID_RAM_CONSOLE
	pxa988_ram_console_init();
#endif
#ifdef CONFIG_GPU_RESERVE_MEM
	pxa_reserve_gpu_memblock();
#endif
	pxa988_reserve_pmmem();
#ifdef CONFIG_ION
	pxa988_reserve_ion();
#endif

	pxa988_reserve_fb_mem();
}

void __init pxa988_init_irq(void)
{
	mmp_wakeupgen_init();
	gic_init(0, 29, IOMEM(GIC_DIST_VIRT_BASE), IOMEM(GIC_CPU_VIRT_BASE));
}

void pxa988_ripc_lock(void)
{
	int cnt = 0;
	unsigned long flags;

	spin_lock_irqsave(&ripc_lock, flags);

	while (__raw_readl(RIPC3_STATUS)) {
		/* give telephnoy chance to detect ripc status */

		ripc_status = 0;
		spin_unlock_irqrestore(&ripc_lock, flags);

		cpu_relax();
		udelay(50);

		cnt++;
		if (cnt >= 10000) {
			pr_warn("AP: ripc cannot be locked!\n");
			cnt = 0;
		}

		spin_lock_irqsave(&ripc_lock, flags);
	}

	/* we are sure to have held ripc */
	ripc_status = 1;

	spin_unlock_irqrestore(&ripc_lock, flags);
}

int pxa988_ripc_trylock(void)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&ripc_lock, flags);
	ret = !__raw_readl(RIPC3_STATUS);
	ripc_status = ret;
	spin_unlock_irqrestore(&ripc_lock, flags);

	return ret;
}

void pxa988_ripc_unlock(void)
{
	unsigned long flags;
	spin_lock_irqsave(&ripc_lock, flags);

	__raw_writel(1, RIPC3_STATUS);
	ripc_status = 0;

	spin_unlock_irqrestore(&ripc_lock, flags);
}

/* the caller _MUST_ hold ripc_lock before calling it */
int pxa988_ripc_status(void)
{
	return ripc_status;
}
EXPORT_SYMBOL(pxa988_ripc_status);

#ifdef CONFIG_CACHE_L2X0

#ifdef CONFIG_PM
static inline void l2x0_save_phys_reg_addr(u32 *addr_ptr, u32 addr)
{
	BUG_ON(!addr_ptr);
	*addr_ptr = addr;
	flush_cache_all();
	outer_clean_range(virt_to_phys(addr_ptr),
		virt_to_phys(addr_ptr) + sizeof(*addr_ptr));
}
#endif

static void pxa988_l2_cache_init(void)
{
	void __iomem *l2x0_base;

	l2x0_base = ioremap(SL2C_PHYS_BASE, SZ_4K);
	BUG_ON(!l2x0_base);

	/* TAG, Data Latency Control */
	writel_relaxed(0x010, l2x0_base + L2X0_TAG_LATENCY_CTRL);
	writel_relaxed(0x010, l2x0_base + L2X0_DATA_LATENCY_CTRL);

	/* L2X0 Power Control  */
	writel_relaxed(0x3, l2x0_base + L2X0_POWER_CTRL);

	/* Enable I/D cache prefetch feature */
	l2x0_init(l2x0_base, 0x30800000, 0xFE7FFFFF);

#ifdef CONFIG_PM
	l2x0_saved_regs.phy_base = SL2C_PHYS_BASE;
	l2x0_save_phys_reg_addr(&l2x0_regs_phys,
				l2x0_saved_regs_phys_addr);
#endif
}
#else
#define pxa988_l2_cache_init()
#endif

#ifdef CONFIG_HAVE_ARM_TWD
static DEFINE_TWD_LOCAL_TIMER(twd_local_timer,
	(unsigned int)TWD_PHYS_BASE, IRQ_LOCALTIMER);

static void __init pxa988_twd_init(void)
{
	int err = twd_local_timer_register(&twd_local_timer);
	if (err)
		pr_err("twd_local_timer_register failed %d\n", err);
}
#else
#define pxa988_twd_init(void)	do {} while (0)
#endif

#ifdef CONFIG_ARM_ARCH_TIMER
static struct arch_timer pxa1088_arch_timer = {
	.rate = 26000000,
	.ppi = {
		IRQ_GIC_PHYS_SECURE_PPI,
		IRQ_GIC_PHYS_NONSECURE_PPI,
		IRQ_GIC_VIRT_PPI,
		IRQ_GIC_HYP_PPI
	},
	.use_virtual = false,
	.base = GENERIC_COUNTER_VIRT_BASE
};
#endif /* #ifdef CONFIG_ARM_ARCH_TIMER */

static void __init pxa988_timer_init(void)
{
#ifdef CONFIG_ARM_ARCH_TIMER
	uint32_t tmp;
#endif /* #ifdef CONFIG_ARM_ARCH_TIMER */

#ifdef CONFIG_APB_LOCALTIMER
	apb_timer_init();
#else /* CONFIG_APB_LOCALTIMER */
	/* Select the configurable timer clock source to be 3.25MHz */
	__raw_writel(APBC_APBCLK | APBC_RST, APBC_PXA988_TIMERS);
	__raw_writel(APBC_APBCLK | APBC_FNCLK | APBC_FNCLKSEL(3),
		     APBC_PXA988_TIMERS);

	timer_init(IRQ_PXA988_AP_TIMER1);
	pxa988_twd_init();
#endif /* CONFIG_APB_LOCALTIMER */


#ifdef CONFIG_ARM_ARCH_TIMER
	tmp = readl(APBC_COUNTER_CLK_SEL);
	/* Default is 26M/32768 = 0x319 */
	if ((tmp >> 16) != 0x319) {
		pr_warn("pxa988_timer_init: Generic Counter"
			" step of Low Frequency is not right\n");
		return;
	}
	/* bit0 = 1: Generic Counter Frequency control by hardware VCTCXO_EN
	   VCTCXO_EN = 1, Generic Counter Frequency is 26Mhz;
	   VCTCXO_EN = 0, Generic Counter Frequency is 32KHz */
	writel(tmp | FREQ_HW_CTRL, APBC_COUNTER_CLK_SEL);

	/* NOTE: can not read CNTCR before write, otherwise write will fail
	   Halt on debug;
	   start the counter */
	writel(CNTCR_HDBG | CNTCR_EN, GENERIC_COUNTER_VIRT_BASE + CNTCR);

	arch_timer_init(&pxa1088_arch_timer);
#endif /* #ifdef CONFIG_ARM_ARCH_TIMER */
}

struct sys_timer pxa988_timer = {
	.init   = pxa988_timer_init,
};

void pxa988_clear_keypad_wakeup(void)
{
	uint32_t val;
	uint32_t mask = APMU_PXA988_KP_WAKE_CLR;

	/* wake event clear is needed in order to clear keypad interrupt */
	val = __raw_readl(APMU_WAKE_CLR);
	__raw_writel(val | mask, APMU_WAKE_CLR);
}

void pxa988_clear_sdh_wakeup(void)
{
	uint32_t val;
	uint32_t mask = APMU_PXA988_SD1_WAKE_CLR | APMU_PXA988_SD2_WAKE_CLR
					| APMU_PXA988_SD3_WAKE_CLR;

	val = __raw_readl(APMU_WAKE_CLR);
	__raw_writel(val | mask, APMU_WAKE_CLR);

}

struct resource pxa988_resource_gpio[] = {
	{
		.start	= 0xd4019000,
		.end	= 0xd40197ff,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= IRQ_PXA988_GPIO_AP,
		.end	= IRQ_PXA988_GPIO_AP,
		.name	= "gpio_mux",
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device pxa988_device_gpio = {
	.name		= "pxa-gpio",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(pxa988_resource_gpio),
	.resource	= pxa988_resource_gpio,
};

#ifdef CONFIG_USB_MV_UDC
static DEFINE_SPINLOCK(phy_lock);
static int phy_init_cnt;

static int usb_phy_init_internal(void __iomem *base)
{
	struct pxa988_usb_phy *phy = (struct pxa988_usb_phy *)base;
	int i;
	u32 phy_old, phy_power;

	pr_debug("init usb phy.\n");

	/*
	 * power up PHY by PIN.
	 * From the datasheet, it can be controlled by current regiter,
	 * but not pin.
	 * Will remove it after debug.
	 */
	phy_old = (u32)ioremap_nocache(0xD4207100, 0x10);
	phy_power = phy_old + 0x4;
	writel(0x10901003, phy_power);

	/* enable usb device PHY */
	writew(PLLVDD18(0x1) | REFDIV(0xd) | FBDIV(0xf0),
		&phy->utmi_pll_reg0);
#ifdef CONFIG_MACH_WILCOX_CMCC
     if (get_board_id() <BOARD_ID_CMCC_REV05) 
	writew(PU_PLL | PLL_LOCK_BYPASS | DLL_RESET_BLK |
		ICP(0x1) | KVCO(0x3) | PLLCAL12(0x3),
		&phy->utmi_pll_reg1);
	else
	writew(PU_PLL | PLL_LOCK_BYPASS | ICP(0x1) | KVCO(0x3) | PLLCAL12(0x3),
		&phy->utmi_pll_reg1);
#else
    writew(PU_PLL | PLL_LOCK_BYPASS | ICP(0x1) | KVCO(0x3) | PLLCAL12(0x3),
		&phy->utmi_pll_reg1);
#endif
	writew(IMPCAL_VTH(0x1) | EXT_HS_RCAL(0x8) | EXT_FS_RCAL(0x8),
		&phy->utmi_tx_reg0);
	writew(TXVDD15(0x1) | TXVDD12(0x3) | LOWVDD_EN |
		AMP(0x5) | CK60_PHSEL(0x4),
		&phy->utmi_tx_reg1);
	writew(DRV_SLEWRATE(0x2) | IMP_CAL_DLY(0x2) |
		FSDRV_EN(0xf) | HSDEV_EN(0xf),
		&phy->utmi_tx_reg2);
#if defined(CONFIG_MACH_CS02) || defined(CONFIG_MACH_WILCOX) || defined(CONFIG_MACH_GOYA)
	writew(PHASE_FREEZE_DLY | ACQ_LENGTH(0x2) | SQ_LENGTH(0x2) |
		DISCON_THRESH(0x2) | SQ_THRESH(0xc) | INTPI(0x1),
		&phy->utmi_rx_reg0);
#elif defined(CONFIG_MACH_LT02)
	writew(PHASE_FREEZE_DLY | ACQ_LENGTH(0x2) | SQ_LENGTH(0x2) |
		DISCON_THRESH(0x2) | SQ_THRESH(0x8) | INTPI(0x1),
		&phy->utmi_rx_reg0);
#else
	writew(PHASE_FREEZE_DLY | ACQ_LENGTH(0x2) | SQ_LENGTH(0x2) |
		DISCON_THRESH(0x2) | SQ_THRESH(0xa) | INTPI(0x1),
		&phy->utmi_rx_reg0);
#endif
	writew(EARLY_VOS_ON_EN | RXDATA_BLOCK_EN | EDGE_DET_EN |
		RXDATA_BLOCK_LENGTH(0x2) | EDGE_DET_SEL(0x1) |
		S2TO3_DLY_SEL(0x2),
		&phy->utmi_rx_reg1);
#ifdef CONFIG_MACH_LT02
	writew(USQ_FILTER | SQ_BUFFER_EN | RXVDD18(0x1) | RXVDD12(0x1),
		&phy->utmi_rx_reg2);
#else
	writew(USQ_FILTER | RXVDD18(0x1) | RXVDD12(0x1),
		&phy->utmi_rx_reg2);
#endif
	writew(BG_VSEL(0x1) | TOPVDD18(0x1),
		&phy->utmi_ana_reg0);
	writew(PU_ANA | SEL_LPFR | V2I(0x6) | R_ROTATE_SEL,
		&phy->utmi_ana_reg1);
	writew(FS_EOP_MODE | FORCE_END_EN | SYNCDET_WINDOW_EN |
		CLK_SUSPEND_EN | FIFO_FILL_NUM(0x6),
		&phy->utmi_dig_reg0);
	writew(FS_RX_ERROR_MODE2 | FS_RX_ERROR_MODE1 |
		FS_RX_ERROR_MODE | ARC_DPDM_MODE,
		&phy->utmi_dig_reg1);
	writew(0x0, &phy->utmi_charger_reg0);

	for (i = 0; i < 0x80; i = i + 4)
		pr_debug("[0x%x] = 0x%x\n", (u32)base + i,
			readw((u32)base + i));

	iounmap((void __iomem *)phy_old);
	return 0;
}

static int usb_phy_deinit_internal(void __iomem *base)
{
	u32 phy_old, phy_power;
	struct pxa988_usb_phy *phy = (struct pxa988_usb_phy *)base;
	u16 val;

	pr_debug("Deinit usb phy.\n");

	/* power down PHY PLL */
	val = readw(&phy->utmi_pll_reg1);
	val &= ~PU_PLL;
	writew(val, &phy->utmi_pll_reg1);

	/* power down PHY Analog part */
	val = readw(&phy->utmi_ana_reg1);
	val &= ~PU_ANA;
	writew(val, &phy->utmi_ana_reg1);

	/* power down PHY by PIN.
	 * From the datasheet, it can be controlled by current regiter,
	 * but not pin.
	 * Will remove it after debug.
	 */
	phy_old = (u32)ioremap_nocache(0xD4207100, 0x10);
	phy_power = phy_old + 0x4;
	writel(0x10901000, phy_power);

	iounmap((void __iomem *)phy_old);
	return 0;
}

int pxa_usb_phy_init(void __iomem *base)
{
	unsigned long flags;

	spin_lock_irqsave(&phy_lock, flags);
	if (phy_init_cnt++ == 0)
		usb_phy_init_internal(base);
	spin_unlock_irqrestore(&phy_lock, flags);
	return 0;
}

void pxa_usb_phy_deinit(void __iomem *base)
{
	unsigned long flags;

	WARN_ON(phy_init_cnt == 0);

	spin_lock_irqsave(&phy_lock, flags);
	if (--phy_init_cnt == 0)
		usb_phy_deinit_internal(base);
	spin_unlock_irqrestore(&phy_lock, flags);
}

static u64 usb_dma_mask = ~(u32)0;

struct resource pxa988_udc_resources[] = {
	/* regbase */
	[0] = {
		.start	= PXA988_UDC_REGBASE + PXA988_UDC_CAPREGS_RANGE,
		.end	= PXA988_UDC_REGBASE + PXA988_UDC_REG_RANGE,
		.flags	= IORESOURCE_MEM,
		.name	= "capregs",
	},
	/* phybase */
	[1] = {
		.start	= PXA988_UDC_PHYBASE,
		.end	= PXA988_UDC_PHYBASE + PXA988_UDC_PHY_RANGE,
		.flags	= IORESOURCE_MEM,
		.name	= "phyregs",
	},
	[2] = {
		.start	= IRQ_PXA988_USB1,
		.end	= IRQ_PXA988_USB1,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device pxa988_device_udc = {
	.name		= "mv-udc",
	.id		= -1,
	.resource	= pxa988_udc_resources,
	.num_resources	= ARRAY_SIZE(pxa988_udc_resources),
	.dev		=  {
		.dma_mask	= &usb_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	}
};
#endif /* CONFIG_USB_MV_UDC */

#if defined(CONFIG_TOUCHSCREEN_VNC)
struct platform_device pxa988_device_vnc_touch = {
	.name   = "vnc-ts",
	.id     = -1,
};
#endif /* CONFIG_TOUCHSCREEN_VNC */

/*
 * This function is used to adjust the xtc for sram.
 * It is used to achieve better Vmin floor.
 */
static void pxa988_set_xtc(void)
{
	/* wtc/rtc for Z1/Z2 silicon */
	if (cpu_is_pxa988_z1() || \
		cpu_is_pxa988_z2() || \
		cpu_is_pxa986_z1() || \
		cpu_is_pxa986_z2()) {
		/* CORE L1 */
		writel_relaxed(0xAAAAAAAA, CIU_CPU_CONF_SRAM_0);
		/* CORE L2 */
		writel_relaxed(0x0000A666, CIU_CPU_CONF_SRAM_1);
		/* GC */
		writel_relaxed(0x00045555, CIU_GPU_XTC_REG);
		/* VPU */
		writel_relaxed(0x00B06655, CIU_VPU_XTC_REG);
	} else if (cpu_is_pxa1088()) {
		writel_relaxed(0x00055555, CIU_GPU_XTC_REG);
	} else {
		/* On Z3, core L1/L2 wtc/rtc change on the fly */
		/* GC */
		writel_relaxed(0x00044444, CIU_GPU_XTC_REG);
		/* VPU keeps default setting */
	}
}

static int __init pxa988_init(void)
{
	pxa988_l2_cache_init();

	mfp_init_base(MFPR_VIRT_BASE);
	mfp_init_addr(pxa988_addr_map);

#ifdef CONFIG_TZ_HYPERVISOR
/* if enable TrustZone, reserve ch30/ch31 for GEU. */
	pxa_init_dma(IRQ_PXA988_DMA_INT0, 30);
#else
	pxa_init_dma(IRQ_PXA988_DMA_INT0, 32);
#endif

#ifdef CONFIG_ION
	platform_device_register(&device_ion);
#endif
#ifdef CONFIG_GPU_RESERVE_MEM
	pxa_add_gpu();
#endif
	platform_device_register(&pxa988_device_gpio);
#if defined(CONFIG_TOUCHSCREEN_VNC)
	platform_device_register(&pxa988_device_vnc_touch);
#endif /* CONFIG_TOUCHSCREEN_VNC */
	mmp_gpio_edge_init(GPIOE_VIRT_BASE, MFP_PIN_MAX, 128);

	pxa988_set_xtc();

	if (cpu_is_pxa1088()) {
		pxa_init_pmua_regdump();
		pxa_init_gic_regdump();
	}
	return 0;
}

postcore_initcall(pxa988_init);

/* on-chip devices */
PXA988_DEVICE(uart0, "pxa2xx-uart", 0, UART0, 0xd4036000, 0x30, 4, 5);
PXA988_DEVICE(uart1, "pxa2xx-uart", 1, UART1, 0xd4017000, 0x30, 21, 22);
PXA988_DEVICE(uart2, "pxa2xx-uart", 2, UART2, 0xd4018000, 0x30, 23, 24);
PXA988_DEVICE(keypad, "pxa27x-keypad", -1, KEYPAD, 0xd4012000, 0x4c);
PXA988_DEVICE(twsi0, "pxa910-i2c", 0, I2C0, 0xd4011000, 0x60);
PXA988_DEVICE(twsi1, "pxa910-i2c", 1, I2C1, 0xd4010800, 0x60);
PXA988_DEVICE(twsi2, "pxa910-i2c", 2, I2C2, 0xd4037000, 0x60);
PXA988_DEVICE(pwm1, "pxa910-pwm", 0, NONE, 0xd401a000, 0x10);
PXA988_DEVICE(pwm2, "pxa910-pwm", 1, NONE, 0xd401a400, 0x10);
PXA988_DEVICE(pwm3, "pxa910-pwm", 2, NONE, 0xd401a800, 0x10);
PXA988_DEVICE(pwm4, "pxa910-pwm", 3, NONE, 0xd401ac00, 0x10);
PXA988_DEVICE(sdh1, "sdhci-pxav3", 0, MMC, 0xd4280000, 0x120);
PXA988_DEVICE(sdh2, "sdhci-pxav3", 1, MMC, 0xd4280800, 0x120);
PXA988_DEVICE(sdh3, "sdhci-pxav3", 2, MMC, 0xd4281000, 0x120);
PXA988_DEVICE(ssp0, "pxa988-ssp", 0, SSP0, 0xd401b000, 0x90, 52, 53);
PXA988_DEVICE(ssp1, "pxa988-ssp", 1, SSP1, 0xd42a0c00, 0x90, 1, 2);
PXA988_DEVICE(ssp2, "pxa988-ssp", 2, SSP2, 0xd401C000, 0x90, 60, 61);
PXA988_DEVICE(gssp, "pxa988-ssp", 4, GSSP, 0xd4039000, 0x90, 6, 7);
PXA988_DEVICE(asram, "asram", 0, NONE, SRAM_AUDIO_BASE, SRAM_AUDIO_SIZE);
PXA988_DEVICE(isram, "isram", 1, NONE, SRAM_VIDEO_BASE, SRAM_VIDEO_SIZE);
PXA988_DEVICE(fb, "pxa168-fb", 0, LCD, 0xd420b000, 0x1fc);
PXA988_DEVICE(fb_ovly, "pxa168fb_ovly", 0, LCD, 0xd420b000, 0x1fc);
PXA988_DEVICE(fb_tv, "pxa168-fb", 1, LCD, 0xd420b000, 0x1fc);
PXA988_DEVICE(fb_tv_ovly, "pxa168fb_ovly", 1, LCD, 0xd420b000, 0x1fc);
PXA988_DEVICE(camera, "mmp-camera", 0, CI, 0xd420a000, 0x1000);
PXA988_DEVICE(thermal, "thermal", -1, DRO_SENSOR, 0xd4013200, 0x34);

static struct resource pxa988_resource_rtc[] = {
	{ 0xd4010000, 0xd40100ff, NULL, IORESOURCE_MEM, },
	{ IRQ_PXA988_RTC, IRQ_PXA988_RTC, "rtc 1Hz", IORESOURCE_IRQ, },
	{ IRQ_PXA988_RTC_ALARM, IRQ_PXA988_RTC_ALARM, "rtc alarm", IORESOURCE_IRQ, },
};

struct platform_device pxa988_device_rtc = {
	.name		= "sa1100-rtc",
	.id		= -1,
	.resource	= pxa988_resource_rtc,
	.num_resources	= ARRAY_SIZE(pxa988_resource_rtc),
};

static struct resource pxa988_resource_squ[] = {
	{
		.start	= 0xd42a0800,
		.end	= 0xd42a08ff,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= IRQ_PXA988_HIFI_DMA,
		.end	= IRQ_PXA988_HIFI_DMA,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device pxa988_device_squ = {
	.name		= "pxa910-squ",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(pxa988_resource_squ),
	.resource	= pxa988_resource_squ,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
};

static struct resource pxa988_resource_pcm_audio[] = {
	 {
		 /* playback dma */
		.name	= "pxa910-squ",
		.start	= 0,
		.flags	= IORESOURCE_DMA,
	},
	 {
		 /* record dma */
		.name	= "pxa910-squ",
		.start	= 1,
		.flags	= IORESOURCE_DMA,
	},
};

static struct mmp_audio_platdata mmp_audio_pdata = {
	.period_max_capture = 4 * 1024,
	.buffer_max_capture = 20 * 1024,
	.period_max_playback = 4 * 1024,
	.buffer_max_playback = 20 * 1024,
};

struct platform_device pxa988_device_asoc_platform = {
	.name		= "mmp-pcm-audio",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(pxa988_resource_pcm_audio),
	.resource	= pxa988_resource_pcm_audio,
	.dev = {
		.platform_data  = &mmp_audio_pdata,
	},
};

#ifdef CONFIG_VIDEO_MVISP
static u64 pxa988_dxo_dma_mask = DMA_BIT_MASK(32);

static struct resource pxa988_dxoisp_resources[] = {
	[0] = {
		.name	= "isp-ipc",
		.start	= IRQ_PXA988_DXO,
		.end	= IRQ_PXA988_DXO,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device pxa988_device_dxoisp = {
	.name           = "pxa988-mvisp",
	.id             = 0,
	.dev            = {
		.dma_mask = &pxa988_dxo_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.resource       = pxa988_dxoisp_resources,
	.num_resources  = ARRAY_SIZE(pxa988_dxoisp_resources),
};

static struct resource pxa988_dxodma_resources[] = {
	{
		.start	= 0xD420F000,
		.end	= 0xD4210000 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= IRQ_PXA988_ISP_DMA,
		.end	= IRQ_PXA988_ISP_DMA,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "ISP-CLK",
		.flags	= MAP_RES_CLK,
	},
};

struct platform_device pxa988_device_dxodma = {
	.name		= "dxo-dma",
	.id             = 0,
	.dev            = {
		.dma_mask = &pxa988_dxo_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.resource       = pxa988_dxodma_resources,
	.num_resources  = ARRAY_SIZE(pxa988_dxodma_resources),
};

static struct resource pxa988_ccic_resources[] = {
#if 0
	/* This is a W/R to avoid iomem conflict with smart sensor driver */
	/* FIXME: after merge MC/SOC camera driver, should enable this mem_res*/
	{
		.start	= 0xD420A000,
		.end	= 0xD420A100 - 1,
		.flags	= IORESOURCE_MEM,
	},
#endif
	{
		.start	= IRQ_PXA988_CI,
		.end	= IRQ_PXA988_CI,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "CCICFUNCLK",
		.flags	= MAP_RES_CLK,
	},
	{
		.name	= "CCICPHYCLK",
		.flags	= MAP_RES_CLK,
	},
};

struct platform_device pxa988_device_ccic = {
	.name           = "ccic",
	.id             = 0,
	.dev            = {
		.dma_mask = &pxa988_dxo_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.resource       = pxa988_ccic_resources,
	.num_resources  = ARRAY_SIZE(pxa988_ccic_resources),
};

void pxa988_register_dxoisp(struct mvisp_platform_data *data)
{
	int ret;

	pxa988_device_dxodma.dev.platform_data = data;
	ret = platform_device_register(&pxa988_device_dxodma);
	if (ret)
		dev_err(&(pxa988_device_dxodma.dev),
				"unable to register dxodma device: %d\n", ret);

	pxa988_device_ccic.dev.platform_data = data;
	ret = platform_device_register(&pxa988_device_ccic);
	if (ret)
		dev_err(&(pxa988_device_ccic.dev),
				"unable to register ccic device: %d\n", ret);

	pxa988_device_dxoisp.dev.platform_data = data;
	ret = platform_device_register(&pxa988_device_dxoisp);
	if (ret)
		dev_err(&(pxa988_device_dxoisp.dev),
				"unable to register dxo device: %d\n", ret);
}

#define ISP_HW_MODE         (0x1 << 15)
#define ISP_AUTO_PWR_ON     (0x1 << 4)
#define ISP_PWR_STAT        (0x1 << 4)
#define ISP_CLK_RST         ((1 << 0) | (1 << 8) | (1 << 10))
#define ISP_CLK_EN          ((1 << 1) | (1 << 9) | (1 << 11))

int pxa988_isp_power_control(int on)
{
	unsigned int val;
	int timeout = 5000;

	/*  HW mode power on/off*/
	if (on) {
		/* set isp HW mode*/
		val = __raw_readl(APMU_ISPDXO);
		val |= ISP_HW_MODE;
		__raw_writel(val, APMU_ISPDXO);

		spin_lock(&gc_vpu_isp_pwr_lock);
		/*  on1, on2, off timer */
		__raw_writel(0x20001fff, APMU_PWR_BLK_TMR_REG);

		/*  isp auto power on */
		val = __raw_readl(APMU_PWR_CTRL_REG);
		val |= ISP_AUTO_PWR_ON;
		__raw_writel(val, APMU_PWR_CTRL_REG);
		spin_unlock(&gc_vpu_isp_pwr_lock);

		/*  polling ISP_PWR_STAT bit */
		while (!(__raw_readl(APMU_PWR_STATUS_REG) & ISP_PWR_STAT)) {
			udelay(500);
			timeout -= 500;
			if (timeout < 0) {
				pr_err("%s: isp power on timeout\n", __func__);
				return -ENODEV;
			}
		}

	} else {
		spin_lock(&gc_vpu_isp_pwr_lock);
		/*  isp auto power off */
		val = __raw_readl(APMU_PWR_CTRL_REG);
		val &= ~ISP_AUTO_PWR_ON;
		__raw_writel(val, APMU_PWR_CTRL_REG);
		spin_unlock(&gc_vpu_isp_pwr_lock);

		/*  polling ISP_PWR_STAT bit */
		while ((__raw_readl(APMU_PWR_STATUS_REG) & ISP_PWR_STAT)) {
			udelay(500);
			timeout -= 500;
			if (timeout < 0) {
				pr_err("%s: ISP power off timeout\n", __func__);
				return -ENODEV;
			}
		}

	}

	return 0;
}

#define LCD_CI_ISP_ACLK_EN		(1 << 3)
#define LCD_CI_ISP_ACLK_RST		(1 << 16)
int pxa988_isp_reset_hw(void *param)
{
	unsigned int val;

	/*disable isp clock*/
	val = __raw_readl(APMU_ISPDXO);
	val &= ~ISP_CLK_EN;
	__raw_writel(val, APMU_ISPDXO);

#if defined(ENABLE_LCD_RST)
	val = __raw_readl(APMU_LCD);
	val &= ~LCD_CI_ISP_ACLK_EN;
	__raw_writel(val, APMU_LCD);

	/*reset clock*/
	val = __raw_readl(APMU_LCD);
	val &= ~LCD_CI_ISP_ACLK_RST;
	__raw_writel(val, APMU_LCD);
#endif

	/*reset isp clock*/
	val = __raw_readl(APMU_ISPDXO);
	val &= ~ISP_CLK_RST;
	__raw_writel(val, APMU_ISPDXO);

	/*de-reset isp clock*/
	val = __raw_readl(APMU_ISPDXO);
	val |= ISP_CLK_RST;
	__raw_writel(val, APMU_ISPDXO);

#if defined(ENABLE_LCD_RST)
	val = __raw_readl(APMU_LCD);
	val |= LCD_CI_ISP_ACLK_RST;
	__raw_writel(val, APMU_LCD);

	val = __raw_readl(APMU_LCD);
	val |= LCD_CI_ISP_ACLK_EN;
	__raw_writel(val, APMU_LCD);
#endif

	/*enable isp clock*/
	val = __raw_readl(APMU_ISPDXO);
	val |= ISP_CLK_EN;
	__raw_writel(val, APMU_ISPDXO);

	return 0;
}
#endif

#ifdef CONFIG_UIO_MVISP
static u64 pxa_uio_mvisp_dma_mask = DMA_BIT_MASK(32);

static struct resource pxa_uio_mvisp_resources[] = {
	[0] = {
		.start	= 0xD4240000,
		.end	= 0xD427FFFF,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device pxa_device_mvisp = {
	.name	= "uio-mvisp",
	.id	= 0,
	.dev	= {
		.dma_mask = &pxa_uio_mvisp_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.resource	= pxa_uio_mvisp_resources,
	.num_resources = ARRAY_SIZE(pxa_uio_mvisp_resources),
};

void __init pxa_register_uio_mvisp(void)
{
	int ret;

	ret = platform_device_register(&pxa_device_mvisp);
	if (ret)
		dev_err(&(pxa_device_mvisp.dev),
			"unable to register uio mvisp device:%d\n", ret);
}
#endif
