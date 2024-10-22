/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef UARTHUB_DRV_CORE_H
#define UARTHUB_DRV_CORE_H

#include "common_def_id.h"
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/fs.h>

#if IS_ENABLED(CONFIG_DYNAMIC_DEBUG)
#define UARTHUB_DBG_SUPPORT 1
#else
#define UARTHUB_DBG_SUPPORT 0
#endif

extern struct uarthub_ops_struct *g_plat_ic_ops;
extern struct uarthub_core_ops_struct *g_plat_ic_core_ops;
extern struct uarthub_debug_ops_struct *g_plat_ic_debug_ops;
extern struct uarthub_ut_test_ops_struct *g_plat_ic_ut_test_ops;

extern int g_uarthub_disable;
extern int g_is_ut_testing;

extern int g_dev0_irq_sta;
extern int g_dev0_inband_irq_sta;

extern struct mutex g_lock_dump_log;
extern struct mutex g_clear_trx_req_lock;

extern struct debug_info_ctrl uarthub_debug_info_ctrl;
extern struct debug_info_ctrl uarthub_debug_clk_info_ctrl;

extern struct workqueue_struct *uarthub_workqueue;

struct uarthub_reg_base_addr {
	unsigned long vir_addr;
	unsigned long phy_addr;
};

struct assert_ctrl {
	int err_type;
	unsigned long err_ts;
	struct work_struct trigger_assert_work;
};

struct debug_info_ctrl {
	char tag[256];
	struct work_struct debug_info_work;
};

struct uarthub_gpio_base_addr {
	unsigned int addr;
	unsigned int mask;
	unsigned int value;
	unsigned int gpio_value;
};

struct uarthub_gpio_trx_info {
	struct uarthub_gpio_base_addr tx_mode;
	struct uarthub_gpio_base_addr rx_mode;
	struct uarthub_gpio_base_addr tx_dir;
	struct uarthub_gpio_base_addr rx_dir;
	struct uarthub_gpio_base_addr tx_ies;
	struct uarthub_gpio_base_addr rx_ies;
	struct uarthub_gpio_base_addr tx_pu;
	struct uarthub_gpio_base_addr rx_pu;
	struct uarthub_gpio_base_addr tx_pd;
	struct uarthub_gpio_base_addr rx_pd;
	struct uarthub_gpio_base_addr tx_drv;
	struct uarthub_gpio_base_addr rx_drv;
	struct uarthub_gpio_base_addr tx_smt;
	struct uarthub_gpio_base_addr rx_smt;
	struct uarthub_gpio_base_addr tx_tdsel;
	struct uarthub_gpio_base_addr rx_tdsel;
	struct uarthub_gpio_base_addr tx_rdsel;
	struct uarthub_gpio_base_addr rx_rdsel;
	struct uarthub_gpio_base_addr tx_sec_en;
	struct uarthub_gpio_base_addr rx_sec_en;
	struct uarthub_gpio_base_addr rx_din;
};

typedef void (*UARTHUB_CORE_IRQ_CB) (unsigned int err_type);

typedef int(*UARTHUB_PLAT_DUMP_APUART_DEBUG_CTRL) (int enable);
typedef int(*UARTHUB_PLAT_GET_APUART_DEBUG_CTRL_STA) (void);
typedef int(*UARTHUB_PLAT_GET_INTFHUB_BASE_ADDR) (void);
typedef int(*UARTHUB_PLAT_GET_UARTIP_BASE_ADDR) (int dev_index);
typedef int(*UARTHUB_PLAT_DUMP_UARTIP_DEBUG_INFO) (
	const char *tag, struct mutex *uartip_lock);
