/*
 * strong_mailbox_common.c - Samsung Strong Mailbox driver for the Exynos
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <asm/barrier.h>
#include <linux/delay.h>
#include "strong_mailbox_common.h"
#include "strong_mailbox_sfr.h"

#define ssp_err(dev, fmt, arg...)	printk("[EXYNOS][CMSSP][ERROR] " fmt, ##arg)
#define ssp_info(dev, fmt, arg...)	printk("[EXYNOS][CMSSP][ INFO] " fmt, ##arg)
#define MB_TIMEOUT (50000)

extern void __iomem *mb_va_base;
static DEFINE_MUTEX(strong_mb_receive_lock);

static uint32_t check_pending(bool delay)
{
	int cnt = 0;

	do {
		if (!(read_sfr(mb_va_base + STR_MB_INTSR1_OFFSET) & STR_MB_INTGR1_ON))
			return 0;

		if (delay)
			usleep_range(100, 500);
	} while (cnt++ < MB_TIMEOUT);

	return -1;
}

static uint32_t busy_waiting(void)
{
	mb();
	write_sfr(mb_va_base + STR_MB_INTGR1_OFFSET, STR_MB_INTGR1_ON);

	if (check_pending(1))
		return RV_MB_WAIT2_TIMEOUT;

	return read_sfr(mb_va_base + STR_MB_CTRL_04_OFFSET_S2R);
}

static void move_data_to_mailbox(uint64_t mbox_addr, uint8_t *data, uint32_t data_len)
{
	uint32_t i;
	uint8_t last_data[4];
	uint8_t *src;

	for (i = 0; i < data_len / WORD_SIZE; i++)
		write_sfr(mbox_addr + (WORD_SIZE * i), ((uint32_t *)data)[i]);

	if (data_len & 0x3) {
		src = (uint8_t *)((uint64_t)data + data_len - (data_len % 4));
		memcpy(last_data, src, data_len % 4);
		write_sfr(mbox_addr + (WORD_SIZE * i), ((uint32_t *)last_data)[0]);
	}
}

void move_mailbox_to_buf(uint8_t *data, uint64_t mbox_addr, uint32_t data_len)
{
	uint32_t i;
	uint8_t last_data[4];
	uint8_t *dst;

	for (i = 0; i < data_len / WORD_SIZE; i++)
		((uint32_t *)data)[i] = read_sfr(mbox_addr + (WORD_SIZE * i));

	if (data_len & 0x3) {
		((uint32_t *)last_data)[0] = read_sfr(mbox_addr + (WORD_SIZE * i));
		dst = (uint8_t *)((uint64_t)data + data_len - (data_len % 4));
		memcpy(dst, last_data, data_len % 4);
	}
}

uint32_t mailbox_send(uint8_t *tx_data, uint32_t tx_total_len, uint32_t *tx_len_out)
{
	uint32_t ret = RV_SUCCESS;
	uint32_t cmd;
	uint32_t i;
	uint32_t tx_data_len;
	int32_t remain_len;

	ssp_sync_mutex(ON);

	if (strong_get_power_count() == 0) {
		ssp_err(0, "%s: ssp disable: count: 0\n", __func__);
		ret = RV_MB_AP_ERR_STRONG_POWER_OFF;
		goto out;
	}

	remain_len = tx_total_len;
	if (remain_len < 0) {
		ret = RV_MB_AP_ERR_REMAIN_LEN_OVERFLOW;
		goto out;
	}

	/* tx mailbox data */
	i = 0;
	while (remain_len > 0) {
		if(check_pending(0)) {
			ret = RV_MB_WAIT1_TIMEOUT;
			break;
		}

		if (remain_len >= STR_MAX_MB_DATA_LEN)
			tx_data_len = STR_MAX_MB_DATA_LEN;
		else
			tx_data_len = remain_len;

		move_data_to_mailbox((uint64_t)mb_va_base + STR_MB_DATA_00_OFFSET_R2S,
				tx_data + (i * STR_MAX_MB_DATA_LEN),
				tx_data_len);

		cmd = (SEND_REE2SEE << STMB_CMD_BIT_OFFSET) + i;
		write_sfr(mb_va_base + STR_MB_CTRL_00_OFFSET_R2S, cmd);
		write_sfr(mb_va_base + STR_MB_CTRL_01_OFFSET_R2S, tx_data_len);
		write_sfr(mb_va_base + STR_MB_CTRL_02_OFFSET_R2S, tx_total_len);
		write_sfr(mb_va_base + STR_MB_CTRL_03_OFFSET_R2S, remain_len);
		ret = busy_waiting();
		if (ret) {
			ssp_err(0, "%s: not send: %d\n", __func__, ret);
			break;
		}

		remain_len -= tx_data_len;
		i++;
	}
	if (ret == RV_SUCCESS)
		*tx_len_out = (i - 1) * STR_MAX_MB_DATA_LEN + tx_data_len;
	else
		*tx_len_out = i * STR_MAX_MB_DATA_LEN;

