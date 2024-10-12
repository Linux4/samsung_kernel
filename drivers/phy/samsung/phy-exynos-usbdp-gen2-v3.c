/*
 * phy-exynos-usbdp-gen2-v3.c
 *
 *  Created on: 2019. 5. 10.
 *      Author: wooseok oh
 */


#ifdef __KERNEL__

#ifndef __EXCITE__
#include <linux/delay.h>
#include <linux/io.h>
#endif

#include <linux/platform_device.h>

#else

#include "types.h"
#ifndef __BOOT__
#include "customfunctions.h"
#include "mct.h"
#else
#include <string.h>
#include <util.h>
#include <pwm.h>
#define dev_info(...) NULL
#define print_log(...) NULL
#endif

#endif

//#include "config.h"
#include "phy-samsung-usb-cal.h"

#include "phy-exynos-usb3p1-reg.h"
#include "phy-exynos-usbdp-gen2-v3.h"
#include "phy-exynos-usbdp-gen2-v3-reg.h"
#include "phy-exynos-usbdp-gen2-v3-reg-pcs.h"

#if defined(USBDP_GEN2_DBG)
static inline u32 usbdp_cal_reg_rd(void *addr)
{
	u32 reg;

	reg = readl(addr);
	print_log("[USB/DP]Rd Reg:0x%x\t\tVal:0x%x", addr, reg);
	return reg;
}

static inline void usbdp_cal_reg_wr(u32 val, void *addr)
{
	print_log("[USB/DP]Wr Reg:0x%x\t\tVal:0x%x", addr, val);
	writel(val, addr);
}
#else
#define usbdp_cal_reg_rd(addr)		readl(addr)
#define usbdp_cal_reg_wr(val, addr)	writel(val, addr)
#endif

void phy_exynos_usbdp_g2_v3_tune_each(struct exynos_usbphy_info *info, char *name, u32 val)
{
	void __iomem *regs_base = info->pma_base;
	void __iomem *regs_base2 = info->link_base;
	void __iomem *regs_base3 = info->pcs_base;

	u32 reg = 0;

	if (!name)
		return;

	if (val == -1)
		return;

	if (!strcmp(name, "ssrx_sqhs_th")) {
		/* RX squelch detect threshold level */
		/* LN0 TX */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_26E_09B8);
		reg &= USBDP_V3_TRSV_26E_LN0_RX_SQHS_TH_CLR;
		reg |= USBDP_V3_TRSV_26E_LN0_RX_SQHS_TH(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_26E_09B8);
		/* LN2 TX */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_066E);
		reg &= USBDP_V3_TRSV_066E_LN2_RX_SQHS_TH_CLR;
		reg |= USBDP_V3_TRSV_066E_LN2_RX_SQHS_TH(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_066E);

	} else if (!strcmp(name, "ssrx_lfps_th")) {
		/* lfps detect threshold control */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_0270);
		reg &= USBDP_V3_TRSV_0270_LN0_RX_LFPS_TH_CTRL_CLR;
		reg |= USBDP_V3_TRSV_0270_LN0_RX_LFPS_TH_CTRL(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_0270);
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_0670);
		reg &= USBDP_V3_TRSV_0670_LN2_RX_LFPS_TH_CTRL_CLR;
		reg |= USBDP_V3_TRSV_0670_LN2_RX_LFPS_TH_CTRL(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_0670);

	} else if (!strcmp(name, "ssrx_mf_eq_ctrl_ss")) {
		/* RX MF EQ CONTROL */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_0311);
		reg &= USBDP_V3_TRSV_0311_LN0_RX_SSLMS_MF_INIT_RATE_SP_CLR;
		reg |= USBDP_V3_TRSV_0311_LN0_RX_SSLMS_MF_INIT_RATE_SP(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_0311);
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_0711);
		reg &= USBDP_V3_TRSV_0711_LN2_RX_SSLMS_MF_INIT_RATE_SP_CLR;
		reg |= USBDP_V3_TRSV_0711_LN2_RX_SSLMS_MF_INIT_RATE_SP(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_0711);
	} else if (!strcmp(name, "ssrx_hf_eq_ctrl_ss")) {
		/* RX HF EQ CONTROL */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_030B);
		reg &= USBDP_V3_TRSV_030B_LN0_RX_SSLMS_HF_INIT_RATE_SP_CLR;
		reg |= USBDP_V3_TRSV_030B_LN0_RX_SSLMS_HF_INIT_RATE_SP(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_030B);
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_070B);
		reg &= USBDP_V3_TRSV_070B_LN2_RX_SSLMS_HF_INIT_RATE_SP_CLR;
		reg |= USBDP_V3_TRSV_070B_LN2_RX_SSLMS_HF_INIT_RATE_SP(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_070B);
	} else if (!strcmp(name, "ssrx_mf_eq_ctrl_ssp")) {
		/* RX MF EQ CONTROL */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_0312);
		reg &= USBDP_V3_TRSV_0312_LN0_RX_SSLMS_MF_INIT_RATE_SSP_CLR;
		reg |= USBDP_V3_TRSV_0312_LN0_RX_SSLMS_MF_INIT_RATE_SSP(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_0312);
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_0712);
		reg &= USBDP_V3_TRSV_0712_LN2_RX_SSLMS_MF_INIT_RATE_SSP_CLR;
		reg |= USBDP_V3_TRSV_0712_LN2_RX_SSLMS_MF_INIT_RATE_SSP(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_0712);
	} else if (!strcmp(name, "ssrx_hf_eq_ctrl_ssp")) {
		/* RX HF EQ CONTROL */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_030C);
		reg &= USBDP_V3_TRSV_030C_LN0_RX_SSLMS_HF_INIT_RATE_SSP_CLR;
		reg |= USBDP_V3_TRSV_030C_LN0_RX_SSLMS_HF_INIT_RATE_SSP(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_030C);
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_070C);
		reg &= USBDP_V3_TRSV_70C_LN2_RX_SSLMS_HF_INIT_RATE_SSP_CLR;
		reg |= USBDP_V3_TRSV_70C_LN2_RX_SSLMS_HF_INIT_RATE_SSP(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_070C);
	} else if (!strcmp(name, "ssrx_dfe1_tap_ctrl")) {
		/* RX DFE1 TAp CONTROL */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_0279);
		reg &= USBDP_V3_TRSV_0279_LN0_RX_SSLMS_C1_INIT_CLR;
		reg |= USBDP_V3_TRSV_0279_LN0_RX_SSLMS_C1_INIT(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_0279);
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_0679);
		reg &= USBDP_V3_TRSV_0679_LN2_RX_SSLMS_C1_INIT_CLR;
		reg |= USBDP_V3_TRSV_0679_LN2_RX_SSLMS_C1_INIT(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_0679);

	} else if (!strcmp(name, "ssrx_dfe2_tap_ctrl")) {
		/* RX DFE2 TAp CONTROL */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_027A);
		reg &= USBDP_V3_TRSV_027A_LN0_RX_SSLMS_C2_INIT_CLR;
		reg |= USBDP_V3_TRSV_027A_LN0_RX_SSLMS_C2_INIT(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_027A);
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_067A);
		reg &= USBDP_V3_TRSV_067A_LN2_RX_SSLMS_C2_INIT_CLR;
		reg |= USBDP_V3_TRSV_067A_LN2_RX_SSLMS_C2_INIT(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_067A);

	} else if (!strcmp(name, "ssrx_dfe3_tap_ctrl")) {
		/* RX DFE3 TAp CONTROL */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_027B);
		reg &= USBDP_V3_TRSV_027B_LN0_RX_SSLMS_C3_INIT_CLR;
		reg |= USBDP_V3_TRSV_027B_LN0_RX_SSLMS_C3_INIT(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_027B);
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_067B);
		reg &= USBDP_V3_TRSV_067B_LN2_RX_SSLMS_C3_INIT_CLR;
		reg |= USBDP_V3_TRSV_067B_LN2_RX_SSLMS_C3_INIT(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_067B);

	} else if (!strcmp(name, "ssrx_dfe4_tap_ctrl")) {
		/* RX DFE4 TAp CONTROL */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_027C);
		reg &= USBDP_V3_TRSV_027C_LN0_RX_SSLMS_C4_INIT_CLR;
		reg |= USBDP_V3_TRSV_027C_LN0_RX_SSLMS_C4_INIT(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_027C);
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_067C);
		reg &= USBDP_V3_TRSV_067C_LN2_RX_SSLMS_C4_INIT_CLR;
		reg |= USBDP_V3_TRSV_067C_LN2_RX_SSLMS_C4_INIT(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_067C);

	} else if (!strcmp(name, "ssrx_dfe5_tap_ctrl")) {
		/* RX DFE5 TAp CONTROL */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_027D);
		reg &= USBDP_V3_TRSV_027D_LN0_RX_SSLMS_C5_INIT_CLR;
		reg |= USBDP_V3_TRSV_027D_LN0_RX_SSLMS_C5_INIT(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_027D);
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_067D);
		reg &= USBDP_V3_TRSV_067D_LN2_RX_SSLMS_C5_INIT_CLR;
		reg |= USBDP_V3_TRSV_067D_LN2_RX_SSLMS_C5_INIT(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_067D);

	} else if (!strcmp(name, "ssrx_term_cal")) {
		/* RX termination calibration */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_02BD);
		reg &= USBDP_V3_TRSV_02BD_LN0_RX_RTERM_CTRL_CLR;
		reg |= USBDP_V3_TRSV_02BD_LN0_RX_RTERM_CTRL(val);
		reg |= USBDP_V3_TRSV_02BD_LN0_RX_RCAL_OPT_CODE(0x1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_02BD);
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_06BD);
		reg &= USBDP_V3_TRSV_06BD_LN2_RX_RTERM_CTRL_CLR;
		reg |= USBDP_V3_TRSV_06BD_LN2_RX_RTERM_CTRL(val);
		reg |= USBDP_V3_TRSV_06BD_LN2_RX_RCAL_OPT_CODE(0x1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_06BD);

	} else if (!strcmp(name, "gen1_tx_amp")) {
		/* Gen1 TX Amplitude */
		reg = usbdp_cal_reg_rd(regs_base3 + EXYNOS_PCS_LEQ_HS_TX_COEF_MAP_0);
		reg &= PCS_LEQ_HS_TX_COEF_MAP_0_AMP_CLR;
		reg |= PCS_LEQ_HS_TX_COEF_MAP_0_AMP(val);
		usbdp_cal_reg_wr(reg, regs_base3 + EXYNOS_PCS_LEQ_HS_TX_COEF_MAP_0);

	} else if (!strcmp(name, "gen1_tx_deemp")) {
		/* Gen1 TX de-emphasise */
		reg = usbdp_cal_reg_rd(regs_base3 + EXYNOS_PCS_LEQ_HS_TX_COEF_MAP_0);
		reg &= PCS_LEQ_HS_TX_COEF_MAP_0_DEEMPH_CLR;
		reg |= PCS_LEQ_HS_TX_COEF_MAP_0_DEEMPH(val);
		usbdp_cal_reg_wr(reg, regs_base3 + EXYNOS_PCS_LEQ_HS_TX_COEF_MAP_0);

	} else if (!strcmp(name, "gen1_tx_pre_shoot")) {
		/* Gen1 TX pre shoot */
		reg = usbdp_cal_reg_rd(regs_base3 + EXYNOS_PCS_LEQ_HS_TX_COEF_MAP_0);
		reg &= PCS_LEQ_HS_TX_COEF_MAP_0_PRESHOOT_CLR;
		reg |= PCS_LEQ_HS_TX_COEF_MAP_0_PRESHOOT(val);
		usbdp_cal_reg_wr(reg, regs_base3 + EXYNOS_PCS_LEQ_HS_TX_COEF_MAP_0);

	} else if (!strcmp(name, "gen2_tx_amp")) {
		/* Gen2 TX Amplitude */
		reg = readl(regs_base3 + USBDP_PCS_V3_LEQ_LOCAL_COEF);
		reg &= USBDP_PCS_V3_LEQ_LOCAL_COEF_PMA_CENTER_COEF_CLR;
		reg |= USBDP_PCS_V3_LEQ_LOCAL_COEF_PMA_CENTER_COEF_SET(val);
		writel(reg, regs_base3 + USBDP_PCS_V3_LEQ_LOCAL_COEF);

	} else if (!strcmp(name, "gen2_tx_deemp")) {
		/* Gen2 TX DeEmphasis */
		reg = usbdp_cal_reg_rd(regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH);
		reg &= USBDRD_LCSR_TX_DEEMPH_CLR;
		reg |= USBDRD_LCSR_TX_DEEMPH(val);
		usbdp_cal_reg_wr(reg, regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH);

		// CP14_Only_Deemphasis - rLCSR_TX_DEEMPH_2(4bit fo 6-bit field is valid)
		reg = readl(regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH_2);
		reg &= USBDRD_LCSR_TX_DEEMPH_2_CLR;
		reg &= USBDRD_LCSR_TX_AMP_2_CLR;
		reg &= USBDRD_LCSR_TX_PRESHOOT_2_CLR;
		reg |= USBDRD_LCSR_TX_DEEMPH_2(val);
		writel(reg, regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH_2);


	} else if (!strcmp(name, "gen2_tx_pre_shoot")) {
		/* Gen2 TX pre_shoot */
		reg = usbdp_cal_reg_rd(regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH);
		reg &= USBDRD_LCSR_TX_PRESHOOT_CLR;
		reg |= USBDRD_LCSR_TX_PRESHOOT(val);
		usbdp_cal_reg_wr(reg, regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH);

		// CP13_Only_Preshoot - rLCSR_TX_DEEMPH_1 (4bit fo 6-bit field is valid)
		reg = readl(regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH_1);
		reg &= USBDRD_LCSR_TX_DEEMPH_1_CLR;
		reg &= USBDRD_LCSR_TX_AMP_1_CLR;
		reg &= USBDRD_LCSR_TX_PRESHOOT_1_CLR;
		reg |= USBDRD_LCSR_TX_PRESHOOT_1(val);
		writel(reg, regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH_1);

	} else if (!strcmp(name, "tx_idrv_up")) {
		/* TX idrv up */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_0407);
		reg &= USBDP_V3_TRSV_0407_LN1_TX_DRV_IDRV_IUP_CTRL_CLR;
		reg |= USBDP_V3_TRSV_0407_LN1_TX_DRV_IDRV_IUP_CTRL(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_0407);
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_COM_TRSV_0807);
		reg &= USBDP_V3_TRSV_0807_LN3_TX_DRV_IDRV_IUP_CTRL_CLR;
		reg |= USBDP_V3_TRSV_0807_LN3_TX_DRV_IDRV_IUP_CTRL(val);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_COM_TRSV_0807);

	}
}

