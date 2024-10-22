/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef UARTHUB_DRV_ADAPTER_H
#define UARTHUB_DRV_ADAPTER_H

enum UARTHUB_baud_rate {
	baud_rate_unknown = -1,
	baud_rate_115200 = 115200,
	baud_rate_3m = 3000000,
	baud_rate_4m = 4000000,
	baud_rate_12m = 12000000,
	baud_rate_16m = 16000000,
	baud_rate_24m = 24000000,
};

enum UARTHUB_irq_err_type {
	uarthub_unknown_irq_err = -1,
	dev0_crc_err = 0,
	dev1_crc_err,
	dev2_crc_err,
	dev0_tx_timeout_err,
	dev1_tx_timeout_err,
	dev2_tx_timeout_err,
	dev0_tx_pkt_type_err,
	dev1_tx_pkt_type_err,
	dev2_tx_pkt_type_err,
	dev0_rx_timeout_err,
	dev1_rx_timeout_err,
	dev2_rx_timeout_err,
	rx_pkt_type_err,
	intfhub_restore_err,
	intfhub_dev_rx_err,
	intfhub_dev0_tx_err,
	intfhub_dev1_tx_err,
	intfhub_dev2_tx_err,
	irq_err_type_max,
};

enum debug_dump_tx_rx_index {
	DUMP0 = 0,
	DUMP1,
};

typedef void (*UARTHUB_IRQ_CB) (unsigned int err_type);
typedef int (*UARTHUB_open) (void);
typedef int (*UARTHUB_close) (void);
typedef int (*UARTHUB_dev0_is_uarthub_ready) (void);
typedef int (*UARTHUB_get_host_wakeup_status) (void);
typedef int (*UARTHUB_get_host_set_fw_own_status) (void);
typedef int (*UARTHUB_dev0_is_txrx_idle) (void);
typedef int (*UARTHUB_dev0_set_tx_request) (void);
typedef int (*UARTHUB_dev0_set_rx_request) (void);
typedef int (*UARTHUB_dev0_set_txrx_request) (void);
typedef int (*UARTHUB_dev0_clear_tx_request) (void);
typedef int (*UARTHUB_dev0_clear_rx_request) (void);
typedef int (*UARTHUB_dev0_clear_txrx_request) (void);
typedef int (*UARTHUB_get_uart_cmm_rx_count) (void);
typedef int (*UARTHUB_is_assert_state) (void);
typedef int (*UARTHUB_irq_register_cb) (UARTHUB_IRQ_CB irq_callback);
typedef int (*UARTHUB_bypass_mode_ctrl) (int enable);
typedef int (*UARTHUB_is_bypass_mode) (void);
typedef int (*UARTHUB_config_internal_baud_rate) (int dev_index, enum UARTHUB_baud_rate rate);
typedef int (*UARTHUB_config_external_baud_rate) (enum UARTHUB_baud_rate rate);
typedef int (*UARTHUB_assert_state_ctrl) (int assert_ctrl);
typedef int (*UARTHUB_reset_flow_control) (void);
typedef int (*UARTHUB_sw_reset) (void);
typedef int (*UARTHUB_md_adsp_fifo_ctrl) (int enable);
typedef int (*UARTHUB_dump_debug_info) (void);
typedef int (*UARTHUB_dump_debug_info_with_tag) (const char *tag);
typedef int (*UARTHUB_config_host_loopback) (int dev_index, int tx_to_rx, int enable);
typedef int (*UARTHUB_config_cmm_loopback) (int tx_to_rx, int enable);
typedef int (*UARTHUB_debug_dump_tx_rx_count) (
	const char *tag, enum debug_dump_tx_rx_index trigger_point);

struct uarthub_drv_bridge {
	UARTHUB_open open;
	UARTHUB_close close;
	UARTHUB_dev0_is_uarthub_ready dev0_is_uarthub_ready;
	UARTHUB_get_host_wakeup_status get_host_wakeup_status;
	UARTHUB_get_host_set_fw_own_status get_host_set_fw_own_status;
	UARTHUB_dev0_is_txrx_idle dev0_is_txrx_idle;
	UARTHUB_dev0_set_tx_request dev0_set_tx_request;
	UARTHUB_dev0_set_rx_request dev0_set_rx_request;
	UARTHUB_dev0_set_txrx_request dev0_set_txrx_request;
	UARTHUB_dev0_clear_tx_request dev0_clear_tx_request;
	UARTHUB_dev0_clear_rx_request dev0_clear_rx_request;
	UARTHUB_dev0_clear_txrx_request dev0_clear_txrx_request;
	UARTHUB_get_uart_cmm_rx_count get_uart_cmm_rx_count;
	UARTHUB_is_assert_state is_assert_state;
	UARTHUB_irq_register_cb irq_register_cb;
	UARTHUB_bypass_mode_ctrl bypass_mode_ctrl;
	UARTHUB_is_bypass_mode is_bypass_mode;
	UARTHUB_config_internal_baud_rate config_internal_baud_rate;
	UARTHUB_config_external_baud_rate config_external_baud_rate;
	UARTHUB_assert_state_ctrl assert_state_ctrl;
	UARTHUB_reset_flow_control reset_flow_control;
	UARTHUB_sw_reset sw_reset;
	UARTHUB_md_adsp_fifo_ctrl md_adsp_fifo_ctrl;
	UARTHUB_dump_debug_info dump_debug_info;
	UARTHUB_dump_debug_info_with_tag dump_debug_info_with_tag;
	UARTHUB_config_host_loopback config_host_loopback;
	UARTHUB_config_cmm_loopback config_cmm_loopback;
	UARTHUB_debug_dump_tx_rx_count debug_dump_tx_rx_count;
};

void uarthub_drv_export_bridge_register(struct uarthub_drv_bridge *cb);
void uarthub_drv_export_bridge_unregister(void);

#endif /* UARTHUB_DRV_ADAPTER_H */
