/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "ili9881x.h"

#define USER_STR_BUFF		PAGE_SIZE
#define IOCTL_I2C_BUFF		PAGE_SIZE
#define ILITEK_IOCTL_MAGIC	100
#define ILITEK_IOCTL_MAXNR	83

#define ILITEK_IOCTL_I2C_WRITE_DATA		_IOWR(ILITEK_IOCTL_MAGIC, 0, u8*)
#define ILITEK_IOCTL_I2C_SET_WRITE_LENGTH	_IOWR(ILITEK_IOCTL_MAGIC, 1, int)
#define ILITEK_IOCTL_I2C_READ_DATA		_IOWR(ILITEK_IOCTL_MAGIC, 2, u8*)
#define ILITEK_IOCTL_I2C_SET_READ_LENGTH	_IOWR(ILITEK_IOCTL_MAGIC, 3, int)

#define ILITEK_IOCTL_TP_HW_RESET		_IOWR(ILITEK_IOCTL_MAGIC, 4, int)
#define ILITEK_IOCTL_TP_POWER_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 5, int)
#define ILITEK_IOCTL_TP_REPORT_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 6, int)
#define ILITEK_IOCTL_TP_IRQ_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 7, int)

#define ILITEK_IOCTL_TP_DEBUG_LEVEL		_IOWR(ILITEK_IOCTL_MAGIC, 8, int)
#define ILITEK_IOCTL_TP_FUNC_MODE		_IOWR(ILITEK_IOCTL_MAGIC, 9, int)

#define ILITEK_IOCTL_TP_FW_VER			_IOWR(ILITEK_IOCTL_MAGIC, 10, u8*)
#define ILITEK_IOCTL_TP_PL_VER			_IOWR(ILITEK_IOCTL_MAGIC, 11, u8*)
#define ILITEK_IOCTL_TP_CORE_VER		_IOWR(ILITEK_IOCTL_MAGIC, 12, u8*)
#define ILITEK_IOCTL_TP_DRV_VER			_IOWR(ILITEK_IOCTL_MAGIC, 13, u8*)
#define ILITEK_IOCTL_TP_CHIP_ID			_IOWR(ILITEK_IOCTL_MAGIC, 14, u32*)

#define ILITEK_IOCTL_TP_MODE_CTRL		_IOWR(ILITEK_IOCTL_MAGIC, 17, u8*)
#define ILITEK_IOCTL_TP_MODE_STATUS		_IOWR(ILITEK_IOCTL_MAGIC, 18, int*)
#define ILITEK_IOCTL_ICE_MODE_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 19, int)

#define ILITEK_IOCTL_TP_INTERFACE_TYPE		_IOWR(ILITEK_IOCTL_MAGIC, 20, u8*)
#define ILITEK_IOCTL_TP_DUMP_FLASH		_IOWR(ILITEK_IOCTL_MAGIC, 21, int)
#define ILITEK_IOCTL_TP_FW_UART_CTRL		_IOWR(ILITEK_IOCTL_MAGIC, 22, u8*)
#define ILITEK_IOCTL_TP_PANEL_INFO		_IOWR(ILITEK_IOCTL_MAGIC, 23, u32*)
#define ILITEK_IOCTL_TP_INFO			_IOWR(ILITEK_IOCTL_MAGIC, 24, u32*)
#define ILITEK_IOCTL_WRAPPER_RW			_IOWR(ILITEK_IOCTL_MAGIC, 25, u8*)
#define ILITEK_IOCTL_DDI_WRITE			_IOWR(ILITEK_IOCTL_MAGIC, 26, u8*)
#define ILITEK_IOCTL_DDI_READ			_IOWR(ILITEK_IOCTL_MAGIC, 27, u8*)

#define ILITEK_IOCTL_OUTPUT_MODE_SET	_IOWR(ILITEK_IOCTL_MAGIC, 80, u8*)
#define ILITEK_IOCTL_MP_TEST_ITEM_SET	_IOWR(ILITEK_IOCTL_MAGIC, 81, u8*)

#ifdef CONFIG_COMPAT
#define ILITEK_COMPAT_IOCTL_I2C_WRITE_DATA		_IOWR(ILITEK_IOCTL_MAGIC, 0, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_I2C_SET_WRITE_LENGTH	_IOWR(ILITEK_IOCTL_MAGIC, 1, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_I2C_READ_DATA		_IOWR(ILITEK_IOCTL_MAGIC, 2, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_I2C_SET_READ_LENGTH		_IOWR(ILITEK_IOCTL_MAGIC, 3, compat_uptr_t)

#define ILITEK_COMPAT_IOCTL_TP_HW_RESET			_IOWR(ILITEK_IOCTL_MAGIC, 4, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_POWER_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 5, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_REPORT_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 6, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_IRQ_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 7, compat_uptr_t)

#define ILITEK_COMPAT_IOCTL_TP_DEBUG_LEVEL		_IOWR(ILITEK_IOCTL_MAGIC, 8, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_FUNC_MODE		_IOWR(ILITEK_IOCTL_MAGIC, 9, compat_uptr_t)

#define ILITEK_COMPAT_IOCTL_TP_FW_VER			_IOWR(ILITEK_IOCTL_MAGIC, 10, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_PL_VER			_IOWR(ILITEK_IOCTL_MAGIC, 11, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_CORE_VER			_IOWR(ILITEK_IOCTL_MAGIC, 12, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_DRV_VER			_IOWR(ILITEK_IOCTL_MAGIC, 13, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_CHIP_ID			_IOWR(ILITEK_IOCTL_MAGIC, 14, compat_uptr_t)

#define ILITEK_COMPAT_IOCTL_TP_MODE_CTRL		_IOWR(ILITEK_IOCTL_MAGIC, 17, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_MODE_STATUS		_IOWR(ILITEK_IOCTL_MAGIC, 18, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_ICE_MODE_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 19, compat_uptr_t)

#define ILITEK_COMPAT_IOCTL_TP_INTERFACE_TYPE		_IOWR(ILITEK_IOCTL_MAGIC, 20, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_DUMP_FLASH		_IOWR(ILITEK_IOCTL_MAGIC, 21, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_FW_UART_CTRL		_IOWR(ILITEK_IOCTL_MAGIC, 22, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_PANEL_INFO		_IOWR(ILITEK_IOCTL_MAGIC, 23, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_TP_INFO			_IOWR(ILITEK_IOCTL_MAGIC, 24, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_WRAPPER_RW			_IOWR(ILITEK_IOCTL_MAGIC, 25, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_DDI_WRITE			_IOWR(ILITEK_IOCTL_MAGIC, 26, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_DDI_READ			_IOWR(ILITEK_IOCTL_MAGIC, 27, compat_uptr_t)

#define ILITEK_COMPAT_IOCTL_OUTPUT_MODE_SET		_IOWR(ILITEK_IOCTL_MAGIC, 80, compat_uptr_t)
#define ILITEK_COMPAT_IOCTL_MP_TEST_ITEM_SET	_IOWR(ILITEK_IOCTL_MAGIC, 81, compat_uptr_t)
#endif

struct record_state {
	u8 touch_palm_state_e : 2;
	u8 app_an_statu_e : 3;
	u8 app_sys_check_bg_abnormal : 1;
	u8 g_b_wrong_bg : 1;
};

static unsigned char g_user_buf[USER_STR_BUFF] = {0};

int ili_str2hex(char *str)
{
	int strlen, result, intermed, intermedtop;
	char *s = str;

	while (*s != 0x0)
		s++;

	strlen = (int)(s - str);
	s = str;
	if (*s != 0x30)
		return -1;

	s++;

	if (*s != 0x78 && *s != 0x58)
		return -1;
	s++;

	strlen = strlen - 3;
	result = 0;
	while (*s != 0x0) {
		intermed = *s & 0x0f;
		intermedtop = *s & 0xf0;
		if (intermedtop == 0x60 || intermedtop == 0x40)
			intermed += 0x09;

		intermed = intermed << (strlen << 2);
		result = result | intermed;
		strlen -= 1;
		s++;
	}
	return result;
}

int ili_katoi(char *str)
{
	int result = 0;
	unsigned int digit;
	int sign;

	if (*str == '-') {
		sign = 1;
		str += 1;
	} else {
		sign = 0;
		if (*str == '+')
			str += 1;
	}

	for (;; str += 1) {
		digit = *str - '0';
		if (digit > 9)
			break;
		result = (10 * result) + digit;
	}

	if (sign)
		return -result;

	return result;
}

struct file_buffer {
	char *ptr;
	char fname[128];
	int32_t wlen;
	int32_t flen;
	int32_t max_size;
};

static int ilitek_debug_node_buff_control(bool open)
{
	int i, ret;

	ilits->dnp = open;

	input_info(true, ilits->dev, "%s Debug buf ctrl = %s\n", __func__, ilits->dnp ? "Enabled" : "Disabled");

	if (open) {
		ilits->dbf = 0;
		ilits->odi = 0;
		ipio_kfree((void **)&ilits->dbl);
		ilits->dbl = kzalloc(TR_BUF_LIST_SIZE * sizeof(*ilits->dbl), GFP_KERNEL);
		if (ERR_ALLOC_MEM(ilits->dbl)) {
			input_err(true, ilits->dev, "%s Failed to allocate ilits->dbl mem, %ld\n",
				__func__, PTR_ERR(ilits->dbl));
			ret = -ENOMEM;
			goto out;
		}

		for (i = 0; i < TR_BUF_LIST_SIZE; i++) {
			ilits->dbl[i].mark = false;
			ipio_kfree((void **)&ilits->dbl[i].data);
			ilits->dbl[i].data = kzalloc(TR_BUF_SIZE * sizeof(unsigned char), GFP_KERNEL);
			if (ERR_ALLOC_MEM(ilits->dbl[i].data)) {
				input_err(true, ilits->dev, "%s Failed to allocate dbl[%d] mem, %ld\n",
					__func__, i, PTR_ERR(ilits->dbl[i].data));
				ret = -ENOMEM;
				goto out;
			}
		}
		return 0;
	}

out:
	for (i = 0; i < TR_BUF_LIST_SIZE; i++) {
		ilits->dbl[i].mark = false;
		ipio_kfree((void **)&ilits->dbl[i].data);
	}
	ipio_kfree((void **)&ilits->dbl);
	return ret;
}

static ssize_t ilitek_proc_get_delta_data_read(struct file *pFile, char __user *buf, size_t size, loff_t *pos)
{
	s16 *delta = NULL;
	int row = 0, col = 0,  index = 0;
	int ret, i, x, y;
	int read_length = 0, len = 0;
	u8 cmd[2] = {0};
	u8 *data = NULL;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	ili_wq_ctrl(WQ_ESD, DISABLE);
	ili_wq_ctrl(WQ_BAT, DISABLE);
	mutex_lock(&ilits->touch_mutex);

	row = ilits->ych_num;
	col = ilits->xch_num;
	read_length = 4 + 2 * row * col + 1;

	input_info(true, ilits->dev, "%s read length = %d\n", __func__, read_length);

	data = kcalloc(read_length + 1, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(data)) {
		input_err(true, ilits->dev, "%s Failed to allocate data mem\n", __func__);
		goto out;
	}

	delta = kcalloc(P5_X_DEBUG_MODE_PACKET_LENGTH, sizeof(s32), GFP_KERNEL);
	if (ERR_ALLOC_MEM(delta)) {
		input_err(true, ilits->dev, "%s Failed to allocate delta mem\n", __func__);
		goto out;
	}

	cmd[0] = 0xB7;
	cmd[1] = 0x1; //get delta
	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, OFF, OFF);
	if (ret < 0) {
		input_err(true, ilits->dev, "%s Failed to write 0xB7,0x1 command, %d\n", __func__, ret);
		goto out;
	}

	msleep(120);

	/* read debug packet header */
	ret = ilits->wrapper(NULL, 0, data, read_length, OFF, OFF);
	if (ret < 0) {
		input_err(true, ilits->dev, "%s Read debug packet header failed, %d\n", __func__, ret);
		goto out;
	}

	cmd[1] = 0x03; //switch to normal mode
	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, ON, OFF);
	if (ret < 0) {
		input_err(true, ilits->dev, "%s Failed to write 0xB7,0x3 command, %d\n", __func__, ret);
		goto out;
	}

	for (i = 4, index = 0; i < row * col * 2 + 4; i += 2, index++)
		delta[index] = (data[i] << 8) + data[i + 1];

	len = snprintf(g_user_buf + size, PAGE_SIZE - len, "======== Deltadata ========\n");
	input_info(true, ilits->dev, "%s ======== Deltadata ========\n", __func__);

	len += snprintf(g_user_buf + len, PAGE_SIZE - len,
		"Header 0x%x ,Type %d, Length %d\n", data[0], data[1], (data[2] << 8) | data[3]);
	input_info(true, ilits->dev, "%s Header 0x%x ,Type %d, Length %d\n",
		__func__, data[0], data[1], (data[2] << 8) | data[3]);

	// print delta data
	for (y = 0; y < row; y++) {
		len += snprintf(g_user_buf + len, PAGE_SIZE - len, "[%2d] ", (y+1));
		input_info(true, ilits->dev, "[%2d]", (y+1));

		for (x = 0; x < col; x++) {
			int shift = y * col + x;

			len += snprintf(g_user_buf + len, PAGE_SIZE - len, "%5d", delta[shift]);
			printk(KERN_CONT "%5d", delta[shift]);
		}
		len += snprintf(g_user_buf + len, PAGE_SIZE - len, "\n");
		printk(KERN_CONT "\n");
	}

	if (copy_to_user(buf, g_user_buf, len))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	*pos += len;

