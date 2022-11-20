/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos3475 SoC.
*/

#include <linux/of.h>
#include <linux/of_address.h>
#include <mach/regs-clock-exynos3475.h>
#include <dt-bindings/clock/exynos3475.h>
#include "composite.h"

#ifndef ALWAYS_ON_CLK
#define ALWAYS_ON_CLK
#endif

#ifndef UNREGISTERED_CLK
#define UNREGISTERED_CLK
#endif

enum exynos3475_clks {
	none,

	/* BUS0 */
	mout_aclk_bus0_333_user = 1,
	dout_pclk_bus0_83, aclk_bus0_333_user_aclk_bus_d_0_aclk_bus0nd_333,
	pclk_bus0_83_aclk_bus_d_0_aclk_bus0nd_82, pclk_bus0_83_pclk_sysreg_bus0_pclk, pclk_bus0_83_pclk_pmu_bus0_pclk,
	pclk_bus0_83_aclk_ahb2apb_bus0_hclk, pclk_bus0_83_aclk_bus_p_0_aclk_bus0np,

	/* BUS2 */
	mout_aclk_bus2_333_user,
	dout_pclk_bus2_83, aclk_bus2_333_user_aclk_bus_d_2_aclk_bus2nd_333 = 11, pclk_bus2_83_aclk_bus_d_2_aclk_bus2nd_82,
	pclk_bus2_83_pclk_sysreg_bus2_pclk, pclk_bus2_83_pclk_pmu_bus2_pclk, pclk_bus2_83_aclk_ahb2apb_bus2_hclk,
	pclk_bus2_83_aclk_bus_p_2_aclk_bus2np_82,

	/* CPU */
	cpu_pll, mout_sclk_bus_pll_user, mout_cpu, dout_cpu_1,
	dout_cpu_2 = 21, dout_atclk_cpu, dout_pclk_dbg, dout_aclk_cpu, dout_pclk_cpu, dout_sclk_hpm_cpu, dout_cpu_pll,
	dout_cpu_run_monitor, atclk_cpu_aclk_stm_event_dma_i_aclk, atclk_cpu_aclk_sasync_dbg_to_imem_hclks,
	atclk_cpu_aclk_sasync_dbg_to_bus_d_mif_0_aclk = 31, atclk_cpu_sclk_cssys_dbg_traceclkin, atclk_cpu_sclk_cssys_dbg_ctmclk,
	atclk_cpu_sclk_cssys_dbg_atclk, atclk_cpu_aclk_masync_dbg_to_apl_3_atclkm, atclk_cpu_aclk_masync_dbg_to_apl_2_atclkm,
	atclk_cpu_aclk_masync_dbg_to_apl_1_atclkm, atclk_cpu_aclk_masync_dbg_to_apl_0_atclkm,
	atclk_cpu_aclk_cssys_dbg_hclk, pclk_dbg_sclk_cpu_pclkdbg, pclk_dbg_pclk_sasync_dbg_to_bus_p_isp_pclksys = 41,
	pclk_dbg_pclk_sasync_dbg_to_apl_pclksys, pclk_dbg_pclk_asyncapb_dump_pc_cpu_cclksys,
	pclk_dbg_pclk_secjtag_pclk, pclk_dbg_sclk_cssys_dbg_pclkdbg, aclk_cpu_aclk_sasync_cpu_to_bus_d_mif_0_i_clk,
	pclk_cpu_aclk_stm_event_dma_i_hclk, pclk_cpu_pclk_asyncapb_dump_pc_cpu_pclksys, pclk_cpu_pclk_dump_pc_cpu_i_clk,
	pclk_cpu_pclk_stm_event_dma_i_pclk, pclk_cpu_pclk_sfr_apbif_hpm_cpu_0_pclk = 51, pclk_cpu_pclk_sysreg_cpu_pclk,
	pclk_cpu_pclk_pmu_cpu_pclk, pclk_cpu_aclk_bus_p_0_cpu_aclk_cpunp, pclk_cpu_aclk_ahb2apb_cpu_hclk,
	sclk_hpm_cpu_0_hpm_clock,

	/* DISPAUD */
	mout_aclk_dispaud_133_user, mout_sclk_decon_int_vclk_user, mout_sclk_decon_int_eclk_user,
	mout_phyclk_mipi_phy_m_txbyteclkhs_m4s4_user, mout_phyclk_mipi_phy_m_rxclkesc0_m4s4_user = 61,
	mout_sclk_mi2s_aud_user, mout_sclk_decon_int_eclk, mout_sclk_mi2s_aud, dout_sclk_decon_int_vclk,
	dout_sclk_decon_int_eclk, dout_sclk_mi2s_aud, dout_sclk_mixer_aud, aclk_dispaud_133_pclk_dispaud_p,
	aclk_dispaud_133_aclk_aud, aclk_dispaud_133_aclk_disp = 71, aclk_dispaud_133_aclk_dispaud_d,
	aclk_dispaud_133_aclk_ppmu, aclk_dispaud_133_secure_cfw_aclk_cfw, sclk_disp_decon_int_vclk,
	sclk_disp_decon_int_eclk, phyclk_mipi_phy_m_txbyteclkhs_m4s4_phyclk_dsi_link0_bitclkdiv8,
	phyclk_mipi_phy_m_rxclkesc0_m4s4_phyclk_dsi_link0_rxclkesc0, sclk_mi2s_aud_sclk_mi2s_i2scodclki,
	sclk_mixer_aud_sysclk, ioclk_aud_i2s_sclk_ap_in_sclk_mi2s_aud_sclk_ap_in = 81, ioclk_aud_i2s_bclk_bt_in_sclk_mi2s_aud_bclk_bt_in,
	ioclk_aud_i2s_bclk_cp_in_sclk_mi2s_aud_bclk_cp_in, ioclk_aud_i2s_bclk_fm_in_sclk_mi2s_aud_bclk_fm_in,

	/* FSYS */
	usb_pll, mout_aclk_fsys_200_user, mout_sclk_fsys_mmc0_sdclkin_user, mout_sclk_fsys_mmc1_sdclkin_user,
	mout_sclk_fsys_mmc2_sdclkin_user, mout_phyclk_usbhost20_usb20_freeclk_user, mout_phyclk_usbhost20_usb20_phyclock_user = 91,
	mout_phyclk_usbhost20_usb20_clk48mohci_user, mout_phyclk_usbotg_otg20_phyclock_user,
	aclk_fsys_200_pclk_ppmu_fsys_pclk, aclk_fsys_200_pclk_gpio_fsys_pclk, aclk_fsys_200_pclk_sysreg_fsys_pclk,
	aclk_fsys_200_pclk_pmu_fsys_pclk, aclk_fsys_200_aclk_ppmu_fsys_aclk, aclk_fsys_200_aclk_upsizer_ahb_usbhs_aclk,
	aclk_fsys_200_aclk_upsizer_dma_fsys1_aclk, aclk_fsys_200_aclk_upsizer_dma_fsys0_aclk = 101,

	aclk_fsys_200_aclk_upsizer_fsys1_to_fsys0_aclk, aclk_fsys_200_aclk_bus_d_fsys_aclk_fsysnd,
	aclk_fsys_200_aclk_ahb2apb_fsys2_hclk, aclk_fsys_200_aclk_ahb2apb_fsys1_hclk, aclk_fsys_200_aclk_ahb2axi_usbhs_hclk,
	aclk_fsys_200_aclk_ahb_usbhs_hclk, aclk_fsys_200_aclk_ahb_fsys0_hclk, aclk_fsys_200_aclk_bus_p_fsys_aclk_fsysnp,
	aclk_fsys_200_aclk_xiu_d_fsys1_aclk, aclk_fsys_200_aclk_xiu_d_fsys0_aclk = 111, aclk_fsys_200_aclk_upsizer_usbotg_aclk,
	aclk_fsys_200_aclk_ahb2axi_usbotg_hclk, aclk_fsys_200_aclk_usbotg20_aclk, aclk_fsys_200_aclk_mmc2_i_aclk,
	aclk_fsys_200_aclk_mmc1_i_aclk, aclk_fsys_200_aclk_mmc0_i_aclk, aclk_fsys_200_aclk_sromc_hclk,
	aclk_fsys_200_aclk_usbhost20_aclk, aclk_fsys_200_aclk_dma_fsys1_aclk, aclk_fsys_200_aclk_dma_fsys0_aclk = 121,
	sclk_fsys_mmc0_sdclkin_sclk_mmc0_sdclkin, sclk_fsys_mmc1_sdclkin_sclk_mmc1_sdclkin, sclk_fsys_mmc2_sdclkin_sclk_mmc2_sdclkin,
	sclk_usb20_phy_clkcore, phyclk_usbhost20_usb20_freeclk, phyclk_usbhost20_usb20_phyclock,
	phyclk_usbhost20_usb20_clk48mohci, phyclk_usbhotg_otg20_phyclock_phyclk_usbotg_otg20_phyclock,

	/* G3D */
	g3d_pll, mout_aclk_g3d_400_user = 131, mout_aclk_g3d_667, dout_aclk_g3d_667, dout_pclk_g3d_167, aclk_g3d_667_aclk_ppmu_g3d_aclk,
	aclk_g3d_667_aclk_qe_g3d_aclk, aclk_g3d_667_aclk_async_ahb2apb_g3d_to_g3d_cclk, aclk_g3d_667_aclk_bus_d_g3d_aclk_g3dnd,
	aclk_g3d_667_aclk_g3d_clk, aclk_g3d_667_secure_cfw_aclk_smmu_g3d_aclk_cfw, pclk_g3d_167_pclk_ppmu_g3d_pclk = 141,
	pclk_g3d_167_pclk_qe_g3d_pclk, pclk_g3d_167_pclk_sysreg_g3d_pclk, pclk_g3d_167_pclk_pmu_g3d_pclk,
	pclk_g3d_167_pclk_async_ahb2apb_g3d_to_g3d_pclk, pclk_g3d_167_aclk_bus_p_0_to_ahb2apb_g3d_aclk_g3dnp,
	pclk_g3d_167_aclk_ahb2apb_g3d_hclk, pclk_g3d_167_secure_cfw_pclk_smmu_g3d_pclk_cfw,

	/* IMEM */
	mout_aclk_imem_200_user,
	mout_aclk_imem_266_user, aclk_imem_200_aclk_masync_xiu_d_imem_to_xiu_p_imem_aclk = 151, aclk_imem_200_pclk_ppmu_imem,
	aclk_imem_200_pclk_sysreg_imem_pclk, aclk_imem_200_pclk_pmu_imem_pclk, aclk_imem_200_aclk_downsizer_imem0p_aclk,
	aclk_imem_200_aclk_downsizer_gic_aclk, aclk_imem_200_aclk_xiu_p_imem_aclk, aclk_imem_200_aclk_axi2apb_imem0p_aclk,
	aclk_imem_200_aclk_bus_p_imem_aclk, aclk_imem_200_aclk_intc_cpu_clk, aclk_imem_200_secure_iramc_top_aclk_iramc_top_aclk = 161,
	aclk_imem_200_secure_sss_pclk_sss_pclk, aclk_imem_200_secure_rtic_pclk_rtic_pclk, aclk_imem_266_aclk_downsizer_async_aclk,
	aclk_imem_266_aclk_sasync_xiu_d_imem_to_xiu_p_imem_aclk, aclk_imem_266_aclk_ppmu_imem,
	aclk_imem_266_aclk_bus_d_imem_aclk, aclk_imem_266_aclk_xiu_d_imem_aclk, aclk_imem_266_secure_sss_aclk_sss_aclk,
	aclk_imem_266_secure_rtic_aclk_rtic_aclk,

	/* ISP */
	mout_aclk_isp_300_user = 171, mout_phyclk_csi_link_rx_byte_clk_hs_user,
	dout_pclk_isp_150, aclk_isp_300_aclk_isp_d, aclk_isp_300_aclk_ppmu, aclk_isp_300_aclk_fd,
	aclk_isp_300_aclk_is, pclk_isp_150_pclk_isp_p, phyclk_csi_link0_rx_byte_clk_hs0,

	/* MFCMSCL */
	mout_aclk_mfcmscl_333_user,
	mout_aclk_mfcmscl_200_user = 181, dout_pclk_mfcmscl_83, aclk_mfcmscl_333_aclk_ppmu, aclk_mfcmscl_333_aclk_mfcmscl_d,
	aclk_mfcmscl_333_aclk_mscl_bi, aclk_mfcmscl_333_aclk_mscl_poly, aclk_mfcmscl_333_aclk_jpeg,
	aclk_mfcmscl_333_aclk_mscl_d, aclk_mfcmscl_333_secure_cfw_aclk_cfw, pclk_mfcmscl_83_pclk_mfcmscl_p,
	aclk_mfcmscl_200_aclk_mfc = 191,

	/* MIF */
	mem0_pll, media_pll, bus_pll, aud_pll, mout_div2_mem0_pll, mout_div2_media_pll,
	mout_clkm_phy_a, mout_clkm_phy_b, mout_clk2x_phy_a, mout_clk2x_phy_b = 201,

	mout_aclk_mif_333, mout_aclk_mif_83,
	mout_aclk_mif_fix_50, mout_aclk_dispaud_133, mout_aclk_isp_300, mout_sclk_disp_decon_int_eclk,
	mout_sclk_disp_decon_int_vclk, dout_clkm_phy, dout_clk2x_phy, dout_aclk_mif_333 = 211, dout_aclk_mif_166,
	dout_aclk_mif_83, dout_aclk_mif_fix_50, dout_aclk_dispaud_133, dout_aclk_isp_300, dout_sclk_disp_decon_int_eclk,
	dout_sclk_disp_decon_int_vclk, dout_mif_noddr, sclk_ddr_phy0_clkm, sclk_ddr_phy0_clk2x = 221, aclk_drex_aclk_bus_d_mif_nrt_aclk_mif_nrtnd,
	aclk_drex_aclk_masync_cp_cleany_0_aclk, aclk_drex_aclk_cleany_0_aclk, aclk_drex_aclk_ppmu_bus_d_mif_rt_to_drex_0_aclk,
	aclk_drex_aclk_ppmu_xiu_mif_d_0_to_drex_0_aclk, aclk_drex_aclk_ppmu_ba_remap_0_to_drex_0_aclk,
	aclk_drex_aclk_ppmu_bus_d_mif_nrt_to_drex_0_aclk, aclk_drex_aclk_masync_bus_d_mif_rt_to_drex_0_aclkm,
	aclk_drex_aclk_masync_xiu_mif_d_0_to_drex_0_aclkm, aclk_drex_sclk_drex_0_tz_clk = 231, aclk_drex_sclk_drex_0_perev_clk,
	aclk_drex_sclk_drex_0_memif_wr_clk, aclk_drex_sclk_drex_0_memif_rd_clk, aclk_drex_sclk_drex_0_memif_clk,
	aclk_drex_sclk_drex_0_sch_wr_clk, aclk_drex_sclk_drex_0_sch_rd_clk, aclk_drex_sclk_drex_0_sch_clk,
	aclk_drex_sclk_drex_0_busif_wr_clk, aclk_drex_sclk_drex_0_busif_rd_clk, aclk_drex_0_aclk = 241,
	aclk_drex_rclk_sclk_drex_0_rclk, aclk_mif_333_aclk_qe, aclk_mif_333_aclk_ppmu_cpu_to_xiu_mif_d_0_aclk,
	aclk_mif_333_aclk_bus_p_mif_cdn_aclk, aclk_mif_333_aclk_sasync_bus_d_mif_rt_to_drex_0_aclks,
	aclk_mif_333_aclk_upsizer_dbg_to_xiu_mif_d_0_aclk, aclk_mif_333_aclk_sasync_xiu_mif_d_0_to_drex_0_aclks,
	aclk_mif_333_aclk_masync_dbg_to_xiu_mif_d_0_aclk, aclk_mif_333_aclk_masync_cpu_to_xiu_mif_d_0_aclk,
	aclk_mif_333_aclk_bus_d_mif_rt_aclk_mif_rtnd = 251, aclk_mif_333_aclk_xiu_mif_d_0_aclk, aclk_mif_166_aclk_cleany_1_aclk,
	aclk_mif_166_aclk_accwindow_1_aclk, aclk_mif_166_aclk_masync_cp_cleany_1_aclk, aclk_mif_166_aclk_bus_p_mif_aclk_mifnp,
	aclk_mif_83_pclk_qe, aclk_mif_83_aclk_asyncapb_cssys_hclk, aclk_mif_83_pclk_bus_p_mif_srv,
	aclk_mif_83_pclk_bus_d_mif_rt_srv, aclk_mif_83_pclk_bus_d_mif_nrt_srv = 261, aclk_mif_83_pclk_ppmu_cpu_to_xiu_mif_d_0_pclk,
	aclk_mif_83_pclk_ppmu_bus_d_mif_rt_to_drex_0_pclk, aclk_mif_83_pclk_ppmu_bus_d_mif_nrt_to_drex_0_pclk,
	aclk_mif_83_pclk_ppmu_ba_remap_0_to_drex_0_pclk, aclk_mif_83_pclk_masync_cp_cleany_0_aclk,
	aclk_mif_83_pclk_ppmu_xiu_mif_d_0_to_drex_0_pclk, aclk_mif_83_pclk_gpio_mif_pclk, aclk_mif_83_pclk_sasync_hsi2c_cp_pclk,
	aclk_mif_83_pclk_sasync_hsi2c_ap_pclk, aclk_mif_83_pclk_mailbox_apbif_pclk = 271, aclk_mif_83_pclk_alv_pmu_apbif_pclk,
	aclk_mif_83_pclk_ddr_phy0_apbif_pclk, aclk_mif_83_pclk_sysreg_mif_pclk, aclk_mif_83_pclk_pmu_mif_pclk,
	aclk_mif_83_pclk_drex_0_pclk, aclk_mif_83_aclk_ahb2apb_mif3_hclk, aclk_mif_83_aclk_ahb2apb_mif2_hclk,
	aclk_mif_83_aclk_ahb2apb_mif1_hclk, aclk_mif_83_aclk_ahb2apb_mif0_hclk, aclk_mif_83_aclk_ahb_bridge_hclk = 281,
	aclk_mif_83_secure_drex_0_tz_pclk_drex_0_pclk_tz, aclk_mif_83_secure_mailbox_apbif_pclk_s_mailbox_apbif_pclk,
	aclk_mif_fix_50_pclk_masync_hsi2c_cp_pclk, aclk_mif_fix_50_pclk_masync_hsi2c_ap_pclk,
	aclk_mif_fix_50_pclk_hsi2c_mif_pclk, sclk_aud_300, aclk_dispaud_133, aclk_isp_300, sclk_decon_int_eclk_i,
	sclk_decon_int_vclk_i = 291, aclk_cpu_800, sclk_bus_pll_top, sclk_media_pll_top, sclk_ap2cp_mif_pll_out_333_sclk_ap2cp_pll_media_clk,
	sclk_ap2cp_mif_pll_out_400_sclk_ap2cp_pll_bus_clk,