typedef int(*UARTHUB_PLAT_DUMP_INTFHUB_DEBUG_INFO) (const char *tag);
typedef int(*UARTHUB_PLAT_DUMP_DEBUG_MONITOR) (const char *tag);
typedef int(*UARTHUB_PLAT_DEBUG_MONITOR_CTRL) (int enable, int mode, int ctrl);
typedef int(*UARTHUB_PLAT_DEBUG_MONITOR_STOP) (int stop);
typedef int(*UARTHUB_PLAT_DEBUG_MONITOR_CLR) (void);
typedef int(*UARTHUB_PLAT_DUMP_DEBUG_TX_RX_COUNT) (const char *tag, int trigger_point);
typedef int(*UARTHUB_PLAT_DUMP_DEBUG_CLK_INFO) (const char *tag);
typedef int(*UARTHUB_PLAT_DUMP_DEBUG_BYTE_CNT_INFO) (const char *tag);
typedef int(*UARTHUB_PLAT_DUMP_DEBUG_APDMA_UART_INFO) (const char *tag);
typedef int(*UARTHUB_PLAT_DUMP_SSPM_LOG) (const char *tag);
typedef int(*UARTHUB_PLAT_TRIGGER_FPGA_TESTING) (int type);
typedef int(*UARTHUB_PLAT_TRIGGER_DVT_TESTING) (int type);
typedef int(*UARTHUB_PLAT_VERIFY_COMBO_CONNECT_STA) (int type, int rx_delay_ms);

struct uarthub_debug_ops_struct {
	UARTHUB_PLAT_DUMP_APUART_DEBUG_CTRL uarthub_plat_dump_apuart_debug_ctrl;
	UARTHUB_PLAT_GET_APUART_DEBUG_CTRL_STA uarthub_plat_get_apuart_debug_ctrl_sta;
	UARTHUB_PLAT_GET_INTFHUB_BASE_ADDR uarthub_plat_get_intfhub_base_addr;
	UARTHUB_PLAT_GET_UARTIP_BASE_ADDR uarthub_plat_get_uartip_base_addr;
	UARTHUB_PLAT_DUMP_UARTIP_DEBUG_INFO uarthub_plat_dump_uartip_debug_info;
	UARTHUB_PLAT_DUMP_INTFHUB_DEBUG_INFO uarthub_plat_dump_intfhub_debug_info;
	UARTHUB_PLAT_DUMP_DEBUG_MONITOR uarthub_plat_dump_debug_monitor;
	UARTHUB_PLAT_DEBUG_MONITOR_CTRL uarthub_plat_debug_monitor_ctrl;
	UARTHUB_PLAT_DEBUG_MONITOR_STOP uarthub_plat_debug_monitor_stop;
	UARTHUB_PLAT_DEBUG_MONITOR_CLR uarthub_plat_debug_monitor_clr;
	UARTHUB_PLAT_DUMP_DEBUG_TX_RX_COUNT uarthub_plat_dump_debug_tx_rx_count;
	UARTHUB_PLAT_DUMP_DEBUG_CLK_INFO uarthub_plat_dump_debug_clk_info;
	UARTHUB_PLAT_DUMP_DEBUG_BYTE_CNT_INFO uarthub_plat_dump_debug_byte_cnt_info;
	UARTHUB_PLAT_DUMP_DEBUG_APDMA_UART_INFO uarthub_plat_dump_debug_apdma_uart_info;
	UARTHUB_PLAT_DUMP_SSPM_LOG uarthub_plat_dump_sspm_log;
	UARTHUB_PLAT_TRIGGER_FPGA_TESTING uarthub_plat_trigger_fpga_testing;
	UARTHUB_PLAT_TRIGGER_DVT_TESTING uarthub_plat_trigger_dvt_testing;
	UARTHUB_PLAT_VERIFY_COMBO_CONNECT_STA uarthub_plat_verify_combo_connect_sta;
};