out:
	mutex_unlock(&ilits->touch_mutex);
	ili_wq_ctrl(WQ_ESD, ENABLE);
	ili_wq_ctrl(WQ_BAT, ENABLE);
	ipio_kfree((void **)&data);
	ipio_kfree((void **)&delta);
	return len;
}

static ssize_t ilitek_proc_fw_get_raw_data_read(struct file *pFile, char __user *buf, size_t size, loff_t *pos)
{
	s16 *rawdata = NULL;
	int row = 0, col = 0,  index = 0;
	int ret, i, x, y;
	int read_length = 0, len = 0;
	u8 cmd[2] = {0};
	u8 *data = NULL;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	ili_wq_ctrl(WQ_ESD, DISABLE);
	ili_wq_ctrl(WQ_BAT, DISABLE);
	mutex_lock(&ilits->touch_mutex);

	row = ilits->ych_num;
	col = ilits->xch_num;
	read_length = 4 + 2 * row * col + 1;

	input_info(true, ilits->dev, "%s read length = %d\n", __func__, read_length);

	data = kcalloc(read_length + 1, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(data)) {
		input_err(true, ilits->dev, "%s Failed to allocate data mem\n", __func__);
		goto out;
	}

	rawdata = kcalloc(P5_X_DEBUG_MODE_PACKET_LENGTH, sizeof(s32), GFP_KERNEL);
	if (ERR_ALLOC_MEM(rawdata)) {
		input_err(true, ilits->dev, "%s Failed to allocate rawdata mem\n", __func__);
		goto out;
	}

	cmd[0] = 0xB7;
	cmd[1] = 0x2; //get rawdata
	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, OFF, OFF);
	if (ret < 0) {
		input_err(true, ilits->dev, "%s Failed to write 0xB7,0x2 command, %d\n", __func__, ret);
		goto out;
	}

	msleep(120);

	/* read debug packet header */
	ret = ilits->wrapper(NULL, 0, data, read_length, OFF, OFF);
	if (ret < 0) {
		input_err(true, ilits->dev, "%s Read debug packet header failed, %d\n", __func__, ret);
		goto out;
	}

	cmd[1] = 0x03; //switch to normal mode
	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, ON, OFF);
	if (ret < 0) {
		input_err(true, ilits->dev, "%s Failed to write 0xB7,0x3 command, %d\n", __func__, ret);
		goto out;
	}

	for (i = 4, index = 0; i < row * col * 2 + 4; i += 2, index++)
		rawdata[index] = (data[i] << 8) + data[i + 1];

	len = snprintf(g_user_buf, PAGE_SIZE, "======== RawData ========\n");
	input_info(true, ilits->dev, "%s ======== RawData ========\n", __func__);

	len += snprintf(g_user_buf + len, PAGE_SIZE - len,
			"Header 0x%x ,Type %d, Length %d\n", data[0], data[1], (data[2] << 8) | data[3]);
	input_info(true, ilits->dev, "%s Header 0x%x ,Type %d, Length %d\n",
		__func__, data[0], data[1], (data[2] << 8) | data[3]);

	// print raw data
	for (y = 0; y < row; y++) {
		len += snprintf(g_user_buf + len, PAGE_SIZE - len, "[%2d] ", (y+1));
		input_info(true, ilits->dev, "[%2d] ", (y+1));

		for (x = 0; x < col; x++) {
			int shift = y * col + x;

			len += snprintf(g_user_buf + len, PAGE_SIZE - len, "%5d", rawdata[shift]);
			printk(KERN_CONT "%5d", rawdata[shift]);
		}
		len += snprintf(g_user_buf + len, PAGE_SIZE - len, "\n");
		printk(KERN_CONT "\n");
	}

	if (copy_to_user(buf, g_user_buf, len))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	*pos += len;

out:
	mutex_unlock(&ilits->touch_mutex);
	ili_wq_ctrl(WQ_ESD, ENABLE);
	ili_wq_ctrl(WQ_BAT, ENABLE);
	ipio_kfree((void **)&data);
	ipio_kfree((void **)&rawdata);
	return len;
}

static ssize_t ilitek_proc_fw_pc_counter_read(struct file *pFile, char __user *buf, size_t size, loff_t *pos)
{
	int len = 0;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	ili_ic_get_pc_counter(0);
	len = snprintf(g_user_buf, PAGE_SIZE, "pc = 0x%x, latch = 0x%x\n", ilits->fw_pc, ilits->fw_latch);

	if (copy_to_user(buf, g_user_buf, len))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	*pos += len;
	return len;
}

static u32 rw_reg[5] = {0};
static ssize_t ilitek_proc_rw_tp_reg_read(struct file *pFile, char __user *buf, size_t size, loff_t *pos)
{
	int ret = 0, len = 0;
	bool mcu_on = 0, read = 0;
	u32 type, addr, read_data, write_data, write_len, stop_mcu;
	bool esd_en = ilits->wq_esd_ctrl, bat_en = ilits->wq_bat_ctrl;

	if (*pos != 0)
		return 0;

	stop_mcu = rw_reg[0];
	type = rw_reg[1];
	addr = rw_reg[2];
	write_data = rw_reg[3];
	write_len = rw_reg[4];

	input_info(true, ilits->dev, "%s stop_mcu = %d\n", __func__, rw_reg[0]);

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, DISABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, DISABLE);

	mutex_lock(&ilits->touch_mutex);

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	if (stop_mcu == mcu_on)
		ret = ili_ice_mode_ctrl(ENABLE, ON);
	else
		ret = ili_ice_mode_ctrl(ENABLE, OFF);

	if (ret < 0) {
		input_err(true, ilits->dev, "%s Failed to enter ICE mode, ret = %d\n", __func__, ret);
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to enter ICE mode");
	}

	if (type == read) {
		if (ili_ice_mode_read(addr, &read_data, sizeof(u32)) < 0) {
			input_err(true, ilits->dev, "%s Read data error\n", __func__);
			len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Read data error");
		}

		input_info(true, ilits->dev, "%s [READ]:addr = 0x%06x, read = 0x%08x\n", __func__, addr, read_data);
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "READ:addr = 0x%06x, read = 0x%08x\n",
				addr, read_data);
	} else {
		if (ili_ice_mode_write(addr, write_data, write_len) < 0) {
			input_err(true, ilits->dev, "%s Write data error\n", __func__);
			len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Write data error");
		}

		input_info(true, ilits->dev, "%s [WRITE]:addr = 0x%06x, write = 0x%08x, len = %d byte\n",
			__func__, addr, write_data, write_len);
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len,
			"WRITE:addr = 0x%06x, write = 0x%08x, len =%d byte\n", addr, write_data, write_len);
	}

	if (stop_mcu == mcu_on)
		ret = ili_ice_mode_ctrl(DISABLE, ON);
	else
		ret = ili_ice_mode_ctrl(DISABLE, OFF);

	if (ret < 0) {
		input_err(true, ilits->dev, "%s Failed to disable ICE mode, ret = %d\n", __func__, ret);
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to disable ICE mode");
	}

	if (copy_to_user((char *)buf, g_user_buf, len))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, ENABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, ENABLE);

	*pos += len;
	mutex_unlock(&ilits->touch_mutex);
	return len;
}

static ssize_t ilitek_proc_rw_tp_reg_write(struct file *filp, const char *buff, size_t size, loff_t *pos)
{
	char *token = NULL, *cur = NULL;
	char cmd[256] = { 0 };
	u32 count = 0;

	if ((size - 1) > sizeof(cmd)) {
		input_err(true, ilits->dev, "%s ERROR! input length is larger than local buffer\n", __func__);
		return -1;
	}

	mutex_lock(&ilits->touch_mutex);

	if (buff != NULL) {
		if (copy_from_user(cmd, buff, size - 1)) {
			input_info(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			size = -1;
			goto out;
		}
	}
	token = cur = cmd;
	while ((token = strsep(&cur, ",")) != NULL) {
		rw_reg[count] = ili_str2hex(token);
		input_info(true, ilits->dev, "%s rw_reg[%d] = 0x%x\n", __func__, count, rw_reg[count]);
		count++;
	}

out:
	mutex_unlock(&ilits->touch_mutex);
	return size;
}

static ssize_t ilitek_proc_debug_switch_read(struct file *pFile, char __user *buff, size_t size, loff_t *pos)
{
	bool open;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	mutex_lock(&ilits->debug_mutex);

	open = !ilits->dnp;

	ilitek_debug_node_buff_control(open);

	size = snprintf(g_user_buf, USER_STR_BUFF * sizeof(unsigned char), "dnp : %s\n",
		ilits->dnp ? "Enabled" : "Disabled");
	*pos = size;

	if (copy_to_user(buff, g_user_buf, size))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	mutex_unlock(&ilits->debug_mutex);
	return size;
}

static ssize_t ilitek_proc_debug_message_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	unsigned long p = *pos;
	int i = 0;
	int send_data_len = 0;
	int ret = 0;
	int data_count = 0;
	int one_data_bytes = 0;
	int need_read_data_len = 0;
	int type = 0;
	unsigned char *tmpbuf = NULL;
	unsigned char tmpbufback[128] = {0};

	if (filp->f_flags & O_NONBLOCK)
		return -EAGAIN;

	if (!ilits->dnp) {
		input_err(true, ilits->dev, "%s Debug flag isn't enabled (%d)\n", __func__, ilits->dnp);
		return -EAGAIN;
	}

	mutex_lock(&ilits->debug_read_mutex);
	ILI_DBG("%s f_count= %d, index = %d, mark = %d\n",
		__func__, ilits->dbf, ilits->odi, ilits->dbl[ilits->odi].mark);
	if (!wait_event_interruptible_timeout(ilits->inq, ilits->dbl[ilits->odi].mark, msecs_to_jiffies(3000))) {
		input_err(true, ilits->dev, "%s WARNING ! there's no data received.\n", __func__);
		mutex_unlock(&ilits->debug_read_mutex);
		*pos = send_data_len;
		return send_data_len;
	}
	mutex_lock(&ilits->debug_mutex);

	tmpbuf = vmalloc(4096);	/* buf size if even */
	if (ERR_ALLOC_MEM(tmpbuf)) {
		input_err(true, ilits->dev, "%s buffer vmalloc error\n", __func__);
		send_data_len += snprintf(tmpbufback + send_data_len, sizeof(tmpbufback), "buffer vmalloc error\n");
		ret = copy_to_user(buff, tmpbufback, send_data_len); /*ilits->dbl[0] */
		goto out;
	}

	if (ilits->dbl[ilits->odi].mark) {
		if (ilits->dbl[ilits->odi].data[0] == P5_X_DEMO_PACKET_ID) {
			need_read_data_len = 43;
		} else if (ilits->dbl[ilits->odi].data[0] == P5_X_I2CUART_PACKET_ID) {
			need_read_data_len =
				 P5_X_DEMO_MODE_PACKET_LEN+P5_X_DEMO_MODE_AXIS_LEN+P5_X_DEMO_MODE_STATE_INFO;
		} else if (ilits->dbl[ilits->odi].data[0] == P5_X_I2CUART_PACKET_ID) {
			type = ilits->dbl[ilits->odi].data[3] & 0x0F;

			data_count = ilits->dbl[ilits->odi].data[1] * ilits->dbl[ilits->odi].data[2];

			if (type == 0 || type == 1 || type == 6)
				one_data_bytes = 1;
			else if (type == 2 || type == 3)
				one_data_bytes = 2;
			else if (type == 4 || type == 5)
				one_data_bytes = 4;

			need_read_data_len = data_count * one_data_bytes + 1 + 5;
		} else if (ilits->dbl[ilits->odi].data[0] == P5_X_DEBUG_PACKET_ID ||
				ilits->dbl[ilits->odi].data[0] == P5_X_DEBUG_LITE_PACKET_ID) {
			send_data_len = 0;	/* ilits->dbl[0][1] - 2; */
			need_read_data_len = TR_BUF_SIZE - 8;
		} else if (ilits->dbl[ilits->odi].data[0] == P5_X_DEBUG_AXIS_PACKET_ID) {
			send_data_len = 0;	/* ilits->dbl[0][1] - 2; */
			need_read_data_len = TR_BUF_SIZE - 8;
		} else if (ilits->dbl[ilits->odi].data[0] == P5_X_DEMO_AXIS_PACKET_ID) {
			need_read_data_len = P5_X_SEC_DEMO_MODE_PACKET_LEN;
		}

		for (i = 0; i < need_read_data_len; i++) {
			send_data_len += snprintf(tmpbuf + send_data_len, sizeof(tmpbufback),
				"%02X", ilits->dbl[ilits->odi].data[i]);
			if (send_data_len >= 4096) {
				input_err(true, ilits->dev, "%s send_data_len = %d set 4096 i = %d\n",
					__func__, send_data_len, i);
				send_data_len = 4096;
				break;
			}
		}

		send_data_len += snprintf(tmpbuf + send_data_len, sizeof(tmpbufback), "\n\n");

		if (p == 5 || size == 4096 || size == 2048) {
			ilits->dbl[ilits->odi].mark = false;
			ilits->odi = ((ilits->odi + 1) % TR_BUF_LIST_SIZE);
		}
	}

	/* Preparing to send debug data to user */
	if (size == 4096)
		ret = copy_to_user(buff, tmpbuf, send_data_len);
	else
		ret = copy_to_user(buff, tmpbuf + p, send_data_len - p);

	if (send_data_len <= 0 || send_data_len > 4096) {
		input_err(true, ilits->dev, "%s send_data_len = %d set 4096\n", __func__, send_data_len);
		send_data_len = 4096;
	}

	if (ret) {
		input_err(true, ilits->dev, "%s copy_to_user err\n", __func__);
		ret = -EFAULT;
	} else {
		*pos += send_data_len;
		ret = send_data_len;
		ILI_DBG("%s Read %d bytes(s) from %ld\n", __func__, send_data_len, p);
	}

out:
	mutex_unlock(&ilits->debug_mutex);
	mutex_unlock(&ilits->debug_read_mutex);
	ipio_vfree((void **)&tmpbuf);
	return send_data_len;
}

