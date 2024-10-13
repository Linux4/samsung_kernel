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
/*#include <asm/hardware/cache-l2x0.h>*/
#include <asm/cacheflush.h>
#include <soc/sprd/system.h>
#include <soc/sprd/pm_debug.h>
#include <soc/sprd/common.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/sci_glb_regs.h>
/*#include <mach/irqs.h>*/
#include <soc/sprd/sci.h>
/*#include "emc_repower.h"*/
#include <linux/clockchips.h>
#include <linux/wakelock.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/arch_misc.h>
#include <asm/suspend.h>
#include <asm/barrier.h>
#include <linux/cpu_pm.h>
#if defined(CONFIG_ARCH_WHALE)
#include <soc/sprd/sprd_persistent_clock.h>
#endif
#if defined(CONFIG_SPRD_DEBUG)
/* For saving Fault status */
#include <soc/sprd/sprd_debug.h>
#endif

extern void (*arm_pm_restart)(char str, const char *cmd);
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
	u32 ca7_div_cfg;
	u32 misc_ckg_en;
	u32 misc_cfg;
	u32 usb_phy_tune;
};
struct ap_clk_reg_bak {
	u32 ap_apb_cfg;
	u32 nandc_2x_cfg;
	u32 nandc_1x_cfg;
	u32 nandc_ecc_cfg;
	u32 otg2_utmi_cfg;
	u32 usb3_utmi_cfg;
	u32 usb3_pipe_cfg;
	u32 usb3_ref_cfg;
	u32 hsic_utmi_cfg;
	u32 zipenc_cfg;
	u32 zipdec_cfg;
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
	u32 i2c5_cfg;
	u32 spi0_cfg;
	u32 spi1_cfg;
	u32 spi2_cfg;
	u32 iis0_cfg;
	u32 iis1_cfg;
	u32 iis2_cfg;
	u32 iis3_cfg;
	u32 bist_192m_cfg;
	u32 bist_256m_cfg;
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

/* bits definition
 * bit_0 : ca7 top
 * bit_1 : ca7 c0
 * bit_2 : pub
 * bit_3 : mm
 * bit_4 : ap sys
 * bit_5 : dcdc arm
*/
static  struct auto_pd_en pd_config = {
	0x6a6aa6a6, 0x3f, 0xa6a66a6a,
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
	return ;
}

static void setup_autopd_mode(void)
{
	if (soc_is_scx35_v0())
		sci_glb_set(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, 0x1);
	else
		sci_glb_set(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, 0x1);

#if defined(CONFIG_ARCH_WHALE)

	sci_glb_set(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, BIT_AP_AHB_AP_EMC_AUTO_GATE_EN);
#else
	sci_glb_set(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, BIT_AP_EMC_AUTO_GATE_EN | BIT_CA7_EMC_AUTO_GATE_EN | BIT_CA7_CORE_AUTO_GATE_EN);
#endif
/* INTC0_EB, INTC1_EB, INTC2_EB, INTC3_EB, INTC4_EB, INTC5_EB*/
sci_glb_set(REG_AON_APB_APB_EB0, 0x1f<<17 | BIT(2));
#if defined(CONFIG_ARCH_WHALE)
	sci_glb_set(REG_PMU_APB_PD_CA53_TOP_CFG, BIT_PMU_APB_PD_CA53_TOP_AUTO_SHUTDOWN_EN);
#else
	sci_glb_set(REG_PMU_APB_PD_CA7_TOP_CFG, BIT_PD_CA7_TOP_AUTO_SHUTDOWN_EN);
#endif
#if defined(CONFIG_ARCH_WHALE)
	sci_glb_set(REG_PMU_APB_PD_CA53_LIT_C0_CFG, BIT_PMU_APB_PD_CA53_LIT_C0_AUTO_SHUTDOWN_EN);
#else
	sci_glb_set(REG_PMU_APB_PD_CA7_C0_CFG, BIT_PD_CA7_C0_AUTO_SHUTDOWN_EN);
#endif
	sci_glb_set(SPRD_INTC0_BASE + 0x8, BIT(24) | BIT(30) | BIT(31)); /*ana & i2c & adi*/
	sci_glb_set(SPRD_INTC1_BASE + 0x8, BIT(18) | BIT(19)); /*kpd & gpio*/
	/* Temporarily put it in here for AON Timer can wakeup cluster gating */
	sci_glb_set(SPRD_INTC1_BASE + 0x8, 0x3f<<21); /*tmr*/
	sci_glb_set(SPRD_INTC0_BASE + 0x8, BIT(26) | BIT(27)); /*tmr*/
	/*sci_adi_set(ANA_REG_GLB_LDO_SLP_CTRL0, BIT_SLP_IO_EN | BIT_SLP_LDORF0_PD_EN | BIT_SLP_LDORF1_PD_EN | BIT_SLP_LDORF2_PD_EN);*/
	/*sci_adi_clr(ANA_REG_GLB_LDO_SLP_CTRL0, BIT_SLP_IO_EN | BIT_SLP_LDORF0_PD_EN | BIT_SLP_LDORF1_PD_EN | BIT_SLP_LDORF2_PD_EN);*/

	/*sci_adi_set(ANA_REG_GLB_LDO_SLP_CTRL1, BIT_SLP_LDO_PD_EN);*/
	/*sci_adi_set(ANA_REG_GLB_XTL_WAIT_CTRL, BIT_SLP_XTLBUF_PD_EN);*/
	/*sci_adi_set(ANA_REG_GLB_LDO_SLP_CTRL2, BIT_SLP_LDORF2_LP_EN | BIT_SLP_LDORF1_LP_EN | BIT_SLP_LDORF0_LP_EN);*/

	/*sci_adi_clr(ANA_REG_GLB_LDO_SLP_CTRL2, BIT_SLP_LDORF2_LP_EN | BIT_SLP_LDORF1_LP_EN | BIT_SLP_LDORF0_LP_EN);*/

	/*sci_glb_set(REG_PMU_APB_DDR_SLEEP_CTRL, 7<<4);*/
	/*sci_glb_set(REG_PMU_APB_DDR_SLEEP_CTRL, 5<<4);*/

	if (soc_is_scx35_v1()) {
		sci_glb_set(REG_PMU_APB_PD_PUB0_SYS_CFG, BIT_PMU_APB_PD_PUB0_SYS_AUTO_SHUTDOWN_EN);
		sci_glb_set(REG_PMU_APB_PD_PUB1_SYS_CFG, BIT_PMU_APB_PD_PUB1_SYS_AUTO_SHUTDOWN_EN);
	}
#if !(defined(CONFIG_ARCH_SCX30G) || defined(CONFIG_ARCH_SCX35L))
	else {
		sci_glb_clr(REG_PMU_APB_PD_PUB_SYS_CFG, BIT_PD_PUB_SYS_AUTO_SHUTDOWN_EN);
	}
#endif

#if defined(CONFIG_SCX35_DMC_FREQ_DDR3)
	sci_glb_clr(REG_PMU_APB_PD_PUB_SYS_CFG, BIT_PD_PUB_SYS_AUTO_SHUTDOWN_EN);
#endif

	/*sci_glb_write(REG_PMU_APB_AP_WAKEUP_POR_CFG, 0x1, -1UL);*/
	/* KEEP eMMC/SD power */
#if defined(CONFIG_ADIE_SC2723)
	sci_adi_clr(ANA_REG_GLB_PWR_SLP_CTRL0, BIT_SLP_LDOEMMCCORE_PD_EN);
	sci_adi_clr(ANA_REG_GLB_PWR_SLP_CTRL1, BIT_SLP_LDOSDCORE_PD_EN | BIT_SLP_LDOSDIO_PD_EN);
#elif defined(CONFIG_ADIE_SC2723S)
	sci_adi_clr(ANA_REG_GLB_PWR_SLP_CTRL0, BIT_SLP_LDOEMMCCORE_PD_EN); /* | BIT_SLP_LDOEMMCIO_PD_EN);*/
	sci_adi_clr(ANA_REG_GLB_PWR_SLP_CTRL1, BIT_SLP_LDOSDIO_PD_EN);
#else
	/*sci_adi_clr(ANA_REG_GLB_LDO_SLP_CTRL0, BIT_SLP_LDOEMMCCORE_PD_EN | BIT_SLP_LDOEMMCIO_PD_EN);*/
	/*sci_adi_clr(ANA_REG_GLB_LDO_SLP_CTRL1, BIT_SLP_LDOSD_PD_EN );*/
#endif
	/*****************  for idle to deep  ****************/
	configure_for_deepsleep(1);
	return;
}
#if 0
void disable_mm(void)
{
	sci_glb_clr(REG_MM_AHB_GEN_CKG_CFG, BIT_JPG_AXI_CKG_EN | BIT_VSP_AXI_CKG_EN |\
		BIT_ISP_AXI_CKG_EN | BIT_DCAM_AXI_CKG_EN | BIT_SENSOR_CKG_EN | BIT_MIPI_CSI_CKG_EN | BIT_CPHY_CFG_CKG_EN);
	sci_glb_clr(REG_MM_AHB_AHB_EB, BIT_JPG_EB | BIT_CSI_EB | BIT_VSP_EB | BIT_ISP_EB | BIT_CCIR_EB | BIT_DCAM_EB);
	sci_glb_clr(REG_MM_AHB_GEN_CKG_CFG, BIT_MM_AXI_CKG_EN);
	sci_glb_clr(REG_MM_AHB_GEN_CKG_CFG, BIT_MM_MTX_AXI_CKG_EN);
	sci_glb_clr(REG_MM_AHB_AHB_EB, BIT_MM_CKG_EB);
	sci_glb_clr(REG_PMU_APB_PD_MM_TOP_CFG, BIT_PD_MM_TOP_AUTO_SHUTDOWN_EN);
	sci_glb_clr(REG_AON_APB_APB_EB0, BIT_MM_EB);
	sci_glb_set(REG_PMU_APB_PD_MM_TOP_CFG, BIT_PD_MM_TOP_AUTO_SHUTDOWN_EN);
	return;
}
void bak_restore_mm(int bak)
{
	static uint32_t gen_ckg, ahb_eb, mm_top, aon_eb0;
	static uint32_t gen_ckg_en_mask = BIT_JPG_AXI_CKG_EN | BIT_VSP_AXI_CKG_EN |\
		BIT_ISP_AXI_CKG_EN | BIT_DCAM_AXI_CKG_EN | BIT_SENSOR_CKG_EN | BIT_MIPI_CSI_CKG_EN | BIT_CPHY_CFG_CKG_EN;
	static uint32_t ahb_eb_mask = BIT_JPG_EB | BIT_CSI_EB | BIT_VSP_EB | BIT_ISP_EB | BIT_CCIR_EB | BIT_DCAM_EB;
	if (bak) {
		gen_ckg = sci_glb_read(REG_MM_AHB_GEN_CKG_CFG, -1UL);
		ahb_eb = sci_glb_read(REG_MM_AHB_AHB_EB, -1UL);
		mm_top = sci_glb_read(REG_PMU_APB_PD_MM_TOP_CFG, -1UL);
		aon_eb0 = sci_glb_read(REG_AON_APB_APB_EB0, -1UL);
	} else {
		sci_glb_clr(REG_PMU_APB_PD_MM_TOP_CFG, BIT_PD_MM_TOP_AUTO_SHUTDOWN_EN);
		if (aon_eb0 & BIT_MM_EB)
			sci_glb_set(REG_AON_APB_APB_EB0, BIT_MM_EB);
		if (ahb_eb & BIT_MM_CKG_EB)
			sci_glb_set(REG_MM_AHB_AHB_EB, BIT_MM_CKG_EB);
		if (gen_ckg & BIT_MM_MTX_AXI_CKG_EN)
			sci_glb_set(REG_MM_AHB_GEN_CKG_CFG, BIT_MM_MTX_AXI_CKG_EN);
		if (gen_ckg & BIT_MM_AXI_CKG_EN)
			sci_glb_set(REG_MM_AHB_GEN_CKG_CFG, BIT_MM_AXI_CKG_EN);
		if (ahb_eb & ahb_eb_mask)
			sci_glb_write(REG_MM_AHB_AHB_EB, ahb_eb, ahb_eb_mask);
		if (gen_ckg & gen_ckg_en_mask)
			sci_glb_write(REG_MM_AHB_GEN_CKG_CFG, gen_ckg, gen_ckg_en_mask);
	}
	return;
}
#endif
void disable_dma(void)
{
	int reg = 0;
	sci_glb_set(REG_AP_AHB_AHB_EB, BIT_DMA_EB);
	reg = sci_glb_read(SPRD_DMA_BASE, BIT(2));
	int cnt = 30;
	if (reg) {
		sci_glb_set((SPRD_DMA_BASE), BIT(0)); /* pause dma*/
		while (cnt-- && reg) {
			sci_glb_read(SPRD_DMA_BASE, BIT(2));
			udelay(100);
			reg = sci_glb_read(SPRD_DMA_BASE, BIT(2));
		}
		if (reg)
			printk("disable dma failed\n");
	}
	sci_glb_clr(REG_AP_AHB_AHB_EB, BIT_DMA_EB);
	return;
}
void disable_ahb_module(void)
{
	/*disable ap_ahb,  BIT_AP_AHB_ZIPMTX_EB:need clk to run ddr fsm*/
	sci_glb_set(REG_AP_AHB_AHB_EB, BIT_AP_AHB_ZIPMTX_EB);
	/*AP_PERI_FORCE_ON CLR*/
	sci_glb_clr(REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG, BIT_AP_AHB_AP_PERI_FORCE_ON);
	/* AP_PERI_FORCE_SLP*/
	sci_glb_set(REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG, BIT_AP_AHB_AP_PERI_FORCE_SLP);
	/*AP_SYS_AUTO_SLEEP_CFG*/
	sci_glb_set(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG, BIT_AP_AHB_AP_AHB_AUTO_GATE_EN|BIT_AP_AHB_AP_EMC_AUTO_GATE_EN);
	/*AP_AHB_MISC_CFG  bit11:ap follow a53 stop enable   bit14:usb3 pmu power off enable bit12:ca53 sys top enable*/
	sci_glb_set(REG_AP_AHB_MISC_CFG	, BIT(12));
	return;
}
void bak_restore_ahb(int bak)
{
	volatile u32 i;
	if (bak) {
		ap_ahb_reg_saved.ahb_eb                 = sci_glb_read(REG_AP_AHB_AHB_EB, -1UL);
		ap_ahb_reg_saved.usb_phy_tune			= sci_glb_read(REG_AP_AHB_OTG_PHY_TUNE, -1UL);
		ap_ahb_reg_saved.misc_ckg_en            = sci_glb_read(REG_AP_AHB_MISC_CKG_EN, -1UL);
		ap_ahb_reg_saved.misc_cfg               = sci_glb_read(REG_AP_AHB_MISC_CFG, -1UL);
	} else{
		sci_glb_write(REG_AP_AHB_AHB_EB, 	ap_ahb_reg_saved.ahb_eb, -1UL);
		sci_glb_write(REG_AP_AHB_OTG_PHY_TUNE, 		ap_ahb_reg_saved.usb_phy_tune, -1UL);
		sci_glb_write(REG_AP_AHB_MISC_CKG_EN, 	ap_ahb_reg_saved.misc_ckg_en, -1UL);
		sci_glb_write(REG_AP_AHB_MISC_CFG, 	ap_ahb_reg_saved.misc_cfg, -1UL);
	}
	return;
}

void disable_apb_module(void)
{
	int stat;
	stat = sci_glb_read(REG_AP_APB_APB_EB, -1UL);
	stat &= (BIT_AP_APB_UART1_EB | BIT_AP_APB_UART0_EB);
	sci_glb_write(REG_AP_APB_APB_EB, stat, -1UL);
	return;
}
void bak_restore_apb(int bak)
{
	if (bak) {
		ap_apb_reg_saved.apb_eb = sci_glb_read(REG_AP_APB_APB_EB, -1UL);
		ap_apb_reg_saved.apb_misc_ctrl = sci_glb_read(REG_AP_APB_APB_MISC_CTRL, -1UL);

	} else {
		sci_glb_write(REG_AP_APB_APB_EB, ap_apb_reg_saved.apb_eb, -1UL);
		sci_glb_write(REG_AP_APB_APB_MISC_CTRL, ap_apb_reg_saved.apb_misc_ctrl, -1UL);
	}
	return;
}

static void bak_ap_clk_reg(int bak)
{
	volatile u32 i;
	if (bak) {
		ap_clk_reg_saved.ap_apb_cfg      = sci_glb_read(REG_AP_CLK_AP_APB_CFG      , -1UL);
		ap_clk_reg_saved.nandc_2x_cfg	 = sci_glb_read(REG_AP_CLK_NANDC_2X_CFG      , -1UL);
		ap_clk_reg_saved.nandc_1x_cfg    = sci_glb_read(REG_AP_CLK_NANDC_1X_CFG      , -1UL);
		ap_clk_reg_saved.nandc_ecc_cfg   = sci_glb_read(REG_AP_CLK_NANDC_ECC_CFG      , -1UL);
		ap_clk_reg_saved.otg2_utmi_cfg   = sci_glb_read(REG_AP_CLK_OTG2_UTMI_CFG         , -1UL);
		ap_clk_reg_saved.usb3_utmi_cfg      = sci_glb_read(REG_AP_CLK_USB3_UTMI_CFG      , -1UL);
		ap_clk_reg_saved.usb3_pipe_cfg  = sci_glb_read(REG_AP_CLK_USB3_PIPE_CFG  , -1UL);
		ap_clk_reg_saved.usb3_ref_cfg  = sci_glb_read(REG_AP_CLK_USB3_REF_CFG  , -1UL);
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
		ap_clk_reg_saved.i2c5_cfg        = sci_glb_read(REG_AP_CLK_I2C5_CFG        , -1UL);
		ap_clk_reg_saved.spi0_cfg        = sci_glb_read(REG_AP_CLK_SPI0_CFG        , -1UL);
		ap_clk_reg_saved.spi1_cfg        = sci_glb_read(REG_AP_CLK_SPI1_CFG        , -1UL);
		ap_clk_reg_saved.spi2_cfg        = sci_glb_read(REG_AP_CLK_SPI2_CFG        , -1UL);
		ap_clk_reg_saved.iis0_cfg        = sci_glb_read(REG_AP_CLK_IIS0_CFG        , -1UL);
		ap_clk_reg_saved.iis1_cfg        = sci_glb_read(REG_AP_CLK_IIS1_CFG        , -1UL);
		ap_clk_reg_saved.iis2_cfg        = sci_glb_read(REG_AP_CLK_IIS2_CFG        , -1UL);
		ap_clk_reg_saved.iis3_cfg        = sci_glb_read(REG_AP_CLK_IIS3_CFG        , -1UL);
		ap_clk_reg_saved.bist_192m_cfg        = sci_glb_read(REG_AP_CLK_BIST_192M_CFG        , -1UL);
		ap_clk_reg_saved.bist_256m_cfg        = sci_glb_read(REG_AP_CLK_BIST_256M_CFG        , -1UL);
	} else {
		sci_glb_write(REG_AP_CLK_AP_APB_CFG      , ap_clk_reg_saved.ap_apb_cfg      , -1UL);
		sci_glb_write(REG_AP_CLK_NANDC_2X_CFG    , ap_clk_reg_saved.nandc_2x_cfg & (~0x3)     , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_NANDC_2X_CFG      , ap_clk_reg_saved.nandc_2x_cfg     , -1UL);
		sci_glb_write(REG_AP_CLK_NANDC_1X_CFG      , ap_clk_reg_saved.nandc_1x_cfg & (~0x3)    , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_NANDC_1X_CFG      , ap_clk_reg_saved.nandc_1x_cfg , -1UL);
		sci_glb_write(REG_AP_CLK_NANDC_ECC_CFG      , ap_clk_reg_saved.nandc_ecc_cfg & (~0x3)     , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_NANDC_ECC_CFG      , ap_clk_reg_saved.nandc_ecc_cfg , -1UL);
		sci_glb_write(REG_AP_CLK_OTG2_UTMI_CFG      , ap_clk_reg_saved.otg2_utmi_cfg & (~0x3)     , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_OTG2_UTMI_CFG      , ap_clk_reg_saved.otg2_utmi_cfg , 1UL);
		sci_glb_write(REG_AP_CLK_USB3_UTMI_CFG      , ap_clk_reg_saved.usb3_utmi_cfg & (~0x3)     , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_USB3_UTMI_CFG      , ap_clk_reg_saved.usb3_utmi_cfg      , -1UL);
		sci_glb_write(REG_AP_CLK_USB3_PIPE_CFG      , ap_clk_reg_saved.usb3_pipe_cfg & (~0x3)    , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_USB3_PIPE_CFG      , ap_clk_reg_saved.usb3_pipe_cfg      , -1UL);
		sci_glb_write(REG_AP_CLK_USB3_REF_CFG      , ap_clk_reg_saved.usb3_ref_cfg & (~0x3)    , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_USB3_REF_CFG      , ap_clk_reg_saved.usb3_ref_cfg      , -1UL);
		sci_glb_write(REG_AP_CLK_UART0_CFG       , ap_clk_reg_saved.uart0_cfg & (~0x3), -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_UART0_CFG       , ap_clk_reg_saved.uart0_cfg       , -1UL);
		sci_glb_write(REG_AP_CLK_UART1_CFG       , ap_clk_reg_saved.uart1_cfg & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_UART1_CFG       , ap_clk_reg_saved.uart1_cfg       , -1UL);
		sci_glb_write(REG_AP_CLK_UART2_CFG       , ap_clk_reg_saved.uart2_cfg & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_UART2_CFG       , ap_clk_reg_saved.uart2_cfg       , -1UL);
		sci_glb_write(REG_AP_CLK_UART3_CFG       , ap_clk_reg_saved.uart3_cfg & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_UART3_CFG       , ap_clk_reg_saved.uart3_cfg       , -1UL);
		sci_glb_write(REG_AP_CLK_UART4_CFG       , ap_clk_reg_saved.uart4_cfg & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_UART4_CFG       , ap_clk_reg_saved.uart4_cfg       , -1UL);
		sci_glb_write(REG_AP_CLK_I2C0_CFG        , ap_clk_reg_saved.i2c0_cfg  & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_I2C0_CFG        , ap_clk_reg_saved.i2c0_cfg        , -1UL);
		sci_glb_write(REG_AP_CLK_I2C1_CFG        , ap_clk_reg_saved.i2c1_cfg  & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_I2C1_CFG        , ap_clk_reg_saved.i2c1_cfg        , -1UL);
		sci_glb_write(REG_AP_CLK_I2C2_CFG        , ap_clk_reg_saved.i2c2_cfg  & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_I2C2_CFG        , ap_clk_reg_saved.i2c2_cfg        , -1UL);
		sci_glb_write(REG_AP_CLK_I2C3_CFG        , ap_clk_reg_saved.i2c3_cfg  & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_I2C3_CFG        , ap_clk_reg_saved.i2c3_cfg        , -1UL);
		sci_glb_write(REG_AP_CLK_I2C4_CFG        , ap_clk_reg_saved.i2c4_cfg  & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_I2C4_CFG        , ap_clk_reg_saved.i2c4_cfg        , -1UL);
		sci_glb_write(REG_AP_CLK_I2C5_CFG        , ap_clk_reg_saved.i2c5_cfg  & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_I2C5_CFG        , ap_clk_reg_saved.i2c5_cfg        , -1UL);
		sci_glb_write(REG_AP_CLK_SPI0_CFG        , ap_clk_reg_saved.spi0_cfg  & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_SPI0_CFG        , ap_clk_reg_saved.spi0_cfg        , -1UL);
		sci_glb_write(REG_AP_CLK_SPI1_CFG        , ap_clk_reg_saved.spi1_cfg  & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_SPI1_CFG        , ap_clk_reg_saved.spi1_cfg        , -1UL);
		sci_glb_write(REG_AP_CLK_SPI2_CFG        , ap_clk_reg_saved.spi2_cfg  & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_SPI2_CFG        , ap_clk_reg_saved.spi2_cfg        , -1UL);
		sci_glb_write(REG_AP_CLK_IIS0_CFG        , ap_clk_reg_saved.iis0_cfg  & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_IIS0_CFG        , ap_clk_reg_saved.iis0_cfg        , -1UL);
		sci_glb_write(REG_AP_CLK_IIS1_CFG        , ap_clk_reg_saved.iis1_cfg  & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_IIS1_CFG        , ap_clk_reg_saved.iis1_cfg        , -1UL);
		sci_glb_write(REG_AP_CLK_IIS2_CFG        , ap_clk_reg_saved.iis2_cfg  & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_IIS2_CFG        , ap_clk_reg_saved.iis2_cfg        , -1UL);
		sci_glb_write(REG_AP_CLK_IIS3_CFG        , ap_clk_reg_saved.iis3_cfg  & (~0x3) , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_IIS3_CFG        , ap_clk_reg_saved.iis3_cfg       , -1UL);
		sci_glb_write(REG_AP_CLK_BIST_192M_CFG        , ap_clk_reg_saved.bist_192m_cfg & (~0x3)       , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_BIST_192M_CFG        , ap_clk_reg_saved.bist_192m_cfg        , -1UL);
		sci_glb_write(REG_AP_CLK_BIST_256M_CFG        , ap_clk_reg_saved.bist_256m_cfg & (~0x3)       , -1UL);
		for (i = 0; i < 10; i++)
			;
		sci_glb_write(REG_AP_CLK_BIST_256M_CFG        , ap_clk_reg_saved.bist_256m_cfg        , -1UL);
	}
}
void disable_aon_module(void)
{
	sci_glb_clr(REG_AON_APB_APB_EB0, BIT_AON_APB_AON_TMR_EB | BIT_AON_APB_AP_TMR0_EB);
	sci_glb_clr(REG_AON_APB_APB_EB1, BIT_AON_APB_AP_TMR2_EB | BIT_AON_APB_AP_TMR1_EB);
	sci_glb_clr(REG_AON_APB_APB_EB1, BIT_AON_APB_DISP_EMC_EB);
	sci_glb_clr(REG_AON_APB_GLB_CLK_AUTO_GATE_EN, BIT(30));
}
void bak_restore_aon(int bak)
{
	static uint32_t apb_eb1, pwr_ctl, apb_eb0, apb_clk;
	if (bak) {
		apb_clk = sci_glb_read(REG_AON_APB_GLB_CLK_AUTO_GATE_EN, -1UL);
		apb_eb0 = sci_glb_read(REG_AON_APB_APB_EB0, -1UL);
		apb_eb1 = sci_glb_read(REG_AON_APB_APB_EB1, -1UL);
		pwr_ctl = sci_glb_read(REG_AON_APB_PWR_CTRL, -1UL);
	} else {
		if (apb_eb1 & BIT_AON_APB_DISP_EMC_EB)
			sci_glb_set(REG_AON_APB_APB_EB1, BIT_AON_APB_DISP_EMC_EB);
		sci_glb_write(REG_AON_APB_APB_EB0, apb_eb0, -1UL);
		sci_glb_write(REG_AON_APB_APB_EB1, apb_eb1, -1UL);
		sci_glb_write(REG_AON_APB_GLB_CLK_AUTO_GATE_EN, apb_clk, -1UL);
	}
	return;
}

void disable_ana_module(void)
{
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	sci_adi_set(ANA_REG_GLB_LDO_PD_CTRL, BIT_LDO_SD_PD | BIT_LDO_SIM0_PD | BIT_LDO_SIM1_PD | BIT_LDO_SIM2_PD | BIT_LDO_CAMA_PD |\
		BIT_LDO_CAMD_PD | BIT_LDO_CAMIO_PD | BIT_LDO_CAMMOT_PD  | BIT_DCDC_WPA_PD);
#endif
#if defined(CONFIG_ADIE_SC2723S)
	sci_adi_set(ANA_REG_GLB_LDO_PD_CTRL, BIT_LDO_SDIO_PD | BIT_LDO_SIM0_PD | BIT_LDO_SIM1_PD | BIT_LDO_SIM2_PD | BIT_LDO_CAMA_PD |\
		BIT_LDO_CAMD_PD | BIT_LDO_CAMIO_PD | BIT_LDO_CAMMOT_PD  | BIT_DCDC_WPA_PD);
#endif
#if defined(CONFIG_ADIE_SC2723)
	sci_adi_set(ANA_REG_GLB_LDO_PD_CTRL, BIT_LDO_SDIO_PD | BIT_LDO_SIM0_PD | BIT_LDO_SIM1_PD | BIT_LDO_SIM2_PD | BIT_LDO_CAMA_PD |\
		BIT_LDO_CAMD_PD | BIT_LDO_CAMIO_PD | BIT_LDO_CAMMOT_PD  | BIT_LDO_SDCORE_PD | BIT_LDO_WIFIPA_PD | BIT_DCDC_CON_PD | BIT_DCDC_WPA_PD);
#endif
}
void bak_restore_ana(int bak)
{
	static uint32_t ldo_pd_ctrl;
	static uint32_t mask;
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	mask = BIT_LDO_SD_PD | BIT_LDO_SIM0_PD | BIT_LDO_SIM1_PD | BIT_LDO_SIM2_PD | BIT_LDO_CAMA_PD |\
		BIT_LDO_CAMD_PD | BIT_LDO_CAMIO_PD | BIT_LDO_CAMMOT_PD  | BIT_DCDC_WPA_PD;
#endif
#if defined(CONFIG_ADIE_SC2723S)
	mask = BIT_LDO_SDIO_PD | BIT_LDO_SIM0_PD | BIT_LDO_SIM1_PD | BIT_LDO_SIM2_PD | BIT_LDO_CAMA_PD |\
		BIT_LDO_CAMD_PD | BIT_LDO_CAMIO_PD | BIT_LDO_CAMMOT_PD  | BIT_DCDC_WPA_PD;
#endif

	if (bak) {
		/*ldo_pd_ctrl = sci_adi_read(ANA_REG_GLB_LDO_PD_CTRL);*/
	} else {
		/*sci_adi_write(ANA_REG_GLB_LDO_PD_CTRL, ldo_pd_ctrl, mask);*/
	}
	return;
}
#define REG_PIN_CHIP_SLEEP              (SPRD_PIN_BASE + 0x0244)
void show_pin_reg(void)
{
	sci_glb_set(SPRD_INTC0_BASE + 0x8, BIT(24) | BIT(30) | BIT(31)); /*ana & i2c & adi*/
	sci_glb_set(SPRD_INTC1_BASE + 0x8, BIT(18) | BIT(19)); /*kpd & gpio*/
	printk("REG_PIN_CHIP_SLEEP    0x%08x\n", sci_glb_read(REG_PIN_CHIP_SLEEP, -1UL));
	printk("REG_PMU_APB_XTL0_REL_CFG 0x%08x\n", sci_glb_read(REG_PMU_APB_XTL0_REL_CFG, -1UL));
	printk("REG_PMU_APB_XTLBUF0_REL_CFG 0x%08x\n", sci_glb_read(REG_PMU_APB_XTLBUF0_REL_CFG, -1UL));
	printk("REG_PMU_APB_XTLBUF1_REL_CFG 0x%08x\n", sci_glb_read(REG_PMU_APB_XTLBUF1_REL_CFG, -1UL));
	printk("REG_PMU_APB_XTLBUF2_REL_CFG 0x%08x\n", sci_glb_read(REG_PMU_APB_XTLBUF2_REL_CFG, -1UL));
}
void force_mcu_core_sleep(void)
{
	/*sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_CORE_SLEEP);*/
}
void enable_mcu_deep_sleep(void)
{
	/*sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_DEEP_SLEEP_EN | BIT_MCU_LIGHT_SLEEP_EN);*/
}
void disable_mcu_deep_sleep(void)
{
	/*sci_glb_clr(REG_AP_AHB_MCU_PAUSE, BIT_MCU_DEEP_SLEEP_EN | BIT_MCU_LIGHT_SLEEP_EN);*/
}
#define BITS_SET_CHECK(__reg, __bits, __string) do {\
	uint32_t mid = sci_glb_read(__reg, __bits); \
	if (mid != __bits) \
		printk(__string " not 1" "\n"); \
	} while (0)
