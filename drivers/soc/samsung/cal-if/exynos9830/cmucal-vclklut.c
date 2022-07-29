#include "../cmucal.h"
#include "cmucal-vclklut.h"


/* DVFS VCLK -> LUT Parameter List */
unsigned int vddi_nm_lut_params[] = {
	1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 3, 1, 0, 0, 2, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 900000, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 7, 7, 2, 1, 7, 1, 2, 0, 0, 0, 0, 0, 0, 3, 1,
};
unsigned int vddi_od_lut_params[] = {
	0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 3, 1, 0, 0, 2, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 900000, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 7, 7, 2, 1, 7, 1, 2, 0, 0, 0, 0, 0, 0, 3, 1,
};
unsigned int vddi_ud_lut_params[] = {
	3, 0, 4, 3, 1, 1, 0, 3, 0, 0, 0, 2, 0, 3, 1, 0, 0, 4, 0, 0, 4, 0, 1, 0, 1, 3, 1, 0, 0, 4, 0, 3, 1, 0, 3, 1, 0, 2, 2, 2, 0, 750000, 1, 2, 0, 1, 3, 0, 3, 0, 4, 0, 0, 4, 4, 0, 3, 0, 1, 0, 4, 0, 1, 2, 1, 7, 7, 2, 1, 7, 1, 2, 0, 0, 0, 0, 0, 0, 3, 1,
};
unsigned int vddi_sud_lut_params[] = {
	3, 2, 3, 3, 2, 3, 2, 3, 1, 1, 1, 2, 1, 3, 1, 1, 2, 0, 2, 2, 0, 1, 1, 1, 1, 2, 2, 1, 1, 4, 3, 3, 3, 2, 3, 0, 2, 3, 2, 1, 1, 450000, 3, 2, 1, 3, 0, 2, 3, 1, 0, 2, 2, 0, 0, 2, 3, 1, 3, 1, 0, 2, 3, 0, 0, 7, 7, 2, 1, 7, 1, 2, 0, 0, 0, 0, 0, 0, 3, 1,
};
unsigned int vddi_uud_lut_params[] = {
	3, 2, 3, 3, 5, 15, 10, 3, 7, 7, 1, 2, 7, 3, 4, 3, 5, 0, 11, 7, 2, 7, 1, 3, 1, 2, 2, 3, 1, 6, 3, 3, 15, 2, 7, 1, 7, 3, 2, 1, 7, 170000, 1, 2, 7, 15, 0, 11, 3, 1, 2, 7, 7, 2, 2, 7, 3, 1, 0, 7, 2, 7, 15, 0, 0, 15, 15, 7, 2, 15, 2, 7, 1, 1, 1, 3, 3, 1, 15, 3,
};
unsigned int vdd_mif_od_lut_params[] = {
	5500000, 5500000,
};
unsigned int vdd_mif_nm_lut_params[] = {
	4266000, 4266000,
};
unsigned int vdd_mif_ud_lut_params[] = {
	2688000, 2688000,
};
unsigned int vdd_mif_sud_lut_params[] = {
	1420000, 1420000,
};
unsigned int vdd_mif_uud_lut_params[] = {
	710000, 710000,
};
unsigned int vdd_cam_nm_lut_params[] = {
	1, 1179648, 67738,
};
unsigned int vdd_cam_sud_lut_params[] = {
	5, 1179648, 67738,
};
unsigned int vdd_cam_ud_lut_params[] = {
	3, 1179648, 67738,
};
unsigned int vdd_cam_uud_lut_params[] = {
	1, 300000, 25000,
};
unsigned int vdd_cpucl0_sod_lut_params[] = {
	2000000, 1,
};
unsigned int vdd_cpucl0_od_lut_params[] = {
	1650000, 1,
};
unsigned int vdd_cpucl0_nm_lut_params[] = {
	1200000, 0,
};
unsigned int vdd_cpucl0_ud_lut_params[] = {
	800000, 0,
};
unsigned int vdd_cpucl0_sud_lut_params[] = {
	480000, 0,
};
unsigned int vdd_cpucl0_uud_lut_params[] = {
	175000, 0,
};
unsigned int vdd_cpucl1_sod_lut_params[] = {
	2600000,
};
unsigned int vdd_cpucl1_od_lut_params[] = {
	1950000,
};
unsigned int vdd_cpucl1_nm_lut_params[] = {
	1550000,
};
unsigned int vdd_cpucl1_ud_lut_params[] = {
	1000000,
};
unsigned int vdd_cpucl1_sud_lut_params[] = {
	600000,
};
unsigned int vdd_cpucl1_uud_lut_params[] = {
	300000,
};

