/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *************************************************
 * Automatically generated C header: do not edit *
 *************************************************
 */

/*
 * Clock (0)Name, Clock (1)fixed rate, Clock Enable (2)Ctrl and (3)Bit,
 * Clock Divisor (4)Ctrl and (5)Bit, Clock Parent (6)Ctrl and (7)Bit,
 * and Parent Select (15)Count and (16)Lists[ ... ...]
 */

SCI_CLK_ADD(ext_26m, 26000000, REG_PMU_APB_CGM_AP_EN, BIT(0),
	0, 0, 0, 0, 0);

SCI_CLK_ADD(ext_32k, 32768, 0, 0,
	0, 0, 0, 0, 0);

SCI_CLK_ADD(clk_mpll, 0, REG_PMU_APB_CGM_AP_EN, BIT(6),
	REG_AON_APB_MPLL_CFG, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10), 0, 0,
	1, &ext_26m);

SCI_CLK_ADD(clk_dpll, 0, REG_PMU_APB_CGM_AP_EN, BIT(1),
	REG_AON_APB_DPLL_CFG, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10), 0, 0,
	1, &ext_26m);

SCI_CLK_ADD(clk_td_sel_i, 0, REG_PMU_APB_TDPLL_REL_CFG, BIT(0),
	0, 0, 0, 0, 0);

SCI_CLK_ADD(clk_w_sel_i, 0, REG_PMU_APB_WPLL_REL_CFG, BIT(0),
	0, 0, 0, 0, 0);

SCI_CLK_ADD(clk_c_sel_i, 0, REG_PMU_APB_CPLL_REL_CFG, BIT(0),
	0, 0, 0, 0, 0);

SCI_CLK_ADD(clk_wifi_sel_i, 0, REG_PMU_APB_WIFIPLL1_REL_CFG, BIT(0),
	0, 0, 0, 0, 0);

SCI_CLK_ADD(clk_tdpll, 768000000, REG_PMU_APB_CGM_AP_EN, BIT(3),
	0, 0, 0, 0,
	1, &clk_td_sel_i);

SCI_CLK_ADD(clk_wpll, 921600000, REG_PMU_APB_CGM_AP_EN, BIT(5),
	0, 0, 0, 0,
	1, &clk_w_sel_i);

SCI_CLK_ADD(clk_cpll, 624000000, REG_PMU_APB_CGM_AP_EN, BIT(2),
	0, 0, 0, 0,
	1, &clk_c_sel_i);

SCI_CLK_ADD(clk_wifipll, 880000000, REG_PMU_APB_CGM_AP_EN, BIT(4),
	0, 0, 0, 0,
	1, &clk_wifi_sel_i);

SCI_CLK_ADD(clk_300m, 0, REG_PMU_APB_PLL_DIV_EN1, BIT(0),
	3, 0, 0, 0,
	1, &clk_mpll);

SCI_CLK_ADD(clk_37m5, 0, REG_PMU_APB_PLL_DIV_EN1, BIT(1),
	24, 0, 0, 0,
	1, &clk_mpll);

SCI_CLK_ADD(clk_533m, 0, 0, 0,
	0, 0, 0, 0,
	1, &clk_dpll);

SCI_CLK_ADD(clk_66m, 66000000, REG_PMU_APB_PLL_DIV_EN1, BIT(2),
	0, 0, 0, 0,
	1, &clk_dpll);

SCI_CLK_ADD(clk_51m2_w, 51200000, REG_PMU_APB_PLL_DIV_EN1, BIT(23),
	0, 0, 0, 0,
	1, &clk_wpll);

SCI_CLK_ADD(clk_40m, 40000000, REG_PMU_APB_PLL_DIV_EN1, BIT(27),
	0, 0, 0, 0,
	1, &clk_wifipll);

SCI_CLK_ADD(clk_312m, 0, REG_PMU_APB_CGM_AP_EN, BIT(7),
	2, 0, 0, 0,
	1, &clk_cpll);

SCI_CLK_ADD(clk_208m, 0, REG_PMU_APB_PLL_DIV_EN1, BIT(17),
	3, 0, 0, 0,
	1, &clk_cpll);

