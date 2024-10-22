// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-fmeter.h"
#include "clk-mt6989-fmeter.h"

#define FM_TIMEOUT			30
#define SUBSYS_PLL_NUM			4
#define VLP_FM_WAIT_TIME		40	/* ~= 38.64ns * 1023 */
#define FM_RST_BITS			(1 << 15)

/* cksys fm setting */
#define FM_PLL_TST_CK			0
#define FM_PLL_CKDIV_CK			1
#define FM_POSTDIV_SHIFT		(24)
#define FM_POSTDIV_MASK			GENMASK(26, 24)
#define FM_CKDIV_EN			(1 << 17)
#define FM_CKDIV_SHIFT			(9)
#define FM_CKDIV_MASK			GENMASK(12, 9)

/* subsys fm setting */
#define SUBSYS_CKDIV_EN			(1 << 16)
#define SUBSYS_TST_EN			((1 << 12) | (1 << 19))
#define SUBSYS_CKDIV_SHIFT			(7)
#define SUBSYS_CKDIV_MASK			GENMASK(10, 7)

static DEFINE_SPINLOCK(meter_lock);
#define fmeter_lock(flags)   spin_lock_irqsave(&meter_lock, flags)
#define fmeter_unlock(flags) spin_unlock_irqrestore(&meter_lock, flags)

static DEFINE_SPINLOCK(subsys_meter_lock);
#define subsys_fmeter_lock(flags)   spin_lock_irqsave(&subsys_meter_lock, flags)
#define subsys_fmeter_unlock(flags) spin_unlock_irqrestore(&subsys_meter_lock, flags)

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

/* check from topckgen&vlpcksys CODA */
#define CLK26CALI_0					(0x220)
#define CLK26CALI_1					(0x224)
#define CLK_MISC_CFG_0					(0x240)
#define CLK_DBG_CFG					(0x28C)
#define CKSYS2_CLK26CALI_0				(0xA20)
#define CKSYS2_CLK26CALI_1				(0xA24)
#define CKSYS2_CLK_MISC_CFG_0				(0xA40)
#define CKSYS2_CLK_DBG_CFG				(0xA8C)
#define VLP_FQMTR_CON0					(0x230)
#define VLP_FQMTR_CON1					(0x234)

/* ARMPLL_LL_PLL_CTRL Register */
#define ARMPLL_LL_CON0					(0x0008)
#define ARMPLL_LL_CON1					(0x000C)
#define ARMPLL_LL_FQMTR_CON0				(0x0040)
#define ARMPLL_LL_FQMTR_CON1				(0x0044)

/* ARMPLL_BL_PLL_CTRL Register */
#define ARMPLL_BL_CON0					(0x0008)
#define ARMPLL_BL_CON1					(0x000C)
#define ARMPLL_BL_FQMTR_CON0				(0x0040)
#define ARMPLL_BL_FQMTR_CON1				(0x0044)

/* ARMPLL_B_PLL_CTRL Register */
#define ARMPLL_B_CON0					(0x0008)
#define ARMPLL_B_CON1					(0x000C)
#define ARMPLL_B_FQMTR_CON0				(0x0040)
#define ARMPLL_B_FQMTR_CON1				(0x0044)

/* CCIPLL_PLL_CTRL Register */
#define CCIPLL_CON0					(0x0008)
#define CCIPLL_CON1					(0x000C)
#define CCIPLL_FQMTR_CON0				(0x0040)
#define CCIPLL_FQMTR_CON1				(0x0044)

/* PTPPLL_PLL_CTRL Register */
#define PTPPLL_CON0					(0x0008)
#define PTPPLL_CON1					(0x000C)
#define PTPPLL_FQMTR_CON0				(0x0040)
#define PTPPLL_FQMTR_CON1				(0x0044)

/* MFGPLL_PLL_CTRL Register */
#define MFGPLL_CON0					(0x0008)
#define MFGPLL_CON1					(0x000C)
#define MFGPLL_FQMTR_CON0				(0x0040)
#define MFGPLL_FQMTR_CON1				(0x0044)

/* MFGPLL_SC0_PLL_CTRL Register */
#define MFGPLL_SC0_CON0					(0x0008)
#define MFGPLL_SC0_CON1					(0x000C)
#define MFGPLL_SC0_FQMTR_CON0				(0x0040)
#define MFGPLL_SC0_FQMTR_CON1				(0x0044)

/* MFGPLL_SC1_PLL_CTRL Register */
#define MFGPLL_SC1_CON0					(0x0008)
#define MFGPLL_SC1_CON1					(0x000C)
#define MFGPLL_SC1_FQMTR_CON0				(0x0040)
#define MFGPLL_SC1_FQMTR_CON1				(0x0044)


static void __iomem *fm_base[FM_SYS_NUM];
static unsigned int fm_ckdiv_en;

struct fmeter_data {
	enum fm_sys_id type;
	const char *name;
	unsigned int pll_con0;
	unsigned int pll_con1;
	unsigned int con0;
	unsigned int con1;
};

static struct fmeter_data subsys_fm[] = {
	[FM_VLP_CKSYS] = {FM_VLP_CKSYS, "fm_vlp_cksys",
		0, 0, VLP_FQMTR_CON0, VLP_FQMTR_CON1},
	[FM_MFGPLL] = {FM_MFGPLL, "fm_mfgpll",
		MFGPLL_CON0, MFGPLL_CON1, MFGPLL_FQMTR_CON0, MFGPLL_FQMTR_CON1},
	[FM_MFGPLL_SC0] = {FM_MFGPLL_SC0, "fm_mfgpll_sc0",
		MFGPLL_SC0_CON0, MFGPLL_SC0_CON1, MFGPLL_SC0_FQMTR_CON0, MFGPLL_SC0_FQMTR_CON1},
	[FM_MFGPLL_SC1] = {FM_MFGPLL_SC1, "fm_mfgpll_sc1",
		MFGPLL_SC1_CON0, MFGPLL_SC1_CON1, MFGPLL_SC1_FQMTR_CON0, MFGPLL_SC1_FQMTR_CON1},
	[FM_ARMPLL_LL] = {FM_ARMPLL_LL, "fm_armpll_ll",
		ARMPLL_LL_CON0, ARMPLL_LL_CON1, ARMPLL_LL_FQMTR_CON0, ARMPLL_LL_FQMTR_CON1},
	[FM_ARMPLL_BL] = {FM_ARMPLL_BL, "fm_armpll_bl",
		ARMPLL_BL_CON0, ARMPLL_BL_CON1, ARMPLL_BL_FQMTR_CON0, ARMPLL_BL_FQMTR_CON1},
	[FM_ARMPLL_B] = {FM_ARMPLL_B, "fm_armpll_b",
		ARMPLL_B_CON0, ARMPLL_B_CON1, ARMPLL_B_FQMTR_CON0, ARMPLL_B_FQMTR_CON1},
	[FM_CCIPLL] = {FM_CCIPLL, "fm_ccipll",
		CCIPLL_CON0, CCIPLL_CON1, CCIPLL_FQMTR_CON0, CCIPLL_FQMTR_CON1},
	[FM_PTPPLL] = {FM_PTPPLL, "fm_ptppll",
		PTPPLL_CON0, PTPPLL_CON1, PTPPLL_FQMTR_CON0, PTPPLL_FQMTR_CON1},
};

