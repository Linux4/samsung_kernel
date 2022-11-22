// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/wait.h>

#include "dsp-log.h"
#include "dsp-device.h"
#include "hardware/dsp-system.h"
#include "hardware/dsp-ctrl.h"
#include "hardware/dsp-dump.h"
#include "dsp-binary.h"
#include "hardware/dsp-mailbox.h"
#include "dsp-npu.h"
#include "dsp-debug.h"
#include "hardware/dsp-debug.h"

#define DSP_HW_DEBUG_LOG_FILE_NAME	"/data/dsp_log.bin"
#define DSP_HW_DEBUG_LOG_LINE_SIZE	(128)
#define DSP_HW_DEBUG_LOG_TIME		(1)

static int dsp_hw_debug_power_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;

	dsp_enter();
	debug = file->private;
	dspdev = debug->dspdev;

	mutex_lock(&dspdev->lock);
	if (dsp_device_power_active(dspdev))
		seq_puts(file, "DSP power on\n");
	else
		seq_puts(file, "DSP power off\n");

	seq_printf(file, "open count %u / start count %u\n",
			dspdev->open_count, dspdev->start_count);
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_power_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_power_show, inode->i_private);
}

static ssize_t dsp_hw_debug_power_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	char command[10];
	ssize_t size;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;

	size = simple_write_to_buffer(command, sizeof(command), ppos,
			user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}

	command[size - 1] = '\0';
	if (sysfs_streq(command, "open")) {
		ret = dsp_device_open(debug->dspdev);
		if (ret)
			goto p_err;
	} else if (sysfs_streq(command, "close")) {
		dsp_device_close(debug->dspdev);
	} else if (sysfs_streq(command, "start")) {
		ret = dsp_device_start(debug->dspdev, 0);
		if (ret)
			goto p_err;
	} else if (sysfs_streq(command, "stop")) {
		dsp_device_stop(debug->dspdev, 1);
	} else {
		ret = -EINVAL;
		dsp_err("power command[%s] of debugfs is invalid\n", command);
		goto p_err;
	}

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_power_fops = {
	.open		= dsp_hw_debug_power_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_power_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_clk_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;

	dsp_enter();
	debug = file->private;
	dspdev = debug->dspdev;

	mutex_lock(&dspdev->lock);
	if (dsp_device_power_active(dspdev))
		dsp_clk_user_dump(&dspdev->system.clk, file);
	else
		seq_puts(file, "power off\n");
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_clk_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_clk_show, inode->i_private);
}

static const struct file_operations dsp_hw_debug_clk_fops = {
	.open		= dsp_hw_debug_clk_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_devfreq_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_pm *pm;
	struct dsp_pm_devfreq *devfreq;
	int count, idx;

	dsp_enter();
	debug = file->private;
	pm = &debug->dspdev->system.pm;

	mutex_lock(&pm->lock);

	for (count = 0; count < DSP_DEVFREQ_COUNT; ++count) {
		devfreq = &pm->devfreq[count];
		seq_printf(file, "[%s] id : %d\n", devfreq->name, count);
		seq_printf(file, "[%s] available level count [0 - %u]\n",
				devfreq->name, devfreq->count - 1);
		for (idx = 0; idx < devfreq->count; ++idx)
			seq_printf(file, "[%s] [L%u] %u\n",
					devfreq->name, idx,
					devfreq->table[idx]);

		seq_printf(file, "[%s] boot    : L%u\n",
				devfreq->name, devfreq->boot_qos);
		seq_printf(file, "[%s] dynamic : L%u\n",
				devfreq->name, devfreq->dynamic_qos);
		seq_printf(file, "[%s] static  : L%u\n",
				devfreq->name, devfreq->static_qos);
		seq_printf(file, "[%s] current : L%u\n",
				devfreq->name, devfreq->current_qos);
		seq_printf(file, "[%s] min     : L%u\n",
				devfreq->name, devfreq->min_qos);
		if (devfreq->force_qos < 0)
			seq_printf(file, "[%s] force   : none\n",
					devfreq->name);
		else
			seq_printf(file, "[%s] force   : L%u\n",
					devfreq->name, devfreq->force_qos);
		seq_printf(file, "[%s] dynamic total count : %u\n",
				devfreq->name, devfreq->dynamic_total_count);
		seq_printf(file, "[%s] dynamic count : ", devfreq->name);
		for (idx = 0; idx < DSP_DEVFREQ_RESERVED_COUNT; ++idx)
			seq_printf(file, "%3u ", devfreq->dynamic_count[idx]);
		seq_puts(file, "\n");
		seq_printf(file, "[%s] static total count : %u\n",
				devfreq->name, devfreq->static_total_count);
		seq_printf(file, "[%s] static count : ", devfreq->name);
		for (idx = 0; idx < DSP_DEVFREQ_RESERVED_COUNT; ++idx)
			seq_printf(file, "%3u ", devfreq->static_count[idx]);
		seq_puts(file, "\n");
	}
	seq_printf(file, "[pm] dvfs mode : %d\n", pm->dvfs);
	seq_printf(file, "[pm] dvfs disable count : %u\n",
			pm->dvfs_disable_count);
	seq_printf(file, "[pm] dvfs mode lock : %d\n", pm->dvfs_lock);

	seq_puts(file, "Command to change devfreq setting\n");
	seq_puts(file, "id/level information can be checked with ");
	seq_puts(file, "'cat /d/dsp/hardware/devfreq'\n");
	seq_puts(file, " [mode 0] add dynamic qos\n");
	seq_puts(file, "  echo 0 {level} > /d/dsp/hardware/devfreq\n");
	seq_puts(file, " [mode 1] del dynamic qos\n");
	seq_puts(file, "  echo 1 {level} > /d/dsp/hardware/devfreq\n");
	seq_puts(file, " [mode 2] add static qos\n");
	seq_puts(file, "  echo 2 {level} > /d/dsp/hardware/devfreq\n");
	seq_puts(file, " [mode 3] del static qos\n");
	seq_puts(file, "  echo 3 {level} > /d/dsp/hardware/devfreq\n");
	seq_puts(file, " [mode 4] enable force qos\n");
	seq_puts(file, "  echo 4 {id} {level} > /d/dsp/hardware/devfreq\n");
	seq_puts(file, " [mode 5] disable force qos\n");
	seq_puts(file, "  echo 5 {id} > /d/dsp/hardware/devfreq\n");
	seq_puts(file, " [mode 6] lock dvfs mode\n");
	seq_puts(file, "  echo 6 {0 or others} > /d/dsp/hardware/devfreq\n");

	mutex_unlock(&pm->lock);
	dsp_leave();
	return 0;
}

