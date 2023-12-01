/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos2100 SoC.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <soc/samsung/cal-if.h>
#include <dt-bindings/clock/s5e8535.h>

#include "../../soc/samsung/cal-if/s5e8535/cmucal/cmucal-vclk.h"
#include "../../soc/samsung/cal-if/s5e8535/cmucal/cmucal-node.h"
#include "../../soc/samsung/cal-if/s5e8535/cmucal/cmucal-qch.h"
#include "../../soc/samsung/cal-if/s5e8535/cmucal/clkout_s5e8535.h"
#include "composite.h"

static struct samsung_clk_provider *s5e8535_clk_provider;
bool clk_exynos_skip_hw;
/*
 * list of controller registers to be saved and restored during a
 * suspend/resume cycle.
 */
/* fixed rate clocks generated outside the soc */

struct samsung_fixed_rate s5e8535_fixed_rate_ext_clks[] = {
	FRATE(OSCCLK1, "fin_pll1", NULL, 0, 52000000),
	FRATE(OSCCLK2, "fin_pll2", NULL, 0, 26000000),	/* for mif/aud/peri */
};

/* CMU_TOP */
struct init_vclk s5e8535_top_hwacg_vclks[] = {
	HWACG_VCLK(GATE_DFTMUX_CMU_QCH_CIS_CLK0, DFTMUX_CMU_QCH_CIS_CLK0, "GATE_DFTMUX_CMU_QCH_CIS_CLK0", NULL, 0, VCLK_GATE | VCLK_QCH_DIS, NULL),
	HWACG_VCLK(GATE_DFTMUX_CMU_QCH_CIS_CLK1, DFTMUX_CMU_QCH_CIS_CLK1, "GATE_DFTMUX_CMU_QCH_CIS_CLK1", NULL, 0, VCLK_GATE | VCLK_QCH_DIS, NULL),
	HWACG_VCLK(GATE_DFTMUX_CMU_QCH_CIS_CLK2, DFTMUX_CMU_QCH_CIS_CLK2, "GATE_DFTMUX_CMU_QCH_CIS_CLK2", NULL, 0, VCLK_GATE | VCLK_QCH_DIS, NULL),
	HWACG_VCLK(GATE_DFTMUX_CMU_QCH_CIS_CLK3, DFTMUX_CMU_QCH_CIS_CLK3, "GATE_DFTMUX_CMU_QCH_CIS_CLK3", NULL, 0, VCLK_GATE | VCLK_QCH_DIS, NULL),
	HWACG_VCLK(GATE_DFTMUX_CMU_QCH_CIS_CLK4, DFTMUX_CMU_QCH_CIS_CLK4, "GATE_DFTMUX_CMU_QCH_CIS_CLK4", NULL, 0, VCLK_GATE | VCLK_QCH_DIS, NULL),
	HWACG_VCLK(GATE_DFTMUX_CMU_QCH_CIS_CLK5, DFTMUX_CMU_QCH_CIS_CLK5, "GATE_DFTMUX_CMU_QCH_CIS_CLK5", NULL, 0, VCLK_GATE | VCLK_QCH_DIS, NULL),
};

