/*
 * Copyright (C) 2010 - 2022 Novatek, Inc.
 *
 * $Revision: 107378 $
 * $Date: 2022-10-26 09:56:01 +0800 (週三, 26 十月 2022) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */


#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "nt36xxx.h"

#if NVT_TOUCH_EXT_PROC
#define NVT_FW_VERSION "nvt_fw_version"
#define NVT_BASELINE "nvt_baseline"
#define NVT_RAW "nvt_raw"
#define NVT_DIFF "nvt_diff"
#define NVT_PEN_DIFF "nvt_pen_diff"
#if NVT_CUST_PROC_CMD
#define PENEL_DIRECTION "panel_direction"
#define GAME_MODE "game_mode"
#define EDGE_GRID_ZONE "edge_grid_zone"
#define SUPPORT_PEN "support_pen"
/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 start */
#if NVT_DPR_SWITCH
#define FW_PEN_STATE "fw_pen_state"
#endif
#define NVT_FW_STATUS "nvt_fw_status"
/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 end */
#define GAME_MODE_PALM_CMD 0x79
#define SUPPORT_PEN_CMD 0x7B
#define NVT_EXT_CMD 0x7F

#define GAME_MODE_NORMAL 0x00
#define GAME_MODE_REMOVE 0x01
#define GAME_MODE_ENHANCE  0x02
#define NVT_EXT_CMD_EDGE_GRID_ZONE 0x01

#define EDGE_REJECT_VERTICLE_CMD 0xBA
#define EDGE_REJECT_LEFT_UP 0xBB
#define EDGE_REJECT_RIGHT_UP 0xBC
#define EDGE_REJECT_VERTICLE_REVERSE_CMD 0xBD
#endif

/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 start */
#define GESTURE_CONTROL "gesture_control"
/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 end */
#define NORMAL_MODE 0x00
#define TEST_MODE_2 0x22
#define HANDSHAKING_HOST_READY 0xBB

#define XDATA_SECTOR_SIZE   256

static uint8_t xdata_tmp[8192] = {0};
static int32_t xdata[4096] = {0};
static int32_t xdata_pen_tip_x[256] = {0};
static int32_t xdata_pen_tip_y[256] = {0};
static int32_t xdata_pen_ring_x[256] = {0};
static int32_t xdata_pen_ring_y[256] = {0};

static struct proc_dir_entry *NVT_proc_fw_version_entry;
static struct proc_dir_entry *NVT_proc_baseline_entry;
static struct proc_dir_entry *NVT_proc_raw_entry;
static struct proc_dir_entry *NVT_proc_diff_entry;
static struct proc_dir_entry *NVT_proc_pen_diff_entry;
#if NVT_CUST_PROC_CMD
static struct proc_dir_entry *NVT_proc_edge_reject_entry;
static struct proc_dir_entry *NVT_proc_edge_grid_zone_entry;
static struct proc_dir_entry *NVT_proc_game_mode_entry;
static struct proc_dir_entry *NVT_proc_support_pen_entry;
/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 start */
#if NVT_DPR_SWITCH
static struct proc_dir_entry *NVT_proc_get_fw_pen_state_entry;
#endif
/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 end */
#endif
static struct proc_dir_entry *NVT_proc_fw_status_entry;

/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 start */
static struct proc_dir_entry *NVT_gesture_entry;
/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 end */

/* Spinel code for OSPINEL-3535 by zhangyd22 at 2023/06/05 start */
extern uint8_t bTouchIsAwake;
/* Spinel code for OSPINEL-3535 by zhangyd22 at 2023/06/05 end */
/*******************************************************
Description:
	Novatek touchscreen change mode function.

return:
	n.a.
*******************************************************/
void nvt_change_mode(uint8_t mode)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = mode;
	CTP_SPI_WRITE(ts->client, buf, 2);

	if (mode == NORMAL_MODE) {
		usleep_range(20000, 20000);
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = HANDSHAKING_HOST_READY;
		CTP_SPI_WRITE(ts->client, buf, 2);
		usleep_range(20000, 20000);
	}
}

int32_t nvt_set_pen_inband_mode_1(uint8_t freq_idx, uint8_t x_term)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 5;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0xC1;
	buf[2] = 0x02;
	buf[3] = freq_idx;
	buf[4] = x_term;
	CTP_SPI_WRITE(ts->client, buf, 5);

	for (i = 0; i < retry; i++) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

