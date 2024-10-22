// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/kernel.h>

#include "uarthub_drv_export.h"
#include "uarthub_drv_core.h"

#include <linux/string.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/cdev.h>

#include <linux/interrupt.h>
#include <linux/ratelimit.h>


UARTHUB_IRQ_CB g_irq_callback;

static void UARTHUB_irq_error_register_cb(unsigned int err_type)
{
	if (g_irq_callback)
		(*g_irq_callback)(err_type);
}

int UARTHUB_open(void)
{
	return uarthub_core_open();
}
EXPORT_SYMBOL(UARTHUB_open);

int UARTHUB_close(void)
{
	return uarthub_core_close();
}
EXPORT_SYMBOL(UARTHUB_close);

int UARTHUB_dev0_is_uarthub_ready(void)
{
	return uarthub_core_dev0_is_uarthub_ready("HUB_DBG_SetTX_E");
}
EXPORT_SYMBOL(UARTHUB_dev0_is_uarthub_ready);

int UARTHUB_get_host_wakeup_status(void)
{
	return uarthub_core_get_host_wakeup_status();
}
EXPORT_SYMBOL(UARTHUB_get_host_wakeup_status);

int UARTHUB_get_host_set_fw_own_status(void)
{
	return uarthub_core_get_host_set_fw_own_status();
}
EXPORT_SYMBOL(UARTHUB_get_host_set_fw_own_status);

int UARTHUB_dev0_is_txrx_idle(int rx)
{
	return uarthub_core_dev0_is_txrx_idle(rx);
}
EXPORT_SYMBOL(UARTHUB_dev0_is_txrx_idle);

int UARTHUB_dev0_set_tx_request(void)
{
	return uarthub_core_dev0_set_tx_request();
}
EXPORT_SYMBOL(UARTHUB_dev0_set_tx_request);

int UARTHUB_dev0_set_rx_request(void)
{
	return uarthub_core_dev0_set_rx_request();
}
EXPORT_SYMBOL(UARTHUB_dev0_set_rx_request);

int UARTHUB_dev0_set_txrx_request(void)
{
	return uarthub_core_dev0_set_txrx_request();
}
EXPORT_SYMBOL(UARTHUB_dev0_set_txrx_request);

int UARTHUB_dev0_clear_tx_request(void)
{
	return uarthub_core_dev0_clear_tx_request();
}
EXPORT_SYMBOL(UARTHUB_dev0_clear_tx_request);

int UARTHUB_dev0_clear_rx_request(void)
{
	return uarthub_core_dev0_clear_rx_request();
}
EXPORT_SYMBOL(UARTHUB_dev0_clear_rx_request);

int UARTHUB_dev0_clear_txrx_request(void)
{
	return uarthub_core_dev0_clear_txrx_request();
}
EXPORT_SYMBOL(UARTHUB_dev0_clear_txrx_request);

int UARTHUB_get_uart_cmm_rx_count(void)
{
	return uarthub_core_get_uart_cmm_rx_count();
}
EXPORT_SYMBOL(UARTHUB_get_uart_cmm_rx_count);

int UARTHUB_is_assert_state(void)
{
	return uarthub_core_is_assert_state();
}
EXPORT_SYMBOL(UARTHUB_is_assert_state);

int UARTHUB_irq_register_cb(UARTHUB_IRQ_CB irq_callback)
{
	g_irq_callback = irq_callback;
	uarthub_core_irq_register_cb(UARTHUB_irq_error_register_cb);
	return 0;
}
EXPORT_SYMBOL(UARTHUB_irq_register_cb);

int UARTHUB_bypass_mode_ctrl(int enable)
{
	return uarthub_core_bypass_mode_ctrl(enable);
}
EXPORT_SYMBOL(UARTHUB_bypass_mode_ctrl);

int UARTHUB_is_bypass_mode(void)
{
	return uarthub_core_is_bypass_mode();
}
EXPORT_SYMBOL(UARTHUB_is_bypass_mode);

int UARTHUB_config_internal_baud_rate(int dev_index, enum UARTHUB_baud_rate rate)
{
	return uarthub_core_config_internal_baud_rate(dev_index, (int)rate);
}
EXPORT_SYMBOL(UARTHUB_config_internal_baud_rate);

int UARTHUB_config_external_baud_rate(enum UARTHUB_baud_rate rate)
{
	return uarthub_core_config_external_baud_rate((int)rate);
}
EXPORT_SYMBOL(UARTHUB_config_external_baud_rate);

int UARTHUB_assert_state_ctrl(int assert_ctrl)
{
	return uarthub_core_assert_state_ctrl(assert_ctrl);
}
EXPORT_SYMBOL(UARTHUB_assert_state_ctrl);

