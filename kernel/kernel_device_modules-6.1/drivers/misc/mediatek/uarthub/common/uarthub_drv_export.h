/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef UARTHUB_DRV_EXPORT_H
#define UARTHUB_DRV_EXPORT_H

//#include <mtk_wcn_cmb_stub.h>
#include <linux/types.h>
#include <linux/fs.h>

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

#define KERNEL_UARTHUB_open                       UARTHUB_open
#define KERNEL_UARTHUB_close                      UARTHUB_close

#define KERNEL_UARTHUB_dev0_is_uarthub_ready      UARTHUB_dev0_is_uarthub_ready
#define KERNEL_UARTHUB_get_host_wakeup_status     UARTHUB_get_host_wakeup_status
#define KERNEL_UARTHUB_get_host_set_fw_own_status UARTHUB_get_host_set_fw_own_status
#define KERNEL_UARTHUB_dev0_is_txrx_idle          UARTHUB_dev0_is_txrx_idle
#define KERNEL_UARTHUB_dev0_set_tx_request        UARTHUB_dev0_set_tx_request
#define KERNEL_UARTHUB_dev0_set_rx_request        UARTHUB_dev0_set_rx_request
#define KERNEL_UARTHUB_dev0_set_txrx_request      UARTHUB_dev0_set_txrx_request
#define KERNEL_UARTHUB_dev0_clear_tx_request      UARTHUB_dev0_clear_tx_request
#define KERNEL_UARTHUB_dev0_clear_rx_request      UARTHUB_dev0_clear_rx_request
#define KERNEL_UARTHUB_dev0_clear_txrx_request    UARTHUB_dev0_clear_txrx_request
#define KERNEL_UARTHUB_get_uart_cmm_rx_count      UARTHUB_get_uart_cmm_rx_count
#define KERNEL_UARTHUB_is_assert_state            UARTHUB_is_assert_state

#define KERNEL_UARTHUB_irq_register_cb            UARTHUB_irq_register_cb
#define KERNEL_UARTHUB_bypass_mode_ctrl           UARTHUB_bypass_mode_ctrl
#define KERNEL_UARTHUB_is_bypass_mode             UARTHUB_is_bypass_mode
#define KERNEL_UARTHUB_config_internal_baud_rate  UARTHUB_config_internal_baud_rate
#define KERNEL_UARTHUB_config_external_baud_rate  UARTHUB_config_external_baud_rate
#define KERNEL_UARTHUB_assert_state_ctrl          UARTHUB_assert_state_ctrl
#define KERNEL_UARTHUB_sw_reset                   UARTHUB_sw_reset
#define KERNEL_UARTHUB_md_adsp_fifo_ctrl          UARTHUB_md_adsp_fifo_ctrl
#define KERNEL_UARTHUB_dump_debug_info            UARTHUB_dump_debug_info
#define KERNEL_UARTHUB_dump_debug_info_with_tag   UARTHUB_dump_debug_info_with_tag
#define KERNEL_UARTHUB_config_host_loopback       UARTHUB_config_host_loopback
#define KERNEL_UARTHUB_config_cmm_loopback        UARTHUB_config_cmm_loopback
#define KERNEL_UARTHUB_debug_dump_tx_rx_count     UARTHUB_debug_dump_tx_rx_count
#define KERNEL_UARTHUB_reset_flow_control         UARTHUB_reset_flow_control

int UARTHUB_open(void);
int UARTHUB_close(void);

int UARTHUB_dev0_is_uarthub_ready(void);
int UARTHUB_get_host_wakeup_status(void);
int UARTHUB_get_host_set_fw_own_status(void);
int UARTHUB_dev0_is_txrx_idle(int rx);
int UARTHUB_dev0_set_tx_request(void);
int UARTHUB_dev0_set_rx_request(void);
int UARTHUB_dev0_set_txrx_request(void);
int UARTHUB_dev0_clear_tx_request(void);
int UARTHUB_dev0_clear_rx_request(void);
int UARTHUB_dev0_clear_txrx_request(void);
int UARTHUB_get_uart_cmm_rx_count(void);
int UARTHUB_is_assert_state(void);

int UARTHUB_irq_register_cb(UARTHUB_IRQ_CB irq_callback);
int UARTHUB_bypass_mode_ctrl(int enable);
int UARTHUB_is_bypass_mode(void);
int UARTHUB_config_internal_baud_rate(int dev_index, enum UARTHUB_baud_rate rate);
int UARTHUB_config_external_baud_rate(enum UARTHUB_baud_rate rate);
int UARTHUB_assert_state_ctrl(int assert_ctrl);
int UARTHUB_reset_flow_control(void);
int UARTHUB_sw_reset(void);
int UARTHUB_md_adsp_fifo_ctrl(int enable);
int UARTHUB_dump_debug_info(void);
int UARTHUB_dump_debug_info_with_tag(const char *tag);
int UARTHUB_config_host_loopback(int dev_index, int tx_to_rx, int enable);
int UARTHUB_config_cmm_loopback(int tx_to_rx, int enable);
int UARTHUB_debug_dump_tx_rx_count(const char *tag, enum debug_dump_tx_rx_index trigger_point);
int UARTHUB_debug_monitor_stop(int stop);
int UARTHUB_debug_monitor_clr(void);

/* FPGA test only */
int UARTHUB_is_host_uarthub_ready_state(int dev_index);
int UARTHUB_set_host_txrx_request(int dev_index, int trx);
int UARTHUB_clear_host_txrx_request(int dev_index, int trx);
int UARTHUB_get_host_irq_sta(int dev_index);
int UARTHUB_clear_host_irq(int dev_index);
int UARTHUB_mask_host_irq(int dev_index, int mask_bit, int is_mask);
int UARTHUB_config_host_irq_ctrl(int dev_index, int enable);
int UARTHUB_get_host_rx_fifo_size(int dev_index);
int UARTHUB_get_cmm_rx_fifo_size(void);
int UARTHUB_config_uartip_dma_en_ctrl(int dev_index, int trx, int enable);
int UARTHUB_reset_fifo_trx(void);
int UARTHUB_config_inband_esc_char(int esc_char);
int UARTHUB_config_inband_esc_sta(int esc_sta);
int UARTHUB_config_inband_enable_ctrl(int enable);
int UARTHUB_config_inband_irq_enable_ctrl(int enable);
int UARTHUB_config_inband_trigger(void);
int UARTHUB_is_inband_tx_complete(void);
int UARTHUB_get_inband_irq_sta(void);
int UARTHUB_clear_inband_irq(void);
int UARTHUB_get_received_inband_sta(void);
int UARTHUB_clear_received_inband_sta(void);
int UARTHUB_uartip_write_data_to_tx_buf(int dev_index, int tx_data);
int UARTHUB_uartip_read_data_from_rx_buf(int dev_index);
int UARTHUB_is_uartip_tx_buf_empty_for_writing(int dev_index);
int UARTHUB_is_uartip_rx_buf_ready_for_reading(int dev_index);
int UARTHUB_is_uartip_throw_xoff(int dev_index);
int UARTHUB_config_uartip_rx_fifo_trig_threshold(int dev_index, int size);
int UARTHUB_ut_ip_verify_pkt_hdr_fmt(void);
int UARTHUB_ut_ip_verify_trx_not_ready(void);
int UARTHUB_get_intfhub_active_sta(void);
int UARTHUB_debug_byte_cnt_info(const char *tag);

#endif /* UARTHUB_DRV_EXPORT_H */
