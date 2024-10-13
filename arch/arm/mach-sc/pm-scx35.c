/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <asm/irqflags.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/cacheflush.h>
#include <mach/system.h>
#include <mach/pm_debug.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>
#include <mach/irqs.h>
#include <mach/sci.h>
#include "emc_repower.h"
#include <linux/clockchips.h>
#include <linux/wakelock.h>
#include <mach/adi.h>
#include <mach/arch_misc.h>
#if defined(CONFIG_SPRD_DEBUG)
/* For saving Fault status */
#include <mach/sprd_debug.h>
#endif

extern int sc8830_get_clock_status(void);
extern void secondary_startup(void);
extern int sp_pm_collapse(unsigned int cpu, unsigned int save_state);
extern void sp_pm_collapse_exit(void);
extern void sc8830_standby_iram(void);
extern void sc8830_standby_iram_end(void);
extern void sc8830_standby_exit_iram(void);
extern int sc_cpuidle_init(void);
void pm_ana_ldo_config(void);
struct ap_ahb_reg_bak {
	u32 ahb_eb;
	u32 ca7_ckg_cfg;
	u32 misc_ckg_en;
	u32 misc_cfg;
};
struct ap_clk_reg_bak {
	u32 ap_ahb_cfg;
	u32 ap_apb_cfg;
	u32 clk_gsp_cfg;
	u32 dispc0_cfg;
	u32 dispc0_dbi_cfg;
	u32 dispc0_dpi_cfg;
	u32 dispc1_cfg;
	u32 dispc1_dbi_cfg;
	u32 dispc1_dpi_cfg;
	u32 nfc_cfg;
	u32 sdio0_cfg;
	u32 sdio1_cfg;
	u32 sdio2_cfg;
	u32 emmc_cfg;
	u32 gps_cfg;
	u32 gps_tcxo_cfg;
	u32 usb_ref_cfg;
	u32 uart0_cfg;
	u32 uart1_cfg;
	u32 uart2_cfg;
	u32 uart3_cfg;
	u32 uart4_cfg;
	u32 i2c0_cfg;
	u32 i2c1_cfg;
	u32 i2c2_cfg;
	u32 i2c3_cfg;
	u32 i2c4_cfg;
	u32 spi0_cfg;
	u32 spi1_cfg;
	u32 spi2_cfg;
	u32 iis0_cfg;
	u32 iis1_cfg;
	u32 iis2_cfg;
	u32 iis3_cfg;
};
struct ap_apb_reg_bak {
	u32 apb_eb;
	u32 usb_phy_tune;
	u32 usb_phy_ctrl;
	u32 apb_misc_ctrl;
};
struct pub_reg_bak {
	u32 ddr_qos_cfg1;
	u32 ddr_qos_cfg2;
	u32 ddr_qos_cfg3;
};
struct mm_reg_bak {
	u32 mm_cfg1;
	u32 mm_cfg2;
};

struct auto_pd_en {
	volatile u32 magic_header;
	volatile u32 bits;
	volatile u32 magic_end;
	char pd_config_menu[6][16];
};

struct dcdc_core_ds_step_info{
	u32 ctl_reg;
	u32 ctl_sht;
	u32 cal_reg;
	u32 cal_sht;
};

static struct ap_ahb_reg_bak ap_ahb_reg_saved;
static struct ap_clk_reg_bak ap_clk_reg_saved;
static struct ap_apb_reg_bak ap_apb_reg_saved;
static struct pub_reg_bak pub_reg_saved;
#if defined(CONFIG_ARCH_SCX15)
static struct mm_reg_bak mm_reg_saved;
#endif

/* bits definition
 * bit_0 : ca7 top
 * bit_1 : ca7 c0
 * bit_2 : pub
 * bit_3 : mm
 * bit_4 : ap sys
 * bit_5 : dcdc arm
*/
static  struct auto_pd_en pd_config = {
	0x6a6aa6a6, 0x3f,0xa6a66a6a,
	.pd_config_menu = {
	"ca7_top",
	"ca7_c0",
	"pub",
	"mm",
	"ap_sys",
	"dcdc_arm",
	},
};

/*
 * we just conigure the auto_pd_en property for parts of  power domain, ldo  here
 * clr = 0 means called when kernel init and after real deep sleep
 * clr = 1 means just called before real deep sleep
 */
static void configure_for_deepsleep(int clr)
{
#if defined(CONFIG_ARCH_SCX15)
	if (clr) {
		sci_glb_clr(REG_PMU_APB_PD_MM_TOP_CFG,BIT_PD_MM_TOP_AUTO_SHUTDOWN_EN);
		sci_glb_clr(REG_PMU_APB_PD_AP_SYS_CFG, BIT_PD_AP_SYS_AUTO_SHUTDOWN_EN);
		sci_glb_clr(REG_PMU_APB_PD_PUB_SYS_CFG, BIT_PD_PUB_SYS_AUTO_SHUTDOWN_EN);
		/* FIXME: we can't powerdown ca7 because of stability problem when idle deep sleep */
		sci_glb_clr(REG_PMU_APB_PD_CA7_TOP_CFG, BIT_PD_CA7_TOP_AUTO_SHUTDOWN_EN);
		sci_glb_clr(REG_PMU_APB_PD_CA7_C0_CFG, BIT_PD_CA7_C0_AUTO_SHUTDOWN_EN);
		sci_adi_clr(ANA_REG_GLB_PWR_SLP_CTRL0, BIT_SLP_DCDCARM_PD_EN);
		/*
		 * because we can power down rf0 and vdd25 in idle deep now
		 * so we can let it be configured in u-boot
		 */
		/* sci_adi_set(ANA_REG_GLB_PWR_SLP_CTRL0, BIT_SLP_LDORF0_PD_EN); */
		/* sci_adi_set(ANA_REG_GLB_PWR_SLP_CTRL0, BIT_SLP_LDOVDD25_PD_EN); */
#if defined(CONFIG_MACH_CORSICA_VE)
		sci_adi_clr(ANA_REG_GLB_PWR_SLP_CTRL1, BIT_SLP_LDOSIM2_PD_EN);
		sci_adi_clr(ANA_REG_GLB_PWR_SLP_CTRL1, BIT_SLP_LDOCAMMOT_PD_EN);
#endif
	} else {
		if (pd_config.bits & (0x1 << 0))
			sci_glb_set(REG_PMU_APB_PD_CA7_TOP_CFG, BIT_PD_CA7_TOP_AUTO_SHUTDOWN_EN);
		if (pd_config.bits & (0x1 << 1))
			sci_glb_set(REG_PMU_APB_PD_CA7_C0_CFG, BIT_PD_CA7_C0_AUTO_SHUTDOWN_EN);
		if (pd_config.bits & (0x1 << 2))
			sci_glb_set(REG_PMU_APB_PD_PUB_SYS_CFG, BIT_PD_PUB_SYS_AUTO_SHUTDOWN_EN);
		if (pd_config.bits & (0x1 << 3))
			sci_glb_set(REG_PMU_APB_PD_MM_TOP_CFG,BIT_PD_MM_TOP_AUTO_SHUTDOWN_EN);
		if (pd_config.bits & (0x1 << 4))
			sci_glb_set(REG_PMU_APB_PD_AP_SYS_CFG, BIT_PD_AP_SYS_AUTO_SHUTDOWN_EN);
		if (pd_config.bits & (0x1 << 5))
			sci_adi_set(ANA_REG_GLB_PWR_SLP_CTRL0, BIT_SLP_DCDCARM_PD_EN);
#if defined(CONFIG_MACH_CORSICA_VE)
		sci_adi_set(ANA_REG_GLB_PWR_SLP_CTRL1, BIT_SLP_LDOSIM2_PD_EN);
		sci_adi_set(ANA_REG_GLB_PWR_SLP_CTRL1, BIT_SLP_LDOCAMMOT_PD_EN);
#endif
		#if defined(CONFIG_SS_FUNCTION)
		sci_glb_set(REG_PMU_APB_PD_PUB_SYS_CFG,BIT_PD_PUB_SYS_AUTO_SHUTDOWN_EN);
		#endif
	}
#endif
}

