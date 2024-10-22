// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/regmap.h>

#include "uarthub_drv_core.h"
#include "../inc/mt6989.h"
#include "../inc/mt6989_debug.h"
#include "mt6989_ut_test.h"
#include "ut_util.h"

struct uarthub_ut_test_ops_struct mt6989_plat_ut_test_data = {
	.uarthub_plat_is_ut_testing = uarthub_is_ut_testing_mt6989,
	.uarthub_plat_is_host_uarthub_ready_state = uarthub_is_host_uarthub_ready_state_mt6989,
	.uarthub_plat_get_host_irq_sta = uarthub_get_host_irq_sta_mt6989,
	.uarthub_plat_get_intfhub_active_sta = uarthub_get_intfhub_active_sta_mt6989,
	.uarthub_plat_clear_host_irq = uarthub_clear_host_irq_mt6989,
	.uarthub_plat_mask_host_irq = uarthub_mask_host_irq_mt6989,
	.uarthub_plat_config_host_irq_ctrl = uarthub_config_host_irq_ctrl_mt6989,
	.uarthub_plat_get_host_rx_fifo_size = uarthub_get_host_rx_fifo_size_mt6989,
	.uarthub_plat_get_cmm_rx_fifo_size = uarthub_get_cmm_rx_fifo_size_mt6989,
	.uarthub_plat_config_uartip_dma_en_ctrl = uarthub_config_uartip_dma_en_ctrl_mt6989,
	.uarthub_plat_reset_fifo_trx = uarthub_reset_fifo_trx_mt6989,
	.uarthub_plat_uartip_write_data_to_tx_buf = uarthub_uartip_write_data_to_tx_buf_mt6989,
	.uarthub_plat_uartip_read_data_from_rx_buf = uarthub_uartip_read_data_from_rx_buf_mt6989,
	.uarthub_plat_is_uartip_tx_buf_empty_for_write =
		uarthub_is_uartip_tx_buf_empty_for_write_mt6989,
	.uarthub_plat_is_uartip_rx_buf_ready_for_read =
		uarthub_is_uartip_rx_buf_ready_for_read_mt6989,
	.uarthub_plat_is_uartip_throw_xoff = uarthub_is_uartip_throw_xoff_mt6989,
	.uarthub_plat_config_uartip_rx_fifo_trig_thr =
		uarthub_config_uartip_rx_fifo_trig_thr_mt6989,
};


int uarthub_is_ut_testing_mt6989(void)
{
#if (UARTHUB_SUPPORT_FPGA) || (UARTHUB_SUPPORT_DVT)
	return 1;
#else
	return 0;
#endif
}

int uarthub_is_host_uarthub_ready_state_mt6989(int dev_index)
{
	int state = 0;

	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (dev_index == 0)
		state = DEV0_STA_GET_dev0_intfhub_ready(DEV0_STA_ADDR);
	else if (dev_index == 1)
		state = DEV1_STA_GET_dev1_intfhub_ready(DEV1_STA_ADDR);
	else if (dev_index == 2)
		state = DEV2_STA_GET_dev2_intfhub_ready(DEV2_STA_ADDR);

	return state;
}

int uarthub_get_host_irq_sta_mt6989(int dev_index)
{
	if (dev_index != 0) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	return UARTHUB_REG_READ(DEV0_IRQ_STA_ADDR);
}

int uarthub_get_intfhub_active_sta_mt6989(void)
{
	return STA0_GET_intfhub_active(STA0_ADDR);
}

int uarthub_clear_host_irq_mt6989(int dev_index, int irq_type)
{
	if (dev_index != 0) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	UARTHUB_REG_WRITE(DEV0_IRQ_CLR_ADDR, irq_type);
	return 0;
}

int uarthub_mask_host_irq_mt6989(int dev_index, int irq_type, int is_mask)
{
	if (dev_index != 0) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	UARTHUB_REG_WRITE_MASK(
		DEV0_IRQ_MASK_ADDR, ((is_mask == 1) ? irq_type : 0x0), irq_type);

	return 0;
}

int uarthub_config_host_irq_ctrl_mt6989(int dev_index, int enable)
{
	if (dev_index != 0) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	UARTHUB_REG_WRITE(DEV0_IRQ_MASK_ADDR, ((enable == 1) ? 0x0 : 0xFFFFFFFF));
	return 0;
}

int uarthub_get_host_rx_fifo_size_mt6989(int dev_index)
{
	if (dev_index < 0 || dev_index >= UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	return UARTHUB_REG_READ_BIT(DEBUG_7(uartip_base_map[dev_index]), 0x3F);
}

int uarthub_get_cmm_rx_fifo_size_mt6989(void)
{
	return UARTHUB_REG_READ_BIT(DEBUG_7(uartip_base_map[uartip_id_cmm]), 0x3F);
}

int uarthub_config_uartip_dma_en_ctrl_mt6989(int dev_index, enum uarthub_trx_type trx, int enable)
{
	void __iomem *uarthub_dev_base = uartip_base_map[uartip_id_ap];
	int i = 0;

	if (dev_index < -1 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	if (trx > TRX) {
		pr_notice("[%s] not support trx_type(%d)\n", __func__, trx);
		return UARTHUB_ERR_ENUM_NOT_SUPPORT;
	}

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++) {
		if (dev_index >= 0 && dev_index != i)
			continue;

		uarthub_dev_base = uartip_base_map[i];

		if (trx == RX)
			DMA_EN_SET_RX_DMA_EN(DMA_EN_ADDR(uarthub_dev_base),
				((enable == 0) ? 0x0 : 0x1));
		else if (trx == TX)
			DMA_EN_SET_TX_DMA_EN(DMA_EN_ADDR(uarthub_dev_base),
				((enable == 0) ? 0x0 : 0x1));
		else if (trx == TRX)
			UARTHUB_REG_WRITE_MASK(DMA_EN_ADDR(uarthub_dev_base),
				(enable == 0) ? 0x0 :
					(REG_FLD_MASK(DMA_EN_FLD_RX_DMA_EN) |
						REG_FLD_MASK(DMA_EN_FLD_TX_DMA_EN)),
				(REG_FLD_MASK(DMA_EN_FLD_RX_DMA_EN) |
					REG_FLD_MASK(DMA_EN_FLD_TX_DMA_EN)));
	}

	return 0;
}

int uarthub_reset_fifo_trx_mt6989(void)
{
	int i = 0;

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++)
		UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map[i]), 0x80);

	usleep_range(50, 60);

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++)
		UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map[i]), 0x87);

	return 0;
}