#define BITS_CLEAR_CHECK(__reg, __bits, __string) do {\
	uint32_t mid = sci_glb_read(__reg, __bits); \
	if (mid & __bits) \
		printk(__string " not 0" "\n"); \
	} while (0)
#define PRINT_REG(__reg) do {\
	uint32_t mid = sci_glb_read(__reg, -1UL); \
	printk(#__reg "value 0x%x\n", mid); \
	} while (0)
void show_reg_status(void)
{
	BITS_CLEAR_CHECK(REG_AON_APB_APB_EB0, BIT_AON_APB_CA53_DAP_EB, "ca53_dap_eb");
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_DMA_EB, "dma_eb");
	sci_glb_set(REG_AP_AHB_AHB_EB, BIT_DMA_EB);
	BITS_CLEAR_CHECK((SPRD_DMA_BASE + 0x1c), BIT(20), "dma_busy");
	sci_glb_clr(REG_AP_AHB_AHB_EB, BIT_DMA_EB);
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_USB_EB, "usb_eb");
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_AP_AHB_SDIO0_EB, "sdio0_eb");
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_AP_AHB_SDIO1_EB, "sdio1_eb");
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_AP_AHB_SDIO2_EB, "sdio2_eb");
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_AP_AHB_EMMC_EB, "emmc_eb");
	BITS_CLEAR_CHECK(REG_AP_AHB_AHB_EB, BIT_AP_AHB_NFC_EB, "nfc_eb");
	PRINT_REG(REG_PMU_APB_SLEEP_STATUS);
}
uint32_t pwr_stat0 = 0, pwr_stat1 = 0, pwr_stat2 = 0, pwr_stat3 = 0, pwr_stat4 = 0, apb_eb0 = 0, apb_eb1 = 0, ahb_eb = 0, \
	apb_eb = 0, mcu_pause = 0, sys_force_sleep = 0, sys_auto_sleep_cfg = 0;