const char *comp_list[] = {
	[FM_TOPCKGEN] = "mediatek,mt6989-topckgen",
	[FM_APMIXEDSYS] = "mediatek,mt6989-apmixedsys",
	[FM_VLP_CKSYS] = "mediatek,mt6989-vlp_cksys",
	[FM_MFGPLL] = "mediatek,mt6989-mfgpll_pll_ctrl",
	[FM_MFGPLL_SC0] = "mediatek,mt6989-mfgpll_sc0_pll_ctrl",
	[FM_MFGPLL_SC1] = "mediatek,mt6989-mfgpll_sc1_pll_ctrl",
	[FM_ARMPLL_LL] = "mediatek,mt6989-armpll_ll_pll_ctrl",
	[FM_ARMPLL_BL] = "mediatek,mt6989-armpll_bl_pll_ctrl",
	[FM_ARMPLL_B] = "mediatek,mt6989-armpll_b_pll_ctrl",
	[FM_CCIPLL] = "mediatek,mt6989-ccipll_pll_ctrl",
	[FM_PTPPLL] = "mediatek,mt6989-ptppll_pll_ctrl",
};

/*
 * clk fmeter
 */

#define FMCLK3(_t, _i, _n, _o, _g, _c) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .grp = _g, .ck_div = _c}
#define FMCLK2(_t, _i, _n, _o, _p, _c) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .pdn = _p, .ck_div = _c, .grp = 0}
#define FMCLK(_t, _i, _n, _c) { .type = _t, .id = _i, .name = _n, .ck_div = _c, .grp = 0}