static void setup_autopd_mode(void)
{
	if (soc_is_scx35_v0())
		sci_glb_write(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, 0x3a, -1UL);
	else
		sci_glb_write(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, 0x3B, -1UL);
	sci_glb_write(REG_PMU_APB_AP_WAKEUP_POR_CFG, 0x1, -1UL);//AP_WAKEUP_POR_CFG

	sci_glb_set(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG,
			BIT_AP_EMC_AUTO_GATE_EN |
			BIT_CA7_EMC_AUTO_GATE_EN |
			BIT_CA7_CORE_AUTO_GATE_EN);

	// INTC0_EB, INTC1_EB, INTC2_EB, INTC3_EB
	sci_glb_set(REG_AP_APB_APB_EB, 0xf<<19);

	//set PD_CA7_C0_AUTO_SHUTDOWN_EN
	sci_glb_set(REG_PMU_APB_PD_CA7_TOP_CFG, BIT_PD_CA7_TOP_AUTO_SHUTDOWN_EN);

	//set PD_CA7_C0_AUTO_SHUTDOWN_EN
	sci_glb_set(REG_PMU_APB_PD_CA7_C0_CFG, BIT_PD_CA7_C0_AUTO_SHUTDOWN_EN);

	sci_glb_set(SPRD_INT_BASE + 0x8, BIT(14) | BIT(4)); //ana & eic

	sci_glb_set(SPRD_INTC1_BASE + 0x8, BIT(6) | BIT(4)); //ana & kpd
	//sci_glb_set(SPRD_INT_BASE + 0x8, BIT(2)); //ana & eic
	//sci_glb_set(SPRD_INTC0_BASE + 0x8, BIT(30) | BIT(31)); //syst & aon_syst

	//sci_adi_set(ANA_REG_GLB_LDO_SLP_CTRL0, BIT_SLP_IO_EN | BIT_SLP_LDORF0_PD_EN | BIT_SLP_LDORF1_PD_EN | BIT_SLP_LDORF2_PD_EN);
	//sci_adi_clr(ANA_REG_GLB_LDO_SLP_CTRL0, BIT_SLP_IO_EN | BIT_SLP_LDORF0_PD_EN | BIT_SLP_LDORF1_PD_EN | BIT_SLP_LDORF2_PD_EN);

	//sci_adi_set(ANA_REG_GLB_LDO_SLP_CTRL1, BIT_SLP_LDO_PD_EN);
	sci_adi_clr(ANA_REG_GLB_XTL_WAIT_CTRL, BIT_SLP_XTLBUF_PD_EN);
	//sci_adi_set(ANA_REG_GLB_XTL_WAIT_CTRL, BIT_SLP_XTLBUF_PD_EN);
	//sci_adi_set(ANA_REG_GLB_LDO_SLP_CTRL2, BIT_SLP_LDORF2_LP_EN | BIT_SLP_LDORF1_LP_EN | BIT_SLP_LDORF0_LP_EN);

	//sci_adi_clr(ANA_REG_GLB_LDO_SLP_CTRL2, BIT_SLP_LDORF2_LP_EN | BIT_SLP_LDORF1_LP_EN | BIT_SLP_LDORF0_LP_EN);

	//sci_glb_set(REG_PMU_APB_DDR_SLEEP_CTRL, 7<<4);
	//sci_glb_set(REG_PMU_APB_DDR_SLEEP_CTRL, 5<<4);
#if defined(CONFIG_ARCH_SCX15)
       sci_glb_set(REG_PMU_APB_PD_PUB_SYS_CFG, BIT_PD_PUB_SYS_AUTO_SHUTDOWN_EN);
#else
	if (soc_is_scx35_v1()) {
		sci_glb_set(REG_PMU_APB_PD_PUB_SYS_CFG, BIT_PD_PUB_SYS_AUTO_SHUTDOWN_EN);
	}
#ifndef CONFIG_ARCH_SCX30G
	else {
		sci_glb_clr(REG_PMU_APB_PD_PUB_SYS_CFG, BIT_PD_PUB_SYS_AUTO_SHUTDOWN_EN);
	}
#endif

#if defined(CONFIG_SCX35_DMC_FREQ_DDR3)
	sci_glb_clr(REG_PMU_APB_PD_PUB_SYS_CFG, BIT_PD_PUB_SYS_AUTO_SHUTDOWN_EN);
#endif

#endif

#if defined(CONFIG_ARCH_SCX15)
	sci_glb_set(REG_PMU_APB_PD_MM_TOP_CFG,BIT_PD_MM_TOP_AUTO_SHUTDOWN_EN);
	sci_glb_clr(REG_PMU_APB_PD_PUB_SYS_CFG,BIT_PD_PUB_SYS_AUTO_SHUTDOWN_EN);
#else
	sci_glb_clr(REG_PMU_APB_PD_MM_TOP_CFG,BIT_PD_MM_TOP_AUTO_SHUTDOWN_EN);
#endif
	sci_glb_write(REG_PMU_APB_AP_WAKEUP_POR_CFG, 0x1, -1UL);
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	/* KEEP eMMC/SD power */
	sci_adi_clr(ANA_REG_GLB_LDO_SLP_CTRL0, BIT_SLP_LDOEMMCCORE_PD_EN | BIT_SLP_LDOEMMCIO_PD_EN);
	sci_adi_clr(ANA_REG_GLB_LDO_SLP_CTRL1, BIT_SLP_LDOSD_PD_EN );
#else
#if defined(CONFIG_ADIE_SC2723S)
	/* KEEP eMMC/SD power */
	sci_adi_clr(ANA_REG_GLB_PWR_SLP_CTRL0, BIT_SLP_LDOEMMCCORE_PD_EN); /* | BIT_SLP_LDOEMMCIO_PD_EN);*/
	sci_adi_clr(ANA_REG_GLB_PWR_SLP_CTRL1, BIT_SLP_LDOSDIO_PD_EN );
#endif
#endif
	/*****************  for idle to deep  ****************/
	configure_for_deepsleep(1);
	return;
}
void disable_mm(void)
{
	sci_glb_clr(REG_MM_AHB_GEN_CKG_CFG, BIT_JPG_AXI_CKG_EN | BIT_VSP_AXI_CKG_EN |\
		BIT_ISP_AXI_CKG_EN | BIT_DCAM_AXI_CKG_EN | BIT_SENSOR_CKG_EN | BIT_MIPI_CSI_CKG_EN | BIT_CPHY_CFG_CKG_EN);
	sci_glb_clr(REG_MM_AHB_AHB_EB, BIT_JPG_EB | BIT_CSI_EB | BIT_VSP_EB | BIT_ISP_EB | BIT_CCIR_EB | BIT_DCAM_EB);
	sci_glb_clr(REG_MM_AHB_GEN_CKG_CFG, BIT_MM_AXI_CKG_EN);
	sci_glb_clr(REG_MM_AHB_GEN_CKG_CFG, BIT_MM_MTX_AXI_CKG_EN);
	sci_glb_clr(REG_MM_AHB_AHB_EB, BIT_MM_CKG_EB);
	sci_glb_clr(REG_PMU_APB_PD_MM_TOP_CFG,BIT_PD_MM_TOP_AUTO_SHUTDOWN_EN);
	sci_glb_clr(REG_AON_APB_APB_EB0, BIT_MM_EB);
	sci_glb_set(REG_PMU_APB_PD_MM_TOP_CFG,BIT_PD_MM_TOP_AUTO_SHUTDOWN_EN);
	return;
}
void bak_restore_mm(int bak)
{
	static uint32_t gen_ckg, ahb_eb, mm_top, aon_eb0;
	static uint32_t gen_ckg_en_mask = BIT_JPG_AXI_CKG_EN | BIT_VSP_AXI_CKG_EN |\
		BIT_ISP_AXI_CKG_EN | BIT_DCAM_AXI_CKG_EN | BIT_SENSOR_CKG_EN | BIT_MIPI_CSI_CKG_EN | BIT_CPHY_CFG_CKG_EN;
	static uint32_t ahb_eb_mask = BIT_JPG_EB | BIT_CSI_EB | BIT_VSP_EB | BIT_ISP_EB | BIT_CCIR_EB | BIT_DCAM_EB;
	if(bak){
		gen_ckg = sci_glb_read(REG_MM_AHB_GEN_CKG_CFG, -1UL);
		ahb_eb = sci_glb_read(REG_MM_AHB_AHB_EB, -1UL);
		mm_top = sci_glb_read(REG_PMU_APB_PD_MM_TOP_CFG, -1UL);
		aon_eb0 = sci_glb_read(REG_AON_APB_APB_EB0, -1UL);
	}else{
		sci_glb_clr(REG_PMU_APB_PD_MM_TOP_CFG,BIT_PD_MM_TOP_AUTO_SHUTDOWN_EN);
		if(aon_eb0 & BIT_MM_EB)
			sci_glb_set(REG_AON_APB_APB_EB0, BIT_MM_EB);
		if(ahb_eb & BIT_MM_CKG_EB)
			sci_glb_set(REG_MM_AHB_AHB_EB, BIT_MM_CKG_EB);
		if(gen_ckg & BIT_MM_MTX_AXI_CKG_EN)
			sci_glb_set(REG_MM_AHB_GEN_CKG_CFG, BIT_MM_MTX_AXI_CKG_EN);
		if(gen_ckg & BIT_MM_AXI_CKG_EN)
			sci_glb_set(REG_MM_AHB_GEN_CKG_CFG, BIT_MM_AXI_CKG_EN);
		if(ahb_eb & ahb_eb_mask)
			sci_glb_write(REG_MM_AHB_AHB_EB, ahb_eb, ahb_eb_mask);
		if(gen_ckg & gen_ckg_en_mask)
			sci_glb_write(REG_MM_AHB_GEN_CKG_CFG, gen_ckg, gen_ckg_en_mask);
	}
	return;
}
void disable_dma(void)
{
	int reg = sci_glb_read(SPRD_DMA0_BASE, BIT(16));
	int cnt = 30;
	if(reg){
		sci_glb_set((SPRD_DMA0_BASE), BIT(0)); // pause dma
		while(cnt-- && reg){
			sci_glb_read(SPRD_DMA0_BASE, BIT(16));
			udelay(100);
			reg = sci_glb_read(SPRD_DMA0_BASE, BIT(16));
		}
		if(reg)
			printk("disable dma failed\n");
	}
	sci_glb_clr(REG_AP_AHB_AHB_EB, BIT_DMA_EB);
}
void disable_ahb_module(void)
{
	//sci_glb_write(REG_AP_AHB_AHB_EB, 0, -1UL);
	sci_glb_clr(REG_AP_AHB_AHB_EB, 0x1fff);

	// AP_PERI_FORCE_SLP

	sci_glb_set(REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG, BIT_AP_PERI_FORCE_SLP);
	sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_SLEEP_FOLLOW_CA7_EN);
	sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_DEEP_SLEEP_EN);

	//AP_SYS_AUTO_SLEEP_CFG
	sci_glb_set(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, 0x3B);

	return;
}
void bak_restore_ahb(int bak)
{
	volatile u32 i;
	if(bak){
		ap_ahb_reg_saved.ahb_eb                 = sci_glb_read(REG_AP_AHB_AHB_EB, -1UL);
		ap_ahb_reg_saved.ca7_ckg_cfg            = sci_glb_read(REG_AP_AHB_CA7_CKG_CFG, -1UL);
		ap_ahb_reg_saved.misc_ckg_en            = sci_glb_read(REG_AP_AHB_MISC_CKG_EN, -1UL);
		ap_ahb_reg_saved.misc_cfg               = sci_glb_read(REG_AP_AHB_MISC_CFG, -1UL);
	} else{
		sci_glb_write(REG_AP_AHB_AHB_EB, 	ap_ahb_reg_saved.ahb_eb, -1UL);
		sci_glb_write(REG_AP_AHB_CA7_CKG_CFG, 	ap_ahb_reg_saved.ca7_ckg_cfg & (~0x7), -1UL);
		for(i = 0; i < 20; i++);
		sci_glb_write(REG_AP_AHB_CA7_CKG_CFG, 	ap_ahb_reg_saved.ca7_ckg_cfg, -1UL);
		sci_glb_write(REG_AP_AHB_MISC_CKG_EN, 	ap_ahb_reg_saved.misc_ckg_en, -1UL);
		sci_glb_write(REG_AP_AHB_MISC_CFG, 	ap_ahb_reg_saved.misc_cfg, -1UL);
	}
	return;
}