uint32_t ldo_slp_ctrl0, ldo_slp_ctrl1, ldo_slp_ctrl2, xtl_wait_ctrl, ldo_slp_ctrl3, ldo_slp_ctrl4, ldo_aud_ctrl4;
uint32_t mm_apb, ldo_pd_ctrl, arm_module_en;
uint32_t pubcp_slp_status_dbg0, wtlcp_slp_status_dbg0, cp_slp_status_dbg1, sleep_ctrl, ddr_sleep_ctrl, sleep_status, pwr_ctrl, ca7_standby_status;
uint32_t pd_pub0_sys, pd_pub1_sys;
#if defined(CONFIG_ARCH_SCX35L)
extern void cp0_power_domain_debug(void);
extern void cp1_power_domain_debug(void);
#endif

void bak_last_reg(void)
{
#if defined(CONFIG_ARCH_SCX35L)
	/*cp0_power_domain_debug();*/
	/*cp1_power_domain_debug();*/
#endif
	pd_pub0_sys = __raw_readl(REG_PMU_APB_PD_PUB0_SYS_CFG);
	pd_pub1_sys = __raw_readl(REG_PMU_APB_PD_PUB1_SYS_CFG);
	pubcp_slp_status_dbg0 = __raw_readl(REG_PMU_APB_PD_PUBCP_SYS_CFG);
	wtlcp_slp_status_dbg0 = __raw_readl(REG_PMU_APB_PD_WTLCP_SYS_CFG);
	pwr_stat0 = __raw_readl(REG_PMU_APB_PWR_STATUS0_DBG);
	pwr_stat1 = __raw_readl(REG_PMU_APB_PWR_STATUS1_DBG);
	pwr_stat2 = __raw_readl(REG_PMU_APB_PWR_STATUS2_DBG);
	sleep_ctrl = __raw_readl(REG_PMU_APB_SLEEP_CTRL);
	ddr_sleep_ctrl = __raw_readl(REG_PMU_APB_DDR_SLEEP_CTRL);
	sleep_status = __raw_readl(REG_PMU_APB_SLEEP_STATUS);
	apb_eb0 = __raw_readl(REG_AON_APB_APB_EB0);
	apb_eb1 = __raw_readl(REG_AON_APB_APB_EB1);
	pwr_ctrl = __raw_readl(REG_AON_APB_PWR_CTRL);
	ahb_eb = __raw_readl(REG_AP_AHB_AHB_EB);
	apb_eb = __raw_readl(REG_AP_APB_APB_EB);
	sys_force_sleep = __raw_readl(REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG);
	sys_auto_sleep_cfg = __raw_readl(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG);

#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	ldo_slp_ctrl0 = sci_adi_read(ANA_REG_GLB_LDO_SLP_CTRL0);
	ldo_slp_ctrl1 = sci_adi_read(ANA_REG_GLB_LDO_SLP_CTRL1);
	ldo_slp_ctrl2 = sci_adi_read(ANA_REG_GLB_LDO_SLP_CTRL2);
	ldo_slp_ctrl3 = sci_adi_read(ANA_REG_GLB_LDO_SLP_CTRL3);
#endif
#if defined(CONFIG_ADIE_SC2723S)
	ldo_slp_ctrl0 = sci_adi_read(ANA_REG_GLB_PWR_SLP_CTRL0);
	ldo_slp_ctrl1 = sci_adi_read(ANA_REG_GLB_PWR_SLP_CTRL1);
	ldo_slp_ctrl2 = sci_adi_read(ANA_REG_GLB_PWR_SLP_CTRL2);
	ldo_slp_ctrl3 = sci_adi_read(ANA_REG_GLB_PWR_SLP_CTRL3);
#endif
#if defined(CONFIG_ADIE_SC2723)
	ldo_slp_ctrl0 = sci_adi_read(ANA_REG_GLB_PWR_SLP_CTRL0);
	ldo_slp_ctrl1 = sci_adi_read(ANA_REG_GLB_PWR_SLP_CTRL1);
	ldo_slp_ctrl2 = sci_adi_read(ANA_REG_GLB_PWR_SLP_CTRL2);
	ldo_slp_ctrl3 = sci_adi_read(ANA_REG_GLB_PWR_SLP_CTRL3);
	ldo_slp_ctrl4 = sci_adi_read(ANA_REG_GLB_PWR_SLP_CTRL4);
#endif

#if defined(CONFIG_ARCH_SCX15)
#else
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	ldo_aud_ctrl4 = sci_adi_read(ANA_REG_GLB_AUD_SLP_CTRL4);
#endif
#if defined(CONFIG_ADIE_SC2723S)
	ldo_aud_ctrl4 = sci_adi_read(ANA_REG_GLB_AUD_SLP_CTRL);
#endif
#if defined(CONFIG_ADIE_SC2723)
	ldo_aud_ctrl4 = sci_adi_read(ANA_REG_GLB_AUD_SLP_CTRL);
#endif
#endif
}
void print_last_reg(void)
{
	printk("aon pmu status reg\n");
	printk("REG_PMU_APB_PD_PUB0_SYS_CFG ------ 0x%08x\n", pd_pub0_sys);
	printk("REG_PMU_APB_PD_PUB1_SYS_CFG ------ 0x%08x\n", pd_pub1_sys);
	printk("REG_PMU_APB_PD_AP_SYS_CFG ------ 0x%08x\n",
			sci_glb_read(REG_PMU_APB_PD_AP_SYS_CFG, -1UL));
#ifdef REG_PMU_APB_PD_AP_DISP_CFG
	printk("REG_PMU_APB_PD_AP_DISP_CFG ------ 0x%08x\n",
			sci_glb_read(REG_PMU_APB_PD_AP_DISP_CFG, -1UL));
#endif

#if defined(CONFIG_ARCH_SCX30G)
	printk("REG_PMU_APB_PD_DDR_PUBL_CFG ------ 0x%08x\n",
			sci_glb_read(REG_PMU_APB_PD_DDR_PUBL_CFG, -1UL));
	printk("REG_PMU_APB_PD_DDR_PHY_CFG ------ 0x%08x\n",
			sci_glb_read(REG_PMU_APB_PD_DDR_PHY_CFG, -1UL));
#endif
	printk("REG_PMU_APB_PUBCP_SLP_STATUS_DBG0 ----- 0x%08x\n", pubcp_slp_status_dbg0);
	printk("REG_PMU_APB_PD_WTLCP_SYS_CFG ----- 0x%08x\n", wtlcp_slp_status_dbg0);
#if !defined(CONFIG_ARCH_SCX35L)
	printk("REG_PMU_APB_CP_SLP_STATUS_DBG1 ----- 0x%08x\n", cp_slp_status_dbg1);
#endif
	printk("REG_PMU_APB_PWR_STATUS0_DBG ----- 0x%08x\n", pwr_stat0);
	printk("REG_PMU_APB_PWR_STATUS1_DBG ----- 0x%08x\n", pwr_stat1);
	printk("REG_PMU_APB_PWR_STATUS2_DBG ----- 0x%08x\n", pwr_stat2);
	printk("REG_PMU_APB_PWR_STATUS3_DBG ----- 0x%08x\n", pwr_stat3);
	printk("REG_PMU_APB_PWR_STATUS4_DBG ----- 0x%08x\n", pwr_stat4);

	printk("REG_PMU_APB_SLEEP_CTRL ----- 0x%08x\n", sleep_ctrl);
	printk("REG_PMU_APB_DDR_SLEEP_CTRL ----- 0x%08x\n", ddr_sleep_ctrl);
	printk("REG_PMU_APB_SLEEP_STATUS ----- 0x%08x\n", sleep_status);
	printk("aon apb reg\n");
	printk("REG_AON_APB_APB_EB0  ----- 0x%08x\n", apb_eb0);
	printk("REG_AON_APB_APB_EB1  ----- 0x%08x\n", apb_eb1);
	printk("REG_AON_APB_PWR_CTRL ----- 0x%08x\n", pwr_ctrl);
	printk("REG_AON_APB_BB_BG_CTRL ------ 0x%08x\n",
			sci_glb_read(REG_AON_APB_BB_BG_CTRL, -1UL));

	printk("ap ahb reg \n");
	printk("REG_AP_AHB_AHB_EB ----- 0x%08x\n", ahb_eb);

	printk("ap apb reg\n");
	printk("REG_AP_APB_APB_EB ---- 0x%08x\n", apb_eb);
	printk("REG_AP_AHB_MCU_PAUSE ---   0x%08x\n", mcu_pause);
	printk("REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG --- 0x%08x\n", sys_force_sleep);
	printk("REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG ---- 0x%08x\n", sys_auto_sleep_cfg);

	printk("ana reg \n");
	printk("ANA_REG_GLB_LDO_SLP_CTRL0 --- 0x%08x\n", ldo_slp_ctrl0);
	printk("ANA_REG_GLB_LDO_SLP_CTRL1 --- 0x%08x\n", ldo_slp_ctrl1);
	printk("ANA_REG_GLB_LDO_SLP_CTRL2 --- 0x%08x\n", ldo_slp_ctrl2);
	printk("ANA_REG_GLB_LDO_SLP_CTRL3 --- 0x%08x\n", ldo_slp_ctrl3);
	printk("ANA_REG_GLB_AUD_SLP_CTRL4 --- 0x%08x\n", ldo_aud_ctrl4);
#if !defined(CONFIG_ADIE_SC2723) && !defined(CONFIG_ADIE_SC2723S)
	/*printk("ANA_REG_GLB_PWR_XTL_EN4 -- 0x%08x\n", sci_adi_read(ANA_REG_GLB_PWR_XTL_EN4));*/
#endif
#if !defined(CONFIG_ARCH_SCX15) && !defined(CONFIG_ADIE_SC2723) && !defined(CONFIG_ADIE_SC2723S)
	/*printk("ANA_REG_GLB_PWR_XTL_EN5 -- 0x%08x\n", sci_adi_read(ANA_REG_GLB_PWR_XTL_EN5));*/
#endif
	printk("mm reg\n");
	printk("REG_MM_AHB_AHB_EB ---- 0x%08x\n", mm_apb);

	printk("ana reg\n");
	printk("ANA_REG_GLB_LDO_PD_CTRL --- 0x%08x\n", ldo_pd_ctrl);
		printk("Enter Deepsleep  Condition Check!\n");

	if (ahb_eb & BIT_AP_AHB_HSIC_EB)

		printk("###---- BIT_AP_AHB_HSIC_EB still set ----###\n");

	if (ahb_eb & BIT_AP_AHB_USB3_EB)

		printk("###---- BIT_USB_EB still set ----###\n");

	if (ahb_eb & BIT_DMA_EB)

		printk("###---- BIT_DMA_EB still set ----###\n");

	if (ahb_eb & BIT_AP_AHB_NFC_EB)

		printk("###---- BIT_AP_AHB_NFC_EB still set ----###\n");

	if (ahb_eb & BIT_AP_AHB_SDIO0_EB)

		printk("###---- BIT_AP_AHB_SDIO0_EB still set ----###\n");

	if (ahb_eb & BIT_AP_AHB_SDIO1_EB)

		printk("###---- BIT_AP_AHB_SDIO1_EB still set ----###\n");

	if (ahb_eb & BIT_AP_AHB_SDIO2_EB)

		printk("###---- BIT_AP_AHB_SDIO2_EB still set ----###\n");

	if (ahb_eb & BIT_AP_AHB_EMMC_EB)

		printk("###---- BIT_AP_AHB_EMMC_EB still set ----###\n");

	if (ahb_eb & BIT_AP_AHB_ZIPENC_EB)

		printk("###---- BIT_AP_AHB_ZIPENC_EB still set ----###\n");

	if (ahb_eb &  BIT_AP_AHB_ZIPDEC_EB)

		printk("###---- BIT_AP_AHB_ZIPDEC_EB still set ----###\n");

	if (ahb_eb & BIT_AP_AHB_CC63S_EB)

		printk("###---- BIT_AP_AHB_CC63S_EB set ----###\n");

	if (ahb_eb & BIT_AP_AHB_CC63P_EB)

		printk("###---- BIT_AP_AHB_CC63P_EB set ----###\n");


	if (apb_eb & BIT_AP_APB_SIM0_EB)

		printk("###---- BIT_AP_APB_SIM0_EB still set ----###\n");

	if (apb_eb & BIT_AP_APB_IIS0_EB)

		printk("###---- BIT_AP_APB_IIS0_EB set ----###\n");

	if (apb_eb & BIT_AP_APB_IIS1_EB)

		printk("###---- BIT_AP_APB_IIS1_EB set ----###\n");

	if (apb_eb & BIT_AP_APB_IIS2_EB)

		printk("###---- BIT_AP_APB_IIS2_EB set ----###\n");

	if (apb_eb & BIT_AP_APB_IIS3_EB)

		printk("###---- BIT_AP_APB_IIS3_EB set ----###\n");

	if (apb_eb & BIT_AP_APB_SPI0_EB)

		printk("###---- BIT_AP_APB_SPI0_EB set ----###\n");

	if (apb_eb & BIT_AP_APB_SPI1_EB)

		printk("###---- BIT_AP_APB_SPI1_EB set ----###\n");

	if (apb_eb & BIT_AP_APB_SPI2_EB)

		printk("###---- BIT_AP_APB_SPI2_EB set ----###\n");

	if (apb_eb & BIT_AP_APB_I2C0_EB)

		printk("###---- BIT_AP_APB_I2C0_EB still set ----###\n");

	if (apb_eb & BIT_AP_APB_I2C1_EB)

		printk("###---- BIT_AP_APB_I2C1_EB still set ----###\n");

	if (apb_eb & BIT_AP_APB_I2C2_EB)

		printk("###---- BIT_AP_APB_I2C2_EB still set ----###\n");

	if (apb_eb & BIT_AP_APB_I2C3_EB)

		printk("###---- BIT_AP_APB_I2C3_EB still set ----###\n");

	if (apb_eb & BIT_AP_APB_I2C4_EB)

		printk("###---- BIT_AP_APB_I2C4_EB still set ----###\n");

	if (apb_eb & BIT_AP_APB_I2C5_EB)

		printk("###---- BIT_AP_APB_I2C5_EB still set ----###\n");

	if (apb_eb & BIT_AP_APB_UART0_EB)

		printk("###---- BIT_AP_APB_UART0_EB still set ----###\n");

	if (apb_eb & BIT_AP_APB_UART1_EB)

		printk("###---- BIT_AP_APB_UART1_EB still set ----###\n");

	if (apb_eb & BIT_AP_APB_UART2_EB)

		printk("###---- BIT_AP_APB_UART2_EB still set ----###\n");

	if (apb_eb & BIT_AP_APB_UART3_EB)

		printk("###---- BIT_AP_APB_UART3_EB still set ----###\n");

	if (apb_eb & BIT_AP_APB_UART4_EB)

		printk("###---- BIT_AP_APB_UART4_EB still set ----###\n");

	if (apb_eb0 & BIT_AON_APB_GPU_EB)

		printk("###---- BIT_AON_APB_GPU_EB still set ----###\n");

	if (apb_eb0 & BIT_AON_APB_CA53_DAP_EB)
		printk("###---- BIT_AON_APB_CA53_DAP_EB set ----###\n");
}
void check_ldo(void)
{
}