int uarthub_reset_intfhub_mt6989(void)
{
	int i = 0;

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++)
		UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map[i]), 0x80);

	CON4_SET_sw4_rst(CON4_ADDR, 1);

	for (i = 0; i <= UARTHUB_MAX_NUM_DEV_HOST; i++)
		UARTHUB_REG_WRITE(FCR_ADDR(uartip_base_map[i]), 0x81);

	return 0;
}

int uarthub_uartip_write_data_to_tx_buf_mt6989(int dev_index, int tx_data)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	UARTHUB_REG_WRITE(THR_ADDR(uartip_base_map[dev_index]), tx_data);
	return 0;
}

int uarthub_uartip_read_data_from_rx_buf_mt6989(int dev_index)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	return UARTHUB_REG_READ(RBR_ADDR(uartip_base_map[dev_index]));
}

int uarthub_is_uartip_tx_buf_empty_for_write_mt6989(int dev_index)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	return LSR_GET_TEMT(LSR_ADDR(uartip_base_map[dev_index]));
}

int uarthub_is_uartip_rx_buf_ready_for_read_mt6989(int dev_index)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	return LSR_GET_DR(LSR_ADDR(uartip_base_map[dev_index]));
}

int uarthub_is_uartip_throw_xoff_mt6989(int dev_index)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	return ((UARTHUB_DEBUG_GET_XCSTATE_WSEND_XOFF_VAL(
		uartip_base_map[dev_index]) & 0x6) ? 1 : 0);
}

int uarthub_config_uartip_rx_fifo_trig_thr_mt6989(int dev_index, int size)
{
	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	pr_info("[%s] FCR_RD_ADDR_1=[0x%x]\n",
		__func__, UARTHUB_REG_READ(FCR_RD_ADDR(uartip_base_map[dev_index])));

	REG_FLD_RD_SET(FCR_FLD_RFTL1_RFTL0,
		FCR_RD_ADDR(uartip_base_map[dev_index]),
		FCR_ADDR(uartip_base_map[dev_index]), size);

	pr_info("[%s] FCR_RD_ADDR_2=[0x%x]\n",
		__func__, UARTHUB_REG_READ(FCR_RD_ADDR(uartip_base_map[dev_index])));

	return 0;
}

int uarthub_uartip_write_tx_data_mt6989(int dev_index, unsigned char *p_tx_data, int tx_len)
{
	int retry = 0;
	int i = 0;

	if (!p_tx_data || tx_len == 0)
		return UARTHUB_ERR_PARA_WRONG;

	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	for (i = 0; i < tx_len; i++) {
		retry = 200;
		while (retry-- > 0) {
			if (uarthub_is_uartip_tx_buf_empty_for_write_mt6989(dev_index) == 1) {
				uarthub_uartip_write_data_to_tx_buf_mt6989(dev_index, p_tx_data[i]);
				break;
			}
			usleep_range(5, 6);
		}

		if (retry <= 0)
			return UARTHUB_UT_ERR_TX_FAIL;
	}

	return 0;
}

int uarthub_uartip_read_rx_data_mt6989(
	int dev_index, unsigned char *p_rx_data, int rx_len, int *p_recv_rx_len)
{
	int retry = 0;
	int i = 0;
	int state = 0;

	if (!p_rx_data || !p_recv_rx_len || rx_len <= 0)
		return UARTHUB_ERR_PARA_WRONG;

	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	for (i = 0; i < rx_len; i++) {
		retry = 200;
		while (retry-- > 0) {
			if (uarthub_is_uartip_rx_buf_ready_for_read_mt6989(dev_index) == 1) {
				p_rx_data[i] =
					uarthub_uartip_read_data_from_rx_buf_mt6989(dev_index);
				*p_recv_rx_len = i+1;
				break;
			}
			usleep_range(5, 6);
		}

		if (retry <= 0) {
			state = 0;
			if (dev_index == 3)
				state = uarthub_get_cmm_rx_fifo_size_mt6989();
			else
				state = uarthub_get_host_rx_fifo_size_mt6989(dev_index);
			if (state > 0)
				retry = 200;
			else
				break;
		}
	}

	return 0;
}

int uarthub_get_crc(unsigned char *pData, int length, unsigned char *pData_CRC)
{
	int i = 0;
	int crc_result = 0xFFFF;

	while (length--) {
		crc_result ^= *(unsigned char *)pData++ << 8;
		for (i = 0; i < 8; i++) {
			crc_result = ((crc_result & 0x8000) ?
				((crc_result << 1) ^ 0x1021) : (crc_result << 1));
		}
	}

	pData_CRC[0] = (crc_result & 0xFF);
	pData_CRC[1] = ((crc_result & 0xFF00) >> 8);

	return 0;
}

