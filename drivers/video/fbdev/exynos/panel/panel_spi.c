// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Kimyung Lee <kernel.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/miscdevice.h>

#include "panel_drv.h"
#include "panel_spi.h"

#define PANEL_SPI_MAX_CMD_SIZE 16
#define PANEL_SPI_RX_BUF_SIZE 2048
//#define DEBUG_PANEL_SPI

const char * const panel_spi_cmd_str[] = {
	[PANEL_SPI_CMD_NONE] = "panel_spi_cmd_none",
	[PANEL_SPI_CMD_FLASH_INIT1] = "panel_spi_cmd_flash_init1",
	[PANEL_SPI_CMD_FLASH_INIT1_DONE] = "panel_spi_cmd_flash_init1_done",
	[PANEL_SPI_CMD_FLASH_INIT2] = "panel_spi_cmd_flash_init2",
	[PANEL_SPI_CMD_FLASH_INIT2_DONE] = "panel_spi_cmd_flash_init2_done",
	[PANEL_SPI_CMD_FLASH_EXIT1] = "panel_spi_cmd_flash_exit1",
	[PANEL_SPI_CMD_FLASH_EXIT1_DONE] = "panel_spi_cmd_flash_exit1_done",
	[PANEL_SPI_CMD_FLASH_EXIT2] = "panel_spi_cmd_flash_exit2",
	[PANEL_SPI_CMD_FLASH_EXIT2_DONE] = "panel_spi_cmd_flash_exit2_done",
	[PANEL_SPI_CMD_FLASH_BUSY_CLEAR] = "panel_spi_cmd_flash_busy_clear",
	[PANEL_SPI_CMD_FLASH_READ] = "panel_spi_cmd_flash_read",
	[PANEL_SPI_CMD_FLASH_WRITE_ENABLE] = "panel_spi_cmd_flash_write_enable",
	[PANEL_SPI_CMD_FLASH_WRITE_DISABLE] = "panel_spi_cmd_flash_write_disable",
	[PANEL_SPI_CMD_FLASH_WRITE] = "panel_spi_cmd_flash_write",
	[PANEL_SPI_CMD_FLASH_WRITE_DONE] = "panel_spi_cmd_flash_write_done",
	[PANEL_SPI_CMD_FLASH_ERASE_4K] = "panel_spi_cmd_flash_erase_4k",
	[PANEL_SPI_CMD_FLASH_ERASE_4K_DONE] = "panel_spi_cmd_flash_erase_4k_done",
	[PANEL_SPI_CMD_FLASH_ERASE_32K] = "panel_spi_cmd_flash_erase_32k",
	[PANEL_SPI_CMD_FLASH_ERASE_32K_DONE] = "panel_spi_cmd_flash_erase_32k_done",
	[PANEL_SPI_CMD_FLASH_ERASE_64K] = "panel_spi_cmd_flash_erase_64k",
	[PANEL_SPI_CMD_FLASH_ERASE_64K_DONE] = "panel_spi_cmd_flash_erase_64k_done",
	[PANEL_SPI_CMD_FLASH_ERASE_256K] = "panel_spi_cmd_flash_erase_256k",
	[PANEL_SPI_CMD_FLASH_ERASE_256K_DONE] = "panel_spi_cmd_flash_erase_256k_done",
	[PANEL_SPI_CMD_FLASH_ERASE_CHIP] = "panel_spi_cmd_flash_erase_chip",
	[PANEL_SPI_CMD_FLASH_ERASE_CHIP_DONE] = "panel_spi_cmd_flash_erase_chip_done",
	[MAX_PANEL_SPI_CMD] = "max_panel_spi_cmd",
};

static inline bool SPI_IS_READY(struct panel_spi_dev *dev)
{
	return dev->spi_info.ready;
}


#define PANEL_SPI_DEV_NAME	"panel_spi"

static bool panel_spi_conv_addressing_bit(u8 *dst, u32 addr, int addr_size)
{
	if (addr_size > MAX_ADDRESSING_BYTE) {
		panel_err("PANEL:ERR:%s:address size is invalid in ic's header file %d, %d. skipped\n", __func__, addr_size, MAX_ADDRESSING_BYTE);
		return false;
	}

	if (addr_size == 4) {
		dst[3] = addr & 0xFF;
		dst[2] = (addr >> 8) & 0xFF;
		dst[1] = (addr >> 16) & 0xFF;
		dst[0] = (addr >> 24) & 0xFF;
	} else if (addr_size == 3) {
		dst[2] = addr & 0xFF;
		dst[1] = (addr >> 8) & 0xFF;
		dst[0] = (addr >> 16) & 0xFF;
	} else {
		panel_err("PANEL:ERR:%s:address size is invalid in ic's header file %d. skipped\n", __func__, addr_size);
		return false;
	}
	return true;
}

static bool panel_spi_cmd_exists(struct panel_spi_dev *spi_dev, int idx)
{
	struct spi_dev_info *spi_info = &spi_dev->spi_info;

	if (!(idx < MAX_PANEL_SPI_CMD)) {
		panel_err("%s cmd %d limit exceeded\n", __func__, idx);
		return false;
	}

	if (!spi_info->cmd_list) {
		panel_spi_dbg("%s cmd_list is null. check matched panel id\n", __func__, idx, panel_spi_cmd_str[idx]);
		return false;
	}

	if (!spi_info->cmd_list[idx]) {
		panel_spi_dbg("%s cmd %d(%s) is null\n", __func__, idx, panel_spi_cmd_str[idx]);
		return false;
	}

	return true;
}
static int panel_spi_pdrv_read_param(struct panel_spi_dev *spi_dev, const u8 *wbuf, int wsize)
{
	if (wsize < 1 || !wbuf)
		return -EINVAL;

	memcpy(spi_dev->setparam_buffer, wbuf, wsize);
	spi_dev->setparam_buffer_size = wsize;
	return wsize;
}