void check_pd(void)
{
}

unsigned int sprd_irq_pending(void)
{
	uint32_t  intc0_bits = 0, intc1_bits = 0, intc2_bits = 0, intc3_bits = 0, intc4_bits = 0, intc5_bits = 0;
	intc0_bits = sci_glb_read(SPRD_INTC0_BASE, -1UL);
	intc1_bits = sci_glb_read(SPRD_INTC1_BASE, -1UL);
	intc2_bits = sci_glb_read(SPRD_INTC2_BASE, -1UL);
	intc3_bits = sci_glb_read(SPRD_INTC3_BASE, -1UL);
	intc4_bits = sci_glb_read(SPRD_INTC4_BASE, -1UL);
	intc5_bits = sci_glb_read(SPRD_INTC5_BASE, -1UL);
	if (intc0_bits | intc1_bits | intc2_bits | intc3_bits | intc4_bits | intc5_bits)
		return 1;
	else
		return 0;
}

/*copy code for deepsleep return */
#define SAVED_VECTOR_SIZE 64
static uint32_t *sp_pm_reset_vector;
static uint32_t saved_vector[SAVED_VECTOR_SIZE];
void __iomem *iram_start;
struct emc_repower_param *repower_param;


#define SPRD_RESET_VECTORS 0X00000000
#define SLEEP_RESUME_CODE_PHYS 	0X400
#define SLEEP_CODE_SIZE 0x800
#define SPRD_IRAM0_PHYS		0x0