typedef int(*UARTHUB_PLAT_IS_UT_TESTING) (void);
typedef int(*UARTHUB_PLAT_IS_HOST_UARTHUB_READY_STATE) (int dev_index);
typedef int(*UARTHUB_PLAT_GET_HOST_IRQ_STA) (int dev_index);
typedef int(*UARTHUB_PLAT_GET_INTFHUB_ACTIVE_STA) (void);
typedef int(*UARTHUB_PLAT_CLEAR_HOST_IRQ) (int dev_index, int irq_type);
typedef int(*UARTHUB_PLAT_MASK_HOST_IRQ) (int dev_index, int irq_type, int is_mask);
typedef int(*UARTHUB_PLAT_CONFIG_HOST_IRQ_CTRL) (int dev_index, int enable);
typedef int(*UARTHUB_PLAT_GET_HOST_RX_FIFO_SIZE) (int dev_index);
typedef int(*UARTHUB_PLAT_GET_CMM_RX_FIFO_SIZE) (void);
typedef int(*UARTHUB_PLAT_CONFIG_UARTIP_DMA_EN_CTRL) (
		int dev_index, enum uarthub_trx_type trx, int enable);
typedef int(*UARTHUB_PLAT_RESET_FIFO_TRX) (void);
typedef int(*UARTHUB_PLAT_UARTIP_WRITE_DATA_TO_TX_BUF) (int dev_index, int tx_data);
typedef int(*UARTHUB_PLAT_UARTIP_READ_DATA_FROM_RX_BUF) (int dev_index);
typedef int(*UARTHUB_PLAT_IS_UARTIP_TX_BUF_EMPTY_FOR_WRITE) (int dev_index);
typedef int(*UARTHUB_PLAT_IS_UARTIP_RX_BUF_READY_FOR_READ) (int dev_index);
typedef int(*UARTHUB_PLAT_IS_UARTIP_THROW_XOFF) (int dev_index);
typedef int(*UARTHUB_PLAT_CONFIG_UARTIP_RX_FIFO_TRIG_THR) (int dev_index, int size);

typedef int(*UARTHUB_PLAT_CONFIG_INBAND_ESC_CHAR) (int esc_char);
typedef int(*UARTHUB_PLAT_CONFIG_INBAND_ESC_STA) (int esc_sta);
typedef int(*UARTHUB_PLAT_CONFIG_INBAND_ENABLE_CTRL) (int enable);
typedef int(*UARTHUB_PLAT_CONFIG_INBAND_IRQ_ENABLE_CTRL) (int enable);
typedef int(*UARTHUB_PLAT_CONFIG_INBAND_TRIGGER) (void);
typedef int(*UARTHUB_PLAT_IS_INBAND_TX_COMPLETE) (void);
typedef int(*UARTHUB_PLAT_GET_INBAND_IRQ_STA) (void);
typedef int(*UARTHUB_PLAT_CLEAR_INBAND_IRQ) (void);
typedef int(*UARTHUB_PLAT_GET_RECEIVED_INBAND_STA) (void);
typedef int(*UARTHUB_PLAT_CLEAR_RECEIVED_INBAND_STA) (void);
typedef int(*UARTHUB_PLAT_UT_IP_VERIFY_PKT_HDR_FMT) (void);
typedef int(*UARTHUB_PLAT_UT_IP_VERIFY_TRX_NOT_READY) (void);
typedef int(*UARTHUB_PLAT_SSPM_IRQ_HANDLE) (int sspm_irq);