static int panel_spi_sync(struct panel_spi_dev *spi_dev, const u8 *wbuf, int wsize, u8 *rbuf, int rsize)
{
	int ret = 0;
	u32 speed_hz;
	struct spi_device *spi;
	struct spi_dev_info *spi_info;
	struct spi_message msg;
	struct spi_transfer x_write = { .bits_per_word = 8, };
	struct spi_transfer x_read = { .bits_per_word = 8, };

	spi = spi_dev->spi;
	spi_info = &spi_dev->spi_info;
	if (IS_ERR_OR_NULL(spi)) {
		panel_err("PANEL:ERR:%s:spi device is not initialized\n", __func__);
		return -ENODEV;
	}

	if (spi_info->speed_hz > 0)
		spi->max_speed_hz = spi_info->speed_hz;

	spi_message_init(&msg);
	if (wsize < 1 || !wbuf)
		return -EINVAL;

	speed_hz = spi_info->speed_hz;
	if (speed_hz <= 0)
		speed_hz = 0;
	else if (speed_hz > spi->max_speed_hz)
		speed_hz = spi->max_speed_hz;

	x_write.len = wsize;
	x_write.tx_buf = wbuf;
	x_write.speed_hz = speed_hz;
	spi_message_add_tail(&x_write, &msg);

	if (rsize) {
		memset(spi_dev->read_buf_data, 0, PANEL_SPI_RX_BUF_SIZE);
		x_read.len = rsize;
		x_read.rx_buf = spi_dev->read_buf_data;
		x_read.speed_hz = speed_hz;
		spi_message_add_tail(&x_read, &msg);
		spi_dev->read_buf_cmd = wbuf[0];
		spi_dev->read_buf_size = rsize;
	}

	ret = spi_sync(spi, &msg);
	if (ret < 0) {
		pr_err("%s, failed to spi_sync %d\n", __func__, ret);
		return ret;
	}

	if (rbuf != NULL)
		memcpy(rbuf, spi_dev->read_buf_data, rsize);

#ifdef DEBUG_PANEL_SPI
	print_hex_dump(KERN_ERR, "spi_cmd_print write ", DUMP_PREFIX_ADDRESS, 16, 1, wbuf, wsize, false);
	if (rbuf != NULL)
		print_hex_dump(KERN_ERR, "spi_cmd_print read ", DUMP_PREFIX_ADDRESS, 16, 1, rbuf, rsize, false);
#endif

	return rsize;
}

static int panel_spi_pdrv_read(struct panel_spi_dev *spi_dev, const u8 rcmd, u8 *rbuf, int rsize)
{
	int ret = 0;
	const u8 *wbuf;
	int wsize;

	//check setparam cmd for read operation
	if (!spi_dev->setparam_buffer)
		return -EINVAL;

	wbuf = &rcmd;
	wsize = 1;

	if (spi_dev->setparam_buffer_size > 0 && spi_dev->setparam_buffer[0] == rcmd) {
		//setparam cmd
		wbuf = spi_dev->setparam_buffer;
		wsize = spi_dev->setparam_buffer_size;
	}
	ret = panel_spi_sync(spi_dev, wbuf, wsize, rbuf, rsize);

	if (ret < 0)
		return ret;

	if (!ret)
		return -EINVAL;

	//setparam_buffer_size set to 0 if setparam once
	spi_dev->setparam_buffer_size = 0;

	return rsize;
}

static int panel_spi_read_id(struct panel_spi_dev *spi_dev, u32 *id)
{
	int ret = 0;
	const u8 wbuf[] = { 0x9F };
	u8 rbuf[3] = { 0x00, };
	int wsize = 1;
	int rsize = 3;

	ret = panel_spi_sync(spi_dev, wbuf, wsize, rbuf, rsize);

	if (ret < 0)
		return ret;

	if (!ret)
		return -EIO;

	*id = (rbuf[0] << 16) | (rbuf[1] << 8) | rbuf[2];
	panel_info("%s 0x%06X\n", __func__, *id);

	return 0;
}