extern unsigned long reg_aon_apb_eb0_vir;
extern unsigned long reg_aon_apb_eb0_phy;
extern unsigned long reg_uart1_enable_vir;
extern unsigned long reg_uart1_enable_phy;

static int init_reset_vector(void)
{
#if !defined(CONFIG_ARCH_WHALE)
	sp_pm_reset_vector = (u32 *)SPRD_IRAM0_BASE;

	reg_aon_apb_eb0_vir = SPRD_AONAPB_BASE;
	reg_aon_apb_eb0_phy = SPRD_AONAPB_PHYS;
	reg_uart1_enable_vir = SPRD_APBREG_BASE;
	reg_uart1_enable_phy = SPRD_APBREG_PHYS;

	iram_start = (void __iomem *)(SPRD_IRAM0_BASE + SLEEP_RESUME_CODE_PHYS);
	/* copy sleep code to (IRAM). */
	if ((sc8830_standby_iram_end - sc8830_standby_iram) > SLEEP_CODE_SIZE) {
		panic("##: code size is larger than expected, need more memory!\n");
	}
	memcpy_toio(iram_start, sc8830_standby_iram, SLEEP_CODE_SIZE);


	/* just make sure*/
	flush_cache_all();
	/*outer_flush_all();*/
#endif
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
static unsigned long *vtest;
static unsigned long *ptest;
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

#if !(defined(CONFIG_ARCH_WHALE))
u32 __attribute__ ((naked)) read_cpsr(void)
{
	__asm__ __volatile__("mrs r0, cpsr\nbx lr");
}
#endif
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
	while (tx_fifo_val != 0) {
		if (timeout <= 0)
			break;
		udelay(100);
		tx_fifo_val = __raw_readl(UART_STS1);
		tx_fifo_val >>= 8;
		tx_fifo_val &= 0xff;
		timeout--;
	}

	timeout = 30;
	really_done = __raw_readl(UART_STS0);
	while (!(really_done & UART_TRANSFER_REALLY_OVER)) {
		if (timeout <= 0)
			break;
		udelay(100);
		really_done = __raw_readl(UART_STS0);
		timeout--;
	}
}
#define SAVE_GLOBAL_REG do { \
	bak_restore_apb(1); \
	bak_ap_clk_reg(1); \
	bak_restore_ahb(1); \
	bak_restore_aon(1); \
	} while (0)
