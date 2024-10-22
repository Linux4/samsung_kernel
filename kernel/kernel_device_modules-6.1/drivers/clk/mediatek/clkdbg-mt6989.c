// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <clk-mux.h>
#include "clkdbg.h"
#include "clkchk.h"
#include "clk-fmeter.h"

const char * const *get_mt6989_all_clk_names(void)
{
	static const char * const clks[] = {
		/* topckgen */
		"axi_sel",
		"peri_faxi_sel",
		"ufs_faxi_sel",
		"pextp_faxi_sel",
		"bus_aximem_sel",
		"mem_sub_sel",
		"peri_fmem_sub_sel",
		"ufs_fmem_sub_sel",
		"pextp_fmem_sub_sel",
		"emi_n_sel",
		"emi_s_sel",
		"emi_slice_n_sel",
		"emi_slice_s_sel",
		"ap2conn_host_sel",
		"atb_sel",
		"cirq_sel",
		"efuse_sel",
		"mcu_l3gic_sel",
		"mcu_infra_sel",
		"dsp_sel",
		"mfg_ref_sel",
		"mfg_eb_sel",
		"uart_sel",
		"spi0_b_sel",
		"spi1_b_sel",
		"spi2_b_sel",
		"spi3_b_sel",
		"spi4_b_sel",
		"spi5_b_sel",
		"spi6_b_sel",
		"spi7_b_sel",
		"msdc_macro_1p_sel",
		"msdc_macro_2p_sel",
		"msdc30_1_sel",
		"msdc30_2_sel",
		"aud_intbus_sel",
		"disp_pwm_sel",
		"usb_sel",
		"ssusb_xhci_sel",
		"usb_1p_sel",
		"ssusb_xhci_1p_sel",
		"i2c_sel",
		"aud_engen1_sel",
		"aud_engen2_sel",
		"aes_ufsfde_sel",
		"ufs_sel",
		"ufs_mbist_sel",
		"pextp_mbist_sel",
		"aud_1_sel",
		"aud_2_sel",
		"audio_h_sel",
		"adsp_sel",
		"adsp_uarthub_b_sel",
		"dpmaif_main_sel",
		"pwm_sel",
		"mcupm_sel",
		"sflash_sel",
		"ipseast_sel",
		"ipswest_sel",
		"tl_sel",
		"md_emi_sel",
		"sdf_sel",
		"uarthub_b_sel",
		"ssr_pka_sel",
		"ssr_dma_sel",
		"ssr_kdf_sel",
		"ssr_rng_sel",
		"ssr_sej_sel",
		"dxcc_sel",
		"dpsw_cmp_26m_sel",
		"smapck_sel",
		"apll_i2sin0_m_sel",
		"apll_i2sin1_m_sel",
		"apll_i2sin2_m_sel",
		"apll_i2sin3_m_sel",
		"apll_i2sin4_m_sel",
		"apll_i2sin6_m_sel",
		"apll_i2sout0_m_sel",
		"apll_i2sout1_m_sel",
		"apll_i2sout2_m_sel",
		"apll_i2sout3_m_sel",
		"apll_i2sout4_m_sel",
		"apll_i2sout6_m_sel",
		"apll_fmi2s_m_sel",
		"apll_tdmout_m_sel",
		"camtg1_sel",
		"camtg2_sel",
		"camtg3_sel",
		"camtg4_sel",
		"camtg5_sel",
		"camtg6_sel",
		"camtg7_sel",
		"seninf0_sel",
		"seninf1_sel",
		"seninf2_sel",
		"seninf3_sel",
		"seninf4_sel",
		"seninf5_sel",
		"ccu_ahb_sel",
		"img1_sel",
		"ipe_sel",
		"cam_sel",
		"camtm_sel",
		"dpe_sel",
		"vdec_sel",
		"ccusys_sel",
		"ccutm_sel",
		"venc_sel",
		"dp_core_sel",
		"dp_sel",
		"disp_sel",
		"mdp_sel",
		"mminfra_sel",
		"mmup_sel",

		/* topckgen */
		"apll12_div_i2sin0",
		"apll12_div_i2sin1",
		"apll12_div_i2sin2",
		"apll12_div_i2sin3",
		"apll12_div_i2sin4",
		"apll12_div_i2sin6",
		"apll12_div_i2sout0",
		"apll12_div_i2sout1",
		"apll12_div_i2sout2",
		"apll12_div_i2sout3",
		"apll12_div_i2sout4",
		"apll12_div_i2sout6",
		"apll12_div_fmi2s",
		"apll12_div_tdmout_m",
		"apll12_div_tdmout_b",

		/* infracfg_ao */
		"ifrao_dpmaif_main",
		"ifrao_dpmaif_26m",
		"ifrao_socsys_fatb",
		"ifrao_socsys_sdf",

		/* apmixedsys */
		"mainpll",
		"univpll",
		"msdcpll",
		"mmpll",
		"adsppll",
		"apll1",
		"apll2",
		"emipll",
		"emipll2",
		"mainpll2",
		"univpll2",
		"mmpll2",
		"tvdpll",
		"imgpll",

		/* pericfg_ao */
		"peraop_uart0",
		"peraop_uart1",
		"peraop_uart2",
		"peraop_uart3",
		"peraop_pwm_h",
		"peraop_pwm_b",
		"peraop_pwm_fb1",
		"peraop_pwm_fb2",
		"peraop_pwm_fb3",
		"peraop_pwm_fb4",
		"peraop_disp_pwm0",
		"peraop_disp_pwm1",
		"peraop_spi0_b",
		"peraop_spi1_b",
		"peraop_spi2_b",
		"peraop_spi3_b",
		"peraop_spi4_b",
		"peraop_spi5_b",
		"peraop_spi6_b",
		"peraop_spi7_b",
		"peraop_dma_b",
		"peraop_ssusb0_frmcnt",
		"peraop_ssusb1_frmcnt",
		"peraop_msdc1",
		"peraop_msdc1_f",
		"peraop_msdc1_h",
		"peraop_msdc2",
		"peraop_msdc2_f",
		"peraop_msdc2_h",
		"peraop_audio_slv",
		"peraop_audio_mst",
		"peraop_audio_intbus",
		"peraop_audio_engen1",
		"peraop_audio_engen2",
		"peraop_aud_tdmob",
		"peraop_audio_h_aud",

		/* afe */
		"afe_memif_intbus_clk",
		"afe_memif_hop_clk",
		"afe_pcm1",
		"afe_pcm0",
		"afe_soundwire",
		"afe_cm1",
		"afe_cm0",
		"afe_stf",
		"afe_hw_gain23",
		"afe_hw_gain01",
		"afe_daibt",
		"afe_merge_if",
		"afe_fm_i2s",
		"afe_ul2_aht",
		"afe_ul2_adc_hires",
		"afe_ul2_tml",
		"afe_ul2_adc",
		"afe_ul1_aht",
		"afe_ul1_adc_hires",
		"afe_ul1_tml",
		"afe_ul1_adc",
		"afe_ul0_aht",
		"afe_ul0_adc_hires",
		"afe_ul0_tml",
		"afe_ul0_adc",
		"afe_dprx_ck",
		"afe_dptx_ck",
		"afe_etdm_in6",
		"afe_etdm_in5",
		"afe_etdm_in4",
		"afe_etdm_in3",
		"afe_etdm_in2",
		"afe_etdm_in1",
		"afe_etdm_in0",
		"afe_etdm_out6",
		"afe_etdm_out5",
		"afe_etdm_out4",
		"afe_etdm_out3",
		"afe_etdm_out2",
		"afe_etdm_out1",
		"afe_etdm_out0",
		"afe_general24_asrc",
		"afe_general23_asrc",
		"afe_general22_asrc",
		"afe_general21_asrc",
		"afe_general20_asrc",
		"afe_general19_asrc",
		"afe_general18_asrc",
		"afe_general17_asrc",
		"afe_general16_asrc",
		"afe_general15_asrc",
		"afe_general14_asrc",
		"afe_general13_asrc",
		"afe_general12_asrc",
		"afe_general11_asrc",
		"afe_general10_asrc",
		"afe_general9_asrc",
		"afe_general8_asrc",
		"afe_general7_asrc",
		"afe_general6_asrc",
		"afe_general5_asrc",
		"afe_general4_asrc",
		"afe_general3_asrc",
		"afe_general2_asrc",
		"afe_general1_asrc",
		"afe_general0_asrc",
		"afe_connsys_i2s_asrc",
		"afe_audio_hopping_ck",
		"afe_audio_f26m_ck",
		"afe_apll1_ck",
		"afe_apll2_ck",
		"afe_h208m_ck",
		"afe_apll_tuner2",
		"afe_apll_tuner1",

		/* imp_iic_wrap_c */
		"impc_i2c10",
		"impc_i2c11",
		"impc_i2c12",
		"impc_i2c13",
		"impc_sec_en",

		/* ufscfg_ao */
		"ufsao_unipro_tx_sym",
		"ufsao_unipro_rx_sym0",
		"ufsao_unipro_rx_sym1",
		"ufsao_unipro_sys",
		"ufsao_unipro_phy_sap",
		"ufsao_p2ufs_bridge_h",

		/* ufscfg_pdn */
		"ufspdn_ufshci_ufs",
		"ufspdn_ufshci_aes",
		"ufspdn_ufshci_u_ahb",
		"ufspdn_ufshci_u_axi",

		/* pextpcfg_ao */
		"pextp_p0_mac_ref",
		"pextp_p0_mac_pl_p",
		"pextp_p0_mac_tl",
		"pextp_p0_mac_axi",
		"pextp_p0_mac_ahb_apb",
		"pextp_p0_phy_ref",
		"pextp_p0_phy_mcu_bus",
		"pextp_p1_mac_ref",
		"pextp_p1_mac_pl_p",
		"pextp_p1_mac_tl",
		"pextp_p1_mac_axi",
		"pextp_p1_mac_ahb_apb",
		"pextp_p1_phy_ref",
		"pextp_p1_phy_mcu_bus",

		/* imp_iic_wrap_s */
		"imps_i2c1",
		"imps_i2c3",
		"imps_i2c5",
		"imps_i2c9",
		"imps_sec_en",

		/* imp_iic_wrap_es */
		"impes_i2c2",
		"impes_i2c4",
		"impes_i2c7",
		"impes_i2c8",
		"impes_sec_en",

		/* imp_iic_wrap_n */
		"impn_i2c0",
		"impn_i2c6",
		"impn_sec_en",

		/* mfgpll_pll_ctrl */
		"mfg-ao-mfgpll",

		/* mfgpll_sc0_pll_ctrl */
		"mfgsc0-ao-mfgpll-sc0",

		/* mfgpll_sc1_pll_ctrl */
		"mfgsc1-ao-mfgpll-sc1",

		/* dispsys_config */
		"mm_config",
		"mm_disp_mutex0",
		"mm_disp_aal0",
		"mm_disp_aal1",
		"mm_disp_c3d0",
		"mm_disp_c3d1",
		"mm_disp_ccorr0",
		"mm_disp_ccorr1",
		"mm_disp_ccorr2",
		"mm_disp_ccorr3",
		"mm_disp_chist0",
		"mm_disp_chist1",
		"mm_disp_color0",
		"mm_disp_color1",
		"mm_disp_dither0",
		"mm_disp_dither1",
		"mm_disp_dither2",
		"mm_dli_async0",
		"mm_dli_async1",
		"mm_dli_async2",
		"mm_dli_async3",
		"mm_dli_async4",
		"mm_dli_async5",
		"mm_dli_async6",
		"mm_dli_async7",
		"mm_dlo_async0",
		"mm_dlo_async1",
		"mm_dlo_async2",
		"mm_dlo_async3",
		"mm_dlo_async4",
		"mm_disp_gamma0",
		"mm_disp_gamma1",
		"mm_mdp_aal0",
		"mm_mdp_rdma0",
		"mm_disp_oddmr0",
		"mm_disp_postalign0",
		"mm_disp_postmask0",
		"mm_disp_postmask1",
		"mm_disp_rsz0",
		"mm_disp_rsz1",
		"mm_disp_spr0",
		"mm_disp_tdshp0",
		"mm_disp_tdshp1",
		"mm_disp_wdma1",
		"mm_disp_y2r0",
		"mm_mdp_aal1",
		"mm_ssc",

		/* dispsys1_config */
		"mm1_dispsys1_config",
		"mm1_disp_mutex0",
		"mm1_disp_dli_async0",
		"mm1_disp_dli_async1",
		"mm1_disp_dli_async2",
		"mm1_mdp_rdma0",
		"mm1_disp_r2y0",
		"mm1_disp_splitter0",
		"mm1_disp_splitter1",
		"mm1_disp_vdcm0",
		"mm1_disp_dsc_wrap0",
		"mm1_disp_dsc_wrap1",
		"mm1_disp_dsc_wrap2",
		"mm1_DP_CLK",
		"mm1_CLK0",
		"mm1_CLK1",
		"mm1_CLK2",
		"mm1_disp_merge0",
		"mm1_disp_wdma0",
		"mm1_ssc",
		"mm1_disp_wdma1",
		"mm1_disp_wdma2",
		"mm1_disp_gdma0",
		"mm1_disp_dli_async3",
		"mm1_disp_dli_async4",
		"mm1_mod1",
		"mm1_mod2",
		"mm1_mod3",
		"mm1_mod4",
		"mm1_mod5",
		"mm1_mod6",
		"mm1_mod7",
		"mm1_subsys_ck",
		"mm1_dsi0_ck",
		"mm1_dsi1_ck",
		"mm1_dsi2_ck",
		"mm1_dp_ck",
		"mm1_f26m_ck",

		/* ovlsys_config */
		"ovlsys_config",
		"ovl_disp_fake_eng0",
		"ovl_disp_fake_eng1",
		"ovl_disp_mutex0",
		"ovl_disp_ovl0_2l",
		"ovl_disp_ovl1_2l",
		"ovl_disp_ovl2_2l",
		"ovl_disp_ovl3_2l",
		"ovl_disp_rsz1",
		"ovl_mdp_rsz0",
		"ovl_disp_wdma0",
		"ovl_disp_ufbc_wdma0",
		"ovl_disp_wdma2",
		"ovl_disp_dli_async0",
		"ovl_disp_dli_async1",
		"ovl_disp_dli_async2",
		"ovl_disp_dl0_async0",
		"ovl_disp_dl0_async1",
		"ovl_disp_dl0_async2",
		"ovl_disp_dl0_async3",
		"ovl_disp_dl0_async4",
		"ovl_disp_dl0_async5",
		"ovl_disp_dl0_async6",
		"ovl_inlinerot0",
		"ovl_ssc",
		"ovl_disp_y2r0",
		"ovl_disp_y2r1",
		"ovl_disp_ovl4_2l",

		/* ovlsys1_config */
		"ovl1_ovlsys_config",
		"ovl1_disp_fake_eng0",
		"ovl1_disp_fake_eng1",
		"ovl1_disp_mutex0",
		"ovl1_disp_ovl0_2l",
		"ovl1_disp_ovl1_2l",
		"ovl1_disp_ovl2_2l",
		"ovl1_disp_ovl3_2l",
		"ovl1_disp_rsz1",
		"ovl1_mdp_rsz0",
		"ovl1_disp_wdma0",
		"ovl1_disp_ufbc_wdma0",
		"ovl1_disp_wdma2",
		"ovl1_disp_dli_async0",
		"ovl1_disp_dli_async1",
		"ovl1_disp_dli_async2",
		"ovl1_disp_dl0_async0",
		"ovl1_disp_dl0_async1",
		"ovl1_disp_dl0_async2",
		"ovl1_disp_dl0_async3",
		"ovl1_disp_dl0_async4",
		"ovl1_disp_dl0_async5",
		"ovl1_disp_dl0_async6",
		"ovl1_inlinerot0",
		"ovl1_ssc",
		"ovl1_disp_y2r0",
		"ovl1_disp_y2r1",
		"ovl1_disp_ovl4_2l",

		/* imgsys_main */
		"img_fdvt",
		"img_me",
		"img_mmg",
		"img_larb12",
		"img_larb9",
		"img_traw0",
		"img_traw1",
		"img_dip0",
		"img_wpe0",
		"img_ipe",
		"img_wpe1",
		"img_wpe2",
		"img_adl_larb",
		"img_adlrd",
		"img_adlwr",
		"img_avs",
		"img_ips",
		"img_sub_common0",
		"img_sub_common1",
		"img_sub_common2",
		"img_sub_common3",
		"img_sub_common4",
		"img_gals_rx_dip0",
		"img_gals_rx_dip1",
		"img_gals_rx_traw0",
		"img_gals_rx_wpe0",
		"img_gals_rx_wpe1",
		"img_gals_rx_wpe2",
		"img_gals_rx_ipe0",
		"img_gals_tx_ipe0",
		"img_gals_rx_ipe1",
		"img_gals_tx_ipe1",
		"img_gals",

		/* dip_top_dip1 */
		"dip_dip1_dip_top",
		"dip_dip1_dip_gals0",
		"dip_dip1_dip_gals1",
		"dip_dip1_dip_gals2",
		"dip_dip1_dip_gals3",
		"dip_dip1_larb10",
		"dip_dip1_larb15",
		"dip_dip1_larb38",
		"dip_dip1_larb39",

		/* dip_nr1_dip1 */
		"dip_nr1_dip1_larb",
		"dip_nr1_dip1_dip_nr1",

		/* dip_nr2_dip1 */
		"dip_nr2_dip1_larb15",
		"dip_nr2_dip1_dip_nr",
		"dip_nr2_dip1_larb39",

		/* wpe1_dip1 */
		"wpe1_dip1_larb11",
		"wpe1_dip1_wpe",
		"wpe1_dip1_gals0",

		/* wpe2_dip1 */
		"wpe2_dip1_larb11",
		"wpe2_dip1_wpe",
		"wpe2_dip1_gals0",

		/* wpe3_dip1 */
		"wpe3_dip1_larb11",
		"wpe3_dip1_wpe",
		"wpe3_dip1_gals0",

		/* traw_dip1 */
		"traw_dip1_larb28",
		"traw_dip1_larb40",
		"traw_dip1_traw",
		"traw_dip1_gals",

		/* traw_cap_dip1 */
		"traw__dip1_cap",

		/* img_vcore_d1a */
		"img_vcore_gals_disp",
		"img_vcore_main",
		"img_vcore_sub0",
		"img_vcore_sub1",

		/* vdec_soc_gcon_base */
		"vde1_lat_cken",
		"vde1_lat_active",
		"vde1_lat_cken_eng",
		"vde1_vdec_cken",
		"vde1_vdec_active",
		"vde1_vdec_cken_eng",
		"vde1_aptv_en",
		"vde1_aptv_topen",
		"vde1_vdec_soc_ips_en",

		/* vdec_gcon_base */
		"vde2_lat_cken",
		"vde2_lat_active",
		"vde2_lat_cken_eng",
		"vde2_vdec_cken",
		"vde2_vdec_active",
		"vde2_vdec_cken_eng",

		/* venc_gcon */
		"ven1_larb",
		"ven1_venc",
		"ven1_jpgenc",
		"ven1_jpgdec",
		"ven1_jpgdec_c1",
		"ven1_gals",
		"ven1_gals_sram",

		/* venc_gcon_core1 */
		"ven2_larb",
		"ven2_venc",
		"ven2_jpgenc",
		"ven2_jpgdec",
		"ven2_gals",
		"ven2_gals_sram",

		/* venc_gcon_core2 */
		"ven_c2_larb",
		"ven_c2_venc",
		"ven_c2_jpgenc",
		"ven_c2_gals",
		"ven_c2_gals_sram",

		/* vlp_cksys */
		"vlp_scp_sel",
		"vlp_scp_spi_sel",
		"vlp_scp_iic_sel",
		"vlp_scp_spi_hs_sel",
		"vlp_scp_iic_hs_sel",
		"vlp_pwrap_ulposc_sel",
		"vlp_spmi_32ksel",
		"vlp_apxgpt_26m_b_sel",
		"vlp_dpsw_sel",
		"vlp_spmi_m_sel",
		"vlp_dvfsrc_sel",
		"vlp_pwm_vlp_sel",
		"vlp_axi_vlp_sel",
		"vlp_systimer_26m_sel",
		"vlp_sspm_sel",
		"vlp_srck_sel",
		"vlp_camtg0_sel",
		"vlp_ips_sel",
		"vlp_sspm_26m_sel",
		"vlp_ulposc_sspm_sel",

		/* scp */
		"scp_set_spi0",
		"scp_set_spi1",
		"scp_set_spi2",
		"scp_set_spi3",

		/* scp_iic */
		"scp_iic_i2c0",
		"scp_iic_i2c1",
		"scp_iic_i2c2",
		"scp_iic_i2c3",
		"scp_iic_i2c4",
		"scp_iic_i2c5",
		"scp_iic_i2c6",
		"scp_iic_i2c7",

		/* scp_fast_iic */
		"scp_fast_iic_i2c0",

		/* camsys_main */
		"cam_m_larb13",
		"cam_m_larb14",
		"cam_m_larb27",
		"cam_m_larb29",
		"cam_m_cam",
		"cam_m_cam_suba",
		"cam_m_cam_subb",
		"cam_m_cam_subc",
		"cam_m_cam_mraw",
		"cam_m_camtg",
		"cam_m_seninf",
		"cam_m_camsv",
		"cam_m_adlrd",
		"cam_m_adlwr",
		"cam_m_uisp",
		"cam_m_cam2mm0_GCON_0",
		"cam_m_cam2mm1_GCON_0",
		"cam_m_cam2sys_GCON_0",
		"cam_m_cam2mm2_GCON_0",
		"cam_m_ccusys",
		"cam_m_cam_dpe",
		"cam_m_cam_asg",
		"cam_m_camsv_a_con_1",
		"cam_m_camsv_b_con_1",
		"cam_m_camsv_c_con_1",
		"cam_m_camsv_d_con_1",
		"cam_m_camsv_e_con_1",
		"cam_m_camsv_con_1",

		/* camsys_rmsa */
		"camsys_rmsa_larbx",
		"camsys_rmsa_cam",
		"camsys_rmsa_camtg",

		/* camsys_rawa */
		"cam_ra_larbx",
		"cam_ra_cam",
		"cam_ra_camtg",

		/* camsys_yuva */
		"cam_ya_larbx",
		"cam_ya_cam",
		"cam_ya_camtg",

		/* camsys_rmsb */
		"camsys_rmsb_larbx",
		"camsys_rmsb_cam",
		"camsys_rmsb_camtg",

		/* camsys_rawb */
		"cam_rb_larbx",
		"cam_rb_cam",
		"cam_rb_camtg",

		/* camsys_yuvb */
		"cam_yb_larbx",
		"cam_yb_cam",
		"cam_yb_camtg",

		/* camsys_rmsc */
		"camsys_rmsc_larbx",
		"camsys_rmsc_cam",
		"camsys_rmsc_camtg",

		/* camsys_rawc */
		"cam_rc_larbx",
		"cam_rc_cam",
		"cam_rc_camtg",

		/* camsys_yuvc */
		"cam_yc_larbx",
		"cam_yc_cam",
		"cam_yc_camtg",

		/* camsys_mraw */
		"cam_mr_larbx",
		"cam_mr_gals",
		"cam_mr_camtg",
		"cam_mr_mraw0",
		"cam_mr_mraw1",
		"cam_mr_mraw2",
		"cam_mr_mraw3",
		"cam_mr_pda0",
		"cam_mr_pda1",

		/* camsys_ipe */
		"camsys_ipe_larb19",
		"camsys_ipe_dpe",
		"camsys_ipe_fus",
		"camsys_ipe_dhze",
		"camsys_ipe_gals",

		/* ccu_main */
		"ccu_larb30_con",
		"ccu_ahb_con",
		"ccusys_ccu0_con",
		"ccusys_ccu1_con",
		"ccu2mm0_GCON",

		/* cam_vcore */
		"CAM_V_MM0_SUBCOMM",

		/* mminfra_config */
		"mminfra_smi",

		/* mminfra_ao_config */
		"mminfra_ao_gce_d",
		"mminfra_ao_gce_m",
		"mminfra_ao_gce_26m",

		/* mdpsys_config */
		"mdp_mutex0",
		"mdp_apb_bus",
		"mdp_smi0",
		"mdp_rdma0",
		"mdp_rdma2",
		"mdp_hdr0",
		"mdp_aal0",
		"mdp_rsz0",
		"mdp_tdshp0",
		"mdp_color0",
		"mdp_wrot0",
		"mdp_fake_eng0",
		"mdp_dli_async0",
		"mdp_dli_async1",
		"mdp_apb_db",
		"mdp_rdma1",
		"mdp_rdma3",
		"mdp_hdr1",
		"mdp_aal1",
		"mdp_rsz1",
		"mdp_tdshp1",
		"mdp_color1",
		"mdp_wrot1",
		"mdp_rsz2",
		"mdp_wrot2",
		"mdp_dlo_async0",
		"mdp_rsz3",
		"mdp_wrot3",
		"mdp_dlo_async1",
		"mdp_dli_async2",
		"mdp_dli_async3",
		"mdp_dlo_async2",
		"mdp_dlo_async3",
		"mdp_birsz0",
		"mdp_birsz1",
		"mdp_img_dl_async0",
		"mdp_img_dl_async1",
		"mdp_hre_mdpsys",
		"mdp_rrot0",
		"mdp_rrot0_2nd",
		"mdp_merge0",
		"mdp_c3d0",
		"mdp_fg0",
		"mdp_img_dl_relay0",
		"mdp_f26m_slow_ck",
		"mdp_f32k_slow_ck",
		"mdp_img_dl_relay1",

		/* mdpsys1_config */
		"mdp1_mdp_mutex0",
		"mdp1_apb_bus",
		"mdp1_smi0",
		"mdp1_mdp_rdma0",
		"mdp1_mdp_rdma2",
		"mdp1_mdp_hdr0",
		"mdp1_mdp_aal0",
		"mdp1_mdp_rsz0",
		"mdp1_mdp_tdshp0",
		"mdp1_mdp_color0",
		"mdp1_mdp_wrot0",
		"mdp1_mdp_fake_eng0",
		"mdp1_mdp_dli_async0",
		"mdp1_mdp_dli_async1",
		"mdp1_apb_db",
		"mdp1_mdp_rdma1",
		"mdp1_mdp_rdma3",
		"mdp1_mdp_hdr1",
		"mdp1_mdp_aal1",
		"mdp1_mdp_rsz1",
		"mdp1_mdp_tdshp1",
		"mdp1_mdp_color1",
		"mdp1_mdp_wrot1",
		"mdp1_mdp_rsz2",
		"mdp1_mdp_wrot2",
		"mdp1_mdp_dlo_async0",
		"mdp1_mdp_rsz3",
		"mdp1_mdp_wrot3",
		"mdp1_mdp_dlo_async1",
		"mdp1_mdp_dli_async2",
		"mdp1_mdp_dli_async3",
		"mdp1_mdp_dlo_async2",
		"mdp1_mdp_dlo_async3",
		"mdp1_mdp_birsz0",
		"mdp1_mdp_birsz1",
		"mdp1_img_dl_async0",
		"mdp1_img_dl_async1",
		"mdp1_hre_mdpsys",
		"mdp1_mdp_rrot0",
		"mdp1_mdp_rrot0_2nd",
		"mdp1_mdp_merge0",
		"mdp1_mdp_c3d0",
		"mdp1_mdp_fg0",
		"mdp1_img_dl_relay0",
		"mdp1_f26m_slow_ck",
		"mdp1_f32k_slow_ck",
		"mdp1_img_dl_relay1",

		/* ccipll_pll_ctrl */
		"ccipll",

		/* armpll_ll_pll_ctrl */
		"cpu-ll-armpll-ll",

		/* armpll_bl_pll_ctrl */
		"cpu-bl-armpll-bl",

		/* armpll_b_pll_ctrl */
		"cpu-b-armpll-b",

		/* ptppll_pll_ctrl */
		"ptppll",


	};

	return clks;
}