static int panel_spi_execute_cmd_addr_data(struct panel_spi_dev *spi_dev, struct spi_cmd *spi_cmd,
	u32 addr, u8 *write_buf, int write_len, u8 *read_buf, const int read_len)
{
	int i, ret;
	int verify_error_cnt;
	u8 send_buf[MAX_PANEL_SPI_TX_BUF] = { 0x00, };
	u8 receive_buf[MAX_PANEL_SPI_RX_BUF] = { 0x00, };
	int send_len = 0, receive_len = 0, read_data_len = 0;
	int addr_byte_size = 0;

	if (!spi_cmd) {
		//err notfound
		panel_err("%s got null cmd\n", __func__);
		return -EINVAL;
	}

	if (spi_cmd->reg) {
		send_buf[send_len++] = spi_cmd->reg;
	} else {
		if ((spi_cmd->opt & PANEL_SPI_CMD_OPTION_ONLY_DELAY) == 0) {
			//err notfound
			return -EINVAL;
		}

		if (spi_cmd->delay_before_usecs > 0)
			usleep_range(spi_cmd->delay_before_usecs, spi_cmd->delay_before_usecs + 10);

		if (spi_cmd->delay_after_usecs > 0)
			usleep_range(spi_cmd->delay_after_usecs, spi_cmd->delay_after_usecs + 10);

		return 0;
	}

	if ((spi_cmd->opt & PANEL_SPI_CMD_OPTION_ADDR_4BYTE) > 0)
		addr_byte_size = 4;
	else if ((spi_cmd->opt & PANEL_SPI_CMD_OPTION_ADDR_3BYTE) > 0)
		addr_byte_size = 3;

	if (addr_byte_size > 0) {
		if (!panel_spi_conv_addressing_bit(send_buf + send_len, addr, addr_byte_size))
			return -EINVAL;

		send_len += addr_byte_size;
	}

	if (spi_cmd->wlen > 0) {
		for (i = 0; i < spi_cmd->wlen; i++)
			send_buf[send_len++] = spi_cmd->wval[i];
	}

	if (write_len > 0) {
		for (i = 0; i < write_len && send_len < MAX_PANEL_SPI_TX_BUF; i++)
			send_buf[send_len++] = write_buf[i];
	}

	if (spi_cmd->rlen > 0)
		receive_len += spi_cmd->rlen;

	if (read_len > 0) {
		read_data_len = read_len;
		if (!(read_data_len < MAX_PANEL_SPI_RX_DATA))
			read_data_len = MAX_PANEL_SPI_RX_DATA;

		receive_len += read_data_len;
	}

	if (!(receive_len < MAX_PANEL_SPI_RX_BUF))
		receive_len = MAX_PANEL_SPI_RX_BUF;

	if (spi_cmd->delay_before_usecs > 0)
		usleep_range(spi_cmd->delay_before_usecs, spi_cmd->delay_before_usecs + 10);

	if (send_len > 0) {
		ret = panel_spi_sync(spi_dev, send_buf, send_len, receive_buf, receive_len);
		if (ret < 0)
			goto fail_delay;
	}

	if (read_data_len > 0)
		memcpy(read_buf, receive_buf + spi_cmd->rlen, read_data_len);

	if ((spi_cmd->opt & PANEL_SPI_CMD_OPTION_READ_COMPARE) > 0) {
		verify_error_cnt = 0;
		for (i = 0; i < spi_cmd->rlen; i++) {
			if ((receive_buf[i] & spi_cmd->rmask[i]) != spi_cmd->rval[i]) {
				//error
				verify_error_cnt++;
				panel_spi_dbg("%s verify mismatch, idx %d(%s) buf %02x mask %02x val %02x\n",
					__func__, i, panel_spi_cmd_str[i], receive_buf[i], spi_cmd->rmask[i], spi_cmd->rval[i]);
			} else {
				panel_spi_dbg("%s verify completed, idx %d(%s) buf %02x mask %02x val %02x\n",
					__func__, i, panel_spi_cmd_str[i], receive_buf[i], spi_cmd->rmask[i], spi_cmd->rval[i]);
			}
		}

		if (verify_error_cnt > 0) {
			ret = verify_error_cnt;
			goto fail_delay;
		}
	}

	if (spi_cmd->delay_after_usecs > 0)
		usleep_range(spi_cmd->delay_after_usecs, spi_cmd->delay_after_usecs + 10);

	return 0;

fail_delay:
	if (spi_cmd->delay_retry_usecs > 0)
		usleep_range(spi_cmd->delay_retry_usecs, spi_cmd->delay_retry_usecs + 10);
	return ret;
}

static inline int panel_spi_execute_cmd_addr(struct panel_spi_dev *spi_dev, struct spi_cmd *spi_cmd, u32 addr)
{
	return panel_spi_execute_cmd_addr_data(spi_dev, spi_cmd, addr, NULL, 0, NULL, 0);
}

static inline int panel_spi_execute_cmd(struct panel_spi_dev *spi_dev, struct spi_cmd *spi_cmd)
{
	return panel_spi_execute_cmd_addr(spi_dev, spi_cmd, INT_MIN);
}
static int panel_spi_execute_cmd_retry(struct panel_spi_dev *spi_dev, struct spi_cmd *spi_cmd, const int retry)
{
	int ret, i;

	for (i = 0; i < retry; i++) {
		ret = panel_spi_execute_cmd(spi_dev, spi_cmd);
		if (!ret)
			return 0;
	}
	panel_err("%s fail cnt exceeded %d\n", __func__, retry);
	return ret;
}

static int panel_spi_flash_init(struct panel_spi_dev *spi_dev)
{
	struct spi_dev_info *spi_info = &spi_dev->spi_info;
	int i, ret = 0;
	const int q[] = {
		PANEL_SPI_CMD_FLASH_WRITE_ENABLE,
		PANEL_SPI_CMD_FLASH_INIT1,
		PANEL_SPI_CMD_FLASH_INIT1_DONE,
		PANEL_SPI_CMD_FLASH_WRITE_ENABLE,
		PANEL_SPI_CMD_FLASH_INIT2,
		PANEL_SPI_CMD_FLASH_INIT2_DONE,
		PANEL_SPI_CMD_FLASH_WRITE_DISABLE,
	};

	if (!SPI_IS_READY(spi_dev))
		return -ENODEV;

	for (i = 0; i < sizeof(q) / sizeof(int); i++) {
		if (panel_spi_cmd_exists(spi_dev, q[i])) {
			ret = panel_spi_execute_cmd_retry(spi_dev, spi_info->cmd_list[q[i]], MAX_STATUS_RETRY);
			if (ret) {
				panel_err("%s error occurred when send cmd %d ret %d\n", __func__, i, ret);
				return ret;
			}
		}
	}

	return ret;
}