static ssize_t ilitek_node_mp_lcm_on_test_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	int ret = 0, len = 2;
	bool esd_en = ilits->wq_esd_ctrl, bat_en = ilits->wq_bat_ctrl;

	if (*pos != 0)
		return 0;

	input_info(true, ilits->dev, "%s Run MP test with LCM on\n", __func__);

	mutex_lock(&ilits->touch_mutex);

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, DISABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, DISABLE);

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));
	ilits->mp_ret_len = 0;

	ret = ili_mp_test_handler(g_user_buf, ON);
	input_info(true, ilits->dev, "%s MP TEST %s, Error code = %d\n", __func__, (ret < 0) ? "FAIL" : "PASS", ret);

	g_user_buf[0] = 3;
	g_user_buf[1] = (ret < 0) ? -ret : ret;
	len += ilits->mp_ret_len;

	len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "MP TEST %s\n", (ret < 0) ? "FAIL" : "PASS");
	if (g_user_buf[1] == EMP_MODE) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to switch MP mode, abort!");
	} else if (g_user_buf[1] == EMP_FW_PROC) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "FW still upgrading, abort!");
	} else if (g_user_buf[1] == EMP_FORMUL_NULL) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "MP formula is null, abort!");
	} else if (g_user_buf[1] == EMP_INI) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Not found ini file, abort!");
	} else if (g_user_buf[1] == EMP_NOMEM) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to allocated memory, abort!");
	} else if (g_user_buf[1] == EMP_PROTOCOL) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len,
			"%s\n", "Protocol version isn't matched, abort!");
	} else if (g_user_buf[1] == EMP_TIMING_INFO) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to get timing info, abort!");
	} else if (g_user_buf[1] == EMP_PARA_NULL) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to get mp parameter, abort!");
	}

	if (copy_to_user((char *)buff, g_user_buf, len))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, ENABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, ENABLE);

	*pos += len;
	mutex_unlock(&ilits->touch_mutex);
	return len;
}

static ssize_t ilitek_node_mp_lcm_off_test_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	int ret = 0, len = 2;
	bool esd_en = ilits->wq_esd_ctrl, bat_en = ilits->wq_bat_ctrl;

	if (*pos != 0)
		return 0;

	input_info(true, ilits->dev, "%s Run MP test with LCM off\n", __func__);

	mutex_lock(&ilits->touch_mutex);

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, DISABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, DISABLE);

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));
	ilits->mp_ret_len = 0;

	ret = ili_mp_test_handler(g_user_buf, OFF);
	input_info(true, ilits->dev, "%s MP TEST %s, Error code = %d\n", __func__, (ret < 0) ? "FAIL" : "PASS", ret);

	g_user_buf[0] = 3;
	g_user_buf[1] = (ret < 0) ? -ret : ret;
	len += ilits->mp_ret_len;

	len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "MP TEST %s\n", (ret < 0) ? "FAIL" : "PASS");
	if (g_user_buf[1] == EMP_MODE) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to switch MP mode, abort!");
	} else if (g_user_buf[1] == EMP_FW_PROC) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "FW still upgrading, abort!");
	} else if (g_user_buf[1] == EMP_FORMUL_NULL) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "MP formula is null, abort!");
	} else if (g_user_buf[1] == EMP_INI) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Not found ini file, abort!");
	} else if (g_user_buf[1] == EMP_NOMEM) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to allocated memory, abort!");
	} else if (g_user_buf[1] == EMP_PROTOCOL) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len,
			"%s\n", "Protocol version isn't matched, abort!");
	} else if (g_user_buf[1] == EMP_TIMING_INFO) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to get timing info, abort!");
	} else if (g_user_buf[1] == EMP_PARA_NULL) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to get mp parameter, abort!");
	}

	if (copy_to_user((char *)buff, g_user_buf, len))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, ENABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, ENABLE);

	*pos += len;
	mutex_unlock(&ilits->touch_mutex);
	return len;
}

static ssize_t ilitek_node_ver_info_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	u32 len = 0;

	if (*pos != 0)
		return 0;

	mutex_lock(&ilits->touch_mutex);

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "CHIP ID = %x\n", ilits->chip->id);
	len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "FW version = %d.%d.%d.%d\n",
			ilits->chip->fw_ver >> 24, (ilits->chip->fw_ver >> 16) & 0xFF,
			(ilits->chip->fw_ver >> 8) & 0xFF, ilits->chip->fw_ver & 0xFF);
	len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "FW MP version = %d.%d.%d.%d\n",
			ilits->chip->fw_mp_ver >> 24, (ilits->chip->fw_mp_ver >> 16) & 0xFF,
			(ilits->chip->fw_mp_ver >> 8) & 0xFF, ilits->chip->fw_mp_ver & 0xFF);

	if (ilits->protocol->core_ver_len == P5_X_CORE_VER_THREE_LENGTH)
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "Core version = %d.%d.%d\n",
			(ilits->chip->core_ver >> 24) & 0xFF, (ilits->chip->core_ver >> 16) & 0xFF,
			(ilits->chip->core_ver >> 8) & 0xFF);
	else
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "Core version = %d.%d.%d.%d\n",
			(ilits->chip->core_ver >> 24) & 0xFF, (ilits->chip->core_ver >> 16) & 0xFF,
			(ilits->chip->core_ver >> 8) & 0xFF, ilits->chip->core_ver & 0xFF);

	len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "Protocol version = %d.%d.%d\n",
			ilits->protocol->ver >> 16, (ilits->protocol->ver >> 8) & 0xFF,
			ilits->protocol->ver & 0xFF);
	len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "Driver version = %s\n", DRIVER_VERSION);

	if (copy_to_user((char *)buff, g_user_buf, len))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	*pos += len;
	mutex_unlock(&ilits->touch_mutex);
	return len;
}

static int ilitek_proc_output_data_show(struct seq_file *m, void *v)
{
	int i;

	input_info(true, ilits->dev, "%s output data node, mode = %d\n", __func__, ilits->output_data_mode);

	mutex_lock(&ilits->debug_mutex);

	if (ilits->output_data_mode == OUTPUT_DATA) {
		if (ilits->output_data != NULL)
			seq_printf(m, "%s", ilits->output_data);
	} else if (ilits->output_data_mode == OUTPUT_CSV_NAME) {
		if (ilits->mp_csv_name != NULL)
			seq_printf(m, "%s", ilits->mp_csv_name);
	} else if (ilits->output_data_mode == OUTPUT_IRAM_DUMP) {
		if (ilits->output_data != NULL) {
			for (i = 0; i < ilits->output_data_len; i++) {
				seq_printf(m, "%02X", ilits->output_data[i]);
				if ((i + 1) % 16 == 0 && i > 0)
					seq_printf(m, "\n");
			}
			seq_printf(m, "\n");
		}
	}

	mutex_unlock(&ilits->debug_mutex);
	return 0;
}

static ssize_t ilitek_node_change_list_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	u32 len = 0;

	if (*pos != 0)
		return 0;

	mutex_lock(&ilits->touch_mutex);

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "============= Change list ==============\n");
	len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "[Drive version] = %s\n", DRIVER_VERSION);
	// len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "[Patch] 202001-0001\n");
	len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "========================================\n");

	if (copy_to_user((char *)buff, g_user_buf, len))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	*pos += len;
	mutex_unlock(&ilits->touch_mutex);
	return len;
}

static ssize_t ilitek_proc_fw_process_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	u32 len = 0;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	len = snprintf(g_user_buf, USER_STR_BUFF * sizeof(unsigned char), "%02d\n", ilits->fw_update_stat);

	input_info(true, ilits->dev, "%s update status = %d\n", __func__, ilits->fw_update_stat);

	if (copy_to_user((char *)buff, &ilits->fw_update_stat, len))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	*pos = len;
	return len;
}

static ssize_t ilitek_node_fw_upgrade_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	int ret = 0, len = 2;
	bool esd_en = ilits->wq_esd_ctrl, bat_en = ilits->wq_bat_ctrl;

	if (*pos != 0)
		return 0;

	input_info(true, ilits->dev, "%s Prepar to upgarde firmware\n", __func__);

	mutex_lock(&ilits->touch_mutex);

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, DISABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, DISABLE);

	ilits->force_fw_update = ENABLE;

	ret = ili_fw_upgrade_handler(NULL);

	ilits->force_fw_update = DISABLE;

	g_user_buf[0] = 0x0;
	g_user_buf[1] = (ret < 0) ? -ret : ret;

	if (g_user_buf[1] == 0)
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Upgrade firmware = PASS");
	else
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Upgrade firmware = FAIL");

	/* Reason for fail */
	if (g_user_buf[1] == EFW_CONVERT_FILE) {
		g_user_buf[0] = 0xFF;
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len,
			"%s\n", "Failed to convert hex/ili file, abort!");
	} else if (g_user_buf[1] == ENOMEM) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to allocate memory, abort!");
	} else if (g_user_buf[1] == EFW_ICE_MODE) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to operate ice mode, abort!");
	} else if (g_user_buf[1] == EFW_CRC) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "CRC not matched, abort!");
	} else if (g_user_buf[1] == EFW_REST) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to do reset, abort!");
	} else if (g_user_buf[1] == EFW_ERASE) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to erase flash, abort!");
	} else if (g_user_buf[1] == EFW_PROGRAM) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to program flash, abort!");
	} else if (g_user_buf[1] == EFW_INTERFACE) {
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len,
			"%s\n", "Failed to hex file interface no match, abort!");
	}

	if (copy_to_user((u32 *) buff, g_user_buf, len))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, ENABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, ENABLE);

	*pos += len;
	mutex_unlock(&ilits->touch_mutex);
	return len;
}