void phy_exynos_usbdp_g2_v3_tune(struct exynos_usbphy_info *info)
{
	u32 cnt = 0;

	if (!info) {
		return;
	}

	if (!info->tune_param) {
		return;
	}

	for (; info->tune_param[cnt].value != EXYNOS_USB_TUNE_LAST; cnt++) {
		char *para_name;
		int val;

		val = info->tune_param[cnt].value;
		if (val == -1) {
			continue;
		}
		para_name = info->tune_param[cnt].name;
		if (!para_name) {
			break;
		}
		phy_exynos_usbdp_g2_v3_tune_each(info, para_name, val);
	}
}

void phy_exynos_usbdp_g2_v3_set_dtb_mux(struct exynos_usbphy_info *info, int mux_val)
{
	// TODO
}

static void phy_exynos_usbdp_g2_v3_ctrl_pma_ready(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->ctrl_base;
	u32 reg;

	/* link pipe_clock selection to pclk of PMA */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBCON_CLKRST);
	reg |= CLKRST_LINK_PCLK_SEL;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBCON_CLKRST);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
	reg |= PMA_REF_FREQ_SEL_SET(1);
	/* SFR reset */
	reg |= PMA_LOW_PWR;
	reg |= PMA_APB_SW_RST;
	/* reference clock 26MHz path from XTAL */
	reg &= ~PMA_ROPLL_REF_CLK_SEL_MASK;
	reg &= ~PMA_LCPLL_REF_CLK_SEL_MASK;
	/* PMA_POWER_OFF */
	reg |= PMA_TRSV_SW_RST;
	reg |= PMA_CMN_SW_RST;
	reg |= PMA_INIT_SW_RST;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);

	udelay(1);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
	reg &= ~PMA_LOW_PWR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);

	/* APB enable */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
	reg &= ~PMA_APB_SW_RST;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
}