SCI_CLK_ADD(clk_104m, 0, REG_PMU_APB_PLL_DIV_EN1, BIT(18),
	6, 0, 0, 0,
	1, &clk_cpll);

SCI_CLK_ADD(clk_52m, 0, REG_PMU_APB_PLL_DIV_EN1, BIT(19),
	12, 0, 0, 0,
	1, &clk_cpll);

SCI_CLK_ADD(clk_384m, 0, REG_PMU_APB_CGM_AP_EN, BIT(8),
	2, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_192m, 0, REG_PMU_APB_CGM_AP_EN, BIT(10),
	4, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_96m, 0, REG_PMU_APB_CGM_AP_EN, BIT(13),
	8, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_48m, 0, REG_PMU_APB_CGM_AP_EN, BIT(17),
	16, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_24m, 0, REG_PMU_APB_CGM_AP_EN, BIT(18),
	32, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_12m, 0, REG_PMU_APB_CGM_AP_EN, BIT(19),
	64, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_256m, 0, REG_PMU_APB_CGM_AP_EN, BIT(9),
	3, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_128m, 0, REG_PMU_APB_CGM_AP_EN, BIT(12),
	6, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_64m, 0, REG_PMU_APB_CGM_AP_EN, BIT(15),
	12, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_153m6, 0, REG_PMU_APB_CGM_AP_EN, BIT(11),
	5, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_51m2, 0, REG_PMU_APB_CGM_AP_EN, BIT(16),
	15, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_76m8, 0, REG_PMU_APB_CGM_AP_EN, BIT(14),
	10, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_38m4, 0, REG_PMU_APB_PLL_DIV_EN1, BIT(15),
	20, 0, 0, 0,
	1, &clk_tdpll);

SCI_CLK_ADD(clk_mcu, 0, 0, 0,
	REG_AP_AHB_CA7_CKG_CFG, BIT(4)|BIT(5)|BIT(6), REG_AP_AHB_CA7_CKG_CFG, BIT(0)|BIT(1)|BIT(2),
	7, &ext_26m, &clk_dpll, &clk_cpll, &clk_tdpll, &clk_wifipll, &clk_wpll, &clk_mpll);

SCI_CLK_ADD(clk_arm, 0, 0, 0,
	0, 0, 0, 0,
	1, &clk_mcu);

SCI_CLK_ADD(clk_axi, 0, 0, 0,
	REG_AP_AHB_CA7_CKG_CFG, BIT(8)|BIT(9)|BIT(10), 0, 0,
	1, &clk_mcu);

SCI_CLK_ADD(clk_dbg, 0, REG_AP_AHB_MISC_CKG_EN, BIT(8),
	REG_AP_AHB_CA7_CKG_CFG, BIT(16)|BIT(17)|BIT(18), 0, 0,
	1, &clk_mcu);

SCI_CLK_ADD(clk_ahb, 0, 0, 0,
	0, 0, REG_AP_CLK_AP_AHB_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_76m8, &clk_128m, &clk_192m);

SCI_CLK_ADD(clk_apb, 0, 0, 0,
	0, 0, REG_AP_CLK_AP_APB_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_64m, &clk_96m, &clk_128m);

SCI_CLK_ADD(clk_pub_ahb, 0, 0, 0,
	0, 0, REG_AON_CLK_PUB_AHB_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_96m, &clk_128m, &clk_153m6);

SCI_CLK_ADD(clk_emc, 0, 0, 0,
	REG_AON_CLK_EMC_CFG, BIT(8)|BIT(9), REG_AON_CLK_EMC_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_256m, &clk_384m, &clk_533m);

SCI_CLK_ADD(clk_aon_apb, 0, 0, 0,
	REG_AON_CLK_AON_APB_CFG, BIT(8)|BIT(9), REG_AON_CLK_AON_APB_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_76m8, &clk_96m, &clk_128m);

SCI_CLK_ADD(clk_disp_emc, 0, REG_AON_APB_APB_EB1, BIT(11),
	0, 0, 0, 0,
	1, &clk_aon_apb);