static const struct fmeter_clk fclks[] = {
	/* CKGEN Part */
	FMCLK2(CKGEN, FM_AXI_CK, "fm_axi_ck", 0x0010, 7, 1),
	FMCLK2(CKGEN, FMP_FAXI_CK, "fmp_faxi_ck", 0x0010, 15, 1),
	FMCLK2(CKGEN, FM_U_FAXI_CK, "fm_u_faxi_ck", 0x0010, 23, 1),
	FMCLK2(CKGEN, FM_PEXTP_FAXI_CK, "fm_pextp_faxi_ck", 0x0010, 31, 1),
	FMCLK2(CKGEN, FM_BUS_AXIMEM_CK, "fm_bus_aximem_ck", 0x0020, 7, 1),
	FMCLK2(CKGEN, FM_MEM_SUB_CK, "fm_mem_sub_ck", 0x0020, 15, 1),
	FMCLK2(CKGEN, FMP_FMEM_SUB_CK, "fmp_fmem_sub_ck", 0x0020, 23, 1),
	FMCLK2(CKGEN, FM_U_FMEM_SUB_CK, "fm_u_fmem_sub_ck", 0x0020, 31, 1),
	FMCLK2(CKGEN, FM_PEXTP_FMEM_SUB_CK, "fm_pextp_fmem_sub_ck", 0x0030, 7, 1),
	FMCLK2(CKGEN, FM_EMI_N_CK, "fm_emi_n_ck", 0x0030, 15, 1),
	FMCLK2(CKGEN, FM_EMI_S_CK, "fm_emi_s_ck", 0x0030, 23, 1),
	FMCLK2(CKGEN, FM_EMI_SLICE_N_CK, "fm_emi_slice_n_ck", 0x0030, 31, 1),
	FMCLK2(CKGEN, FM_EMI_SLICE_S_CK, "fm_emi_slice_s_ck", 0x0040, 7, 1),
	FMCLK2(CKGEN, FM_AP2CONN_HOST_CK, "fm_ap2conn_host_ck", 0x0040, 15, 1),
	FMCLK2(CKGEN, FM_ATB_CK, "fm_atb_ck", 0x0040, 23, 1),
	FMCLK2(CKGEN, FM_CIRQ_CK, "fm_cirq_ck", 0x0040, 31, 1),
	FMCLK2(CKGEN, FM_EFUSE_CK, "fm_efuse_ck", 0x0050, 7, 1),
	FMCLK2(CKGEN, FM_MCU_L3GIC_CK, "fm_mcu_l3gic_ck", 0x0050, 15, 1),
	FMCLK2(CKGEN, FM_MCU_INFRA_CK, "fm_mcu_infra_ck", 0x0050, 23, 1),
	FMCLK2(CKGEN, FM_DSP_CK, "fm_dsp_ck", 0x0060, 7, 1),
	FMCLK2(CKGEN, FM_MFG_REF_CK, "fm_mfg_ref_ck", 0x0060, 15, 1),
	FMCLK2(CKGEN, FM_MFGSC_REF_CK, "fm_mfgsc_ref_ck", 0x0060, 23, 1),
	FMCLK2(CKGEN, FM_MFG_EB_CK, "fm_mfg_eb_ck", 0x0060, 31, 1),
	FMCLK2(CKGEN, FM_UART_CK, "fm_uart_ck", 0x0070, 7, 1),
	FMCLK2(CKGEN, FM_SPI0_B_CK, "fm_spi0_b_ck", 0x0070, 15, 1),
	FMCLK2(CKGEN, FM_SPI1_B_CK, "fm_spi1_b_ck", 0x0070, 23, 1),
	FMCLK2(CKGEN, FM_SPI2_B_CK, "fm_spi2_b_ck", 0x0070, 31, 1),
	FMCLK2(CKGEN, FM_SPI3_B_CK, "fm_spi3_b_ck", 0x0080, 7, 1),
	FMCLK2(CKGEN, FM_SPI4_B_CK, "fm_spi4_b_ck", 0x0080, 15, 1),
	FMCLK2(CKGEN, FM_SPI5_B_CK, "fm_spi5_b_ck", 0x0080, 23, 1),
	FMCLK2(CKGEN, FM_SPI6_B_CK, "fm_spi6_b_ck", 0x0080, 31, 1),
	FMCLK2(CKGEN, FM_SPI7_B_CK, "fm_spi7_b_ck", 0x0090, 7, 1),
	FMCLK2(CKGEN, FM_MSDC_MACRO_1P_CK, "fm_msdc_macro_1p_ck", 0x0090, 15, 1),
	FMCLK2(CKGEN, FM_MSDC_MACRO_2P_CK, "fm_msdc_macro_2p_ck", 0x0090, 23, 1),
	FMCLK2(CKGEN, FM_MSDC30_1_CK, "fm_msdc30_1_ck", 0x0090, 31, 1),
	FMCLK2(CKGEN, FM_MSDC30_2_CK, "fm_msdc30_2_ck", 0x00A0, 7, 1),
	FMCLK2(CKGEN, FM_AUD_INTBUS_CK, "fm_aud_intbus_ck", 0x00A0, 15, 1),
	FMCLK2(CKGEN, FM_DISP_PWM_CK, "fm_disp_pwm_ck", 0x00A0, 23, 1),
	FMCLK2(CKGEN, FM_USB_CK, "fm_usb_ck", 0x00A0, 31, 1),
	FMCLK2(CKGEN, FM_USB_XHCI_CK, "fm_usb_xhci_ck", 0x00B0, 7, 1),
	FMCLK2(CKGEN, FM_USB_1P_CK, "fm_usb_1p_ck", 0x00B0, 15, 1),
	FMCLK2(CKGEN, FM_USB_XHCI_1P_CK, "fm_usb_xhci_1p_ck", 0x00B0, 23, 1),
	FMCLK2(CKGEN, FM_I2C_CK, "fm_i2c_ck", 0x00B0, 31, 1),
	FMCLK2(CKGEN, FM_AUD_ENGEN1_CK, "fm_aud_engen1_ck", 0x00C0, 7, 1),
	FMCLK2(CKGEN, FM_AUD_ENGEN2_CK, "fm_aud_engen2_ck", 0x00C0, 15, 1),
	FMCLK2(CKGEN, FM_AES_UFSFDE_CK, "fm_aes_ufsfde_ck", 0x00C0, 23, 1),
	FMCLK2(CKGEN, FM_U_CK, "fm_u_ck", 0x00C0, 31, 1),
	FMCLK2(CKGEN, FM_U_MBIST_CK, "fm_u_mbist_ck", 0x00D0, 7, 1),
	FMCLK2(CKGEN, FM_PEXTP_MBIST_CK, "fm_pextp_mbist_ck", 0x00D0, 15, 1),
	FMCLK2(CKGEN, FM_AUD_1_CK, "fm_aud_1_ck", 0x00D0, 23, 1),
	FMCLK2(CKGEN, FM_AUD_2_CK, "fm_aud_2_ck", 0x00D0, 31, 1),
	FMCLK2(CKGEN, FM_AUDIO_H_CK, "fm_audio_h_ck", 0x00E0, 7, 1),
	FMCLK2(CKGEN, FM_ADSP_CK, "fm_adsp_ck", 0x00E0, 15, 1),
	FMCLK2(CKGEN, FM_ADSP_UARTHUB_B_CK, "fm_adsp_uarthub_b_ck", 0x00E0, 23, 1),
	FMCLK2(CKGEN, FM_DPMAIF_MAIN_CK, "fm_dpmaif_main_ck", 0x00E0, 31, 1),
	FMCLK2(CKGEN, FM_PWM_CK, "fm_pwm_ck", 0x00F0, 7, 1),
	FMCLK2(CKGEN, FM_MCUPM_CK, "fm_mcupm_ck", 0x00F0, 15, 1),
	FMCLK2(CKGEN, FM_SFLASH_CK, "fm_sflash_ck", 0x00F0, 23, 1),
	FMCLK2(CKGEN, FM_IPSEAST_CK, "fm_ipseast_ck", 0x00F0, 31, 1),
	FMCLK2(CKGEN, FM_IPSWEST_CK, "fm_ipswest_ck", 0x0100, 7, 1),
	FMCLK2(CKGEN, FM_TL_CK, "fm_tl_ck", 0x0100, 15, 1),
	FMCLK2(CKGEN, FM_EMI_INTERFACE_546_CK, "fm_emi_interface_546_ck", 0x0100, 23, 1),
	FMCLK2(CKGEN, FM_SDF_CK, "fm_sdf_ck", 0x0100, 31, 1),
	FMCLK2(CKGEN, FM_UARTHUB_B_CK, "fm_uarthub_b_ck", 0x0110, 7, 1),
	FMCLK2(CKGEN, FM_SSR_PKA_CK, "fm_ssr_pka_ck", 0x0110, 15, 1),
	FMCLK2(CKGEN, FM_SSR_DMA_CK, "fm_ssr_dma_ck", 0x0110, 23, 1),
	FMCLK2(CKGEN, FM_SSR_KDF_CK, "fm_ssr_kdf_ck", 0x0110, 31, 1),
	FMCLK2(CKGEN, FM_SSR_RNG_CK, "fm_ssr_rng_ck", 0x0120, 7, 1),
	FMCLK2(CKGEN, FM_SSR_SEJ_CK, "fm_ssr_sej_ck", 0x0120, 15, 1),
	FMCLK2(CKGEN, FM_SPU0_CK, "fm_spu0_ck", 0x0120, 23, 1),
	FMCLK2(CKGEN, FM_SPU1_CK, "fm_spu1_ck", 0x0120, 31, 1),
	FMCLK2(CKGEN, FM_DXCC_CK, "fm_dxcc_ck", 0x0130, 7, 1),
	FMCLK2(CKGEN, FM_DPSW_CMP_26M_CK, "fm_dpsw_cmp_26m_ck", 0x0130, 15, 1),
	FMCLK2(CKGEN, FM_SMAPCK_CK, "fm_smapck_ck", 0x0130, 23, 1),
	/* ABIST Part */
	FMCLK(ABIST, FM_MDPLL_FS26M_CK, "fm_mdpll_fs26m_ck", 1),
	FMCLK(ABIST, FM_RTC32K_I, "fm_rtc32k_i", 1),
	FMCLK3(ABIST, FM_MAINPLL_CKDIV_CK, "fm_mainpll_ckdiv_ck", 0x0354, 3, 13),
	FMCLK3(ABIST, FM_UNIVPLL_CKDIV_CK, "fm_univpll_ckdiv_ck", 0x030c, 3, 13),
	FMCLK3(ABIST, FM_APLL2_CKDIV_CK, "fm_apll2_ckdiv_ck", 0x0340, 3, 13),
	FMCLK3(ABIST, FM_APLL1_CKDIV_CK, "fm_apll1_ckdiv_ck", 0x032c, 3, 13),
	FMCLK3(ABIST, FM_ADSPPLL_CKDIV_CK, "fm_adsppll_ckdiv_ck", 0x0384, 3, 13),
	FMCLK3(ABIST, FM_MMPLL_CKDIV_CK, "fm_mmpll_ckdiv_ck", 0x03A4, 3, 13),
	FMCLK3(ABIST, FM_EMIPLL2_CKDIV_CK, "fm_emipll2_ckdiv_ck", 0x03C4, 3, 13),
	FMCLK3(ABIST, FM_EMIPLL_CKDIV_CK, "fm_emipll_ckdiv_ck", 0x03B4, 3, 13),
	FMCLK3(ABIST, FM_MSDCPLL_CKDIV_CK, "fm_msdcpll_ckdiv_ck", 0x0364, 3, 13),
	FMCLK(ABIST, FM_MCUSYS_ARM_OUT_ALL, "fm_mcusys_arm_out_all", 1),
	FMCLK(ABIST, FM_DSI1_LNTC_DSICLK_FQMTR_CK, "fm_dsi1_lntc_dsiclk_fqmtr_ck", 1),
	FMCLK(ABIST, FM_DSI0_MPPLL_TST_CK, "fm_dsi0_mppll_tst_ck", 1),
	FMCLK(ABIST, FM_DSI0_LNTC_DSICLK_FQMTR_CK, "fm_dsi0_lntc_dsiclk_fqmtr_ck", 1),
	FMCLK(ABIST, FM_DSI0_MPPLL_TST_CK_2, "fm_dsi0_mppll_tst_ck_2", 1),
	FMCLK(ABIST, FM_CSI0A_DPHY_DELAYCAL_CK, "fm_csi0a_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI0B_DPHY_DELAYCAL_CK, "fm_csi0b_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_U_CLK2FREQ_CK, "fm_u_clk2freq_ck", 1),
	FMCLK(ABIST, FM_MSDC11_IN_CK, "fm_msdc11_in_ck", 1),
	FMCLK(ABIST, FM_MSDC12_IN_CK, "fm_msdc12_in_ck", 1),
	FMCLK(ABIST, FM_MSDC21_IN_CK, "fm_msdc21_in_ck", 1),
	FMCLK(ABIST, FM_MSDC22_IN_CK, "fm_msdc22_in_ck", 1),
	FMCLK(ABIST, FM_466M_FMEM_INFRASYS_CK, "fm_466m_fmem_infrasys_ck", 1),
	FMCLK(ABIST, FM_R0_OUT_FM, "fm_r0_out_fm", 1),
	FMCLK(ABIST, FM_466M_FMEM_INFRASYS_2ND, "fm_466m_fmem_infrasys_2nd", 1),
	FMCLK(ABIST, FM_STH_MEM2_CK, "fm_sth_mem2_ck", 1),
	FMCLK(ABIST, FM_NTH_MEM2_CK, "fm_nth_mem2_ck", 1),
	/* CKGEN_CK2 Part */
	FMCLK2(CKGEN_CK2, FM_CAMTG1_CK, "fm_camtg1_ck", 0x0810, 7, 1),
	FMCLK2(CKGEN_CK2, FM_CAMTG2_CK, "fm_camtg2_ck", 0x0810, 15, 1),
	FMCLK2(CKGEN_CK2, FM_CAMTG3_CK, "fm_camtg3_ck", 0x0810, 23, 1),
	FMCLK2(CKGEN_CK2, FM_CAMTG4_CK, "fm_camtg4_ck", 0x0810, 31, 1),
	FMCLK2(CKGEN_CK2, FM_CAMTG5_CK, "fm_camtg5_ck", 0x0820, 7, 1),
	FMCLK2(CKGEN_CK2, FM_CAMTG6_CK, "fm_camtg6_ck", 0x0820, 15, 1),
	FMCLK2(CKGEN_CK2, FM_CAMTG7_CK, "fm_camtg7_ck", 0x0820, 23, 1),
	FMCLK2(CKGEN_CK2, FM_SENINF0_CK, "fm_seninf0_ck", 0x0820, 31, 1),
	FMCLK2(CKGEN_CK2, FM_SENINF1_CK, "fm_seninf1_ck", 0x0830, 7, 1),
	FMCLK2(CKGEN_CK2, FM_SENINF2_CK, "fm_seninf2_ck", 0x0830, 15, 1),
	FMCLK2(CKGEN_CK2, FM_SENINF3_CK, "fm_seninf3_ck", 0x0830, 23, 1),
	FMCLK2(CKGEN_CK2, FM_SENINF4_CK, "fm_seninf4_ck", 0x0830, 31, 1),
	FMCLK2(CKGEN_CK2, FM_SENINF5_CK, "fm_seninf5_ck", 0x0840, 7, 1),
	FMCLK2(CKGEN_CK2, FM_CCU_AHB_CK, "fm_ccu_ahb_ck", 0x0840, 15, 1),
	FMCLK2(CKGEN_CK2, FM_IMG1_CK, "fm_img1_ck", 0x0840, 23, 1),
	FMCLK2(CKGEN_CK2, FM_IPE_CK, "fm_ipe_ck", 0x0840, 31, 1),
	FMCLK2(CKGEN_CK2, FM_CAM_CK, "fm_cam_ck", 0x0850, 7, 1),
	FMCLK2(CKGEN_CK2, FM_CAMTM_CK, "fm_camtm_ck", 0x0850, 15, 1),
	FMCLK2(CKGEN_CK2, FM_DPE_CK, "fm_dpe_ck", 0x0850, 23, 1),
	FMCLK2(CKGEN_CK2, FM_VDEC_CK, "fm_vdec_ck", 0x0850, 31, 1),
	FMCLK2(CKGEN_CK2, FM_CCUSYS_CK, "fm_ccusys_ck", 0x0860, 7, 1),
	FMCLK2(CKGEN_CK2, FM_CCUTM_CK, "fm_ccutm_ck", 0x0860, 15, 1),
	FMCLK2(CKGEN_CK2, FM_VENC_CK, "fm_venc_ck", 0x0860, 23, 1),
	FMCLK2(CKGEN_CK2, FM_DP_CORE_CK, "fm_dp_core_ck", 0x0860, 31, 1),
	FMCLK2(CKGEN_CK2, FM_DP_CK, "fm_dp_ck", 0x0870, 7, 1),
	FMCLK2(CKGEN_CK2, FM_DISP_CK, "fm_disp_ck", 0x0870, 15, 1),
	FMCLK2(CKGEN_CK2, FM_MDP_CK, "fm_mdp_ck", 0x0870, 23, 1),
	FMCLK2(CKGEN_CK2, FM_MMINFRA_CK, "fm_mminfra_ck", 0x0870, 31, 1),
	FMCLK2(CKGEN_CK2, FM_MMUP_CK, "fm_mmup_ck", 0x0880, 7, 1),
	/* ABIST_CK2 Part */
	FMCLK3(ABIST_CK2, FM_MAINPLL2_CKDIV_CK, "fm_mainpll2_ckdiv_ck", 0x0284, 3, 13),
	FMCLK3(ABIST_CK2, FM_UNIVPLL2_192M_CK, "fm_univpll2_192m_ck", 0x0294, 3, 1),
	FMCLK3(ABIST_CK2, FM_MMPLL2_CKDIV_CK, "fm_mmpll2_ckdiv_ck", 0x02a4, 3, 13),
	FMCLK3(ABIST_CK2, FM_IMGPLL_CKDIV_CK, "fm_imgpll_ckdiv_ck", 0x0374, 3, 13),
	FMCLK3(ABIST_CK2, FM_TVDPLL_CKDIV_CK, "fm_tvdpll_ckdiv_ck", 0x024c, 3, 13),
	/* VLPCK Part */
	FMCLK2(VLPCK, FM_SCP_CK, "fm_scp_ck", 0x0010, 7, 1),
	FMCLK2(VLPCK, FM_SCP_SPI_CK, "fm_scp_spi_ck", 0x0010, 15, 1),
	FMCLK2(VLPCK, FM_SCP_IIC_CK, "fm_scp_iic_ck", 0x0010, 23, 1),
	FMCLK2(VLPCK, FM_SCP_SPI_HS_CK, "fm_scp_spi_hs_ck", 0x0010, 31, 1),
	FMCLK2(VLPCK, FM_SCP_IIC_HS_CK, "fm_scp_iic_hs_ck", 0x0020, 7, 1),
	FMCLK2(VLPCK, FM_PWRAP_ULPOSC_CK, "fm_pwrap_ulposc_ck", 0x0020, 15, 1),
	FMCLK2(VLPCK, FM_SPMI_32KCK, "fm_spmi_32kck", 0x0020, 23, 1),
	FMCLK2(VLPCK, FM_APXGPT_26M_B_CK, "fm_apxgpt_26m_b_ck", 0x0020, 31, 1),
	FMCLK2(VLPCK, FM_DPSW_CK, "fm_dpsw_ck", 0x0030, 7, 1),
	FMCLK2(VLPCK, FM_SPMI_M_CK, "fm_spmi_m_ck", 0x0030, 15, 1),
	FMCLK2(VLPCK, FM_DVFSRC_CK, "fm_dvfsrc_ck", 0x0030, 23, 1),
	FMCLK2(VLPCK, FM_PWM_VLP_CK, "fm_pwm_vlp_ck", 0x0030, 31, 1),
	FMCLK2(VLPCK, FM_AXI_VLP_CK, "fm_axi_vlp_ck", 0x0040, 7, 1),
	FMCLK2(VLPCK, FM_SYSTIMER_26M_CK, "fm_systimer_26m_ck", 0x0040, 15, 1),
	FMCLK2(VLPCK, FM_SSPM_CK, "fm_sspm_ck", 0x0040, 23, 1),
	FMCLK2(VLPCK, FM_SRCK_CK, "fm_srck_ck", 0x0040, 31, 1),
	FMCLK2(VLPCK, FM_CAMTG0_CK, "fm_camtg0_ck", 0x0050, 15, 1),
	FMCLK2(VLPCK, FM_IPS_CK, "fm_ips_ck", 0x0050, 23, 1),
	FMCLK2(VLPCK, FM_SSPM_26M_CK, "fm_sspm_26m_ck", 0x0050, 31, 1),
	FMCLK2(VLPCK, FM_ULPOSC_SSPM_CK, "fm_ulposc_sspm_ck", 0x0060, 7, 1),
	FMCLK(VLPCK, FM_ULPOSC_CK, "fm_ulposc_ck", 1),
	FMCLK(VLPCK, FM_ULPOSC2_CK, "fm_ulposc2_ck", 1),
	FMCLK(VLPCK, FM_PMIF_SPMI_M_SYS_CK, "fm_pmif_spmi_m_sys_ck", 1),
	/* SUBSYS Part */
	FMCLK(SUBSYS, FM_MFGPLL, "fm_mfgpll", 1),
	FMCLK(SUBSYS, FM_MFGPLL_SC0, "fm_mfgpll_sc0", 1),
	FMCLK(SUBSYS, FM_MFGPLL_SC1, "fm_mfgpll_sc1", 1),
	FMCLK(SUBSYS, FM_ARMPLL_LL, "fm_armpll_ll", 1),
	FMCLK(SUBSYS, FM_ARMPLL_BL, "fm_armpll_bl", 1),
	FMCLK(SUBSYS, FM_ARMPLL_B, "fm_armpll_b", 1),
	FMCLK(SUBSYS, FM_CCIPLL, "fm_ccipll", 1),
	FMCLK(SUBSYS, FM_PTPPLL, "fm_ptppll", 1),
	{},
};