enum uarthub_pkt_fmt_type uarthub_check_packet_format(int dev_index, unsigned char byte1)
{
	enum uarthub_pkt_fmt_type type = pkt_fmt_undef;

	if (dev_index == 0 && ((byte1 >= 0x1 && byte1 <= 0x5) || byte1 == 0x40 || byte1 == 0x41))
		type = pkt_fmt_dev0;
	else if (dev_index == 1 && (byte1 == 0x86 || byte1 == 0x87))
		type = pkt_fmt_dev1;
	else if (dev_index == 2 && (byte1 >= 0x81 && byte1 <= 0x85))
		type = pkt_fmt_dev2;
	else if (byte1 == 0xff)
		type = pkt_fmt_esp;
	else if (dev_index == 3 && (byte1 == 0xf0 || byte1 == 0xf1 || byte1 == 0xf2))
		type = pkt_fmt_esp;

	return type;
}

int uarthub_check_packet_is_complete(int dev_index, unsigned char *pData, int length)
{
	enum uarthub_pkt_fmt_type type = pkt_fmt_undef;
	int payload_len = 0;
	int pkt_len = 0;
	unsigned char rx_data_crc[2] = { 0 };

	if (pData == NULL || length <= 0)
		return 0;

	type = uarthub_check_packet_format(dev_index, pData[0]);

	if (type == pkt_fmt_esp)
		return ((length == 1) ? 1 : 0);

	if (length < 4)
		return 0;

	if ((type == pkt_fmt_dev0 && pData[0] == 0x1) ||
			(type == pkt_fmt_dev2 && pData[0] == 0x81)) {
		if (length < 5)
			return 0;
		payload_len = pData[3];
		pkt_len = payload_len + 4;
	} else if ((type == pkt_fmt_dev0 && (pData[0] == 0x3 || pData[0] == 0x4)) ||
			(type == pkt_fmt_dev2 && (pData[0] == 0x83 || pData[0] == 0x84))) {
		if (length < 4)
			return 0;
		payload_len = pData[2];
		pkt_len = payload_len + 3;
	} else {
		if (length < 6)
			return 0;
		payload_len = ((pData[4] << 8) | pData[3]);
		pkt_len = payload_len + 5;
	}

	if (length == (pkt_len + 2)) {
		uarthub_get_crc(pData, pkt_len, rx_data_crc);
		if (pData[length-2] == rx_data_crc[0] && pData[length-1] == rx_data_crc[1])
			return 1;
	} else if (length == pkt_len)
		return 1;

	return 0;
}

/* __uh_ut_data_to_string:
 *	format print data to char string.
 */
static int __uh_ut_data_to_string(char *str, const unsigned char *pData, int sz, int szMax)
{
	int i, len = 0;

	for (i = 0; i < sz && (szMax - len) > 0; i++)
		len += snprintf(str+len, szMax-len, "0x%x ", pData[i]);
	str[len] = 0;
	return len;
}

/*
 *  __uh_ut_comparing_data: verfiy the packet context with the targeted one.
 * parameter:
 *    pData, szData: the packet and byte length to be verified.
 *    pTarget: the given targeted packet.
 *    szCompare: the length for comparing. No longer than the size of pTarget.
 * note:
 *    szCompare is at least 1 byte.
 *    szData should not be less than szCompare.
 *    return 0 if matched; else for error or mismatched.
 */
static int __uh_ut_comparing_data(const char *pData, int szData, const char *pTarget, int szCompare)
{
	if ((szCompare <= 0) || (szData < szCompare))
		return -1;
	return memcmp(pData, pTarget, szCompare);
}

/*
 *  __uh_ut_internal_loopback_send_and_recv: write data to tx buf and read from rx buf
 * parameter:
 *    tx_dev_id, rx_dev_id: the uart_ip device ids.
 *    pTxData, szTx: the data and size to write.
 *    rcvBuf: The buffer to read from rx device.
 *    szBuf: The max size for reading.
 * return:
 *    return the size of actual received bytes.
 *    return 0 if recv dev id == -1
 *    return negative codes if error
 */
static int __uh_ut_internal_loopback_send_and_recv(int tx_dev_id, const unsigned char *pTxData, int szTx,
			int rx_dev_id, unsigned char *rcvBuf, int szBuf)
{
	int retry, curTx, szRcv;

	curTx = szRcv = 0;

	if ((!pTxData || szTx <= 0) || (!rcvBuf || szBuf <= 0))
		return UARTHUB_ERR_PARA_WRONG;

	if ((tx_dev_id < 0 || tx_dev_id > UARTHUB_MAX_NUM_DEV_HOST) ||
		(rx_dev_id < -1 || rx_dev_id > UARTHUB_MAX_NUM_DEV_HOST))
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;

	/*Send data*/
	while (curTx < szTx) {
		for (retry = 200; retry > 0; retry--) {
			if (uarthub_is_uartip_tx_buf_empty_for_write_mt6989(tx_dev_id) == 1) {
				uarthub_uartip_write_data_to_tx_buf_mt6989(tx_dev_id, pTxData[curTx++]);
				break;
			}
			usleep_range(5, 6);
		}
		if (retry == 0)
			return UARTHUB_UT_ERR_TX_FAIL;
	}

	/* Receive data */
	if (rx_dev_id == -1)
		return 0;
	while (szRcv < szBuf) {
		for (retry = 200; retry > 0; retry--) {
			if (uarthub_is_uartip_rx_buf_ready_for_read_mt6989(rx_dev_id) == 1) {
				rcvBuf[szRcv++] = uarthub_uartip_read_data_from_rx_buf_mt6989(rx_dev_id);
				break;
			}
			usleep_range(5, 6);
		}

		// Did not get data, try to fetch FIFO again.
		if (retry == 0) {
			int szFIFO = 0;

			if (rx_dev_id == DEV_CMM)
				szFIFO = uarthub_get_cmm_rx_fifo_size_mt6989();
			else
				szFIFO = uarthub_get_host_rx_fifo_size_mt6989(rx_dev_id);

			//FIFO is empty, stop reading.
			if (szFIFO <= 0)
				break;
		}
	}
	return (szRcv == 0) ? UARTHUB_UT_ERR_RX_FAIL : szRcv;
}