SCI_CLK_ADD(clk_gsp, 0, REG_AP_AHB_AHB_EB, BIT(3),
	0, 0, REG_AP_CLK_GSP_CFG, BIT(0)|BIT(1),
	4, &clk_96m, &clk_153m6, &clk_192m, &clk_256m);

SCI_CLK_ADD(clk_disc0, 0, REG_AP_AHB_AHB_EB, BIT(1),
	REG_AP_CLK_DISPC0_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_DISPC0_CFG, BIT(0)|BIT(1),
	4, &clk_153m6, &clk_192m, &clk_256m, &clk_312m);

SCI_CLK_ADD(clk_disc0_dbi, 0, REG_AP_AHB_AHB_EB, BIT(1),
	REG_AP_CLK_DISPC0_DBI_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_DISPC0_DBI_CFG, BIT(0)|BIT(1),
	4, &clk_128m, &clk_153m6, &clk_192m, &clk_256m);

SCI_CLK_ADD(clk_disc0_dpi, 0, REG_AP_AHB_AHB_EB, BIT(1),
	REG_AP_CLK_DISPC0_DPI_CFG, BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15), REG_AP_CLK_DISPC0_DPI_CFG, BIT(0)|BIT(1),
	4, &clk_128m, &clk_153m6, &clk_192m, &clk_384m);

SCI_CLK_ADD(clk_disc1, 0, REG_AP_AHB_AHB_EB, BIT(2),
	REG_AP_CLK_DISPC1_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_DISPC1_CFG, BIT(0)|BIT(1),
	4, &clk_153m6, &clk_192m, &clk_256m, &clk_312m);

SCI_CLK_ADD(clk_disc1_dbi, 0, REG_AP_AHB_AHB_EB, BIT(2),
	REG_AP_CLK_DISPC1_DBI_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_DISPC1_DBI_CFG, BIT(0)|BIT(1),
	4, &clk_128m, &clk_153m6, &clk_192m, &clk_256m);

SCI_CLK_ADD(clk_disc1_dpi, 0, REG_AP_AHB_AHB_EB, BIT(2),
	REG_AP_CLK_DISPC1_DPI_CFG, BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15), REG_AP_CLK_DISPC1_DPI_CFG, BIT(0)|BIT(1),
	4, &clk_128m, &clk_153m6, &clk_192m, &clk_384m);

SCI_CLK_ADD(clk_nfc, 0, REG_AP_AHB_AHB_EB, BIT(6),
	REG_AP_CLK_NFC_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_NFC_CFG, BIT(0)|BIT(1),
	3, &clk_64m, &clk_128m, &clk_153m6);

SCI_CLK_ADD(clk_sdio0, 0, REG_AP_AHB_AHB_EB, BIT(8),
	REG_AP_CLK_SDIO0_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_SDIO0_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_192m, &clk_256m, &clk_312m);

SCI_CLK_ADD(clk_sdio1, 0, REG_AP_AHB_AHB_EB, BIT(9),
	0, 0, REG_AP_CLK_SDIO1_CFG, BIT(0)|BIT(1),
	4, &clk_48m, &clk_76m8, &clk_96m, &clk_128m);

SCI_CLK_ADD(clk_sdio2, 0, REG_AP_AHB_AHB_EB, BIT(10),
	0, 0, REG_AP_CLK_SDIO2_CFG, BIT(0)|BIT(1),
	4, &clk_48m, &clk_76m8, &clk_96m, &clk_128m);

SCI_CLK_ADD(clk_emmc, 0, REG_AP_AHB_AHB_EB, BIT(11),
	0, 0, REG_AP_CLK_EMMC_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_192m, &clk_256m, &clk_312m);

SCI_CLK_ADD(clk_gps_tcxo, 64000000, REG_AP_CLK_GPS_TCXO_CFG, BIT(16),
	0, 0, 0, 0, 0);

SCI_CLK_ADD(clk_gps, 0, REG_AP_AHB_AHB_EB, BIT(12),
	0, 0, REG_AP_CLK_GPS_CFG, BIT(0),
	2, &clk_64m, &clk_76m8);