static int dsp_hw_debug_devfreq_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_devfreq_show, inode->i_private);
}

static ssize_t dsp_hw_debug_devfreq_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_pm *pm;
	char buf[30];
	ssize_t size;
	int mode, num1, num2;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	pm = &debug->dspdev->system.pm;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%d %d %d", &mode, &num1, &num2);
	if ((ret != 2) && (ret != 3)) {
		dsp_err("Failed to get devfreq parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	switch (mode) {
	case 0:
		dsp_pm_update_devfreq_busy(pm, num1);
		break;
	case 1:
		dsp_pm_update_devfreq_idle(pm, num1);
		break;
	case 2:
		dsp_pm_dvfs_disable(pm, num1);
		break;
	case 3:
		dsp_pm_dvfs_enable(pm, num1);
		break;
	case 4:
		dsp_pm_set_force_qos(pm, num1, num2);
		break;
	case 5:
		dsp_pm_set_force_qos(pm, num1, -1);
		break;
	case 6:
		pm->dvfs_lock = num1;
		break;
	default:
		ret = -EINVAL;
		dsp_err("mode for devfreq setting is invalid(%d)\n", mode);
		goto p_err;
	}

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_devfreq_fops = {
	.open		= dsp_hw_debug_devfreq_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_devfreq_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_sfr_show(struct seq_file *file, void *unused)
{
	int ret;
	struct dsp_hw_debug *debug;

	dsp_enter();
	debug = file->private;

	ret = dsp_device_open(debug->dspdev);
	if (ret)
		goto p_err_open;

	ret = dsp_device_start(debug->dspdev, 0);
	if (ret)
		goto p_err_start;

	dsp_dump_ctrl_user(file);
	dsp_device_stop(debug->dspdev, 1);
	dsp_device_close(debug->dspdev);

	dsp_leave();
	return 0;
p_err_start:
	dsp_device_close(debug->dspdev);
p_err_open:
	return 0;
}

static int dsp_hw_debug_sfr_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_sfr_show, inode->i_private);
}

static const struct file_operations dsp_hw_debug_sfr_fops = {
	.open		= dsp_hw_debug_sfr_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_mem_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_memory *mem;
	int idx;

	dsp_enter();
	debug = file->private;
	mem = &debug->dspdev->system.memory;

	for (idx = 0; idx < DSP_PRIV_MEM_COUNT; ++idx)
		seq_printf(file, "[id:%d][%15s] : %zu KB (%zu KB ~ %zu KB)\n",
				idx, mem->priv_mem[idx].name,
				mem->priv_mem[idx].size / SZ_1K,
				mem->priv_mem[idx].min_size / SZ_1K,
				mem->priv_mem[idx].max_size / SZ_1K);

	seq_puts(file, "Command to change allocated memory size\n");
	seq_puts(file, " echo {mem_id} {size} > /d/dsp/hardware/mem\n");
	seq_puts(file, " (Size must be in KB\n");
	seq_puts(file, "  Command only works when power is off)\n");

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_mem_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_mem_show, inode->i_private);
}

static ssize_t dsp_hw_debug_mem_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;
	struct dsp_memory *mem;
	char buf[128];
	ssize_t len;
	unsigned int id, size;
	struct dsp_priv_mem *pmem;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	dspdev = debug->dspdev;
	mem = &dspdev->system.memory;

	if (count > sizeof(buf)) {
		dsp_err("writing size(%zd) is larger than buffer\n", count);
		goto out;
	}

	len = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (len <= 0) {
		dsp_err("Failed to get user parameter(%zd)\n", len);
		goto out;
	}
	buf[len - 1] = '\0';

	ret = sscanf(buf, "%u %u\n", &id, &size);
	if (ret != 2) {
		dsp_err("Failed to get command changing memory size(%d)\n",
				ret);
		goto out;
	}

	if (id >= DSP_PRIV_MEM_COUNT) {
		dsp_err("memory id(%u) of command is invalid(0 ~ %u)\n",
				id, DSP_PRIV_MEM_COUNT - 1);
		goto out;
	}

	mutex_lock(&dspdev->lock);
	if (dspdev->open_count) {
		dsp_err("device was already running(%u)\n", dspdev->open_count);
		mutex_unlock(&dspdev->lock);
		goto out;
	}

	pmem = &mem->priv_mem[id];
	size = PAGE_ALIGN(size * SZ_1K);
	if (size >= pmem->min_size && size <= pmem->max_size) {
		dsp_info("size of %s is changed(%zu KB -> %u KB)\n",
				pmem->name, pmem->size / SZ_1K, size / SZ_1K);
		pmem->size = size;
	} else {
		dsp_warn("invalid size %u KB (%s, %zu KB ~ %zu KB)\n",
				size / SZ_1K, pmem->name,
				pmem->min_size / SZ_1K,
				pmem->max_size / SZ_1K);
	}

	mutex_unlock(&dspdev->lock);

	dsp_leave();