const struct fmeter_clk *mt6989_get_fmeter_clks(void)
{
	return fclks;
}

static unsigned int check_pdn(void __iomem *base,
		unsigned int type, unsigned int ID)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++) {
		if (fclks[i].type == type && fclks[i].id == ID)
			break;
	}

	if (i >= ARRAY_SIZE(fclks) - 1)
		return 1;

	if (!fclks[i].ofs)
		return 0;

	if (type == SUBSYS) {
		if ((clk_readl(base + fclks[i].ofs) & fclks[i].pdn)
				!= fclks[i].pdn) {
			return 1;
		}
	} else if (type != SUBSYS && ((clk_readl(base + fclks[i].ofs)
			& BIT(fclks[i].pdn)) == BIT(fclks[i].pdn)))
		return 1;

	return 0;
}

static unsigned int get_post_div(unsigned int type, unsigned int ID)
{
	unsigned int post_div = 1;
	int i;

	if (type != ABIST && type != ABIST_CK2)
		return post_div;

	if ((ID <= 0) || (type == ABIST && ID >= FM_ABIST_NUM)
			|| (type == ABIST_CK2 && ID >= FM_ABIST_2_NUM))
		return post_div;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++) {
		if (fclks[i].type == type && fclks[i].id == ID
				&& fclks[i].grp != 0) {
			post_div =  clk_readl(fm_base[FM_APMIXEDSYS] + fclks[i].ofs);
			post_div = 1 << ((post_div & FM_POSTDIV_MASK) >> FM_POSTDIV_SHIFT);
			break;
		}
	}

	if (i == (ARRAY_SIZE(fclks) - 1))
		return post_div;

	return post_div;
}