struct uarthub_ut_test_ops_struct {
	UARTHUB_PLAT_IS_UT_TESTING uarthub_plat_is_ut_testing;
	UARTHUB_PLAT_IS_HOST_UARTHUB_READY_STATE uarthub_plat_is_host_uarthub_ready_state;
	UARTHUB_PLAT_GET_HOST_IRQ_STA uarthub_plat_get_host_irq_sta;
	UARTHUB_PLAT_GET_INTFHUB_ACTIVE_STA uarthub_plat_get_intfhub_active_sta;
	UARTHUB_PLAT_CLEAR_HOST_IRQ uarthub_plat_clear_host_irq;
	UARTHUB_PLAT_MASK_HOST_IRQ uarthub_plat_mask_host_irq;
	UARTHUB_PLAT_CONFIG_HOST_IRQ_CTRL uarthub_plat_config_host_irq_ctrl;
	UARTHUB_PLAT_GET_HOST_RX_FIFO_SIZE uarthub_plat_get_host_rx_fifo_size;
	UARTHUB_PLAT_GET_CMM_RX_FIFO_SIZE uarthub_plat_get_cmm_rx_fifo_size;
	UARTHUB_PLAT_CONFIG_UARTIP_DMA_EN_CTRL uarthub_plat_config_uartip_dma_en_ctrl;
	UARTHUB_PLAT_RESET_FIFO_TRX uarthub_plat_reset_fifo_trx;
	UARTHUB_PLAT_UARTIP_WRITE_DATA_TO_TX_BUF uarthub_plat_uartip_write_data_to_tx_buf;
	UARTHUB_PLAT_UARTIP_READ_DATA_FROM_RX_BUF uarthub_plat_uartip_read_data_from_rx_buf;
	UARTHUB_PLAT_IS_UARTIP_TX_BUF_EMPTY_FOR_WRITE uarthub_plat_is_uartip_tx_buf_empty_for_write;
	UARTHUB_PLAT_IS_UARTIP_RX_BUF_READY_FOR_READ uarthub_plat_is_uartip_rx_buf_ready_for_read;
	UARTHUB_PLAT_IS_UARTIP_THROW_XOFF uarthub_plat_is_uartip_throw_xoff;
	UARTHUB_PLAT_CONFIG_UARTIP_RX_FIFO_TRIG_THR uarthub_plat_config_uartip_rx_fifo_trig_thr;

	UARTHUB_PLAT_CONFIG_INBAND_ESC_CHAR uarthub_plat_config_inband_esc_char;
	UARTHUB_PLAT_CONFIG_INBAND_ESC_STA uarthub_plat_config_inband_esc_sta;
	UARTHUB_PLAT_CONFIG_INBAND_ENABLE_CTRL uarthub_plat_config_inband_enable_ctrl;
	UARTHUB_PLAT_CONFIG_INBAND_IRQ_ENABLE_CTRL uarthub_plat_config_inband_irq_enable_ctrl;
	UARTHUB_PLAT_CONFIG_INBAND_TRIGGER uarthub_plat_config_inband_trigger;
	UARTHUB_PLAT_IS_INBAND_TX_COMPLETE uarthub_plat_is_inband_tx_complete;
	UARTHUB_PLAT_GET_INBAND_IRQ_STA uarthub_plat_get_inband_irq_sta;
	UARTHUB_PLAT_CLEAR_INBAND_IRQ uarthub_plat_clear_inband_irq;
	UARTHUB_PLAT_GET_RECEIVED_INBAND_STA uarthub_plat_get_received_inband_sta;
	UARTHUB_PLAT_CLEAR_RECEIVED_INBAND_STA uarthub_plat_clear_received_inband_sta;
	UARTHUB_PLAT_UT_IP_VERIFY_PKT_HDR_FMT uarthub_plat_ut_ip_verify_pkt_hdr_fmt;
	UARTHUB_PLAT_UT_IP_VERIFY_TRX_NOT_READY uarthub_plat_ut_ip_verify_trx_not_ready;
	UARTHUB_PLAT_SSPM_IRQ_HANDLE uarthub_plat_sspm_irq_handle;
};