out:
	return count;
}

static const struct file_operations dsp_hw_debug_mem_fops = {
	.open		= dsp_hw_debug_mem_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_mem_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_fw_log_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_hw_debug_log *log;
	struct dsp_device *dspdev;

	dsp_enter();
	debug = file->private;
	log = debug->log;
	dspdev = debug->dspdev;

	seq_puts(file, "< Information about fw log of DSP >\n");
	if (log->log_file)
		seq_printf(file, "%s will be created when device closes\n",
				DSP_HW_DEBUG_LOG_FILE_NAME);
	else
		seq_printf(file, "%s is not created\n",
				DSP_HW_DEBUG_LOG_FILE_NAME);

	mutex_lock(&dspdev->lock);
	if (dspdev->start_count) {
		seq_printf(file, "front : %d / rear : %d\n",
				readl(&log->queue->front),
				readl(&log->queue->rear));
		seq_printf(file, "data_size : %d / data_count : %d\n",
				readl(&log->queue->data_size),
				readl(&log->queue->data_count));
	} else {
		seq_puts(file, "log structure is not initilized\n");
	}
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_fw_log_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_fw_log_show, inode->i_private);
}

static ssize_t dsp_hw_debug_fw_log_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_hw_debug_log *log;
	char command[10];
	ssize_t size;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	log = debug->log;

	size = simple_write_to_buffer(command, sizeof(command), ppos,
			user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}

	command[size - 1] = '\0';
	if (sysfs_streq(command, "enable")) {
		log->log_file = true;
	} else if (sysfs_streq(command, "disable")) {
		log->log_file = false;
	} else {
		ret = -EINVAL;
		dsp_err("command[%s] about fw_log is invalid\n", command);
		goto p_err;
	}

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_fw_log_fops = {
	.open		= dsp_hw_debug_fw_log_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_fw_log_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_wait_time_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;

	dsp_enter();
	debug = file->private;
	sys = &debug->dspdev->system;

	seq_printf(file, "[id:%u] boot wait time %ums\n",
			DSP_SYSTEM_WAIT_BOOT, sys->wait[DSP_SYSTEM_WAIT_BOOT]);
	seq_printf(file, "[id:%u] mailbox wait time %ums\n",
			DSP_SYSTEM_WAIT_MAILBOX,
			sys->wait[DSP_SYSTEM_WAIT_MAILBOX]);
	seq_printf(file, "[id:%u] reset wait time %ums\n",
			DSP_SYSTEM_WAIT_RESET,
			sys->wait[DSP_SYSTEM_WAIT_RESET]);

	seq_puts(file, "Command to change wait time\n");
	seq_puts(file, " echo {id} {time} > /d/dsp/hardware/wait_time\n");
	dsp_leave();
	return 0;
}

static int dsp_hw_debug_wait_time_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_wait_time_show, inode->i_private);
}

static ssize_t dsp_hw_debug_wait_time_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;
	char buf[30];
	ssize_t size;
	unsigned int id, time;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	dspdev = debug->dspdev;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%u %u", &id, &time);
	if (ret != 2) {
		dsp_err("Failed to get wait_time parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	if (id >= DSP_SYSTEM_WAIT_NUM) {
		ret = -EINVAL;
		dsp_err("wait_time id(%u) of command is invalid(0 ~ %u)\n",
				id, DSP_SYSTEM_WAIT_NUM - 1);
		goto p_err;
	}

	mutex_lock(&dspdev->lock);
	if (dspdev->open_count) {
		dsp_err("device already opened (%u/%u)\n",
				dspdev->open_count, dspdev->start_count);
	} else {
		dsp_info("wait_time of id[%u] is changed from %ums to %ums\n",
				id, dspdev->system.wait[id], time);
		dspdev->system.wait[id] = time;
	}

	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_wait_time_fops = {
	.open           = dsp_hw_debug_wait_time_open,
	.read           = seq_read,
	.write          = dsp_hw_debug_wait_time_write,
	.llseek         = seq_lseek,
	.release        = single_release
};

static int dsp_hw_debug_layer_range_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;

	dsp_enter();
	debug = file->private;
	sys = &debug->dspdev->system;

	seq_puts(file, "Command to set layer_range to run layer by layer\n");
	seq_puts(file, " echo ${start} ${end} > /d/dsp/hardware/layer_range\n");

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_layer_range_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_layer_range_show,
			inode->i_private);
}