void disable_apb_module(void)
{
	int stat;
	stat = sci_glb_read(REG_AP_APB_APB_EB, -1UL);
	stat &=(0xf<<19 | BIT_UART1_EB | BIT_UART0_EB); // leave intc on
	sci_glb_write(REG_AP_APB_APB_EB, stat, -1UL);
	return;
}
void bak_restore_apb(int bak)
{
	if(bak){
		ap_apb_reg_saved.apb_eb = sci_glb_read(REG_AP_APB_APB_EB, -1UL);
		ap_apb_reg_saved.apb_misc_ctrl = sci_glb_read(REG_AP_APB_APB_MISC_CTRL, -1UL);
		ap_apb_reg_saved.usb_phy_tune = sci_glb_read(REG_AP_APB_USB_PHY_TUNE, -1UL);
	}else{
		sci_glb_write(REG_AP_APB_APB_EB, ap_apb_reg_saved.apb_eb, -1UL);
		sci_glb_write(REG_AP_APB_APB_MISC_CTRL, ap_apb_reg_saved.apb_misc_ctrl, -1UL);
		sci_glb_write(REG_AP_APB_USB_PHY_TUNE, ap_apb_reg_saved.usb_phy_tune, -1UL);
	}
	return;
}
#if defined(CONFIG_ARCH_SCX15)
extern void __iomap_page(unsigned long virt, unsigned long size, int enable);
void bak_restore_mm_scx15(int bak)
{
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_MM_EB);
	__iomap_page(REGS_MM_AHB_BASE, SZ_4K, 1);
	if(bak){
		mm_reg_saved.mm_cfg1= sci_glb_read(SPRD_MMAHB_BASE, -1UL);
		mm_reg_saved.mm_cfg2 = sci_glb_read(SPRD_MMAHB_BASE + 0x8, -1UL);
	}else{
		sci_glb_write(SPRD_MMAHB_BASE, mm_reg_saved.mm_cfg1, -1UL);
		sci_glb_write(SPRD_MMAHB_BASE + 0x8, mm_reg_saved.mm_cfg2, -1UL);
	}
	__iomap_page(REGS_MM_AHB_BASE, SZ_4K, 0);
	sci_glb_clr(REG_AON_APB_APB_EB0, BIT_MM_EB);
	return;
}
#endif
static void bak_ap_clk_reg(int bak)
{
	volatile u32 i;
	if(bak) {
		ap_clk_reg_saved.ap_ahb_cfg      = sci_glb_read(REG_AP_CLK_AP_AHB_CFG      , -1UL);
		ap_clk_reg_saved.ap_apb_cfg      = sci_glb_read(REG_AP_CLK_AP_APB_CFG      , -1UL);
		ap_clk_reg_saved.clk_gsp_cfg     = sci_glb_read(REG_AP_CLK_GSP_CFG         , -1UL);
		ap_clk_reg_saved.dispc0_cfg      = sci_glb_read(REG_AP_CLK_DISPC0_CFG      , -1UL);
		ap_clk_reg_saved.dispc0_dbi_cfg  = sci_glb_read(REG_AP_CLK_DISPC0_DBI_CFG  , -1UL);
		ap_clk_reg_saved.dispc0_dpi_cfg  = sci_glb_read(REG_AP_CLK_DISPC0_DPI_CFG  , -1UL);
		ap_clk_reg_saved.dispc1_cfg      = sci_glb_read(REG_AP_CLK_DISPC1_CFG      , -1UL);
		ap_clk_reg_saved.dispc1_dbi_cfg  = sci_glb_read(REG_AP_CLK_DISPC1_DBI_CFG  , -1UL);
		ap_clk_reg_saved.dispc1_dpi_cfg  = sci_glb_read(REG_AP_CLK_DISPC1_DPI_CFG  , -1UL);
		ap_clk_reg_saved.nfc_cfg         = sci_glb_read(REG_AP_CLK_NFC_CFG         , -1UL);
		ap_clk_reg_saved.sdio0_cfg       = sci_glb_read(REG_AP_CLK_SDIO0_CFG       , -1UL);
		ap_clk_reg_saved.sdio1_cfg       = sci_glb_read(REG_AP_CLK_SDIO1_CFG       , -1UL);
		ap_clk_reg_saved.sdio2_cfg       = sci_glb_read(REG_AP_CLK_SDIO2_CFG       , -1UL);
		ap_clk_reg_saved.emmc_cfg        = sci_glb_read(REG_AP_CLK_EMMC_CFG        , -1UL);
		ap_clk_reg_saved.gps_cfg         = sci_glb_read(REG_AP_CLK_GPS_CFG         , -1UL);
		ap_clk_reg_saved.gps_tcxo_cfg    = sci_glb_read(REG_AP_CLK_GPS_TCXO_CFG    , -1UL);
		ap_clk_reg_saved.usb_ref_cfg     = sci_glb_read(REG_AP_CLK_USB_REF_CFG     , -1UL);
		ap_clk_reg_saved.uart0_cfg       = sci_glb_read(REG_AP_CLK_UART0_CFG       , -1UL);
		ap_clk_reg_saved.uart1_cfg       = sci_glb_read(REG_AP_CLK_UART1_CFG       , -1UL);
		ap_clk_reg_saved.uart2_cfg       = sci_glb_read(REG_AP_CLK_UART2_CFG       , -1UL);
		ap_clk_reg_saved.uart3_cfg       = sci_glb_read(REG_AP_CLK_UART3_CFG       , -1UL);
		ap_clk_reg_saved.uart4_cfg       = sci_glb_read(REG_AP_CLK_UART4_CFG       , -1UL);
		ap_clk_reg_saved.i2c0_cfg        = sci_glb_read(REG_AP_CLK_I2C0_CFG        , -1UL);
		ap_clk_reg_saved.i2c1_cfg        = sci_glb_read(REG_AP_CLK_I2C1_CFG        , -1UL);
		ap_clk_reg_saved.i2c2_cfg        = sci_glb_read(REG_AP_CLK_I2C2_CFG        , -1UL);
		ap_clk_reg_saved.i2c3_cfg        = sci_glb_read(REG_AP_CLK_I2C3_CFG        , -1UL);
		ap_clk_reg_saved.i2c4_cfg        = sci_glb_read(REG_AP_CLK_I2C4_CFG        , -1UL);
		ap_clk_reg_saved.spi0_cfg        = sci_glb_read(REG_AP_CLK_SPI0_CFG        , -1UL);
		ap_clk_reg_saved.spi1_cfg        = sci_glb_read(REG_AP_CLK_SPI1_CFG        , -1UL);
		ap_clk_reg_saved.spi2_cfg        = sci_glb_read(REG_AP_CLK_SPI2_CFG        , -1UL);
		ap_clk_reg_saved.iis0_cfg        = sci_glb_read(REG_AP_CLK_IIS0_CFG        , -1UL);
		ap_clk_reg_saved.iis1_cfg        = sci_glb_read(REG_AP_CLK_IIS1_CFG        , -1UL);
		ap_clk_reg_saved.iis2_cfg        = sci_glb_read(REG_AP_CLK_IIS2_CFG        , -1UL);
		ap_clk_reg_saved.iis3_cfg        = sci_glb_read(REG_AP_CLK_IIS3_CFG        , -1UL);
	}
	else {
		sci_glb_write(REG_AP_CLK_AP_AHB_CFG      , ap_clk_reg_saved.ap_ahb_cfg      ,-1UL);
		sci_glb_write(REG_AP_CLK_AP_APB_CFG      , ap_clk_reg_saved.ap_apb_cfg      ,-1UL);
		sci_glb_write(REG_AP_CLK_GSP_CFG         , ap_clk_reg_saved.clk_gsp_cfg     ,-1UL);
		sci_glb_write(REG_AP_CLK_DISPC0_CFG      , ap_clk_reg_saved.dispc0_cfg & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_DISPC0_CFG      , ap_clk_reg_saved.dispc0_cfg      ,-1UL);
		sci_glb_write(REG_AP_CLK_DISPC0_DBI_CFG  , ap_clk_reg_saved.dispc0_dbi_cfg & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_DISPC0_DBI_CFG  , ap_clk_reg_saved.dispc0_dbi_cfg  ,-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_DISPC0_DPI_CFG  , ap_clk_reg_saved.dispc0_dpi_cfg & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_DISPC0_DPI_CFG  , ap_clk_reg_saved.dispc0_dpi_cfg  ,-1UL);
		sci_glb_write(REG_AP_CLK_DISPC1_CFG      , ap_clk_reg_saved.dispc1_cfg & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_DISPC1_CFG      , ap_clk_reg_saved.dispc1_cfg      ,-1UL);
		sci_glb_write(REG_AP_CLK_DISPC1_DBI_CFG  , ap_clk_reg_saved.dispc1_dbi_cfg & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_DISPC1_DBI_CFG  , ap_clk_reg_saved.dispc1_dbi_cfg  ,-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_DISPC1_DPI_CFG  , ap_clk_reg_saved.dispc1_dpi_cfg & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_DISPC1_DPI_CFG  , ap_clk_reg_saved.dispc1_dpi_cfg  ,-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_NFC_CFG         , ap_clk_reg_saved.nfc_cfg & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_NFC_CFG         , ap_clk_reg_saved.nfc_cfg         ,-1UL);
		sci_glb_write(REG_AP_CLK_SDIO0_CFG       , ap_clk_reg_saved.sdio0_cfg & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_SDIO0_CFG       , ap_clk_reg_saved.sdio0_cfg       ,-1UL);
		sci_glb_write(REG_AP_CLK_SDIO1_CFG       , ap_clk_reg_saved.sdio1_cfg       ,-1UL);
		sci_glb_write(REG_AP_CLK_SDIO2_CFG       , ap_clk_reg_saved.sdio2_cfg       ,-1UL);
		sci_glb_write(REG_AP_CLK_EMMC_CFG        , ap_clk_reg_saved.emmc_cfg        ,-1UL);
		sci_glb_write(REG_AP_CLK_GPS_CFG         , ap_clk_reg_saved.gps_cfg         ,-1UL);
		sci_glb_write(REG_AP_CLK_GPS_TCXO_CFG    , ap_clk_reg_saved.gps_tcxo_cfg    ,-1UL);
		sci_glb_write(REG_AP_CLK_USB_REF_CFG     , ap_clk_reg_saved.usb_ref_cfg     ,-1UL);
		sci_glb_write(REG_AP_CLK_UART0_CFG       , ap_clk_reg_saved.uart0_cfg & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_UART0_CFG       , ap_clk_reg_saved.uart0_cfg       ,-1UL);
		sci_glb_write(REG_AP_CLK_UART1_CFG       , ap_clk_reg_saved.uart1_cfg & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_UART1_CFG       , ap_clk_reg_saved.uart1_cfg       ,-1UL);
		sci_glb_write(REG_AP_CLK_UART2_CFG       , ap_clk_reg_saved.uart2_cfg & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_UART2_CFG       , ap_clk_reg_saved.uart2_cfg       ,-1UL);
		sci_glb_write(REG_AP_CLK_UART3_CFG       , ap_clk_reg_saved.uart3_cfg & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_UART3_CFG       , ap_clk_reg_saved.uart3_cfg       ,-1UL);
		sci_glb_write(REG_AP_CLK_UART4_CFG       , ap_clk_reg_saved.uart4_cfg & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_UART4_CFG       , ap_clk_reg_saved.uart4_cfg       ,-1UL);
		sci_glb_write(REG_AP_CLK_I2C0_CFG        , ap_clk_reg_saved.i2c0_cfg  & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_I2C0_CFG        , ap_clk_reg_saved.i2c0_cfg        ,-1UL);
		sci_glb_write(REG_AP_CLK_I2C1_CFG        , ap_clk_reg_saved.i2c1_cfg  & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_I2C1_CFG        , ap_clk_reg_saved.i2c1_cfg        ,-1UL);
		sci_glb_write(REG_AP_CLK_I2C2_CFG        , ap_clk_reg_saved.i2c2_cfg  & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_I2C2_CFG        , ap_clk_reg_saved.i2c2_cfg        ,-1UL);
		sci_glb_write(REG_AP_CLK_I2C3_CFG        , ap_clk_reg_saved.i2c3_cfg  & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_I2C3_CFG        , ap_clk_reg_saved.i2c3_cfg        ,-1UL);
		sci_glb_write(REG_AP_CLK_I2C4_CFG        , ap_clk_reg_saved.i2c4_cfg  & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_I2C4_CFG        , ap_clk_reg_saved.i2c4_cfg        ,-1UL);
		sci_glb_write(REG_AP_CLK_SPI0_CFG        , ap_clk_reg_saved.spi0_cfg  & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_SPI0_CFG        , ap_clk_reg_saved.spi0_cfg        ,-1UL);
		sci_glb_write(REG_AP_CLK_SPI1_CFG        , ap_clk_reg_saved.spi1_cfg  & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_SPI1_CFG        , ap_clk_reg_saved.spi1_cfg        ,-1UL);
		sci_glb_write(REG_AP_CLK_SPI2_CFG        , ap_clk_reg_saved.spi2_cfg  & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_SPI2_CFG        , ap_clk_reg_saved.spi2_cfg        ,-1UL);
		sci_glb_write(REG_AP_CLK_IIS0_CFG        , ap_clk_reg_saved.iis0_cfg  & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_IIS0_CFG        , ap_clk_reg_saved.iis0_cfg        ,-1UL);
		sci_glb_write(REG_AP_CLK_IIS1_CFG        , ap_clk_reg_saved.iis1_cfg  & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_IIS1_CFG        , ap_clk_reg_saved.iis1_cfg        ,-1UL);
		sci_glb_write(REG_AP_CLK_IIS2_CFG        , ap_clk_reg_saved.iis2_cfg  & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_IIS2_CFG        , ap_clk_reg_saved.iis2_cfg        ,-1UL);
		sci_glb_write(REG_AP_CLK_IIS3_CFG        , ap_clk_reg_saved.iis3_cfg  & (~0x3),-1UL);
		for(i = 0; i < 10; i++);
		sci_glb_write(REG_AP_CLK_IIS3_CFG        , ap_clk_reg_saved.iis3_cfg        ,-1UL);
	}
}