static void phy_exynos_usbdp_g2_v3_pma_default_sfr_update(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	/* base
	 * EVT0 ML4_DEV07
	 * TN_DIPD_USBDP_NEUS_EVT0_SFR.xls ver.0.0 */

	/* ========================================================================
	 * Common
	 */

	/* PLL proportional path charge-pump current contorl 0x64 = 0x77 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG0019);
		reg &= CMN_REG0019_LCPLL_ANA_CPP_CTRL_COARSE_CLR;
		reg |= CMN_REG0019_LCPLL_ANA_CPP_CTRL_COARSE_SET(0x7);
		reg &= CMN_REG0019_LCPLL_ANA_CPP_CTRL_FINE_CLR;
		reg |= CMN_REG0019_LCPLL_ANA_CPP_CTRL_FINE_SET(0x7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG0019);

	/* ana lcpll 0x104 = 0x44 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG0041);
		reg &= CMN_REG0041_ANA_LCPLL_RESERVED_CLR;
		reg |= CMN_REG0041_ANA_LCPLL_RESERVED_SET(0x44);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG0041);

	/* ropll beacon lfps out 0x234 = 0xE8 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG008D);
		reg &= CMN_REG008D_OVRD_ROPLL_BEACON_LFPS_OUT_EN_CLR;
		reg |= CMN_REG008D_OVRD_ROPLL_BEACON_LFPS_OUT_EN_SET(0x1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG008D);

	/* ana ropll 0x248 = 0x44 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG0092);
		reg &= CMN_REG0092_ANA_ROPLL_RESERVED_CLR;
		reg |= CMN_REG0092_ANA_ROPLL_RESERVE_SET(0x44);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG0092);

	/* rx_cdr_data_mode_exit 0x28c = 0x18 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00A3);
		reg &= CMN_REG00A3_OVRD_RX_CDR_DATA_MODE_EXIT_CLR;
		reg |= CMN_REG00A3_OVRD_RX_CDR_DATA_MODE_EXIT_SET(0x1);
		reg &= CMN_REG00A3_RX_CDR_DATA_MODE_EXIT_CLR;
		reg |= CMN_REG00A3_RX_CDR_DATA_MODE_EXIT_SET(0x1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00A3);

	/* ln0_ana_tx_drv_idrv_iup_ctrl 0x81c = 0xe5 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0207);
		reg &= TRSV_REG0207_LN0_ANA_TX_DRV_IDRV_IUP_CTRL_CLR;
		reg |= TRSV_REG0207_LN0_ANA_TX_DRV_IDRV_IUP_CTRL_SET(0x7);
		reg &= TRSV_REG0207_LN0_ANA_TX_DRV_BEACON_IDRV_DELAY_CTRL_CLR;
		reg |= TRSV_REG0207_LN0_ANA_TX_DRV_BEACON_IDRV_DELAY_CTRL_SET(0x1);
		reg &= TRSV_REG0207_LN0_ANA_TX_DRV_ACCDRV_EN_CLR;
		reg &= TRSV_REG0207_LN0_ANA_TX_DRV_ACCDRV_POL_SEL_CLR;
		reg |= TRSV_REG0207_LN0_ANA_TX_DRV_ACCDRV_POL_SEL_SET(0x1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0207);

	/* ln0_ana_tx 0x878 = 0x00 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG021E);
		reg &= TRSV_REG021E_LN0_ANA_TX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG021E);

	/* ln0_rx_dfe_vga_rl_ctrl_sp/ssp 0x994 = 0x1c */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0265);
		reg &= TRSV_REG0265_LN0_RX_DFE_VGA_RL_CTRL_SP_CLR;
		reg |= TRSV_REG0265_LN0_RX_DFE_VGA_RL_CTRL_SP_SET(0x3);
		reg &= TRSV_REG0265_LN0_RX_DFE_VGA_RL_CTRL_SSP_CLR;
		reg |= TRSV_REG0265_LN0_RX_DFE_VGA_RL_CTRL_SSP_SET(0x4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0265);

	/* ln0_rx_sigval_lpf_bypass 0xaf0 = 0x00 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02BC);
		reg &= TRSV_REG02BC_LN0_TG_RX_SIGVAL_LPF_DELAY_TIME_CLR;
		reg &= TRSV_REG02BC_LN0_RX_CTLE_EQ_HF_MAN_SEL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02BC);

	/* override enable flag for ln0_rx_rcal_rstns 0xaf4 = 0x15 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02BD);
		reg &= TRSV_REG02BD_LN0_RX_RCAL_OPT_CODE_CLR;
		reg |= TRSV_REG02BD_LN0_RX_RCAL_OPT_CODE_SET(0x1);
		reg &= TRSV_REG02BD_LN0_RX_RTERM_CTRL_CLR;
		reg |= TRSV_REG02BD_LN0_RX_RTERM_CTRL_SET(0x5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02BD);

	/* ln0_rx_sslms_hf_init_rate_ssp 0xc30 = 0x03 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG030C);
		reg &= TRSV_REG030C_LN0_RX_SSLMS_HF_INIT_RATE_SSP_CLR;
		reg |= TRSV_REG030C_LN0_RX_SSLMS_HF_INIT_RATE_SSP_SET(0x03);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG030C);

	/* ln0_rx_sslms_mf_init_rate_ssp 0xc48 = 0x03 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG030C);
		reg &= TRSV_REG0312_LN0_RX_SSLMS_MF_INIT_RATE_SSP_CLR;
		reg |= TRSV_REG0312_LN0_RX_SSLMS_MF_INIT_RATE_SSP_SET(0x03);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG030C);

	/* ln1_ana_tx_reserved 0x1078 = 0x00 */
		//reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG041E);
		//reg &= TRSV_REG041E_LN1_ANA_TX_RESERVED_CLR;
		//usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG041E);

	/* ln2_ana_tx_drv_accdrv_en 0x181c = 0xe5 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0607);
		reg &= TRSV_REG0607_LN2_ANA_TX_DRV_IDRV_IUP_CTRL_CLR;
		reg |= TRSV_REG0607_LN2_ANA_TX_DRV_IDRV_IUP_CTRL_SET(0x7);
		reg &= TRSV_REG0607_LN2_ANA_TX_DRV_BEACON_IDRV_DELAY_CTRL_CLR;
		reg |= TRSV_REG0607_LN2_ANA_TX_DRV_BEACON_IDRV_DELAY_CTRL_SET(0x1);
		reg &= TRSV_REG0607_LN2_ANA_TX_DRV_ACCDRV_EN_CLR;
		reg &= TRSV_REG0607_LN2_ANA_TX_DRV_ACCDRV_POL_SEL_CLR;
		reg |= TRSV_REG0607_LN2_ANA_TX_DRV_ACCDRV_POL_SEL_SET(0x1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0607);

	/* ln2_ana_tx_reserved=0x00 0x1878 = 0x00 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG061E);
		reg &= TRSV_REG061E_LN2_ANA_TX_RESERVED_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG061E);

	/* ln2_rx_dfe_vga_rl_ctrl_sp/ssp 0x1994 = 0x1c */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0665);
		reg &= TRSV_REG0665_LN2_RX_DFE_VGA_RL_CTRL_SP_CLR;
		reg |= TRSV_REG0665_LN2_RX_DFE_VGA_RL_CTRL_SP_SET(0x3);
		reg &= TRSV_REG0665_LN2_RX_DFE_VGA_RL_CTRL_SSP_CLR;
		reg |= TRSV_REG0665_LN2_RX_DFE_VGA_RL_CTRL_SSP_SET(0x4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0665);

	/* ln2_rx_sigval_lpf_bypass 0x1af0 = 0x00 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06BC);
		reg &= TRSV_REG06BC_LN2_TG_RX_SIGVAL_LPF_DELAY_TIME_CLR;
		reg &= TRSV_REG06BC_LN2_RX_CTLE_EQ_HF_MAN_SEL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06BC);

	/* override enable flag for ln2_rx_rcal_rstns 0x1af4 = 0x15 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06BD);
		reg &= TRSV_REG06BD_LN2_RX_RCAL_OPT_CODE_CLR;
		reg |= TRSV_REG06BD_LN2_RX_RCAL_OPT_CODE_SET(0x1);
		reg &= TRSV_REG06BD_LN2_RX_RTERM_CTRL_CLR;
		reg |= TRSV_REG06BD_LN2_RX_RTERM_CTRL_SET(0x5);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06BD);

	/* ln2_rx_sslms_hf_init_rate_ssp 0x1c30 = 0x03 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG070C);
		reg &= TRSV_REG070C_LN2_RX_SSLMS_HF_INIT_RATE_SSP_CLR;
		reg |= TRSV_REG070C_LN2_RX_SSLMS_HF_INIT_RATE_SSP_SET(0x03);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG070C);

	/* ln2_rx_sslms_mf_init_rate_ssp 0x1c48 = 0x03 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0712);
		reg &= TRSV_REG0712_LN2_RX_SSLMS_MF_INIT_RATE_SSP_CLR;
		reg |= TRSV_REG0712_LN2_RX_SSLMS_MF_INIT_RATE_SSP_SET(0x03);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0712);

	/* ln3_ana_tx_reserved 0x2078 = 0x00 */
		//reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG081E);
		//reg &= TRSV_REG081E_LN3_ANA_TX_RESERVED_CLR;
		//usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG081E);

	/* ofs_cal_rate_change_restart_en 0x0428 = 0x60 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG010A);
		reg &= CMN_REG010A_POWER_OFF_CLR;
		reg &= CMN_REG010A_LCPLL_RATE_CHANGE_RESTART_EN_CLR;
		reg |= CMN_REG010A_LCPLL_RATE_CHANGE_RESTART_EN_SET(0x1);
		reg &= CMN_REG010A_CDR_RATE_CHANGE_RESTART_EN_CLR;
		reg |= CMN_REG010A_CDR_RATE_CHANGE_RESTART_EN_SET(0x1);
		reg &= CMN_REG010A_OFS_CAL_RATE_CHANGE_RESTART_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG010A);

	/* ln0_ana_rx_sqhs_th_ctrl_sp/ssp 0x0d58 = 0x33 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0356);
		reg &= TRSV_REG0356_LN0_ANA_RX_SQHS_TH_CTRL_SP_CLR;
		reg |= TRSV_REG0356_LN0_ANA_RX_SQHS_TH_CTRL_SP_SET(0x3);
		reg &= TRSV_REG0356_LN0_ANA_RX_SQHS_TH_CTRL_SSP_CLR;
		reg |= TRSV_REG0356_LN0_ANA_RX_SQHS_TH_CTRL_SSP_SET(0x3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0356);

	/* ln2_ana_rx_sqhs_th_ctrl_sp/ssp 0x1d58 = 0x33 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0756);
		reg &= TRSV_REG0756_LN2_ANA_RX_SQHS_TH_CTRL_SP_CLR;
		reg |= TRSV_REG0756_LN2_ANA_RX_SQHS_TH_CTRL_SP_SET(0x3);
		reg &= TRSV_REG0756_LN2_ANA_RX_SQHS_TH_CTRL_SSP_CLR;
		reg |= TRSV_REG0756_LN2_ANA_RX_SQHS_TH_CTRL_SSP_SET(0x3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0756);

	/* ln0_ana_rx_dfe_add_rl_ctrl 0xe38 = 0x06 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG038E);
		reg &= TRSV_REG038E_LN0_ANA_RX_DFE_ADD_RL_CTRL_CLR;
		reg |= TRSV_REG038E_LN0_ANA_RX_DFE_ADD_RL_CTRL_SET(0x6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG038E);

	/* ln2_ana_rx_dfe_add_rl_ctrl 0x1e38 = 0x06 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG078E);
		reg &= TRSV_REG078E_LN2_ANA_RX_DFE_ADD_RL_CTRL_CLR;
		reg |= TRSV_REG078E_LN2_ANA_RX_DFE_ADD_RL_CTRL_SET(0x6);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG078E);

	/* ln0_ana_rx_ctle_rl_hf_ctrl_g4 0x08fc = 0x3a */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG023F);
		reg &= TRSV_REG023F_LN0_RX_CTLE_RL_HF_CTRL_SSP_CLR;
		reg |= TRSV_REG023F_LN0_RX_CTLE_RL_HF_CTRL_SSP_SET(0x7);
		reg &= TRSV_REG023F_LN0_RX_CTLE_RL_HF_CTRL_RBR_CLR;
		reg |= TRSV_REG023F_LN0_RX_CTLE_RL_HF_CTRL_RBR_SET(0x2);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG023F);

	/* ln2_ana_rx_ctle_rl_hf_ctrl_g4 0x18fc = 0x3a */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG063F);
		reg &= TRSV_REG063F_LN2_RX_CTLE_RL_HF_CTRL_SSP_CLR;
		reg |= TRSV_REG063F_LN2_RX_CTLE_RL_HF_CTRL_SSP_SET(0x7);
		reg &= TRSV_REG063F_LN2_RX_CTLE_RL_HF_CTRL_RBR_CLR;
		reg |= TRSV_REG063F_LN2_RX_CTLE_RL_HF_CTRL_RBR_SET(0x2);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG063F);

	/* ln0_ana_rx_dfe_dac_vcm_ctrl 0x0990 = 0x74 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0264);
		reg &= TRSV_REG0264_LN0_ANA_RX_DFE_DAC_VCM_CTRL_CLR;
		reg |= TRSV_REG0264_LN0_ANA_RX_DFE_DAC_VCM_CTRL_SET(0x7);
		reg &= TRSV_REG0264_LN0_ANA_RX_DFE_DAC_LSB_CTRL_CLR;
		reg |= TRSV_REG0264_LN0_ANA_RX_DFE_DAC_LSB_CTRL_SET(0x1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0264);

	/* ln2_ana_rx_dfe_dac_vcm_ctrl 0x1990 = 0x74 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0664);
		reg &= TRSV_REG0664_LN2_ANA_RX_DFE_DAC_VCM_CTRL_CLR;
		reg |= TRSV_REG0664_LN2_ANA_RX_DFE_DAC_VCM_CTRL_SET(0x7);
		reg &= TRSV_REG0664_LN2_ANA_RX_DFE_DAC_LSB_CTRL_CLR;
		reg |= TRSV_REG0664_LN2_ANA_RX_DFE_DAC_LSB_CTRL_SET(0x1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0664);

	/* ln0_ana_rx_dfe_vga_i_ctrl_sp 0x0d64 = 0x17 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0359);
		reg &= TRSV_REG0359_LN0_ANA_RX_DFE_VGA_I_CTRL_SP_CLR;
		reg |= TRSV_REG0359_LN0_ANA_RX_DFE_VGA_I_CTRL_SP_SET(0x2);
		reg &= TRSV_REG0359_LN0_ANA_RX_DFE_VGA_I_CTRL_SSP_CLR;
		reg |= TRSV_REG0359_LN0_ANA_RX_DFE_VGA_I_CTRL_SSP_SET(0x7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0359);

	/* ln2_ana_rx_dfe_vga_i_ctrl_sp 0x1d64 = 0x17 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0759);
		reg &= TRSV_REG0759_LN2_ANA_RX_DFE_VGA_I_CTRL_SP_CLR;
		reg |= TRSV_REG0759_LN2_ANA_RX_DFE_VGA_I_CTRL_SP_SET(0x2);
		reg &= TRSV_REG0759_LN2_ANA_RX_DFE_VGA_I_CTRL_SSP_CLR;
		reg |= TRSV_REG0759_LN2_ANA_RX_DFE_VGA_I_CTRL_SSP_SET(0x7);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0759);

	/* ln0_rx_cdr_vco_freq_boost_hbr2 0x08d0 = 0x34 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0234);
		reg &= TRSV_REG0234_LN0_RX_CDR_VCO_FREQ_BOOST_HBR2_CLR;
		reg |= TRSV_REG0234_LN0_RX_CDR_VCO_FREQ_BOOST_HBR2_SET(0x3);
		reg &= TRSV_REG0234_LN0_RX_CDR_VCO_FREQ_BOOST_HBR3_CLR;
		reg |= TRSV_REG0234_LN0_RX_CDR_VCO_FREQ_BOOST_HBR3_SET(0x4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0234);

	/* ln2_rx_cdr_vco_freq_boost_hbr2 0x18d0 = 0x34 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0634);
		reg &= TRSV_REG0634_LN2_RX_CDR_VCO_FREQ_BOOST_HBR2_CLR;
		reg |= TRSV_REG0634_LN2_RX_CDR_VCO_FREQ_BOOST_HBR2_SET(0x3);
		reg &= TRSV_REG0634_LN2_RX_CDR_VCO_FREQ_BOOST_HBR3_CLR;
		reg |= TRSV_REG0634_LN2_RX_CDR_VCO_FREQ_BOOST_HBR3_SET(0x4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0634);

	/* ln0_rx_cdr_vco_freq_boost_sp 0x08c8 = 0x13 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0232);
		reg &= TRSV_REG0232_LN0_RX_CDR_VCO_FREQ_BOOST_SP_CLR;
		reg |= TRSV_REG0232_LN0_RX_CDR_VCO_FREQ_BOOST_SP_SET(0x1);
		reg &= TRSV_REG0232_LN0_RX_CDR_VCO_FREQ_BOOST_SSP_CLR;
		reg |= TRSV_REG0232_LN0_RX_CDR_VCO_FREQ_BOOST_SSP_SET(0x3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0232);

	/* ln2_rx_cdr_vco_freq_boost_sp 0x18c8 = 0x13 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0632);
		reg &= TRSV_REG0632_LN2_RX_CDR_VCO_FREQ_BOOST_SP_CLR;
		reg |= TRSV_REG0632_LN2_RX_CDR_VCO_FREQ_BOOST_SP_SET(0x1);
		reg &= TRSV_REG0632_LN2_RX_CDR_VCO_FREQ_BOOST_SSP_CLR;
		reg |= TRSV_REG0632_LN2_RX_CDR_VCO_FREQ_BOOST_SSP_SET(0x3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0632);


	/* ln0_rx_sslms_c0_o_init_sp 0x0dc0 = 0x40 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0370);
		reg &= TRSV_REG0370_LN0_RX_SSLMS_C0_O_INIT_SP_CLR;
		reg |= TRSV_REG0370_LN0_RX_SSLMS_C0_O_INIT_SP_SET(0x40);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0370);

	/* ln2_rx_sslms_c0_o_init_sp 0x1dc0 = 0x40 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0770);
		reg &= TRSV_REG0770_LN2_RX_SSLMS_C0_O_INIT_SP_CLR;
		reg |= TRSV_REG0770_LN2_RX_SSLMS_C0_O_INIT_SP_SET(0x40);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0770);

	/* ln0_rx_sslms_c0_e_init_ssp 0x0da8 = 0x40 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG036A);
		reg &= TRSV_REG036A_LN0_RX_SSLMS_C0_E_INIT_SSP_CLR;
		reg |= TRSV_REG036A_LN0_RX_SSLMS_C0_E_INIT_SSP_SET(0x40);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG036A);

	/* ln2_rx_sslms_c0_e_init_ssp 0x1da8 = 0x40 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG076A);
		reg &= TRSV_REG076A_LN2_RX_SSLMS_C0_E_INIT_SSP_CLR;
		reg |= TRSV_REG076A_LN2_RX_SSLMS_C0_E_INIT_SSP_SET(0x40);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG076A);

	/* ln0_rx_sslms_c0_o_init_ssp 0x0dd8 = 0x40 - 190531 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0376);
		reg &= TRSV_REG0376_LN0_RX_SSLMS_C0_O_INIT_SSP_CLR;
		reg |= TRSV_REG0376_LN0_RX_SSLMS_C0_O_INIT_SSP_SET(0x40);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0376);

	/* ln2_rx_sslms_c0_o_init_ssp 0x1dd8 = 0x40 - 190531 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0776);
		reg &= TRSV_REG0776_LN2_RX_SSLMS_C0_O_INIT_SSP_CLR;
		reg |= TRSV_REG0776_LN2_RX_SSLMS_C0_O_INIT_SSP_SET(0x40);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0776);

	/* ln0_rx_sslms_c0_o_init_ssp 0x0dd8 = 0x40 - 190531 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0364);
		reg &= TRSV_REG0364_LN0_RX_SSLMS_C0_E_INIT_SS_CLR;
		reg |= TRSV_REG0364_LN0_RX_SSLMS_C0_E_INIT_SS_SET(0x40);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0364);
	/* ANA_LCPLL_CD_RSTN_WEST/WEST_SEL 0x03c0 = 0x30 - 190531 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00F0);
		reg &= CMN_REG00F0_ANA_LCPLL_CD_RSTN_WEST_SEL_CLR;
		reg |= CMN_REG00F0_ANA_LCPLL_CD_RSTN_WEST_SEL_SET(0x1);
		reg &= CMN_REG00F0_ANA_LCPLL_CD_RSTN_EAST_SEL_CLR;
		reg |= CMN_REG00F0_ANA_LCPLL_CD_RSTN_EAST_SEL_SET(0x1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00F0);

	/* ln0_rx_cdr_cco_band_sel_sp,	 ln0_rx_cdr_cco_band_sel_ssp
		ln0_rx_cdr_cco_band_sel_rbr =1  */
		usbdp_cal_reg_wr(0x04, regs_base + 0x08D4);
		usbdp_cal_reg_wr(0x04, regs_base + 0x18D4);


	/* ln0_ana_rx_bias_dfe_i_ctrl,	 ln2_ana_rx_bias_dfe_i_ctrl  */
		usbdp_cal_reg_wr(0x10, regs_base + 0x0E10);
		usbdp_cal_reg_wr(0x10, regs_base + 0x1E10);

	/* SSP - SS downspeed solution - tseq mask :2019.06.27 */
		usbdp_cal_reg_wr(0x0f, regs_base + 0x043C);
		usbdp_cal_reg_wr(0x40, regs_base + 0x0D90);
		usbdp_cal_reg_wr(0x40, regs_base + 0x1D90);

	/* SS U1/U2 fail - rx_cdr_data_mode_exit_en :2019.06.27 */
		usbdp_cal_reg_wr(0xff, regs_base + 0x0D2C);
		usbdp_cal_reg_wr(0xff, regs_base + 0x1D2C);
		usbdp_cal_reg_wr(0x0f, regs_base + 0x0D34);
		usbdp_cal_reg_wr(0x0f, regs_base + 0x1D34);
		usbdp_cal_reg_wr(0x00, regs_base + 0x0D30);
		usbdp_cal_reg_wr(0x00, regs_base + 0x1D30);
		usbdp_cal_reg_wr(0x00, regs_base + 0x0D38);
		usbdp_cal_reg_wr(0x00, regs_base + 0x1D38);
		usbdp_cal_reg_wr(0x78, regs_base + 0x0D3C);
		usbdp_cal_reg_wr(0x78, regs_base + 0x1D3C);

	/* RX JTOL enhanced :2019.07.08 */
		usbdp_cal_reg_wr(0x2A, regs_base + 0x08FC); // ln0_ana_rx_ctle_rl_hf_ctrl_ssp=5
		usbdp_cal_reg_wr(0x28, regs_base + 0x0914); // ln0_rx_ctle_i_hf_ctrl_ssp=5
		usbdp_cal_reg_wr(0x03, regs_base + 0x0A30); // ln0_rx_sslms_adap_coef_sel__7_0=
		usbdp_cal_reg_wr(0x05, regs_base + 0x0E38); // ln0_ana_rx_dfe_add_rl_ctrl=5
		usbdp_cal_reg_wr(0x27, regs_base + 0x0ECC); // ln0_rx_sslms_hf_bin_ssp=7
		usbdp_cal_reg_wr(0x22, regs_base + 0x0ED0); // ln0_rx_sslms_mf_bin_ssp=2
		usbdp_cal_reg_wr(0x26, regs_base + 0x0ED4); // ln0_rx_sslms_vga_bin_ssp=6
		usbdp_cal_reg_wr(0x2A, regs_base + 0x18FC); // ln2_ana_rx_ctle_rl_hf_ctrl_ssp=5
		usbdp_cal_reg_wr(0x28, regs_base + 0x1914); // ln2_rx_ctle_i_hf_ctrl_ssp=5
		usbdp_cal_reg_wr(0x03, regs_base + 0x1A30); // ln2_rx_sslms_adap_coef_sel__7_0=3
		usbdp_cal_reg_wr(0x05, regs_base + 0x1E38); // ln2_ana_rx_dfe_add_rl_ctrl=5
		usbdp_cal_reg_wr(0x27, regs_base + 0x1ECC); // ln2_rx_sslms_hf_bin_ssp=7
		usbdp_cal_reg_wr(0x22, regs_base + 0x1ED0); // ln2_rx_sslms_mf_bin_ssp=2
		usbdp_cal_reg_wr(0x26, regs_base + 0x1ED4); // ln2_rx_sslms_vga_bin_ssp=6

}