static ssize_t dsp_hw_debug_layer_range_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;
	char buf[30];
	ssize_t size;
	unsigned int start_layer;
	unsigned int end_layer;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	dspdev = debug->dspdev;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%u %u", &start_layer, &end_layer);
	if (ret != 2) {
		dsp_err("Failed to get layer_range parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	mutex_lock(&dspdev->lock);
	if (dspdev->open_count) {
		dsp_err("device already opened (%u/%u)\n",
				dspdev->open_count, dspdev->start_count);
	} else {
		dspdev->system.layer_start = start_layer;
		dspdev->system.layer_end = end_layer;
		dsp_info("layer_range is set %d to %d\n",
				start_layer, end_layer);
	}
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_layer_range_fops = {
	.open		= dsp_hw_debug_layer_range_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_layer_range_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_mailbox_show(struct seq_file *file, void *unused)
{
	dsp_enter();
	seq_printf(file, "mailbox supportes version : v%u ~ v%u\n",
			DSP_MAILBOX_VERSION_START + 1,
			DSP_MAILBOX_VERSION_END - 1);
	seq_printf(file, "message supportes version : v%u ~ v%u\n",
			DSP_MESSAGE_VERSION_START + 1,
			DSP_MESSAGE_VERSION_END - 1);

	seq_puts(file, "Command to send mail as count value\n");
	seq_puts(file, " echo {count} > /d/dsp/hardware/mailbox\n");
	dsp_leave();
	return 0;
}

static int dsp_hw_debug_mailbox_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_mailbox_show, inode->i_private);
}

static void __dsp_hw_debug_mailbox_test(struct dsp_device *dspdev,
		unsigned int count)
{
	int ret;
	struct dsp_system *sys;
	int idx = -1, fail;
	struct dsp_mailbox_pool **pool;
	struct dsp_task **task;

	dsp_enter();
	sys = &dspdev->system;

	pool = vmalloc(sizeof(*pool) * count);
	if (!pool)
		goto p_err_pool;

	task = vmalloc(sizeof(*task) * count);
	if (!task)
		goto p_err_task;

	for (idx = 0; idx < count; ++idx) {
		pool[idx] = dsp_mailbox_alloc_pool(&sys->mailbox,
				SZ_1K * (idx % 2 + 1));
		if (IS_ERR(pool[idx])) {
			for (fail = 0; fail < idx; ++fail)
				dsp_mailbox_free_pool(pool[fail]);
			goto p_err_alloc;
		}
	}

	for (idx = 0; idx < count; ++idx) {
		task[idx] = dsp_task_create(&sys->task_manager, false);
		if (IS_ERR(task[idx])) {
			for (fail = 0; fail < idx; ++fail)
				dsp_task_destroy(task[fail]);
			goto p_err_create;
		}

		task[idx]->message_id = idx % DSP_COMMON_MESSAGE_NUM;
		task[idx]->pool = pool[idx];
		task[idx]->wait = true;
	}

	for (idx = 0; idx < count; ++idx) {
		ret = dsp_system_execute_task(sys, task[idx]);
		if (ret)
			goto p_err_execute;
	}

	for (idx = 0; idx < count; ++idx) {
		dsp_task_destroy(task[idx]);
		dsp_mailbox_free_pool(pool[idx]);
	}

	vfree(task);
	vfree(pool);
	dsp_leave();
	return;
p_err_execute:
	for (fail = 0; fail < count; ++fail)
		dsp_task_destroy(task[fail]);
p_err_create:
	for (fail = 0; fail < count; ++fail)
		dsp_mailbox_free_pool(pool[fail]);
p_err_alloc:
	vfree(task);
p_err_task:
	vfree(pool);
p_err_pool:
	dsp_err("mailbox test is fail(%u/%d)\n", count, idx);
}

static ssize_t dsp_hw_debug_mailbox_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	unsigned int mail_count;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;

	ret = kstrtouint_from_user(user_buf, count, 0, &mail_count);
	if (ret) {
		dsp_err("Failed to get user parameter(%d)\n", ret);
		goto p_err;
	}

	if (!mail_count) {
		ret = -EINVAL;
		dsp_err("mail count must be bigger than zero\n");
		goto p_err;
	}

	ret = dsp_device_open(debug->dspdev);
	if (ret)
		goto p_err_open;

	ret = dsp_device_start(debug->dspdev, 0);
	if (ret)
		goto p_err_start;

	__dsp_hw_debug_mailbox_test(debug->dspdev, mail_count);

	dsp_device_stop(debug->dspdev, 1);
	dsp_device_close(debug->dspdev);

	dsp_leave();
	return count;
p_err_start:
	dsp_device_close(debug->dspdev);
p_err_open:
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_mailbox_fops = {
	.open		= dsp_hw_debug_mailbox_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_mailbox_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_userdefined_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;

	dsp_enter();
	debug = file->private;
	sys = &debug->dspdev->system;

	seq_printf(file, "USERDEFINED : base_addr(%#x/%#x)/size(%u words)\n",
			(int)(sys->sfr_pa + DSP_SM_USERDEFINED_BASE),
			DSP_HW_BASE_ADDR + DSP_SM_USERDEFINED_BASE,
			DSP_SM_USERDEFINED_SIZE >> 2);

	seq_puts(file, "Command to change userdefined value\n");
	seq_puts(file, " echo {num} {0x(val)} > /d/dsp/hardware/userdefined\n");
	dsp_leave();
	return 0;
}

static int dsp_hw_debug_userdefined_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_userdefined_show,
			inode->i_private);
}