	/* PERI */
	mout_aclk_peri_66_user, mout_sclk_peri_spi0_spi_ext_clk_user,
	mout_sclk_peri_spi1_spi_ext_clk_user, mout_sclk_peri_spi2_spi_ext_clk_user, mout_sclk_peri_uart0_ext_uclk_user = 301,

	mout_sclk_peri_uart1_ext_uclk_user, mout_sclk_peri_uart2_ext_uclk_user, aclk_peri_66_1_pclk_hsi2c8_ipclk,
	aclk_peri_66_1_pclk_hsi2c7_ipclk, aclk_peri_66_1_pclk_hsi2c1_ipclk, aclk_peri_66_1_pclk_hsi2c0_ipclk,
	aclk_peri_66_1_pclk_i2c6_pclk, aclk_peri_66_1_pclk_i2c5_pclk, aclk_peri_66_1_pclk_i2c4_pclk,
	aclk_peri_66_1_pclk_i2c3_pclk = 311, aclk_peri_66_1_aclk_ahb2apb_peric1_hclk, aclk_peri_66_1_aclk_ahb2apb_peric0_hclk,
	aclk_peri_66_1_pclk_custom_efuse_apbif_pclk, aclk_peri_66_1_pclk_efuse_writer_sc_apbif_pclk,
	aclk_peri_66_1_pclk_cmu_top_apbif_pclk, aclk_peri_66_1_pclk_wdt_cpu_pclk, aclk_peri_66_1_pclk_mct_pclk,
	aclk_peri_66_1_aclk_ahb2apb_peris1_hclk, aclk_peri_66_1_aclk_ahb2apb_peris0_hclk, aclk_peri_66_1_aclk_bus_p_peri_aclk_perinp = 321,
	aclk_peri_66_1_aclk_ahb_peri_hclk, aclk_peri_66_secure_tzpc_pclk_tzpc10_pclk, aclk_peri_66_secure_tzpc_pclk_tzpc9_pclk,
	aclk_peri_66_secure_tzpc_pclk_tzpc8_pclk, aclk_peri_66_secure_tzpc_pclk_tzpc7_pclk, aclk_peri_66_secure_tzpc_pclk_tzpc6_pclk,
	aclk_peri_66_secure_tzpc_pclk_tzpc5_pclk, aclk_peri_66_secure_tzpc_pclk_tzpc4_pclk, aclk_peri_66_secure_tzpc_pclk_tzpc3_pclk,
	aclk_peri_66_secure_tzpc_pclk_tzpc2_pclk = 331, aclk_peri_66_secure_tzpc_pclk_tzpc1_pclk, aclk_peri_66_secure_tzpc_pclk_tzpc0_pclk,
	aclk_peri_66_secure_rtc_top_pclk_rtc_top_pclk, aclk_peri_66_secure_chipid_pclk_chipid_apbif_pclk,
	aclk_peri_66_secure_seckey_pclk_seckey_apbif_pclk, aclk_peri_66_secure_antirbk_cnt_pclk_antirbk_cnt_apbif_pclk,
	aclk_peri_66_secure_monotonic_cnt_pclk_monotonic_cnt_apbif_pclk, aclk_peri_66_secure_rtc_pclk_rtc_apbif_pclk,
	aclk_peri_66_2_pclk_sysreg_peri_pclk, aclk_peri_66_2_pclk_pmu_peri_pclk = 341, aclk_peri_66_2_pclk_gpio_alive_pclk,
	aclk_peri_66_2_pclk_gpio_ese_pclk, aclk_peri_66_2_pclk_gpio_touch_pclk, aclk_peri_66_2_pclk_gpio_nfc_pclk,
	aclk_peri_66_2_pclk_gpio_peri_pclk, aclk_peri_66_2_pclk_tmu0_apbif_pclk, aclk_peri_66_2_pclk_pwm_pclk,
	aclk_peri_66_2_pclk_i2c10_pclk, aclk_peri_66_2_pclk_i2c9_pclk, aclk_peri_66_2_pclk_spi2_pclk = 351,
	aclk_peri_66_2_pclk_spi1_pclk, aclk_peri_66_2_pclk_spi0_pclk, aclk_peri_66_2_pclk_adcif_pclk,
	aclk_peri_66_2_pclk_uart2_pclk, aclk_peri_66_2_pclk_uart1_pclk, aclk_peri_66_2_pclk_uart0_pclk,
	sclk_peri_spi0_spi_ext_clk_sclk_spi0_spi_ext_clk, sclk_peri_spi1_spi_ext_clk_sclk_spi1_spi_ext_clk,
	sclk_peri_spi2_spi_ext_clk_sclk_spi2_spi_ext_clk, sclk_peri_uart0_ext_uclk_sclk_uart0_ext_uclk = 361,
	sclk_peri_uart1_ext_uclk_sclk_uart1_ext_uclk, sclk_peri_uart2_ext_uclk_sclk_uart2_ext_uclk,
	ioclk_peri_oscclk_sclk_pwm_tclk0, ioclk_peri_oscclk_sclk_adcif_i_osc_sys, ioclk_peri_oscclk_efuse_sclk_custom_efuse_clk,
	ioclk_peri_oscclk_efuse_sclk_efuse_writer_clk, ioclk_peri_oscclk_efuse_sclk_tmu0_clk,
	ioclk_peri_oscclk_chipid_secure_sclk_chipid_clk, ioclk_peri_oscclk_seckey_secure_sclk_seckey_clk,
	ioclk_peri_oscclk_antirbk_cnt_secure_sclk_antirbk_cnt_clk = 371,

	/* TOP */
	mout_bus_pll_user, mout_media_pll_user,
	mout_aclk_bus0_333, mout_aclk_bus2_333, mout_aclk_mfcmscl_200, mout_aclk_mfcmscl_333, mout_sclk_fsys_mmc0_sdclkin,
	mout_sclk_fsys_mmc1_sdclkin, mout_sclk_fsys_mmc2_sdclkin, mout_sclk_peri_spi0_spi_ext_clk = 381,
	mout_sclk_peri_spi1_spi_ext_clk, mout_sclk_peri_spi2_spi_ext_clk, mout_sclk_peri_uart0_ext_uclk,
	mout_sclk_peri_uart1_ext_uclk, mout_sclk_peri_uart2_ext_uclk, mout_sclk_isp_sensor0, mout_sclk_isp_sensor1,
	dout_aclk_fsys_200, dout_aclk_imem_266, dout_aclk_imem_200 = 391, dout_aclk_bus0_333, dout_aclk_bus2_333,
	dout_aclk_peri_66, dout_aclk_g3d_400, dout_aclk_mfcmscl_200, dout_aclk_mfcmscl_333, dout_sclk_fsys_mmc0_sdclkin_a,
	dout_sclk_fsys_mmc0_sdclkin_b, dout_sclk_fsys_mmc1_sdclkin_a, dout_sclk_fsys_mmc1_sdclkin_b = 401,

	dout_sclk_fsys_mmc2_sdclkin_a, dout_sclk_fsys_mmc2_sdclkin_b, dout_sclk_peri_spi0_a, dout_sclk_peri_spi0_b,
	dout_sclk_peri_spi1_a, dout_sclk_peri_spi1_b, dout_sclk_peri_spi2_a, dout_sclk_peri_spi2_b,
	dout_sclk_peri_uart0, dout_sclk_peri_uart1 = 411, dout_sclk_peri_uart2, dout_sclk_isp_sensor0_a,
	dout_sclk_isp_sensor0_b, dout_sclk_isp_sensor1_a, dout_sclk_isp_sensor1_b, aclk_fsys_200,
	aclk_imem_266, aclk_imem_200, aclk_bus0_333, aclk_bus2_333 = 421, aclk_peri_66, aclk_g3d_400, aclk_mfcmscl_200,
	aclk_mfcmscl_333, sclk_fsys_mmc0_sdclkin, sclk_fsys_mmc1_sdclkin, sclk_fsys_mmc2_sdclkin,
	sclk_peri_spi0_spi_ext_clk, sclk_peri_spi1_spi_ext_clk, sclk_peri_spi2_spi_ext_clk = 431, sclk_peri_uart0_ext_uclk,
	sclk_peri_uart1_ext_uclk, sclk_peri_uart2_ext_uclk, sclk_isp_sensor0, sclk_isp_sensor1,

	/* oscclk */
	oscclk = 437,

	/* FSYS */
	phyclk_usbhost20_phy_usb20_freeclk, phyclk_usbhost20_phy_usb20_phyclock,
	phyclk_usbhost20_phy_usb20_clk48mohci, phyclk_usbotg_phy_otg20_phyclock,
	/* DISPAUD */
	phyclk_mipi_phy_m_txbyteclkhs_m4s4, phyclk_mipi_phy_m_rxclkesc0_m4s4,
	ioclk_aud_i2s_sclk_ap_in, ioclk_aud_i2s_bclk_bt_in, ioclk_aud_i2s_bclk_cp_in, ioclk_aud_i2s_bclk_fm_in,
	/* ISP */
	phyclk_csi_phy0_rxbyteclkhs,
	/* PERI */
	ioclk_peri_spi0_spiclkin, ioclk_peri_spi1_spiclkin, ioclk_peri_spi2_spiclkin,
	/* MIF */
	ffac_mem0_pll_div2, ffac_media_pll_div2, ffac_media_pll_div4, ffac_bus_pll_div2,

	nr_clks,
};

PNAME(mout_cpu_p) = {"cpu_pll", "mout_sclk_bus_pll_user"};
PNAME(mout_sclk_decon_int_eclk_p) = {"mout_sclk_decon_int_vclk_user", "mout_sclk_decon_int_eclk_user"};
PNAME(mout_sclk_mi2s_aud_p) = {"mout_sclk_mi2s_aud_user", "ioclk_audi2s0cdclk"};
PNAME(mout_aclk_g3d_667_p) = {"g3d_pll", "mout_aclk_g3d_400_user"};
PNAME(mout_div2_mem0_pll_p) = {"mem0_pll", "ffac_mem0_pll_div2"};
PNAME(mout_div2_media_pll_p) = {"media_pll", "ffac_media_pll_div2"};
PNAME(mout_clkm_phy_a_p) = {"mout_div2_media_pll", "mout_div2_mem0_pll"};
PNAME(mout_clkm_phy_b_p) = {"mout_clkm_phy_a", "bus_pll"};
PNAME(mout_clk2x_phy_a_p) = {"mout_div2_media_pll", "mout_div2_mem0_pll"};
PNAME(mout_clk2x_phy_b_p) = {"mout_clk2x_phy_a", "bus_pll"};
PNAME(mout_aclk_mif_333_p) = {"dout_mif_noddr", "bus_pll", "mout_div2_mem0_pll"};
PNAME(mout_aclk_mif_83_p) = {"dout_mif_noddr", "bus_pll", "mout_div2_mem0_pll"};
PNAME(mout_aclk_mif_fix_50_p) = {"bus_pll", "dout_mif_noddr"};
PNAME(mout_aclk_dispaud_133_p) = {"bus_pll", "dout_mif_noddr", "aud_pll"};
PNAME(mout_aclk_isp_300_p) = {"aud_pll", "bus_pll", "dout_mif_noddr"};
PNAME(mout_sclk_disp_decon_int_eclk_p) = {"bus_pll", "dout_mif_noddr", "aud_pll"};
PNAME(mout_sclk_disp_decon_int_vclk_p) = {"bus_pll", "dout_mif_noddr", "aud_pll"};
PNAME(mout_aclk_bus0_333_p) = {"mout_media_pll_user", "mout_bus_pll_user"};
PNAME(mout_aclk_bus2_333_p) = {"mout_media_pll_user", "mout_bus_pll_user"};
PNAME(mout_aclk_mfcmscl_200_p) = {"mout_media_pll_user", "mout_bus_pll_user"};
PNAME(mout_aclk_mfcmscl_333_p) = {"mout_media_pll_user", "mout_bus_pll_user"};
PNAME(mout_sclk_fsys_mmc0_sdclkin_p) = {"mout_bus_pll_user", "mout_media_pll_user"};
PNAME(mout_sclk_fsys_mmc1_sdclkin_p) = {"mout_bus_pll_user", "mout_media_pll_user"};
PNAME(mout_sclk_fsys_mmc2_sdclkin_p) = {"mout_bus_pll_user", "mout_media_pll_user"};
PNAME(mout_sclk_peri_spi0_spi_ext_clk_p) = {"mout_bus_pll_user", "mout_media_pll_user"};
PNAME(mout_sclk_peri_spi1_spi_ext_clk_p) = {"mout_bus_pll_user", "mout_media_pll_user"};
PNAME(mout_sclk_peri_spi2_spi_ext_clk_p) = {"mout_bus_pll_user", "mout_media_pll_user"};
PNAME(mout_sclk_peri_uart0_ext_uclk_p) = {"mout_bus_pll_user", "mout_media_pll_user"};
PNAME(mout_sclk_peri_uart1_ext_uclk_p) = {"mout_bus_pll_user", "mout_media_pll_user"};
PNAME(mout_sclk_peri_uart2_ext_uclk_p) = {"mout_bus_pll_user", "mout_media_pll_user"};
PNAME(mout_sclk_isp_sensor0_p) = {"fin_pll", "mout_bus_pll_user"};
PNAME(mout_sclk_isp_sensor1_p) = {"fin_pll", "mout_bus_pll_user"};

struct samsung_pll_rate_table table_cpu[] = {
	/* rate            p,      m,      s,      k */
	{ 2000000000U,    13,   1000,      0,      0 }, /* L0 */
	{ 1898000000U,     4,    292,      0,      0 }, /* L1 */
	{ 1794000000U,     4,    276,      0,      0 }, /* L2 */
	{ 1703000000U,     4,    262,      0,      0 }, /* L3 */
	{ 1599000000U,     4,    246,      0,      0 }, /* L4 */
	{ 1495000000U,     4,    230,      0,      0 }, /* L5 */
	{ 1404000000U,     4,    216,      0,      0 }, /* L6 */
	{ 1300000000U,     4,    200,      0,      0 }, /* L7 */
	{ 1196000000U,     4,    368,      1,      0 }, /* L8 */
	{ 1105000000U,     4,    340,      1,      0 }, /* L9 */
	{ 1001000000U,     4,    308,      1,      0 }, /* L10 */
	{  897000000U,     4,    276,      1,      0 }, /* L11 */
	{  806000000U,     4,    248,      1,      0 }, /* L12 */
	{  702000000U,     4,    216,      1,      0 }, /* L13 */
	{  598000000U,     4,    368,      2,      0 }, /* L14 */
	{  507000000U,     4,    312,      2,      0 }, /* L15 */
	{  403000000U,     4,    248,      2,      0 }, /* L16 */
	{  299000000U,     4,    368,      3,      0 }, /* L17 */
	{  195000000U,     4,    240,      3,      0 }, /* L18 */
};

struct samsung_pll_rate_table table_mem0[] = {
	/* rate            p,      m,      s,      k */
	{ 1650000000U,    13,    825,      0,      0 }, /* L0 */
	{ 1332000000U,    13,    666,      0,      0 }, /* L1 */
	{ 1118000000U,     4,    344,      1,      0 }, /* L2 */
	{  676000000U,     4,    208,      1,      0 }, /* L3 */
	{  546000000U,     4,    336,      2,      0 }, /* L4 */
	{  400000000U,    13,    800,      2,      0 }, /* L5 */
};

struct samsung_pll_rate_table table_media[] = {
	/* rate            p,      m,      s,      k */
	{ 1334000000U,    13,    667,      0,      0 },
	{ 1332000000U,    13,    666,      0,      0 },
};

struct samsung_pll_rate_table table_bus[] = {
	/* rate            p,      m,      s,      k */
	{ 825000000U,     26,    825,      0,      0 },
	{ 800000000U,     13,    400,      0,      0 },
};

struct samsung_pll_rate_table table_g3d[] = {
	/* rate            p,      m,      s,     k */
	{ 667000000U,     12,    308,      0,     0 }, /* L0 */
	{ 600000000U,     13,    300,      0,     0 }, /* L1 */
	{ 533000000U,     14,    287,      0,     0 }, /* L2 */
	{ 480000000U,     13,    480,      1,     0 }, /* L3 */
	{ 440000000U,     13,    440,      1,     0 }, /* L4 */
	{ 420000000U,     13,    420,      1,     0 }, /* L5 */
	{ 370000000U,     13,    370,      1,     0 }, /* L6 */
	{ 350000000U,     13,    350,      1,     0 }, /* L7 */
	{ 266000000U,     13,    266,      1,     0 }, /* L8 */
	{ 160000000U,     13,    320,      2,     0 }, /* L9 */
	{ 100000000U,     13,    400,      3,     0 }, /* L10 */
};

