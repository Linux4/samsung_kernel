#include "../cmucal.h"
#include "cmucal-vclklut.h"


/* DVFS VCLK -> LUT Parameter List */
unsigned int vdd_mif_nm_lut_params[] = {
	4266000,
};
unsigned int vdd_mif_ud_lut_params[] = {
	2666000,
};
unsigned int vdd_mif_sud_lut_params[] = {
	1400000,
};
unsigned int vdd_cam_nm_lut_params[] = {
	1200000, 0, 1,
};
unsigned int vdd_cam_sud_lut_params[] = {
	1200000, 2, 5,
};
unsigned int vdd_cam_ud_lut_params[] = {
	1200000, 0, 2,
};
unsigned int vdd_cpucl0_sod_lut_params[] = {
	1800000, 1,
};
unsigned int vdd_cpucl0_od_lut_params[] = {
	1500000, 1,
};
unsigned int vdd_cpucl0_nm_lut_params[] = {
	1100000, 0,
};
unsigned int vdd_cpucl0_ud_lut_params[] = {
	650000, 0,
};
unsigned int vdd_cpucl0_sud_lut_params[] = {
	350000, 0,
};
unsigned int vddi_od_lut_params[] = {
	0, 1, 950000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
};
unsigned int vddi_nm_lut_params[] = {
	3, 0, 860000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,
};
unsigned int vddi_ud_lut_params[] = {
	2, 1, 650000, 0, 3, 0, 3, 0, 0, 0, 3, 0, 3, 2, 2, 0, 0, 1, 1, 0, 0, 3, 1, 2, 0, 2, 2, 3, 2, 0, 0, 1, 2, 0, 2, 0, 3, 0, 2, 0, 1, 0, 3, 0, 3, 0, 0, 1, 1, 9, 0, 1, 2, 0, 2, 0, 1, 3, 0, 2, 0, 0,
};
unsigned int vddi_sud_lut_params[] = {
	2, 3, 300000, 1, 7, 1, 3, 1, 1, 1, 3, 1, 3, 2, 1, 1, 2, 1, 1, 3, 1, 3, 1, 2, 1, 2, 2, 2, 1, 1, 3, 1, 2, 1, 2, 1, 2, 1, 2, 1, 1, 3, 3, 1, 7, 1, 3, 1, 3, 5, 3, 1, 2, 1, 1, 1, 3, 3, 1, 2, 1, 2,
};

