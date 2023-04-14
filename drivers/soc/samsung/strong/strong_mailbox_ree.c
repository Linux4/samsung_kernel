/*
 * strong_mailbox_ree.c - Samsung Strong Mailbox driver for the Exynos
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "strong_mailbox_common.h"
#include "strong_mailbox_ree.h"
#include "strong_mailbox_sfr.h"

#define ssp_err(dev, fmt, arg...)       printk("[EXYNOS][CMSSP][ERROR] " fmt, ##arg)
#define ssp_info(dev, fmt, arg...)      printk("[EXYNOS][CMSSP][ INFO] " fmt, ##arg)

void __iomem *mb_va_base;

camellia_func_t cf = {
	.mem_allocator_flag = OFF,
	.msg_receiver_flag = OFF,
	.msg_mlog_write = OFF,
};

uint32_t exynos_strong_mailbox_map_sfr(void)
{
	uint32_t ret = 0;

	mb_va_base = ioremap_np(STR_MB_BASE, SZ_4K);
	if (!mb_va_base) {
		ssp_err(0, "%s: fail to ioremap\n", __func__);
		ret = -1;
	}

	return ret;
}

uint32_t mailbox_receive_and_callback(void)
{
	uint32_t ret = 0;
	uint32_t reg = 0;
	uint32_t cmd = 0;
	uint32_t i = 0;
	uint32_t rx_data_len;
	uint32_t rx_total_len;
	uint32_t rx_remain_len;
	static uint8_t *buf = NULL;

	/* Check that camellia functions are initialized */
	if (cf.mem_allocator_flag == OFF) {
		ssp_err(0, "mem_allocator is not registerd\n");
		return -1;
	}
	if (cf.msg_receiver_flag == OFF) {
		ssp_err(0, "msg_receiver is not registerd\n");
		return -1;
	}

	/* Read cmd */
	reg = read_sfr(mb_va_base + STR_MB_CTRL_00_OFFSET_S2R);
	cmd = reg >> STMB_CMD_BIT_OFFSET;
	i = reg & 0xFFFF;

	if (cmd != SEND_SEE2REE) {
		ssp_err(0, "Invalid command [0x%X], check blk cnt overflow\n", cmd);
		return -1;
	}

	/* Read tx_total_len */
	reg = read_sfr(mb_va_base + STR_MB_CTRL_02_OFFSET_S2R);
	if (i == 0)
		buf = cf.mem_allocator(reg);
	if (buf == NULL) {
		ssp_err(0, "Fail to mem_allocator\n");
		return -1;
	}

	/* Receive data */
	ret = mailbox_receive(buf, &rx_data_len, &rx_total_len, &rx_remain_len);
	if ((rx_remain_len == 0) || (ret))
		cf.msg_receiver(buf, rx_total_len - rx_remain_len, ret);
	if (ret) {
		ssp_err(0, "mailbox_receive [ret: 0x%X]\n", __func__, ret);
		ssp_err(0, "rx_data_len: 0x%X\n", rx_data_len);
		ssp_err(0, "rx_total_len: 0x%X\n", rx_total_len);
		ssp_err(0, "rx_remain_len: 0x%X\n", rx_remain_len);
		return ret;
	}

	return ret;
}

void mailbox_register_allocator(void *(*fp)(ssize_t))
{
	cf.mem_allocator = fp;
	cf.mem_allocator_flag = ON;
	ssp_info(0, "%s is called\n", __func__);
}
EXPORT_SYMBOL(mailbox_register_allocator);

void mailbox_register_message_receiver(int (*fp)(void *, unsigned int, unsigned int))
{
	cf.msg_receiver = fp;
	cf.msg_receiver_flag = ON;
	ssp_info(0, "%s is called\n", __func__);
}
EXPORT_SYMBOL(mailbox_register_message_receiver);

void mailbox_register_message_mlog_writer(void (*fp)(void))
{
	cf.msg_mlog_write = fp;
	cf.msg_mlog_write_flag = ON;
	ssp_info(0, "%s is called\n", __func__);
}
EXPORT_SYMBOL(mailbox_register_message_mlog_writer);

uint32_t mailbox_open(void)
{
	uint32_t ret = 0;

	ret = strong_enable();
	if (ret)
		ssp_err(0, "%s fail [ret: 0x%X]\n", __func__, ret);
	else
		ssp_info(0, "%s done\n", __func__);

	return ret;
}
EXPORT_SYMBOL(mailbox_open);

uint32_t mailbox_close(void)
{
	uint32_t ret = 0;

	ret = strong_disable();
	if (ret)
		ssp_err(0, "%s fail [ret: 0x%X]\n", __func__, ret);
	else
		ssp_info(0, "%s done\n", __func__);

	return ret;
}
EXPORT_SYMBOL(mailbox_close);

void mailbox_fault_and_callback(bool sfrdump, unsigned int *val, char *str)
{
	int i;
	unsigned int msgsize;
	unsigned int wordsize;
	unsigned int sindex;

	if (sfrdump) {
		msgsize = read_sfr(mb_va_base + STR_MB_DATA_DEBUG_START_OFFSET);
		wordsize = (msgsize % 4) ? (msgsize / 4) + 1 : (msgsize / 4);
		sindex = (MB_DBG_MSG + wordsize - 1);

		if ((msgsize > 0 && msgsize <= MB_DBG_MSG_MAX_SIZE) &&
			(sindex >= MB_DBG_MSG && sindex <= MB_DBG_MAX_INDEX)) {
			ssp_info(0, "ssp: %s: msg size:%d/%d, index:%d/%d\n",
				__func__, msgsize, MB_DBG_MSG_MAX_SIZE, sindex, MB_DBG_MAX_INDEX);
			move_mailbox_to_buf(str,
				(uint64_t)mb_va_base + STR_MB_DATA_DEBUG_START_OFFSET - (sindex * 4), msgsize);
		} else {
			ssp_info(0, "ssp: %s: invalid msg size:%d, index:%d\n",
				__func__, msgsize, sindex);
			str[0] = '\0';
		}

		for (i = 1; i <= MB_DBG_PSP; i++)
			val[i] = read_sfr(mb_va_base + STR_MB_DATA_DEBUG_START_OFFSET - (i * 4));
	}

	if (cf.msg_mlog_write == OFF)
		ssp_err(0, "msg_mlog_write is not registerd\n");
	else
		cf.msg_mlog_write();
}