static ssize_t ilitek_proc_debug_level_read(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	debug_en = !debug_en;

	input_info(true, ilits->dev, "%s  %s debug level = %x\n", __func__, debug_en ? "Enable" : "Disable", debug_en);

	size = snprintf(g_user_buf, USER_STR_BUFF * sizeof(unsigned char), "debug level : %s\n",
			debug_en ? "Enable" : "Disable");

	*pos += size;

	if (copy_to_user((u32 *) buff, g_user_buf, size))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	return size;
}

static ssize_t ilitek_proc_get_customer_info(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{

	u8 cmd[1] = {0x26};
	u8 buf[8] = {0};
	u8 retry = 25, checksum = 0;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	input_info(true, ilits->dev, "%s %c%c%02d%02d%02d%02d\n",
		__func__, ilits->fw_customer_info[0], ilits->fw_customer_info[1], ilits->fw_customer_info[2],
		ilits->fw_customer_info[3], ilits->fw_customer_info[4], ilits->fw_customer_info[5]);
	do {
		if (ilits->wrapper(cmd, sizeof(u8), buf, 8, ON, OFF) < 0)
			input_err(true, ilits->dev, "%s Write pre cmd failed\n", __func__);

		checksum = ili_calc_packet_checksum(buf, 7);

		if (retry <= 0) {
			buf[6] = 0;
			break;
		}
		input_info(true, ilits->dev, "%s retry time:%d\n", __func__, retry);
		retry--;
	} while (checksum != buf[7]);
	input_info(true, ilits->dev, "%s %02d%c%c%02d%02d%02d%02d%02d\n",
		__func__, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

	size = snprintf(g_user_buf, USER_STR_BUFF * sizeof(unsigned char),
			 "%c%c%02d%02d%02d%02d\nFirmware version(From FW file) = %02d\nFirmware version(From IC) = %02d\n"
			  , ilits->fw_customer_info[0], ilits->fw_customer_info[1], ilits->fw_customer_info[2]
			  , ilits->fw_customer_info[3], ilits->fw_customer_info[4], ilits->fw_customer_info[5]
			  , ilits->fw_customer_info[5]
			  , buf[6]);

	*pos += size;

	if (copy_to_user((u32 *) buff, g_user_buf, size))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	return size;
}

static ssize_t ilitek_proc_sram_test_info(struct file *filp, char __user *buff, size_t size, loff_t *pos)
{
	int len = 0, ret = 0;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	ret = ili_reset_ctrl(TP_IC_WHOLE_RST);
	if (ret < 0) {
		input_err(true, ilits->dev, "%s TP_IC_WHOLE_RST Failed, ret = %d\n", __func__, ret);
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "TP_IC_WHOLE_RST Failed");
	}

	msleep(30);

	ret = ili_ice_mode_ctrl(ENABLE, OFF);
	if (ret < 0) {
		input_err(true, ilits->dev, "Failed to disable ICE mode, ret = %d\n", ret);
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "Failed to disable ICE mode");
	}

	ret = ili_tddi_ic_sram_test();
	if (ret < 0)
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "SRAM TEST Failed");
	else
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "SRAM TEST Pass");

	ret = ili_reset_ctrl(TP_IC_WHOLE_RST);
	if (ret < 0) {
		input_err(true, ilits->dev, "TP_IC_WHOLE_RST Failed, ret = %d\n", ret);
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "TP_IC_WHOLE_RST Failed");
	}

	msleep(50);

	ret = ili_fw_upgrade_handler(NULL);
	if (ret < 0) {
		input_err(true, ilits->dev, "fw upgrade Failed, ret = %d\n", ret);
		len += snprintf(g_user_buf + len, USER_STR_BUFF - len, "%s\n", "fw upgrade Failed");
	}

	if (copy_to_user((char *)buff, g_user_buf, len))
		input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);

	*pos += len;
	return len;
}

int ili_get_tp_recore_ctrl(int data)
{
	int ret = 0;

	switch ((int)data) {

	case ENABLE_RECORD:
		input_info(true, ilits->dev, "%s recore enable", __func__);
		ret = ili_ic_func_ctrl("tp_recore", 1);
		msleep(200);
		break;
	case DATA_RECORD:
		msleep(50);
		input_info(true, ilits->dev, "%s Get data", __func__);
		ret = ili_ic_func_ctrl("tp_recore", 2);
		if (ret < 0) {
			input_err(true, ilits->dev, "%s cmd fail\n", __func__);
			goto out;
		}

		ret = ili_get_tp_recore_data();
		if (ret < 0)
			input_err(true, ilits->dev, "%s get data fail\n", __func__);

		input_info(true, ilits->dev, "%s recore reset", __func__);
		ret = ili_ic_func_ctrl("tp_recore", 3);
		if (ret < 0) {
			input_err(true, ilits->dev, "%s cmd fail\n", __func__);
			goto out;
		}
		break;
	case DISABLE_RECORD:
		input_info(true, ilits->dev, "%s recore disable", __func__);
		ret = ili_ic_func_ctrl("tp_recore", 0);
		break;

	}
out:
	return ret;

}

int ili_get_tp_recore_data(void)
{
	u8 buf[8] = {0}, record_case = 0;
	s8 index;
	u16 *raw = NULL, *raw_ptr = NULL, frame_len = 0;
	u32 base_addr = 0x20000, addr, len, *ptr, i, fcnt;
	struct record_state record_stat;
	bool ice = atomic_read(&ilits->ice_stat);

	if (ilits->wrapper(NULL, 0, buf, sizeof(buf), OFF, OFF) < 0) {
		input_err(true, ilits->dev, "%s Get info fail\n", __func__);
		return -1;
	}

	addr = ((buf[0] << 8) | buf[1]) + base_addr;
	len = ((buf[2] << 8) | buf[3]);
	index = buf[4];
	fcnt = buf[5];
	record_case = buf[6];
	ipio_memcpy(&record_stat, &buf[7], 1, 1);
	input_info(true, ilits->dev, "%s addr = 0x%x, len = %d, lndex = %d, fram num = %d, record_case = 0x%x\n",
		__func__, addr, len, index, fcnt, record_case);
	ili_dump_data(buf, 8, sizeof(buf), 0, "all record bytes");

	raw = kcalloc(len, sizeof(u8), GFP_ATOMIC);
	if (ERR_ALLOC_MEM(raw)) {
		input_err(true, ilits->dev, "%s Failed to allocate packet memory, %ld\n", __func__, PTR_ERR(raw));
		return -1;
	}
	ptr = (u32 *)raw;

	if (!ice)
		ili_ice_mode_ctrl(ENABLE, ON);

	buf[0] = 0x25;
	buf[3] = (char)((addr & 0x00FF0000) >> 16);
	buf[2] = (char)((addr & 0x0000FF00) >> 8);
	buf[1] = (char)((addr & 0x000000FF));

	if (ilits->wrapper(buf, 4, NULL, 0, ON, OFF)) {
		input_err(true, ilits->dev, "%s Failed to write iram data\n", __func__);
		return -ENODEV;
	}

	if (ilits->wrapper(NULL, 0, (u8 *)raw, len, ON, OFF)) {
		input_err(true, ilits->dev, "%s Failed to Read iram data\n", __func__);
		return -ENODEV;
	}

	frame_len = (len / (fcnt * 2));
	for (i = 0; i < fcnt; i++) {
		raw_ptr = raw + (index * frame_len);

		ili_dump_data(raw_ptr, 16, frame_len, ilits->xch_num, "recore_data");
		index--;
		if (index < 0)
			index = fcnt - 1;
	}

	if (!ice)
		ili_ice_mode_ctrl(DISABLE, ON);

	if (record_case == 2) {
		input_info(true, ilits->dev, "%s tp_palm_stat = %d\n", __func__, record_stat.touch_palm_state_e);
		input_info(true, ilits->dev, "%s app_an_stat = %d\n", __func__, record_stat.app_an_statu_e);
		input_info(true, ilits->dev, "%s app_check_abnor = %d\n",
			__func__, record_stat.app_sys_check_bg_abnormal);
		input_info(true, ilits->dev, "%s wrong_bg = %d\n", __func__, record_stat.g_b_wrong_bg);
	}

	ipio_kfree((void **)&raw);

	return 0;
}

void ili_gesture_fail_reason(bool enable)
{

	u8 cmd[24] = {0};

	/* set symbol */
	if (ili_ic_func_ctrl("knock_en", 0x8) < 0)
		input_err(true, ilits->dev, "%s set symbol failed", __func__);

	/* enable gesture fail reason */
	cmd[0] = 0x01;
	cmd[1] = 0x0A;
	cmd[2] = 0x10;
	if (enable)
		cmd[3] = 0x01;
	else
		cmd[3] = 0x00;
	cmd[4] = 0xFF;
	cmd[5] = 0xFF;
	if ((ilits->wrapper(cmd, 6, NULL, 0, ON, OFF)) < 0)
		input_err(true, ilits->dev, "%s enable gesture fail reason failed", __func__);

	/* set gesture parameters */
	cmd[0] = 0x01;
	cmd[1] = 0x0A;
	cmd[2] = 0x12;
	cmd[3] = 0x01;
	memset(cmd + 4, 0xFF, 20);
	if ((ilits->wrapper(cmd, 24, NULL, 0, ON, OFF)) < 0)
		input_err(true, ilits->dev, "%s set gesture parameters failed", __func__);

	/* get gesture parameters */
	cmd[0] = 0x01;
	cmd[1] = 0x0A;
	cmd[2] = 0x11;
	cmd[3] = 0x01;
	if ((ilits->wrapper(cmd, 4, NULL, 0, ON, OFF)) < 0)
		input_err(true, ilits->dev, "%s get gesture parameters failed", __func__);

}