/**
 * uarthub_ut_loopback_send_and_recv - verify loopback send and recv data
 * @tx_dev_id: Device id for sending cmd
 * @rx_dev_id: Device id for receiving evt. Will disable the rx dma for
 *   receiving data. Do not use DEV_OFF except the escape packet from CMM.
 * @pCmd, @pEvt: pointers to cmd and evt data
 * @szCmd, @szEvt: data size
 * Return - 0 for success and data matched
 *
 * With loopback enabled, this function send cmd data in one end and compare
 * the received data with the expected evt data.
 **/
int uarthub_ut_loopback_send_and_recv(
	int tx_dev_id, const unsigned char *pCmd, int szCmd,
	int rx_dev_id, const unsigned char *pEvt, int szEvt)
{
	int ret = 0, intf_lb_sta[4] = {0}, szRx;
	unsigned char pRcvBuf[64];

	intf_lb_sta[0] = LOOPBACK_GET_dev0_tx2rx_loopback(LOOPBACK_ADDR);
	intf_lb_sta[1] = LOOPBACK_GET_dev1_tx2rx_loopback(LOOPBACK_ADDR);
	intf_lb_sta[2] = LOOPBACK_GET_dev2_tx2rx_loopback(LOOPBACK_ADDR);
	intf_lb_sta[3] = LOOPBACK_GET_dev_cmm_tx2rx_loopback(LOOPBACK_ADDR);
	if ((intf_lb_sta[tx_dev_id] == 0) || (rx_dev_id != DEV_OFF && intf_lb_sta[rx_dev_id] == 0)) {
		UTLOG_VEB("[%s] LOOPBACK CONFIG FAIL (tx_dev(%d)=[%d],rx_dev(%d)=[%d])",
				__func__, tx_dev_id, intf_lb_sta[tx_dev_id], rx_dev_id, intf_lb_sta[rx_dev_id]);
		return UARTHUB_ERR_PARA_WRONG;
	}

	uarthub_config_uartip_dma_en_ctrl_mt6989(tx_dev_id, TX, 0);
	if (rx_dev_id != DEV_OFF)
		uarthub_config_uartip_dma_en_ctrl_mt6989(rx_dev_id, RX, 0);

	/* Internal send and receive */
	ret = __uh_ut_internal_loopback_send_and_recv(tx_dev_id, pCmd, szCmd, rx_dev_id, pRcvBuf, 64);
	if (ret < 0) {
		UTLOG_VEB("failed to interanlly loopback send and recv. error=[%d] from=[%d] to=[%d]",
			ret, tx_dev_id, rx_dev_id);
		goto exit_;
	}

	szRx = ret;
	if (g_uh_ut_verbose) {
		char _strBuf[256] = {0};

		__uh_ut_data_to_string(_strBuf, pCmd, szCmd, 256);
		UTLOG("loopback send: (dev=%d sz=%d) :[%s]", tx_dev_id, szCmd, _strBuf);
		__uh_ut_data_to_string(_strBuf, pRcvBuf, szRx, 256);
		UTLOG("loopback recv: (dev=%d sz=%d) :[%s]", rx_dev_id, szRx, _strBuf);
		__uh_ut_data_to_string(_strBuf, pEvt, szEvt, 256);
		UTLOG("loopback event:(dev=%d sz=%d) :[%s]", rx_dev_id, szEvt, _strBuf);
	}

	if (pEvt && szEvt > 0) {
		ret = __uh_ut_comparing_data(pRcvBuf, szRx, pEvt, szEvt);
		if (ret) {
			UTLOG_VEB("comparing RX data with EVT is mistached.");
			goto exit_;
		}
	}

exit_:
	uarthub_config_uartip_dma_en_ctrl_mt6989(tx_dev_id, TX, 1);
	if (rx_dev_id != DEV_OFF)
		uarthub_config_uartip_dma_en_ctrl_mt6989(rx_dev_id, RX, 1);
	return ret;
}