static void phy_exynos_usbdp_g2_v3_set_pcs(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pcs_base;
	void __iomem *regs_base2 = info->link_base;
	u32 reg;

	/* Change Elastic buffer threshold */
	reg = usbdp_cal_reg_rd(regs_base + USBDP_PCS_V3_RX_EBUF_PARAM);
	reg &= USBDP_PCS_V3_RX_EBUF_PARAM_NUM_INIT_BUFFERING_CLR;
	reg &= USBDP_PCS_V3_RX_EBUF_PARAM_SKP_REMOVE_TH_HALF_FULL_MODE_CLR;
	reg |= USBDP_PCS_V3_RX_EBUF_PARAM_SKP_REMOVE_TH_HALF_FULL_MODE_SET(0x1);
	usbdp_cal_reg_wr(reg, regs_base + USBDP_PCS_V3_RX_EBUF_PARAM);

	/* Change Elastic buffer Drain param
	 * -> enable TSEQ Mask
	 */

	reg = usbdp_cal_reg_rd(regs_base + USBDP_PCS_V3_RX_EBUF_DRAINER_PARAM);
	reg |= USBDP_PCS_V3_RX_EBUF_DRAINER_PARAM_EN_MASK_TSEQ_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + USBDP_PCS_V3_RX_EBUF_DRAINER_PARAM);

	/* abnormal comman pattern mask */
	reg = usbdp_cal_reg_rd(regs_base + USBDP_PCS_V3_RX_BACK_END_MODE_VEC);
	reg &= USBDP_PCS_V3_RX_BACK_END_MODE_VEC_DISABLE_DATA_MASK_CLR;
	usbdp_cal_reg_wr(reg, regs_base + USBDP_PCS_V3_RX_BACK_END_MODE_VEC);

	/* De-serializer enabled when U2 */
	reg = usbdp_cal_reg_rd(regs_base + USBDP_PCS_V3_PM_OUT_VEC_2);
	reg &= USBDP_PCS_V3_PM_OUT_VEC_2_B4_DYNAMIC_CLR;
	reg |= USBDP_PCS_V3_PM_OUT_VEC_2_B4_SEL_OUT_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + USBDP_PCS_V3_PM_OUT_VEC_2);


	/* TX Keeper Disable, Squelch off when U3 */
	reg = usbdp_cal_reg_rd(regs_base + USBDP_PCS_V3_PM_OUT_VEC_3);
	reg &= USBDP_PCS_V3_PM_OUT_VEC_3_B7_DYNAMIC_CLR;
	reg |= USBDP_PCS_V3_PM_OUT_VEC_3_B7_SEL_OUT_SET(1);
	reg &= USBDP_PCS_V3_PM_OUT_VEC_3_B2_SEL_OUT_CLR;	// Squelch off when U3
	usbdp_cal_reg_wr(reg, regs_base + USBDP_PCS_V3_PM_OUT_VEC_3);

	 // 20180822 PCS SFR setting : Noh M.W
	usbdp_cal_reg_wr(0x05700000, regs_base + USBDP_PCS_V3_PM_NS_VEC_PS1_N1);
	usbdp_cal_reg_wr(0x01700202, regs_base + USBDP_PCS_V3_PM_NS_VEC_PS2_N0);
	usbdp_cal_reg_wr(0x01700707, regs_base + USBDP_PCS_V3_PM_NS_VEC_PS3_N0);
	usbdp_cal_reg_wr(0x0070, regs_base + USBDP_PCS_V3_PM_TIMEOUT_0);
	usbdp_cal_reg_wr(0x1000, regs_base + USBDP_PCS_V3_PM_TIMEOUT_3);
	//usbdp_cal_reg_wr(0x21401, regs_base + USBDP_PCS_V3_RX_EBUF_DRAINER_PARAM);
	usbdp_cal_reg_wr(0x1401, regs_base + USBDP_PCS_V3_RX_EBUF_DRAINER_PARAM);

    /* Gen1 Tx Deemphasis/Preshoot Control by PCS
    *        jhkim 19/3/13 */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_PCS_LEQ_HS_TX_COEF_MAP_0);
	reg &= PCS_LEQ_HS_TX_COEF_MAP_0_DEEMPH_CLR;
	reg &= PCS_LEQ_HS_TX_COEF_MAP_0_AMP_CLR;
	reg &= PCS_LEQ_HS_TX_COEF_MAP_0_PRESHOOT_CLR;
	reg |= PCS_LEQ_HS_TX_COEF_MAP_0_DEEMPH(8);
	reg |= PCS_LEQ_HS_TX_COEF_MAP_0_AMP(0x1f);
	reg |= PCS_LEQ_HS_TX_COEF_MAP_0_PRESHOOT(0);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_PCS_LEQ_HS_TX_COEF_MAP_0);

	/* Gen2 Preshoot & Deemphasis */

	// CP13_Only_Preshoot - rLCSR_TX_DEEMPH_1 (4bit fo 6-bit field is valid)
	reg = readl(regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH_1);
	reg &= USBDRD_LCSR_TX_DEEMPH_1_CLR;
	reg &= USBDRD_LCSR_TX_AMP_1_CLR;
	reg &= USBDRD_LCSR_TX_PRESHOOT_1_CLR;
	reg |= USBDRD_LCSR_TX_AMP_1(0);
	reg |= USBDRD_LCSR_TX_PRESHOOT_1(4);
	writel(reg, regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH_1);
	// CP14_Only_Deemphasis - rLCSR_TX_DEEMPH_2(4bit fo 6-bit field is valid)
	reg = readl(regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH_2);
	reg &= USBDRD_LCSR_TX_DEEMPH_2_CLR;
	reg &= USBDRD_LCSR_TX_AMP_2_CLR;
	reg &= USBDRD_LCSR_TX_PRESHOOT_2_CLR;
	reg |= USBDRD_LCSR_TX_AMP_2(0);
	reg |= USBDRD_LCSR_TX_DEEMPH_2(4);
	writel(reg, regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH_2);

	// Normal Deemphasis&Preshoot - rLCSR_TX_DEEMPH(4bit fo 6-bit field is valid)
	reg = readl(regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH);
	reg &= USBDRD_LCSR_TX_DEEMPH_CLR;
	reg &= USBDRD_LCSR_TX_AMP_CLR;
	reg &= USBDRD_LCSR_TX_PRESHOOT_CLR;
	reg |= USBDRD_LCSR_TX_DEEMPH(4);
	reg |= USBDRD_LCSR_TX_AMP(0);
	reg |= USBDRD_LCSR_TX_PRESHOOT(4);
	writel(reg, regs_base2 + EXYNOS_USBDRD_LCSR_TX_DEEMPH);

	// Normal TX_AMP
	reg = readl(regs_base + USBDP_PCS_V3_LEQ_LOCAL_COEF);
	reg &= USBDP_PCS_V3_LEQ_LOCAL_COEF_PMA_CENTER_COEF_CLR;
	reg |= USBDP_PCS_V3_LEQ_LOCAL_COEF_PMA_CENTER_COEF_SET(0xf);
	writel(reg, regs_base + USBDP_PCS_V3_LEQ_LOCAL_COEF);

	// Skip Symbol Block aligner type-B: 2019.08.26
	reg = readl(regs_base + USBDP_PCS_V3_RX_RX_CONTROL);
	reg &= USBDP_PCS_V3_RX_RX_CONTROL_EN_BLOCK_ALIGNER_TYPE_B_CLR;
	reg |= USBDP_PCS_V3_RX_RX_CONTROL_EN_BLOCK_ALIGNER_TYPE_B_SET(0x1);
	writel(reg, regs_base + USBDP_PCS_V3_RX_RX_CONTROL);

		/* Block align at TS1/TS2 for Gen2 stability (Gen2 only) */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_PCS_V3_RX_RX_CONTROL_DEBUG);
	reg &= USBDP_PCS_V3_RX_RX_CONTROL_DEBUG_EN_TS_CHECK_CLR;
	reg |= USBDP_PCS_V3_RX_RX_CONTROL_DEBUG_EN_TS_CHECK_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_PCS_V3_RX_RX_CONTROL_DEBUG);


}