static int panel_spi_flash_exit(struct panel_spi_dev *spi_dev)
{
	struct spi_dev_info *spi_info = &spi_dev->spi_info;
	int i, ret = 0;
	const int q[] = {
		PANEL_SPI_CMD_FLASH_WRITE_ENABLE,
		PANEL_SPI_CMD_FLASH_EXIT1,
		PANEL_SPI_CMD_FLASH_EXIT1_DONE,
		PANEL_SPI_CMD_FLASH_WRITE_ENABLE,
		PANEL_SPI_CMD_FLASH_EXIT2,
		PANEL_SPI_CMD_FLASH_EXIT2_DONE,
		PANEL_SPI_CMD_FLASH_WRITE_DISABLE,
	};

	if (!SPI_IS_READY(spi_dev))
		return -ENODEV;

	for (i = 0; i < sizeof(q) / sizeof(int); i++) {
		if (panel_spi_cmd_exists(spi_dev, q[i])) {
			ret = panel_spi_execute_cmd_retry(spi_dev, spi_info->cmd_list[q[i]], MAX_STATUS_RETRY);
			if (ret) {
				panel_err("%s error occurred when send cmd %d ret %d\n", __func__, i, ret);
				return ret;
			}
		}
	}

	return ret;
}

static int panel_spi_flash_erase(struct panel_spi_dev *spi_dev, struct spi_data_buffer *data_buf)
{
	struct spi_dev_info *spi_info;
	int ret = 0;
	int remain_size, erase_idx, erase_wait_idx;
	int erase_size = 0, target_addr = 0;

	if (!SPI_IS_READY(spi_dev)) {
		ret = -ENODEV;
		goto erase_out;
	}

	if (!data_buf) {
		ret = -EINVAL;
		goto erase_out;
	}

	panel_info("%s: +++, 0x%06x %d\n", __func__, data_buf->addr, data_buf->size);

	spi_info = &spi_dev->spi_info;
	remain_size = data_buf->size;
	target_addr = data_buf->addr;
	erase_idx = erase_wait_idx = erase_size = -1;

	while (remain_size > 0) {
		if (spi_info->erase_type == PANEL_SPI_ERASE_TYPE_BLOCK) {
			if (remain_size >= SZ_256K
				&& panel_spi_cmd_exists(spi_dev, PANEL_SPI_CMD_FLASH_ERASE_256K)) {
				erase_idx = PANEL_SPI_CMD_FLASH_ERASE_256K;
				erase_wait_idx = PANEL_SPI_CMD_FLASH_ERASE_256K_DONE;
				erase_size = SZ_256K;
			} else if (remain_size >= SZ_64K
				&& panel_spi_cmd_exists(spi_dev, PANEL_SPI_CMD_FLASH_ERASE_64K)) {
				erase_idx = PANEL_SPI_CMD_FLASH_ERASE_64K;
				erase_wait_idx = PANEL_SPI_CMD_FLASH_ERASE_64K_DONE;
				erase_size = SZ_64K;
			} else if (remain_size >= SZ_32K
				&& panel_spi_cmd_exists(spi_dev, PANEL_SPI_CMD_FLASH_ERASE_32K)) {
				erase_idx = PANEL_SPI_CMD_FLASH_ERASE_32K;
				erase_wait_idx = PANEL_SPI_CMD_FLASH_ERASE_32K_DONE;
				erase_size = SZ_32K;
			} else if (panel_spi_cmd_exists(spi_dev, PANEL_SPI_CMD_FLASH_ERASE_4K)) {
				erase_idx = PANEL_SPI_CMD_FLASH_ERASE_4K;
				erase_wait_idx = PANEL_SPI_CMD_FLASH_ERASE_4K_DONE;
				erase_size = SZ_4K;
			}
		} else if (spi_info->erase_type == PANEL_SPI_ERASE_TYPE_CHIP) {
			if (panel_spi_cmd_exists(spi_dev, PANEL_SPI_CMD_FLASH_ERASE_CHIP)) {
				erase_idx = PANEL_SPI_CMD_FLASH_ERASE_CHIP;
				erase_wait_idx = PANEL_SPI_CMD_FLASH_ERASE_CHIP_DONE;
				erase_size = SZ_1G;
			}
		}

		if (erase_idx < 0) {
			panel_err("%s operation is not supported in this ic. check ic's header file %d\n", __func__, remain_size);
			ret = -ENODATA;	// 74
			goto erase_out;
		}

		// write en
		if (panel_spi_cmd_exists(spi_dev, PANEL_SPI_CMD_FLASH_WRITE_ENABLE)) {
			ret = panel_spi_execute_cmd_retry(spi_dev, spi_info->cmd_list[PANEL_SPI_CMD_FLASH_WRITE_ENABLE], MAX_BUSY_RETRY);
			if (ret) {
				panel_err("%s error occurred when send cmd %d(%s) retry %d ret %d\n", __func__, PANEL_SPI_CMD_FLASH_BUSY_CLEAR,
					panel_spi_cmd_str[PANEL_SPI_CMD_FLASH_BUSY_CLEAR], MAX_BUSY_RETRY, ret);
				goto erase_out;
			}
		}
		//send erase cmd
		ret = panel_spi_execute_cmd_addr(spi_dev, spi_info->cmd_list[erase_idx], target_addr);
		if (ret) {
			panel_err("%s error occurred when send cmd %d(%s) ret %d\n", __func__, erase_idx,
				panel_spi_cmd_str[erase_wait_idx], ret);
			goto erase_out;
		}

		// busy wait
		if (panel_spi_cmd_exists(spi_dev, erase_wait_idx)) {
			ret = panel_spi_execute_cmd_retry(spi_dev, spi_info->cmd_list[erase_wait_idx], MAX_BUSY_RETRY);
			if (ret) {
				panel_err("%s error occurred when send cmd %d(%s) retry %d ret %d\n", __func__, erase_wait_idx,
					panel_spi_cmd_str[erase_wait_idx], MAX_BUSY_RETRY, ret);
				goto erase_out;
			}
		}
		target_addr += erase_size;
		remain_size -= erase_size;
	}

erase_out:
	panel_info("%s: ---, 0x%06x %d\n", __func__, target_addr, remain_size);
	return ret;
}

