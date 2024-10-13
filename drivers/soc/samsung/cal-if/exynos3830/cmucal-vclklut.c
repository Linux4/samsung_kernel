#include "../cmucal.h"
#include "cmucal-vclklut.h"


/* DVFS VCLK -> LUT Parameter List */
unsigned int vdd_int_od_lut_params[] = {
	0, 0, 0, 1, 1, 1000000, 1, 7, 1, 6, 2, 5, 5, 1, 1, 5, 5, 1, 5, 5, 5, 5, 2, 5, 1, 5, 3, 3, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 2, 1, 7, 1, 1, 1, 2, 1, 3, 2, 1, 3, 1, 1, 2, 3, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3198000, 26000,
};
unsigned int vdd_int_nm_lut_params[] = {
	2, 1, 1, 2, 2, 800000, 1, 7, 1, 6, 2, 5, 5, 1, 1, 5, 5, 1, 5, 5, 5, 5, 2, 5, 1, 5, 3, 3, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 2, 1, 7, 1, 1, 1, 2, 1, 3, 2, 1, 3, 1, 1, 2, 3, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3200000, 26000,
};
unsigned int vdd_int_ud_lut_params[] = {
	3, 3, 0, 0, 0, 600000, 1, 7, 3, 6, 2, 5, 5, 1, 1, 11, 5, 1, 5, 5, 5, 5, 2, 5, 1, 5, 3, 3, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 2, 1, 7, 1, 1, 0, 3, 1, 2, 2, 1, 3, 1, 1, 2, 3, 1, 1, 1, 1, 1, 1, 1, 3, 3, 1, 1, 1, 1, 1, 2, 1, 2667000, 26000,
};
unsigned int vdd_int_reset_lut_params[] = {
	0, 0, 0, 0, 0, 26000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 26000, 26000,
};
unsigned int vdd_cpu_sod_lut_params[] = {
	2000000, 1, 2000000, 1,
};
unsigned int vdd_cpu_od_lut_params[] = {
	1600000, 1, 1600000, 1,
};
unsigned int vdd_cpu_nm_lut_params[] = {
	1150000, 0, 1150000, 0,
};
unsigned int vdd_cpu_ud_lut_params[] = {
	650000, 1, 650000, 1,
};

/* SPECIAL VCLK -> LUT Parameter List */
unsigned int mux_clk_apm_i3c_nm_lut_params[] = {
	0, 1,
};
unsigned int clkcmu_apm_bus_ud_lut_params[] = {
	0, 0,
};
unsigned int mux_clk_aud_uaif0_ud_lut_params[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
};
unsigned int mux_clk_chub_timer_fclk_nm_lut_params[] = {
	0,
};
unsigned int clk_cmgp_adc_nm_lut_params[] = {
	1, 13,
};
unsigned int mux_clk_cmu_cmuref_ud_lut_params[] = {
	0,
};
unsigned int mux_clk_hsi_rtc_nm_lut_params[] = {
	0,
};
unsigned int mux_clkcmu_mif_busp_ud_lut_params[] = {
	0,
};
unsigned int mux_clkcmu_cpucl1_dbg_ud_lut_params[] = {
	0,
};
unsigned int div_clk_chub_dmic_if_div2_nm_lut_params[] = {
	1,
};
unsigned int div_clk_chub_dmic_nm_lut_params[] = {
	0,
};
unsigned int div_clk_cmgp_usi_cmgp0_nm_lut_params[] = {
	1, 1,
};
unsigned int div_clk_cmgp_usi_cmgp1_nm_lut_params[] = {
	1, 1,
};
unsigned int ap2cp_shared0_pll_clk_ud_lut_params[] = {
	0,
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
unsigned int div_clk_cluster0_atclk_sod_lut_params[] = {
	3, 1,
};
unsigned int div_clk_cluster0_atclk_od_lut_params[] = {
	3, 1,
};
unsigned int div_clk_cluster0_atclk_nm_lut_params[] = {
	3, 1,
};
unsigned int div_clk_cluster0_atclk_ud_lut_params[] = {
	3, 1,
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
unsigned int div_clk_cluster1_periphclk_sod_lut_params[] = {
	1, 3,
};
unsigned int div_clk_cluster1_periphclk_od_lut_params[] = {
	1, 3,
};
unsigned int div_clk_cluster1_periphclk_nm_lut_params[] = {
	1, 3,
};
unsigned int div_clk_cluster1_periphclk_ud_lut_params[] = {
	1, 3,
};
unsigned int clkcmu_peri_ip_ud_lut_params[] = {
	0,
};
unsigned int div_clk_peri_spi_0_ud_lut_params[] = {
	0,
};

/* COMMON VCLK -> LUT Parameter List */
unsigned int blk_aud_od_lut_params[] = {
	1033000, 0, 0,
};
unsigned int blk_aud_ud_lut_params[] = {
	1033000, 0, 0,
};
unsigned int blk_cmu_ud_lut_params[] = {
	1600000, 1333000, 808000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
unsigned int blk_apm_nm_lut_params[] = {
	1, 0, 0, 0,
};
unsigned int blk_chub_nm_lut_params[] = {
	1, 0, 0,
};
unsigned int blk_core_ud_lut_params[] = {
	0,
};
unsigned int blk_cpucl0_sod_lut_params[] = {
	3, 3,
};
unsigned int blk_cpucl0_od_lut_params[] = {
	3, 3,
};
unsigned int blk_cpucl0_nm_lut_params[] = {
	3, 3,
};
unsigned int blk_cpucl0_ud_lut_params[] = {
	3, 3,
};
unsigned int blk_cpucl1_sod_lut_params[] = {
	3, 3,
};
unsigned int blk_cpucl1_od_lut_params[] = {
	3, 3,
};
unsigned int blk_cpucl1_nm_lut_params[] = {
	3, 3,
};
unsigned int blk_cpucl1_ud_lut_params[] = {
	3, 3,
};