static void phy_exynos_usbdp_g2_v3_pma_lane_mux_sel(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;


	/* Lane configuration */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00A2);
	reg &= CMN_REG00A2_LANE_MUX_SEL_DP_CLR;

	if (info->used_phy_port == 0) {
		reg |= CMN_REG00A2_LANE_MUX_SEL_DP_SET(0xc);
		usbdp_cal_reg_wr(0x06, regs_base + 0x104C);
		usbdp_cal_reg_wr(0xa6, regs_base + 0x204C);
	} else {
		reg |= CMN_REG00A2_LANE_MUX_SEL_DP_SET(0x3);
		usbdp_cal_reg_wr(0xa6, regs_base + 0x104C);
		usbdp_cal_reg_wr(0x06, regs_base + 0x204C);
	}
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00A2);

	/* dp_lane_en = 0  */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00A2);
	reg &= CMN_REG00A2_DP_LANE_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00A2);

}

static void phy_exynos_usbdp_g2_v3_ctrl_pma_rst_release(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->ctrl_base;
	u32 reg;

	/* Reset release from port */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
	reg &= ~PMA_TRSV_SW_RST;
	reg &= ~PMA_CMN_SW_RST;
	reg &= ~PMA_INIT_SW_RST;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
}

static int phy_exynos_usbdp_g2_v3_pma_check_pll_lock(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;
	u32 cnt;

	for (cnt = 1000; cnt != 0; cnt--) {
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00D4);
		if ((reg & CMN_REG00D4_ANA_LCPLL_LOCK_DONE_MSK)) {
			break;
		}
		udelay(1);
	}

	if (!cnt) {
		return -1;
	}

	return 0;
}

static int phy_exynos_usbdp_g2_v3_pma_check_cdr_lock(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;
	u32 cnt;

	if (info->used_phy_port == 0) {
		for (cnt = 1000; cnt != 0; cnt--) {
			reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02E1);
			if ((reg & TRSV_REG02E1_LN0_MON_RX_CDR_LOCK_DONE_MSK)) {
				break;
			}
			udelay(1);
		}
	} else {
		for (cnt = 1000; cnt != 0; cnt--) {
			reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06E1);
			if ((reg & TRSV_REG06E1_LN2_MON_RX_CDR_LOCK_DONE_MSK)) {
				break;
			}
			udelay(1);
		}
	}

	if (!cnt) {
		return -2;
	}

	return 0;
}

static void phy_exynos_usbdp_g2_v3_aux_force_off(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG0007);
	reg &= USBDP_V3_CMN_OVRD_AUX_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_AUX_EN_SET(1);
	reg &= USBDP_V3_CMN_AUX_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG0007);
}


static void phy_exynos_usbdp_g2_v3_pma_ovrd_enable(struct exynos_usbphy_info *info, u32 cmn_rate)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	/* Write value with USBPCS : USB RX0 TX1 & USB RX2 TX3 column */
	/*
	pcs_pll_en = 0
	0x0310 0x02
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00C4);
	reg &= USBDP_V3_CMN_OVRD_PCS_PLL_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_PLL_EN_SET(1);
	reg &= USBDP_V3_CMN_PCS_PLL_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00C4);

	/*
	pcs_bgr_en = 0
	pcs_bias_en = 0
	0x0308 0x8A
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00C2);
	reg &= USBDP_V3_CMN_OVRD_PCS_PM_STATE_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_PM_STATE_SET(1);
	reg &= USBDP_V3_CMN_OVRD_PCS_BGR_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_BGR_EN_SET(1);
	reg &= USBDP_V3_CMN_PCS_BGR_EN_CLR;
	reg &= USBDP_V3_CMN_OVRD_PCS_BIAS_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_BIAS_EN_SET(1);
	reg &= USBDP_V3_CMN_PCS_BIAS_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00C2);

	/*
	cmn_rate
	0x0258 0x20 // Gen1
		   0x28 // Gen2
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG0096);
	reg &= USBDP_V3_CMN_OVRD_CMN_RATE_CLR;
	reg |= USBDP_V3_CMN_OVRD_CMN_RATE_SET(1);
	reg &= USBDP_V3_CMN_CMN_RATE_CLR;
	reg |= USBDP_V3_CMN_CMN_RATE_SET(cmn_rate);	// 0: Gen1, 1: Gen2
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG0096);

	/*
	lane_mux_sel_dp
	ln1_tx_rxd_en
	ln3_tx_rxd_en
	0x0288 0x00
	0x104C 0x06
	0x204C 0x06
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00A2);
	reg &= CMN_REG00A2_LANE_MUX_SEL_DP_CLR;
	reg &= CMN_REG00A2_DP_LANE_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00A2);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0413);
	reg &= USBDP_V3_TRSV_LN1_TX_RXD_EN_CLR;
	reg &= USBDP_V3_TRSV_LN1_TX_RXD_COMP_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0413);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0813);
	reg &= USBDP_V3_TRSV_LN3_TX_RXD_EN_CLR;
	reg &= USBDP_V3_TRSV_LN3_TX_RXD_COMP_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0813);


	/*
	pcs_bgr_en = 1
	pcs_bias_en = 1
	pcs_powerdown = 0
	pcs_des_en = 0
	pcs_cdr_en = 1
	pcs_pll_en = 1
	pcs_rx_ctle_en = 1
	pcs_rx_sqhs_en = 1
	pcs_rx_term_en = 1
	pcs_tx_drv_en =1
	pcs_tx_elecidle = 1
	pcs_tx_lfps_en = 0
	pcs_tx_rcv_det_en =1
	pcs_tx_ser_en =0
	0x0308 0x8F
	0x0310 0x03
	0x0314 0x4E
	0x0318 0xAF
	0x030c 0xBE
	0x031C 0x2B
	0x0328 0x03
	0x032C 0xBE
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00C2);
	reg &= USBDP_V3_CMN_OVRD_PCS_PM_STATE_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_PM_STATE_SET(1);
	reg &= USBDP_V3_CMN_OVRD_PCS_BGR_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_BGR_EN_SET(1);
	reg &= USBDP_V3_CMN_PCS_BGR_EN_CLR;
	reg |= USBDP_V3_CMN_PCS_BGR_EN_SET(1);
	reg &= USBDP_V3_CMN_OVRD_PCS_BIAS_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_BIAS_EN_SET(1);
	reg &= USBDP_V3_CMN_PCS_BIAS_EN_CLR;
	reg |= USBDP_V3_CMN_PCS_BIAS_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00C2);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00C4);
	reg &= USBDP_V3_CMN_OVRD_PCS_PLL_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_PLL_EN_SET(1);
	reg &= USBDP_V3_CMN_PCS_PLL_EN_CLR;
	reg |= USBDP_V3_CMN_PCS_PLL_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00C4);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00C5);
	reg &= USBDP_V3_CMN_OVRD_PCS_REF_FREQ_SEL_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_REF_FREQ_SEL_SET(1);
	reg &= USBDP_V3_CMN_OVRD_PCS_RX_CTLE_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_RX_CTLE_EN_SET(1);
	reg &= USBDP_V3_CMN_PCS_RX_CTLE_EN_CLR;
	reg |= USBDP_V3_CMN_PCS_RX_CTLE_EN_SET(1);
	reg &= USBDP_V3_CMN_OVRD_PCS_RX_DFE_ADAP_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_RX_DFE_ADAP_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00C5);


	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00C6);
	reg &= USBDP_V3_CMN_OVRD_PCS_RX_DFE_ADAP_HOLD_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_RX_DFE_ADAP_HOLD_SET(1);
	reg &= USBDP_V3_CMN_OVRD_PCS_RX_FOM_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_RX_FOM_EN_SET(1);
	reg &= USBDP_V3_CMN_OVRD_PCS_RX_SQHS_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_RX_SQHS_EN_SET(1);
	reg &= USBDP_V3_CMN_PCS_RX_SQHS_EN_CLR;
	reg |= USBDP_V3_CMN_PCS_RX_SQHS_EN_SET(1);
	reg &= USBDP_V3_CMN_OVRD_PCS_RX_TERM_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_RX_TERM_EN_SET(1);
	reg &= USBDP_V3_CMN_PCS_RX_TERM_EN_CLR;
	reg |= USBDP_V3_CMN_PCS_RX_TERM_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00C6);



	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00C3);
	reg &= USBDP_V3_CMN_OVRD_PCS_POWERDOWN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_POWERDOWN_SET(1);
	reg &= USBDP_V3_CMN_PCS_POWERDOWN_CLR;
	reg &= USBDP_V3_CMN_OVRD_PCS_CDR_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_CDR_EN_SET(1);
	reg &= USBDP_V3_CMN_PCS_CDR_EN_CLR;
	reg |= USBDP_V3_CMN_PCS_CDR_EN_SET(1);
	reg &= USBDP_V3_CMN_OVRD_PCS_CMN_RSTN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_CMN_RSTN_SET(1);
	reg &= USBDP_V3_CMN_PCS_CMN_RSTN_CLR;
	reg |= USBDP_V3_CMN_PCS_CMN_RSTN_SET(1);
	reg &= USBDP_V3_CMN_OVRD_PCS_DES_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_DES_EN_SET(1);
	reg &= USBDP_V3_CMN_PCS_DES_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00C3);


	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00C7);
	reg &= USBDP_V3_CMN_OVRD_PCS_TX_DRV_BEACON_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_TX_DRV_BEACON_SET(1);
	reg &= USBDP_V3_CMN_OVRD_PCS_TX_DRV_CM_KEEPER_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_TX_DRV_CM_KEEPER_EN_SET(1);
	reg &= USBDP_V3_CMN_OVRD_PCS_TX_DRV_EN_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_TX_DRV_EN_SET(1);
	reg &= USBDP_V3_CMN_PCS_TX_DRV_EN_CLR;
	reg |= USBDP_V3_CMN_PCS_TX_DRV_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00C7);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00CA);
	reg &= USBDP_V3_CMN_OVRD_PCS_TX_ELECIDLE_CLR;
	reg |= USBDP_V3_CMN_OVRD_PCS_TX_ELECIDLE_SET(1);
	reg &= USBDP_V3_CMN_PCS_TX_ELECIDLE_CLR;
	reg |= USBDP_V3_CMN_PCS_TX_ELECIDLE_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00CA);

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00E3);
	reg &= USBDP_V3_CMN_DP_INIT_RSTN_CLR;
	reg &= USBDP_V3_CMN_DP_CMN_RSTN_CLR;
	reg &= USBDP_V3_CMN_CDR_WATCHDOG_EN_CLR;
	reg &= USBDP_V3_CMN_CDR_WATCHDOG_MASK_CDR_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00E3);

}
#if 0
static void phy_exynos_usbdp_g2_v3_pma_ovrd_pcs_rst_release(struct exynos_usbphy_info *info)
{
	//void __iomem *regs_base = info->pma_base;
	//u32 reg;


}
#endif
static void phy_exynos_usbdp_g2_v3_pma_ovrd_power_on(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;

	/*
	pcs_des_en = 1
	pcs_tx_ser_en =1
	pcs_tx_elecidle = 0
	*/
	usbdp_cal_reg_wr(0xBF, regs_base + 0x030C);
	usbdp_cal_reg_wr(0x02, regs_base + 0x0328);
	usbdp_cal_reg_wr(0xBF, regs_base + 0x032C);

}