static ssize_t dsp_hw_debug_userdefined_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;
	char buf[30];
	ssize_t size;
	unsigned int num, val;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	dspdev = debug->dspdev;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%u 0x%x", &num, &val);
	if (ret != 2) {
		dsp_err("Failed to get userdefined parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	if (num >= DSP_SM_USERDEFINED_SIZE >> 2) {
		ret = -EINVAL;
		dsp_err("num(%u) of command is invalid(0 ~ %u)\n",
				num, (DSP_SM_USERDEFINED_SIZE >> 2) - 1);
		goto p_err;
	}

	mutex_lock(&dspdev->lock);
	if (dsp_device_power_active(dspdev)) {
		dsp_info("USERDEFINED(%u) is changed from %#x to %#x\n",
				num,
				dsp_ctrl_sm_readl(DSP_SM_USERDEFINED(num)),
				val);
		dsp_ctrl_sm_writel(DSP_SM_USERDEFINED(num), val);
	} else {
		dsp_err("Failed to change userdefined as power is off\n");
	}
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_userdefined_fops = {
	.open		= dsp_hw_debug_userdefined_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_userdefined_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_dump_value_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;

	dsp_enter();
	debug = file->private;
	sys = &debug->dspdev->system;

	seq_puts(file, "How to change dump_value\n");
	seq_puts(file, "- $echo ${dump_value} > /d/dsp/hardware/dump_value\n");
	seq_puts(file, "- ${dump_value} should be HEX like 0x0000\n");

	dsp_dump_print_status_user(file);

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_dump_value_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_dump_value_show,
			inode->i_private);
}