SCI_CLK_ADD(clk_usb_ref, 0, REG_AP_AHB_AHB_EB, BIT(4),
	0, 0, REG_AP_CLK_USB_REF_CFG, BIT(0),
	2, &clk_12m, &clk_24m);

SCI_CLK_ADD(clk_uart0, 0, REG_AP_APB_APB_EB, BIT(13),
	REG_AP_CLK_UART0_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_UART0_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_51m2, &clk_96m);

SCI_CLK_ADD(clk_uart1, 0, REG_AP_APB_APB_EB, BIT(14),
	REG_AP_CLK_UART1_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_UART1_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_51m2, &clk_96m);

SCI_CLK_ADD(clk_uart2, 0, REG_AP_APB_APB_EB, BIT(15),
	REG_AP_CLK_UART2_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_UART2_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_51m2, &clk_96m);

SCI_CLK_ADD(clk_uart3, 0, REG_AP_APB_APB_EB, BIT(16),
	REG_AP_CLK_UART3_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_UART3_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_51m2, &clk_96m);

SCI_CLK_ADD(clk_uart4, 0, REG_AP_APB_APB_EB, BIT(17),
	REG_AP_CLK_UART4_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_UART4_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_51m2, &clk_96m);

SCI_CLK_ADD(clk_i2c0, 0, REG_AP_APB_APB_EB, BIT(8),
	REG_AP_CLK_I2C0_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_I2C0_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_51m2, &clk_96m);

SCI_CLK_ADD(clk_i2c1, 0, REG_AP_APB_APB_EB, BIT(9),
	REG_AP_CLK_I2C1_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_I2C1_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_51m2, &clk_96m);

SCI_CLK_ADD(clk_i2c2, 0, REG_AP_APB_APB_EB, BIT(10),
	REG_AP_CLK_I2C2_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_I2C2_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_51m2, &clk_96m);

SCI_CLK_ADD(clk_i2c3, 0, REG_AP_APB_APB_EB, BIT(11),
	REG_AP_CLK_I2C3_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_I2C3_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_51m2, &clk_96m);

SCI_CLK_ADD(clk_i2c4, 0, REG_AP_APB_APB_EB, BIT(12),
	REG_AP_CLK_I2C4_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_I2C4_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_51m2, &clk_96m);

SCI_CLK_ADD(clk_spi0, 0, REG_AP_APB_APB_EB, BIT(5),
	REG_AP_CLK_SPI0_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_SPI0_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_96m, &clk_153m6, &clk_192m);

SCI_CLK_ADD(clk_spi1, 0, REG_AP_APB_APB_EB, BIT(6),
	REG_AP_CLK_SPI1_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_SPI1_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_96m, &clk_153m6, &clk_192m);

SCI_CLK_ADD(clk_spi2, 0, REG_AP_APB_APB_EB, BIT(7),
	REG_AP_CLK_SPI2_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_SPI2_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_96m, &clk_153m6, &clk_192m);

SCI_CLK_ADD(clk_iis0, 0, REG_AP_APB_APB_EB, BIT(1),
	REG_AP_CLK_IIS0_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_IIS0_CFG, BIT(0)|BIT(1),
	3, &ext_26m, &clk_76m8, &clk_128m);

SCI_CLK_ADD(clk_iis1, 0, REG_AP_APB_APB_EB, BIT(2),
	REG_AP_CLK_IIS1_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_IIS1_CFG, BIT(0)|BIT(1),
	3, &ext_26m, &clk_76m8, &clk_128m);

SCI_CLK_ADD(clk_iis2, 0, REG_AP_APB_APB_EB, BIT(3),
	REG_AP_CLK_IIS2_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_IIS2_CFG, BIT(0)|BIT(1),
	3, &ext_26m, &clk_76m8, &clk_128m);

SCI_CLK_ADD(clk_iis3, 0, REG_AP_APB_APB_EB, BIT(4),
	REG_AP_CLK_IIS3_CFG, BIT(8)|BIT(9)|BIT(10), REG_AP_CLK_IIS3_CFG, BIT(0)|BIT(1),
	3, &ext_26m, &clk_76m8, &clk_128m);