static int phy_exynos_usbdp_g2_v3_pma_bist_en(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	int ret = 0;
	int temp;

	/* Internal Serial Loopback - USB RX0/RX2 - USB TX0 TX3 : SEQ1(4)*/
	usbdp_cal_reg_wr(0x00, regs_base + 0x0288);
	usbdp_cal_reg_wr(0x00, regs_base + 0x02DC);
	usbdp_cal_reg_wr(0x8F, regs_base + 0x0308);
	usbdp_cal_reg_wr(0xBF, regs_base + 0x030C);
	usbdp_cal_reg_wr(0x03, regs_base + 0x0310);
	usbdp_cal_reg_wr(0x4A, regs_base + 0x0314);
	usbdp_cal_reg_wr(0xAA, regs_base + 0x0318);
	usbdp_cal_reg_wr(0x02, regs_base + 0x031C);
	usbdp_cal_reg_wr(0x02, regs_base + 0x0328);
	usbdp_cal_reg_wr(0x00, regs_base + 0x087C);
	usbdp_cal_reg_wr(0x00, regs_base + 0x187C);
	usbdp_cal_reg_wr(0xC4, regs_base + 0x0B40);
	usbdp_cal_reg_wr(0xC4, regs_base + 0x1B40);
	usbdp_cal_reg_wr(0x01, regs_base + 0x10B4);
	usbdp_cal_reg_wr(0x01, regs_base + 0x20B4);
	usbdp_cal_reg_wr(0xE5, regs_base + 0x081C);
	usbdp_cal_reg_wr(0xE5, regs_base + 0x101C);
	usbdp_cal_reg_wr(0xE5, regs_base + 0x181C);
	usbdp_cal_reg_wr(0xE5, regs_base + 0x201C);
	usbdp_cal_reg_wr(0x00, regs_base + 0x0864);
	usbdp_cal_reg_wr(0x02, regs_base + 0x1064);
	usbdp_cal_reg_wr(0x00, regs_base + 0x1864);
	usbdp_cal_reg_wr(0x02, regs_base + 0x2064);
	usbdp_cal_reg_wr(0x80, regs_base + 0x0B3C);
	usbdp_cal_reg_wr(0x80, regs_base + 0x1B3C);

	usbdp_cal_reg_wr(0x20, regs_base + 0x09C4); // rx_slb_d_lane_sel = 1
	usbdp_cal_reg_wr(0x20, regs_base + 0x19C4);
	usbdp_cal_reg_wr(0x30, regs_base + 0x09C4); // rx_slb_en = 1 - rx slb path open
	usbdp_cal_reg_wr(0x30, regs_base + 0x19C4);


	udelay(100);

	temp = info->used_phy_port;
	info->used_phy_port = 0;
	ret |= phy_exynos_usbdp_g2_v3_pma_check_cdr_lock(info);
	info->used_phy_port = 1;
	ret |= phy_exynos_usbdp_g2_v3_pma_check_cdr_lock(info);
	info->used_phy_port = temp;

	/*
	ln0/2_bist_rx_en
	0x0B40 0xE4
	0x1B40 0xE4
	*/
	usbdp_cal_reg_wr(0xE4, regs_base + 0x0B40);
	usbdp_cal_reg_wr(0xE4, regs_base + 0x1B40);

	if (ret)
		return -3;
	else
		return 0;
}

static int phy_exynos_usbdp_g2_v3_pma_bist_result(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;
	u32 pass_flag = 0, start_flag = 0;

	/*
	LN0 BIST pass/start flag
	LN2 BIST pass/start flag
	0x0B90 LN0 BIST PASS flag(Read)
	0x0B94 LN0 BIST START flag(Read)
	0x1B90 LN2 BIST PASS flag(read)
	0x1B94 LN2 BIST START flag(read)
	*/
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02E4);
	pass_flag = USBDP_V3_TRSV_LN0_MON_BIST_COMP_TEST_GET(reg);
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02E5);
	start_flag = USBDP_V3_TRSV_LN0_MON_BIST_COMP_START_GET(reg);

	if (!(pass_flag & start_flag))
		return -4;

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06E4);
	pass_flag = USBDP_V3_TRSV_LN2_MON_BIST_COMP_TEST_GET(reg);
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06E5);
	start_flag = USBDP_V3_TRSV_LN2_MON_BIST_COMP_START_GET(reg);

	if (!(pass_flag & start_flag))
		return -5;

	return 0;
}

int phy_exynos_usbdp_g2_v3_internal_loopback(struct exynos_usbphy_info *info, u32 cmn_rate)
{
	int ret = 0;
	int temp;

	phy_exynos_usbdp_g2_v3_ctrl_pma_ready(info);
	phy_exynos_usbdp_g2_v3_aux_force_off(info);
	phy_exynos_usbdp_g2_v3_pma_default_sfr_update(info);
	phy_exynos_usbdp_g2_v3_tune(info);

	phy_exynos_usbdp_g2_v3_pma_ovrd_enable(info, cmn_rate);
	phy_exynos_usbdp_g2_v3_ctrl_pma_rst_release(info);

	do {
		ret = phy_exynos_usbdp_g2_v3_pma_check_pll_lock(info);
		if (ret)
			break;

		phy_exynos_usbdp_g2_v3_pma_ovrd_power_on(info);

		temp = info->used_phy_port;
		info->used_phy_port = 0;
		ret = phy_exynos_usbdp_g2_v3_pma_check_cdr_lock(info);
		info->used_phy_port = temp;
		if (ret)
			break;

		info->used_phy_port = 1;
		ret = phy_exynos_usbdp_g2_v3_pma_check_cdr_lock(info);
		info->used_phy_port = temp;
		if (ret)
			break;

		mdelay(10);	// it is necessary experimentally

		ret = phy_exynos_usbdp_g2_v3_pma_bist_en(info);
		if (ret)
			break;

		udelay(100);

		ret = phy_exynos_usbdp_g2_v3_pma_bist_result(info);
		if (ret)
			break;
	} while (0);

	return ret;
}

void phy_exynos_usbdp_g2_v3_eom_init(struct exynos_usbphy_info *info, u32 cmn_rate)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	// EOM Sample Number ( 2^10 )  ----> applied 2^(sample_number)
	if (info->used_phy_port == 0) {
		/*
		0x0AD0 0x00	ln0_rx_efom_num_of_sample__13_8 = 0
		0x0AD4 0x0A	ln0_rx_efom_num_of_sample__7_0 = 0xa
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B4);
		reg &= USBDP_V3_TRSV_LN0_RX_EFOM_NUM_OF_SAMPLE__13_8_CLR;
		reg |= USBDP_V3_TRSV_LN0_RX_EFOM_NUM_OF_SAMPLE__13_8_SET(0);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B4);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B5);
		reg &= USBDP_V3_TRSV_LN0_RX_EFOM_NUM_OF_SAMPLE__7_0_CLR;
		reg |= USBDP_V3_TRSV_LN0_RX_EFOM_NUM_OF_SAMPLE__7_0_SET(0xa);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B5);
	} else {
		/*
		0x1AD0 0x00	ln2_rx_efom_num_of_sample__13_8 = 0
		0x1AD4 0x0A	ln2_rx_efom_num_of_sample__7_0 = 0xa
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B4);
		reg &= USBDP_V3_TRSV_LN2_RX_EFOM_NUM_OF_SAMPLE__13_8_CLR;
		reg |= USBDP_V3_TRSV_LN2_RX_EFOM_NUM_OF_SAMPLE__13_8_SET(0);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B4);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B5);
		reg &= USBDP_V3_TRSV_LN2_RX_EFOM_NUM_OF_SAMPLE__7_0_CLR;
		reg |= USBDP_V3_TRSV_LN2_RX_EFOM_NUM_OF_SAMPLE__7_0_SET(0xa);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B5);
	}

	// ovrd adap en, eom_en
	if (info->used_phy_port == 0) {
		/*
		0x0980 0x01	ovrd_ln0_rx_dfe_vref_odd_ctrl = 1
		0x0988 0x01	ovrd_ln0_rx_dfe_vref_even_ctrl = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0260);
		reg |= USBDP_V3_TRSV_OVRD_LN0_RX_DFE_VREF_ODD_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0260);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0262);
		reg |= USBDP_V3_TRSV_OVRD_LN0_RX_DFE_VREF_EVEN_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0262);
		/*
		0x0958 0x2F	ovrd_ln0_rx_dfe_adap_en = 1
					ln0_rx_dfe_adap_en = 0
					ovrd_ln0_rx_dfe_eom_en = 1
					ln0_rx_dfe_eom_en = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0256);
		reg |= USBDP_V3_TRSV_OVRD_LN0_RX_DFE_ADAP_EN_SET(1);
		reg &= USBDP_V3_TRSV_LN0_RX_DFE_ADAP_EN_CLR;
		reg |= USBDP_V3_TRSV_OVRD_LN0_RX_DFE_EOM_EN_SET(1);
		reg |= USBDP_V3_TRSV_LN0_RX_DFE_EOM_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0256);
	} else {
		/*
		0x1980 0x01	ovrd_ln2_rx_dfe_vref_odd_ctrl = 1
		0x1988 0x01	ovrd_ln2_rx_dfe_vref_even_ctrl = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0660);
		reg |= USBDP_V3_TRSV_OVRD_LN2_RX_DFE_VREF_ODD_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0660);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0662);
		reg |= USBDP_V3_TRSV_OVRD_LN2_RX_DFE_VREF_EVEN_CTRL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0662);

		/*
		0x1958 0x2F	ovrd_ln2_rx_dfe_adap_en = 1
					ln2_rx_dfe_adap_en = 0
					ovrd_ln2_rx_dfe_eom_en = 1
					ln2_rx_dfe_eom_en = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0656);
		reg |= USBDP_V3_TRSV_OVRD_LN2_RX_DFE_ADAP_EN_SET(1);
		reg &= USBDP_V3_TRSV_LN2_RX_DFE_ADAP_EN_CLR;
		reg |= USBDP_V3_TRSV_OVRD_LN2_RX_DFE_EOM_EN_SET(1);
		reg |= USBDP_V3_TRSV_LN2_RX_DFE_EOM_EN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0656);
	}

	// PI STR ( 0 min, f:max )
	if (info->used_phy_port == 0) {
		/*
		0x095c 0x03	ln0_ana_rx_dfe_eom_pi_str_ctrl = 3
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0257);
		reg &= USBDP_V3_TRSV_LN0_ANA_RX_DFE_EOM_PI_STR_CTRL_CLR;
		reg |= USBDP_V3_TRSV_LN0_ANA_RX_DFE_EOM_PI_STR_CTRL_SET(0x3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0257);
	} else {
		/*
		0x195C 0x03	ln2_ana_rx_dfe_eom_pi_str_ctrl = 3
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0657);
		reg &= USBDP_V3_TRSV_LN2_ANA_RX_DFE_EOM_PI_STR_CTRL_CLR;
		reg |= USBDP_V3_TRSV_LN2_ANA_RX_DFE_EOM_PI_STR_CTRL_SET(0x3);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0657);
	}


	// EOM MODE
	if (info->used_phy_port == 0) {
		/*
		0x0AC4 0x10	ln0_rx_efom_mode = 4
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B1);
		reg &= USBDP_V3_TRSV_LN0_RX_EFOM_MODE_CLR;
		reg |= USBDP_V3_TRSV_LN0_RX_EFOM_MODE_SET(0x4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B1);
	} else {
		/*
		0x1AC4 0x10	ln2_rx_efom_mode = 4
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B1);
		reg &= USBDP_V3_TRSV_LN2_RX_EFOM_MODE_CLR;
		reg |= USBDP_V3_TRSV_LN2_RX_EFOM_MODE_SET(0x4);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B1);
	}

	// EOM START SSM DISABLE
	if (info->used_phy_port == 0) {
		/*
		0x0AC8 0x10	ln0_rx_efom_start_ssm_disable = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B2);
		reg &= USBDP_V3_TRSV_LN0_RX_EFOM_START_SSM_DISABLE_CLR;
		reg |= USBDP_V3_TRSV_LN0_RX_EFOM_START_SSM_DISABLE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B2);
	} else {
		/*
		0x1AC8 0x10	ln2_rx_efom_start_ssm_disable = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B2);
		reg &= USBDP_V3_TRSV_LN2_RX_EFOM_START_SSM_DISABLE_CLR;
		reg |= USBDP_V3_TRSV_LN2_RX_EFOM_START_SSM_DISABLE_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B2);
	}

	// PCS EOM INPUT
	/*
	0x0318 0x0C	ovrd_pcs_rx_fom_en = 1
				pcs_rx_fom_en = 1
	 */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00C6);
	reg |= USBDP_V3_CMN_OVRD_PCS_RX_FOM_EN_SET(1);
	reg |= USBDP_V3_CMN_PCS_RX_FOM_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00C6);

	// NON DATA, DE-SER EN
	if (info->used_phy_port == 0) {
		/*
		0x0930 0x2C	ovrd_ln0_rx_des_non_data_sel = 1
					ln0_rx_des_non_data_sel = 0
		0x0934		ovrd_ln0_rx_des_rstn = 1
					ln0_rx_des_rstn = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG024C);
		reg |= USBDP_V3_TRSV_OVRD_LN0_RX_DES_NON_DATA_SEL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG024C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG024D);
		reg |= USBDP_V3_TRSV_OVRD_LN0_RX_DES_RSTN_SET(1);
		reg |= USBDP_V3_TRSV_LN0_RX_DES_RSTN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG024D);

	} else {
		/*
		0x1930 0x2C	ovrd_ln2_rx_des_non_data_sel = 1
					ln2_rx_des_non_data_sel = 0
		0x1934		ovrd_ln2_rx_des_rstn = 1
					ln2_rx_des_rstn = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG064C);
		reg |= USBDP_V3_TRSV_OVRD_LN2_RX_DES_NON_DATA_SEL_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG064C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG064D);
		reg |= USBDP_V3_TRSV_OVRD_LN2_RX_DES_RSTN_SET(1);
		reg |= USBDP_V3_TRSV_LN2_RX_DES_RSTN_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG064D);

	}

	// EOM CLOCK DIV  <----------------- Need to Confirm about value, it just predicted value
	/* it was set at phy_exynos_usbdp_g2_v3_pma_default_sfr_update
	0x09A0 0x24
	0x19A0 0x24
	 */

	// EOM BIT WIDTH  <----------------- Need to Confirm about value, it just predicted value
	if (info->used_phy_port == 0) {
		/*
		Gen2
		0x0acc 0x47	ln0_rx_efom_settle_time = 4
					ln0_rx_efom_bit_width_sel = 1

		Gen1
		0x0acc 0x4F	ln0_rx_efom_settle_time = 4
					ln0_rx_efom_bit_width_sel = 3
		 */
		if (cmn_rate) {
			reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B3);
			reg &= USBDP_V3_TRSV_LN0_RX_EFOM_SETTLE_TIME_CLR;
			reg |= USBDP_V3_TRSV_LN0_RX_EFOM_SETTLE_TIME_SET(0x4);
			reg &= USBDP_V3_TRSV_LN0_RX_EFOM_BIT_WIDTH_SEL_CLR;
			reg |= USBDP_V3_TRSV_LN0_RX_EFOM_BIT_WIDTH_SEL_SET(0x1);
			usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B3);
		} else {
			reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B3);
			reg &= USBDP_V3_TRSV_LN0_RX_EFOM_SETTLE_TIME_CLR;
			reg |= USBDP_V3_TRSV_LN0_RX_EFOM_SETTLE_TIME_SET(0x4);
			reg &= USBDP_V3_TRSV_LN0_RX_EFOM_BIT_WIDTH_SEL_CLR;
			reg |= USBDP_V3_TRSV_LN0_RX_EFOM_BIT_WIDTH_SEL_SET(0x0);
			usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B3);
		}
	} else {
		/*
		Gen2
		0x1acc 0x47	ln2_rx_efom_settle_time = 4
					ln2_rx_efom_bit_width_sel = 1

		Gen1
		0x1acc 0x4F	ln2_rx_efom_settle_time = 4
					ln2_rx_efom_bit_width_sel = 0
		 */
		if (cmn_rate) {
			reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B3);
			reg &= USBDP_V3_TRSV_LN2_RX_EFOM_SETTLE_TIME_CLR;
			reg |= USBDP_V3_TRSV_LN2_RX_EFOM_SETTLE_TIME_SET(0x4);
			reg &= USBDP_V3_TRSV_LN2_RX_EFOM_BIT_WIDTH_SEL_CLR;
			reg |= USBDP_V3_TRSV_LN2_RX_EFOM_BIT_WIDTH_SEL_SET(0x1);
			usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B3);
		} else {
			reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B3);
			reg &= USBDP_V3_TRSV_LN2_RX_EFOM_SETTLE_TIME_CLR;
			reg |= USBDP_V3_TRSV_LN2_RX_EFOM_SETTLE_TIME_SET(0x4);
			reg &= USBDP_V3_TRSV_LN2_RX_EFOM_BIT_WIDTH_SEL_CLR;
			reg |= USBDP_V3_TRSV_LN2_RX_EFOM_BIT_WIDTH_SEL_SET(0x0);
			usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B3);
		}
	}

	// Switch to 6:New EOM, E:legacy EFOM mode
	/*
	0x0554 0x06	efom_legacy_mode_en = 0
	 */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0155);
	reg &= USBDP_V3_CMN_EFOM_LEGACY_MODE_EN_CLR;
	reg |= USBDP_V3_CMN_EFOM_LEGACY_MODE_EN_SET(1);
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0155);
}