#define RESTORE_GLOBAL_REG do { \
	bak_restore_apb(0); \
	bak_ap_clk_reg(0); \
	bak_restore_aon(0); \
	bak_restore_ahb(0); \
	} while (0)

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
	/*sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_SYS_SLEEP_EN);*/
	cpu_do_idle();
	/*sci_glb_clr(REG_AP_AHB_MCU_PAUSE, BIT_MCU_SYS_SLEEP_EN);*/
	hard_irq_set();
	RESTORE_GLOBAL_REG;
}

static void light_sleep(void)
{
	/*sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_LIGHT_SLEEP_EN);*/
	cpu_do_idle();
	/*sci_glb_clr(REG_AP_AHB_MCU_PAUSE, BIT_MCU_LIGHT_SLEEP_EN);*/
}
void int_work_round(void)
{
	return;
}
void show_deep_reg_status(void)
{
	printk("PWR_STATUS0_DBG	0x%08x\n", sci_glb_read(REG_PMU_APB_PWR_STATUS0_DBG, -1UL));
	printk("PWR_STATUS1_DBG	0x%08x\n", sci_glb_read(REG_PMU_APB_PWR_STATUS1_DBG, -1UL));
	printk("PWR_STATUS2_DBG	0x%08x\n", sci_glb_read(REG_PMU_APB_PWR_STATUS2_DBG, -1UL));
	printk("PWR_STATUS3_DBG	0x%08x\n", sci_glb_read(REG_PMU_APB_PWR_STATUS3_DBG, -1UL));
	printk("PWR_STATUS4_DBG	0x%08x\n", sci_glb_read(REG_PMU_APB_PWR_STATUS4_DBG, -1UL));
}