SCI_CLK_ADD(clk_gpu_top_i, 0, REG_PMU_APB_PD_GPU_TOP_CFG+1, BIT(25),
	0, 0, 0, 0,
	1, &clk_aon_apb);

SCI_CLK_ADD(clk_gpu_i, 0, REG_AON_APB_APB_EB0, BIT(27),
	0, 0, 0, 0,
	1, &clk_gpu_top_i);

SCI_CLK_ADD(clk_gpu, 0, &clk_gpu_i, 0,
	REG_GPU_APB_APB_CLK_CTRL, BIT(2)|BIT(3), REG_GPU_APB_APB_CLK_CTRL, BIT(0)|BIT(1),
	4, &clk_208m, &clk_256m, &clk_300m, &clk_312m);

SCI_CLK_ADD(clk_mm_top_i, 0, REG_PMU_APB_PD_MM_TOP_CFG+1, BIT(25),
	0, 0, 0, 0,
	1, &clk_aon_apb);

SCI_CLK_ADD(clk_mm_i, 0, REG_AON_APB_APB_EB0, BIT(25),
	0, 0, 0, 0,
	1, &clk_mm_top_i);

SCI_CLK_ADD(clk_mm_ahb, 0, 0, 0,
	0, 0, REG_MM_CLK_MM_AHB_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_96m, &clk_128m, &clk_153m6);

SCI_CLK_ADD(clk_mm_mtx_axi, 0, REG_MM_AHB_GEN_CKG_CFG, BIT(8),
	0, 0, 0, 0,
	1, &clk_mm_i);

SCI_CLK_ADD(clk_mm_axi, 0, REG_MM_AHB_GEN_CKG_CFG, BIT(7),
	0, 0, 0, 0,
	1, &clk_mm_mtx_axi);

SCI_CLK_ADD(clk_mm_ckg_i, 0, REG_MM_AHB_AHB_EB, BIT(6),
	0, 0, 0, 0,
	1, &clk_mm_i);

SCI_CLK_ADD(clk_jpg_i, 0, REG_MM_AHB_AHB_EB, BIT(5),
	0, 0, 0, 0,
	1, &clk_mm_ahb);

SCI_CLK_ADD(clk_jpg_ckg_i, 0, REG_MM_AHB_GEN_CKG_CFG, BIT(6),
	0, 0, 0, 0,
	1, &clk_jpg_i);

SCI_CLK_ADD(clk_jpg, 0, &clk_jpg_ckg_i, 0,
	0, 0, REG_MM_CLK_JPG_CFG, BIT(0)|BIT(1),
	4, &clk_76m8, &clk_128m, &clk_192m, &clk_256m);

SCI_CLK_ADD(clk_vsp_i, 0, REG_MM_AHB_AHB_EB, BIT(3),
	0, 0, 0, 0,
	1, &clk_mm_ahb);

SCI_CLK_ADD(clk_vsp_ckg_i, 0, REG_MM_AHB_GEN_CKG_CFG, BIT(5),
	0, 0, 0, 0,
	1, &clk_vsp_i);

SCI_CLK_ADD(clk_vsp, 0, &clk_vsp_ckg_i, 0,
	0, 0, REG_MM_CLK_VSP_CFG, BIT(0)|BIT(1),
	4, &clk_76m8, &clk_128m, &clk_192m, &clk_256m);

SCI_CLK_ADD(clk_isp_i, 0, REG_MM_AHB_AHB_EB, BIT(2),
	0, 0, 0, 0,
	1, &clk_mm_ahb);

SCI_CLK_ADD(clk_isp_ckg_i, 0, REG_MM_AHB_GEN_CKG_CFG, BIT(4),
	0, 0, 0, 0,
	1, &clk_isp_i);

SCI_CLK_ADD(clk_isp, 0, &clk_isp_ckg_i, 0,
	0, 0, REG_MM_CLK_ISP_CFG, BIT(0)|BIT(1),
	4, &clk_76m8, &clk_128m, &clk_192m, &clk_256m);