static void bak_restore_ap_intc(int bak)
{
	static u32 intc0_eb, intc1_eb, intc2_eb,intc3_eb;
	if(bak)
	{
		intc0_eb = sci_glb_read(SPRD_INTC0_BASE + 0x8, -1UL);
		intc1_eb = sci_glb_read(SPRD_INTC1_BASE + 0x8, -1UL);
		intc2_eb = sci_glb_read(SPRD_INTC2_BASE + 0x8, -1UL);
		intc3_eb = sci_glb_read(SPRD_INTC3_BASE + 0x8, -1UL);
	}
	else
	{
		sci_glb_write(SPRD_INTC0_BASE + 0x8, intc0_eb, -1UL);
		sci_glb_write(SPRD_INTC1_BASE + 0x8, intc1_eb, -1UL);
		sci_glb_write(SPRD_INTC2_BASE + 0x8, intc2_eb, -1UL);
		sci_glb_write(SPRD_INTC3_BASE + 0x8, intc3_eb, -1UL);
	}
}

void disable_aon_module(void)
{
	//sci_glb_clr(REG_AON_APB_APB_EB0, BIT_CA7_DAP_EB | BIT_GPU_EB);
	//sci_glb_clr(REG_AON_APB_APB_EB0, BIT_CA7_DAP_EB);

	sci_glb_clr(REG_AON_APB_APB_EB0, BIT_AON_TMR_EB | BIT_AP_TMR0_EB);
	sci_glb_clr(REG_AON_APB_APB_EB1, BIT_AP_TMR2_EB | BIT_AP_TMR1_EB);
	sci_glb_clr(REG_AON_APB_APB_EB1, BIT_DISP_EMC_EB);
}
void bak_restore_aon(int bak)
{
	static uint32_t apb_eb1, pwr_ctl, apb_eb0;
	if(bak){
		apb_eb0 = sci_glb_read(REG_AON_APB_APB_EB0, -1UL);
		apb_eb1 = sci_glb_read(REG_AON_APB_APB_EB1, -1UL);
		pwr_ctl = sci_glb_read(REG_AON_APB_PWR_CTRL, -1UL);
	}else{
		if(apb_eb1 & BIT_DISP_EMC_EB)
			sci_glb_set(REG_AON_APB_APB_EB1, BIT_DISP_EMC_EB);
		sci_glb_write(REG_AON_APB_APB_EB0, apb_eb0, -1UL);
		sci_glb_write(REG_AON_APB_APB_EB1, apb_eb1, -1UL);
	}
	return;
}
void disable_ana_module(void)
{
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	sci_adi_set(ANA_REG_GLB_LDO_PD_CTRL, BIT_LDO_SD_PD | BIT_LDO_SIM0_PD | BIT_LDO_SIM1_PD | BIT_LDO_SIM2_PD | BIT_LDO_CAMA_PD |\
		BIT_LDO_CAMD_PD | BIT_LDO_CAMIO_PD | BIT_LDO_CAMMOT_PD  | BIT_DCDC_WPA_PD);
#else
#if defined(CONFIG_ADIE_SC2723S)
	sci_adi_set(ANA_REG_GLB_LDO_PD_CTRL, BIT_LDO_SDIO_PD | BIT_LDO_SIM0_PD | BIT_LDO_SIM1_PD | BIT_LDO_SIM2_PD | BIT_LDO_CAMA_PD |\
                BIT_LDO_CAMD_PD | BIT_LDO_CAMIO_PD | BIT_LDO_CAMMOT_PD  | BIT_DCDC_WPA_PD);
#endif
#endif
}
void bak_restore_ana(int bak)
{
	static uint32_t ldo_pd_ctrl;
	static uint32_t mask;
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	mask = BIT_LDO_SD_PD | BIT_LDO_SIM0_PD | BIT_LDO_SIM1_PD | BIT_LDO_SIM2_PD | BIT_LDO_CAMA_PD |\
		BIT_LDO_CAMD_PD | BIT_LDO_CAMIO_PD | BIT_LDO_CAMMOT_PD  | BIT_DCDC_WPA_PD;
#else
#if defined(CONFIG_ADIE_SC2723S)
	mask = BIT_LDO_SDIO_PD | BIT_LDO_SIM0_PD | BIT_LDO_SIM1_PD | BIT_LDO_SIM2_PD | BIT_LDO_CAMA_PD |\
                BIT_LDO_CAMD_PD | BIT_LDO_CAMIO_PD | BIT_LDO_CAMMOT_PD  | BIT_DCDC_WPA_PD;
#endif
#endif
	if(bak){
		ldo_pd_ctrl = sci_adi_read(ANA_REG_GLB_LDO_PD_CTRL);
	}else{
		sci_adi_write(ANA_REG_GLB_LDO_PD_CTRL, ldo_pd_ctrl, mask);
	}
	return;
}
static void bak_restore_pub(int bak)
{
	/*v0 not set auto power down*/
	if (soc_is_scx35_v0()) {
		return ;
	}
	if(bak) {
		pub_reg_saved.ddr_qos_cfg1 = sci_glb_read(REG_PUB_APB_DDR_QOS_CFG1, -1UL);
		pub_reg_saved.ddr_qos_cfg2 = sci_glb_read(REG_PUB_APB_DDR_QOS_CFG2, -1UL);
		pub_reg_saved.ddr_qos_cfg3 = sci_glb_read(REG_PUB_APB_DDR_QOS_CFG3, -1UL);
	} else {
		sci_glb_write(REG_PUB_APB_DDR_QOS_CFG1, pub_reg_saved.ddr_qos_cfg1, -1UL);
		sci_glb_write(REG_PUB_APB_DDR_QOS_CFG2, pub_reg_saved.ddr_qos_cfg2, -1UL);
		sci_glb_write(REG_PUB_APB_DDR_QOS_CFG3, pub_reg_saved.ddr_qos_cfg3, -1UL);
	}
}
#define REG_PIN_XTLEN                   ( SPRD_PIN_BASE + 0x0138 )
#define REG_PIN_CHIP_SLEEP              ( SPRD_PIN_BASE + 0x0208 )
#define REG_PIN_XTL_BUF_EN0             ( SPRD_PIN_BASE + 0x020C )
#define REG_PIN_XTL_BUF_EN1             ( SPRD_PIN_BASE + 0x0210 )
#define REG_PIN_XTL_BUF_EN2             ( SPRD_PIN_BASE + 0x0214 )
void show_pin_reg(void)
{
	sci_glb_set(SPRD_INT_BASE + 0x8, BIT(14) | BIT(4)); //ana & eic
	printk("REG_PIN_XTLEN   0x%08x\n", sci_glb_read(REG_PIN_XTLEN, -1UL));
	printk("REG_PIN_XTL_BUF_EN0   0x%08x\n", sci_glb_read(REG_PIN_XTL_BUF_EN0, -1UL));
	printk("REG_PIN_XTL_BUF_EN1   0x%08x\n", sci_glb_read(REG_PIN_XTL_BUF_EN1, -1UL));
	printk("REG_PIN_XTL_BUF_EN2   0x%08x\n", sci_glb_read(REG_PIN_XTL_BUF_EN2, -1UL));
	printk("REG_PIN_CHIP_SLEEP    0x%08x\n", sci_glb_read(REG_PIN_CHIP_SLEEP, -1UL));
	printk("### uart1 ckd 0x%08x\n", sci_glb_read(SPRD_UART1_BASE + 0X24, -1UL));
	printk("### uart1 ctl 0x%08x\n", sci_glb_read(SPRD_UART1_BASE + 0X18, -1UL));
#if defined(CONFIG_ARCH_SCX15)
	sci_glb_set(REG_PMU_APB_XTL0_REL_CFG, 0x4);
	sci_glb_set(REG_PMU_APB_XTLBUF0_REL_CFG, 0x4);
	sci_glb_clr(REG_PMU_APB_PD_CP1_TD_CFG, BIT(24));
#endif
	printk("REG_PMU_APB_XTL0_REL_CFG 0x%08x\n", sci_glb_read(REG_PMU_APB_XTL0_REL_CFG, -1UL));
	printk("REG_PMU_APB_XTL1_REL_CFG 0x%08x\n", sci_glb_read(REG_PMU_APB_XTL1_REL_CFG, -1UL));
	printk("REG_PMU_APB_XTL2_REL_CFG 0x%08x\n", sci_glb_read(REG_PMU_APB_XTL2_REL_CFG, -1UL));
	printk("REG_PMU_APB_XTLBUF0_REL_CFG 0x%08x\n", sci_glb_read(REG_PMU_APB_XTLBUF0_REL_CFG, -1UL));
	printk("REG_PMU_APB_XTLBUF1_REL_CFG 0x%08x\n", sci_glb_read(REG_PMU_APB_XTLBUF1_REL_CFG, -1UL));
	printk("REG_PMU_APB_PD_CP1_TD_CFG 0x%08x\n", sci_glb_read(REG_PMU_APB_PD_CP1_TD_CFG, -1Ul));
}
void force_mcu_core_sleep(void)
{
	sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_CORE_SLEEP);
}
void enable_mcu_deep_sleep(void)
{
	sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_DEEP_SLEEP_EN | BIT_MCU_LIGHT_SLEEP_EN);
}
void disable_mcu_deep_sleep(void)
{
	sci_glb_clr(REG_AP_AHB_MCU_PAUSE, BIT_MCU_DEEP_SLEEP_EN | BIT_MCU_LIGHT_SLEEP_EN);
}
#define BITS_SET_CHECK(__reg, __bits, __string) do{\
	uint32_t mid =sci_glb_read(__reg, __bits); \
	if(mid != __bits) \
		printk(__string " not 1" "\n"); \
	}while(0)
#define BITS_CLEAR_CHECK(__reg, __bits, __string) do{\
	uint32_t mid = sci_glb_read(__reg, __bits); \
	if(mid & __bits) \
		printk(__string " not 0" "\n"); \
	}while(0)
#define PRINT_REG(__reg) do{\
	uint32_t mid = sci_glb_read(__reg, -1UL); \
	printk(#__reg "value 0x%x\n", mid); \
	}while(0)
