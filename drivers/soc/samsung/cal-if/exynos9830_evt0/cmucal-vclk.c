#include "../cmucal.h"
#include "cmucal-node.h"
#include "cmucal-vclk.h"
#include "cmucal-vclklut.h"

/* DVFS VCLK -> Clock Node List */
enum clk_id cmucal_vclk_vddi[] = {
	MUX_CLKCMU_MIF_SWITCH,
	CLKCMU_CORE_BUS,
	MUX_CLKCMU_CORE_BUS,
	CLKCMU_DSP_BUS,
	MUX_CLKCMU_DSP_BUS,
	CLKCMU_NPU_BUS,
	MUX_CLKCMU_NPU_BUS,
	CLKCMU_DNC_BUS,
	MUX_CLKCMU_DNC_BUS,
	CLKCMU_MIF_BUSP,
	MUX_CLKCMU_MIF_BUSP,
	CLKCMU_BUS0_BUS,
	MUX_CLKCMU_BUS0_BUS,
	DIV_CLK_CMU_CMUREF,
	MUX_CLK_CMU_CMUREF,
	CLKCMU_BUS1_BUS,
	MUX_CLKCMU_BUS1_BUS,
	CLKCMU_BUS1_SSS,
	CLKCMU_DNC_BUSM,
	MUX_CLKCMU_DNC_BUSM,
	PLL_SHARED1,
	CLKCMU_CPUCL0_SWITCH,
	MUX_CLKCMU_CPUCL0_SWITCH,
	CLKCMU_G3D_SWITCH,
	CLKCMU_MFC0_MFC0,
	MUX_CLKCMU_MFC0_MFC0,
	CLKCMU_G2D_G2D,
	MUX_CLKCMU_G2D_G2D,
	CLKCMU_IPP_BUS,
	MUX_CLKCMU_IPP_BUS,
	CLKCMU_ITP_BUS,
	MUX_CLKCMU_ITP_BUS,
	CLKCMU_G2D_MSCL,
	MUX_CLKCMU_G2D_MSCL,
	CLKCMU_HSI2_BUS,
	MUX_CLKCMU_HSI2_BUS,
	CLKCMU_HSI0_DPGTC,
	MUX_CLKCMU_HSI0_DPGTC,
	CLKCMU_HSI1_BUS,
	MUX_CLKCMU_HSI1_BUS,
	MUX_CLKCMU_CPUCL2_SWITCH,
	CLKCMU_MFC0_WFD,
	MUX_CLKCMU_MFC0_WFD,
	CLKCMU_DPU_BUS,
	DIV_CLKCMU_DPU,
	CLKCMU_CPUCL1_SWITCH,
	MUX_CLKCMU_CPUCL1_SWITCH,
	CLKCMU_VRA_BUS,
	MUX_CLKCMU_VRA_BUS,
	PLL_G3D,
	CLKCMU_VRA_STR,
	MUX_CLKCMU_VRA_STR,
	CLKCMU_CPUCL2_BUSP,
	CLKCMU_CSIS_BUS,
	MUX_CLKCMU_CSIS_BUS,
	CLKCMU_TNR_BUS,
	MUX_CLKCMU_TNR_BUS,
	CLKCMU_MCSC_BUS,
	MUX_CLKCMU_MCSC_BUS,
	CLKCMU_DNS_BUS,
	MUX_CLKCMU_DNS_BUS,
	CLKCMU_HSI1_MMC_CARD,
	CLKCMU_MCSC_MEIP,
	MUX_CLKCMU_MCSC_MEIP,
	CLKCMU_MCSC_GDC,
	MUX_CLKCMU_MCSC_GDC,
	CLKCMU_SSP_BUS,
	CLKCMU_CPUCL0_DBG_BUS,
	MUX_CLKCMU_CPUCL0_DBG_BUS,
	DIV_CLKCMU_DPU_BUS,
	MUX_CLKCMU_DPU_BUS,
};
enum clk_id cmucal_vclk_vdd_mif[] = {
	PLL_MIF,
	PLL_MIF_EXT,
};
enum clk_id cmucal_vclk_vdd_cam[] = {
	DIV_CLK_AUD_BUS,
};
enum clk_id cmucal_vclk_vdd_cpucl0[] = {
	DIV_CLK_CLUSTER0_ACLK,
	PLL_CPUCL0,
};
enum clk_id cmucal_vclk_vdd_cpucl1[] = {
	PLL_CPUCL1,
};
/* SPECIAL VCLK -> Clock Node List */
enum clk_id cmucal_vclk_mux_clk_apm_i3c_pmic[] = {
	MUX_CLK_APM_I3C_PMIC,
	DIV_CLK_APM_I3C_PMIC,
};
enum clk_id cmucal_vclk_clkcmu_apm_bus[] = {
	CLKCMU_APM_BUS,
	MUX_CLKCMU_APM_BUS,
};
enum clk_id cmucal_vclk_mux_clk_apm_i3c_cp[] = {
	MUX_CLK_APM_I3C_CP,
	DIV_CLK_APM_I3C_CP,
};
enum clk_id cmucal_vclk_mux_clk_aud_dsif[] = {
	MUX_CLK_AUD_DSIF,
	DIV_CLK_AUD_DSIF,
	DIV_CLK_AUD_CPU_PCLKDBG,
	DIV_CLK_AUD_CPU_ACLK,
	DIV_CLK_AUD_PLL,
	DIV_CLK_AUD_SCLK,
	DIV_CLK_AUD_UAIF0,
	MUX_CLK_AUD_UAIF0,
	DIV_CLK_AUD_UAIF1,
	MUX_CLK_AUD_UAIF1,
	DIV_CLK_AUD_UAIF2,
	MUX_CLK_AUD_UAIF2,
	DIV_CLK_AUD_UAIF3,
	MUX_CLK_AUD_UAIF3,
	DIV_CLK_AUD_UAIF4,
	MUX_CLK_AUD_UAIF4,
	DIV_CLK_AUD_UAIF5,
	MUX_CLK_AUD_UAIF5,
	DIV_CLK_AUD_UAIF6,
	MUX_CLK_AUD_UAIF6,
	DIV_CLK_AUD_CNT,
	MUX_CLK_AUD_CNT,
	DIV_CLK_AUD_AUDIF,
};
enum clk_id cmucal_vclk_mux_bus0_cmuref[] = {
	MUX_BUS0_CMUREF,
};
enum clk_id cmucal_vclk_clkcmu_cmu_boost[] = {
	CLKCMU_CMU_BOOST,
	MUX_CLKCMU_CMU_BOOST,
};
enum clk_id cmucal_vclk_mux_bus1_cmuref[] = {
	MUX_BUS1_CMUREF,
};
enum clk_id cmucal_vclk_mux_clk_cmgp_adc[] = {
	MUX_CLK_CMGP_ADC,
	DIV_CLK_CMGP_ADC,
};
enum clk_id cmucal_vclk_clkcmu_cmgp_bus[] = {
	CLKCMU_CMGP_BUS,
	MUX_CLKCMU_CMGP_BUS,
};
enum clk_id cmucal_vclk_mux_cmu_cmuref[] = {
	MUX_CMU_CMUREF,
};
enum clk_id cmucal_vclk_mux_core_cmuref[] = {
	MUX_CORE_CMUREF,
};
enum clk_id cmucal_vclk_mux_cpucl0_cmuref[] = {
	MUX_CPUCL0_CMUREF,
};
enum clk_id cmucal_vclk_mux_cpucl1_cmuref[] = {
	MUX_CPUCL1_CMUREF,
};
enum clk_id cmucal_vclk_mux_cpucl2_cmuref[] = {
	MUX_CPUCL2_CMUREF,
};
enum clk_id cmucal_vclk_mux_mif_cmuref[] = {
	MUX_MIF_CMUREF,
};
enum clk_id cmucal_vclk_mux_mif1_cmuref[] = {
	MUX_MIF1_CMUREF,
};
enum clk_id cmucal_vclk_mux_mif2_cmuref[] = {
	MUX_MIF2_CMUREF,
};
enum clk_id cmucal_vclk_mux_mif3_cmuref[] = {
	MUX_MIF3_CMUREF,
};
enum clk_id cmucal_vclk_mux_clkcmu_hsi0_usbdp_debug[] = {
	MUX_CLKCMU_HSI0_USBDP_DEBUG,
};
enum clk_id cmucal_vclk_mux_clkcmu_hsi1_mmc_card[] = {
	MUX_CLKCMU_HSI1_MMC_CARD,
};
enum clk_id cmucal_vclk_clkcmu_hsi1_ufs_card[] = {
	CLKCMU_HSI1_UFS_CARD,
	MUX_CLKCMU_HSI1_UFS_CARD,
};
enum clk_id cmucal_vclk_mux_clkcmu_hsi2_pcie[] = {
	MUX_CLKCMU_HSI2_PCIE,
};
enum clk_id cmucal_vclk_div_clk_i2c_cmgp0[] = {
	DIV_CLK_I2C_CMGP0,
	MUX_CLK_I2C_CMGP0,
};
enum clk_id cmucal_vclk_div_clk_usi_cmgp1[] = {
	DIV_CLK_USI_CMGP1,
	MUX_CLK_USI_CMGP1,
};
enum clk_id cmucal_vclk_div_clk_usi_cmgp0[] = {
	DIV_CLK_USI_CMGP0,
	MUX_CLK_USI_CMGP0,
};
enum clk_id cmucal_vclk_div_clk_usi_cmgp2[] = {
	DIV_CLK_USI_CMGP2,
	MUX_CLK_USI_CMGP2,
};
enum clk_id cmucal_vclk_div_clk_usi_cmgp3[] = {
	DIV_CLK_USI_CMGP3,
	MUX_CLK_USI_CMGP3,
};
enum clk_id cmucal_vclk_div_clk_i2c_cmgp1[] = {
	DIV_CLK_I2C_CMGP1,
	MUX_CLK_I2C_CMGP1,
};
enum clk_id cmucal_vclk_div_clk_i2c_cmgp2[] = {
	DIV_CLK_I2C_CMGP2,
	MUX_CLK_I2C_CMGP2,
};
enum clk_id cmucal_vclk_div_clk_i2c_cmgp3[] = {
	DIV_CLK_I2C_CMGP3,
	MUX_CLK_I2C_CMGP3,
};
enum clk_id cmucal_vclk_div_clk_dbgcore_uart_cmgp[] = {
	DIV_CLK_DBGCORE_UART_CMGP,
	MUX_CLK_DBGCORE_UART_CMGP,
};
enum clk_id cmucal_vclk_clkcmu_cpucl2_switch[] = {
	CLKCMU_CPUCL2_SWITCH,
};
enum clk_id cmucal_vclk_clkcmu_hpm[] = {
	CLKCMU_HPM,
	MUX_CLKCMU_HPM,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk0[] = {
	CLKCMU_CIS_CLK0,
	MUX_CLKCMU_CIS_CLK0,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk1[] = {
	CLKCMU_CIS_CLK1,
	MUX_CLKCMU_CIS_CLK1,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk2[] = {
	CLKCMU_CIS_CLK2,
	MUX_CLKCMU_CIS_CLK2,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk3[] = {
	CLKCMU_CIS_CLK3,
	MUX_CLKCMU_CIS_CLK3,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk4[] = {
	CLKCMU_CIS_CLK4,
	MUX_CLKCMU_CIS_CLK4,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk5[] = {
	CLKCMU_CIS_CLK5,
	MUX_CLKCMU_CIS_CLK5,
};
enum clk_id cmucal_vclk_div_clk_cpucl0_cmuref[] = {
	DIV_CLK_CPUCL0_CMUREF,
};
enum clk_id cmucal_vclk_div_clk_cluster0_periphclk[] = {
	DIV_CLK_CLUSTER0_PERIPHCLK,
};
enum clk_id cmucal_vclk_div_clk_cpucl1_cmuref[] = {
	DIV_CLK_CPUCL1_CMUREF,
};
enum clk_id cmucal_vclk_div_clk_dsp1_busp[] = {
	DIV_CLK_DSP1_BUSP,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi00_usi[] = {
	DIV_CLK_PERIC0_USI00_USI,
	MUX_CLKCMU_PERIC0_USI00_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi01_usi[] = {
	DIV_CLK_PERIC0_USI01_USI,
	MUX_CLKCMU_PERIC0_USI01_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi02_usi[] = {
	DIV_CLK_PERIC0_USI02_USI,
	MUX_CLKCMU_PERIC0_USI02_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi03_usi[] = {
	DIV_CLK_PERIC0_USI03_USI,
	MUX_CLKCMU_PERIC0_USI03_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi04_usi[] = {
	DIV_CLK_PERIC0_USI04_USI,
	MUX_CLKCMU_PERIC0_USI04_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi05_usi[] = {
	DIV_CLK_PERIC0_USI05_USI,
	MUX_CLKCMU_PERIC0_USI05_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric0_uart_dbg[] = {
	DIV_CLK_PERIC0_UART_DBG,
	MUX_CLKCMU_PERIC0_UART_DBG,
};
enum clk_id cmucal_vclk_clkcmu_peric0_ip[] = {
	CLKCMU_PERIC0_IP,
	MUX_CLKCMU_PERIC0_IP,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi13_usi[] = {
	DIV_CLK_PERIC0_USI13_USI,
	MUX_CLKCMU_PERIC0_USI13_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi14_usi[] = {
	DIV_CLK_PERIC0_USI14_USI,
	MUX_CLKCMU_PERIC0_USI14_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi15_usi[] = {
	DIV_CLK_PERIC0_USI15_USI,
	MUX_CLKCMU_PERIC0_USI15_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric1_uart_bt[] = {
	DIV_CLK_PERIC1_UART_BT,
	MUX_CLKCMU_PERIC1_UART_BT_USER,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi06_usi[] = {
	DIV_CLK_PERIC1_USI06_USI,
	MUX_CLKCMU_PERIC1_USI06_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi07_usi[] = {
	DIV_CLK_PERIC1_USI07_USI,
	MUX_CLKCMU_PERIC1_USI07_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi08_usi[] = {
	DIV_CLK_PERIC1_USI08_USI,
	MUX_CLKCMU_PERIC1_USI08_USI_USER,
};
enum clk_id cmucal_vclk_clkcmu_peric1_ip[] = {
	CLKCMU_PERIC1_IP,
	MUX_CLKCMU_PERIC1_IP,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi18_usi[] = {
	DIV_CLK_PERIC1_USI18_USI,
	MUX_CLKCMU_PERIC1_USI18_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi12_usi[] = {
	DIV_CLK_PERIC1_USI12_USI,
	MUX_CLKCMU_PERIC1_USI12_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi09_usi[] = {
	DIV_CLK_PERIC1_USI09_USI,
	MUX_CLKCMU_PERIC1_USI09_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi10_usi[] = {
	DIV_CLK_PERIC1_USI10_USI,
	MUX_CLKCMU_PERIC1_USI10_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi11_usi[] = {
	DIV_CLK_PERIC1_USI11_USI,
	MUX_CLKCMU_PERIC1_USI11_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi16_usi[] = {
	DIV_CLK_PERIC1_USI16_USI,
	MUX_CLKCMU_PERIC1_USI16_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi17_usi[] = {
	DIV_CLK_PERIC1_USI17_USI,
	MUX_CLKCMU_PERIC1_USI17_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_vts_dmic_if_pad[] = {
	DIV_CLK_VTS_DMIC_IF_PAD,
	DIV_CLK_VTS_DMIC_AUD_PAD,
};
enum clk_id cmucal_vclk_div_clk_aud_dmic0[] = {
	DIV_CLK_AUD_DMIC0,
};
enum clk_id cmucal_vclk_div_clk_aud_dmic1[] = {
	DIV_CLK_AUD_DMIC1,
};
enum clk_id cmucal_vclk_div_clk_vts_serial_lif[] = {
	DIV_CLK_VTS_SERIAL_LIF,
	MUX_CLK_VTS_SERIAL_LIF,
};
/* COMMON VCLK -> Clock Node List */
enum clk_id cmucal_vclk_blk_aud[] = {
	DIV_CLK_AUD_BUSP,
	PLL_AUD0,
	PLL_AUD1,
};
enum clk_id cmucal_vclk_blk_cmu[] = {
	MUX_CLKCMU_SSP_BUS,
	PLL_SHARED4_DIV4,
	PLL_SHARED4_DIV2,
	PLL_SHARED4_DIV3,
	PLL_SHARED4,
	PLL_SHARED3,
	MUX_CLKCMU_HSI1_PCIE,
	MUX_CLKCMU_BUS1_SSS,
	CLKCMU_PERIC1_BUS,
	MUX_CLKCMU_PERIC1_BUS,
	CLKCMU_PERIS_BUS,
	MUX_CLKCMU_PERIS_BUS,
	CLKCMU_HSI0_BUS,
	MUX_CLKCMU_HSI0_BUS,
	CLKCMU_HSI0_USB31DRD,
	MUX_CLKCMU_HSI0_USB31DRD,
	CLKCMU_HSI1_UFS_EMBD,
	MUX_CLKCMU_HSI1_UFS_EMBD,
	CLKCMU_CSIS_OIS_MCU,
	MUX_CLKCMU_CSIS_OIS_MCU,
	MUX_CLKCMU_CPUCL2_BUSP,
	CLKCMU_PERIC0_BUS,
	MUX_CLKCMU_PERIC0_BUS,
	PLL_SHARED2_DIV2,
	PLL_SHARED2,
	MUX_CLKCMU_DPU,
	PLL_SHARED0_DIV3,
	PLL_SHARED0_DIV4,
	PLL_SHARED0_DIV2,
	PLL_SHARED0,
	PLL_MMC,
	PLL_SHARED1_DIV4,
	PLL_SHARED1_DIV2,
	PLL_SHARED1_DIV3,
};
enum clk_id cmucal_vclk_blk_mif1[] = {
	PLL_MIF1,
};
enum clk_id cmucal_vclk_blk_mif2[] = {
	PLL_MIF2,
};
enum clk_id cmucal_vclk_blk_mif3[] = {
	PLL_MIF3,
};
enum clk_id cmucal_vclk_blk_s2d[] = {
	MUX_CLK_S2D_CORE,
	PLL_MIF_S2D,
};
enum clk_id cmucal_vclk_blk_apm[] = {
	DIV_CLK_APM_BUS,
	MUX_CLK_APM_BUS,
	CLKCMU_VTS_BUS,
	MUX_CLKCMU_VTS_BUS,
};
enum clk_id cmucal_vclk_blk_cmgp[] = {
	DIV_CLK_CMGP_BUS,
	MUX_CLK_CMGP_BUS,
};
enum clk_id cmucal_vclk_blk_cpucl0[] = {
	DIV_CLK_CLUSTER0_ATCLK,
	DIV_CLK_CLUSTER0_PCLKDBG,
	DIV_CLK_CPUCL0_PCLK,
	MUX_CLK_CPUCL0_PLL,
	DIV_CLK_CPUCL0_DBG_PCLKDBG,
	DIV_CLK_CPUCL0_DBG_BUS,
};
enum clk_id cmucal_vclk_blk_cpucl1[] = {
	MUX_CLK_CPUCL1_PLL,
};
enum clk_id cmucal_vclk_blk_g3d[] = {
	DIV_CLK_G3D_BUSP,
	MUX_CLK_G3D_BUSD,
};
enum clk_id cmucal_vclk_blk_vts[] = {
	DIV_CLK_VTS_BUS,
	MUX_CLK_VTS_BUS,
	DIV_CLK_VTS_DMIC_IF_DIV2,
	DIV_CLK_VTS_DMIC_IF,
	MUX_CLK_VTS_DMIC_IF,
	DIV_CLK_VTS_DMIC_AUD_DIV2,
	DIV_CLK_VTS_DMIC_AUD,
	MUX_CLK_VTS_DMIC_AUD,
};
enum clk_id cmucal_vclk_blk_bus0[] = {
	DIV_CLK_BUS0_BUSP,
};
enum clk_id cmucal_vclk_blk_bus1[] = {
	DIV_CLK_BUS1_BUSP,
};
enum clk_id cmucal_vclk_blk_core[] = {
	DIV_CLK_CORE_BUSP,
};
enum clk_id cmucal_vclk_blk_csis[] = {
	DIV_CLK_CSIS_BUSP,
};
enum clk_id cmucal_vclk_blk_dnc[] = {
	DIV_CLK_DNC_BUSP,
};
enum clk_id cmucal_vclk_blk_dns[] = {
	DIV_CLK_DNS_BUSP,
};
enum clk_id cmucal_vclk_blk_dpu[] = {
	DIV_CLK_DPU_BUSP,
};
enum clk_id cmucal_vclk_blk_dsp[] = {
	DIV_CLK_DSP_BUSP,
};
enum clk_id cmucal_vclk_blk_g2d[] = {
	DIV_CLK_G2D_BUSP,
};
enum clk_id cmucal_vclk_blk_ipp[] = {
	DIV_CLK_IPP_BUSP,
};
enum clk_id cmucal_vclk_blk_itp[] = {
	DIV_CLK_ITP_BUSP,
};
enum clk_id cmucal_vclk_blk_mcsc[] = {
	DIV_CLK_MCSC_BUSP,
};
enum clk_id cmucal_vclk_blk_mfc0[] = {
	DIV_CLK_MFC0_BUSP,
};
enum clk_id cmucal_vclk_blk_npu[] = {
	DIV_CLK_NPU_BUSP,
};
enum clk_id cmucal_vclk_blk_npu1[] = {
	DIV_CLK_NPU1_BUSP,
};
enum clk_id cmucal_vclk_blk_peric0[] = {
	DIV_CLK_PERIC0_USI_I2C,
};
enum clk_id cmucal_vclk_blk_peric1[] = {
	DIV_CLK_PERIC1_USI_I2C,
};
enum clk_id cmucal_vclk_blk_ssp[] = {
	DIV_CLK_SSP_BUSP,
};
enum clk_id cmucal_vclk_blk_tnr[] = {
	DIV_CLK_TNR_BUSP,
};
enum clk_id cmucal_vclk_blk_vra[] = {
	DIV_CLK_VRA_BUSP,
};
/* GATE VCLK -> Clock Node List */
enum clk_id cmucal_vclk_ip_lhs_axi_d_apm[] = {
	GOUT_BLK_APM_UID_LHS_AXI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_apm[] = {
	GOUT_BLK_APM_UID_LHM_AXI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_wdt_apm[] = {
	GOUT_BLK_APM_UID_WDT_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_apm[] = {
	GOUT_BLK_APM_UID_SYSREG_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_ap[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_pmu_alive[] = {
	GOUT_BLK_APM_UID_APBIF_PMU_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_intmem[] = {
	GOUT_BLK_APM_UID_INTMEM_IPCLKPORT_ACLK,
	GOUT_BLK_APM_UID_INTMEM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_scan2dram[] = {
	GOUT_BLK_APM_UID_LHS_AXI_G_SCAN2DRAM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pmu_intr_gen[] = {
	GOUT_BLK_APM_UID_PMU_INTR_GEN_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pem[] = {
	GOUT_BLK_APM_UID_PEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_speedy_apm[] = {
	GOUT_BLK_APM_UID_SPEEDY_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_apm[] = {
	GOUT_BLK_APM_UID_XIU_DP_APM_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_apm_cmu_apm[] = {
	CLK_BLK_APM_UID_APM_CMU_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_grebeintegration[] = {
	GOUT_BLK_APM_UID_GREBEINTEGRATION_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_apbif_gpio_alive[] = {
	GOUT_BLK_APM_UID_APBIF_GPIO_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_top_rtc[] = {
	GOUT_BLK_APM_UID_APBIF_TOP_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ss_dbgcore[] = {
	GOUT_BLK_APM_UID_SS_DBGCORE_IPCLKPORT_SS_DBGCORE_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_dtzpc_apm[] = {
	GOUT_BLK_APM_UID_DTZPC_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_vts[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_vts[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_dbgcore[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_DBGCORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_lp_vts[] = {
	GOUT_BLK_APM_UID_LHS_AXI_LP_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_dbgcore[] = {
	GOUT_BLK_APM_UID_LHS_AXI_G_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_apbif_rtc[] = {
	GOUT_BLK_APM_UID_APBIF_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_c_cmgp[] = {
	GOUT_BLK_APM_UID_LHS_AXI_C_CMGP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_apm[] = {
	GOUT_BLK_APM_UID_VGEN_LITE_APM_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_speedy_sub_apm[] = {
	GOUT_BLK_APM_UID_SPEEDY_SUB_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_rom_crc32_host[] = {
	GOUT_BLK_APM_UID_ROM_CRC32_HOST_IPCLKPORT_PCLK,
	GOUT_BLK_APM_UID_ROM_CRC32_HOST_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_i3c_apm_pmic[] = {
	GOUT_BLK_APM_UID_I3C_APM_PMIC_IPCLKPORT_I_PCLK,
	GOUT_BLK_APM_UID_I3C_APM_PMIC_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_i3c_apm_cp[] = {
	GOUT_BLK_APM_UID_I3C_APM_CP_IPCLKPORT_I_PCLK,
	GOUT_BLK_APM_UID_I3C_APM_CP_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_aud_cmu_aud[] = {
	CLK_BLK_AUD_UID_AUD_CMU_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_aud[] = {
	GOUT_BLK_AUD_UID_LHS_AXI_D_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_aud[] = {
	GOUT_BLK_AUD_UID_PPMU_AUD_IPCLKPORT_ACLK,
	GOUT_BLK_AUD_UID_PPMU_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_aud[] = {
	GOUT_BLK_AUD_UID_SYSREG_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_abox[] = {
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF0,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF1,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF3,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_DSIF,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF2,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_DAP,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK_IRQ,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK_IRQ,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_CNT,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF4,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF5,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_ASB,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_CA32,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_SCLK,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF6,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_aud[] = {
	GOUT_BLK_AUD_UID_LHM_AXI_P_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_peri_axi_asb[] = {
	GOUT_BLK_AUD_UID_PERI_AXI_ASB_IPCLKPORT_PCLK,
	GOUT_BLK_AUD_UID_PERI_AXI_ASB_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_wdt_aud[] = {
	GOUT_BLK_AUD_UID_WDT_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_smmu_aud[] = {
	GOUT_BLK_AUD_UID_SMMU_AUD_IPCLKPORT_CLK_S1,
	GOUT_BLK_AUD_UID_SMMU_AUD_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ad_apb_smmu_aud[] = {
	GOUT_BLK_AUD_UID_AD_APB_SMMU_AUD_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_smmu_aud_s[] = {
	GOUT_BLK_AUD_UID_AD_APB_SMMU_AUD_S_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_vgen_lite_aud[] = {
	GOUT_BLK_AUD_UID_VGEN_LITE_AUD_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_smmu_aud_ns1[] = {
	GOUT_BLK_AUD_UID_AD_APB_SMMU_AUD_NS1_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_bus0_cmu_bus0[] = {
	CLK_BLK_BUS0_UID_BUS0_CMU_BUS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_bus0[] = {
	GOUT_BLK_BUS0_UID_SYSREG_BUS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d0_bus0[] = {
	GOUT_BLK_BUS0_UID_TREX_D0_BUS0_IPCLKPORT_PCLK,
	GOUT_BLK_BUS0_UID_TREX_D0_BUS0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif0[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_MIF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif1[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_MIF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif2[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_MIF2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif3[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_MIF3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_peris[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_PERIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_peric1[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_PERIC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mcsc[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_csis[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_vra[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_VRA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_itp[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_ITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_tnr[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_hsi1[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_trex_p_bus0[] = {
	GOUT_BLK_BUS0_UID_TREX_P_BUS0_IPCLKPORT_ACLK_BUS0,
	GOUT_BLK_BUS0_UID_TREX_P_BUS0_IPCLKPORT_PCLK_BUS0,
	GOUT_BLK_BUS0_UID_TREX_P_BUS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d1_bus0[] = {
	GOUT_BLK_BUS0_UID_TREX_D1_BUS0_IPCLKPORT_ACLK,
	GOUT_BLK_BUS0_UID_TREX_D1_BUS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_aud[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_vra[] = {
	GOUT_BLK_BUS0_UID_LHM_AXI_D_VRA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_hsi1[] = {
	GOUT_BLK_BUS0_UID_LHM_ACEL_D_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_aud[] = {
	GOUT_BLK_BUS0_UID_LHM_AXI_D_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_ipp[] = {
	GOUT_BLK_BUS0_UID_LHM_AXI_D_IPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dns[] = {
	GOUT_BLK_BUS0_UID_LHM_AXI_D_DNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_mcsc[] = {
	GOUT_BLK_BUS0_UID_LHM_AXI_D0_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_tnr[] = {
	GOUT_BLK_BUS0_UID_LHM_AXI_D_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_bus0[] = {
	GOUT_BLK_BUS0_UID_D_TZPC_BUS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_dbg_g_bus0[] = {
	GOUT_BLK_BUS0_UID_LHS_DBG_G_BUS0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_mcsc[] = {
	GOUT_BLK_BUS0_UID_LHM_AXI_D1_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_csis[] = {
	GOUT_BLK_BUS0_UID_LHM_AXI_D0_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_csis[] = {
	GOUT_BLK_BUS0_UID_LHM_AXI_D1_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_busif_cmutopc[] = {
	GOUT_BLK_BUS0_UID_BUSIF_CMUTOPC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_ipp[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_IPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cacheaid_bus0[] = {
	GOUT_BLK_BUS0_UID_CACHEAID_BUS0_IPCLKPORT_ACLK,
	GOUT_BLK_BUS0_UID_CACHEAID_BUS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_ssp[] = {
	GOUT_BLK_BUS0_UID_LHM_AXI_D_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_ssp[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bus1_cmu_bus1[] = {
	CLK_BLK_BUS1_UID_BUS1_CMU_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_dit[] = {
	GOUT_BLK_BUS1_UID_AD_APB_DIT_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_adm_ahb_sss[] = {
	GOUT_BLK_BUS1_UID_ADM_AHB_SSS_IPCLKPORT_HCLKM,
};
enum clk_id cmucal_vclk_ip_d_tzpc_bus1[] = {
	GOUT_BLK_BUS1_UID_D_TZPC_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dit[] = {
	GOUT_BLK_BUS1_UID_DIT_IPCLKPORT_ICLKL2A,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dnc[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_hsi0[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_hsi2[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_HSI2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mfc0[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_vts[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_dbg_g_bus1[] = {
	GOUT_BLK_BUS1_UID_LHS_DBG_G_BUS1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_puf[] = {
	GOUT_BLK_BUS1_UID_PUF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qe_pdma[] = {
	GOUT_BLK_BUS1_UID_QE_PDMA_IPCLKPORT_ACLK,
	GOUT_BLK_BUS1_UID_QE_PDMA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_rtic[] = {
	GOUT_BLK_BUS1_UID_QE_RTIC_IPCLKPORT_ACLK,
	GOUT_BLK_BUS1_UID_QE_RTIC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pdma[] = {
	GOUT_BLK_BUS1_UID_PDMA_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_spdma[] = {
	GOUT_BLK_BUS1_UID_QE_SPDMA_IPCLKPORT_ACLK,
	GOUT_BLK_BUS1_UID_QE_SPDMA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_sss[] = {
	GOUT_BLK_BUS1_UID_QE_SSS_IPCLKPORT_PCLK,
	GOUT_BLK_BUS1_UID_QE_SSS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_rtic[] = {
	GOUT_BLK_BUS1_UID_RTIC_IPCLKPORT_I_PCLK,
	GOUT_BLK_BUS1_UID_RTIC_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_sbic[] = {
	GOUT_BLK_BUS1_UID_SBIC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_spdma[] = {
	GOUT_BLK_BUS1_UID_SPDMA_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_bus1[] = {
	GOUT_BLK_BUS1_UID_SYSREG_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d0_bus1[] = {
	GOUT_BLK_BUS1_UID_TREX_D0_BUS1_IPCLKPORT_ACLK,
	GOUT_BLK_BUS1_UID_TREX_D0_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_p_bus1[] = {
	GOUT_BLK_BUS1_UID_TREX_P_BUS1_IPCLKPORT_PCLK_BUS1,
	GOUT_BLK_BUS1_UID_TREX_P_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_rb_bus1[] = {
	GOUT_BLK_BUS1_UID_TREX_RB_BUS1_IPCLKPORT_CLK,
	GOUT_BLK_BUS1_UID_TREX_RB_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_bus1[] = {
	GOUT_BLK_BUS1_UID_XIU_D0_BUS1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sss[] = {
	GOUT_BLK_BUS1_UID_SSS_IPCLKPORT_I_PCLK,
	GOUT_BLK_BUS1_UID_SSS_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d0_dnc[] = {
	GOUT_BLK_BUS1_UID_LHM_ACEL_D0_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d1_dnc[] = {
	GOUT_BLK_BUS1_UID_LHM_ACEL_D1_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_hsi0[] = {
	GOUT_BLK_BUS1_UID_LHM_ACEL_D_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_hsi2[] = {
	GOUT_BLK_BUS1_UID_LHM_ACEL_D_HSI2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d2_dnc[] = {
	GOUT_BLK_BUS1_UID_LHM_ACEL_D2_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_mfc0[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D0_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_mfc0[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D1_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_apm[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d0_g2d[] = {
	GOUT_BLK_BUS1_UID_LHM_ACEL_D0_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_vts[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_sbic[] = {
	GOUT_BLK_BUS1_UID_AD_APB_SBIC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_vgen_pdma[] = {
	GOUT_BLK_BUS1_UID_AD_APB_VGEN_PDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_xiu_d1_bus1[] = {
	GOUT_BLK_BUS1_UID_XIU_D1_BUS1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_pdma[] = {
	GOUT_BLK_BUS1_UID_AD_APB_PDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_spdma[] = {
	GOUT_BLK_BUS1_UID_AD_APB_SPDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_acvps[] = {
	GOUT_BLK_BUS1_UID_AD_APB_SYSMMU_ACVPS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_dit[] = {
	GOUT_BLK_BUS1_UID_AD_APB_SYSMMU_DIT_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_sbic[] = {
	GOUT_BLK_BUS1_UID_AD_APB_SYSMMU_SBIC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_sss[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D_SSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_sss[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_D_SSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_bus1[] = {
	GOUT_BLK_BUS1_UID_VGEN_LITE_BUS1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_baaw_d_sss[] = {
	GOUT_BLK_BUS1_UID_BAAW_D_SSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_vts[] = {
	GOUT_BLK_BUS1_UID_BAAW_P_VTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_npu0[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_npu1[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_NPU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_s2_acvps[] = {
	GOUT_BLK_BUS1_UID_SYSMMU_S2_ACVPS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_s2_dit[] = {
	GOUT_BLK_BUS1_UID_SYSMMU_S2_DIT_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_s2_sbic[] = {
	GOUT_BLK_BUS1_UID_SYSMMU_S2_SBIC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d1_g2d[] = {
	GOUT_BLK_BUS1_UID_LHM_ACEL_D1_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d2_g2d[] = {
	GOUT_BLK_BUS1_UID_LHM_ACEL_D2_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_g2d[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_dnc[] = {
	GOUT_BLK_BUS1_UID_BAAW_P_DNC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_cacheaid_bus1[] = {
	GOUT_BLK_BUS1_UID_CACHEAID_BUS1_IPCLKPORT_ACLK,
	GOUT_BLK_BUS1_UID_CACHEAID_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d1_bus1[] = {
	GOUT_BLK_BUS1_UID_TREX_D1_BUS1_IPCLKPORT_ACLK,
	GOUT_BLK_BUS1_UID_TREX_D1_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_peric0[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_PERIC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dpu[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d2_dpu[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D2_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_dpu[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D1_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_dpu[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D0_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_d0bus1_p0core[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_D0BUS1_P0CORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_pdma[] = {
	GOUT_BLK_BUS1_UID_VGEN_PDMA_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_cmgp_cmu_cmgp[] = {
	CLK_BLK_CMGP_UID_CMGP_CMU_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_adc_cmgp[] = {
	GOUT_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_PCLK_S0,
	GOUT_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_PCLK_S1,
	CLK_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_gpio_cmgp[] = {
	GOUT_BLK_CMGP_UID_GPIO_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp0[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp1[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP1_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP1_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp2[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP2_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP2_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp3[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP3_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP3_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp0[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp1[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp2[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp3[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2pmu_ap[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2PMU_AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cmgp[] = {
	GOUT_BLK_CMGP_UID_D_TZPC_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_cmgp[] = {
	GOUT_BLK_CMGP_UID_LHM_AXI_C_CMGP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2apm[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dbgcore_uart_cmgp[] = {
	GOUT_BLK_CMGP_UID_DBGCORE_UART_CMGP_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_DBGCORE_UART_CMGP_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_core_cmu_core[] = {
	CLK_BLK_CORE_UID_CORE_CMU_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_core[] = {
	GOUT_BLK_CORE_UID_SYSREG_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mpace2axi_0[] = {
	GOUT_BLK_CORE_UID_MPACE2AXI_0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mpace2axi_1[] = {
	GOUT_BLK_CORE_UID_MPACE2AXI_1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppc_debug_cci[] = {
	GOUT_BLK_CORE_UID_PPC_DEBUG_CCI_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_DEBUG_CCI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_p0_core[] = {
	GOUT_BLK_CORE_UID_TREX_P0_CORE_IPCLKPORT_ACLK_CORE,
	GOUT_BLK_CORE_UID_TREX_P0_CORE_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_TREX_P0_CORE_IPCLKPORT_PCLK_CORE,
};
enum clk_id cmucal_vclk_ip_ppmu_cpucl2_0[] = {
	GOUT_BLK_CORE_UID_PPMU_CPUCL2_0_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPMU_CPUCL2_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t_bdu[] = {
	GOUT_BLK_CORE_UID_LHS_ATB_T_BDU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bdu[] = {
	GOUT_BLK_CORE_UID_BDU_IPCLKPORT_I_CLK,
	GOUT_BLK_CORE_UID_BDU_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_p1_core[] = {
	GOUT_BLK_CORE_UID_TREX_P1_CORE_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_TREX_P1_CORE_IPCLKPORT_PCLK_CORE,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_g3d[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_cpucl0[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_cpucl2[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CPUCL2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d0_g3d[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D0_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d1_g3d[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D1_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d2_g3d[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D2_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d3_g3d[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D3_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_trex_d_core[] = {
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppcfw_g3d[] = {
	GOUT_BLK_CORE_UID_PPCFW_G3D_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPCFW_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_apm[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_cpucl2_1[] = {
	GOUT_BLK_CORE_UID_PPMU_CPUCL2_1_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPMU_CPUCL2_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_core[] = {
	GOUT_BLK_CORE_UID_D_TZPC_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_cpucl2_0[] = {
	GOUT_BLK_CORE_UID_PPC_CPUCL2_0_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_CPUCL2_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_cpucl2_1[] = {
	GOUT_BLK_CORE_UID_PPC_CPUCL2_1_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_CPUCL2_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_g3d0[] = {
	GOUT_BLK_CORE_UID_PPC_G3D0_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_G3D0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_g3d1[] = {
	GOUT_BLK_CORE_UID_PPC_G3D1_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_G3D1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_g3d2[] = {
	GOUT_BLK_CORE_UID_PPC_G3D2_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_G3D2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_g3d3[] = {
	GOUT_BLK_CORE_UID_PPC_G3D3_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_G3D3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_irps0[] = {
	GOUT_BLK_CORE_UID_PPC_IRPS0_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_IRPS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_irps1[] = {
	GOUT_BLK_CORE_UID_PPC_IRPS1_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_IRPS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_l_core[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_L_CORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d0_cluster0[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D0_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d1_cluster0[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D1_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppc_cpucl0_0[] = {
	GOUT_BLK_CORE_UID_PPC_CPUCL0_0_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_CPUCL0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_cpucl0_1[] = {
	GOUT_BLK_CORE_UID_PPC_CPUCL0_1_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_CPUCL0_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_cpucl0_0[] = {
	GOUT_BLK_CORE_UID_PPMU_CPUCL0_0_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPMU_CPUCL0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_cpucl0_1[] = {
	GOUT_BLK_CORE_UID_PPMU_CPUCL0_1_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPMU_CPUCL0_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mpace_asb_d0_mif[] = {
	GOUT_BLK_CORE_UID_MPACE_ASB_D0_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mpace_asb_d1_mif[] = {
	GOUT_BLK_CORE_UID_MPACE_ASB_D1_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mpace_asb_d2_mif[] = {
	GOUT_BLK_CORE_UID_MPACE_ASB_D2_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mpace_asb_d3_mif[] = {
	GOUT_BLK_CORE_UID_MPACE_ASB_D3_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_axi_asb_cssys[] = {
	GOUT_BLK_CORE_UID_AXI_ASB_CSSYS_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_AXI_ASB_CSSYS_IPCLKPORT_ACLKM,
	GOUT_BLK_CORE_UID_AXI_ASB_CSSYS_IPCLKPORT_ACLKS,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_cssys[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_G_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cci[] = {
	CLK_BLK_CORE_UID_CCI_IPCLKPORT_PCLK,
	CLK_BLK_CORE_UID_CCI_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_l_core[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_L_CORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_g3d0[] = {
	GOUT_BLK_CORE_UID_PPMU_G3D0_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_PPMU_G3D0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_g3d1[] = {
	GOUT_BLK_CORE_UID_PPMU_G3D1_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_PPMU_G3D1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_g3d2[] = {
	GOUT_BLK_CORE_UID_PPMU_G3D2_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_PPMU_G3D2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_g3d3[] = {
	GOUT_BLK_CORE_UID_PPMU_G3D3_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_PPMU_G3D3_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_g3d0[] = {
	GOUT_BLK_CORE_UID_QE_G3D0_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_QE_G3D0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_g3d1[] = {
	GOUT_BLK_CORE_UID_QE_G3D1_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_QE_G3D1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_g3d2[] = {
	GOUT_BLK_CORE_UID_QE_G3D2_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_QE_G3D2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_g3d3[] = {
	GOUT_BLK_CORE_UID_QE_G3D3_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_QE_G3D3_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_g3d0[] = {
	GOUT_BLK_CORE_UID_SYSMMU_G3D0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_g3d1[] = {
	GOUT_BLK_CORE_UID_SYSMMU_G3D1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_g3d2[] = {
	GOUT_BLK_CORE_UID_SYSMMU_G3D2_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_g3d3[] = {
	GOUT_BLK_CORE_UID_SYSMMU_G3D3_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_xiu_d_core[] = {
	GOUT_BLK_CORE_UID_XIU_D_CORE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_d0bus1_p0core[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_P_D0BUS1_P0CORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_apb_async_sysmmu_g3d0[] = {
	GOUT_BLK_CORE_UID_APB_ASYNC_SYSMMU_G3D0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysreg_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_SYSREG_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hpm_apbif_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_HPM_APBIF_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cssys[] = {
	GOUT_BLK_CPUCL0_UID_CSSYS_IPCLKPORT_PCLKDBG,
	GOUT_BLK_CPUCL0_UID_CSSYS_IPCLKPORT_ATCLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t_bdu[] = {
	GOUT_BLK_CPUCL0_UID_LHM_ATB_T_BDU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t0_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHM_ATB_T0_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t0_cluster2[] = {
	GOUT_BLK_CPUCL0_UID_LHM_ATB_T0_CLUSTER2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t1_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHM_ATB_T1_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t1_cluster2[] = {
	GOUT_BLK_CPUCL0_UID_LHM_ATB_T1_CLUSTER2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t2_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHM_ATB_T2_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t3_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHM_ATB_T3_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_secjtag[] = {
	GOUT_BLK_CPUCL0_UID_SECJTAG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_LHM_AXI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ace_d0_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHS_ACE_D0_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t0_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHS_ATB_T0_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t1_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHS_ATB_T1_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t2_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHS_ATB_T2_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t3_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHS_ATB_T3_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_adm_apb_g_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_ADM_APB_G_CLUSTER0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_cpucl0_cmu_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_CPUCL0_CMU_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_CPUCL0_IPCLKPORT_PERIPHCLK,
	CLK_BLK_CPUCL0_UID_CPUCL0_IPCLKPORT_SCLK,
	CLK_BLK_CPUCL0_UID_CPUCL0_IPCLKPORT_PCLK,
	CLK_BLK_CPUCL0_UID_CPUCL0_IPCLKPORT_ATCLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t4_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHM_ATB_T4_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t5_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHM_ATB_T5_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ace_d1_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHS_ACE_D1_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t4_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHS_ATB_T4_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t5_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHS_ATB_T5_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_D_TZPC_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_int_cssys[] = {
	GOUT_BLK_CPUCL0_UID_LHS_AXI_G_INT_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_int_cssys[] = {
	GOUT_BLK_CPUCL0_UID_LHM_AXI_G_INT_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_p_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_XIU_P_CPUCL0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_cssys[] = {
	GOUT_BLK_CPUCL0_UID_XIU_DP_CSSYS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_trex_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_TREX_CPUCL0_IPCLKPORT_CLK,
	GOUT_BLK_CPUCL0_UID_TREX_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_cssys[] = {
	GOUT_BLK_CPUCL0_UID_LHS_AXI_G_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_hpm_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_HPM_CPUCL0_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_apb_async_p_cssys_0[] = {
	GOUT_BLK_CPUCL0_UID_APB_ASYNC_P_CSSYS_0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_int_etr[] = {
	GOUT_BLK_CPUCL0_UID_LHS_AXI_G_INT_ETR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_int_etr[] = {
	GOUT_BLK_CPUCL0_UID_LHM_AXI_G_INT_ETR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bps_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_BPS_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_int_dbgcore[] = {
	GOUT_BLK_CPUCL0_UID_LHM_AXI_G_INT_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_dbgcore[] = {
	GOUT_BLK_CPUCL0_UID_LHM_AXI_G_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_int_dbgcore[] = {
	GOUT_BLK_CPUCL0_UID_LHS_AXI_G_INT_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_int_stm[] = {
	GOUT_BLK_CPUCL0_UID_LHS_AXI_G_INT_STM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_int_stm[] = {
	GOUT_BLK_CPUCL0_UID_LHM_AXI_G_INT_STM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cpucl1_cmu_cpucl1[] = {
	CLK_BLK_CPUCL1_UID_CPUCL1_CMU_CPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl1[] = {
	CLK_BLK_CPUCL1_UID_CPUCL1_IPCLKPORT_SCLK,
	CLK_BLK_CPUCL1_UID_CPUCL1_IPCLKPORT_DDD_CPUCL0_1_CPU4_CK_IN,
	CLK_BLK_CPUCL1_UID_CPUCL1_IPCLKPORT_DDD_CPUCL0_1_CPU5_CK_IN,
};
enum clk_id cmucal_vclk_ip_cpucl2_cmu_cpucl2[] = {
	CLK_BLK_CPUCL2_UID_CPUCL2_CMU_CPUCL2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cpucl2[] = {
	GOUT_BLK_CPUCL2_UID_SYSREG_CPUCL2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hpm_apbif_cpucl2[] = {
	GOUT_BLK_CPUCL2_UID_HPM_APBIF_CPUCL2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hpm_cpucl2[] = {
	CLK_BLK_CPUCL2_UID_HPM_CPUCL2_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_cpucl2[] = {
	GOUT_BLK_CPUCL2_UID_LHM_AXI_P_CPUCL2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cpucl2[] = {
	GOUT_BLK_CPUCL2_UID_D_TZPC_CPUCL2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_csis_cmu_csis[] = {
	CLK_BLK_CSIS_UID_CSIS_CMU_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_csis[] = {
	GOUT_BLK_CSIS_UID_LHS_AXI_D0_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_csis[] = {
	GOUT_BLK_CSIS_UID_LHS_AXI_D1_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_csis[] = {
	GOUT_BLK_CSIS_UID_D_TZPC_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_csis_pdp[] = {
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_C2_CSIS,
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_CSIS0,
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_CSIS1,
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_CSIS2,
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_CSIS3,
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_CSIS4,
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_CSIS_DMA,
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_PDP_TOP,
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_CSIS5,
};
enum clk_id cmucal_vclk_ip_ppmu_csis_dma0_csis[] = {
	GOUT_BLK_CSIS_UID_PPMU_CSIS_DMA0_CSIS_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_PPMU_CSIS_DMA0_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_csis_dma1_csis[] = {
	GOUT_BLK_CSIS_UID_PPMU_CSIS_DMA1_CSIS_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_PPMU_CSIS_DMA1_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_csis[] = {
	GOUT_BLK_CSIS_UID_SYSMMU_D0_CSIS_IPCLKPORT_CLK_S1,
	GOUT_BLK_CSIS_UID_SYSMMU_D0_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_csis[] = {
	GOUT_BLK_CSIS_UID_SYSMMU_D1_CSIS_IPCLKPORT_CLK_S1,
	GOUT_BLK_CSIS_UID_SYSMMU_D1_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_csis[] = {
	GOUT_BLK_CSIS_UID_SYSREG_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_csis[] = {
	GOUT_BLK_CSIS_UID_VGEN_LITE_CSIS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_csis[] = {
	GOUT_BLK_CSIS_UID_LHM_AXI_P_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf0_csisipp[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_OTF0_CSISIPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_vo_mcsccsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_VO_MCSCCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_vo_csisipp[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_VO_CSISIPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_zotf0_ippcsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_ZOTF0_IPPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_zotf1_ippcsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_ZOTF1_IPPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_zotf2_ippcsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_ZOTF2_IPPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_csis0[] = {
	GOUT_BLK_CSIS_UID_AD_APB_CSIS0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_xiu_d0_csis[] = {
	GOUT_BLK_CSIS_UID_XIU_D0_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_csis[] = {
	GOUT_BLK_CSIS_UID_XIU_D1_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_csis_dma1[] = {
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA1_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_zsl[] = {
	GOUT_BLK_CSIS_UID_QE_ZSL_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_QE_ZSL_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_strp[] = {
	GOUT_BLK_CSIS_UID_QE_STRP_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_QE_STRP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_stat[] = {
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf1_csisipp[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_OTF1_CSISIPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf2_csisipp[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_OTF2_CSISIPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_sotf0_ippcsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_SOTF0_IPPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_sotf1_ippcsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_SOTF1_IPPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_sotf2_ippcsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_SOTF2_IPPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_zsl_csis[] = {
	GOUT_BLK_CSIS_UID_PPMU_ZSL_CSIS_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_PPMU_ZSL_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_pdp_stat_csis[] = {
	GOUT_BLK_CSIS_UID_PPMU_PDP_STAT_CSIS_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_PPMU_PDP_STAT_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_strp_csis[] = {
	GOUT_BLK_CSIS_UID_PPMU_STRP_CSIS_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_PPMU_STRP_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ois_mcu_top[] = {
	GOUT_BLK_CSIS_UID_OIS_MCU_TOP_IPCLKPORT_I_DCLK,
	GOUT_BLK_CSIS_UID_OIS_MCU_TOP_IPCLKPORT_I_HCLK,
	GOUT_BLK_CSIS_UID_OIS_MCU_TOP_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_ad_axi_ois_mcu_top[] = {
	GOUT_BLK_CSIS_UID_AD_AXI_OIS_MCU_TOP_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_qe_csis_dma0[] = {
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA0_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_csisperic1[] = {
	GOUT_BLK_CSIS_UID_LHS_AXI_P_CSISPERIC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dnc_cmu_dnc[] = {
	CLK_BLK_DNC_UID_DNC_CMU_DNC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dnc[] = {
	GOUT_BLK_DNC_UID_D_TZPC_DNC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dncdsp0[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_P_DNCDSP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dncdsp1[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_P_DNCDSP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dncdsp0_sfr[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_D_DNCDSP0_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ip_npuc[] = {
	GOUT_BLK_DNC_UID_IP_NPUC_IPCLKPORT_I_ACLK,
	GOUT_BLK_DNC_UID_IP_NPUC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_dnc[] = {
	GOUT_BLK_DNC_UID_LHM_AXI_P_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dsp0dnc_sfr[] = {
	GOUT_BLK_DNC_UID_LHM_AXI_D_DSP0DNC_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dsp0dnc_cache[] = {
	GOUT_BLK_DNC_UID_LHM_AXI_D_DSP0DNC_CACHE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dsp1dnc_cache[] = {
	GOUT_BLK_DNC_UID_LHM_AXI_D_DSP1DNC_CACHE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dsp1dnc_sfr[] = {
	GOUT_BLK_DNC_UID_LHM_AXI_D_DSP1DNC_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npu0_unit0_done[] = {
	GOUT_BLK_DNC_UID_LHM_AST_D_NPU0_UNIT0_DONE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npu0_unit1_done[] = {
	GOUT_BLK_DNC_UID_LHM_AST_D_NPU0_UNIT1_DONE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npu1_unit0_done[] = {
	GOUT_BLK_DNC_UID_LHM_AST_D_NPU1_UNIT0_DONE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npu1_unit1_done[] = {
	GOUT_BLK_DNC_UID_LHM_AST_D_NPU1_UNIT1_DONE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_dnc[] = {
	GOUT_BLK_DNC_UID_VGEN_LITE_DNC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dnc0[] = {
	GOUT_BLK_DNC_UID_PPMU_DNC0_IPCLKPORT_PCLK,
	GOUT_BLK_DNC_UID_PPMU_DNC0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dnc1[] = {
	GOUT_BLK_DNC_UID_PPMU_DNC1_IPCLKPORT_PCLK,
	GOUT_BLK_DNC_UID_PPMU_DNC1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dnc2[] = {
	GOUT_BLK_DNC_UID_PPMU_DNC2_IPCLKPORT_PCLK,
	GOUT_BLK_DNC_UID_PPMU_DNC2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ip_dspc[] = {
	GOUT_BLK_DNC_UID_IP_DSPC_IPCLKPORT_I_CLK_HIGH,
	GOUT_BLK_DNC_UID_IP_DSPC_IPCLKPORT_I_CLK_MID,
	GOUT_BLK_DNC_UID_IP_DSPC_IPCLKPORT_I_CLK_LOW,
};
enum clk_id cmucal_vclk_ip_sysmmu_dnc0[] = {
	GOUT_BLK_DNC_UID_SYSMMU_DNC0_IPCLKPORT_CLK_S1,
	GOUT_BLK_DNC_UID_SYSMMU_DNC0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_dnc1[] = {
	GOUT_BLK_DNC_UID_SYSMMU_DNC1_IPCLKPORT_CLK_S1,
	GOUT_BLK_DNC_UID_SYSMMU_DNC1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_dnc2[] = {
	GOUT_BLK_DNC_UID_SYSMMU_DNC2_IPCLKPORT_CLK_S1,
	GOUT_BLK_DNC_UID_SYSMMU_DNC2_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_dnc[] = {
	GOUT_BLK_DNC_UID_SYSREG_DNC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_adm_dap_dnc[] = {
	GOUT_BLK_DNC_UID_ADM_DAP_DNC_IPCLKPORT_DAPCLKM,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dncdsp1_sfr[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_D_DNCDSP1_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npu0_unit0_setreg[] = {
	GOUT_BLK_DNC_UID_LHS_AST_D_NPU0_UNIT0_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npu0_unit1_setreg[] = {
	GOUT_BLK_DNC_UID_LHS_AST_D_NPU0_UNIT1_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npu1_unit0_setreg[] = {
	GOUT_BLK_DNC_UID_LHS_AST_D_NPU1_UNIT0_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npu1_unit1_setreg[] = {
	GOUT_BLK_DNC_UID_LHS_AST_D_NPU1_UNIT1_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_dnc0[] = {
	GOUT_BLK_DNC_UID_AD_APB_DNC0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d0_dnc[] = {
	GOUT_BLK_DNC_UID_LHS_ACEL_D0_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d1_dnc[] = {
	GOUT_BLK_DNC_UID_LHS_ACEL_D1_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d2_dnc[] = {
	GOUT_BLK_DNC_UID_LHS_ACEL_D2_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dspc_200[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_P_DSPC_200_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_dspc_800[] = {
	GOUT_BLK_DNC_UID_LHM_AXI_P_DSPC_800_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dspcnpuc_200[] = {
	GOUT_BLK_DNC_UID_LHM_AXI_D_DSPCNPUC_200_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_npucdspc_533[] = {
	GOUT_BLK_DNC_UID_LHM_AXI_D_NPUCDSPC_533_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dspcnpuc_800[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_D_DSPCNPUC_800_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_npucdspc_800[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_D_NPUCDSPC_800_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dncdsp0_dma[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_D_DNCDSP0_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dncdsp1_dma[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_D_DNCDSP1_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dns_cmu_dns[] = {
	CLK_BLK_DNS_UID_DNS_CMU_DNS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_dns[] = {
	GOUT_BLK_DNS_UID_AD_APB_DNS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ppmu_dns[] = {
	GOUT_BLK_DNS_UID_PPMU_DNS_IPCLKPORT_PCLK,
	GOUT_BLK_DNS_UID_PPMU_DNS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_dns[] = {
	GOUT_BLK_DNS_UID_SYSMMU_DNS_IPCLKPORT_CLK_S1,
	GOUT_BLK_DNS_UID_SYSMMU_DNS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_itpdns[] = {
	GOUT_BLK_DNS_UID_LHM_AXI_P_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dns[] = {
	GOUT_BLK_DNS_UID_LHS_AXI_D_DNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dns[] = {
	GOUT_BLK_DNS_UID_SYSREG_DNS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_dns[] = {
	GOUT_BLK_DNS_UID_VGEN_LITE_DNS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_dns[] = {
	GOUT_BLK_DNS_UID_DNS_IPCLKPORT_CLK,
	GOUT_BLK_DNS_UID_DNS_IPCLKPORT_CLK_C2COM,
	GOUT_BLK_DNS_UID_DNS_IPCLKPORT_CLK_C2COM,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf0_ippdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF0_IPPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf1_ippdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF1_IPPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf_itpdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf_tnrdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF_TNRDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf0_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF0_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf1_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF1_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf2_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF2_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf3_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF3_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf4_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF4_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_ctl_itpdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_CTL_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_ctl_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_CTL_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dns[] = {
	GOUT_BLK_DNS_UID_D_TZPC_DNS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_vo_dnstnr[] = {
	GOUT_BLK_DNS_UID_LHS_AST_VO_DNSTNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_vo_ippdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_VO_IPPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dpu_cmu_dpu[] = {
	CLK_BLK_DPU_UID_DPU_CMU_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dpu[] = {
	GOUT_BLK_DPU_UID_SYSREG_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_dpud0[] = {
	GOUT_BLK_DPU_UID_SYSMMU_DPUD0_IPCLKPORT_CLK_S1,
	GOUT_BLK_DPU_UID_SYSMMU_DPUD0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_dpu[] = {
	GOUT_BLK_DPU_UID_LHM_AXI_P_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_dpu[] = {
	GOUT_BLK_DPU_UID_LHS_AXI_D1_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_decon0[] = {
	GOUT_BLK_DPU_UID_AD_APB_DECON0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d2_dpu[] = {
	GOUT_BLK_DPU_UID_LHS_AXI_D2_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_dpud2[] = {
	GOUT_BLK_DPU_UID_SYSMMU_DPUD2_IPCLKPORT_CLK_S1,
	GOUT_BLK_DPU_UID_SYSMMU_DPUD2_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_dpud1[] = {
	GOUT_BLK_DPU_UID_SYSMMU_DPUD1_IPCLKPORT_CLK_S1,
	GOUT_BLK_DPU_UID_SYSMMU_DPUD1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ppmu_dpud0[] = {
	GOUT_BLK_DPU_UID_PPMU_DPUD0_IPCLKPORT_ACLK,
	GOUT_BLK_DPU_UID_PPMU_DPUD0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dpud1[] = {
	GOUT_BLK_DPU_UID_PPMU_DPUD1_IPCLKPORT_ACLK,
	GOUT_BLK_DPU_UID_PPMU_DPUD1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dpud2[] = {
	GOUT_BLK_DPU_UID_PPMU_DPUD2_IPCLKPORT_ACLK,
	GOUT_BLK_DPU_UID_PPMU_DPUD2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dpu[] = {
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_DPU_WB_MUX,
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_DECON,
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_DMA,
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_DPP,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_dpu[] = {
	GOUT_BLK_DPU_UID_LHS_AXI_D0_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dpu[] = {
	GOUT_BLK_DPU_UID_D_TZPC_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_npu_sram0[] = {
	GOUT_BLK_DSP_UID_LHM_AXI_D_NPU_SRAM0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dsp_cmu_dsp[] = {
	CLK_BLK_DSP_UID_DSP_CMU_DSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dsp[] = {
	GOUT_BLK_DSP_UID_SYSREG_DSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dspdnc_sfr[] = {
	GOUT_BLK_DSP_UID_LHS_AXI_D_DSPDNC_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ip_dsp[] = {
	GOUT_BLK_DSP_UID_IP_DSP_IPCLKPORT_I_CLK_HIGH,
	GOUT_BLK_DSP_UID_IP_DSP_IPCLKPORT_I_CLK_LOW,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dspdnc_cache[] = {
	GOUT_BLK_DSP_UID_LHS_AXI_D_DSPDNC_CACHE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_abm_apb_dsp_dbg[] = {
	GOUT_BLK_DSP_UID_ABM_APB_DSP_DBG_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_npu_sram1[] = {
	GOUT_BLK_DSP_UID_LHM_AXI_D_NPU_SRAM1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dncdsp_dma[] = {
	GOUT_BLK_DSP_UID_LHM_AXI_D_DNCDSP_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_dncdsp[] = {
	GOUT_BLK_DSP_UID_LHM_AXI_P_DNCDSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dncdsp_sfr[] = {
	GOUT_BLK_DSP_UID_LHM_AXI_D_DNCDSP_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dsp[] = {
	GOUT_BLK_DSP_UID_D_TZPC_DSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dsp1_cmu_dsp1[] = {
	CLK_BLK_DSP1_UID_DSP1_CMU_DSP1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_g2d_cmu_g2d[] = {
	CLK_BLK_G2D_UID_G2D_CMU_G2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d0_g2d[] = {
	GOUT_BLK_G2D_UID_PPMU_D0_G2D_IPCLKPORT_ACLK,
	GOUT_BLK_G2D_UID_PPMU_D0_G2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d1_g2d[] = {
	GOUT_BLK_G2D_UID_PPMU_D1_G2D_IPCLKPORT_ACLK,
	GOUT_BLK_G2D_UID_PPMU_D1_G2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_g2d[] = {
	GOUT_BLK_G2D_UID_SYSMMU_D0_G2D_IPCLKPORT_CLK_S1,
	GOUT_BLK_G2D_UID_SYSMMU_D0_G2D_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_g2d[] = {
	GOUT_BLK_G2D_UID_SYSREG_G2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d0_g2d[] = {
	GOUT_BLK_G2D_UID_LHS_ACEL_D0_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d1_g2d[] = {
	GOUT_BLK_G2D_UID_LHS_ACEL_D1_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_g2d[] = {
	GOUT_BLK_G2D_UID_LHM_AXI_P_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qe_jpeg[] = {
	GOUT_BLK_G2D_UID_QE_JPEG_IPCLKPORT_ACLK,
	GOUT_BLK_G2D_UID_QE_JPEG_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_mscl[] = {
	GOUT_BLK_G2D_UID_QE_MSCL_IPCLKPORT_ACLK,
	GOUT_BLK_G2D_UID_QE_MSCL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d2_g2d[] = {
	GOUT_BLK_G2D_UID_SYSMMU_D2_G2D_IPCLKPORT_CLK_S1,
	GOUT_BLK_G2D_UID_SYSMMU_D2_G2D_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ppmu_d2_g2d[] = {
	GOUT_BLK_G2D_UID_PPMU_D2_G2D_IPCLKPORT_ACLK,
	GOUT_BLK_G2D_UID_PPMU_D2_G2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d2_g2d[] = {
	GOUT_BLK_G2D_UID_LHS_ACEL_D2_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_g2d[] = {
	GOUT_BLK_G2D_UID_XIU_D_G2D_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_as_apb_astc[] = {
	GOUT_BLK_G2D_UID_AS_APB_ASTC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_qe_astc[] = {
	GOUT_BLK_G2D_UID_QE_ASTC_IPCLKPORT_ACLK,
	GOUT_BLK_G2D_UID_QE_ASTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_g2d[] = {
	GOUT_BLK_G2D_UID_VGEN_LITE_G2D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_g2d[] = {
	GOUT_BLK_G2D_UID_G2D_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_g2d[] = {
	GOUT_BLK_G2D_UID_SYSMMU_D1_G2D_IPCLKPORT_CLK_S1,
	GOUT_BLK_G2D_UID_SYSMMU_D1_G2D_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_jpeg[] = {
	GOUT_BLK_G2D_UID_JPEG_IPCLKPORT_I_SMFC_CLK,
};
enum clk_id cmucal_vclk_ip_mscl[] = {
	GOUT_BLK_G2D_UID_MSCL_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_astc[] = {
	GOUT_BLK_G2D_UID_ASTC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_jsqz[] = {
	GOUT_BLK_G2D_UID_QE_JSQZ_IPCLKPORT_ACLK,
	GOUT_BLK_G2D_UID_QE_JSQZ_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_g2d[] = {
	GOUT_BLK_G2D_UID_D_TZPC_G2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_jsqz[] = {
	GOUT_BLK_G2D_UID_JSQZ_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_as_apb_g2d[] = {
	GOUT_BLK_G2D_UID_AS_APB_G2D_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_g3d[] = {
	GOUT_BLK_G3D_UID_LHM_AXI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_busif_hpmg3d[] = {
	GOUT_BLK_G3D_UID_BUSIF_HPMG3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hpm_g3d[] = {
	CLK_BLK_G3D_UID_HPM_G3D_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_sysreg_g3d[] = {
	GOUT_BLK_G3D_UID_SYSREG_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_g3d_cmu_g3d[] = {
	CLK_BLK_G3D_UID_G3D_CMU_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_int_g3d[] = {
	GOUT_BLK_G3D_UID_LHS_AXI_P_INT_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_g3d[] = {
	GOUT_BLK_G3D_UID_VGEN_LITE_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_gpu[] = {
	CLK_BLK_G3D_UID_GPU_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_int_g3d[] = {
	GOUT_BLK_G3D_UID_LHM_AXI_P_INT_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gray2bin_g3d[] = {
	GOUT_BLK_G3D_UID_GRAY2BIN_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_g3d[] = {
	GOUT_BLK_G3D_UID_D_TZPC_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_add_apbif_g3d[] = {
	GOUT_BLK_G3D_UID_ADD_APBIF_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_add_g3d[] = {
	CLK_BLK_G3D_UID_ADD_G3D_IPCLKPORT_CLK,
	CLK_BLK_G3D_UID_ADD_G3D_IPCLKPORT_CH_CLK,
};
enum clk_id cmucal_vclk_ip_asb_g3d[] = {
	CLK_BLK_G3D_UID_ASB_G3D_IPCLKPORT_DDD_G3D_CK_IN,
};
enum clk_id cmucal_vclk_ip_usb31drd[] = {
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_I_USBDPPHY_REF_SOC_PLL,
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_I_USB31DRD_REF_CLK_40,
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_ACLK_PHYCTRL,
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_I_USBDPPHY_SCL_APB_PCLK,
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_I_USBPCS_APB_CLK,
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_BUS_CLK_EARLY,
};
enum clk_id cmucal_vclk_ip_dp_link[] = {
	GOUT_BLK_HSI0_UID_DP_LINK_IPCLKPORT_I_DP_GTC_CLK,
	GOUT_BLK_HSI0_UID_DP_LINK_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_hsi0_bus1[] = {
	GOUT_BLK_HSI0_UID_PPMU_HSI0_BUS1_IPCLKPORT_ACLK,
	GOUT_BLK_HSI0_UID_PPMU_HSI0_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_hsi0[] = {
	GOUT_BLK_HSI0_UID_LHS_ACEL_D_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_hsi0[] = {
	GOUT_BLK_HSI0_UID_VGEN_LITE_HSI0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_hsi0[] = {
	GOUT_BLK_HSI0_UID_D_TZPC_HSI0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_hsi0[] = {
	GOUT_BLK_HSI0_UID_LHM_AXI_P_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_usb[] = {
	GOUT_BLK_HSI0_UID_SYSMMU_USB_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_hsi0[] = {
	GOUT_BLK_HSI0_UID_SYSREG_HSI0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hsi0_cmu_hsi0[] = {
	CLK_BLK_HSI0_UID_HSI0_CMU_HSI0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_hsi1[] = {
	GOUT_BLK_HSI1_UID_SYSMMU_HSI1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_hsi1_cmu_hsi1[] = {
	GOUT_BLK_HSI1_UID_HSI1_CMU_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mmc_card[] = {
	GOUT_BLK_HSI1_UID_MMC_CARD_IPCLKPORT_SDCLKIN,
	GOUT_BLK_HSI1_UID_MMC_CARD_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_pcie_gen2[] = {
	GOUT_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_IEEE1500_WRAPPER_FOR_PCIEG2_PHY_X1_INST_0_I_SCL_APB_PCLK,
	CLK_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PHY_REFCLK_IN,
	GOUT_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_SLV_ACLK,
	GOUT_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_DBI_ACLK,
	GOUT_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PCIE_SUB_CTRL_INST_0_I_DRIVER_APB_CLK,
	GOUT_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PIPE2_DIGITAL_X1_WRAP_INST_0_I_APB_PCLK_SCL,
	GOUT_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_MSTR_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_hsi1[] = {
	GOUT_BLK_HSI1_UID_SYSREG_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_hsi1[] = {
	GOUT_BLK_HSI1_UID_GPIO_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_hsi1[] = {
	GOUT_BLK_HSI1_UID_LHS_ACEL_D_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_hsi1[] = {
	GOUT_BLK_HSI1_UID_LHM_AXI_P_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_hsi1[] = {
	GOUT_BLK_HSI1_UID_XIU_D_HSI1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_p_hsi1[] = {
	GOUT_BLK_HSI1_UID_XIU_P_HSI1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_hsi1[] = {
	GOUT_BLK_HSI1_UID_PPMU_HSI1_IPCLKPORT_ACLK,
	GOUT_BLK_HSI1_UID_PPMU_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ufs_card[] = {
	GOUT_BLK_HSI1_UID_UFS_CARD_IPCLKPORT_I_CLK_UNIPRO,
	GOUT_BLK_HSI1_UID_UFS_CARD_IPCLKPORT_I_ACLK,
	GOUT_BLK_HSI1_UID_UFS_CARD_IPCLKPORT_I_FMP_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_hsi1[] = {
	GOUT_BLK_HSI1_UID_VGEN_LITE_HSI1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_pcie_ia_gen2[] = {
	GOUT_BLK_HSI1_UID_PCIE_IA_GEN2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_hsi1[] = {
	GOUT_BLK_HSI1_UID_D_TZPC_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ufs_embd[] = {
	GOUT_BLK_HSI1_UID_UFS_EMBD_IPCLKPORT_I_ACLK,
	GOUT_BLK_HSI1_UID_UFS_EMBD_IPCLKPORT_I_FMP_CLK,
	GOUT_BLK_HSI1_UID_UFS_EMBD_IPCLKPORT_I_CLK_UNIPRO,
};
enum clk_id cmucal_vclk_ip_pcie_gen4_0[] = {
	GOUT_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCIE_001_PCIE_SUB_CTRL_INST_0_I_DRIVER_APB_CLK,
	CLK_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCIE_001_PCIE_SUB_CTRL_INST_0_PHY_REFCLK_IN,
	GOUT_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCIE_001_G4X2_DWC_PCIE_CTL_INST_0_DBI_ACLK_UG,
	GOUT_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCIE_001_G4X2_DWC_PCIE_CTL_INST_0_MSTR_ACLK_UG,
	GOUT_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCIE_001_G4X2_DWC_PCIE_CTL_INST_0_SLV_ACLK_UG,
	GOUT_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCS_PMA_INST_0_PIPE_PAL_PCIE_INST_0_I_APB_PCLK,
	GOUT_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCS_PMA_INST_0_SF_PCIEPHY000X2_LN07LPP_QCH_TM_WRAPPER_INST_0_I_APB_PCLK,
};
enum clk_id cmucal_vclk_ip_pcie_ia_gen4_0[] = {
	GOUT_BLK_HSI1_UID_PCIE_IA_GEN4_0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_hsi2_cmu_hsi2[] = {
	CLK_BLK_HSI2_UID_HSI2_CMU_HSI2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_hsi2[] = {
	GOUT_BLK_HSI2_UID_LHS_ACEL_D_HSI2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_hsi2[] = {
	GOUT_BLK_HSI2_UID_LHM_AXI_P_HSI2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gpio_hsi2[] = {
	GOUT_BLK_HSI2_UID_GPIO_HSI2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_hsi2[] = {
	GOUT_BLK_HSI2_UID_SYSREG_HSI2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_pcie_gen4_1[] = {
	GOUT_BLK_HSI2_UID_VGEN_LITE_PCIE_GEN4_1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_hsi2[] = {
	GOUT_BLK_HSI2_UID_PPMU_HSI2_IPCLKPORT_ACLK,
	GOUT_BLK_HSI2_UID_PPMU_HSI2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_hsi2[] = {
	GOUT_BLK_HSI2_UID_SYSMMU_HSI2_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_pcie_gen4_1[] = {
	GOUT_BLK_HSI2_UID_PCIE_GEN4_1_IPCLKPORT_PCS_PMA_INST_0_PIPE_PAL_PCIE_INST_0_I_APB_PCLK,
	GOUT_BLK_HSI2_UID_PCIE_GEN4_1_IPCLKPORT_PCIE_001_G4X2_DWC_PCIE_CTL_INST_0_DBI_ACLK_UG,
	GOUT_BLK_HSI2_UID_PCIE_GEN4_1_IPCLKPORT_PCIE_001_G4X2_DWC_PCIE_CTL_INST_0_MSTR_ACLK_UG,
	GOUT_BLK_HSI2_UID_PCIE_GEN4_1_IPCLKPORT_PCIE_001_G4X2_DWC_PCIE_CTL_INST_0_SLV_ACLK_UG,
	GOUT_BLK_HSI2_UID_PCIE_GEN4_1_IPCLKPORT_PCIE_001_PCIE_SUB_CTRL_INST_0_I_DRIVER_APB_CLK,
	GOUT_BLK_HSI2_UID_PCIE_GEN4_1_IPCLKPORT_PCIE_001_PCIE_SUB_CTRL_INST_0_PHY_REFCLK_IN,
	GOUT_BLK_HSI2_UID_PCIE_GEN4_1_IPCLKPORT_PCS_PMA_INST_0_SF_PCIEPHY000X2_LN07LPP_QCH_TM_WRAPPER_INST_0_I_APB_PCLK,
};
enum clk_id cmucal_vclk_ip_pcie_ia_gen4_1[] = {
	GOUT_BLK_HSI2_UID_PCIE_IA_GEN4_1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_hsi2[] = {
	GOUT_BLK_HSI2_UID_D_TZPC_HSI2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_p_hsi2[] = {
	GOUT_BLK_HSI2_UID_XIU_P_HSI2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_ipp[] = {
	GOUT_BLK_IPP_UID_LHS_AXI_D_IPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_ipp[] = {
	GOUT_BLK_IPP_UID_LHM_AXI_P_IPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_ipp[] = {
	GOUT_BLK_IPP_UID_SYSREG_IPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ipp_cmu_ipp[] = {
	CLK_BLK_IPP_UID_IPP_CMU_IPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf0_ippdns[] = {
	GOUT_BLK_IPP_UID_LHS_AST_OTF0_IPPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_ipp[] = {
	GOUT_BLK_IPP_UID_D_TZPC_IPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf0_csisipp[] = {
	GOUT_BLK_IPP_UID_LHM_AST_OTF0_CSISIPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf1_ippdns[] = {
	GOUT_BLK_IPP_UID_LHS_AST_OTF1_IPPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sipu_ipp[] = {
	GOUT_BLK_IPP_UID_SIPU_IPP_IPCLKPORT_CLK,
	GOUT_BLK_IPP_UID_SIPU_IPP_IPCLKPORT_CLK_C2COM,
	GOUT_BLK_IPP_UID_SIPU_IPP_IPCLKPORT_CLK_C2COM,
};
enum clk_id cmucal_vclk_ip_ad_apb_ipp[] = {
	GOUT_BLK_IPP_UID_AD_APB_IPP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhs_ast_zotf0_ippcsis[] = {
	GOUT_BLK_IPP_UID_LHS_AST_ZOTF0_IPPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_zotf1_ippcsis[] = {
	GOUT_BLK_IPP_UID_LHS_AST_ZOTF1_IPPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_zotf2_ippcsis[] = {
	GOUT_BLK_IPP_UID_LHS_AST_ZOTF2_IPPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_ipp[] = {
	GOUT_BLK_IPP_UID_PPMU_IPP_IPCLKPORT_ACLK,
	GOUT_BLK_IPP_UID_PPMU_IPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_ipp[] = {
	GOUT_BLK_IPP_UID_SYSMMU_IPP_IPCLKPORT_CLK_S1,
	GOUT_BLK_IPP_UID_SYSMMU_IPP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_vgen_lite_ipp[] = {
	GOUT_BLK_IPP_UID_VGEN_LITE_IPP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf1_csisipp[] = {
	GOUT_BLK_IPP_UID_LHM_AST_OTF1_CSISIPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf2_csisipp[] = {
	GOUT_BLK_IPP_UID_LHM_AST_OTF2_CSISIPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_sotf0_ippcsis[] = {
	GOUT_BLK_IPP_UID_LHS_AST_SOTF0_IPPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_sotf1_ippcsis[] = {
	GOUT_BLK_IPP_UID_LHS_AST_SOTF1_IPPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_sotf2_ippcsis[] = {
	GOUT_BLK_IPP_UID_LHS_AST_SOTF2_IPPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_vo_csisipp[] = {
	GOUT_BLK_IPP_UID_LHM_AST_VO_CSISIPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_vo_ippdns[] = {
	GOUT_BLK_IPP_UID_LHS_AST_VO_IPPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_itp[] = {
	GOUT_BLK_ITP_UID_LHM_AXI_P_ITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_itp[] = {
	GOUT_BLK_ITP_UID_SYSREG_ITP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_itp_cmu_itp[] = {
	CLK_BLK_ITP_UID_ITP_CMU_ITP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf0_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF0_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf3_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF3_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf2_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF2_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_itp[] = {
	GOUT_BLK_ITP_UID_ITP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_itp[] = {
	GOUT_BLK_ITP_UID_D_TZPC_ITP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf1_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF1_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_itpdns[] = {
	GOUT_BLK_ITP_UID_LHS_AXI_P_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_itp[] = {
	GOUT_BLK_ITP_UID_AD_APB_ITP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf4_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF4_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_ctl_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_CTL_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_ctl_itpdns[] = {
	GOUT_BLK_ITP_UID_LHS_AST_CTL_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf0_itpmcsc[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF0_ITPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf1_itpmcsc[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF1_ITPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf2_itpmcsc[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF2_ITPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf3_itpmcsc[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF3_ITPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf_itpdns[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mcsc_cmu_mcsc[] = {
	CLK_BLK_MCSC_UID_MCSC_CMU_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mcsc[] = {
	GOUT_BLK_MCSC_UID_LHS_AXI_D0_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AXI_P_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_mcsc[] = {
	GOUT_BLK_MCSC_UID_XIU_D0_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mcsc[] = {
	GOUT_BLK_MCSC_UID_SYSREG_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mcsc[] = {
	GOUT_BLK_MCSC_UID_MCSC_IPCLKPORT_CLK,
	GOUT_BLK_MCSC_UID_MCSC_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mcsc[] = {
	GOUT_BLK_MCSC_UID_PPMU_MCSC_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_PPMU_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_meip[] = {
	GOUT_BLK_MCSC_UID_PPMU_MEIP_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_PPMU_MEIP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_mcsc[] = {
	GOUT_BLK_MCSC_UID_SYSMMU_D0_MCSC_IPCLKPORT_CLK_S1,
	GOUT_BLK_MCSC_UID_SYSMMU_D0_MCSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mcsc[] = {
	GOUT_BLK_MCSC_UID_D_TZPC_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_itsc[] = {
	GOUT_BLK_MCSC_UID_QE_ITSC_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_QE_ITSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_mcsc[] = {
	GOUT_BLK_MCSC_UID_QE_MCSC_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_QE_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_mcsc[] = {
	GOUT_BLK_MCSC_UID_VGEN_LITE_MCSC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_mcsc[] = {
	GOUT_BLK_MCSC_UID_AD_APB_MCSC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_qe_meip[] = {
	GOUT_BLK_MCSC_UID_QE_MEIP_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_QE_MEIP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_gdc[] = {
	GOUT_BLK_MCSC_UID_QE_GDC_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_QE_GDC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_itsc[] = {
	GOUT_BLK_MCSC_UID_ITSC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_itsc[] = {
	GOUT_BLK_MCSC_UID_PPMU_ITSC_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_PPMU_ITSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_gdc[] = {
	GOUT_BLK_MCSC_UID_PPMU_GDC_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_PPMU_GDC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_mcsc[] = {
	GOUT_BLK_MCSC_UID_XIU_D1_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_meip[] = {
	GOUT_BLK_MCSC_UID_MEIP_IPCLKPORT_CLK,
	GOUT_BLK_MCSC_UID_MEIP_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_gdc[] = {
	GOUT_BLK_MCSC_UID_GDC_IPCLKPORT_CLK,
	GOUT_BLK_MCSC_UID_GDC_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_ad_axi_meip[] = {
	GOUT_BLK_MCSC_UID_AD_AXI_MEIP_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_ad_axi_gdc[] = {
	GOUT_BLK_MCSC_UID_AD_AXI_GDC_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_mcsc[] = {
	GOUT_BLK_MCSC_UID_SYSMMU_D1_MCSC_IPCLKPORT_CLK_S1,
	GOUT_BLK_MCSC_UID_SYSMMU_D1_MCSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mcsc[] = {
	GOUT_BLK_MCSC_UID_LHS_AXI_D1_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf0_itpmcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AST_OTF0_ITPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf1_itpmcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AST_OTF1_ITPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf2_itpmcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AST_OTF2_ITPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf3_itpmcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AST_OTF3_ITPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_vo_tnrmcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AST_VO_TNRMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_vo_mcsccsis[] = {
	GOUT_BLK_MCSC_UID_LHS_AST_VO_MCSCCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_int_gdcmeip[] = {
	GOUT_BLK_MCSC_UID_LHM_AST_INT_GDCMEIP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_int_meipmcsc[] = {
	GOUT_BLK_MCSC_UID_LHS_AST_INT_MEIPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_int_meipmcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AST_INT_MEIPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_int_gdcmeip[] = {
	GOUT_BLK_MCSC_UID_LHS_AST_INT_GDCMEIP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_gdc[] = {
	GOUT_BLK_MCSC_UID_AD_APB_GDC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_meip[] = {
	GOUT_BLK_MCSC_UID_AD_APB_MEIP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_c2agent_mcsc[] = {
	GOUT_BLK_MCSC_UID_C2AGENT_MCSC_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_mfc0_cmu_mfc0[] = {
	CLK_BLK_MFC0_UID_MFC0_CMU_MFC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_apb_mfc0[] = {
	GOUT_BLK_MFC0_UID_AS_APB_MFC0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysreg_mfc0[] = {
	GOUT_BLK_MFC0_UID_SYSREG_MFC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mfc0[] = {
	GOUT_BLK_MFC0_UID_LHS_AXI_D0_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mfc0[] = {
	GOUT_BLK_MFC0_UID_LHS_AXI_D1_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mfc0[] = {
	GOUT_BLK_MFC0_UID_LHM_AXI_P_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfc0d0[] = {
	GOUT_BLK_MFC0_UID_SYSMMU_MFC0D0_IPCLKPORT_CLK_S1,
	GOUT_BLK_MFC0_UID_SYSMMU_MFC0D0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfc0d1[] = {
	GOUT_BLK_MFC0_UID_SYSMMU_MFC0D1_IPCLKPORT_CLK_S1,
	GOUT_BLK_MFC0_UID_SYSMMU_MFC0D1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ppmu_mfc0d0[] = {
	GOUT_BLK_MFC0_UID_PPMU_MFC0D0_IPCLKPORT_ACLK,
	GOUT_BLK_MFC0_UID_PPMU_MFC0D0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mfc0d1[] = {
	GOUT_BLK_MFC0_UID_PPMU_MFC0D1_IPCLKPORT_ACLK,
	GOUT_BLK_MFC0_UID_PPMU_MFC0D1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_axi_wfd[] = {
	GOUT_BLK_MFC0_UID_AS_AXI_WFD_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_ppmu_wfd[] = {
	GOUT_BLK_MFC0_UID_PPMU_WFD_IPCLKPORT_PCLK,
	GOUT_BLK_MFC0_UID_PPMU_WFD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_mfc0[] = {
	GOUT_BLK_MFC0_UID_XIU_D_MFC0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_vgen_mfc0[] = {
	GOUT_BLK_MFC0_UID_VGEN_MFC0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mfc0[] = {
	GOUT_BLK_MFC0_UID_MFC0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_wfd[] = {
	GOUT_BLK_MFC0_UID_WFD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_mfc0[] = {
	GOUT_BLK_MFC0_UID_LH_ATB_MFC0_IPCLKPORT_I_CLK_MI,
	GOUT_BLK_MFC0_UID_LH_ATB_MFC0_IPCLKPORT_I_CLK_SI,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mfc0[] = {
	GOUT_BLK_MFC0_UID_D_TZPC_MFC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mif_cmu_mif[] = {
	CLK_BLK_MIF_UID_MIF_CMU_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ddrphy[] = {
	GOUT_BLK_MIF_UID_DDRPHY_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mif[] = {
	GOUT_BLK_MIF_UID_SYSREG_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mif[] = {
	GOUT_BLK_MIF_UID_LHM_AXI_P_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_apbbr_ddrphy[] = {
	GOUT_BLK_MIF_UID_APBBR_DDRPHY_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbbr_dmc[] = {
	GOUT_BLK_MIF_UID_APBBR_DMC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmc[] = {
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK,
	CLK_BLK_MIF_UID_DMC_IPCLKPORT_SOC_CLK,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_SOC_MPACE_CLK,
};
enum clk_id cmucal_vclk_ip_qch_adapter_ppc_debug[] = {
	GOUT_BLK_MIF_UID_QCH_ADAPTER_PPC_DEBUG_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qch_adapter_ppc_dvfs[] = {
	GOUT_BLK_MIF_UID_QCH_ADAPTER_PPC_DVFS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mif[] = {
	GOUT_BLK_MIF_UID_D_TZPC_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_debug[] = {
	CLK_BLK_MIF_UID_PPC_DEBUG_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppc_dvfs[] = {
	CLK_BLK_MIF_UID_PPC_DVFS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mif1_cmu_mif1[] = {
	CLK_BLK_MIF1_UID_MIF1_CMU_MIF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbbr_ddrphy1[] = {
	GOUT_BLK_MIF1_UID_APBBR_DDRPHY1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbbr_dmc1[] = {
	GOUT_BLK_MIF1_UID_APBBR_DMC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbbr_dmctz1[] = {
	GOUT_BLK_MIF1_UID_APBBR_DMCTZ1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_axi2apb_mif1[] = {
	GOUT_BLK_MIF1_UID_AXI2APB_MIF1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ddrphy1[] = {
	GOUT_BLK_MIF1_UID_DDRPHY1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmc1[] = {
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_PCLK1,
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_PCLK2,
	CLK_BLK_MIF1_UID_DMC1_IPCLKPORT_SOC_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mif1[] = {
	GOUT_BLK_MIF1_UID_LHM_AXI_P_MIF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmuppc_debug1[] = {
	GOUT_BLK_MIF1_UID_PPMUPPC_DEBUG1_IPCLKPORT_PCLK,
	CLK_BLK_MIF1_UID_PPMUPPC_DEBUG1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmuppc_dvfs1[] = {
	GOUT_BLK_MIF1_UID_PPMUPPC_DVFS1_IPCLKPORT_PCLK,
	CLK_BLK_MIF1_UID_PPMUPPC_DVFS1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mif1[] = {
	GOUT_BLK_MIF1_UID_SYSREG_MIF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qch_adapter_ppmuppc_debug1[] = {
	GOUT_BLK_MIF1_UID_QCH_ADAPTER_PPMUPPC_DEBUG1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qch_adapter_ppmuppc_dvfs1[] = {
	GOUT_BLK_MIF1_UID_QCH_ADAPTER_PPMUPPC_DVFS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbbr_ddrphy2[] = {
	GOUT_BLK_MIF2_UID_APBBR_DDRPHY2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbbr_dmc2[] = {
	GOUT_BLK_MIF2_UID_APBBR_DMC2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbbr_dmctz2[] = {
	GOUT_BLK_MIF2_UID_APBBR_DMCTZ2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_axi2apb_mif2[] = {
	GOUT_BLK_MIF2_UID_AXI2APB_MIF2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ddrphy2[] = {
	GOUT_BLK_MIF2_UID_DDRPHY2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmc2[] = {
	GOUT_BLK_MIF2_UID_DMC2_IPCLKPORT_PCLK1,
	GOUT_BLK_MIF2_UID_DMC2_IPCLKPORT_PCLK2,
	CLK_BLK_MIF2_UID_DMC2_IPCLKPORT_SOC_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mif2[] = {
	GOUT_BLK_MIF2_UID_LHM_AXI_P_MIF2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmuppc_debug2[] = {
	GOUT_BLK_MIF2_UID_PPMUPPC_DEBUG2_IPCLKPORT_PCLK,
	CLK_BLK_MIF2_UID_PPMUPPC_DEBUG2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmuppc_dvfs2[] = {
	GOUT_BLK_MIF2_UID_PPMUPPC_DVFS2_IPCLKPORT_PCLK,
	CLK_BLK_MIF2_UID_PPMUPPC_DVFS2_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mif2[] = {
	GOUT_BLK_MIF2_UID_SYSREG_MIF2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qch_adapter_ppmuppc_debug2[] = {
	GOUT_BLK_MIF2_UID_QCH_ADAPTER_PPMUPPC_DEBUG2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qch_adapter_ppmuppc_dvfs2[] = {
	GOUT_BLK_MIF2_UID_QCH_ADAPTER_PPMUPPC_DVFS2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mif2_cmu_mif2[] = {
	CLK_BLK_MIF2_UID_MIF2_CMU_MIF2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbbr_ddrphy3[] = {
	GOUT_BLK_MIF3_UID_APBBR_DDRPHY3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbbr_dmc3[] = {
	GOUT_BLK_MIF3_UID_APBBR_DMC3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbbr_dmctz3[] = {
	GOUT_BLK_MIF3_UID_APBBR_DMCTZ3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_axi2apb_mif3[] = {
	GOUT_BLK_MIF3_UID_AXI2APB_MIF3_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ddrphy3[] = {
	GOUT_BLK_MIF3_UID_DDRPHY3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmc3[] = {
	GOUT_BLK_MIF3_UID_DMC3_IPCLKPORT_PCLK1,
	GOUT_BLK_MIF3_UID_DMC3_IPCLKPORT_PCLK2,
	CLK_BLK_MIF3_UID_DMC3_IPCLKPORT_SOC_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mif3[] = {
	GOUT_BLK_MIF3_UID_LHM_AXI_P_MIF3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmuppc_debug3[] = {
	GOUT_BLK_MIF3_UID_PPMUPPC_DEBUG3_IPCLKPORT_PCLK,
	CLK_BLK_MIF3_UID_PPMUPPC_DEBUG3_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmuppc_dvfs3[] = {
	GOUT_BLK_MIF3_UID_PPMUPPC_DVFS3_IPCLKPORT_PCLK,
	CLK_BLK_MIF3_UID_PPMUPPC_DVFS3_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mif3[] = {
	GOUT_BLK_MIF3_UID_SYSREG_MIF3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mif3_cmu_mif3[] = {
	CLK_BLK_MIF3_UID_MIF3_CMU_MIF3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qch_adapter_ppmuppc_debug3[] = {
	GOUT_BLK_MIF3_UID_QCH_ADAPTER_PPMUPPC_DEBUG3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qch_adapter_ppmuppc_dvfs3[] = {
	GOUT_BLK_MIF3_UID_QCH_ADAPTER_PPMUPPC_DVFS3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_npu_cmu_npu[] = {
	CLK_BLK_NPU_UID_NPU_CMU_NPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_npud_unit1[] = {
	GOUT_BLK_NPU_UID_NPUD_UNIT1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_npu[] = {
	GOUT_BLK_NPU_UID_D_TZPC_NPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d0_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D0_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d1_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D1_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d2_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D2_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d3_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D3_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d4_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D4_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d7_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D7_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d10_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D10_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d13_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D13_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npu_unit0_setreg[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D_NPU_UNIT0_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npu_unit1_setreg[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D_NPU_UNIT1_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d0_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D0_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d5_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D5_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d8_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D8_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d12_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D12_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d1_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D1_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d6_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D6_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d9_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D9_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d14_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D14_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npu_unit0_done[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D_NPU_UNIT0_DONE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_npu_sram1[] = {
	GOUT_BLK_NPU_UID_LHS_AXI_D_NPU_SRAM1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_npu_sram0[] = {
	GOUT_BLK_NPU_UID_LHS_AXI_D_NPU_SRAM0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_npud_unit0[] = {
	GOUT_BLK_NPU_UID_NPUD_UNIT0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_unit0[] = {
	GOUT_BLK_NPU_UID_PPMU_UNIT0_IPCLKPORT_ACLK,
	GOUT_BLK_NPU_UID_PPMU_UNIT0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_npu[] = {
	GOUT_BLK_NPU_UID_SYSREG_NPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_unit1[] = {
	GOUT_BLK_NPU_UID_PPMU_UNIT1_IPCLKPORT_ACLK,
	GOUT_BLK_NPU_UID_PPMU_UNIT1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AXI_P_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d5_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D5_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d8_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D8_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d11_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D11_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d14_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D14_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d6_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D6_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d9_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D9_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d12_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D12_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d15_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D15_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npu_unit1_done[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D_NPU_UNIT1_DONE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d4_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D4_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d2_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D2_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d3_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D3_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d7_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D7_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d10_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D10_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d11_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D11_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d13_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D13_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d15_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D15_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_npu1_cmu_npu1[] = {
	CLK_BLK_NPU1_UID_NPU1_CMU_NPU1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npud0_d1_0[] = {
	GOUT_BLK_NPU1_UID_LHM_AST_D_NPUD0_D1_0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_npu1[] = {
	GOUT_BLK_NPU1_UID_LHM_AXI_P_NPU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npud0_d1_1[] = {
	GOUT_BLK_NPU1_UID_LHM_AST_D_NPUD0_D1_1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npud0_d1_2[] = {
	GOUT_BLK_NPU1_UID_LHM_AST_D_NPUD0_D1_2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npud0_d1_3[] = {
	GOUT_BLK_NPU1_UID_LHM_AST_D_NPUD0_D1_3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npud0_d1_4[] = {
	GOUT_BLK_NPU1_UID_LHM_AST_D_NPUD0_D1_4_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npud0_d1_5[] = {
	GOUT_BLK_NPU1_UID_LHM_AST_D_NPUD0_D1_5_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npud0_d1_6[] = {
	GOUT_BLK_NPU1_UID_LHM_AST_D_NPUD0_D1_6_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npud0_d1_7[] = {
	GOUT_BLK_NPU1_UID_LHM_AST_D_NPUD0_D1_7_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_p_npud1_setreg[] = {
	GOUT_BLK_NPU1_UID_LHM_AST_P_NPUD1_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_idpsram1[] = {
	GOUT_BLK_NPU1_UID_LHM_AXI_D_IDPSRAM1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_idpsram3[] = {
	GOUT_BLK_NPU1_UID_LHM_AXI_D_IDPSRAM3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npud1_d1_0[] = {
	GOUT_BLK_NPU1_UID_LHS_AST_D_NPUD1_D1_0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npud1_d1_1[] = {
	GOUT_BLK_NPU1_UID_LHS_AST_D_NPUD1_D1_1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npud1_d1_2[] = {
	GOUT_BLK_NPU1_UID_LHS_AST_D_NPUD1_D1_2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npud1_d1_3[] = {
	GOUT_BLK_NPU1_UID_LHS_AST_D_NPUD1_D1_3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npud1_d1_4[] = {
	GOUT_BLK_NPU1_UID_LHS_AST_D_NPUD1_D1_4_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npud1_d1_5[] = {
	GOUT_BLK_NPU1_UID_LHS_AST_D_NPUD1_D1_5_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npud1_d1_6[] = {
	GOUT_BLK_NPU1_UID_LHS_AST_D_NPUD1_D1_6_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npud1_d1_7[] = {
	GOUT_BLK_NPU1_UID_LHS_AST_D_NPUD1_D1_7_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_npu1[] = {
	GOUT_BLK_NPU1_UID_SYSREG_NPU1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_p_npu1_done[] = {
	GOUT_BLK_NPU1_UID_LHS_AST_P_NPU1_DONE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_npu1[] = {
	GOUT_BLK_NPU1_UID_PPMU_NPU1_IPCLKPORT_ACLK,
	GOUT_BLK_NPU1_UID_PPMU_NPU1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_npu1_ppc_wrapper[] = {
	GOUT_BLK_NPU1_UID_NPU1_PPC_WRAPPER_IPCLKPORT_ACLK,
	GOUT_BLK_NPU1_UID_NPU1_PPC_WRAPPER_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_peric0[] = {
	GOUT_BLK_PERIC0_UID_GPIO_PERIC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_peric0[] = {
	GOUT_BLK_PERIC0_UID_SYSREG_PERIC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peric0_cmu_peric0[] = {
	CLK_BLK_PERIC0_UID_PERIC0_CMU_PERIC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_peric0[] = {
	GOUT_BLK_PERIC0_UID_LHM_AXI_P_PERIC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peric0[] = {
	GOUT_BLK_PERIC0_UID_D_TZPC_PERIC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peric0_top0[] = {
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_4,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_4,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_5,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_6,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_7,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_8,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_9,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_10,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_11,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_12,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_13,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_14,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_15,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_5,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_6,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_7,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_8,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_9,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_10,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_11,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_12,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_13,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_14,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_15,
};
enum clk_id cmucal_vclk_ip_peric0_top1[] = {
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_0,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_3,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_4,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_5,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_6,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_7,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_8,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_15,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_0,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_3,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_4,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_5,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_6,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_7,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_8,
};
enum clk_id cmucal_vclk_ip_gpio_peric1[] = {
	GOUT_BLK_PERIC1_UID_GPIO_PERIC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_peric1[] = {
	GOUT_BLK_PERIC1_UID_SYSREG_PERIC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peric1_cmu_peric1[] = {
	CLK_BLK_PERIC1_UID_PERIC1_CMU_PERIC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_peric1[] = {
	GOUT_BLK_PERIC1_UID_LHM_AXI_P_PERIC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peric1[] = {
	GOUT_BLK_PERIC1_UID_D_TZPC_PERIC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peric1_top0[] = {
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_PCLK_4,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_PCLK_10,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_PCLK_11,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_PCLK_12,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_PCLK_13,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_PCLK_14,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_PCLK_15,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_IPCLK_4,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_IPCLK_10,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_IPCLK_11,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_IPCLK_12,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_IPCLK_13,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_IPCLK_14,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_IPCLK_15,
};
enum clk_id cmucal_vclk_ip_peric1_top1[] = {
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_1,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_0,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_2,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_3,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_4,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_5,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_6,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_7,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_9,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_10,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_10,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_0,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_1,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_2,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_3,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_4,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_5,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_6,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_7,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_9,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_10,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_10,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_12,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_12,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_12,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_12,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_13,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_13,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_14,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_14,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_15,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_15,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_13,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_13,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_14,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_14,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_15,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_15,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_csisperic1[] = {
	GOUT_BLK_PERIC1_UID_LHM_AXI_P_CSISPERIC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_p_peric1[] = {
	GOUT_BLK_PERIC1_UID_XIU_P_PERIC1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_usi16_i3c[] = {
	GOUT_BLK_PERIC1_UID_USI16_I3C_IPCLKPORT_I_PCLK,
	GOUT_BLK_PERIC1_UID_USI16_I3C_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_usi17_i3c[] = {
	GOUT_BLK_PERIC1_UID_USI17_I3C_IPCLKPORT_I_SCLK,
	GOUT_BLK_PERIC1_UID_USI17_I3C_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_peris[] = {
	GOUT_BLK_PERIS_UID_SYSREG_PERIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_cluster2[] = {
	GOUT_BLK_PERIS_UID_WDT_CLUSTER2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_cluster0[] = {
	GOUT_BLK_PERIS_UID_WDT_CLUSTER0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peris_cmu_peris[] = {
	CLK_BLK_PERIS_UID_PERIS_CMU_PERIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_axi_p_peris[] = {
	GOUT_BLK_PERIS_UID_AD_AXI_P_PERIS_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_otp_con_bira[] = {
	GOUT_BLK_PERIS_UID_OTP_CON_BIRA_IPCLKPORT_PCLK,
	CLK_BLK_PERIS_UID_OTP_CON_BIRA_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_gic[] = {
	GOUT_BLK_PERIS_UID_GIC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_peris[] = {
	GOUT_BLK_PERIS_UID_LHM_AXI_P_PERIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mct[] = {
	GOUT_BLK_PERIS_UID_MCT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp_con_top[] = {
	GOUT_BLK_PERIS_UID_OTP_CON_TOP_IPCLKPORT_PCLK,
	CLK_BLK_PERIS_UID_OTP_CON_TOP_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peris[] = {
	GOUT_BLK_PERIS_UID_D_TZPC_PERIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_tmu_sub[] = {
	GOUT_BLK_PERIS_UID_TMU_SUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_tmu_top[] = {
	GOUT_BLK_PERIS_UID_TMU_TOP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp_con_bisr[] = {
	CLK_BLK_PERIS_UID_OTP_CON_BISR_IPCLKPORT_I_OSCCLK,
	GOUT_BLK_PERIS_UID_OTP_CON_BISR_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_s2d_cmu_s2d[] = {
	CLK_BLK_S2D_UID_S2D_CMU_S2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_bis_s2d[] = {
	GOUT_BLK_S2D_UID_BIS_S2D_IPCLKPORT_CLK,
	CLK_BLK_S2D_UID_BIS_S2D_IPCLKPORT_SCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_scan2dram[] = {
	GOUT_BLK_S2D_UID_LHM_AXI_G_SCAN2DRAM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ssp_cmu_ssp[] = {
	CLK_BLK_SSP_UID_SSP_CMU_SSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_ssp[] = {
	GOUT_BLK_SSP_UID_LHM_AXI_P_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_ssp[] = {
	GOUT_BLK_SSP_UID_D_TZPC_SSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_ssp[] = {
	GOUT_BLK_SSP_UID_SYSREG_SSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ss_sspcore[] = {
	GOUT_BLK_SSP_UID_SS_SSPCORE_IPCLKPORT_SS_SSPCORE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_tnr_cmu_tnr[] = {
	CLK_BLK_TNR_UID_TNR_CMU_TNR_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_tnr[] = {
	GOUT_BLK_TNR_UID_LHS_AXI_D_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_tnr[] = {
	GOUT_BLK_TNR_UID_LHM_AXI_P_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_vo_dnstnr[] = {
	GOUT_BLK_TNR_UID_LHM_AST_VO_DNSTNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_vo_tnrmcsc[] = {
	GOUT_BLK_TNR_UID_LHS_AST_VO_TNRMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf_tnrdns[] = {
	GOUT_BLK_TNR_UID_LHS_AST_OTF_TNRDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_tnr[] = {
	GOUT_BLK_TNR_UID_SYSREG_TNR_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_tnr[] = {
	GOUT_BLK_TNR_UID_D_TZPC_TNR_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_tnr[] = {
	GOUT_BLK_TNR_UID_VGEN_LITE_TNR_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_tnr[] = {
	GOUT_BLK_TNR_UID_PPMU_TNR_IPCLKPORT_PCLK,
	GOUT_BLK_TNR_UID_PPMU_TNR_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_tnr[] = {
	GOUT_BLK_TNR_UID_SYSMMU_TNR_IPCLKPORT_CLK_S1,
	GOUT_BLK_TNR_UID_SYSMMU_TNR_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_apb_async_tnr[] = {
	GOUT_BLK_TNR_UID_APB_ASYNC_TNR_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_tnr[] = {
	GOUT_BLK_TNR_UID_TNR_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_vra_cmu_vra[] = {
	CLK_BLK_VRA_UID_VRA_CMU_VRA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_vra[] = {
	GOUT_BLK_VRA_UID_AD_APB_VRA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_d_tzpc_vra[] = {
	GOUT_BLK_VRA_UID_D_TZPC_VRA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_vra[] = {
	GOUT_BLK_VRA_UID_LHM_AXI_P_VRA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qe_vra[] = {
	GOUT_BLK_VRA_UID_QE_VRA_IPCLKPORT_ACLK,
	GOUT_BLK_VRA_UID_QE_VRA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_vra[] = {
	GOUT_BLK_VRA_UID_SYSREG_VRA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_vra[] = {
	GOUT_BLK_VRA_UID_VGEN_LITE_VRA_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vra[] = {
	GOUT_BLK_VRA_UID_VRA_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_vra[] = {
	GOUT_BLK_VRA_UID_PPMU_VRA_IPCLKPORT_PCLK,
	GOUT_BLK_VRA_UID_PPMU_VRA_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_str[] = {
	GOUT_BLK_VRA_UID_STR_IPCLKPORT_I_STR_CLK,
};
enum clk_id cmucal_vclk_ip_ad_axi_str[] = {
	GOUT_BLK_VRA_UID_AD_AXI_STR_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_sysmmu_vra[] = {
	GOUT_BLK_VRA_UID_SYSMMU_VRA_IPCLKPORT_CLK_S1,
	GOUT_BLK_VRA_UID_SYSMMU_VRA_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_qe_str[] = {
	GOUT_BLK_VRA_UID_QE_STR_IPCLKPORT_PCLK,
	GOUT_BLK_VRA_UID_QE_STR_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_str[] = {
	GOUT_BLK_VRA_UID_PPMU_STR_IPCLKPORT_PCLK,
	GOUT_BLK_VRA_UID_PPMU_STR_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_vra[] = {
	GOUT_BLK_VRA_UID_XIU_D_VRA_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_vra[] = {
	GOUT_BLK_VRA_UID_LHS_AXI_D_VRA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_str[] = {
	GOUT_BLK_VRA_UID_AD_APB_STR_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ahb_busmatrix[] = {
	GOUT_BLK_VTS_UID_AHB_BUSMATRIX_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_dmic_if0[] = {
	GOUT_BLK_VTS_UID_DMIC_IF0_IPCLKPORT_DMIC_IF_CLK,
	CLK_BLK_VTS_UID_DMIC_IF0_IPCLKPORT_DMIC_IF_DIV2_CLK,
	GOUT_BLK_VTS_UID_DMIC_IF0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_vts[] = {
	GOUT_BLK_VTS_UID_SYSREG_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vts_cmu_vts[] = {
	CLK_BLK_VTS_UID_VTS_CMU_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_vts[] = {
	GOUT_BLK_VTS_UID_LHM_AXI_P_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gpio_vts[] = {
	GOUT_BLK_VTS_UID_GPIO_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_vts[] = {
	GOUT_BLK_VTS_UID_WDT_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb0[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB0_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb1[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB1_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_c_vts[] = {
	GOUT_BLK_VTS_UID_LHS_AXI_C_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_asyncinterrupt[] = {
	GOUT_BLK_VTS_UID_ASYNCINTERRUPT_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic0[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic1[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC1_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC1_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC1_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_ss_vts_glue[] = {
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_ACLK_CPU,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_AUD_PAD_CLK0,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_IF_PAD_CLK0,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_AUD_PAD_CLK1,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_AUD_PAD_CLK2,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_IF_PAD_CLK1,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_IF_PAD_CLK2,
};
enum clk_id cmucal_vclk_ip_cortexm4integration[] = {
	GOUT_BLK_VTS_UID_CORTEXM4INTEGRATION_IPCLKPORT_FCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_lp_vts[] = {
	GOUT_BLK_VTS_UID_LHM_AXI_LP_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_vts[] = {
	GOUT_BLK_VTS_UID_LHS_AXI_D_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_baaw_c_vts[] = {
	GOUT_BLK_VTS_UID_BAAW_C_VTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_vts[] = {
	GOUT_BLK_VTS_UID_D_TZPC_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite[] = {
	GOUT_BLK_VTS_UID_VGEN_LITE_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_bps_lp_vts[] = {
	GOUT_BLK_VTS_UID_BPS_LP_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bps_p_vts[] = {
	GOUT_BLK_VTS_UID_BPS_P_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sweeper_c_vts[] = {
	GOUT_BLK_VTS_UID_SWEEPER_C_VTS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_baaw_d_vts[] = {
	GOUT_BLK_VTS_UID_BAAW_D_VTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_abox_vts[] = {
	GOUT_BLK_VTS_UID_MAILBOX_ABOX_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb2[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB2_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB2_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb3[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB3_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB3_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic2[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC2_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC2_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC2_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic3[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC3_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC3_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC3_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_dmic_if2[] = {
	GOUT_BLK_VTS_UID_DMIC_IF2_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_IF2_IPCLKPORT_DMIC_IF_CLK,
	GOUT_BLK_VTS_UID_DMIC_IF2_IPCLKPORT_DMIC_IF_DIV2_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_vts[] = {
	GOUT_BLK_VTS_UID_MAILBOX_AP_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer[] = {
	GOUT_BLK_VTS_UID_TIMER_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pdma_vts[] = {
	GOUT_BLK_VTS_UID_PDMA_VTS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb4[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB4_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB4_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb5[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB5_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB5_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_aud0[] = {
	GOUT_BLK_VTS_UID_DMIC_AUD0_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_CLK,
	GOUT_BLK_VTS_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_DIV2_CLK,
};
enum clk_id cmucal_vclk_ip_dmic_if1[] = {
	GOUT_BLK_VTS_UID_DMIC_IF1_IPCLKPORT_DMIC_IF_CLK,
	GOUT_BLK_VTS_UID_DMIC_IF1_IPCLKPORT_DMIC_IF_DIV2_CLK,
	GOUT_BLK_VTS_UID_DMIC_IF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_aud1[] = {
	GOUT_BLK_VTS_UID_DMIC_AUD1_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_CLK,
	GOUT_BLK_VTS_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_DIV2_CLK,
};
enum clk_id cmucal_vclk_ip_dmic_aud2[] = {
	GOUT_BLK_VTS_UID_DMIC_AUD2_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AUD2_IPCLKPORT_DMIC_AUD_CLK,
	GOUT_BLK_VTS_UID_DMIC_AUD2_IPCLKPORT_DMIC_AUD_DIV2_CLK,
};
enum clk_id cmucal_vclk_ip_serial_lif[] = {
	GOUT_BLK_VTS_UID_SERIAL_LIF_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_SERIAL_LIF_IPCLKPORT_CLK,
	GOUT_BLK_VTS_UID_SERIAL_LIF_IPCLKPORT_BCLK,
	GOUT_BLK_VTS_UID_SERIAL_LIF_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic4[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC4_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC4_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC4_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic5[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC5_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC5_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC5_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_serial_lif[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_SERIAL_LIF_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_SERIAL_LIF_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_SERIAL_LIF_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_timer1[] = {
	GOUT_BLK_VTS_UID_TIMER1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer2[] = {
	GOUT_BLK_VTS_UID_TIMER2_IPCLKPORT_PCLK,
};

/* DVFS VCLK -> LUT List */
struct vclk_lut cmucal_vclk_vddi_lut[] = {
	{2333000, vddi_od_lut_params},
	{1866000, vddi_nm_lut_params},
	{1866000, vddi_sud_lut_params},
	{1866000, vddi_ud_lut_params},
	{1866000, vddi_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_mif_lut[] = {
	{5500000, vdd_mif_od_lut_params},
	{4266000, vdd_mif_nm_lut_params},
	{2688000, vdd_mif_ud_lut_params},
	{1420000, vdd_mif_sud_lut_params},
	{710000, vdd_mif_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cam_lut[] = {
	{300000, vdd_cam_ud_lut_params},
	{200000, vdd_cam_sud_lut_params},
	{100000, vdd_cam_nm_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cpucl0_lut[] = {
	{1950000, vdd_cpucl0_sod_lut_params},
	{1600000, vdd_cpucl0_od_lut_params},
	{1150000, vdd_cpucl0_nm_lut_params},
	{650000, vdd_cpucl0_ud_lut_params},
	{350000, vdd_cpucl0_sud_lut_params},
	{175000, vdd_cpucl0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cpucl1_lut[] = {
	{2399000, vdd_cpucl1_sod_lut_params},
	{1898000, vdd_cpucl1_od_lut_params},
	{1500000, vdd_cpucl1_nm_lut_params},
	{851000, vdd_cpucl1_ud_lut_params},
	{501000, vdd_cpucl1_sud_lut_params},
};

/* SPECIAL VCLK -> LUT List */
struct vclk_lut cmucal_vclk_mux_clk_apm_i3c_pmic_lut[] = {
	{200000, mux_clk_apm_i3c_pmic_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_apm_bus_lut[] = {
	{400000, clkcmu_apm_bus_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_apm_i3c_cp_lut[] = {
	{200000, mux_clk_apm_i3c_cp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_aud_dsif_lut[] = {
	{600000, mux_clk_aud_dsif_od_lut_params},
	{533250, mux_clk_aud_dsif_ud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_bus0_cmuref_lut[] = {
	{200000, mux_bus0_cmuref_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cmu_boost_lut[] = {
	{200000, clkcmu_cmu_boost_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_bus1_cmuref_lut[] = {
	{200000, mux_bus1_cmuref_nm_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_cmgp_adc_lut[] = {
	{26000, mux_clk_cmgp_adc_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cmgp_bus_lut[] = {
	{400000, clkcmu_cmgp_bus_nm_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cmu_cmuref_lut[] = {
	{533250, mux_cmu_cmuref_od_lut_params},
	{400000, mux_cmu_cmuref_ud_lut_params},
	{200000, mux_cmu_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_core_cmuref_lut[] = {
	{200000, mux_core_cmuref_nm_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cpucl0_cmuref_lut[] = {
	{200000, mux_cpucl0_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cpucl1_cmuref_lut[] = {
	{200000, mux_cpucl1_cmuref_ud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cpucl2_cmuref_lut[] = {
	{200000, mux_cpucl2_cmuref_usod_lut_params},
};
struct vclk_lut cmucal_vclk_mux_mif_cmuref_lut[] = {
	{266625, mux_mif_cmuref_uud_lut_params},
	{26000, mux_mif_cmuref_od_lut_params},
};
struct vclk_lut cmucal_vclk_mux_mif1_cmuref_lut[] = {
	{26000, mux_mif1_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_mif2_cmuref_lut[] = {
	{26000, mux_mif2_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_mif3_cmuref_lut[] = {
	{26000, mux_mif3_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_hsi0_usbdp_debug_lut[] = {
	{800000, mux_clkcmu_hsi0_usbdp_debug_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_hsi1_mmc_card_lut[] = {
	{808000, mux_clkcmu_hsi1_mmc_card_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_hsi1_ufs_card_lut[] = {
	{177750, clkcmu_hsi1_ufs_card_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_hsi2_pcie_lut[] = {
	{800000, mux_clkcmu_hsi2_pcie_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_i2c_cmgp0_lut[] = {
	{400000, div_clk_i2c_cmgp0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_usi_cmgp1_lut[] = {
	{400000, div_clk_usi_cmgp1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_usi_cmgp0_lut[] = {
	{400000, div_clk_usi_cmgp0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_usi_cmgp2_lut[] = {
	{400000, div_clk_usi_cmgp2_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_usi_cmgp3_lut[] = {
	{400000, div_clk_usi_cmgp3_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_i2c_cmgp1_lut[] = {
	{400000, div_clk_i2c_cmgp1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_i2c_cmgp2_lut[] = {
	{400000, div_clk_i2c_cmgp2_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_i2c_cmgp3_lut[] = {
	{400000, div_clk_i2c_cmgp3_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_dbgcore_uart_cmgp_lut[] = {
	{400000, div_clk_dbgcore_uart_cmgp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cpucl2_switch_lut[] = {
	{1333000, clkcmu_cpucl2_switch_od_lut_params},
	{1066500, clkcmu_cpucl2_switch_ud_lut_params},
	{533250, clkcmu_cpucl2_switch_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_hpm_lut[] = {
	{400000, clkcmu_hpm_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk0_lut[] = {
	{100000, clkcmu_cis_clk0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk1_lut[] = {
	{100000, clkcmu_cis_clk1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk2_lut[] = {
	{100000, clkcmu_cis_clk2_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk3_lut[] = {
	{100000, clkcmu_cis_clk3_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk4_lut[] = {
	{100000, clkcmu_cis_clk4_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk5_lut[] = {
	{100000, clkcmu_cis_clk5_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl0_cmuref_lut[] = {
	{975000, div_clk_cpucl0_cmuref_sod_lut_params},
	{800000, div_clk_cpucl0_cmuref_od_lut_params},
	{575000, div_clk_cpucl0_cmuref_nm_lut_params},
	{325000, div_clk_cpucl0_cmuref_ud_lut_params},
	{175000, div_clk_cpucl0_cmuref_sud_lut_params},
	{87500, div_clk_cpucl0_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cluster0_periphclk_lut[] = {
	{975000, div_clk_cluster0_periphclk_sod_lut_params},
	{800000, div_clk_cluster0_periphclk_od_lut_params},
	{575000, div_clk_cluster0_periphclk_nm_lut_params},
	{325000, div_clk_cluster0_periphclk_ud_lut_params},
	{175000, div_clk_cluster0_periphclk_sud_lut_params},
	{87500, div_clk_cluster0_periphclk_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl1_cmuref_lut[] = {
	{1199500, div_clk_cpucl1_cmuref_sod_lut_params},
	{949000, div_clk_cpucl1_cmuref_od_lut_params},
	{750000, div_clk_cpucl1_cmuref_nm_lut_params},
	{425500, div_clk_cpucl1_cmuref_ud_lut_params},
	{250500, div_clk_cpucl1_cmuref_sud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_dsp1_busp_lut[] = {
	{466500, div_clk_dsp1_busp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric0_usi00_usi_lut[] = {
	{400000, div_clk_peric0_usi00_usi_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_peric0_ip_lut[] = {
	{400000, clkcmu_peric0_ip_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric0_usi13_usi_lut[] = {
	{400000, div_clk_peric0_usi13_usi_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric1_uart_bt_lut[] = {
	{400000, div_clk_peric1_uart_bt_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_peric1_ip_lut[] = {
	{400000, clkcmu_peric1_ip_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric1_usi18_usi_lut[] = {
	{400000, div_clk_peric1_usi18_usi_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_vts_dmic_if_pad_lut[] = {
	{75000, div_clk_vts_dmic_if_pad_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_aud_dmic0_lut[] = {
	{300000, div_clk_aud_dmic0_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_aud_dmic1_lut[] = {
	{69000, div_clk_aud_dmic1_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_vts_serial_lif_lut[] = {
	{75000, div_clk_vts_serial_lif_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_pericx_usixx_usi_lut[] = {
	{400000, div_clk_peric_400_lut_params},
	{200000, div_clk_peric_200_lut_params},
	{133000, div_clk_peric_133_lut_params},
	{100000, div_clk_peric_100_lut_params},
	{66000, div_clk_peric_66_lut_params},
	{50000, div_clk_peric_50_lut_params},
	{26000, div_clk_peric_26_lut_params},
	{13000, div_clk_peric_13_lut_params},
};

/* COMMON VCLK -> LUT List */
struct vclk_lut cmucal_vclk_blk_aud_lut[] = {
	{200000, blk_aud_od_lut_params},
	{133333, blk_aud_ud_lut_params},
	{69000, blk_aud_sud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cmu_lut[] = {
	{1166000, blk_cmu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mif1_lut[] = {
	{8528000, blk_mif1_nm_lut_params},
	{26000, blk_mif1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mif2_lut[] = {
	{8528000, blk_mif2_nm_lut_params},
	{26000, blk_mif2_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mif3_lut[] = {
	{8528000, blk_mif3_nm_lut_params},
	{26000, blk_mif3_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_s2d_lut[] = {
	{200000, blk_s2d_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_apm_lut[] = {
	{400000, blk_apm_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cmgp_lut[] = {
	{400000, blk_cmgp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl0_lut[] = {
	{487500, blk_cpucl0_sod_lut_params},
	{400000, blk_cpucl0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl1_lut[] = {
	{2399000, blk_cpucl1_sod_lut_params},
	{1898000, blk_cpucl1_od_lut_params},
	{1500000, blk_cpucl1_nm_lut_params},
	{851000, blk_cpucl1_ud_lut_params},
	{501000, blk_cpucl1_sud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_g3d_lut[] = {
	{225000, blk_g3d_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_vts_lut[] = {
	{400000, blk_vts_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_bus0_lut[] = {
	{266625, blk_bus0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_bus1_lut[] = {
	{266625, blk_bus1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_core_lut[] = {
	{355500, blk_core_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_csis_lut[] = {
	{333250, blk_csis_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dnc_lut[] = {
	{200000, blk_dnc_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dns_lut[] = {
	{333250, blk_dns_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dpu_lut[] = {
	{166625, blk_dpu_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dsp_lut[] = {
	{466500, blk_dsp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_g2d_lut[] = {
	{266625, blk_g2d_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_ipp_lut[] = {
	{333250, blk_ipp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_itp_lut[] = {
	{333250, blk_itp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mcsc_lut[] = {
	{333250, blk_mcsc_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mfc0_lut[] = {
	{166625, blk_mfc0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_npu_lut[] = {
	{233250, blk_npu_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_npu1_lut[] = {
	{233250, blk_npu1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_peric0_lut[] = {
	{200000, blk_peric0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_peric1_lut[] = {
	{200000, blk_peric1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_ssp_lut[] = {
	{200000, blk_ssp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_tnr_lut[] = {
	{333250, blk_tnr_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_vra_lut[] = {
	{400000, blk_vra_nm_lut_params},
};

/* Switch VCLK -> LUT Parameter List */
struct switch_lut mux_clk_aud_cpu_lut[] = {
	{1066500, 0, 0},
	{666500, 2, 0},
	{400000, 1, 1},
};
/*================================ SWPLL List =================================*/
struct vclk_switch switch_blk_aud[] = {
	{MUX_CLK_AUD_CPU, MUX_CLKCMU_AUD_CPU, CLKCMU_AUD_CPU, GATE_CLKCMU_AUD_CPU, MUX_CLKCMU_AUD_CPU_USER, mux_clk_aud_cpu_lut, 3},
};

/*================================ VCLK List =================================*/
unsigned int cmucal_vclk_size = 925;
struct vclk cmucal_vclk_list[] = {

/* DVFS VCLK*/
	CMUCAL_VCLK(VCLK_VDDI, cmucal_vclk_vddi_lut, cmucal_vclk_vddi, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_MIF, cmucal_vclk_vdd_mif_lut, cmucal_vclk_vdd_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_CAM, cmucal_vclk_vdd_cam_lut, cmucal_vclk_vdd_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_CPUCL0, cmucal_vclk_vdd_cpucl0_lut, cmucal_vclk_vdd_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_CPUCL1, cmucal_vclk_vdd_cpucl1_lut, cmucal_vclk_vdd_cpucl1, NULL, NULL),

/* SPECIAL VCLK*/
	CMUCAL_VCLK(VCLK_MUX_CLK_APM_I3C_PMIC, cmucal_vclk_mux_clk_apm_i3c_pmic_lut, cmucal_vclk_mux_clk_apm_i3c_pmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_APM_BUS, cmucal_vclk_clkcmu_apm_bus_lut, cmucal_vclk_clkcmu_apm_bus, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_APM_I3C_CP, cmucal_vclk_mux_clk_apm_i3c_cp_lut, cmucal_vclk_mux_clk_apm_i3c_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_AUD_DSIF, cmucal_vclk_mux_clk_aud_dsif_lut, cmucal_vclk_mux_clk_aud_dsif, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_BUS0_CMUREF, cmucal_vclk_mux_bus0_cmuref_lut, cmucal_vclk_mux_bus0_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CMU_BOOST, cmucal_vclk_clkcmu_cmu_boost_lut, cmucal_vclk_clkcmu_cmu_boost, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_BUS1_CMUREF, cmucal_vclk_mux_bus1_cmuref_lut, cmucal_vclk_mux_bus1_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CMGP_ADC, cmucal_vclk_mux_clk_cmgp_adc_lut, cmucal_vclk_mux_clk_cmgp_adc, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CMGP_BUS, cmucal_vclk_clkcmu_cmgp_bus_lut, cmucal_vclk_clkcmu_cmgp_bus, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CMU_CMUREF, cmucal_vclk_mux_cmu_cmuref_lut, cmucal_vclk_mux_cmu_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CORE_CMUREF, cmucal_vclk_mux_core_cmuref_lut, cmucal_vclk_mux_core_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CPUCL0_CMUREF, cmucal_vclk_mux_cpucl0_cmuref_lut, cmucal_vclk_mux_cpucl0_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CPUCL1_CMUREF, cmucal_vclk_mux_cpucl1_cmuref_lut, cmucal_vclk_mux_cpucl1_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CPUCL2_CMUREF, cmucal_vclk_mux_cpucl2_cmuref_lut, cmucal_vclk_mux_cpucl2_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_MIF_CMUREF, cmucal_vclk_mux_mif_cmuref_lut, cmucal_vclk_mux_mif_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_MIF1_CMUREF, cmucal_vclk_mux_mif1_cmuref_lut, cmucal_vclk_mux_mif1_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_MIF2_CMUREF, cmucal_vclk_mux_mif2_cmuref_lut, cmucal_vclk_mux_mif2_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_MIF3_CMUREF, cmucal_vclk_mux_mif3_cmuref_lut, cmucal_vclk_mux_mif3_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_HSI0_USBDP_DEBUG, cmucal_vclk_mux_clkcmu_hsi0_usbdp_debug_lut, cmucal_vclk_mux_clkcmu_hsi0_usbdp_debug, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_HSI1_MMC_CARD, cmucal_vclk_mux_clkcmu_hsi1_mmc_card_lut, cmucal_vclk_mux_clkcmu_hsi1_mmc_card, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_HSI1_UFS_CARD, cmucal_vclk_clkcmu_hsi1_ufs_card_lut, cmucal_vclk_clkcmu_hsi1_ufs_card, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_HSI2_PCIE, cmucal_vclk_mux_clkcmu_hsi2_pcie_lut, cmucal_vclk_mux_clkcmu_hsi2_pcie, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_I2C_CMGP0, cmucal_vclk_div_clk_i2c_cmgp0_lut, cmucal_vclk_div_clk_i2c_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_USI_CMGP1, cmucal_vclk_div_clk_usi_cmgp1_lut, cmucal_vclk_div_clk_usi_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_USI_CMGP0, cmucal_vclk_div_clk_usi_cmgp0_lut, cmucal_vclk_div_clk_usi_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_USI_CMGP2, cmucal_vclk_div_clk_usi_cmgp2_lut, cmucal_vclk_div_clk_usi_cmgp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_USI_CMGP3, cmucal_vclk_div_clk_usi_cmgp3_lut, cmucal_vclk_div_clk_usi_cmgp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_I2C_CMGP1, cmucal_vclk_div_clk_i2c_cmgp1_lut, cmucal_vclk_div_clk_i2c_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_I2C_CMGP2, cmucal_vclk_div_clk_i2c_cmgp2_lut, cmucal_vclk_div_clk_i2c_cmgp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_I2C_CMGP3, cmucal_vclk_div_clk_i2c_cmgp3_lut, cmucal_vclk_div_clk_i2c_cmgp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_DBGCORE_UART_CMGP, cmucal_vclk_div_clk_dbgcore_uart_cmgp_lut, cmucal_vclk_div_clk_dbgcore_uart_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CPUCL2_SWITCH, cmucal_vclk_clkcmu_cpucl2_switch_lut, cmucal_vclk_clkcmu_cpucl2_switch, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_HPM, cmucal_vclk_clkcmu_hpm_lut, cmucal_vclk_clkcmu_hpm, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK0, cmucal_vclk_clkcmu_cis_clk0_lut, cmucal_vclk_clkcmu_cis_clk0, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK1, cmucal_vclk_clkcmu_cis_clk1_lut, cmucal_vclk_clkcmu_cis_clk1, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK2, cmucal_vclk_clkcmu_cis_clk2_lut, cmucal_vclk_clkcmu_cis_clk2, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK3, cmucal_vclk_clkcmu_cis_clk3_lut, cmucal_vclk_clkcmu_cis_clk3, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK4, cmucal_vclk_clkcmu_cis_clk4_lut, cmucal_vclk_clkcmu_cis_clk4, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK5, cmucal_vclk_clkcmu_cis_clk5_lut, cmucal_vclk_clkcmu_cis_clk5, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL0_CMUREF, cmucal_vclk_div_clk_cpucl0_cmuref_lut, cmucal_vclk_div_clk_cpucl0_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CLUSTER0_PERIPHCLK, cmucal_vclk_div_clk_cluster0_periphclk_lut, cmucal_vclk_div_clk_cluster0_periphclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL1_CMUREF, cmucal_vclk_div_clk_cpucl1_cmuref_lut, cmucal_vclk_div_clk_cpucl1_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_DSP1_BUSP, cmucal_vclk_div_clk_dsp1_busp_lut, cmucal_vclk_div_clk_dsp1_busp, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI00_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi00_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI01_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi01_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI02_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi02_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI03_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi03_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI04_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi04_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI05_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi05_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_UART_DBG, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_uart_dbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_PERIC0_IP, cmucal_vclk_clkcmu_peric0_ip_lut, cmucal_vclk_clkcmu_peric0_ip, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI13_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi13_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI14_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi14_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI15_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi15_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_UART_BT, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_uart_bt, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_PERIC1_IP, cmucal_vclk_clkcmu_peric1_ip_lut, cmucal_vclk_clkcmu_peric1_ip, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI06_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi06_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI07_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi07_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI08_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi08_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI09_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi09_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI10_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi10_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI11_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi11_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI12_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi12_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI16_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi16_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI17_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi17_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI18_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi18_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_VTS_DMIC_IF_PAD, cmucal_vclk_div_clk_vts_dmic_if_pad_lut, cmucal_vclk_div_clk_vts_dmic_if_pad, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_DMIC0, cmucal_vclk_div_clk_aud_dmic0_lut, cmucal_vclk_div_clk_aud_dmic0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_DMIC1, cmucal_vclk_div_clk_aud_dmic1_lut, cmucal_vclk_div_clk_aud_dmic1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_VTS_SERIAL_LIF, cmucal_vclk_div_clk_vts_serial_lif_lut, cmucal_vclk_div_clk_vts_serial_lif, NULL, NULL),

/* COMMON VCLK*/
	CMUCAL_VCLK(VCLK_BLK_AUD, cmucal_vclk_blk_aud_lut, cmucal_vclk_blk_aud, NULL, switch_blk_aud),
	CMUCAL_VCLK(VCLK_BLK_CMU, cmucal_vclk_blk_cmu_lut, cmucal_vclk_blk_cmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MIF1, cmucal_vclk_blk_mif1_lut, cmucal_vclk_blk_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MIF2, cmucal_vclk_blk_mif2_lut, cmucal_vclk_blk_mif2, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MIF3, cmucal_vclk_blk_mif3_lut, cmucal_vclk_blk_mif3, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_S2D, cmucal_vclk_blk_s2d_lut, cmucal_vclk_blk_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_APM, cmucal_vclk_blk_apm_lut, cmucal_vclk_blk_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CMGP, cmucal_vclk_blk_cmgp_lut, cmucal_vclk_blk_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL0, cmucal_vclk_blk_cpucl0_lut, cmucal_vclk_blk_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL1, cmucal_vclk_blk_cpucl1_lut, cmucal_vclk_blk_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_G3D, cmucal_vclk_blk_g3d_lut, cmucal_vclk_blk_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_VTS, cmucal_vclk_blk_vts_lut, cmucal_vclk_blk_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_BUS0, cmucal_vclk_blk_bus0_lut, cmucal_vclk_blk_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_BUS1, cmucal_vclk_blk_bus1_lut, cmucal_vclk_blk_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CORE, cmucal_vclk_blk_core_lut, cmucal_vclk_blk_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CSIS, cmucal_vclk_blk_csis_lut, cmucal_vclk_blk_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DNC, cmucal_vclk_blk_dnc_lut, cmucal_vclk_blk_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DNS, cmucal_vclk_blk_dns_lut, cmucal_vclk_blk_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DPU, cmucal_vclk_blk_dpu_lut, cmucal_vclk_blk_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DSP, cmucal_vclk_blk_dsp_lut, cmucal_vclk_blk_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_G2D, cmucal_vclk_blk_g2d_lut, cmucal_vclk_blk_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_IPP, cmucal_vclk_blk_ipp_lut, cmucal_vclk_blk_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_ITP, cmucal_vclk_blk_itp_lut, cmucal_vclk_blk_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MCSC, cmucal_vclk_blk_mcsc_lut, cmucal_vclk_blk_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MFC0, cmucal_vclk_blk_mfc0_lut, cmucal_vclk_blk_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_NPU, cmucal_vclk_blk_npu_lut, cmucal_vclk_blk_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_NPU1, cmucal_vclk_blk_npu1_lut, cmucal_vclk_blk_npu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_PERIC0, cmucal_vclk_blk_peric0_lut, cmucal_vclk_blk_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_PERIC1, cmucal_vclk_blk_peric1_lut, cmucal_vclk_blk_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_SSP, cmucal_vclk_blk_ssp_lut, cmucal_vclk_blk_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_TNR, cmucal_vclk_blk_tnr_lut, cmucal_vclk_blk_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_VRA, cmucal_vclk_blk_vra_lut, cmucal_vclk_blk_vra, NULL, NULL),

/* GATE VCLK*/
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_APM, NULL, cmucal_vclk_ip_lhs_axi_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_APM, NULL, cmucal_vclk_ip_lhm_axi_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_APM, NULL, cmucal_vclk_ip_wdt_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_APM, NULL, cmucal_vclk_ip_sysreg_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_AP, NULL, cmucal_vclk_ip_mailbox_apm_ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_PMU_ALIVE, NULL, cmucal_vclk_ip_apbif_pmu_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_INTMEM, NULL, cmucal_vclk_ip_intmem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_SCAN2DRAM, NULL, cmucal_vclk_ip_lhs_axi_g_scan2dram, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PMU_INTR_GEN, NULL, cmucal_vclk_ip_pmu_intr_gen, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PEM, NULL, cmucal_vclk_ip_pem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPEEDY_APM, NULL, cmucal_vclk_ip_speedy_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_APM, NULL, cmucal_vclk_ip_xiu_dp_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APM_CMU_APM, NULL, cmucal_vclk_ip_apm_cmu_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GREBEINTEGRATION, NULL, cmucal_vclk_ip_grebeintegration, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_GPIO_ALIVE, NULL, cmucal_vclk_ip_apbif_gpio_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_TOP_RTC, NULL, cmucal_vclk_ip_apbif_top_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SS_DBGCORE, NULL, cmucal_vclk_ip_ss_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DTZPC_APM, NULL, cmucal_vclk_ip_dtzpc_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_VTS, NULL, cmucal_vclk_ip_lhm_axi_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_VTS, NULL, cmucal_vclk_ip_mailbox_apm_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_DBGCORE, NULL, cmucal_vclk_ip_mailbox_ap_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_LP_VTS, NULL, cmucal_vclk_ip_lhs_axi_lp_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_DBGCORE, NULL, cmucal_vclk_ip_lhs_axi_g_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_RTC, NULL, cmucal_vclk_ip_apbif_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_C_CMGP, NULL, cmucal_vclk_ip_lhs_axi_c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_APM, NULL, cmucal_vclk_ip_vgen_lite_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPEEDY_SUB_APM, NULL, cmucal_vclk_ip_speedy_sub_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ROM_CRC32_HOST, NULL, cmucal_vclk_ip_rom_crc32_host, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C_APM_PMIC, NULL, cmucal_vclk_ip_i3c_apm_pmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C_APM_CP, NULL, cmucal_vclk_ip_i3c_apm_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AUD_CMU_AUD, NULL, cmucal_vclk_ip_aud_cmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_AUD, NULL, cmucal_vclk_ip_lhs_axi_d_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_AUD, NULL, cmucal_vclk_ip_ppmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_AUD, NULL, cmucal_vclk_ip_sysreg_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ABOX, NULL, cmucal_vclk_ip_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_AUD, NULL, cmucal_vclk_ip_lhm_axi_p_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERI_AXI_ASB, NULL, cmucal_vclk_ip_peri_axi_asb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_AUD, NULL, cmucal_vclk_ip_wdt_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SMMU_AUD, NULL, cmucal_vclk_ip_smmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SMMU_AUD, NULL, cmucal_vclk_ip_ad_apb_smmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SMMU_AUD_S, NULL, cmucal_vclk_ip_ad_apb_smmu_aud_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_AUD, NULL, cmucal_vclk_ip_vgen_lite_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SMMU_AUD_NS1, NULL, cmucal_vclk_ip_ad_apb_smmu_aud_ns1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUS0_CMU_BUS0, NULL, cmucal_vclk_ip_bus0_cmu_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_BUS0, NULL, cmucal_vclk_ip_sysreg_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D0_BUS0, NULL, cmucal_vclk_ip_trex_d0_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF0, NULL, cmucal_vclk_ip_lhs_axi_p_mif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF1, NULL, cmucal_vclk_ip_lhs_axi_p_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF2, NULL, cmucal_vclk_ip_lhs_axi_p_mif2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF3, NULL, cmucal_vclk_ip_lhs_axi_p_mif3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_PERIS, NULL, cmucal_vclk_ip_lhs_axi_p_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_PERIC1, NULL, cmucal_vclk_ip_lhs_axi_p_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MCSC, NULL, cmucal_vclk_ip_lhs_axi_p_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CSIS, NULL, cmucal_vclk_ip_lhs_axi_p_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_VRA, NULL, cmucal_vclk_ip_lhs_axi_p_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_ITP, NULL, cmucal_vclk_ip_lhs_axi_p_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_TNR, NULL, cmucal_vclk_ip_lhs_axi_p_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_HSI1, NULL, cmucal_vclk_ip_lhs_axi_p_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_BUS0, NULL, cmucal_vclk_ip_trex_p_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D1_BUS0, NULL, cmucal_vclk_ip_trex_d1_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_AUD, NULL, cmucal_vclk_ip_lhs_axi_p_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_VRA, NULL, cmucal_vclk_ip_lhm_axi_d_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_HSI1, NULL, cmucal_vclk_ip_lhm_acel_d_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_AUD, NULL, cmucal_vclk_ip_lhm_axi_d_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_IPP, NULL, cmucal_vclk_ip_lhm_axi_d_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DNS, NULL, cmucal_vclk_ip_lhm_axi_d_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_MCSC, NULL, cmucal_vclk_ip_lhm_axi_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_TNR, NULL, cmucal_vclk_ip_lhm_axi_d_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_BUS0, NULL, cmucal_vclk_ip_d_tzpc_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_DBG_G_BUS0, NULL, cmucal_vclk_ip_lhs_dbg_g_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_MCSC, NULL, cmucal_vclk_ip_lhm_axi_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_CSIS, NULL, cmucal_vclk_ip_lhm_axi_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_CSIS, NULL, cmucal_vclk_ip_lhm_axi_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_CMUTOPC, NULL, cmucal_vclk_ip_busif_cmutopc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_IPP, NULL, cmucal_vclk_ip_lhs_axi_p_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CACHEAID_BUS0, NULL, cmucal_vclk_ip_cacheaid_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_SSP, NULL, cmucal_vclk_ip_lhm_axi_d_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_SSP, NULL, cmucal_vclk_ip_lhs_axi_p_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUS1_CMU_BUS1, NULL, cmucal_vclk_ip_bus1_cmu_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DIT, NULL, cmucal_vclk_ip_ad_apb_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_AHB_SSS, NULL, cmucal_vclk_ip_adm_ahb_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_BUS1, NULL, cmucal_vclk_ip_d_tzpc_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DIT, NULL, cmucal_vclk_ip_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DNC, NULL, cmucal_vclk_ip_lhs_axi_p_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_HSI0, NULL, cmucal_vclk_ip_lhs_axi_p_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_HSI2, NULL, cmucal_vclk_ip_lhs_axi_p_hsi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MFC0, NULL, cmucal_vclk_ip_lhs_axi_p_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_VTS, NULL, cmucal_vclk_ip_lhs_axi_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_DBG_G_BUS1, NULL, cmucal_vclk_ip_lhs_dbg_g_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PUF, NULL, cmucal_vclk_ip_puf, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDMA, NULL, cmucal_vclk_ip_qe_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_RTIC, NULL, cmucal_vclk_ip_qe_rtic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA, NULL, cmucal_vclk_ip_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_SPDMA, NULL, cmucal_vclk_ip_qe_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_SSS, NULL, cmucal_vclk_ip_qe_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_RTIC, NULL, cmucal_vclk_ip_rtic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SBIC, NULL, cmucal_vclk_ip_sbic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPDMA, NULL, cmucal_vclk_ip_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_BUS1, NULL, cmucal_vclk_ip_sysreg_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D0_BUS1, NULL, cmucal_vclk_ip_trex_d0_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_BUS1, NULL, cmucal_vclk_ip_trex_p_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_RB_BUS1, NULL, cmucal_vclk_ip_trex_rb_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_BUS1, NULL, cmucal_vclk_ip_xiu_d0_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SSS, NULL, cmucal_vclk_ip_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D0_DNC, NULL, cmucal_vclk_ip_lhm_acel_d0_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D1_DNC, NULL, cmucal_vclk_ip_lhm_acel_d1_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_HSI0, NULL, cmucal_vclk_ip_lhm_acel_d_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_HSI2, NULL, cmucal_vclk_ip_lhm_acel_d_hsi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D2_DNC, NULL, cmucal_vclk_ip_lhm_acel_d2_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_MFC0, NULL, cmucal_vclk_ip_lhm_axi_d0_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_MFC0, NULL, cmucal_vclk_ip_lhm_axi_d1_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_APM, NULL, cmucal_vclk_ip_lhm_axi_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D0_G2D, NULL, cmucal_vclk_ip_lhm_acel_d0_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_VTS, NULL, cmucal_vclk_ip_lhm_axi_d_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SBIC, NULL, cmucal_vclk_ip_ad_apb_sbic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_VGEN_PDMA, NULL, cmucal_vclk_ip_ad_apb_vgen_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_BUS1, NULL, cmucal_vclk_ip_xiu_d1_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PDMA, NULL, cmucal_vclk_ip_ad_apb_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SPDMA, NULL, cmucal_vclk_ip_ad_apb_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_ACVPS, NULL, cmucal_vclk_ip_ad_apb_sysmmu_acvps, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_DIT, NULL, cmucal_vclk_ip_ad_apb_sysmmu_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_SBIC, NULL, cmucal_vclk_ip_ad_apb_sysmmu_sbic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_SSS, NULL, cmucal_vclk_ip_lhm_axi_d_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_SSS, NULL, cmucal_vclk_ip_lhs_axi_d_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_BUS1, NULL, cmucal_vclk_ip_vgen_lite_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_D_SSS, NULL, cmucal_vclk_ip_baaw_d_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_VTS, NULL, cmucal_vclk_ip_baaw_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_NPU0, NULL, cmucal_vclk_ip_lhs_axi_p_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_NPU1, NULL, cmucal_vclk_ip_lhs_axi_p_npu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_S2_ACVPS, NULL, cmucal_vclk_ip_sysmmu_s2_acvps, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_S2_DIT, NULL, cmucal_vclk_ip_sysmmu_s2_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_S2_SBIC, NULL, cmucal_vclk_ip_sysmmu_s2_sbic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D1_G2D, NULL, cmucal_vclk_ip_lhm_acel_d1_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D2_G2D, NULL, cmucal_vclk_ip_lhm_acel_d2_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_G2D, NULL, cmucal_vclk_ip_lhs_axi_p_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_DNC, NULL, cmucal_vclk_ip_baaw_p_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CACHEAID_BUS1, NULL, cmucal_vclk_ip_cacheaid_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D1_BUS1, NULL, cmucal_vclk_ip_trex_d1_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_PERIC0, NULL, cmucal_vclk_ip_lhs_axi_p_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DPU, NULL, cmucal_vclk_ip_lhs_axi_p_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D2_DPU, NULL, cmucal_vclk_ip_lhm_axi_d2_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_DPU, NULL, cmucal_vclk_ip_lhm_axi_d1_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_DPU, NULL, cmucal_vclk_ip_lhm_axi_d0_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_D0BUS1_P0CORE, NULL, cmucal_vclk_ip_lhs_axi_p_d0bus1_p0core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_PDMA, NULL, cmucal_vclk_ip_vgen_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CMGP_CMU_CMGP, NULL, cmucal_vclk_ip_cmgp_cmu_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADC_CMGP, NULL, cmucal_vclk_ip_adc_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_CMGP, NULL, cmucal_vclk_ip_gpio_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP0, NULL, cmucal_vclk_ip_i2c_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP1, NULL, cmucal_vclk_ip_i2c_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP2, NULL, cmucal_vclk_ip_i2c_cmgp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP3, NULL, cmucal_vclk_ip_i2c_cmgp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP, NULL, cmucal_vclk_ip_sysreg_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP0, NULL, cmucal_vclk_ip_usi_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP1, NULL, cmucal_vclk_ip_usi_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP2, NULL, cmucal_vclk_ip_usi_cmgp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP3, NULL, cmucal_vclk_ip_usi_cmgp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2PMU_AP, NULL, cmucal_vclk_ip_sysreg_cmgp2pmu_ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CMGP, NULL, cmucal_vclk_ip_d_tzpc_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_CMGP, NULL, cmucal_vclk_ip_lhm_axi_c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2APM, NULL, cmucal_vclk_ip_sysreg_cmgp2apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DBGCORE_UART_CMGP, NULL, cmucal_vclk_ip_dbgcore_uart_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CORE_CMU_CORE, NULL, cmucal_vclk_ip_core_cmu_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CORE, NULL, cmucal_vclk_ip_sysreg_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE2AXI_0, NULL, cmucal_vclk_ip_mpace2axi_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE2AXI_1, NULL, cmucal_vclk_ip_mpace2axi_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_DEBUG_CCI, NULL, cmucal_vclk_ip_ppc_debug_cci, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P0_CORE, NULL, cmucal_vclk_ip_trex_p0_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CPUCL2_0, NULL, cmucal_vclk_ip_ppmu_cpucl2_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T_BDU, NULL, cmucal_vclk_ip_lhs_atb_t_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BDU, NULL, cmucal_vclk_ip_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P1_CORE, NULL, cmucal_vclk_ip_trex_p1_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_G3D, NULL, cmucal_vclk_ip_lhs_axi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CPUCL0, NULL, cmucal_vclk_ip_lhs_axi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CPUCL2, NULL, cmucal_vclk_ip_lhs_axi_p_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D0_G3D, NULL, cmucal_vclk_ip_lhm_ace_d0_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D1_G3D, NULL, cmucal_vclk_ip_lhm_ace_d1_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D2_G3D, NULL, cmucal_vclk_ip_lhm_ace_d2_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D3_G3D, NULL, cmucal_vclk_ip_lhm_ace_d3_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_CORE, NULL, cmucal_vclk_ip_trex_d_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPCFW_G3D, NULL, cmucal_vclk_ip_ppcfw_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_APM, NULL, cmucal_vclk_ip_lhs_axi_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CPUCL2_1, NULL, cmucal_vclk_ip_ppmu_cpucl2_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CORE, NULL, cmucal_vclk_ip_d_tzpc_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_CPUCL2_0, NULL, cmucal_vclk_ip_ppc_cpucl2_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_CPUCL2_1, NULL, cmucal_vclk_ip_ppc_cpucl2_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_G3D0, NULL, cmucal_vclk_ip_ppc_g3d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_G3D1, NULL, cmucal_vclk_ip_ppc_g3d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_G3D2, NULL, cmucal_vclk_ip_ppc_g3d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_G3D3, NULL, cmucal_vclk_ip_ppc_g3d3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_IRPS0, NULL, cmucal_vclk_ip_ppc_irps0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_IRPS1, NULL, cmucal_vclk_ip_ppc_irps1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_L_CORE, NULL, cmucal_vclk_ip_lhs_axi_l_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D0_CLUSTER0, NULL, cmucal_vclk_ip_lhm_ace_d0_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D1_CLUSTER0, NULL, cmucal_vclk_ip_lhm_ace_d1_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_CPUCL0_0, NULL, cmucal_vclk_ip_ppc_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_CPUCL0_1, NULL, cmucal_vclk_ip_ppc_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CPUCL0_0, NULL, cmucal_vclk_ip_ppmu_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CPUCL0_1, NULL, cmucal_vclk_ip_ppmu_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE_ASB_D0_MIF, NULL, cmucal_vclk_ip_mpace_asb_d0_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE_ASB_D1_MIF, NULL, cmucal_vclk_ip_mpace_asb_d1_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE_ASB_D2_MIF, NULL, cmucal_vclk_ip_mpace_asb_d2_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE_ASB_D3_MIF, NULL, cmucal_vclk_ip_mpace_asb_d3_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI_ASB_CSSYS, NULL, cmucal_vclk_ip_axi_asb_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_CSSYS, NULL, cmucal_vclk_ip_lhm_axi_g_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CCI, NULL, cmucal_vclk_ip_cci, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_L_CORE, NULL, cmucal_vclk_ip_lhm_axi_l_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_G3D0, NULL, cmucal_vclk_ip_ppmu_g3d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_G3D1, NULL, cmucal_vclk_ip_ppmu_g3d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_G3D2, NULL, cmucal_vclk_ip_ppmu_g3d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_G3D3, NULL, cmucal_vclk_ip_ppmu_g3d3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_G3D0, NULL, cmucal_vclk_ip_qe_g3d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_G3D1, NULL, cmucal_vclk_ip_qe_g3d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_G3D2, NULL, cmucal_vclk_ip_qe_g3d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_G3D3, NULL, cmucal_vclk_ip_qe_g3d3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_G3D0, NULL, cmucal_vclk_ip_sysmmu_g3d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_G3D1, NULL, cmucal_vclk_ip_sysmmu_g3d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_G3D2, NULL, cmucal_vclk_ip_sysmmu_g3d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_G3D3, NULL, cmucal_vclk_ip_sysmmu_g3d3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_CORE, NULL, cmucal_vclk_ip_xiu_d_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_D0BUS1_P0CORE, NULL, cmucal_vclk_ip_lhm_axi_p_d0bus1_p0core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_SYSMMU_G3D0, NULL, cmucal_vclk_ip_apb_async_sysmmu_g3d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CPUCL0, NULL, cmucal_vclk_ip_sysreg_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_APBIF_CPUCL0, NULL, cmucal_vclk_ip_hpm_apbif_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSSYS, NULL, cmucal_vclk_ip_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T_BDU, NULL, cmucal_vclk_ip_lhm_atb_t_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T0_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t0_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T0_CLUSTER2, NULL, cmucal_vclk_ip_lhm_atb_t0_cluster2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T1_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t1_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T1_CLUSTER2, NULL, cmucal_vclk_ip_lhm_atb_t1_cluster2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T2_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t2_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T3_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t3_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SECJTAG, NULL, cmucal_vclk_ip_secjtag, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CPUCL0, NULL, cmucal_vclk_ip_lhm_axi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACE_D0_CLUSTER0, NULL, cmucal_vclk_ip_lhs_ace_d0_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T0_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t0_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T1_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t1_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T2_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t2_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T3_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t3_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_G_CLUSTER0, NULL, cmucal_vclk_ip_adm_apb_g_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0_CMU_CPUCL0, NULL, cmucal_vclk_ip_cpucl0_cmu_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0, NULL, cmucal_vclk_ip_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T4_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t4_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T5_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t5_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACE_D1_CLUSTER0, NULL, cmucal_vclk_ip_lhs_ace_d1_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T4_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t4_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T5_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t5_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CPUCL0, NULL, cmucal_vclk_ip_d_tzpc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_INT_CSSYS, NULL, cmucal_vclk_ip_lhs_axi_g_int_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_INT_CSSYS, NULL, cmucal_vclk_ip_lhm_axi_g_int_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_CPUCL0, NULL, cmucal_vclk_ip_xiu_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_CSSYS, NULL, cmucal_vclk_ip_xiu_dp_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_CPUCL0, NULL, cmucal_vclk_ip_trex_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_CSSYS, NULL, cmucal_vclk_ip_lhs_axi_g_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_CPUCL0, NULL, cmucal_vclk_ip_hpm_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_P_CSSYS_0, NULL, cmucal_vclk_ip_apb_async_p_cssys_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_INT_ETR, NULL, cmucal_vclk_ip_lhs_axi_g_int_etr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_INT_ETR, NULL, cmucal_vclk_ip_lhm_axi_g_int_etr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_CPUCL0, NULL, cmucal_vclk_ip_bps_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_INT_DBGCORE, NULL, cmucal_vclk_ip_lhm_axi_g_int_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_DBGCORE, NULL, cmucal_vclk_ip_lhm_axi_g_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_INT_DBGCORE, NULL, cmucal_vclk_ip_lhs_axi_g_int_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_INT_STM, NULL, cmucal_vclk_ip_lhs_axi_g_int_stm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_INT_STM, NULL, cmucal_vclk_ip_lhm_axi_g_int_stm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL1_CMU_CPUCL1, NULL, cmucal_vclk_ip_cpucl1_cmu_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL1, NULL, cmucal_vclk_ip_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL2_CMU_CPUCL2, NULL, cmucal_vclk_ip_cpucl2_cmu_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CPUCL2, NULL, cmucal_vclk_ip_sysreg_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_APBIF_CPUCL2, NULL, cmucal_vclk_ip_hpm_apbif_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_CPUCL2, NULL, cmucal_vclk_ip_hpm_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CPUCL2, NULL, cmucal_vclk_ip_lhm_axi_p_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CPUCL2, NULL, cmucal_vclk_ip_d_tzpc_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSIS_CMU_CSIS, NULL, cmucal_vclk_ip_csis_cmu_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_CSIS, NULL, cmucal_vclk_ip_lhs_axi_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_CSIS, NULL, cmucal_vclk_ip_lhs_axi_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CSIS, NULL, cmucal_vclk_ip_d_tzpc_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSIS_PDP, NULL, cmucal_vclk_ip_csis_pdp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CSIS_DMA0_CSIS, NULL, cmucal_vclk_ip_ppmu_csis_dma0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CSIS_DMA1_CSIS, NULL, cmucal_vclk_ip_ppmu_csis_dma1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_CSIS, NULL, cmucal_vclk_ip_sysmmu_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_CSIS, NULL, cmucal_vclk_ip_sysmmu_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CSIS, NULL, cmucal_vclk_ip_sysreg_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_CSIS, NULL, cmucal_vclk_ip_vgen_lite_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CSIS, NULL, cmucal_vclk_ip_lhm_axi_p_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF0_CSISIPP, NULL, cmucal_vclk_ip_lhs_ast_otf0_csisipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_VO_MCSCCSIS, NULL, cmucal_vclk_ip_lhm_ast_vo_mcsccsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_VO_CSISIPP, NULL, cmucal_vclk_ip_lhs_ast_vo_csisipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_ZOTF0_IPPCSIS, NULL, cmucal_vclk_ip_lhm_ast_zotf0_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_ZOTF1_IPPCSIS, NULL, cmucal_vclk_ip_lhm_ast_zotf1_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_ZOTF2_IPPCSIS, NULL, cmucal_vclk_ip_lhm_ast_zotf2_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_CSIS0, NULL, cmucal_vclk_ip_ad_apb_csis0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_CSIS, NULL, cmucal_vclk_ip_xiu_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_CSIS, NULL, cmucal_vclk_ip_xiu_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_DMA1, NULL, cmucal_vclk_ip_qe_csis_dma1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_ZSL, NULL, cmucal_vclk_ip_qe_zsl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_STRP, NULL, cmucal_vclk_ip_qe_strp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_STAT, NULL, cmucal_vclk_ip_qe_pdp_stat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF1_CSISIPP, NULL, cmucal_vclk_ip_lhs_ast_otf1_csisipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF2_CSISIPP, NULL, cmucal_vclk_ip_lhs_ast_otf2_csisipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_SOTF0_IPPCSIS, NULL, cmucal_vclk_ip_lhm_ast_sotf0_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_SOTF1_IPPCSIS, NULL, cmucal_vclk_ip_lhm_ast_sotf1_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_SOTF2_IPPCSIS, NULL, cmucal_vclk_ip_lhm_ast_sotf2_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_ZSL_CSIS, NULL, cmucal_vclk_ip_ppmu_zsl_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_PDP_STAT_CSIS, NULL, cmucal_vclk_ip_ppmu_pdp_stat_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_STRP_CSIS, NULL, cmucal_vclk_ip_ppmu_strp_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OIS_MCU_TOP, NULL, cmucal_vclk_ip_ois_mcu_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_OIS_MCU_TOP, NULL, cmucal_vclk_ip_ad_axi_ois_mcu_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_DMA0, NULL, cmucal_vclk_ip_qe_csis_dma0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CSISPERIC1, NULL, cmucal_vclk_ip_lhs_axi_p_csisperic1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DNC_CMU_DNC, NULL, cmucal_vclk_ip_dnc_cmu_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DNC, NULL, cmucal_vclk_ip_d_tzpc_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DNCDSP0, NULL, cmucal_vclk_ip_lhs_axi_p_dncdsp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DNCDSP1, NULL, cmucal_vclk_ip_lhs_axi_p_dncdsp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DNCDSP0_SFR, NULL, cmucal_vclk_ip_lhs_axi_d_dncdsp0_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_NPUC, NULL, cmucal_vclk_ip_ip_npuc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DNC, NULL, cmucal_vclk_ip_lhm_axi_p_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DSP0DNC_SFR, NULL, cmucal_vclk_ip_lhm_axi_d_dsp0dnc_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DSP0DNC_CACHE, NULL, cmucal_vclk_ip_lhm_axi_d_dsp0dnc_cache, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DSP1DNC_CACHE, NULL, cmucal_vclk_ip_lhm_axi_d_dsp1dnc_cache, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DSP1DNC_SFR, NULL, cmucal_vclk_ip_lhm_axi_d_dsp1dnc_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPU0_UNIT0_DONE, NULL, cmucal_vclk_ip_lhm_ast_d_npu0_unit0_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPU0_UNIT1_DONE, NULL, cmucal_vclk_ip_lhm_ast_d_npu0_unit1_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPU1_UNIT0_DONE, NULL, cmucal_vclk_ip_lhm_ast_d_npu1_unit0_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPU1_UNIT1_DONE, NULL, cmucal_vclk_ip_lhm_ast_d_npu1_unit1_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_DNC, NULL, cmucal_vclk_ip_vgen_lite_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DNC0, NULL, cmucal_vclk_ip_ppmu_dnc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DNC1, NULL, cmucal_vclk_ip_ppmu_dnc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DNC2, NULL, cmucal_vclk_ip_ppmu_dnc2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_DSPC, NULL, cmucal_vclk_ip_ip_dspc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DNC0, NULL, cmucal_vclk_ip_sysmmu_dnc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DNC1, NULL, cmucal_vclk_ip_sysmmu_dnc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DNC2, NULL, cmucal_vclk_ip_sysmmu_dnc2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DNC, NULL, cmucal_vclk_ip_sysreg_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_DAP_DNC, NULL, cmucal_vclk_ip_adm_dap_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DNCDSP1_SFR, NULL, cmucal_vclk_ip_lhs_axi_d_dncdsp1_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPU0_UNIT0_SETREG, NULL, cmucal_vclk_ip_lhs_ast_d_npu0_unit0_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPU0_UNIT1_SETREG, NULL, cmucal_vclk_ip_lhs_ast_d_npu0_unit1_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPU1_UNIT0_SETREG, NULL, cmucal_vclk_ip_lhs_ast_d_npu1_unit0_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPU1_UNIT1_SETREG, NULL, cmucal_vclk_ip_lhs_ast_d_npu1_unit1_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DNC0, NULL, cmucal_vclk_ip_ad_apb_dnc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D0_DNC, NULL, cmucal_vclk_ip_lhs_acel_d0_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D1_DNC, NULL, cmucal_vclk_ip_lhs_acel_d1_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D2_DNC, NULL, cmucal_vclk_ip_lhs_acel_d2_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DSPC_200, NULL, cmucal_vclk_ip_lhs_axi_p_dspc_200, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DSPC_800, NULL, cmucal_vclk_ip_lhm_axi_p_dspc_800, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DSPCNPUC_200, NULL, cmucal_vclk_ip_lhm_axi_d_dspcnpuc_200, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_NPUCDSPC_533, NULL, cmucal_vclk_ip_lhm_axi_d_npucdspc_533, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DSPCNPUC_800, NULL, cmucal_vclk_ip_lhs_axi_d_dspcnpuc_800, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_NPUCDSPC_800, NULL, cmucal_vclk_ip_lhs_axi_d_npucdspc_800, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DNCDSP0_DMA, NULL, cmucal_vclk_ip_lhs_axi_d_dncdsp0_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DNCDSP1_DMA, NULL, cmucal_vclk_ip_lhs_axi_d_dncdsp1_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DNS_CMU_DNS, NULL, cmucal_vclk_ip_dns_cmu_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DNS, NULL, cmucal_vclk_ip_ad_apb_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DNS, NULL, cmucal_vclk_ip_ppmu_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DNS, NULL, cmucal_vclk_ip_sysmmu_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_ITPDNS, NULL, cmucal_vclk_ip_lhm_axi_p_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DNS, NULL, cmucal_vclk_ip_lhs_axi_d_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DNS, NULL, cmucal_vclk_ip_sysreg_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_DNS, NULL, cmucal_vclk_ip_vgen_lite_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DNS, NULL, cmucal_vclk_ip_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_IPPDNS, NULL, cmucal_vclk_ip_lhm_ast_otf0_ippdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF1_IPPDNS, NULL, cmucal_vclk_ip_lhm_ast_otf1_ippdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF_ITPDNS, NULL, cmucal_vclk_ip_lhm_ast_otf_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF_TNRDNS, NULL, cmucal_vclk_ip_lhm_ast_otf_tnrdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF0_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf0_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF1_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf1_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF2_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf2_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF3_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf3_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF4_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf4_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_CTL_ITPDNS, NULL, cmucal_vclk_ip_lhm_ast_ctl_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_CTL_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_ctl_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DNS, NULL, cmucal_vclk_ip_d_tzpc_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_VO_DNSTNR, NULL, cmucal_vclk_ip_lhs_ast_vo_dnstnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_VO_IPPDNS, NULL, cmucal_vclk_ip_lhm_ast_vo_ippdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPU_CMU_DPU, NULL, cmucal_vclk_ip_dpu_cmu_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DPU, NULL, cmucal_vclk_ip_sysreg_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DPUD0, NULL, cmucal_vclk_ip_sysmmu_dpud0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DPU, NULL, cmucal_vclk_ip_lhm_axi_p_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_DPU, NULL, cmucal_vclk_ip_lhs_axi_d1_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DECON0, NULL, cmucal_vclk_ip_ad_apb_decon0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D2_DPU, NULL, cmucal_vclk_ip_lhs_axi_d2_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DPUD2, NULL, cmucal_vclk_ip_sysmmu_dpud2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DPUD1, NULL, cmucal_vclk_ip_sysmmu_dpud1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPUD0, NULL, cmucal_vclk_ip_ppmu_dpud0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPUD1, NULL, cmucal_vclk_ip_ppmu_dpud1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPUD2, NULL, cmucal_vclk_ip_ppmu_dpud2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPU, NULL, cmucal_vclk_ip_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_DPU, NULL, cmucal_vclk_ip_lhs_axi_d0_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DPU, NULL, cmucal_vclk_ip_d_tzpc_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_NPU_SRAM0, NULL, cmucal_vclk_ip_lhm_axi_d_npu_sram0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DSP_CMU_DSP, NULL, cmucal_vclk_ip_dsp_cmu_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DSP, NULL, cmucal_vclk_ip_sysreg_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DSPDNC_SFR, NULL, cmucal_vclk_ip_lhs_axi_d_dspdnc_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_DSP, NULL, cmucal_vclk_ip_ip_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DSPDNC_CACHE, NULL, cmucal_vclk_ip_lhs_axi_d_dspdnc_cache, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ABM_APB_DSP_DBG, NULL, cmucal_vclk_ip_abm_apb_dsp_dbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_NPU_SRAM1, NULL, cmucal_vclk_ip_lhm_axi_d_npu_sram1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DNCDSP_DMA, NULL, cmucal_vclk_ip_lhm_axi_d_dncdsp_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DNCDSP, NULL, cmucal_vclk_ip_lhm_axi_p_dncdsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DNCDSP_SFR, NULL, cmucal_vclk_ip_lhm_axi_d_dncdsp_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DSP, NULL, cmucal_vclk_ip_d_tzpc_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DSP1_CMU_DSP1, NULL, cmucal_vclk_ip_dsp1_cmu_dsp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G2D_CMU_G2D, NULL, cmucal_vclk_ip_g2d_cmu_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0_G2D, NULL, cmucal_vclk_ip_ppmu_d0_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1_G2D, NULL, cmucal_vclk_ip_ppmu_d1_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_G2D, NULL, cmucal_vclk_ip_sysmmu_d0_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_G2D, NULL, cmucal_vclk_ip_sysreg_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D0_G2D, NULL, cmucal_vclk_ip_lhs_acel_d0_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D1_G2D, NULL, cmucal_vclk_ip_lhs_acel_d1_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_G2D, NULL, cmucal_vclk_ip_lhm_axi_p_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_JPEG, NULL, cmucal_vclk_ip_qe_jpeg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_MSCL, NULL, cmucal_vclk_ip_qe_mscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D2_G2D, NULL, cmucal_vclk_ip_sysmmu_d2_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D2_G2D, NULL, cmucal_vclk_ip_ppmu_d2_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D2_G2D, NULL, cmucal_vclk_ip_lhs_acel_d2_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_G2D, NULL, cmucal_vclk_ip_xiu_d_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_ASTC, NULL, cmucal_vclk_ip_as_apb_astc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_ASTC, NULL, cmucal_vclk_ip_qe_astc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_G2D, NULL, cmucal_vclk_ip_vgen_lite_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G2D, NULL, cmucal_vclk_ip_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_G2D, NULL, cmucal_vclk_ip_sysmmu_d1_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_JPEG, NULL, cmucal_vclk_ip_jpeg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MSCL, NULL, cmucal_vclk_ip_mscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASTC, NULL, cmucal_vclk_ip_astc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_JSQZ, NULL, cmucal_vclk_ip_qe_jsqz, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_G2D, NULL, cmucal_vclk_ip_d_tzpc_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_JSQZ, NULL, cmucal_vclk_ip_jsqz, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_G2D, NULL, cmucal_vclk_ip_as_apb_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_G3D, NULL, cmucal_vclk_ip_lhm_axi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPMG3D, NULL, cmucal_vclk_ip_busif_hpmg3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_G3D, NULL, cmucal_vclk_ip_hpm_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_G3D, NULL, cmucal_vclk_ip_sysreg_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G3D_CMU_G3D, NULL, cmucal_vclk_ip_g3d_cmu_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_INT_G3D, NULL, cmucal_vclk_ip_lhs_axi_p_int_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_G3D, NULL, cmucal_vclk_ip_vgen_lite_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPU, NULL, cmucal_vclk_ip_gpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_INT_G3D, NULL, cmucal_vclk_ip_lhm_axi_p_int_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GRAY2BIN_G3D, NULL, cmucal_vclk_ip_gray2bin_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_G3D, NULL, cmucal_vclk_ip_d_tzpc_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_APBIF_G3D, NULL, cmucal_vclk_ip_add_apbif_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_G3D, NULL, cmucal_vclk_ip_add_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASB_G3D, NULL, cmucal_vclk_ip_asb_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USB31DRD, NULL, cmucal_vclk_ip_usb31drd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DP_LINK, NULL, cmucal_vclk_ip_dp_link, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_HSI0_BUS1, NULL, cmucal_vclk_ip_ppmu_hsi0_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_HSI0, NULL, cmucal_vclk_ip_lhs_acel_d_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_HSI0, NULL, cmucal_vclk_ip_vgen_lite_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_HSI0, NULL, cmucal_vclk_ip_d_tzpc_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_HSI0, NULL, cmucal_vclk_ip_lhm_axi_p_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_USB, NULL, cmucal_vclk_ip_sysmmu_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_HSI0, NULL, cmucal_vclk_ip_sysreg_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HSI0_CMU_HSI0, NULL, cmucal_vclk_ip_hsi0_cmu_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_HSI1, NULL, cmucal_vclk_ip_sysmmu_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HSI1_CMU_HSI1, NULL, cmucal_vclk_ip_hsi1_cmu_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MMC_CARD, NULL, cmucal_vclk_ip_mmc_card, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_GEN2, NULL, cmucal_vclk_ip_pcie_gen2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_HSI1, NULL, cmucal_vclk_ip_sysreg_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_HSI1, NULL, cmucal_vclk_ip_gpio_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_HSI1, NULL, cmucal_vclk_ip_lhs_acel_d_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_HSI1, NULL, cmucal_vclk_ip_lhm_axi_p_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_HSI1, NULL, cmucal_vclk_ip_xiu_d_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_HSI1, NULL, cmucal_vclk_ip_xiu_p_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_HSI1, NULL, cmucal_vclk_ip_ppmu_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UFS_CARD, NULL, cmucal_vclk_ip_ufs_card, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_HSI1, NULL, cmucal_vclk_ip_vgen_lite_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_IA_GEN2, NULL, cmucal_vclk_ip_pcie_ia_gen2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_HSI1, NULL, cmucal_vclk_ip_d_tzpc_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UFS_EMBD, NULL, cmucal_vclk_ip_ufs_embd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_GEN4_0, NULL, cmucal_vclk_ip_pcie_gen4_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_IA_GEN4_0, NULL, cmucal_vclk_ip_pcie_ia_gen4_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HSI2_CMU_HSI2, NULL, cmucal_vclk_ip_hsi2_cmu_hsi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_HSI2, NULL, cmucal_vclk_ip_lhs_acel_d_hsi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_HSI2, NULL, cmucal_vclk_ip_lhm_axi_p_hsi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_HSI2, NULL, cmucal_vclk_ip_gpio_hsi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_HSI2, NULL, cmucal_vclk_ip_sysreg_hsi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_PCIE_GEN4_1, NULL, cmucal_vclk_ip_vgen_lite_pcie_gen4_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_HSI2, NULL, cmucal_vclk_ip_ppmu_hsi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_HSI2, NULL, cmucal_vclk_ip_sysmmu_hsi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_GEN4_1, NULL, cmucal_vclk_ip_pcie_gen4_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_IA_GEN4_1, NULL, cmucal_vclk_ip_pcie_ia_gen4_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_HSI2, NULL, cmucal_vclk_ip_d_tzpc_hsi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_HSI2, NULL, cmucal_vclk_ip_xiu_p_hsi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_IPP, NULL, cmucal_vclk_ip_lhs_axi_d_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_IPP, NULL, cmucal_vclk_ip_lhm_axi_p_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_IPP, NULL, cmucal_vclk_ip_sysreg_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IPP_CMU_IPP, NULL, cmucal_vclk_ip_ipp_cmu_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF0_IPPDNS, NULL, cmucal_vclk_ip_lhs_ast_otf0_ippdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_IPP, NULL, cmucal_vclk_ip_d_tzpc_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_CSISIPP, NULL, cmucal_vclk_ip_lhm_ast_otf0_csisipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF1_IPPDNS, NULL, cmucal_vclk_ip_lhs_ast_otf1_ippdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIPU_IPP, NULL, cmucal_vclk_ip_sipu_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_IPP, NULL, cmucal_vclk_ip_ad_apb_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_ZOTF0_IPPCSIS, NULL, cmucal_vclk_ip_lhs_ast_zotf0_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_ZOTF1_IPPCSIS, NULL, cmucal_vclk_ip_lhs_ast_zotf1_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_ZOTF2_IPPCSIS, NULL, cmucal_vclk_ip_lhs_ast_zotf2_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_IPP, NULL, cmucal_vclk_ip_ppmu_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_IPP, NULL, cmucal_vclk_ip_sysmmu_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_IPP, NULL, cmucal_vclk_ip_vgen_lite_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF1_CSISIPP, NULL, cmucal_vclk_ip_lhm_ast_otf1_csisipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF2_CSISIPP, NULL, cmucal_vclk_ip_lhm_ast_otf2_csisipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_SOTF0_IPPCSIS, NULL, cmucal_vclk_ip_lhs_ast_sotf0_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_SOTF1_IPPCSIS, NULL, cmucal_vclk_ip_lhs_ast_sotf1_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_SOTF2_IPPCSIS, NULL, cmucal_vclk_ip_lhs_ast_sotf2_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_VO_CSISIPP, NULL, cmucal_vclk_ip_lhm_ast_vo_csisipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_VO_IPPDNS, NULL, cmucal_vclk_ip_lhs_ast_vo_ippdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_ITP, NULL, cmucal_vclk_ip_lhm_axi_p_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_ITP, NULL, cmucal_vclk_ip_sysreg_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ITP_CMU_ITP, NULL, cmucal_vclk_ip_itp_cmu_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf0_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF3_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf3_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF2_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf2_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ITP, NULL, cmucal_vclk_ip_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_ITP, NULL, cmucal_vclk_ip_d_tzpc_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF1_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf1_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_ITPDNS, NULL, cmucal_vclk_ip_lhs_axi_p_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_ITP, NULL, cmucal_vclk_ip_ad_apb_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF4_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf4_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_CTL_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_ctl_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_CTL_ITPDNS, NULL, cmucal_vclk_ip_lhs_ast_ctl_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF0_ITPMCSC, NULL, cmucal_vclk_ip_lhs_ast_otf0_itpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF1_ITPMCSC, NULL, cmucal_vclk_ip_lhs_ast_otf1_itpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF2_ITPMCSC, NULL, cmucal_vclk_ip_lhs_ast_otf2_itpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF3_ITPMCSC, NULL, cmucal_vclk_ip_lhs_ast_otf3_itpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF_ITPDNS, NULL, cmucal_vclk_ip_lhs_ast_otf_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCSC_CMU_MCSC, NULL, cmucal_vclk_ip_mcsc_cmu_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MCSC, NULL, cmucal_vclk_ip_lhs_axi_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MCSC, NULL, cmucal_vclk_ip_lhm_axi_p_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_MCSC, NULL, cmucal_vclk_ip_xiu_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MCSC, NULL, cmucal_vclk_ip_sysreg_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCSC, NULL, cmucal_vclk_ip_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MCSC, NULL, cmucal_vclk_ip_ppmu_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MEIP, NULL, cmucal_vclk_ip_ppmu_meip, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_MCSC, NULL, cmucal_vclk_ip_sysmmu_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MCSC, NULL, cmucal_vclk_ip_d_tzpc_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_ITSC, NULL, cmucal_vclk_ip_qe_itsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_MCSC, NULL, cmucal_vclk_ip_qe_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_MCSC, NULL, cmucal_vclk_ip_vgen_lite_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_MCSC, NULL, cmucal_vclk_ip_ad_apb_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_MEIP, NULL, cmucal_vclk_ip_qe_meip, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_GDC, NULL, cmucal_vclk_ip_qe_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ITSC, NULL, cmucal_vclk_ip_itsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_ITSC, NULL, cmucal_vclk_ip_ppmu_itsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_GDC, NULL, cmucal_vclk_ip_ppmu_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_MCSC, NULL, cmucal_vclk_ip_xiu_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MEIP, NULL, cmucal_vclk_ip_meip, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GDC, NULL, cmucal_vclk_ip_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_MEIP, NULL, cmucal_vclk_ip_ad_axi_meip, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_GDC, NULL, cmucal_vclk_ip_ad_axi_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_MCSC, NULL, cmucal_vclk_ip_sysmmu_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MCSC, NULL, cmucal_vclk_ip_lhs_axi_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_ITPMCSC, NULL, cmucal_vclk_ip_lhm_ast_otf0_itpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF1_ITPMCSC, NULL, cmucal_vclk_ip_lhm_ast_otf1_itpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF2_ITPMCSC, NULL, cmucal_vclk_ip_lhm_ast_otf2_itpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF3_ITPMCSC, NULL, cmucal_vclk_ip_lhm_ast_otf3_itpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_VO_TNRMCSC, NULL, cmucal_vclk_ip_lhm_ast_vo_tnrmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_VO_MCSCCSIS, NULL, cmucal_vclk_ip_lhs_ast_vo_mcsccsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_INT_GDCMEIP, NULL, cmucal_vclk_ip_lhm_ast_int_gdcmeip, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_INT_MEIPMCSC, NULL, cmucal_vclk_ip_lhs_ast_int_meipmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_INT_MEIPMCSC, NULL, cmucal_vclk_ip_lhm_ast_int_meipmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_INT_GDCMEIP, NULL, cmucal_vclk_ip_lhs_ast_int_gdcmeip, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_GDC, NULL, cmucal_vclk_ip_ad_apb_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_MEIP, NULL, cmucal_vclk_ip_ad_apb_meip, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_C2AGENT_MCSC, NULL, cmucal_vclk_ip_c2agent_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC0_CMU_MFC0, NULL, cmucal_vclk_ip_mfc0_cmu_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_MFC0, NULL, cmucal_vclk_ip_as_apb_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MFC0, NULL, cmucal_vclk_ip_sysreg_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MFC0, NULL, cmucal_vclk_ip_lhs_axi_d0_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MFC0, NULL, cmucal_vclk_ip_lhs_axi_d1_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MFC0, NULL, cmucal_vclk_ip_lhm_axi_p_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFC0D0, NULL, cmucal_vclk_ip_sysmmu_mfc0d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFC0D1, NULL, cmucal_vclk_ip_sysmmu_mfc0d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFC0D0, NULL, cmucal_vclk_ip_ppmu_mfc0d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFC0D1, NULL, cmucal_vclk_ip_ppmu_mfc0d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_AXI_WFD, NULL, cmucal_vclk_ip_as_axi_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_WFD, NULL, cmucal_vclk_ip_ppmu_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_MFC0, NULL, cmucal_vclk_ip_xiu_d_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_MFC0, NULL, cmucal_vclk_ip_vgen_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC0, NULL, cmucal_vclk_ip_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WFD, NULL, cmucal_vclk_ip_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MFC0, NULL, cmucal_vclk_ip_lh_atb_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MFC0, NULL, cmucal_vclk_ip_d_tzpc_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIF_CMU_MIF, NULL, cmucal_vclk_ip_mif_cmu_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDRPHY, NULL, cmucal_vclk_ip_ddrphy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MIF, NULL, cmucal_vclk_ip_sysreg_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MIF, NULL, cmucal_vclk_ip_lhm_axi_p_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DDRPHY, NULL, cmucal_vclk_ip_apbbr_ddrphy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DMC, NULL, cmucal_vclk_ip_apbbr_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMC, NULL, cmucal_vclk_ip_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QCH_ADAPTER_PPC_DEBUG, NULL, cmucal_vclk_ip_qch_adapter_ppc_debug, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QCH_ADAPTER_PPC_DVFS, NULL, cmucal_vclk_ip_qch_adapter_ppc_dvfs, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MIF, NULL, cmucal_vclk_ip_d_tzpc_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_DEBUG, NULL, cmucal_vclk_ip_ppc_debug, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_DVFS, NULL, cmucal_vclk_ip_ppc_dvfs, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIF1_CMU_MIF1, NULL, cmucal_vclk_ip_mif1_cmu_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DDRPHY1, NULL, cmucal_vclk_ip_apbbr_ddrphy1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DMC1, NULL, cmucal_vclk_ip_apbbr_dmc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DMCTZ1, NULL, cmucal_vclk_ip_apbbr_dmctz1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI2APB_MIF1, NULL, cmucal_vclk_ip_axi2apb_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDRPHY1, NULL, cmucal_vclk_ip_ddrphy1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMC1, NULL, cmucal_vclk_ip_dmc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MIF1, NULL, cmucal_vclk_ip_lhm_axi_p_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMUPPC_DEBUG1, NULL, cmucal_vclk_ip_ppmuppc_debug1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMUPPC_DVFS1, NULL, cmucal_vclk_ip_ppmuppc_dvfs1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MIF1, NULL, cmucal_vclk_ip_sysreg_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QCH_ADAPTER_PPMUPPC_DEBUG1, NULL, cmucal_vclk_ip_qch_adapter_ppmuppc_debug1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QCH_ADAPTER_PPMUPPC_DVFS1, NULL, cmucal_vclk_ip_qch_adapter_ppmuppc_dvfs1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DDRPHY2, NULL, cmucal_vclk_ip_apbbr_ddrphy2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DMC2, NULL, cmucal_vclk_ip_apbbr_dmc2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DMCTZ2, NULL, cmucal_vclk_ip_apbbr_dmctz2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI2APB_MIF2, NULL, cmucal_vclk_ip_axi2apb_mif2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDRPHY2, NULL, cmucal_vclk_ip_ddrphy2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMC2, NULL, cmucal_vclk_ip_dmc2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MIF2, NULL, cmucal_vclk_ip_lhm_axi_p_mif2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMUPPC_DEBUG2, NULL, cmucal_vclk_ip_ppmuppc_debug2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMUPPC_DVFS2, NULL, cmucal_vclk_ip_ppmuppc_dvfs2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MIF2, NULL, cmucal_vclk_ip_sysreg_mif2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QCH_ADAPTER_PPMUPPC_DEBUG2, NULL, cmucal_vclk_ip_qch_adapter_ppmuppc_debug2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QCH_ADAPTER_PPMUPPC_DVFS2, NULL, cmucal_vclk_ip_qch_adapter_ppmuppc_dvfs2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIF2_CMU_MIF2, NULL, cmucal_vclk_ip_mif2_cmu_mif2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DDRPHY3, NULL, cmucal_vclk_ip_apbbr_ddrphy3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DMC3, NULL, cmucal_vclk_ip_apbbr_dmc3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DMCTZ3, NULL, cmucal_vclk_ip_apbbr_dmctz3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI2APB_MIF3, NULL, cmucal_vclk_ip_axi2apb_mif3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDRPHY3, NULL, cmucal_vclk_ip_ddrphy3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMC3, NULL, cmucal_vclk_ip_dmc3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MIF3, NULL, cmucal_vclk_ip_lhm_axi_p_mif3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMUPPC_DEBUG3, NULL, cmucal_vclk_ip_ppmuppc_debug3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMUPPC_DVFS3, NULL, cmucal_vclk_ip_ppmuppc_dvfs3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MIF3, NULL, cmucal_vclk_ip_sysreg_mif3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIF3_CMU_MIF3, NULL, cmucal_vclk_ip_mif3_cmu_mif3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QCH_ADAPTER_PPMUPPC_DEBUG3, NULL, cmucal_vclk_ip_qch_adapter_ppmuppc_debug3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QCH_ADAPTER_PPMUPPC_DVFS3, NULL, cmucal_vclk_ip_qch_adapter_ppmuppc_dvfs3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPU_CMU_NPU, NULL, cmucal_vclk_ip_npu_cmu_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPUD_UNIT1, NULL, cmucal_vclk_ip_npud_unit1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_NPU, NULL, cmucal_vclk_ip_d_tzpc_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D0_NPU, NULL, cmucal_vclk_ip_lhm_ast_d0_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D1_NPU, NULL, cmucal_vclk_ip_lhm_ast_d1_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D2_NPU, NULL, cmucal_vclk_ip_lhm_ast_d2_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D3_NPU, NULL, cmucal_vclk_ip_lhm_ast_d3_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D4_NPU, NULL, cmucal_vclk_ip_lhm_ast_d4_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D7_NPU, NULL, cmucal_vclk_ip_lhm_ast_d7_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D10_NPU, NULL, cmucal_vclk_ip_lhm_ast_d10_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D13_NPU, NULL, cmucal_vclk_ip_lhm_ast_d13_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPU_UNIT0_SETREG, NULL, cmucal_vclk_ip_lhm_ast_d_npu_unit0_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPU_UNIT1_SETREG, NULL, cmucal_vclk_ip_lhm_ast_d_npu_unit1_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D0_NPU, NULL, cmucal_vclk_ip_lhs_ast_d0_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D5_NPU, NULL, cmucal_vclk_ip_lhs_ast_d5_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D8_NPU, NULL, cmucal_vclk_ip_lhs_ast_d8_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D12_NPU, NULL, cmucal_vclk_ip_lhs_ast_d12_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D1_NPU, NULL, cmucal_vclk_ip_lhs_ast_d1_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D6_NPU, NULL, cmucal_vclk_ip_lhs_ast_d6_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D9_NPU, NULL, cmucal_vclk_ip_lhs_ast_d9_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D14_NPU, NULL, cmucal_vclk_ip_lhs_ast_d14_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPU_UNIT0_DONE, NULL, cmucal_vclk_ip_lhs_ast_d_npu_unit0_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_NPU_SRAM1, NULL, cmucal_vclk_ip_lhs_axi_d_npu_sram1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_NPU_SRAM0, NULL, cmucal_vclk_ip_lhs_axi_d_npu_sram0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPUD_UNIT0, NULL, cmucal_vclk_ip_npud_unit0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_UNIT0, NULL, cmucal_vclk_ip_ppmu_unit0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_NPU, NULL, cmucal_vclk_ip_sysreg_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_UNIT1, NULL, cmucal_vclk_ip_ppmu_unit1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_NPU, NULL, cmucal_vclk_ip_lhm_axi_p_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D5_NPU, NULL, cmucal_vclk_ip_lhm_ast_d5_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D8_NPU, NULL, cmucal_vclk_ip_lhm_ast_d8_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D11_NPU, NULL, cmucal_vclk_ip_lhm_ast_d11_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D14_NPU, NULL, cmucal_vclk_ip_lhm_ast_d14_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D6_NPU, NULL, cmucal_vclk_ip_lhm_ast_d6_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D9_NPU, NULL, cmucal_vclk_ip_lhm_ast_d9_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D12_NPU, NULL, cmucal_vclk_ip_lhm_ast_d12_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D15_NPU, NULL, cmucal_vclk_ip_lhm_ast_d15_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPU_UNIT1_DONE, NULL, cmucal_vclk_ip_lhs_ast_d_npu_unit1_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D4_NPU, NULL, cmucal_vclk_ip_lhs_ast_d4_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D2_NPU, NULL, cmucal_vclk_ip_lhs_ast_d2_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D3_NPU, NULL, cmucal_vclk_ip_lhs_ast_d3_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D7_NPU, NULL, cmucal_vclk_ip_lhs_ast_d7_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D10_NPU, NULL, cmucal_vclk_ip_lhs_ast_d10_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D11_NPU, NULL, cmucal_vclk_ip_lhs_ast_d11_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D13_NPU, NULL, cmucal_vclk_ip_lhs_ast_d13_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D15_NPU, NULL, cmucal_vclk_ip_lhs_ast_d15_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPU1_CMU_NPU1, NULL, cmucal_vclk_ip_npu1_cmu_npu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPUD0_D1_0, NULL, cmucal_vclk_ip_lhm_ast_d_npud0_d1_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_NPU1, NULL, cmucal_vclk_ip_lhm_axi_p_npu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPUD0_D1_1, NULL, cmucal_vclk_ip_lhm_ast_d_npud0_d1_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPUD0_D1_2, NULL, cmucal_vclk_ip_lhm_ast_d_npud0_d1_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPUD0_D1_3, NULL, cmucal_vclk_ip_lhm_ast_d_npud0_d1_3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPUD0_D1_4, NULL, cmucal_vclk_ip_lhm_ast_d_npud0_d1_4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPUD0_D1_5, NULL, cmucal_vclk_ip_lhm_ast_d_npud0_d1_5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPUD0_D1_6, NULL, cmucal_vclk_ip_lhm_ast_d_npud0_d1_6, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPUD0_D1_7, NULL, cmucal_vclk_ip_lhm_ast_d_npud0_d1_7, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_P_NPUD1_SETREG, NULL, cmucal_vclk_ip_lhm_ast_p_npud1_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_IDPSRAM1, NULL, cmucal_vclk_ip_lhm_axi_d_idpsram1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_IDPSRAM3, NULL, cmucal_vclk_ip_lhm_axi_d_idpsram3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPUD1_D1_0, NULL, cmucal_vclk_ip_lhs_ast_d_npud1_d1_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPUD1_D1_1, NULL, cmucal_vclk_ip_lhs_ast_d_npud1_d1_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPUD1_D1_2, NULL, cmucal_vclk_ip_lhs_ast_d_npud1_d1_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPUD1_D1_3, NULL, cmucal_vclk_ip_lhs_ast_d_npud1_d1_3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPUD1_D1_4, NULL, cmucal_vclk_ip_lhs_ast_d_npud1_d1_4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPUD1_D1_5, NULL, cmucal_vclk_ip_lhs_ast_d_npud1_d1_5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPUD1_D1_6, NULL, cmucal_vclk_ip_lhs_ast_d_npud1_d1_6, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPUD1_D1_7, NULL, cmucal_vclk_ip_lhs_ast_d_npud1_d1_7, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_NPU1, NULL, cmucal_vclk_ip_sysreg_npu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_P_NPU1_DONE, NULL, cmucal_vclk_ip_lhs_ast_p_npu1_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_NPU1, NULL, cmucal_vclk_ip_ppmu_npu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPU1_PPC_WRAPPER, NULL, cmucal_vclk_ip_npu1_ppc_wrapper, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERIC0, NULL, cmucal_vclk_ip_gpio_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERIC0, NULL, cmucal_vclk_ip_sysreg_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC0_CMU_PERIC0, NULL, cmucal_vclk_ip_peric0_cmu_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_PERIC0, NULL, cmucal_vclk_ip_lhm_axi_p_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERIC0, NULL, cmucal_vclk_ip_d_tzpc_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC0_TOP0, NULL, cmucal_vclk_ip_peric0_top0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC0_TOP1, NULL, cmucal_vclk_ip_peric0_top1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERIC1, NULL, cmucal_vclk_ip_gpio_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERIC1, NULL, cmucal_vclk_ip_sysreg_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC1_CMU_PERIC1, NULL, cmucal_vclk_ip_peric1_cmu_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_PERIC1, NULL, cmucal_vclk_ip_lhm_axi_p_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERIC1, NULL, cmucal_vclk_ip_d_tzpc_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC1_TOP0, NULL, cmucal_vclk_ip_peric1_top0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC1_TOP1, NULL, cmucal_vclk_ip_peric1_top1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CSISPERIC1, NULL, cmucal_vclk_ip_lhm_axi_p_csisperic1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_PERIC1, NULL, cmucal_vclk_ip_xiu_p_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI16_I3C, NULL, cmucal_vclk_ip_usi16_i3c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI17_I3C, NULL, cmucal_vclk_ip_usi17_i3c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERIS, NULL, cmucal_vclk_ip_sysreg_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_CLUSTER2, NULL, cmucal_vclk_ip_wdt_cluster2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_CLUSTER0, NULL, cmucal_vclk_ip_wdt_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIS_CMU_PERIS, NULL, cmucal_vclk_ip_peris_cmu_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_P_PERIS, NULL, cmucal_vclk_ip_ad_axi_p_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_BIRA, NULL, cmucal_vclk_ip_otp_con_bira, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GIC, NULL, cmucal_vclk_ip_gic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_PERIS, NULL, cmucal_vclk_ip_lhm_axi_p_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCT, NULL, cmucal_vclk_ip_mct, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_TOP, NULL, cmucal_vclk_ip_otp_con_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERIS, NULL, cmucal_vclk_ip_d_tzpc_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TMU_SUB, NULL, cmucal_vclk_ip_tmu_sub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TMU_TOP, NULL, cmucal_vclk_ip_tmu_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_BISR, NULL, cmucal_vclk_ip_otp_con_bisr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_S2D_CMU_S2D, NULL, cmucal_vclk_ip_s2d_cmu_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BIS_S2D, NULL, cmucal_vclk_ip_bis_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_SCAN2DRAM, NULL, cmucal_vclk_ip_lhm_axi_g_scan2dram, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SSP_CMU_SSP, NULL, cmucal_vclk_ip_ssp_cmu_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_SSP, NULL, cmucal_vclk_ip_lhm_axi_p_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_SSP, NULL, cmucal_vclk_ip_d_tzpc_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_SSP, NULL, cmucal_vclk_ip_sysreg_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SS_SSPCORE, NULL, cmucal_vclk_ip_ss_sspcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TNR_CMU_TNR, NULL, cmucal_vclk_ip_tnr_cmu_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_TNR, NULL, cmucal_vclk_ip_lhs_axi_d_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_TNR, NULL, cmucal_vclk_ip_lhm_axi_p_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_VO_DNSTNR, NULL, cmucal_vclk_ip_lhm_ast_vo_dnstnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_VO_TNRMCSC, NULL, cmucal_vclk_ip_lhs_ast_vo_tnrmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF_TNRDNS, NULL, cmucal_vclk_ip_lhs_ast_otf_tnrdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_TNR, NULL, cmucal_vclk_ip_sysreg_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_TNR, NULL, cmucal_vclk_ip_d_tzpc_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_TNR, NULL, cmucal_vclk_ip_vgen_lite_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_TNR, NULL, cmucal_vclk_ip_ppmu_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_TNR, NULL, cmucal_vclk_ip_sysmmu_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_TNR, NULL, cmucal_vclk_ip_apb_async_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TNR, NULL, cmucal_vclk_ip_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VRA_CMU_VRA, NULL, cmucal_vclk_ip_vra_cmu_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_VRA, NULL, cmucal_vclk_ip_ad_apb_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_VRA, NULL, cmucal_vclk_ip_d_tzpc_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_VRA, NULL, cmucal_vclk_ip_lhm_axi_p_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_VRA, NULL, cmucal_vclk_ip_qe_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VRA, NULL, cmucal_vclk_ip_sysreg_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_VRA, NULL, cmucal_vclk_ip_vgen_lite_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VRA, NULL, cmucal_vclk_ip_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_VRA, NULL, cmucal_vclk_ip_ppmu_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_STR, NULL, cmucal_vclk_ip_str, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_STR, NULL, cmucal_vclk_ip_ad_axi_str, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_VRA, NULL, cmucal_vclk_ip_sysmmu_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_STR, NULL, cmucal_vclk_ip_qe_str, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_STR, NULL, cmucal_vclk_ip_ppmu_str, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_VRA, NULL, cmucal_vclk_ip_xiu_d_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_VRA, NULL, cmucal_vclk_ip_lhs_axi_d_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_STR, NULL, cmucal_vclk_ip_ad_apb_str, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AHB_BUSMATRIX, NULL, cmucal_vclk_ip_ahb_busmatrix, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF0, NULL, cmucal_vclk_ip_dmic_if0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VTS, NULL, cmucal_vclk_ip_sysreg_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VTS_CMU_VTS, NULL, cmucal_vclk_ip_vts_cmu_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_VTS, NULL, cmucal_vclk_ip_lhm_axi_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_VTS, NULL, cmucal_vclk_ip_gpio_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_VTS, NULL, cmucal_vclk_ip_wdt_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB0, NULL, cmucal_vclk_ip_dmic_ahb0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB1, NULL, cmucal_vclk_ip_dmic_ahb1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_C_VTS, NULL, cmucal_vclk_ip_lhs_axi_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCINTERRUPT, NULL, cmucal_vclk_ip_asyncinterrupt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC0, NULL, cmucal_vclk_ip_hwacg_sys_dmic0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC1, NULL, cmucal_vclk_ip_hwacg_sys_dmic1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SS_VTS_GLUE, NULL, cmucal_vclk_ip_ss_vts_glue, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CORTEXM4INTEGRATION, NULL, cmucal_vclk_ip_cortexm4integration, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_LP_VTS, NULL, cmucal_vclk_ip_lhm_axi_lp_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_VTS, NULL, cmucal_vclk_ip_lhs_axi_d_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_C_VTS, NULL, cmucal_vclk_ip_baaw_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_VTS, NULL, cmucal_vclk_ip_d_tzpc_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE, NULL, cmucal_vclk_ip_vgen_lite, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_LP_VTS, NULL, cmucal_vclk_ip_bps_lp_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_P_VTS, NULL, cmucal_vclk_ip_bps_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_C_VTS, NULL, cmucal_vclk_ip_sweeper_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_D_VTS, NULL, cmucal_vclk_ip_baaw_d_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_ABOX_VTS, NULL, cmucal_vclk_ip_mailbox_abox_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB2, NULL, cmucal_vclk_ip_dmic_ahb2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB3, NULL, cmucal_vclk_ip_dmic_ahb3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC2, NULL, cmucal_vclk_ip_hwacg_sys_dmic2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC3, NULL, cmucal_vclk_ip_hwacg_sys_dmic3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF2, NULL, cmucal_vclk_ip_dmic_if2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_VTS, NULL, cmucal_vclk_ip_mailbox_ap_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER, NULL, cmucal_vclk_ip_timer, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA_VTS, NULL, cmucal_vclk_ip_pdma_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB4, NULL, cmucal_vclk_ip_dmic_ahb4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB5, NULL, cmucal_vclk_ip_dmic_ahb5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AUD0, NULL, cmucal_vclk_ip_dmic_aud0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF1, NULL, cmucal_vclk_ip_dmic_if1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AUD1, NULL, cmucal_vclk_ip_dmic_aud1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AUD2, NULL, cmucal_vclk_ip_dmic_aud2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SERIAL_LIF, NULL, cmucal_vclk_ip_serial_lif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC4, NULL, cmucal_vclk_ip_hwacg_sys_dmic4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC5, NULL, cmucal_vclk_ip_hwacg_sys_dmic5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_SERIAL_LIF, NULL, cmucal_vclk_ip_hwacg_sys_serial_lif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER1, NULL, cmucal_vclk_ip_timer1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER2, NULL, cmucal_vclk_ip_timer2, NULL, NULL),
};
