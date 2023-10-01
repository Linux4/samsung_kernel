#include "../cmucal.h"
#include "cmucal-node.h"
#include "cmucal-vclk.h"
#include "cmucal-vclklut.h"

/* DVFS VCLK -> Clock Node List */
enum clk_id cmucal_vclk_vdd_int[] = {
	DIV_CLK_G3D_BUSP,
	PLL_G3D,
	DIV_CLK_AUD_BUSD,
	CLK_AUD_MCLK,
	DIV_CLK_AUD_UAIF0,
	DIV_CLK_AUD_UAIF1,
	DIV_CLK_AUD_UAIF2,
	DIV_CLK_AUD_CNT,
	DIV_CLK_AUD_UAIF3,
	DIV_CLK_AUD_UAIF4,
	DIV_CLK_AUD_UAIF5,
	DIV_CLK_AUD_UAIF6,
	DIV_CLK_AUD_AUDIF,
	MUX_CLK_AUD_FM,
	DIV_CLK_AUD_FM_SPDY,
	DIV_CLK_AUD_BUSP,
	MUX_CLKCMU_HSI_MMC_CARD,
	DIV_CLK_MFCMSCL_BUSP,
	MUX_CLKCMU_MFCMSCL_MFC,
	DIV_CLK_CORE_BUSP,
	CLKCMU_CORE_BUS,
	MUX_CLKCMU_CORE_BUS,
	DIV_CLK_DPU_BUSP,
	MUX_CLKCMU_DPU,
	DIV_CLK_AUD_CPU_ACLK,
	DIV_CLK_AUD_CPU_PCLKDBG,
	MUX_CLKCMU_CORE_MMC_EMBD,
	CLKCMU_IS_ITP,
	MUX_CLKCMU_IS_ITP,
	DIV_CLK_IS_BUSP,
	MUX_CLKCMU_IS_BUS,
	MUX_CLKCMU_IS_VRA,
	CLKCMU_MFCMSCL_MCSC,
	MUX_CLKCMU_MFCMSCL_MCSC,
	CLKCMU_MFCMSCL_JPEG,
	MUX_CLKCMU_MFCMSCL_JPEG,
	CLKCMU_CORE_SSS,
	MUX_CLKCMU_CORE_SSS,
	MUX_CLKCMU_MFCMSCL_M2M,
	MUX_CLKCMU_CORE_CCI,
	CLKCMU_IS_GDC,
	MUX_CLKCMU_IS_GDC,
	PLL_SHARED0_DIV3,
	CLKCMU_HSI_BUS,
	CLKCMU_CIS_CLK0,
	MUX_CLKCMU_CIS_CLK0,
	CLKCMU_CIS_CLK1,
	MUX_CLKCMU_CIS_CLK1,
	CLKCMU_PERI_BUS,
	DIV_CLK_PERI_HSI2C_0,
	DIV_CLK_PERI_HSI2C_1,
	DIV_CLK_PERI_HSI2C_2,
	MUX_CLKCMU_PERI_IP,
	MUX_MIF_CMUREF,
	MUX_MIF1_CMUREF,
	CLKCMU_MIF_BUSP,
	CLKCMU_HSI_USB20DRD,
	MUX_CLKCMU_HSI_USB20DRD,
	CLKCMU_CPUCL0_DBG,
	CLKCMU_PERI_UART,
	MUX_CLKCMU_PERI_UART,
	MUX_CMU_CMUREF,
	DIV_CLK_CMU_CMUREF,
	CLKCMU_CIS_CLK2,
	MUX_CLKCMU_CIS_CLK2,
	CLKCMU_CPUCL1_DBG,
	PLL_SHARED0_DIV4,
	PLL_SHARED0_DIV2,
	PLL_SHARED1_DIV4,
	PLL_SHARED1_DIV2,
	PLL_SHARED1_DIV3,
	PLL_MIF,
	PLL_MIF1,
};
enum clk_id cmucal_vclk_vdd_cpu[] = {
	DIV_CLK_CLUSTER0_ACLK,
	PLL_CPUCL0,
	DIV_CLK_CLUSTER1_ACLK,
	PLL_CPUCL1,
};
/* SPECIAL VCLK -> Clock Node List */
enum clk_id cmucal_vclk_mux_clk_apm_i3c[] = {
	MUX_CLK_APM_I3C,
	DIV_CLK_APM_I3C,
};
enum clk_id cmucal_vclk_clkcmu_apm_bus[] = {
	CLKCMU_APM_BUS,
	MUX_CLKCMU_APM_BUS,
};
enum clk_id cmucal_vclk_mux_clk_aud_uaif0[] = {
	MUX_CLK_AUD_UAIF0,
	MUX_CLK_AUD_UAIF1,
	MUX_CLK_AUD_UAIF2,
	MUX_CLK_AUD_UAIF3,
	MUX_CLK_AUD_UAIF4,
	MUX_CLK_AUD_UAIF5,
	MUX_CLK_AUD_UAIF6,
	DIV_CLK_AUD_FM,
};
enum clk_id cmucal_vclk_mux_clk_chub_timer_fclk[] = {
	MUX_CLK_CHUB_TIMER_FCLK,
};
enum clk_id cmucal_vclk_clk_cmgp_adc[] = {
	CLK_CMGP_ADC,
	DIV_CLK_CMGP_ADC,
};
enum clk_id cmucal_vclk_mux_clk_cmu_cmuref[] = {
	MUX_CLK_CMU_CMUREF,
};
enum clk_id cmucal_vclk_mux_clk_hsi_rtc[] = {
	MUX_CLK_HSI_RTC,
};
enum clk_id cmucal_vclk_mux_clkcmu_mif_busp[] = {
	MUX_CLKCMU_MIF_BUSP,
};
enum clk_id cmucal_vclk_mux_clkcmu_cpucl1_dbg[] = {
	MUX_CLKCMU_CPUCL1_DBG,
};
enum clk_id cmucal_vclk_div_clk_chub_dmic_if_div2[] = {
	DIV_CLK_CHUB_DMIC_IF_DIV2,
};
enum clk_id cmucal_vclk_div_clk_chub_dmic[] = {
	DIV_CLK_CHUB_DMIC,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi_cmgp0[] = {
	DIV_CLK_CMGP_USI_CMGP0,
	MUX_CLK_CMGP_USI_CMGP0,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi_cmgp1[] = {
	DIV_CLK_CMGP_USI_CMGP1,
	MUX_CLK_CMGP_USI_CMGP1,
};
enum clk_id cmucal_vclk_ap2cp_shared0_pll_clk[] = {
	AP2CP_SHARED0_PLL_CLK,
};
enum clk_id cmucal_vclk_div_clk_cpucl0_cmuref[] = {
	DIV_CLK_CPUCL0_CMUREF,
};
enum clk_id cmucal_vclk_div_clk_cluster0_atclk[] = {
	DIV_CLK_CLUSTER0_ATCLK,
	DIV_CLK_CLUSTER0_PERIPHCLK,
};
enum clk_id cmucal_vclk_div_clk_cpucl1_cmuref[] = {
	DIV_CLK_CPUCL1_CMUREF,
};
enum clk_id cmucal_vclk_div_clk_cluster1_periphclk[] = {
	DIV_CLK_CLUSTER1_PERIPHCLK,
	DIV_CLK_CLUSTER1_ATCLK,
};
enum clk_id cmucal_vclk_clkcmu_peri_ip[] = {
	CLKCMU_PERI_IP,
};
enum clk_id cmucal_vclk_div_clk_peri_spi_0[] = {
	DIV_CLK_PERI_SPI_0,
};
/* COMMON VCLK -> Clock Node List */
enum clk_id cmucal_vclk_blk_aud[] = {
	MUX_CLK_AUD_CPU_HCH,
	DIV_CLK_AUD_CPU,
	PLL_AUD,
};
enum clk_id cmucal_vclk_blk_cmu[] = {
	CLKCMU_HSI_MMC_CARD,
	CLKCMU_MFCMSCL_MFC,
	AP2CP_SHARED2_PLL_CLK,
	CLKCMU_DPU,
	CLKCMU_CORE_MMC_EMBD,
	CLKCMU_IS_BUS,
	CLKCMU_IS_VRA,
	CLKCMU_MFCMSCL_M2M,
	CLKCMU_CORE_CCI,
	MUX_CLKCMU_HSI_BUS,
	MUX_CLKCMU_PERI_BUS,
	MUX_CLKCMU_CPUCL0_DBG,
	PLL_SHARED0,
	AP2CP_SHARED1_PLL_CLK,
	PLL_SHARED1,
	PLL_MMC,
};
enum clk_id cmucal_vclk_blk_apm[] = {
	DIV_CLK_APM_BUS,
	MUX_CLK_APM_BUS,
	CLKCMU_CHUB_BUS,
	MUX_CLKCMU_CHUB_BUS,
};
enum clk_id cmucal_vclk_blk_chub[] = {
	DIV_CLK_CHUB_BUS,
	MUX_CLK_CHUB_BUS,
	DIV_CLK_CHUB_DMIC_IF,
};
enum clk_id cmucal_vclk_blk_core[] = {
	MUX_CLK_CORE_GIC,
};
enum clk_id cmucal_vclk_blk_cpucl0[] = {
	DIV_CLK_CPUCL0_PCLK,
	DIV_CLK_CLUSTER0_PCLKDBG,
};
enum clk_id cmucal_vclk_blk_cpucl1[] = {
	DIV_CLK_CPUCL1_PCLK,
	DIV_CLK_CLUSTER1_PCLKDBG,
};
/* GATE VCLK -> Clock Node List */
enum clk_id cmucal_vclk_ip_lhm_axi_p_apm[] = {
	GOUT_BLK_APM_UID_LHM_AXI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_apm[] = {
	GOUT_BLK_APM_UID_LHS_AXI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_ap[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_cp[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_apm[] = {
	GOUT_BLK_APM_UID_SYSREG_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_pmu_alive[] = {
	GOUT_BLK_APM_UID_APBIF_PMU_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_gpio_alive[] = {
	GOUT_BLK_APM_UID_APBIF_GPIO_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apm_cmu_apm[] = {
	CLK_BLK_APM_UID_APM_CMU_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_top_rtc[] = {
	GOUT_BLK_APM_UID_APBIF_TOP_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_intmem[] = {
	GOUT_BLK_APM_UID_INTMEM_IPCLKPORT_ACLK,
	GOUT_BLK_APM_UID_INTMEM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_modem[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_gnss[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_cp[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_cp_s[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_CP_S_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_wlbt_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_WLBT_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_wlbt_abox[] = {
	GOUT_BLK_APM_UID_MAILBOX_WLBT_ABOX_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pmu_intr_gen[] = {
	GOUT_BLK_APM_UID_PMU_INTR_GEN_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_wlbt[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_apm[] = {
	GOUT_BLK_APM_UID_XIU_DP_APM_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_CP_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_CP_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_speedy_apm[] = {
	GOUT_BLK_APM_UID_SPEEDY_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_grebeintegration[] = {
	GOUT_BLK_APM_UID_GREBEINTEGRATION_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_chub[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_lp_chub[] = {
	GOUT_BLK_APM_UID_LHS_AXI_LP_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_CP_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_gnss_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_GNSS_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_gnss_wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_GNSS_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_apm[] = {
	GOUT_BLK_APM_UID_D_TZPC_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_apm[] = {
	GOUT_BLK_APM_UID_WDT_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_rtc[] = {
	GOUT_BLK_APM_UID_APBIF_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_rom_crc32_host[] = {
	GOUT_BLK_APM_UID_ROM_CRC32_HOST_IPCLKPORT_PCLK,
	GOUT_BLK_APM_UID_ROM_CRC32_HOST_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_i3c_apm_pmic[] = {
	GOUT_BLK_APM_UID_I3C_APM_PMIC_IPCLKPORT_I_PCLK,
	GOUT_BLK_APM_UID_I3C_APM_PMIC_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_aud_cmu_aud[] = {
	CLK_BLK_AUD_UID_AUD_CMU_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_aud[] = {
	GOUT_BLK_AUD_UID_LHS_AXI_D_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_aud[] = {
	GOUT_BLK_AUD_UID_PPMU_AUD_IPCLKPORT_PCLK,
	GOUT_BLK_AUD_UID_PPMU_AUD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_aud[] = {
	GOUT_BLK_AUD_UID_SYSMMU_AUD_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_peri_axi_asb[] = {
	GOUT_BLK_AUD_UID_PERI_AXI_ASB_IPCLKPORT_ACLKM,
	GOUT_BLK_AUD_UID_PERI_AXI_ASB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_axi_us_32to128[] = {
	GOUT_BLK_AUD_UID_AXI_US_32TO128_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_wdt_aud[] = {
	GOUT_BLK_AUD_UID_WDT_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_abox[] = {
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_DAP,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_CA32,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_ASB,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_SPDY,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF0,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF1,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF2,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_CNT,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF3,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF4,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF6,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF5,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_OSC_SPDY,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_aud[] = {
	GOUT_BLK_AUD_UID_AD_APB_SYSMMU_AUD_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_dftmux_aud[] = {
	GOUT_BLK_AUD_UID_DFTMUX_AUD_IPCLKPORT_AUD_CODEC_MCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_aud_s[] = {
	GOUT_BLK_AUD_UID_AD_APB_SYSMMU_AUD_S_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_aud[] = {
	GOUT_BLK_AUD_UID_LHM_AXI_P_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_aud[] = {
	GOUT_BLK_AUD_UID_SYSREG_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_aud[] = {
	GOUT_BLK_AUD_UID_GPIO_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_aud[] = {
	GOUT_BLK_AUD_UID_D_TZPC_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_chub_cmu_chub[] = {
	CLK_BLK_CHUB_UID_CHUB_CMU_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_d_chub[] = {
	GOUT_BLK_CHUB_UID_BAAW_D_CHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_c_chub[] = {
	GOUT_BLK_CHUB_UID_BAAW_C_CHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_cm4_chub[] = {
	GOUT_BLK_CHUB_UID_CM4_CHUB_IPCLKPORT_FCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_lp_chub[] = {
	GOUT_BLK_CHUB_UID_LHM_AXI_LP_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_chub[] = {
	GOUT_BLK_CHUB_UID_LHM_AXI_P_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_chub[] = {
	GOUT_BLK_CHUB_UID_LHS_AXI_D_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_c_chub[] = {
	GOUT_BLK_CHUB_UID_LHS_AXI_C_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pwm_chub[] = {
	GOUT_BLK_CHUB_UID_PWM_CHUB_IPCLKPORT_I_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_sweeper_d_chub[] = {
	GOUT_BLK_CHUB_UID_SWEEPER_D_CHUB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sweeper_c_chub[] = {
	GOUT_BLK_CHUB_UID_SWEEPER_C_CHUB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_chub[] = {
	GOUT_BLK_CHUB_UID_SYSREG_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer_chub[] = {
	GOUT_BLK_CHUB_UID_TIMER_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_chub[] = {
	GOUT_BLK_CHUB_UID_WDT_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ahb_busmatrix_chub[] = {
	GOUT_BLK_CHUB_UID_AHB_BUSMATRIX_CHUB_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_chub[] = {
	GOUT_BLK_CHUB_UID_D_TZPC_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_bps_axi_lp_chub[] = {
	GOUT_BLK_CHUB_UID_BPS_AXI_LP_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bps_axi_p_chub[] = {
	GOUT_BLK_CHUB_UID_BPS_AXI_P_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xhb_p_chub[] = {
	GOUT_BLK_CHUB_UID_XHB_P_CHUB_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_xhb_lp_chub[] = {
	GOUT_BLK_CHUB_UID_XHB_LP_CHUB_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb0[] = {
	GOUT_BLK_CHUB_UID_DMIC_AHB0_IPCLKPORT_PCLK,
	GOUT_BLK_CHUB_UID_DMIC_AHB0_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_dmic_if[] = {
	GOUT_BLK_CHUB_UID_DMIC_IF_IPCLKPORT_PCLK,
	GOUT_BLK_CHUB_UID_DMIC_IF_IPCLKPORT_DMIC_IF_CLK,
	GOUT_BLK_CHUB_UID_DMIC_IF_IPCLKPORT_DMIC_CLK,
};
enum clk_id cmucal_vclk_ip_u_dmic_clk_scan_mux[] = {
	GOUT_BLK_CHUB_UID_U_DMIC_CLK_SCAN_MUX_IPCLKPORT_D0,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic0[] = {
	GOUT_BLK_CHUB_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_CHUB_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_CHUB_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_cmgp_cmu_cmgp[] = {
	CLK_BLK_CMGP_UID_CMGP_CMU_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2cp[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2gnss[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2wlbt[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_cmgp[] = {
	GOUT_BLK_CMGP_UID_GPIO_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2chub[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2pmu_chub[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2PMU_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2pmu_ap[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2PMU_AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cmgp[] = {
	GOUT_BLK_CMGP_UID_D_TZPC_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp0[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp1[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_adc_cmgp[] = {
	GOUT_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_PCLK_S0,
	GOUT_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_PCLK_S1,
};
enum clk_id cmucal_vclk_ip_otp[] = {
	CLK_BLK_CMU_UID_OTP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_pdma0[] = {
	GOUT_BLK_CORE_UID_AD_APB_PDMA0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_spdma[] = {
	GOUT_BLK_CORE_UID_AD_APB_SPDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_axi_gic[] = {
	GOUT_BLK_CORE_UID_AD_AXI_GIC_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_baaw_p_gnss[] = {
	GOUT_BLK_CORE_UID_BAAW_P_GNSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_chub[] = {
	GOUT_BLK_CORE_UID_BAAW_P_CHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_modem[] = {
	GOUT_BLK_CORE_UID_BAAW_P_MODEM_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_wlbt[] = {
	GOUT_BLK_CORE_UID_BAAW_P_WLBT_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_gic[] = {
	GOUT_BLK_CORE_UID_GIC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_is[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D0_IS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mfcmscl[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_MFCMSCL_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dpu[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_hsi[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_HSI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_modem[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D0_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_modem[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D1_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_aud[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_apm[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_chub[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_cssys[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_G_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_gnss[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_wlbt[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_apm[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_chub[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_cpucl0[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dpu[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_hsi[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_HSI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_gnss[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_is[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_IS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mfcmscl[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MFCMSCL_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_modem[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_peri[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_PERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_wlbt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pdma_core[] = {
	GOUT_BLK_CORE_UID_PDMA_CORE_IPCLKPORT_ACLK_PDMA0,
};
enum clk_id cmucal_vclk_ip_sfr_apbif_cmu_topc[] = {
	GOUT_BLK_CORE_UID_SFR_APBIF_CMU_TOPC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_spdma_core[] = {
	GOUT_BLK_CORE_UID_SPDMA_CORE_IPCLKPORT_ACLK_PDMA1,
};
enum clk_id cmucal_vclk_ip_sysreg_core[] = {
	GOUT_BLK_CORE_UID_SYSREG_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d_core[] = {
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_p_core[] = {
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_PCLK_PCORE,
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_ACLK_PCORE,
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_CCLK_PCORE,
};
enum clk_id cmucal_vclk_ip_xiu_d_core[] = {
	GOUT_BLK_CORE_UID_XIU_D_CORE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_core[] = {
	GOUT_BLK_CORE_UID_D_TZPC_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_g3d[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_core_cmu_core[] = {
	CLK_BLK_CORE_UID_CORE_CMU_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_g3d[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mif_rt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D0_MIF_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mif_nrt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D0_MIF_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mif_rt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D1_MIF_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mif_nrt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D1_MIF_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif0[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MIF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif1[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MIF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_is[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D1_IS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_aud[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_axi_sss[] = {
	GOUT_BLK_CORE_UID_AD_AXI_SSS_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_adm_ahb_sss[] = {
	GOUT_BLK_CORE_UID_ADM_AHB_SSS_IPCLKPORT_HCLKM,
};
enum clk_id cmucal_vclk_ip_rtic[] = {
	GOUT_BLK_CORE_UID_RTIC_IPCLKPORT_I_ACLK,
	GOUT_BLK_CORE_UID_RTIC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_sss[] = {
	GOUT_BLK_CORE_UID_SSS_IPCLKPORT_I_ACLK,
	GOUT_BLK_CORE_UID_SSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_core[] = {
	GOUT_BLK_CORE_UID_GPIO_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_axi_mmc[] = {
	GOUT_BLK_CORE_UID_AD_AXI_MMC_IPCLKPORT_ACLKM,
	GOUT_BLK_CORE_UID_AD_AXI_MMC_IPCLKPORT_ACLKS,
};
enum clk_id cmucal_vclk_ip_mmc_embd[] = {
	GOUT_BLK_CORE_UID_MMC_EMBD_IPCLKPORT_I_ACLK,
	GOUT_BLK_CORE_UID_MMC_EMBD_IPCLKPORT_SDCLKIN,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mif_cpu[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D1_MIF_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mif_cpu[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D0_MIF_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d_cpucl0[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d_cpucl1[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D_CPUCL1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_cpucl1[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CPUCL1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mif_cp[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D0_MIF_CP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mif_cp[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D1_MIF_CP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_cci_550[] = {
	GOUT_BLK_CORE_UID_AD_APB_CCI_550_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_cci_550[] = {
	GOUT_BLK_CORE_UID_CCI_550_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_ace_cpucl0[] = {
	GOUT_BLK_CORE_UID_PPMU_ACE_CPUCL0_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPMU_ACE_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_ace_cpucl1[] = {
	GOUT_BLK_CORE_UID_PPMU_ACE_CPUCL1_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPMU_ACE_CPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_adm_apb_g_dump_pc_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_ADM_APB_G_DUMP_PC_CPUCL0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_axi2apb_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_AXI2APB_CPUCL0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ads_ahb_g_cssys_sss[] = {
	GOUT_BLK_CPUCL0_UID_ADS_AHB_G_CSSYS_SSS_IPCLKPORT_HCLKS,
};
enum clk_id cmucal_vclk_ip_ads_apb_g_dump_pc_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_ADS_APB_G_DUMP_PC_CPUCL0_IPCLKPORT_PCLKS,
};
enum clk_id cmucal_vclk_ip_ads_apb_g_cssys_cpucl1[] = {
	GOUT_BLK_CPUCL0_UID_ADS_APB_G_CSSYS_CPUCL1_IPCLKPORT_PCLKS,
};
enum clk_id cmucal_vclk_ip_cluster0[] = {
	CLK_BLK_CPUCL0_UID_CLUSTER0_IPCLKPORT_ATCLK,
	CLK_BLK_CPUCL0_UID_CLUSTER0_IPCLKPORT_PCLK,
	CLK_BLK_CPUCL0_UID_CLUSTER0_IPCLKPORT_PERIPHCLK,
	CLK_BLK_CPUCL0_UID_CLUSTER0_IPCLKPORT_SCLK,
};
enum clk_id cmucal_vclk_ip_lhs_ace_d_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_LHS_ACE_D_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cssys_dbg[] = {
	GOUT_BLK_CPUCL0_UID_CSSYS_DBG_IPCLKPORT_PCLKDBG,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_cssys[] = {
	GOUT_BLK_CPUCL0_UID_LHS_AXI_G_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_secjtag[] = {
	GOUT_BLK_CPUCL0_UID_SECJTAG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_SYSREG_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_D_TZPC_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_adm_apb_g_cssys_core[] = {
	GOUT_BLK_CPUCL0_UID_ADM_APB_G_CSSYS_CORE_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_LHM_AXI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cpucl0_cmu_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_CPUCL0_CMU_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl1_cmu_cpucl1[] = {
	CLK_BLK_CPUCL1_UID_CPUCL1_CMU_CPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_axi2apb_cpucl1[] = {
	GOUT_BLK_CPUCL1_UID_AXI2APB_CPUCL1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cpucl1[] = {
	GOUT_BLK_CPUCL1_UID_D_TZPC_CPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_cpucl1[] = {
	GOUT_BLK_CPUCL1_UID_LHM_AXI_P_CPUCL1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cpucl1[] = {
	GOUT_BLK_CPUCL1_UID_SYSREG_CPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_ace_d_cpucl1[] = {
	GOUT_BLK_CPUCL1_UID_LHS_ACE_D_CPUCL1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cluster1[] = {
	CLK_BLK_CPUCL1_UID_CLUSTER1_IPCLKPORT_SCLK,
	CLK_BLK_CPUCL1_UID_CLUSTER1_IPCLKPORT_ATCLK,
	CLK_BLK_CPUCL1_UID_CLUSTER1_IPCLKPORT_PCLK,
	CLK_BLK_CPUCL1_UID_CLUSTER1_IPCLKPORT_PERIPHCLK,
};
enum clk_id cmucal_vclk_ip_adm_apb_g_cssys_cpucl1[] = {
	GOUT_BLK_CPUCL1_UID_ADM_APB_G_CSSYS_CPUCL1_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_dpu_cmu_dpu[] = {
	CLK_BLK_DPU_UID_DPU_CMU_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_decon0[] = {
	GOUT_BLK_DPU_UID_AD_APB_DECON0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_dpu[] = {
	GOUT_BLK_DPU_UID_LHM_AXI_P_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dpu[] = {
	GOUT_BLK_DPU_UID_LHS_AXI_D_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dpu[] = {
	GOUT_BLK_DPU_UID_SYSREG_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dpu[] = {
	GOUT_BLK_DPU_UID_PPMU_DPU_IPCLKPORT_ACLK,
	GOUT_BLK_DPU_UID_PPMU_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_smmu_dpu[] = {
	GOUT_BLK_DPU_UID_SMMU_DPU_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_dpu[] = {
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_DECON0,
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_DPP,
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_DMA,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dpu[] = {
	GOUT_BLK_DPU_UID_D_TZPC_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_g3d[] = {
	GOUT_BLK_G3D_UID_SYSREG_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_g3d_cmu_g3d[] = {
	CLK_BLK_G3D_UID_G3D_CMU_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpu[] = {
	CLK_BLK_G3D_UID_GPU_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_g3d[] = {
	GOUT_BLK_G3D_UID_D_TZPC_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_int_g3d[] = {
	GOUT_BLK_G3D_UID_LHS_AXI_INT_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_int_g3d[] = {
	GOUT_BLK_G3D_UID_LHM_AXI_INT_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_g3d[] = {
	GOUT_BLK_G3D_UID_LHS_AXI_D_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gray2bin_g3d[] = {
	GOUT_BLK_G3D_UID_GRAY2BIN_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_g3d[] = {
	GOUT_BLK_G3D_UID_LHM_AXI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gpio_hsi[] = {
	GOUT_BLK_HSI_UID_GPIO_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_hsi[] = {
	GOUT_BLK_HSI_UID_LHM_AXI_P_HSI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_hsi[] = {
	GOUT_BLK_HSI_UID_LHS_AXI_D_HSI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_hsi[] = {
	GOUT_BLK_HSI_UID_PPMU_HSI_IPCLKPORT_ACLK,
	GOUT_BLK_HSI_UID_PPMU_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_hsi[] = {
	GOUT_BLK_HSI_UID_SYSREG_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_hsi[] = {
	GOUT_BLK_HSI_UID_XIU_D_HSI_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_mmc_card[] = {
	GOUT_BLK_HSI_UID_MMC_CARD_IPCLKPORT_I_ACLK,
	GOUT_BLK_HSI_UID_MMC_CARD_IPCLKPORT_SDCLKIN,
};
enum clk_id cmucal_vclk_ip_d_tzpc_hsi[] = {
	GOUT_BLK_HSI_UID_D_TZPC_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usb20drd_top[] = {
	GOUT_BLK_HSI_UID_USB20DRD_TOP_IPCLKPORT_ACLK_PHYCTRL_20,
	GOUT_BLK_HSI_UID_USB20DRD_TOP_IPCLKPORT_BUS_CLK_EARLY,
	CLK_BLK_HSI_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_50,
	CLK_BLK_HSI_UID_USB20DRD_TOP_IPCLKPORT_I_USB20_PHY_REFCLK_26,
	CLK_BLK_HSI_UID_USB20DRD_TOP_IPCLKPORT_I_RTC_CLK__ALV,
};
enum clk_id cmucal_vclk_ip_us_64to128_hsi[] = {
	GOUT_BLK_HSI_UID_US_64TO128_HSI_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_hsi_cmu_hsi[] = {
	CLK_BLK_HSI_UID_HSI_CMU_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_is_cmu_is[] = {
	CLK_BLK_IS_UID_IS_CMU_IS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_is[] = {
	GOUT_BLK_IS_UID_SYSREG_IS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_is[] = {
	GOUT_BLK_IS_UID_LHM_AXI_P_IS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_is[] = {
	GOUT_BLK_IS_UID_LHS_AXI_D0_IS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_is[] = {
	GOUT_BLK_IS_UID_D_TZPC_IS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_is[] = {
	GOUT_BLK_IS_UID_XIU_D0_IS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_is0[] = {
	GOUT_BLK_IS_UID_PPMU_IS0_IPCLKPORT_ACLK,
	GOUT_BLK_IS_UID_PPMU_IS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_is1[] = {
	GOUT_BLK_IS_UID_PPMU_IS1_IPCLKPORT_PCLK,
	GOUT_BLK_IS_UID_PPMU_IS1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_is[] = {
	GOUT_BLK_IS_UID_LHS_AXI_D1_IS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_is0[] = {
	GOUT_BLK_IS_UID_SYSMMU_IS0_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_sysmmu_is1[] = {
	GOUT_BLK_IS_UID_SYSMMU_IS1_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_csis0[] = {
	GOUT_BLK_IS_UID_CSIS0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_csis1[] = {
	GOUT_BLK_IS_UID_CSIS1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_csis2[] = {
	GOUT_BLK_IS_UID_CSIS2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_is_top[] = {
	GOUT_BLK_IS_UID_IS_TOP_IPCLKPORT_CLK_CSIS_DMA,
	GOUT_BLK_IS_UID_IS_TOP_IPCLKPORT_CLK_IPP,
	GOUT_BLK_IS_UID_IS_TOP_IPCLKPORT_CLK_ITP,
	GOUT_BLK_IS_UID_IS_TOP_IPCLKPORT_CLK_MCSC,
	GOUT_BLK_IS_UID_IS_TOP_IPCLKPORT_CLK_VRA,
	GOUT_BLK_IS_UID_IS_TOP_IPCLKPORT_CLK_GDC,
};
enum clk_id cmucal_vclk_ip_xiu_d1_is[] = {
	GOUT_BLK_IS_UID_XIU_D1_IS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_csis0[] = {
	GOUT_BLK_IS_UID_AD_APB_CSIS0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_csis1[] = {
	GOUT_BLK_IS_UID_AD_APB_CSIS1_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_csis2[] = {
	GOUT_BLK_IS_UID_AD_APB_CSIS2_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_csis_dma[] = {
	GOUT_BLK_IS_UID_AD_APB_CSIS_DMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_ipp[] = {
	GOUT_BLK_IS_UID_AD_APB_IPP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_itp[] = {
	GOUT_BLK_IS_UID_AD_APB_ITP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_mcsc[] = {
	GOUT_BLK_IS_UID_AD_APB_MCSC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_vra[] = {
	GOUT_BLK_IS_UID_AD_APB_VRA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_is0_ns[] = {
	GOUT_BLK_IS_UID_AD_APB_SYSMMU_IS0_NS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_is0_s[] = {
	GOUT_BLK_IS_UID_AD_APB_SYSMMU_IS0_S_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_is1_ns[] = {
	GOUT_BLK_IS_UID_AD_APB_SYSMMU_IS1_NS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_is1_s[] = {
	GOUT_BLK_IS_UID_AD_APB_SYSMMU_IS1_S_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_axi_vra[] = {
	GOUT_BLK_IS_UID_AD_AXI_VRA_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_gdc[] = {
	GOUT_BLK_IS_UID_AD_APB_GDC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_axi_gdc[] = {
	GOUT_BLK_IS_UID_AD_AXI_GDC_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_mfc[] = {
	GOUT_BLK_MFCMSCL_UID_MFC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_PPMU_MFCMSCL_IPCLKPORT_PCLK,
	GOUT_BLK_MFCMSCL_UID_PPMU_MFCMSCL_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_SYSREG_MFCMSCL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_m2m[] = {
	GOUT_BLK_MFCMSCL_UID_M2M_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_LHM_AXI_P_MFCMSCL_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_D_TZPC_MFCMSCL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_apb_mfc[] = {
	GOUT_BLK_MFCMSCL_UID_AS_APB_MFC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_as_apb_sysmmu_ns_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_AS_APB_SYSMMU_NS_MFCMSCL_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_as_apb_sysmmu_s_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_AS_APB_SYSMMU_S_MFCMSCL_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_xiu_d_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_XIU_D_MFCMSCL_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_SYSMMU_MFCMSCL_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_as_apb_m2m[] = {
	GOUT_BLK_MFCMSCL_UID_AS_APB_M2M_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_as_axi_m2m[] = {
	GOUT_BLK_MFCMSCL_UID_AS_AXI_M2M_IPCLKPORT_ACLKM,
	GOUT_BLK_MFCMSCL_UID_AS_AXI_M2M_IPCLKPORT_ACLKS,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_LHS_AXI_D_MFCMSCL_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_as_apb_mcsc[] = {
	GOUT_BLK_MFCMSCL_UID_AS_APB_MCSC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_mcsc[] = {
	GOUT_BLK_MFCMSCL_UID_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_as_axi_jpeg[] = {
	GOUT_BLK_MFCMSCL_UID_AS_AXI_JPEG_IPCLKPORT_ACLKM,
	GOUT_BLK_MFCMSCL_UID_AS_AXI_JPEG_IPCLKPORT_ACLKS,
};
enum clk_id cmucal_vclk_ip_as_axi_mcsc[] = {
	GOUT_BLK_MFCMSCL_UID_AS_AXI_MCSC_IPCLKPORT_ACLKS,
	GOUT_BLK_MFCMSCL_UID_AS_AXI_MCSC_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_as_apb_jpeg[] = {
	GOUT_BLK_MFCMSCL_UID_AS_APB_JPEG_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_jpeg[] = {
	GOUT_BLK_MFCMSCL_UID_JPEG_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_mfcmscl_cmu_mfcmscl[] = {
	CLK_BLK_MFCMSCL_UID_MFCMSCL_CMU_MFCMSCL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_axi2apb_mfcmscl[] = {
	GOUT_BLK_MFCMSCL_UID_AXI2APB_MFCMSCL_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dmc_cpu[] = {
	GOUT_BLK_MIF_UID_PPMU_DMC_CPU_IPCLKPORT_PCLK,
	CLK_BLK_MIF_UID_PPMU_DMC_CPU_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mif[] = {
	GOUT_BLK_MIF_UID_SYSREG_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mif_cmu_mif[] = {
	CLK_BLK_MIF_UID_MIF_CMU_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmc[] = {
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_PF,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_PF,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_SECURE,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_SECURE,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mif[] = {
	GOUT_BLK_MIF_UID_D_TZPC_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mif[] = {
	GOUT_BLK_MIF_UID_LHM_AXI_P_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ddr_phy[] = {
	GOUT_BLK_MIF_UID_DDR_PHY_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_ddr_phy[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DDR_PHY_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DMC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc_pf[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DMC_PF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc_secure[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DMC_SECURE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif_cpu[] = {
	CLK_BLK_MIF_UID_LHM_AXI_D_MIF_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif_nrt[] = {
	CLK_BLK_MIF_UID_LHM_AXI_D_MIF_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif_rt[] = {
	CLK_BLK_MIF_UID_LHM_AXI_D_MIF_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif_cp[] = {
	CLK_BLK_MIF_UID_LHM_AXI_D_MIF_CP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ddr_phy1[] = {
	GOUT_BLK_MIF1_UID_DDR_PHY1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmc1[] = {
	CLK_BLK_MIF1_UID_DMC1_IPCLKPORT_ACLK,
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_PCLK,
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_PCLK_PF,
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_PCLK_PF,
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_PCLK_PPMPU,
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_PCLK_PPMPU,
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_PCLK_SECURE,
	GOUT_BLK_MIF1_UID_DMC1_IPCLKPORT_PCLK_SECURE,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mif1[] = {
	GOUT_BLK_MIF1_UID_D_TZPC_MIF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif1_cpu[] = {
	CLK_BLK_MIF1_UID_LHM_AXI_D_MIF1_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif1_nrt[] = {
	CLK_BLK_MIF1_UID_LHM_AXI_D_MIF1_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif1_rt[] = {
	CLK_BLK_MIF1_UID_LHM_AXI_D_MIF1_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mif1[] = {
	GOUT_BLK_MIF1_UID_LHM_AXI_P_MIF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dcm1_cpu[] = {
	CLK_BLK_MIF1_UID_PPMU_DCM1_CPU_IPCLKPORT_ACLK,
	GOUT_BLK_MIF1_UID_PPMU_DCM1_CPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_ddr_phy1[] = {
	GOUT_BLK_MIF1_UID_SFRAPB_BRIDGE_DDR_PHY1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc1_ppmpu[] = {
	GOUT_BLK_MIF1_UID_SFRAPB_BRIDGE_DMC1_PPMPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc1_secure[] = {
	GOUT_BLK_MIF1_UID_SFRAPB_BRIDGE_DMC1_SECURE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfr_apb_bridge_dmc1[] = {
	GOUT_BLK_MIF1_UID_SFR_APB_BRIDGE_DMC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfr_apb_bridge_dmc1_pf[] = {
	GOUT_BLK_MIF1_UID_SFR_APB_BRIDGE_DMC1_PF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mif1[] = {
	GOUT_BLK_MIF1_UID_SYSREG_MIF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mif1_cmu_mif1[] = {
	CLK_BLK_MIF1_UID_MIF1_CMU_MIF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_modem_cmu_modem[] = {
	CLK_BLK_MODEM_UID_MODEM_CMU_MODEM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_tmu[] = {
	GOUT_BLK_PERI_UID_BUSIF_TMU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp_con_top[] = {
	GOUT_BLK_PERI_UID_OTP_CON_TOP_IPCLKPORT_PCLK,
	CLK_BLK_PERI_UID_OTP_CON_TOP_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_peri[] = {
	GOUT_BLK_PERI_UID_SYSREG_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_0[] = {
	GOUT_BLK_PERI_UID_WDT_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mct[] = {
	GOUT_BLK_PERI_UID_MCT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pwm_motor[] = {
	GOUT_BLK_PERI_UID_PWM_MOTOR_IPCLKPORT_I_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_gpio_peri[] = {
	GOUT_BLK_PERI_UID_GPIO_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_spi_0[] = {
	GOUT_BLK_PERI_UID_SPI_0_IPCLKPORT_PCLK,
	GOUT_BLK_PERI_UID_SPI_0_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_uart[] = {
	GOUT_BLK_PERI_UID_UART_IPCLKPORT_PCLK,
	GOUT_BLK_PERI_UID_UART_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_hsi2c_0[] = {
	GOUT_BLK_PERI_UID_HSI2C_0_IPCLKPORT_PCLK,
	GOUT_BLK_PERI_UID_HSI2C_0_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_peri_cmu_peri[] = {
	CLK_BLK_PERI_UID_PERI_CMU_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hsi2c_1[] = {
	GOUT_BLK_PERI_UID_HSI2C_1_IPCLKPORT_PCLK,
	GOUT_BLK_PERI_UID_HSI2C_1_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_hsi2c_2[] = {
	GOUT_BLK_PERI_UID_HSI2C_2_IPCLKPORT_PCLK,
	GOUT_BLK_PERI_UID_HSI2C_2_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_peri[] = {
	GOUT_BLK_PERI_UID_LHM_AXI_P_PERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peri[] = {
	GOUT_BLK_PERI_UID_D_TZPC_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_3[] = {
	GOUT_BLK_PERI_UID_I2C_3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_4[] = {
	GOUT_BLK_PERI_UID_I2C_4_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_1[] = {
	GOUT_BLK_PERI_UID_WDT_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_0[] = {
	GOUT_BLK_PERI_UID_I2C_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_1[] = {
	GOUT_BLK_PERI_UID_I2C_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_2[] = {
	GOUT_BLK_PERI_UID_I2C_2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_5[] = {
	GOUT_BLK_PERI_UID_I2C_5_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_6[] = {
	GOUT_BLK_PERI_UID_I2C_6_IPCLKPORT_PCLK,
};

/* DVFS VCLK -> LUT List */
struct vclk_lut cmucal_vclk_vdd_int_lut[] = {
	{1000000, vdd_int_od_lut_params},
	{800000, vdd_int_nm_lut_params},
	{600000, vdd_int_ud_lut_params},
	{26000, vdd_int_reset_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cpu_lut[] = {
	{2000000, vdd_cpu_sod_lut_params},
	{1600000, vdd_cpu_od_lut_params},
	{1150000, vdd_cpu_nm_lut_params},
	{650000, vdd_cpu_ud_lut_params},
};

/* SPECIAL VCLK -> LUT List */
struct vclk_lut cmucal_vclk_mux_clk_apm_i3c_lut[] = {
	{200000, mux_clk_apm_i3c_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_apm_bus_lut[] = {
	{400000, clkcmu_apm_bus_ud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_aud_uaif0_lut[] = {
	{30000, mux_clk_aud_uaif0_ud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_chub_timer_fclk_lut[] = {
	{30000, mux_clk_chub_timer_fclk_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clk_cmgp_adc_lut[] = {
	{28571, clk_cmgp_adc_nm_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_cmu_cmuref_lut[] = {
	{400000, mux_clk_cmu_cmuref_ud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_hsi_rtc_lut[] = {
	{33, mux_clk_hsi_rtc_nm_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_mif_busp_lut[] = {
	{400000, mux_clkcmu_mif_busp_ud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_cpucl1_dbg_lut[] = {
	{400000, mux_clkcmu_cpucl1_dbg_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_dmic_if_div2_lut[] = {
	{30000, div_clk_chub_dmic_if_div2_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_dmic_lut[] = {
	{60000, div_clk_chub_dmic_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi_cmgp0_lut[] = {
	{200000, div_clk_cmgp_usi_cmgp0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi_cmgp1_lut[] = {
	{200000, div_clk_cmgp_usi_cmgp1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_ap2cp_shared0_pll_clk_lut[] = {
	{800000, ap2cp_shared0_pll_clk_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl0_cmuref_lut[] = {
	{1000000, div_clk_cpucl0_cmuref_sod_lut_params},
	{800000, div_clk_cpucl0_cmuref_od_lut_params},
	{575000, div_clk_cpucl0_cmuref_nm_lut_params},
	{325000, div_clk_cpucl0_cmuref_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cluster0_atclk_lut[] = {
	{1000000, div_clk_cluster0_atclk_sod_lut_params},
	{800000, div_clk_cluster0_atclk_od_lut_params},
	{575000, div_clk_cluster0_atclk_nm_lut_params},
	{325000, div_clk_cluster0_atclk_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl1_cmuref_lut[] = {
	{1000000, div_clk_cpucl1_cmuref_sod_lut_params},
	{800000, div_clk_cpucl1_cmuref_od_lut_params},
	{575000, div_clk_cpucl1_cmuref_nm_lut_params},
	{325000, div_clk_cpucl1_cmuref_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cluster1_periphclk_lut[] = {
	{1000000, div_clk_cluster1_periphclk_sod_lut_params},
	{800000, div_clk_cluster1_periphclk_od_lut_params},
	{575000, div_clk_cluster1_periphclk_nm_lut_params},
	{325000, div_clk_cluster1_periphclk_ud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_peri_ip_lut[] = {
	{400000, clkcmu_peri_ip_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_spi_0_lut[] = {
	{400000, div_clk_peri_spi_0_ud_lut_params},
};

/* COMMON VCLK -> LUT List */
struct vclk_lut cmucal_vclk_blk_aud_lut[] = {
	{1033000, blk_aud_od_lut_params},
	{800000, blk_aud_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cmu_lut[] = {
	{808000, blk_cmu_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_apm_lut[] = {
	{400000, blk_apm_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_chub_lut[] = {
	{400000, blk_chub_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_core_lut[] = {
	{266667, blk_core_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl0_lut[] = {
	{500000, blk_cpucl0_sod_lut_params},
	{400000, blk_cpucl0_od_lut_params},
	{287500, blk_cpucl0_nm_lut_params},
	{162500, blk_cpucl0_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl1_lut[] = {
	{500000, blk_cpucl1_sod_lut_params},
	{400000, blk_cpucl1_od_lut_params},
	{287500, blk_cpucl1_nm_lut_params},
	{162500, blk_cpucl1_ud_lut_params},
};

/* Switch VCLK -> LUT Parameter List */
struct switch_lut mux_clk_aud_cpu_lut[] = {
	{800000, 1, 0},
	{666500, 2, 0},
};
struct switch_lut mux_clk_cpucl0_pll_lut[] = {
	{1600000, 0, 0},
	{800000, 2, 0},
	{400000, 2, 1},
};
struct switch_lut mux_clk_cpucl1_pll_lut[] = {
	{1600000, 0, 0},
	{800000, 2, 0},
	{400000, 2, 1},
};
struct switch_lut mux_clk_g3d_busd_lut[] = {
	{800000, 0, 0},
	{533333, 2, 0},
};
struct switch_lut mux_clk_mif_ddrphy_clk2x_lut[] = {
	{1600000, 0, -1},
	{1333000, 1, -1},
};
struct switch_lut mux_clk_mif1_ddrphy_clk2x_lut[] = {
	{1600000, 0, -1},
	{1333000, 1, -1},
};
/*================================ SWPLL List =================================*/
struct vclk_switch switch_vdd_int[] = {
	{MUX_CLK_G3D_BUSD, MUX_CLKCMU_G3D_SWITCH, CLKCMU_G3D_SWITCH, GATE_CLKCMU_G3D_SWITCH, MUX_CLKCMU_G3D_SWITCH_USER, mux_clk_g3d_busd_lut, 2},
	{MUX_CLK_MIF_DDRPHY_CLK2X, MUX_CLKCMU_MIF_SWITCH, EMPTY_CAL_ID, CLKCMU_MIF_SWITCH, EMPTY_CAL_ID, mux_clk_mif_ddrphy_clk2x_lut, 2},
	{MUX_CLK_MIF1_DDRPHY_CLK2X, MUX_CLKCMU_MIF_SWITCH, EMPTY_CAL_ID, CLKCMU_MIF_SWITCH, EMPTY_CAL_ID, mux_clk_mif1_ddrphy_clk2x_lut, 2},
};
struct vclk_switch switch_vdd_cpu[] = {
	{MUX_CLK_CPUCL0_PLL, MUX_CLKCMU_CPUCL0_SWITCH, CLKCMU_CPUCL0_SWITCH, GATE_CLKCMU_CPUCL0_SWITCH, MUX_CLKCMU_CPUCL0_SWITCH_USER, mux_clk_cpucl0_pll_lut, 3},
	{MUX_CLK_CPUCL1_PLL, MUX_CLKCMU_CPUCL1_SWITCH, CLKCMU_CPUCL1_SWITCH, GATE_CLKCMU_CPUCL1_SWITCH, MUX_CLKCMU_CPUCL1_SWITCH_USER, mux_clk_cpucl1_pll_lut, 3},
};
struct vclk_switch switch_blk_aud[] = {
	{MUX_CLK_AUD_CPU, MUX_CLKCMU_AUD, CLKCMU_AUD, GATE_CLKCMU_AUD, MUX_CLKCMU_AUD_CPU_USER, mux_clk_aud_cpu_lut, 2},
};

/*================================ VCLK List =================================*/
unsigned int cmucal_vclk_size = 348;
struct vclk cmucal_vclk_list[] = {

/* DVFS VCLK*/
	CMUCAL_VCLK(VCLK_VDD_INT, cmucal_vclk_vdd_int_lut, cmucal_vclk_vdd_int, NULL, switch_vdd_int),
	CMUCAL_VCLK(VCLK_VDD_CPU, cmucal_vclk_vdd_cpu_lut, cmucal_vclk_vdd_cpu, NULL, switch_vdd_cpu),

/* SPECIAL VCLK*/
	CMUCAL_VCLK(VCLK_MUX_CLK_APM_I3C, cmucal_vclk_mux_clk_apm_i3c_lut, cmucal_vclk_mux_clk_apm_i3c, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_APM_BUS, cmucal_vclk_clkcmu_apm_bus_lut, cmucal_vclk_clkcmu_apm_bus, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_AUD_UAIF0, cmucal_vclk_mux_clk_aud_uaif0_lut, cmucal_vclk_mux_clk_aud_uaif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CHUB_TIMER_FCLK, cmucal_vclk_mux_clk_chub_timer_fclk_lut, cmucal_vclk_mux_clk_chub_timer_fclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLK_CMGP_ADC, cmucal_vclk_clk_cmgp_adc_lut, cmucal_vclk_clk_cmgp_adc, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CMU_CMUREF, cmucal_vclk_mux_clk_cmu_cmuref_lut, cmucal_vclk_mux_clk_cmu_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_HSI_RTC, cmucal_vclk_mux_clk_hsi_rtc_lut, cmucal_vclk_mux_clk_hsi_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_MIF_BUSP, cmucal_vclk_mux_clkcmu_mif_busp_lut, cmucal_vclk_mux_clkcmu_mif_busp, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_CPUCL1_DBG, cmucal_vclk_mux_clkcmu_cpucl1_dbg_lut, cmucal_vclk_mux_clkcmu_cpucl1_dbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_DMIC_IF_DIV2, cmucal_vclk_div_clk_chub_dmic_if_div2_lut, cmucal_vclk_div_clk_chub_dmic_if_div2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_DMIC, cmucal_vclk_div_clk_chub_dmic_lut, cmucal_vclk_div_clk_chub_dmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI_CMGP0, cmucal_vclk_div_clk_cmgp_usi_cmgp0_lut, cmucal_vclk_div_clk_cmgp_usi_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI_CMGP1, cmucal_vclk_div_clk_cmgp_usi_cmgp1_lut, cmucal_vclk_div_clk_cmgp_usi_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_AP2CP_SHARED0_PLL_CLK, cmucal_vclk_ap2cp_shared0_pll_clk_lut, cmucal_vclk_ap2cp_shared0_pll_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL0_CMUREF, cmucal_vclk_div_clk_cpucl0_cmuref_lut, cmucal_vclk_div_clk_cpucl0_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CLUSTER0_ATCLK, cmucal_vclk_div_clk_cluster0_atclk_lut, cmucal_vclk_div_clk_cluster0_atclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL1_CMUREF, cmucal_vclk_div_clk_cpucl1_cmuref_lut, cmucal_vclk_div_clk_cpucl1_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CLUSTER1_PERIPHCLK, cmucal_vclk_div_clk_cluster1_periphclk_lut, cmucal_vclk_div_clk_cluster1_periphclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_PERI_IP, cmucal_vclk_clkcmu_peri_ip_lut, cmucal_vclk_clkcmu_peri_ip, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_SPI_0, cmucal_vclk_div_clk_peri_spi_0_lut, cmucal_vclk_div_clk_peri_spi_0, NULL, NULL),

/* COMMON VCLK*/
	CMUCAL_VCLK(VCLK_BLK_AUD, cmucal_vclk_blk_aud_lut, cmucal_vclk_blk_aud, NULL, switch_blk_aud),
	CMUCAL_VCLK(VCLK_BLK_CMU, cmucal_vclk_blk_cmu_lut, cmucal_vclk_blk_cmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_APM, cmucal_vclk_blk_apm_lut, cmucal_vclk_blk_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CHUB, cmucal_vclk_blk_chub_lut, cmucal_vclk_blk_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CORE, cmucal_vclk_blk_core_lut, cmucal_vclk_blk_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL0, cmucal_vclk_blk_cpucl0_lut, cmucal_vclk_blk_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL1, cmucal_vclk_blk_cpucl1_lut, cmucal_vclk_blk_cpucl1, NULL, NULL),

/* GATE VCLK*/
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_APM, NULL, cmucal_vclk_ip_lhm_axi_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_APM, NULL, cmucal_vclk_ip_lhs_axi_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_AP, NULL, cmucal_vclk_ip_mailbox_apm_ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_CP, NULL, cmucal_vclk_ip_mailbox_apm_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_GNSS, NULL, cmucal_vclk_ip_mailbox_apm_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_WLBT, NULL, cmucal_vclk_ip_mailbox_apm_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_APM, NULL, cmucal_vclk_ip_sysreg_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_PMU_ALIVE, NULL, cmucal_vclk_ip_apbif_pmu_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_GPIO_ALIVE, NULL, cmucal_vclk_ip_apbif_gpio_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APM_CMU_APM, NULL, cmucal_vclk_ip_apm_cmu_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_TOP_RTC, NULL, cmucal_vclk_ip_apbif_top_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_INTMEM, NULL, cmucal_vclk_ip_intmem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_MODEM, NULL, cmucal_vclk_ip_lhm_axi_c_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_GNSS, NULL, cmucal_vclk_ip_lhm_axi_c_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CP, NULL, cmucal_vclk_ip_mailbox_ap_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CP_S, NULL, cmucal_vclk_ip_mailbox_ap_cp_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_GNSS, NULL, cmucal_vclk_ip_mailbox_ap_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_WLBT, NULL, cmucal_vclk_ip_mailbox_ap_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_WLBT_CHUB, NULL, cmucal_vclk_ip_mailbox_wlbt_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_WLBT_ABOX, NULL, cmucal_vclk_ip_mailbox_wlbt_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PMU_INTR_GEN, NULL, cmucal_vclk_ip_pmu_intr_gen, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_WLBT, NULL, cmucal_vclk_ip_lhm_axi_c_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_APM, NULL, cmucal_vclk_ip_xiu_dp_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_GNSS, NULL, cmucal_vclk_ip_mailbox_cp_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_WLBT, NULL, cmucal_vclk_ip_mailbox_cp_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPEEDY_APM, NULL, cmucal_vclk_ip_speedy_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GREBEINTEGRATION, NULL, cmucal_vclk_ip_grebeintegration, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_CHUB, NULL, cmucal_vclk_ip_lhm_axi_c_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_LP_CHUB, NULL, cmucal_vclk_ip_lhs_axi_lp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_CHUB, NULL, cmucal_vclk_ip_mailbox_cp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_GNSS_CHUB, NULL, cmucal_vclk_ip_mailbox_gnss_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_GNSS_WLBT, NULL, cmucal_vclk_ip_mailbox_gnss_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_CHUB, NULL, cmucal_vclk_ip_mailbox_apm_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CHUB, NULL, cmucal_vclk_ip_mailbox_ap_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_APM, NULL, cmucal_vclk_ip_d_tzpc_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_APM, NULL, cmucal_vclk_ip_wdt_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_RTC, NULL, cmucal_vclk_ip_apbif_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ROM_CRC32_HOST, NULL, cmucal_vclk_ip_rom_crc32_host, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C_APM_PMIC, NULL, cmucal_vclk_ip_i3c_apm_pmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AUD_CMU_AUD, NULL, cmucal_vclk_ip_aud_cmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_AUD, NULL, cmucal_vclk_ip_lhs_axi_d_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_AUD, NULL, cmucal_vclk_ip_ppmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_AUD, NULL, cmucal_vclk_ip_sysmmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERI_AXI_ASB, NULL, cmucal_vclk_ip_peri_axi_asb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI_US_32TO128, NULL, cmucal_vclk_ip_axi_us_32to128, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_AUD, NULL, cmucal_vclk_ip_wdt_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ABOX, NULL, cmucal_vclk_ip_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_AUD, NULL, cmucal_vclk_ip_ad_apb_sysmmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DFTMUX_AUD, NULL, cmucal_vclk_ip_dftmux_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_AUD_S, NULL, cmucal_vclk_ip_ad_apb_sysmmu_aud_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_AUD, NULL, cmucal_vclk_ip_lhm_axi_p_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_AUD, NULL, cmucal_vclk_ip_sysreg_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_AUD, NULL, cmucal_vclk_ip_gpio_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_AUD, NULL, cmucal_vclk_ip_d_tzpc_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CHUB_CMU_CHUB, NULL, cmucal_vclk_ip_chub_cmu_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_D_CHUB, NULL, cmucal_vclk_ip_baaw_d_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_C_CHUB, NULL, cmucal_vclk_ip_baaw_c_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CM4_CHUB, NULL, cmucal_vclk_ip_cm4_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_LP_CHUB, NULL, cmucal_vclk_ip_lhm_axi_lp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CHUB, NULL, cmucal_vclk_ip_lhm_axi_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_CHUB, NULL, cmucal_vclk_ip_lhs_axi_d_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_C_CHUB, NULL, cmucal_vclk_ip_lhs_axi_c_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PWM_CHUB, NULL, cmucal_vclk_ip_pwm_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_D_CHUB, NULL, cmucal_vclk_ip_sweeper_d_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_C_CHUB, NULL, cmucal_vclk_ip_sweeper_c_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CHUB, NULL, cmucal_vclk_ip_sysreg_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER_CHUB, NULL, cmucal_vclk_ip_timer_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_CHUB, NULL, cmucal_vclk_ip_wdt_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AHB_BUSMATRIX_CHUB, NULL, cmucal_vclk_ip_ahb_busmatrix_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CHUB, NULL, cmucal_vclk_ip_d_tzpc_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_AXI_LP_CHUB, NULL, cmucal_vclk_ip_bps_axi_lp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_AXI_P_CHUB, NULL, cmucal_vclk_ip_bps_axi_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XHB_P_CHUB, NULL, cmucal_vclk_ip_xhb_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XHB_LP_CHUB, NULL, cmucal_vclk_ip_xhb_lp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB0, NULL, cmucal_vclk_ip_dmic_ahb0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF, NULL, cmucal_vclk_ip_dmic_if, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_U_DMIC_CLK_SCAN_MUX, NULL, cmucal_vclk_ip_u_dmic_clk_scan_mux, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC0, NULL, cmucal_vclk_ip_hwacg_sys_dmic0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CMGP_CMU_CMGP, NULL, cmucal_vclk_ip_cmgp_cmu_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2CP, NULL, cmucal_vclk_ip_sysreg_cmgp2cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2GNSS, NULL, cmucal_vclk_ip_sysreg_cmgp2gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2WLBT, NULL, cmucal_vclk_ip_sysreg_cmgp2wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_CMGP, NULL, cmucal_vclk_ip_gpio_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP, NULL, cmucal_vclk_ip_sysreg_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2CHUB, NULL, cmucal_vclk_ip_sysreg_cmgp2chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2PMU_CHUB, NULL, cmucal_vclk_ip_sysreg_cmgp2pmu_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2PMU_AP, NULL, cmucal_vclk_ip_sysreg_cmgp2pmu_ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CMGP, NULL, cmucal_vclk_ip_d_tzpc_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP0, NULL, cmucal_vclk_ip_usi_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP1, NULL, cmucal_vclk_ip_usi_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADC_CMGP, NULL, cmucal_vclk_ip_adc_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP, NULL, cmucal_vclk_ip_otp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PDMA0, NULL, cmucal_vclk_ip_ad_apb_pdma0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SPDMA, NULL, cmucal_vclk_ip_ad_apb_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_GIC, NULL, cmucal_vclk_ip_ad_axi_gic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_GNSS, NULL, cmucal_vclk_ip_baaw_p_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_CHUB, NULL, cmucal_vclk_ip_baaw_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_MODEM, NULL, cmucal_vclk_ip_baaw_p_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_WLBT, NULL, cmucal_vclk_ip_baaw_p_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GIC, NULL, cmucal_vclk_ip_gic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_IS, NULL, cmucal_vclk_ip_lhm_axi_d0_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MFCMSCL, NULL, cmucal_vclk_ip_lhm_axi_d_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DPU, NULL, cmucal_vclk_ip_lhm_axi_d_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_HSI, NULL, cmucal_vclk_ip_lhm_axi_d_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_MODEM, NULL, cmucal_vclk_ip_lhm_axi_d0_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_MODEM, NULL, cmucal_vclk_ip_lhm_axi_d1_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_AUD, NULL, cmucal_vclk_ip_lhm_axi_d_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_APM, NULL, cmucal_vclk_ip_lhm_axi_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_CHUB, NULL, cmucal_vclk_ip_lhm_axi_d_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_CSSYS, NULL, cmucal_vclk_ip_lhm_axi_g_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_GNSS, NULL, cmucal_vclk_ip_lhm_axi_d_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_WLBT, NULL, cmucal_vclk_ip_lhm_axi_d_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_APM, NULL, cmucal_vclk_ip_lhs_axi_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CHUB, NULL, cmucal_vclk_ip_lhs_axi_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CPUCL0, NULL, cmucal_vclk_ip_lhs_axi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DPU, NULL, cmucal_vclk_ip_lhs_axi_p_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_HSI, NULL, cmucal_vclk_ip_lhs_axi_p_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_GNSS, NULL, cmucal_vclk_ip_lhs_axi_p_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_IS, NULL, cmucal_vclk_ip_lhs_axi_p_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MFCMSCL, NULL, cmucal_vclk_ip_lhs_axi_p_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MODEM, NULL, cmucal_vclk_ip_lhs_axi_p_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_PERI, NULL, cmucal_vclk_ip_lhs_axi_p_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_WLBT, NULL, cmucal_vclk_ip_lhs_axi_p_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA_CORE, NULL, cmucal_vclk_ip_pdma_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFR_APBIF_CMU_TOPC, NULL, cmucal_vclk_ip_sfr_apbif_cmu_topc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPDMA_CORE, NULL, cmucal_vclk_ip_spdma_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CORE, NULL, cmucal_vclk_ip_sysreg_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_CORE, NULL, cmucal_vclk_ip_trex_d_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_CORE, NULL, cmucal_vclk_ip_trex_p_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_CORE, NULL, cmucal_vclk_ip_xiu_d_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CORE, NULL, cmucal_vclk_ip_d_tzpc_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_G3D, NULL, cmucal_vclk_ip_lhm_axi_d_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CORE_CMU_CORE, NULL, cmucal_vclk_ip_core_cmu_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_G3D, NULL, cmucal_vclk_ip_lhs_axi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MIF_RT, NULL, cmucal_vclk_ip_lhs_axi_d0_mif_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MIF_NRT, NULL, cmucal_vclk_ip_lhs_axi_d0_mif_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MIF_RT, NULL, cmucal_vclk_ip_lhs_axi_d1_mif_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MIF_NRT, NULL, cmucal_vclk_ip_lhs_axi_d1_mif_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF0, NULL, cmucal_vclk_ip_lhs_axi_p_mif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF1, NULL, cmucal_vclk_ip_lhs_axi_p_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_IS, NULL, cmucal_vclk_ip_lhm_axi_d1_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_AUD, NULL, cmucal_vclk_ip_lhs_axi_p_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_SSS, NULL, cmucal_vclk_ip_ad_axi_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_AHB_SSS, NULL, cmucal_vclk_ip_adm_ahb_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_RTIC, NULL, cmucal_vclk_ip_rtic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SSS, NULL, cmucal_vclk_ip_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_CORE, NULL, cmucal_vclk_ip_gpio_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_MMC, NULL, cmucal_vclk_ip_ad_axi_mmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MMC_EMBD, NULL, cmucal_vclk_ip_mmc_embd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MIF_CPU, NULL, cmucal_vclk_ip_lhs_axi_d1_mif_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MIF_CPU, NULL, cmucal_vclk_ip_lhs_axi_d0_mif_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D_CPUCL0, NULL, cmucal_vclk_ip_lhm_ace_d_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D_CPUCL1, NULL, cmucal_vclk_ip_lhm_ace_d_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CPUCL1, NULL, cmucal_vclk_ip_lhs_axi_p_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MIF_CP, NULL, cmucal_vclk_ip_lhs_axi_d0_mif_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MIF_CP, NULL, cmucal_vclk_ip_lhs_axi_d1_mif_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_CCI_550, NULL, cmucal_vclk_ip_ad_apb_cci_550, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CCI_550, NULL, cmucal_vclk_ip_cci_550, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_ACE_CPUCL0, NULL, cmucal_vclk_ip_ppmu_ace_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_ACE_CPUCL1, NULL, cmucal_vclk_ip_ppmu_ace_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_G_DUMP_PC_CPUCL0, NULL, cmucal_vclk_ip_adm_apb_g_dump_pc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI2APB_CPUCL0, NULL, cmucal_vclk_ip_axi2apb_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADS_AHB_G_CSSYS_SSS, NULL, cmucal_vclk_ip_ads_ahb_g_cssys_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADS_APB_G_DUMP_PC_CPUCL0, NULL, cmucal_vclk_ip_ads_apb_g_dump_pc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADS_APB_G_CSSYS_CPUCL1, NULL, cmucal_vclk_ip_ads_apb_g_cssys_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CLUSTER0, NULL, cmucal_vclk_ip_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACE_D_CPUCL0, NULL, cmucal_vclk_ip_lhs_ace_d_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSSYS_DBG, NULL, cmucal_vclk_ip_cssys_dbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_CSSYS, NULL, cmucal_vclk_ip_lhs_axi_g_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SECJTAG, NULL, cmucal_vclk_ip_secjtag, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CPUCL0, NULL, cmucal_vclk_ip_sysreg_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CPUCL0, NULL, cmucal_vclk_ip_d_tzpc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_G_CSSYS_CORE, NULL, cmucal_vclk_ip_adm_apb_g_cssys_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CPUCL0, NULL, cmucal_vclk_ip_lhm_axi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0_CMU_CPUCL0, NULL, cmucal_vclk_ip_cpucl0_cmu_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL1_CMU_CPUCL1, NULL, cmucal_vclk_ip_cpucl1_cmu_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI2APB_CPUCL1, NULL, cmucal_vclk_ip_axi2apb_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CPUCL1, NULL, cmucal_vclk_ip_d_tzpc_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CPUCL1, NULL, cmucal_vclk_ip_lhm_axi_p_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CPUCL1, NULL, cmucal_vclk_ip_sysreg_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACE_D_CPUCL1, NULL, cmucal_vclk_ip_lhs_ace_d_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CLUSTER1, NULL, cmucal_vclk_ip_cluster1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_G_CSSYS_CPUCL1, NULL, cmucal_vclk_ip_adm_apb_g_cssys_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPU_CMU_DPU, NULL, cmucal_vclk_ip_dpu_cmu_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DECON0, NULL, cmucal_vclk_ip_ad_apb_decon0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DPU, NULL, cmucal_vclk_ip_lhm_axi_p_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DPU, NULL, cmucal_vclk_ip_lhs_axi_d_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DPU, NULL, cmucal_vclk_ip_sysreg_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPU, NULL, cmucal_vclk_ip_ppmu_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SMMU_DPU, NULL, cmucal_vclk_ip_smmu_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPU, NULL, cmucal_vclk_ip_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DPU, NULL, cmucal_vclk_ip_d_tzpc_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_G3D, NULL, cmucal_vclk_ip_sysreg_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G3D_CMU_G3D, NULL, cmucal_vclk_ip_g3d_cmu_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPU, NULL, cmucal_vclk_ip_gpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_G3D, NULL, cmucal_vclk_ip_d_tzpc_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_INT_G3D, NULL, cmucal_vclk_ip_lhs_axi_int_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_INT_G3D, NULL, cmucal_vclk_ip_lhm_axi_int_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_G3D, NULL, cmucal_vclk_ip_lhs_axi_d_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GRAY2BIN_G3D, NULL, cmucal_vclk_ip_gray2bin_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_G3D, NULL, cmucal_vclk_ip_lhm_axi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_HSI, NULL, cmucal_vclk_ip_gpio_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_HSI, NULL, cmucal_vclk_ip_lhm_axi_p_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_HSI, NULL, cmucal_vclk_ip_lhs_axi_d_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_HSI, NULL, cmucal_vclk_ip_ppmu_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_HSI, NULL, cmucal_vclk_ip_sysreg_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_HSI, NULL, cmucal_vclk_ip_xiu_d_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MMC_CARD, NULL, cmucal_vclk_ip_mmc_card, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_HSI, NULL, cmucal_vclk_ip_d_tzpc_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USB20DRD_TOP, NULL, cmucal_vclk_ip_usb20drd_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_US_64TO128_HSI, NULL, cmucal_vclk_ip_us_64to128_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HSI_CMU_HSI, NULL, cmucal_vclk_ip_hsi_cmu_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IS_CMU_IS, NULL, cmucal_vclk_ip_is_cmu_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_IS, NULL, cmucal_vclk_ip_sysreg_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_IS, NULL, cmucal_vclk_ip_lhm_axi_p_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_IS, NULL, cmucal_vclk_ip_lhs_axi_d0_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_IS, NULL, cmucal_vclk_ip_d_tzpc_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_IS, NULL, cmucal_vclk_ip_xiu_d0_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_IS0, NULL, cmucal_vclk_ip_ppmu_is0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_IS1, NULL, cmucal_vclk_ip_ppmu_is1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_IS, NULL, cmucal_vclk_ip_lhs_axi_d1_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_IS0, NULL, cmucal_vclk_ip_sysmmu_is0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_IS1, NULL, cmucal_vclk_ip_sysmmu_is1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSIS0, NULL, cmucal_vclk_ip_csis0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSIS1, NULL, cmucal_vclk_ip_csis1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSIS2, NULL, cmucal_vclk_ip_csis2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IS_TOP, NULL, cmucal_vclk_ip_is_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_IS, NULL, cmucal_vclk_ip_xiu_d1_is, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_CSIS0, NULL, cmucal_vclk_ip_ad_apb_csis0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_CSIS1, NULL, cmucal_vclk_ip_ad_apb_csis1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_CSIS2, NULL, cmucal_vclk_ip_ad_apb_csis2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_CSIS_DMA, NULL, cmucal_vclk_ip_ad_apb_csis_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_IPP, NULL, cmucal_vclk_ip_ad_apb_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_ITP, NULL, cmucal_vclk_ip_ad_apb_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_MCSC, NULL, cmucal_vclk_ip_ad_apb_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_VRA, NULL, cmucal_vclk_ip_ad_apb_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_IS0_NS, NULL, cmucal_vclk_ip_ad_apb_sysmmu_is0_ns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_IS0_S, NULL, cmucal_vclk_ip_ad_apb_sysmmu_is0_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_IS1_NS, NULL, cmucal_vclk_ip_ad_apb_sysmmu_is1_ns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_IS1_S, NULL, cmucal_vclk_ip_ad_apb_sysmmu_is1_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_VRA, NULL, cmucal_vclk_ip_ad_axi_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_GDC, NULL, cmucal_vclk_ip_ad_apb_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_GDC, NULL, cmucal_vclk_ip_ad_axi_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC, NULL, cmucal_vclk_ip_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFCMSCL, NULL, cmucal_vclk_ip_ppmu_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MFCMSCL, NULL, cmucal_vclk_ip_sysreg_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_M2M, NULL, cmucal_vclk_ip_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MFCMSCL, NULL, cmucal_vclk_ip_lhm_axi_p_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MFCMSCL, NULL, cmucal_vclk_ip_d_tzpc_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_MFC, NULL, cmucal_vclk_ip_as_apb_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_SYSMMU_NS_MFCMSCL, NULL, cmucal_vclk_ip_as_apb_sysmmu_ns_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_SYSMMU_S_MFCMSCL, NULL, cmucal_vclk_ip_as_apb_sysmmu_s_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_MFCMSCL, NULL, cmucal_vclk_ip_xiu_d_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFCMSCL, NULL, cmucal_vclk_ip_sysmmu_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_M2M, NULL, cmucal_vclk_ip_as_apb_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_AXI_M2M, NULL, cmucal_vclk_ip_as_axi_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_MFCMSCL, NULL, cmucal_vclk_ip_lhs_axi_d_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_MCSC, NULL, cmucal_vclk_ip_as_apb_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCSC, NULL, cmucal_vclk_ip_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_AXI_JPEG, NULL, cmucal_vclk_ip_as_axi_jpeg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_AXI_MCSC, NULL, cmucal_vclk_ip_as_axi_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_JPEG, NULL, cmucal_vclk_ip_as_apb_jpeg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_JPEG, NULL, cmucal_vclk_ip_jpeg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFCMSCL_CMU_MFCMSCL, NULL, cmucal_vclk_ip_mfcmscl_cmu_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI2APB_MFCMSCL, NULL, cmucal_vclk_ip_axi2apb_mfcmscl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DMC_CPU, NULL, cmucal_vclk_ip_ppmu_dmc_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MIF, NULL, cmucal_vclk_ip_sysreg_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIF_CMU_MIF, NULL, cmucal_vclk_ip_mif_cmu_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMC, NULL, cmucal_vclk_ip_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MIF, NULL, cmucal_vclk_ip_d_tzpc_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MIF, NULL, cmucal_vclk_ip_lhm_axi_p_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDR_PHY, NULL, cmucal_vclk_ip_ddr_phy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DDR_PHY, NULL, cmucal_vclk_ip_sfrapb_bridge_ddr_phy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC_PF, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc_pf, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC_SECURE, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc_secure, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF_CPU, NULL, cmucal_vclk_ip_lhm_axi_d_mif_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF_NRT, NULL, cmucal_vclk_ip_lhm_axi_d_mif_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF_RT, NULL, cmucal_vclk_ip_lhm_axi_d_mif_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF_CP, NULL, cmucal_vclk_ip_lhm_axi_d_mif_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDR_PHY1, NULL, cmucal_vclk_ip_ddr_phy1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMC1, NULL, cmucal_vclk_ip_dmc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MIF1, NULL, cmucal_vclk_ip_d_tzpc_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF1_CPU, NULL, cmucal_vclk_ip_lhm_axi_d_mif1_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF1_NRT, NULL, cmucal_vclk_ip_lhm_axi_d_mif1_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF1_RT, NULL, cmucal_vclk_ip_lhm_axi_d_mif1_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MIF1, NULL, cmucal_vclk_ip_lhm_axi_p_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DCM1_CPU, NULL, cmucal_vclk_ip_ppmu_dcm1_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DDR_PHY1, NULL, cmucal_vclk_ip_sfrapb_bridge_ddr_phy1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC1_PPMPU, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc1_ppmpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC1_SECURE, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc1_secure, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFR_APB_BRIDGE_DMC1, NULL, cmucal_vclk_ip_sfr_apb_bridge_dmc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFR_APB_BRIDGE_DMC1_PF, NULL, cmucal_vclk_ip_sfr_apb_bridge_dmc1_pf, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MIF1, NULL, cmucal_vclk_ip_sysreg_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIF1_CMU_MIF1, NULL, cmucal_vclk_ip_mif1_cmu_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MODEM_CMU_MODEM, NULL, cmucal_vclk_ip_modem_cmu_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_TMU, NULL, cmucal_vclk_ip_busif_tmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_TOP, NULL, cmucal_vclk_ip_otp_con_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERI, NULL, cmucal_vclk_ip_sysreg_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_0, NULL, cmucal_vclk_ip_wdt_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCT, NULL, cmucal_vclk_ip_mct, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PWM_MOTOR, NULL, cmucal_vclk_ip_pwm_motor, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERI, NULL, cmucal_vclk_ip_gpio_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_0, NULL, cmucal_vclk_ip_spi_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UART, NULL, cmucal_vclk_ip_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HSI2C_0, NULL, cmucal_vclk_ip_hsi2c_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERI_CMU_PERI, NULL, cmucal_vclk_ip_peri_cmu_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HSI2C_1, NULL, cmucal_vclk_ip_hsi2c_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HSI2C_2, NULL, cmucal_vclk_ip_hsi2c_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_PERI, NULL, cmucal_vclk_ip_lhm_axi_p_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERI, NULL, cmucal_vclk_ip_d_tzpc_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_3, NULL, cmucal_vclk_ip_i2c_3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_4, NULL, cmucal_vclk_ip_i2c_4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_1, NULL, cmucal_vclk_ip_wdt_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_0, NULL, cmucal_vclk_ip_i2c_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_1, NULL, cmucal_vclk_ip_i2c_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_2, NULL, cmucal_vclk_ip_i2c_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_5, NULL, cmucal_vclk_ip_i2c_5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_6, NULL, cmucal_vclk_ip_i2c_6, NULL, NULL),
};