void show_reg_status(void)
{
	BITS_CLEAR_CHECK(REG_AON_APB_APB_EB0, BIT_CA7_DAP_EB, "ca7_dap_eb");
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_DMA_EB, "dma_eb");
	BITS_CLEAR_CHECK((SPRD_DMA0_BASE + 0x1c), BIT(20), "dma_busy");
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_USB_EB, "usb_eb");
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_SDIO0_EB, "sdio0_eb");
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_SDIO1_EB, "sdio1_eb");
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_SDIO2_EB, "sdio2_eb");
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_EMMC_EB, "emmc_eb");
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_NFC_EB, "nfc_eb");
	PRINT_REG(REG_PMU_APB_SLEEP_STATUS);
}
uint32_t pwr_stat0=0, pwr_stat1=0, pwr_stat2=0, pwr_stat3=0, apb_eb0=0, apb_eb1=0, ahb_eb=0, apb_eb=0, mcu_pause=0, sys_force_sleep=0, sys_auto_sleep_cfg=0;
uint32_t ldo_slp_ctrl0, ldo_slp_ctrl1, ldo_slp_ctrl2, xtl_wait_ctrl, ldo_slp_ctrl3, ldo_aud_ctrl4;
uint32_t mm_apb, ldo_pd_ctrl, arm_module_en;
uint32_t cp_slp_status_dbg0, cp_slp_status_dbg1, sleep_ctrl, ddr_sleep_ctrl, sleep_status, pwr_ctrl, ca7_standby_status;
uint32_t pd_pub_sys;
#if defined(CONFIG_ARCH_SCX15)
uint32_t ldo_dcdc_pd_ctrl;
uint32_t xtl0_rel_cfg, xtl1_rel_cfg, xtl2_rel_cfg, xtlbuf0_rel_cfg, xtlbuf1_rel_cfg;
uint32_t mpll_rel_cfg, dpll_rel_cfg, tdpll_rel_cfg, wpll_rel_cfg, cpll_rel_cfg, wifipll1_rel_cfg, wifipll2_rel_cfg;
uint32_t ddr_chn_sleep_ctrl0, ddr_chn_sleep_ctrl1;
#endif
void bak_last_reg(void)
{
	pd_pub_sys = __raw_readl(REG_PMU_APB_PD_PUB_SYS_CFG);
	cp_slp_status_dbg0 = __raw_readl(REG_PMU_APB_CP_SLP_STATUS_DBG0);
	cp_slp_status_dbg1 = __raw_readl(REG_PMU_APB_CP_SLP_STATUS_DBG1);
	pwr_stat0 = __raw_readl(REG_PMU_APB_PWR_STATUS0_DBG);
	pwr_stat1 = __raw_readl(REG_PMU_APB_PWR_STATUS1_DBG);
	pwr_stat2 = __raw_readl(REG_PMU_APB_PWR_STATUS2_DBG);
	pwr_stat3 = __raw_readl(REG_PMU_APB_PWR_STATUS3_DBG);
	sleep_ctrl = __raw_readl(REG_PMU_APB_SLEEP_CTRL);
	ddr_sleep_ctrl = __raw_readl(REG_PMU_APB_DDR_SLEEP_CTRL);
	sleep_status = __raw_readl(REG_PMU_APB_SLEEP_STATUS);

#if defined(CONFIG_ARCH_SCX15)
	xtl0_rel_cfg = __raw_readl(REG_PMU_APB_XTL0_REL_CFG);
	xtl1_rel_cfg = __raw_readl(REG_PMU_APB_XTL1_REL_CFG);
	xtl2_rel_cfg = __raw_readl(REG_PMU_APB_XTL2_REL_CFG);
	xtlbuf0_rel_cfg = __raw_readl(REG_PMU_APB_XTLBUF0_REL_CFG);
	xtlbuf1_rel_cfg = __raw_readl(REG_PMU_APB_XTLBUF1_REL_CFG);
	mpll_rel_cfg = __raw_readl(REG_PMU_APB_MPLL_REL_CFG);
	dpll_rel_cfg = __raw_readl(REG_PMU_APB_DPLL_REL_CFG);
	tdpll_rel_cfg = __raw_readl(REG_PMU_APB_TDPLL_REL_CFG);
	wpll_rel_cfg = __raw_readl(REG_PMU_APB_WPLL_REL_CFG);
	cpll_rel_cfg = __raw_readl(REG_PMU_APB_CPLL_REL_CFG);
	wifipll1_rel_cfg = __raw_readl(REG_PMU_APB_WIFIPLL1_REL_CFG);
	wifipll2_rel_cfg = __raw_readl(REG_PMU_APB_WIFIPLL2_REL_CFG);
	ddr_chn_sleep_ctrl0 = __raw_readl(REG_PMU_APB_DDR_CHN_SLEEP_CTRL0);
	ddr_chn_sleep_ctrl1 = __raw_readl(REG_PMU_APB_DDR_CHN_SLEEP_CTRL1);
#endif

	apb_eb0 = __raw_readl(REG_AON_APB_APB_EB0);
	apb_eb1 = __raw_readl(REG_AON_APB_APB_EB1);
	pwr_ctrl = __raw_readl(REG_AON_APB_PWR_CTRL);

	ahb_eb = __raw_readl(REG_AP_AHB_AHB_EB);

	apb_eb = __raw_readl(REG_AP_APB_APB_EB);
	mcu_pause = __raw_readl(REG_AP_AHB_MCU_PAUSE);
	sys_force_sleep = __raw_readl(REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG);
	sys_auto_sleep_cfg = __raw_readl(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG);
	ca7_standby_status = __raw_readl(REG_AP_AHB_CA7_STANDBY_STATUS);

#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	ldo_slp_ctrl0 = sci_adi_read(ANA_REG_GLB_LDO_SLP_CTRL0);
	ldo_slp_ctrl1 = sci_adi_read(ANA_REG_GLB_LDO_SLP_CTRL1);
	ldo_slp_ctrl2 = sci_adi_read(ANA_REG_GLB_LDO_SLP_CTRL2);
	ldo_slp_ctrl3 = sci_adi_read(ANA_REG_GLB_LDO_SLP_CTRL3);
#else
#if defined(CONFIG_ADIE_SC2723S)
	ldo_slp_ctrl0 = sci_adi_read(ANA_REG_GLB_PWR_SLP_CTRL0);
	ldo_slp_ctrl1 = sci_adi_read(ANA_REG_GLB_PWR_SLP_CTRL1);
	ldo_slp_ctrl2 = sci_adi_read(ANA_REG_GLB_PWR_SLP_CTRL2);
	ldo_slp_ctrl3 = sci_adi_read(ANA_REG_GLB_PWR_SLP_CTRL3);
#endif
#endif

#if defined(CONFIG_ARCH_SCX15)
#else
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	ldo_aud_ctrl4 = sci_adi_read(ANA_REG_GLB_AUD_SLP_CTRL4);
#else
#if defined(CONFIG_ADIE_SC2723S)
	ldo_aud_ctrl4 = sci_adi_read(ANA_REG_GLB_AUD_SLP_CTRL);
#endif
#endif
#endif
	xtl_wait_ctrl = sci_adi_read(ANA_REG_GLB_XTL_WAIT_CTRL);
#if defined(CONFIG_ARCH_SCX15)
	ldo_dcdc_pd_ctrl = sci_adi_read(ANA_REG_GLB_LDO_DCDC_PD);
#endif
	ldo_pd_ctrl = sci_adi_read(ANA_REG_GLB_LDO_PD_CTRL);
	arm_module_en = sci_adi_read(ANA_REG_GLB_ARM_MODULE_EN);
}
void print_last_reg(void)
{
	printk("aon pmu status reg\n");
	printk("REG_PMU_APB_PD_PUB_SYS_CFG ------ 0x%08x\n", pd_pub_sys);
	printk("REG_PMU_APB_PD_MM_TOP_CFG ------ 0x%08x\n",
			sci_glb_read(REG_PMU_APB_PD_MM_TOP_CFG, -1UL) );
	printk("REG_PMU_APB_PD_AP_SYS_CFG ------ 0x%08x\n",
			sci_glb_read(REG_PMU_APB_PD_AP_SYS_CFG, -1UL) );
	printk("REG_PMU_APB_PD_AP_DISP_CFG ------ 0x%08x\n",
			sci_glb_read(REG_PMU_APB_PD_AP_DISP_CFG, -1UL) );

#if defined(CONFIG_ARCH_SCX30G)
	printk("REG_PMU_APB_PD_DDR_PUBL_CFG ------ 0x%08x\n",
			sci_glb_read(REG_PMU_APB_PD_DDR_PUBL_CFG, -1UL) );
	printk("REG_PMU_APB_PD_DDR_PHY_CFG ------ 0x%08x\n",
			sci_glb_read(REG_PMU_APB_PD_DDR_PHY_CFG, -1UL) );
#endif
	printk("REG_PMU_APB_CP_SLP_STATUS_DBG0 ----- 0x%08x\n", cp_slp_status_dbg0);
	printk("REG_PMU_APB_CP_SLP_STATUS_DBG1 ----- 0x%08x\n", cp_slp_status_dbg1);
	printk("REG_PMU_APB_PWR_STATUS0_DBG ----- 0x%08x\n", pwr_stat0);
	printk("REG_PMU_APB_PWR_STATUS1_DBG ----- 0x%08x\n", pwr_stat1);
	printk("REG_PMU_APB_PWR_STATUS2_DBG ----- 0x%08x\n", pwr_stat2);
	printk("REG_PMU_APB_PWR_STATUS3_DBG ----- 0x%08x\n", pwr_stat3);
	printk("REG_PMU_APB_SLEEP_CTRL ----- 0x%08x\n", sleep_ctrl);
	printk("REG_PMU_APB_DDR_SLEEP_CTRL ----- 0x%08x\n", ddr_sleep_ctrl);
	printk("REG_PMU_APB_SLEEP_STATUS ----- 0x%08x\n", sleep_status);
#if defined(CONFIG_ARCH_SCX15)
	printk("REG_PMU_APB_XTL0_REL_CFG ----- 0x%08x\n", xtl0_rel_cfg);
	printk("REG_PMU_APB_XTL1_REL_CFG ----- 0x%08x\n", xtl1_rel_cfg);
	printk("REG_PMU_APB_XTL2_REL_CFG ----- 0x%08x\n", xtl2_rel_cfg);
	printk("REG_PMU_APB_XTLBUF0_REL_CFG ----- 0x%08x\n", xtlbuf0_rel_cfg);
	printk("REG_PMU_APB_XTLBUF1_REL_CFG ----- 0x%08x\n", xtlbuf1_rel_cfg);
	printk("REG_PMU_APB_MPLL_REL_CFG ----- 0x%08x\n", mpll_rel_cfg);
	printk("REG_PMU_APB_DPLL_REL_CFG ----- 0x%08x\n", dpll_rel_cfg);
	printk("REG_PMU_APB_TDPLL_REL_CFG ----- 0x%08x\n", tdpll_rel_cfg);
	printk("REG_PMU_APB_WPLL_REL_CFG ----- 0x%08x\n", wpll_rel_cfg);
	printk("REG_PMU_APB_CPLL_REL_CFG ----- 0x%08x\n", cpll_rel_cfg);
	printk("REG_PMU_APB_WIFIPLL1_REL_CFG ----- 0x%08x\n", wifipll1_rel_cfg);
	printk("REG_PMU_APB_WIFIPLL2_REL_CFG ----- 0x%08x\n", wifipll2_rel_cfg);
	printk("REG_PMU_APB_DDR_CHN_SLEEP_CTRL0 ----- 0x%08x\n", ddr_chn_sleep_ctrl0);
	printk("REG_PMU_APB_DDR_CHN_SLEEP_CTRL1 ----- 0x%08x\n", ddr_chn_sleep_ctrl1);
#endif

	printk("aon apb reg\n");
	printk("REG_AON_APB_APB_EB0  ----- 0x%08x\n", apb_eb0);
	printk("REG_AON_APB_APB_EB1  ----- 0x%08x\n", apb_eb1);
	printk("REG_AON_APB_PWR_CTRL ----- 0x%08x\n", pwr_ctrl);
	printk("REG_AON_APB_BB_BG_CTRL ------ 0x%08x\n",
			sci_glb_read(REG_AON_APB_BB_BG_CTRL, -1UL) );

	printk("ap ahb reg \n");
	printk("REG_AP_AHB_AHB_EB ----- 0x%08x\n", ahb_eb);

	printk("ap apb reg\n");
	printk("REG_AP_APB_APB_EB ---- 0x%08x\n", apb_eb);
	printk("REG_AP_AHB_MCU_PAUSE ---   0x%08x\n", mcu_pause);
	printk("REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG --- 0x%08x\n", sys_force_sleep);
	printk("REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG ---- 0x%08x\n", sys_auto_sleep_cfg);
	printk("REG_AP_AHB_CA7_STANDBY_STATUS ---- 0x%08x\n", ca7_standby_status);

	printk("ana reg \n");
	printk("ANA_REG_GLB_LDO_SLP_CTRL0 --- 0x%08x\n", ldo_slp_ctrl0);
	printk("ANA_REG_GLB_LDO_SLP_CTRL1 --- 0x%08x\n", ldo_slp_ctrl1);
	printk("ANA_REG_GLB_LDO_SLP_CTRL2 --- 0x%08x\n", ldo_slp_ctrl2);
	printk("ANA_REG_GLB_LDO_SLP_CTRL3 --- 0x%08x\n", ldo_slp_ctrl3);
	printk("ANA_REG_GLB_AUD_SLP_CTRL4 --- 0x%08x\n", ldo_aud_ctrl4);
	printk("ANA_REG_GLB_XTL_WAIT_CTRL --- 0x%08x\n", xtl_wait_ctrl);

	printk("ANA_REG_GLB_PWR_XTL_EN0 -- 0x%08x\n", sci_adi_read(ANA_REG_GLB_PWR_XTL_EN0));
        printk("ANA_REG_GLB_PWR_XTL_EN1 -- 0x%08x\n", sci_adi_read(ANA_REG_GLB_PWR_XTL_EN1));
        printk("ANA_REG_GLB_PWR_XTL_EN2 -- 0x%08x\n", sci_adi_read(ANA_REG_GLB_PWR_XTL_EN2));
        printk("ANA_REG_GLB_PWR_XTL_EN3 -- 0x%08x\n", sci_adi_read(ANA_REG_GLB_PWR_XTL_EN3));
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	printk("ANA_REG_GLB_PWR_XTL_EN4 -- 0x%08x\n", sci_adi_read(ANA_REG_GLB_PWR_XTL_EN4));
#if defined(CONFIG_ARCH_SCX15)
#else
	printk("ANA_REG_GLB_PWR_XTL_EN5 -- 0x%08x\n", sci_adi_read(ANA_REG_GLB_PWR_XTL_EN5));
#endif
#endif
	printk("mm reg\n");
	printk("REG_MM_AHB_AHB_EB ---- 0x%08x\n", mm_apb);

	printk("ana reg\n");
#if defined(CONFIG_ARCH_SCX15)
	printk("ANA_REG_GLB_LDO_DCDC_PD --- 0x%08x\n", ldo_dcdc_pd_ctrl);
#endif
	printk("ANA_REG_GLB_LDO_PD_CTRL --- 0x%08x\n", ldo_pd_ctrl);
	printk("ANA_REG_GLB_ARM_MODULE_EN --- 0x%08x\n", arm_module_en);
}
#if 0
static void test_setup_timer(int load)
{
	//AP_TMR0_EB
	sci_glb_set(REG_AON_APB_APB_EB0, BIT(12));

	//AP_TMR0_RTC_EB
	sci_glb_set(REG_AON_APB_APB_RTC_EB, BIT(5));

	//load value
	sci_glb_write(SPRD_APTIMER0_BASE, load, -1UL);

	//int en
	sci_glb_set(SPRD_APTIMER0_BASE + 0xc, BIT(0));


	//AP_TMR0_RTC_EB --- TIMER_INT_EN   timer0 INTC0 en
	sci_glb_set(SPRD_INT_BASE + 0x8, BIT(2) | BIT(29));

	//int en
	sci_glb_set(SPRD_APTIMER0_BASE + 0x8, BIT(7));

	sci_glb_write(SPRD_INT_BASE + 0x8, 0xffffffff, -1UL);
	return;
}
#endif
void check_ldo(void)
{
}