static int panel_spi_flash_read(struct panel_spi_dev *spi_dev, struct spi_data_buffer *data_buf)
{
	struct spi_dev_info *spi_info;
	int ret = 0;
	int read_size, data_buf_pos;
	int remain_size = 0, read_addr = 0;
	u8 *buf;

	if (!SPI_IS_READY(spi_dev)) {
		ret = -ENODEV;
		goto read_out;
	}

	if (!data_buf) {
		ret = -EINVAL;
		goto read_out;
	}

	panel_info("%s: +++, 0x%06x %d\n", __func__, data_buf->addr, data_buf->size);

	spi_info = &spi_dev->spi_info;
	remain_size = data_buf->size;
	read_addr = data_buf->addr;
	data_buf_pos = 0;

	if (!panel_spi_cmd_exists(spi_dev, PANEL_SPI_CMD_FLASH_READ)) {
		ret = -ENOENT;
		goto read_out;
	}

	if (data_buf->size < 1) {
		panel_err("%s data_buf is invalid %d\n", __func__, data_buf->size);
		ret = -EINVAL;
		goto read_out;
	}

	buf = kzalloc(sizeof(u8) * MAX_PANEL_SPI_RX_DATA, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto read_out;
	}

	while (remain_size > 0) {
		if (remain_size > MAX_PANEL_SPI_RX_DATA)
			read_size = MAX_PANEL_SPI_RX_DATA;
		else
			read_size = remain_size;

		//send read cmd
		ret = panel_spi_execute_cmd_addr_data(spi_dev, spi_info->cmd_list[PANEL_SPI_CMD_FLASH_READ],
			read_addr, NULL, 0, buf, read_size);

		if (ret) {
			panel_err("%s error occurred when send cmd %d(%s) ret %d\n", __func__, PANEL_SPI_CMD_FLASH_READ,
				panel_spi_cmd_str[PANEL_SPI_CMD_FLASH_READ], ret);
			goto read_out;
		}

		//copy readed data
		memcpy(data_buf->buf + data_buf_pos, buf, read_size);
		data_buf_pos += read_size;
		read_addr += read_size;
		remain_size -= read_size;
	}
read_out:
	panel_info("%s: ---, 0x%06x %d\n", __func__, read_addr, remain_size);

	return ret;
}

static int panel_spi_flash_write(struct panel_spi_dev *spi_dev, struct spi_data_buffer *data_buf)
{
	struct spi_dev_info *spi_info;
	int ret = 0;
	int write_size, data_buf_pos;
	int remain_size = 0, write_addr = 0;
	u8 *buf;

	if (!SPI_IS_READY(spi_dev)) {
		ret = -ENODEV;
		goto write_out;
	}

	if (!data_buf) {
		ret = -EINVAL;
		goto write_out;
	}

	panel_info("%s: +++, 0x%06x %d\n", __func__, data_buf->addr, data_buf->size);

	spi_info = &spi_dev->spi_info;
	remain_size = data_buf->size;
	write_addr = data_buf->addr;
	data_buf_pos = 0;

	if (!panel_spi_cmd_exists(spi_dev, PANEL_SPI_CMD_FLASH_WRITE)) {
		ret = -ENOENT;
		goto write_out;
	}

	if (data_buf->size < 1) {
		panel_err("%s data_buf is invalid %d\n", __func__, data_buf->size);
		ret = -EINVAL;
		goto write_out;
	}

	buf = kzalloc(sizeof(u8) * MAX_PANEL_SPI_TX_DATA, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto write_out;
	}

	while (remain_size > 0) {
		if (remain_size > MAX_PANEL_SPI_TX_DATA)
			write_size = MAX_PANEL_SPI_TX_DATA;
		else
			write_size = remain_size;

		//copy write data
		memcpy(buf, data_buf->buf + data_buf_pos, write_size);

		// busy wait
		if (panel_spi_cmd_exists(spi_dev, PANEL_SPI_CMD_FLASH_WRITE_ENABLE)) {
			ret = panel_spi_execute_cmd_retry(spi_dev, spi_info->cmd_list[PANEL_SPI_CMD_FLASH_WRITE_ENABLE], MAX_CMD_RETRY);
			if (ret) {
				panel_err("%s error occurred when send cmd %d(%s) retry %d ret %d\n",
					__func__, PANEL_SPI_CMD_FLASH_WRITE_ENABLE,
					panel_spi_cmd_str[PANEL_SPI_CMD_FLASH_WRITE_ENABLE], MAX_CMD_RETRY, ret);
				goto write_out;

			}
		}

		//send read cmd
		if (panel_spi_cmd_exists(spi_dev, PANEL_SPI_CMD_FLASH_WRITE_DONE)) {
			ret = panel_spi_execute_cmd_addr_data(spi_dev, spi_info->cmd_list[PANEL_SPI_CMD_FLASH_WRITE],
				write_addr, buf, write_size, NULL, 0);

			if (ret) {
				panel_err("%s error occurred when send cmd %d(%s) ret %d\n", __func__, PANEL_SPI_CMD_FLASH_WRITE,
					panel_spi_cmd_str[PANEL_SPI_CMD_FLASH_WRITE], ret);
				goto write_out;

			}
		}

		// busy wait
		if (panel_spi_cmd_exists(spi_dev, PANEL_SPI_CMD_FLASH_WRITE_DONE)) {
			ret = panel_spi_execute_cmd_retry(spi_dev, spi_info->cmd_list[PANEL_SPI_CMD_FLASH_WRITE_DONE], MAX_BUSY_RETRY);
			if (ret) {
				panel_err("%s error occurred when send cmd %d(%s) retry %d ret %d\n",
					__func__, PANEL_SPI_CMD_FLASH_WRITE_DONE, panel_spi_cmd_str[PANEL_SPI_CMD_FLASH_WRITE_DONE], MAX_BUSY_RETRY, ret);
				goto write_out;
			}
		}

		data_buf_pos += write_size;
		write_addr += write_size;
		remain_size -= write_size;
	}
write_out:
	panel_info("%s: ---, 0x%06x %d\n", __func__, write_addr, remain_size);

	return ret;
}

