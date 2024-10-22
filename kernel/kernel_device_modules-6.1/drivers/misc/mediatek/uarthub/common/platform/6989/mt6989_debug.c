// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/regmap.h>

#include "uarthub_drv_core.h"
#include "uarthub_drv_export.h"
#include "common_def_id.h"
#include "inc/mt6989.h"
#include "inc/mt6989_debug.h"
#if UARTHUB_SUPPORT_DVT
#include "ut/mt6989_ut_test.h"
#endif

static int uarthub_read_dbg_monitor(int *sel, int *tx_monitor, int *rx_monitor);
#if !(UARTHUB_SUPPORT_FPGA)
static int uarthub_get_peri_uart_pad_mode_mt6989(void);
static int uarthub_get_gpio_trx_info_mt6989(struct uarthub_gpio_trx_info *info);
#endif

static int uarthub_dump_debug_monitor_packet_info_mode_mt6989(const char *tag);
static int uarthub_dump_debug_monitor_check_data_mode_mt6989(const char *tag);
static int uarthub_dump_debug_monitor_crc_result_mode_mt6989(const char *tag);

static int uarthub_record_check_data_mode_sta_to_buffer(
	unsigned char *dmp_info_buf, int len,
	int debug_monitor_sel,
	int *tx_monitor, int *rx_monitor,
	int tx_monitor_pointer, int rx_monitor_pointer,
	int check_data_mode_sel);

static int uarthub_record_intfhub_fsm_sta_to_buffer(
	unsigned char *dmp_info_buf, int len, int fsm_dbg_sta);

struct uarthub_debug_ops_struct mt6989_plat_debug_data = {
	.uarthub_plat_get_intfhub_base_addr = uarthub_get_intfhub_base_addr_mt6989,
	.uarthub_plat_get_uartip_base_addr = uarthub_get_uartip_base_addr_mt6989,
	.uarthub_plat_dump_uartip_debug_info = uarthub_dump_uartip_debug_info_mt6989,
	.uarthub_plat_dump_intfhub_debug_info = uarthub_dump_intfhub_debug_info_mt6989,
	.uarthub_plat_dump_debug_monitor = uarthub_dump_debug_monitor_mt6989,
	.uarthub_plat_debug_monitor_ctrl = uarthub_debug_monitor_ctrl_mt6989,
	.uarthub_plat_debug_monitor_stop = uarthub_debug_monitor_stop_mt6989,
	.uarthub_plat_debug_monitor_clr = uarthub_debug_monitor_clr_mt6989,
	.uarthub_plat_dump_debug_tx_rx_count = uarthub_dump_debug_tx_rx_count_mt6989,
	.uarthub_plat_dump_debug_clk_info = uarthub_dump_debug_clk_info_mt6989,
	.uarthub_plat_dump_debug_byte_cnt_info = uarthub_dump_debug_byte_cnt_info_mt6989,
	.uarthub_plat_dump_debug_apdma_uart_info = uarthub_dump_debug_apdma_uart_info_mt6989,
	.uarthub_plat_dump_sspm_log = uarthub_dump_sspm_log_mt6989,
	.uarthub_plat_trigger_fpga_testing = uarthub_trigger_fpga_testing_mt6989,
	.uarthub_plat_trigger_dvt_testing = uarthub_trigger_dvt_testing_mt6989,
#if UARTHUB_SUPPORT_DVT
	.uarthub_plat_verify_combo_connect_sta = uarthub_verify_combo_connect_sta_mt6989,
#endif
};

int uarthub_get_uart_mux_info_mt6989(void)
{
	if (!topckgen_base_remap_addr_mt6989) {
		pr_notice("[%s] topckgen_base_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(topckgen_base_remap_addr_mt6989 + CLK_CFG_6,
		CLK_CFG_6_UART_SEL_MASK) >> CLK_CFG_6_UART_SEL_SHIFT);
}

int uarthub_get_uarthub_mux_info_mt6989(void)
{
	if (!topckgen_base_remap_addr_mt6989) {
		pr_notice("[%s] topckgen_base_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	return UARTHUB_REG_READ_BIT(topckgen_base_remap_addr_mt6989 + CLK_CFG_16,
		CLK_CFG_16_UARTHUB_BCLK_SEL_MASK);
}

#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_uarthub_cg_info_mt6989(int *p_topckgen_cg, int *p_peri_cg)
{
	int topckgen_cg = 0, peri_cg = 0;

	if (!pericfg_ao_remap_addr_mt6989) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	if (!topckgen_base_remap_addr_mt6989) {
		pr_notice("[%s] topckgen_base_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	topckgen_cg = (UARTHUB_REG_READ_BIT(topckgen_base_remap_addr_mt6989 + CLK_CFG_16,
		CLK_CFG_16_PDN_UARTHUB_BCLK_MASK) >> CLK_CFG_16_PDN_UARTHUB_BCLK_SHIFT);

	peri_cg = (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6989 + PERI_CG_1,
		PERI_CG_1_UARTHUB_CG_MASK) >> PERI_CG_1_UARTHUB_CG_SHIFT);

	if (p_topckgen_cg)
		*p_topckgen_cg = topckgen_cg;

	if (p_peri_cg)
		*p_peri_cg = peri_cg;

	return 0;
}
#endif

#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_peri_uart_pad_mode_mt6989(void)
{
	if (!pericfg_ao_remap_addr_mt6989) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	/* 1: UART_PAD mode */
	/* 0: UARTHUB mode */
	return (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6989 + PERI_UART_WAKEUP,
		PERI_UART_WAKEUP_UART_GPHUB_SEL_MASK) >> PERI_UART_WAKEUP_UART_GPHUB_SEL_SHIFT);
}
#endif

#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_uart_src_clk_info_mt6989(void)
{
	if (!pericfg_ao_remap_addr_mt6989) {
		pr_notice("[%s] pericfg_ao_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	return (UARTHUB_REG_READ_BIT(pericfg_ao_remap_addr_mt6989 + PERI_CLOCK_CON,
		PERI_UART_FBCLK_CKSEL_MASK) >> PERI_UART_FBCLK_CKSEL_SHIFT);
}
#endif