void check_pd(void)
{
}

unsigned int sprd_irq_pending(void)
{
	uint32_t bits = 0;
	bits = sci_glb_read(SPRD_INT_BASE, -1UL);
	if(bits)
		return 1;
	else
		return 0;
}

/*copy code for deepsleep return */
#define SAVED_VECTOR_SIZE 64
static uint32_t *sp_pm_reset_vector = NULL;
static uint32_t saved_vector[SAVED_VECTOR_SIZE];
void __iomem *iram_start;
struct emc_repower_param *repower_param;


#define SPRD_RESET_VECTORS 0X00000000
#define SLEEP_RESUME_CODE_PHYS 	0X400
#define SLEEP_CODE_SIZE 0x800

static int init_reset_vector(void)
{
	sp_pm_reset_vector = (u32 *)SPRD_IRAM0_BASE;

	iram_start = (void __iomem *)(SPRD_IRAM0_BASE + SLEEP_RESUME_CODE_PHYS);
	/* copy sleep code to (IRAM). */
	if ((sc8830_standby_iram_end - sc8830_standby_iram) > SLEEP_CODE_SIZE) {
		panic("##: code size is larger than expected, need more memory!\n");
	}
	memcpy_toio(iram_start, sc8830_standby_iram, SLEEP_CODE_SIZE);


	/* just make sure*/
	flush_cache_all();
	outer_flush_all();
	return 0;
}
void save_reset_vector(void)
{
	int i = 0;
	for (i = 0; i < SAVED_VECTOR_SIZE; i++)
		saved_vector[i] = sp_pm_reset_vector[i];
}

void set_reset_vector(void)
{
	int i = 0;
	printk("SAVED_VECTOR_SIZE 0x%x\n", SAVED_VECTOR_SIZE);
	for (i = 0; i < SAVED_VECTOR_SIZE; i++)
		sp_pm_reset_vector[i] = 0xe320f000; /* nop*/

	sp_pm_reset_vector[SAVED_VECTOR_SIZE - 2] = 0xE51FF004; /* ldr pc, 4 */

	sp_pm_reset_vector[SAVED_VECTOR_SIZE - 1] = (sc8830_standby_exit_iram -
		sc8830_standby_iram + SLEEP_RESUME_CODE_PHYS); /* place v7_standby_iram here */
}

#ifdef CONFIG_DDR_VALIDITY_TEST
#include <linux/slab.h>
static unsigned long *vtest = NULL;
static unsigned long *ptest = NULL;
static void test_memory(void)
{
	int i;
#ifndef CONFIG_ARCH_SCX30G
	vtest = kmalloc(64*1024, GFP_KERNEL);
	if (vtest) {
		for (i = 0; i < (64*1024/4); i++) {
			vtest[i] = 0x12345678;
		}
#else
	vtest = kmalloc(2 * sizeof(int), GFP_KERNEL);
	if (vtest) {
		vtest[0] = 0x12345678;
#endif
		printk("%s %p %p\n", __func__, vtest, virt_to_phys(vtest));
		ptest = virt_to_phys(vtest);
	} else {
		printk("error kmalloc\n");
	}
	sp_pm_reset_vector[64] = ptest;
}
#endif

void restore_reset_vector(void)
{
	int i;
	for (i = 0; i < SAVED_VECTOR_SIZE; i++)
		sp_pm_reset_vector[i] = saved_vector[i];
}
/* irq functions */
#define hw_raw_irqs_disabled_flags(flags)	\
({						\
	(int)((flags) & PSR_I_BIT);		\
})

#define hw_irqs_disabled()			\
({						\
	unsigned long _flags;			\
	local_irq_save(_flags);			\
	hw_raw_irqs_disabled_flags(_flags);	\
})
u32 __attribute__ ((naked)) read_cpsr(void)
{
	__asm__ __volatile__("mrs r0, cpsr\nbx lr");
}
/* make sure printk is end, if not maybe some messy code  in SERIAL1 output */
#define UART_TRANSFER_REALLY_OVER (0x1UL << 15)
#define UART_STS0 (SPRD_UART1_BASE + 0x08)
#define UART_STS1 (SPRD_UART1_BASE + 0x0c)

static __used void wait_until_uart1_tx_done(void)
{
	u32 tx_fifo_val;
	u32 really_done = 0;
	u32 timeout = 2000;

	tx_fifo_val = __raw_readl(UART_STS1);
	tx_fifo_val >>= 8;
	tx_fifo_val &= 0xff;
	while(tx_fifo_val != 0) {
		if (timeout <= 0) break;
		udelay(100);
		tx_fifo_val = __raw_readl(UART_STS1);
		tx_fifo_val >>= 8;
		tx_fifo_val &= 0xff;
		timeout--;
	}

	timeout = 30;
	really_done = __raw_readl(UART_STS0);
	while(!(really_done & UART_TRANSFER_REALLY_OVER)) {
		if (timeout <= 0) break;
		udelay(100);
		really_done = __raw_readl(UART_STS0);
		timeout--;
	}
}
#define SAVE_GLOBAL_REG do{ \
	bak_restore_apb(1); \
	bak_ap_clk_reg(1); \
	bak_restore_ahb(1); \
	bak_restore_aon(1); \
	bak_restore_pub(1); \
	bak_restore_ap_intc(1); \
	}while(0)
#define RESTORE_GLOBAL_REG do{ \
	bak_restore_apb(0); \
	bak_ap_clk_reg(0); \
	bak_restore_aon(0); \
	bak_restore_ahb(0); \
	bak_restore_pub(0); \
	bak_restore_ap_intc(0); \
	}while(0)

/* arm core sleep*/
static void arm_sleep(void)
{
	cpu_do_idle();
	hard_irq_set();
}

/* arm core + arm sys */
static void mcu_sleep(void)
{
	SAVE_GLOBAL_REG;
	sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_SYS_SLEEP_EN);
	cpu_do_idle();
	sci_glb_clr(REG_AP_AHB_MCU_PAUSE, BIT_MCU_SYS_SLEEP_EN);
	hard_irq_set();
	RESTORE_GLOBAL_REG;
}