extern void pm_debug_set_wakeup_timer(void);
extern void pm_debug_set_apwdt(void);
typedef u32 (*iram_standby_entry_ptr)(u32, u32);
static int sc_sleep_call(unsigned long flag)
{
	u32 ret = 0;
	struct mm_struct *mm = current->active_mm;

	iram_standby_entry_ptr func_ptr;
	cpu_switch_mm(init_mm.pgd, &init_mm);
	func_ptr = (iram_standby_entry_ptr)(SLEEP_RESUME_CODE_PHYS + (u32)sp_pm_collapse - (u32)sc8830_standby_iram);
	ret = (func_ptr)(0, flag);
	cpu_switch_mm(mm->pgd, mm);

	return ret;
}

int deep_sleep(int from_idle)
{
	int ret = 0;
	static unsigned int cnt;

	if (!from_idle) {
		SAVE_GLOBAL_REG;
		show_pin_reg();
		enable_mcu_deep_sleep();
		configure_for_deepsleep(0);
		disable_ahb_module();
		disable_dma();
		disable_aon_module();
		show_reg_status();
		bak_last_reg();
		print_last_reg();
		print_int_status();
		int_work_round();
		disable_apb_module();
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

#if defined(CONFIG_ARCH_SCX35L64) || defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE)
	ret = cpu_suspend(5);
	ret = (ret ? 0 : 1);
#else
	ret = sc_sleep_call(from_idle);
#endif
	udelay(50);

	if (!from_idle) {
		printk("ret %d not from idle\n", ret);
		if (ret) {
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
		/*sci_glb_clr(SPRD_PMU_BASE+0x00F4, 0x3FF);*/
		RESTORE_GLOBAL_REG;
	} else {
		/* __raw_writel(0x1, REG_PMU_APB_CA7_C0_CFG); */
		pr_debug("ret %d  from idle\n", ret);
	}

	udelay(5);
#if !(defined(CONFIG_ARCH_SCX35L64) || defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE))
	if (ret)
		cpu_init();
#endif

	/*if (soc_is_scx35_v0())
		sci_glb_clr(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG,BIT_CA7_CORE_AUTO_GATE_EN);*/
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
	__raw_writel(0xfdffbfff, SPRD_INTC0_BASE + 0xc);/*intc0*/
	__raw_writel(0x02004000, SPRD_INTC0_BASE + 0x8);/*intc0*/
	__raw_writel(0xffffffff, SPRD_INTC0_BASE + 0x100c);/*intc1*/
#endif

	time = get_sys_cnt();
	if (!hw_irqs_disabled())  {
#if !(defined(CONFIG_ARCH_SCX35L64) || defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE))
		flags = read_cpsr();
#endif
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
		printk("###### %s,  DEVICE_AHB ###\n", __func__);
		set_sleep_mode(SLP_MODE_ARM);
		arm_sleep();
	} else if (status & DEVICE_APB) {
		printk("###### %s,	DEVICE_APB ###\n", __func__);
		set_sleep_mode(SLP_MODE_MCU);
		mcu_sleep();
	} else if (status & LIGHT_SLEEP) {
		printk("###### %s,	DEVICE_APB ###\n", __func__);
		set_sleep_mode(SLP_MODE_LIT);
		light_sleep();
	} else {
		/*printk("###### %s,	DEEP ###\n", __func__ );*/
		set_sleep_mode(SLP_MODE_DEP);
#if 1

		ret = deep_sleep(0);
#else
		/*pm_debug_set_wakeup_timer();*/
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
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
#ifdef CONFIG_ARCH_SCX30G
	static struct dcdc_core_ds_step_info step_info[5] = {
		{0, 0, 0, 0},
		{0, 3, 0, 5},
		{0, 6, 0, 10},
		{0, 9, 0, 0},
		{0, 12, 0, 5}
	};
	step_info[0].ctl_reg = ANA_REG_GLB_MP_PWR_CTRL1;
	step_info[1].ctl_reg = ANA_REG_GLB_MP_PWR_CTRL1;
	step_info[2].ctl_reg = ANA_REG_GLB_MP_PWR_CTRL1;
	step_info[3].ctl_reg = ANA_REG_GLB_MP_PWR_CTRL1;
	step_info[4].ctl_reg = ANA_REG_GLB_MP_PWR_CTRL1;
	step_info[0].cal_reg = ANA_REG_GLB_MP_PWR_CTRL2;
	step_info[1].cal_reg = ANA_REG_GLB_MP_PWR_CTRL2;
	step_info[2].cal_reg = ANA_REG_GLB_MP_PWR_CTRL2;
	step_info[3].cal_reg = ANA_REG_GLB_MP_PWR_CTRL3;
	step_info[4].cal_reg = ANA_REG_GLB_MP_PWR_CTRL3;

	static u8 dcdc_core_down_volt[] = {4, 1, 1, 2, 3, 5, 0, 6};
	static u8 dcdc_core_up_volt[] = {6, 2, 3, 4, 0, 1, 7, 7};
	u32 dcdc_core_cal_adi, i;
	/*1100,700,800,900,1000,650,1200,1300*/
	static u32 step_ratio[] = {10, 10, 6, 3, 3};
	dcdc_core_ctl_adi = (sci_adi_read(ANA_REG_GLB_MP_MISC_CTRL) >> 3) & 0x7;
	if ((ana_chip_id == 0x2713ca00) || (ana_chip_id == 0x2723a000)) {
		dcdc_core_ctl_ds  = dcdc_core_ctl_adi;
	} else {
		dcdc_core_ctl_ds  = dcdc_core_down_volt[dcdc_core_ctl_adi];
	}
	printk("ana_chip_id:%x, dcdc_core_ctl_adi = %d, dcdc_core_ctl_ds = %d\n"
				, ana_chip_id, dcdc_core_ctl_adi, dcdc_core_ctl_ds);
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	val = sci_adi_read(ANA_REG_GLB_DCDC_SLP_CTRL);
#endif
#if defined(CONFIG_ADIE_SC2723S)
	val = sci_adi_read(ANA_REG_GLB_DCDC_SLP_CTRL0);
#endif
	val &= ~0x7;
	val |= dcdc_core_ctl_ds;
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	sci_adi_write(ANA_REG_GLB_DCDC_SLP_CTRL, val, 0xffff);
#endif
#if defined(CONFIG_ADIE_SC2723S)
	sci_adi_write(ANA_REG_GLB_DCDC_SLP_CTRL0, val, 0xffff);
#endif

	if (dcdc_core_ctl_ds < dcdc_core_ctl_adi) {
		dcdc_core_cal_adi = sci_adi_read(ANA_REG_GLB_DCDC_CORE_ADI) & 0x1F;
		/*last step must equel function mode */
		sci_adi_write(step_info[4].ctl_reg, dcdc_core_ctl_adi<<step_info[4].ctl_sht, 0x07 << step_info[4].ctl_sht);
		sci_adi_write(step_info[4].cal_reg, dcdc_core_cal_adi<<step_info[4].cal_sht, 0x1F << step_info[4].cal_sht);

		for (i = 0; i < 4; i++) {
			val = dcdc_core_cal_adi + step_ratio[i];
			if (val <= 0x1F) {
				sci_adi_write(step_info[i].ctl_reg, dcdc_core_ctl_ds<<step_info[i].ctl_sht, 0x07<<step_info[i].ctl_sht);
				sci_adi_write(step_info[i].cal_reg, val<<step_info[i].cal_sht, 0x1F << step_info[i].cal_sht);
				dcdc_core_cal_adi = val;
			} else {
				sci_adi_write(step_info[i].ctl_reg, dcdc_core_up_volt[dcdc_core_ctl_ds]<<step_info[i].ctl_sht,
												0x07 << step_info[i].ctl_sht);
				sci_adi_write(step_info[i].cal_reg, (val-0x1F)<<step_info[i].cal_sht, 0x1F << step_info[i].cal_sht);
				dcdc_core_ctl_ds = dcdc_core_up_volt[dcdc_core_ctl_ds];
				dcdc_core_cal_adi = val - 0x1F;
			}
		}
	} else {
		dcdc_core_cal_adi = sci_adi_read(ANA_REG_GLB_DCDC_CORE_ADI) & 0x1F;
		for (i = 0; i < 5; i++) {
			sci_adi_write(step_info[i].ctl_reg, dcdc_core_ctl_adi<<step_info[i].ctl_sht, 0x07<<step_info[i].ctl_sht);
			sci_adi_write(step_info[i].cal_reg, dcdc_core_cal_adi<<step_info[i].cal_sht, 0x1F<<step_info[i].cal_sht);
		}
	}
#else
	dcdc_core_ctl_adi = sci_adi_read(ANA_REG_GLB_DCDC_CORE_ADI);
	dcdc_core_ctl_adi = dcdc_core_ctl_adi >> 0x5;
	dcdc_core_ctl_adi = dcdc_core_ctl_adi & 0x7;
	/*only support dcdc_core_ctl_adi 0.9v,1.0v, 1.1v, 1.2v, 1.3v*/
	switch (dcdc_core_ctl_adi) {
	case 0:/*1.1v*/
		dcdc_core_ctl_ds = 0x4; /*1.0v*/
		break;
	case 4: /*1.0v*/
		dcdc_core_ctl_ds = 0x3;/*0.9v*/
		break;
	case 6: /*1.2v*/
		dcdc_core_ctl_ds = 0x0;/*1.1v*/
		break;
	case 7: /*1.3v*/
		dcdc_core_ctl_ds = 0x6;/*1.2v*/
		break;
	case 3: /*0.9v*/
		dcdc_core_ctl_ds = 0x2;/*0.8v*/
		break;
	case 1: /*0.7v*/
	case 2: /*0.8v*/
	case 5: /*0.65v*/
		/*unvalid value*/
		break;
	}
	printk("dcdc_core_ctl_adi = %d, dcdc_core_ctl_ds = %d\n", dcdc_core_ctl_adi, dcdc_core_ctl_ds);
	/*valid value*/
	if (dcdc_core_ctl_ds != -1) {
#ifndef CONFIG_ARCH_SCX15
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	val = sci_adi_read(ANA_REG_GLB_DCDC_SLP_CTRL);
	val &= ~(0x7);
	val |= dcdc_core_ctl_ds;
	sci_adi_write(ANA_REG_GLB_DCDC_SLP_CTRL, val, 0xffff);
#endif
#if defined(CONFIG_ADIE_SC2723S)
	val = sci_adi_read(ANA_REG_GLB_DCDC_SLP_CTRL0);
	val &= ~(0x7);
	val |= dcdc_core_ctl_ds;
	sci_adi_write(ANA_REG_GLB_DCDC_SLP_CTRL0, val, 0xffff);
#endif
#if defined(CONFIG_ADIE_SC2723)
		val = sci_adi_read(ANA_REG_GLB_DCDC_SLP_CTRL1);
		val &= ~(0xF);
		val |= dcdc_core_ctl_ds;
		sci_adi_write(ANA_REG_GLB_DCDC_SLP_CTRL1, val, 0xffff);
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
#ifdef CONFIG_ARCH_SCX30G
	ana_chip_id = (sci_adi_read(ANA_REG_GLB_CHIP_ID_HIGH) & 0xFFFF) << 16;
	ana_chip_id |= (sci_adi_read(ANA_REG_GLB_CHIP_ID_LOW) & 0xFFFF);
#endif

#if defined(CONFIG_ARCH_SCX15) || defined(CONFIG_ADIE_SC2723)
	dcdc_core_ds_config();
#else
	/*set vddcore deep sleep voltage to 0.9v*/
	/*sci_adi_set(ANA_REG_GLB_DCDC_SLP_CTRL, BITS_DCDC_CORE_CTL_DS(3));*/
	dcdc_core_ds_config();
	/*open vddcore lp VDDMEM, DCDCGEN mode*/
	/*sci_adi_set(ANA_REG_GLB_LDO_SLP_CTRL2, BIT_SLP_DCDCCORE_LP_EN | BIT_SLP_DCDCMEM_LP_EN | BIT_SLP_DCDCGEN_LP_EN);*/
	/*open vdd28, vdd18 lp mode*/
	/*sci_adi_set(ANA_REG_GLB_LDO_SLP_CTRL3, BIT_SLP_LDOVDD28_LP_EN | BIT_SLP_LDOVDD18_LP_EN);*/
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

#ifdef CONFIG_SPRD_EXT_IC_POWER
/*    EXT IC should disable otg mode when power off*/
	extern void sprd_extic_otg_power(int enable);
#endif

static void sc8830_power_off(void)
{
	u32 reg_val;
	/*turn off all modules's ldo*/

#ifdef CONFIG_SPRD_EXT_IC_POWER
	sprd_extic_otg_power(0);
#endif
	/*sci_adi_raw_write(ANA_REG_GLB_LDO_PD_CTRL, 0x1fff);*/

#if defined(CONFIG_ARCH_SCX15) || defined(CONFIG_ADIE_SC2723) || defined(CONFIG_ADIE_SC2723S)
	sci_adi_raw_write(ANA_REG_GLB_PWR_WR_PROT_VALUE, BITS_PWR_WR_PROT_VALUE(0x6e7f));
	do {
		reg_val = (sci_adi_read(ANA_REG_GLB_PWR_WR_PROT_VALUE) & BIT_PWR_WR_PROT);
	} while (reg_val == 0);
	sci_adi_raw_write(ANA_REG_GLB_LDO_PD_CTRL, 0xfff);
	sci_adi_raw_write(ANA_REG_GLB_LDO_DCDC_PD, 0x7fff);
#endif

#if !defined(CONFIG_64BIT)
#if defined(CONFIG_ADIE_SC2713S) || defined(CONFIG_ADIE_SC2713)
	/*turn off system core's ldo*/
	sci_adi_raw_write(ANA_REG_GLB_LDO_DCDC_PD_RTCCLR, 0x0);
	sci_adi_raw_write(ANA_REG_GLB_LDO_DCDC_PD_RTCSET, 0X7fff);
#endif
#endif

}


static void sc8830_machine_restart(char mode, const char *cmd)
{
	local_irq_disable();
	local_fiq_disable();

	arch_reset(mode, cmd);

	mdelay(1000);

	printk("reboot failed!\n");

	while (1)
		;
}
void resume_code_remap(void)
{
	int ret;
	ret = ioremap_page_range(SPRD_IRAM0_PHYS, SPRD_IRAM0_PHYS+SZ_8K, SPRD_IRAM0_PHYS, PAGE_KERNEL_EXEC);
	if (ret) {
		printk("resume_code_remap err %d\n", ret);
		BUG();
	}
}
void __init sc_pm_init(void)
{
#if !(defined(CONFIG_ARCH_SCX35L64) || defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE))
	init_reset_vector();
	resume_code_remap();
#endif
	pm_power_off   = sc8830_power_off;
	arm_pm_restart = sc8830_machine_restart;
	pr_info("power off %pf, restart %pf\n", pm_power_off, arm_pm_restart);
	init_gr();
	setup_autopd_mode();
	pm_ana_ldo_config();
	init_led();
	/* disable all sleep mode */
	/*sci_glb_clr(REG_AP_AHB_MCU_PAUSE, BIT_MCU_DEEP_SLEEP_EN | BIT_MCU_LIGHT_SLEEP_EN | \
		BIT_MCU_SYS_SLEEP_EN | BIT_MCU_CORE_SLEEP);*/
#if !(defined(CONFIG_ARCH_SCX35L64) || defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE))
	set_reset_vector();
#endif
#ifdef CONFIG_DDR_VALIDITY_TEST
	test_memory();
#endif
#ifndef CONFIG_SPRD_PM_DEBUG
	pm_debug_init();
#endif

	/* enable arm clock auto gating*/
	/*sci_glb_set(REG_AHB_AHB_CTL1, BIT_ARM_AUTO_GATE_EN);*/
#ifdef CONFIG_CPU_IDLE
#ifndef CONFIG_64BIT
	/* Used to armv7 */
	sc_cpuidle_init();
#endif
#endif
/*
	wake_lock_init(&pm_debug_lock, WAKE_LOCK_SUSPEND, "pm_not_ready");
	wake_lock(&pm_debug_lock);
*/
#if defined(CONFIG_SPRD_DEBUG)
#if !(defined(CONFIG_ARCH_SCX35L64) || defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE))
	sprd_debug_init();
#endif
#endif
}
