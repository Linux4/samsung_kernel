/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 52752 $
 * $Date: 2019-11-06 18:05:46 +0800 (?±‰∏â, 06 ?Å‰??? 2019) $
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

#include "nvt_dev.h"
#include "nvt_reg.h"

static uint8_t xdata_tmp[2048] = {0};
static int32_t xdata[2048] = {0};

static struct nvt_ts_data *proc_ts;
static struct proc_dir_entry *NVT_proc_fw_version_entry;
static struct proc_dir_entry *NVT_proc_baseline_entry;
static struct proc_dir_entry *NVT_proc_raw_entry;
static struct proc_dir_entry *NVT_proc_diff_entry;

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
	nvt_set_page(proc_ts, proc_ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = mode;
	proc_ts->nvt_ts_write(proc_ts, buf, 2, NULL, 0);

	if (mode == NORMAL_MODE) {
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = HANDSHAKING_HOST_READY;
		proc_ts->nvt_ts_write(proc_ts, buf, 2, NULL, 0);
		sec_delay(20);
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
	nvt_set_page(proc_ts, proc_ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

	//---read fw status---
	buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
	buf[1] = 0x00;
	proc_ts->nvt_ts_read(proc_ts, &buf[0], 1, &buf[1], 1);

	//pr_info("FW pipe=%d, buf[1]=0x%02X\n", (buf[1]&0x01), buf[1]);

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
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 2] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = proc_ts->plat_data->x_node_num * proc_ts->plat_data->y_node_num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//printk("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n", head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		nvt_set_page(proc_ts, head_addr + XDATA_SECTOR_SIZE * i);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (XDATA_SECTOR_SIZE / BUS_TRANSFER_LENGTH); j++) {
			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			proc_ts->nvt_ts_read(proc_ts, &buf[0], 1, &buf[1], BUS_TRANSFER_LENGTH);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_set_page(proc_ts, xdata_addr + data_len - residual_len);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (residual_len / BUS_TRANSFER_LENGTH + 1); j++) {
			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			proc_ts->nvt_ts_read(proc_ts, &buf[0], 1, &buf[1], BUS_TRANSFER_LENGTH);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++) {
		xdata[i] = (int16_t)(xdata_tmp[dummy_len + i * 2] + 256 * xdata_tmp[dummy_len + i * 2 + 1]);
	}

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(proc_ts, proc_ts->mmap->EVENT_BUF_ADDR);
}