static unsigned int get_clk_div(unsigned int type, unsigned int ID)
{
	unsigned int clk_div = 1;
	int i;

	if (type != ABIST && type != ABIST_CK2)
		return clk_div;

	if ((ID <= 0) || (type == ABIST && ID >= FM_ABIST_NUM)
			|| (type == ABIST_CK2 && ID >= FM_ABIST_2_NUM))
		return clk_div;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++) {
		if (fclks[i].type == type && fclks[i].id == ID
				&& fclks[i].grp != 0) {
			clk_div =  clk_readl(fm_base[FM_APMIXEDSYS] + fclks[i].ofs - 0x4);
			clk_div = (clk_div & FM_CKDIV_MASK) >> FM_CKDIV_SHIFT;
			break;
		}
	}

	if (i == (ARRAY_SIZE(fclks) - 1))
		return clk_div;

	return clk_div;
}

static void set_clk_div_en(unsigned int type, unsigned int ID, bool onoff)
{
	void __iomem *pll_con0 = NULL;
	int i;

	if (type != ABIST && type != ABIST_CK2)
		return;

	if ((ID <= 0) || (type == ABIST && ID >= FM_ABIST_NUM)
			|| (type == ABIST_CK2 && ID >= FM_ABIST_2_NUM))
		return;

	for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++) {
		if (fclks[i].type == type && fclks[i].id == ID
				&& fclks[i].grp != 0) {
			pll_con0 =  fm_base[FM_APMIXEDSYS] + fclks[i].ofs - 0x4;
			break;
		}
	}

	if ((i == (ARRAY_SIZE(fclks) - 1)) || pll_con0 == NULL)
		return;

	if (onoff) {
		// check ckdiv_en
		if (clk_readl(pll_con0) & FM_CKDIV_EN)
			fm_ckdiv_en = 1;
		// pll con0[17] = 1
		// select pll_ckdiv, enable pll_ckdiv, enable test clk
		clk_writel(pll_con0, (clk_readl(pll_con0) | FM_CKDIV_EN));
	} else {
		if (!fm_ckdiv_en)
			clk_writel(pll_con0, (clk_readl(pll_con0) & ~(FM_CKDIV_EN)));
	}
}