struct init_vclk s5e8535_top_vclks[] = {
	VCLK(DOUT_DIV_CLKCMU_AUD_NOC, DIV_CLKCMU_AUD_NOC, "DOUT_DIV_CLKCMU_AUD_NOC", 0, 0, NULL),
	VCLK(DOUT_DIV_CLKCMU_AUD_CPU, DIV_CLKCMU_AUD_CPU, "DOUT_DIV_CLKCMU_AUD_CPU", 0, 0, NULL),
	VCLK(DOUT_DIV_CLKCMU_CIS_CLK0, DIV_CLKCMU_CIS_CLK0, "DOUT_DIV_CLKCMU_CIS_CLK0", 0, 0, NULL),
	VCLK(DOUT_DIV_CLKCMU_CIS_CLK1, DIV_CLKCMU_CIS_CLK1, "DOUT_DIV_CLKCMU_CIS_CLK1", 0, 0, NULL),
	VCLK(DOUT_DIV_CLKCMU_CIS_CLK2, DIV_CLKCMU_CIS_CLK2, "DOUT_DIV_CLKCMU_CIS_CLK2", 0, 0, NULL),
	VCLK(DOUT_DIV_CLKCMU_CIS_CLK3, DIV_CLKCMU_CIS_CLK3, "DOUT_DIV_CLKCMU_CIS_CLK3", 0, 0, NULL),
	VCLK(DOUT_DIV_CLKCMU_CIS_CLK4, DIV_CLKCMU_CIS_CLK4, "DOUT_DIV_CLKCMU_CIS_CLK4", 0, 0, NULL),
	VCLK(DOUT_DIV_CLKCMU_CIS_CLK5, DIV_CLKCMU_CIS_CLK5, "DOUT_DIV_CLKCMU_CIS_CLK5", 0, 0, NULL),
	VCLK(DOUT_DIV_CLKCMU_HSI_NOC, DIV_CLKCMU_HSI_NOC, "DOUT_DIV_CLKCMU_HSI_NOC", 0, 0, NULL),
	VCLK(DOUT_CLKCMU_HSI_UFS_EMBD, VCLK_DIV_CLKCMU_HSI_UFS_EMBD, "DOUT_CLKCMU_HSI_UFS_EMBD", 0, 0, NULL),
	VCLK(DOUT_CLKCMU_USB_NOC, DIV_CLKCMU_USB_NOC, "DOUT_CLKCMU_USB_NOC", 0, 0, NULL),
	VCLK(DOUT_CLKCMU_USB_USB20DRD, DIV_CLKCMU_USB_USB20DRD, "DOUT_CLKCMU_USB_USB20DRD", 0, 0, NULL),
	VCLK(DOUT_CLKCMU_M2M_NOC, DIV_CLKCMU_M2M_NOC, "DOUT_CLKCMU_M2M_NOC", 0, 0, NULL),
	VCLK(DOUT_CLKCMU_M2M_JPEG, DIV_CLKCMU_M2M_JPEG, "DOUT_CLKCMU_M2M_JPEG", 0, 0, NULL),
	VCLK(DOUT_CLKCMU_M2M_GDC, DIV_CLKCMU_M2M_GDC, "DOUT_CLKCMU_M2M_GDC", 0, 0, NULL),
	VCLK(DOUT_CLKCMU_M2M_LME, DIV_CLKCMU_M2M_LME, "DOUT_CLKCMU_M2M_LME", 0, 0, NULL),
	VCLK(DOUT_CLKCMU_M2M_VRA, DIV_CLKCMU_M2M_VRA, "DOUT_CLKCMU_M2M_VRA", 0, 0, NULL),
	VCLK(DOUT_CLKCMU_PERI_MMC_CARD, DIV_CLKCMU_PERI_MMC_CARD, "DOUT_CLKCMU_PERI_MMC_CARD", 0, 0, NULL),
};