int32_t nvt_set_pen_normal_mode(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 5;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0xC1;
	buf[2] = 0x04;
	CTP_SPI_WRITE(ts->client, buf, 3);

	for (i = 0; i < retry; i++) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen get firmware pipe function.

return:
	Executive outcomes. 0---pipe 0. 1---pipe 1.
*******************************************************/
uint8_t nvt_get_fw_pipe(void)
{
	uint8_t buf[8]= {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

	//---read fw status---
	buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
	buf[1] = 0x00;
	CTP_SPI_READ(ts->client, buf, 2);

	//NVT_LOG("FW pipe=%d, buf[1]=0x%02X\n", (buf[1]&0x01), buf[1]);

	return (buf[1] & 0x01);
}

/*******************************************************
Description:
	Novatek touchscreen read meta data function.

return:
	n.a.
*******************************************************/
void nvt_read_mdata(uint32_t xdata_addr, uint32_t xdata_btn_addr)
{
	int32_t transfer_len = 0;
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 2] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	if (BUS_TRANSFER_LENGTH <= XDATA_SECTOR_SIZE)
		transfer_len = BUS_TRANSFER_LENGTH;
	else
		transfer_len = XDATA_SECTOR_SIZE;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = ts->x_num * ts->y_num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//pr_info("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n", head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		nvt_set_page(head_addr + XDATA_SECTOR_SIZE * i);

		//---read xdata by transfer_len
		for (j = 0; j < (XDATA_SECTOR_SIZE / transfer_len); j++) {
			//---read data---
			buf[0] = transfer_len * j;
			CTP_SPI_READ(ts->client, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + transfer_len * j + k] = buf[k + 1];
				//pr_info("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + transfer_len*j + k));
			}
		}
		//pr_info("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_set_page(xdata_addr + data_len - residual_len);

		//---read xdata by transfer_len
		for (j = 0; j < (residual_len / transfer_len + 1); j++) {
			//---read data---
			buf[0] = transfer_len * j;
			CTP_SPI_READ(ts->client, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + transfer_len * j + k] = buf[k + 1];
				//pr_info("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + transfer_len*j + k));
			}
		}
		//pr_info("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++) {
		xdata[i] = (int16_t)(xdata_tmp[dummy_len + i * 2] + 256 * xdata_tmp[dummy_len + i * 2 + 1]);
	}

#if TOUCH_KEY_NUM > 0
	//read button xdata : step3
	//---change xdata index---
	nvt_set_page(xdata_btn_addr);

	//---read data---
	buf[0] = (xdata_btn_addr & 0xFF);
	CTP_SPI_READ(ts->client, buf, (TOUCH_KEY_NUM * 2 + 1));

	//---2bytes-to-1data---
	for (i = 0; i < TOUCH_KEY_NUM; i++) {
		xdata[ts->x_num * ts->y_num + i] = (int16_t)(buf[1 + i * 2] + 256 * buf[1 + i * 2 + 1]);
	}
#endif

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}

/*******************************************************
Description:
    Novatek touchscreen get meta data function.

return:
    n.a.
*******************************************************/
void nvt_get_mdata(int32_t *buf, uint8_t *m_x_num, uint8_t *m_y_num)
{
    *m_x_num = ts->x_num;
    *m_y_num = ts->y_num;
    memcpy(buf, xdata, ((ts->x_num * ts->y_num + TOUCH_KEY_NUM) * sizeof(int32_t)));
}

/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 start */
static ssize_t gesture_control_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	char input;
	if (count > 0) {
		if (get_user(input, buffer))
			return -EFAULT;
		if (input != '1')
			nvt_gesture_flag = false;
		else
			nvt_gesture_flag = true;
	}
	return count;
}
static int gesture_control_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", nvt_gesture_flag);
	return 0;
}
/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 end */
/*******************************************************
Description:
	Novatek touchscreen read and get number of meta data function.

return:
	n.a.
*******************************************************/
void nvt_read_get_num_mdata(uint32_t xdata_addr, int32_t *buffer, uint32_t num)
{
	int32_t transfer_len = 0;
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 2] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	if (BUS_TRANSFER_LENGTH <= XDATA_SECTOR_SIZE)
		transfer_len = BUS_TRANSFER_LENGTH;
	else
		transfer_len = XDATA_SECTOR_SIZE;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//pr_info("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n", head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		nvt_set_page(head_addr + XDATA_SECTOR_SIZE * i);

		//---read xdata by transfer_len
		for (j = 0; j < (XDATA_SECTOR_SIZE / transfer_len); j++) {
			//---read data---
			buf[0] = transfer_len * j;
			CTP_SPI_READ(ts->client, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + transfer_len * j + k] = buf[k + 1];
				//pr_info("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + transfer_len*j + k));
			}
		}
		//pr_info("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_set_page(xdata_addr + data_len - residual_len);

		//---read xdata by transfer_len
		for (j = 0; j < (residual_len / transfer_len + 1); j++) {
			//---read data---
			buf[0] = transfer_len * j;
			CTP_SPI_READ(ts->client, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + transfer_len * j + k] = buf[k + 1];
				//pr_info("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + transfer_len*j + k));
			}
		}
		//pr_info("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++) {
		buffer[i] = (int16_t)(xdata_tmp[dummy_len + i * 2] + 256 * xdata_tmp[dummy_len + i * 2 + 1]);
	}

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}