int uarthub_uartip_send_data_internal_mt6989(
	int dev_index, unsigned char *p_tx_data, int tx_len, int dump_trxlog)
{
	int state = 0;
	int len = 0;
	int i = 0;
	enum uarthub_pkt_fmt_type pkt_type = pkt_fmt_undef;
	unsigned char dmp_info_buf_rx[TRX_BUF_LEN] = { 0 };
	unsigned char dmp_info_buf_rx_expect[TRX_BUF_LEN] = { 0 };
	unsigned char dmp_info_buf_tx[TRX_BUF_LEN] = { 0 };
	unsigned char evtBuf[TRX_BUF_LEN] = { 0 };
	unsigned char evtBuf_expect[TRX_BUF_LEN] = { 0 };
	int rx_len = 0;
	int rx_len_expect = 0;
	int rx_dev_index = -1;
	unsigned char rx_data_crc[2] = { 0 };
	int byte_cnt_tx_0 = 0, byte_cnt_tx_1 = 0, byte_cnt_tx_2 = 0;
	int byte_cnt_rx_0 = 0, byte_cnt_rx_1 = 0, byte_cnt_rx_2 = 0;
	int is_pkt_complete = 0;
	int intfhub_loopback_t2r_sta[4] = { 0 };

	if (p_tx_data == NULL || tx_len <= 0) {
		pr_notice("[%s] tx_data error, p_tx_data=[NULL], tx_len=[%d]\n",
			__func__, tx_len);
		return UARTHUB_ERR_PARA_WRONG;
	}

	if (dev_index < 0 || dev_index > UARTHUB_MAX_NUM_DEV_HOST) {
		pr_notice("[%s] not support dev_index(%d)\n", __func__, dev_index);
		return UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT;
	}

	pkt_type = uarthub_check_packet_format(dev_index, p_tx_data[0]);

	if (pkt_type == pkt_fmt_esp && tx_len != 1) {
		pr_notice("[%s] tx_data error, pkt_type is 0x%02X, tx_len=[%d]\n",
			__func__, p_tx_data[0], tx_len);
		return UARTHUB_ERR_PARA_WRONG;
	}

	if (dev_index == 3) {
		if (pkt_type == pkt_fmt_dev0)
			rx_dev_index = 0;
		else if (pkt_type == pkt_fmt_dev1)
			rx_dev_index = 1;
		else if (pkt_type == pkt_fmt_dev2)
			rx_dev_index = 2;
		if (pkt_type == pkt_fmt_esp) {
			if (p_tx_data[0] == 0xF0)
				rx_dev_index = 0;
			else if (p_tx_data[0] == 0xF1)
				rx_dev_index = 1;
			else if (p_tx_data[0] == 0xF2)
				rx_dev_index = 2;
			else if (p_tx_data[0] == 0xFF)
				rx_dev_index = -1;
		} else if (pkt_type == pkt_fmt_undef)
			rx_dev_index = 0;
	} else
		rx_dev_index = 3;

	intfhub_loopback_t2r_sta[0] = LOOPBACK_GET_dev0_tx2rx_loopback(LOOPBACK_ADDR);
	intfhub_loopback_t2r_sta[1] = LOOPBACK_GET_dev1_tx2rx_loopback(LOOPBACK_ADDR);
	intfhub_loopback_t2r_sta[2] = LOOPBACK_GET_dev2_tx2rx_loopback(LOOPBACK_ADDR);
	intfhub_loopback_t2r_sta[3] = LOOPBACK_GET_dev_cmm_tx2rx_loopback(LOOPBACK_ADDR);
	if (intfhub_loopback_t2r_sta[dev_index] == 0) {
		if (dev_index == 3)
			pr_info("[%s] LOOPBACK FAIL: uart_cmm has not set loopback mode (dev[%d],rx_dev=[%d])\n",
				__func__, dev_index, rx_dev_index);
		else
			pr_info("[%s] LOOPBACK FAIL: uart_%d has not set loopback mode (dev[%d],rx_dev=[%d])\n",
				__func__, dev_index, dev_index, rx_dev_index);
		return UARTHUB_ERR_PARA_WRONG;
	}

	if (rx_dev_index != -1 && intfhub_loopback_t2r_sta[rx_dev_index] == 0) {
		if (rx_dev_index == 3)
			pr_info("[%s] LOOPBACK FAIL: uart_cmm has not set loopback mode (dev[%d],rx_dev=[%d])\n",
				__func__, dev_index, rx_dev_index);
		else
			pr_info("[%s] LOOPBACK FAIL: uart_%d has not set loopback mode (dev[%d],rx_dev=[%d])\n",
				__func__, rx_dev_index, dev_index, rx_dev_index);
		return UARTHUB_ERR_PARA_WRONG;
	}

	is_pkt_complete = uarthub_check_packet_is_complete(dev_index, p_tx_data, tx_len);

	//uarthub_reset_intfhub_mt6989();
	if (rx_dev_index >= 0)
		uarthub_config_uartip_dma_en_ctrl_mt6989(rx_dev_index, RX, 0);

	len = 0;
	for (i = 0; i < tx_len; i++) {
		len += snprintf(dmp_info_buf_tx + len, TRX_BUF_LEN - len,
			((i == 0) ? "%02X" : ",%02X"), p_tx_data[i]);
	}

	if (dump_trxlog == 1) {
		if (dev_index == 3) {
			if (pkt_type == pkt_fmt_esp) {
				if (p_tx_data[0] == 0xFF)
					pr_info("[%s] uart_cmm send esp [FF] to uarthub, pkt_complete=[%d]\n",
						__func__, is_pkt_complete);
				else
					pr_info("[%s] uart_cmm send esp [%s --> FF] to uart_%d, pkt_complete=[%d]\n",
						__func__, dmp_info_buf_tx,
						rx_dev_index, is_pkt_complete);
			} else if (pkt_type == pkt_fmt_undef)
				pr_info("[%s] uart_cmm send undef pkt tpye [%s] to uart_%d, pkt_complete=[%d]\n",
					__func__, dmp_info_buf_tx,
					rx_dev_index, is_pkt_complete);
			else
				pr_info("[%s] uart_cmm send [%s] to uart_%d, pkt_complete=[%d]\n",
					__func__, dmp_info_buf_tx,
					rx_dev_index, is_pkt_complete);
		} else {
			if (pkt_type == pkt_fmt_esp)
				pr_info("[%s] uart_%d send esp [%s] to uart_cmm, pkt_complete=[%d]\n",
					__func__, dev_index,
					dmp_info_buf_tx, is_pkt_complete);
			else if (pkt_type == pkt_fmt_undef)
				pr_info("[%s] uart_%d send undef pkt tpye [%s] to uart_cmm, pkt_complete=[%d]\n",
					__func__, dev_index,
					dmp_info_buf_tx, is_pkt_complete);
			else
				pr_info("[%s] uart_%d send [%s] to uart_cmm, pkt_complete=[%d]\n",
					__func__, dev_index,
					dmp_info_buf_tx, is_pkt_complete);
		}
	}

	state = uarthub_uartip_write_tx_data_mt6989(dev_index, p_tx_data, tx_len);

	if (state != 0) {
		if (dev_index == 3) {
			if (pkt_type == pkt_fmt_esp) {
				if (p_tx_data[0] == 0xF0)
					pr_info("[%s] TX FAIL: uart_cmm send esp [FF] to uarthub, state=[%d] (dev[%d],tx_len=[%d])\n",
						__func__, state, dev_index, tx_len);
				else
					pr_info("[%s] TX FAIL: uart_cmm send esp [%s --> FF] to uart_%d, state=[%d] (dev[%d],tx_len=[%d])\n",
						__func__, dmp_info_buf_tx,
						rx_dev_index, state, dev_index, tx_len);
			}	else if (pkt_type == pkt_fmt_undef)
				pr_info("[%s] TX FAIL: uart_cmm send undef pkt tpye [%s] to uart_%d, state=[%d] (dev[%d],tx_len=[%d])\n",
					__func__, dmp_info_buf_tx, rx_dev_index,
					state, dev_index, tx_len);
			else
				pr_info("[%s] TX FAIL: uart_cmm send [%s] to uart_%d, state=[%d] (dev[%d],tx_len=[%d])\n",
					__func__, dmp_info_buf_tx, rx_dev_index,
					state, dev_index, tx_len);
		} else {
			if (pkt_type == pkt_fmt_esp)
				pr_info("[%s] TX FAIL: uart_%d send esp [%s] to uart_cmm, state=[%d] (dev[%d],tx_len=[%d])\n",
					__func__, dev_index, dmp_info_buf_tx,
					state, dev_index, tx_len);
			else if (pkt_type == pkt_fmt_undef)
				pr_info("[%s] TX FAIL: uart_%d send undef pkt tpye [%s] to uart_cmm, state=[%d] (dev[%d],tx_len=[%d])\n",
					__func__, dev_index, dmp_info_buf_tx,
					state, dev_index, tx_len);
			else
				pr_info("[%s] TX FAIL: uart_%d send [%s] to uart_cmm, state=[%d] (dev[%d],tx_len=[%d])\n",
					__func__, dev_index, dmp_info_buf_tx,
					state, dev_index, tx_len);
		}
		state = UARTHUB_UT_ERR_TX_FAIL;
		goto verify_err;
	}

	rx_len = 0;
	state = uarthub_uartip_read_rx_data_mt6989(rx_dev_index, evtBuf, TRX_BUF_LEN, &rx_len);

	if (rx_len > 0) {
		len = 0;
		for (i = 0; i < rx_len; i++) {
			len += snprintf(dmp_info_buf_rx + len, TRX_BUF_LEN - len,
				((i == 0) ? "%02X" : ",%02X"), evtBuf[i]);
		}
	}

	rx_len_expect = tx_len;
	if (is_pkt_complete == 1) {
		if (rx_dev_index == 3)
			rx_len_expect += 2;
		else if (rx_dev_index >= 0)
			rx_len_expect -= 2;
	}
	if (pkt_type == pkt_fmt_esp)
		rx_len_expect = 1;
	if (p_tx_data[0] == 0xFF)
		rx_len_expect = 0;

	if (rx_len_expect < 0) {
		pr_notice("[%s] rx_len_expect error, rx_len_expect=[%d], rx_len=[%d]\n",
			__func__, rx_len_expect, rx_len);
		state = UARTHUB_UT_ERR_RX_FAIL;
		goto verify_err;
	}

	if (dump_trxlog == 1) {
		if (rx_len > 0) {
			if (rx_dev_index == 3)
				pr_info("[%s] uart_cmm received [%s] from uart_%d\n",
					__func__, dmp_info_buf_rx, dev_index);
			else if (rx_dev_index >= 0)
				pr_info("[%s] uart_%d received [%s] from uart_cmm\n",
					__func__, rx_dev_index, dmp_info_buf_rx);
		} else {
			if (rx_dev_index == 3)
				pr_info("[%s] uart_cmm received nothing from uart_%d\n",
					__func__, dev_index);
			else
				pr_info("[%s] uart_%d received nothing from uart_cmm\n",
					__func__, rx_dev_index);
		}
	}

	if (state != 0) {
		if (rx_dev_index == 3)
			pr_info("[%s] RX FAIL: uart_cmm received [%s] from uart_%d, size=[%d], state=[%d] (dev[%d],tx_len=[%d])\n",
				__func__, dmp_info_buf_rx, dev_index,
				rx_len, state, dev_index, tx_len);
		else if (rx_dev_index >= 0)
			pr_info("[%s] RX FAIL: uart_%d received [%s] from uart_cmm, size=[%d], state=[%d] (dev[%d],tx_len=[%d])\n",
				__func__, rx_dev_index, dmp_info_buf_rx,
				rx_len, state, dev_index, tx_len);
		state = UARTHUB_UT_ERR_RX_FAIL;
		goto verify_err;
	}

	if (rx_dev_index == -1) {
		byte_cnt_rx_0 = UARTHUB_DEBUG_GET_OP_RX_REQ_VAL(uartip_base_map[uartip_id_ap]);
		byte_cnt_tx_0 = UARTHUB_DEBUG_GET_IP_TX_DMA_VAL(uartip_base_map[uartip_id_ap]);
		byte_cnt_rx_1 = UARTHUB_DEBUG_GET_OP_RX_REQ_VAL(uartip_base_map[uartip_id_md]);
		byte_cnt_tx_1 = UARTHUB_DEBUG_GET_IP_TX_DMA_VAL(uartip_base_map[uartip_id_md]);
		byte_cnt_rx_2 = UARTHUB_DEBUG_GET_OP_RX_REQ_VAL(uartip_base_map[uartip_id_adsp]);
		byte_cnt_tx_2 = UARTHUB_DEBUG_GET_IP_TX_DMA_VAL(uartip_base_map[uartip_id_adsp]);

		if ((byte_cnt_rx_0 + byte_cnt_tx_0 + byte_cnt_rx_1 +
			byte_cnt_tx_1 + byte_cnt_rx_2 + byte_cnt_tx_2) != 0) {
			pr_notice("[%s] RX FAIL: uart_cmm send [FF] should not influence dev0/1/2 byte count,dev0=[R:%d,T:%d],dev1=[R:%d,T:%d],dev2=[R:%d,T:%d] (dev[%d],tx_len=[%d])\n",
				__func__, byte_cnt_rx_0, byte_cnt_tx_0,
				byte_cnt_rx_1, byte_cnt_tx_1,
				byte_cnt_rx_2, byte_cnt_tx_2, dev_index, tx_len);
			state = UARTHUB_UT_ERR_RX_FAIL;
			goto verify_err;
		}
	} else {
		if (rx_len != rx_len_expect) {
			if (rx_dev_index == 3)
				pr_notice("[%s] RX FAIL: uart_cmm received size=[%d] is different with expect size=[%d] (dev[%d],tx_len=[%d])\n",
					__func__, rx_len,
					rx_len_expect, dev_index, tx_len);
			else
				pr_notice("[%s] RX FAIL: uart_%d received size=[%d] is different with expect size=[%d] (dev[%d],tx_len=[%d])\n",
					__func__, rx_dev_index, rx_len,
					rx_len_expect, dev_index, tx_len);
			state = UARTHUB_UT_ERR_RX_FAIL;
			goto verify_err;
		}

		if (pkt_type != pkt_fmt_esp) {
			for (i = 0; i < tx_len; i++)
				evtBuf_expect[i] = p_tx_data[i];
			if (rx_dev_index == 3) {
				if (is_pkt_complete == 1) {
					uarthub_get_crc(p_tx_data, tx_len, rx_data_crc);
					evtBuf_expect[tx_len] = rx_data_crc[0];
					evtBuf_expect[tx_len + 1] = rx_data_crc[1];
				}
			}
		} else
			evtBuf_expect[0] = 0xFF;

		len = 0;
		for (i = 0; i < rx_len_expect; i++) {
			len += snprintf(dmp_info_buf_rx_expect + len, TRX_BUF_LEN - len,
				((i == 0) ? "%02X" : ",%02X"), evtBuf_expect[i]);
		}

		for (i = 0; i < rx_len_expect; i++) {
			if (evtBuf[i] != evtBuf_expect[i]) {
				pr_notice("[%s] RX FAIL: uart_cmm received [%s] is different with rx_expect_data=[%s] (dev[%d],tx_len=[%d])\n",
					__func__, dmp_info_buf_rx,
					dmp_info_buf_rx_expect,
					dev_index, tx_len);
				state = UARTHUB_UT_ERR_RX_FAIL;
				goto verify_err;
			}
		}
	}

	state = 0;
verify_err:
	/* Uninitialize */
	if (rx_dev_index >= 0)
		uarthub_config_uartip_dma_en_ctrl_mt6989(rx_dev_index, RX, 1);
	//uarthub_reset_intfhub_mt6989();

	return state;
}