static void light_sleep(void)
{
	sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_LIGHT_SLEEP_EN);
	cpu_do_idle();
	sci_glb_clr(REG_AP_AHB_MCU_PAUSE, BIT_MCU_LIGHT_SLEEP_EN);
}
void int_work_round(void)
{
	//sci_glb_set(SPRD_AONAPB_BASE + 0x8, (0x1<<30 | 1<<8 /*| 1<<1*/));
	//sci_glb_clr(SPRD_LPDDR2_PHY_BASE + 0x2c, BIT(4));
	//sci_glb_set(SPRD_LPDDR2_PHY_BASE + 0x2c, BIT(4));
	//sci_glb_write(SPRD_LPDDR2_BASE + 0x30, 0, -1UL);
	//sci_glb_set(SPRD_PMU_BASE + 0xa8, 1<<1);
	//sci_adi_clr(ANA_EIC_BASE + 0x18, 0x20);
}
void show_deep_reg_status(void)
{
	printk("PWR_STATUS0_DBG	0x%08x\n", sci_glb_read(REG_PMU_APB_PWR_STATUS0_DBG, -1UL));
	printk("PWR_STATUS1_DBG	0x%08x\n", sci_glb_read(REG_PMU_APB_PWR_STATUS1_DBG, -1UL));
	printk("PWR_STATUS2_DBG	0x%08x\n", sci_glb_read(REG_PMU_APB_PWR_STATUS2_DBG, -1UL));
	printk("PWR_STATUS3_DBG	0x%08x\n", sci_glb_read(REG_PMU_APB_PWR_STATUS3_DBG, -1UL));

	printk("ANA_REG_GLB_SWRST_CTRL 0x%08x = 0x%08x\n", ANA_REG_GLB_SWRST_CTRL, sci_adi_read(ANA_REG_GLB_SWRST_CTRL));
	printk("ANA_REG_GLB_POR_SRC_FLAG 0x%08x = 0x%08x\n", ANA_REG_GLB_POR_SRC_FLAG, sci_adi_read(ANA_REG_GLB_POR_SRC_FLAG));
	printk("ANA_REG_GLB_POR_7S_CTRL 0x%08x = 0x%08x\n", ANA_REG_GLB_POR_7S_CTRL, sci_adi_read(ANA_REG_GLB_POR_7S_CTRL));
}


#if defined(CONFIG_MACH_SC9620OPENPHONE)
void cp0_sys_power_domain_close(void)
{
	sci_glb_set(REG_PMU_APB_PD_CP0_SYS_CFG, BIT_CP0_FORCE_DEEP_SLEEP);//enable cp0 force sleep
}

void cp0_sys_power_domain_open(void)
{
	sci_glb_set(REG_PMU_APB_CP_SOFT_RST, BIT_CP0_SOFT_RST);//reset cp0
	sci_glb_clr(REG_PMU_APB_PD_CP0_SYS_CFG, BIT_CP0_FORCE_DEEP_SLEEP);//close cp0 force sleep
	mdelay(2);
	sci_glb_clr(REG_PMU_APB_CP_SOFT_RST, BIT_CP0_SOFT_RST);//release cp0
}
#endif

extern void pm_debug_set_wakeup_timer(void);
extern void pm_debug_set_apwdt(void);
int deep_sleep(int from_idle)
{
	int ret = 0;
	static unsigned int cnt = 0;

	if(!from_idle){
		SAVE_GLOBAL_REG;
		/* some prepare here */
#if defined(CONFIG_ARCH_SCX15)
		bak_restore_mm_scx15(1);
#endif
		show_pin_reg();
		enable_mcu_deep_sleep();
		configure_for_deepsleep(0);
		disable_ahb_module();
	    //disable_pmu_ddr_module();
	    
		//sci_glb_set(SPRD_PMU_BASE+0x00F4, 0x3FF);
		
		disable_dma();
		//disable_mm();
		//disable_ana_module();
		disable_aon_module();
		show_reg_status();
		bak_last_reg();
		print_last_reg();
		print_int_status();
		//wait_until_uart1_tx_done();
		int_work_round();
		//pm_debug_set_apwdt();
		disable_apb_module();
		//pm_debug_set_wakeup_timer();
		//force_mcu_core_sleep();
		//bak_last_reg();

		__raw_writel(0x0, REG_PMU_APB_CA7_C0_CFG);
		show_deep_reg_status();
	} else {
		/* __raw_writel(0x0, REG_PMU_APB_CA7_C0_CFG); */
	}
	/*
	 * sp_pm_collapse(param0, param1)
	 * param0, param1 are not used before
	 * so we use second param to distinguish idle deep or real deep
	 */

#define dmc_retention_func	(SPRD_IRAM1_BASE + 0x1800)
#define dmc_retention_para_vaddr	(SPRD_IRAM1_BASE + 0x2400)

#if defined(CONFIG_ARCH_SCX30G)
	/* set auto-self refresh mode */
	sci_glb_clr(SPRD_LPDDR2_BASE+0x30, BIT(1));
	sci_glb_set(SPRD_LPDDR2_BASE+0x30, BIT(0));
	((void (*)(unsigned long,unsigned long))(dmc_retention_func))(1,dmc_retention_para_vaddr);
#endif
#if defined(CONFIG_MACH_SC9620OPENPHONE)
        cp0_sys_power_domain_close();
#endif

#define SLEEP_MAGIC (SPRD_IRAM0_BASE + 0xFB0)
#if defined(CONFIG_ADIE_SC2723S) || defined(CONFIG_ADIE_SC2723)
	sci_adi_write(ANA_REG_GLB_DCDC_GEN_ADI,0x01b0,0xffff);
#endif
    __raw_writel(0x12345678, SLEEP_MAGIC); 
	ret = sp_pm_collapse(0, from_idle);
    __raw_writel(0xaaaaaaaa, SLEEP_MAGIC); 
#if defined(CONFIG_ADIE_SC2723S) || defined(CONFIG_ADIE_SC2723)
	sci_adi_write(ANA_REG_GLB_DCDC_GEN_ADI,0x01e0,0xffff);
#endif

#if defined(CONFIG_MACH_SC9620OPENPHONE)
        cp0_sys_power_domain_open();
#endif

#if defined(CONFIG_ARCH_SCX30G)
	/* set auto powerdown mode */
	sci_glb_set(SPRD_LPDDR2_BASE+0x30, BIT(1));
	sci_glb_clr(SPRD_LPDDR2_BASE+0x30, BIT(0));
#endif

	udelay(50);


	if(!from_idle){
		__raw_writel(0x1, REG_PMU_APB_CA7_C0_CFG);
		printk("ret %d not from idle\n", ret);
		if(ret){
#if defined(CONFIG_DDR_VALIDITY_TEST) && defined(CONFIG_ARCH_SCX30G)
			printk("DDR test: read times: %ld\n", *(vtest + 1));
#endif
			printk("deep sleep %u times\n", cnt);
			cnt++;
		}
		sci_glb_set(REG_AP_APB_APB_EB, 0xf<<19);
		hard_irq_set();
		sci_glb_clr(REG_AP_APB_APB_EB, 0xf<<19);
		disable_mcu_deep_sleep();
		configure_for_deepsleep(1);
		//sci_glb_clr(SPRD_PMU_BASE+0x00F4, 0x3FF);
		RESTORE_GLOBAL_REG;
#if defined(CONFIG_ARCH_SCX15)
		bak_restore_mm_scx15(0);
#endif
	} else {
		/* __raw_writel(0x1, REG_PMU_APB_CA7_C0_CFG); */
		pr_debug("ret %d  from idle\n", ret);
	}

	udelay(5);
	if (ret) cpu_init();

	if (soc_is_scx35_v0())
		sci_glb_clr(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG,BIT_CA7_CORE_AUTO_GATE_EN);

	return ret;
}

#define DEVICE_AHB              (0x1UL << 20)
#define DEVICE_APB              (0x1UL << 21)
#define DEVICE_VIR              (0x1UL << 22)
#define DEVICE_AWAKE            (0x1UL << 23)
#define LIGHT_SLEEP            (0x1UL << 24)
#define DEVICE_TEYP_MASK        (DEVICE_AHB | DEVICE_APB | DEVICE_VIR | DEVICE_AWAKE | LIGHT_SLEEP)
int sprd_cpu_deep_sleep(unsigned int cpu)
{
	int status, ret = 0;
	unsigned long flags, time;

#ifdef CONFIG_SPRD_PM_DEBUG
	__raw_writel(0xfdffbfff, SPRD_INTC0_BASE + 0xc);//intc0
	__raw_writel(0x02004000, SPRD_INTC0_BASE + 0x8);//intc0
	__raw_writel(0xffffffff, SPRD_INTC0_BASE + 0x100c);//intc1
#endif	


	time = get_sys_cnt();
	if (!hw_irqs_disabled())  {
		flags = read_cpsr();
		printk("##: Error(%s): IRQ is enabled(%08lx)!\n",
			 "wakelock_suspend", flags);
	}
	/*TODO:
	* we need to known clock status in modem side
	*/
#ifdef FORCE_DISABLE_DSP
	status = 0;
#else
	/*
	* TODO: get clock status in native version, force deep sleep now
	*/
	status = 0;
#endif
	if (status & DEVICE_AHB)  {
		printk("###### %s,  DEVICE_AHB ###\n", __func__ );
		set_sleep_mode(SLP_MODE_ARM);
		arm_sleep();
	} else if (status & DEVICE_APB) {
		printk("###### %s,	DEVICE_APB ###\n", __func__ );
		set_sleep_mode(SLP_MODE_MCU);
		mcu_sleep();
	} else if (status & LIGHT_SLEEP) {
		printk("###### %s,	DEVICE_APB ###\n", __func__ );
		set_sleep_mode(SLP_MODE_LIT);
		light_sleep();
	} else {
		/*printk("###### %s,	DEEP ###\n", __func__ );*/
		set_sleep_mode(SLP_MODE_DEP);
#if 1

		ret = deep_sleep(0);
#else
		//pm_debug_set_wakeup_timer();
		ret = 0;
		arm_sleep();
#endif
		flush_cache_all();
	}
	
	time_add(get_sys_cnt() - time, ret);
	print_hard_irq_inloop(ret);

	return ret;
}
static void init_gr(void){}
void sc_default_idle(void)
{
	cpu_do_idle();
	local_irq_enable();
	return;
}