struct samsung_pll_rate_table table_usb[] = {
	/* rate            p,      m,      s,     k */
	{  50000000U,	  13,    400,      4,     0 },
	{  24000000U,	  13,    384,      5,     0 },
};

struct samsung_pll_rate_table table_aud[] = {
	/* rate            p,      m,      s,     k */
	{ 393216000U,	   3,    181,      2, 31740 },
	{ 294912000U,	   3,    136,      2,  7421 },
};

static struct samsung_composite_pll exynos3475_pll_clks[] __refdata = {
	/* CPU */
	PLL(cpu_pll, "cpu_pll", pll_2555x, EXYNOS3475_CPU_PLL_LOCK, EXYNOS3475_CPU_PLL_CON0, EXYNOS3475_CPU_PLL_CON0, 31, EXYNOS3475_CLK_CON_MUX_CPU_PLL, 12, EXYNOS3475_CLK_CON_MUX_CPU_PLL, 26, table_cpu, 0, CLK_IGNORE_UNUSED, "cpu_pll"),
	/* FSYS */
	PLL(none, "usb_pll", pll_2551x, EXYNOS3475_USB_PLL_LOCK, EXYNOS3475_USB_PLL_CON0, EXYNOS3475_USB_PLL_CON0, 31, EXYNOS3475_CLK_CON_MUX_USB_PLL, 12, EXYNOS3475_CLK_CON_MUX_USB_PLL, 26, table_usb, 0, CLK_IGNORE_UNUSED, NULL),
	/* G3D */
	PLL(g3d_pll, "g3d_pll", pll_2551x, EXYNOS3475_G3D_PLL_LOCK, EXYNOS3475_G3D_PLL_CON0, EXYNOS3475_G3D_PLL_CON0, 31, EXYNOS3475_CLK_CON_MUX_G3D_PLL, 12, EXYNOS3475_CLK_CON_MUX_G3D_PLL, 26, table_g3d, 0, CLK_IGNORE_UNUSED, NULL),
	/* MIF */
	PLL(mem0_pll, "mem0_pll", pll_2555x, EXYNOS3475_MEM0_PLL_LOCK, EXYNOS3475_MEM0_PLL_CON0, EXYNOS3475_MEM0_PLL_CON0, 31, EXYNOS3475_CLK_CON_MUX_MEM0_PLL, 12, EXYNOS3475_CLK_CON_MUX_MEM0_PLL, 26, table_mem0, 0, CLK_IGNORE_UNUSED, "mem0_pll"),
	PLL(media_pll, "media_pll", pll_2555x, EXYNOS3475_MEDIA_PLL_LOCK, EXYNOS3475_MEDIA_PLL_CON0, EXYNOS3475_MEDIA_PLL_CON0, 31, EXYNOS3475_CLK_CON_MUX_MEDIA_PLL, 12, EXYNOS3475_CLK_CON_MUX_MEDIA_PLL, 26, table_media, 0, CLK_IGNORE_UNUSED, "media_pll"),
	PLL(bus_pll, "bus_pll", pll_2551x, EXYNOS3475_BUS_PLL_LOCK, EXYNOS3475_BUS_PLL_CON0, EXYNOS3475_BUS_PLL_CON0, 31, EXYNOS3475_CLK_CON_MUX_BUS_PLL, 12, EXYNOS3475_CLK_CON_MUX_BUS_PLL, 26, table_bus, 0, CLK_IGNORE_UNUSED, "bus_pll"),
	PLL(aud_pll, "aud_pll", pll_2650x, EXYNOS3475_AUD_PLL_LOCK, EXYNOS3475_AUD_PLL_CON0, EXYNOS3475_AUD_PLL_CON0, 31, EXYNOS3475_CLK_CON_MUX_AUD_PLL, 12, EXYNOS3475_CLK_CON_MUX_AUD_PLL, 26, table_aud, 0, CLK_IGNORE_UNUSED, "aud_pll"),
};

static struct samsung_composite_mux exynos3475_mux_clks[] __refdata = {
	/* CPU */
	MUX(mout_cpu, "mout_cpu", mout_cpu_p, EXYNOS3475_CLK_CON_MUX_CPU, 12, 1, EXYNOS3475_CLK_CON_MUX_CPU, 26, 1, 0, "mout_cpu"),
	/* DISPAUD */
	MUX(none, "mout_sclk_decon_int_eclk", mout_sclk_decon_int_eclk_p, EXYNOS3475_CLK_CON_MUX_SCLK_DECON_INT_ECLK, 12, 1, EXYNOS3475_CLK_CON_MUX_SCLK_DECON_INT_ECLK, 26, 1, 0, NULL),
	MUX(mout_sclk_mi2s_aud, "mout_sclk_mi2s_aud", mout_sclk_mi2s_aud_p, EXYNOS3475_CLK_CON_MUX_SCLK_MI2S_AUD, 12, 1, NULL, 0, 0, 0, NULL),
	/* G3D */
	MUX(mout_aclk_g3d_667, "mout_aclk_g3d_667", mout_aclk_g3d_667_p, EXYNOS3475_CLK_CON_MUX_ACLK_G3D_667, 12, 1, EXYNOS3475_CLK_CON_MUX_ACLK_G3D_667, 26, 1, 0, NULL),
	/* MIF */
	MUX(none, "mout_div2_mem0_pll", mout_div2_mem0_pll_p, EXYNOS3475_CLK_CON_MUX_DIV2_MEM0_PLL, 12, 1, EXYNOS3475_CLK_CON_MUX_DIV2_MEM0_PLL, 26, 1, 0, NULL),
	MUX(none, "mout_div2_media_pll", mout_div2_media_pll_p, EXYNOS3475_CLK_CON_MUX_DIV2_MEDIA_PLL, 12, 1, EXYNOS3475_CLK_CON_MUX_DIV2_MEDIA_PLL, 26, 1, 0, NULL),
	MUX(none, "mout_clkm_phy_a", mout_clkm_phy_a_p, EXYNOS3475_CLK_CON_MUX_CLKM_PHY_A, 12, 1, EXYNOS3475_CLK_CON_MUX_CLKM_PHY_A, 26, 1, 0, NULL),
	MUX(none, "mout_clkm_phy_b", mout_clkm_phy_b_p, EXYNOS3475_CLK_CON_MUX_CLKM_PHY_B, 12, 1, EXYNOS3475_CLK_CON_MUX_CLKM_PHY_B, 26, 1, 0, NULL),
	MUX(none, "mout_clk2x_phy_a", mout_clk2x_phy_a_p, EXYNOS3475_CLK_CON_MUX_CLK2X_PHY_A, 12, 1, EXYNOS3475_CLK_CON_MUX_CLK2X_PHY_A, 26, 1, 0, NULL),
	MUX(none, "mout_clk2x_phy_b", mout_clk2x_phy_b_p, EXYNOS3475_CLK_CON_MUX_CLK2X_PHY_B, 12, 1, EXYNOS3475_CLK_CON_MUX_CLK2X_PHY_B, 26, 1, 0, NULL),
	MUX(mout_aclk_mif_333, "mout_aclk_mif_333", mout_aclk_mif_333_p, EXYNOS3475_CLK_CON_MUX_ACLK_MIF_333, 12, 2, EXYNOS3475_CLK_CON_MUX_ACLK_MIF_333, 26, 1, 0, "mout_aclk_mif_333"),
	MUX(mout_aclk_mif_83, "mout_aclk_mif_83", mout_aclk_mif_83_p, EXYNOS3475_CLK_CON_MUX_ACLK_MIF_83, 12, 2, EXYNOS3475_CLK_CON_MUX_ACLK_MIF_83, 26, 1, 0, "mout_aclk_mif_83"),
	MUX(mout_aclk_mif_fix_50, "mout_aclk_mif_fix_50", mout_aclk_mif_fix_50_p, EXYNOS3475_CLK_CON_MUX_ACLK_MIF_FIX_50, 12, 1, EXYNOS3475_CLK_CON_MUX_ACLK_MIF_FIX_50, 26, 1, 0, "mout_aclk_mif_fix_50"),
	MUX(mout_aclk_dispaud_133, "mout_aclk_dispaud_133", mout_aclk_dispaud_133_p, EXYNOS3475_CLK_CON_MUX_ACLK_DISPAUD_133, 12, 2, EXYNOS3475_CLK_CON_MUX_ACLK_DISPAUD_133, 26, 1, 0, "mout_aclk_dispaud_133"),
	MUX(mout_aclk_isp_300, "mout_aclk_isp_300", mout_aclk_isp_300_p, EXYNOS3475_CLK_CON_MUX_ACLK_ISP_300, 12, 2, EXYNOS3475_CLK_CON_MUX_ACLK_ISP_300, 26, 1, 0, "mout_aclk_isp_300"),
	MUX(mout_sclk_disp_decon_int_eclk, "mout_sclk_disp_decon_int_eclk", mout_sclk_disp_decon_int_eclk_p, EXYNOS3475_CLK_CON_MUX_SCLK_DISP_DECON_INT_ECLK, 12, 2, EXYNOS3475_CLK_CON_MUX_SCLK_DISP_DECON_INT_ECLK, 26, 1, 0, NULL),
	MUX(mout_sclk_disp_decon_int_vclk, "mout_sclk_disp_decon_int_vclk", mout_sclk_disp_decon_int_vclk_p, EXYNOS3475_CLK_CON_MUX_SCLK_DISP_DECON_INT_VCLK, 12, 2, EXYNOS3475_CLK_CON_MUX_SCLK_DISP_DECON_INT_VCLK, 26, 1, 0, NULL),
	/* TOP */
	MUX(mout_aclk_bus0_333, "mout_aclk_bus0_333", mout_aclk_bus0_333_p, EXYNOS3475_CLK_CON_MUX_ACLK_BUS0_333, 12, 1, EXYNOS3475_CLK_CON_MUX_ACLK_BUS0_333, 26, 1, 0, "mout_aclk_bus0_333"),
	MUX(mout_aclk_bus2_333, "mout_aclk_bus2_333", mout_aclk_bus2_333_p, EXYNOS3475_CLK_CON_MUX_ACLK_BUS2_333, 12, 1, EXYNOS3475_CLK_CON_MUX_ACLK_BUS2_333, 26, 1, 0, "mout_aclk_bus2_333"),
	MUX(mout_aclk_mfcmscl_200, "mout_aclk_mfcmscl_200", mout_aclk_mfcmscl_200_p, EXYNOS3475_CLK_CON_MUX_ACLK_MFCMSCL_200, 12, 1, EXYNOS3475_CLK_CON_MUX_ACLK_MFCMSCL_200, 26, 1, 0, "mout_aclk_mfcmscl_200"),
	MUX(mout_aclk_mfcmscl_333, "mout_aclk_mfcmscl_333", mout_aclk_mfcmscl_333_p, EXYNOS3475_CLK_CON_MUX_ACLK_MFCMSCL_333, 12, 1, EXYNOS3475_CLK_CON_MUX_ACLK_MFCMSCL_333, 26, 1, 0, "mout_aclk_mfcmscl_333"),
	MUX(mout_sclk_fsys_mmc0_sdclkin, "mout_sclk_fsys_mmc0_sdclkin", mout_sclk_fsys_mmc0_sdclkin_p, EXYNOS3475_CLK_CON_MUX_SCLK_FSYS_MMC0_SDCLKIN, 12, 1, EXYNOS3475_CLK_CON_MUX_SCLK_FSYS_MMC0_SDCLKIN, 26, 1, 0, NULL),
	MUX(mout_sclk_fsys_mmc1_sdclkin, "mout_sclk_fsys_mmc1_sdclkin", mout_sclk_fsys_mmc1_sdclkin_p, EXYNOS3475_CLK_CON_MUX_SCLK_FSYS_MMC1_SDCLKIN, 12, 1, EXYNOS3475_CLK_CON_MUX_SCLK_FSYS_MMC1_SDCLKIN, 26, 1, 0, NULL),
	MUX(mout_sclk_fsys_mmc2_sdclkin, "mout_sclk_fsys_mmc2_sdclkin", mout_sclk_fsys_mmc2_sdclkin_p, EXYNOS3475_CLK_CON_MUX_SCLK_FSYS_MMC2_SDCLKIN, 12, 1, EXYNOS3475_CLK_CON_MUX_SCLK_FSYS_MMC2_SDCLKIN, 26, 1, 0, NULL),
	MUX(none, "mout_sclk_peri_spi0_spi_ext_clk", mout_sclk_peri_spi0_spi_ext_clk_p, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_SPI0_SPI_EXT_CLK, 12, 1, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_SPI0_SPI_EXT_CLK, 26, 1, 0, NULL),
	MUX(none, "mout_sclk_peri_spi1_spi_ext_clk", mout_sclk_peri_spi1_spi_ext_clk_p, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_SPI1_SPI_EXT_CLK, 12, 1, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_SPI1_SPI_EXT_CLK, 26, 1, 0, NULL),
	MUX(none, "mout_sclk_peri_spi2_spi_ext_clk", mout_sclk_peri_spi2_spi_ext_clk_p, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_SPI2_SPI_EXT_CLK, 12, 1, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_SPI2_SPI_EXT_CLK, 26, 1, 0, NULL),
	MUX(none, "mout_sclk_peri_uart0_ext_uclk", mout_sclk_peri_uart0_ext_uclk_p, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_UART0_EXT_UCLK, 12, 1, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_UART0_EXT_UCLK, 26, 1, 0, NULL),
	MUX(none, "mout_sclk_peri_uart1_ext_uclk", mout_sclk_peri_uart1_ext_uclk_p, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_UART1_EXT_UCLK, 12, 1, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_UART1_EXT_UCLK, 26, 1, 0, NULL),
	MUX(none, "mout_sclk_peri_uart2_ext_uclk", mout_sclk_peri_uart2_ext_uclk_p, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_UART2_EXT_UCLK, 12, 1, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_UART2_EXT_UCLK, 26, 1, 0, NULL),
	MUX(mout_sclk_isp_sensor0, "mout_sclk_isp_sensor0", mout_sclk_isp_sensor0_p, EXYNOS3475_CLK_CON_MUX_SCLK_ISP_SENSOR0, 12, 1, EXYNOS3475_CLK_CON_MUX_SCLK_ISP_SENSOR0, 26, 1, 0, NULL),
	MUX(mout_sclk_isp_sensor1, "mout_sclk_isp_sensor1", mout_sclk_isp_sensor1_p, EXYNOS3475_CLK_CON_MUX_SCLK_ISP_SENSOR1, 12, 1, EXYNOS3475_CLK_CON_MUX_SCLK_ISP_SENSOR1, 26, 1, 0, NULL),
};