int uarthub_is_apuart_tx_buf_empty_for_write_mt6989(int port_no)
{
	if (port_no < 1 || port_no > 3) {
		pr_notice("[%s] not support port_no(%d)\n", __func__, port_no);
		return UARTHUB_ERR_PORT_NO_NOT_SUPPORT;
	}

	return LSR_GET_TEMT(LSR_ADDR(apuart_base_map[port_no]));
}

int uarthub_is_apuart_rx_buf_ready_for_read_mt6989(int port_no)
{
	if (port_no < 1 || port_no > 3) {
		pr_notice("[%s] not support port_no(%d)\n", __func__, port_no);
		return UARTHUB_ERR_PORT_NO_NOT_SUPPORT;
	}

	return LSR_GET_DR(LSR_ADDR(apuart_base_map[port_no]));
}

int uarthub_apuart_write_data_to_tx_buf_mt6989(int port_no, int tx_data)
{
	if (port_no < 1 || port_no > 3) {
		pr_notice("[%s] not support port_no(%d)\n", __func__, port_no);
		return UARTHUB_ERR_PORT_NO_NOT_SUPPORT;
	}

	UARTHUB_REG_WRITE(THR_ADDR(apuart_base_map[port_no]), tx_data);
	return 0;
}

int uarthub_apuart_read_data_from_rx_buf_mt6989(int port_no)
{
	if (port_no < 1 || port_no > 3) {
		pr_notice("[%s] not support port_no(%d)\n", __func__, port_no);
		return UARTHUB_ERR_PORT_NO_NOT_SUPPORT;
	}

	return UARTHUB_REG_READ(RBR_ADDR(apuart_base_map[port_no]));
}