/*******************************************************
Description:
	Novatek touchscreen firmware version show function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_fw_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "fw_ver=%d, x_num=%d, y_num=%d\n", proc_ts->fw_ver, proc_ts->plat_data->x_node_num, proc_ts->plat_data->y_node_num);
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

	for (i = 0; i < proc_ts->plat_data->y_node_num; i++) {
		for (j = 0; j < proc_ts->plat_data->x_node_num; j++) {
			seq_printf(m, "%5d, ", xdata[i * proc_ts->plat_data->x_node_num + j]);
		}
		seq_puts(m, "\n");
	}

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

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_fw_version open
	function.

return:
	n.a.
*******************************************************/
static int32_t nvt_fw_version_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&proc_ts->lock)) {
		return -ERESTARTSYS;
	}

	input_info(true, &proc_ts->client->dev, "%s  ++\n", __func__);

	if (nvt_get_fw_info(proc_ts)) {
		mutex_unlock(&proc_ts->lock);
		return -EAGAIN;
	}

	mutex_unlock(&proc_ts->lock);

	input_info(true, &proc_ts->client->dev, "%s  --\n", __func__);

	return seq_open(file, &nvt_fw_version_seq_ops);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops nvt_fw_version_fops = {
	.proc_open = nvt_fw_version_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
const struct file_operations nvt_fw_version_fops = {
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
	if (mutex_lock_interruptible(&proc_ts->lock)) {
		return -ERESTARTSYS;
	}

	input_info(true, &proc_ts->client->dev, "%s  ++\n", __func__);

	if (nvt_clear_fw_status(proc_ts)) {
		mutex_unlock(&proc_ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status(proc_ts)) {
		mutex_unlock(&proc_ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info(proc_ts)) {
		mutex_unlock(&proc_ts->lock);
		return -EAGAIN;
	}

	nvt_read_mdata(proc_ts->mmap->BASELINE_ADDR, proc_ts->mmap->BASELINE_BTN_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&proc_ts->lock);

	input_info(true, &proc_ts->client->dev, "%s  --\n", __func__);

	return seq_open(file, &nvt_seq_ops);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops nvt_baseline_fops = {
	.proc_open = nvt_baseline_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
const struct file_operations nvt_baseline_fops = {
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
	if (mutex_lock_interruptible(&proc_ts->lock)) {
		return -ERESTARTSYS;
	}

	input_info(true, &proc_ts->client->dev, "%s  ++\n", __func__);

	if (nvt_clear_fw_status(proc_ts)) {
		mutex_unlock(&proc_ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status(proc_ts)) {
		mutex_unlock(&proc_ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info(proc_ts)) {
		mutex_unlock(&proc_ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(proc_ts->mmap->RAW_PIPE0_ADDR, proc_ts->mmap->RAW_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(proc_ts->mmap->RAW_PIPE1_ADDR, proc_ts->mmap->RAW_BTN_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&proc_ts->lock);

	input_info(true, &proc_ts->client->dev, "%s  --\n", __func__);

	return seq_open(file, &nvt_seq_ops);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops nvt_raw_fops = {
	.proc_open = nvt_raw_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
const struct file_operations nvt_raw_fops = {
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
	if (mutex_lock_interruptible(&proc_ts->lock)) {
		return -ERESTARTSYS;
	}

	input_info(true, &proc_ts->client->dev, "%s  ++\n", __func__);

	if (nvt_clear_fw_status(proc_ts)) {
		mutex_unlock(&proc_ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status(proc_ts)) {
		mutex_unlock(&proc_ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info(proc_ts)) {
		mutex_unlock(&proc_ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(proc_ts->mmap->DIFF_PIPE0_ADDR, proc_ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(proc_ts->mmap->DIFF_PIPE1_ADDR, proc_ts->mmap->DIFF_BTN_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&proc_ts->lock);

	input_info(true, &proc_ts->client->dev, "%s  --\n", __func__);

	return seq_open(file, &nvt_seq_ops);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops nvt_diff_fops = {
	.proc_open = nvt_diff_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
const struct file_operations nvt_diff_fops = {
	.open = nvt_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
int32_t nvt_extra_proc_init(struct nvt_ts_data *ts)
{

	proc_ts = (struct nvt_ts_data *)ts;
	NVT_proc_fw_version_entry = proc_create(NVT_FW_VERSION, 0444, NULL,&nvt_fw_version_fops);
	if (NVT_proc_fw_version_entry == NULL) {
		input_err(true, &ts->client->dev, "%s : create proc/%s Failed!\n", __func__, NVT_FW_VERSION);
		return -ENOMEM;
	} else {
		input_info(true, &ts->client->dev, "%s: create proc/%s Succeeded!\n", __func__, NVT_FW_VERSION);
	}

	NVT_proc_baseline_entry = proc_create(NVT_BASELINE, 0444, NULL,&nvt_baseline_fops);
	if (NVT_proc_baseline_entry == NULL) {
		input_err(true, &ts->client->dev, "%s : create proc/%s Failed!\n", __func__, NVT_BASELINE);
		return -ENOMEM;
	} else {
		input_info(true, &ts->client->dev, "%s: create proc/%s Succeeded!\n", __func__, NVT_BASELINE);
	}

	NVT_proc_raw_entry = proc_create(NVT_RAW, 0444, NULL,&nvt_raw_fops);
	if (NVT_proc_raw_entry == NULL) {
		input_err(true, &ts->client->dev, "%s : create proc/%s Failed!\n", __func__, NVT_RAW);
		return -ENOMEM;
	} else {
		input_info(true, &proc_ts->client->dev, "%s: create proc/%s Succeeded!\n", __func__, NVT_RAW);
	}

	NVT_proc_diff_entry = proc_create(NVT_DIFF, 0444, NULL,&nvt_diff_fops);
	if (NVT_proc_diff_entry == NULL) {
		input_err(true, &ts->client->dev, "%s : create proc/%s Failed!\n", __func__, NVT_DIFF);
		return -ENOMEM;
	} else {
		input_info(true, &ts->client->dev, "%s: create proc/%s Succeeded!\n", __func__, NVT_DIFF);
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	deinitial function.

return:
	n.a.
*******************************************************/
void nvt_extra_proc_deinit(struct nvt_ts_data *ts)
{
	if (NVT_proc_fw_version_entry != NULL) {
		remove_proc_entry(NVT_FW_VERSION, NULL);
		NVT_proc_fw_version_entry = NULL;
		input_info(true, &ts->client->dev, "%s: Removed /proc/%s\n", __func__, NVT_FW_VERSION);
	}

	if (NVT_proc_baseline_entry != NULL) {
		remove_proc_entry(NVT_BASELINE, NULL);
		NVT_proc_baseline_entry = NULL;
		input_info(true, &ts->client->dev, "%s: Removed /proc/%s\n", __func__, NVT_BASELINE);
	}

	if (NVT_proc_raw_entry != NULL) {
		remove_proc_entry(NVT_RAW, NULL);
		NVT_proc_raw_entry = NULL;
		input_info(true, &ts->client->dev, "%s: Removed /proc/%s\n", __func__, NVT_RAW);
	}

	if (NVT_proc_diff_entry != NULL) {
		remove_proc_entry(NVT_DIFF, NULL);
		NVT_proc_diff_entry = NULL;
		input_info(true, &ts->client->dev, "%s: Removed /proc/%s\n", __func__, NVT_DIFF);
	}
}

static struct proc_dir_entry *NVT_proc_entry;
#define DEVICE_NAME	"NVTSPI"

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI read function.

return:
	Executive outcomes. 2---succeed. -5,-14---failed.
*******************************************************/
static ssize_t nvt_flash_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	uint8_t *str = NULL;
	int32_t ret = 0;
	int32_t retries = 0;
	int8_t spi_wr = 0;
	uint8_t *buf;

	if ((count > NVT_TRANSFER_LEN + 3) || (count < 3)) {
		input_err(true, &proc_ts->client->dev, "invalid transfer len!\n");
		return -EFAULT;
	}

	/* allocate buffer for spi transfer */
	str = (uint8_t *)kzalloc((count), GFP_KERNEL);
	if(str == NULL) {
		input_err(true, &proc_ts->client->dev, "kzalloc for buf failed!\n");
		ret = -ENOMEM;
		goto kzalloc_failed;
	}

	buf = (uint8_t *)kzalloc((count), GFP_KERNEL | GFP_DMA);
	if(buf == NULL) {
		input_err(true, &proc_ts->client->dev, "kzalloc for buf failed!\n");
		ret = -ENOMEM;
		kfree(str);
		str = NULL;
		goto kzalloc_failed;
	}

	if (copy_from_user(str, buff, count)) {
		input_err(true, &proc_ts->client->dev, "copy from user error\n");
		ret = -EFAULT;
		goto out;
	}

	spi_wr = str[0] >> 7;
	memcpy(buf, str+2, ((str[0] & 0x7F) << 8) | str[1]);

	if (spi_wr == NVTWRITE) {	//SPI write
		while (retries < 20) {
			ret = proc_ts->nvt_ts_write(proc_ts, buf, ((str[0] & 0x7F) << 8) | str[1], NULL, 0);
			if (!ret)
				break;
			else
				input_err(true, &proc_ts->client->dev, "error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		if (unlikely(retries == 20)) {
			input_err(true, &proc_ts->client->dev, "error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else if (spi_wr == NVTREAD) {	//SPI read
		while (retries < 20) {
			ret = proc_ts->nvt_ts_read(proc_ts, &buf[0], 1, &buf[1], ((str[0] & 0x7F) << 8) | str[1] - 1);
			if (!ret)
				break;
			else
				input_err(true, &proc_ts->client->dev, "error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		memcpy(str+2, buf, ((str[0] & 0x7F) << 8) | str[1]);
		// copy buff to user if spi transfer
		if (retries < 20) {
			if (copy_to_user(buff, str, count)) {
				ret = -EFAULT;
				goto out;
			}
		}

		if (unlikely(retries == 20)) {
			input_err(true, &proc_ts->client->dev, "error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else {
		input_err(true, &proc_ts->client->dev, "Call error, str[0]=%d\n", str[0]);
		ret = -EFAULT;
		goto out;
	}

out:
	kfree(str);
	kfree(buf);
kzalloc_failed:
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI open function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
static int32_t nvt_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev;

	dev = kzalloc(sizeof(struct nvt_flash_data), GFP_KERNEL);
	if (dev == NULL) {
		pr_err("Failed to allocate memory for nvt flash data\n");
		return -ENOMEM;
	}

	rwlock_init(&dev->lock);
	file->private_data = dev;

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI close function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev = file->private_data;

	if (dev)
		kfree(dev);

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops nvt_flash_fops = {
	.proc_open = nvt_flash_open,
	.proc_release = nvt_flash_close,
	.proc_read = nvt_flash_read,
};
#else
const struct file_operations nvt_flash_fops = {
	.open = nvt_flash_open,
	.release = nvt_flash_close,
	.read = nvt_flash_read,
};
#endif

int nvt_flash_proc_init(struct nvt_ts_data *ts)
{
	proc_ts = (struct nvt_ts_data *)ts;
	NVT_proc_entry = proc_create(DEVICE_NAME, 0444, NULL,&nvt_flash_fops);
	if (NVT_proc_entry == NULL) {
		input_err(true, &ts->client->dev, "%s: Failed!\n", __func__);
		return -ENOMEM;
	} else {
		input_info(true, &ts->client->dev, "%s: Succeeded!\n", __func__);
	}

	input_info(true, &proc_ts->client->dev, "%s: ============================================================\n", __func__);
	input_info(true, &proc_ts->client->dev, "%s: Create /proc/%s\n", __func__, DEVICE_NAME);
	input_info(true, &proc_ts->client->dev, "%s: ============================================================\n", __func__);

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI deinitial function.

return:
	n.a.
*******************************************************/
void nvt_flash_proc_deinit(struct nvt_ts_data *ts)
{
	if (NVT_proc_entry != NULL) {
		remove_proc_entry(DEVICE_NAME, NULL);
		NVT_proc_entry = NULL;
		input_info(true, &ts->client->dev, "%s: Removed /proc/%s\n", __func__, DEVICE_NAME);
	}
}

#define LPWG_DUMP_LOG_LEN	505	//2B Slot ID + 2B History Size + 500B History + 1B Checksum
#define LPWG_DUMP_EVENT_LEN	5
#define LPWG_DUMP_EVENT_MAX_NUM	100
#define LPWG_DUMP_EVENT_MSG_LEN	20

void nvt_ts_lpwg_dump_buf_init(struct nvt_ts_data *ts)
{
	ts->lpwg_dump_buf = devm_kzalloc(&ts->client->dev, LPWG_DUMP_TOTAL_SIZE, GFP_KERNEL);
	if (ts->lpwg_dump_buf == NULL) {
		input_err(true, &ts->client->dev, "kzalloc for lpwg_dump_buf failed!\n");
		return;
	}
	ts->lpwg_dump_buf_idx = 0;
	input_info(true, &ts->client->dev, "%s : done\n", __func__);
}

int nvt_ts_lpwg_dump_buf_write(struct nvt_ts_data *ts, u8 *buf)
{
	int i = 0;

	if (ts->lpwg_dump_buf == NULL) {
		input_err(true, &ts->client->dev, "%s : kzalloc for lpwg_dump_buf failed!\n", __func__);
		return -1;
	}
//	input_info(true, &proc_ts->client->dev, "%s : idx(%d) data (0x%X,0x%X,0x%X,0x%X,0x%X)\n",
//			__func__, ts->lpwg_dump_buf_idx, buf[0], buf[1], buf[2], buf[3], buf[4]);

	for (i = 0 ; i < LPWG_DUMP_PACKET_SIZE ; i++) {
		ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx++] = buf[i];
	}
	if (ts->lpwg_dump_buf_idx >= LPWG_DUMP_TOTAL_SIZE) {
		input_info(true, &ts->client->dev, "%s : write end of data buf(%d)!\n",
					__func__, ts->lpwg_dump_buf_idx);
		ts->lpwg_dump_buf_idx = 0;
	}
	return 0;
}

int nvt_ts_lpwg_dump_buf_read(struct nvt_ts_data *ts, u8 *buf)
{

	u8 read_buf[30] = { 0 };
	int read_packet_cnt;
	int start_idx;
	int i;

	if (ts->lpwg_dump_buf == NULL) {
		input_err(true, &proc_ts->client->dev, "%s : kzalloc for lpwg_dump_buf failed!\n", __func__);
		return 0;
	}

	if (ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx] == 0
		&& ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx + 1] == 0
		&& ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx + 2] == 0) {
		start_idx = 0;
		read_packet_cnt = ts->lpwg_dump_buf_idx / LPWG_DUMP_PACKET_SIZE;
	} else {
		start_idx = ts->lpwg_dump_buf_idx;
		read_packet_cnt = LPWG_DUMP_TOTAL_SIZE / LPWG_DUMP_PACKET_SIZE;
	}

	input_info(true, &proc_ts->client->dev, "%s : lpwg_dump_buf_idx(%d), start_idx (%d), read_packet_cnt(%d)\n",
				__func__, ts->lpwg_dump_buf_idx, start_idx, read_packet_cnt);

	for (i = 0 ; i < read_packet_cnt ; i++) {
		memset(read_buf, 0x00, 30);
		snprintf(read_buf, 30, "%03d : %02X%02X%02X%02X%02X\n",
					i, ts->lpwg_dump_buf[start_idx + 0], ts->lpwg_dump_buf[start_idx + 1],
					ts->lpwg_dump_buf[start_idx + 2], ts->lpwg_dump_buf[start_idx + 3],
					ts->lpwg_dump_buf[start_idx + 4]);

//		input_info(true, &proc_ts->client->dev, "%s : %s\n", __func__, read_buf);
		strlcat(buf, read_buf, PAGE_SIZE);

		if (start_idx + LPWG_DUMP_PACKET_SIZE >= LPWG_DUMP_TOTAL_SIZE) {
			start_idx = 0;
		} else {
			start_idx += 5;
		}
	}

	return 0;
}

/*******************************************************
Description:
	Novatek lpwg log dump function.

return:
	n.a.
*******************************************************/
void nvt_ts_lpwg_dump(struct nvt_ts_data *ts)
{
#if 0
	struct nvt_ts_lpwg_coordinate_event *p_lpwg_coordinate_event;
	struct nvt_ts_lpwg_gesture_event *p_lpwg_gesture_event;
	struct nvt_ts_lpwg_vendor_event *p_lpwg_vendor_event;
	char buff[LPWG_DUMP_EVENT_MSG_LEN] = { 0 };
#endif
	uint8_t log_dump[LPWG_DUMP_LOG_LEN + DUMMY_BYTES] = {0};
	u16 next_slot = 0, history_size = 0, event_cnt = 0, i = 0;
	int32_t ret = -1;

	if (ts->mmap->LPWG_DUMP_ADDR == 0) {
		input_err(true, &proc_ts->client->dev, "%s: Invalid LPWG dump address.(%d)\n", __func__, ts->mmap->LPWG_DUMP_ADDR);
		return;
	}

	mutex_lock(&ts->lock);

	//---set xdata index to LPWG_DUMP_ADDR---
	nvt_set_page(ts, ts->mmap->LPWG_DUMP_ADDR);

	//---read data from index---
	log_dump[0] = ts->mmap->LPWG_DUMP_ADDR & (0x7F);
	ret = ts->nvt_ts_read(ts, &log_dump[0], 1, &log_dump[1], LPWG_DUMP_LOG_LEN + DUMMY_BYTES - 1);
	if (ret < 0) {
		input_err(true, &proc_ts->client->dev, "%s:  CTP_SPI_READ failed.(%d)\n", __func__, ret);
		goto XFER_ERROR;
	}

#if POINT_DATA_CHECKSUM
	ret = nvt_ts_lpwg_dump_checksum(ts, log_dump, LPWG_DUMP_LOG_LEN);
	if (ret) {
		goto XFER_ERROR;
	}
#endif /* POINT_DATA_CHECKSUM */

	// Deal with 4 header bytes, skip 1 dummy byte
	next_slot = log_dump[1] + (log_dump[2] << 8);
	history_size = log_dump[3] + (log_dump[4] << 8);
	event_cnt = history_size / LPWG_DUMP_EVENT_LEN;

	if (event_cnt < LPWG_DUMP_EVENT_MAX_NUM) {
		i = 0;
	} else {
		i = next_slot;
	}
	input_info(true, &proc_ts->client->dev, "%s: event_cnt(%d) start i(%d)", __func__, event_cnt, i);

	for (; event_cnt > 0; i++, event_cnt--) {

		i %= LPWG_DUMP_EVENT_MAX_NUM;	// in case overrun, FIFO (Round Robin scheme)

		nvt_ts_lpwg_dump_buf_write(ts, &log_dump[LPWG_DUMP_EVENT_LEN * (i + 1)]);

#if 0
		//extra 1 event len for 1 dummy byte and 4 header bytes
		p_lpwg_coordinate_event = (struct nvt_ts_lpwg_coordinate_event *)&log_dump[LPWG_DUMP_EVENT_LEN * (i + 1)];
//		input_info(true, &proc_ts->client->dev, "%s: Event slot(%d) Event type(%d)",
//				__func__, i, p_lpwg_coordinate_event->event_type);

		if (p_lpwg_coordinate_event->event_type == LPWG_EVENT_COOR) {
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &proc_ts->client->dev, "%s: slot(%2d) [%s] TID(%2d) : X(%4d) Y(%4d) Frame count(%4d)\n",
				__func__, i, p_lpwg_coordinate_event->touch_status ? "R" : "P", p_lpwg_coordinate_event->tid,
				((p_lpwg_coordinate_event->x_11_4 << 4) + p_lpwg_coordinate_event->x_3_0),
				((p_lpwg_coordinate_event->y_11_4 << 4) + p_lpwg_coordinate_event->y_3_0),
				((p_lpwg_coordinate_event->frame_count_9_8 << 8) + p_lpwg_coordinate_event->frame_count_7_0));
#else
			input_info(true, &proc_ts->client->dev, "%s: slot(%2d) [%s] TID(%2d) Frame count(%4d)\n",
				__func__, i, p_lpwg_coordinate_event->touch_status ? "R" : "P", p_lpwg_coordinate_event->tid,
				((p_lpwg_coordinate_event->frame_count_9_8 << 8) + p_lpwg_coordinate_event->frame_count_7_0));
#endif
		} else if (p_lpwg_coordinate_event->event_type == LPWG_EVENT_GESTURE) {
			p_lpwg_gesture_event = (struct nvt_ts_lpwg_gesture_event *)p_lpwg_coordinate_event;

			input_info(true, &proc_ts->client->dev, "%s: slot(%2d) Gesture ID(%11s)\n",
				__func__, i, p_lpwg_gesture_event->gesture_id ? "Swipe up" : "Double tap");
		} else if (p_lpwg_coordinate_event->event_type == LPWG_EVENT_VENDOR) {
			p_lpwg_vendor_event = (struct nvt_ts_lpwg_vendor_event *)p_lpwg_coordinate_event;

			switch (p_lpwg_vendor_event->ng_code) {
			case 4:
				snprintf(buff, sizeof(buff), "%s", "Timing Err");
				break;
			case 5:
				snprintf(buff, sizeof(buff), "%s", "Distance Err");
				break;
			case 6:
				snprintf(buff, sizeof(buff), "%s", "Long Touch Err");
				break;
			case 7:
				snprintf(buff, sizeof(buff), "%s", "Multi-Finger");
				break;
			default:
				snprintf(buff, sizeof(buff), "%s", "Unknown Err Code");
			}
			input_info(true, &proc_ts->client->dev, "%s: slot(%2d) Event type(Vendor) Info(%2d) NgType(%20s)\n",
					__func__, i, p_lpwg_vendor_event->info_type, buff);
		} else {	// invalid event
			input_info(true, &proc_ts->client->dev, "%s: Event slot(%2d) Event type(Unknown)"
				"    invalid event!!! (%02X, %02X, %02X, %02X, %02X)\n",
				__func__, i, log_dump[LPWG_DUMP_EVENT_LEN * (i + 1)],
				log_dump[LPWG_DUMP_EVENT_LEN * (i + 1) + 1], log_dump[LPWG_DUMP_EVENT_LEN * (i + 1) + 2],
				log_dump[LPWG_DUMP_EVENT_LEN * (i + 1) + 3], log_dump[LPWG_DUMP_EVENT_LEN * (i + 1) + 4]);
		}
#endif

	}

XFER_ERROR:
	//---set xdata index to EVENT_BUF_ADDR---
	nvt_set_page(ts, ts->mmap->EVENT_BUF_ADDR);

	mutex_unlock(&ts->lock);

	return;
}
MODULE_LICENSE("GPL");