/* CMU ALVIE */
struct init_vclk s5e8535_alive_hwacg_vclks[] = {
	HWACG_VCLK(GATE_MCT_ALIVE_QCH, MCT_ALIVE_QCH, "GATE_MCT_ALIVE_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI_ALIVE0_QCH, USI_ALIVE0_QCH, "GATE_USI_ALIVE0_QCH", NULL, 0, VCLK_GATE, NULL),
};

struct init_vclk s5e8535_alive_vclks[] = {
	VCLK(DOUT_DIV_CLK_ALIVE_CHUB_NOC, DIV_CLK_ALIVE_CHUB_NOC, "DOUT_DIV_CLK_ALIVE_CHUB_NOC", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_ALIVE_USI0, DIV_CLK_ALIVE_USI0, "DOUT_DIV_CLK_ALIVE_USI0", 0, 0, NULL),
	VCLK(DOUT_CLK_ALIVE_OSCCLK_USB_LINK, DIV_CLK_ALIVE_OSCCLK_USB_LINK, "DOUT_CLK_ALIVE_OSCCLK_USB_LINK", 0, 0, NULL),
};

/* CMU_AUD */
struct init_vclk s5e8535_aud_hwacg_vclks[] = {
	HWACG_VCLK(GATE_ABOX_QCH_BCLK_DSIF, ABOX_QCH_BCLK_DSIF, "GATE_ABOX_QCH_BCLK_DSIF", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_ABOX_QCH_BCLK0, ABOX_QCH_BCLK0, "GATE_ABOX_QCH_BCLK0", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_ABOX_QCH_BCLK1, ABOX_QCH_BCLK1, "GATE_ABOX_QCH_BCLK1", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_ABOX_QCH_BCLK2, ABOX_QCH_BCLK2, "GATE_ABOX_QCH_BCLK2", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_ABOX_QCH_BCLK3, ABOX_QCH_BCLK3, "GATE_ABOX_QCH_BCLK3", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_ABOX_QCH_BCLK4, ABOX_QCH_BCLK4, "GATE_ABOX_QCH_BCLK4", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_ABOX_QCH_BCLK5, ABOX_QCH_BCLK5, "GATE_ABOX_QCH_BCLK5", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_ABOX_QCH_BCLK6, ABOX_QCH_BCLK6, "GATE_ABOX_QCH_BCLK6", NULL, 0, VCLK_GATE, NULL),
};

struct init_vclk s5e8535_aud_vclks[] = {
	VCLK(MOUT_CLK_AUD_UAIF0, MUX_CLK_AUD_UAIF0, "MOUT_CLK_AUD_UAIF0", 0, 0, NULL),
	VCLK(MOUT_CLK_AUD_UAIF1, MUX_CLK_AUD_UAIF1, "MOUT_CLK_AUD_UAIF1", 0, 0, NULL),
	VCLK(MOUT_CLK_AUD_UAIF2, MUX_CLK_AUD_UAIF2, "MOUT_CLK_AUD_UAIF2", 0, 0, NULL),
	VCLK(MOUT_CLK_AUD_UAIF3, MUX_CLK_AUD_UAIF3, "MOUT_CLK_AUD_UAIF3", 0, 0, NULL),
	VCLK(MOUT_CLK_AUD_UAIF4, MUX_CLK_AUD_UAIF4, "MOUT_CLK_AUD_UAIF4", 0, 0, NULL),
	VCLK(MOUT_CLK_AUD_UAIF5, MUX_CLK_AUD_UAIF5, "MOUT_CLK_AUD_UAIF5", 0, 0, NULL),
	VCLK(MOUT_CLK_AUD_UAIF6, MUX_CLK_AUD_UAIF6, "MOUT_CLK_AUD_UAIF6", 0, 0, NULL),
	VCLK(UMUX_CP_PCMC_CLK, MUX_CP_PCMC_CLK_USER, "UMUX_CP_PCMC_CLK", 0, 0, NULL),

	VCLK(DOUT_DIV_CLK_AUD_DSIF, DIV_CLK_AUD_DSIF, "DOUT_DIV_CLK_AUD_DSIF", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_AUD_USB_NOC, DIV_CLK_AUD_USB_NOC, "DOUT_DIV_CLK_AUD_USB_NOC", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_AUD_UAIF0, DIV_CLK_AUD_UAIF0, "DOUT_DIV_CLK_AUD_UAIF0", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_AUD_UAIF1, DIV_CLK_AUD_UAIF1, "DOUT_DIV_CLK_AUD_UAIF1", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_AUD_UAIF2, DIV_CLK_AUD_UAIF2, "DOUT_DIV_CLK_AUD_UAIF2", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_AUD_UAIF3, DIV_CLK_AUD_UAIF3, "DOUT_DIV_CLK_AUD_UAIF3", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_AUD_UAIF4, DIV_CLK_AUD_UAIF4, "DOUT_DIV_CLK_AUD_UAIF4", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_AUD_UAIF5, DIV_CLK_AUD_UAIF5, "DOUT_DIV_CLK_AUD_UAIF5", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_AUD_UAIF6, DIV_CLK_AUD_UAIF6, "DOUT_DIV_CLK_AUD_UAIF6", 0, 0, NULL),