int uarthub_apuart_write_tx_data_mt6989(int port_no, unsigned char *p_tx_data, int tx_len)
{
	int retry = 0;
	int i = 0;

	if (!p_tx_data || tx_len == 0)
		return UARTHUB_ERR_PARA_WRONG;

	if (port_no < 1 || port_no > 3) {
		pr_notice("[%s] not support port_no(%d)\n", __func__, port_no);
		return UARTHUB_ERR_PORT_NO_NOT_SUPPORT;
	}

	for (i = 0; i < tx_len; i++) {
		retry = 200;
		while (retry-- > 0) {
			if (uarthub_is_apuart_tx_buf_empty_for_write_mt6989(port_no) == 1) {
				uarthub_apuart_write_data_to_tx_buf_mt6989(port_no, p_tx_data[i]);
				break;
			}
			usleep_range(5, 6);
		}

		if (retry <= 0)
			return UARTHUB_UT_ERR_TX_FAIL;
	}

	return 0;
}

int uarthub_apuart_read_rx_data_mt6989(
	int port_no, unsigned char *p_rx_data, int rx_len, int *p_recv_rx_len)
{
	int retry = 0;
	int i = 0;
	int state = 0;

	if (!p_rx_data || !p_recv_rx_len || rx_len <= 0)
		return UARTHUB_ERR_PARA_WRONG;

	if (port_no < 1 || port_no > 3) {
		pr_notice("[%s] not support port_no(%d)\n", __func__, port_no);
		return UARTHUB_ERR_PORT_NO_NOT_SUPPORT;
	}

	for (i = 0; i < rx_len; i++) {
		retry = 200;
		while (retry-- > 0) {
			if (uarthub_is_apuart_rx_buf_ready_for_read_mt6989(port_no) == 1) {
				p_rx_data[i] =
					uarthub_apuart_read_data_from_rx_buf_mt6989(port_no);
				*p_recv_rx_len = i+1;
				break;
			}
			usleep_range(5, 6);
		}

		if (retry <= 0) {
			state = UARTHUB_DEBUG_GET_RX_WOFFSET_VAL(apuart_base_map[port_no]);
			if (state > 0)
				retry = 200;
			else
				break;
		}
	}

	return 0;
}
int uarthub_init_default_apuart_config_mt6989(void)
{
	void __iomem *uarthub_dev_base = NULL;
	int baud_rate = 115200;
	int i = 0;

	if (!apuart_base_map[1] || !apuart_base_map[2] || !apuart_base_map[3]) {
		pr_notice("[%s] apuart_base_map[1/2/3] is not all init\n",
			__func__);
		return -1;
	}

	for (i = 1; i < 4; i++)
		uarthub_usb_rx_pin_ctrl_mt6989(apuart_base_map[i], 1);

	for (i = 1; i < 4; i++) {
		uarthub_dev_base = apuart_base_map[i];
		uarthub_config_baud_rate_m6989(uarthub_dev_base, baud_rate);

		/* 0x0c = 0x3,  byte length: 8 bit*/
		UARTHUB_REG_WRITE(LCR_ADDR(uarthub_dev_base), 0x3);
		/* 0x98 = 0xa,  xon1/xoff1 flow control enable */
		UARTHUB_REG_WRITE(EFR_ADDR(uarthub_dev_base), 0xa);
		/* 0xa8 = 0x13, xoff1 keyword */
		UARTHUB_REG_WRITE(XOFF1_ADDR(uarthub_dev_base), 0x13);
		/* 0xa0 = 0x11, xon1 keyword */
		UARTHUB_REG_WRITE(XON1_ADDR(uarthub_dev_base), 0x11);
		/* 0xac = 0x13, xoff2 keyword */
		UARTHUB_REG_WRITE(XOFF2_ADDR(uarthub_dev_base), 0x13);
		/* 0xa4 = 0x11, xon2 keyword */
		UARTHUB_REG_WRITE(XON2_ADDR(uarthub_dev_base), 0x11);
		/* 0x44 = 0x1,  esc char enable */
		UARTHUB_REG_WRITE(ESCAPE_EN_ADDR(uarthub_dev_base), 0x1);
		/* 0x40 = 0xdb, esc char */
		UARTHUB_REG_WRITE(ESCAPE_DAT_ADDR(uarthub_dev_base), 0xdb);
	}

	for (i = 1; i < 4; i++)
		uarthub_usb_rx_pin_ctrl_mt6989(apuart_base_map[i], 0);

	usleep_range(2000, 2010);

	/* 0x4c = 0x3,  rx/tx channel dma enable */
	for (i = 1; i < 4; i++)
		UARTHUB_REG_WRITE(DMA_EN_ADDR(apuart_base_map[i]), 0x3);

	/* 0x08 = 0x87, fifo control register */
	for (i = 1; i < 4; i++)
		UARTHUB_REG_WRITE(FCR_ADDR(apuart_base_map[i]), 0x87);

	return 0;
}

int uarthub_clear_all_ut_irq_sta_mt6989(void)
{
	uarthub_mask_host_irq_mt6989(0, BIT_0xFFFF_FFFF, 1);
	uarthub_clear_host_irq_mt6989(0, BIT_0xFFFF_FFFF);
	g_dev0_irq_sta = 0;
	g_dev0_inband_irq_sta = 0;
	uarthub_mask_host_irq_mt6989(0, BIT_0xFFFF_FFFF, 0);

	return 0;
}