/*******************************************************
Description:
	Novatek touchscreen firmware version show function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_fw_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "fw_ver=%d, x_num=%d, y_num=%d, button_num=%d\n", ts->fw_ver, ts->x_num, ts->y_num, ts->max_button_num);
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_show(struct seq_file *m, void *v)
{
	int32_t i = 0;
	int32_t j = 0;

	for (i = 0; i < ts->y_num; i++) {
		for (j = 0; j < ts->x_num; j++) {
			seq_printf(m, "%5d, ", xdata[i * ts->x_num + j]);
		}
		seq_puts(m, "\n");
	}

#if TOUCH_KEY_NUM > 0
	for (i = 0; i < TOUCH_KEY_NUM; i++) {
		seq_printf(m, "%5d, ", xdata[ts->x_num * ts->y_num + i]);
	}
	seq_puts(m, "\n");
#endif

	seq_printf(m, "\n\n");
	return 0;
}

/*******************************************************
Description:
	Novatek pen 1D diff xdata sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_pen_1d_diff_show(struct seq_file *m, void *v)
{
	int32_t i = 0;

	seq_printf(m, "Tip X:\n");
	for (i = 0; i < ts->x_num; i++) {
		seq_printf(m, "%5d, ", xdata_pen_tip_x[i]);
	}
	seq_puts(m, "\n");
	seq_printf(m, "Tip Y:\n");
	for (i = 0; i < ts->y_num; i++) {
		seq_printf(m, "%5d, ", xdata_pen_tip_y[i]);
	}
	seq_puts(m, "\n");
	seq_printf(m, "Ring X:\n");
	for (i = 0; i < ts->x_num; i++) {
		seq_printf(m, "%5d, ", xdata_pen_ring_x[i]);
	}
	seq_puts(m, "\n");
	seq_printf(m, "Ring Y:\n");
	for (i = 0; i < ts->y_num; i++) {
		seq_printf(m, "%5d, ", xdata_pen_ring_y[i]);
	}
	seq_puts(m, "\n");

	seq_printf(m, "\n\n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print start
	function.

return:
	Executive outcomes. 1---call next function.
	NULL---not call next function and sequence loop
	stop.
*******************************************************/
static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print next
	function.

return:
	Executive outcomes. NULL---no next and call sequence
	stop function.
*******************************************************/
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print stop
	function.

return:
	n.a.
*******************************************************/
static void c_stop(struct seq_file *m, void *v)
{
	return;
}

const struct seq_operations nvt_fw_version_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_fw_version_show
};

const struct seq_operations nvt_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show
};

const struct seq_operations nvt_pen_diff_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_pen_1d_diff_show
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_fw_version open
	function.

return:
	n.a.