/*config dcdc core deep sleep voltage*/
static void dcdc_core_ds_config(void)
{
	u32 dcdc_core_ctl_adi = 0;
	u32 val = 0;
	u32 dcdc_core_ctl_ds = -1;

#if defined(CONFIG_ADIE_SC2723S) || defined(CONFIG_ADIE_SC2723)
	unsigned int reg_val,reg_val1;
	reg_val =(sci_adi_read(ANA_REG_GLB_CHIP_ID_HIGH) & 0xFFFF) << 16;
	reg_val |= (sci_adi_read(ANA_REG_GLB_CHIP_ID_LOW) & 0xFFFF); 

	if (reg_val == 0x2723a000) {
		reg_val = sci_adi_read(ANA_REG_GLB_DCDC_CORE_ADI);
		reg_val1 = (reg_val & 0x1F) << 5;
		reg_val1 |=(reg_val >>5) & 0x1f;
		sci_adi_write(ANA_REG_GLB_DCDC_SLP_CTRL1, reg_val1, 0xFFFF);
		sci_adi_write(ANA_REG_GLB_DCDC_SLP_CTRL0, 0, 0x2);

		printk("ANA_REG_GLB_DCDC_SLP_CTRL0 = 0x%08x\n", sci_adi_read(ANA_REG_GLB_DCDC_SLP_CTRL0));
		printk("ANA_REG_GLB_DCDC_SLP_CTRL1 = 0x%08x\n", sci_adi_read(ANA_REG_GLB_DCDC_SLP_CTRL1));
	}
#endif

#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
#ifdef CONFIG_ARCH_SCX30G
	static struct dcdc_core_ds_step_info step_info[5]={
		{ANA_REG_GLB_MP_PWR_CTRL1, 0,ANA_REG_GLB_MP_PWR_CTRL2, 0},
		{ANA_REG_GLB_MP_PWR_CTRL1, 3,ANA_REG_GLB_MP_PWR_CTRL2, 5},
		{ANA_REG_GLB_MP_PWR_CTRL1, 6,ANA_REG_GLB_MP_PWR_CTRL2,10},
		{ANA_REG_GLB_MP_PWR_CTRL1, 9,ANA_REG_GLB_MP_PWR_CTRL3, 0},
		{ANA_REG_GLB_MP_PWR_CTRL1,12,ANA_REG_GLB_MP_PWR_CTRL3, 5}
	};
	static u8 dcdc_core_down_volt[]={4,1,1,2,3,5,0,6};
	static u8 dcdc_core_up_volt[]={6,2,3,4,0,1,7,7};
	u32 dcdc_core_cal_adi,i;
	/*1100,700,800,900,1000,650,1200,1300*/
	static u32 step_ratio[]={10,10,6,3,3};
	dcdc_core_ctl_adi = (sci_adi_read(ANA_REG_GLB_MP_MISC_CTRL) >> 3) & 0x7;
	//dcdc_core_ctl_ds  = dcdc_core_down_volt[dcdc_core_ctl_adi];
	dcdc_core_ctl_ds  = dcdc_core_ctl_adi;
	printk("dcdc_core_ctl_adi = %d, dcdc_core_ctl_ds = %d\n",dcdc_core_ctl_adi,dcdc_core_ctl_ds);

#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	val = sci_adi_read(ANA_REG_GLB_DCDC_SLP_CTRL);
#else
#if defined(CONFIG_ADIE_SC2723S)
	val = sci_adi_read(ANA_REG_GLB_DCDC_SLP_CTRL0);
#endif
#endif
	val &= ~0x7;
	val |= dcdc_core_ctl_ds;
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	sci_adi_write(ANA_REG_GLB_DCDC_SLP_CTRL, val, 0xffff);
#else
#if defined(CONFIG_ADIE_SC2723S)
	sci_adi_write(ANA_REG_GLB_DCDC_SLP_CTRL0, val, 0xffff);
#endif
#endif

	if(dcdc_core_ctl_ds < dcdc_core_ctl_adi){
		dcdc_core_cal_adi = sci_adi_read(ANA_REG_GLB_DCDC_CORE_ADI) & 0x1F;
		/*last step must equel function mode */
		sci_adi_write(step_info[4].ctl_reg,dcdc_core_ctl_adi<<step_info[4].ctl_sht,0x07 << step_info[4].ctl_sht);
		sci_adi_write(step_info[4].cal_reg,dcdc_core_cal_adi<<step_info[4].cal_sht,0x1F << step_info[4].cal_sht);

		for(i=0;i<4;i++) {
			val = dcdc_core_cal_adi + step_ratio[i];
			if(val <= 0x1F) {
				sci_adi_write(step_info[i].ctl_reg,dcdc_core_ctl_ds<<step_info[i].ctl_sht,0x07<<step_info[i].ctl_sht);
				sci_adi_write(step_info[i].cal_reg,val<<step_info[i].cal_sht,0x1F << step_info[i].cal_sht);
				dcdc_core_cal_adi = val;
			} else {
				sci_adi_write(step_info[i].ctl_reg,dcdc_core_up_volt[dcdc_core_ctl_ds]<<step_info[i].ctl_sht,
												0x07 << step_info[i].ctl_sht);
				sci_adi_write(step_info[i].cal_reg,(val-0x1F)<<step_info[i].cal_sht,0x1F << step_info[i].cal_sht);
				dcdc_core_ctl_ds = dcdc_core_up_volt[dcdc_core_ctl_ds];
				dcdc_core_cal_adi = val - 0x1F;
			}
		}
	} else {
		dcdc_core_cal_adi = sci_adi_read(ANA_REG_GLB_DCDC_CORE_ADI) & 0x1F;
		for(i=0;i<5;i++){
			sci_adi_write(step_info[i].ctl_reg,dcdc_core_ctl_adi<<step_info[i].ctl_sht,0x07<<step_info[i].ctl_sht);
			sci_adi_write(step_info[i].cal_reg,dcdc_core_cal_adi<<step_info[i].cal_sht,0x1F<<step_info[i].cal_sht);
		}
	}
#else
	dcdc_core_ctl_adi = sci_adi_read(ANA_REG_GLB_DCDC_CORE_ADI);
	dcdc_core_ctl_adi = dcdc_core_ctl_adi >> 0x5;
	dcdc_core_ctl_adi = dcdc_core_ctl_adi & 0x7;
	/*only support dcdc_core_ctl_adi 0.9v,1.0v, 1.1v, 1.2v, 1.3v*/
	switch(dcdc_core_ctl_adi) {
	case 0://1.1v
		dcdc_core_ctl_ds = 0x4; //1.0v
		break;
	case 4: //1.0v
		dcdc_core_ctl_ds = 0x3;//0.9v
		break;
	case 6: //1.2v
		dcdc_core_ctl_ds = 0x0;//1.1v
		break;
	case 7: //1.3v
		dcdc_core_ctl_ds = 0x6;//1.2v
		break;
	case 3: //0.9v
		dcdc_core_ctl_ds = 0x2;//0.8v
		break;
	case 1: //0.7v
	case 2: //0.8v
	case 5: //0.65v
		//unvalid value
		break;
	}
	printk("dcdc_core_ctl_adi = %d, dcdc_core_ctl_ds = %d\n", dcdc_core_ctl_adi, dcdc_core_ctl_ds);
	/*valid value*/
	if(dcdc_core_ctl_ds != -1) {
#ifndef CONFIG_ARCH_SCX15
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
        val = sci_adi_read(ANA_REG_GLB_DCDC_SLP_CTRL);
#else
#if defined(CONFIG_ADIE_SC2723S)
        val = sci_adi_read(ANA_REG_GLB_DCDC_SLP_CTRL0);
#endif
#endif
		val &= ~(0x7);
		val |= dcdc_core_ctl_ds;
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
		sci_adi_write(ANA_REG_GLB_DCDC_SLP_CTRL, val, 0xffff);
#else
#if defined(CONFIG_ADIE_SC2723S)
		sci_adi_write(ANA_REG_GLB_DCDC_SLP_CTRL0, val, 0xffff);
#endif
#endif
#else
		val = sci_adi_read(ANA_REG_GLB_DCDC_SLP_CTRL0);
		val &= ~(0x7 << 4);
		val |= (dcdc_core_ctl_ds << 4);
		sci_adi_write(ANA_REG_GLB_DCDC_SLP_CTRL0, val, 0xffff);
#endif
	}
#endif
#endif
}

void pm_ana_ldo_config(void)
{
#if defined(CONFIG_ARCH_SCX15)
	dcdc_core_ds_config();
#else
	/*set vddcore deep sleep voltage to 0.9v*/
	//sci_adi_set(ANA_REG_GLB_DCDC_SLP_CTRL, BITS_DCDC_CORE_CTL_DS(3));
	dcdc_core_ds_config();
	/*open vddcore lp VDDMEM, DCDCGEN mode*/
	//sci_adi_set(ANA_REG_GLB_LDO_SLP_CTRL2, BIT_SLP_DCDCCORE_LP_EN | BIT_SLP_DCDCMEM_LP_EN | BIT_SLP_DCDCGEN_LP_EN);
	/*open vdd28, vdd18 lp mode*/
	//sci_adi_set(ANA_REG_GLB_LDO_SLP_CTRL3, BIT_SLP_LDOVDD28_LP_EN | BIT_SLP_LDOVDD18_LP_EN);
	/*ddr2_buf quiesent curretn set to 4-5uA in deep sleep mode*/
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	sci_adi_clr(ANA_REG_GLB_DDR2_CTRL, BITS_DDR2_BUF_S_DS(0x3));
#endif
#endif
}
static void init_led(void){}

void sprd_pbint_7s_reset_disable(void)
{
	sci_adi_set(ANA_REG_GLB_POR_7S_CTRL, BIT_PBINT_7S_RST_DISABLE);
}

void sprd_smpl_disable(void)
{
	sci_adi_set(ANA_REG_GLB_SMPL_CTRL0, 0x0);
}

#ifdef CONFIG_SPRD_EXT_IC_POWER
//    EXT IC should disable otg mode when power off
	extern void sprd_extic_otg_power(int enable);
#endif


static void sc8830_power_off(void)
{
	u32 reg_val;
	/*turn off all modules's ldo*/

#if defined(CONFIG_ADIE_SC2723S) || defined(CONFIG_ADIE_SC2723)
		  sprd_smpl_disable();
#endif

#ifdef CONFIG_SPRD_EXT_IC_POWER
	sprd_extic_otg_power(0);
#endif
	sci_adi_raw_write(ANA_REG_GLB_LDO_PD_CTRL, 0x1fff);

#if defined(CONFIG_ARCH_SCX15) || defined(CONFIG_ADIE_SC2723S) || defined(CONFIG_ADIE_SC2723)
	sci_adi_raw_write(ANA_REG_GLB_PWR_WR_PROT_VALUE,BITS_PWR_WR_PROT_VALUE(0x6e7f));
	do{
		reg_val = (sci_adi_read(ANA_REG_GLB_PWR_WR_PROT_VALUE) & BIT_PWR_WR_PROT);
	}while(reg_val == 0);
	sci_adi_raw_write(ANA_REG_GLB_LDO_PD_CTRL,0xfff);
	sci_adi_raw_write(ANA_REG_GLB_LDO_DCDC_PD,0x7fff);
#else
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	/*turn off system core's ldo*/
	sci_adi_raw_write(ANA_REG_GLB_LDO_DCDC_PD_RTCCLR, 0x0);
	sci_adi_raw_write(ANA_REG_GLB_LDO_DCDC_PD_RTCSET, 0X7fff);
#endif
#endif
	pr_emerg("%s\n", __func__);
	flush_cache_all();
	mdelay(10000);
	printk("power off failed!\n");
	BUG();
}

void sc8830_machine_restart(char mode, const char *cmd)
{
	local_irq_disable();
	local_fiq_disable();

	arch_reset(mode, cmd);

	mdelay(1000);

	printk("reboot failed!\n");

	while (1);
}

void __init sc_pm_init(void)
{
	
	init_reset_vector();
	pm_power_off   = sc8830_power_off;
	arm_pm_restart = sc8830_machine_restart;
	pr_info("power off %pf, restart %pf\n", pm_power_off, arm_pm_restart);
	init_gr();
	setup_autopd_mode();
	pm_ana_ldo_config();
	init_led();
	/* disable all sleep mode */
	sci_glb_clr(REG_AP_AHB_MCU_PAUSE, BIT_MCU_DEEP_SLEEP_EN | BIT_MCU_LIGHT_SLEEP_EN | \
		BIT_MCU_SYS_SLEEP_EN | BIT_MCU_CORE_SLEEP);
	set_reset_vector();
#ifdef CONFIG_DDR_VALIDITY_TEST
	test_memory();
#endif
#ifndef CONFIG_SPRD_PM_DEBUG
	pm_debug_init();
#endif

	/* enable arm clock auto gating*/
	//sci_glb_set(REG_AHB_AHB_CTL1, BIT_ARM_AUTO_GATE_EN);
#ifdef CONFIG_CPU_IDLE
	sc_cpuidle_init();
#endif
/*
	wake_lock_init(&pm_debug_lock, WAKE_LOCK_SUSPEND, "pm_not_ready");
	wake_lock(&pm_debug_lock);
*/
#if defined(CONFIG_SPRD_DEBUG)
	sprd_debug_init();
#endif
}