out:
	ssp_sync_mutex(OFF);

	return ret;
}
EXPORT_SYMBOL(mailbox_send);

/**
 @brief Function STRONG to get a (portion of long) message from mailbox
 @param message : Address for message buffer to receive
 @param message_len : Address to get the received message length of the message buffer (in byte)
 @param total_len : Address to get total message length to be received ultimately (in byte)
 @param remain_len : Address to get remaining message length be be recevied subsequently (in byte)
 @return 0: success, otherwise: error code
 @note : If sender (ex. REE) sends a long "total_len" message, this function can be called multiple times in the interrupt handler.
	 when this function gets:
	 - the first portion of the long message, the total_len is message_len plus remain_len.
	 - the final portion of the long message, the remain_len is 0.
	 - a short message, the total_len can be equal to message_len and the remain_len can be 0.
 */
uint32_t mailbox_receive(uint8_t *rx_buf, uint32_t *rx_data_len,
		uint32_t *rx_total_len, uint32_t *rx_remain_len)
{
	uint32_t ret = RV_SUCCESS;
	uint32_t cmd;
	uint32_t transmitted_len;
	static uint32_t pre_cmd;

	mutex_lock(&strong_mb_receive_lock);

	cmd = read_sfr(mb_va_base + STR_MB_CTRL_00_OFFSET_S2R);
	*rx_data_len = read_sfr(mb_va_base + STR_MB_CTRL_01_OFFSET_S2R);
	*rx_total_len = read_sfr(mb_va_base + STR_MB_CTRL_02_OFFSET_S2R);
	*rx_remain_len = read_sfr(mb_va_base + STR_MB_CTRL_03_OFFSET_S2R);

	if ((cmd >> STMB_CMD_BIT_OFFSET) != SEND_SEE2REE) {
		ret = RV_MB_AP_ERR_INVALID_CMD;
		goto out;
	}

	if (*rx_data_len > STR_MAX_MB_DATA_LEN) {
		ret = RV_MB_AP_ERR_DATA_LEN_OVERFLOW;
		goto out;
	}

	if (*rx_data_len != STR_MAX_MB_DATA_LEN && *rx_remain_len > *rx_data_len) {
		ret = RV_MB_AP_ERR_INVALID_REMAIN_LEN;
		goto out;
	}

	if ((cmd & 0xFFFF) && (pre_cmd == cmd)) {
		ret = RV_MB_AP_ERR_REPEATED_BLOCK;
		goto out;
	}

	pre_cmd = cmd;
	transmitted_len = *rx_total_len - *rx_remain_len;
	move_mailbox_to_buf(rx_buf + transmitted_len,
			(uint64_t)mb_va_base + STR_MB_DATA_00_OFFSET_S2R, *rx_data_len);
	*rx_remain_len -= *rx_data_len;

out:
	write_sfr(mb_va_base + STR_MB_CTRL_04_OFFSET_R2S, ret);
	mb();
	mutex_unlock(&strong_mb_receive_lock);

	return ret;
}
EXPORT_SYMBOL(mailbox_receive);

/*
 * mailbox_cmd must be used on environment
 * which ssp_sync_mutex turn on
 */
uint32_t mailbox_cmd(uint32_t mbox_cmd)
{
	uint32_t cmd;
	uint32_t intsr;

	/* tx mailbox data */
	cmd = mbox_cmd << STMB_CMD_BIT_OFFSET;
	write_sfr(mb_va_base + STR_MB_CTRL_00_OFFSET_R2S, cmd);
	write_sfr(mb_va_base + STR_MB_INTGR1_OFFSET, STR_MB_INTGR1_ON);

	do {
		intsr = read_sfr(mb_va_base + STR_MB_INTSR1_OFFSET) & STR_MB_INTGR1_ON;
	} while (intsr);

	return read_sfr(mb_va_base + STR_MB_CTRL_04_OFFSET_S2R);
}
EXPORT_SYMBOL(mailbox_cmd);

MODULE_LICENSE("GPL");