typedef int(*UARTHUB_PLAT_IS_READY_STATE) (void);
typedef int(*UARTHUB_PLAT_UARTHUB_INIT) (struct platform_device *pdev);
typedef int(*UARTHUB_PLAT_UARTHUB_EXIT) (void);
typedef int(*UARTHUB_PLAT_UARTHUB_OPEN) (void);
typedef int(*UARTHUB_PLAT_UARTHUB_CLOSE) (void);
typedef int(*UARTHUB_PLAT_CONFIG_HOST_BAUD_RATE) (int dev_index, int rate_index);
typedef int(*UARTHUB_PLAT_CONFIG_CMM_BAUD_RATE) (int rate_index);
typedef int(*UARTHUB_PLAT_IRQ_MASK_CTRL) (int mask);
typedef int(*UARTHUB_PLAT_IRQ_CLEAR_CTRL) (int irq_type);
typedef int(*UARTHUB_PLAT_SSPM_IRQ_CLEAR_CTRL) (int irq_type);
typedef int(*UARTHUB_PLAT_SSPM_IRQ_GET_STA) (void);
typedef int(*UARTHUB_PLAT_SSPM_IRQ_MASK_CTRL) (int irq_type, int is_mask);
typedef int(*UARTHUB_PLAT_IS_APB_BUS_CLK_ENABLE) (void);
typedef int(*UARTHUB_PLAT_IS_UARTHUB_CLK_ENABLE) (void);
typedef int(*UARTHUB_PLAT_SET_HOST_LOOPBACK_CTRL) (int dev_index, int tx_to_rx, int enable);
typedef int(*UARTHUB_PLAT_SET_CMM_LOOPBACK_CTRL) (int tx_to_rx, int enable);
typedef int(*UARTHUB_PLAT_RESET_TO_AP_ENABLE_ONLY) (int ap_only);
typedef int(*UARTHUB_PLAT_RESET_FLOW_CONTROL) (void);
typedef int(*UARTHUB_PLAT_IS_ASSERT_STATE) (void);
typedef int(*UARTHUB_PLAT_ASSERT_STATE_CTRL) (int assert_ctrl);
typedef int(*UARTHUB_PLAT_IS_BYPASS_MODE) (void);
typedef int(*UARTHUB_PLAT_GET_HOST_STATUS) (int dev_index);
typedef int(*UARTHUB_PLAT_GET_HOST_WAKEUP_STATUS) (void);
typedef int(*UARTHUB_PLAT_GET_HOST_SET_FW_OWN_STATUS) (void);
typedef int(*UARTHUB_PLAT_IS_HOST_TRX_IDLE) (int dev_index, enum uarthub_trx_type trx);
typedef int(*UARTHUB_PLAT_SET_HOST_TRX_REQUEST) (int dev_index, enum uarthub_trx_type trx);
typedef int(*UARTHUB_PLAT_CLEAR_HOST_TRX_REQUEST) (int dev_index, enum uarthub_trx_type trx);
typedef int(*UARTHUB_PLAT_GET_HOST_BYTE_CNT) (int dev_index, enum uarthub_trx_type trx);
typedef int(*UARTHUB_PLAT_GET_CMM_BYTE_CNT) (enum uarthub_trx_type trx);
typedef int(*UARTHUB_PLAT_CONFIG_CRC_CTRL) (int enable);
typedef int(*UARTHUB_PLAT_CONFIG_BYPASS_CTRL) (int enable);
typedef int(*UARTHUB_PLAT_CONFIG_HOST_FIFOE_CTRL) (int dev_index, int enable);
typedef int(*UARTHUB_PLAT_GET_RX_ERROR_CRC_INFO) (
		int dev_index, int *p_crc_error_data, int *p_crc_result);
typedef int(*UARTHUB_PLAT_GET_TRX_TIMEOUT_INFO) (
		int dev_index, enum uarthub_trx_type trx,
		int *p_timeout_counter, int *p_pkt_counter);
typedef int(*UARTHUB_PLAT_GET_IRQ_ERR_TYPE) (void);
typedef int(*UARTHUB_PLAT_INIT_REMAP_REG) (void);
typedef int(*UARTHUB_PLAT_DEINIT_UNMAP_REG) (void);
typedef int(*UARTHUB_PLAT_GET_HWCCF_UNIVPLL_ON_INFO) (void);
typedef int(*UARTHUB_PLAT_GET_SPM_SYS_TIMER) (uint32_t *hi, uint32_t *lo);

