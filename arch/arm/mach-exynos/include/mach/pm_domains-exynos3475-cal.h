/*
 * Cal header file for Exynos Generic power domain.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* ########################
   ##### BLK_G3D info #####
   ######################## */

static struct exynos_pd_reg sys_pwr_regs_g3d[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_G3D_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_G3D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_G3D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_G3D_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_G3D_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_g3d[] = {
	{ .reg = EXYNOS3475_CLK_CON_DIV_ACLK_G3D_667, },
	{ .reg = EXYNOS3475_CLK_CON_DIV_PCLK_G3D_167, },
	{ .reg = EXYNOS3475_G3D_PLL_LOCK },
	{ .reg = EXYNOS3475_G3D_PLL_CON0 },
	{ .reg = EXYNOS3475_CLK_CON_MUX_G3D_PLL },
	{ .reg = EXYNOS3475_CLK_CON_MUX_ACLK_G3D_400_USER },
	{ .reg = EXYNOS3475_CLK_CON_MUX_ACLK_G3D_667 },
	{ .reg = EXYNOS3475_CLK_ENABLE_ACLK_G3D_667 },
	{ .reg = EXYNOS3475_CLK_ENABLE_ACLK_G3D_667_SECURE_CFW },
	{ .reg = EXYNOS3475_CLK_ENABLE_PCLK_G3D_167 },
	{ .reg = EXYNOS3475_CLK_ENABLE_PCLK_G3D_167_SECURE_CFW },
};

/* #########################
   ##### BLK_ISP info #####
   ######################### */

static struct exynos_pd_reg sys_pwr_regs_isp[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_ISP_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_ISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_ISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_ISP_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_ISP_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_isp[] = {
	{ .reg = EXYNOS3475_CLK_CON_DIV_PCLK_ISP_150, },
	{ .reg = EXYNOS3475_CLK_CON_MUX_ACLK_ISP_300_USER, },
	{ .reg = EXYNOS3475_CLK_CON_MUX_PHYCLK_CSI_LINK_RX_BYTE_CLK_HS_USER, },
	{ .reg = EXYNOS3475_CLK_ENABLE_ACLK_ISP_300, },
	{ .reg = EXYNOS3475_CLK_ENABLE_PCLK_ISP_150, },
	{ .reg = EXYNOS3475_CLK_ENABLE_PHYCLK_CSI_LINK0_RX_BYTE_CLK_HS0, },
};

/* ############################
   ##### BLK_DISPAUD info #####
   ############################ */

static struct exynos_pd_reg sys_pwr_regs_dispaud[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_DISPAUD_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_DISPAUD_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_DISPAUD_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_DISPAUD_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_DISPAUD_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_dispaud[] = {
	{ .reg = EXYNOS3475_CLK_CON_MUX_ACLK_DISPAUD_133_USER, },
	{ .reg = EXYNOS3475_CLK_CON_MUX_SCLK_DECON_INT_VCLK_USER,},
	{ .reg = EXYNOS3475_CLK_CON_MUX_SCLK_DECON_INT_ECLK_USER,},
	{ .reg = EXYNOS3475_CLK_CON_MUX_PHYCLK_MIPI_PHY_M_TXBYTECLKHS_M4S4_USER,},
	{ .reg = EXYNOS3475_CLK_CON_MUX_PHYCLK_MIPI_PHY_M_RXCLKESC0_M4S4_USER,},
	{ .reg = EXYNOS3475_CLK_CON_MUX_SCLK_MI2S_AUD_USER,},
	{ .reg = EXYNOS3475_CLK_CON_MUX_SCLK_DECON_INT_ECLK,},
	{ .reg = EXYNOS3475_CLK_CON_MUX_SCLK_MI2S_AUD,},
	{ .reg = EXYNOS3475_CLK_CON_DIV_SCLK_DECON_INT_VCLK,},
	{ .reg = EXYNOS3475_CLK_CON_DIV_SCLK_DECON_INT_ECLK,},
	{ .reg = EXYNOS3475_CLK_CON_DIV_SCLK_MI2S_AUD,},
	{ .reg = EXYNOS3475_CLK_CON_DIV_SCLK_MIXER_AUD,},
	{ .reg = EXYNOS3475_CLK_ENABLE_ACLK_DISPAUD_133,},
	{ .reg = EXYNOS3475_CLK_ENABLE_ACLK_DISPAUD_133_SECURE_CFW,},
	{ .reg = EXYNOS3475_CLK_ENABLE_SCLK_DECON_INT_VCLK,},
	{ .reg = EXYNOS3475_CLK_ENABLE_SCLK_DECON_INT_ECLK,},
	{ .reg = EXYNOS3475_CLK_ENABLE_PHYCLK_MIPI_PHY_M_TXBYTECLKHS_M4S4,},
	{ .reg = EXYNOS3475_CLK_ENABLE_PHYCLK_MIPI_PHY_M_RXCLKESC0_M4S4,},
	{ .reg = EXYNOS3475_CLK_ENABLE_SCLK_MI2S_AUD,},
	{ .reg = EXYNOS3475_CLK_ENABLE_SCLK_MIXER_AUD,},
	{ .reg = EXYNOS3475_CLK_ENABLE_IOCLK_AUD_I2S_SCLK_AP_IN,},
	{ .reg = EXYNOS3475_CLK_ENABLE_IOCLK_AUD_I2S_BCLK_BT_IN,},
	{ .reg = EXYNOS3475_CLK_ENABLE_IOCLK_AUD_I2S_BCLK_CP_IN,},
	{ .reg = EXYNOS3475_CLK_ENABLE_IOCLK_AUD_I2S_BCLK_FM_IN,},
	{ .reg = EXYNOS3475_CLKOUT_CMU_DISPAUD,},
	{ .reg = EXYNOS3475_CLKOUT_CMU_DISPAUD_DIV_STAT,},
};

/* ############################
   ##### BLK_MFCMSCL info #####
   ############################ */

static struct exynos_pd_reg sys_pwr_regs_mscl[] = {
	{ .reg = EXYNOS_PMU_CLKRUN_CMU_MFCMSCL_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_CLKSTOP_CMU_MFCMSCL_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_DISABLE_PLL_CMU_MFCMSCL_SYS_PWR_REG,	.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_LOGIC_MFCMSCL_SYS_PWR_REG,		.bit_offset = 0, },
	{ .reg = EXYNOS_PMU_RESET_CMU_MFCMSCL_SYS_PWR_REG,		.bit_offset = 0, },
};

static struct sfr_save save_list_mscl[] = {
	{ .reg = EXYNOS3475_CLK_CON_DIV_PCLK_MFCMSCL_83, },
	{ .reg = EXYNOS3475_CLK_CON_MUX_ACLK_MFCMSCL_333_USER, },
	{ .reg = EXYNOS3475_CLK_CON_MUX_ACLK_MFCMSCL_200_USER, },
	{ .reg = EXYNOS3475_CLK_ENABLE_ACLK_MFCMSCL_333, },
	{ .reg = EXYNOS3475_CLK_ENABLE_ACLK_MFCMSCL_333_SECURE_CFW, },
	{ .reg = EXYNOS3475_CLK_ENABLE_PCLK_MFCMSCL_83, },
	{ .reg = EXYNOS3475_CLK_ENABLE_ACLK_MFCMSCL_200, },
};