	VCLK(MOUT_CLK_AUD_CPU_PLL, MUX_CLK_AUD_CPU_PLL, "MOUT_CLK_AUD_CPU_PLL", 0, 0, NULL),
	VCLK(MOUT_CLK_AUD_PCMC, MUX_CLK_AUD_PCMC, "MOUT_CLK_AUD_PCMC", 0, 0, NULL),
	VCLK(DOUT_CLK_AUD_PCMC, DIV_CLK_AUD_PCMC, "DOUT_CLK_AUD_PCMC", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_AUD_NOCD, DIV_CLK_AUD_NOCD, "DOUT_DIV_CLK_AUD_NOCD", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_AUD_CNT, DIV_CLK_AUD_CNT, "DOUT_DIV_CLK_AUD_CNT", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_AUD_AUDIF, DIV_CLK_AUD_AUDIF, "DOUT_DIV_CLK_AUD_AUDIF", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_AUD_FM, DIV_CLK_AUD_FM, "DOUT_DIV_CLK_AUD_FM", 0, 0, NULL),
};

/* CMU_HSI */
struct init_vclk s5e8535_hsi_hwacg_vclks[] = {
	HWACG_VCLK(GATE_UFS_EMBD_QCH, UFS_EMBD_QCH, "GATE_UFS_EMBD_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_UFS_EMBD_QCH_FMP, UFS_EMBD_QCH_FMP, "GATE_UFS_EMBD_QCH_FMP", NULL, 0, VCLK_GATE, NULL),
};

struct init_vclk s5e8535_hsi_vclks[] = {
};

/* CMU_MFC */
struct init_vclk s5e8535_mfc_hwacg_vclks[] = {
	HWACG_VCLK(UMUX_CLKCMU_MFC_NOC, MUX_CLKCMU_MFC_NOC_USER, "UMUX_CLKCMU_MFC_NOC", NULL, 0, 0, NULL),
};

struct init_vclk s5e8535_mfc_vclks[] = {
};

/* CMU_MIF0 */
struct init_vclk s5e8535_mif0_hwacg_vclks[] = {
	HWACG_VCLK(MUX_MIF_DDRPHY2X_MIF0, CLKMUX_MIF_DDRPHY2X_MIF0, "MUX_MIF_DDRPHY2X_MIF0", NULL, 0, 0, NULL),
};

struct init_vclk s5e8535_mif0_vclks[] = {
};

/* CMU_MIF1 */
struct init_vclk s5e8535_mif1_hwacg_vclks[] = {
	HWACG_VCLK(MUX_MIF_DDRPHY2X_MIF1, CLKMUX_MIF_DDRPHY2X_MIF1, "MUX_MIF_DDRPHY2X_MIF1", NULL, 0, 0, NULL),
};

struct init_vclk s5e8535_mif1_vclks[] = {
};

/* CMU_NOCL0 */
struct init_vclk s5e8535_nocl0_hwacg_vclks[] = {
};

struct init_vclk s5e8535_nocl0_vclks[] = {
};

/* CMU_NOCL1A */
struct init_vclk s5e8535_nocl1a_hwacg_vclks[] = {
	HWACG_VCLK(GATE_PDMA_NOCL1A_QCH, PDMA_NOCL1A_QCH, "GATE_PDMA_NOCL1A_QCH", NULL, 0, VCLK_GATE, NULL),
};

struct init_vclk s5e8535_nocl1a_vclks[] = {
};

/* CMU_PERI */
struct init_vclk s5e8535_peri_hwacg_vclks[] = {
	HWACG_VCLK(GATE_USI00_I2C_QCH, USI00_I2C_QCH, "GATE_USI00_I2C_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI01_I2C_QCH, USI01_I2C_QCH, "GATE_USI01_I2C_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI02_I2C_QCH, USI02_I2C_QCH, "GATE_USI02_I2C_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI03_I2C_QCH, USI03_I2C_QCH, "GATE_USI03_I2C_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI04_I2C_QCH, USI04_I2C_QCH, "GATE_USI04_I2C_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI05_I2C_QCH, USI05_I2C_QCH, "GATE_USI05_I2C_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI07_I2C_QCH, USI07_I2C_QCH, "GATE_USI07_I2C_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI06_I2C_QCH, USI06_I2C_QCH, "GATE_USI06_I2C_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI00_USI_QCH, USI00_USI_QCH, "GATE_USI00_USI_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI01_USI_QCH, USI01_USI_QCH, "GATE_USI01_USI_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI02_USI_QCH, USI02_USI_QCH, "GATE_USI02_USI_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI03_USI_QCH, USI03_USI_QCH, "GATE_USI03_USI_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI04_USI_QCH, USI04_USI_QCH, "GATE_USI04_USI_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI05_USI_QCH, USI05_USI_QCH, "GATE_USI05_USI_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI06_USI_QCH, USI06_USI_QCH, "GATE_USI06_USI_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_WDT0_QCH, WDT0_QCH, "GATE_WDT0_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_WDT1_QCH, WDT1_QCH, "GATE_WDT1_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_PWM_QCH, PWM_QCH, "GATE_PWM_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_UART_DBG_QCH, UART_DBG_QCH, "GATE_UART_DBG_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_MMC_CARD_QCH, MMC_CARD_QCH, "GATE_MMC_CARD_QCH", NULL, 0, VCLK_GATE, NULL),
};

struct init_vclk s5e8535_peri_vclks[] = {
	VCLK(UMUX_CLKCMU_PERI_MMC_CARD_USER,MUX_CLKCMU_PERI_MMC_CARD_USER, "UMUX_CLKCMU_PERI_MMC_CARD_USER", 0, 0, NULL),
	VCLK(UMUX_CLK_PERI_UART_DBG,MUX_CLK_PERI_UART_DBG,"UMUX_CLK_PERI_UART_DBG", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_PERI_UART_DBG,DIV_CLK_PERI_UART_DBG,"DOUT_DIV_CLK_PERI_UART_DBG", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_PERI_USI_I2C, DIV_CLK_PERI_USI_I2C, "DOUT_DIV_CLK_PERI_USI_I2C", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_PERI_USI06_USI, VCLK_DIV_CLK_PERI_USI06_USI, "DOUT_DIV_CLK_PERI_USI06_USI", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_PERI_USI05_USI, VCLK_DIV_CLK_PERI_USI05_USI, "DOUT_DIV_CLK_PERI_USI05_USI", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_PERI_USI04_USI, VCLK_DIV_CLK_PERI_USI04_USI, "DOUT_DIV_CLK_PERI_USI04_USI", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_PERI_USI03_USI, VCLK_DIV_CLK_PERI_USI03_USI, "DOUT_DIV_CLK_PERI_USI03_USI", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_PERI_USI02_USI, VCLK_DIV_CLK_PERI_USI02_USI, "DOUT_DIV_CLK_PERI_USI02_USI", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_PERI_USI01_USI, VCLK_DIV_CLK_PERI_USI01_USI, "DOUT_DIV_CLK_PERI_USI01_USI", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_PERI_USI00_USI, VCLK_DIV_CLK_PERI_USI00_USI, "DOUT_DIV_CLK_PERI_USI00_USI", 0, 0, NULL),
};

/* CMU_RGBP */
struct init_vclk s5e8535_rgbp_hwacg_vclks[] = {
	HWACG_VCLK(GATE_MCFP_QCH, MCFP_QCH, "GATE_MCFP_QCH", NULL, 0, VCLK_GATE, NULL),
};

struct init_vclk s5e8535_rgbp_vclks[] = {
};

/* CMU_USB */
struct init_vclk s5e8535_usb_hwacg_vclks[] = {
};

struct init_vclk s5e8535_usb_vclks[] = {
};

/* CMU_YUVP */
struct init_vclk s5e8535_yuvp_hwacg_vclks[] = {
	HWACG_VCLK(GATE_YUVP_QCH, YUVP_QCH, "GATE_YUVP_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_MCSC_QCH, MCSC_QCH, "GATE_MCSC_QCH", NULL, 0, VCLK_GATE, NULL),
};

struct init_vclk s5e8535_yuvp_vclks[] = {
};

/* CMU_CSIS */
struct init_vclk s5e8535_csis_hwacg_vclks[] = {
	HWACG_VCLK(GATE_MIPI_DCPHY_LINK_WRAP_QCH_CSIS0, MIPI_DCPHY_LINK_WRAP_QCH_CSIS0, "GATE_MIPI_DCPHY_LINK_WRAP_QCH_CSIS0", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_MIPI_DCPHY_LINK_WRAP_QCH_CSIS1, MIPI_DCPHY_LINK_WRAP_QCH_CSIS1, "GATE_MIPI_DCPHY_LINK_WRAP_QCH_CSIS1", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_MIPI_DCPHY_LINK_WRAP_QCH_CSIS2, MIPI_DCPHY_LINK_WRAP_QCH_CSIS2, "GATE_MIPI_DCPHY_LINK_WRAP_QCH_CSIS2", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_MIPI_DCPHY_LINK_WRAP_QCH_CSIS3, MIPI_DCPHY_LINK_WRAP_QCH_CSIS3, "GATE_MIPI_DCPHY_LINK_WRAP_QCH_CSIS3", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_CSIS_PDP_QCH_DMA, CSIS_PDP_QCH_DMA, "GATE_CSIS_PDP_QCH_DMA", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_CSIS_PDP_QCH_PDP_TOP, CSIS_PDP_QCH_PDP_TOP, "GATE_CSIS_PDP_QCH_PDP_TOP", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_LH_AST_MI_OTF0_CSISCSTAT_QCH, LH_AST_MI_OTF0_CSISCSTAT_QCH, "GATE_LH_AST_MI_OTF0_CSISCSTAT_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_LH_AST_MI_OTF1_CSISCSTAT_QCH, LH_AST_MI_OTF1_CSISCSTAT_QCH, "GATE_LH_AST_MI_OTF1_CSISCSTAT_QCH", NULL, 0, VCLK_GATE, NULL),
};

struct init_vclk s5e8535_csis_vclks[] = {
};

/* CMU_CSTAT */
struct init_vclk s5e8535_cstat_hwacg_vclks[] = {
};

struct init_vclk s5e8535_cstat_vclks[] = {
};

/* CMU_M2M */
struct init_vclk s5e8535_m2m_hwacg_vclks[] = {
	HWACG_VCLK(GATE_M2M_QCH, M2M_QCH, "GATE_M2M_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_LME_QCH, LME_QCH, "GATE_LME_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_GDC_QCH, GDC_QCH, "GATE_GDC_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_VRA_QCH, VRA_QCH, "GATE_VRA_QCH", NULL, 0, VCLK_GATE, NULL),
};

struct init_vclk s5e8535_m2m_vclks[] = {
};

/* CMU_CMGP */
struct init_vclk s5e8535_cmgp_hwacg_vclks[] = {
	HWACG_VCLK(GATE_I2C_CMGP0_QCH, I2C_CMGP0_QCH, "GATE_I2C_CMGP0_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_I2C_CMGP1_QCH, I2C_CMGP1_QCH, "GATE_I2C_CMGP1_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_I2C_CMGP2_QCH, I2C_CMGP2_QCH, "GATE_I2C_CMGP2_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_I2C_CMGP3_QCH, I2C_CMGP3_QCH, "GATE_I2C_CMGP3_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_I2C_CMGP4_QCH, I2C_CMGP4_QCH, "GATE_I2C_CMGP4_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI_CMGP0_QCH, USI_CMGP0_QCH, "GATE_USI_CMGP0_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI_CMGP1_QCH, USI_CMGP1_QCH, "GATE_USI_CMGP1_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI_CMGP2_QCH, USI_CMGP2_QCH, "GATE_USI_CMGP2_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI_CMGP3_QCH, USI_CMGP3_QCH, "GATE_USI_CMGP3_QCH", NULL, 0, VCLK_GATE, NULL),
	HWACG_VCLK(GATE_USI_CMGP4_QCH, USI_CMGP4_QCH, "GATE_USI_CMGP4_QCH", NULL, 0, VCLK_GATE, NULL),
};

struct init_vclk s5e8535_cmgp_vclks[] = {
	VCLK(DOUT_DIV_CLK_CMGP_USI00_USI, DIV_CLK_CMGP_USI00_USI, "DOUT_DIV_CLK_CMGP_USI00_USI", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_CMGP_USI01_USI, DIV_CLK_CMGP_USI01_USI, "DOUT_DIV_CLK_CMGP_USI01_USI", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_CMGP_USI02_USI, DIV_CLK_CMGP_USI02_USI, "DOUT_DIV_CLK_CMGP_USI02_USI", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_CMGP_USI03_USI, DIV_CLK_CMGP_USI03_USI, "DOUT_DIV_CLK_CMGP_USI03_USI", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_CMGP_USI04_USI, DIV_CLK_CMGP_USI04_USI, "DOUT_DIV_CLK_CMGP_USI04_USI", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_CMGP_USI_I2C, DIV_CLK_CMGP_USI_I2C, "DOUT_DIV_CLK_CMGP_USI_I2C", 0, 0, NULL),
	VCLK(DOUT_DIV_CLK_CMGP_USI_I3C, DIV_CLK_CMGP_USI_I3C, "DOUT_DIV_CLK_CMGP_USI_I3C", 0, 0, NULL),
};

/* CMU_CPUCL0_GLB */
struct init_vclk s5e8535_cpucl0_glb_hwacg_vclks[] = {
};

struct init_vclk s5e8535_cpucl0_glb_vclks[] = {
	VCLK(MOUT_CLKCMU_CPUCL0_DBG_NOC_USER, MUX_CLKCMU_CPUCL0_DBG_NOC_USER, "MOUT_CLKCMU_CPUCL0_DBG_NOC_USER", 0, 0, NULL),
};

struct init_vclk s5e8535_icpu_hwacg_vclks[] = {
	HWACG_VCLK(GATE_ICPU_QCH_CPU0, ICPU_QCH_CPU0, "GATE_ICPU_QCH_CPU0", NULL, 0, VCLK_GATE | VCLK_QCH_DIS, NULL),
	HWACG_VCLK(GATE_ICPU_QCH_PERI, ICPU_QCH_PERI, "GATE_ICPU_QCH_PERI", NULL, 0, VCLK_GATE | VCLK_QCH_DIS, NULL),
};

static struct init_vclk s5e8535_clkout_vclks[] = {
	VCLK(OSC_AUD, VCLK_CLKOUT0, "OSC_AUD", 0, 0, NULL),
};

static struct of_device_id ext_clk_match[] = {
	{.compatible = "samsung,s5e8535-oscclk", .data = (void *)0},
	{},
};

void s5e8535_vclk_init(void)
{
	/* Common clock init */
}

static int s5e8535_clock_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	void __iomem *reg_base;