SCI_CLK_ADD(clk_sensor_i, 0, REG_MM_AHB_AHB_EB, BIT(1),
	0, 0, 0, 0,
	1, &clk_mm_ahb);

SCI_CLK_ADD(clk_sensor_ckg_i, 0, REG_MM_AHB_GEN_CKG_CFG, BIT(2),
	0, 0, 0, 0,
	1, &clk_sensor_i);

SCI_CLK_ADD(clk_sensor, 0, &clk_sensor_ckg_i, 0,
	REG_MM_CLK_SENSOR_CFG, BIT(8)|BIT(9)|BIT(10), REG_MM_CLK_SENSOR_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_76m8, &clk_96m);

SCI_CLK_ADD(clk_ccir_in, 64000000, 0, 0,
	0, 0, 0, 0, 0);

SCI_CLK_ADD(clk_ccir_i, 0, REG_MM_AHB_AHB_EB, BIT(1),
	0, 0, 0, 0,
	1, &clk_mm_ahb);

SCI_CLK_ADD(clk_ccir, 0, &clk_ccir_i, 0,
	0, 0, REG_MM_CLK_CCIR_CFG, BIT(16),
	2, &clk_24m, &clk_ccir_in);

SCI_CLK_ADD(clk_dcam_i, 0, REG_MM_AHB_AHB_EB, BIT(0),
	0, 0, 0, 0,
	1, &clk_mm_ahb);

SCI_CLK_ADD(clk_dcam_ckg_i, 0, REG_MM_AHB_GEN_CKG_CFG, BIT(3),
	0, 0, 0, 0,
	1, &clk_dcam_i);

SCI_CLK_ADD(clk_dcam, 0, &clk_dcam_ckg_i, 0,
	0, 0, REG_MM_CLK_DCAM_CFG, BIT(0)|BIT(1),
	4, &clk_76m8, &clk_128m, &clk_192m, &clk_256m);

SCI_CLK_ADD(clk_csi_i, 0, REG_MM_AHB_AHB_EB, BIT(4),
	0, 0, 0, 0,
	1, &clk_mm_ahb);

SCI_CLK_ADD(clk_csi_ckg_i, 0, REG_MM_AHB_GEN_CKG_CFG, BIT(1),
	0, 0, 0, 0,
	1, &clk_csi_i);

SCI_CLK_ADD(clk_cphy_cfg_i, 0, REG_MM_AHB_GEN_CKG_CFG, BIT(0),
	0, 0, 0, 0,
	1, &clk_mm_ahb);

SCI_CLK_ADD(clk_dcam_mipi, 0, &clk_cphy_cfg_i, 0,
	0, 0, 0, 0,
	1, &clk_csi_ckg_i);

SCI_CLK_ADD(clk_aud, 0, REG_AON_APB_APB_EB0, BIT(18),
	0, 0, 0, 0,
	1, &ext_26m);

SCI_CLK_ADD(clk_audif, 0, REG_AON_APB_APB_EB0, BIT(17),
	0, 0, REG_AON_CLK_AUDIF_CFG, BIT(0)|BIT(1),
	3, &ext_26m, &clk_38m4, &clk_51m2);

SCI_CLK_ADD(clk_vbc, 0, REG_AON_APB_APB_EB0, BIT(19),
	0, 0, 0, 0,
	1, &ext_26m);

SCI_CLK_ADD(clk_fm_in, 64000000, REG_AON_CLK_FM_CFG, BIT(16),
	0, 0, 0, 0, 0);

SCI_CLK_ADD(clk_fm, 0, REG_AON_APB_APB_EB0, BIT(1),
	0, 0, 0, 0,
	1, &clk_fm_in);

SCI_CLK_ADD(clk_adi, 0, REG_AON_APB_APB_EB0, BIT(16),
	0, 0, REG_AON_CLK_ADI_CFG, BIT(0)|BIT(1),
	3, &ext_26m, &clk_51m2, &clk_76m8);