/* SPECIAL VCLK -> LUT Parameter List */
unsigned int mux_clk_apm_i3c_pmic_nm_lut_params[] = {
	0, 1,
};
unsigned int clkcmu_apm_bus_uud_lut_params[] = {
	0, 1,
};
unsigned int mux_clk_apm_i3c_cp_nm_lut_params[] = {
	0, 1,
};
unsigned int mux_clk_aud_dsif_od_lut_params[] = {
	0, 2, 7, 0, 7, 0, 2, 7, 0, 7, 0, 7, 0, 1, 7, 0, 7, 0, 7, 0, 7, 7, 0,
};
unsigned int mux_clk_aud_dsif_uud_lut_params[] = {
	0, 2, 7, 0, 7, 0, 2, 7, 0, 7, 0, 7, 0, 1, 7, 0, 7, 0, 7, 0, 7, 7, 0,
};
unsigned int mux_bus0_cmuref_nm_lut_params[] = {
	1,
};
unsigned int clkcmu_cmu_boost_uud_lut_params[] = {
	1, 2,
};
unsigned int mux_bus1_cmuref_nm_lut_params[] = {
	1,
};
unsigned int mux_clk_cmgp_adc_nm_lut_params[] = {
	0, 13,
};
unsigned int clkcmu_cmgp_bus_nm_lut_params[] = {
	0, 0,
};
unsigned int mux_cmu_cmuref_od_lut_params[] = {
	1,
};
unsigned int mux_cmu_cmuref_ud_lut_params[] = {
	1,
};
unsigned int mux_cmu_cmuref_sud_lut_params[] = {
	1,
};
unsigned int mux_cmu_cmuref_uud_lut_params[] = {
	1,
};
unsigned int mux_core_cmuref_nm_lut_params[] = {
	1,
};
unsigned int mux_cpucl0_cmuref_uud_lut_params[] = {
	1,
};
unsigned int mux_clkcmu_cmu_boost_cpu_uud_lut_params[] = {
	2,
};
unsigned int mux_cpucl1_cmuref_uud_lut_params[] = {
	1,
};
unsigned int mux_cpucl2_cmuref_usod_lut_params[] = {
	1,
};
unsigned int mux_mif_cmuref_uud_lut_params[] = {
	1,
};
unsigned int mux_mif_cmuref_od_lut_params[] = {
	1,
};
unsigned int mux_mif1_cmuref_uud_lut_params[] = {
	0,
};
unsigned int mux_mif2_cmuref_uud_lut_params[] = {
	0,
};
unsigned int mux_mif3_cmuref_uud_lut_params[] = {
	0,
};
unsigned int mux_clkcmu_hsi0_usbdp_debug_uud_lut_params[] = {
	1,
};
unsigned int mux_clkcmu_hsi1_mmc_card_uud_lut_params[] = {
	2,
};
unsigned int mux_clkcmu_hsi2_pcie_uud_lut_params[] = {
	1,
};
unsigned int div_clk_i2c_cmgp0_nm_lut_params[] = {
	1, 1,
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
unsigned int div_clk_i2c_cmgp1_nm_lut_params[] = {
	1, 1,
};
unsigned int div_clk_i2c_cmgp2_nm_lut_params[] = {
	1, 1,
};
unsigned int div_clk_i2c_cmgp3_nm_lut_params[] = {
	1, 1,
};
unsigned int div_clk_dbgcore_uart_cmgp_nm_lut_params[] = {
	1, 1,
};
unsigned int clkcmu_cpucl2_switch_ud_lut_params[] = {
	0,
};
unsigned int clkcmu_cpucl2_switch_uud_lut_params[] = {
	0,
};
unsigned int clkcmu_hpm_uud_lut_params[] = {
	0, 2,
};
unsigned int clkcmu_cis_clk0_uud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk1_uud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk2_uud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk3_uud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk4_uud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk5_uud_lut_params[] = {
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
unsigned int div_clk_cpucl0_cmuref_uud_lut_params[] = {
	1,
};
unsigned int div_clk_cluster0_periphclk_sod_lut_params[] = {
	1,
};
unsigned int div_clk_cluster0_periphclk_od_lut_params[] = {
	1,
};
unsigned int div_clk_cluster0_periphclk_nm_lut_params[] = {
	1,
};
unsigned int div_clk_cluster0_periphclk_ud_lut_params[] = {
	1,
};
unsigned int div_clk_cluster0_periphclk_sud_lut_params[] = {
	1,
};
unsigned int div_clk_cluster0_periphclk_uud_lut_params[] = {
	1,
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
unsigned int div_clk_cpucl1_cmuref_sud_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_cmuref_uud_lut_params[] = {
	1,
};
unsigned int div_clk_dsp1_busp_nm_lut_params[] = {
	1,
};
unsigned int div_clk_dsp2_busp_nm_lut_params[] = {
	1,
};
unsigned int div_clk_npu10_busp_nm_lut_params[] = {
	0,
};
unsigned int div_clk_npu11_busp_nm_lut_params[] = {
	0,
};
unsigned int div_clk_peric0_usi00_usi_nm_lut_params[] = {
	0, 0, 0, 0, 0, 0, 1,
};
unsigned int mux_clkcmu_peric0_ip_uud_lut_params[] = {
	1,
};
unsigned int div_clk_peric0_usi13_usi_nm_lut_params[] = {
	0, 0, 0,
};
unsigned int div_clk_peric1_uart_bt_nm_lut_params[] = {
	1, 0, 0, 0,
};
unsigned int mux_clkcmu_peric1_ip_uud_lut_params[] = {
	1,
};
unsigned int div_clk_peric1_usi18_usi_nm_lut_params[] = {
	0, 0, 0, 0, 0, 0, 0,
};
unsigned int div_clk_vts_dmic_if_pad_nm_lut_params[] = {
	0, 0,
};
unsigned int div_clk_aud_dmic0_ud_lut_params[] = {
	3,
};
unsigned int div_clk_aud_dmic0_uud_lut_params[] = {
	3,
};
unsigned int div_clk_aud_dmic1_ud_lut_params[] = {
	0,
};
unsigned int div_clk_aud_dmic1_uud_lut_params[] = {
	0,
};
/* parent clock source(DIV_CLKCMU_PERICx_IP): 400Mhz */
unsigned int div_clk_peric_400_lut_params[] = {
	0, 1,
};
unsigned int div_clk_peric_200_lut_params[] = {
	1, 1,
};
unsigned int div_clk_peric_133_lut_params[] = {
	2, 1,
};
unsigned int div_clk_peric_100_lut_params[] = {
	3, 1,
};
unsigned int div_clk_peric_66_lut_params[] = {
	5, 1,
};
unsigned int div_clk_peric_50_lut_params[] = {
	7, 1,
};
unsigned int div_clk_peric_26_lut_params[] = {
	0, 0,
};
unsigned int div_clk_peric_13_lut_params[] = {
	1, 0,
};

/* COMMON VCLK -> LUT Parameter List */
unsigned int blk_cmu_uud_lut_params[] = {
	1866000, 1333000, 1166000, 800000, 2133000, 808000, 2, 1, 0, 0, 0, 2, 1, 3, 1, 1, 1, 2, 1, 1, 5, 1, 2, 2, 1, 1,
};
unsigned int blk_mif1_nm_lut_params[] = {
	8528000,
};
unsigned int blk_mif1_uud_lut_params[] = {
	26000,
};
unsigned int blk_mif2_nm_lut_params[] = {
	8528000,
};
unsigned int blk_mif2_uud_lut_params[] = {
	26000,
};
unsigned int blk_mif3_nm_lut_params[] = {
	8528000,
};
unsigned int blk_mif3_uud_lut_params[] = {
	26000,
};
unsigned int blk_s2d_nm_lut_params[] = {
	800000, 1,
};
unsigned int blk_apm_nm_lut_params[] = {
	1, 0, 0, 0,
};
unsigned int blk_cmgp_nm_lut_params[] = {
	1, 0,
};
unsigned int blk_cpucl0_sod_lut_params[] = {
	0, 3, 3, 3, 7, 1,
};
unsigned int blk_cpucl0_od_lut_params[] = {
	0, 3, 3, 3, 7, 1,
};
unsigned int blk_cpucl0_uud_lut_params[] = {
	0, 3, 3, 3, 7, 1,
};
unsigned int blk_cpucl1_sod_lut_params[] = {
	0,
};
unsigned int blk_cpucl1_od_lut_params[] = {
	0,
};
unsigned int blk_cpucl1_nm_lut_params[] = {
	0,
};
unsigned int blk_cpucl1_ud_lut_params[] = {
	0,
};
unsigned int blk_cpucl1_sud_lut_params[] = {
	0,
};
unsigned int blk_cpucl1_uud_lut_params[] = {
	0,
};
unsigned int blk_g3d_nm_lut_params[] = {
	0, 3,
};
unsigned int blk_vts_nm_lut_params[] = {
	1, 1, 1, 1, 3, 1, 0, 3, 1, 3,
};
unsigned int blk_aud_od_lut_params[] = {
	2,
};
unsigned int blk_aud_ud_lut_params[] = {
	2,
};
unsigned int blk_aud_sud_lut_params[] = {
	2,
};
unsigned int blk_aud_uud_lut_params[] = {
	2,
};
unsigned int blk_bus0_nm_lut_params[] = {
	1,
};
unsigned int blk_bus1_nm_lut_params[] = {
	1,
};
unsigned int blk_core_nm_lut_params[] = {
	2,
};
unsigned int blk_csis_nm_lut_params[] = {
	1,
};
unsigned int blk_dnc_nm_lut_params[] = {
	3,
};
unsigned int blk_dns_nm_lut_params[] = {
	1,
};
unsigned int blk_dpu_uud_lut_params[] = {
	3,
};
unsigned int blk_dsp_nm_lut_params[] = {
	1,
};
unsigned int blk_g2d_nm_lut_params[] = {
	1,
};
unsigned int blk_ipp_nm_lut_params[] = {
	1,
};
unsigned int blk_itp_nm_lut_params[] = {
	1,
};
unsigned int blk_mcsc_nm_lut_params[] = {
	1,
};
unsigned int blk_mfc0_nm_lut_params[] = {
	3,
};
unsigned int blk_npu_nm_lut_params[] = {
	3,
};
unsigned int blk_npuc_nm_lut_params[] = {
	3,
};
unsigned int blk_peric0_nm_lut_params[] = {
	1,
};
unsigned int blk_peric1_nm_lut_params[] = {
	1,
};
unsigned int blk_ssp_nm_lut_params[] = {
	1,
};
unsigned int blk_tnr_nm_lut_params[] = {
	1,
};
unsigned int blk_vra_nm_lut_params[] = {
	1,
};