static int panel_spi_ctrl(struct panel_spi_dev *spi_dev, int ctrl_msg)
{
	int ret;
	u32 id = 0;

	if (!SPI_IS_READY(spi_dev))
		return -ENODEV;

	switch (ctrl_msg) {
	case PANEL_SPI_CTRL_INIT:
		ret = panel_spi_flash_init(spi_dev);
		break;
	case PANEL_SPI_CTRL_EXIT:
		ret = panel_spi_flash_exit(spi_dev);
		break;
	case PANEL_SPI_CTRL_BUSY_CHECK:
		ret = panel_spi_execute_cmd(spi_dev, spi_dev->spi_info.cmd_list[PANEL_SPI_CMD_FLASH_BUSY_CLEAR]);
		break;
	case PANEL_SPI_CTRL_ID_READ:
		ret = panel_spi_read_id(spi_dev, &id);
		break;
	default:
		break;
	}

	if (ret)
		panel_err("%s ret %d\n", __func__, ret);

	return ret;
}

static int panel_spi_probe_dt(struct spi_device *spi)
{
	struct device_node *nc = spi->dev.of_node;
	int rc;
	u32 value;

	rc = of_property_read_u32(nc, "bits-per-word", &value);
	if (rc) {
		dev_err(&spi->dev, "%s has no valid 'bits-per-word' property (%d)\n",
				nc->full_name, rc);
		return rc;
	}
	spi->bits_per_word = value;

	rc = of_property_read_u32(nc, "spi-max-frequency", &value);
	if (rc) {
		dev_err(&spi->dev, "%s has no valid 'spi-max-frequency' property (%d)\n",
				nc->full_name, rc);
		return rc;
	}
	spi->max_speed_hz = value;

	return 0;
}

static int panel_spi_probe(struct spi_device *spi)
{
	int ret;

	if (unlikely(!spi)) {
		pr_err("%s, invalid spi\n", __func__);
		return -EINVAL;
	}

	ret = panel_spi_probe_dt(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "%s, failed to parse device tree, ret %d\n",
				__func__, ret);
		return ret;
	}

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "%s, failed to setup spi, ret %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int panel_spi_remove(struct spi_device *spi)
{
	return 0;
}

static int panel_spi_fops_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct miscdevice *dev = file->private_data;
	struct panel_spi_dev *spi_dev = container_of(dev, struct panel_spi_dev, dev);

	if (!SPI_IS_READY(spi_dev))
		return -ENODEV;

	mutex_lock(&spi_dev->f_lock);

	panel_info("%s was called\n", __func__);

	file->private_data = spi_dev;
/*
 *	if (spi_dev->fopened) {
 *		panel_err("Already in use");
 *		mutex_unlock(&spi_dev->f_lock);
 *		return -EBUSY;
 *	}
 */
	ret = panel_spi_flash_init(spi_dev);
	if (ret) {
		panel_err("%s: Error occurred. init at read-only mode.", __func__);
//		mutex_unlock(&spi_dev->f_lock);
//		return -EBUSY;
	}

	spi_dev->fopened = true;

	mutex_unlock(&spi_dev->f_lock);

	return ret;
}

static int panel_spi_fops_release(struct inode *inode, struct file *file)
{
	struct panel_spi_dev *spi_dev = file->private_data;

	if (!SPI_IS_READY(spi_dev))
		return -ENODEV;

	panel_info("%s was called\n", __func__);
	mutex_lock(&spi_dev->f_lock);

	panel_spi_flash_exit(spi_dev);

	spi_dev->fopened = false;
	mutex_unlock(&spi_dev->f_lock);

	return 0;
}