struct uarthub_core_ops_struct {
	UARTHUB_PLAT_IS_READY_STATE uarthub_plat_is_ready_state;
	UARTHUB_PLAT_UARTHUB_INIT uarthub_plat_uarthub_init;
	UARTHUB_PLAT_UARTHUB_EXIT uarthub_plat_uarthub_exit;
	UARTHUB_PLAT_UARTHUB_OPEN uarthub_plat_uarthub_open;
	UARTHUB_PLAT_UARTHUB_CLOSE uarthub_plat_uarthub_close;
	UARTHUB_PLAT_CONFIG_HOST_BAUD_RATE uarthub_plat_config_host_baud_rate;
	UARTHUB_PLAT_CONFIG_CMM_BAUD_RATE uarthub_plat_config_cmm_baud_rate;
	UARTHUB_PLAT_IRQ_MASK_CTRL uarthub_plat_irq_mask_ctrl;
	UARTHUB_PLAT_IRQ_CLEAR_CTRL uarthub_plat_irq_clear_ctrl;
	UARTHUB_PLAT_SSPM_IRQ_CLEAR_CTRL uarthub_plat_sspm_irq_clear_ctrl;
	UARTHUB_PLAT_SSPM_IRQ_GET_STA uarthub_plat_sspm_irq_get_sta;
	UARTHUB_PLAT_SSPM_IRQ_MASK_CTRL uarthub_plat_sspm_irq_mask_ctrl;
	UARTHUB_PLAT_IS_APB_BUS_CLK_ENABLE uarthub_plat_is_apb_bus_clk_enable;
	UARTHUB_PLAT_IS_UARTHUB_CLK_ENABLE uarthub_plat_is_uarthub_clk_enable;
	UARTHUB_PLAT_SET_HOST_LOOPBACK_CTRL uarthub_plat_set_host_loopback_ctrl;
	UARTHUB_PLAT_SET_CMM_LOOPBACK_CTRL uarthub_plat_set_cmm_loopback_ctrl;
	UARTHUB_PLAT_RESET_TO_AP_ENABLE_ONLY uarthub_plat_reset_to_ap_enable_only;
	UARTHUB_PLAT_RESET_FLOW_CONTROL uarthub_plat_reset_flow_control;
	UARTHUB_PLAT_IS_ASSERT_STATE uarthub_plat_is_assert_state;
	UARTHUB_PLAT_ASSERT_STATE_CTRL uarthub_plat_assert_state_ctrl;
	UARTHUB_PLAT_IS_BYPASS_MODE uarthub_plat_is_bypass_mode;
	UARTHUB_PLAT_GET_HOST_STATUS uarthub_plat_get_host_status;
	UARTHUB_PLAT_GET_HOST_WAKEUP_STATUS uarthub_plat_get_host_wakeup_status;
	UARTHUB_PLAT_GET_HOST_SET_FW_OWN_STATUS uarthub_plat_get_host_set_fw_own_status;
	UARTHUB_PLAT_IS_HOST_TRX_IDLE uarthub_plat_is_host_trx_idle;
	UARTHUB_PLAT_SET_HOST_TRX_REQUEST uarthub_plat_set_host_trx_request;
	UARTHUB_PLAT_CLEAR_HOST_TRX_REQUEST uarthub_plat_clear_host_trx_request;
	UARTHUB_PLAT_GET_HOST_BYTE_CNT uarthub_plat_get_host_byte_cnt;
	UARTHUB_PLAT_GET_CMM_BYTE_CNT uarthub_plat_get_cmm_byte_cnt;
	UARTHUB_PLAT_CONFIG_CRC_CTRL uarthub_plat_config_crc_ctrl;
	UARTHUB_PLAT_CONFIG_BYPASS_CTRL uarthub_plat_config_bypass_ctrl;
	UARTHUB_PLAT_CONFIG_HOST_FIFOE_CTRL uarthub_plat_config_host_fifoe_ctrl;
	UARTHUB_PLAT_GET_RX_ERROR_CRC_INFO uarthub_plat_get_rx_error_crc_info;
	UARTHUB_PLAT_GET_TRX_TIMEOUT_INFO uarthub_plat_get_trx_timeout_info;
	UARTHUB_PLAT_GET_IRQ_ERR_TYPE uarthub_plat_get_irq_err_type;
	UARTHUB_PLAT_INIT_REMAP_REG uarthub_plat_init_remap_reg;
	UARTHUB_PLAT_DEINIT_UNMAP_REG uarthub_plat_deinit_unmap_reg;
	UARTHUB_PLAT_GET_HWCCF_UNIVPLL_ON_INFO uarthub_plat_get_hwccf_univpll_on_info;
	UARTHUB_PLAT_GET_SPM_SYS_TIMER uarthub_plat_get_spm_sys_timer;
};