/*
 * clkdbg test tasks
 */
static struct task_struct *clkdbg_test_thread;

static int clkdbg_thread_fn(void *data)
{
	while (!kthread_should_stop()) {
		pr_info("clkdbg_thread is running...\n");
		ssleep(10); /* 10s */
	}
	return 0;
}

static int start_clkdbg_test_task(void)
{
	if (clkdbg_test_thread) {
		pr_info("clkdbg_thread is already running\n");
		return -EBUSY;
	}

	clkdbg_test_thread = kthread_run(clkdbg_thread_fn, NULL, "clkdbg_thread");
	if (IS_ERR(clkdbg_test_thread)) {
		pr_info("Failed to start clkdbg_thread\n");
		return PTR_ERR(clkdbg_test_thread);
	}
	return 0;
}

static void stop_clkdbg_test_task(void)
{
	if (clkdbg_test_thread) {
		kthread_stop(clkdbg_test_thread);
		clkdbg_test_thread = NULL;
	}
}

/*
 * clkdbg dump all fmeter clks
 */
static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return mt_get_fmeter_clks();
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	return mt_get_fmeter_freq(fclk->id, fclk->type);
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6989_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6989_all_clk_names,
	.start_task = start_clkdbg_test_task,
};

static int clk_dbg_mt6989_probe(struct platform_device *pdev)
{
	set_clkdbg_ops(&clkdbg_mt6989_ops);

	return 0;
}

static struct platform_driver clk_dbg_mt6989_drv = {
	.probe = clk_dbg_mt6989_probe,
	.driver = {
		.name = "clk-dbg-mt6989",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */

static int __init clkdbg_mt6989_init(void)
{
	return clk_dbg_driver_register(&clk_dbg_mt6989_drv, "clk-dbg-mt6989");
}

static void __exit clkdbg_mt6989_exit(void)
{
	unset_clkdbg_ops();
	stop_clkdbg_test_task();
	platform_driver_unregister(&clk_dbg_mt6989_drv);
}

subsys_initcall(clkdbg_mt6989_init);
module_exit(clkdbg_mt6989_exit);
MODULE_LICENSE("GPL");