static ssize_t dsp_hw_debug_dump_value_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;
	char buf[30];
	ssize_t size;
	unsigned int dump_value;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	dspdev = debug->dspdev;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "0x%x", &dump_value);
	if (ret != 1) {
		dsp_err("Failed to get dump_value parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	mutex_lock(&dspdev->lock);
	if (dspdev->open_count) {
		dsp_err("device already opened (%u/%u)\n",
				dspdev->open_count, dspdev->start_count);
	} else {
		dsp_dump_set_value(dump_value);
		dsp_dump_print_value();
	}
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_dump_value_fops = {
	.open		= dsp_hw_debug_dump_value_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_dump_value_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_firmware_mode_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;

	dsp_enter();
	debug = file->private;
	sys = &debug->dspdev->system;

	if (sys->fw_postfix[0] == '\0')
		seq_puts(file, "Firmware mode : normal (none postfix)\n");
	else
		seq_printf(file, "Firmware mode : %s\n", sys->fw_postfix);

	seq_puts(file, "Command to control firmware mode\n");
	seq_puts(file, " echo {postfix} > /d/dsp/hardware/firmware_mode\n");
	seq_puts(file, "  : load [FW]_{postfix}.bin instead of [FW].bin\n");
	seq_puts(file, " echo > /d/dsp/hardware/firwmare_mode\n");
	seq_puts(file, "  : revert to loading [FW].bin\n");

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_firmware_mode_open(struct inode *inode,
		struct file *filp)
{
	return single_open(filp, dsp_hw_debug_firmware_mode_show,
			inode->i_private);
}

static ssize_t dsp_hw_debug_firmware_mode_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_system *sys;
	char command[32];
	ssize_t size;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	sys = &debug->dspdev->system;

	size = simple_write_to_buffer(command, sizeof(command), ppos,
			user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}

	if (size > sizeof(sys->fw_postfix)) {
		ret = -EINVAL;
		dsp_err("command size is invalid(%zu > %zu)\n",
				size + 1, sizeof(sys->fw_postfix));
		goto p_err;
	}

	command[size - 1] = '\0';
	if (sys->fw_postfix[0] == '\0')
		dsp_info("Firmware mode is changed [normal] -> [%s]\n",
				command);
	else
		dsp_info("Firmware mode is changed [%s] -> [%s]\n",
				sys->fw_postfix, command);

	memcpy(sys->fw_postfix, command, size);
	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_firmware_mode_fops = {
	.open		= dsp_hw_debug_firmware_mode_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_firmware_mode_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_bus_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_bus *bus;
	int idx;

	dsp_enter();
	debug = file->private;
	bus = &debug->dspdev->system.bus;

	seq_printf(file, "DSP mo scenario count[%u]\n", DSP_MO_SCENARIO_COUNT);
	for (idx = 0; idx < DSP_MO_SCENARIO_COUNT; ++idx)
		seq_printf(file, "[%d] [%32s] bts idx:%u, status:%d\n",
				idx, bus->scen[idx].name,
				bus->scen[idx].bts_scen_idx,
				bus->scen[idx].enabled);

	seq_puts(file, "Command to control DSP bus setting\n");
	seq_puts(file, " [mode 0] set mo setting\n");
	seq_puts(file, "  echo 0 {scen_name} > /d/dsp/hardware/bus\n");
	seq_puts(file, " [mode 1] unset mo setting\n");
	seq_puts(file, "  echo 1 {scen_name} > /d/dsp/hardware/bus\n");

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_bus_open(struct inode *inode,
		struct file *filp)
{
	return single_open(filp, dsp_hw_debug_bus_show, inode->i_private);
}

static ssize_t dsp_hw_debug_bus_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	struct dsp_bus *bus;
	char buf[30];
	ssize_t size;
	int mode;
	char bus_scen[DSP_BUS_SCENARIO_NAME_LEN];

	dsp_enter();
	file = filp->private_data;
	debug = file->private;
	bus = &debug->dspdev->system.bus;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%d %s", &mode, bus_scen);
	if (ret != 2) {
		dsp_err("Failed to get bus parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	switch (mode) {
	case 0:
		dsp_bus_mo_get(bus, bus_scen);
		break;
	case 1:
		dsp_bus_mo_put(bus, bus_scen);
		break;
	default:
		ret = -EINVAL;
		dsp_err("mode for bus setting is invalid(%d)\n", mode);
		goto p_err;
	}

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_bus_fops = {
	.open		= dsp_hw_debug_bus_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_bus_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_npu_test_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;
	struct dsp_device *dspdev;

	dsp_enter();
	debug = file->private;
	dspdev = debug->dspdev;

	mutex_lock(&dspdev->lock);
	seq_printf(file, "DSP boot init : %lx\n", dspdev->system.boot_init);
	mutex_unlock(&dspdev->lock);


	seq_puts(file, "Command to control CA5 Core1 for NPU\n");
	seq_puts(file, " on) echo 1 address(0x~) > /d/dsp/hardware/npu_test\n");
	seq_puts(file, "off) echo 0 > /d/dsp/hardware/npu_test\n");

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_npu_test_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_npu_test_show, inode->i_private);
}

static void __dsp_hw_debug_npu_enable(struct dsp_hw_debug *debug,
		dma_addr_t addr)
{
	int ret;
	struct dsp_device *dspdev;
	struct dsp_memory *mem;

	dsp_enter();
	dspdev = debug->dspdev;
	mem = &dspdev->system.memory;

	mutex_lock(&dspdev->lock);
	if (dspdev->system.boot_init & BIT(DSP_SYSTEM_NPU_INIT)) {
		mutex_unlock(&dspdev->lock);
		return;
	}
	mutex_unlock(&dspdev->lock);

	snprintf(debug->npu_fw.name, DSP_PRIV_MEM_NAME_LEN, "npu_fw");
	debug->npu_fw.size = PAGE_ALIGN(DSP_NPU_FW_SIZE);
	debug->npu_fw.flags = 0;
	debug->npu_fw.dir = DMA_TO_DEVICE;
	debug->npu_fw.kmap = true;
	debug->npu_fw.fixed_iova = true;
	debug->npu_fw.iova = addr;

	ret = dsp_memory_ion_alloc(mem, &debug->npu_fw);
	if (ret)
		return;

	ret = dsp_binary_load(DSP_NPU_FW_NAME, NULL, DSP_FW_EXTENSION,
			debug->npu_fw.kvaddr, debug->npu_fw.size,
			&debug->npu_fw.used_size);
	if (ret)
		goto p_err_free;

	mutex_lock(&dspdev->lock);
	ret = dsp_device_power_on(dspdev, 0);
	mutex_unlock(&dspdev->lock);
	if (ret)
		goto p_err_free;

	ret = dsp_npu_release(true, debug->npu_fw.iova);
	if (ret)
		goto p_err_power_off;

	dsp_ctrl_init(&dspdev->system.ctrl);
	dsp_dump_ctrl();

	dsp_leave();
	return;
p_err_power_off:
	mutex_lock(&dspdev->lock);
	dsp_device_power_off(dspdev);
	mutex_unlock(&dspdev->lock);
p_err_free:
	dsp_memory_ion_free(mem, &debug->npu_fw);
}

static void __dsp_hw_debug_npu_disable(struct dsp_hw_debug *debug)
{
	struct dsp_device *dspdev;
	struct dsp_memory *mem;

	dsp_enter();
	dspdev = debug->dspdev;
	mem = &dspdev->system.memory;

	mutex_lock(&dspdev->lock);
	if (!(dspdev->system.boot_init & BIT(DSP_SYSTEM_NPU_INIT))) {
		mutex_unlock(&dspdev->lock);
		return;
	}
	mutex_unlock(&dspdev->lock);

	dsp_npu_release(false, 0);

	mutex_lock(&dspdev->lock);
	dsp_device_power_off(dspdev);
	mutex_unlock(&dspdev->lock);

	dsp_memory_ion_free(mem, &debug->npu_fw);
	dsp_leave();
}

static ssize_t dsp_hw_debug_npu_test_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	char command[16];
	ssize_t size;
	unsigned int enable, fw_addr;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;

	size = simple_write_to_buffer(command, sizeof(command), ppos,
			user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	command[size - 1] = '\0';

	ret = sscanf(command, "%d 0x%x\n", &enable, &fw_addr);
	if (ret != 2 && ret != 1) {
		ret = -EINVAL;
		dsp_err("command[%s] about npu is invalid\n", command);
		goto p_err;
	}

	if (enable)
		__dsp_hw_debug_npu_enable(debug, fw_addr);
	else
		__dsp_hw_debug_npu_disable(debug);

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_npu_test_fops = {
	.open		= dsp_hw_debug_npu_test_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_npu_test_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int dsp_hw_debug_test_show(struct seq_file *file, void *unused)
{
	struct dsp_hw_debug *debug;

	dsp_enter();
	debug = file->private;

	seq_puts(file, "cmd: echo {test} > /d/dsp/hardware/test\n");

	dsp_leave();
	return 0;
}

static int dsp_hw_debug_test_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, dsp_hw_debug_test_show, inode->i_private);
}

static ssize_t dsp_hw_debug_test_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	struct seq_file *file;
	struct dsp_hw_debug *debug;
	char buf[30];
	ssize_t len;
	int test_id, repeat;

	dsp_enter();
	file = filp->private_data;
	debug = file->private;

	if (count > sizeof(buf)) {
		ret = -EINVAL;
		dsp_err("writing size(%zd) is larger than buffer\n", count);
		goto p_err;
	}

	len = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (len <= 0) {
		ret = -EINVAL;
		dsp_err("Failed to get user buf(%zu)\n", len);
		goto p_err;
	}
	buf[len - 1] = '\0';

	ret = sscanf(buf, "%d %d\n", &test_id, &repeat);
	if (ret != 2) {
		dsp_err("Failed to get user parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	dsp_leave();
	return count;
p_err:
	return ret;
}

static const struct file_operations dsp_hw_debug_test_fops = {
	.open		= dsp_hw_debug_test_open,
	.read		= seq_read,
	.write		= dsp_hw_debug_test_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static void dsp_hw_debug_log_print(struct timer_list *t)
{
	int ret;
	struct dsp_hw_debug_log *log;
	char line[DSP_HW_DEBUG_LOG_LINE_SIZE];
	int count = 0;

	dsp_enter();
	log = from_timer(log, t, timer);

	while (true) {
		/* Restricted from working too long */
		if (count == dsp_util_queue_read_count(log->queue))
			break;

		if (dsp_util_queue_check_empty(log->queue))
			break;

		ret = dsp_util_queue_dequeue(log->queue, line,
				DSP_HW_DEBUG_LOG_LINE_SIZE);
		if (ret)
			break;

		line[DSP_HW_DEBUG_LOG_LINE_SIZE - 2] = '\n';
		line[DSP_HW_DEBUG_LOG_LINE_SIZE - 1] = '\0';

		dsp_info("[timer(%4d)] %s", readl(&log->queue->front), line);
		count++;
	}

	mod_timer(&log->timer,
			jiffies + msecs_to_jiffies(DSP_HW_DEBUG_LOG_TIME));
	dsp_leave();
}

void dsp_hw_debug_log_flush(struct dsp_hw_debug *debug)
{
	int ret;
	struct dsp_hw_debug_log *log;
	char line[DSP_HW_DEBUG_LOG_LINE_SIZE];

	dsp_enter();
	log = debug->log;

	if (!log->queue)
		return;

	while (true) {
		if (dsp_util_queue_check_empty(log->queue))
			break;

		ret = dsp_util_queue_dequeue(log->queue, line,
				DSP_HW_DEBUG_LOG_LINE_SIZE);
		if (ret)
			break;

		line[DSP_HW_DEBUG_LOG_LINE_SIZE - 2] = '\n';
		line[DSP_HW_DEBUG_LOG_LINE_SIZE - 1] = '\0';

		dsp_info("[flush(%4d)] %s", readl(&log->queue->front), line);
	}
	dsp_leave();
}

int dsp_hw_debug_log_start(struct dsp_hw_debug *debug)
{
	struct dsp_hw_debug_log *log;
	struct dsp_system *sys;
	struct dsp_priv_mem *log_mem;

	dsp_enter();
	log = debug->log;
	sys = &debug->dspdev->system;
	log_mem = &sys->memory.priv_mem[DSP_PRIV_MEM_FW_LOG];

	log->queue = sys->sfr + DSP_SM_RESERVED(LOG_QUEUE);
	dsp_util_queue_init(log->queue, DSP_HW_DEBUG_LOG_LINE_SIZE,
			log_mem->size, (unsigned int)log_mem->iova,
			(unsigned long long)log_mem->kvaddr);

	mod_timer(&log->timer,
			jiffies + msecs_to_jiffies(DSP_HW_DEBUG_LOG_TIME));
	dsp_leave();
	return 0;
}

static int dsp_hw_debug_write_log_binary(struct dsp_hw_debug *debug)
{
	int ret;
	struct dsp_hw_debug_log *log;
	char fname[32], head[32], line[256];
	mm_segment_t old_fs;
	int fd, write_size;
	struct file *fp;
	loff_t pos = 0;
	unsigned int front, rear;
	unsigned int data_size, data_count, idx;
	unsigned long long kva_low, kva_high;
	void *kva;

	dsp_enter();
	log = debug->log;
	snprintf(fname, sizeof(fname), "%s", DSP_HW_DEBUG_LOG_FILE_NAME);

	if (!current->fs) {
		dsp_warn("Failed to write %s as fs is invalid\n", fname);
		return -ESRCH;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = ksys_open(fname, O_RDWR | O_CREAT | O_TRUNC, 0640);
	if (fd < 0) {
		ret = fd;
		dsp_err("open(%s) is fail(%d)\n", fname, ret);
		goto p_err;
	}

	fp = fget(fd);
	if (!fp) {
		ret = -EFAULT;
		dsp_err("fget(%s) is fail\n", fname);
		goto p_err;
	}

	front = readl(&log->queue->front);
	rear = readl(&log->queue->rear);
	data_size = readl(&log->queue->data_size);
	data_count = readl(&log->queue->data_count);
	kva_low = readl(&log->queue->kva_low);
	kva_high = readl(&log->queue->kva_high);
	kva = (void *)((kva_high << 32) | kva_low);

	write_size = snprintf(head, sizeof(head), "%d/%d/%d/%d\n",
			front, rear, data_size, data_count);

	vfs_write(fp, head, write_size, &pos);

	for (idx = 0; idx < data_count; ++idx) {
		write_size = snprintf(line, sizeof(line), "[%6d]%s",
				idx, (char *)(kva + (data_size * idx)));
		if (write_size < 9)
			continue;

		if (write_size >= (data_size + 8))
			write_size = (data_size + 8) - 1;

		if (line[write_size - 1] != '\n')
			line[write_size - 1] = '\n';

		if (line[write_size] != '\0')
			line[write_size] = '\0';

		vfs_write(fp, line, write_size, &pos);
	}

	fput(fp);
	ksys_close(fd);
	set_fs(old_fs);

	dsp_info("%s was created for debugging\n", fname);
	dsp_leave();
	return 0;
p_err:
	set_fs(old_fs);
	return ret;
}

int dsp_hw_debug_log_stop(struct dsp_hw_debug *debug)
{
	dsp_enter();
	del_timer_sync(&debug->log->timer);
	dsp_hw_debug_log_flush(debug);
	if (debug->log->log_file)
		dsp_hw_debug_write_log_binary(debug);
	debug->log->queue = NULL;
	dsp_leave();
	return 0;
}

int dsp_hw_debug_open(struct dsp_hw_debug *debug)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_debug_close(struct dsp_hw_debug *debug)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static void __dsp_hw_debug_log_init(struct dsp_hw_debug *debug)
{
	struct dsp_hw_debug_log *log;

	dsp_enter();
	log = debug->log;

	timer_setup(&log->timer, dsp_hw_debug_log_print, 0);
	dsp_leave();
}

int dsp_hw_debug_probe(struct dsp_device *dspdev)
{
	int ret;
	struct dsp_hw_debug *debug;

	dsp_enter();
	debug = &dspdev->system.debug;
	debug->dspdev = dspdev;

	debug->root = debugfs_create_dir("hardware", dspdev->debug.root);
	if (!debug->root) {
		ret = -EFAULT;
		dsp_err("Failed to create hw debug root file\n");
		goto p_err_root;
	}

	debug->power = debugfs_create_file("power", 0640, debug->root, debug,
			&dsp_hw_debug_power_fops);
	if (!debug->power)
		dsp_warn("Failed to create power debugfs file\n");

	debug->clk = debugfs_create_file("clk", 0640, debug->root, debug,
			&dsp_hw_debug_clk_fops);
	if (!debug->clk)
		dsp_warn("Failed to create clk debugfs file\n");

	debug->devfreq = debugfs_create_file("devfreq", 0640, debug->root,
			debug, &dsp_hw_debug_devfreq_fops);
	if (!debug->devfreq)
		dsp_warn("Failed to create devfreq debugfs file\n");

	debug->sfr = debugfs_create_file("sfr", 0640, debug->root, debug,
			&dsp_hw_debug_sfr_fops);
	if (!debug->sfr)
		dsp_warn("Failed to create sfr debugfs file\n");

	debug->mem = debugfs_create_file("mem", 0640, debug->root, debug,
			&dsp_hw_debug_mem_fops);
	if (!debug->mem)
		dsp_warn("Failed to create mem debugfs file\n");

	debug->fw_log = debugfs_create_file("fw_log", 0640, debug->root, debug,
			&dsp_hw_debug_fw_log_fops);
	if (!debug->fw_log)
		dsp_warn("Failed to create fw_log debugfs file\n");

	debug->wait_time = debugfs_create_file("wait_time", 0640, debug->root,
			debug, &dsp_hw_debug_wait_time_fops);
	if (!debug->wait_time)
		dsp_warn("Failed to create wait_time debugfs file\n");

	debug->layer_range = debugfs_create_file("layer_range", 0640,
			debug->root, debug, &dsp_hw_debug_layer_range_fops);
	if (!debug->layer_range)
		dsp_warn("Failed to create layer_range debugfs file\n");

	debug->mailbox = debugfs_create_file("mailbox", 0640, debug->root,
			debug, &dsp_hw_debug_mailbox_fops);
	if (!debug->mailbox)
		dsp_warn("Failed to create mailbox debugfs file\n");

	debug->userdefined = debugfs_create_file("userdefined", 0640,
			debug->root, debug, &dsp_hw_debug_userdefined_fops);
	if (!debug->userdefined)
		dsp_warn("Failed to create userdefined debugfs file\n");

	debug->dump_value = debugfs_create_file("dump_value", 0640,
			debug->root, debug, &dsp_hw_debug_dump_value_fops);
	if (!debug->dump_value)
		dsp_warn("Failed to create dump_value debugfs file\n");

	debug->firmware_mode = debugfs_create_file("firmware_mode", 0640,
			debug->root, debug, &dsp_hw_debug_firmware_mode_fops);
	if (!debug->firmware_mode)
		dsp_warn("Failed to create firmware_mode debugfs file\n");

	debug->bus = debugfs_create_file("bus", 0640, debug->root, debug,
			&dsp_hw_debug_bus_fops);
	if (!debug->bus)
		dsp_warn("Failed to create bus debugfs file\n");

	debug->npu_test = debugfs_create_file("npu_test", 0640, debug->root,
			debug, &dsp_hw_debug_npu_test_fops);
	if (!debug->npu_test)
		dsp_warn("Failed to create npu_test debugfs file\n");

	debug->test = debugfs_create_file("test", 0640, debug->root, debug,
			&dsp_hw_debug_test_fops);
	if (!debug->test)
		dsp_warn("Failed to create test debugfs file\n");

	debug->log = kzalloc(sizeof(*debug->log), GFP_KERNEL);
	if (debug->log)
		__dsp_hw_debug_log_init(debug);
	else
		dsp_warn("Failed to alloc dsp_hw_debug_log\n");

	dsp_leave();
	return 0;
p_err_root:
	return ret;
}

void dsp_hw_debug_remove(struct dsp_hw_debug *debug)
{
	dsp_enter();
	kfree(debug->log);
	debugfs_remove_recursive(debug->root);
	dsp_leave();
}
