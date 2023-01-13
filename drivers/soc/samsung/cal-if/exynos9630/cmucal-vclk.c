#include "../cmucal.h"
#include "cmucal-node.h"
#include "cmucal-vclk.h"
#include "cmucal-vclklut.h"

/* DVFS VCLK -> Clock Node List */
enum clk_id cmucal_vclk_vdd_mif[] = {
	PLL_MIF,
};
enum clk_id cmucal_vclk_vdd_cam[] = {
	DIV_CLK_AUD_PLL,
	DIV_CLK_AUD_BUS,
	PLL_AUD,
};
enum clk_id cmucal_vclk_vdd_cpucl0[] = {
	DIV_CLK_CLUSTER0_ACLK,
	PLL_CPUCL0,
};
enum clk_id cmucal_vclk_vddi[] = {
	CLKCMU_MFC_WFD,
	MUX_CLKCMU_MFC_WFD,
	PLL_G3D,
	CLKCMU_MFC_MFC,
	MUX_CLKCMU_MFC_MFC,
	CLKCMU_CORE_BUS,
	MUX_CLKCMU_CORE_BUS,
	CLKCMU_G2D_G2D,
	MUX_CLKCMU_G2D_G2D,
	CLKCMU_DSP_BUS,
	MUX_CLKCMU_DSP_BUS,
	CLKCMU_CPUCL0_SWITCH,
	MUX_CLKCMU_CPUCL0_SWITCH,
	CLKCMU_IPP_BUS,
	MUX_CLKCMU_IPP_BUS,
	CLKCMU_ITP_BUS,
	MUX_CLKCMU_ITP_BUS,
	CLKCMU_G2D_MSCL,
	MUX_CLKCMU_G2D_MSCL,
	CLKCMU_PERI_BUS,
	MUX_CLKCMU_PERI_BUS,
	CLKCMU_NPU_BUS,
	MUX_CLKCMU_NPU_BUS,
	CLKCMU_HSI_BUS,
	MUX_CLKCMU_HSI_BUS,
	CLKCMU_DPU_BUS,
	MUX_CLKCMU_DPU_BUS,
	CLKCMU_CPUCL1_SWITCH,
	MUX_CLKCMU_CPUCL1_SWITCH,
	CLKCMU_TNR_BUS,
	MUX_CLKCMU_TNR_BUS,
	CLKCMU_CORE_CCI,
	MUX_CLKCMU_CORE_CCI,
	CLKCMU_CORE_G3D,
	MUX_CLKCMU_CORE_G3D,
	CLKCMU_CSIS_BUS,
	MUX_CLKCMU_CSIS_BUS,
	CLKCMU_DNC_BUS,
	MUX_CLKCMU_DNC_BUS,
	CLKCMU_MCSC_BUS,
	MUX_CLKCMU_MCSC_BUS,
	CLKCMU_DNC_BUSM,
	MUX_CLKCMU_DNC_BUSM,
	CLKCMU_PERI_MMC_CARD,
	CLKCMU_MCSC_GDC,
	MUX_CLKCMU_MCSC_GDC,
	CLKCMU_SSP_BUS,
	CLKCMU_USB_USB31DRD,
	CLKCMU_DNS_BUS,
	MUX_CLKCMU_DNS_BUS,
	CLKCMU_G3D_SWITCH,
	MUX_CLKCMU_G3D_SWITCH,
	CLKCMU_HSI_MMC_EMBD,
	CLKCMU_VRA_BUS,
	MUX_CLKCMU_VRA_BUS,
	CLKCMU_CORE_SSS,
	CLKCMU_BUSC_BUS,
	MUX_CLKCMU_BUSC_BUS,
	CLKCMU_VRA_CLAHE,
	MUX_CLKCMU_VRA_CLAHE,
	CLKCMU_CPUCL0_DBG_BUS,
	MUX_CLKCMU_CPUCL0_DBG_BUS,
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
enum clk_id cmucal_vclk_mux_busc_cmuref[] = {
	MUX_BUSC_CMUREF,
};
enum clk_id cmucal_vclk_clkcmu_cmu_boost[] = {
	CLKCMU_CMU_BOOST,
	MUX_CLKCMU_CMU_BOOST,
};
enum clk_id cmucal_vclk_mux_clk_chub_timer_fclk[] = {
	MUX_CLK_CHUB_TIMER_FCLK,
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
enum clk_id cmucal_vclk_mux_mif_cmuref[] = {
	MUX_MIF_CMUREF,
};
enum clk_id cmucal_vclk_mux_clkcmu_peri_mmc_card[] = {
	MUX_CLKCMU_PERI_MMC_CARD,
};
enum clk_id cmucal_vclk_mux_clkcmu_usb_usbdp_debug[] = {
	MUX_CLKCMU_USB_USBDP_DEBUG,
};
enum clk_id cmucal_vclk_clkcmu_usb_dpgtc[] = {
	CLKCMU_USB_DPGTC,
	MUX_CLKCMU_USB_DPGTC,
};
enum clk_id cmucal_vclk_div_clk_apm_dbgcore_uart[] = {
	DIV_CLK_APM_DBGCORE_UART,
	MUX_CLK_APM_DBGCORE_UART,
};
enum clk_id cmucal_vclk_div_clk_aud_cpu_pclkdbg[] = {
	DIV_CLK_AUD_CPU_PCLKDBG,
	DIV_CLK_AUD_UAIF0,
	MUX_CLK_AUD_UAIF0,
	DIV_CLK_AUD_UAIF1,
	MUX_CLK_AUD_UAIF1,
	DIV_CLK_AUD_UAIF2,
	MUX_CLK_AUD_UAIF2,
	DIV_CLK_AUD_UAIF3,
	MUX_CLK_AUD_UAIF3,
	DIV_CLK_AUD_CNT,
	DIV_CLK_AUD_UAIF4,
	MUX_CLK_AUD_UAIF4,
	DIV_CLK_AUD_DSIF,
	MUX_CLK_AUD_DSIF,
	DIV_CLK_AUD_FM,
	MUX_CLK_AUD_FM,
	DIV_CLK_AUD_FM_SPDY,
	DIV_CLK_AUD_UAIF5,
	MUX_CLK_AUD_UAIF5,
	DIV_CLK_AUD_UAIF6,
	MUX_CLK_AUD_UAIF6,
	DIV_CLK_AUD_SCLK,
};
enum clk_id cmucal_vclk_div_clk_aud_mclk[] = {
	DIV_CLK_AUD_MCLK,
};
enum clk_id cmucal_vclk_div_clk_chub_usi0[] = {
	DIV_CLK_CHUB_USI0,
	MUX_CLK_CHUB_USI0,
};
enum clk_id cmucal_vclk_clkcmu_chub_bus[] = {
	CLKCMU_CHUB_BUS,
	MUX_CLKCMU_CHUB_BUS,
};
enum clk_id cmucal_vclk_div_clk_chub_usi1[] = {
	DIV_CLK_CHUB_USI1,
	MUX_CLK_CHUB_USI1,
};
enum clk_id cmucal_vclk_div_clk_chub_usi2[] = {
	DIV_CLK_CHUB_USI2,
	MUX_CLK_CHUB_USI2,
};
enum clk_id cmucal_vclk_div_clk_chub_i2c[] = {
	DIV_CLK_CHUB_I2C,
	MUX_CLK_CHUB_I2C,
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
enum clk_id cmucal_vclk_div_clk_i2c_cmgp[] = {
	DIV_CLK_I2C_CMGP,
	MUX_CLK_I2C_CMGP,
};
enum clk_id cmucal_vclk_div_clk_i3c_cmgp[] = {
	DIV_CLK_I3C_CMGP,
	MUX_CLK_I3C_CMGP,
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
enum clk_id cmucal_vclk_div_clk_dsp1_busp[] = {
	DIV_CLK_DSP1_BUSP,
};
enum clk_id cmucal_vclk_div_clk_npu1_busp[] = {
	DIV_CLK_NPU1_BUSP,
};
enum clk_id cmucal_vclk_div_clk_peri_usi00_usi[] = {
	DIV_CLK_PERI_USI00_USI,
	MUX_CLKCMU_PERI_USI00_USI_USER,
};
enum clk_id cmucal_vclk_clkcmu_peri_ip[] = {
	CLKCMU_PERI_IP,
	MUX_CLKCMU_PERI_IP,
};
enum clk_id cmucal_vclk_div_clk_peri_usi01_usi[] = {
	DIV_CLK_PERI_USI01_USI,
	MUX_CLKCMU_PERI_USI01_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peri_usi02_usi[] = {
	DIV_CLK_PERI_USI02_USI,
	MUX_CLKCMU_PERI_USI02_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peri_usi03_usi[] = {
	DIV_CLK_PERI_USI03_USI,
	MUX_CLKCMU_PERI_USI03_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peri_usi04_usi[] = {
	DIV_CLK_PERI_USI04_USI,
	MUX_CLKCMU_PERI_USI04_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peri_usi05_usi[] = {
	DIV_CLK_PERI_USI05_USI,
	MUX_CLKCMU_PERI_USI05_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peri_uart_dbg[] = {
	DIV_CLK_PERI_UART_DBG,
};
enum clk_id cmucal_vclk_div_clk_peri_spi_ois[] = {
	DIV_CLK_PERI_SPI_OIS,
	MUX_CLKCMU_PERI_SPI_OIS_USER,
};
enum clk_id cmucal_vclk_mux_clkcmu_ap2gnss[] = {
	MUX_CLKCMU_AP2GNSS,
};
/* COMMON VCLK -> Clock Node List */
enum clk_id cmucal_vclk_blk_cmu[] = {
	MUX_CLKCMU_SSP_BUS,
	CLKCMU_USB_BUS,
	MUX_CLKCMU_USB_BUS,
	PLL_SHARED1_DIV3,
	MUX_CLKCMU_HSI_MMC_EMBD,
	CLKCMU_MIF_BUSP,
	MUX_CLKCMU_MIF_BUSP,
	MUX_CLKCMU_USB_USB31DRD,
	CLKCMU_HSI_UFS_EMBD,
	MUX_CLKCMU_HSI_UFS_EMBD,
	MUX_CLKCMU_CORE_SSS,
	PLL_SHARED1_DIV4,
	PLL_SHARED1_DIV2,
	PLL_SHARED1,
	CLKCMU_CSIS_OIS_MCU,
	MUX_CLKCMU_CSIS_OIS_MCU,
	PLL_SHARED0_DIV3,
	PLL_SHARED0_DIV4,
	PLL_SHARED0_DIV2,
	PLL_SHARED0,
	PLL_MMC,
};
enum clk_id cmucal_vclk_blk_cpucl1[] = {
	MUX_CLK_CPUCL1_PLL,
	PLL_CPUCL1,
};
enum clk_id cmucal_vclk_blk_s2d[] = {
	MUX_CLK_S2D_CORE,
	CLKCMU_MIF_DDRPHY2X_S2D,
	PLL_MIF_S2D,
};
enum clk_id cmucal_vclk_blk_apm[] = {
	DIV_CLK_APM_BUS,
	MUX_CLK_APM_BUS,
	CLKCMU_VTS_BUS,
	MUX_CLKCMU_VTS_BUS,
};
enum clk_id cmucal_vclk_blk_chub[] = {
	DIV_CLK_CHUB_BUS,
	MUX_CLK_CHUB_BUS,
};
enum clk_id cmucal_vclk_blk_cmgp[] = {
	DIV_CLK_CMGP_BUS,
	MUX_CLK_CMGP_BUS,
};
enum clk_id cmucal_vclk_blk_core[] = {
	MUX_CLK_CORE_GIC,
	DIV_CLK_CORE_BUSP,
};
enum clk_id cmucal_vclk_blk_cpucl0[] = {
	DIV_CLK_CLUSTER0_PCLKDBG,
	DIV_CLK_CPUCL0_PCLK,
	MUX_CLK_CPUCL0_PLL,
	DIV_CLK_CPUCL0_DBG_PCLKDBG,
};
enum clk_id cmucal_vclk_blk_g3d[] = {
	DIV_CLK_G3D_BUSP,
	MUX_CLK_G3D_BUSD,
};
enum clk_id cmucal_vclk_blk_vts[] = {
	DIV_CLK_VTS_BUS,
	MUX_CLK_VTS_BUS,
	DIV_CLK_VTS_DMIC_IF_DIV2,
	DIV_CLK_VTS_DMIC_IF_PAD,
	DIV_CLK_VTS_DMIC_IF,
};
enum clk_id cmucal_vclk_blk_aud[] = {
	DIV_CLK_AUD_AUDIF,
	DIV_CLK_AUD_CPU_ACLK,
	DIV_CLK_AUD_BUSP,
};
enum clk_id cmucal_vclk_blk_busc[] = {
	DIV_CLK_BUSC_BUSP,
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
enum clk_id cmucal_vclk_blk_mfc[] = {
	DIV_CLK_MFC_BUSP,
};
enum clk_id cmucal_vclk_blk_npu[] = {
	DIV_CLK_NPU_BUSP,
};
enum clk_id cmucal_vclk_blk_peri[] = {
	DIV_CLK_PERI_USI_I2C,
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
enum clk_id cmucal_vclk_ip_dbgcore_uart[] = {
	GOUT_BLK_APM_UID_DBGCORE_UART_IPCLKPORT_PCLK,
	CLK_BLK_APM_UID_DBGCORE_UART_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_rom_crc32_host[] = {
	GOUT_BLK_APM_UID_ROM_CRC32_HOST_IPCLKPORT_PCLK,
	GOUT_BLK_APM_UID_ROM_CRC32_HOST_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_gnss[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_modem[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_chub[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_wlbt[] = {
	GOUT_BLK_APM_UID_LHM_AXI_C_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_lp_chub[] = {
	GOUT_BLK_APM_UID_LHS_AXI_LP_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_wlbt_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_WLBT_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_wlbt_abox[] = {
	GOUT_BLK_APM_UID_MAILBOX_WLBT_ABOX_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_gnss_wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_GNSS_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_gnss_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_GNSS_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_gnss[] = {
	GOUT_BLK_APM_UID_MAILBOX_CP_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_wlbt[] = {
	GOUT_BLK_APM_UID_MAILBOX_CP_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_CP_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_cp_s[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_CP_S_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_cp[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_cp[] = {
	GOUT_BLK_APM_UID_MAILBOX_APM_CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_vts_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_VTS_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_chub[] = {
	GOUT_BLK_APM_UID_MAILBOX_AP_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i3c_apm_pmic[] = {
	GOUT_BLK_APM_UID_I3C_APM_PMIC_IPCLKPORT_I_PCLK,
	GOUT_BLK_APM_UID_I3C_APM_PMIC_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_rfmspeedy_apm[] = {
	GOUT_BLK_APM_UID_RFMSPEEDY_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_vgpio2ap[] = {
	GOUT_BLK_APM_UID_SYSREG_VGPIO2AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_vgpio2apm[] = {
	GOUT_BLK_APM_UID_SYSREG_VGPIO2APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_vgpio2pmu[] = {
	GOUT_BLK_APM_UID_SYSREG_VGPIO2PMU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_chub_rtc[] = {
	GOUT_BLK_APM_UID_APBIF_CHUB_RTC_IPCLKPORT_PCLK,
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
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF2,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_DAP,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK_IRQ,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK_IRQ,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_CNT,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF4,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_ASB,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_CA32,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_DSIF,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_OSC_SPDY,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_SPDY,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF5,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF6,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_SCLK,
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
enum clk_id cmucal_vclk_ip_sysmmu_aud[] = {
	GOUT_BLK_AUD_UID_SYSMMU_AUD_IPCLKPORT_CLK_S1,
	GOUT_BLK_AUD_UID_SYSMMU_AUD_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_aud[] = {
	GOUT_BLK_AUD_UID_AD_APB_SYSMMU_AUD_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_aud_s[] = {
	GOUT_BLK_AUD_UID_AD_APB_SYSMMU_AUD_S_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_vgen_lite_aud[] = {
	GOUT_BLK_AUD_UID_VGEN_LITE_AUD_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_aud_ns1[] = {
	GOUT_BLK_AUD_UID_AD_APB_SYSMMU_AUD_NS1_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_dftmux_aud[] = {
	GOUT_BLK_AUD_UID_DFTMUX_AUD_IPCLKPORT_AUD_CODEC_MCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_aud0[] = {
	GOUT_BLK_AUD_UID_MAILBOX_AUD0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_aud1[] = {
	GOUT_BLK_AUD_UID_MAILBOX_AUD1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_aud[] = {
	GOUT_BLK_AUD_UID_D_TZPC_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busc_cmu_busc[] = {
	CLK_BLK_BUSC_UID_BUSC_CMU_BUSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_pdma[] = {
	GOUT_BLK_BUSC_UID_AD_APB_PDMA_IPCLKPORT_PCLKM,
	GOUT_BLK_BUSC_UID_AD_APB_PDMA_IPCLKPORT_PCLKS,
};
enum clk_id cmucal_vclk_ip_xiu_p_busc[] = {
	GOUT_BLK_BUSC_UID_XIU_P_BUSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_busc[] = {
	GOUT_BLK_BUSC_UID_XIU_D_BUSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d0_dnc[] = {
	GOUT_BLK_BUSC_UID_LHM_ACEL_D0_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d0_g2d[] = {
	GOUT_BLK_BUSC_UID_LHM_ACEL_D0_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d1_dnc[] = {
	GOUT_BLK_BUSC_UID_LHM_ACEL_D1_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d1_g2d[] = {
	GOUT_BLK_BUSC_UID_LHM_ACEL_D1_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d2_dnc[] = {
	GOUT_BLK_BUSC_UID_LHM_ACEL_D2_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_peri[] = {
	GOUT_BLK_BUSC_UID_LHM_ACEL_D_PERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_usb[] = {
	GOUT_BLK_BUSC_UID_LHM_ACEL_D_USB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_mfc[] = {
	GOUT_BLK_BUSC_UID_LHM_AXI_D0_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_apm[] = {
	GOUT_BLK_BUSC_UID_LHM_AXI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_chub[] = {
	GOUT_BLK_BUSC_UID_LHM_AXI_D_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_ssp[] = {
	GOUT_BLK_BUSC_UID_LHM_AXI_D_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_tnr[] = {
	GOUT_BLK_BUSC_UID_LHM_AXI_D0_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_vts[] = {
	GOUT_BLK_BUSC_UID_LHM_AXI_D_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pdma_busc[] = {
	GOUT_BLK_BUSC_UID_PDMA_BUSC_IPCLKPORT_ACLK_PDMA0,
};
enum clk_id cmucal_vclk_ip_spdma_busc[] = {
	GOUT_BLK_BUSC_UID_SPDMA_BUSC_IPCLKPORT_ACLK_PDMA1,
};
enum clk_id cmucal_vclk_ip_sysmmu_axi_d_busc[] = {
	GOUT_BLK_BUSC_UID_SYSMMU_AXI_D_BUSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_busc[] = {
	GOUT_BLK_BUSC_UID_SYSREG_BUSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d_busc[] = {
	GOUT_BLK_BUSC_UID_TREX_D_BUSC_IPCLKPORT_ACLK,
	GOUT_BLK_BUSC_UID_TREX_D_BUSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_pdma[] = {
	GOUT_BLK_BUSC_UID_VGEN_PDMA_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_mfc[] = {
	GOUT_BLK_BUSC_UID_LHM_AXI_D1_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_busc[] = {
	GOUT_BLK_BUSC_UID_LHM_AXI_P_BUSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_busc[] = {
	GOUT_BLK_BUSC_UID_D_TZPC_BUSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_tnr[] = {
	GOUT_BLK_BUSC_UID_LHM_AXI_D1_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_apbif_gpio_chub[] = {
	GOUT_BLK_CHUB_UID_APBIF_GPIO_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_chub_combine_wakeup_src[] = {
	GOUT_BLK_CHUB_UID_APBIF_CHUB_COMBINE_WAKEUP_SRC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_chub_cmu_chub[] = {
	CLK_BLK_CHUB_UID_CHUB_CMU_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_chub[] = {
	GOUT_BLK_CHUB_UID_XIU_DP_CHUB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_baaw_d_chub[] = {
	GOUT_BLK_CHUB_UID_BAAW_D_CHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_apm_chub[] = {
	GOUT_BLK_CHUB_UID_BAAW_P_APM_CHUB_IPCLKPORT_I_PCLK,
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
enum clk_id cmucal_vclk_ip_pdma_chub[] = {
	GOUT_BLK_CHUB_UID_PDMA_CHUB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_pwm_chub[] = {
	GOUT_BLK_CHUB_UID_PWM_CHUB_IPCLKPORT_I_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_sweeper_d_chub[] = {
	GOUT_BLK_CHUB_UID_SWEEPER_D_CHUB_IPCLKPORT_ACLK,
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
enum clk_id cmucal_vclk_ip_bps_axi_lp_chub[] = {
	GOUT_BLK_CHUB_UID_BPS_AXI_LP_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bps_axi_p_chub[] = {
	GOUT_BLK_CHUB_UID_BPS_AXI_P_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_chub[] = {
	GOUT_BLK_CHUB_UID_D_TZPC_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_chub0[] = {
	GOUT_BLK_CHUB_UID_I2C_CHUB0_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_I2C_CHUB0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_chub1[] = {
	GOUT_BLK_CHUB_UID_I2C_CHUB1_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_I2C_CHUB1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_chub2[] = {
	GOUT_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_chub0[] = {
	GOUT_BLK_CHUB_UID_USI_CHUB0_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_USI_CHUB0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_chub1[] = {
	GOUT_BLK_CHUB_UID_USI_CHUB1_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_USI_CHUB1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_chub2[] = {
	GOUT_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_combine_chub2ap[] = {
	GOUT_BLK_CHUB_UID_SYSREG_COMBINE_CHUB2AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_combine_chub2apm[] = {
	GOUT_BLK_CHUB_UID_SYSREG_COMBINE_CHUB2APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_combine_chub2cp[] = {
	GOUT_BLK_CHUB_UID_SYSREG_COMBINE_CHUB2CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_combine_chub2gnss[] = {
	GOUT_BLK_CHUB_UID_SYSREG_COMBINE_CHUB2GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_combine_chub2wlbt[] = {
	GOUT_BLK_CHUB_UID_SYSREG_COMBINE_CHUB2WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cmgp_cmu_cmgp[] = {
	CLK_BLK_CMGP_UID_CMGP_CMU_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_adc_cmgp[] = {
	GOUT_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_PCLK_S0,
	GOUT_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_PCLK_S1,
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
enum clk_id cmucal_vclk_ip_sysreg_cmgp2gnss[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2wlbt[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2cp[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2chub[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i3c_cmgp[] = {
	GOUT_BLK_CMGP_UID_I3C_CMGP_IPCLKPORT_I_SCLK,
	GOUT_BLK_CMGP_UID_I3C_CMGP_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_core_cmu_core[] = {
	CLK_BLK_CORE_UID_CORE_CMU_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_core[] = {
	GOUT_BLK_CORE_UID_SYSREG_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sirex[] = {
	GOUT_BLK_CORE_UID_SIREX_IPCLKPORT_I_ACLK,
	GOUT_BLK_CORE_UID_SIREX_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dns[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_DNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_trex_p_core[] = {
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_ACLK_P_CORE,
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_PCLK_P_CORE,
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_CCLK_P_CORE,
};
enum clk_id cmucal_vclk_ip_ppmu_ace_cpucl1[] = {
	GOUT_BLK_CORE_UID_PPMU_ACE_CPUCL1_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_PPMU_ACE_CPUCL1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_dit[] = {
	GOUT_BLK_CORE_UID_DIT_IPCLKPORT_ICLKL2A,
};
enum clk_id cmucal_vclk_ip_trex_d_nrt[] = {
	GOUT_BLK_CORE_UID_TREX_D_NRT_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_TREX_D_NRT_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_g3d[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_cpucl0[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_csis[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dpu[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_hsi[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D_HSI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_trex_d_core[] = {
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_GCLK,
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_CCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_apm[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_core[] = {
	GOUT_BLK_CORE_UID_D_TZPC_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_wlbt[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_csis[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D0_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_csis[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D1_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_wlbt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_ipp[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_IPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_modem[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D0_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_modem[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D1_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_mcsc[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D0_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_mcsc[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D1_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_gnss[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mif_nrt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D0_MIF_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_ace_cpucl0[] = {
	GOUT_BLK_CORE_UID_PPMU_ACE_CPUCL0_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_PPMU_ACE_CPUCL0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_cci_550[] = {
	GOUT_BLK_CORE_UID_AD_APB_CCI_550_IPCLKPORT_PCLKS,
	GOUT_BLK_CORE_UID_AD_APB_CCI_550_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_dit[] = {
	GOUT_BLK_CORE_UID_AD_APB_DIT_IPCLKPORT_PCLKS,
	GOUT_BLK_CORE_UID_AD_APB_DIT_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_axi_gic[] = {
	GOUT_BLK_CORE_UID_AD_AXI_GIC_IPCLKPORT_ACLKS,
	GOUT_BLK_CORE_UID_AD_AXI_GIC_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_usb[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_USB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_modem[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif0[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MIF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif1[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MIF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mfc[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_gnss[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_chub[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gic[] = {
	GOUT_BLK_CORE_UID_GIC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_aud[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mif_cpu[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D0_MIF_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mif_cp[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D1_MIF_CP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mif_cpu[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D1_MIF_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mif_nrt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D1_MIF_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_aud[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dnc[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dpu[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_g2d[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_hsi[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_HSI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_ipp[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_IPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_gnss[] = {
	GOUT_BLK_CORE_UID_BAAW_P_GNSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_modem[] = {
	GOUT_BLK_CORE_UID_BAAW_P_MODEM_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_chub[] = {
	GOUT_BLK_CORE_UID_BAAW_P_CHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_wlbt[] = {
	GOUT_BLK_CORE_UID_BAAW_P_WLBT_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_sfr_apbif_cmu_topc[] = {
	GOUT_BLK_CORE_UID_SFR_APBIF_CMU_TOPC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cci_550[] = {
	GOUT_BLK_CORE_UID_CCI_550_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dns[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_DNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mcsc[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_ssp[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_tnr[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_vts[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_sirex[] = {
	GOUT_BLK_CORE_UID_VGEN_LITE_SIREX_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_itp[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_ITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_npu0[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_peri[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_PERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_sss[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_SSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_acel_d_dit[] = {
	GOUT_BLK_CORE_UID_SYSMMU_ACEL_D_DIT_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_baaw_p_vts[] = {
	GOUT_BLK_CORE_UID_BAAW_P_VTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_core[] = {
	GOUT_BLK_CORE_UID_XIU_D1_CORE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d0_cluster0[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D0_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d1_cluster0[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D1_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_sss[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D_SSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_cssys[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_G_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mif_cp[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D0_MIF_CP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sss[] = {
	GOUT_BLK_CORE_UID_SSS_IPCLKPORT_I_PCLK,
	GOUT_BLK_CORE_UID_SSS_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_adm_ahb_sss[] = {
	GOUT_BLK_CORE_UID_ADM_AHB_SSS_IPCLKPORT_HCLKM,
};
enum clk_id cmucal_vclk_ip_puf[] = {
	GOUT_BLK_CORE_UID_PUF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_puf[] = {
	GOUT_BLK_CORE_UID_AD_APB_PUF_IPCLKPORT_PCLKS,
	GOUT_BLK_CORE_UID_AD_APB_PUF_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_rtic[] = {
	GOUT_BLK_CORE_UID_RTIC_IPCLKPORT_I_ACLK,
	GOUT_BLK_CORE_UID_RTIC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_dnc[] = {
	GOUT_BLK_CORE_UID_BAAW_P_DNC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_npu1[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_NPU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mif_rt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D0_MIF_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mif_rt[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_D1_MIF_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_axi_d_core1[] = {
	GOUT_BLK_CORE_UID_SYSMMU_AXI_D_CORE1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d2_modem[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D2_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_vra[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_VRA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_busc[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_BUSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mcw[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MCW_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_vra[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_VRA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d0_g3d[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D0_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d1_g3d[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D1_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_baaw_d_sss[] = {
	GOUT_BLK_CORE_UID_BAAW_D_SSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_bdu[] = {
	GOUT_BLK_CORE_UID_BDU_IPCLKPORT_I_CLK,
	GOUT_BLK_CORE_UID_BDU_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_g_bdu[] = {
	GOUT_BLK_CORE_UID_XIU_G_BDU_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppc_debug[] = {
	GOUT_BLK_CORE_UID_PPC_DEBUG_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_dbg_int_g_bdu[] = {
	GOUT_BLK_CORE_UID_LHS_DBG_INT_G_BDU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_dbg_int_g_bdu[] = {
	GOUT_BLK_CORE_UID_LHM_DBG_INT_G_BDU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_SYSREG_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_hpmcpucl0[] = {
	GOUT_BLK_CPUCL0_UID_BUSIF_HPMCPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cssys[] = {
	GOUT_BLK_CPUCL0_UID_CSSYS_IPCLKPORT_PCLKDBG,
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
enum clk_id cmucal_vclk_ip_adm_apb_g_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_ADM_APB_G_CLUSTER0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_cpucl0_cmu_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_CPUCL0_CMU_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cluster0[] = {
	CLK_BLK_CPUCL0_UID_CLUSTER0_IPCLKPORT_PERIPHCLK,
	CLK_BLK_CPUCL0_UID_CLUSTER0_IPCLKPORT_SCLK,
	CLK_BLK_CPUCL0_UID_CLUSTER0_IPCLKPORT_PCLK,
	CLK_BLK_CPUCL0_UID_CLUSTER0_IPCLKPORT_ATCLK,
};
enum clk_id cmucal_vclk_ip_lhs_ace_d1_cluster0[] = {
	GOUT_BLK_CPUCL0_UID_LHS_ACE_D1_CLUSTER0_IPCLKPORT_I_CLK,
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
enum clk_id cmucal_vclk_ip_lhs_axi_g_cssys[] = {
	GOUT_BLK_CPUCL0_UID_LHS_AXI_G_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_hpm_cpucl0_0[] = {
	CLK_BLK_CPUCL0_UID_HPM_CPUCL0_0_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_apb_async_p_cssys_0[] = {
	GOUT_BLK_CPUCL0_UID_APB_ASYNC_P_CSSYS_0_IPCLKPORT_PCLKM,
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
enum clk_id cmucal_vclk_ip_cpucl1_cmu_cpucl1[] = {
	CLK_BLK_CPUCL1_UID_CPUCL1_CMU_CPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl1[] = {
	CLK_BLK_CPUCL1_UID_CPUCL1_IPCLKPORT_SCLK,
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
	GOUT_BLK_CSIS_UID_OIS_MCU_TOP_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_ad_axi_ois_mcu_top[] = {
	GOUT_BLK_CSIS_UID_AD_AXI_OIS_MCU_TOP_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_csisperi[] = {
	GOUT_BLK_CSIS_UID_LHS_AXI_P_CSISPERI_IPCLKPORT_I_CLK,
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
enum clk_id cmucal_vclk_ip_lhm_ast_d_npu0_unit0_done[] = {
	GOUT_BLK_DNC_UID_LHM_AST_D_NPU0_UNIT0_DONE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npu0_unit1_done[] = {
	GOUT_BLK_DNC_UID_LHM_AST_D_NPU0_UNIT1_DONE_IPCLKPORT_I_CLK,
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
enum clk_id cmucal_vclk_ip_sysreg_dnc[] = {
	GOUT_BLK_DNC_UID_SYSREG_DNC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_adm_dap_dnc[] = {
	GOUT_BLK_DNC_UID_ADM_DAP_DNC_IPCLKPORT_DAPCLKM,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npu0_unit0_setreg[] = {
	GOUT_BLK_DNC_UID_LHS_AST_D_NPU0_UNIT0_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npu0_unit1_setreg[] = {
	GOUT_BLK_DNC_UID_LHS_AST_D_NPU0_UNIT1_SETREG_IPCLKPORT_I_CLK,
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
enum clk_id cmucal_vclk_ip_lhs_axi_p_dspc_200[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_P_DSPC_200_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_dspc_800[] = {
	GOUT_BLK_DNC_UID_LHM_AXI_P_DSPC_800_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dspcnpuc_200[] = {
	GOUT_BLK_DNC_UID_LHM_AXI_D_DSPCNPUC_200_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dspcnpuc_800[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_D_DSPCNPUC_800_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dncdsp0_dma[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_D_DNCDSP0_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dsp1dnc_cache[] = {
	GOUT_BLK_DNC_UID_LHM_AXI_D_DSP1DNC_CACHE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dsp1dnc_sfr[] = {
	GOUT_BLK_DNC_UID_LHM_AXI_D_DSP1DNC_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npu1_unit0_done[] = {
	GOUT_BLK_DNC_UID_LHM_AST_D_NPU1_UNIT0_DONE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npu1_unit1_done[] = {
	GOUT_BLK_DNC_UID_LHM_AST_D_NPU1_UNIT1_DONE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dncdsp1[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_P_DNCDSP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dncdsp1_dma[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_D_DNCDSP1_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dncdsp1_sfr[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_D_DNCDSP1_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npu1_unit0_setreg[] = {
	GOUT_BLK_DNC_UID_LHS_AST_D_NPU1_UNIT0_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npu1_unit1_setreg[] = {
	GOUT_BLK_DNC_UID_LHS_AST_D_NPU1_UNIT1_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dncnpu0_dma[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_D_DNCNPU0_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dncnpu1_dma[] = {
	GOUT_BLK_DNC_UID_LHS_AXI_D_DNCNPU1_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_busif_hpmdnc[] = {
	GOUT_BLK_DNC_UID_BUSIF_HPMDNC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hpm_dnc[] = {
	GOUT_BLK_DNC_UID_HPM_DNC_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dsp0dnc_cache[] = {
	GOUT_BLK_DNC_UID_LHM_AXI_D_DSP0DNC_CACHE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_dnc2[] = {
	GOUT_BLK_DNC_UID_SYSMMU_DNC2_IPCLKPORT_CLK_S1,
	GOUT_BLK_DNC_UID_SYSMMU_DNC2_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d2_dnc[] = {
	GOUT_BLK_DNC_UID_LHS_ACEL_D2_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_dnc6[] = {
	GOUT_BLK_DNC_UID_AD_APB_DNC6_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_dns_cmu_dns[] = {
	CLK_BLK_DNS_UID_DNS_CMU_DNS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_dns[] = {
	GOUT_BLK_DNS_UID_AD_APB_DNS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ppmu_dns[] = {
	GOUT_BLK_DNS_UID_PPMU_DNS_IPCLKPORT_ACLK,
	GOUT_BLK_DNS_UID_PPMU_DNS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_dns[] = {
	GOUT_BLK_DNS_UID_SYSMMU_DNS_IPCLKPORT_CLK_S1,
	GOUT_BLK_DNS_UID_SYSMMU_DNS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dns[] = {
	GOUT_BLK_DNS_UID_D_TZPC_DNS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_dns[] = {
	GOUT_BLK_DNS_UID_LHM_AXI_P_DNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dns[] = {
	GOUT_BLK_DNS_UID_LHS_AXI_D_DNS_IPCLKPORT_I_CLK,
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
enum clk_id cmucal_vclk_ip_lhm_ast_otf_ippdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF_IPPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf_tnrdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF_TNRDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf0_itpdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF0_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_ctl_itpdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_CTL_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_ctl_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_CTL_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_vo_ippdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_VO_IPPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_vo_dnstnr[] = {
	GOUT_BLK_DNS_UID_LHS_AST_VO_DNSTNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dns[] = {
	GOUT_BLK_DNS_UID_SYSREG_DNS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_dns[] = {
	GOUT_BLK_DNS_UID_VGEN_LITE_DNS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_dns[] = {
	GOUT_BLK_DNS_UID_DNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf3_itpdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF3_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dpu_cmu_dpu[] = {
	CLK_BLK_DPU_UID_DPU_CMU_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dpu[] = {
	GOUT_BLK_DPU_UID_SYSREG_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_dpu[] = {
	GOUT_BLK_DPU_UID_SYSMMU_DPU_IPCLKPORT_CLK_S1,
	GOUT_BLK_DPU_UID_SYSMMU_DPU_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_dpu[] = {
	GOUT_BLK_DPU_UID_LHM_AXI_P_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_decon0[] = {
	GOUT_BLK_DPU_UID_AD_APB_DECON0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ppmu_dpu[] = {
	GOUT_BLK_DPU_UID_PPMU_DPU_IPCLKPORT_ACLK,
	GOUT_BLK_DPU_UID_PPMU_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dpu[] = {
	GOUT_BLK_DPU_UID_LHS_AXI_D_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dpu[] = {
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_DECON,
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_DMA,
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_DPP,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dpu[] = {
	GOUT_BLK_DPU_UID_D_TZPC_DPU_IPCLKPORT_PCLK,
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
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dspdnc_cache[] = {
	GOUT_BLK_DSP_UID_LHS_AXI_D_DSPDNC_CACHE_IPCLKPORT_I_CLK,
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
enum clk_id cmucal_vclk_ip_lhm_axi_d_dncdsp_dma[] = {
	GOUT_BLK_DSP_UID_LHM_AXI_D_DNCDSP_DMA_IPCLKPORT_I_CLK,
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
enum clk_id cmucal_vclk_ip_lhm_axi_p_g2d[] = {
	GOUT_BLK_G2D_UID_LHM_AXI_P_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_g2d[] = {
	GOUT_BLK_G2D_UID_SYSMMU_D1_G2D_IPCLKPORT_CLK_S1,
	GOUT_BLK_G2D_UID_SYSMMU_D1_G2D_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ppmu_d1_g2d[] = {
	GOUT_BLK_G2D_UID_PPMU_D1_G2D_IPCLKPORT_ACLK,
	GOUT_BLK_G2D_UID_PPMU_D1_G2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_g2d[] = {
	GOUT_BLK_G2D_UID_XIU_D_G2D_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_as_apb_jpeg[] = {
	GOUT_BLK_G2D_UID_AS_APB_JPEG_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_vgen_lite_g2d[] = {
	GOUT_BLK_G2D_UID_VGEN_LITE_G2D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_g2d[] = {
	GOUT_BLK_G2D_UID_G2D_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_jpeg[] = {
	GOUT_BLK_G2D_UID_JPEG_IPCLKPORT_I_SMFC_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_g2d[] = {
	GOUT_BLK_G2D_UID_D_TZPC_G2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_apb_g2d[] = {
	GOUT_BLK_G2D_UID_AS_APB_G2D_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d1_g2d[] = {
	GOUT_BLK_G2D_UID_LHS_ACEL_D1_G2D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_m2m[] = {
	GOUT_BLK_G2D_UID_M2M_IPCLKPORT_ACLK,
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
enum clk_id cmucal_vclk_ip_vgen_lite_g3d[] = {
	GOUT_BLK_G3D_UID_VGEN_LITE_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_gpu[] = {
	CLK_BLK_G3D_UID_GPU_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_gray2bin_g3d[] = {
	GOUT_BLK_G3D_UID_GRAY2BIN_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_g3d[] = {
	GOUT_BLK_G3D_UID_D_TZPC_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_int_g3d[] = {
	GOUT_BLK_G3D_UID_LHS_AXI_P_INT_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_int_g3d[] = {
	GOUT_BLK_G3D_UID_LHM_AXI_P_INT_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d0_g3d[] = {
	GOUT_BLK_G3D_UID_LHS_ACEL_D0_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d1_g3d[] = {
	GOUT_BLK_G3D_UID_LHS_ACEL_D1_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_g3d[] = {
	GOUT_BLK_G3D_UID_SYSMMU_D0_G3D_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_g3d[] = {
	GOUT_BLK_G3D_UID_SYSMMU_D1_G3D_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_as_apb_sysmmu_d0_g3d[] = {
	GOUT_BLK_G3D_UID_AS_APB_SYSMMU_D0_G3D_IPCLKPORT_PCLKS,
	GOUT_BLK_G3D_UID_AS_APB_SYSMMU_D0_G3D_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ppmu_d0_g3d[] = {
	GOUT_BLK_G3D_UID_PPMU_D0_G3D_IPCLKPORT_ACLK,
	GOUT_BLK_G3D_UID_PPMU_D0_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d1_g3d[] = {
	GOUT_BLK_G3D_UID_PPMU_D1_G3D_IPCLKPORT_ACLK,
	GOUT_BLK_G3D_UID_PPMU_D1_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gnss_cmu_gnss[] = {
	CLK_BLK_GNSS_UID_GNSS_CMU_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_hsi[] = {
	GOUT_BLK_HSI_UID_VGEN_LITE_HSI_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_hsi_cmu_hsi[] = {
	GOUT_BLK_HSI_UID_HSI_CMU_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_hsi[] = {
	GOUT_BLK_HSI_UID_SYSREG_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_hsi[] = {
	GOUT_BLK_HSI_UID_GPIO_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_hsi[] = {
	GOUT_BLK_HSI_UID_LHS_ACEL_D_HSI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_hsi[] = {
	GOUT_BLK_HSI_UID_LHM_AXI_P_HSI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_hsi[] = {
	GOUT_BLK_HSI_UID_XIU_D_HSI_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_hsi[] = {
	GOUT_BLK_HSI_UID_PPMU_HSI_IPCLKPORT_ACLK,
	GOUT_BLK_HSI_UID_PPMU_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_hsi[] = {
	GOUT_BLK_HSI_UID_D_TZPC_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ufs_embd[] = {
	GOUT_BLK_HSI_UID_UFS_EMBD_IPCLKPORT_I_ACLK,
	GOUT_BLK_HSI_UID_UFS_EMBD_IPCLKPORT_I_FMP_CLK,
	GOUT_BLK_HSI_UID_UFS_EMBD_IPCLKPORT_I_CLK_UNIPRO,
};
enum clk_id cmucal_vclk_ip_xhb_hsi[] = {
	GOUT_BLK_HSI_UID_XHB_HSI_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_s2mpu_d_hsi[] = {
	GOUT_BLK_HSI_UID_S2MPU_D_HSI_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_mmc_embd[] = {
	GOUT_BLK_HSI_UID_MMC_EMBD_IPCLKPORT_I_ACLK,
	GOUT_BLK_HSI_UID_MMC_EMBD_IPCLKPORT_SDCLKIN,
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
enum clk_id cmucal_vclk_ip_lhs_ast_otf_ippdns[] = {
	GOUT_BLK_IPP_UID_LHS_AST_OTF_IPPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_ipp[] = {
	GOUT_BLK_IPP_UID_D_TZPC_IPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf0_csisipp[] = {
	GOUT_BLK_IPP_UID_LHM_AST_OTF0_CSISIPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sipu_ipp[] = {
	GOUT_BLK_IPP_UID_SIPU_IPP_IPCLKPORT_CLK,
	GOUT_BLK_IPP_UID_SIPU_IPP_IPCLKPORT_CLK_C2COM_STAT,
	GOUT_BLK_IPP_UID_SIPU_IPP_IPCLKPORT_CLK_C2COM_STAT,
	GOUT_BLK_IPP_UID_SIPU_IPP_IPCLKPORT_CLK_C2COM_YDS,
	GOUT_BLK_IPP_UID_SIPU_IPP_IPCLKPORT_CLK_C2COM_YDS,
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
enum clk_id cmucal_vclk_ip_ad_apb_ipp[] = {
	GOUT_BLK_IPP_UID_AD_APB_IPP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysmmu_ipp[] = {
	GOUT_BLK_IPP_UID_SYSMMU_IPP_IPCLKPORT_CLK_S1,
	GOUT_BLK_IPP_UID_SYSMMU_IPP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_ipp[] = {
	GOUT_BLK_IPP_UID_LHS_AXI_D_IPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_ipp[] = {
	GOUT_BLK_IPP_UID_XIU_D_IPP_IPCLKPORT_ACLK,
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
	GOUT_BLK_ITP_UID_ITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_itp[] = {
	GOUT_BLK_ITP_UID_D_TZPC_ITP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf1_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF1_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_itp[] = {
	GOUT_BLK_ITP_UID_AD_APB_ITP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhm_ast_ctl_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_CTL_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_ctl_itpdns[] = {
	GOUT_BLK_ITP_UID_LHS_AST_CTL_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf_itpmcsc[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF_ITPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf_tnritp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF_TNRITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf0_itpdns[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF0_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf3_itpdns[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF3_ITPDNS_IPCLKPORT_I_CLK,
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
enum clk_id cmucal_vclk_ip_sysmmu_d0_mcsc[] = {
	GOUT_BLK_MCSC_UID_SYSMMU_D0_MCSC_IPCLKPORT_CLK_S1,
	GOUT_BLK_MCSC_UID_SYSMMU_D0_MCSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mcsc[] = {
	GOUT_BLK_MCSC_UID_D_TZPC_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_mcsc[] = {
	GOUT_BLK_MCSC_UID_VGEN_LITE_MCSC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_mcsc[] = {
	GOUT_BLK_MCSC_UID_AD_APB_MCSC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ppmu_gdc[] = {
	GOUT_BLK_MCSC_UID_PPMU_GDC_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_PPMU_GDC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_gdc[] = {
	GOUT_BLK_MCSC_UID_GDC_IPCLKPORT_CLK,
	GOUT_BLK_MCSC_UID_GDC_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_mcsc[] = {
	GOUT_BLK_MCSC_UID_SYSMMU_D1_MCSC_IPCLKPORT_CLK_S1,
	GOUT_BLK_MCSC_UID_SYSMMU_D1_MCSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mcsc[] = {
	GOUT_BLK_MCSC_UID_LHS_AXI_D1_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf_itpmcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AST_OTF_ITPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_vo_mcsccsis[] = {
	GOUT_BLK_MCSC_UID_LHS_AST_VO_MCSCCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_gdc[] = {
	GOUT_BLK_MCSC_UID_AD_APB_GDC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_c2agent_d0_mcsc[] = {
	GOUT_BLK_MCSC_UID_C2AGENT_D0_MCSC_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_vo_tnrmcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AST_VO_TNRMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_int_gdcmcsc[] = {
	GOUT_BLK_MCSC_UID_LHS_AST_INT_GDCMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_int_gdcmcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AST_INT_GDCMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_c2agent_d1_mcsc[] = {
	GOUT_BLK_MCSC_UID_C2AGENT_D1_MCSC_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_c2agent_d2_mcsc[] = {
	GOUT_BLK_MCSC_UID_C2AGENT_D2_MCSC_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_mfc_cmu_mfc[] = {
	CLK_BLK_MFC_UID_MFC_CMU_MFC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_apb_mfc[] = {
	GOUT_BLK_MFC_UID_AS_APB_MFC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysreg_mfc[] = {
	GOUT_BLK_MFC_UID_SYSREG_MFC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mfc[] = {
	GOUT_BLK_MFC_UID_LHS_AXI_D0_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mfc[] = {
	GOUT_BLK_MFC_UID_LHS_AXI_D1_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mfc[] = {
	GOUT_BLK_MFC_UID_LHM_AXI_P_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfcd0[] = {
	GOUT_BLK_MFC_UID_SYSMMU_MFCD0_IPCLKPORT_CLK_S1,
	GOUT_BLK_MFC_UID_SYSMMU_MFCD0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfcd1[] = {
	GOUT_BLK_MFC_UID_SYSMMU_MFCD1_IPCLKPORT_CLK_S1,
	GOUT_BLK_MFC_UID_SYSMMU_MFCD1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ppmu_mfcd0[] = {
	GOUT_BLK_MFC_UID_PPMU_MFCD0_IPCLKPORT_ACLK,
	GOUT_BLK_MFC_UID_PPMU_MFCD0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mfcd1[] = {
	GOUT_BLK_MFC_UID_PPMU_MFCD1_IPCLKPORT_ACLK,
	GOUT_BLK_MFC_UID_PPMU_MFCD1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_axi_wfd[] = {
	GOUT_BLK_MFC_UID_AS_AXI_WFD_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_ppmu_wfd[] = {
	GOUT_BLK_MFC_UID_PPMU_WFD_IPCLKPORT_PCLK,
	GOUT_BLK_MFC_UID_PPMU_WFD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_mfc[] = {
	GOUT_BLK_MFC_UID_XIU_D_MFC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_vgen_mfc[] = {
	GOUT_BLK_MFC_UID_VGEN_MFC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mfc[] = {
	GOUT_BLK_MFC_UID_MFC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_wfd[] = {
	GOUT_BLK_MFC_UID_WFD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_mfc[] = {
	GOUT_BLK_MFC_UID_LH_ATB_MFC_IPCLKPORT_I_CLK_MI,
	GOUT_BLK_MFC_UID_LH_ATB_MFC_IPCLKPORT_I_CLK_SI,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mfc[] = {
	GOUT_BLK_MFC_UID_D_TZPC_MFC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_apb_wfd_ns[] = {
	GOUT_BLK_MFC_UID_AS_APB_WFD_NS_IPCLKPORT_PCLKM,
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
enum clk_id cmucal_vclk_ip_ppmu_dmc_cpu[] = {
	GOUT_BLK_MIF_UID_PPMU_DMC_CPU_IPCLKPORT_PCLK,
	CLK_BLK_MIF_UID_PPMU_DMC_CPU_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_dmc_cpu[] = {
	GOUT_BLK_MIF_UID_QE_DMC_CPU_IPCLKPORT_PCLK,
	CLK_BLK_MIF_UID_QE_DMC_CPU_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_dmc[] = {
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK,
	CLK_BLK_MIF_UID_DMC_IPCLKPORT_ACLK,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_PF,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_PF,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_PPMPU,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_PPMPU,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_SECURE,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_SECURE,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mif[] = {
	GOUT_BLK_MIF_UID_D_TZPC_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_ddrphy[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DDRPHY_IPCLKPORT_PCLK,
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
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc_ppmpu[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DMC_PPMPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif_cpu[] = {
	CLK_BLK_MIF_UID_LHM_AXI_D_MIF_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif_rt[] = {
	CLK_BLK_MIF_UID_LHM_AXI_D_MIF_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif_nrt[] = {
	CLK_BLK_MIF_UID_LHM_AXI_D_MIF_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mif_cp[] = {
	CLK_BLK_MIF_UID_LHM_AXI_D_MIF_CP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_modem_cmu_modem[] = {
	CLK_BLK_MODEM_UID_MODEM_CMU_MODEM_IPCLKPORT_PCLK,
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
enum clk_id cmucal_vclk_ip_lhm_ast_d_npu_unit0_setreg[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D_NPU_UNIT0_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d_npu_unit1_setreg[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D_NPU_UNIT1_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d_npu_unit0_done[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D_NPU_UNIT0_DONE_IPCLKPORT_I_CLK,
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
enum clk_id cmucal_vclk_ip_lhs_ast_d_npu_unit1_done[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D_NPU_UNIT1_DONE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d0_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D0_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d10_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D10_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d11_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D11_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d12_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D12_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d13_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D13_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d14_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D14_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d15_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D15_NPU_IPCLKPORT_I_CLK,
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
enum clk_id cmucal_vclk_ip_lhm_ast_d5_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D5_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d6_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D6_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d7_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D7_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d8_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D8_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_d9_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AST_D9_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d0_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D0_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d10_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D10_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d11_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D11_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d12_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D12_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d13_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D13_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d14_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D14_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d15_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D15_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d1_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D1_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d2_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D2_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d3_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D3_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d4_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D4_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d5_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D5_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d6_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D6_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d7_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D7_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d8_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D8_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_d9_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AST_D9_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dncnpu_dma[] = {
	GOUT_BLK_NPU_UID_LHM_AXI_D_DNCNPU_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_npu1_cmu_npu1[] = {
	CLK_BLK_NPU1_UID_NPU1_CMU_NPU1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_peri[] = {
	GOUT_BLK_PERI_UID_GPIO_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_peri[] = {
	GOUT_BLK_PERI_UID_SYSREG_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peri_cmu_peri[] = {
	CLK_BLK_PERI_UID_PERI_CMU_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_peri[] = {
	GOUT_BLK_PERI_UID_LHM_AXI_P_PERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peri[] = {
	GOUT_BLK_PERI_UID_D_TZPC_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_p_peri[] = {
	GOUT_BLK_PERI_UID_XIU_P_PERI_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_mct[] = {
	GOUT_BLK_PERI_UID_MCT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp_con_top[] = {
	GOUT_BLK_PERI_UID_OTP_CON_TOP_IPCLKPORT_PCLK,
	CLK_BLK_PERI_UID_OTP_CON_TOP_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_wdt0[] = {
	GOUT_BLK_PERI_UID_WDT0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt1[] = {
	GOUT_BLK_PERI_UID_WDT1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_tmu[] = {
	GOUT_BLK_PERI_UID_TMU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pwm[] = {
	GOUT_BLK_PERI_UID_PWM_IPCLKPORT_I_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_bc_emul[] = {
	GOUT_BLK_PERI_UID_BC_EMUL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_uart_dbg[] = {
	GOUT_BLK_PERI_UID_UART_DBG_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_UART_DBG_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi00_usi[] = {
	GOUT_BLK_PERI_UID_USI00_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI00_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi00_i2c[] = {
	GOUT_BLK_PERI_UID_USI00_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI00_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi01_usi[] = {
	GOUT_BLK_PERI_UID_USI01_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI01_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi01_i2c[] = {
	GOUT_BLK_PERI_UID_USI01_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI01_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi02_usi[] = {
	GOUT_BLK_PERI_UID_USI02_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI02_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi02_i2c[] = {
	GOUT_BLK_PERI_UID_USI02_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI02_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi03_usi[] = {
	GOUT_BLK_PERI_UID_USI03_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI03_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi03_i2c[] = {
	GOUT_BLK_PERI_UID_USI03_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI03_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi04_usi[] = {
	GOUT_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI04_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi04_i2c[] = {
	GOUT_BLK_PERI_UID_USI04_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI04_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi05_usi[] = {
	GOUT_BLK_PERI_UID_USI05_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI05_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi05_i2c[] = {
	GOUT_BLK_PERI_UID_USI05_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI05_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp_con_bira[] = {
	GOUT_BLK_PERI_UID_OTP_CON_BIRA_IPCLKPORT_PCLK,
	CLK_BLK_PERI_UID_OTP_CON_BIRA_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_csisperi[] = {
	GOUT_BLK_PERI_UID_LHM_AXI_P_CSISPERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_spi_ois[] = {
	GOUT_BLK_PERI_UID_SPI_OIS_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_SPI_OIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_ois[] = {
	GOUT_BLK_PERI_UID_I2C_OIS_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_I2C_OIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_mmccard[] = {
	GOUT_BLK_PERI_UID_GPIO_MMCCARD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_peri[] = {
	GOUT_BLK_PERI_UID_VGEN_LITE_PERI_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_s2mpu_d_peri[] = {
	GOUT_BLK_PERI_UID_S2MPU_D_PERI_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_mmc_card[] = {
	GOUT_BLK_PERI_UID_MMC_CARD_IPCLKPORT_I_ACLK,
	GOUT_BLK_PERI_UID_MMC_CARD_IPCLKPORT_SDCLKIN,
};
enum clk_id cmucal_vclk_ip_ppmu_peri[] = {
	GOUT_BLK_PERI_UID_PPMU_PERI_IPCLKPORT_ACLK,
	GOUT_BLK_PERI_UID_PPMU_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_peri[] = {
	GOUT_BLK_PERI_UID_LHS_ACEL_D_PERI_IPCLKPORT_I_CLK,
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
enum clk_id cmucal_vclk_ip_sysreg_sspctrl[] = {
	GOUT_BLK_SSP_UID_SYSREG_SSPCTRL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_uss_sspcore[] = {
	GOUT_BLK_SSP_UID_USS_SSPCORE_IPCLKPORT_SS_SSPCORE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_tnr_cmu_tnr[] = {
	CLK_BLK_TNR_UID_TNR_CMU_TNR_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apb_async_tnr[] = {
	GOUT_BLK_TNR_UID_APB_ASYNC_TNR_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_d_tzpc_tnr[] = {
	GOUT_BLK_TNR_UID_D_TZPC_TNR_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_tnr[] = {
	GOUT_BLK_TNR_UID_LHM_AXI_P_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_vo_dnstnr[] = {
	GOUT_BLK_TNR_UID_LHM_AST_VO_DNSTNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_tnr[] = {
	GOUT_BLK_TNR_UID_SYSREG_TNR_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_tnr[] = {
	GOUT_BLK_TNR_UID_VGEN_LITE_TNR_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_tnr[] = {
	GOUT_BLK_TNR_UID_TNR_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d0_tnr[] = {
	GOUT_BLK_TNR_UID_PPMU_D0_TNR_IPCLKPORT_PCLK,
	GOUT_BLK_TNR_UID_PPMU_D0_TNR_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_tnr[] = {
	GOUT_BLK_TNR_UID_SYSMMU_D0_TNR_IPCLKPORT_CLK_S1,
	GOUT_BLK_TNR_UID_SYSMMU_D0_TNR_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_tnr[] = {
	GOUT_BLK_TNR_UID_LHS_AXI_D0_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf_tnrdns[] = {
	GOUT_BLK_TNR_UID_LHS_AST_OTF_TNRDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d1_tnr[] = {
	GOUT_BLK_TNR_UID_PPMU_D1_TNR_IPCLKPORT_PCLK,
	GOUT_BLK_TNR_UID_PPMU_D1_TNR_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf_tnritp[] = {
	GOUT_BLK_TNR_UID_LHS_AST_OTF_TNRITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_orbmch[] = {
	GOUT_BLK_TNR_UID_ORBMCH_IPCLKPORT_ACLK,
	GOUT_BLK_TNR_UID_ORBMCH_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_vo_tnrmcsc[] = {
	GOUT_BLK_TNR_UID_LHS_AST_VO_TNRMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_tnr[] = {
	GOUT_BLK_TNR_UID_LHS_AXI_D1_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_tnr[] = {
	GOUT_BLK_TNR_UID_SYSMMU_D1_TNR_IPCLKPORT_CLK_S1,
	GOUT_BLK_TNR_UID_SYSMMU_D1_TNR_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_xiu_d1_tnr[] = {
	GOUT_BLK_TNR_UID_XIU_D1_TNR_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_usb31drd[] = {
	GOUT_BLK_USB_UID_USB31DRD_IPCLKPORT_I_USB31DRD_REF_CLK_40,
	GOUT_BLK_USB_UID_USB31DRD_IPCLKPORT_ACLK_PHYCTRL,
	GOUT_BLK_USB_UID_USB31DRD_IPCLKPORT_I_USBDPPHY_SCL_APB_PCLK,
	GOUT_BLK_USB_UID_USB31DRD_IPCLKPORT_I_USBPCS_APB_CLK,
	GOUT_BLK_USB_UID_USB31DRD_IPCLKPORT_BUS_CLK_EARLY,
	GOUT_BLK_USB_UID_USB31DRD_IPCLKPORT_I_USBDPPHY_REF_SOC_PLL,
};
enum clk_id cmucal_vclk_ip_dp_link[] = {
	GOUT_BLK_USB_UID_DP_LINK_IPCLKPORT_I_DP_GTC_CLK,
	GOUT_BLK_USB_UID_DP_LINK_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_usb[] = {
	GOUT_BLK_USB_UID_PPMU_USB_IPCLKPORT_ACLK,
	GOUT_BLK_USB_UID_PPMU_USB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_usb[] = {
	GOUT_BLK_USB_UID_LHS_ACEL_D_USB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_usb[] = {
	GOUT_BLK_USB_UID_VGEN_LITE_USB_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_usb[] = {
	GOUT_BLK_USB_UID_D_TZPC_USB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_usb[] = {
	GOUT_BLK_USB_UID_LHM_AXI_P_USB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_s2mpu_d_usb[] = {
	GOUT_BLK_USB_UID_S2MPU_D_USB_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_usb[] = {
	GOUT_BLK_USB_UID_SYSREG_USB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usb_cmu_usb[] = {
	CLK_BLK_USB_UID_USB_CMU_USB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_us_d_usb[] = {
	GOUT_BLK_USB_UID_US_D_USB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_usb[] = {
	GOUT_BLK_USB_UID_XIU_D_USB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_vra_cmu_vra[] = {
	CLK_BLK_VRA_UID_VRA_CMU_VRA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_clahe[] = {
	GOUT_BLK_VRA_UID_CLAHE_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_vra[] = {
	GOUT_BLK_VRA_UID_D_TZPC_VRA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_vra[] = {
	GOUT_BLK_VRA_UID_LHM_AXI_P_VRA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_vra[] = {
	GOUT_BLK_VRA_UID_LHS_AXI_D_VRA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d0_clahe[] = {
	GOUT_BLK_VRA_UID_PPMU_D0_CLAHE_IPCLKPORT_ACLK,
	GOUT_BLK_VRA_UID_PPMU_D0_CLAHE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d1_clahe[] = {
	GOUT_BLK_VRA_UID_PPMU_D1_CLAHE_IPCLKPORT_ACLK,
	GOUT_BLK_VRA_UID_PPMU_D1_CLAHE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_vra[] = {
	GOUT_BLK_VRA_UID_PPMU_VRA_IPCLKPORT_ACLK,
	GOUT_BLK_VRA_UID_PPMU_VRA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_vra[] = {
	GOUT_BLK_VRA_UID_SYSMMU_VRA_IPCLKPORT_CLK_S1,
	GOUT_BLK_VRA_UID_SYSMMU_VRA_IPCLKPORT_CLK_S2,
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
enum clk_id cmucal_vclk_ip_xiu_d_vra[] = {
	GOUT_BLK_VRA_UID_XIU_D_VRA_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_vra[] = {
	GOUT_BLK_VRA_UID_AD_APB_VRA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_clahe[] = {
	GOUT_BLK_VRA_UID_AD_APB_CLAHE_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_axi_d0_clahe[] = {
	GOUT_BLK_VRA_UID_AD_AXI_D0_CLAHE_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_ahb_busmatrix[] = {
	GOUT_BLK_VTS_UID_AHB_BUSMATRIX_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_dmic_if[] = {
	GOUT_BLK_VTS_UID_DMIC_IF_IPCLKPORT_DMIC_IF_CLK,
	CLK_BLK_VTS_UID_DMIC_IF_IPCLKPORT_DMIC_IF_DIV2_CLK,
	GOUT_BLK_VTS_UID_DMIC_IF_IPCLKPORT_PCLK,
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
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_IF_PAD_CLK,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_IF_3RD_PAD_CLK,
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
enum clk_id cmucal_vclk_ip_mailbox_ap_vts[] = {
	GOUT_BLK_VTS_UID_MAILBOX_AP_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer[] = {
	GOUT_BLK_VTS_UID_TIMER_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_if_3rd[] = {
	GOUT_BLK_VTS_UID_DMIC_IF_3RD_IPCLKPORT_DMIC_IF_CLK,
	GOUT_BLK_VTS_UID_DMIC_IF_3RD_IPCLKPORT_DMIC_IF_DIV2_CLK,
	GOUT_BLK_VTS_UID_DMIC_IF_3RD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_u_dmic_clk_scan_mux[] = {
	CLK_BLK_VTS_UID_U_DMIC_CLK_SCAN_MUX_IPCLKPORT_D0,
};

/* DVFS VCLK -> LUT List */
struct vclk_lut cmucal_vclk_vdd_mif_lut[] = {
	{4266000, vdd_mif_nm_lut_params},
	{2666000, vdd_mif_ud_lut_params},
	{1400000, vdd_mif_sud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cam_lut[] = {
	{1200000, vdd_cam_nm_lut_params},
	{1200000, vdd_cam_sud_lut_params},
	{1200000, vdd_cam_ud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cpucl0_lut[] = {
	{1800000, vdd_cpucl0_sod_lut_params},
	{1500000, vdd_cpucl0_od_lut_params},
	{1100000, vdd_cpucl0_nm_lut_params},
	{650000, vdd_cpucl0_ud_lut_params},
	{350000, vdd_cpucl0_sud_lut_params},
};
struct vclk_lut cmucal_vclk_vddi_lut[] = {
	{950000, vddi_od_lut_params},
	{860000, vddi_nm_lut_params},
	{650000, vddi_ud_lut_params},
	{300000, vddi_sud_lut_params},
};

/* SPECIAL VCLK -> LUT List */
struct vclk_lut cmucal_vclk_mux_clk_apm_i3c_pmic_lut[] = {
	{200000, mux_clk_apm_i3c_pmic_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_apm_bus_lut[] = {
	{400000, clkcmu_apm_bus_ud_lut_params},
	{399750, clkcmu_apm_bus_od_lut_params},
};
struct vclk_lut cmucal_vclk_mux_busc_cmuref_lut[] = {
	{100000, mux_busc_cmuref_ud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cmu_boost_lut[] = {
	{200000, clkcmu_cmu_boost_ud_lut_params},
	{199875, clkcmu_cmu_boost_od_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_chub_timer_fclk_lut[] = {
	{30000, mux_clk_chub_timer_fclk_nm_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_cmgp_adc_lut[] = {
	{26000, mux_clk_cmgp_adc_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cmgp_bus_lut[] = {
	{400000, clkcmu_cmgp_bus_nm_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cmu_cmuref_lut[] = {
	{26000, mux_cmu_cmuref_ud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_core_cmuref_lut[] = {
	{200000, mux_core_cmuref_ud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cpucl0_cmuref_lut[] = {
	{200000, mux_cpucl0_cmuref_ud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cpucl1_cmuref_lut[] = {
	{200000, mux_cpucl1_cmuref_ud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_mif_cmuref_lut[] = {
	{200000, mux_mif_cmuref_ud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_peri_mmc_card_lut[] = {
	{808000, mux_clkcmu_peri_mmc_card_ud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_usb_usbdp_debug_lut[] = {
	{400000, mux_clkcmu_usb_usbdp_debug_ud_lut_params},
	{399750, mux_clkcmu_usb_usbdp_debug_od_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_usb_dpgtc_lut[] = {
	{133333, clkcmu_usb_dpgtc_ud_lut_params},
	{133250, clkcmu_usb_dpgtc_od_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_apm_dbgcore_uart_lut[] = {
	{200000, div_clk_apm_dbgcore_uart_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_aud_cpu_pclkdbg_lut[] = {
	{150000, div_clk_aud_cpu_pclkdbg_nm_lut_params},
	{100000, div_clk_aud_cpu_pclkdbg_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_aud_mclk_lut[] = {
	{50000, div_clk_aud_mclk_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_usi0_lut[] = {
	{400000, div_clk_chub_usi0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_chub_bus_lut[] = {
	{400000, clkcmu_chub_bus_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_usi1_lut[] = {
	{400000, div_clk_chub_usi1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_usi2_lut[] = {
	{400000, div_clk_chub_usi2_nm_lut_params},
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
struct vclk_lut cmucal_vclk_div_clk_i3c_cmgp_lut[] = {
	{200000, div_clk_i3c_cmgp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_hpm_lut[] = {
	{400000, clkcmu_hpm_ud_lut_params},
	{399750, clkcmu_hpm_od_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk0_lut[] = {
	{100000, clkcmu_cis_clk0_ud_lut_params},
	{99938, clkcmu_cis_clk0_od_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk1_lut[] = {
	{100000, clkcmu_cis_clk1_ud_lut_params},
	{99938, clkcmu_cis_clk1_od_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk2_lut[] = {
	{100000, clkcmu_cis_clk2_ud_lut_params},
	{99938, clkcmu_cis_clk2_od_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk3_lut[] = {
	{100000, clkcmu_cis_clk3_ud_lut_params},
	{99938, clkcmu_cis_clk3_od_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk4_lut[] = {
	{100000, clkcmu_cis_clk4_ud_lut_params},
	{99938, clkcmu_cis_clk4_od_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl0_cmuref_lut[] = {
	{900000, div_clk_cpucl0_cmuref_sod_lut_params},
	{750000, div_clk_cpucl0_cmuref_od_lut_params},
	{550000, div_clk_cpucl0_cmuref_nm_lut_params},
	{325000, div_clk_cpucl0_cmuref_ud_lut_params},
	{175000, div_clk_cpucl0_cmuref_sud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cluster0_atclk_lut[] = {
	{450000, div_clk_cluster0_atclk_sod_lut_params},
	{375000, div_clk_cluster0_atclk_od_lut_params},
	{275000, div_clk_cluster0_atclk_nm_lut_params},
	{162500, div_clk_cluster0_atclk_ud_lut_params},
	{87500, div_clk_cluster0_atclk_sud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl1_cmuref_lut[] = {
	{1200000, div_clk_cpucl1_cmuref_sod_lut_params},
	{950000, div_clk_cpucl1_cmuref_od_lut_params},
	{700000, div_clk_cpucl1_cmuref_nm_lut_params},
	{425000, div_clk_cpucl1_cmuref_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_dsp1_busp_lut[] = {
};
struct vclk_lut cmucal_vclk_div_clk_npu1_busp_lut[] = {
};
struct vclk_lut cmucal_vclk_div_clk_peri_usi00_usi_lut[] = {
	{400000, div_clk_peri_usi00_usi_ud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_peri_ip_lut[] = {
	{400000, clkcmu_peri_ip_ud_lut_params},
	{399750, clkcmu_peri_ip_od_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_usi01_usi_lut[] = {
	{400000, div_clk_peri_usi01_usi_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_usi02_usi_lut[] = {
	{400000, div_clk_peri_usi02_usi_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_usi03_usi_lut[] = {
	{400000, div_clk_peri_usi03_usi_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_usi04_usi_lut[] = {
	{400000, div_clk_peri_usi04_usi_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_usi05_usi_lut[] = {
	{400000, div_clk_peri_usi05_usi_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_uart_dbg_lut[] = {
	{200000, div_clk_peri_uart_dbg_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_spi_ois_lut[] = {
	{400000, div_clk_peri_spi_ois_ud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_ap2gnss_lut[] = {
	{400000, mux_clkcmu_ap2gnss_nm_lut_params},
};

/* COMMON VCLK -> LUT List */
struct vclk_lut cmucal_vclk_blk_cmu_lut[] = {
	{808000, blk_cmu_od_lut_params},
	{808000, blk_cmu_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl1_lut[] = {
	{2400000, blk_cpucl1_sod_lut_params},
	{1900000, blk_cpucl1_od_lut_params},
	{1400000, blk_cpucl1_nm_lut_params},
	{850000, blk_cpucl1_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_s2d_lut[] = {
	{100000, blk_s2d_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_apm_lut[] = {
	{400000, blk_apm_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_chub_lut[] = {
	{400000, blk_chub_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cmgp_lut[] = {
	{400000, blk_cmgp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_core_lut[] = {
	{333250, blk_core_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl0_lut[] = {
	{450000, blk_cpucl0_sod_lut_params},
	{375000, blk_cpucl0_od_lut_params},
	{275000, blk_cpucl0_nm_lut_params},
	{200000, blk_cpucl0_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_g3d_lut[] = {
	{215000, blk_g3d_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_vts_lut[] = {
	{400000, blk_vts_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_aud_lut[] = {
	{600000, blk_aud_nm_lut_params},
	{400000, blk_aud_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_busc_lut[] = {
	{333250, blk_busc_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_csis_lut[] = {
	{333250, blk_csis_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dnc_lut[] = {
	{200000, blk_dnc_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dns_lut[] = {
	{266667, blk_dns_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dpu_lut[] = {
	{177778, blk_dpu_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dsp_lut[] = {
	{400000, blk_dsp_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_g2d_lut[] = {
	{266667, blk_g2d_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_ipp_lut[] = {
	{266667, blk_ipp_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_itp_lut[] = {
	{266667, blk_itp_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mcsc_lut[] = {
	{266667, blk_mcsc_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mfc_lut[] = {
	{166625, blk_mfc_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_npu_lut[] = {
	{200000, blk_npu_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_peri_lut[] = {
	{200000, blk_peri_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_ssp_lut[] = {
	{200000, blk_ssp_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_tnr_lut[] = {
	{266667, blk_tnr_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_vra_lut[] = {
	{400000, blk_vra_ud_lut_params},
};

/* Switch VCLK -> LUT Parameter List */
struct switch_lut mux_clk_aud_cpu_lut[] = {
	{800000, 0, 0},
	{799500, 0, 0},
	{666500, 1, 0},
	{400000, 0, 1},
};
struct switch_lut clkmux_mif_ddrphy2x_lut[] = {
	{1600000, 0, -1},
	{1599000, 0, -1},
	{1333000, 1, -1},
};

struct vclk_lut cmucal_vclk_div_clk_usixx_lut[] = {
	{200000, div_clk_200_lut_params},
	{100000, div_clk_100_lut_params},
	{50000, div_clk_50_lut_params},
	{26000, div_clk_26_lut_params},
	{25000, div_clk_25_lut_params},
	{13000, div_clk_13_lut_params},
	{8600, div_clk_8_lut_params},
	{6500, div_clk_6_lut_params},
};

/*================================ SWPLL List =================================*/
struct vclk_switch switch_vdd_mif[] = {
	{CLKMUX_MIF_DDRPHY2X, MUX_CLKCMU_MIF_SWITCH, EMPTY_CAL_ID, CLKCMU_MIF_SWITCH, EMPTY_CAL_ID, clkmux_mif_ddrphy2x_lut, 3},
};
struct vclk_switch switch_vdd_cam[] = {
	{MUX_CLK_AUD_CPU, MUX_CLKCMU_AUD_CPU, CLKCMU_AUD_CPU, GATE_CLKCMU_AUD_CPU, MUX_CLKCMU_AUD_CPU_USER, mux_clk_aud_cpu_lut, 4},
};

/*================================ VCLK List =================================*/
unsigned int cmucal_vclk_size = 788;
struct vclk cmucal_vclk_list[] = {

/* DVFS VCLK*/
	CMUCAL_VCLK(VCLK_VDD_MIF, cmucal_vclk_vdd_mif_lut, cmucal_vclk_vdd_mif, NULL, switch_vdd_mif),
	CMUCAL_VCLK(VCLK_VDD_CAM, cmucal_vclk_vdd_cam_lut, cmucal_vclk_vdd_cam, NULL, switch_vdd_cam),
	CMUCAL_VCLK(VCLK_VDD_CPUCL0, cmucal_vclk_vdd_cpucl0_lut, cmucal_vclk_vdd_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDDI, cmucal_vclk_vddi_lut, cmucal_vclk_vddi, NULL, NULL),

/* SPECIAL VCLK*/
	CMUCAL_VCLK(VCLK_MUX_CLK_APM_I3C_PMIC, cmucal_vclk_mux_clk_apm_i3c_pmic_lut, cmucal_vclk_mux_clk_apm_i3c_pmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_APM_BUS, cmucal_vclk_clkcmu_apm_bus_lut, cmucal_vclk_clkcmu_apm_bus, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_BUSC_CMUREF, cmucal_vclk_mux_busc_cmuref_lut, cmucal_vclk_mux_busc_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CMU_BOOST, cmucal_vclk_clkcmu_cmu_boost_lut, cmucal_vclk_clkcmu_cmu_boost, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CHUB_TIMER_FCLK, cmucal_vclk_mux_clk_chub_timer_fclk_lut, cmucal_vclk_mux_clk_chub_timer_fclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CMGP_ADC, cmucal_vclk_mux_clk_cmgp_adc_lut, cmucal_vclk_mux_clk_cmgp_adc, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CMGP_BUS, cmucal_vclk_clkcmu_cmgp_bus_lut, cmucal_vclk_clkcmu_cmgp_bus, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CMU_CMUREF, cmucal_vclk_mux_cmu_cmuref_lut, cmucal_vclk_mux_cmu_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CORE_CMUREF, cmucal_vclk_mux_core_cmuref_lut, cmucal_vclk_mux_core_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CPUCL0_CMUREF, cmucal_vclk_mux_cpucl0_cmuref_lut, cmucal_vclk_mux_cpucl0_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CPUCL1_CMUREF, cmucal_vclk_mux_cpucl1_cmuref_lut, cmucal_vclk_mux_cpucl1_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_MIF_CMUREF, cmucal_vclk_mux_mif_cmuref_lut, cmucal_vclk_mux_mif_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_PERI_MMC_CARD, cmucal_vclk_mux_clkcmu_peri_mmc_card_lut, cmucal_vclk_mux_clkcmu_peri_mmc_card, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_USB_USBDP_DEBUG, cmucal_vclk_mux_clkcmu_usb_usbdp_debug_lut, cmucal_vclk_mux_clkcmu_usb_usbdp_debug, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_USB_DPGTC, cmucal_vclk_clkcmu_usb_dpgtc_lut, cmucal_vclk_clkcmu_usb_dpgtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_APM_DBGCORE_UART, cmucal_vclk_div_clk_apm_dbgcore_uart_lut, cmucal_vclk_div_clk_apm_dbgcore_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_CPU_PCLKDBG, cmucal_vclk_div_clk_aud_cpu_pclkdbg_lut, cmucal_vclk_div_clk_aud_cpu_pclkdbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_MCLK, cmucal_vclk_div_clk_aud_mclk_lut, cmucal_vclk_div_clk_aud_mclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_USI0, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_chub_usi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CHUB_BUS, cmucal_vclk_clkcmu_chub_bus_lut, cmucal_vclk_clkcmu_chub_bus, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_USI1, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_chub_usi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_USI2, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_chub_usi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_I2C, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_chub_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_USI_CMGP1, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_usi_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_USI_CMGP0, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_usi_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_USI_CMGP2, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_usi_cmgp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_USI_CMGP3, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_usi_cmgp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_I2C_CMGP, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_i2c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_I3C_CMGP, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_i3c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_HPM, cmucal_vclk_clkcmu_hpm_lut, cmucal_vclk_clkcmu_hpm, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK0, cmucal_vclk_clkcmu_cis_clk0_lut, cmucal_vclk_clkcmu_cis_clk0, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK1, cmucal_vclk_clkcmu_cis_clk1_lut, cmucal_vclk_clkcmu_cis_clk1, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK2, cmucal_vclk_clkcmu_cis_clk2_lut, cmucal_vclk_clkcmu_cis_clk2, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK3, cmucal_vclk_clkcmu_cis_clk3_lut, cmucal_vclk_clkcmu_cis_clk3, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK4, cmucal_vclk_clkcmu_cis_clk4_lut, cmucal_vclk_clkcmu_cis_clk4, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL0_CMUREF, cmucal_vclk_div_clk_cpucl0_cmuref_lut, cmucal_vclk_div_clk_cpucl0_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CLUSTER0_ATCLK, cmucal_vclk_div_clk_cluster0_atclk_lut, cmucal_vclk_div_clk_cluster0_atclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL1_CMUREF, cmucal_vclk_div_clk_cpucl1_cmuref_lut, cmucal_vclk_div_clk_cpucl1_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_DSP1_BUSP, cmucal_vclk_div_clk_dsp1_busp_lut, cmucal_vclk_div_clk_dsp1_busp, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_NPU1_BUSP, cmucal_vclk_div_clk_npu1_busp_lut, cmucal_vclk_div_clk_npu1_busp, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI00_USI, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_peri_usi00_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_PERI_IP, cmucal_vclk_clkcmu_peri_ip_lut, cmucal_vclk_clkcmu_peri_ip, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI01_USI, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_peri_usi01_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI02_USI, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_peri_usi02_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI03_USI, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_peri_usi03_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI04_USI, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_peri_usi04_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI05_USI, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_peri_usi05_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_UART_DBG, cmucal_vclk_div_clk_peri_uart_dbg_lut, cmucal_vclk_div_clk_peri_uart_dbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_SPI_OIS, cmucal_vclk_div_clk_usixx_lut, cmucal_vclk_div_clk_peri_spi_ois, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_AP2GNSS, cmucal_vclk_mux_clkcmu_ap2gnss_lut, cmucal_vclk_mux_clkcmu_ap2gnss, NULL, NULL),

/* COMMON VCLK*/
	CMUCAL_VCLK(VCLK_BLK_CMU, cmucal_vclk_blk_cmu_lut, cmucal_vclk_blk_cmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL1, cmucal_vclk_blk_cpucl1_lut, cmucal_vclk_blk_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_S2D, cmucal_vclk_blk_s2d_lut, cmucal_vclk_blk_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_APM, cmucal_vclk_blk_apm_lut, cmucal_vclk_blk_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CHUB, cmucal_vclk_blk_chub_lut, cmucal_vclk_blk_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CMGP, cmucal_vclk_blk_cmgp_lut, cmucal_vclk_blk_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CORE, cmucal_vclk_blk_core_lut, cmucal_vclk_blk_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL0, cmucal_vclk_blk_cpucl0_lut, cmucal_vclk_blk_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_G3D, cmucal_vclk_blk_g3d_lut, cmucal_vclk_blk_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_VTS, cmucal_vclk_blk_vts_lut, cmucal_vclk_blk_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_AUD, cmucal_vclk_blk_aud_lut, cmucal_vclk_blk_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_BUSC, cmucal_vclk_blk_busc_lut, cmucal_vclk_blk_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CSIS, cmucal_vclk_blk_csis_lut, cmucal_vclk_blk_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DNC, cmucal_vclk_blk_dnc_lut, cmucal_vclk_blk_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DNS, cmucal_vclk_blk_dns_lut, cmucal_vclk_blk_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DPU, cmucal_vclk_blk_dpu_lut, cmucal_vclk_blk_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DSP, cmucal_vclk_blk_dsp_lut, cmucal_vclk_blk_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_G2D, cmucal_vclk_blk_g2d_lut, cmucal_vclk_blk_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_IPP, cmucal_vclk_blk_ipp_lut, cmucal_vclk_blk_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_ITP, cmucal_vclk_blk_itp_lut, cmucal_vclk_blk_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MCSC, cmucal_vclk_blk_mcsc_lut, cmucal_vclk_blk_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MFC, cmucal_vclk_blk_mfc_lut, cmucal_vclk_blk_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_NPU, cmucal_vclk_blk_npu_lut, cmucal_vclk_blk_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_PERI, cmucal_vclk_blk_peri_lut, cmucal_vclk_blk_peri, NULL, NULL),
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
	CMUCAL_VCLK(VCLK_IP_DBGCORE_UART, NULL, cmucal_vclk_ip_dbgcore_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ROM_CRC32_HOST, NULL, cmucal_vclk_ip_rom_crc32_host, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_GNSS, NULL, cmucal_vclk_ip_lhm_axi_c_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_MODEM, NULL, cmucal_vclk_ip_lhm_axi_c_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_CHUB, NULL, cmucal_vclk_ip_lhm_axi_c_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_WLBT, NULL, cmucal_vclk_ip_lhm_axi_c_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_LP_CHUB, NULL, cmucal_vclk_ip_lhs_axi_lp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_CHUB, NULL, cmucal_vclk_ip_mailbox_apm_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_WLBT_CHUB, NULL, cmucal_vclk_ip_mailbox_wlbt_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_WLBT_ABOX, NULL, cmucal_vclk_ip_mailbox_wlbt_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_WLBT, NULL, cmucal_vclk_ip_mailbox_ap_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_WLBT, NULL, cmucal_vclk_ip_mailbox_apm_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_GNSS_WLBT, NULL, cmucal_vclk_ip_mailbox_gnss_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_GNSS_CHUB, NULL, cmucal_vclk_ip_mailbox_gnss_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_GNSS, NULL, cmucal_vclk_ip_mailbox_ap_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_GNSS, NULL, cmucal_vclk_ip_mailbox_apm_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_GNSS, NULL, cmucal_vclk_ip_mailbox_cp_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_WLBT, NULL, cmucal_vclk_ip_mailbox_cp_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_CHUB, NULL, cmucal_vclk_ip_mailbox_cp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CP_S, NULL, cmucal_vclk_ip_mailbox_ap_cp_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CP, NULL, cmucal_vclk_ip_mailbox_ap_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_CP, NULL, cmucal_vclk_ip_mailbox_apm_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_VTS_CHUB, NULL, cmucal_vclk_ip_mailbox_vts_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CHUB, NULL, cmucal_vclk_ip_mailbox_ap_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C_APM_PMIC, NULL, cmucal_vclk_ip_i3c_apm_pmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_RFMSPEEDY_APM, NULL, cmucal_vclk_ip_rfmspeedy_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VGPIO2AP, NULL, cmucal_vclk_ip_sysreg_vgpio2ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VGPIO2APM, NULL, cmucal_vclk_ip_sysreg_vgpio2apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VGPIO2PMU, NULL, cmucal_vclk_ip_sysreg_vgpio2pmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_CHUB_RTC, NULL, cmucal_vclk_ip_apbif_chub_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AUD_CMU_AUD, NULL, cmucal_vclk_ip_aud_cmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_AUD, NULL, cmucal_vclk_ip_lhs_axi_d_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_AUD, NULL, cmucal_vclk_ip_ppmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_AUD, NULL, cmucal_vclk_ip_sysreg_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ABOX, NULL, cmucal_vclk_ip_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_AUD, NULL, cmucal_vclk_ip_lhm_axi_p_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERI_AXI_ASB, NULL, cmucal_vclk_ip_peri_axi_asb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_AUD, NULL, cmucal_vclk_ip_wdt_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_AUD, NULL, cmucal_vclk_ip_sysmmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_AUD, NULL, cmucal_vclk_ip_ad_apb_sysmmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_AUD_S, NULL, cmucal_vclk_ip_ad_apb_sysmmu_aud_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_AUD, NULL, cmucal_vclk_ip_vgen_lite_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_AUD_NS1, NULL, cmucal_vclk_ip_ad_apb_sysmmu_aud_ns1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DFTMUX_AUD, NULL, cmucal_vclk_ip_dftmux_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AUD0, NULL, cmucal_vclk_ip_mailbox_aud0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AUD1, NULL, cmucal_vclk_ip_mailbox_aud1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_AUD, NULL, cmucal_vclk_ip_d_tzpc_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSC_CMU_BUSC, NULL, cmucal_vclk_ip_busc_cmu_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PDMA, NULL, cmucal_vclk_ip_ad_apb_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_BUSC, NULL, cmucal_vclk_ip_xiu_p_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_BUSC, NULL, cmucal_vclk_ip_xiu_d_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D0_DNC, NULL, cmucal_vclk_ip_lhm_acel_d0_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D0_G2D, NULL, cmucal_vclk_ip_lhm_acel_d0_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D1_DNC, NULL, cmucal_vclk_ip_lhm_acel_d1_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D1_G2D, NULL, cmucal_vclk_ip_lhm_acel_d1_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D2_DNC, NULL, cmucal_vclk_ip_lhm_acel_d2_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_PERI, NULL, cmucal_vclk_ip_lhm_acel_d_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_USB, NULL, cmucal_vclk_ip_lhm_acel_d_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_MFC, NULL, cmucal_vclk_ip_lhm_axi_d0_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_APM, NULL, cmucal_vclk_ip_lhm_axi_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_CHUB, NULL, cmucal_vclk_ip_lhm_axi_d_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_SSP, NULL, cmucal_vclk_ip_lhm_axi_d_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_TNR, NULL, cmucal_vclk_ip_lhm_axi_d0_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_VTS, NULL, cmucal_vclk_ip_lhm_axi_d_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA_BUSC, NULL, cmucal_vclk_ip_pdma_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPDMA_BUSC, NULL, cmucal_vclk_ip_spdma_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_AXI_D_BUSC, NULL, cmucal_vclk_ip_sysmmu_axi_d_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_BUSC, NULL, cmucal_vclk_ip_sysreg_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_BUSC, NULL, cmucal_vclk_ip_trex_d_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_PDMA, NULL, cmucal_vclk_ip_vgen_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_MFC, NULL, cmucal_vclk_ip_lhm_axi_d1_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_BUSC, NULL, cmucal_vclk_ip_lhm_axi_p_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_BUSC, NULL, cmucal_vclk_ip_d_tzpc_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_TNR, NULL, cmucal_vclk_ip_lhm_axi_d1_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_GPIO_CHUB, NULL, cmucal_vclk_ip_apbif_gpio_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_CHUB_COMBINE_WAKEUP_SRC, NULL, cmucal_vclk_ip_apbif_chub_combine_wakeup_src, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CHUB_CMU_CHUB, NULL, cmucal_vclk_ip_chub_cmu_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_CHUB, NULL, cmucal_vclk_ip_xiu_dp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_D_CHUB, NULL, cmucal_vclk_ip_baaw_d_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_APM_CHUB, NULL, cmucal_vclk_ip_baaw_p_apm_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CM4_CHUB, NULL, cmucal_vclk_ip_cm4_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_LP_CHUB, NULL, cmucal_vclk_ip_lhm_axi_lp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CHUB, NULL, cmucal_vclk_ip_lhm_axi_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_CHUB, NULL, cmucal_vclk_ip_lhs_axi_d_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_C_CHUB, NULL, cmucal_vclk_ip_lhs_axi_c_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA_CHUB, NULL, cmucal_vclk_ip_pdma_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PWM_CHUB, NULL, cmucal_vclk_ip_pwm_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_D_CHUB, NULL, cmucal_vclk_ip_sweeper_d_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CHUB, NULL, cmucal_vclk_ip_sysreg_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER_CHUB, NULL, cmucal_vclk_ip_timer_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_CHUB, NULL, cmucal_vclk_ip_wdt_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_AXI_LP_CHUB, NULL, cmucal_vclk_ip_bps_axi_lp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_AXI_P_CHUB, NULL, cmucal_vclk_ip_bps_axi_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CHUB, NULL, cmucal_vclk_ip_d_tzpc_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CHUB0, NULL, cmucal_vclk_ip_i2c_chub0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CHUB1, NULL, cmucal_vclk_ip_i2c_chub1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CHUB2, NULL, cmucal_vclk_ip_i2c_chub2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CHUB0, NULL, cmucal_vclk_ip_usi_chub0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CHUB1, NULL, cmucal_vclk_ip_usi_chub1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CHUB2, NULL, cmucal_vclk_ip_usi_chub2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_COMBINE_CHUB2AP, NULL, cmucal_vclk_ip_sysreg_combine_chub2ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_COMBINE_CHUB2APM, NULL, cmucal_vclk_ip_sysreg_combine_chub2apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_COMBINE_CHUB2CP, NULL, cmucal_vclk_ip_sysreg_combine_chub2cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_COMBINE_CHUB2GNSS, NULL, cmucal_vclk_ip_sysreg_combine_chub2gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_COMBINE_CHUB2WLBT, NULL, cmucal_vclk_ip_sysreg_combine_chub2wlbt, NULL, NULL),
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
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2GNSS, NULL, cmucal_vclk_ip_sysreg_cmgp2gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2WLBT, NULL, cmucal_vclk_ip_sysreg_cmgp2wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2CP, NULL, cmucal_vclk_ip_sysreg_cmgp2cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2CHUB, NULL, cmucal_vclk_ip_sysreg_cmgp2chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C_CMGP, NULL, cmucal_vclk_ip_i3c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CORE_CMU_CORE, NULL, cmucal_vclk_ip_core_cmu_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CORE, NULL, cmucal_vclk_ip_sysreg_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIREX, NULL, cmucal_vclk_ip_sirex, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DNS, NULL, cmucal_vclk_ip_lhm_axi_d_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_CORE, NULL, cmucal_vclk_ip_trex_p_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_ACE_CPUCL1, NULL, cmucal_vclk_ip_ppmu_ace_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DIT, NULL, cmucal_vclk_ip_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_NRT, NULL, cmucal_vclk_ip_trex_d_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_G3D, NULL, cmucal_vclk_ip_lhs_axi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CPUCL0, NULL, cmucal_vclk_ip_lhs_axi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CSIS, NULL, cmucal_vclk_ip_lhs_axi_p_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DPU, NULL, cmucal_vclk_ip_lhm_axi_d_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_HSI, NULL, cmucal_vclk_ip_lhm_acel_d_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_CORE, NULL, cmucal_vclk_ip_trex_d_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_APM, NULL, cmucal_vclk_ip_lhs_axi_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CORE, NULL, cmucal_vclk_ip_d_tzpc_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_WLBT, NULL, cmucal_vclk_ip_lhm_axi_d_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_CSIS, NULL, cmucal_vclk_ip_lhm_axi_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_CSIS, NULL, cmucal_vclk_ip_lhm_axi_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_WLBT, NULL, cmucal_vclk_ip_lhs_axi_p_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_IPP, NULL, cmucal_vclk_ip_lhm_axi_d_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_MODEM, NULL, cmucal_vclk_ip_lhm_axi_d0_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_MODEM, NULL, cmucal_vclk_ip_lhm_axi_d1_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_MCSC, NULL, cmucal_vclk_ip_lhm_axi_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_MCSC, NULL, cmucal_vclk_ip_lhm_axi_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_GNSS, NULL, cmucal_vclk_ip_lhm_axi_d_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MIF_NRT, NULL, cmucal_vclk_ip_lhs_axi_d0_mif_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_ACE_CPUCL0, NULL, cmucal_vclk_ip_ppmu_ace_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_CCI_550, NULL, cmucal_vclk_ip_ad_apb_cci_550, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DIT, NULL, cmucal_vclk_ip_ad_apb_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_GIC, NULL, cmucal_vclk_ip_ad_axi_gic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_USB, NULL, cmucal_vclk_ip_lhs_axi_p_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MODEM, NULL, cmucal_vclk_ip_lhs_axi_p_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF0, NULL, cmucal_vclk_ip_lhs_axi_p_mif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF1, NULL, cmucal_vclk_ip_lhs_axi_p_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MFC, NULL, cmucal_vclk_ip_lhs_axi_p_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_GNSS, NULL, cmucal_vclk_ip_lhs_axi_p_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CHUB, NULL, cmucal_vclk_ip_lhs_axi_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GIC, NULL, cmucal_vclk_ip_gic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_AUD, NULL, cmucal_vclk_ip_lhm_axi_d_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MIF_CPU, NULL, cmucal_vclk_ip_lhs_axi_d0_mif_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MIF_CP, NULL, cmucal_vclk_ip_lhs_axi_d1_mif_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MIF_CPU, NULL, cmucal_vclk_ip_lhs_axi_d1_mif_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MIF_NRT, NULL, cmucal_vclk_ip_lhs_axi_d1_mif_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_AUD, NULL, cmucal_vclk_ip_lhs_axi_p_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DNC, NULL, cmucal_vclk_ip_lhs_axi_p_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DPU, NULL, cmucal_vclk_ip_lhs_axi_p_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_G2D, NULL, cmucal_vclk_ip_lhs_axi_p_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_HSI, NULL, cmucal_vclk_ip_lhs_axi_p_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_IPP, NULL, cmucal_vclk_ip_lhs_axi_p_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_GNSS, NULL, cmucal_vclk_ip_baaw_p_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_MODEM, NULL, cmucal_vclk_ip_baaw_p_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_CHUB, NULL, cmucal_vclk_ip_baaw_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_WLBT, NULL, cmucal_vclk_ip_baaw_p_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFR_APBIF_CMU_TOPC, NULL, cmucal_vclk_ip_sfr_apbif_cmu_topc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CCI_550, NULL, cmucal_vclk_ip_cci_550, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DNS, NULL, cmucal_vclk_ip_lhs_axi_p_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MCSC, NULL, cmucal_vclk_ip_lhs_axi_p_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_SSP, NULL, cmucal_vclk_ip_lhs_axi_p_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_TNR, NULL, cmucal_vclk_ip_lhs_axi_p_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_VTS, NULL, cmucal_vclk_ip_lhs_axi_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_SIREX, NULL, cmucal_vclk_ip_vgen_lite_sirex, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_ITP, NULL, cmucal_vclk_ip_lhs_axi_p_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_NPU0, NULL, cmucal_vclk_ip_lhs_axi_p_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_PERI, NULL, cmucal_vclk_ip_lhs_axi_p_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_SSS, NULL, cmucal_vclk_ip_lhm_axi_d_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_ACEL_D_DIT, NULL, cmucal_vclk_ip_sysmmu_acel_d_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_VTS, NULL, cmucal_vclk_ip_baaw_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_CORE, NULL, cmucal_vclk_ip_xiu_d1_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D0_CLUSTER0, NULL, cmucal_vclk_ip_lhm_ace_d0_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D1_CLUSTER0, NULL, cmucal_vclk_ip_lhm_ace_d1_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_SSS, NULL, cmucal_vclk_ip_lhs_axi_d_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_CSSYS, NULL, cmucal_vclk_ip_lhm_axi_g_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MIF_CP, NULL, cmucal_vclk_ip_lhs_axi_d0_mif_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SSS, NULL, cmucal_vclk_ip_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_AHB_SSS, NULL, cmucal_vclk_ip_adm_ahb_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PUF, NULL, cmucal_vclk_ip_puf, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PUF, NULL, cmucal_vclk_ip_ad_apb_puf, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_RTIC, NULL, cmucal_vclk_ip_rtic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_DNC, NULL, cmucal_vclk_ip_baaw_p_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_NPU1, NULL, cmucal_vclk_ip_lhs_axi_p_npu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MIF_RT, NULL, cmucal_vclk_ip_lhs_axi_d0_mif_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MIF_RT, NULL, cmucal_vclk_ip_lhs_axi_d1_mif_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_AXI_D_CORE1, NULL, cmucal_vclk_ip_sysmmu_axi_d_core1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D2_MODEM, NULL, cmucal_vclk_ip_lhm_axi_d2_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_VRA, NULL, cmucal_vclk_ip_lhm_axi_d_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_BUSC, NULL, cmucal_vclk_ip_lhs_axi_p_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MCW, NULL, cmucal_vclk_ip_lhs_axi_p_mcw, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_VRA, NULL, cmucal_vclk_ip_lhs_axi_p_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D0_G3D, NULL, cmucal_vclk_ip_lhm_acel_d0_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D1_G3D, NULL, cmucal_vclk_ip_lhm_acel_d1_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_D_SSS, NULL, cmucal_vclk_ip_baaw_d_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BDU, NULL, cmucal_vclk_ip_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_G_BDU, NULL, cmucal_vclk_ip_xiu_g_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_DEBUG, NULL, cmucal_vclk_ip_ppc_debug, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_DBG_INT_G_BDU, NULL, cmucal_vclk_ip_lhs_dbg_int_g_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_DBG_INT_G_BDU, NULL, cmucal_vclk_ip_lhm_dbg_int_g_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CPUCL0, NULL, cmucal_vclk_ip_sysreg_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPMCPUCL0, NULL, cmucal_vclk_ip_busif_hpmcpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSSYS, NULL, cmucal_vclk_ip_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SECJTAG, NULL, cmucal_vclk_ip_secjtag, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CPUCL0, NULL, cmucal_vclk_ip_lhm_axi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACE_D0_CLUSTER0, NULL, cmucal_vclk_ip_lhs_ace_d0_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_G_CLUSTER0, NULL, cmucal_vclk_ip_adm_apb_g_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0_CMU_CPUCL0, NULL, cmucal_vclk_ip_cpucl0_cmu_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CLUSTER0, NULL, cmucal_vclk_ip_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACE_D1_CLUSTER0, NULL, cmucal_vclk_ip_lhs_ace_d1_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CPUCL0, NULL, cmucal_vclk_ip_d_tzpc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_INT_CSSYS, NULL, cmucal_vclk_ip_lhs_axi_g_int_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_INT_CSSYS, NULL, cmucal_vclk_ip_lhm_axi_g_int_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_CPUCL0, NULL, cmucal_vclk_ip_xiu_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_CSSYS, NULL, cmucal_vclk_ip_xiu_dp_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_CSSYS, NULL, cmucal_vclk_ip_lhs_axi_g_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_CPUCL0_0, NULL, cmucal_vclk_ip_hpm_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_P_CSSYS_0, NULL, cmucal_vclk_ip_apb_async_p_cssys_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_CPUCL0, NULL, cmucal_vclk_ip_bps_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_INT_DBGCORE, NULL, cmucal_vclk_ip_lhm_axi_g_int_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_DBGCORE, NULL, cmucal_vclk_ip_lhm_axi_g_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_INT_DBGCORE, NULL, cmucal_vclk_ip_lhs_axi_g_int_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL1_CMU_CPUCL1, NULL, cmucal_vclk_ip_cpucl1_cmu_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL1, NULL, cmucal_vclk_ip_cpucl1, NULL, NULL),
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
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CSISPERI, NULL, cmucal_vclk_ip_lhs_axi_p_csisperi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DNC_CMU_DNC, NULL, cmucal_vclk_ip_dnc_cmu_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DNC, NULL, cmucal_vclk_ip_d_tzpc_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DNCDSP0, NULL, cmucal_vclk_ip_lhs_axi_p_dncdsp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DNCDSP0_SFR, NULL, cmucal_vclk_ip_lhs_axi_d_dncdsp0_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_NPUC, NULL, cmucal_vclk_ip_ip_npuc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DNC, NULL, cmucal_vclk_ip_lhm_axi_p_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DSP0DNC_SFR, NULL, cmucal_vclk_ip_lhm_axi_d_dsp0dnc_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPU0_UNIT0_DONE, NULL, cmucal_vclk_ip_lhm_ast_d_npu0_unit0_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPU0_UNIT1_DONE, NULL, cmucal_vclk_ip_lhm_ast_d_npu0_unit1_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_DNC, NULL, cmucal_vclk_ip_vgen_lite_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DNC0, NULL, cmucal_vclk_ip_ppmu_dnc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DNC1, NULL, cmucal_vclk_ip_ppmu_dnc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DNC2, NULL, cmucal_vclk_ip_ppmu_dnc2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_DSPC, NULL, cmucal_vclk_ip_ip_dspc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DNC0, NULL, cmucal_vclk_ip_sysmmu_dnc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DNC1, NULL, cmucal_vclk_ip_sysmmu_dnc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DNC, NULL, cmucal_vclk_ip_sysreg_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_DAP_DNC, NULL, cmucal_vclk_ip_adm_dap_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPU0_UNIT0_SETREG, NULL, cmucal_vclk_ip_lhs_ast_d_npu0_unit0_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPU0_UNIT1_SETREG, NULL, cmucal_vclk_ip_lhs_ast_d_npu0_unit1_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DNC0, NULL, cmucal_vclk_ip_ad_apb_dnc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D0_DNC, NULL, cmucal_vclk_ip_lhs_acel_d0_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D1_DNC, NULL, cmucal_vclk_ip_lhs_acel_d1_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DSPC_200, NULL, cmucal_vclk_ip_lhs_axi_p_dspc_200, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DSPC_800, NULL, cmucal_vclk_ip_lhm_axi_p_dspc_800, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DSPCNPUC_200, NULL, cmucal_vclk_ip_lhm_axi_d_dspcnpuc_200, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DSPCNPUC_800, NULL, cmucal_vclk_ip_lhs_axi_d_dspcnpuc_800, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DNCDSP0_DMA, NULL, cmucal_vclk_ip_lhs_axi_d_dncdsp0_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DSP1DNC_CACHE, NULL, cmucal_vclk_ip_lhm_axi_d_dsp1dnc_cache, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DSP1DNC_SFR, NULL, cmucal_vclk_ip_lhm_axi_d_dsp1dnc_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPU1_UNIT0_DONE, NULL, cmucal_vclk_ip_lhm_ast_d_npu1_unit0_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPU1_UNIT1_DONE, NULL, cmucal_vclk_ip_lhm_ast_d_npu1_unit1_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DNCDSP1, NULL, cmucal_vclk_ip_lhs_axi_p_dncdsp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DNCDSP1_DMA, NULL, cmucal_vclk_ip_lhs_axi_d_dncdsp1_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DNCDSP1_SFR, NULL, cmucal_vclk_ip_lhs_axi_d_dncdsp1_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPU1_UNIT0_SETREG, NULL, cmucal_vclk_ip_lhs_ast_d_npu1_unit0_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPU1_UNIT1_SETREG, NULL, cmucal_vclk_ip_lhs_ast_d_npu1_unit1_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DNCNPU0_DMA, NULL, cmucal_vclk_ip_lhs_axi_d_dncnpu0_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DNCNPU1_DMA, NULL, cmucal_vclk_ip_lhs_axi_d_dncnpu1_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPMDNC, NULL, cmucal_vclk_ip_busif_hpmdnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_DNC, NULL, cmucal_vclk_ip_hpm_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DSP0DNC_CACHE, NULL, cmucal_vclk_ip_lhm_axi_d_dsp0dnc_cache, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DNC2, NULL, cmucal_vclk_ip_sysmmu_dnc2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D2_DNC, NULL, cmucal_vclk_ip_lhs_acel_d2_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DNC6, NULL, cmucal_vclk_ip_ad_apb_dnc6, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DNS_CMU_DNS, NULL, cmucal_vclk_ip_dns_cmu_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DNS, NULL, cmucal_vclk_ip_ad_apb_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DNS, NULL, cmucal_vclk_ip_ppmu_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DNS, NULL, cmucal_vclk_ip_sysmmu_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DNS, NULL, cmucal_vclk_ip_d_tzpc_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DNS, NULL, cmucal_vclk_ip_lhm_axi_p_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DNS, NULL, cmucal_vclk_ip_lhs_axi_d_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF0_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf0_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF1_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf1_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF2_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf2_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF3_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf3_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF_IPPDNS, NULL, cmucal_vclk_ip_lhm_ast_otf_ippdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF_TNRDNS, NULL, cmucal_vclk_ip_lhm_ast_otf_tnrdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_ITPDNS, NULL, cmucal_vclk_ip_lhm_ast_otf0_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_CTL_ITPDNS, NULL, cmucal_vclk_ip_lhm_ast_ctl_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_CTL_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_ctl_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_VO_IPPDNS, NULL, cmucal_vclk_ip_lhm_ast_vo_ippdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_VO_DNSTNR, NULL, cmucal_vclk_ip_lhs_ast_vo_dnstnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DNS, NULL, cmucal_vclk_ip_sysreg_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_DNS, NULL, cmucal_vclk_ip_vgen_lite_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DNS, NULL, cmucal_vclk_ip_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF3_ITPDNS, NULL, cmucal_vclk_ip_lhm_ast_otf3_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPU_CMU_DPU, NULL, cmucal_vclk_ip_dpu_cmu_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DPU, NULL, cmucal_vclk_ip_sysreg_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DPU, NULL, cmucal_vclk_ip_sysmmu_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DPU, NULL, cmucal_vclk_ip_lhm_axi_p_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DECON0, NULL, cmucal_vclk_ip_ad_apb_decon0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPU, NULL, cmucal_vclk_ip_ppmu_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DPU, NULL, cmucal_vclk_ip_lhs_axi_d_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPU, NULL, cmucal_vclk_ip_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DPU, NULL, cmucal_vclk_ip_d_tzpc_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DSP_CMU_DSP, NULL, cmucal_vclk_ip_dsp_cmu_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DSP, NULL, cmucal_vclk_ip_sysreg_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DSPDNC_SFR, NULL, cmucal_vclk_ip_lhs_axi_d_dspdnc_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_DSP, NULL, cmucal_vclk_ip_ip_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DSPDNC_CACHE, NULL, cmucal_vclk_ip_lhs_axi_d_dspdnc_cache, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DNCDSP, NULL, cmucal_vclk_ip_lhm_axi_p_dncdsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DNCDSP_SFR, NULL, cmucal_vclk_ip_lhm_axi_d_dncdsp_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DSP, NULL, cmucal_vclk_ip_d_tzpc_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DNCDSP_DMA, NULL, cmucal_vclk_ip_lhm_axi_d_dncdsp_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DSP1_CMU_DSP1, NULL, cmucal_vclk_ip_dsp1_cmu_dsp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G2D_CMU_G2D, NULL, cmucal_vclk_ip_g2d_cmu_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0_G2D, NULL, cmucal_vclk_ip_ppmu_d0_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_G2D, NULL, cmucal_vclk_ip_sysmmu_d0_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_G2D, NULL, cmucal_vclk_ip_sysreg_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D0_G2D, NULL, cmucal_vclk_ip_lhs_acel_d0_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_G2D, NULL, cmucal_vclk_ip_lhm_axi_p_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_G2D, NULL, cmucal_vclk_ip_sysmmu_d1_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1_G2D, NULL, cmucal_vclk_ip_ppmu_d1_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_G2D, NULL, cmucal_vclk_ip_xiu_d_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_JPEG, NULL, cmucal_vclk_ip_as_apb_jpeg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_G2D, NULL, cmucal_vclk_ip_vgen_lite_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G2D, NULL, cmucal_vclk_ip_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_JPEG, NULL, cmucal_vclk_ip_jpeg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_G2D, NULL, cmucal_vclk_ip_d_tzpc_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_G2D, NULL, cmucal_vclk_ip_as_apb_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D1_G2D, NULL, cmucal_vclk_ip_lhs_acel_d1_g2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_M2M, NULL, cmucal_vclk_ip_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_G3D, NULL, cmucal_vclk_ip_lhm_axi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPMG3D, NULL, cmucal_vclk_ip_busif_hpmg3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_G3D, NULL, cmucal_vclk_ip_hpm_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_G3D, NULL, cmucal_vclk_ip_sysreg_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G3D_CMU_G3D, NULL, cmucal_vclk_ip_g3d_cmu_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_G3D, NULL, cmucal_vclk_ip_vgen_lite_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPU, NULL, cmucal_vclk_ip_gpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GRAY2BIN_G3D, NULL, cmucal_vclk_ip_gray2bin_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_G3D, NULL, cmucal_vclk_ip_d_tzpc_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_INT_G3D, NULL, cmucal_vclk_ip_lhs_axi_p_int_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_INT_G3D, NULL, cmucal_vclk_ip_lhm_axi_p_int_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D0_G3D, NULL, cmucal_vclk_ip_lhs_acel_d0_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D1_G3D, NULL, cmucal_vclk_ip_lhs_acel_d1_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_G3D, NULL, cmucal_vclk_ip_sysmmu_d0_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_G3D, NULL, cmucal_vclk_ip_sysmmu_d1_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_SYSMMU_D0_G3D, NULL, cmucal_vclk_ip_as_apb_sysmmu_d0_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0_G3D, NULL, cmucal_vclk_ip_ppmu_d0_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1_G3D, NULL, cmucal_vclk_ip_ppmu_d1_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GNSS_CMU_GNSS, NULL, cmucal_vclk_ip_gnss_cmu_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_HSI, NULL, cmucal_vclk_ip_vgen_lite_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HSI_CMU_HSI, NULL, cmucal_vclk_ip_hsi_cmu_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_HSI, NULL, cmucal_vclk_ip_sysreg_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_HSI, NULL, cmucal_vclk_ip_gpio_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_HSI, NULL, cmucal_vclk_ip_lhs_acel_d_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_HSI, NULL, cmucal_vclk_ip_lhm_axi_p_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_HSI, NULL, cmucal_vclk_ip_xiu_d_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_HSI, NULL, cmucal_vclk_ip_ppmu_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_HSI, NULL, cmucal_vclk_ip_d_tzpc_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UFS_EMBD, NULL, cmucal_vclk_ip_ufs_embd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XHB_HSI, NULL, cmucal_vclk_ip_xhb_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_S2MPU_D_HSI, NULL, cmucal_vclk_ip_s2mpu_d_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MMC_EMBD, NULL, cmucal_vclk_ip_mmc_embd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_IPP, NULL, cmucal_vclk_ip_lhm_axi_p_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_IPP, NULL, cmucal_vclk_ip_sysreg_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IPP_CMU_IPP, NULL, cmucal_vclk_ip_ipp_cmu_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF_IPPDNS, NULL, cmucal_vclk_ip_lhs_ast_otf_ippdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_IPP, NULL, cmucal_vclk_ip_d_tzpc_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_CSISIPP, NULL, cmucal_vclk_ip_lhm_ast_otf0_csisipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIPU_IPP, NULL, cmucal_vclk_ip_sipu_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_ZOTF0_IPPCSIS, NULL, cmucal_vclk_ip_lhs_ast_zotf0_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_ZOTF1_IPPCSIS, NULL, cmucal_vclk_ip_lhs_ast_zotf1_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_ZOTF2_IPPCSIS, NULL, cmucal_vclk_ip_lhs_ast_zotf2_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_IPP, NULL, cmucal_vclk_ip_ppmu_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_IPP, NULL, cmucal_vclk_ip_vgen_lite_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF1_CSISIPP, NULL, cmucal_vclk_ip_lhm_ast_otf1_csisipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF2_CSISIPP, NULL, cmucal_vclk_ip_lhm_ast_otf2_csisipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_SOTF0_IPPCSIS, NULL, cmucal_vclk_ip_lhs_ast_sotf0_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_SOTF1_IPPCSIS, NULL, cmucal_vclk_ip_lhs_ast_sotf1_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_SOTF2_IPPCSIS, NULL, cmucal_vclk_ip_lhs_ast_sotf2_ippcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_VO_CSISIPP, NULL, cmucal_vclk_ip_lhm_ast_vo_csisipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_VO_IPPDNS, NULL, cmucal_vclk_ip_lhs_ast_vo_ippdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_IPP, NULL, cmucal_vclk_ip_ad_apb_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_IPP, NULL, cmucal_vclk_ip_sysmmu_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_IPP, NULL, cmucal_vclk_ip_lhs_axi_d_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_IPP, NULL, cmucal_vclk_ip_xiu_d_ipp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_ITP, NULL, cmucal_vclk_ip_lhm_axi_p_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_ITP, NULL, cmucal_vclk_ip_sysreg_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ITP_CMU_ITP, NULL, cmucal_vclk_ip_itp_cmu_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf0_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF3_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf3_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF2_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf2_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ITP, NULL, cmucal_vclk_ip_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_ITP, NULL, cmucal_vclk_ip_d_tzpc_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF1_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf1_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_ITP, NULL, cmucal_vclk_ip_ad_apb_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_CTL_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_ctl_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_CTL_ITPDNS, NULL, cmucal_vclk_ip_lhs_ast_ctl_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF_ITPMCSC, NULL, cmucal_vclk_ip_lhs_ast_otf_itpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF_TNRITP, NULL, cmucal_vclk_ip_lhm_ast_otf_tnritp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF0_ITPDNS, NULL, cmucal_vclk_ip_lhs_ast_otf0_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF3_ITPDNS, NULL, cmucal_vclk_ip_lhs_ast_otf3_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCSC_CMU_MCSC, NULL, cmucal_vclk_ip_mcsc_cmu_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MCSC, NULL, cmucal_vclk_ip_lhs_axi_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MCSC, NULL, cmucal_vclk_ip_lhm_axi_p_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MCSC, NULL, cmucal_vclk_ip_sysreg_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCSC, NULL, cmucal_vclk_ip_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MCSC, NULL, cmucal_vclk_ip_ppmu_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_MCSC, NULL, cmucal_vclk_ip_sysmmu_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MCSC, NULL, cmucal_vclk_ip_d_tzpc_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_MCSC, NULL, cmucal_vclk_ip_vgen_lite_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_MCSC, NULL, cmucal_vclk_ip_ad_apb_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_GDC, NULL, cmucal_vclk_ip_ppmu_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GDC, NULL, cmucal_vclk_ip_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_MCSC, NULL, cmucal_vclk_ip_sysmmu_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MCSC, NULL, cmucal_vclk_ip_lhs_axi_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF_ITPMCSC, NULL, cmucal_vclk_ip_lhm_ast_otf_itpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_VO_MCSCCSIS, NULL, cmucal_vclk_ip_lhs_ast_vo_mcsccsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_GDC, NULL, cmucal_vclk_ip_ad_apb_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_C2AGENT_D0_MCSC, NULL, cmucal_vclk_ip_c2agent_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_VO_TNRMCSC, NULL, cmucal_vclk_ip_lhm_ast_vo_tnrmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_INT_GDCMCSC, NULL, cmucal_vclk_ip_lhs_ast_int_gdcmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_INT_GDCMCSC, NULL, cmucal_vclk_ip_lhm_ast_int_gdcmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_C2AGENT_D1_MCSC, NULL, cmucal_vclk_ip_c2agent_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_C2AGENT_D2_MCSC, NULL, cmucal_vclk_ip_c2agent_d2_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC_CMU_MFC, NULL, cmucal_vclk_ip_mfc_cmu_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_MFC, NULL, cmucal_vclk_ip_as_apb_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MFC, NULL, cmucal_vclk_ip_sysreg_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MFC, NULL, cmucal_vclk_ip_lhs_axi_d0_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MFC, NULL, cmucal_vclk_ip_lhs_axi_d1_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MFC, NULL, cmucal_vclk_ip_lhm_axi_p_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFCD0, NULL, cmucal_vclk_ip_sysmmu_mfcd0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFCD1, NULL, cmucal_vclk_ip_sysmmu_mfcd1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFCD0, NULL, cmucal_vclk_ip_ppmu_mfcd0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFCD1, NULL, cmucal_vclk_ip_ppmu_mfcd1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_AXI_WFD, NULL, cmucal_vclk_ip_as_axi_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_WFD, NULL, cmucal_vclk_ip_ppmu_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_MFC, NULL, cmucal_vclk_ip_xiu_d_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_MFC, NULL, cmucal_vclk_ip_vgen_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC, NULL, cmucal_vclk_ip_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WFD, NULL, cmucal_vclk_ip_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MFC, NULL, cmucal_vclk_ip_lh_atb_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MFC, NULL, cmucal_vclk_ip_d_tzpc_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_WFD_NS, NULL, cmucal_vclk_ip_as_apb_wfd_ns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIF_CMU_MIF, NULL, cmucal_vclk_ip_mif_cmu_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDRPHY, NULL, cmucal_vclk_ip_ddrphy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MIF, NULL, cmucal_vclk_ip_sysreg_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MIF, NULL, cmucal_vclk_ip_lhm_axi_p_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DMC_CPU, NULL, cmucal_vclk_ip_ppmu_dmc_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_DMC_CPU, NULL, cmucal_vclk_ip_qe_dmc_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMC, NULL, cmucal_vclk_ip_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MIF, NULL, cmucal_vclk_ip_d_tzpc_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DDRPHY, NULL, cmucal_vclk_ip_sfrapb_bridge_ddrphy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC_PF, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc_pf, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC_SECURE, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc_secure, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC_PPMPU, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc_ppmpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF_CPU, NULL, cmucal_vclk_ip_lhm_axi_d_mif_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF_RT, NULL, cmucal_vclk_ip_lhm_axi_d_mif_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF_NRT, NULL, cmucal_vclk_ip_lhm_axi_d_mif_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MIF_CP, NULL, cmucal_vclk_ip_lhm_axi_d_mif_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MODEM_CMU_MODEM, NULL, cmucal_vclk_ip_modem_cmu_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPU_CMU_NPU, NULL, cmucal_vclk_ip_npu_cmu_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPUD_UNIT1, NULL, cmucal_vclk_ip_npud_unit1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_NPU, NULL, cmucal_vclk_ip_d_tzpc_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPU_UNIT0_SETREG, NULL, cmucal_vclk_ip_lhm_ast_d_npu_unit0_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D_NPU_UNIT1_SETREG, NULL, cmucal_vclk_ip_lhm_ast_d_npu_unit1_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPU_UNIT0_DONE, NULL, cmucal_vclk_ip_lhs_ast_d_npu_unit0_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPUD_UNIT0, NULL, cmucal_vclk_ip_npud_unit0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_UNIT0, NULL, cmucal_vclk_ip_ppmu_unit0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_NPU, NULL, cmucal_vclk_ip_sysreg_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_UNIT1, NULL, cmucal_vclk_ip_ppmu_unit1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_NPU, NULL, cmucal_vclk_ip_lhm_axi_p_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D_NPU_UNIT1_DONE, NULL, cmucal_vclk_ip_lhs_ast_d_npu_unit1_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D0_NPU, NULL, cmucal_vclk_ip_lhm_ast_d0_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D10_NPU, NULL, cmucal_vclk_ip_lhm_ast_d10_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D11_NPU, NULL, cmucal_vclk_ip_lhm_ast_d11_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D12_NPU, NULL, cmucal_vclk_ip_lhm_ast_d12_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D13_NPU, NULL, cmucal_vclk_ip_lhm_ast_d13_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D14_NPU, NULL, cmucal_vclk_ip_lhm_ast_d14_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D15_NPU, NULL, cmucal_vclk_ip_lhm_ast_d15_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D1_NPU, NULL, cmucal_vclk_ip_lhm_ast_d1_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D2_NPU, NULL, cmucal_vclk_ip_lhm_ast_d2_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D3_NPU, NULL, cmucal_vclk_ip_lhm_ast_d3_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D4_NPU, NULL, cmucal_vclk_ip_lhm_ast_d4_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D5_NPU, NULL, cmucal_vclk_ip_lhm_ast_d5_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D6_NPU, NULL, cmucal_vclk_ip_lhm_ast_d6_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D7_NPU, NULL, cmucal_vclk_ip_lhm_ast_d7_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D8_NPU, NULL, cmucal_vclk_ip_lhm_ast_d8_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_D9_NPU, NULL, cmucal_vclk_ip_lhm_ast_d9_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D0_NPU, NULL, cmucal_vclk_ip_lhs_ast_d0_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D10_NPU, NULL, cmucal_vclk_ip_lhs_ast_d10_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D11_NPU, NULL, cmucal_vclk_ip_lhs_ast_d11_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D12_NPU, NULL, cmucal_vclk_ip_lhs_ast_d12_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D13_NPU, NULL, cmucal_vclk_ip_lhs_ast_d13_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D14_NPU, NULL, cmucal_vclk_ip_lhs_ast_d14_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D15_NPU, NULL, cmucal_vclk_ip_lhs_ast_d15_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D1_NPU, NULL, cmucal_vclk_ip_lhs_ast_d1_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D2_NPU, NULL, cmucal_vclk_ip_lhs_ast_d2_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D3_NPU, NULL, cmucal_vclk_ip_lhs_ast_d3_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D4_NPU, NULL, cmucal_vclk_ip_lhs_ast_d4_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D5_NPU, NULL, cmucal_vclk_ip_lhs_ast_d5_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D6_NPU, NULL, cmucal_vclk_ip_lhs_ast_d6_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D7_NPU, NULL, cmucal_vclk_ip_lhs_ast_d7_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D8_NPU, NULL, cmucal_vclk_ip_lhs_ast_d8_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_D9_NPU, NULL, cmucal_vclk_ip_lhs_ast_d9_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DNCNPU_DMA, NULL, cmucal_vclk_ip_lhm_axi_d_dncnpu_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPU1_CMU_NPU1, NULL, cmucal_vclk_ip_npu1_cmu_npu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERI, NULL, cmucal_vclk_ip_gpio_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERI, NULL, cmucal_vclk_ip_sysreg_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERI_CMU_PERI, NULL, cmucal_vclk_ip_peri_cmu_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_PERI, NULL, cmucal_vclk_ip_lhm_axi_p_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERI, NULL, cmucal_vclk_ip_d_tzpc_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_PERI, NULL, cmucal_vclk_ip_xiu_p_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCT, NULL, cmucal_vclk_ip_mct, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_TOP, NULL, cmucal_vclk_ip_otp_con_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT0, NULL, cmucal_vclk_ip_wdt0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT1, NULL, cmucal_vclk_ip_wdt1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TMU, NULL, cmucal_vclk_ip_tmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PWM, NULL, cmucal_vclk_ip_pwm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BC_EMUL, NULL, cmucal_vclk_ip_bc_emul, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UART_DBG, NULL, cmucal_vclk_ip_uart_dbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI00_USI, NULL, cmucal_vclk_ip_usi00_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI00_I2C, NULL, cmucal_vclk_ip_usi00_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI01_USI, NULL, cmucal_vclk_ip_usi01_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI01_I2C, NULL, cmucal_vclk_ip_usi01_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI02_USI, NULL, cmucal_vclk_ip_usi02_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI02_I2C, NULL, cmucal_vclk_ip_usi02_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI03_USI, NULL, cmucal_vclk_ip_usi03_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI03_I2C, NULL, cmucal_vclk_ip_usi03_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI04_USI, NULL, cmucal_vclk_ip_usi04_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI04_I2C, NULL, cmucal_vclk_ip_usi04_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI05_USI, NULL, cmucal_vclk_ip_usi05_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI05_I2C, NULL, cmucal_vclk_ip_usi05_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_BIRA, NULL, cmucal_vclk_ip_otp_con_bira, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CSISPERI, NULL, cmucal_vclk_ip_lhm_axi_p_csisperi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_OIS, NULL, cmucal_vclk_ip_spi_ois, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_OIS, NULL, cmucal_vclk_ip_i2c_ois, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_MMCCARD, NULL, cmucal_vclk_ip_gpio_mmccard, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_PERI, NULL, cmucal_vclk_ip_vgen_lite_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_S2MPU_D_PERI, NULL, cmucal_vclk_ip_s2mpu_d_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MMC_CARD, NULL, cmucal_vclk_ip_mmc_card, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_PERI, NULL, cmucal_vclk_ip_ppmu_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_PERI, NULL, cmucal_vclk_ip_lhs_acel_d_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_S2D_CMU_S2D, NULL, cmucal_vclk_ip_s2d_cmu_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BIS_S2D, NULL, cmucal_vclk_ip_bis_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_SCAN2DRAM, NULL, cmucal_vclk_ip_lhm_axi_g_scan2dram, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SSP_CMU_SSP, NULL, cmucal_vclk_ip_ssp_cmu_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_SSP, NULL, cmucal_vclk_ip_lhm_axi_p_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_SSP, NULL, cmucal_vclk_ip_d_tzpc_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_SSPCTRL, NULL, cmucal_vclk_ip_sysreg_sspctrl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USS_SSPCORE, NULL, cmucal_vclk_ip_uss_sspcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TNR_CMU_TNR, NULL, cmucal_vclk_ip_tnr_cmu_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_TNR, NULL, cmucal_vclk_ip_apb_async_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_TNR, NULL, cmucal_vclk_ip_d_tzpc_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_TNR, NULL, cmucal_vclk_ip_lhm_axi_p_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_VO_DNSTNR, NULL, cmucal_vclk_ip_lhm_ast_vo_dnstnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_TNR, NULL, cmucal_vclk_ip_sysreg_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_TNR, NULL, cmucal_vclk_ip_vgen_lite_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TNR, NULL, cmucal_vclk_ip_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0_TNR, NULL, cmucal_vclk_ip_ppmu_d0_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_TNR, NULL, cmucal_vclk_ip_sysmmu_d0_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_TNR, NULL, cmucal_vclk_ip_lhs_axi_d0_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF_TNRDNS, NULL, cmucal_vclk_ip_lhs_ast_otf_tnrdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1_TNR, NULL, cmucal_vclk_ip_ppmu_d1_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF_TNRITP, NULL, cmucal_vclk_ip_lhs_ast_otf_tnritp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ORBMCH, NULL, cmucal_vclk_ip_orbmch, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_VO_TNRMCSC, NULL, cmucal_vclk_ip_lhs_ast_vo_tnrmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_TNR, NULL, cmucal_vclk_ip_lhs_axi_d1_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_TNR, NULL, cmucal_vclk_ip_sysmmu_d1_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_TNR, NULL, cmucal_vclk_ip_xiu_d1_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USB31DRD, NULL, cmucal_vclk_ip_usb31drd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DP_LINK, NULL, cmucal_vclk_ip_dp_link, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_USB, NULL, cmucal_vclk_ip_ppmu_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_USB, NULL, cmucal_vclk_ip_lhs_acel_d_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_USB, NULL, cmucal_vclk_ip_vgen_lite_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_USB, NULL, cmucal_vclk_ip_d_tzpc_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_USB, NULL, cmucal_vclk_ip_lhm_axi_p_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_S2MPU_D_USB, NULL, cmucal_vclk_ip_s2mpu_d_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_USB, NULL, cmucal_vclk_ip_sysreg_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USB_CMU_USB, NULL, cmucal_vclk_ip_usb_cmu_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_US_D_USB, NULL, cmucal_vclk_ip_us_d_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_USB, NULL, cmucal_vclk_ip_xiu_d_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VRA_CMU_VRA, NULL, cmucal_vclk_ip_vra_cmu_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CLAHE, NULL, cmucal_vclk_ip_clahe, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_VRA, NULL, cmucal_vclk_ip_d_tzpc_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_VRA, NULL, cmucal_vclk_ip_lhm_axi_p_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_VRA, NULL, cmucal_vclk_ip_lhs_axi_d_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0_CLAHE, NULL, cmucal_vclk_ip_ppmu_d0_clahe, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1_CLAHE, NULL, cmucal_vclk_ip_ppmu_d1_clahe, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_VRA, NULL, cmucal_vclk_ip_ppmu_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_VRA, NULL, cmucal_vclk_ip_sysmmu_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VRA, NULL, cmucal_vclk_ip_sysreg_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_VRA, NULL, cmucal_vclk_ip_vgen_lite_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VRA, NULL, cmucal_vclk_ip_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_VRA, NULL, cmucal_vclk_ip_xiu_d_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_VRA, NULL, cmucal_vclk_ip_ad_apb_vra, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_CLAHE, NULL, cmucal_vclk_ip_ad_apb_clahe, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_D0_CLAHE, NULL, cmucal_vclk_ip_ad_axi_d0_clahe, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AHB_BUSMATRIX, NULL, cmucal_vclk_ip_ahb_busmatrix, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF, NULL, cmucal_vclk_ip_dmic_if, NULL, NULL),
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
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_VTS, NULL, cmucal_vclk_ip_mailbox_ap_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER, NULL, cmucal_vclk_ip_timer, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF_3RD, NULL, cmucal_vclk_ip_dmic_if_3rd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_U_DMIC_CLK_SCAN_MUX, NULL, cmucal_vclk_ip_u_dmic_clk_scan_mux, NULL, NULL),
};