void phy_exynos_usbdp_g2_v3_eom_deinit(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	// EOM START ovrd clear
	if (info->used_phy_port == 0) {
		/*
		0x0AC4 0x10	ln0_ovrd_rx_efom_start = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B1);
		reg &= USBDP_V3_TRSV_LN0_OVRD_RX_EFOM_START_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B1);
	} else {
		/*
		0x1AC4 0x10	ln2_ovrd_rx_efom_start = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B1);
		reg &= USBDP_V3_TRSV_LN2_OVRD_RX_EFOM_START_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B1);
	}


	// EOM Sample Number clear
	if (info->used_phy_port == 0) {
		/*
		0x0AD0 0x00	ln0_rx_efom_num_of_sample__13_8 = 0
		0x0AD4 0x00	ln0_rx_efom_num_of_sample__7_0 = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B4);
		reg &= USBDP_V3_TRSV_LN0_RX_EFOM_NUM_OF_SAMPLE__13_8_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B4);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B5);
		reg &= USBDP_V3_TRSV_LN0_RX_EFOM_NUM_OF_SAMPLE__7_0_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B5);
	} else {
		/*
		0x1AD0 0x00	ln2_rx_efom_num_of_sample__13_8 = 0
		0x1AD4 0x00	ln2_rx_efom_num_of_sample__7_0 = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B4);
		reg &= USBDP_V3_TRSV_LN2_RX_EFOM_NUM_OF_SAMPLE__13_8_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B4);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B5);
		reg &= USBDP_V3_TRSV_LN2_RX_EFOM_NUM_OF_SAMPLE__7_0_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B5);
	}

	// ovrd adap en, eom_en
	if (info->used_phy_port == 0) {
		/*
		0x0980 0x00	ovrd_ln0_rx_dfe_vref_odd_ctrl = 0
		0x0988 0x00	ovrd_ln0_rx_dfe_vref_even_ctrl = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0260);
		reg &= USBDP_V3_TRSV_OVRD_LN0_RX_DFE_VREF_ODD_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0260);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0262);
		reg &= USBDP_V3_TRSV_OVRD_LN0_RX_DFE_VREF_EVEN_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0262);

		/*
		0x0958 0x1C	ovrd_ln0_rx_dfe_adap_en = 0
					ln0_rx_dfe_adap_en = 1
					ovrd_ln0_rx_dfe_eom_en = 0
					ln0_rx_dfe_eom_en = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0256);
		reg &= USBDP_V3_TRSV_OVRD_LN0_RX_DFE_ADAP_EN_CLR;
		reg |= USBDP_V3_TRSV_LN0_RX_DFE_ADAP_EN_SET(1);
		reg &= USBDP_V3_TRSV_OVRD_LN0_RX_DFE_EOM_EN_CLR;
		reg &= USBDP_V3_TRSV_LN0_RX_DFE_EOM_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0256);
	} else {
		/*
		0x1980 0x00	ovrd_ln2_rx_dfe_vref_odd_ctrl = 0
		0x1988 0x00	ovrd_ln2_rx_dfe_vref_even_ctrl = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0660);
		reg &= USBDP_V3_TRSV_OVRD_LN2_RX_DFE_VREF_ODD_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0660);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0662);
		reg &= USBDP_V3_TRSV_OVRD_LN2_RX_DFE_VREF_EVEN_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0662);

		/*
		0x1958 0x1C	ovrd_ln0_rx_dfe_adap_en = 0
					ln0_rx_dfe_adap_en = 1
					ovrd_ln0_rx_dfe_eom_en = 0
					ln0_rx_dfe_eom_en = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0656);
		reg &= USBDP_V3_TRSV_OVRD_LN2_RX_DFE_ADAP_EN_CLR;
		reg |= USBDP_V3_TRSV_LN2_RX_DFE_ADAP_EN_SET(1);
		reg &= USBDP_V3_TRSV_OVRD_LN2_RX_DFE_EOM_EN_CLR;
		reg &= USBDP_V3_TRSV_LN2_RX_DFE_EOM_EN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0656);
	}

	// PI STR ( 0 min, f:max )
	if (info->used_phy_port == 0) {
		/*
		0x095c 0x00	ln0_ana_rx_dfe_eom_pi_str_ctrl = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0257);
		reg &= USBDP_V3_TRSV_LN0_ANA_RX_DFE_EOM_PI_STR_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0257);
	} else {
		/*
		0x195C 0x00	ln2_ana_rx_dfe_eom_pi_str_ctrl = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0657);
		reg &= USBDP_V3_TRSV_LN2_ANA_RX_DFE_EOM_PI_STR_CTRL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0657);
	}

	// EOM MODE
	if (info->used_phy_port == 0) {
		/*
		0x0ac4 0x00	ln0_rx_efom_mode = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B1);
		reg &= USBDP_V3_TRSV_LN0_RX_EFOM_MODE_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B1);
	} else {
		/*
		0x1ac4 0x00	ln2_rx_efom_mode = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B1);
		reg &= USBDP_V3_TRSV_LN2_RX_EFOM_MODE_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B1);
	}

	// EOM START SSM DISABLE
	if (info->used_phy_port == 0) {
		/*
		0x0ac8 0x00	ln0_rx_efom_start_ssm_disable = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B2);
		reg &= USBDP_V3_TRSV_LN0_RX_EFOM_START_SSM_DISABLE_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B2);
	} else {
		/*
		0x1ac8 0x00	ln2_rx_efom_start_ssm_disable = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B2);
		reg &= USBDP_V3_TRSV_LN2_RX_EFOM_START_SSM_DISABLE_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B2);
	}

	// PCS EOM INPUT
	/*
	0x0318 0x00	ovrd_pcs_rx_fom_en = 0
				pcs_rx_fom_en = 0
	 */
	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG00C6);
	reg &= USBDP_V3_CMN_OVRD_PCS_RX_FOM_EN_CLR;
	reg &= USBDP_V3_CMN_PCS_RX_FOM_EN_CLR;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_CMN_REG00C6);

	// NON DATA, DE-SER EN
	if (info->used_phy_port == 0) {
		/*
		0x0930 0x00	ovrd_ln0_rx_des_non_data_sel = 0
		0x0934		ovrd_ln0_rx_des_rstn = 0
					ln0_rx_des_rstn = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG024C);
		reg &= USBDP_V3_TRSV_OVRD_LN0_RX_DES_NON_DATA_SEL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG024C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG024D);
		reg &= USBDP_V3_TRSV_OVRD_LN0_RX_DES_RSTN_CLR;
		reg &= USBDP_V3_TRSV_LN0_RX_DES_RSTN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG024D);

	} else {
		/*
		0x1930 0x00	ovrd_ln2_rx_des_non_data_sel = 0
		0x1934		ovrd_ln2_rx_des_rstn = 0
					ln2_rx_des_rstn = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG064C);
		reg &= USBDP_V3_TRSV_OVRD_LN2_RX_DES_NON_DATA_SEL_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG064C);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG064D);
		reg &= USBDP_V3_TRSV_OVRD_LN2_RX_DES_RSTN_CLR;
		reg &= USBDP_V3_TRSV_LN2_RX_DES_RSTN_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG064D);

	}

	// EOM BIT WIDTH
	if (info->used_phy_port == 0) {
		/*
		0x0acc 0x07	ln0_rx_efom_settle_time = 0
					ln0_rx_efom_bit_width_sel = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B3);
		reg &= USBDP_V3_TRSV_LN0_RX_EFOM_SETTLE_TIME_CLR;
		reg &= USBDP_V3_TRSV_LN0_RX_EFOM_BIT_WIDTH_SEL_CLR;
		reg |= USBDP_V3_TRSV_LN0_RX_EFOM_BIT_WIDTH_SEL_SET(0x1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B3);
	} else {
		/*
		0x1acc 0x07	ln2_rx_efom_settle_time = 0
					ln2_rx_efom_bit_width_sel = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B3);
		reg &= USBDP_V3_TRSV_LN2_RX_EFOM_SETTLE_TIME_CLR;
		reg &= USBDP_V3_TRSV_LN2_RX_EFOM_BIT_WIDTH_SEL_CLR;
		reg |= USBDP_V3_TRSV_LN2_RX_EFOM_BIT_WIDTH_SEL_SET(0x1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B3);
	}
}

void phy_exynos_usbdp_g2_v3_eom_start(struct exynos_usbphy_info *info, u32 ph_sel, u32 def_vref)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	// EOM PHASE SETTING
	if (info->used_phy_port == 0) {
		/*
		0x0ae8	ln0_rx_efom_eom_ph_sel
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02BA);
		reg &= USBDP_V3_TRSV_LN0_RX_EFOM_EOM_PH_SEL_CLR;
		reg |= USBDP_V3_TRSV_LN0_RX_EFOM_EOM_PH_SEL_SET(ph_sel);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02BA);
	} else {
		/*
		0x1ae8	ln2_rx_efom_eom_ph_sel
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06BA);
		reg &= USBDP_V3_TRSV_LN2_RX_EFOM_EOM_PH_SEL_CLR;
		reg |= USBDP_V3_TRSV_LN2_RX_EFOM_EOM_PH_SEL_SET(ph_sel);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06BA);
	}

	// EOM VREF SETTING
	if (info->used_phy_port == 0) {
		/*
		0x0984	ln0_rx_dfe_vref_odd_ctrl__7_0
		0x098C	ln0_rx_dfe_vref_even_ctrl__7_0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0261);
		reg &= USBDP_V3_TRSV_LN0_RX_DFE_VREF_ODD_CTRL__7_0_CLR;
		reg |= USBDP_V3_TRSV_LN0_RX_DFE_VREF_ODD_CTRL__7_0_SET(def_vref);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0261);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0263);
		reg &= USBDP_V3_TRSV_LN0_RX_DFE_VREF_EVEN_CTRL__7_0_CLR;
		reg |= USBDP_V3_TRSV_LN0_RX_DFE_VREF_EVEN_CTRL__7_0_SET(def_vref);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0263);
	} else {
		/*
		0x1984	ln2_rx_dfe_vref_odd_ctrl__7_0
		0x198C	ln2_rx_dfe_vref_even_ctrl__7_0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0661);
		reg &= USBDP_V3_TRSV_LN2_RX_DFE_VREF_ODD_CTRL__7_0_CLR;
		reg |= USBDP_V3_TRSV_LN2_RX_DFE_VREF_ODD_CTRL__7_0_SET(def_vref);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0661);

		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0663);
		reg &= USBDP_V3_TRSV_LN2_RX_DFE_VREF_EVEN_CTRL__7_0_CLR;
		reg |= USBDP_V3_TRSV_LN2_RX_DFE_VREF_EVEN_CTRL__7_0_SET(def_vref);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG0663);
	}

	// EOM START
	if (info->used_phy_port == 0) {
		/*
		0x0ac4 0x13	ln0_ovrd_rx_efom_start = 1
					ln0_rx_efom_start = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B1);
		reg |= USBDP_V3_TRSV_LN0_OVRD_RX_EFOM_START_SET(1);
		reg |= USBDP_V3_TRSV_LN0_RX_EFOM_START_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B1);
	} else {
		/*
		0x1ac4 0x13	ln2_ovrd_rx_efom_start = 1
					ln2_rx_efom_start = 1
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B1);
		reg |= USBDP_V3_TRSV_LN2_OVRD_RX_EFOM_START_SET(1);
		reg |= USBDP_V3_TRSV_LN2_RX_EFOM_START_SET(1);
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B1);
	}
}

int phy_exynos_usbdp_g2_v3_eom_get_done_status(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;
	u32 eom_done = 0;

	// Check efom_done
	if (info->used_phy_port == 0) {
		/*
		0x0c10	ln0_mon_rx_efom_done
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0304);
		eom_done = USBDP_V3_TRSV_LN0_MON_RX_EFOM_DONE_GET(reg);
	} else {
		/*
		0x1c10	ln2_mon_rx_efom_done
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG0704);
		eom_done = USBDP_V3_TRSV_LN2_MON_RX_EFOM_DONE_GET(reg);
	}

	if (eom_done)
		return 0;
	else
		return -1;
}

u64 phy_exynos_usbdp_g2_v3_eom_get_err_cnt(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;
	u64 err_cnt_lo = 0;
	u64 err_cnt_hi = 0;

	// Get error count
	if (info->used_phy_port == 0) {
		/*
		0x077C	ln0_mon_rx_efom_err_cnt__7_0
		0x0778	ln0_mon_rx_efom_err_cnt__15_8
		0x0774	ln0_mon_rx_efom_err_cnt__23_16
		0x0770	ln0_mon_rx_efom_err_cnt__31_24
		0x076C	ln0_mon_rx_efom_err_cnt__39_32
		0x0768	ln0_mon_rx_efom_err_cnt__47_40
		0x0764	ln0_mon_rx_efom_err_cnt__52_48
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG03C3);
		err_cnt_lo |= USBDP_V3_CMN_LN0_MON_RX_EFOM_ERR_CNT__7_0_GET(reg) << 0;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG03C2);
		err_cnt_lo |= USBDP_V3_CMN_LN0_MON_RX_EFOM_ERR_CNT__15_8_GET(reg) << 8;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG03C1);
		err_cnt_lo |= USBDP_V3_CMN_LN0_MON_RX_EFOM_ERR_CNT__23_16_GET(reg) << 16;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG03C0);
		err_cnt_lo |= USBDP_V3_CMN_LN0_MON_RX_EFOM_ERR_CNT__31_24_GET(reg) << 24;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG03BF);
		err_cnt_hi |= USBDP_V3_CMN_LN0_MON_RX_EFOM_ERR_CNT__39_32_GET(reg) << 0;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG03BE);
		err_cnt_hi |= USBDP_V3_CMN_LN0_MON_RX_EFOM_ERR_CNT__47_40_GET(reg) << 4;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG03BD);
		err_cnt_hi |= USBDP_V3_CMN_LN0_MON_RX_EFOM_ERR_CNT__52_48_GET(reg) << 8;
	} else {
		/*
		0x1f0c	ln2_mon_rx_efom_err_cnt__7_0
		0x1f08	ln2_mon_rx_efom_err_cnt__15_8
		0x1f04	ln2_mon_rx_efom_err_cnt__23_16
		0x1f00	ln2_mon_rx_efom_err_cnt__31_24
		0x1efc	ln2_mon_rx_efom_err_cnt__39_32
		0x1ef8	ln2_mon_rx_efom_err_cnt__47_40
		0x1ef4	ln2_mon_rx_efom_err_cnt__52_48
		*/
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG07C3);
		err_cnt_lo |= USBDP_V3_CMN_LN2_MON_RX_EFOM_ERR_CNT__7_0_GET(reg) << 0;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG07C2);
		err_cnt_lo |= USBDP_V3_CMN_LN2_MON_RX_EFOM_ERR_CNT__15_8_GET(reg) << 8;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG07C1);
		err_cnt_lo |= USBDP_V3_CMN_LN2_MON_RX_EFOM_ERR_CNT__23_16_GET(reg) << 16;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG07C0);
		err_cnt_lo |= USBDP_V3_CMN_LN2_MON_RX_EFOM_ERR_CNT__31_24_GET(reg) << 24;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG07BF);
		err_cnt_hi |= USBDP_V3_CMN_LN2_MON_RX_EFOM_ERR_CNT__39_32_SET(reg) << 0;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG07BE);
		err_cnt_hi |= USBDP_V3_CMN_LN2_MON_RX_EFOM_ERR_CNT__47_40_GET(reg) << 4;
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_CMN_REG07BD);
		err_cnt_hi |= USBDP_V3_CMN_LN2_MON_RX_EFOM_ERR_CNT__52_48_SET(reg) << 8;
	}

	return err_cnt_hi << 32 | err_cnt_lo;
}