static struct samsung_composite_divider exynos3475_div_clks[] __refdata = {
	/* BUS0 */
	DIV(none, "dout_pclk_bus0_83", "mout_aclk_bus0_333_user", EXYNOS3475_CLK_CON_DIV_PCLK_BUS0_83, 0, 3, EXYNOS3475_CLK_CON_DIV_PCLK_BUS0_83, 25, 1, 0, NULL),
	/* BUS2 */
	DIV(none, "dout_pclk_bus2_83", "mout_aclk_bus2_333_user", EXYNOS3475_CLK_CON_DIV_PCLK_BUS2_83, 0, 3, EXYNOS3475_CLK_CON_DIV_PCLK_BUS2_83, 25, 1, 0, NULL),
	/* CPU */
	DIV(none, "dout_cpu_1", "mout_cpu", EXYNOS3475_CLK_CON_DIV_CPU_1, 0, 3, EXYNOS3475_CLK_CON_DIV_CPU_1, 25, 1, 0, NULL),
	DIV(none, "dout_cpu_2", "dout_cpu_1", EXYNOS3475_CLK_CON_DIV_CPU_2, 0, 3, EXYNOS3475_CLK_CON_DIV_CPU_2, 25, 1, 0, NULL),
	DIV(none, "dout_atclk_cpu", "dout_cpu_2", EXYNOS3475_CLK_CON_DIV_ATCLK_CPU, 0, 3, EXYNOS3475_CLK_CON_DIV_ATCLK_CPU, 25, 1, 0, NULL),
	DIV(none, "dout_pclk_dbg", "dout_cpu_2", EXYNOS3475_CLK_CON_DIV_PCLK_DBG, 0, 3, EXYNOS3475_CLK_CON_DIV_PCLK_DBG, 25, 1, 0, NULL),
	DIV(none, "dout_aclk_cpu", "dout_cpu_2", EXYNOS3475_CLK_CON_DIV_ACLK_CPU, 0, 3, EXYNOS3475_CLK_CON_DIV_ACLK_CPU, 25, 1, 0, NULL),
	DIV(none, "dout_pclk_cpu", "dout_cpu_2", EXYNOS3475_CLK_CON_DIV_PCLK_CPU, 0, 3, EXYNOS3475_CLK_CON_DIV_PCLK_CPU, 25, 1, 0, NULL),
	DIV(none, "dout_sclk_hpm_cpu", "mout_cpu", EXYNOS3475_CLK_CON_DIV_SCLK_HPM_CPU, 0, 3, EXYNOS3475_CLK_CON_DIV_SCLK_HPM_CPU, 25, 1, 0, NULL),
	DIV(none, "dout_cpu_pll", "mout_cpu", EXYNOS3475_CLK_CON_DIV_CPU_PLL, 0, 3, EXYNOS3475_CLK_CON_DIV_CPU_PLL, 25, 1, 0, NULL),
	DIV(none, "dout_cpu_run_monitor", "dout_cpu_2", EXYNOS3475_CLK_CON_DIV_CPU_RUN_MONITOR, 0, 3, EXYNOS3475_CLK_CON_DIV_CPU_RUN_MONITOR, 25, 1, 0, NULL),
	/* DISPAUD */
	DIV(dout_sclk_decon_int_vclk, "dout_sclk_decon_int_vclk", "mout_sclk_decon_int_vclk_user", EXYNOS3475_CLK_CON_DIV_SCLK_DECON_INT_VCLK, 0, 3, EXYNOS3475_CLK_CON_DIV_SCLK_DECON_INT_VCLK, 25, 1, 0, NULL),
	DIV(dout_sclk_decon_int_eclk, "dout_sclk_decon_int_eclk", "mout_sclk_decon_int_eclk", EXYNOS3475_CLK_CON_DIV_SCLK_DECON_INT_ECLK, 0, 3, EXYNOS3475_CLK_CON_DIV_SCLK_DECON_INT_ECLK, 25, 1, 0, NULL),
	DIV(dout_sclk_mi2s_aud, "dout_sclk_mi2s_aud", "mout_sclk_mi2s_aud", EXYNOS3475_CLK_CON_DIV_SCLK_MI2S_AUD, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_MI2S_AUD, 25, 1, 0, NULL),
	DIV(dout_sclk_mixer_aud, "dout_sclk_mixer_aud", "mout_sclk_mi2s_aud_user", EXYNOS3475_CLK_CON_DIV_SCLK_MIXER_AUD, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_MIXER_AUD, 25, 1, 0, NULL),
	/* G3D */
	DIV(none, "dout_aclk_g3d_667", "mout_aclk_g3d_667", EXYNOS3475_CLK_CON_DIV_ACLK_G3D_667, 0, 3, EXYNOS3475_CLK_CON_DIV_ACLK_G3D_667, 25, 1, 0, NULL),
	DIV(none, "dout_pclk_g3d_167", "dout_aclk_g3d_667", EXYNOS3475_CLK_CON_DIV_PCLK_G3D_167, 0, 3, EXYNOS3475_CLK_CON_DIV_PCLK_G3D_167, 25, 1, 0, NULL),
	/* ISP */
	DIV(dout_pclk_isp_150, "dout_pclk_isp_150", "mout_aclk_isp_300_user", EXYNOS3475_CLK_CON_DIV_PCLK_ISP_150, 0, 4, EXYNOS3475_CLK_CON_DIV_PCLK_ISP_150, 25, 1, 0, NULL),
	/* MFCMSCL */
	DIV(none, "dout_pclk_mfcmscl_83", "mout_aclk_mfcmscl_333_user", EXYNOS3475_CLK_CON_DIV_PCLK_MFCMSCL_83, 0, 2, EXYNOS3475_CLK_CON_DIV_PCLK_MFCMSCL_83, 25, 1, 0, NULL),
	/* MIF */
	DIV(none, "dout_clkm_phy", "mout_clkm_phy_b", EXYNOS3475_CLK_CON_DIV_CLKM_PHY, 0, 4, EXYNOS3475_CLK_CON_DIV_CLKM_PHY, 25, 1, 0, NULL),
	DIV(none, "dout_clk2x_phy", "mout_clk2x_phy_b", EXYNOS3475_CLK_CON_DIV_CLK2X_PHY, 0, 4, EXYNOS3475_CLK_CON_DIV_CLK2X_PHY, 25, 1, 0, NULL),
	DIV(dout_aclk_mif_333, "dout_aclk_mif_333", "mout_aclk_mif_333", EXYNOS3475_CLK_CON_DIV_ACLK_MIF_333, 0, 3, EXYNOS3475_CLK_CON_DIV_ACLK_MIF_333, 25, 1, 0, "dout_aclk_mif_333"),
	DIV(dout_aclk_mif_166, "dout_aclk_mif_166", "dout_aclk_mif_333", EXYNOS3475_CLK_CON_DIV_ACLK_MIF_166, 0, 2, EXYNOS3475_CLK_CON_DIV_ACLK_MIF_166, 25, 1, 0, "dout_aclk_mif_166"),
	DIV(dout_aclk_mif_83, "dout_aclk_mif_83", "mout_aclk_mif_83", EXYNOS3475_CLK_CON_DIV_ACLK_MIF_83, 0, 4, EXYNOS3475_CLK_CON_DIV_ACLK_MIF_83, 25, 1, 0, "dout_aclk_mif_83"),
	DIV(dout_aclk_mif_fix_50, "dout_aclk_mif_fix_50", "mout_aclk_mif_fix_50", EXYNOS3475_CLK_CON_DIV_ACLK_MIF_FIX_50, 0, 4, EXYNOS3475_CLK_CON_DIV_ACLK_MIF_FIX_50, 25, 1, 0, "dout_aclk_mif_fix_50"),
	DIV(dout_aclk_dispaud_133, "dout_aclk_dispaud_133", "mout_aclk_dispaud_133", EXYNOS3475_CLK_CON_DIV_ACLK_DISPAUD_133, 0, 4, EXYNOS3475_CLK_CON_DIV_ACLK_DISPAUD_133, 25, 1, 0, "dout_aclk_dispaud_133"),
	DIV(dout_aclk_isp_300, "dout_aclk_isp_300", "mout_aclk_isp_300", EXYNOS3475_CLK_CON_DIV_ACLK_ISP_300, 0, 4, EXYNOS3475_CLK_CON_DIV_ACLK_ISP_300, 25, 1, 0, "dout_aclk_isp_300"),
	DIV(dout_sclk_disp_decon_int_eclk, "dout_sclk_disp_decon_int_eclk", "mout_sclk_disp_decon_int_eclk", EXYNOS3475_CLK_CON_DIV_SCLK_DISP_DECON_INT_ECLK, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_DISP_DECON_INT_ECLK, 25, 1, 0, NULL),
	DIV(dout_sclk_disp_decon_int_vclk, "dout_sclk_disp_decon_int_vclk", "mout_sclk_disp_decon_int_vclk", EXYNOS3475_CLK_CON_DIV_SCLK_DISP_DECON_INT_VCLK, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_DISP_DECON_INT_VCLK, 25, 1, 0, NULL),
	DIV(dout_mif_noddr, "dout_mif_noddr", "mout_div2_media_pll", EXYNOS3475_CLK_CON_DIV_MIF_NODDR, 0, 4, EXYNOS3475_CLK_CON_DIV_MIF_NODDR, 25, 1, 0, "dout_mif_noddr"),
	/* TOP */
	DIV(dout_aclk_fsys_200, "dout_aclk_fsys_200", "mout_bus_pll_user", EXYNOS3475_CLK_CON_DIV_ACLK_FSYS_200, 0, 4, EXYNOS3475_CLK_CON_DIV_ACLK_FSYS_200, 25, 1, 0, "dout_aclk_fsys_200"),
	DIV(dout_aclk_imem_266, "dout_aclk_imem_266", "mout_bus_pll_user", EXYNOS3475_CLK_CON_DIV_ACLK_IMEM_266, 0, 4, EXYNOS3475_CLK_CON_DIV_ACLK_IMEM_266, 25, 1, 0, "dout_aclk_imem_266"),
	DIV(dout_aclk_imem_200, "dout_aclk_imem_200", "mout_bus_pll_user", EXYNOS3475_CLK_CON_DIV_ACLK_IMEM_200, 0, 4, EXYNOS3475_CLK_CON_DIV_ACLK_IMEM_200, 25, 1, 0, "dout_aclk_imem_200"),
	DIV(dout_aclk_bus0_333, "dout_aclk_bus0_333", "mout_aclk_bus0_333", EXYNOS3475_CLK_CON_DIV_ACLK_BUS0_333, 0, 4, EXYNOS3475_CLK_CON_DIV_ACLK_BUS0_333, 25, 1, 0, "dout_aclk_bus0_333"),
	DIV(dout_aclk_bus2_333, "dout_aclk_bus2_333", "mout_aclk_bus2_333", EXYNOS3475_CLK_CON_DIV_ACLK_BUS2_333, 0, 4, EXYNOS3475_CLK_CON_DIV_ACLK_BUS2_333, 25, 1, 0, "dout_aclk_bus2_333"),
	DIV(dout_aclk_peri_66, "dout_aclk_peri_66", "mout_bus_pll_user", EXYNOS3475_CLK_CON_DIV_ACLK_PERI_66, 0, 4, EXYNOS3475_CLK_CON_DIV_ACLK_PERI_66, 25, 1, 0, NULL),
	DIV(dout_aclk_g3d_400, "dout_aclk_g3d_400", "mout_bus_pll_user", EXYNOS3475_CLK_CON_DIV_ACLK_G3D_400, 0, 4, EXYNOS3475_CLK_CON_DIV_ACLK_G3D_400, 25, 1, 0, NULL),
	DIV(dout_aclk_mfcmscl_200, "dout_aclk_mfcmscl_200", "mout_aclk_mfcmscl_200", EXYNOS3475_CLK_CON_DIV_ACLK_MFCMSCL_200, 0, 4, EXYNOS3475_CLK_CON_DIV_ACLK_MFCMSCL_200, 25, 1, 0, "dout_aclk_mfcmscl_200"),
	DIV(dout_aclk_mfcmscl_333, "dout_aclk_mfcmscl_333", "mout_aclk_mfcmscl_333", EXYNOS3475_CLK_CON_DIV_ACLK_MFCMSCL_333, 0, 4, EXYNOS3475_CLK_CON_DIV_ACLK_MFCMSCL_333, 25, 1, 0, "dout_aclk_mfcmscl_333"),
	DIV(dout_sclk_fsys_mmc0_sdclkin_a, "dout_sclk_fsys_mmc0_sdclkin_a", "mout_sclk_fsys_mmc0_sdclkin", EXYNOS3475_CLK_CON_DIV_SCLK_FSYS_MMC0_SDCLKIN_A, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_FSYS_MMC0_SDCLKIN_A, 25, 1, 0, NULL),
	DIV(dout_sclk_fsys_mmc0_sdclkin_b, "dout_sclk_fsys_mmc0_sdclkin_b", "dout_sclk_fsys_mmc0_sdclkin_a", EXYNOS3475_CLK_CON_DIV_SCLK_FSYS_MMC0_SDCLKIN_B, 0, 8, EXYNOS3475_CLK_CON_DIV_SCLK_FSYS_MMC0_SDCLKIN_B, 25, 1, 0, NULL),
	DIV(dout_sclk_fsys_mmc1_sdclkin_a, "dout_sclk_fsys_mmc1_sdclkin_a", "mout_sclk_fsys_mmc1_sdclkin", EXYNOS3475_CLK_CON_DIV_SCLK_FSYS_MMC1_SDCLKIN_A, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_FSYS_MMC1_SDCLKIN_A, 25, 1, 0, NULL),
	DIV(dout_sclk_fsys_mmc1_sdclkin_b, "dout_sclk_fsys_mmc1_sdclkin_b", "dout_sclk_fsys_mmc1_sdclkin_a", EXYNOS3475_CLK_CON_DIV_SCLK_FSYS_MMC1_SDCLKIN_B, 0, 8, EXYNOS3475_CLK_CON_DIV_SCLK_FSYS_MMC1_SDCLKIN_B, 25, 1, 0, NULL),
	DIV(dout_sclk_fsys_mmc2_sdclkin_a, "dout_sclk_fsys_mmc2_sdclkin_a", "mout_sclk_fsys_mmc2_sdclkin", EXYNOS3475_CLK_CON_DIV_SCLK_FSYS_MMC2_SDCLKIN_A, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_FSYS_MMC2_SDCLKIN_A, 25, 1, 0, NULL),
	DIV(dout_sclk_fsys_mmc2_sdclkin_b, "dout_sclk_fsys_mmc2_sdclkin_b", "dout_sclk_fsys_mmc2_sdclkin_a", EXYNOS3475_CLK_CON_DIV_SCLK_FSYS_MMC2_SDCLKIN_B, 0, 8, EXYNOS3475_CLK_CON_DIV_SCLK_FSYS_MMC2_SDCLKIN_B, 25, 1, 0, NULL),
	DIV(none, "dout_sclk_peri_spi0_a", "mout_sclk_peri_spi0_spi_ext_clk", EXYNOS3475_CLK_CON_DIV_SCLK_PERI_SPI0_A, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_PERI_SPI0_A, 25, 1, 0, NULL),
	DIV(none, "dout_sclk_peri_spi0_b", "dout_sclk_peri_spi0_a", EXYNOS3475_CLK_CON_DIV_SCLK_PERI_SPI0_B, 0, 8, EXYNOS3475_CLK_CON_DIV_SCLK_PERI_SPI0_B, 25, 1, 0, NULL),
	DIV(none, "dout_sclk_peri_spi1_a", "mout_sclk_peri_spi1_spi_ext_clk", EXYNOS3475_CLK_CON_DIV_SCLK_PERI_SPI1_A, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_PERI_SPI1_A, 25, 1, 0, NULL),
	DIV(none, "dout_sclk_peri_spi1_b", "dout_sclk_peri_spi1_a", EXYNOS3475_CLK_CON_DIV_SCLK_PERI_SPI1_B, 0, 8, EXYNOS3475_CLK_CON_DIV_SCLK_PERI_SPI1_B, 25, 1, 0, NULL),
	DIV(none, "dout_sclk_peri_spi2_a", "mout_sclk_peri_spi2_spi_ext_clk", EXYNOS3475_CLK_CON_DIV_SCLK_PERI_SPI2_A, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_PERI_SPI2_A, 25, 1, 0, NULL),
	DIV(none, "dout_sclk_peri_spi2_b", "dout_sclk_peri_spi2_a", EXYNOS3475_CLK_CON_DIV_SCLK_PERI_SPI2_B, 0, 8, EXYNOS3475_CLK_CON_DIV_SCLK_PERI_SPI2_B, 25, 1, 0, NULL),
	DIV(dout_sclk_peri_uart0, "dout_sclk_peri_uart0", "mout_sclk_peri_uart0_ext_uclk", EXYNOS3475_CLK_CON_DIV_SCLK_PERI_UART0, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_PERI_UART0, 25, 1, 0, NULL),
	DIV(dout_sclk_peri_uart1, "dout_sclk_peri_uart1", "mout_sclk_peri_uart1_ext_uclk", EXYNOS3475_CLK_CON_DIV_SCLK_PERI_UART1, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_PERI_UART1, 25, 1, 0, NULL),
	DIV(dout_sclk_peri_uart2, "dout_sclk_peri_uart2", "mout_sclk_peri_uart2_ext_uclk", EXYNOS3475_CLK_CON_DIV_SCLK_PERI_UART2, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_PERI_UART2, 25, 1, 0, NULL),
	DIV(dout_sclk_isp_sensor0_a, "dout_sclk_isp_sensor0_a", "mout_sclk_isp_sensor0", EXYNOS3475_CLK_CON_DIV_SCLK_ISP_SENSOR0_A, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_ISP_SENSOR0_A, 25, 1, 0, NULL),
	DIV(dout_sclk_isp_sensor0_b, "dout_sclk_isp_sensor0_b", "dout_sclk_isp_sensor0_a", EXYNOS3475_CLK_CON_DIV_SCLK_ISP_SENSOR0_B, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_ISP_SENSOR0_B, 25, 1, 0, NULL),
	DIV(dout_sclk_isp_sensor1_a, "dout_sclk_isp_sensor1_a", "mout_sclk_isp_sensor1", EXYNOS3475_CLK_CON_DIV_SCLK_ISP_SENSOR1_A, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_ISP_SENSOR1_A, 25, 1, 0, NULL),
	DIV(dout_sclk_isp_sensor1_b, "dout_sclk_isp_sensor1_b", "dout_sclk_isp_sensor1_a", EXYNOS3475_CLK_CON_DIV_SCLK_ISP_SENSOR1_B, 0, 4, EXYNOS3475_CLK_CON_DIV_SCLK_ISP_SENSOR1_B, 25, 1, 0, NULL),
};

