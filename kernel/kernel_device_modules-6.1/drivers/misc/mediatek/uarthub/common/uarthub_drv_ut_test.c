// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/sched/clock.h>

#include "uarthub_drv_core.h"
#include "uarthub_drv_export.h"

#include <linux/regmap.h>

/* FPGA test API only */
int uarthub_core_set_host_txrx_request(int dev_index, int trx)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_set_host_trx_request == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d], trx=[%d]\n", __func__, dev_index, trx);
#endif
	g_plat_ic_core_ops->uarthub_plat_set_host_trx_request(dev_index, trx);

	if (g_is_ut_testing == 1) {
		usleep_range(50, 60);
		pr_info("[%s] is_ready=[%d]\n",
			__func__, uarthub_core_dev0_is_uarthub_ready("HUB_DBG_ut_SetTRX"));
	}

	return 0;
}

int uarthub_core_clear_host_txrx_request(int dev_index, int trx)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_core_ops == NULL ||
		  g_plat_ic_core_ops->uarthub_plat_clear_host_trx_request == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d], trx=[%d]\n", __func__, dev_index, trx);
#endif
	g_plat_ic_core_ops->uarthub_plat_clear_host_trx_request(dev_index, trx);

	if (g_is_ut_testing == 1) {
		usleep_range(50, 60);
		pr_info("[%s] is_ready=[%d]\n",
			__func__, uarthub_core_dev0_is_uarthub_ready("HUB_DBG_ut_ClrTRX"));
	}

	return 0;
}

int uarthub_core_is_host_uarthub_ready_state(int dev_index)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_is_host_uarthub_ready_state == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d]\n", __func__, dev_index);
#endif
	return g_plat_ic_ut_test_ops->uarthub_plat_is_host_uarthub_ready_state(dev_index);
}

int uarthub_core_get_host_irq_sta(int dev_index)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_get_host_irq_sta == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	if (dev_index == 0 && g_is_ut_testing == 1) {
#if UARTHUB_INFO_LOG
		pr_info("[%s] dev_index=[%d], g_dev0_irq_sta=[0x%x/0x%x]\n",
			__func__, dev_index, g_dev0_irq_sta,
			g_plat_ic_ut_test_ops->uarthub_plat_get_host_irq_sta(0));
#endif
		return g_dev0_irq_sta;
	}

	state = g_plat_ic_ut_test_ops->uarthub_plat_get_host_irq_sta(dev_index);
#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d], uarthub_plat_get_host_irq_sta=[0x%x]\n",
		__func__, dev_index, state);
#endif
	return state;
}

int uarthub_core_clear_host_irq(int dev_index)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_clear_host_irq == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d]\n", __func__, dev_index);
#endif
	state = g_plat_ic_ut_test_ops->uarthub_plat_clear_host_irq(dev_index, BIT_0xFFFF_FFFF);

	if (dev_index == 0 && g_is_ut_testing == 1)
		uarthub_core_sync_uarthub_irq_sta(50);

	return state;
}

int uarthub_core_mask_host_irq(int dev_index, int mask_bit, int is_mask)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_mask_host_irq == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d], mask_bit=[0x%x], is_mask=[%d]\n",
		__func__, dev_index, mask_bit, is_mask);
#endif
	state = g_plat_ic_ut_test_ops->uarthub_plat_mask_host_irq(dev_index, mask_bit, is_mask);

	return state;
}

int uarthub_core_config_host_irq_ctrl(int dev_index, int enable)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_config_host_irq_ctrl == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d], enable=[%d]\n", __func__, dev_index, enable);
#endif
	state = g_plat_ic_ut_test_ops->uarthub_plat_config_host_irq_ctrl(dev_index, enable);

	return state;
}

int uarthub_core_get_host_rx_fifo_size(int dev_index)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_get_host_rx_fifo_size == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d]\n", __func__, dev_index);
#endif
	return g_plat_ic_ut_test_ops->uarthub_plat_get_host_rx_fifo_size(dev_index);
}