*******************************************************/
static int32_t nvt_fw_version_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_fw_version_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_fw_version_fops = {
	.proc_open = nvt_fw_version_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_fw_version_fops = {
	.owner = THIS_MODULE,
	.open = nvt_fw_version_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_baseline open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_baseline_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_read_mdata(ts->mmap->BASELINE_ADDR, ts->mmap->BASELINE_BTN_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_baseline_fops = {
	.proc_open = nvt_baseline_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_baseline_fops = {
	.owner = THIS_MODULE,
	.open = nvt_baseline_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_raw open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_raw_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR, ts->mmap->RAW_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR, ts->mmap->RAW_BTN_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_raw_fops = {
	.proc_open = nvt_raw_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_raw_fops = {
	.owner = THIS_MODULE,
	.open = nvt_raw_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_diff open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_diff_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_diff_fops = {
	.proc_open = nvt_diff_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_diff_fops = {
	.owner = THIS_MODULE,
	.open = nvt_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif
/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 start */
static int32_t gesture_control_open(struct inode *inode, struct file *file)
{
	return single_open(file, gesture_control_show, NULL);
}
#ifdef HAVE_PROC_OPS
static const struct proc_ops gesture_control_fops = {
	.proc_open = gesture_control_open,
	.proc_read = seq_read,
	.proc_write = gesture_control_write,
};
#else
static const struct file_operations gesture_control_fops = {
	.owner = THIS_MODULE,
	.open = gesture_control_open,
	.read = seq_read,
	.write = gesture_control_write,
};
#endif
/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 end */

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_pen_diff open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_pen_diff_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_set_pen_inband_mode_1(0xFF, 0x00)) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_read_get_num_mdata(ts->mmap->PEN_1D_DIFF_TIP_X_ADDR, xdata_pen_tip_x, ts->x_num);
	nvt_read_get_num_mdata(ts->mmap->PEN_1D_DIFF_TIP_Y_ADDR, xdata_pen_tip_y, ts->y_num);
	nvt_read_get_num_mdata(ts->mmap->PEN_1D_DIFF_RING_X_ADDR, xdata_pen_ring_x, ts->x_num);
	nvt_read_get_num_mdata(ts->mmap->PEN_1D_DIFF_RING_Y_ADDR, xdata_pen_ring_y, ts->y_num);

	nvt_change_mode(NORMAL_MODE);

	nvt_set_pen_normal_mode();

	nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_pen_diff_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_pen_diff_fops = {
	.proc_open = nvt_pen_diff_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_pen_diff_fops = {
	.owner = THIS_MODULE,
	.open = nvt_pen_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif
/* Spinel code for OSPINEL-3913 by zhangyd22 at 2023/06/07 start */
/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_fw_status open
	function.

return:
	n.a.
*******************************************************/
static int32_t nvt_fw_status_show(struct seq_file *m, void *v)
{
	uint8_t buf[8] = {0};

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_RESET_COMPLETE);

	//---read reset state---
	buf[0] = EVENT_MAP_RESET_COMPLETE;
	buf[1] = 0x00;
	CTP_SPI_READ(ts->client, buf, 6);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	seq_printf(m, "%02X\n", buf[1]);

	return 0;
}
static int32_t nvt_fw_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvt_fw_status_show, NULL);
}
#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_fw_status_fops = {
	.proc_open = nvt_fw_status_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_fw_status_fops = {
	.owner = THIS_MODULE,
	.open = nvt_fw_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif
/* Spinel code for OSPINEL-3913 by zhangyd22 at 2023/06/07 end */
#if NVT_CUST_PROC_CMD
int32_t nvt_cmd_store(uint8_t u8Cmd){
	int i, retry = 5;
	uint8_t buf[3] = {0};
	int32_t ret = 0;

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}
/* Spinel code for OSPINEL-3535 by zhangyd22 at 2023/06/05 start */
	if(bTouchIsAwake == 0){
		NVT_ERR("Touch is already suspend, ignore 0x%02x cmd", u8Cmd);
		mutex_unlock(&ts->lock);
		return -1;
	}
/* Spinel code for OSPINEL-3535 by zhangyd22 at 2023/06/05 end */
	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		mutex_unlock(&ts->lock);
		return ret;
	}

	for (i = 0; i < retry; i++) {
		//---set cmd status---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = u8Cmd;
		CTP_SPI_WRITE(ts->client, buf, 2);
		msleep(20);
		//---read cmd status---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);
		if (buf[1] == 0x00)
			break;
	}

	if (unlikely(i == retry)) {
		NVT_LOG("send Cmd 0x%02X failed, buf[1]=0x%02X\n", u8Cmd, buf[1]);
		ret = -1;
	} else {
		NVT_LOG("send Cmd 0x%02X success, tried %d times\n", u8Cmd, i);
	}

	mutex_unlock(&ts->lock);

	return ret;
}
int32_t nvt_ext_cmd_store(uint8_t u8Cmd, uint8_t u8subCmd){
	int i, retry = 5;
	uint8_t buf[4] = {0};
	int32_t ret = 0;

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

/* Spinel code for OSPINEL-3535 by zhangyd22 at 2023/06/05 start */
	if(bTouchIsAwake == 0){
		NVT_ERR("Touch is already suspend, ignore 0x%02x 0x%02x cmd", u8Cmd, u8subCmd);
		mutex_unlock(&ts->lock);
		return -1;
	}
/* Spinel code for OSPINEL-3535 by zhangyd22 at 2023/06/05 end */

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		mutex_unlock(&ts->lock);
		return ret;
	}

	for (i = 0; i < retry; i++) {
		//---set cmd status---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = u8Cmd;
		buf[2] = u8subCmd;
		CTP_SPI_WRITE(ts->client, buf, 3);

		msleep(20);

		//---read cmd status---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);
		if (buf[1] == 0x00)
			break;
	}

	if (unlikely(i == retry)) {
		NVT_LOG("send Cmd 0x%02X 0x%02X failed, buf[1]=0x%02X\n", u8Cmd, u8subCmd, buf[1]);
		ret = -1;
	} else {
		NVT_LOG("send Cmd 0x%02X 0x%02X success, tried %d times\n", u8Cmd, u8subCmd, i);
	}

	mutex_unlock(&ts->lock);

	return ret;
}

/*
 *	/proc/panel_angle cmd_param
 *      [0], 0 : panel rotate 0 degree
 *			 1 : panel rotate 90 degree
 *			 2 : panel rotate 180 degree
 *			 3 : panel rotate 270 degree
 *
 */
int32_t nvt_edge_reject_set(int32_t status) {
	int ret = 0;

	if(status == 0)//rotate 0 degree
		ret = nvt_cmd_store(EDGE_REJECT_VERTICLE_CMD);
	else if(status == 1) //rotate 270 degree
		ret = nvt_cmd_store(EDGE_REJECT_LEFT_UP);
	else if(status == 2) //rotate 180 degree
		ret = nvt_cmd_store(EDGE_REJECT_VERTICLE_REVERSE_CMD);
	else if(status == 3) //rotate 90 degree
		ret = nvt_cmd_store(EDGE_REJECT_RIGHT_UP);

	return ret;
}

static ssize_t nvt_edge_reject_store(struct file *file, const char *buffer, size_t count, loff_t *pos) {
	char dbg[10] = { 0 };
	int res = 0;
	uint8_t state;

	res = copy_from_user(dbg, (uint8_t *) buffer, sizeof(uint8_t));
	if (res)
		return -EINVAL;

	res = kstrtou8(dbg, 16, &state);
	if (res < 0)
		return res;

	ts->edge_reject_state = state;

	nvt_edge_reject_set(ts->edge_reject_state);

	return count;
}

static int nvt_edge_reject_show(struct seq_file *sfile, void *v) {

	if(ts->edge_reject_state == 0)
		seq_printf(sfile, "Vertical Direction!(0 degree)\n"); //rotate 0 degree
	else if(ts->edge_reject_state == 1)
		seq_printf(sfile, "Right Up Direction!(270 degree)\n"); //rotate 270 degree
	else if(ts->edge_reject_state == 2)
		seq_printf(sfile, "Vertical reverse Direction!(180 degree)\n"); //rotate 180 degree
	else if(ts->edge_reject_state == 3)
		seq_printf(sfile, "Left Up Direction!(90 degree)\n"); //rotate 90 degree
	else

		seq_printf(sfile, "Not Support!\n");

	return 0;
}

static int32_t nvt_edge_reject_open(struct inode *inode, struct file *file) {
	return single_open(file, nvt_edge_reject_show, NULL);
}


#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_edge_reject_fops = {
	.proc_open = nvt_edge_reject_open,
	.proc_read = seq_read,
	.proc_write = nvt_edge_reject_store,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_edge_reject_fops = {
	.owner = THIS_MODULE,
	.open = nvt_edge_reject_open,
	.read = seq_read,
	.write = nvt_edge_reject_store,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/*
 *	/proc/edge_grid_zone cmd_param
 *      [0], 1 : panel rotate 0 degreee
 *			 2 : panel rotate 90 degree
 *			 3 : panel rotate 180 degree
 *			 4 : panel rotate 270 degree
 *
 *		[1], 1 : floating view at left side
 *		     2 : floating view at right side
 *
 *		[2], floating view start Y coordinate
 *
 *		[3], floating view end Y coordinate 
 *
 */
int32_t nvt_edge_grid_zone_set(uint8_t deg, uint8_t dir, uint16_t y1, uint16_t y2)
{
    int i, retry = 5;
    uint8_t buf[12] = {0};
/* Spinel code for OSPINEL-3535 by zhangyd22 at 2023/06/05 start */

    if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}
    if(bTouchIsAwake == 0){
		NVT_ERR("Touch is already suspend, ignore cmd");
		mutex_unlock(&ts->lock);
        return -1;
    }
/* Spinel code for OSPINEL-3535 by zhangyd22 at 2023/06/05 end */
    //---set xdata index to EVENT BUF ADDR---(set page)
    nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

    for (i = 0; i < retry; i++) {
        /*---set cmd status---*/
        buf[0] = EVENT_MAP_HOST_CMD;
        buf[1] = NVT_EXT_CMD;
        buf[2] = NVT_EXT_CMD_EDGE_GRID_ZONE;
        buf[3] = deg;
        buf[4] = dir;
        buf[5] = (uint8_t) (y1 & 0xFF);
        buf[6] = (uint8_t) ((y1 >> 8) & 0xFF);
        buf[7] = (uint8_t) (y2 & 0xFF);
        buf[8] = (uint8_t) ((y2 >> 8) & 0xFF);
        CTP_SPI_WRITE(ts->client, buf, 9);
        

        msleep(20);

        //---read cmd status---
        buf[0] = EVENT_MAP_HOST_CMD;
        buf[1] = 0xFF;
        buf[2] = 0xFF;
        buf[3] = 0xFF;
        buf[4] = 0xFF;
        buf[5] = 0xFF;
        buf[6] = 0xFF;
        buf[7] = 0xFF;
        buf[8] = 0xFF;
        CTP_SPI_READ(ts->client, buf, 9);
        if (buf[1] == 0x00)
            break;
    }

    if (unlikely(i == retry)) {
        NVT_ERR("send Cmd 0x%02X 0x%02X failed, buf[1]=0x%02X\n",
            NVT_EXT_CMD, NVT_EXT_CMD_EDGE_GRID_ZONE, buf[1]);
		mutex_unlock(&ts->lock);
        return -1;
    } else {
        NVT_LOG("send Cmd 0x%02X 0x%02X success, tried %d times\n",
            NVT_EXT_CMD, NVT_EXT_CMD_EDGE_GRID_ZONE, i);
    }
	mutex_unlock(&ts->lock);
    return 0;
}
static ssize_t nvt_edge_grid_zone_store(struct file *file, const char *buffer, size_t count, loff_t *pos) {
    int32_t tmp[4];
    uint8_t ret;
    char buf[16] = { 0 };

    ret = copy_from_user(buf, (uint8_t *) buffer, count);
    if (ret)
        return -EINVAL;

    NVT_LOG("buf=%s\n", buf);

    ret = sscanf(buf, "%d,%d,%d,%d",
        tmp, tmp+1, tmp+2, tmp+3);

    ts->edge_grid_zone_info.degree = (uint8_t) tmp[0];
    ts->edge_grid_zone_info.direction = (uint8_t) tmp[1];
    ts->edge_grid_zone_info.y1 = (uint16_t) tmp[2];
    ts->edge_grid_zone_info.y2 = (uint16_t) tmp[3];

    NVT_LOG("cmd_parm = %d,%d,%d,%d\n",
        ts->edge_grid_zone_info.degree,
        ts->edge_grid_zone_info.direction,
        ts->edge_grid_zone_info.y1,
        ts->edge_grid_zone_info.y2);

    nvt_edge_grid_zone_set(ts->edge_grid_zone_info.degree,
        ts->edge_grid_zone_info.direction,
        ts->edge_grid_zone_info.y1,
        ts->edge_grid_zone_info.y2);

	return count;
}
static int nvt_edge_grid_zone_show(struct seq_file *sfile, void *v) {
    seq_printf(sfile, "Edge Grid Zone %d,%d,%d,%d\n",
        ts->edge_grid_zone_info.degree,
        ts->edge_grid_zone_info.direction,
        ts->edge_grid_zone_info.y1,
        ts->edge_grid_zone_info.y2);
    return 0;
}
static int32_t nvt_edge_grid_zone_open(struct inode *inode, struct file *file) {
	return single_open(file, nvt_edge_grid_zone_show, NULL);
}

#ifdef HAVE_PROC_OPS
//static const struct file_operations nvt_edge_grid_zone_fops = {
static const struct proc_ops nvt_edge_grid_zone_fops = {
	.proc_open = nvt_edge_grid_zone_open,
	.proc_read = seq_read,
	.proc_write = nvt_edge_grid_zone_store,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,

};
#else
static const struct file_operations nvt_edge_grid_zone_fops = {
	.owner = THIS_MODULE,
	.open = nvt_edge_grid_zone_open,
	.read = seq_read,
	.write = nvt_edge_grid_zone_store,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/*
 *	/proc/game_mode cmd_param
 *      [0], 0 : normal mode
 *			 1 : pen mode(enhance palm reject)
 *			 2 : game mode(remove palm reject)
 *
 */
int32_t nvt_game_mode_set(uint8_t status) {
	int ret = 0;

	if(status == 0)
		ret = nvt_ext_cmd_store(GAME_MODE_PALM_CMD, GAME_MODE_NORMAL);
	else if(status == 2)
		ret = nvt_ext_cmd_store(GAME_MODE_PALM_CMD, GAME_MODE_REMOVE);
	else if(status == 1)
		ret = nvt_ext_cmd_store(GAME_MODE_PALM_CMD, GAME_MODE_ENHANCE);


	return ret;
}
static ssize_t nvt_game_mode_store(struct file *file, const char *buffer, size_t count, loff_t *pos) {
	char dbg[10] = { 0 };
	uint8_t state;
	uint8_t ret;
	
	ret = copy_from_user(dbg, (uint8_t *) buffer, sizeof(uint8_t));
	if (ret)
		return -EINVAL;

	ret = kstrtou8(dbg, 16, &state);
	if (ret < 0)
		return ret;

	NVT_LOG("moto_apk_state %d!\n", state);
	ts->game_mode_state = state;
	nvt_game_mode_set(ts->game_mode_state);

	return count;
}
static int nvt_game_mode_show(struct seq_file *sfile, void *v) {
	seq_printf(sfile, "Game Mode state %d!\n", ts->game_mode_state);
	return 0;
}
static int32_t nvt_game_mode_open(struct inode *inode, struct file *file) {
	return single_open(file, nvt_game_mode_show, NULL);
}

#ifdef HAVE_PROC_OPS
//static const struct file_operations nvt_game_mode_fops = {
static const struct proc_ops nvt_game_mode_fops = {
	.proc_open = nvt_game_mode_open,
	.proc_read = seq_read,
	.proc_write = nvt_game_mode_store,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,

};

#else
//static const struct file_operations nvt_game_mode_fops = {
static const struct proc_ops nvt_game_mode_fops = {
	.owner = THIS_MODULE,
	.open = nvt_game_mode_open,
	.read = seq_read,
	.write = nvt_game_mode_store,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/*Spinel code for control pen state by zhangyd22 at 2023/04/04 start*/
int32_t nvt_support_pen_set_nfc(uint8_t state)
{
	if (state == 1) {
		ts->nfc_state = 0;
		if (ts->pen_state == 1){
			nvt_support_pen_set(ts->pen_state, ts->pen_version);
		} else {
			NVT_LOG("The pen no connect!");
			return 0;
		}
	} else {
		nvt_support_pen_set(0, 0);
		ts->nfc_state = 1;
	}
	return 0;
}
/*Spinel code for control pen state by zhangyd22 at 2023/04/04 end*/
/*
 *	/proc/support_pen cmd_param
 *      [0], 0 : not support pen
 *			 1 : support pen
 *		[1], 0 : old pen
 *			 1 : new pen
 *
 */
int32_t nvt_support_pen_set(uint8_t state, uint8_t version) {
	int i, retry = 5;
	uint8_t buf[12] = {0};
/* Spinel code for OSPINEL-3535 by zhangyd22 at 2023/06/05 start */
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}
	if(bTouchIsAwake == 0){
		NVT_ERR("Touch is already suspend, ignore cmd");
		mutex_unlock(&ts->lock);
		return -1;
	}
/* Spinel code for OSPINEL-3535 by zhangyd22 at 2023/06/05 end */
	//---set xdata index to EVENT BUF ADDR---(set page)
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	for (i = 0; i < retry; i++) {
		/*---set cmd status---*/
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = SUPPORT_PEN_CMD;
		buf[2] = state;
		buf[3] = version;
		CTP_SPI_WRITE(ts->client, buf, 4);

		msleep(20);

		//---read cmd status---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		buf[2] = 0xFF;
		buf[3] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 4);
		if (buf[1] == 0x00)
			break;
	}

	if (unlikely(i == retry)) {
		NVT_ERR("send Cmd 0x%02X 0x%02X 0x%02X failed, buf[1]=0x%02X\n",
			SUPPORT_PEN_CMD, state, version, buf[1]);
		mutex_unlock(&ts->lock);
		return -1;
	} else {
		NVT_LOG("send Cmd 0x%02X 0x%02X 0x%02X success, tried %d times\n",
			SUPPORT_PEN_CMD, state, version, i);
	}
	mutex_unlock(&ts->lock);
	return 0;
}
static ssize_t nvt_support_pen_store(struct file *file, const char *buffer, size_t count, loff_t *pos) {
    int32_t tmp[4];
    uint8_t ret;
    char buf[16] = { 0 };

	ret = copy_from_user(buf, (uint8_t *) buffer, count);
	if (ret)
		return -EINVAL;

	NVT_LOG("buf=%s\n", buf);

/*Spinel code for open old pen by zhangyd22 at 2023/04/011 star*/
	ret = sscanf(buf, "%d,%d", tmp, tmp+1);
	ts->pen_state = tmp[0];
	ts->pen_version = tmp[1];

	NVT_LOG("support pen state %d, %d!\n", ts->pen_state, ts->pen_version);
/*Spinel code for control pen state by zhangyd22 at 2023/04/04 start*/
	if (ts->nfc_state) {
		NVT_LOG("The pen is chargering!");
	} else {
		NVT_LOG("The pen is ready!");
		nvt_support_pen_set(ts->pen_state, ts->pen_version);
	}
/*Spinel code for control pen state by zhangyd22 at 2023/04/04 end*/
/*Spinel code for open old pen by zhangyd22 at 2023/04/11 end*/

	return count;
}
static int nvt_support_pen_show(struct seq_file *sfile, void *v) {
	seq_printf(sfile, "Pen state %d!\n", ts->pen_state);
	return 0;
}
static int32_t nvt_support_pen_open(struct inode *inode, struct file *file) {
	return single_open(file, nvt_support_pen_show, NULL);
}

#ifdef HAVE_PROC_OPS
//static const struct file_operations nvt_support_pen_fops = {
static const struct proc_ops nvt_support_pen_fops = {
	.proc_open = nvt_support_pen_open,
	.proc_read = seq_read,
	.proc_write = nvt_support_pen_store,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

#else
static const struct file_operations nvt_support_pen_fops = {
	.owner = THIS_MODULE,
	.open = nvt_support_pen_open,
	.read = seq_read,
	.write = nvt_support_pen_store,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 start */
#if NVT_DPR_SWITCH
/*
* /proc/fw_pen_state
*	0:finger mode
*	1:pen mode
*/
static int32_t nvt_get_fw_pen_state_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", ts->fw_pen_state);
    return 0;
}

static int32_t nvt_get_fw_pen_state_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvt_get_fw_pen_state_show, NULL);
}
#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_get_fw_pen_state_fops = {
    .proc_open = nvt_get_fw_pen_state_open,
};
#else
static const struct file_operations nvt_get_fw_pen_state_fops = {
    .owner = THIS_MODULE,
    .open = nvt_get_fw_pen_state_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#endif
#endif
/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 end */
#endif

/*Spinel code for charging by wangxin77 at 2023/03/06 start*/
int32_t nvt_set_charger(uint8_t charger_on_off)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("nvt set charger: %d\n", charger_on_off);

	msleep(20);

	mutex_lock(&ts->lock);
	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("nvt Set event buffer index fail!\n");
		goto nvt_set_charger_out;
	}

	if (charger_on_off == 1) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = CMD_CHARGER_ON;
		ret = CTP_SPI_WRITE(ts->client, buf, 2);
		if (ret < 0) {
			NVT_ERR("nvt Write set charger command fail!\n");
			goto nvt_set_charger_out;
		} else {
			NVT_LOG("nvt set charger on cmd succeeded\n");
		}
	} else if (charger_on_off == 0) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = CMD_CHARGER_OFF;
		ret = CTP_SPI_WRITE(ts->client, buf, 2);
		if (ret < 0) {
			NVT_ERR("nvt Write set charger command fail!\n");
			goto nvt_set_charger_out;
		} else {
			NVT_LOG("nvt set charger off cmd succeeded\n");
		}
	} else {
		NVT_ERR("nvt Invalid charger parameter!\n");
		ret = -EINVAL;
	}

	nvt_set_charger_out:

	mutex_unlock(&ts->lock);

	return ret;
}
EXPORT_SYMBOL(nvt_set_charger);
/*Spinel code for charging by wangxin77 at 2023/03/06 end*/
/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
int32_t nvt_extra_proc_init(void)
{
	NVT_proc_fw_version_entry = proc_create(NVT_FW_VERSION, 0444, NULL,&nvt_fw_version_fops);
	if (NVT_proc_fw_version_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_FW_VERSION);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_FW_VERSION);
	}

	NVT_proc_baseline_entry = proc_create(NVT_BASELINE, 0444, NULL,&nvt_baseline_fops);
	if (NVT_proc_baseline_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_BASELINE);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_BASELINE);
	}

	NVT_proc_raw_entry = proc_create(NVT_RAW, 0444, NULL,&nvt_raw_fops);
	if (NVT_proc_raw_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_RAW);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_RAW);
	}

	NVT_proc_diff_entry = proc_create(NVT_DIFF, 0444, NULL,&nvt_diff_fops);
	if (NVT_proc_diff_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_DIFF);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_DIFF);
	}
	/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 start */
	NVT_gesture_entry = proc_create(GESTURE_CONTROL, 0444, NULL, &gesture_control_fops);
	if (NVT_gesture_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", GESTURE_CONTROL);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", GESTURE_CONTROL);
	}
	/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 end */
	if (ts->pen_support) {
		NVT_proc_pen_diff_entry = proc_create(NVT_PEN_DIFF, 0444, NULL,&nvt_pen_diff_fops);
		if (NVT_proc_pen_diff_entry == NULL) {
			NVT_ERR("create proc/%s Failed!\n", NVT_PEN_DIFF);
			return -ENOMEM;
		} else {
			NVT_LOG("create proc/%s Succeeded!\n", NVT_PEN_DIFF);
		}
	}
	/* Spinel code for OSPINEL-3913 by zhangyd22 at 2023/06/07 start */
	NVT_proc_fw_status_entry = proc_create(NVT_FW_STATUS, 0444, NULL,&nvt_fw_status_fops);
	if (NVT_proc_fw_status_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_FW_STATUS);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_FW_STATUS);
	}
	/* Spinel code for OSPINEL-3913 by zhangyd22 at 2023/06/07 end */