static struct samsung_usermux exynos3475_usermux_clks[] __refdata = {
	/* BUS0 */
	USERMUX(none, "mout_aclk_bus0_333_user", "aclk_bus0_333", EXYNOS3475_CLK_CON_MUX_ACLK_BUS0_333_USER, 12, EXYNOS3475_CLK_CON_MUX_ACLK_BUS0_333_USER, 26, CLK_IGNORE_UNUSED, NULL),
	/* BUS2 */
	USERMUX(none, "mout_aclk_bus2_333_user", "aclk_bus2_333", EXYNOS3475_CLK_CON_MUX_ACLK_BUS2_333_USER, 12, EXYNOS3475_CLK_CON_MUX_ACLK_BUS2_333_USER, 26, CLK_IGNORE_UNUSED, NULL),
	/* CPU */
	USERMUX(mout_sclk_bus_pll_user, "mout_sclk_bus_pll_user", "aclk_cpu_800", EXYNOS3475_CLK_CON_MUX_SCLK_BUS_PLL_USER, 12, EXYNOS3475_CLK_CON_MUX_SCLK_BUS_PLL_USER, 26, CLK_IGNORE_UNUSED, "mout_sclk_bus_pll_user"),
	/* DISPAUD */
	USERMUX(none, "mout_aclk_dispaud_133_user", "aclk_dispaud_133", EXYNOS3475_CLK_CON_MUX_ACLK_DISPAUD_133_USER, 12, EXYNOS3475_CLK_CON_MUX_ACLK_DISPAUD_133_USER, 26, 0, NULL),
	USERMUX(mout_sclk_decon_int_vclk_user, "mout_sclk_decon_int_vclk_user", "sclk_decon_int_vclk_i", EXYNOS3475_CLK_CON_MUX_SCLK_DECON_INT_VCLK_USER, 12, EXYNOS3475_CLK_CON_MUX_SCLK_DECON_INT_VCLK_USER, 26, 0, NULL),
	USERMUX(mout_sclk_decon_int_eclk_user, "mout_sclk_decon_int_eclk_user", "sclk_decon_int_eclk_i", EXYNOS3475_CLK_CON_MUX_SCLK_DECON_INT_ECLK_USER, 12, EXYNOS3475_CLK_CON_MUX_SCLK_DECON_INT_ECLK_USER, 26, 0, NULL),
	USERMUX(none, "mout_phyclk_mipi_phy_m_txbyteclkhs_m4s4_user", "phyclk_mipi_phy_m_txbyteclkhs_m4s4", EXYNOS3475_CLK_CON_MUX_PHYCLK_MIPI_PHY_M_TXBYTECLKHS_M4S4_USER, 12, 0, 0, 0, NULL),
	USERMUX(none, "mout_phyclk_mipi_phy_m_rxclkesc0_m4s4_user", "phyclk_mipi_phy_m_rxclkesc0_m4s4", EXYNOS3475_CLK_CON_MUX_PHYCLK_MIPI_PHY_M_RXCLKESC0_M4S4_USER, 12, 0, 0, 0, NULL),
	USERMUX(mout_sclk_mi2s_aud_user, "mout_sclk_mi2s_aud_user", "sclk_aud_300", EXYNOS3475_CLK_CON_MUX_SCLK_MI2S_AUD_USER, 12, EXYNOS3475_CLK_CON_MUX_SCLK_MI2S_AUD_USER, 26, 0, NULL),
	/* FSYS */
	USERMUX(none, "mout_aclk_fsys_200_user", "aclk_fsys_200", EXYNOS3475_CLK_CON_MUX_ACLK_FSYS_200_USER, 12, EXYNOS3475_CLK_CON_MUX_ACLK_FSYS_200_USER, 26, 0, NULL),
	USERMUX(mout_sclk_fsys_mmc0_sdclkin_user, "mout_sclk_fsys_mmc0_sdclkin_user", "sclk_fsys_mmc0_sdclkin", EXYNOS3475_CLK_CON_MUX_SCLK_FSYS_MMC0_SDCLKIN_USER, 12, EXYNOS3475_CLK_CON_MUX_SCLK_FSYS_MMC0_SDCLKIN_USER, 26, CLK_IGNORE_UNUSED, NULL),
	USERMUX(mout_sclk_fsys_mmc1_sdclkin_user, "mout_sclk_fsys_mmc1_sdclkin_user", "sclk_fsys_mmc1_sdclkin", EXYNOS3475_CLK_CON_MUX_SCLK_FSYS_MMC1_SDCLKIN_USER, 12, EXYNOS3475_CLK_CON_MUX_SCLK_FSYS_MMC1_SDCLKIN_USER, 26, CLK_IGNORE_UNUSED, NULL),
	USERMUX(mout_sclk_fsys_mmc2_sdclkin_user, "mout_sclk_fsys_mmc2_sdclkin_user", "sclk_fsys_mmc2_sdclkin", EXYNOS3475_CLK_CON_MUX_SCLK_FSYS_MMC2_SDCLKIN_USER, 12, EXYNOS3475_CLK_CON_MUX_SCLK_FSYS_MMC2_SDCLKIN_USER, 26, CLK_IGNORE_UNUSED, NULL),
	USERMUX(none, "mout_phyclk_usbhost20_usb20_freeclk_user", "phyclk_usbhost20_phy_usb20_freeclk", EXYNOS3475_CLK_CON_MUX_PHYCLK_USBHOST20_USB20_FREECLK_USER, 12, EXYNOS3475_CLK_CON_MUX_PHYCLK_USBHOST20_USB20_FREECLK_USER, 26, 0, NULL),
	USERMUX(none, "mout_phyclk_usbhost20_usb20_phyclock_user", "phyclk_usbhost20_phy_usb20_phyclock", EXYNOS3475_CLK_CON_MUX_PHYCLK_USBHOST20_USB20_PHYCLOCK_USER, 12, EXYNOS3475_CLK_CON_MUX_PHYCLK_USBHOST20_USB20_PHYCLOCK_USER, 26, 0, NULL),
	USERMUX(none, "mout_phyclk_usbhost20_usb20_clk48mohci_user", "phyclk_usbhost20_phy_usb20_clk48mohci", EXYNOS3475_CLK_CON_MUX_PHYCLK_USBHOST20_USB20_CLK48MOHCI_USER, 12, EXYNOS3475_CLK_CON_MUX_PHYCLK_USBHOST20_USB20_CLK48MOHCI_USER, 26, 0, NULL),
	USERMUX(mout_phyclk_usbotg_otg20_phyclock_user, "mout_phyclk_usbotg_otg20_phyclock_user", "phyclk_usbotg_phy_otg20_phyclock", EXYNOS3475_CLK_CON_MUX_PHYCLK_USBOTG_OTG20_PHYCLOCK_USER, 12, EXYNOS3475_CLK_CON_MUX_PHYCLK_USBOTG_OTG20_PHYCLOCK_USER, 26, 0, NULL),
	/* G3D */
	USERMUX(mout_aclk_g3d_400_user, "mout_aclk_g3d_400_user", "aclk_g3d_400", EXYNOS3475_CLK_CON_MUX_ACLK_G3D_400_USER, 12, EXYNOS3475_CLK_CON_MUX_ACLK_G3D_400_USER, 26, CLK_IGNORE_UNUSED, NULL),
	/* IMEM */
	USERMUX(none, "mout_aclk_imem_200_user", "aclk_imem_200", EXYNOS3475_CLK_CON_MUX_ACLK_IMEM_200_USER, 12, EXYNOS3475_CLK_CON_MUX_ACLK_IMEM_200_USER, 26, CLK_IGNORE_UNUSED, NULL),
	USERMUX(none, "mout_aclk_imem_266_user", "aclk_imem_266", EXYNOS3475_CLK_CON_MUX_ACLK_IMEM_266_USER, 12, EXYNOS3475_CLK_CON_MUX_ACLK_IMEM_266_USER, 26, CLK_IGNORE_UNUSED, NULL),
	/* ISP */
	USERMUX(mout_aclk_isp_300_user, "mout_aclk_isp_300_user", "aclk_isp_300", EXYNOS3475_CLK_CON_MUX_ACLK_ISP_300_USER, 12, EXYNOS3475_CLK_CON_MUX_ACLK_ISP_300_USER, 26, 0, NULL),
	USERMUX(none, "mout_phyclk_csi_link_rx_byte_clk_hs_user", "phyclk_csi_phy0_rxbyteclkhs", EXYNOS3475_CLK_CON_MUX_PHYCLK_CSI_LINK_RX_BYTE_CLK_HS_USER, 12, 0, 0, 0, NULL),
	/* MFCMSCL */
	USERMUX(none, "mout_aclk_mfcmscl_333_user", "aclk_mfcmscl_333", EXYNOS3475_CLK_CON_MUX_ACLK_MFCMSCL_333_USER, 12, EXYNOS3475_CLK_CON_MUX_ACLK_MFCMSCL_333_USER, 26, CLK_IGNORE_UNUSED, NULL),
	USERMUX(none, "mout_aclk_mfcmscl_200_user", "aclk_mfcmscl_200", EXYNOS3475_CLK_CON_MUX_ACLK_MFCMSCL_200_USER, 12, EXYNOS3475_CLK_CON_MUX_ACLK_MFCMSCL_200_USER, 26, CLK_IGNORE_UNUSED, NULL),
	/* PERI */
	USERMUX(mout_aclk_peri_66_user, "mout_aclk_peri_66_user", "aclk_peri_66", EXYNOS3475_CLK_CON_MUX_ACLK_PERI_66_USER, 12, EXYNOS3475_CLK_CON_MUX_ACLK_PERI_66_USER, 26, CLK_IGNORE_UNUSED, NULL),
	USERMUX(none, "mout_sclk_peri_spi0_spi_ext_clk_user", "sclk_peri_spi0_spi_ext_clk", EXYNOS3475_CLK_CON_MUX_SCLK_PERI_SPI0_SPI_EXT_CLK_USER, 12, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_SPI0_SPI_EXT_CLK_USER, 26, CLK_SET_RATE_PARENT, NULL),
	USERMUX(none, "mout_sclk_peri_spi1_spi_ext_clk_user", "sclk_peri_spi1_spi_ext_clk", EXYNOS3475_CLK_CON_MUX_SCLK_PERI_SPI1_SPI_EXT_CLK_USER, 12, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_SPI1_SPI_EXT_CLK_USER, 26, CLK_SET_RATE_PARENT, NULL),
	USERMUX(none, "mout_sclk_peri_spi2_spi_ext_clk_user", "sclk_peri_spi2_spi_ext_clk", EXYNOS3475_CLK_CON_MUX_SCLK_PERI_SPI2_SPI_EXT_CLK_USER, 12, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_SPI2_SPI_EXT_CLK_USER, 26, CLK_SET_RATE_PARENT, NULL),
	USERMUX(none, "mout_sclk_peri_uart0_ext_uclk_user", "sclk_peri_uart0_ext_uclk", EXYNOS3475_CLK_CON_MUX_SCLK_PERI_UART0_EXT_UCLK_USER, 12, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_UART0_EXT_UCLK_USER, 26, 0, NULL),
	USERMUX(none, "mout_sclk_peri_uart1_ext_uclk_user", "sclk_peri_uart1_ext_uclk", EXYNOS3475_CLK_CON_MUX_SCLK_PERI_UART1_EXT_UCLK_USER, 12, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_UART1_EXT_UCLK_USER, 26, 0, NULL),
	USERMUX(none, "mout_sclk_peri_uart2_ext_uclk_user", "sclk_peri_uart2_ext_uclk", EXYNOS3475_CLK_CON_MUX_SCLK_PERI_UART2_EXT_UCLK_USER, 12, EXYNOS3475_CLK_CON_MUX_SCLK_PERI_UART2_EXT_UCLK_USER, 26, 0, NULL),
	/* TOP */
	USERMUX(mout_bus_pll_user, "mout_bus_pll_user", "sclk_bus_pll_top", EXYNOS3475_CLK_CON_MUX_BUS_PLL_USER, 12, EXYNOS3475_CLK_CON_MUX_BUS_PLL_USER, 26, CLK_IGNORE_UNUSED, "mout_bus_pll_user"),
	USERMUX(mout_media_pll_user, "mout_media_pll_user", "sclk_media_pll_top", EXYNOS3475_CLK_CON_MUX_MEDIA_PLL_USER, 12, EXYNOS3475_CLK_CON_MUX_MEDIA_PLL_USER, 26, CLK_IGNORE_UNUSED, "mout_media_pll_user"),
};