/* SPECIAL VCLK -> LUT Parameter List */
unsigned int mux_clk_apm_i3c_pmic_nm_lut_params[] = {
	0, 1,
};
unsigned int clkcmu_apm_bus_ud_lut_params[] = {
	0, 0,
};
unsigned int clkcmu_apm_bus_od_lut_params[] = {
	0, 0,
};
unsigned int mux_busc_cmuref_ud_lut_params[] = {
	0,
};
unsigned int clkcmu_cmu_boost_ud_lut_params[] = {
	1, 0,
};
unsigned int clkcmu_cmu_boost_od_lut_params[] = {
	1, 0,
};
unsigned int mux_clk_chub_timer_fclk_nm_lut_params[] = {
	0,
};
unsigned int mux_clk_cmgp_adc_nm_lut_params[] = {
	0, 13,
};
unsigned int clkcmu_cmgp_bus_nm_lut_params[] = {
	0, 1,
};
unsigned int mux_cmu_cmuref_ud_lut_params[] = {
	0,
};
unsigned int mux_core_cmuref_ud_lut_params[] = {
	1,
};
unsigned int mux_cpucl0_cmuref_ud_lut_params[] = {
	1,
};
unsigned int mux_cpucl1_cmuref_ud_lut_params[] = {
	1,
};
unsigned int mux_mif_cmuref_ud_lut_params[] = {
	1,
};
unsigned int mux_clkcmu_peri_mmc_card_ud_lut_params[] = {
	2,
};
unsigned int mux_clkcmu_usb_usbdp_debug_ud_lut_params[] = {
	1,
};
unsigned int mux_clkcmu_usb_usbdp_debug_od_lut_params[] = {
	1,
};
unsigned int clkcmu_usb_dpgtc_ud_lut_params[] = {
	2, 2,
};
unsigned int clkcmu_usb_dpgtc_od_lut_params[] = {
	2, 2,
};
unsigned int div_clk_apm_dbgcore_uart_nm_lut_params[] = {
	1, 1,
};
unsigned int div_clk_aud_cpu_pclkdbg_nm_lut_params[] = {
	7, 15, 0, 15, 0, 15, 0, 24, 0, 3, 24, 0, 15, 0, 0, 1, 0, 15, 0, 15, 0, 7,
};
unsigned int div_clk_aud_cpu_pclkdbg_ud_lut_params[] = {
	7, 15, 0, 15, 0, 15, 0, 24, 0, 3, 24, 0, 15, 0, 0, 1, 0, 15, 0, 15, 0, 7,
};
unsigned int div_clk_aud_mclk_ud_lut_params[] = {
	7,
};
unsigned int div_clk_chub_usi0_nm_lut_params[] = {
	0, 1,
};
unsigned int clkcmu_chub_bus_nm_lut_params[] = {
	0, 1,
};
unsigned int div_clk_chub_usi1_nm_lut_params[] = {
	0, 1,
};
unsigned int div_clk_chub_usi2_nm_lut_params[] = {
	0, 1,
};
unsigned int div_clk_usi_cmgp1_nm_lut_params[] = {
	0, 1,
};
unsigned int div_clk_usi_cmgp0_nm_lut_params[] = {
	0, 1,
};
unsigned int div_clk_usi_cmgp2_nm_lut_params[] = {
	0, 1,
};
unsigned int div_clk_usi_cmgp3_nm_lut_params[] = {
	0, 1,
};
unsigned int div_clk_i3c_cmgp_nm_lut_params[] = {
	1, 1,
};
unsigned int clkcmu_hpm_ud_lut_params[] = {
	0, 2,
};
unsigned int clkcmu_hpm_od_lut_params[] = {
	0, 2,
};
unsigned int clkcmu_cis_clk0_ud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk0_od_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk1_ud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk1_od_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk2_ud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk2_od_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk3_ud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk3_od_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk4_ud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk4_od_lut_params[] = {
	3, 1,
};
unsigned int div_clk_cpucl0_cmuref_sod_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl0_cmuref_od_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl0_cmuref_nm_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl0_cmuref_ud_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl0_cmuref_sud_lut_params[] = {
	1,
};
unsigned int div_clk_cluster0_atclk_sod_lut_params[] = {
	3, 3,
};
unsigned int div_clk_cluster0_atclk_od_lut_params[] = {
	3, 3,
};
unsigned int div_clk_cluster0_atclk_nm_lut_params[] = {
	3, 3,
};
unsigned int div_clk_cluster0_atclk_ud_lut_params[] = {
	3, 3,
};
unsigned int div_clk_cluster0_atclk_sud_lut_params[] = {
	3, 3,
};
unsigned int div_clk_cpucl1_cmuref_sod_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_cmuref_od_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_cmuref_nm_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_cmuref_ud_lut_params[] = {
	1,
};
unsigned int div_clk_peri_usi00_usi_ud_lut_params[] = {
	0,
};
unsigned int clkcmu_peri_ip_ud_lut_params[] = {
	0, 0,
};
unsigned int clkcmu_peri_ip_od_lut_params[] = {
	0, 0,
};
unsigned int div_clk_peri_usi01_usi_ud_lut_params[] = {
	0,
};
unsigned int div_clk_peri_usi02_usi_ud_lut_params[] = {
	0,
};
unsigned int div_clk_peri_usi03_usi_ud_lut_params[] = {
	0,
};
unsigned int div_clk_peri_usi04_usi_ud_lut_params[] = {
	0,
};
unsigned int div_clk_peri_usi05_usi_ud_lut_params[] = {
	0,
};
unsigned int div_clk_peri_uart_dbg_ud_lut_params[] = {
	1,
};
unsigned int div_clk_peri_spi_ois_ud_lut_params[] = {
	0,
};
unsigned int mux_clkcmu_ap2gnss_nm_lut_params[] = {
	0,
};