struct uarthub_ops_struct {
	struct uarthub_core_ops_struct *core_ops;
	struct uarthub_debug_ops_struct *debug_ops;
	struct uarthub_ut_test_ops_struct *ut_test_ops;
};

static char * const UARTHUB_irq_err_type_str[] = {
	"dev0_crc_err",
	"dev1_crc_err",
	"dev2_crc_err",
	"dev0_tx_timeout_err",
	"dev1_tx_timeout_err",
	"dev2_tx_timeout_err",
	"dev0_tx_pkt_type_err",
	"dev1_tx_pkt_type_err",
	"dev2_tx_pkt_type_err",
	"dev0_rx_timeout_err",
	"dev1_rx_timeout_err",
	"dev2_rx_timeout_err",
	"rx_pkt_type_err",
	"intfhub_restore_err",
	"intfhub_dev_rx_err",
	"intfhub_dev0_tx_err",
	"intfhub_dev1_tx_err",
	"intfhub_dev2_tx_err",
	"sspm_2_ap_cr_irq",
	"dev1_2_ap_cr_irq",
	"dev2_2_ap_cr_irq",
	"feedback_tx_timeout_err",
	"feedback_tx_pkt_type_err",
	"dev0_sema_own_rel_irq",
	"dev0_sema_own_timeout_irq",
	"dev1_sema_own_timeout_irq",
	"dev2_sema_own_timeout_irq",
	"feedback_tx_fifo_full_err"
};

/*******************************************************************************
 *                              internal function
 *******************************************************************************/
struct uarthub_ops_struct *uarthub_core_get_platform_ic_ops(struct platform_device *pdev);
int uarthub_core_irq_register(struct platform_device *pdev);
int uarthub_core_get_uarthub_reg(void);
int uarthub_core_check_disable_from_dts(struct platform_device *pdev);
int uarthub_core_config_hub_mode_gpio(void);
int uarthub_core_clk_get_from_dts(struct platform_device *pdev);
int uarthub_core_get_default_baud_rate(int dev_index);
int uarthub_core_check_irq_err_type(void);
int uarthub_core_irq_mask_ctrl(int mask);
int uarthub_core_irq_clear_ctrl(int err_type);

int uarthub_core_crc_ctrl(int enable);
int uarthub_core_is_univpll_on(void);
int uarthub_core_rx_error_crc_info(int dev_index, int *p_crc_error_data, int *p_crc_result);
int uarthub_core_timeout_info(int dev_index, int rx, int *p_timeout_counter, int *p_pkt_counter);
int uarthub_core_config_baud_rate(void __iomem *uarthub_dev_base, int rate_index);
int uarthub_core_reset_to_ap_enable_only(int ap_only);
void uarthub_core_set_trigger_uarthub_error_worker(int err_type, unsigned long err_ts);
int uarthub_core_is_apb_bus_clk_enable(void);
int uarthub_core_is_uarthub_clk_enable(void);
int uarthub_core_debug_apdma_uart_info(const char *tag);
int uarthub_core_debug_info_with_tag_worker(const char *tag);
int uarthub_core_debug_clk_info_worker(const char *tag);
int uarthub_core_debug_byte_cnt_info(const char *tag);
int uarthub_core_debug_clk_info(const char *tag);

int uarthub_dbg_setup(void);
int uarthub_dbg_remove(void);

int uarthub_core_sync_uarthub_irq_sta(int delay_us);
int uarthub_core_handle_ut_test_irq(void);

/*******************************************************************************
 *                              public function
 *******************************************************************************/