#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_spm_res_info_mt6989(
	int *p_spm_res_uarthub, int *p_spm_res_internal, int *p_spm_res_26m_off)
{
	int spm_res_uarthub = 0, spm_res_internal = 0, spm_res_26m_off = 0;

	if (!spm_remap_addr_mt6989) {
		pr_notice("[%s] spm_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	/* SPM_REQ_STA_11[7] = UART_HUB_VRF18_REQ */
	/* SPM_REQ_STA_11[6] = UART_HUB_VCORE_REQ */
	/* SPM_REQ_STA_11[5] = UART_HUB_SRCCLKENA */
	/* SPM_REQ_STA_11[3] = UART_HUB_INFRA_REQ */
	spm_res_uarthub = (UARTHUB_REG_READ_BIT(
		spm_remap_addr_mt6989 + SPM_REQ_STA_11,
		SPM_REQ_STA_11_UARTHUB_REQ_MASK) >>
		SPM_REQ_STA_11_UARTHUB_REQ_SHIFT);

	if (p_spm_res_uarthub)
		*p_spm_res_uarthub = spm_res_uarthub;

#if SPM_RES_CHK_EN
	if (spm_res_uarthub != 0x1D)
		return 0;
#endif

	spm_res_internal = UARTHUB_REG_READ(spm_remap_addr_mt6989 + MD32PCM_SCU_CTRL1);
	spm_res_26m_off = UARTHUB_REG_READ(spm_remap_addr_mt6989 + MD32PCM_SCU_CTRL0);

	// SPM res is not loaded.
	if (spm_res_internal == 0 && spm_res_26m_off == 0)
		return 2;

	/* MD32PCM_SCU_CTRL1[21] = spm_pmic_internal_ack */
	/* MD32PCM_SCU_CTRL1[19] = spm_vcore_internal_ack */
	/* MD32PCM_SCU_CTRL1[18] = spm_vrf18_internal_ack */
	/* MD32PCM_SCU_CTRL1[17] = spm_infra_internal_ack */
	spm_res_internal = ((spm_res_internal & MD32PCM_SCU_CTRL1_SPM_HUB_INTL_ACK_MASK) >>
		MD32PCM_SCU_CTRL1_SPM_HUB_INTL_ACK_SHIFT);

	/* MD32PCM_SCU_CTRL0[5] = MD26M_CK_OFF */
	spm_res_26m_off = ((spm_res_26m_off & MD32PCM_SCU_CTRL0_SC_MD26M_CK_OFF_MASK) >>
		MD32PCM_SCU_CTRL0_SC_MD26M_CK_OFF_SHIFT);

	if (p_spm_res_internal)
		*p_spm_res_internal = spm_res_internal;

	if (p_spm_res_26m_off)
		*p_spm_res_26m_off = spm_res_26m_off;

#if SPM_RES_CHK_EN
	if (spm_res_internal != 0x17 || spm_res_26m_off != 0x0)
		return 0;
#endif

	return 1;
}
#endif

#if !(UARTHUB_SUPPORT_FPGA)
int uarthub_get_gpio_trx_info_mt6989(struct uarthub_gpio_trx_info *info)
{
	if (!info) {
		pr_notice("[%s] info is NULL\n", __func__);
		return -1;
	}

	if (!gpio_base_remap_addr_mt6989) {
		pr_notice("[%s] gpio_base_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	if (!iocfg_rm_remap_addr_mt6989) {
		pr_notice("[%s] iocfg_rm_remap_addr_mt6989 is NULL\n", __func__);
		return -1;
	}

	UARTHUB_READ_GPIO(info->tx_mode, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6989,
		GPIO_HUB_MODE_TX, GPIO_HUB_MODE_TX_MASK, GPIO_HUB_MODE_TX_VALUE);
	UARTHUB_READ_GPIO(info->rx_mode, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6989,
		GPIO_HUB_MODE_RX, GPIO_HUB_MODE_RX_MASK, GPIO_HUB_MODE_RX_VALUE);
	UARTHUB_READ_GPIO_BIT(info->tx_dir, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6989,
		GPIO_HUB_DIR_TX, GPIO_HUB_DIR_TX_MASK, GPIO_HUB_DIR_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_dir, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6989,
		GPIO_HUB_DIR_RX, GPIO_HUB_DIR_RX_MASK, GPIO_HUB_DIR_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_ies, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_IES_TX, GPIO_HUB_IES_TX_MASK, GPIO_HUB_IES_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_ies, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_IES_RX, GPIO_HUB_IES_RX_MASK, GPIO_HUB_IES_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_pu, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_PU_TX, GPIO_HUB_PU_TX_MASK, GPIO_HUB_PU_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_pu, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_PU_RX, GPIO_HUB_PU_RX_MASK, GPIO_HUB_PU_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_pd, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_PD_TX, GPIO_HUB_PD_TX_MASK, GPIO_HUB_PD_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_pd, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_PD_RX, GPIO_HUB_PD_RX_MASK, GPIO_HUB_PD_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_drv, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_DRV_TX, GPIO_HUB_DRV_TX_MASK, GPIO_HUB_DRV_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_drv, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_DRV_RX, GPIO_HUB_DRV_RX_MASK, GPIO_HUB_DRV_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_smt, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_SMT_TX, GPIO_HUB_SMT_TX_MASK, GPIO_HUB_SMT_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_smt, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_SMT_RX, GPIO_HUB_SMT_RX_MASK, GPIO_HUB_SMT_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_tdsel, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_TDSEL_TX, GPIO_HUB_TDSEL_TX_MASK, GPIO_HUB_TDSEL_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_tdsel, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_TDSEL_RX, GPIO_HUB_TDSEL_RX_MASK, GPIO_HUB_TDSEL_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_rdsel, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_RDSEL_TX, GPIO_HUB_RDSEL_TX_MASK, GPIO_HUB_RDSEL_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_rdsel, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_RDSEL_RX, GPIO_HUB_RDSEL_RX_MASK, GPIO_HUB_RDSEL_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->tx_sec_en, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_SEC_EN_TX, GPIO_HUB_SEC_EN_TX_MASK, GPIO_HUB_SEC_EN_TX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_sec_en, IOCFG_RM_BASE_ADDR, iocfg_rm_remap_addr_mt6989,
		GPIO_HUB_SEC_EN_RX, GPIO_HUB_SEC_EN_RX_MASK, GPIO_HUB_SEC_EN_RX_SHIFT);
	UARTHUB_READ_GPIO_BIT(info->rx_din, GPIO_BASE_ADDR, gpio_base_remap_addr_mt6989,
		GPIO_HUB_DIN_RX, GPIO_HUB_DIN_RX_MASK, GPIO_HUB_DIN_RX_SHIFT);

	return 0;
}
#endif

int uarthub_get_intfhub_base_addr_mt6989(void)
{
	return UARTHUB_INTFHUB_BASE_ADDR;
}

int uarthub_get_uartip_base_addr_mt6989(int dev_index)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		return UARTHUB_DEV_0_BASE_ADDR;
	else if (dev_index == 1)
		return UARTHUB_DEV_1_BASE_ADDR;
	else if (dev_index == 2)
		return UARTHUB_DEV_2_BASE_ADDR;

	return UARTHUB_CMM_BASE_ADDR;
}

int uarthub_dump_uartip_debug_info_mt6989(
	const char *tag, struct mutex *uartip_lock)
{
	const char *def_tag = "HUB_DBG_UIP";
	int print_ap = 0;
	char dmp_info_buf[DBG_LOG_LEN] = {'\0'};

	if (!uartip_lock)
		pr_notice("[%s] uartip_lock is NULL\n", __func__);

	if (uartip_lock) {
		if (mutex_lock_killable(uartip_lock)) {
			pr_notice("[%s] mutex_lock_killable(uartip_lock) fail\n", __func__);
			return UARTHUB_ERR_MUTEX_LOCK_FAIL;
		}
	}

	if (apuart_base_map[3] != NULL)
		print_ap = 1;

	UARTHUB_DEBUG_PRINT_OP_RX_REQ(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_IP_TX_DMA(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_RX_WOFFSET(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_TX_WOFFSET(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_TX_ROFFSET(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_ROFFSET_DMA(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_XCSTATE_WSEND_XOFF(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_SWTXDIS_DET_XOFF(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_FEATURE_SEL(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_HIGHSPEEND(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_DLL(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_SAMPLE_CNT(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_SAMPLE_PT(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_FRACDIV_L(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_FRACDIV_M(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_DMA_EN(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_IIR(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_LCR(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_EFR(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_XON1(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_XOFF1(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_XON2(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_XOFF2(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_ESCAPE_EN(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_ESCAPE_DAT(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_RXTRI_AD(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_FCR_RD(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_MCR(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_TX_OFFSET_DMA(def_tag, tag, print_ap, 0);
	UARTHUB_DEBUG_PRINT_LSR(def_tag, tag, print_ap, 1);

	if (uartip_lock)
		mutex_unlock(uartip_lock);

	return 0;
}

int uarthub_dump_intfhub_debug_info_mt6989(const char *tag)
{
	int val = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int ret = 0;
	const char *def_tag = "HUB_DBG";
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;

#if !(UARTHUB_SUPPORT_FPGA)
	int topckgen_cg = 0, peri_cg = 0;
	int spm_res_uarthub = 0, spm_res_internal = 0, spm_res_26m_off = 0;
	struct uarthub_gpio_trx_info gpio_base_addr;

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s][%s][%s] IDBG=[0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag), MT6989_UARTHUB_DUMP_VERSION, val);
	if (ret > 0)
		len += ret;

	val = uarthub_is_apb_bus_clk_enable_mt6989();
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",APB_BUS=[%d]", val);
	if (ret > 0)
		len += ret;

	if (val == 0) {
		pr_info("%s\n", dmp_info_buf);
		return 0;
	}

	val = uarthub_get_uarthub_cg_info_mt6989(&topckgen_cg, &peri_cg);
	if (val >= 0) {
		/* the expect value is 0x0 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HUB_CG=[topck:0x%x,peri:0x%x]", topckgen_cg, peri_cg);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uart_src_clk_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UART_SRC_CLK=[0x%x(%s)]", val, ((val == 0) ? "26M" : "TOPCK"));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_peri_uart_pad_mode_mt6989();
	if (val >= 0) {
		/* the expect value is 0x0 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UART_MODE=[0x%x(%s)]", val, ((val == 0) ? "HUB" : "UART_PAD"));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_spm_res_info_mt6989(
		&spm_res_uarthub, &spm_res_internal, &spm_res_26m_off);
	if (val == 1) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[1]");
		if (ret > 0)
			len += ret;
	} else if (val == 0 || val == 2) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[%d(0x%x/0x%x/0x%x,exp:0x1D/0x17/0x0)]",
			val, spm_res_uarthub, spm_res_internal, spm_res_26m_off);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_hwccf_univpll_on_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UNIVPLL=[%d]", val);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uart_mux_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x2 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UART_MUX=[0x%x(%s)]", val,
			((val == 0) ? "26M" : ((val == 1) ? "52M" :
				((val == 2) ? "104M" : "208M"))));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uarthub_mux_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HUB_MUX=[0x%x(%s)]", val,
			((val == 0) ? "26M" : ((val == 1) ? "104M" : "208M")));
		if (ret > 0)
			len += ret;
	}

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	val = uarthub_get_gpio_trx_info_mt6989(&gpio_base_addr);
	if (val == 0) {
		ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
			"[%s][%s] GPIO MODE=[T:0x%x,R:0x%x]",
			def_tag, ((tag == NULL) ? "null" : tag),
			gpio_base_addr.tx_mode.gpio_value,
			gpio_base_addr.rx_mode.gpio_value);
		if (ret > 0)
			len += ret;
	}

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ILPBACK(0xe4)=[0x%x]", UARTHUB_REG_READ(LOOPBACK_ADDR));
	if (ret > 0)
		len += ret;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG(0xf4)=[0x%x]", UARTHUB_REG_READ(DBG_CTRL_ADDR));
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
		"[%s][%s] IDEVx_STA(0x0/0x40/0x80)=[0x%x-0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;
#else
	len = 0;
	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
		"[%s][%s][%s] IDEVx_STA(0x0/0x40/0x80)=[0x%x-0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag), MT6989_UARTHUB_DUMP_VERSION,
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;
#endif

	dev0_sta = UARTHUB_REG_READ(DEV0_PKT_CNT_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_PKT_CNT_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_PKT_CNT_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDEVx_PKT_CNT(0x1c/0x50/0x90)=[0x%x-0x%x-0x%x]",
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",PKT_CNT=[R:%d-%d-%d,T:%d-%d-%d]",
		((dev0_sta & REG_FLD_MASK(DEV0_PKT_CNT_FLD_dev0_rx_pkt_cnt)) >>
			REG_FLD_SHIFT(DEV0_PKT_CNT_FLD_dev0_rx_pkt_cnt)),
		((dev1_sta & REG_FLD_MASK(DEV1_PKT_CNT_FLD_dev1_rx_pkt_cnt)) >>
			REG_FLD_SHIFT(DEV1_PKT_CNT_FLD_dev1_rx_pkt_cnt)),
		((dev2_sta & REG_FLD_MASK(DEV2_PKT_CNT_FLD_dev2_rx_pkt_cnt)) >>
			REG_FLD_SHIFT(DEV2_PKT_CNT_FLD_dev2_rx_pkt_cnt)),
		((dev0_sta & REG_FLD_MASK(DEV0_PKT_CNT_FLD_dev0_tx_timeout_cnt)) >>
			REG_FLD_SHIFT(DEV0_PKT_CNT_FLD_dev0_tx_timeout_cnt)),
		((dev1_sta & REG_FLD_MASK(DEV1_PKT_CNT_FLD_dev1_tx_timeout_cnt)) >>
			REG_FLD_SHIFT(DEV1_PKT_CNT_FLD_dev1_tx_timeout_cnt)),
		((dev2_sta & REG_FLD_MASK(DEV2_PKT_CNT_FLD_dev2_tx_timeout_cnt)) >>
			REG_FLD_SHIFT(DEV2_PKT_CNT_FLD_dev2_tx_timeout_cnt)));
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	dev0_sta = UARTHUB_REG_READ(DBG_STATE_ADDR);
	ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
		"[%s][%s] IDBG_STA=[0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag), dev0_sta);
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_intfhub_ctrl_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_intfhub_ctrl_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",intfhub_fsm=[%s]",
		((val == 0) ? "IDLE" : ((val == 1) ? "PREPARE" :
			((val == 2) ? "READY" : ((val == 3) ? "CKON" :
			((val == 4) ? "CKOFF" : "WAIT"))))));
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_tx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_tx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",tx_fsm=[%s]",
		((val == 0) ? "IDLE" : ((val == 1) ? "HEADER" :
			((val == 2) ? "PAYLOAD" : ((val == 3) ? "CRC" :
			((val == 4) ? "ESP" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",rx_fsm=[%s]",
		((val == 0) ? "IDLE" : ((val == 1) ? "DEC" :
			((val == 2) ? "DEV0" : ((val == 3) ? "DEV1" :
			((val == 4) ? "DEV2" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_dev0_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_dev0_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",dev0_rx_fsm=[%s]",
		((val == 0) ? "IDLE" : ((val == 1) ? "HEADER" :
			((val == 2) ? "PAYLOAD" : ((val == 3) ? "CRC" :
			((val == 4) ? "ESP" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_dev1_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_dev1_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",dev1_rx_fsm=[%s]",
		((val == 0) ? "IDLE" : ((val == 1) ? "HEADER" :
			((val == 2) ? "PAYLOAD" : ((val == 3) ? "CRC" :
			((val == 4) ? "ESP" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_dev2_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_dev2_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",dev2_rx_fsm=[%s]",
		((val == 0) ? "IDLE" : ((val == 1) ? "HEADER" :
			((val == 2) ? "PAYLOAD" : ((val == 3) ? "CRC" :
			((val == 4) ? "ESP" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((dev0_sta & REG_FLD_MASK(DBG_STATE_FLD_intfhub_dbg)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_intfhub_dbg));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",intfhub_dbg=[0x%x]", val);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	dev0_sta = UARTHUB_REG_READ(DEV0_CRC_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_CRC_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_CRC_STA_ADDR);
	ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
		"[%s][%s] IDEVx_CRC_STA(0x20/0x54/0x94)=[0x%x-0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;

	dev0_sta = UARTHUB_REG_READ(DEV0_RX_ERR_CRC_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_RX_ERR_CRC_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_RX_ERR_CRC_STA_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDEVx_RX_ERR_CRC_STA(0x10/0x14/0x18)=[0x%x-0x%x-0x%x]",
		dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	len = 0;
	ret = snprintf(dmp_info_buf, DBG_LOG_LEN,
		"[%s][%s] IDEV0_IRQ_STA/MASK(0x30/0x38)=[0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(DEV0_IRQ_STA_ADDR),
		UARTHUB_REG_READ(DEV0_IRQ_MASK_ADDR));
	if (ret > 0)
		len += ret;

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IIRQ_STA/MASK(0xd0/0xd8)=[0x%x-0x%x]",
		UARTHUB_REG_READ(IRQ_STA_ADDR),
		UARTHUB_REG_READ(IRQ_MASK_ADDR));
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0(0xe0)=[0x%x]", val);
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(CON2_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ICON2(0xc8)=[0x%x]", val);
	if (ret > 0)
		len += ret;

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_read_dbg_monitor(int *sel, int *tx_monitor, int *rx_monitor)
{
	if (sel == NULL || tx_monitor == NULL || rx_monitor == NULL) {
		pr_info("%s failed due to parameter is NULL\n", __func__);
		return -1;
	}

	if (DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR) == 0) {
		pr_info("%s intfhub_cg_en is 0\n", __func__);
		return -2;
	}

	*sel = DEBUG_MODE_CRTL_GET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR);
#if UARTHUB_DEBUG_LOG
	pr_info("%s sel = %d\n", __func__, *sel);
#endif

	tx_monitor[0] = DEBUG_TX_MOINTOR_0_GET_intfhub_debug_tx_monitor0(DEBUG_TX_MOINTOR_0_ADDR);
	tx_monitor[1] = DEBUG_TX_MOINTOR_1_GET_intfhub_debug_tx_monitor1(DEBUG_TX_MOINTOR_1_ADDR);
	tx_monitor[2] = DEBUG_TX_MOINTOR_2_GET_intfhub_debug_tx_monitor2(DEBUG_TX_MOINTOR_2_ADDR);
	tx_monitor[3] = DEBUG_TX_MOINTOR_3_GET_intfhub_debug_tx_monitor3(DEBUG_TX_MOINTOR_3_ADDR);

	rx_monitor[0] = DEBUG_RX_MOINTOR_0_GET_intfhub_debug_rx_monitor0(DEBUG_RX_MOINTOR_0_ADDR);
	rx_monitor[1] = DEBUG_RX_MOINTOR_1_GET_intfhub_debug_rx_monitor1(DEBUG_RX_MOINTOR_1_ADDR);
	rx_monitor[2] = DEBUG_RX_MOINTOR_2_GET_intfhub_debug_rx_monitor2(DEBUG_RX_MOINTOR_2_ADDR);
	rx_monitor[3] = DEBUG_RX_MOINTOR_3_GET_intfhub_debug_rx_monitor3(DEBUG_RX_MOINTOR_3_ADDR);

	return 0;
}

int uarthub_dump_debug_monitor_mt6989(const char *tag)
{
	uarthub_dump_debug_monitor_packet_info_mode_mt6989(tag);
	uarthub_dump_debug_monitor_check_data_mode_mt6989(tag);
	uarthub_dump_debug_monitor_crc_result_mode_mt6989(tag);

	return 0;
}

int uarthub_debug_monitor_ctrl_mt6989(int enable, int mode, int ctrl)
{
	if (mode < 0 || mode > 2) {
		pr_notice("[%s] not support mode(%d)\n", __func__, mode);
		return UARTHUB_ERR_PARA_WRONG;
	}

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(
		DEBUG_MODE_CRTL_ADDR, ((enable == 0) ? 0 : 1));

	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_sel(DEBUG_MODE_CRTL_ADDR, mode);

	if (mode == 0)
		DEBUG_MODE_CRTL_SET_packet_info_bypass_esp_pkt_en(
			DEBUG_MODE_CRTL_ADDR, ctrl);
	else if (mode == 1)
		DEBUG_MODE_CRTL_SET_check_data_mode_select(
			DEBUG_MODE_CRTL_ADDR, ctrl);
	else if (mode == 2)
		DEBUG_MODE_CRTL_SET_tx_monitor_display_rx_crc_data_en(
			DEBUG_MODE_CRTL_ADDR, ctrl);

	return 0;
}

int uarthub_debug_monitor_stop_mt6989(int stop)
{
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_stop(DEBUG_MODE_CRTL_ADDR, stop);
	return 0;
}

int uarthub_debug_monitor_clr_mt6989(void)
{
	DEBUG_MODE_CRTL_SET_intfhub_debug_monitor_clr(DEBUG_MODE_CRTL_ADDR, 1);
	return 0;
}

int uarthub_dump_debug_monitor_packet_info_mode_mt6989(const char *tag)
{
	int debug_monitor_sel = 0;
	const char *def_tag = "HUB_DBG_PKT_INF";
	int tx_monitor[4] = { 0 };
	int rx_monitor[4] = { 0 };

	if (uarthub_read_dbg_monitor(&debug_monitor_sel, tx_monitor, rx_monitor) < 0)
		return 0;

	if (debug_monitor_sel != 0x0)
		return 0;

	pr_info("[%s][%s] TIME_PRECISE=[0x%x],BYPASS_ESP=[%d],MON_PTR=[R:%d,T:%d]",
		def_tag, ((tag == NULL) ? "null" : tag),
		DEBUG_MODE_CRTL_GET_packet_info_mode_time_precise(DEBUG_MODE_CRTL_ADDR),
		DEBUG_MODE_CRTL_GET_packet_info_bypass_esp_pkt_en(DEBUG_MODE_CRTL_ADDR),
		DEBUG_MODE_CRTL_GET_packet_info_mode_rx_monitor_pointer(DEBUG_MODE_CRTL_ADDR),
		DEBUG_MODE_CRTL_GET_packet_info_mode_tx_monitor_pointer(DEBUG_MODE_CRTL_ADDR));

	UARTHUB_DEBUG_PRINT_DBG_MONITOR_PKT_INFO(def_tag, tag, "TX_MON[3:0]", tx_monitor);
	UARTHUB_DEBUG_PRINT_DBG_MONITOR_PKT_INFO(def_tag, tag, "RX_MON[3:0]", rx_monitor);

	return 0;
}

int uarthub_dump_debug_monitor_check_data_mode_mt6989(const char *tag)
{
	int debug_monitor_sel = 0;
	const char *def_tag = "HUB_DBG_CHK_DATA";
	int tx_monitor[4] = { 0 };
	int rx_monitor[4] = { 0 };

	if (uarthub_read_dbg_monitor(&debug_monitor_sel, tx_monitor, rx_monitor) < 0)
		return 0;

	if (debug_monitor_sel != 0x1)
		return 0;

	pr_info("[%s][%s] CHK_DATA_MON[0:15](%d)=[R(%d):%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X,T(%d):%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X]",
		def_tag, ((tag == NULL) ? "null" : tag),
		DEBUG_MODE_CRTL_GET_check_data_mode_select(DEBUG_MODE_CRTL_ADDR),
		DEBUG_MODE_CRTL_GET_check_data_mode_rx_monitor_pointer(DEBUG_MODE_CRTL_ADDR),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(rx_monitor[0]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(rx_monitor[1]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(rx_monitor[2]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(rx_monitor[3]),
		DEBUG_MODE_CRTL_GET_check_data_mode_tx_monitor_pointer(DEBUG_MODE_CRTL_ADDR),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(tx_monitor[0]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(tx_monitor[1]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(tx_monitor[2]),
		UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(tx_monitor[3]));

	return 0;
}

int uarthub_dump_debug_monitor_crc_result_mode_mt6989(const char *tag)
{
	int debug_monitor_sel = 0;
	int rx_crc_data_en = 0;
	const char *def_tag = "HUB_DBG_CRC_INF";
	int tx_monitor[4] = { 0 };
	int rx_monitor[4] = { 0 };

	if (uarthub_read_dbg_monitor(&debug_monitor_sel, tx_monitor, rx_monitor) < 0)
		return 0;

	if (debug_monitor_sel != 0x2)
		return 0;

	rx_crc_data_en =
		DEBUG_MODE_CRTL_GET_tx_monitor_display_rx_crc_data_en(DEBUG_MODE_CRTL_ADDR);

	pr_info("[%s][%s] RX_CRC_DATA_EN=[0x%x],MON_PTR=[R:%d,T:%d],RX_CRC_CNT=[M:%d,D:%d]",
		def_tag, ((tag == NULL) ? "null" : tag), rx_crc_data_en,
		DEBUG_MODE_CRTL_GET_crc_result_mode_rx_monitor_pointer(DEBUG_MODE_CRTL_ADDR),
		DEBUG_MODE_CRTL_GET_crc_result_mode_tx_monitor_pointer(DEBUG_MODE_CRTL_ADDR),
		CRC_CNT_GET_crc_rx_match_cnt(CRC_CNT_ADDR),
		CRC_CNT_GET_crc_rx_dismatch_cnt(CRC_CNT_ADDR));

	pr_info("[%s][%s] %s[7:0]=[0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x],RX_CRC_RESULT[7:0]=[0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		((rx_crc_data_en == 1) ? "RX_CRC_DATA" : "TX_CRC_RESULT"),
		((tx_monitor[3] & 0xFFFF0000) >> 16), (tx_monitor[3] & 0xFFFF),
		((tx_monitor[2] & 0xFFFF0000) >> 16), (tx_monitor[2] & 0xFFFF),
		((tx_monitor[1] & 0xFFFF0000) >> 16), (tx_monitor[1] & 0xFFFF),
		((tx_monitor[0] & 0xFFFF0000) >> 16), (tx_monitor[0] & 0xFFFF),
		((rx_monitor[3] & 0xFFFF0000) >> 16), (rx_monitor[3] & 0xFFFF),
		((rx_monitor[2] & 0xFFFF0000) >> 16), (rx_monitor[2] & 0xFFFF),
		((rx_monitor[1] & 0xFFFF0000) >> 16), (rx_monitor[1] & 0xFFFF),
		((rx_monitor[0] & 0xFFFF0000) >> 16), (rx_monitor[0] & 0xFFFF));

	return 0;
}

int uarthub_dump_debug_tx_rx_count_mt6989(const char *tag, int trigger_point)
{
	static int cur_tx_pkt_cnt_d0;
	static int cur_tx_pkt_cnt_d1;
	static int cur_tx_pkt_cnt_d2;
	static int cur_rx_pkt_cnt_d0;
	static int cur_rx_pkt_cnt_d1;
	static int cur_rx_pkt_cnt_d2;
	static int debug_monitor_sel;
	static int tx_monitor[4] = { 0 };
	static int rx_monitor[4] = { 0 };
	static int tx_monitor_pointer;
	static int rx_monitor_pointer;
	static int check_data_mode_sel;
	static int fsm_dbg_sta;
	static int d0_wait_for_send_xoff;
	static int d1_wait_for_send_xoff;
	static int d2_wait_for_send_xoff;
	static int cmm_wait_for_send_xoff;
	static int ap_wait_for_send_xoff;
	static int d0_detect_xoff;
	static int d1_detect_xoff;
	static int d2_detect_xoff;
	static int cmm_detect_xoff;
	static int ap_detect_xoff;
	static int d0_rx_bcnt;
	static int d1_rx_bcnt;
	static int d2_rx_bcnt;
	static int cmm_rx_bcnt;
	static int ap_rx_bcnt;
	static int d0_tx_bcnt;
	static int d1_tx_bcnt;
	static int d2_tx_bcnt;
	static int cmm_tx_bcnt;
	static int ap_tx_bcnt;
	static int pre_trigger_point = -1;
	struct uarthub_uartip_debug_info pkt_cnt = {0};
	struct uarthub_uartip_debug_info debug1 = {0};
	struct uarthub_uartip_debug_info debug2 = {0};
	struct uarthub_uartip_debug_info debug3 = {0};
	struct uarthub_uartip_debug_info debug4 = {0};
	struct uarthub_uartip_debug_info debug5 = {0};
	struct uarthub_uartip_debug_info debug6 = {0};
	struct uarthub_uartip_debug_info debug7 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};
	const char *def_tag = "HUB_DBG";
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int ret = 0;

	if (trigger_point != DUMP0 && trigger_point != DUMP1) {
		pr_notice("[%s] trigger_point = %d is invalid\n", __func__, trigger_point);
		return -1;
	}

	if (trigger_point == DUMP1 && pre_trigger_point == 0) {
		len = 0;
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s], dump0, pcnt=[R:%d-%d-%d",
			def_tag, ((tag == NULL) ? "null" : tag),
			cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d-%d-%d]",
			cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",bcnt=[R:%d-%d-%d-%d-%d",
			d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt,
			cmm_rx_bcnt, ap_rx_bcnt);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d-%d-%d-%d-%d]",
			d0_tx_bcnt, d1_tx_bcnt, d2_tx_bcnt,
			cmm_tx_bcnt, ap_tx_bcnt);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",wsend_xoff=[%d-%d-%d-%d-%d]",
			d0_wait_for_send_xoff, d1_wait_for_send_xoff,
			d2_wait_for_send_xoff, cmm_wait_for_send_xoff,
			ap_wait_for_send_xoff);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
				",det_xoff=[%d-%d-%d-%d-%d]",
				d0_detect_xoff, d1_detect_xoff, d2_detect_xoff,
				cmm_detect_xoff, ap_detect_xoff);
		if (ret > 0)
			len += ret;

		len = uarthub_record_check_data_mode_sta_to_buffer(
			dmp_info_buf, len, debug_monitor_sel, tx_monitor, rx_monitor,
			tx_monitor_pointer, rx_monitor_pointer, check_data_mode_sel);

		len = uarthub_record_intfhub_fsm_sta_to_buffer(
			dmp_info_buf, len, fsm_dbg_sta);

		pr_info("%s\n", dmp_info_buf);
	}

	if (uarthub_is_apb_bus_clk_enable_mt6989() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	pkt_cnt.dev0 = UARTHUB_REG_READ(DEV0_PKT_CNT_ADDR);
	pkt_cnt.dev1 = UARTHUB_REG_READ(DEV1_PKT_CNT_ADDR);
	pkt_cnt.dev2 = UARTHUB_REG_READ(DEV2_PKT_CNT_ADDR);

	cur_tx_pkt_cnt_d0 = ((pkt_cnt.dev0 & 0xFF000000) >> 24);
	cur_tx_pkt_cnt_d1 = ((pkt_cnt.dev1 & 0xFF000000) >> 24);
	cur_tx_pkt_cnt_d2 = ((pkt_cnt.dev2 & 0xFF000000) >> 24);
	cur_rx_pkt_cnt_d0 = ((pkt_cnt.dev0 & 0xFF00) >> 8);
	cur_rx_pkt_cnt_d1 = ((pkt_cnt.dev1 & 0xFF00) >> 8);
	cur_rx_pkt_cnt_d2 = ((pkt_cnt.dev2 & 0xFF00) >> 8);

	if ((uarthub_read_dbg_monitor(&debug_monitor_sel, tx_monitor, rx_monitor) == 0) &&
			(debug_monitor_sel == 0x1)) {
		tx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_tx_monitor_pointer(
			DEBUG_MODE_CRTL_ADDR);
		rx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_rx_monitor_pointer(
			DEBUG_MODE_CRTL_ADDR);
		check_data_mode_sel = DEBUG_MODE_CRTL_GET_check_data_mode_select(
			DEBUG_MODE_CRTL_ADDR);
	}

	fsm_dbg_sta = UARTHUB_REG_READ(DBG_STATE_ADDR);

	UARTHUB_DEBUG_READ_DEBUG_REG(dev0, uartip, uartip_id_ap);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev1, uartip, uartip_id_md);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev2, uartip, uartip_id_adsp);
	UARTHUB_DEBUG_READ_DEBUG_REG(cmm, uartip, uartip_id_cmm);

	if (apuart_base_map[3] != NULL) {
		UARTHUB_DEBUG_READ_DEBUG_REG(ap, apuart, 3);
	} else {
		debug1.ap = 0;
		debug2.ap = 0;
		debug3.ap = 0;
		debug5.ap = 0;
		debug6.ap = 0;
		debug8.ap = 0;
	}

	d0_wait_for_send_xoff = UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev0);
	d1_wait_for_send_xoff = UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev1);
	d2_wait_for_send_xoff = UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.dev2);
	cmm_wait_for_send_xoff = UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.cmm);
	ap_wait_for_send_xoff = UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF(debug1.ap);

	d0_detect_xoff = UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev0);
	d1_detect_xoff = UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev1);
	d2_detect_xoff = UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.dev2);
	cmm_detect_xoff = UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.cmm);
	ap_detect_xoff = UARTHUB_DEBUG_GET_SWTXDIS_DET_XOFF(debug8.ap);

	d0_rx_bcnt = UARTHUB_DEBUG_GET_OP_RX_REQ(debug5.dev0, debug6.dev0);
	d1_rx_bcnt = UARTHUB_DEBUG_GET_OP_RX_REQ(debug5.dev1, debug6.dev1);
	d2_rx_bcnt = UARTHUB_DEBUG_GET_OP_RX_REQ(debug5.dev2, debug6.dev2);
	cmm_rx_bcnt = UARTHUB_DEBUG_GET_OP_RX_REQ(debug5.cmm, debug6.cmm);
	ap_rx_bcnt = UARTHUB_DEBUG_GET_OP_RX_REQ(debug5.ap, debug6.ap);
	d0_tx_bcnt = UARTHUB_DEBUG_GET_IP_TX_DMA(debug2.dev0, debug3.dev0);
	d1_tx_bcnt = UARTHUB_DEBUG_GET_IP_TX_DMA(debug2.dev1, debug3.dev1);
	d2_tx_bcnt = UARTHUB_DEBUG_GET_IP_TX_DMA(debug2.dev2, debug3.dev2);
	cmm_tx_bcnt = UARTHUB_DEBUG_GET_IP_TX_DMA(debug2.cmm, debug3.cmm);
	ap_tx_bcnt = UARTHUB_DEBUG_GET_IP_TX_DMA(debug2.ap, debug3.ap);

	if (trigger_point != DUMP0) {
		len = 0;
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			"[%s][%s], dump1, pcnt=[R:%d-%d-%d",
			def_tag, ((tag == NULL) ? "null" : tag),
			cur_rx_pkt_cnt_d0, cur_rx_pkt_cnt_d1, cur_rx_pkt_cnt_d2);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d-%d-%d]",
			cur_tx_pkt_cnt_d0, cur_tx_pkt_cnt_d1, cur_tx_pkt_cnt_d2);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",bcnt=[R:%d-%d-%d-%d-%d",
			d0_rx_bcnt, d1_rx_bcnt, d2_rx_bcnt,
			cmm_rx_bcnt, ap_rx_bcnt);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",T:%d-%d-%d-%d-%d]",
			d0_tx_bcnt, d1_tx_bcnt, d2_tx_bcnt,
			cmm_tx_bcnt, ap_tx_bcnt);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",wsend_xoff=[%d-%d-%d-%d-%d]",
			d0_wait_for_send_xoff, d1_wait_for_send_xoff,
			d2_wait_for_send_xoff, cmm_wait_for_send_xoff,
			ap_wait_for_send_xoff);
		if (ret > 0)
			len += ret;

		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",det_xoff=[%d-%d-%d-%d-%d]",
			d0_detect_xoff, d1_detect_xoff,
			d2_detect_xoff, cmm_detect_xoff,
			ap_detect_xoff);
		if (ret > 0)
			len += ret;

		len = uarthub_record_check_data_mode_sta_to_buffer(
			dmp_info_buf, len, debug_monitor_sel, tx_monitor, rx_monitor,
			tx_monitor_pointer, rx_monitor_pointer, check_data_mode_sel);

		len = uarthub_record_intfhub_fsm_sta_to_buffer(
			dmp_info_buf, len, fsm_dbg_sta);

		pr_info("%s\n", dmp_info_buf);
	}

	pre_trigger_point = trigger_point;
	return 0;
}

int uarthub_dump_debug_clk_info_mt6989(const char *tag)
{
	int val = 0;
#if !(UARTHUB_SUPPORT_FPGA)
	int spm_res_uarthub = 0, spm_res_internal = 0, spm_res_26m_off = 0;
	int topckgen_cg = 0, peri_cg = 0;
	uint32_t timer_h = 0, timer_l = 0;
#endif
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	int len = 0;
	int ret = 0;
	int debug_monitor_sel = 0;
	int tx_monitor[4] = { 0 };
	int rx_monitor[4] = { 0 };
	int tx_monitor_pointer = 0, rx_monitor_pointer = 0;
	int check_data_mode_sel = 0;
	int fsm_dbg_sta = 0;

	if (uarthub_is_apb_bus_clk_enable_mt6989() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] IDBG=[0x%x]", ((tag == NULL) ? "null" : tag), val);
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);
	if (ret > 0)
		len += ret;

	val = uarthub_is_apb_bus_clk_enable_mt6989();
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",APB=[0x%x]", val);
	if (ret > 0)
		len += ret;

	if (val == 0) {
		pr_info("%s\n", dmp_info_buf);
		return -1;
	}

#if !(UARTHUB_SUPPORT_FPGA)
	val = uarthub_get_uarthub_cg_info_mt6989(&topckgen_cg, &peri_cg);
	if (val >= 0) {
		/* the expect value is 0x0 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HUB_CG=[topck:0x%x,peri:0x%x,exp:0x0]", topckgen_cg, peri_cg);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uart_src_clk_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UART_SRC_CLK=[0x%x(%s)]", val, ((val == 0) ? "26M" : "TOPCK"));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_spm_res_info_mt6989(
		&spm_res_uarthub, &spm_res_internal, &spm_res_26m_off);
	if (val == 1) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[1]");
		if (ret > 0)
			len += ret;
	} else if (val == 0 || val == 2) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",SPM=[%d(0x%x/0x%x/0x%x,exp:0x1D/0x17/0x0)]",
			val, spm_res_uarthub, spm_res_internal, spm_res_26m_off);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_hwccf_univpll_on_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UNIVPLL=[%d]", val);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uart_mux_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x2 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UART_MUX=[0x%x(%s)]", val,
			((val == 0) ? "26M" : ((val == 1) ? "52M" :
				((val == 2) ? "104M" : "208M"))));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uarthub_mux_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HUB_MUX=[0x%x(%s)]", val,
			((val == 0) ? "26M" : ((val == 1) ? "104M" : "208M")));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_spm_sys_timer_mt6989(&timer_h, &timer_l);
	if (val == 1) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",sys_timer=[H:%x,L:%x]", timer_h, timer_l);
		if (ret > 0)
			len += ret;
	}
#endif

	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	if (dev0_sta == dev1_sta && dev1_sta == dev2_sta) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x]", dev0_sta);
		if (ret > 0)
			len += ret;
	} else {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",IDEV_STA=[0x%x-0x%x-0x%x]", dev0_sta, dev1_sta, dev2_sta);
		if (ret > 0)
			len += ret;
	}

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);
	if (ret > 0)
		len += ret;

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);
	if (ret > 0)
		len += ret;

	if ((uarthub_read_dbg_monitor(&debug_monitor_sel, tx_monitor, rx_monitor) == 0) &&
			(debug_monitor_sel == 0x1)) {
		tx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_tx_monitor_pointer(
			DEBUG_MODE_CRTL_ADDR);
		rx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_rx_monitor_pointer(
			DEBUG_MODE_CRTL_ADDR);
		check_data_mode_sel = DEBUG_MODE_CRTL_GET_check_data_mode_select(
			DEBUG_MODE_CRTL_ADDR);

		len = uarthub_record_check_data_mode_sta_to_buffer(
			dmp_info_buf, len, debug_monitor_sel, tx_monitor, rx_monitor,
			tx_monitor_pointer, rx_monitor_pointer, check_data_mode_sel);
	}

	fsm_dbg_sta = UARTHUB_REG_READ(DBG_STATE_ADDR);
	len = uarthub_record_intfhub_fsm_sta_to_buffer(dmp_info_buf, len, fsm_dbg_sta);

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_dump_debug_byte_cnt_info_mt6989(const char *tag)
{
	struct uarthub_uartip_debug_info debug1 = {0};
	struct uarthub_uartip_debug_info debug2 = {0};
	struct uarthub_uartip_debug_info debug3 = {0};
	struct uarthub_uartip_debug_info debug4 = {0};
	struct uarthub_uartip_debug_info debug5 = {0};
	struct uarthub_uartip_debug_info debug6 = {0};
	struct uarthub_uartip_debug_info debug7 = {0};
	struct uarthub_uartip_debug_info debug8 = {0};
	int dev0_sta = 0, dev1_sta = 0, dev2_sta = 0;
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int val = 0;
	int ret = 0;
	int debug_monitor_sel = 0;
	int tx_monitor[4] = { 0 };
	int rx_monitor[4] = { 0 };
	int tx_monitor_pointer = 0, rx_monitor_pointer = 0;
	int check_data_mode_sel = 0;
	int fsm_dbg_sta = 0;
	int print_ap = 0;
#if !(UARTHUB_SUPPORT_FPGA)
	uint32_t timer_h = 0, timer_l = 0;
#endif

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	len = 0;
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] IDBG=[0x%x]", ((tag == NULL) ? "null" : tag), val);
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);
	if (ret > 0)
		len += ret;

	UARTHUB_DEBUG_READ_DEBUG_REG(dev0, uartip, uartip_id_ap);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev1, uartip, uartip_id_md);
	UARTHUB_DEBUG_READ_DEBUG_REG(dev2, uartip, uartip_id_adsp);
	UARTHUB_DEBUG_READ_DEBUG_REG(cmm, uartip, uartip_id_cmm);

	if (apuart_base_map[3] != NULL) {
		print_ap = 1;
		UARTHUB_DEBUG_READ_DEBUG_REG(ap, apuart, 3);
	}

	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug5, 0xF0, 4, debug6, 0x3, 4, print_ap, ",bcnt=[R:%d-%d-%d-%d-%s");
	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug2, 0xF0, 4, debug3, 0x3, 4, print_ap, ",T:%d-%d-%d-%d-%s]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug7, 0x3F, 0, print_ap, ",fifo_woffset=[R:%d-%d-%d-%d-%s");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug4, 0x3F, 0, print_ap, ",T:%d-%d-%d-%d-%s]");
	UARTHUB_DEBUG_PRINT_DEBUG_2_REG(debug4, 0xC0, 6, debug5, 0xF, 2, print_ap,
		",fifo_tx_roffset=[%d-%d-%d-%d-%s]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug6, 0xFC, 2, print_ap, ",offset_dma=[R:%d-%d-%d-%d-%s");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug3, 0xFC, 2, print_ap, ",T:%d-%d-%d-%d-%s]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug1, 0xE0, 5, print_ap, ",wsend_xoff=[%d-%d-%d-%d-%s]");
	UARTHUB_DEBUG_PRINT_DEBUG_1_REG(debug8, 0x8, 3, print_ap, ",det_xoff=[%d-%d-%d-%d-%s]");

#if !(UARTHUB_SUPPORT_FPGA)
	val = uarthub_get_hwccf_univpll_on_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UNIVPLL=[%d]", val);
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uart_src_clk_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UART_SRC_CLK=[0x%x(%s)]", val, ((val == 0) ? "26M" : "TOPCK"));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uart_mux_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x2 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",UART_MUX=[0x%x(%s)]", val,
			((val == 0) ? "26M" : ((val == 1) ? "52M" :
				((val == 2) ? "104M" : "208M"))));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_uarthub_mux_info_mt6989();
	if (val >= 0) {
		/* the expect value is 0x1 */
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",HUB_MUX=[0x%x(%s)]", val,
			((val == 0) ? "26M" : ((val == 1) ? "104M" : "208M")));
		if (ret > 0)
			len += ret;
	}

	val = uarthub_get_spm_sys_timer_mt6989(&timer_h, &timer_l);
	if (val == 1) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",sys_timer=[H:%x,L:%x]", timer_h, timer_l);
		if (ret > 0)
			len += ret;
	}
#endif

	dev0_sta = UARTHUB_REG_READ(DEV0_STA_ADDR);
	dev1_sta = UARTHUB_REG_READ(DEV1_STA_ADDR);
	dev2_sta = UARTHUB_REG_READ(DEV2_STA_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDEV_STA=[0x%x-0x%x-0x%x]", dev0_sta, dev1_sta, dev2_sta);
	if (ret > 0)
		len += ret;

	val = UARTHUB_REG_READ(STA0_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",ISTA0=[0x%x]", val);
	if (ret > 0)
		len += ret;

	val = DBG_CTRL_GET_intfhub_dbg_sel(DBG_CTRL_ADDR);
	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",IDBG=[0x%x]", val);
	if (ret > 0)
		len += ret;

	if ((uarthub_read_dbg_monitor(&debug_monitor_sel, tx_monitor, rx_monitor) == 0) &&
			(debug_monitor_sel == 0x1)) {
		tx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_tx_monitor_pointer(
			DEBUG_MODE_CRTL_ADDR);
		rx_monitor_pointer = DEBUG_MODE_CRTL_GET_check_data_mode_rx_monitor_pointer(
			DEBUG_MODE_CRTL_ADDR);
		check_data_mode_sel = DEBUG_MODE_CRTL_GET_check_data_mode_select(
			DEBUG_MODE_CRTL_ADDR);

		len = uarthub_record_check_data_mode_sta_to_buffer(
			dmp_info_buf, len, debug_monitor_sel, tx_monitor, rx_monitor,
			tx_monitor_pointer, rx_monitor_pointer, check_data_mode_sel);
	}

	fsm_dbg_sta = UARTHUB_REG_READ(DBG_STATE_ADDR);
	len = uarthub_record_intfhub_fsm_sta_to_buffer(dmp_info_buf, len, fsm_dbg_sta);

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_dump_debug_apdma_uart_info_mt6989(const char *tag)
{
	const char *def_tag = "HUB_DBG_APMDA";

	pr_info("[%s][%s] 0=[0x%x],4=[0x%x],8=[0x%x],c=[0x%x],10=[0x%x],14=[0x%x],18=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x00),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x04),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x08),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x0c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x10),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x14),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x18));

	pr_info("[%s][%s] 1c=[0x%x],20=[0x%x],24=[0x%x],28=[0x%x],2c=[0x%x],30=[0x%x],34=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x1c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x20),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x24),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x28),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x2c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x30),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x34));

	pr_info("[%s][%s] 38=[0x%x],3c=[0x%x],40=[0x%x],44=[0x%x],48=[0x%x],4c=[0x%x],50=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x38),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x3c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x40),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x44),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x48),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x4c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x50));

	pr_info("[%s][%s] 54=[0x%x],58=[0x%x],5c=[0x%x],60=[0x%x],64=[0x%x]\n",
		def_tag, ((tag == NULL) ? "null" : tag),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x54),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x58),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x5c),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x60),
		UARTHUB_REG_READ(apdma_uart_tx_int_remap_addr_mt6989 + 0x64));

	return 0;
}

#define UARTHUB_IRQ_OP_LOG_SIZE     5
#define UARTHUB_LOG_IRQ_PKT_SIZE    12
#define UARTHUB_LOG_IRQ_IDX_ADDR(addr) (addr)

#define UARTHUB_TSK_OP_LOG_SIZE     20
#define UARTHUB_LOG_TSK_PKT_SIZE    20
#define UARTHUB_LOG_TSK_IDX_ADDR(addr) \
		(addr + (UARTHUB_LOG_IRQ_PKT_SIZE * UARTHUB_IRQ_OP_LOG_SIZE) + 4)

#define UARTHUB_CK_CNT_ADDR(addr) \
	(UARTHUB_LOG_TSK_IDX_ADDR(addr) + (UARTHUB_TSK_OP_LOG_SIZE * UARTHUB_LOG_TSK_PKT_SIZE) + 4)


#define UARTHUB_LAST_CK_ON(addr) (UARTHUB_CK_CNT_ADDR(addr) + 4)
#define UARTHUB_LAST_CK_ON_CNT(addr) (UARTHUB_LAST_CK_ON(addr) + 8)

#define UARTHUB_TMP_BUF_SZ  512
char g_buf_m6989[UARTHUB_TMP_BUF_SZ];

int uarthub_dump_sspm_log_mt6989(const char *tag)
{
	void __iomem *log_addr = NULL;
	int i, n, used;
	uint32_t val, irq_idx = 0, tsk_idx = 0;
	uint32_t v1, v2, v3;
	uint64_t t;
	char *tmp;
	const char *def_tag = "HUB_DBG_SSPM";

	g_buf_m6989[0] = '\0';
	log_addr = UARTHUB_LOG_IRQ_IDX_ADDR(sys_sram_remap_addr_mt6989);
	irq_idx = UARTHUB_REG_READ(log_addr);
	log_addr += 4;

	tmp = g_buf_m6989;
	used = 0;
	for (i = 0; i < UARTHUB_IRQ_OP_LOG_SIZE; i++) {
		t = UARTHUB_REG_READ(log_addr);
		t = t << 32 | UARTHUB_REG_READ(log_addr + 4);
		n = snprintf(tmp + used, UARTHUB_TMP_BUF_SZ - used, "[%llu:%X] ",
							t,
							UARTHUB_REG_READ(log_addr + 8));
		if (n > 0)
			used += n;
		log_addr += UARTHUB_LOG_IRQ_PKT_SIZE;
	}
	pr_info("[%s][%s] [%x] %s",
		def_tag, ((tag == NULL) ? "null" : tag), irq_idx, g_buf_m6989);

	log_addr = UARTHUB_LOG_TSK_IDX_ADDR(sys_sram_remap_addr_mt6989);
	tsk_idx = UARTHUB_REG_READ(log_addr);
	log_addr += 4;
	g_buf_m6989[0] = '\0';
	tmp = g_buf_m6989;
	used = 0;
	for (i = 0; i < UARTHUB_TSK_OP_LOG_SIZE; i++) {
		t = UARTHUB_REG_READ(log_addr);
		t = t << 32 | UARTHUB_REG_READ(log_addr + 4);
		n = snprintf(tmp + used, UARTHUB_TMP_BUF_SZ - used, "[%llu:%x-%x-%x]",
							t,
							UARTHUB_REG_READ(log_addr + 8),
							UARTHUB_REG_READ(log_addr + 12),
							UARTHUB_REG_READ(log_addr + 16));
		if (n > 0) {
			used += n;
			if ((i % (UARTHUB_TSK_OP_LOG_SIZE/2))
					== ((UARTHUB_TSK_OP_LOG_SIZE/2) - 1)) {
				pr_info("[%s][%s] [%x] %s",
					def_tag, ((tag == NULL) ? "null" : tag),
					tsk_idx, g_buf_m6989);
				g_buf_m6989[0] = '\0';
				tmp = g_buf_m6989;
				used = 0;
			}
		}
		log_addr += UARTHUB_LOG_TSK_PKT_SIZE;
	}

	log_addr = UARTHUB_CK_CNT_ADDR(sys_sram_remap_addr_mt6989);
	val = UARTHUB_REG_READ(log_addr);

	log_addr = UARTHUB_LAST_CK_ON(sys_sram_remap_addr_mt6989);
	v1 = UARTHUB_REG_READ(log_addr);
	v2 = UARTHUB_REG_READ(log_addr + 4);

	log_addr = UARTHUB_LAST_CK_ON_CNT(sys_sram_remap_addr_mt6989);
	v3 = UARTHUB_REG_READ(log_addr);

	pr_info("[%s][%s] off/on cnt=[%d][%d] ckon=[%x][%x] cnt=[%x]",
		def_tag, ((tag == NULL) ? "null" : tag),
		(val & 0xFFFF), (val >> 16),
		v1, v2, v3);

	return 0;
}

int uarthub_trigger_fpga_testing_mt6989(int type)
{
#if UARTHUB_SUPPORT_FPGA
	pr_info("[%s] FPGA type=[%d]\n", __func__, type);
#else
	pr_info("[%s] NOT support FPGA\n", __func__);
#endif
	return 0;
}

/* Entry : dvt test (mt6989)*/
int uarthub_trigger_dvt_testing_mt6989(int type)
{
#if UARTHUB_SUPPORT_DVT
	int state = 0;

	int (*dvt_func[])(void) = {
		uarthub_ut_ip_help_info_mt6989,
		uarthub_ut_ip_host_tx_packet_loopback_mt6989,
		uarthub_ut_ip_timeout_init_fsm_ctrl_mt6989,
		uarthub_ut_ip_clear_rx_data_irq_mt6989,
		uarthub_ut_ip_verify_debug_monitor_packet_info_mode_mt6989,
		uarthub_ut_ip_verify_debug_monitor_check_data_mode_mt6989,
		uarthub_ut_ip_verify_debug_monitor_crc_result_mode_mt6989
	};
	const char * const func_name[] = {
		"help info",
		"host_tx_packet_loopback",
		"timeout_init_fsm_ctrl",
		"clear_rx_data_irq",
		"verify_debug_monitor_packet_info_mode",
		"verify_debug_monitor_check_data_mode",
		"verify_debug_monitor_crc_result_mode"
	};
	pr_info("[%s] DVT type=[%d]\n", __func__, type);

	if (type < 0 || type >= sizeof(func_name)) {
		pr_info("[%s] Unkonwn type\n", __func__);
		return 0;
	}
	state = dvt_func[type]();
	pr_info("[DVT_%d] %s: RESULT=[%s], state=[%d]\n",
		type, func_name[type], ((state) ? "FAIL" : "PASS"), state);

#else
	pr_info("[%s] NOT support DVT\n", __func__);
#endif
	return 0;
}

#if UARTHUB_SUPPORT_DVT
int uarthub_verify_combo_connect_sta_mt6989(int type, int rx_delay_ms)
{
	int state = -1;

	if (type == 0)
		state = uarthub_verify_cmm_trx_connsys_sta_mt6989(rx_delay_ms);
	else if (type == 1)
		state = uarthub_verify_cmm_loopback_sta_mt6989();
	else
		pr_notice("[%s] Not support type value=[%d]\n", __func__, type);

	return state;
}
#endif

int uarthub_start_sample_baud_rate(int dev_index)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 1);
	DBG_CTRL_SET_sample_baud_en(DBG_CTRL_ADDR, 1);
	DBG_CTRL_SET_sample_baud_sel(DBG_CTRL_ADDR, dev_index);
	DBG_CTRL_SET_sample_baud_clr(DBG_CTRL_ADDR, 1);

	return 0;
}

int uarthub_stop_sample_baud_rate(void)
{
	int cg_en = 0;
	int sample_baud_en = 0;

	cg_en = DEBUG_MODE_CRTL_GET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR);
	sample_baud_en = DBG_CTRL_GET_sample_baud_en(DBG_CTRL_ADDR);

	if (cg_en == 0 || sample_baud_en == 0) {
		pr_notice("[%s] failed. cg_en = %d, sample_baud_en = %d\n",
			__func__, cg_en, sample_baud_en);
		return -1;
	}

	pr_info("[%s] sample_baud_sel = %d, sample_baud_count = %d\n", __func__,
		DBG_CTRL_GET_sample_baud_sel(DBG_CTRL_ADDR),
		DBG_CTRL_GET_sample_baud_count(DBG_CTRL_ADDR));

	DBG_CTRL_SET_sample_baud_en(DBG_CTRL_ADDR, 0);
	DEBUG_MODE_CRTL_SET_intfhub_cg_en(DEBUG_MODE_CRTL_ADDR, 0);

	return 0;
}

int uarthub_record_check_data_mode_sta_to_buffer(
	unsigned char *dmp_info_buf, int len,
	int debug_monitor_sel,
	int *tx_monitor, int *rx_monitor,
	int tx_monitor_pointer, int rx_monitor_pointer,
	int check_data_mode_sel)
{
	int ret = 0;

	if (debug_monitor_sel == 0x1) {
		ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			",CHK_DATA_MON[0:15](%d)=[R(%d):%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X,T(%d):%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X]",
			check_data_mode_sel,
			rx_monitor_pointer,
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(rx_monitor[0]),
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(rx_monitor[1]),
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(rx_monitor[2]),
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(rx_monitor[3]),
			tx_monitor_pointer,
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(tx_monitor[0]),
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(tx_monitor[1]),
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(tx_monitor[2]),
			UARTHUB_DEBUG_GET_DBG_MONITOR_CHECK_DATA(tx_monitor[3]));
		if (ret > 0)
			len += ret;
	}

	return len;
}

int uarthub_record_intfhub_fsm_sta_to_buffer(
	unsigned char *dmp_info_buf, int len, int fsm_dbg_sta)
{
	int ret = 0;
	int val = 0;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_intfhub_ctrl_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_intfhub_ctrl_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",fsm=[hub:%s",
		((val == 0) ? "IDLE" : ((val == 1) ? "PREPARE" :
			((val == 2) ? "READY" : ((val == 3) ? "CKON" :
			((val == 4) ? "CKOFF" : "WAIT"))))));
	if (ret > 0)
		len += ret;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_tx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_tx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",tx:%s",
		((val == 0) ? "IDLE" : ((val == 1) ? "HEADER" :
			((val == 2) ? "PAYLOAD" : ((val == 3) ? "CRC" :
			((val == 4) ? "ESP" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",rx:%s",
		((val == 0) ? "IDLE" : ((val == 1) ? "DEC" :
			((val == 2) ? "DEV0" : ((val == 3) ? "DEV1" :
			((val == 4) ? "DEV2" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_dev0_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_dev0_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",d0_rx:%s",
		((val == 0) ? "IDLE" : ((val == 1) ? "HEADER" :
			((val == 2) ? "PAYLOAD" : ((val == 3) ? "CRC" :
			((val == 4) ? "ESP" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_dev1_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_dev1_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",d1_rx:%s",
		((val == 0) ? "IDLE" : ((val == 1) ? "HEADER" :
			((val == 2) ? "PAYLOAD" : ((val == 3) ? "CRC" :
			((val == 4) ? "ESP" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_dbg_dev2_rx_fsm_state)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_dbg_dev2_rx_fsm_state));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",d2_rx:%s",
		((val == 0) ? "IDLE" : ((val == 1) ? "HEADER" :
			((val == 2) ? "PAYLOAD" : ((val == 3) ? "CRC" :
			((val == 4) ? "ESP" : "END"))))));
	if (ret > 0)
		len += ret;

	val = ((fsm_dbg_sta & REG_FLD_MASK(DBG_STATE_FLD_intfhub_dbg)) >>
		REG_FLD_SHIFT(DBG_STATE_FLD_intfhub_dbg));

	ret = snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		",dbg:0x%x]", val);
	if (ret > 0)
		len += ret;

	return len;
}