static ssize_t panel_spi_fops_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	struct panel_spi_dev *spi_dev = file->private_data;
	struct panel_device *panel = container_of(spi_dev, struct panel_device, panel_spi_dev);
	struct spi_data_buffer spi_data_buf;
	int ret;

	u8 *read_buf;
	u32 read_done = 0, read_addr;
	ssize_t read_size, res;
	loff_t empty_pos = 0;

	if (!SPI_IS_READY(spi_dev))
		return -ENODEV;

	panel_info("%s was called, count %d ppos %d fpos: %d\n", __func__, count, *ppos, file->f_pos);

	if (!spi_dev->fopened) {
		panel_err("%s: panel_spi device is not opened\n", __func__);
		return -EIO;
	}

	if (!buf) {
		panel_err("%s: invalid read buffer\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&spi_dev->f_lock);
	read_buf = (u8 *)devm_kzalloc(panel->dev,  count * sizeof(u8), GFP_KERNEL);
	if (!read_buf) {
		panel_err("%s: invalid alloc mem\n", __func__);
		mutex_unlock(&spi_dev->f_lock);
		return -EINVAL;
	}

	while (read_done < count) {
		read_addr = *ppos + read_done;
		read_size = count - read_done;

		if (read_size > PANEL_SPI_RX_BUF_SIZE)
			read_size = PANEL_SPI_RX_BUF_SIZE;

		spi_data_buf.addr = read_addr;
		spi_data_buf.size = read_size;
		spi_data_buf.buf = read_buf + read_done;

		panel_spi_dbg("%s: addr 0x%06X, size %d\n", __func__, spi_data_buf.addr, spi_data_buf.size);

		ret = panel_spi_flash_read(spi_dev, &spi_data_buf);
		if (ret < 0) {
			panel_err("%s: failed to read 0x%06x %d\n", __func__, read_addr, read_size);
			mutex_unlock(&spi_dev->f_lock);
			return -EIO;
		}

		read_done += read_size;
	}

	print_hex_dump(KERN_ERR, "panel_spi_read ", DUMP_PREFIX_ADDRESS, 16, 1, read_buf, read_size, false);

	res = simple_read_from_buffer(buf, count, &empty_pos, read_buf, count);
	*ppos = *ppos + count;
	mutex_unlock(&spi_dev->f_lock);
	return count;
}

static ssize_t panel_spi_fops_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct panel_spi_dev *spi_dev = file->private_data;
	struct panel_device *panel = container_of(spi_dev, struct panel_device, panel_spi_dev);
	struct spi_data_buffer spi_data_buf;
	u8 *write_buf;
	u32 write_done = 0, write_addr;

	ssize_t res = 0, write_size;
	loff_t empty_pos = 0;

	if (!SPI_IS_READY(spi_dev))
		return -ENODEV;

	panel_info("%s was called count %d ppos %d fpos: %d\n", __func__, count, *ppos, file->f_pos);

	if (!spi_dev->fopened) {
		panel_err("%s: panel_spi device is not opened\n", __func__);
		return -EIO;
	}

	if (!buf) {
		panel_err("%s: invalid write buffer\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&spi_dev->f_lock);
	write_buf = (u8 *)devm_kzalloc(panel->dev,  count * sizeof(u8), GFP_KERNEL);
	if (!write_buf) {
		panel_err("%s: invalid alloc mem\n", __func__);
		mutex_unlock(&spi_dev->f_lock);
		return -EINVAL;
	}

	res = simple_write_to_buffer(write_buf, count, &empty_pos, buf, count);

	while (write_done < count) {
		write_addr = *ppos + write_done;
		if (write_addr % SZ_256 != 0) {
			panel_err("%s: byte align error 0x%06X\n", __func__, write_addr);
			mutex_unlock(&spi_dev->f_lock);
			return -EINVAL;
		}

		if (spi_dev->auto_erase && write_addr % SZ_4K == 0) {
			panel_info("%s: auto erase 0x%06X\n", __func__, write_addr);
			spi_data_buf.addr = write_addr;
			spi_data_buf.size = SZ_4K;
			panel_spi_flash_erase(spi_dev, &spi_data_buf);
		}

		write_size = count - write_done;
		if (write_size > SZ_256)
			write_size = SZ_256;

		spi_data_buf.addr = write_addr;
		spi_data_buf.size = write_size;
		spi_data_buf.buf = write_buf + write_done;
		panel_spi_flash_write(spi_dev, &spi_data_buf);

		write_done += write_size;
	}

	*ppos += write_done;

	mutex_unlock(&spi_dev->f_lock);
	return count;
}

static int __ioctl_erase(struct panel_spi_dev *spi_dev, unsigned long arg)
{
	struct ioc_erase_info erase_info;
	struct spi_data_buffer spi_data_buf;
	int ret;

	copy_from_user(&erase_info, (struct ioc_erase_info __user *)arg,
			sizeof(struct ioc_erase_info));

	if (erase_info.offset % SZ_4K != 0) {
		panel_err("%s: offset must be 4K aligned value\n", __func__);
		return -EINVAL;
	}

	if (erase_info.length % SZ_4K != 0) {
		panel_err("%s: length must be 4K aligned value\n", __func__);
		return -EINVAL;
	}

	spi_data_buf.addr = erase_info.offset;
	spi_data_buf.size = erase_info.length;

	ret = panel_spi_flash_erase(spi_dev, &spi_data_buf);

	return ret;

}

static long panel_spi_fops_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct panel_spi_dev *spi_dev = file->private_data;
	int ret = 0;
	bool old_val;

	panel_info("%s was called\n", __func__);

	if (!SPI_IS_READY(spi_dev))
		return -ENODEV;

	mutex_lock(&spi_dev->f_lock);

	switch (cmd) {
	case IOCTL_AUTO_ERASE_ENABLE:
		old_val = spi_dev->auto_erase;
		spi_dev->auto_erase = true;
		panel_info("%s: IOCTL_AUTO_ERASE_ENABLE %d -> %d\n", __func__, old_val, spi_dev->auto_erase);
		break;
	case IOCTL_AUTO_ERASE_DISABLE:
		old_val = spi_dev->auto_erase;
		spi_dev->auto_erase = false;
		panel_info("%s: IOCTL_AUTO_ERASE_DISABLE %d -> %d\n", __func__, old_val, spi_dev->auto_erase);
		break;
	case IOCTL_ERASE:
		panel_info("%s: IOCTL_ERASE\n", __func__);
		ret = __ioctl_erase(spi_dev, arg);
		break;
	default:
		panel_info("%s: Unknown cmd %d\n", __func__, cmd);
		break;
	}

	mutex_unlock(&spi_dev->f_lock);
	return ret;
}

static const struct file_operations panel_spi_drv_fops = {
	.owner = THIS_MODULE,
	.open = panel_spi_fops_open,
	.release = panel_spi_fops_release,
	.read = panel_spi_fops_read,
	.write = panel_spi_fops_write,
	.unlocked_ioctl = panel_spi_fops_ioctl,
	.llseek = generic_file_llseek,
};

static const struct of_device_id panel_spi_match_table[] = {
	{   .compatible = "poc_spi",},
	{}
};

static struct spi_drv_ops panel_spi_drv_ops = {
	.ctl = panel_spi_ctrl,
	.cmd = panel_spi_sync,
	.erase = panel_spi_flash_erase,
	.read = panel_spi_flash_read,
	.write = panel_spi_flash_write,
	.init = panel_spi_flash_init,
	.exit = panel_spi_flash_exit,
};

static struct spi_drv_pdrv_ops panel_spi_drv_pdrv_ops = {
	.pdrv_read = panel_spi_pdrv_read,
	.pdrv_cmd = panel_spi_sync,
	.pdrv_read_param = panel_spi_pdrv_read_param,
};

static int __match_panel_spi(struct device *dev, void *data)
{
	struct spi_device *spi;

	spi = (struct spi_device *)dev;
	if (spi == NULL) {
		pr_err("PANEL:ERR:%s:failed to get spi drvdata\n", __func__);
		return 0;
	}

	pr_info("%s spi driver %s found!\n",
		__func__, spi->dev.driver->name);

	return 1;
}

static struct spi_driver panel_spi_driver = {
	.driver = {
		.name		= PANEL_SPI_DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = panel_spi_match_table,
	},
	.probe		= panel_spi_probe,
	.remove		= panel_spi_remove,
};

struct spi_data *panel_spi_find_matched_data(struct spi_data **spi_data_tbl, int nr_spi_data_tbl, u32 read_id)
{
	int i;
	struct spi_data *data = NULL;

	for (i = 0; i < nr_spi_data_tbl; i++) {
		data = spi_data_tbl[i];
		if ((read_id & data->compat_mask) == data->compat_id) {
			panel_info("%s found spi_ic %s(%s)\n", __func__, data->model, data->vendor);
			return data;
		}
	}
	panel_err("%s cannot found spi_ic 0x%06X\n", __func__, read_id);
	return NULL;
}

int panel_spi_drv_probe(struct panel_device *panel, struct spi_data **spi_data_tbl, int nr_spi_data_tbl)
{
	int ret = 0, i;
	struct panel_spi_dev *spi_dev;
	struct device_driver *driver;
	struct device *dev;
	struct spi_data *spi_data;
	struct spi_dev_info *spi_info;

	if (!panel || nr_spi_data_tbl < 1) {
		pr_err("%s panel(%p) or spi_data_tbl(%d) not exist\n",
				__func__, panel, nr_spi_data_tbl);
		ret = -ENODEV;
		goto rest_init;
	}

	spi_dev = &panel->panel_spi_dev;
	spi_dev->ops = &panel_spi_drv_ops;
	spi_dev->pdrv_ops = &panel_spi_drv_pdrv_ops;
	spi_dev->setparam_buffer = (u8 *)devm_kzalloc(panel->dev, PANEL_SPI_MAX_CMD_SIZE * sizeof(u8), GFP_KERNEL);
	spi_dev->read_buf_data = (u8 *)devm_kzalloc(panel->dev, PANEL_SPI_RX_BUF_SIZE * sizeof(u8), GFP_KERNEL);
	spi_dev->dev.minor = MISC_DYNAMIC_MINOR;
	spi_dev->dev.fops = &panel_spi_drv_fops;
	spi_dev->dev.name = DRIVER_NAME;
	spi_info = &spi_dev->spi_info;
	spi_dev->auto_erase = true;
	mutex_init(&spi_dev->f_lock);
	ret = misc_register(&spi_dev->dev);
	if (ret) {
		panel_err("%s failed to register for spi_dev\n", __func__);
		goto rest_init;
	}

	ret = spi_register_driver(&panel_spi_driver);
	if (ret) {
		panel_err("%s failed to register for spi device\n", __func__);
		goto rest_init;
	}

	driver = driver_find(PANEL_SPI_DRIVER_NAME, &spi_bus_type);
	if (IS_ERR_OR_NULL(driver)) {
		panel_err("%s failed to find driver\n", __func__);
		return -ENODEV;
	}

	dev = driver_find_device(driver, NULL, NULL, __match_panel_spi);
	if (IS_ERR_OR_NULL(dev)) {
		panel_err("%s failed to find device\n", __func__);
		return -ENODEV;
	}

	spi_dev->spi = to_spi_device(dev);
	if (IS_ERR_OR_NULL(spi_dev->spi)) {
		panel_err("%s failed to find spi device\n", __func__);
		return -ENODEV;
	}

	ret = panel_spi_read_id(spi_dev, &spi_info->id);
	if (ret < 0) {
		panel_err("%s failed to read id. check cable connection\n", __func__);
		return -EIO;
	}

	panel_info("%s read id: 0x%06X\n", __func__, spi_info->id);
	spi_data = panel_spi_find_matched_data(spi_data_tbl, nr_spi_data_tbl, spi_info->id);
	if (spi_data == NULL) {
		panel_err("%s failed to find spi data. unknown ic 0x%06X\n", __func__, spi_info->id);
		return -ENODATA;
	}
	spi_info->model = spi_data->model;
	spi_info->vendor = spi_data->vendor;
	spi_info->cmd_list = spi_data->cmd_list;
	spi_info->speed_hz = spi_data->speed_hz;
	spi_info->erase_type = spi_data->erase_type;

	if (spi_info->cmd_list) {
		for (i = 0; i < MAX_PANEL_SPI_CMD; i++) {
			if (panel_spi_cmd_exists(spi_dev, i)) {
				panel_spi_dbg("%s cmd found %d(%s) reg:0x%02X opt:%d\n", __func__,
					i, panel_spi_cmd_str[i], spi_info->cmd_list[i]->reg, spi_info->cmd_list[i]->opt);
			} else {
				panel_spi_dbg("%s cmd empty %d(%s)\n", __func__, i, panel_spi_cmd_str[i]);
			}
		}
		spi_info->ready = true;
	} else {
		panel_err("%s cmd list not found. check ic's header file\n", __func__);
	}

	panel_info("%s done\n", __func__);

rest_init:
	return ret;
}