int uarthub_core_open(void);
int uarthub_core_close(void);

int uarthub_core_dev0_is_uarthub_ready(const char *tag);
int uarthub_core_get_host_wakeup_status(void);
int uarthub_core_get_host_set_fw_own_status(void);
int uarthub_core_dev0_is_txrx_idle(int rx);
int uarthub_core_dev0_set_tx_request(void);
int uarthub_core_dev0_set_rx_request(void);
int uarthub_core_dev0_set_txrx_request(void);
int uarthub_core_dev0_clear_tx_request(void);
int uarthub_core_dev0_clear_rx_request(void);
int uarthub_core_dev0_clear_txrx_request(void);
int uarthub_core_get_uart_cmm_rx_count(void);
int uarthub_core_is_assert_state(void);

int uarthub_core_irq_register_cb(UARTHUB_CORE_IRQ_CB irq_callback);
int uarthub_core_bypass_mode_ctrl(int enable);
int uarthub_core_md_adsp_fifo_ctrl(int enable);
int uarthub_core_is_bypass_mode(void);
int uarthub_core_config_internal_baud_rate(int dev_index, int rate_index);
int uarthub_core_config_external_baud_rate(int rate_index);
int uarthub_core_assert_state_ctrl(int assert_ctrl);
int uarthub_core_reset_flow_control(void);
int uarthub_core_reset(void);
int uarthub_core_config_host_loopback(int dev_index, int tx_to_rx, int enable);
int uarthub_core_config_cmm_loopback(int tx_to_rx, int enable);
int uarthub_core_debug_info(const char *tag);
int uarthub_core_debug_dump_tx_rx_count(const char *tag, int trigger_point);
int uarthub_core_debug_monitor_stop(int stop);
int uarthub_core_debug_monitor_clr(void);

/* FPGA test API only */
int uarthub_core_is_host_uarthub_ready_state(int dev_index);
int uarthub_core_set_host_txrx_request(int dev_index, int trx);
int uarthub_core_clear_host_txrx_request(int dev_index, int trx);
int uarthub_core_get_host_irq_sta(int dev_index);
int uarthub_core_clear_host_irq(int dev_index);
int uarthub_core_mask_host_irq(int dev_index, int mask_bit, int is_mask);
int uarthub_core_config_host_irq_ctrl(int dev_index, int enable);
int uarthub_core_get_host_rx_fifo_size(int dev_index);
int uarthub_core_get_cmm_rx_fifo_size(void);
int uarthub_core_config_uartip_dma_en_ctrl(int dev_index, int trx, int enable);
int uarthub_core_reset_fifo_trx(void);
int uarthub_core_config_inband_esc_char(int esc_char);
int uarthub_core_config_inband_esc_sta(int esc_sta);
int uarthub_core_config_inband_enable_ctrl(int enable);
int uarthub_core_config_inband_irq_enable_ctrl(int enable);
int uarthub_core_config_inband_trigger(void);
int uarthub_core_is_inband_tx_complete(void);
int uarthub_core_get_inband_irq_sta(void);
int uarthub_core_clear_inband_irq(void);
int uarthub_core_get_received_inband_sta(void);
int uarthub_core_clear_received_inband_sta(void);
int uarthub_core_uartip_write_data_to_tx_buf(int dev_index, int tx_data);
int uarthub_core_uartip_read_data_from_rx_buf(int dev_index);
int uarthub_core_is_uartip_tx_buf_empty_for_write(int dev_index);
int uarthub_core_is_uartip_rx_buf_ready_for_read(int dev_index);
int uarthub_core_is_uartip_throw_xoff(int dev_index);
int uarthub_core_config_uartip_rx_fifo_trig_thr(int dev_index, int size);
int uarthub_core_ut_ip_verify_pkt_hdr_fmt(void);
int uarthub_core_ut_ip_verify_trx_not_ready(void);
int uarthub_core_get_intfhub_active_sta(void);

#endif /* UARTHUB_DRV_CORE_H */