int uarthub_core_get_cmm_rx_fifo_size(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_get_cmm_rx_fifo_size == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_ut_test_ops->uarthub_plat_get_cmm_rx_fifo_size();

#if UARTHUB_INFO_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_config_uartip_dma_en_ctrl(int dev_index, int trx, int enable)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_config_uartip_dma_en_ctrl == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] enable=[%d]\n", __func__, enable);
#endif
	return g_plat_ic_ut_test_ops->uarthub_plat_config_uartip_dma_en_ctrl(
		dev_index, trx, enable);
}

int uarthub_core_reset_fifo_trx(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_reset_fifo_trx == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_ut_test_ops->uarthub_plat_reset_fifo_trx();

#if UARTHUB_INFO_LOG
	pr_info("[%s] state=[0x%x]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_config_inband_esc_char(int esc_char)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_config_inband_esc_char == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] esc_char=[0x%x]\n", __func__, esc_char);
#endif
	return g_plat_ic_ut_test_ops->uarthub_plat_config_inband_esc_char(esc_char);
}

int uarthub_core_config_inband_esc_sta(int esc_sta)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_config_inband_esc_sta == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] esc_sta=[0x%x]\n", __func__, esc_sta);
#endif
	return g_plat_ic_ut_test_ops->uarthub_plat_config_inband_esc_sta(esc_sta);
}

int uarthub_core_config_inband_enable_ctrl(int enable)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_config_inband_enable_ctrl == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] enable=[%d]\n", __func__, enable);
#endif
	state = g_plat_ic_ut_test_ops->uarthub_plat_config_inband_enable_ctrl(enable);

	if (enable == 1 && g_is_ut_testing == 1)
		uarthub_core_sync_uarthub_irq_sta(50);

	return state;
}

int uarthub_core_config_inband_irq_enable_ctrl(int enable)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_config_inband_irq_enable_ctrl == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] enable=[%d]\n", __func__, enable);
#endif
	return g_plat_ic_ut_test_ops->uarthub_plat_config_inband_irq_enable_ctrl(enable);
}

int uarthub_core_config_inband_trigger(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_config_inband_trigger == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_ut_test_ops->uarthub_plat_config_inband_trigger();

#if UARTHUB_INFO_LOG
	pr_info("[%s] state=[0x%x]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_is_inband_tx_complete(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_is_inband_tx_complete == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_ut_test_ops->uarthub_plat_is_inband_tx_complete();

#if UARTHUB_INFO_LOG
	pr_info("[%s] state=[0x%x]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_get_inband_irq_sta(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_get_inband_irq_sta == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	if (g_is_ut_testing == 1) {
#if UARTHUB_INFO_LOG
		pr_info("[%s] g_dev0_inband_irq_sta=[0x%x/0x%x]\n",
			__func__, g_dev0_inband_irq_sta,
			g_plat_ic_ut_test_ops->uarthub_plat_get_inband_irq_sta());
#endif
		return g_dev0_inband_irq_sta;
	}

	state = g_plat_ic_ut_test_ops->uarthub_plat_get_inband_irq_sta();
#if UARTHUB_INFO_LOG
	pr_info("[%s] uarthub_plat_get_inband_irq_sta=[0x%x]\n",
		__func__, state);
#endif
	return state;
}

int uarthub_core_clear_inband_irq(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_clear_inband_irq == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_ut_test_ops->uarthub_plat_clear_inband_irq();

#if UARTHUB_INFO_LOG
	pr_info("[%s] state=[0x%x]\n", __func__, state);
#endif

	if (g_is_ut_testing == 1)
		uarthub_core_sync_uarthub_irq_sta(50);

	return state;
}

int uarthub_core_get_received_inband_sta(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_get_received_inband_sta == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_ut_test_ops->uarthub_plat_get_received_inband_sta();

#if UARTHUB_INFO_LOG
	pr_info("[%s] state=[0x%x]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_clear_received_inband_sta(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_clear_received_inband_sta == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_ut_test_ops->uarthub_plat_clear_received_inband_sta();

#if UARTHUB_INFO_LOG
	pr_info("[%s] state=[0x%x]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_uartip_write_data_to_tx_buf(int dev_index, int tx_data)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_uartip_write_data_to_tx_buf == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d], tx_data=[0x%x]\n", __func__, dev_index, tx_data);
#endif
	return g_plat_ic_ut_test_ops->uarthub_plat_uartip_write_data_to_tx_buf(dev_index, tx_data);
}

int uarthub_core_uartip_read_data_from_rx_buf(int dev_index)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_uartip_read_data_from_rx_buf == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d]\n", __func__, dev_index);
#endif
	return g_plat_ic_ut_test_ops->uarthub_plat_uartip_read_data_from_rx_buf(dev_index);
}