int UARTHUB_reset_flow_control(void)
{
	return uarthub_core_reset_flow_control();
}
EXPORT_SYMBOL(UARTHUB_reset_flow_control);

int UARTHUB_sw_reset(void)
{
	uarthub_core_reset();
	return 0;
}
EXPORT_SYMBOL(UARTHUB_sw_reset);

int UARTHUB_md_adsp_fifo_ctrl(int enable)
{
	return uarthub_core_md_adsp_fifo_ctrl(enable);
}
EXPORT_SYMBOL(UARTHUB_md_adsp_fifo_ctrl);

int UARTHUB_dump_debug_info(void)
{
	return uarthub_core_debug_info(NULL);
}
EXPORT_SYMBOL(UARTHUB_dump_debug_info);

int UARTHUB_dump_debug_info_with_tag(const char *tag)
{
	return uarthub_core_debug_info(tag);
}
EXPORT_SYMBOL(UARTHUB_dump_debug_info_with_tag);

int UARTHUB_config_host_loopback(int dev_index, int tx_to_rx, int enable)
{
	return uarthub_core_config_host_loopback(dev_index, tx_to_rx, enable);
}
EXPORT_SYMBOL(UARTHUB_config_host_loopback);

int UARTHUB_config_cmm_loopback(int tx_to_rx, int enable)
{
	return uarthub_core_config_cmm_loopback(tx_to_rx, enable);
}
EXPORT_SYMBOL(UARTHUB_config_cmm_loopback);

int UARTHUB_debug_dump_tx_rx_count(const char *tag, enum debug_dump_tx_rx_index trigger_point)
{
	return uarthub_core_debug_dump_tx_rx_count(tag, (int)trigger_point);
}
EXPORT_SYMBOL(UARTHUB_debug_dump_tx_rx_count);


/* FPGA test only */
int UARTHUB_is_host_uarthub_ready_state(int dev_index)
{
	return uarthub_core_is_host_uarthub_ready_state(dev_index);
}
EXPORT_SYMBOL(UARTHUB_is_host_uarthub_ready_state);

int UARTHUB_set_host_txrx_request(int dev_index, int trx)
{
	return uarthub_core_set_host_txrx_request(dev_index, trx);
}
EXPORT_SYMBOL(UARTHUB_set_host_txrx_request);

int UARTHUB_clear_host_txrx_request(int dev_index, int trx)
{
	return uarthub_core_clear_host_txrx_request(dev_index, trx);
}
EXPORT_SYMBOL(UARTHUB_clear_host_txrx_request);

int UARTHUB_get_host_irq_sta(int dev_index)
{
	return uarthub_core_get_host_irq_sta(dev_index);
}
EXPORT_SYMBOL(UARTHUB_get_host_irq_sta);

int UARTHUB_clear_host_irq(int dev_index)
{
	return uarthub_core_clear_host_irq(dev_index);
}
EXPORT_SYMBOL(UARTHUB_clear_host_irq);

int UARTHUB_mask_host_irq(int dev_index, int mask_bit, int is_mask)
{
	return uarthub_core_mask_host_irq(dev_index, mask_bit, is_mask);
}
EXPORT_SYMBOL(UARTHUB_mask_host_irq);

int UARTHUB_config_host_irq_ctrl(int dev_index, int enable)
{
	return uarthub_core_config_host_irq_ctrl(dev_index, enable);
}
EXPORT_SYMBOL(UARTHUB_config_host_irq_ctrl);

int UARTHUB_get_host_rx_fifo_size(int dev_index)
{
	return uarthub_core_get_host_rx_fifo_size(dev_index);
}
EXPORT_SYMBOL(UARTHUB_get_host_rx_fifo_size);

int UARTHUB_get_cmm_rx_fifo_size(void)
{
	return uarthub_core_get_cmm_rx_fifo_size();
}
EXPORT_SYMBOL(UARTHUB_get_cmm_rx_fifo_size);

int UARTHUB_config_uartip_dma_en_ctrl(int dev_index, int trx, int enable)
{
	return uarthub_core_config_uartip_dma_en_ctrl(dev_index, trx, enable);
}
EXPORT_SYMBOL(UARTHUB_config_uartip_dma_en_ctrl);

int UARTHUB_reset_fifo_trx(void)
{
	return uarthub_core_reset_fifo_trx();
}
EXPORT_SYMBOL(UARTHUB_reset_fifo_trx);

int UARTHUB_config_inband_esc_char(int esc_char)
{
	return uarthub_core_config_inband_esc_char(esc_char);
}
EXPORT_SYMBOL(UARTHUB_config_inband_esc_char);

int UARTHUB_config_inband_esc_sta(int esc_sta)
{
	return uarthub_core_config_inband_esc_sta(esc_sta);
}
EXPORT_SYMBOL(UARTHUB_config_inband_esc_sta);