void phy_exynos_usbdp_g2_v3_eom_stop(struct exynos_usbphy_info *info)
{
	void __iomem *regs_base = info->pma_base;
	u32 reg;

	// EOM STOP
	if (info->used_phy_port == 0) {
		/*
		0x0ac4 0x12	ln0_rx_efom_start = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG02B1);
		reg &= USBDP_V3_TRSV_LN0_RX_EFOM_START_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG02B1);
	} else {
		/*
		0x1ac4 0x12	ln2_rx_efom_start = 0
		 */
		reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBDP_V3_TRSV_REG06B1);
		reg &= USBDP_V3_TRSV_LN2_RX_EFOM_START_CLR;
		usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBDP_V3_TRSV_REG06B1);
	}
}

void phy_exynos_usbdp_g2_v3_eom(struct exynos_usbphy_info *info, u32 cmn_rate, struct usb_eom_result_s *eom_result)
{
	u32 ph_sel, def_vref = 0;
	u32 err_cnt = 0;
	u32 test_cnt = 0;
	u32 ui_cnt;

	phy_exynos_usbdp_g2_v3_eom_init(info, cmn_rate);	// 0: Gen1, 1: Gen2

	ui_cnt = cmn_rate;
	while (ui_cnt < 2) {
		for (ph_sel = 0; ph_sel < EOM_PH_SEL_MAX; ph_sel++) {
			for (def_vref = 0; def_vref < EOM_DEF_VREF_MAX; def_vref++) {
				phy_exynos_usbdp_g2_v3_eom_start(info, ph_sel, def_vref);
				while (phy_exynos_usbdp_g2_v3_eom_get_done_status(info));
				err_cnt = phy_exynos_usbdp_g2_v3_eom_get_err_cnt(info);
				phy_exynos_usbdp_g2_v3_eom_stop(info);

				// Save result
				eom_result[test_cnt].phase = ph_sel;
				eom_result[test_cnt].vref = def_vref;
				eom_result[test_cnt].err = err_cnt;
				test_cnt++;
			}
		}
		ui_cnt++;
	}
	phy_exynos_usbdp_g2_v3_eom_deinit(info);
}


int phy_exynos_usbdp_g2_v3_enable(struct exynos_usbphy_info *info)
{
	int ret = 0;

	phy_exynos_usbdp_g2_v3_ctrl_pma_ready(info);
	phy_exynos_usbdp_g2_v3_pma_default_sfr_update(info);
	phy_exynos_usbdp_g2_v3_pma_lane_mux_sel(info);
	phy_exynos_usbdp_g2_v3_ctrl_pma_rst_release(info);

	ret = phy_exynos_usbdp_g2_v3_pma_check_pll_lock(info);
	if (!ret) {
		ret = phy_exynos_usbdp_g2_v3_pma_check_cdr_lock(info);
	}
	phy_exynos_usbdp_g2_v3_set_pcs(info);
	phy_exynos_usbdp_g2_v3_tune(info);	// TODO confirm to setting sequence about tune.

	return ret;
}

void phy_exynos_usbdp_g2_v3_disable(struct exynos_usbphy_info *info)
{
	// TODO
	void __iomem *regs_base = info->ctrl_base;
	u32 reg;

	reg = usbdp_cal_reg_rd(regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
	reg &= ~PMA_ROPLL_REF_CLK_SEL_MASK;     // ropll refclk off for reducing powerdown current
	reg |= PMA_ROPLL_REF_CLK_SEL_SET(1);
	reg &= ~PMA_LCPLL_REF_CLK_SEL_MASK;     // lcpll refclk off for reducing powerdown current
	reg |= PMA_LCPLL_REF_CLK_SEL_SET(1);
	reg |= PMA_LOW_PWR;
	reg |= PMA_TRSV_SW_RST;
	reg |= PMA_CMN_SW_RST;
	reg |= PMA_INIT_SW_RST;
	reg |= PMA_APB_SW_RST;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);

	udelay(1);

	reg &= ~PMA_TRSV_SW_RST;
	reg &= ~PMA_CMN_SW_RST;
	reg &= ~PMA_INIT_SW_RST;
	reg &= ~PMA_APB_SW_RST;
	usbdp_cal_reg_wr(reg, regs_base + EXYNOS_USBCON_COMBO_PMA_CTRL);
}