int uarthub_core_is_uartip_tx_buf_empty_for_write(int dev_index)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_is_uartip_tx_buf_empty_for_write == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d]\n", __func__, dev_index);
#endif
	return g_plat_ic_ut_test_ops->uarthub_plat_is_uartip_tx_buf_empty_for_write(dev_index);
}

int uarthub_core_is_uartip_rx_buf_ready_for_read(int dev_index)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_is_uartip_rx_buf_ready_for_read == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d]\n", __func__, dev_index);
#endif
	return g_plat_ic_ut_test_ops->uarthub_plat_is_uartip_rx_buf_ready_for_read(dev_index);
}

int uarthub_core_is_uartip_throw_xoff(int dev_index)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_is_uartip_throw_xoff == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d]\n", __func__, dev_index);
#endif
	return g_plat_ic_ut_test_ops->uarthub_plat_is_uartip_throw_xoff(dev_index);
}

int uarthub_core_config_uartip_rx_fifo_trig_thr(int dev_index, int size)
{
	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_config_uartip_rx_fifo_trig_thr == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

#if UARTHUB_INFO_LOG
	pr_info("[%s] dev_index=[%d], size=[0x%x]\n", __func__, dev_index, size);
#endif
	return g_plat_ic_ut_test_ops->uarthub_plat_config_uartip_rx_fifo_trig_thr(
		dev_index, size);
}