#if NVT_CUST_PROC_CMD
	NVT_proc_edge_reject_entry = proc_create(PENEL_DIRECTION, 0644, NULL, &nvt_edge_reject_fops);
	if (NVT_proc_edge_reject_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", PENEL_DIRECTION);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", PENEL_DIRECTION);
	} 

	NVT_proc_edge_grid_zone_entry = proc_create(EDGE_GRID_ZONE, 0644, NULL, &nvt_edge_grid_zone_fops);
	if (NVT_proc_edge_grid_zone_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", EDGE_GRID_ZONE);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", EDGE_GRID_ZONE);
	} 


	NVT_proc_game_mode_entry = proc_create(GAME_MODE, 0666, NULL, &nvt_game_mode_fops);
	if (NVT_proc_game_mode_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", GAME_MODE);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", GAME_MODE);
	} 

	NVT_proc_support_pen_entry = proc_create(SUPPORT_PEN, 0666, NULL, &nvt_support_pen_fops);
	if (NVT_proc_support_pen_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", SUPPORT_PEN);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", SUPPORT_PEN);
	} 
/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 start */
#if NVT_DPR_SWITCH
	NVT_proc_get_fw_pen_state_entry = proc_create(FW_PEN_STATE, 0664, NULL, &nvt_get_fw_pen_state_fops);
	if (NVT_proc_get_fw_pen_state_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", FW_PEN_STATE);
		return -ENOMEM;
	} else {
	NVT_LOG("create proc/%s Succeeded!\n", FW_PEN_STATE);
	}