int ili_tp_data_mode_ctrl(u8 *cmd)
{
	int ret = 0;

	switch (cmd[0]) {
	case AP_MODE:
		if (ilits->tp_suspend) {
			if (ili_set_tp_data_len(DATA_FORMAT_GESTURE_DEMO, false, NULL) < 0) {
				input_err(true, ilits->dev, "%s Failed to set demo len from gesture mode\n", __func__);
				ret = -ENOTTY;
			}
		} else {
			if (ili_switch_tp_mode(P5_X_FW_AP_MODE) < 0) {
				input_err(true, ilits->dev, "%s Failed to switch demo mode\n", __func__);
				ret = -ENOTTY;
			}
		}
		ilits->noiseflag = false;
		break;
	case TEST_MODE:
		if (ili_switch_tp_mode(P5_X_FW_TEST_MODE) < 0) {
			input_err(true, ilits->dev, "%s Failed to switch test mode\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case DEBUG_MODE:
		if (!ilits->tp_suspend && (ilits->actual_tp_mode != P5_X_FW_AP_MODE)) {
			if (ili_switch_tp_mode(P5_X_FW_AP_MODE) < 0) {
				input_err(true, ilits->dev, "%s Failed to switch demo mode\n", __func__);
				ret = -ENOTTY;
				break;
			}
		}
		if (ilits->tp_suspend) {
			if (ili_set_tp_data_len(DATA_FORMAT_GESTURE_DEBUG, false, NULL) < 0) {
				input_err(true, ilits->dev, "%s Failed to set debug len from gesture mode\n", __func__);
				ret = -ENOTTY;
			}
		} else {
			if (ili_set_tp_data_len(DATA_FORMAT_DEBUG, false, NULL) < 0) {
				input_err(true, ilits->dev, "%s Failed to set debug len\n", __func__);
				ret = -ENOTTY;
			}
		}
		break;
	case DEBUG_LITE_ROI:
		if (ili_set_tp_data_len(DATA_FORMAT_DEBUG_LITE_ROI, false, NULL) < 0) {
			input_err(true, ilits->dev, "%s Failed to set debug lite roi from gesture mode\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case DEBUG_LITE_WINDOW:
		if (ili_set_tp_data_len(DATA_FORMAT_DEBUG_LITE_WINDOW, false, NULL) < 0) {
			input_err(true, ilits->dev, "%s Failed to set debug lite window from gesture mode\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case DEBUG_LITE_AREA:
		if (ili_set_tp_data_len(DATA_FORMAT_DEBUG_LITE_AREA, false, &cmd[1]) < 0) {
			input_err(true, ilits->dev, "%s Failed to set debug lite area from gesture mode\n", __func__);
			ret = -ENOTTY;
		}
		break;
	default:
		input_err(true, ilits->dev, "%s Unknown TP mode ctrl\n", __func__);
		ret = -ENOTTY;
		break;
	}
	ilits->tp_data_mode = cmd[0];

	return ret;
}

static ssize_t ilitek_node_ioctl_write(struct file *filp, const char *buff, size_t size, loff_t *pos)
{
	int i, ret = 0, count = 0;
	char cmd[512] = {0};
	char *token = NULL, *cur = NULL;
	u8 temp[256] = {0};
	u32 *data = NULL;

	if ((size - 1) > sizeof(cmd)) {
		input_err(true, ilits->dev, "%s ERROR! input length is larger than local buffer\n", __func__);
		return -1;
	}

	mutex_lock(&ilits->touch_mutex);

	if (buff != NULL) {
		if (copy_from_user(cmd, buff, size - 1)) {
			input_info(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			mutex_unlock(&ilits->touch_mutex);
			return -1;
		}
	}

	input_info(true, ilits->dev, "%s size = %d, cmd = %s\n", __func__, (int)size, cmd);

	token = cur = cmd;

	data = kcalloc(512, sizeof(u32), GFP_KERNEL);
	if (ERR_ALLOC_MEM(data)) {
		input_err(true, ilits->dev, "%s Failed to allocate data mem\n", __func__);
		size = -1;
		goto out;
	}

	while ((token = strsep(&cur, ",")) != NULL) {
		data[count] = ili_str2hex(token);
		input_info(true, ilits->dev, "%s data[%d] = %x\n", __func__, count, data[count]);
		count++;
	}

	input_info(true, ilits->dev, "%s cmd = %s\n", __func__, cmd);

	if (strncmp(cmd, "hwreset", strlen(cmd)) == 0) {
		ili_reset_ctrl(TP_HW_RST_ONLY);
	} else if (strcmp(cmd, "rawdatarecore") == 0) {
		if (data[1] == ENABLE_RECORD)
			ili_get_tp_recore_ctrl(ENABLE_RECORD);
		else if (data[1] == DATA_RECORD)
			ili_get_tp_recore_ctrl(DATA_RECORD);
		else if (data[1] == DISABLE_RECORD)
			ili_get_tp_recore_ctrl(DISABLE_RECORD);
	} else if (strcmp(cmd, "switchdemodebuginfomode") == 0) {
		ili_set_tp_data_len(DATA_FORMAT_DEMO_DEBUG_INFO, false, NULL);
	} else if (strcmp(cmd, "gesturedemoen") == 0) {
		if (data[1] == DISABLE)
			ilits->gesture_demo_ctrl = DISABLE;
		else
			ilits->gesture_demo_ctrl = ENABLE;
		ili_set_tp_data_len(DATA_FORMAT_GESTURE_DEMO, false, NULL);
	} else if (strcmp(cmd, "gesturefailrsn") == 0) {
		if (data[1] == DISABLE)
			ili_gesture_fail_reason(DISABLE);
		else
			ili_gesture_fail_reason(ENABLE);
		input_info(true, ilits->dev, "%s %s gesture fail reason\n", __func__, data[1] ? "ENABLE" : "DISABLE");
	} else if (strncmp(cmd, "icwholereset", strlen(cmd)) == 0) {
		ili_ice_mode_ctrl(ENABLE, OFF);
		ili_reset_ctrl(TP_IC_WHOLE_RST);
	} else if (strncmp(cmd, "iccodereset", strlen(cmd)) == 0) {
		ili_ice_mode_ctrl(ENABLE, OFF);
		ili_reset_ctrl(TP_IC_CODE_RST);
		ili_ice_mode_ctrl(DISABLE, OFF);
	} else if (strncmp(cmd, "enableirq", strlen(cmd)) == 0) {
		input_info(true, ilits->dev, "%s Enable IRQ\n", __func__);
		ili_irq_enable();
	} else if (strncmp(cmd, "disableirq", strlen(cmd)) == 0) {
		input_info(true, ilits->dev, "%s Disable IRQ\n", __func__);
		ili_irq_disable();
	} else if (strncmp(cmd, "infofromhex", strlen(cmd)) == 0) {
		ilits->info_from_hex = data[1];
		input_info(true, ilits->dev, "%s info from hex = %d\n", __func__, data[1]);
	} else if (strncmp(cmd, "getinfo", strlen(cmd)) == 0) {
		ili_ice_mode_ctrl(ENABLE, OFF);
		ili_ic_get_info();
		ili_ice_mode_ctrl(DISABLE, OFF);
		ili_ic_get_protocl_ver();
		ili_ic_get_fw_ver();
		ili_ic_get_core_ver();
		ili_ic_get_tp_info();
		ili_ic_get_panel_info();
		input_info(true, ilits->dev, "%s Driver version = %s\n", __func__, DRIVER_VERSION);
	} else if (strncmp(cmd, "enableicemode", strlen(cmd)) == 0) {
		if (data[1] == ON)
			ili_ice_mode_ctrl(ENABLE, ON);
		else
			ili_ice_mode_ctrl(ENABLE, OFF);
	} else if (strncmp(cmd, "wqctrl", strlen(cmd)) == 0) {
		ilits->wq_ctrl = !ilits->wq_ctrl;
		input_info(true, ilits->dev, "%s wq_ctrl flag= %d\n", __func__, ilits->wq_ctrl);
	} else if (strncmp(cmd, "disableicemode", strlen(cmd)) == 0) {
		ili_ice_mode_ctrl(DISABLE, OFF);
	} else if (strncmp(cmd, "enablewqesd", strlen(cmd)) == 0) {
		ili_wq_ctrl(WQ_ESD, ENABLE);
	} else if (strncmp(cmd, "enablewqbat", strlen(cmd)) == 0) {
		ili_wq_ctrl(WQ_BAT, ENABLE);
	} else if (strncmp(cmd, "disablewqesd", strlen(cmd)) == 0) {
		ili_wq_ctrl(WQ_ESD, DISABLE);
	} else if (strncmp(cmd, "disablewqbat", strlen(cmd)) == 0) {
		ili_wq_ctrl(WQ_BAT, DISABLE);
	} else if (strncmp(cmd, "gesture", strlen(cmd)) == 0) {
		ilits->gesture = !ilits->gesture;
		input_info(true, ilits->dev, "%s gesture = %d\n", __func__, ilits->gesture);
	} else if (strncmp(cmd, "esdgesture", strlen(cmd)) == 0) {
		if (ili_gesture_recovery() < 0) {
			input_err(true, ilits->dev, "%s Gesture recovery failed\n", __func__);
			size = -1;
		}
	} else if (strncmp(cmd, "esdspi", strlen(cmd)) == 0) {
		ili_spi_recovery();
	} else if (strncmp(cmd, "sleepin", strlen(cmd)) == 0) {
		ili_ic_func_ctrl("sleep", SLEEP_IN);
	} else if (strncmp(cmd, "deepsleepin", strlen(cmd)) == 0) {
		ili_ic_func_ctrl("sleep", DEEP_SLEEP_IN);
	} else if (strncmp(cmd, "iceflag", strlen(cmd)) == 0) {
		if (data[1] == ENABLE)
			atomic_set(&ilits->ice_stat, ENABLE);
		else
			atomic_set(&ilits->ice_stat, DISABLE);
		input_info(true, ilits->dev, "%s ice mode flag = %d\n", __func__, atomic_read(&ilits->ice_stat));
	} else if (strncmp(cmd, "gesturenormal", strlen(cmd)) == 0) {
		ilits->gesture_mode = DATA_FORMAT_GESTURE_NORMAL;
		input_info(true, ilits->dev, "%s gesture mode = %d\n", __func__, ilits->gesture_mode);
	} else if (strncmp(cmd, "gestureinfo", strlen(cmd)) == 0) {
		ilits->gesture_mode = DATA_FORMAT_GESTURE_INFO;
		input_info(true, ilits->dev, "%s gesture mode = %d\n", __func__, ilits->gesture_mode);
	} else if (strncmp(cmd, "switchtestmode", strlen(cmd)) == 0) {
		ili_switch_tp_mode(P5_X_FW_TEST_MODE);
	} else if (strncmp(cmd, "switchdebugmode", strlen(cmd)) == 0) {
		ili_set_tp_data_len(DATA_FORMAT_DEBUG, false, NULL);
	} else if (strncmp(cmd, "switchdemomode", strlen(cmd)) == 0) {
		ili_set_tp_data_len(DATA_FORMAT_DEMO, false, NULL);
	} else if (strncmp(cmd, "switchgesturedebugmode", strlen(cmd)) == 0) {
		ili_set_tp_data_len(DATA_FORMAT_GESTURE_DEBUG, false, NULL);
	} else if (strncmp(cmd, "dbgflag", strlen(cmd)) == 0) {
		ilits->dnp = !ilits->dnp;
		input_info(true, ilits->dev, "%s debug flag message = %d\n", __func__, ilits->dnp);
	} else if (strncmp(cmd, "spiclk", strlen(cmd)) == 0) {
		input_info(true, ilits->dev, "%s spi clk num = %d\n", __func__, data[1]);
		ili_core_spi_setup(data[1]);
	} else if (strncmp(cmd, "ss", strlen(cmd)) == 0) {
		input_info(true, ilits->dev, "%s sense_stop = %d\n", __func__, data[1]);
		ilits->ss_ctrl = data[1];
	} else if (strncmp(cmd, "iow", strlen(cmd)) == 0) {
		int w_len = 0;

		w_len = data[1];
		input_info(true, ilits->dev, "%s w_len = %d\n", __func__, w_len);
		for (i = 0; i < w_len; i++) {
			temp[i] = data[2 + i];
			input_info(true, ilits->dev, "%s write[%d] = %x\n", __func__, i, temp[i]);
		}

		ilits->wrapper(temp, w_len, NULL, 0, ON, OFF);
	} else if (strncmp(cmd, "ior", strlen(cmd)) == 0) {
		int r_len = 0;

		r_len = data[1];
		input_info(true, ilits->dev, "%s r_len = %d\n", __func__, r_len);
		ilits->wrapper(NULL, 0, temp, r_len, ON, OFF);
		for (i = 0; i < r_len; i++)
			input_info(true, ilits->dev, "%s read[%d] = %x\n", __func__, i, temp[i]);
	} else if (strncmp(cmd, "iowr", strlen(cmd)) == 0) {
		int delay = 0;
		int w_len = 0, r_len = 0;

		w_len = data[1];
		r_len = data[2];
		delay = data[3];
		input_info(true, ilits->dev, "%s w_len = %d, r_len = %d, delay = %d\n", __func__, w_len, r_len, delay);

		for (i = 0; i < w_len; i++) {
			temp[i] = data[4 + i];
			input_info(true, ilits->dev, "%s write[%d] = %x\n", __func__, i, temp[i]);
		}
		ret = ilits->wrapper(temp, w_len, NULL, 0, ON, OFF);
		memset(temp, 0, sizeof(temp));
		msleep(delay);
		ilits->wrapper(NULL, 0, temp, r_len, ON, OFF);
		for (i = 0; i < r_len; i++)
			input_info(true, ilits->dev, "%s read[%d] = %x\n", __func__, i, temp[i]);
	} else if (strcmp(cmd, "iowrint") == 0) {
		int delay = 0;
		int w_len = 0, r_len = 0;

		w_len = data[1];
		r_len = data[2];
		delay = data[3];
		input_info(true, ilits->dev, "%s w_len = %d, r_len = %d, delay = %d\n", __func__, w_len, r_len, delay);
		for (i = 0; i < w_len; i++) {
			temp[i] = data[4 + i];
			input_info(true, ilits->dev, "%s write[%d] = %x\n", __func__, i, temp[i]);
		}
		ret = ilits->wrapper(temp, w_len, temp, r_len, ON, ON);
		for (i = 0; i < r_len; i++)
			input_info(true, ilits->dev, "%s read[%d] = %x\n", __func__, i, temp[i]);
	} else if (strncmp(cmd, "getddiregdata", strlen(cmd)) == 0) {
		u8 ddi_data = 0;

		input_info(true, ilits->dev, "%s Get ddi reg one page: page = %x, reg = %x\n",
			__func__, data[1], data[2]);
		ili_ic_get_ddi_reg_onepage(data[1], data[2], &ddi_data);
		input_info(true, ilits->dev, "%s ddi_data = %x\n", __func__, ddi_data);
	} else if (strncmp(cmd, "setddiregdata", strlen(cmd)) == 0) {
		input_info(true, ilits->dev, "%s Set ddi reg one page: page = %x, reg = %x, data = %x\n",
			__func__, data[1], data[2], data[3]);
		ili_ic_set_ddi_reg_onepage(data[1], data[2], data[3]);
	} else if (strncmp(cmd, "dumpflashdata", strlen(cmd)) == 0) {
		ilits->fw_update_stat = FW_STAT_INIT;
		input_info(true, ilits->dev, "%s Start = 0x%x, End = 0x%x, Dump Hex path = %s\n",
			__func__, data[1], data[2], DUMP_FLASH_PATH);
		ret = ili_fw_dump_flash_data(data[1], data[2], false);
		ilits->fw_update_stat = (ret < 0) ? FW_UPDATE_FAIL : FW_UPDATE_PASS;
		input_info(true, ilits->dev, "%s ilits->fw_update_stat = %d\n", __func__, ilits->fw_update_stat);
	} else if (strncmp(cmd, "dumpiramdata", strlen(cmd)) == 0) {
		input_info(true, ilits->dev, "%s Start = 0x%x, End = 0x%x\n",
			__func__, data[1], data[2]);
		ilits->fw_update_stat = FW_STAT_INIT;
		ret = ili_fw_dump_iram_data(data[1], data[2], true);
		ilits->fw_update_stat = (ret < 0) ? FW_UPDATE_FAIL : FW_UPDATE_PASS;
		input_info(true, ilits->dev, "%s ilits->fw_update_stat = %d\n", __func__, ilits->fw_update_stat);
	} else if (strncmp(cmd, "edge_palm_ctrl", strlen(cmd)) == 0) {
		ili_ic_func_ctrl("edge_palm", data[1]);
	} else if (strncmp(cmd, "uart_mode_ctrl", strlen(cmd)) == 0) {
		ili_fw_uart_ctrl(data[1]);
	} else if (strncmp(cmd, "flashesdgesture", strlen(cmd)) == 0) {
		ili_touch_esd_gesture_flash();
	} else if (strncmp(cmd, "intact", strlen(cmd)) == 0) {
		input_info(true, ilits->dev, "%s Set INT trigger = %x\n", __func__, data[1]);
		ili_ic_int_trigger_ctrl(data[1]);
	} else if (strncmp(cmd, "spiw", strlen(cmd)) == 0) {
		int wlen;

		wlen = data[1];
		temp[0] = 0x82;
		for (i = 0; i < wlen; i++) {
			temp[i] = data[2 + i];
			input_info(true, ilits->dev, "%s write[%d] = %x\n", __func__, i, temp[i]);
		}
		ilits->spi_write_then_read(ilits->spi, temp, wlen, NULL, 0);
	} else if (strncmp(cmd, "spir", strlen(cmd)) == 0) {
		int rlen;
		u8 *rbuf = NULL;

		rlen = data[1];
		rbuf = kzalloc(rlen, GFP_KERNEL | GFP_DMA);
		if (ERR_ALLOC_MEM(rbuf)) {
			input_err(true, ilits->dev, "%s Failed to allocate dma_rxbuf, %ld\n", __func__, PTR_ERR(rbuf));
			kfree(rbuf);
			goto out;
		}
		temp[0] = 0x83;
		ilits->spi_write_then_read(ilits->spi, temp, 1, rbuf, rlen);
		for (i = 0; i < rlen; i++)
			input_info(true, ilits->dev, "%s read[%d] = %x\n", __func__, i, rbuf[i]);
		kfree(rbuf);
	} else if (strncmp(cmd, "spirw", strlen(cmd)) == 0) {
		int wlen, rlen;
		u8 *rbuf = NULL;

		wlen = data[1];
		rlen = data[2];
		for (i = 0; i < wlen; i++) {
			temp[i] = data[3 + i];
			input_info(true, ilits->dev, "%s write[%d] = %x\n", __func__, i, temp[i]);
		}
		if (rlen != 0) {
			rbuf = kzalloc(rlen, GFP_KERNEL | GFP_DMA);
			if (ERR_ALLOC_MEM(rbuf)) {
				input_err(true, ilits->dev, "%s Failed to allocate dma_rxbuf, %ld\n",
					__func__, PTR_ERR(rbuf));
				kfree(rbuf);
				goto out;
			}
		}
		ilits->spi_write_then_read(ilits->spi, temp, wlen, rbuf, rlen);
		if (rlen != 0) {
			for (i = 0; i < rlen; i++)
				input_info(true, ilits->dev, "%s read[%d] = %x\n", __func__, i, rbuf[i]);
		}
		kfree(rbuf);
	} else if (strncmp(cmd, "ges_sym", strlen(cmd)) == 0) {
		struct gesture_symbol *ptr_sym = &ilits->ges_sym;
		u8 *ptr;

		ptr = (u8 *) ptr_sym;
		ptr[0] = data[1];
		ptr[1] = data[2];
		ptr[2] = data[3];
		ili_dump_data(ptr, 8, 3, 0, "Gesture symbol");
	} else if (strncmp(cmd, "mode", strlen(cmd)) == 0) {
		temp[0] = data[1];
		temp[1] = data[2];
		temp[2] = data[3];
		temp[3] = data[4];
		temp[4] = data[5];
		ret = ili_tp_data_mode_ctrl(temp);
	} else if (strncmp(cmd, "switchmpretry", strlen(cmd)) == 0) {
		ilits->mp_retry = !ilits->mp_retry;
		input_info(true, ilits->dev, "%s switchmpretry = %d\n", __func__, ilits->mp_retry);
	} else if (strncmp(cmd, "proxfacemode", strlen(cmd)) == 0) {
		ilits->prox_face_mode = data[1];

		if (ili_ic_func_ctrl("proximity", ilits->prox_face_mode) < 0)
			input_err(true, ilits->dev, "%s Write proximity cmd failed\n", __func__);

		input_info(true, ilits->dev, "%s switch proximity face mode = %s\n",
			__func__, (ilits->prox_face_mode ? "ON" : "OFF"));
	} else if (strncmp(cmd, "sipmode", strlen(cmd)) == 0) {
		if (ili_ic_func_ctrl("sip_mode", data[1]) < 0)
			input_err(true, ilits->dev, "%s Write sip_mode cmd failed\n", __func__);
	} else if (strncmp(cmd, "gamemode", strlen(cmd)) == 0) {
		bool option = (data[1] == 0x01) ? DISABLE : ENABLE;

		if (ili_ic_func_ctrl("lock_point", option) < 0)
			input_err(true, ilits->dev, "%s Write game_mode cmd failed\n", __func__);
	} else if (strncmp(cmd, "deadzonectrl", strlen(cmd)) == 0) {
		if (ili_ic_func_ctrl("dead_zone_ctrl", data[1]) < 0)
			input_err(true, ilits->dev, "%s Write dead_zone_ctrl cmd failed\n", __func__);
	} else if (strncmp(cmd, "proxfacemode_simulation", strlen(cmd)) == 0) {
		u8 temp = data[1];

		ili_report_proximity_mode(temp, 1);
	} else if (strncmp(cmd, "doubletap", strlen(cmd)) == 0) {
		ilits->ges_sym.double_tap = !ilits->ges_sym.double_tap;
		input_info(true, ilits->dev, "double_tap = %d\n", ilits->ges_sym.double_tap);
	} else if (strncmp(cmd, "swipe", strlen(cmd)) == 0) {
		ilits->ges_sym.alphabet_line_2_top = !ilits->ges_sym.alphabet_line_2_top;
		input_info(true, ilits->dev, "alphabet_line_2_top = %d\n", ilits->ges_sym.alphabet_line_2_top);
	} else if (strncmp(cmd, "covermode", strlen(cmd)) == 0) {
		if (ili_ic_func_ctrl("cover_mode", data[1]) < 0)
			input_info(true, ilits->dev, "%s Write cover_mode cmd failed\n", __func__);
	} else if (strncmp(cmd, "outputdatamode", strlen(cmd)) == 0) {
		ilits->output_data_mode = data[1];
		input_info(true, ilits->dev, "%s set output data mode = %d\n", __func__, ilits->output_data_mode);
	} else if (strncmp(cmd, "mptestitem", strlen(cmd)) == 0) {
		ilits->mp_test_item = data[1];
		input_info(true, ilits->dev, "%s set mp test item = %d\n", __func__, ilits->mp_test_item);
	} else {
		input_err(true, ilits->dev, "%s Unknown command\n", __func__);
		size = -1;
	}
out:
	ipio_kfree((void **)&data);
	mutex_unlock(&ilits->touch_mutex);
	return size;
}

#ifdef CONFIG_COMPAT
static long ilitek_node_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		input_err(true, ilits->dev, "%s There's no unlocked_ioctl defined in file\n", __func__);
		return -ENOTTY;
	}

	ILI_DBG("%s cmd = %d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
	case ILITEK_COMPAT_IOCTL_I2C_WRITE_DATA:
		ILI_DBG("%s compat_ioctl: convert i2c/spi write\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_I2C_WRITE_DATA, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_I2C_READ_DATA:
		ILI_DBG("%s compat_ioctl: convert i2c/spi read\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_I2C_READ_DATA, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_I2C_SET_WRITE_LENGTH:
		ILI_DBG("%s compat_ioctl: convert set write length\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_I2C_SET_WRITE_LENGTH,
			(unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_I2C_SET_READ_LENGTH:
		ILI_DBG("%s compat_ioctl: convert set read length\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_I2C_SET_READ_LENGTH,
			(unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_HW_RESET:
		ILI_DBG("%s compat_ioctl: convert hw reset\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_HW_RESET, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_POWER_SWITCH:
		ILI_DBG("%s compat_ioctl: convert power switch\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_POWER_SWITCH, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_REPORT_SWITCH:
		ILI_DBG("%s compat_ioctl: convert report switch\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_REPORT_SWITCH, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_IRQ_SWITCH:
		ILI_DBG("%s compat_ioctl: convert irq switch\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_IRQ_SWITCH, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_DEBUG_LEVEL:
		ILI_DBG("%s compat_ioctl: convert debug level\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_DEBUG_LEVEL, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_FUNC_MODE:
		ILI_DBG("%s compat_ioctl: convert format mode\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_FUNC_MODE, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_FW_VER:
		ILI_DBG("%s compat_ioctl: convert set read length\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_FW_VER, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_PL_VER:
		ILI_DBG("%s compat_ioctl: convert fw version\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_PL_VER, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_CORE_VER:
		ILI_DBG("%s compat_ioctl: convert core version\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_CORE_VER, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_DRV_VER:
		ILI_DBG("%s compat_ioctl: convert driver version\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_DRV_VER, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_CHIP_ID:
		ILI_DBG("%s compat_ioctl: convert chip id\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_CHIP_ID, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_MODE_CTRL:
		ILI_DBG("%s compat_ioctl: convert tp mode ctrl\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_MODE_CTRL, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_MODE_STATUS:
		ILI_DBG("%s compat_ioctl: convert tp mode status\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_MODE_STATUS, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_ICE_MODE_SWITCH:
		ILI_DBG("%s compat_ioctl: convert tp mode switch\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_ICE_MODE_SWITCH, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_INTERFACE_TYPE:
		ILI_DBG("%s compat_ioctl: convert interface type\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_INTERFACE_TYPE, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_DUMP_FLASH:
		ILI_DBG("%s compat_ioctl: convert dump flash\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_DUMP_FLASH, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_FW_UART_CTRL:
		ILI_DBG("%s compat_ioctl: convert fw uart\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_FW_UART_CTRL, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_PANEL_INFO:
		ILI_DBG("%s compat_ioctl: convert resolution\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_PANEL_INFO,
			(unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_TP_INFO:
		ILI_DBG("%s compat_ioctl: convert tp info\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_TP_INFO, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_WRAPPER_RW:
		ILI_DBG("%s compat_ioctl: convert wrapper\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_WRAPPER_RW, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_DDI_WRITE:
		ILI_DBG("%s compat_ioctl: convert ddi write\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_DDI_WRITE, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_DDI_READ:
		ILI_DBG("%s compat_ioctl: convert ddi read\n", __func__);
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_DDI_READ, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_OUTPUT_MODE_SET:
		ILI_DBG("compat_ioctl: output data mode\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_OUTPUT_MODE_SET, (unsigned long)compat_ptr(arg));
		return ret;
	case ILITEK_COMPAT_IOCTL_MP_TEST_ITEM_SET:
		ILI_DBG("compat_ioctl: set mp test item\n");
		ret = filp->f_op->unlocked_ioctl(filp, ILITEK_IOCTL_MP_TEST_ITEM_SET, (unsigned long)compat_ptr(arg));
		return ret;
	default:
		input_err(true, ilits->dev, "%s no ioctl cmd, return ilitek_node_ioctl\n", __func__);
		return -ENOIOCTLCMD;
	}
}
#endif

static long ilitek_node_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0, length = 0;
	u8 *szBuf = NULL, if_to_user = 0;
	static u16 i2c_rw_length;
	u32 id_to_user[64] = {0};
	bool esd_en = ilits->wq_esd_ctrl, bat_en = ilits->wq_bat_ctrl;
	u8 *wrap_rbuf = NULL, wrap_int = 0;
	u16 wrap_wlen = 0, wrap_rlen = 0;
	bool spi_irq, i2c_irq;

	if (_IOC_TYPE(cmd) != ILITEK_IOCTL_MAGIC) {
		input_err(true, ilits->dev, "%s The Magic number doesn't match\n", __func__);
		return -ENOTTY;
	}

	if (_IOC_NR(cmd) > ILITEK_IOCTL_MAXNR) {
		input_err(true, ilits->dev, "%s The number of ioctl doesn't match\n", __func__);
		return -ENOTTY;
	}

	ILI_DBG("%s cmd = %d\n", __func__, _IOC_NR(cmd));

	mutex_lock(&ilits->touch_mutex);

	szBuf = kcalloc(IOCTL_I2C_BUFF, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(szBuf)) {
		input_err(true, ilits->dev, "%s Failed to allocate mem\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, DISABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, DISABLE);

	switch (cmd) {
	case ILITEK_IOCTL_I2C_WRITE_DATA:
		ILI_DBG("%s ioctl: write len = %d\n", __func__, i2c_rw_length);

		if (i2c_rw_length > IOCTL_I2C_BUFF) {
			input_err(true, ilits->dev, "%s ERROR! write len is largn than ioctl buf (%d, %ld)\n",
				__func__, i2c_rw_length, IOCTL_I2C_BUFF);
			ret = -ENOTTY;
			break;
		}

		if (copy_from_user(szBuf, (u8 *) arg, i2c_rw_length)) {
			input_err(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			ret = -ENOTTY;
			break;
		}

		ret = ilits->wrapper(szBuf, i2c_rw_length, NULL, 0, OFF, OFF);
		if (ret < 0)
			input_err(true, ilits->dev, "%s Failed to write data\n", __func__);
		break;
	case ILITEK_IOCTL_I2C_READ_DATA:
		ILI_DBG("%s ioctl: read len = %d\n", __func__, i2c_rw_length);

		if (i2c_rw_length > IOCTL_I2C_BUFF) {
			input_err(true, ilits->dev, "%s ERROR! read len is largn than ioctl buf (%d, %ld)\n",
					__func__, i2c_rw_length, IOCTL_I2C_BUFF);
			ret = -ENOTTY;
			break;
		}

		ret = ilits->wrapper(NULL, 0, szBuf, i2c_rw_length, OFF, OFF);
		if (ret < 0) {
			input_err(true, ilits->dev, "%s Failed to read data\n", __func__);
			break;
		}

		if (copy_to_user((u8 *) arg, szBuf, i2c_rw_length)) {
			input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case ILITEK_IOCTL_I2C_SET_WRITE_LENGTH:
	case ILITEK_IOCTL_I2C_SET_READ_LENGTH:
		i2c_rw_length = arg;
		break;
	case ILITEK_IOCTL_TP_HW_RESET:
		ILI_DBG("%s ioctl: hw reset\n", __func__);
		ili_reset_ctrl(ilits->reset);
		break;
	case ILITEK_IOCTL_TP_POWER_SWITCH:
		input_info(true, ilits->dev, "%s Not implemented yet\n", __func__);
		break;
	case ILITEK_IOCTL_TP_REPORT_SWITCH:
		if (copy_from_user(szBuf, (u8 *) arg, 1)) {
			input_err(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			ret = -ENOTTY;
			break;
		}

		ILI_DBG("%s ioctl: report switch = %d\n", __func__, szBuf[0]);
		if (szBuf[0]) {
			ilits->report = ENABLE;
			ILI_DBG("%s report is enabled\n", __func__);
		} else {
			ilits->report = DISABLE;
			ILI_DBG("%s report is disabled\n", __func__);
		}
		break;
	case ILITEK_IOCTL_TP_IRQ_SWITCH:
		if (copy_from_user(szBuf, (u8 *) arg, 1)) {
			input_err(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			ret = -ENOTTY;
			break;
		}
		ILI_DBG("%s ioctl: irq switch = %d\n", __func__, szBuf[0]);
		if (szBuf[0])
			ili_irq_enable();
		else
			ili_irq_disable();
		break;
	case ILITEK_IOCTL_TP_DEBUG_LEVEL:
		if (copy_from_user(szBuf, (u8 *) arg, sizeof(u32))) {
			input_err(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			ret = -ENOTTY;
			break;
		}

		debug_en = !debug_en;
		ILI_DBG("%s debug_en = %d", __func__, debug_en);
		break;
	case ILITEK_IOCTL_TP_FUNC_MODE:
		if (copy_from_user(szBuf, (u8 *) arg, 3)) {
			input_err(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			ret = -ENOTTY;
			break;
		}

		ILI_DBG("%s ioctl: set func mode = %x,%x,%x\n",
			__func__, szBuf[0], szBuf[1], szBuf[2]);
		ret = ilits->wrapper(szBuf, 3, NULL, 0, ON, OFF);
		break;
	case ILITEK_IOCTL_TP_FW_VER:
		ILI_DBG("%s ioctl: get fw and mp version\n", __func__);
		szBuf[7] = ilits->chip->fw_mp_ver & 0xFF;
		szBuf[6] = (ilits->chip->fw_mp_ver >> 8) & 0xFF;
		szBuf[5] = (ilits->chip->fw_mp_ver >> 16) & 0xFF;
		szBuf[4] = ilits->chip->fw_mp_ver >> 24;
		szBuf[3] = ilits->chip->fw_ver & 0xFF;
		szBuf[2] = (ilits->chip->fw_ver >> 8) & 0xFF;
		szBuf[1] = (ilits->chip->fw_ver >> 16) & 0xFF;
		szBuf[0] = ilits->chip->fw_ver >> 24;
		ILI_DBG("%s Firmware version = %d.%d.%d.%d\n",
			__func__, szBuf[0], szBuf[1], szBuf[2], szBuf[3]);
		ILI_DBG("%s Firmware MP version = %d.%d.%d.%d\n",
			__func__, szBuf[4], szBuf[5], szBuf[6], szBuf[7]);

		if (copy_to_user((u8 *) arg, szBuf, 8)) {
			input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case ILITEK_IOCTL_TP_PL_VER:
		ILI_DBG("%s ioctl: get protocl version\n", __func__);
		szBuf[2] = ilits->protocol->ver & 0xFF;
		szBuf[1] = (ilits->protocol->ver >> 8) & 0xFF;
		szBuf[0] = ilits->protocol->ver >> 16;
		ILI_DBG("%s Protocol version = %d.%d.%d\n",
			__func__, szBuf[0], szBuf[1], szBuf[2]);

		if (copy_to_user((u8 *) arg, szBuf, 3)) {
			input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case ILITEK_IOCTL_TP_CORE_VER:
		ILI_DBG("%s ioctl: get core version\n", __func__);
		if (ilits->protocol->core_ver_len == P5_X_CORE_VER_THREE_LENGTH)
			szBuf[3] = 0xFF;
		else
			szBuf[3] = ilits->chip->core_ver & 0xFF;
		szBuf[2] = (ilits->chip->core_ver >> 8) & 0xFF;
		szBuf[1] = (ilits->chip->core_ver >> 16) & 0xFF;
		szBuf[0] = ilits->chip->core_ver >> 24;
		ILI_DBG("%s Core version = %d.%d.%d.%d, length = %d\n",
			__func__, szBuf[0], szBuf[1], szBuf[2], szBuf[3], ilits->protocol->core_ver_len);

		if (copy_to_user((u8 *) arg, szBuf, 4)) {
			input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case ILITEK_IOCTL_TP_DRV_VER:
		ILI_DBG("%s ioctl: get driver version\n", __func__);
		length = snprintf(szBuf, USER_STR_BUFF * sizeof(unsigned char), "%s", DRIVER_VERSION);

		if (copy_to_user((u8 *) arg, szBuf, length)) {
			input_err(true, ilits->dev, "%s Failed to copy driver ver to user space\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case ILITEK_IOCTL_TP_CHIP_ID:
		ILI_DBG("%s ioctl: get chip id\n", __func__);
		id_to_user[0] = ilits->chip->pid;
		id_to_user[1] = ilits->chip->otp_id;
		id_to_user[2] = ilits->chip->ana_id;

		if (copy_to_user((u32 *) arg, id_to_user, sizeof(u32) * 3)) {
			input_err(true, ilits->dev, "%s Failed to copy driver ver to user space\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case ILITEK_IOCTL_TP_MODE_CTRL:
		if (copy_from_user(szBuf, (u8 *) arg, 10)) {
			input_err(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			ret = -ENOTTY;
			break;
		}
		ILI_DBG("%s ioctl: switch fw format = %d\n", __func__, szBuf[0]);
		ret = ili_tp_data_mode_ctrl(szBuf);

		break;
	case ILITEK_IOCTL_TP_MODE_STATUS:
		ILI_DBG("%s ioctl: current firmware mode = %d", __func__, ilits->tp_data_mode);
		if (copy_to_user((int *)arg, &ilits->tp_data_mode, sizeof(int))) {
			input_err(true, ilits->dev, "%s Failed to copy driver ver to user space\n", __func__);
			ret = -ENOTTY;
		}
		break;
	/* It works for host downloado only */
	case ILITEK_IOCTL_ICE_MODE_SWITCH:
		if (copy_from_user(szBuf, (u8 *) arg, 1)) {
			input_err(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			ret = -ENOTTY;
			break;
		}

		ILI_DBG("%s ioctl: switch ice mode = %d, mcu on = %d\n",
			__func__, szBuf[0], szBuf[1]);

		if (ili_ice_mode_ctrl(szBuf[0] ? ENABLE : DISABLE,
			szBuf[1] ? ENABLE : DISABLE) < 0)
			input_err(true, ilits->dev, "%s ioctl: set ice mode failed\n", __func__);

		break;
	case ILITEK_IOCTL_TP_INTERFACE_TYPE:
		if_to_user = ilits->hwif->bus_type;
		if (copy_to_user((u8 *) arg, &if_to_user, sizeof(if_to_user))) {
			input_err(true, ilits->dev, "%s Failed to copy driver ver to user space\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case ILITEK_IOCTL_TP_DUMP_FLASH:
		ILI_DBG("%s ioctl: dump flash data\n", __func__);
		ret = ili_fw_dump_flash_data(0, 0, true);
		if (ret < 0)
			input_err(true, ilits->dev, "%s ioctl: Failed to dump flash data\n", __func__);
		break;
	case ILITEK_IOCTL_TP_FW_UART_CTRL:
		if (copy_from_user(szBuf, (u8 *) arg, 1)) {
			input_err(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			ret = -ENOTTY;
			break;
		}
		ILI_DBG("%s ioctl: fw UART  = %d\n", __func__, szBuf[0]);

		ili_fw_uart_ctrl(szBuf[0]);

		if_to_user = szBuf[0];

		if (copy_to_user((u8 *) arg, &if_to_user, sizeof(if_to_user))) {
			input_err(true, ilits->dev, "%s Failed to copy driver ver to user space\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case ILITEK_IOCTL_TP_PANEL_INFO:
		ILI_DBG("%s ioctl: get panel resolution\n", __func__);
		id_to_user[0] = ilits->panel_wid;
		id_to_user[1] = ilits->panel_hei;

		if (copy_to_user((u32 *) arg, id_to_user, sizeof(u32) * 2)) {
			input_err(true, ilits->dev, "%s Failed to copy driver ver to user space\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case ILITEK_IOCTL_TP_INFO:
		ILI_DBG("%s ioctl: get tp info\n", __func__);
		id_to_user[0] = ilits->min_x;
		id_to_user[1] = ilits->min_y;
		id_to_user[2] = ilits->max_x;
		id_to_user[3] = ilits->max_y;
		id_to_user[4] = ilits->xch_num;
		id_to_user[5] = ilits->ych_num;
		id_to_user[6] = ilits->stx;
		id_to_user[7] = ilits->srx;

		if (copy_to_user((u32 *) arg, id_to_user, sizeof(u32) * 8)) {
			input_err(true, ilits->dev, "%s Failed to copy driver ver to user space\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case ILITEK_IOCTL_WRAPPER_RW:
		ILI_DBG("%s ioctl: wrapper rw\n", __func__);

		if (i2c_rw_length > IOCTL_I2C_BUFF || i2c_rw_length == 0) {
			input_err(true, ilits->dev, "%s ERROR! i2c_rw_length is is invalid\n", __func__);
			ret = -ENOTTY;
			break;
		}

		if (copy_from_user(szBuf, (u8 *) arg, i2c_rw_length)) {
			input_err(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			ret = -ENOTTY;
			break;
		}

		wrap_int = szBuf[0];
		wrap_rlen = (szBuf[1] << 8) | szBuf[2];
		wrap_wlen = (szBuf[3] << 8) | szBuf[4];

		ILI_DBG("%s wrap_int = %d\n", __func__, wrap_int);
		ILI_DBG("%s wrap_rlen = %d\n", __func__, wrap_rlen);
		ILI_DBG("%s wrap_wlen = %d\n", __func__, wrap_wlen);

		if (wrap_wlen > IOCTL_I2C_BUFF || wrap_rlen > IOCTL_I2C_BUFF) {
			input_err(true, ilits->dev, "%s ERROR! R/W len is largn than ioctl buf\n", __func__);
			ret = -ENOTTY;
			break;
		}

		if (wrap_rlen > 0) {
			wrap_rbuf = kcalloc(IOCTL_I2C_BUFF, sizeof(u8), GFP_KERNEL);
			if (ERR_ALLOC_MEM(wrap_rbuf)) {
				input_err(true, ilits->dev, "%s Failed to allocate mem\n", __func__);
				ret = -ENOMEM;
				break;
			}
		}

		if (wrap_int == 1) {
			i2c_irq = ON;
			spi_irq = ON;
		} else if (wrap_int == 2) {
			i2c_irq = OFF;
			spi_irq = OFF;
		} else {
			i2c_irq = OFF;
			spi_irq = (wrap_rlen > 0 ? ON : OFF);
		}

		ILI_DBG("%s i2c_irq = %d, spi_irq = %d\n", __func__, i2c_irq, spi_irq);

		ilits->wrapper(szBuf + 5, wrap_wlen, wrap_rbuf, wrap_rlen, spi_irq, i2c_irq);

		ili_dump_data(szBuf + 5, 8, wrap_wlen, 16, "wrap_wbuf:");
		ili_dump_data(wrap_rbuf, 8, wrap_rlen, 16, "wrap_rbuf:");
		if (copy_to_user((u8 *) arg, wrap_rbuf, wrap_rlen)) {
			input_err(true, ilits->dev, "%s Failed to copy driver ver to user space\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case ILITEK_IOCTL_DDI_WRITE:
		if (copy_from_user(szBuf, (u8 *) arg, 3)) {
			input_err(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			ret = -ENOTTY;
			break;
		}
		ILI_DBG("%s ioctl: page = %x, reg = %x, data = %x\n",
			__func__, szBuf[0], szBuf[1], szBuf[2]);
		ili_ic_set_ddi_reg_onepage(szBuf[0], szBuf[1], szBuf[2]);
		break;
	case ILITEK_IOCTL_DDI_READ:
		if (copy_from_user(szBuf, (u8 *) arg, 2)) {
			input_err(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			ret = -ENOTTY;
			break;
		}
		ILI_DBG("%s ioctl: page = %x, reg = %x\n", __func__, szBuf[0], szBuf[1]);
		ili_ic_get_ddi_reg_onepage(szBuf[0], szBuf[1], &szBuf[2]);
		ILI_DBG("%s ioctl: data = %x\n", __func__, szBuf[2]);
		if (copy_to_user((u8 *) arg, szBuf + 2, 1)) {
			input_err(true, ilits->dev, "%s Failed to copy data to user space\n", __func__);
			ret = -ENOTTY;
		}
		break;
	case ILITEK_IOCTL_OUTPUT_MODE_SET:
		if (copy_from_user(szBuf, (u8 *) arg, 1)) {
			input_err(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			ret = -ENOTTY;
			break;
		}
		ilits->output_data_mode = szBuf[0];
		input_info(true, ilits->dev, "%s ioctl: set outout mode = %d\n", __func__, ilits->output_data_mode);
		break;
	case ILITEK_IOCTL_MP_TEST_ITEM_SET:
		if (copy_from_user(szBuf, (u8 *) arg, 1)) {
			input_err(true, ilits->dev, "%s Failed to copy data from user space\n", __func__);
			ret = -ENOTTY;
			break;
		}
		ilits->mp_test_item = szBuf[0];
		input_info(true, ilits->dev, "%s ioctl: set mp test item = %d\n", __func__, ilits->mp_test_item);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	ipio_kfree((void **)&szBuf);
	ipio_kfree((void **)&wrap_rbuf);

out:
	if (esd_en)
		ili_wq_ctrl(WQ_ESD, ENABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, ENABLE);

	mutex_unlock(&ilits->touch_mutex);
	return ret;
}

static struct proc_dir_entry *proc_dir_ilitek;

static int ilitek_proc_seq_output_data_read(struct inode *inode, struct file *file)
{
	return single_open_size(file, ilitek_proc_output_data_show, NULL, 16 * K);
}


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))

typedef struct {
	char *name;
	struct proc_dir_entry *node;
	const struct proc_ops *fops;
	bool isCreated;
} proc_node;


static struct proc_ops proc_output_data_fops = {
	.proc_open = ilitek_proc_seq_output_data_read,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static struct proc_ops proc_mp_lcm_on_test_fops = {
	.proc_read = ilitek_node_mp_lcm_on_test_read,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_mp_lcm_off_test_fops = {
	.proc_read = ilitek_node_mp_lcm_off_test_read,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_ver_info_fops = {
	.proc_read = ilitek_node_ver_info_read,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_change_list_fops = {
	.proc_read = ilitek_node_change_list_read,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_debug_message_fops = {
	.proc_read = ilitek_proc_debug_message_read,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_debug_message_switch_fops = {
	.proc_read = ilitek_proc_debug_switch_read,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_ioctl_fops = {
	.proc_ioctl = ilitek_node_ioctl,
#ifdef CONFIG_COMPAT
	.proc_compat_ioctl = ilitek_node_compat_ioctl,
#endif
	.proc_write = ilitek_node_ioctl_write,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_fw_upgrade_fops = {
	.proc_read = ilitek_node_fw_upgrade_read,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_fw_process_fops = {
	.proc_read = ilitek_proc_fw_process_read,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_get_delta_data_fops = {
	.proc_read = ilitek_proc_get_delta_data_read,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_get_raw_data_fops = {
	.proc_read = ilitek_proc_fw_get_raw_data_read,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_rw_tp_reg_fops = {
	.proc_read = ilitek_proc_rw_tp_reg_read,
	.proc_write = ilitek_proc_rw_tp_reg_write,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_fw_pc_counter_fops = {
	.proc_read = ilitek_proc_fw_pc_counter_read,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_debug_level_fops = {
	.proc_read = ilitek_proc_debug_level_read,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_get_customer_info_fops = {
	.proc_read = ilitek_proc_get_customer_info,
	.proc_lseek = default_llseek,
};

static struct proc_ops proc_sram_test_fops = {
	.proc_read = ilitek_proc_sram_test_info,
	.proc_lseek = default_llseek,
};


#else

typedef struct {
	char *name;
	struct proc_dir_entry *node;
	const struct file_operations *fops;
	bool isCreated;
} proc_node;


static struct file_operations proc_output_data_fops = {
	.open = ilitek_proc_seq_output_data_read,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct file_operations proc_mp_lcm_on_test_fops = {
	.read = ilitek_node_mp_lcm_on_test_read,
	.llseek = default_llseek,
};

static struct file_operations proc_mp_lcm_off_test_fops = {
	.read = ilitek_node_mp_lcm_off_test_read,
	.llseek = default_llseek,
};

static struct file_operations proc_ver_info_fops = {
	.read = ilitek_node_ver_info_read,
	.llseek = default_llseek,
};

static struct file_operations proc_change_list_fops = {
	.read = ilitek_node_change_list_read,
	.llseek = default_llseek,
};

static struct file_operations proc_debug_message_fops = {
	.read = ilitek_proc_debug_message_read,
	.llseek = default_llseek,
};

static struct file_operations proc_debug_message_switch_fops = {
	.read = ilitek_proc_debug_switch_read,
	.llseek = default_llseek,
};

static struct file_operations proc_ioctl_fops = {
	.unlocked_ioctl = ilitek_node_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ilitek_node_compat_ioctl,
#endif
	.write = ilitek_node_ioctl_write,
	.llseek = default_llseek,
};

static struct file_operations proc_fw_upgrade_fops = {
	.read = ilitek_node_fw_upgrade_read,
	.llseek = default_llseek,
};

static struct file_operations proc_fw_process_fops = {
	.read = ilitek_proc_fw_process_read,
	.llseek = default_llseek,
};

static struct file_operations proc_get_delta_data_fops = {
	.read = ilitek_proc_get_delta_data_read,
	.llseek = default_llseek,
};

static struct file_operations proc_get_raw_data_fops = {
	.read = ilitek_proc_fw_get_raw_data_read,
	.llseek = default_llseek,
};

static struct file_operations proc_rw_tp_reg_fops = {
	.read = ilitek_proc_rw_tp_reg_read,
	.write = ilitek_proc_rw_tp_reg_write,
	.llseek = default_llseek,
};

static struct file_operations proc_fw_pc_counter_fops = {
	.read = ilitek_proc_fw_pc_counter_read,
	.llseek = default_llseek,
};

static struct file_operations proc_debug_level_fops = {
	.read = ilitek_proc_debug_level_read,
	.llseek = default_llseek,
};

static struct file_operations proc_get_customer_info_fops = {
	.read = ilitek_proc_get_customer_info,
	.llseek = default_llseek,
};

static struct file_operations proc_sram_test_fops = {
	.read = ilitek_proc_sram_test_info,
	.llseek = default_llseek,
};
#endif

proc_node iliproc[] = {
	{"ioctl", NULL, &proc_ioctl_fops, false},
	{"fw_process", NULL, &proc_fw_process_fops, false},
	{"fw_upgrade", NULL, &proc_fw_upgrade_fops, false},
	{"debug_level", NULL, &proc_debug_level_fops, false},
	{"customer_info", NULL, &proc_get_customer_info_fops, false},
	{"mp_lcm_on_test", NULL, &proc_mp_lcm_on_test_fops, false},
	{"mp_lcm_off_test", NULL, &proc_mp_lcm_off_test_fops, false},
	{"debug_message", NULL, &proc_debug_message_fops, false},
	{"debug_message_switch", NULL, &proc_debug_message_switch_fops, false},
	{"fw_pc_counter", NULL, &proc_fw_pc_counter_fops, false},
	{"show_delta_data", NULL, &proc_get_delta_data_fops, false},
	{"show_raw_data", NULL, &proc_get_raw_data_fops, false},
	{"rw_tp_reg", NULL, &proc_rw_tp_reg_fops, false},
	{"ver_info", NULL, &proc_ver_info_fops, false},
	{"change_list", NULL, &proc_change_list_fops, false},
	{"sram_test", NULL, &proc_sram_test_fops, false},
	{"output_data", NULL, &proc_output_data_fops, false},
};

void ili_node_init(void)
{
	int i = 0, ret = 0;

	proc_dir_ilitek = proc_mkdir("ilitek", NULL);

	for (; i < ARRAY_SIZE(iliproc); i++) {
		iliproc[i].node = proc_create(iliproc[i].name, 0644, proc_dir_ilitek, iliproc[i].fops);

		if (iliproc[i].node == NULL) {
			iliproc[i].isCreated = false;
			input_err(true, ilits->dev, "%s Failed to create %s under /proc\n", __func__, iliproc[i].name);
			ret = -ENODEV;
		} else {
			iliproc[i].isCreated = true;
			input_info(true, ilits->dev, "%s Succeed to create %s under /proc\n",
					__func__, iliproc[i].name);
		}
	}
}