SCI_CLK_ADD(clk_aux0, 0, REG_AON_APB_APB_EB1, BIT(2),
	REG_AON_APB_AON_CGM_CFG, BIT(16)|BIT(17)|BIT(18)|BIT(19), REG_AON_APB_AON_CGM_CFG, BIT(0)|BIT(1)|BIT(2)|BIT(3),
	10, &ext_32k, &ext_26m, &ext_26m, &clk_48m, &clk_52m, &clk_51m2, &clk_37m5, &clk_40m, &clk_66m, &clk_40m);

SCI_CLK_ADD(clk_aux1, 0, REG_AON_APB_APB_EB1, BIT(3),
	REG_AON_APB_AON_CGM_CFG, BIT(20)|BIT(21)|BIT(22)|BIT(23), REG_AON_APB_AON_CGM_CFG, BIT(4)|BIT(5)|BIT(6)|BIT(7),
	10, &ext_32k, &ext_26m, &ext_26m, &clk_48m, &clk_52m, &clk_51m2, &clk_37m5, &clk_40m, &clk_66m, &clk_40m);

SCI_CLK_ADD(clk_aux2, 0, REG_AON_APB_APB_EB1, BIT(4),
	REG_AON_APB_AON_CGM_CFG, BIT(24)|BIT(25)|BIT(26)|BIT(27), REG_AON_APB_AON_CGM_CFG, BIT(8)|BIT(9)|BIT(10)|BIT(11),
	10, &ext_32k, &ext_26m, &ext_26m, &clk_48m, &clk_52m, &clk_51m2, &clk_37m5, &clk_40m, &clk_66m, &clk_40m);

SCI_CLK_ADD(clk_pwm0, 0, REG_AON_APB_APB_EB0, BIT(4),
	0, 0, REG_AON_CLK_PWM0_CFG, BIT(0),
	2, &ext_32k, &ext_26m);

SCI_CLK_ADD(clk_pwm1, 0, REG_AON_APB_APB_EB0, BIT(5),
	0, 0, REG_AON_CLK_PWM1_CFG, BIT(0),
	2, &ext_32k, &ext_26m);

SCI_CLK_ADD(clk_pwm2, 0, REG_AON_APB_APB_EB0, BIT(6),
	0, 0, REG_AON_CLK_PWM2_CFG, BIT(0),
	2, &ext_32k, &ext_26m);

SCI_CLK_ADD(clk_pwm3, 0, REG_AON_APB_APB_EB0, BIT(7),
	0, 0, REG_AON_CLK_PWM3_CFG, BIT(0),
	2, &ext_32k, &ext_26m);

SCI_CLK_ADD(clk_efuse, 0, REG_AON_APB_APB_EB0, BIT(13),
	0, 0, 0, 0,
	1, &ext_26m);

SCI_CLK_ADD(clk_dap, 0, REG_AON_APB_APB_EB0, BIT(30),
	0, 0, REG_AON_CLK_CA7_DAP_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_76m8, &clk_128m, &clk_153m6);

SCI_CLK_ADD(clk_ts, 0, REG_AON_APB_APB_EB0, BIT(28),
	0, 0, REG_AON_CLK_CA7_TS_CFG, BIT(0)|BIT(1),
	4, &ext_32k, &ext_26m, &clk_128m, &clk_153m6);

SCI_CLK_ADD(clk_mspi, 0, REG_AON_APB_APB_EB0, BIT(23),
	0, 0, REG_AON_CLK_MSPI_CFG, BIT(0)|BIT(1),
	3, &clk_52m, &clk_76m8, &clk_96m);

SCI_CLK_ADD(clk_i2c, 0, REG_AON_APB_APB_EB0, BIT(31),
	0, 0, REG_AON_CLK_I2C_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_51m2, &clk_96m);

SCI_CLK_ADD(clk_avs0, 0, REG_AON_APB_APB_EB0, BIT(6),
	0, 0, REG_AON_CLK_AVS0_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_51m2, &clk_96m);

SCI_CLK_ADD(clk_avs1, 0, REG_AON_APB_APB_EB0, BIT(7),
	0, 0, REG_AON_CLK_AVS1_CFG, BIT(0)|BIT(1),
	4, &ext_26m, &clk_48m, &clk_51m2, &clk_96m);