#endif
/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 end */
#endif

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	deinitial function.

return:
	n.a.
*******************************************************/
void nvt_extra_proc_deinit(void)
{
	if (NVT_proc_fw_version_entry != NULL) {
		remove_proc_entry(NVT_FW_VERSION, NULL);
		NVT_proc_fw_version_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_FW_VERSION);
	}

	if (NVT_proc_baseline_entry != NULL) {
		remove_proc_entry(NVT_BASELINE, NULL);
		NVT_proc_baseline_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_BASELINE);
	}

	if (NVT_proc_raw_entry != NULL) {
		remove_proc_entry(NVT_RAW, NULL);
		NVT_proc_raw_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_RAW);
	}

	if (NVT_proc_diff_entry != NULL) {
		remove_proc_entry(NVT_DIFF, NULL);
		NVT_proc_diff_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_DIFF);
	}
	/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 start */
	if (NVT_gesture_entry != NULL) {
		remove_proc_entry(GESTURE_CONTROL, NULL);
		NVT_gesture_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", GESTURE_CONTROL);
	}
	/* Spinel code for OSPINEL-192 by dingying3 at 2023/3/6 end */
	if (ts->pen_support) {
		if (NVT_proc_pen_diff_entry != NULL) {
			remove_proc_entry(NVT_PEN_DIFF, NULL);
			NVT_proc_pen_diff_entry = NULL;
			NVT_LOG("Removed /proc/%s\n", NVT_PEN_DIFF);
		}
	}
	/* Spinel code for OSPINEL-3913 by zhangyd22 at 2023/06/07 start */
	if (NVT_proc_fw_status_entry != NULL) {
		remove_proc_entry(NVT_FW_STATUS, NULL);
		NVT_proc_fw_status_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_FW_STATUS);
	}
	/* Spinel code for OSPINEL-3913 by zhangyd22 at 2023/06/07 end */