static struct samsung_gate exynos3475_gate_clks[] __initdata = {
	/* BUS0 */
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_bus0_333_user_aclk_bus_d_0_aclk_bus0nd_333", "mout_aclk_bus0_333_user", EXYNOS3475_CLK_ENABLE_ACLK_BUS0_333_USER, 0, ALWAYS_ON_CLK, NULL),
	GATE(none, "pclk_bus0_83_aclk_bus_d_0_aclk_bus0nd_82", "dout_pclk_bus0_83", EXYNOS3475_CLK_ENABLE_PCLK_BUS0_83, 4, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "pclk_bus0_83_pclk_sysreg_bus0_pclk", "dout_pclk_bus0_83", EXYNOS3475_CLK_ENABLE_PCLK_BUS0_83, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_bus0_83_pclk_pmu_bus0_pclk", "dout_pclk_bus0_83", EXYNOS3475_CLK_ENABLE_PCLK_BUS0_83, 2, 0, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "pclk_bus0_83_aclk_ahb2apb_bus0_hclk", "dout_pclk_bus0_83", EXYNOS3475_CLK_ENABLE_PCLK_BUS0_83, 1, ALWAYS_ON_CLK, NULL),
	GATE(none, "pclk_bus0_83_aclk_bus_p_0_aclk_bus0np", "dout_pclk_bus0_83", EXYNOS3475_CLK_ENABLE_PCLK_BUS0_83, 0, ALWAYS_ON_CLK, NULL),
#endif
	/* BUS2 */
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_bus2_333_user_aclk_bus_d_2_aclk_bus2nd_333", "mout_aclk_bus2_333_user", EXYNOS3475_CLK_ENABLE_ACLK_BUS2_333_USER, 0, ALWAYS_ON_CLK, NULL),
	GATE(none, "pclk_bus2_83_aclk_bus_d_2_aclk_bus2nd_82", "dout_pclk_bus2_83", EXYNOS3475_CLK_ENABLE_PCLK_BUS2_83, 4, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "pclk_bus2_83_pclk_sysreg_bus2_pclk", "dout_pclk_bus2_83", EXYNOS3475_CLK_ENABLE_PCLK_BUS2_83, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_bus2_83_pclk_pmu_bus2_pclk", "dout_pclk_bus2_83", EXYNOS3475_CLK_ENABLE_PCLK_BUS2_83, 2, 0, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "pclk_bus2_83_aclk_ahb2apb_bus2_hclk", "dout_pclk_bus2_83", EXYNOS3475_CLK_ENABLE_PCLK_BUS2_83, 1, ALWAYS_ON_CLK, NULL),
	GATE(none, "pclk_bus2_83_aclk_bus_p_2_aclk_bus2np_82", "dout_pclk_bus2_83", EXYNOS3475_CLK_ENABLE_PCLK_BUS2_83, 0, ALWAYS_ON_CLK, NULL),
#endif
	/* CPU */
#ifndef UNREGISTERED_CLK
	GATE(none, "atclk_cpu_aclk_stm_event_dma_i_aclk", "dout_atclk_cpu", EXYNOS3475_CLK_ENABLE_ATCLK_CPU, 10, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "atclk_cpu_aclk_sasync_dbg_to_imem_hclks", "dout_atclk_cpu", EXYNOS3475_CLK_ENABLE_ATCLK_CPU, 9, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "atclk_cpu_aclk_sasync_dbg_to_bus_d_mif_0_aclk", "dout_atclk_cpu", EXYNOS3475_CLK_ENABLE_ATCLK_CPU, 8, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "atclk_cpu_sclk_cssys_dbg_traceclkin", "dout_atclk_cpu", EXYNOS3475_CLK_ENABLE_ATCLK_CPU, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "atclk_cpu_sclk_cssys_dbg_ctmclk", "dout_atclk_cpu", EXYNOS3475_CLK_ENABLE_ATCLK_CPU, 6, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "atclk_cpu_sclk_cssys_dbg_atclk", "dout_atclk_cpu", EXYNOS3475_CLK_ENABLE_ATCLK_CPU, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "atclk_cpu_aclk_masync_dbg_to_apl_3_atclkm", "dout_atclk_cpu", EXYNOS3475_CLK_ENABLE_ATCLK_CPU, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "atclk_cpu_aclk_masync_dbg_to_apl_2_atclkm", "dout_atclk_cpu", EXYNOS3475_CLK_ENABLE_ATCLK_CPU, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "atclk_cpu_aclk_masync_dbg_to_apl_1_atclkm", "dout_atclk_cpu", EXYNOS3475_CLK_ENABLE_ATCLK_CPU, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "atclk_cpu_aclk_masync_dbg_to_apl_0_atclkm", "dout_atclk_cpu", EXYNOS3475_CLK_ENABLE_ATCLK_CPU, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "atclk_cpu_aclk_cssys_dbg_hclk", "dout_atclk_cpu", EXYNOS3475_CLK_ENABLE_ATCLK_CPU, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_dbg_sclk_cpu_pclkdbg", "dout_pclk_dbg", EXYNOS3475_CLK_ENABLE_PCLK_DBG, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_dbg_pclk_sasync_dbg_to_bus_p_isp_pclksys", "dout_pclk_dbg", EXYNOS3475_CLK_ENABLE_PCLK_DBG, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_dbg_pclk_sasync_dbg_to_apl_pclksys", "dout_pclk_dbg", EXYNOS3475_CLK_ENABLE_PCLK_DBG, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_dbg_pclk_asyncapb_dump_pc_cpu_cclksys", "dout_pclk_dbg", EXYNOS3475_CLK_ENABLE_PCLK_DBG, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_dbg_pclk_secjtag_pclk", "dout_pclk_dbg", EXYNOS3475_CLK_ENABLE_PCLK_DBG, 1, CLK_IGNORE_UNUSED, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "pclk_dbg_sclk_cssys_dbg_pclkdbg", "dout_pclk_dbg", EXYNOS3475_CLK_ENABLE_PCLK_DBG, 0, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_cpu_aclk_sasync_cpu_to_bus_d_mif_0_i_clk", "dout_aclk_cpu", EXYNOS3475_CLK_ENABLE_ACLK_CPU, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_cpu_aclk_stm_event_dma_i_hclk", "dout_pclk_cpu", EXYNOS3475_CLK_ENABLE_PCLK_CPU, 8, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_cpu_pclk_asyncapb_dump_pc_cpu_pclksys", "dout_pclk_cpu", EXYNOS3475_CLK_ENABLE_PCLK_CPU, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_cpu_pclk_dump_pc_cpu_i_clk", "dout_pclk_cpu", EXYNOS3475_CLK_ENABLE_PCLK_CPU, 6, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_cpu_pclk_stm_event_dma_i_pclk", "dout_pclk_cpu", EXYNOS3475_CLK_ENABLE_PCLK_CPU, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_cpu_pclk_sfr_apbif_hpm_cpu_0_pclk", "dout_pclk_cpu", EXYNOS3475_CLK_ENABLE_PCLK_CPU, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_cpu_pclk_sysreg_cpu_pclk", "dout_pclk_cpu", EXYNOS3475_CLK_ENABLE_PCLK_CPU, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_cpu_pclk_pmu_cpu_pclk", "dout_pclk_cpu", EXYNOS3475_CLK_ENABLE_PCLK_CPU, 2, CLK_IGNORE_UNUSED, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "pclk_cpu_aclk_bus_p_0_cpu_aclk_cpunp", "dout_pclk_cpu", EXYNOS3475_CLK_ENABLE_PCLK_CPU, 1, ALWAYS_ON_CLK, NULL),
	GATE(none, "pclk_cpu_aclk_ahb2apb_cpu_hclk", "dout_pclk_cpu", EXYNOS3475_CLK_ENABLE_PCLK_CPU, 0, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "sclk_hpm_cpu_0_hpm_clock", "dout_sclk_hpm_cpu", EXYNOS3475_CLK_ENABLE_SCLK_HPM_CPU, 0, CLK_IGNORE_UNUSED, NULL),
#endif /* UNREGISTERED_CLK */
	/* DISPAUD */
#ifndef ALWAYS_ON_CLK
	GATE(aclk_dispaud_133_pclk_dispaud_p, "aclk_dispaud_133_pclk_dispaud_p", "mout_aclk_dispaud_133_user", EXYNOS3475_CLK_ENABLE_ACLK_DISPAUD_133, 4, ALWAYS_ON_CLK, NULL),
#endif
	GATE(aclk_dispaud_133_aclk_aud, "aclk_dispaud_133_aclk_aud", "mout_aclk_dispaud_133_user", EXYNOS3475_CLK_ENABLE_ACLK_DISPAUD_133, 3, 0, NULL),
	GATE(aclk_dispaud_133_aclk_disp, "aclk_dispaud_133_aclk_disp", "mout_aclk_dispaud_133_user", EXYNOS3475_CLK_ENABLE_ACLK_DISPAUD_133, 2, 0, NULL),
	GATE(aclk_dispaud_133_aclk_dispaud_d, "aclk_dispaud_133_aclk_dispaud_d", "mout_aclk_dispaud_133_user", EXYNOS3475_CLK_ENABLE_ACLK_DISPAUD_133, 1, 0, NULL),
	GATE(none, "aclk_dispaud_133_aclk_ppmu", "mout_aclk_dispaud_133_user", EXYNOS3475_CLK_ENABLE_ACLK_DISPAUD_133, 0, 0, NULL),
	GATE(none, "aclk_dispaud_133_secure_cfw_aclk_cfw", "mout_aclk_dispaud_133_user", EXYNOS3475_CLK_ENABLE_ACLK_DISPAUD_133_SECURE_CFW, 0, 0, NULL),
	GATE(sclk_disp_decon_int_vclk, "sclk_disp_decon_int_vclk", "dout_sclk_decon_int_vclk", EXYNOS3475_CLK_ENABLE_SCLK_DECON_INT_VCLK, 0, 0, NULL),
	GATE(sclk_disp_decon_int_eclk, "sclk_disp_decon_int_eclk", "dout_sclk_decon_int_eclk", EXYNOS3475_CLK_ENABLE_SCLK_DECON_INT_ECLK, 0, 0, NULL),
	GATE(phyclk_mipi_phy_m_txbyteclkhs_m4s4_phyclk_dsi_link0_bitclkdiv8, "phyclk_mipi_phy_m_txbyteclkhs_m4s4_phyclk_dsi_link0_bitclkdiv8", "mout_phyclk_mipi_phy_m_txbyteclkhs_m4s4_user", EXYNOS3475_CLK_ENABLE_PHYCLK_MIPI_PHY_M_TXBYTECLKHS_M4S4, 0, 0, NULL),
	GATE(phyclk_mipi_phy_m_rxclkesc0_m4s4_phyclk_dsi_link0_rxclkesc0, "phyclk_mipi_phy_m_rxclkesc0_m4s4_phyclk_dsi_link0_rxclkesc0", "mout_phyclk_mipi_phy_m_rxclkesc0_m4s4_user", EXYNOS3475_CLK_ENABLE_PHYCLK_MIPI_PHY_M_RXCLKESC0_M4S4, 0, 0, NULL),
	GATE(sclk_mi2s_aud_sclk_mi2s_i2scodclki, "sclk_mi2s_aud_sclk_mi2s_i2scodclki", "dout_sclk_mi2s_aud", EXYNOS3475_CLK_ENABLE_SCLK_MI2S_AUD, 0, 0, NULL),
	GATE(sclk_mixer_aud_sysclk, "sclk_mixer_aud_sysclk", "dout_sclk_mixer_aud", EXYNOS3475_CLK_ENABLE_SCLK_MIXER_AUD, 0, 0, NULL),
	GATE(ioclk_aud_i2s_sclk_ap_in_sclk_mi2s_aud_sclk_ap_in, "ioclk_aud_i2s_sclk_ap_in_sclk_mi2s_aud_sclk_ap_in", "ioclk_aud_i2s_sclk_ap_in", EXYNOS3475_CLK_ENABLE_IOCLK_AUD_I2S_SCLK_AP_IN, 0, 0, NULL),
	GATE(ioclk_aud_i2s_bclk_bt_in_sclk_mi2s_aud_bclk_bt_in, "ioclk_aud_i2s_bclk_bt_in_sclk_mi2s_aud_bclk_bt_in", "ioclk_aud_i2s_bclk_bt_in", EXYNOS3475_CLK_ENABLE_IOCLK_AUD_I2S_BCLK_BT_IN, 0, 0, NULL),
	GATE(ioclk_aud_i2s_bclk_cp_in_sclk_mi2s_aud_bclk_cp_in, "ioclk_aud_i2s_bclk_cp_in_sclk_mi2s_aud_bclk_cp_in", "ioclk_aud_i2s_bclk_cp_in", EXYNOS3475_CLK_ENABLE_IOCLK_AUD_I2S_BCLK_CP_IN, 0, 0, NULL),
	GATE(ioclk_aud_i2s_bclk_fm_in_sclk_mi2s_aud_bclk_fm_in, "ioclk_aud_i2s_bclk_fm_in_sclk_mi2s_aud_bclk_fm_in", "ioclk_aud_i2s_bclk_fm_in", EXYNOS3475_CLK_ENABLE_IOCLK_AUD_I2S_BCLK_FM_IN, 0, 0, NULL),
	/* FSYS */
	GATE(none, "aclk_fsys_200_pclk_ppmu_fsys_pclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 27, 0, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_fsys_200_pclk_gpio_fsys_pclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 26, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_fsys_200_pclk_sysreg_fsys_pclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 25, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_fsys_200_pclk_pmu_fsys_pclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 24, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_fsys_200_aclk_ppmu_fsys_aclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 23, 0, NULL),
	GATE(aclk_fsys_200_aclk_upsizer_ahb_usbhs_aclk, "aclk_fsys_200_aclk_upsizer_ahb_usbhs_aclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 22, 0, NULL),
	GATE(none, "aclk_fsys_200_aclk_upsizer_dma_fsys1_aclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 21, 0, NULL),
	GATE(none, "aclk_fsys_200_aclk_upsizer_dma_fsys0_aclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 20, 0, NULL),
	GATE(aclk_fsys_200_aclk_upsizer_fsys1_to_fsys0_aclk, "aclk_fsys_200_aclk_upsizer_fsys1_to_fsys0_aclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 19, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_fsys_200_aclk_bus_d_fsys_aclk_fsysnd", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 18, CLK_IGNORE_UNUSED, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_fsys_200_aclk_ahb2apb_fsys2_hclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 17, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_fsys_200_aclk_ahb2apb_fsys1_hclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 16, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_fsys_200_aclk_ahb2axi_usbhs_hclk, "aclk_fsys_200_aclk_ahb2axi_usbhs_hclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 15, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_fsys_200_aclk_ahb_usbhs_hclk, "aclk_fsys_200_aclk_ahb_usbhs_hclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 14, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_fsys_200_aclk_ahb_fsys0_hclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 13, CLK_IGNORE_UNUSED, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_fsys_200_aclk_bus_p_fsys_aclk_fsysnp", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 12, ALWAYS_ON_CLK, NULL),
#endif
	GATE(aclk_fsys_200_aclk_xiu_d_fsys1_aclk, "aclk_fsys_200_aclk_xiu_d_fsys1_aclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 11, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_fsys_200_aclk_xiu_d_fsys0_aclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 10, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_fsys_200_aclk_upsizer_usbotg_aclk, "aclk_fsys_200_aclk_upsizer_usbotg_aclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 9, 0, NULL),
	GATE(aclk_fsys_200_aclk_ahb2axi_usbotg_hclk, "aclk_fsys_200_aclk_ahb2axi_usbotg_hclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 8, 0, NULL),
	GATE(aclk_fsys_200_aclk_usbotg20_aclk, "aclk_fsys_200_aclk_usbotg20_aclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 7, 0, NULL),
	GATE(aclk_fsys_200_aclk_mmc2_i_aclk, "aclk_fsys_200_aclk_mmc2_i_aclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 6, 0, NULL),
	GATE(aclk_fsys_200_aclk_mmc1_i_aclk, "aclk_fsys_200_aclk_mmc1_i_aclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 5, 0, NULL),
	GATE(aclk_fsys_200_aclk_mmc0_i_aclk, "aclk_fsys_200_aclk_mmc0_i_aclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 4, 0, NULL),
	GATE(none, "aclk_fsys_200_aclk_sromc_hclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 3, 0, NULL),
	GATE(aclk_fsys_200_aclk_usbhost20_aclk, "aclk_fsys_200_aclk_usbhost20_aclk", "mout_aclk_fsys_200_user", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 2, 0, NULL),
	GATE(aclk_fsys_200_aclk_dma_fsys1_aclk, "aclk_fsys_200_aclk_dma_fsys1_aclk", "aclk_fsys_200_aclk_upsizer_dma_fsys1_aclk", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 1, 0, NULL),
	GATE(aclk_fsys_200_aclk_dma_fsys0_aclk, "aclk_fsys_200_aclk_dma_fsys0_aclk", "aclk_fsys_200_aclk_upsizer_dma_fsys0_aclk", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200, 0, 0, NULL),
	GATE(sclk_fsys_mmc0_sdclkin_sclk_mmc0_sdclkin, "sclk_fsys_mmc0_sdclkin_sclk_mmc0_sdclkin", "mout_sclk_fsys_mmc0_sdclkin_user", EXYNOS3475_CLK_ENABLE_SCLK_FSYS_MMC0_SDCLKIN, 0,	CLK_IGNORE_UNUSED, NULL),
	GATE(sclk_fsys_mmc1_sdclkin_sclk_mmc1_sdclkin, "sclk_fsys_mmc1_sdclkin_sclk_mmc1_sdclkin", "mout_sclk_fsys_mmc1_sdclkin_user", EXYNOS3475_CLK_ENABLE_SCLK_FSYS_MMC1_SDCLKIN, 0,	CLK_IGNORE_UNUSED, NULL),
	GATE(sclk_fsys_mmc2_sdclkin_sclk_mmc2_sdclkin, "sclk_fsys_mmc2_sdclkin_sclk_mmc2_sdclkin", "mout_sclk_fsys_mmc2_sdclkin_user", EXYNOS3475_CLK_ENABLE_SCLK_FSYS_MMC2_SDCLKIN, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(usb_pll, "sclk_usb20_phy_clkcore", "usb_pll", EXYNOS3475_CLK_ENABLE_SCLK_USB20_PHY_CLKCORE, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(phyclk_usbhost20_usb20_freeclk, "phyclk_usbhost20_usb20_freeclk", "mout_phyclk_usbhost20_usb20_freeclk_user", EXYNOS3475_CLK_ENABLE_PHYCLK_USBHOST20_USB20_FREECLK, 0, 0, NULL),
	GATE(phyclk_usbhost20_usb20_phyclock, "phyclk_usbhost20_usb20_phyclock", "mout_phyclk_usbhost20_usb20_phyclock_user", EXYNOS3475_CLK_ENABLE_PHYCLK_USBHOST20_USB20_PHYCLOCK, 0, 0, NULL),
	GATE(phyclk_usbhost20_usb20_clk48mohci, "phyclk_usbhost20_usb20_clk48mohci", "mout_phyclk_usbhost20_usb20_clk48mohci_user", EXYNOS3475_CLK_ENABLE_PHYCLK_USBHOST20_USB20_CLK48MOHCI, 0, 0, NULL),
	GATE(none, "phyclk_usbhotg_otg20_phyclock_phyclk_usbotg_otg20_phyclock", "mout_phyclk_usbotg_otg20_phyclock_user", EXYNOS3475_CLK_ENABLE_PHYCLK_USBHOTG_OTG20_PHYCLOCK, 0, CLK_IGNORE_UNUSED, NULL),
	/* G3D */
	GATE(none, "aclk_g3d_667_aclk_ppmu_g3d_aclk", "dout_aclk_g3d_667", EXYNOS3475_CLK_ENABLE_ACLK_G3D_667, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_g3d_667_aclk_qe_g3d_aclk", "dout_aclk_g3d_667", EXYNOS3475_CLK_ENABLE_ACLK_G3D_667, 3, CLK_IGNORE_UNUSED, "aclk_g3d_667_aclk_qe_g3d_aclk"),
	GATE(none, "aclk_g3d_667_aclk_async_ahb2apb_g3d_to_g3d_cclk", "dout_aclk_g3d_667", EXYNOS3475_CLK_ENABLE_ACLK_G3D_667, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_g3d_667_aclk_bus_d_g3d_aclk_g3dnd", "dout_aclk_g3d_667", EXYNOS3475_CLK_ENABLE_ACLK_G3D_667, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_g3d_667_aclk_g3d_clk, "aclk_g3d_667_aclk_g3d_clk", "dout_aclk_g3d_667", EXYNOS3475_CLK_ENABLE_ACLK_G3D_667, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_g3d_667_secure_cfw_aclk_smmu_g3d_aclk_cfw", "dout_aclk_g3d_667", EXYNOS3475_CLK_ENABLE_ACLK_G3D_667_SECURE_CFW, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_g3d_167_pclk_ppmu_g3d_pclk", "dout_pclk_g3d_167", EXYNOS3475_CLK_ENABLE_PCLK_G3D_167, 6, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_g3d_167_pclk_qe_g3d_pclk", "dout_pclk_g3d_167", EXYNOS3475_CLK_ENABLE_PCLK_G3D_167, 5, CLK_IGNORE_UNUSED, "pclk_g3d_167_pclk_qe_g3d_pclk"),
	GATE(none, "pclk_g3d_167_pclk_sysreg_g3d_pclk", "dout_pclk_g3d_167", EXYNOS3475_CLK_ENABLE_PCLK_G3D_167, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_g3d_167_pclk_pmu_g3d_pclk", "dout_pclk_g3d_167", EXYNOS3475_CLK_ENABLE_PCLK_G3D_167, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "pclk_g3d_167_pclk_async_ahb2apb_g3d_to_g3d_pclk", "dout_pclk_g3d_167", EXYNOS3475_CLK_ENABLE_PCLK_G3D_167, 2, CLK_IGNORE_UNUSED, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "pclk_g3d_167_aclk_bus_p_0_to_ahb2apb_g3d_aclk_g3dnp", "dout_pclk_g3d_167", EXYNOS3475_CLK_ENABLE_PCLK_G3D_167, 1, ALWAYS_ON_CLK, NULL),
	GATE(none, "pclk_g3d_167_aclk_ahb2apb_g3d_hclk", "dout_pclk_g3d_167", EXYNOS3475_CLK_ENABLE_PCLK_G3D_167, 0, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "pclk_g3d_167_secure_cfw_pclk_smmu_g3d_pclk_cfw", "dout_pclk_g3d_167", EXYNOS3475_CLK_ENABLE_PCLK_G3D_167_SECURE_CFW, 0, CLK_IGNORE_UNUSED, NULL),
	/* IMEM */
	GATE(none, "aclk_imem_200_aclk_masync_xiu_d_imem_to_xiu_p_imem_aclk", "mout_aclk_imem_200_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200, 9, 0, NULL),
	GATE(none, "aclk_imem_200_pclk_ppmu_imem", "mout_aclk_imem_200_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200, 8, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_200_pclk_sysreg_imem_pclk", "mout_aclk_imem_200_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200, 7, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_200_pclk_pmu_imem_pclk", "mout_aclk_imem_200_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200, 6, CLK_IGNORE_UNUSED, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_imem_200_aclk_downsizer_imem0p_aclk", "mout_aclk_imem_200_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200, 5, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_imem_200_aclk_downsizer_gic_aclk", "mout_aclk_imem_200_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200, 4, CLK_IGNORE_UNUSED, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_imem_200_aclk_xiu_p_imem_aclk", "mout_aclk_imem_200_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200, 3, ALWAYS_ON_CLK, NULL),
	GATE(none, "aclk_imem_200_aclk_axi2apb_imem0p_aclk", "mout_aclk_imem_200_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200, 2, ALWAYS_ON_CLK, NULL),
	GATE(none, "aclk_imem_200_aclk_bus_p_imem_aclk", "mout_aclk_imem_200_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200, 1, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_imem_200_aclk_intc_cpu_clk", "mout_aclk_imem_200_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_200_secure_iramc_top_aclk_iramc_top_aclk", "mout_aclk_imem_200_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200_SECURE_IRAMC_TOP, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_200_secure_sss_pclk_sss_pclk", "mout_aclk_imem_200_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200_SECURE_SSS, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_200_secure_rtic_pclk_rtic_pclk", "mout_aclk_imem_200_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200_SECURE_RTIC, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_266_aclk_downsizer_async_aclk", "mout_aclk_imem_266_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_266, 4, 0, NULL),
	GATE(none, "aclk_imem_266_aclk_sasync_xiu_d_imem_to_xiu_p_imem_aclk", "mout_aclk_imem_266_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_266, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_266_aclk_ppmu_imem", "mout_aclk_imem_266_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_266, 2, 0, NULL),
	GATE(none, "aclk_imem_266_aclk_bus_d_imem_aclk", "mout_aclk_imem_266_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_266, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_266_aclk_xiu_d_imem_aclk", "mout_aclk_imem_266_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_266, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_266_secure_sss_aclk_sss_aclk", "mout_aclk_imem_266_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_266_SECURE_SSS, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_266_secure_rtic_aclk_rtic_aclk", "mout_aclk_imem_266_user", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_266_SECURE_RTIC, 0, CLK_IGNORE_UNUSED, NULL),
	/* ISP */
	GATE(aclk_isp_300_aclk_isp_d, "aclk_isp_300_aclk_isp_d", "mout_aclk_isp_300_user", EXYNOS3475_CLK_ENABLE_ACLK_ISP_300, 3, 0, NULL),
	GATE(aclk_isp_300_aclk_ppmu, "aclk_isp_300_aclk_ppmu", "mout_aclk_isp_300_user", EXYNOS3475_CLK_ENABLE_ACLK_ISP_300, 2, 0, NULL),
	GATE(aclk_isp_300_aclk_fd, "aclk_isp_300_aclk_fd", "mout_aclk_isp_300_user", EXYNOS3475_CLK_ENABLE_ACLK_ISP_300, 1, 0, NULL),
	GATE(aclk_isp_300_aclk_is, "aclk_isp_300_aclk_is", "mout_aclk_isp_300_user", EXYNOS3475_CLK_ENABLE_ACLK_ISP_300, 0, 0, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "pclk_isp_150_pclk_isp_p", "dout_pclk_isp_150", EXYNOS3475_CLK_ENABLE_PCLK_ISP_150, 0, ALWAYS_ON_CLK, NULL),
#endif
	GATE(phyclk_csi_link0_rx_byte_clk_hs0, "phyclk_csi_link0_rx_byte_clk_hs0", "mout_phyclk_csi_link_rx_byte_clk_hs_user", EXYNOS3475_CLK_ENABLE_PHYCLK_CSI_LINK0_RX_BYTE_CLK_HS0, 0, 0, NULL),
	/* MFCMSCL */
	GATE(none, "aclk_mfcmscl_333_aclk_ppmu", "mout_aclk_mfcmscl_333_user", EXYNOS3475_CLK_ENABLE_ACLK_MFCMSCL_333, 5, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_mfcmscl_333_aclk_mfcmscl_d, "aclk_mfcmscl_333_aclk_mfcmscl_d", "mout_aclk_mfcmscl_333_user", EXYNOS3475_CLK_ENABLE_ACLK_MFCMSCL_333, 4, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_mfcmscl_333_aclk_mscl_bi, "aclk_mfcmscl_333_aclk_mscl_bi", "mout_aclk_mfcmscl_333_user", EXYNOS3475_CLK_ENABLE_ACLK_MFCMSCL_333, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_mfcmscl_333_aclk_mscl_poly, "aclk_mfcmscl_333_aclk_mscl_poly", "mout_aclk_mfcmscl_333_user", EXYNOS3475_CLK_ENABLE_ACLK_MFCMSCL_333, 2, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_mfcmscl_333_aclk_jpeg, "aclk_mfcmscl_333_aclk_jpeg", "mout_aclk_mfcmscl_333_user", EXYNOS3475_CLK_ENABLE_ACLK_MFCMSCL_333, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_mfcmscl_333_aclk_mscl_d, "aclk_mfcmscl_333_aclk_mscl_d", "mout_aclk_mfcmscl_333_user", EXYNOS3475_CLK_ENABLE_ACLK_MFCMSCL_333, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_mfcmscl_333_secure_cfw_aclk_cfw", "mout_aclk_mfcmscl_333_user", EXYNOS3475_CLK_ENABLE_ACLK_MFCMSCL_333_SECURE_CFW, 0, CLK_IGNORE_UNUSED, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "pclk_mfcmscl_83_pclk_mfcmscl_p", "dout_pclk_mfcmscl_83", EXYNOS3475_CLK_ENABLE_PCLK_MFCMSCL_83, 0, ALWAYS_ON_CLK, NULL),
#endif
	GATE(aclk_mfcmscl_200_aclk_mfc, "aclk_mfcmscl_200_aclk_mfc", "mout_aclk_mfcmscl_200_user", EXYNOS3475_CLK_ENABLE_ACLK_MFCMSCL_200, 0, CLK_IGNORE_UNUSED, NULL),
	/* MIF */
#ifndef UNREGISTERED_CLK
	GATE(none, "sclk_ddr_phy0_clkm", "dout_clkm_phy", EXYNOS3475_CLK_ENABLE_SCLK_DDR_PHY0_CLKM, 0, UNREGISTERED_CLK, NULL),
	GATE(none, "sclk_ddr_phy0_clk2x", "dout_clk2x_phy", EXYNOS3475_CLK_ENABLE_SCLK_DDR_PHY0_CLK2X, 0, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_aclk_bus_d_mif_nrt_aclk_mif_nrtnd", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 19, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_aclk_masync_cp_cleany_0_aclk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 18, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_aclk_cleany_0_aclk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 17, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_aclk_ppmu_bus_d_mif_rt_to_drex_0_aclk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 16, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_aclk_ppmu_xiu_mif_d_0_to_drex_0_aclk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 15, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_aclk_ppmu_ba_remap_0_to_drex_0_aclk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 14, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_aclk_ppmu_bus_d_mif_nrt_to_drex_0_aclk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 13, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_aclk_masync_bus_d_mif_rt_to_drex_0_aclkm", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 12, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_aclk_masync_xiu_mif_d_0_to_drex_0_aclkm", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 11, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_sclk_drex_0_tz_clk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 10, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_sclk_drex_0_perev_clk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 9, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_sclk_drex_0_memif_wr_clk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 8, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_sclk_drex_0_memif_rd_clk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 7, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_sclk_drex_0_memif_clk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 6, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_sclk_drex_0_sch_wr_clk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 5, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_sclk_drex_0_sch_rd_clk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 4, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_sclk_drex_0_sch_clk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 3, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_sclk_drex_0_busif_wr_clk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 2, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_sclk_drex_0_busif_rd_clk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 1, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_0_aclk", "aclk_ddr_phy0_dfi_clk", EXYNOS3475_CLK_ENABLE_ACLK_DREX, 0, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_drex_rclk_sclk_drex_0_rclk", "fin_pll", EXYNOS3475_CLK_ENABLE_ACLK_DREX_RCLK, 0, UNREGISTERED_CLK, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_mif_333_aclk_qe", "dout_aclk_mif_333", EXYNOS3475_CLK_ENABLE_ACLK_MIF_333, 9, ALWAYS_ON_CLK, "aclk_mif_333_aclk_qe"),
	GATE(none, "aclk_mif_333_aclk_ppmu_cpu_to_xiu_mif_d_0_aclk", "dout_aclk_mif_333", EXYNOS3475_CLK_ENABLE_ACLK_MIF_333, 8, ALWAYS_ON_CLK, NULL),
	GATE(none, "aclk_mif_333_aclk_bus_p_mif_cdn_aclk", "dout_aclk_mif_333", EXYNOS3475_CLK_ENABLE_ACLK_MIF_333, 7, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_mif_333_aclk_sasync_bus_d_mif_rt_to_drex_0_aclks", "dout_aclk_mif_333", EXYNOS3475_CLK_ENABLE_ACLK_MIF_333, 6, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_333_aclk_upsizer_dbg_to_xiu_mif_d_0_aclk", "dout_aclk_mif_333", EXYNOS3475_CLK_ENABLE_ACLK_MIF_333, 5, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_333_aclk_sasync_xiu_mif_d_0_to_drex_0_aclks", "dout_aclk_mif_333", EXYNOS3475_CLK_ENABLE_ACLK_MIF_333, 4, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_333_aclk_masync_dbg_to_xiu_mif_d_0_aclk", "dout_aclk_mif_333", EXYNOS3475_CLK_ENABLE_ACLK_MIF_333, 3, UNREGISTERED_CLK, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_mif_333_aclk_masync_cpu_to_xiu_mif_d_0_aclk", "dout_aclk_mif_333", EXYNOS3475_CLK_ENABLE_ACLK_MIF_333, 2, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_mif_333_aclk_bus_d_mif_rt_aclk_mif_rtnd", "dout_aclk_mif_333", EXYNOS3475_CLK_ENABLE_ACLK_MIF_333, 1, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_166_aclk_cleany_1_aclk", "dout_aclk_mif_166", EXYNOS3475_CLK_ENABLE_ACLK_MIF_166, 3, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_166_aclk_accwindow_1_aclk", "dout_aclk_mif_166", EXYNOS3475_CLK_ENABLE_ACLK_MIF_166, 2, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_166_aclk_masync_cp_cleany_1_aclk", "dout_aclk_mif_166", EXYNOS3475_CLK_ENABLE_ACLK_MIF_166, 1, UNREGISTERED_CLK, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_mif_166_aclk_bus_p_mif_aclk_mifnp", "dout_aclk_mif_166", EXYNOS3475_CLK_ENABLE_ACLK_MIF_166, 0, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_mif_83_pclk_qe", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 24, UNREGISTERED_CLK, "aclk_mif_83_pclk_qe"),
	GATE(none, "aclk_mif_83_aclk_asyncapb_cssys_hclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 23, UNREGISTERED_CLK, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_mif_83_pclk_bus_p_mif_srv", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 22, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_mif_83_pclk_bus_d_mif_rt_srv", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 21, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_pclk_bus_d_mif_nrt_srv", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 20, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_pclk_ppmu_cpu_to_xiu_mif_d_0_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 19, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_pclk_ppmu_bus_d_mif_rt_to_drex_0_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 18, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_pclk_ppmu_bus_d_mif_nrt_to_drex_0_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 17, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_pclk_ppmu_ba_remap_0_to_drex_0_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 16, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_pclk_masync_cp_cleany_0_aclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 15, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_pclk_ppmu_xiu_mif_d_0_to_drex_0_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 14, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_pclk_gpio_mif_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 13, UNREGISTERED_CLK, NULL),
	GATE(aclk_mif_83_pclk_sasync_hsi2c_cp_pclk, "aclk_mif_83_pclk_sasync_hsi2c_cp_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 12, UNREGISTERED_CLK, NULL),
	GATE(aclk_mif_83_pclk_sasync_hsi2c_ap_pclk, "aclk_mif_83_pclk_sasync_hsi2c_ap_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 11, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_pclk_mailbox_apbif_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 10, UNREGISTERED_CLK, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_mif_83_pclk_alv_pmu_apbif_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 9, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_mif_83_pclk_ddr_phy0_apbif_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 8, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_pclk_sysreg_mif_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 7, UNREGISTERED_CLK, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_mif_83_pclk_pmu_mif_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 6, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_mif_83_pclk_drex_0_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 5, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_aclk_ahb2apb_mif3_hclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 4, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_aclk_ahb2apb_mif2_hclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 3, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_aclk_ahb2apb_mif1_hclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 2, UNREGISTERED_CLK, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_mif_83_aclk_ahb2apb_mif0_hclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 1, ALWAYS_ON_CLK, NULL),
	GATE(none, "aclk_mif_83_aclk_ahb_bridge_hclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83, 0, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_mif_83_secure_drex_0_tz_pclk_drex_0_pclk_tz", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83_SECURE_DREX_0_TZ, 0, UNREGISTERED_CLK, NULL),
	GATE(none, "aclk_mif_83_secure_mailbox_apbif_pclk_s_mailbox_apbif_pclk", "dout_aclk_mif_83", EXYNOS3475_CLK_ENABLE_ACLK_MIF_83_SECURE_MAILBOX_APBIF, 0, UNREGISTERED_CLK, NULL),
	GATE(aclk_mif_fix_50_pclk_masync_hsi2c_cp_pclk, "aclk_mif_fix_50_pclk_masync_hsi2c_cp_pclk", "dout_aclk_mif_fix_50", EXYNOS3475_CLK_ENABLE_ACLK_MIF_FIX_50, 2, UNREGISTERED_CLK, NULL),
	GATE(aclk_mif_fix_50_pclk_masync_hsi2c_ap_pclk, "aclk_mif_fix_50_pclk_masync_hsi2c_ap_pclk", "dout_aclk_mif_fix_50", EXYNOS3475_CLK_ENABLE_ACLK_MIF_FIX_50, 1, UNREGISTERED_CLK, NULL),
	GATE(aclk_mif_fix_50_pclk_hsi2c_mif_pclk, "aclk_mif_fix_50_pclk_hsi2c_mif_pclk", "dout_aclk_mif_fix_50", EXYNOS3475_CLK_ENABLE_ACLK_MIF_FIX_50, 0, UNREGISTERED_CLK, NULL),
#endif /* UNREGISTERED_CLK */
	GATE(none, "aclk_mif_333_aclk_xiu_mif_d_0_aclk", "dout_aclk_mif_333", EXYNOS3475_CLK_ENABLE_ACLK_MIF_333, 0, CLK_GATE_ENABLE, NULL),
	GATE(none, "sclk_aud_300", "aud_pll", EXYNOS3475_CLK_ENABLE_SCLK_AUD_300, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_dispaud_133", "dout_aclk_dispaud_133", EXYNOS3475_CLK_ENABLE_ACLK_DISPAUD_133_MIF, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_isp_300", "dout_aclk_isp_300", EXYNOS3475_CLK_ENABLE_ACLK_ISP_300_MIF, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_decon_int_eclk_i", "dout_sclk_disp_decon_int_eclk", EXYNOS3475_CLK_ENABLE_SCLK_DISP_DECON_INT_ECLK, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_decon_int_vclk_i", "dout_sclk_disp_decon_int_vclk", EXYNOS3475_CLK_ENABLE_SCLK_DISP_DECON_INT_VCLK, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_cpu_800", "bus_pll", EXYNOS3475_CLK_ENABLE_ACLK_CPU_800, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_bus_pll_top", "bus_pll", EXYNOS3475_CLK_ENABLE_SCLK_BUS_PLL_TOP, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_media_pll_top", "ffac_media_pll_div2", EXYNOS3475_CLK_ENABLE_SCLK_MEDIA_PLL_TOP, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_ap2cp_mif_pll_out_333_sclk_ap2cp_pll_media_clk", "ffac_media_pll_div4", EXYNOS3475_CLK_ENABLE_SCLK_AP2CP_MIF_PLL_OUT_333, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "sclk_ap2cp_mif_pll_out_400_sclk_ap2cp_pll_bus_clk", "ffac_bus_pll_div2", EXYNOS3475_CLK_ENABLE_SCLK_AP2CP_MIF_PLL_OUT_400, 0, CLK_IGNORE_UNUSED, NULL),
	/* PERI */
	GATE(aclk_peri_66_1_pclk_hsi2c8_ipclk, "aclk_peri_66_1_pclk_hsi2c8_ipclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 18, 0, NULL),
	GATE(aclk_peri_66_1_pclk_hsi2c7_ipclk, "aclk_peri_66_1_pclk_hsi2c7_ipclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 17, 0, NULL),
	GATE(aclk_peri_66_1_pclk_hsi2c1_ipclk, "aclk_peri_66_1_pclk_hsi2c1_ipclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 16, 0, NULL),
	GATE(aclk_peri_66_1_pclk_hsi2c0_ipclk, "aclk_peri_66_1_pclk_hsi2c0_ipclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 15, 0, NULL),
	GATE(aclk_peri_66_1_pclk_i2c6_pclk, "aclk_peri_66_1_pclk_i2c6_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 14, 0, NULL),
	GATE(aclk_peri_66_1_pclk_i2c5_pclk, "aclk_peri_66_1_pclk_i2c5_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 13, 0, NULL),
	GATE(aclk_peri_66_1_pclk_i2c4_pclk, "aclk_peri_66_1_pclk_i2c4_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 12, 0, NULL),
	GATE(aclk_peri_66_1_pclk_i2c3_pclk, "aclk_peri_66_1_pclk_i2c3_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 11, 0, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_peri_66_1_aclk_ahb2apb_peric1_hclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 10, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_peri_66_1_aclk_ahb2apb_peric0_hclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 9, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_peri_66_1_pclk_custom_efuse_apbif_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 8, CLK_GATE_ENABLE, NULL),
	GATE(none, "aclk_peri_66_1_pclk_efuse_writer_sc_apbif_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 7, 0, NULL),
	GATE(none, "aclk_peri_66_1_pclk_cmu_top_apbif_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 6, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_peri_66_1_pclk_wdt_cpu_pclk, "aclk_peri_66_1_pclk_wdt_cpu_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 5, 0, NULL),
	GATE(aclk_peri_66_1_pclk_mct_pclk, "aclk_peri_66_1_pclk_mct_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 4, 0, NULL),
	GATE(none, "aclk_peri_66_1_aclk_ahb2apb_peris1_hclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 3, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_peri_66_1_aclk_ahb2apb_peris0_hclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 2, CLK_IGNORE_UNUSED, NULL),
#ifndef ALWAYS_ON_CLK
	GATE(none, "aclk_peri_66_1_aclk_bus_p_peri_aclk_perinp", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 1, ALWAYS_ON_CLK, NULL),
	GATE(none, "aclk_peri_66_1_aclk_ahb_peri_hclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_1, 0, ALWAYS_ON_CLK, NULL),
#endif
	GATE(none, "aclk_peri_66_secure_tzpc_pclk_tzpc10_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_TZPC, 10, 0, NULL),
	GATE(none, "aclk_peri_66_secure_tzpc_pclk_tzpc9_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_TZPC, 9, 0, NULL),
	GATE(none, "aclk_peri_66_secure_tzpc_pclk_tzpc8_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_TZPC, 8, 0, NULL),
	GATE(none, "aclk_peri_66_secure_tzpc_pclk_tzpc7_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_TZPC, 7, 0, NULL),
	GATE(none, "aclk_peri_66_secure_tzpc_pclk_tzpc6_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_TZPC, 6, 0, NULL),
	GATE(none, "aclk_peri_66_secure_tzpc_pclk_tzpc5_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_TZPC, 5, CLK_GATE_ENABLE, NULL),
	GATE(none, "aclk_peri_66_secure_tzpc_pclk_tzpc4_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_TZPC, 4, 0, NULL),
	GATE(none, "aclk_peri_66_secure_tzpc_pclk_tzpc3_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_TZPC, 3, 0, NULL),
	GATE(none, "aclk_peri_66_secure_tzpc_pclk_tzpc2_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_TZPC, 2, 0, NULL),
	GATE(none, "aclk_peri_66_secure_tzpc_pclk_tzpc1_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_TZPC, 1, 0, NULL),
	GATE(none, "aclk_peri_66_secure_tzpc_pclk_tzpc0_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_TZPC, 0, 0, NULL),
	GATE(none, "aclk_peri_66_secure_rtc_top_pclk_rtc_top_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_RTC_TOP, 0, 0, NULL),
	GATE(none, "aclk_peri_66_secure_chipid_pclk_chipid_apbif_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_CHIPID, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_peri_66_secure_seckey_pclk_seckey_apbif_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_SECKEY, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_peri_66_secure_antirbk_cnt_pclk_antirbk_cnt_apbif_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_ANTIRBK_CNT, 0, CLK_GATE_ENABLE, NULL),
	GATE(none, "aclk_peri_66_secure_monotonic_cnt_pclk_monotonic_cnt_apbif_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_MONOTONIC_CNT, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_peri_66_secure_rtc_pclk_rtc_apbif_pclk, "aclk_peri_66_secure_rtc_pclk_rtc_apbif_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_SECURE_RTC, 0, 0, NULL),
	GATE(none, "aclk_peri_66_2_pclk_sysreg_peri_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 17, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_peri_66_2_pclk_pmu_peri_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 16, 0, NULL),
	GATE(none, "aclk_peri_66_2_pclk_gpio_alive_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 15, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_peri_66_2_pclk_gpio_ese_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 14, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_peri_66_2_pclk_gpio_touch_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 13, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_peri_66_2_pclk_gpio_nfc_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 12, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_peri_66_2_pclk_gpio_peri_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 11, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_peri_66_2_pclk_tmu0_apbif_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 10, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_peri_66_2_pclk_pwm_pclk, "aclk_peri_66_2_pclk_pwm_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 9, 0, NULL),
	GATE(aclk_peri_66_2_pclk_i2c10_pclk, "aclk_peri_66_2_pclk_i2c10_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 8, 0, NULL),
	GATE(aclk_peri_66_2_pclk_i2c9_pclk, "aclk_peri_66_2_pclk_i2c9_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 7, 0, NULL),
	GATE(aclk_peri_66_2_pclk_spi2_pclk, "aclk_peri_66_2_pclk_spi2_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 6, 0, NULL),
	GATE(aclk_peri_66_2_pclk_spi1_pclk, "aclk_peri_66_2_pclk_spi1_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 5, 0, NULL),
	GATE(aclk_peri_66_2_pclk_spi0_pclk, "aclk_peri_66_2_pclk_spi0_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 4, 0, NULL),
	GATE(aclk_peri_66_2_pclk_adcif_pclk, "aclk_peri_66_2_pclk_adcif_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 3, 0, NULL),
	GATE(aclk_peri_66_2_pclk_uart2_pclk, "aclk_peri_66_2_pclk_uart2_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 2, 0, "console-pclk2"),
	GATE(aclk_peri_66_2_pclk_uart1_pclk, "aclk_peri_66_2_pclk_uart1_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 1, 0, "console-pclk1"),
	GATE(aclk_peri_66_2_pclk_uart0_pclk, "aclk_peri_66_2_pclk_uart0_pclk", "mout_aclk_peri_66_user", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66_2, 0, 0, "console-pclk0"),
	GATE(sclk_peri_spi0_spi_ext_clk_sclk_spi0_spi_ext_clk, "sclk_peri_spi0_spi_ext_clk_sclk_spi0_spi_ext_clk", "mout_sclk_peri_spi0_spi_ext_clk_user", EXYNOS3475_CLK_ENABLE_SCLK_PERI_SPI0_SPI_EXT_CLK, 0, CLK_SET_RATE_PARENT, NULL),
	GATE(sclk_peri_spi1_spi_ext_clk_sclk_spi1_spi_ext_clk, "sclk_peri_spi1_spi_ext_clk_sclk_spi1_spi_ext_clk", "mout_sclk_peri_spi1_spi_ext_clk_user", EXYNOS3475_CLK_ENABLE_SCLK_PERI_SPI1_SPI_EXT_CLK, 0, CLK_SET_RATE_PARENT, NULL),
	GATE(sclk_peri_spi2_spi_ext_clk_sclk_spi2_spi_ext_clk, "sclk_peri_spi2_spi_ext_clk_sclk_spi2_spi_ext_clk", "mout_sclk_peri_spi2_spi_ext_clk_user", EXYNOS3475_CLK_ENABLE_SCLK_PERI_SPI2_SPI_EXT_CLK, 0, CLK_SET_RATE_PARENT, NULL),
	GATE(sclk_peri_uart0_ext_uclk_sclk_uart0_ext_uclk, "sclk_peri_uart0_ext_uclk_sclk_uart0_ext_uclk", "mout_sclk_peri_uart0_ext_uclk_user", EXYNOS3475_CLK_ENABLE_SCLK_PERI_UART0_EXT_UCLK, 0, 0, "console-sclk0"),
	GATE(sclk_peri_uart1_ext_uclk_sclk_uart1_ext_uclk, "sclk_peri_uart1_ext_uclk_sclk_uart1_ext_uclk", "mout_sclk_peri_uart1_ext_uclk_user", EXYNOS3475_CLK_ENABLE_SCLK_PERI_UART1_EXT_UCLK, 0, 0, "console-sclk1"),
	GATE(sclk_peri_uart2_ext_uclk_sclk_uart2_ext_uclk, "sclk_peri_uart2_ext_uclk_sclk_uart2_ext_uclk", "mout_sclk_peri_uart2_ext_uclk_user", EXYNOS3475_CLK_ENABLE_SCLK_PERI_UART2_EXT_UCLK, 0, 0, "console-sclk2"),
	GATE(none, "ioclk_peri_oscclk_sclk_pwm_tclk0", "fin_pll", EXYNOS3475_CLK_ENABLE_IOCLK_PERI_OSCCLK, 1, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "ioclk_peri_oscclk_sclk_adcif_i_osc_sys", "fin_pll", EXYNOS3475_CLK_ENABLE_IOCLK_PERI_OSCCLK, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "ioclk_peri_oscclk_efuse_sclk_custom_efuse_clk", "fin_pll", EXYNOS3475_CLK_ENABLE_IOCLK_PERI_OSCCLK_EFUSE, 2, CLK_GATE_ENABLE, NULL),
	GATE(none, "ioclk_peri_oscclk_efuse_sclk_efuse_writer_clk", "fin_pll", EXYNOS3475_CLK_ENABLE_IOCLK_PERI_OSCCLK_EFUSE, 1, 0, NULL),
	GATE(none, "ioclk_peri_oscclk_efuse_sclk_tmu0_clk", "fin_pll", EXYNOS3475_CLK_ENABLE_IOCLK_PERI_OSCCLK_EFUSE, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "ioclk_peri_oscclk_chipid_secure_sclk_chipid_clk", "fin_pll", EXYNOS3475_CLK_ENABLE_IOCLK_PERI_OSCCLK_CHIPID_SECURE, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "ioclk_peri_oscclk_seckey_secure_sclk_seckey_clk", "fin_pll", EXYNOS3475_CLK_ENABLE_IOCLK_PERI_OSCCLK_SECKEY_SECURE, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "ioclk_peri_oscclk_antirbk_cnt_secure_sclk_antirbk_cnt_clk", "fin_pll", EXYNOS3475_CLK_ENABLE_IOCLK_PERI_OSCCLK_ANTIRBK_CNT_SECURE, 0, CLK_GATE_ENABLE, NULL),
	/* TOP */
	GATE(none, "aclk_fsys_200", "dout_aclk_fsys_200", EXYNOS3475_CLK_ENABLE_ACLK_FSYS_200_TOP, 0, 0, NULL),
	GATE(none, "aclk_imem_266", "dout_aclk_imem_266", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_266_TOP, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_imem_200", "dout_aclk_imem_200", EXYNOS3475_CLK_ENABLE_ACLK_IMEM_200_TOP, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_bus0_333", "dout_aclk_bus0_333", EXYNOS3475_CLK_ENABLE_ACLK_BUS0_333, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(none, "aclk_bus2_333", "dout_aclk_bus2_333", EXYNOS3475_CLK_ENABLE_ACLK_BUS2_333, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(aclk_peri_66, "aclk_peri_66", "dout_aclk_peri_66", EXYNOS3475_CLK_ENABLE_ACLK_PERI_66, 0, 0, NULL),
	GATE(aclk_g3d_400, "aclk_g3d_400", "dout_aclk_g3d_400", EXYNOS3475_CLK_ENABLE_ACLK_G3D_400, 0, 0, NULL),
	GATE(none, "aclk_mfcmscl_200", "dout_aclk_mfcmscl_200", EXYNOS3475_CLK_ENABLE_ACLK_MFCMSCL_200_TOP, 0, 0, NULL),
	GATE(none, "aclk_mfcmscl_333", "dout_aclk_mfcmscl_333", EXYNOS3475_CLK_ENABLE_ACLK_MFCMSCL_333_TOP, 0, CLK_IGNORE_UNUSED, NULL),
	GATE(sclk_fsys_mmc0_sdclkin, "sclk_fsys_mmc0_sdclkin", "dout_sclk_fsys_mmc0_sdclkin_b", EXYNOS3475_CLK_ENABLE_SCLK_FSYS_MMC0_SDCLKIN_TOP, 0, 0, NULL),
	GATE(sclk_fsys_mmc1_sdclkin, "sclk_fsys_mmc1_sdclkin", "dout_sclk_fsys_mmc1_sdclkin_b", EXYNOS3475_CLK_ENABLE_SCLK_FSYS_MMC1_SDCLKIN_TOP, 0, 0, NULL),
	GATE(sclk_fsys_mmc2_sdclkin, "sclk_fsys_mmc2_sdclkin", "dout_sclk_fsys_mmc2_sdclkin_b", EXYNOS3475_CLK_ENABLE_SCLK_FSYS_MMC2_SDCLKIN_TOP, 0, 0, NULL),
	GATE(none, "sclk_peri_spi0_spi_ext_clk", "dout_sclk_peri_spi0_b", EXYNOS3475_CLK_ENABLE_SCLK_PERI_SPI0_SPI_EXT_CLK_TOP, 0, CLK_SET_RATE_PARENT, NULL),
	GATE(none, "sclk_peri_spi1_spi_ext_clk", "dout_sclk_peri_spi1_b", EXYNOS3475_CLK_ENABLE_SCLK_PERI_SPI1_SPI_EXT_CLK_TOP, 0, CLK_SET_RATE_PARENT, NULL),
	GATE(none, "sclk_peri_spi2_spi_ext_clk", "dout_sclk_peri_spi2_b", EXYNOS3475_CLK_ENABLE_SCLK_PERI_SPI2_SPI_EXT_CLK_TOP, 0, CLK_SET_RATE_PARENT, NULL),
	GATE(none, "sclk_peri_uart0_ext_uclk", "dout_sclk_peri_uart0", EXYNOS3475_CLK_ENABLE_SCLK_PERI_UART0_EXT_UCLK_TOP, 0, 0, NULL),
	GATE(none, "sclk_peri_uart1_ext_uclk", "dout_sclk_peri_uart1", EXYNOS3475_CLK_ENABLE_SCLK_PERI_UART1_EXT_UCLK_TOP, 0, 0, NULL),
	GATE(none, "sclk_peri_uart2_ext_uclk", "dout_sclk_peri_uart2", EXYNOS3475_CLK_ENABLE_SCLK_PERI_UART2_EXT_UCLK_TOP, 0, 0, NULL),
	GATE(sclk_isp_sensor0, "sclk_isp_sensor0", "dout_sclk_isp_sensor0_b", EXYNOS3475_CLK_ENABLE_SCLK_ISP_SENSOR0, 0, 0, NULL),
	GATE(sclk_isp_sensor1, "sclk_isp_sensor1", "dout_sclk_isp_sensor1_b", EXYNOS3475_CLK_ENABLE_SCLK_ISP_SENSOR1, 0, 0, NULL),
};

struct samsung_fixed_rate exynos3475_fixed_rate_ext_clks[] __initdata = {
	FRATE(oscclk, "fin_pll", NULL, CLK_IS_ROOT, 0),
};

struct samsung_fixed_rate exynos3475_fixed_rate_clks[] __initdata = {
	FRATE(none, "phyclk_usbhost20_phy_usb20_freeclk", NULL, CLK_IS_ROOT, 60000000),
	FRATE(none, "phyclk_usbhost20_phy_usb20_phyclock", NULL, CLK_IS_ROOT, 60000000),
	FRATE(none, "phyclk_usbhost20_phy_usb20_clk48mohci", NULL, CLK_IS_ROOT, 48000000),
	FRATE(none, "phyclk_usbotg_phy_otg20_phyclock", NULL, CLK_IS_ROOT, 60000000),
	FRATE(none, "phyclk_mipi_phy_m_txbyteclkhs_m4s4", NULL, CLK_IS_ROOT, 188000000),
	FRATE(none, "phyclk_mipi_phy_m_rxclkesc0_m4s4", NULL, CLK_IS_ROOT, 20000000),
	FRATE(none, "phyclk_csi_phy0_rxbyteclkhs", NULL, CLK_IS_ROOT, 165000000),
	FRATE(none, "ioclk_audi2s0cdclk", NULL, CLK_IS_ROOT, 104000000),
	FRATE(none, "ioclk_aud_i2s_sclk_ap_in", NULL, CLK_IS_ROOT, 12500000),
	FRATE(none, "ioclk_aud_i2s_bclk_bt_in", NULL, CLK_IS_ROOT, 12500000),
	FRATE(none, "ioclk_aud_i2s_bclk_cp_in", NULL, CLK_IS_ROOT, 12500000),
	FRATE(none, "ioclk_aud_i2s_bclk_fm_in", NULL, CLK_IS_ROOT, 12500000),
	FRATE(none, "ioclk_peri_spi0_spiclkin", NULL, CLK_IS_ROOT, 50000000),
	FRATE(none, "ioclk_peri_spi1_spiclkin", NULL, CLK_IS_ROOT, 50000000),
	FRATE(none, "ioclk_peri_spi2_spiclkin", NULL, CLK_IS_ROOT, 50000000),
};

struct samsung_fixed_factor exynos3475_fixed_factor_clks[] __initdata = {
	FFACTOR(none, "ffac_mem0_pll_div2", "mem0_pll", 1, 2, 0),
	FFACTOR(none, "ffac_media_pll_div2", "media_pll", 1, 2, 0),
	FFACTOR(none, "ffac_media_pll_div4", "ffac_media_pll_div2", 1, 2, 0),
	FFACTOR(none, "ffac_bus_pll_div2", "bus_pll", 1, 2, 0),
};

/*
 * list of controller registers to be saved and restored during a
 * suspend/resume cycle.
 */
static __initdata unsigned long exynos3475_clk_regs[] = {
};

static struct of_device_id ext_clk_match[] __initdata = {
	{ .compatible = "samsung,exynos3475-oscclk", .data = (void *)0, },
};

static void pwm_init_clock(void)
{
	clk_register_fixed_factor(NULL, "pwm-clock",
			"aclk_peri_66_2_pclk_pwm_pclk",CLK_SET_RATE_PARENT, 1, 1);
}

static void __init exynos3475_clk_init(struct device_node *np)
{
	BUILD_BUG_ON(aclk_mfcmscl_333_aclk_jpeg != CLOCK_ACLK_JPEG);

	if (!np)
		panic("%s: unable to determine SoC\n", __func__);
	/*
	 * Register clocks for exynos3475 series.
	 * Gate clocks should be registered at last because of some gate clocks.
	 * Some gate clocks should be enabled at initial time.
	 */
	samsung_clk_init(np, 0, nr_clks, (unsigned long *)exynos3475_clk_regs,
			ARRAY_SIZE(exynos3475_clk_regs), NULL, 0);
	samsung_register_of_fixed_ext(exynos3475_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos3475_fixed_rate_ext_clks),
			ext_clk_match);
	samsung_register_comp_pll(exynos3475_pll_clks,
			ARRAY_SIZE(exynos3475_pll_clks));
	samsung_register_fixed_rate(exynos3475_fixed_rate_clks,
			ARRAY_SIZE(exynos3475_fixed_rate_clks));
	samsung_register_fixed_factor(exynos3475_fixed_factor_clks,
			ARRAY_SIZE(exynos3475_fixed_factor_clks));
	samsung_register_comp_mux(exynos3475_mux_clks,
			ARRAY_SIZE(exynos3475_mux_clks));
	samsung_register_comp_divider(exynos3475_div_clks,
			ARRAY_SIZE(exynos3475_div_clks));
	samsung_register_usermux(exynos3475_usermux_clks,
			ARRAY_SIZE(exynos3475_usermux_clks));
	samsung_register_gate(exynos3475_gate_clks,
			ARRAY_SIZE(exynos3475_gate_clks));
	pwm_init_clock();
	pr_info("EXYNOS3475: Clock setup completed\n");
}
CLK_OF_DECLARE(exynos3475_clks, "samsung,exynos3475-clock", exynos3475_clk_init);