/* COMMON VCLK -> LUT Parameter List */
unsigned int blk_cmu_od_lut_params[] = {
	1333000, 1599000, 808000, 2, 0, 0, 1, 3, 2, 2, 0, 1, 1, 1, 1, 1, 1, 2, 2, 1, 3,
};
unsigned int blk_cmu_ud_lut_params[] = {
	1333000, 1600000, 808000, 2, 0, 0, 1, 3, 2, 2, 0, 1, 1, 1, 1, 1, 1, 2, 2, 1, 3,
};
unsigned int blk_cpucl1_sod_lut_params[] = {
	2400000, 0,
};
unsigned int blk_cpucl1_od_lut_params[] = {
	1900000, 0,
};
unsigned int blk_cpucl1_nm_lut_params[] = {
	1400000, 0,
};
unsigned int blk_cpucl1_ud_lut_params[] = {
	850000, 0,
};
unsigned int blk_s2d_nm_lut_params[] = {
	400000, 1, 0,
};
unsigned int blk_apm_nm_lut_params[] = {
	1, 1, 0, 0,
};
unsigned int blk_chub_nm_lut_params[] = {
	1, 1,
};
unsigned int blk_cmgp_nm_lut_params[] = {
	1, 0,
};
unsigned int blk_core_ud_lut_params[] = {
	0, 1,
};
unsigned int blk_cpucl0_sod_lut_params[] = {
	0, 3, 1, 7,
};
unsigned int blk_cpucl0_od_lut_params[] = {
	0, 3, 1, 7,
};
unsigned int blk_cpucl0_nm_lut_params[] = {
	0, 3, 1, 7,
};
unsigned int blk_cpucl0_ud_lut_params[] = {
	0, 3, 1, 7,
};
unsigned int blk_g3d_ud_lut_params[] = {
	0, 3,
};
unsigned int blk_vts_nm_lut_params[] = {
	1, 0, 1, 0, 0,
};
unsigned int blk_aud_nm_lut_params[] = {
	2, 1, 2,
};
unsigned int blk_aud_ud_lut_params[] = {
	2, 1, 2,
};
unsigned int blk_busc_ud_lut_params[] = {
	1,
};
unsigned int blk_csis_ud_lut_params[] = {
	1,
};
unsigned int blk_dnc_ud_lut_params[] = {
	3,
};
unsigned int blk_dns_ud_lut_params[] = {
	1,
};
unsigned int blk_dpu_ud_lut_params[] = {
	2,
};
unsigned int blk_dsp_ud_lut_params[] = {
	1,
};
unsigned int blk_g2d_ud_lut_params[] = {
	1,
};
unsigned int blk_ipp_ud_lut_params[] = {
	1,
};
unsigned int blk_itp_ud_lut_params[] = {
	1,
};
unsigned int blk_mcsc_ud_lut_params[] = {
	1,
};
unsigned int blk_mfc_ud_lut_params[] = {
	3,
};
unsigned int blk_npu_ud_lut_params[] = {
	3,
};
unsigned int blk_peri_ud_lut_params[] = {
	1,
};
unsigned int blk_ssp_ud_lut_params[] = {
	1,
};
unsigned int blk_tnr_ud_lut_params[] = {
	1,
};
unsigned int blk_vra_ud_lut_params[] = {
	1,
};

/* parent clock source: 400Mhz */
unsigned int div_clk_200_lut_params[] = {
	1, 1,
};
unsigned int div_clk_100_lut_params[] = {
	3, 1,
};
unsigned int div_clk_50_lut_params[] = {
	7, 1,
};
unsigned int div_clk_25_lut_params[] = {
	15, 1,
};
unsigned int div_clk_26_lut_params[] = {
	0, 0,
};
unsigned int div_clk_13_lut_params[] = {
	1, 0,
};
unsigned int div_clk_8_lut_params[] = {
	2, 0,
};
unsigned int div_clk_6_lut_params[] = {
	3, 0,
};