#if NVT_CUST_PROC_CMD
	if (NVT_proc_edge_reject_entry != NULL) {
		remove_proc_entry(PENEL_DIRECTION, NULL);
		NVT_proc_edge_reject_entry = NULL;
		NVT_LOG("Removed /proc/%s \n", PENEL_DIRECTION);
	}

	if (NVT_proc_edge_grid_zone_entry != NULL) {
		remove_proc_entry(EDGE_GRID_ZONE, NULL);
		NVT_proc_edge_grid_zone_entry = NULL;
		NVT_LOG("Removed /proc/%s \n", EDGE_GRID_ZONE);
	}

	if (NVT_proc_game_mode_entry != NULL) {
		remove_proc_entry(GAME_MODE, NULL);
		NVT_proc_game_mode_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", GAME_MODE);
	}

	if (NVT_proc_support_pen_entry != NULL) {
		remove_proc_entry(SUPPORT_PEN, NULL);
		NVT_proc_support_pen_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", SUPPORT_PEN);
	}
/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 start */
#if NVT_DPR_SWITCH
	if (NVT_proc_get_fw_pen_state_entry != NULL) {
		remove_proc_entry(FW_PEN_STATE, NULL);
		NVT_proc_get_fw_pen_state_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", FW_PEN_STATE);
	}
#endif
/* Spinel code for OSPINEL-2020 by zhangyd22 at 2023/4/4 end */
#endif
}
#endif