/* implement ckgen&abist api (example as below) */

static int __mt_get_freq(unsigned int ID, int type)
{
	void __iomem *dbg_addr = fm_base[FM_TOPCKGEN] + CLK_DBG_CFG;
	void __iomem *misc_addr = fm_base[FM_TOPCKGEN] + CLK_MISC_CFG_0;
	void __iomem *cali0_addr = fm_base[FM_TOPCKGEN] + CLK26CALI_0;
	void __iomem *cali1_addr = fm_base[FM_TOPCKGEN] + CLK26CALI_1;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;
	unsigned int clk_div = 1, post_div = 1;
	unsigned long flags;
	int output = 0, i = 0;

	fmeter_lock(flags);

	set_clk_div_en(type, ID, true);

	if (type == CKGEN && check_pdn(fm_base[FM_TOPCKGEN], CKGEN, ID)) {
		pr_notice("ID-%d: MUX PDN, return 0.\n", ID);
		fmeter_unlock(flags);
		return 0;
	}

	while (clk_readl(cali0_addr) & 0x10) {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	}

	/* CLK26CALI_0[15]: rst 1 -> 0 */
	clk_writel(cali0_addr, (clk_readl(cali0_addr) & ~(FM_RST_BITS)));
	/* CLK26CALI_0[15]: rst 0 -> 1 */
	clk_writel(cali0_addr, (clk_readl(cali0_addr) | FM_RST_BITS));

	if (type == CKGEN) {
		clk_dbg_cfg = clk_readl(dbg_addr);
		clk_writel(dbg_addr,
			(clk_dbg_cfg & 0xFFFF80FC) | (ID << 8) | (0x1));
	} else if (type == ABIST) {
		clk_dbg_cfg = clk_readl(dbg_addr);
		clk_writel(dbg_addr,
			(clk_dbg_cfg & 0xFF80FFFC) | (ID << 16));
	} else {
		fmeter_unlock(flags);
		return 0;
	}

	clk_misc_cfg_0 = clk_readl(misc_addr);
	clk_writel(misc_addr, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk26cali_1 = clk_readl(cali1_addr);
	clk_writel(cali0_addr, 0x9000);
	clk_writel(cali0_addr, 0x9010);

	/* wait frequency meter finish */
	i = 0;
	do {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	} while (clk_readl(cali0_addr) & 0x10);

	temp = clk_readl(cali1_addr) & 0xFFFF;

	if (type == ABIST)
		post_div = get_post_div(type, ID);

	clk_div = get_clk_div(type, ID);

	output = (temp * 26000) / 1024 * clk_div / post_div;

	set_clk_div_en(type, ID, false);

	clk_writel(dbg_addr, clk_dbg_cfg);
	clk_writel(misc_addr, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/

	clk_writel(cali0_addr, FM_RST_BITS);
	fmeter_unlock(flags);

	if (i > FM_TIMEOUT)
		return 0;

	if ((output * 4) < 1000) {
		pr_notice("%s(%d): CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			ID,
			clk_readl(dbg_addr),
			clk_readl(misc_addr),
			clk_readl(cali0_addr),
			clk_readl(cali1_addr));
	}

	return (output * 4);
}

/* implement ckgen&abist api (example as below) */

static int __mt_get_freq_ck2(unsigned int ID, int type)
{
	void __iomem *dbg_addr = fm_base[FM_TOPCKGEN] + CKSYS2_CLK_DBG_CFG;
	void __iomem *misc_addr = fm_base[FM_TOPCKGEN] + CKSYS2_CLK_MISC_CFG_0;
	void __iomem *cali0_addr = fm_base[FM_TOPCKGEN] + CKSYS2_CLK26CALI_0;
	void __iomem *cali1_addr = fm_base[FM_TOPCKGEN] + CKSYS2_CLK26CALI_1;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;
	unsigned int clk_div = 1, post_div = 1;
	unsigned long flags;
	int output = 0, i = 0;

	fmeter_lock(flags);

	set_clk_div_en(type, ID, true);

	if (type == CKGEN_CK2 && check_pdn(fm_base[FM_TOPCKGEN], CKGEN_CK2, ID)) {
		pr_notice("ID-%d: MUX PDN, return 0.\n", ID);
		fmeter_unlock(flags);
		return 0;
	}

	while (clk_readl(cali0_addr) & 0x10) {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	}

	/* CLK26CALI_0[15]: rst 1 -> 0 */
	clk_writel(cali0_addr, (clk_readl(cali0_addr) & ~(FM_RST_BITS)));
	/* CLK26CALI_0[15]: rst 0 -> 1 */
	clk_writel(cali0_addr, (clk_readl(cali0_addr) | FM_RST_BITS));

	if (type == CKGEN_CK2) {
		clk_dbg_cfg = clk_readl(dbg_addr);
		clk_writel(dbg_addr,
			(clk_dbg_cfg & 0xFFFF80FC) | (ID << 8) | (0x1));
	} else if (type == ABIST_CK2) {
		clk_dbg_cfg = clk_readl(dbg_addr);
		clk_writel(dbg_addr,
			(clk_dbg_cfg & 0xFFC0FFFC) | (ID << 16));
	} else {
		fmeter_unlock(flags);
		return 0;
	}

	clk_misc_cfg_0 = clk_readl(misc_addr);
	clk_writel(misc_addr, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk26cali_1 = clk_readl(cali1_addr);
	clk_writel(cali0_addr, 0x9000);
	clk_writel(cali0_addr, 0x9010);

	/* wait frequency meter finish */
	i = 0;
	do {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	} while (clk_readl(cali0_addr) & 0x10);

	temp = clk_readl(cali1_addr) & 0xFFFF;

	if (type == ABIST_CK2)
		post_div = get_post_div(type, ID);

	clk_div = get_clk_div(type, ID);

	output = (temp * 26000) / 1024 * clk_div / post_div;

	set_clk_div_en(type, ID, false);

	if (i > FM_TIMEOUT) {
		pr_notice("%s(%d): CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			ID,
			clk_readl(dbg_addr),
			clk_readl(misc_addr),
			clk_readl(cali0_addr),
			clk_readl(cali1_addr));
	}

	clk_writel(dbg_addr, clk_dbg_cfg);
	clk_writel(misc_addr, clk_misc_cfg_0);
	/*clk_writel(CKSYS2_CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CKSYS2_CLK26CALI_1, clk26cali_1);*/

	clk_writel(cali0_addr, FM_RST_BITS);
	fmeter_unlock(flags);

	if (i > FM_TIMEOUT)
		return 0;

	if ((output * 4) < 1000) {
		pr_notice("%s(%d): CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			ID,
			clk_readl(dbg_addr),
			clk_readl(misc_addr),
			clk_readl(cali0_addr),
			clk_readl(cali1_addr));
	}

	return (output * 4);
}

/* implement ckgen&abist api (example as below) */

static int __mt_get_freq2(unsigned int  type, unsigned int id)
{
	void __iomem *pll_con0 = fm_base[type] + subsys_fm[type].pll_con0;
	void __iomem *pll_con1 = fm_base[type] + subsys_fm[type].pll_con1;
	void __iomem *con0 = fm_base[type] + subsys_fm[type].con0;
	void __iomem *con1 = fm_base[type] + subsys_fm[type].con1;
	unsigned int temp, clk_div = 1, post_div = 1, ckdiv_en = -1;
	unsigned long flags;
	int output = 0, i = 0;

	fmeter_lock(flags);

	if (type != FM_VLP_CKSYS) {
		// check ckdiv_en
		if (clk_readl(pll_con0) & SUBSYS_CKDIV_EN)
			ckdiv_en = 1;
		// pll con0[19] = 1, pll con0[16] = 1, pll con0[12] = 1
		// select pll_ckdiv, enable pll_ckdiv, enable test clk
		clk_writel(pll_con0, (clk_readl(pll_con0) | (SUBSYS_CKDIV_EN | SUBSYS_TST_EN)));
	}

	/* PLL4H_FQMTR_CON1[15]: rst 1 -> 0 */
	clk_writel(con0, clk_readl(con0) & ~(FM_RST_BITS));
	/* PLL4H_FQMTR_CON1[15]: rst 0 -> 1 */
	clk_writel(con0, clk_readl(con0) | FM_RST_BITS);

	/* sel fqmtr_cksel */
	if (type == FM_VLP_CKSYS)
		clk_writel(con0, (clk_readl(con0) & 0xFFE0FFFF) | (id << 16));
	else
		clk_writel(con0, (clk_readl(con0) & 0x00FFFFF8) | (id << 0));
	/* set ckgen_load_cnt to 1024 */
	clk_writel(con1, (clk_readl(con1) & 0xFC00FFFF) | (0x1FF << 16));

	/* sel fqmtr_cksel and set ckgen_k1 to 0(DIV4) */
	clk_writel(con0, (clk_readl(con0) & 0x00FFFFFF) | (3 << 24));

	/* fqmtr_en set to 1, fqmtr_exc set to 0, fqmtr_start set to 0 */
	clk_writel(con0, (clk_readl(con0) & 0xFFFF8007) | 0x1000);
	/*fqmtr_start set to 1 */
	clk_writel(con0, clk_readl(con0) | 0x10);

	/* wait frequency meter finish */
	if (type == FM_VLP_CKSYS) {
		udelay(VLP_FM_WAIT_TIME);
	} else {
		while (clk_readl(con0) & 0x10) {
			udelay(10);
			i++;
			if (i > 30) {
				pr_notice("[%d]con0: 0x%x, con1: 0x%x\n",
					id, clk_readl(con0), clk_readl(con1));
				break;
			}
		}
	}

	temp = clk_readl(con1) & 0xFFFF;
	output = ((temp * 26000)) / 512; // Khz

	if (type != FM_VLP_CKSYS) {
		clk_div = (clk_readl(pll_con0) & SUBSYS_CKDIV_MASK) >> SUBSYS_CKDIV_SHIFT;
		if (ckdiv_en)
			clk_writel(pll_con0, (clk_readl(pll_con0) & ~(SUBSYS_TST_EN)));
		else
			clk_writel(pll_con0, (clk_readl(pll_con0)
				& ~(SUBSYS_CKDIV_EN | SUBSYS_TST_EN)));
	}

	if (clk_div == 0)
		clk_div = 1;

	if (type != FM_VLP_CKSYS)
		post_div = 1 << ((clk_readl(pll_con1) & FM_POSTDIV_MASK) >> FM_POSTDIV_SHIFT);

	clk_writel(con0, FM_RST_BITS);

	fmeter_unlock(flags);

	return (output * 4 * clk_div) / post_div;
}

static unsigned int mt6989_get_ckgen_freq(unsigned int ID)
{
	return __mt_get_freq(ID, CKGEN);
}

static unsigned int mt6989_get_abist_freq(unsigned int ID)
{
	return __mt_get_freq(ID, ABIST);
}

static unsigned int mt6989_get_ckgen_ck2_freq(unsigned int ID)
{
	return __mt_get_freq_ck2(ID, CKGEN_CK2);
}

static unsigned int mt6989_get_abist_ck2_freq(unsigned int ID)
{
	return __mt_get_freq_ck2(ID, ABIST_CK2);
}

static unsigned int mt6989_get_vlpck_freq(unsigned int ID)
{
	return __mt_get_freq2(FM_VLP_CKSYS, ID);
}

static unsigned int mt6989_get_subsys_freq(unsigned int ID)
{
	int output = 0;
	unsigned long flags;

	subsys_fmeter_lock(flags);

	if (ID >= FM_SYS_NUM)
		return 0;

	output = __mt_get_freq2(ID, FM_PLL_TST_CK);

	subsys_fmeter_unlock(flags);

	return output;
}

static unsigned int mt6989_get_fmeter_freq(unsigned int id,
		enum FMETER_TYPE type)
{
	if (type == CKGEN)
		return mt6989_get_ckgen_freq(id);
	else if (type == ABIST)
		return mt6989_get_abist_freq(id);
	else if (type == CKGEN_CK2)
		return mt6989_get_ckgen_ck2_freq(id);
	else if (type == ABIST_CK2)
		return mt6989_get_abist_ck2_freq(id);
	else if (type == SUBSYS)
		return mt6989_get_subsys_freq(id);
	else if (type == VLPCK)
		return mt6989_get_vlpck_freq(id);

	return FT_NULL;
}

// implement fmeter id for ulposc1/2

static int mt6989_get_fmeter_id(enum FMETER_ID fid)
{
	if (fid == FID_DISP_PWM)
		return FM_DISP_PWM_CK;
	else if (fid == FID_ULPOSC1)
		return FM_ULPOSC_CK;
	else if (fid == FID_ULPOSC2)
		return FM_ULPOSC2_CK;

	return FID_NULL;
}

static void __iomem *get_base_from_comp(const char *comp)
{
	struct device_node *node;
	static void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, comp);
	if (node) {
		base = of_iomap(node, 0);
		if (!base) {
			pr_err("%s() can't find iomem for %s\n",
					__func__, comp);
			return ERR_PTR(-EINVAL);
		}

		return base;
	}

	pr_err("%s can't find compatible node\n", __func__);

	return ERR_PTR(-EINVAL);
}

/*
 * init functions
 */

static struct fmeter_ops fm_ops = {
	.get_fmeter_clks = mt6989_get_fmeter_clks,
	.get_fmeter_freq = mt6989_get_fmeter_freq,
	.get_fmeter_id = mt6989_get_fmeter_id,
};

static int clk_fmeter_mt6989_probe(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < FM_SYS_NUM; i++) {
		fm_base[i] = get_base_from_comp(comp_list[i]);
		if (IS_ERR(fm_base[i]))
			goto ERR;

	}

	fmeter_set_ops(&fm_ops);

	return 0;
ERR:
	pr_err("%s(%s) can't find base\n", __func__, comp_list[i]);

	return -EINVAL;
}

static struct platform_driver clk_fmeter_mt6989_drv = {
	.probe = clk_fmeter_mt6989_probe,
	.driver = {
		.name = "clk-fmeter-mt6989",
		.owner = THIS_MODULE,
	},
};

static int __init clk_fmeter_init(void)
{
	static struct platform_device *clk_fmeter_dev;

	clk_fmeter_dev = platform_device_register_simple("clk-fmeter-mt6989", -1, NULL, 0);
	if (IS_ERR(clk_fmeter_dev))
		pr_warn("unable to register clk-fmeter device");

	return platform_driver_register(&clk_fmeter_mt6989_drv);
}

static void __exit clk_fmeter_exit(void)
{
	platform_driver_unregister(&clk_fmeter_mt6989_drv);
}

subsys_initcall(clk_fmeter_init);
module_exit(clk_fmeter_exit);
MODULE_LICENSE("GPL");