	if (np) {
		reg_base = of_iomap(np, 0);
		if (!reg_base)
			panic("%s: failed to map registers\n", __func__);
	} else {
		panic("%s: unable to determine soc\n", __func__);
	}

	s5e8535_clk_provider = samsung_clk_init(np, reg_base, CLK_NR_CLKS);
	if (!s5e8535_clk_provider)
		panic("%s: unable to allocate context.\n", __func__);

	samsung_register_of_fixed_ext(s5e8535_clk_provider, s5e8535_fixed_rate_ext_clks,
					  ARRAY_SIZE(s5e8535_fixed_rate_ext_clks),
					  ext_clk_match);
	/* register HWACG vclk */
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_top_hwacg_vclks, ARRAY_SIZE(s5e8535_top_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_alive_hwacg_vclks, ARRAY_SIZE(s5e8535_alive_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_hsi_hwacg_vclks, ARRAY_SIZE(s5e8535_hsi_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_mif0_hwacg_vclks, ARRAY_SIZE(s5e8535_mif0_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_mif1_hwacg_vclks, ARRAY_SIZE(s5e8535_mif1_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_nocl0_hwacg_vclks, ARRAY_SIZE(s5e8535_nocl0_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_nocl1a_hwacg_vclks, ARRAY_SIZE(s5e8535_nocl1a_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_peri_hwacg_vclks, ARRAY_SIZE(s5e8535_peri_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_usb_hwacg_vclks, ARRAY_SIZE(s5e8535_usb_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_cmgp_hwacg_vclks, ARRAY_SIZE(s5e8535_cmgp_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_cpucl0_glb_hwacg_vclks, ARRAY_SIZE(s5e8535_cpucl0_glb_hwacg_vclks));

	/* register special vclk */
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_top_vclks, ARRAY_SIZE(s5e8535_top_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_alive_vclks, ARRAY_SIZE(s5e8535_alive_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_hsi_vclks, ARRAY_SIZE(s5e8535_hsi_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_mif0_vclks, ARRAY_SIZE(s5e8535_mif0_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_mif1_vclks, ARRAY_SIZE(s5e8535_mif1_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_nocl0_vclks, ARRAY_SIZE(s5e8535_nocl0_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_nocl1a_vclks, ARRAY_SIZE(s5e8535_nocl1a_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_peri_vclks, ARRAY_SIZE(s5e8535_peri_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_usb_vclks, ARRAY_SIZE(s5e8535_usb_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_cmgp_vclks, ARRAY_SIZE(s5e8535_cmgp_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_cpucl0_glb_vclks, ARRAY_SIZE(s5e8535_cpucl0_glb_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_clkout_vclks, ARRAY_SIZE(s5e8535_clkout_vclks));

	clk_exynos_skip_hw = true;
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_aud_hwacg_vclks, ARRAY_SIZE(s5e8535_aud_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_mfc_hwacg_vclks, ARRAY_SIZE(s5e8535_mfc_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_rgbp_hwacg_vclks, ARRAY_SIZE(s5e8535_rgbp_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_yuvp_hwacg_vclks, ARRAY_SIZE(s5e8535_yuvp_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_csis_hwacg_vclks, ARRAY_SIZE(s5e8535_csis_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_cstat_hwacg_vclks, ARRAY_SIZE(s5e8535_cstat_hwacg_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_m2m_hwacg_vclks, ARRAY_SIZE(s5e8535_m2m_hwacg_vclks));

	samsung_register_vclk(s5e8535_clk_provider, s5e8535_aud_vclks, ARRAY_SIZE(s5e8535_aud_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_mfc_vclks, ARRAY_SIZE(s5e8535_mfc_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_rgbp_vclks, ARRAY_SIZE(s5e8535_rgbp_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_yuvp_vclks, ARRAY_SIZE(s5e8535_yuvp_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_csis_vclks, ARRAY_SIZE(s5e8535_csis_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_cstat_vclks, ARRAY_SIZE(s5e8535_cstat_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_m2m_vclks, ARRAY_SIZE(s5e8535_m2m_vclks));
	samsung_register_vclk(s5e8535_clk_provider, s5e8535_icpu_hwacg_vclks, ARRAY_SIZE(s5e8535_icpu_hwacg_vclks));
	clk_exynos_skip_hw = false;

	clk_register_fixed_factor(NULL, "pwm-clock", "fin_pll", CLK_SET_RATE_PARENT, 1, 1);
	samsung_clk_of_add_provider(np, s5e8535_clk_provider);

	s5e8535_vclk_init();

	pr_info("S5E8535: Clock setup completed\n");
	return 0;
}

static const struct of_device_id of_exynos_clock_match[] = {
	{ .compatible = "samsung,s5e8535-clock", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_clock_match);

static const struct platform_device_id exynos_clock_ids[] = {
	{ "s5e8535-clock", },
	{ }
};

static struct platform_driver s5e8535_clock_driver = {
	.driver = {
		.name = "s5e8535_clock",
		.of_match_table = of_exynos_clock_match,
	},
	.probe		= s5e8535_clock_probe,
	.id_table	= exynos_clock_ids,
};

static int s5e8535_clock_init(void)
{
	return platform_driver_register(&s5e8535_clock_driver);
}
arch_initcall(s5e8535_clock_init);

static void s5e8535_clock_exit(void)
{
	return platform_driver_unregister(&s5e8535_clock_driver);
}
module_exit(s5e8535_clock_exit);

MODULE_LICENSE("GPL");