int UARTHUB_config_inband_enable_ctrl(int enable)
{
	return uarthub_core_config_inband_enable_ctrl(enable);
}
EXPORT_SYMBOL(UARTHUB_config_inband_enable_ctrl);

int UARTHUB_config_inband_irq_enable_ctrl(int enable)
{
	return uarthub_core_config_inband_irq_enable_ctrl(enable);
}
EXPORT_SYMBOL(UARTHUB_config_inband_irq_enable_ctrl);

int UARTHUB_config_inband_trigger(void)
{
	return uarthub_core_config_inband_trigger();
}
EXPORT_SYMBOL(UARTHUB_config_inband_trigger);

int UARTHUB_is_inband_tx_complete(void)
{
	return uarthub_core_is_inband_tx_complete();
}
EXPORT_SYMBOL(UARTHUB_is_inband_tx_complete);

int UARTHUB_get_inband_irq_sta(void)
{
	return uarthub_core_get_inband_irq_sta();
}
EXPORT_SYMBOL(UARTHUB_get_inband_irq_sta);

int UARTHUB_clear_inband_irq(void)
{
	return uarthub_core_clear_inband_irq();
}
EXPORT_SYMBOL(UARTHUB_clear_inband_irq);

int UARTHUB_get_received_inband_sta(void)
{
	return uarthub_core_get_received_inband_sta();
}
EXPORT_SYMBOL(UARTHUB_get_received_inband_sta);

int UARTHUB_clear_received_inband_sta(void)
{
	return uarthub_core_clear_received_inband_sta();
}
EXPORT_SYMBOL(UARTHUB_clear_received_inband_sta);

int UARTHUB_uartip_write_data_to_tx_buf(int dev_index, int tx_data)
{
	return uarthub_core_uartip_write_data_to_tx_buf(dev_index, tx_data);
}
EXPORT_SYMBOL(UARTHUB_uartip_write_data_to_tx_buf);

int UARTHUB_uartip_read_data_from_rx_buf(int dev_index)
{
	return uarthub_core_uartip_read_data_from_rx_buf(dev_index);
}
EXPORT_SYMBOL(UARTHUB_uartip_read_data_from_rx_buf);

int UARTHUB_is_uartip_tx_buf_empty_for_writing(int dev_index)
{
	return uarthub_core_is_uartip_tx_buf_empty_for_write(dev_index);
}
EXPORT_SYMBOL(UARTHUB_is_uartip_tx_buf_empty_for_writing);

int UARTHUB_is_uartip_rx_buf_ready_for_reading(int dev_index)
{
	return uarthub_core_is_uartip_rx_buf_ready_for_read(dev_index);
}
EXPORT_SYMBOL(UARTHUB_is_uartip_rx_buf_ready_for_reading);

int UARTHUB_is_uartip_throw_xoff(int dev_index)
{
	return uarthub_core_is_uartip_throw_xoff(dev_index);
}
EXPORT_SYMBOL(UARTHUB_is_uartip_throw_xoff);

int UARTHUB_config_uartip_rx_fifo_trig_threshold(int dev_index, int size)
{
	return uarthub_core_config_uartip_rx_fifo_trig_thr(dev_index, size);
}
EXPORT_SYMBOL(UARTHUB_config_uartip_rx_fifo_trig_threshold);

int UARTHUB_ut_ip_verify_pkt_hdr_fmt(void)
{
	return uarthub_core_ut_ip_verify_pkt_hdr_fmt();
}
EXPORT_SYMBOL(UARTHUB_ut_ip_verify_pkt_hdr_fmt);

int UARTHUB_ut_ip_verify_trx_not_ready(void)
{
	return uarthub_core_ut_ip_verify_trx_not_ready();
}
EXPORT_SYMBOL(UARTHUB_ut_ip_verify_trx_not_ready);

int UARTHUB_get_intfhub_active_sta(void)
{
	return uarthub_core_get_intfhub_active_sta();
}
EXPORT_SYMBOL(UARTHUB_get_intfhub_active_sta);

int UARTHUB_debug_monitor_stop(int stop)
{
	return uarthub_core_debug_monitor_stop(stop);
}
EXPORT_SYMBOL(UARTHUB_debug_monitor_stop);

int UARTHUB_debug_monitor_clr(void)
{
	return uarthub_core_debug_monitor_clr();
}
EXPORT_SYMBOL(UARTHUB_debug_monitor_clr);

int UARTHUB_debug_byte_cnt_info(const char *tag)
{
	return uarthub_core_debug_byte_cnt_info(tag);
}
EXPORT_SYMBOL(UARTHUB_debug_byte_cnt_info);

MODULE_LICENSE("GPL");