int uarthub_core_ut_ip_verify_pkt_hdr_fmt(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_ut_ip_verify_pkt_hdr_fmt == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_ut_test_ops->uarthub_plat_ut_ip_verify_pkt_hdr_fmt();

#if UARTHUB_INFO_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_ut_ip_verify_trx_not_ready(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_ut_ip_verify_trx_not_ready == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_ut_test_ops->uarthub_plat_ut_ip_verify_trx_not_ready();

#if UARTHUB_INFO_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_get_intfhub_active_sta(void)
{
	int state = 0;

	if (g_uarthub_disable == 1)
		return 0;

	if (g_plat_ic_ut_test_ops == NULL ||
		  g_plat_ic_ut_test_ops->uarthub_plat_get_intfhub_active_sta == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (uarthub_core_is_apb_bus_clk_enable() == 0) {
		pr_notice("[%s] apb bus clk disable\n", __func__);
		return UARTHUB_ERR_APB_BUS_CLK_DISABLE;
	}

	state = g_plat_ic_ut_test_ops->uarthub_plat_get_intfhub_active_sta();

#if UARTHUB_INFO_LOG
	pr_info("[%s] state=[%d]\n", __func__, state);
#endif

	return state;
}

int uarthub_core_sync_uarthub_irq_sta(int delay_us)
{
	unsigned char dmp_info_buf[DBG_LOG_LEN];
	int len = 0;
	int id = 0;
	int dev0_irq_sta = 0;
	int err_type = 0;
	int err_index = 0;
	int err_total = 0;
	unsigned long err_ts, rem_nsec;

	if (!g_plat_ic_ut_test_ops) {
		pr_notice("[%s] g_plat_ic_ut_test_ops is NULL\n", __func__);
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;
	}

	if (!g_plat_ic_core_ops) {
		pr_notice("[%s] g_plat_ic_ut_test_ops is NULL\n", __func__);
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;
	}

	if (g_plat_ic_ut_test_ops->uarthub_plat_get_host_irq_sta == NULL)
		return UARTHUB_ERR_PLAT_API_NOT_EXIST;

	if (delay_us > 0)
		usleep_range(delay_us, (delay_us + 10));

	dev0_irq_sta = g_plat_ic_ut_test_ops->uarthub_plat_get_host_irq_sta(0);
	err_type = (dev0_irq_sta & BIT_0x7FFF_FFFF);
	err_ts = sched_clock();
	rem_nsec = do_div(err_ts, 1000000000);

	pr_info("[%s] err_type=[0x%x] err_time=[%5lu.%06lu]\n",
		__func__, err_type, err_ts, (rem_nsec/1000));
	err_total = 0;
	for (id = 0; id < irq_err_type_max; id++) {
		if (((err_type >> id) & 0x1) == 0x1)
			err_total++;
	}

	if (err_total > 0) {
		err_index = 0;
		for (id = 0; id < irq_err_type_max; id++) {
			if (((err_type >> id) & 0x1) == 0x1) {
				err_index++;
				pr_info("[%s] %d-%d, err_id=[%d], reason=[%s]\n",
					__func__, err_total, err_index,
					id, UARTHUB_irq_err_type_str[id]);
			}
		}
#if UARTHUB_INFO_LOG
		uarthub_core_debug_info_with_tag_worker(__func__);
#endif
	}

	g_dev0_irq_sta = g_dev0_irq_sta | dev0_irq_sta;
	if ((g_dev0_irq_sta & BIT_0x7FFF_FFFF) != 0x0)
		g_dev0_irq_sta = (g_dev0_irq_sta & BIT_0x7FFF_FFFF);

	len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
		"[%s] irq_sta_dev0=[0x%x]", __func__, g_dev0_irq_sta);

	if (g_plat_ic_ut_test_ops->uarthub_plat_get_inband_irq_sta) {
		g_dev0_inband_irq_sta =
			g_plat_ic_ut_test_ops->uarthub_plat_get_inband_irq_sta();

		len += snprintf(dmp_info_buf + len, DBG_LOG_LEN - len,
			", inband_irq_sta=[0x%x]", g_dev0_inband_irq_sta);
	}

	pr_info("%s\n", dmp_info_buf);

	return 0;
}

int uarthub_core_handle_ut_test_irq(void)
{
	int err_type = -1;
	int sspm_irq_type = -1;

	if (g_is_ut_testing == 1) {
		/* mask sspm irq */
		g_plat_ic_core_ops->uarthub_plat_sspm_irq_mask_ctrl(BIT_0xFFFF_FFFF, 1);
		/* disable inband irq */
		if (g_plat_ic_ut_test_ops &&
				g_plat_ic_ut_test_ops->uarthub_plat_config_inband_irq_enable_ctrl)
			g_plat_ic_ut_test_ops->uarthub_plat_config_inband_irq_enable_ctrl(0);
		/* sync irq sta */
		uarthub_core_sync_uarthub_irq_sta(0);
		/* handle sspm irq */
		if (g_plat_ic_ut_test_ops &&
				g_plat_ic_ut_test_ops->uarthub_plat_sspm_irq_handle &&
				g_plat_ic_core_ops->uarthub_plat_sspm_irq_get_sta) {
			err_type = g_plat_ic_core_ops->uarthub_plat_sspm_irq_get_sta();
			if (err_type > 0)
				sspm_irq_type =
					g_plat_ic_ut_test_ops->uarthub_plat_sspm_irq_handle(
					err_type);
		}
		/* clear dev0 irq */
		g_plat_ic_core_ops->uarthub_plat_irq_clear_ctrl(g_dev0_irq_sta);
		/* clear sspm irq */
		g_plat_ic_core_ops->uarthub_plat_sspm_irq_clear_ctrl(BIT_0xFFFF_FFFF);
		if (sspm_irq_type >= 0)
			uarthub_core_debug_info_with_tag_worker(__func__);
		/* clear inband irq */
		if (g_plat_ic_ut_test_ops &&
				g_plat_ic_ut_test_ops->uarthub_plat_clear_inband_irq)
			g_plat_ic_ut_test_ops->uarthub_plat_clear_inband_irq();
		/* unmask dev0 irq */
		g_plat_ic_core_ops->uarthub_plat_irq_mask_ctrl(0);
		/* unmask sspm irq */
		g_plat_ic_core_ops->uarthub_plat_sspm_irq_mask_ctrl(BIT_0xFFFF_FFFF, 0);
	  /* enable inband irq */
		if (g_plat_ic_ut_test_ops &&
				g_plat_ic_ut_test_ops->uarthub_plat_config_inband_irq_enable_ctrl)
			g_plat_ic_ut_test_ops->uarthub_plat_config_inband_irq_enable_ctrl(1);
	}

	return g_is_ut_testing;
}
