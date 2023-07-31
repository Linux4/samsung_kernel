/*
 *  drivers/misc/sec_param.c
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/delay.h>

#include <linux/qcom/sec_debug.h>
#include <linux/qcom/sec_debug_summary.h>
#include <linux/qcom/sec_debug_partition.h>

static DEFINE_MUTEX(debug_partition_mutex);

/* single global instance */
struct debug_partition_data_s sched_debug_data;

static int driver_initialized;
static struct delayed_work dbg_partition_notify_work;
static struct workqueue_struct *dbg_part_wq;
static struct delayed_work ap_health_work;
static ap_health_t ap_health_data;
static int in_panic;
static bool init_ap_health_data(void);
static DEFINE_MUTEX(ap_health_work_lock);

static void debug_partition_operation(struct work_struct *work)
{
	/* Read from PARAM(parameter) partition  */
	struct file *filp;
	mm_segment_t fs;
	int ret = true;
	struct debug_partition_data_s *sched_data =
		container_of(work, struct debug_partition_data_s, debug_partition_work);

	int flag = (sched_data->direction == PARTITION_WR) ? (O_RDWR | O_SYNC) : O_RDONLY;

	if (!sched_data->value) {
		pr_err("%s %p %x %d %d - value is NULL!!\n", __func__,
			sched_data->value, sched_data->offset,
			sched_data->size, sched_data->direction);
		sched_data->error = -1;
		complete(&sched_data->work);
		return;
	}

	fs = get_fs();
	set_fs(get_ds());

	sched_data->error = 0;

	filp = filp_open(DEBUG_PARTITION_NAME, flag, 0);

	if (IS_ERR(filp)) {
		pr_err("%s: filp_open failed. (%ld)\n", __func__, PTR_ERR(filp));
		sched_data->error = -2;
		set_fs(fs);
		complete(&sched_data->work);
		return;
	}

	ret = filp->f_op->llseek(filp, sched_data->offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s FAIL LLSEEK\n", __func__);
		sched_data->error = -3;
		goto param_sec_debug_out;
	}

	if (sched_data->direction == PARTITION_RD)
		vfs_read(filp, (char __user *)sched_data->value, sched_data->size, &filp->f_pos);
	else if (sched_data->direction == PARTITION_WR)
		vfs_write(filp, (char __user *)sched_data->value, sched_data->size, &filp->f_pos);

param_sec_debug_out:
	set_fs(fs);
	filp_close(filp, NULL);
	complete(&sched_data->work);
	return;
}

static void ap_health_work_write_fn(struct work_struct *work)
{
	struct file *filp;
	mm_segment_t fs;
	int ret = true;
	unsigned long delay = 5 * HZ;

	pr_info("%s - start.\n", __func__);
	if (!mutex_trylock(&ap_health_work_lock)) {
		pr_err("%s - already locked.\n", __func__);
		delay = 2 * HZ;
		goto occupied_retry;
	}

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(DEBUG_PARTITION_NAME, (O_RDWR | O_SYNC), 0);

	if (IS_ERR(filp)) {
		pr_err("%s: filp_open failed. (%ld)\n", __func__, PTR_ERR(filp));
		goto openfail_retry;
	}

	ret = filp->f_op->llseek(filp, SEC_DEBUG_AP_HEALTH_OFFSET, SEEK_SET);
	if (ret < 0) {
		pr_err("%s FAIL LLSEEK\n", __func__);
		ret = false;
		goto seekfail_retry;
	}

	vfs_write(filp, (char __user *)&ap_health_data, sizeof(ap_health_t), &filp->f_pos);

	if (--ap_health_data.header.need_write) {
		goto remained;
	}

	filp_close(filp, NULL);
	set_fs(fs);

	mutex_unlock(&ap_health_work_lock);
	pr_info("%s - end.\n", __func__);
	return;

remained:
seekfail_retry:
	filp_close(filp, NULL);
openfail_retry:
	set_fs(fs);
	mutex_unlock(&ap_health_work_lock);
occupied_retry:
	queue_delayed_work(dbg_part_wq, &ap_health_work, delay);
	pr_info("%s - end, will retry, wr(%u).\n", __func__,
		ap_health_data.header.need_write);
	return;
}

bool check_magic_data(void)
{
	int ret = true, retry = 0;
	static int checked_magic = 0;
	struct debug_reset_header partition_header = {0,};

	if (checked_magic)
		return true;

	pr_info("%s\n",__func__);

	do {
		if (retry++) {
			pr_err("%s : will retry...\n",__func__);
			msleep(1000);
		}

		mutex_lock(&debug_partition_mutex);

		sched_debug_data.value = &partition_header;
		sched_debug_data.offset = SEC_DEBUG_RESET_HEADER_OFFSET;
		sched_debug_data.size = sizeof(struct debug_reset_header);
		sched_debug_data.direction = PARTITION_RD;

		schedule_work(&sched_debug_data.debug_partition_work);
		wait_for_completion(&sched_debug_data.work);

		mutex_unlock(&debug_partition_mutex);

	} while (sched_debug_data.error);


	if (partition_header.magic != DEBUG_PARTITION_MAGIC) {
		init_debug_partition();
	}

	checked_magic = 1;

	return ret;
}

bool init_debug_partition(void)
{
	int ret = true, retry = 0;
	struct debug_reset_header init_reset_header;

	pr_info("%s start\n",__func__);

	/*++ add here need init data ++*/
	init_ap_health_data();
	/*-- add here need init data --*/

	do {
		if (retry++) {
			pr_err("%s : will retry...\n",__func__);
			msleep(1000);
		}
		mutex_lock(&debug_partition_mutex);

		memset(&init_reset_header, 0, sizeof(struct debug_reset_header));
		init_reset_header.magic = DEBUG_PARTITION_MAGIC;

		sched_debug_data.value = &init_reset_header;
		sched_debug_data.offset = SEC_DEBUG_RESET_HEADER_OFFSET;
		sched_debug_data.size = sizeof(struct debug_reset_header);
		sched_debug_data.direction = PARTITION_WR;

		schedule_work(&sched_debug_data.debug_partition_work);
		wait_for_completion(&sched_debug_data.work);

		mutex_unlock(&debug_partition_mutex);
	} while (sched_debug_data.error);

	pr_info("%s end\n",__func__);

	return ret;
}

bool read_debug_partition(enum debug_partition_index index, void *value)
{
	int ret = true;
	ret = check_magic_data();
	if (!ret)
		return ret;

	switch (index) {
		case debug_index_reset_ex_info:
			mutex_lock(&debug_partition_mutex);
			sched_debug_data.value = value;
			sched_debug_data.offset = SEC_DEBUG_EXTRA_INFO_OFFSET;
			sched_debug_data.size = SEC_DEBUG_EX_INFO_SIZE;
			sched_debug_data.direction = PARTITION_RD;
			schedule_work(&sched_debug_data.debug_partition_work);
			wait_for_completion(&sched_debug_data.work);
			mutex_unlock(&debug_partition_mutex);
			break;
		case debug_index_reset_klog_info:
		case debug_index_reset_summary_info:
		case debug_index_reset_tzlog_info:
			mutex_lock(&debug_partition_mutex);
			sched_debug_data.value = value;
			sched_debug_data.offset = SEC_DEBUG_RESET_HEADER_OFFSET;
			sched_debug_data.size = sizeof(struct debug_reset_header);
			sched_debug_data.direction = PARTITION_RD;
			schedule_work(&sched_debug_data.debug_partition_work);
			wait_for_completion(&sched_debug_data.work);
			mutex_unlock(&debug_partition_mutex);
			break;
		case debug_index_reset_summary:
			mutex_lock(&debug_partition_mutex);
			sched_debug_data.value = value;
			sched_debug_data.offset = SEC_DEBUG_RESET_SUMMARY_OFFSET;
			sched_debug_data.size = SEC_DEBUG_RESET_SUMMARY_SIZE;
			sched_debug_data.direction = PARTITION_RD;
			schedule_work(&sched_debug_data.debug_partition_work);
			wait_for_completion(&sched_debug_data.work);
			mutex_unlock(&debug_partition_mutex);
			break;
		case debug_index_reset_klog:
			mutex_lock(&debug_partition_mutex);
			sched_debug_data.value = value;
			sched_debug_data.offset = SEC_DEBUG_RESET_KLOG_OFFSET;
			sched_debug_data.size = SEC_DEBUG_RESET_KLOG_SIZE;
			sched_debug_data.direction = PARTITION_RD;
			schedule_work(&sched_debug_data.debug_partition_work);
			wait_for_completion(&sched_debug_data.work);
			mutex_unlock(&debug_partition_mutex);
			break;
		case debug_index_reset_tzlog:
			mutex_lock(&debug_partition_mutex);
			sched_debug_data.value = value;
			sched_debug_data.offset = SEC_DEBUG_RESET_TZLOG_OFFSET;
			sched_debug_data.size = SEC_DEBUG_RESET_TZLOG_SIZE;
			sched_debug_data.direction = PARTITION_RD;
			schedule_work(&sched_debug_data.debug_partition_work);
			wait_for_completion(&sched_debug_data.work);
			mutex_unlock(&debug_partition_mutex);
			break;
		case debug_index_ap_health:
			mutex_lock(&debug_partition_mutex);
			sched_debug_data.value = value;
			sched_debug_data.offset = SEC_DEBUG_AP_HEALTH_OFFSET;
			sched_debug_data.size = SEC_DEBUG_AP_HEALTH_SIZE;
			sched_debug_data.direction = PARTITION_RD;
			schedule_work(&sched_debug_data.debug_partition_work);
			wait_for_completion(&sched_debug_data.work);
			mutex_unlock(&debug_partition_mutex);
			break;
		case debug_index_reset_extrc_info:
			mutex_lock(&debug_partition_mutex);
			sched_debug_data.value = value;
			sched_debug_data.offset = SEC_DEBUG_RESET_EXTRC_OFFSET;
			sched_debug_data.size = SEC_DEBUG_RESET_EXTRC_SIZE;
			sched_debug_data.direction = PARTITION_RD;
			schedule_work(&sched_debug_data.debug_partition_work);
			wait_for_completion(&sched_debug_data.work);
			mutex_unlock(&debug_partition_mutex);
			break;
		case debug_index_lcd_debug_info:
			mutex_lock(&debug_partition_mutex);
			sched_debug_data.value = value;
			sched_debug_data.offset = SEC_DEBUG_LCD_DEBUG_OFFSET;
			sched_debug_data.size = sizeof(struct lcd_debug_t);
			sched_debug_data.direction = PARTITION_RD;
			schedule_work(&sched_debug_data.debug_partition_work);
			wait_for_completion(&sched_debug_data.work);
			mutex_unlock(&debug_partition_mutex);
			break;
		case debug_index_modem_info:
			mutex_lock(&debug_partition_mutex);
			sched_debug_data.value = value;
			sched_debug_data.offset = SEC_DEBUG_RESET_MODEM_OFFSET;
			sched_debug_data.size = sizeof(struct sec_debug_summary_data_modem);
			sched_debug_data.direction = PARTITION_RD;
			schedule_work(&sched_debug_data.debug_partition_work);
			wait_for_completion(&sched_debug_data.work);
			mutex_unlock(&debug_partition_mutex);
			break;
		default:
			return false;
	}

	return true;
}
EXPORT_SYMBOL(read_debug_partition);

bool write_debug_partition(enum debug_partition_index index, void *value)
{
	int ret = true;

	ret = check_magic_data();
	if (!ret)
		return ret;

	switch (index) {
		case debug_index_reset_klog_info:
		case debug_index_reset_summary_info:
		case debug_index_reset_tzlog_info:
			mutex_lock(&debug_partition_mutex);
			sched_debug_data.value = (struct debug_reset_header *)value;
			sched_debug_data.offset = SEC_DEBUG_RESET_HEADER_OFFSET;
			sched_debug_data.size = sizeof(struct debug_reset_header);
			sched_debug_data.direction = PARTITION_WR;
			schedule_work(&sched_debug_data.debug_partition_work);
			wait_for_completion(&sched_debug_data.work);
			mutex_unlock(&debug_partition_mutex);
			break;
		case debug_index_reset_ex_info:
		case debug_index_reset_summary:
			// do nothing.
			break;
		case debug_index_lcd_debug_info:
			mutex_lock(&debug_partition_mutex);
			sched_debug_data.value = (struct lcd_debug_t *)value;
			sched_debug_data.offset = SEC_DEBUG_LCD_DEBUG_OFFSET;
			sched_debug_data.size = sizeof(struct lcd_debug_t);
			sched_debug_data.direction = PARTITION_WR;
			schedule_work(&sched_debug_data.debug_partition_work);
			wait_for_completion(&sched_debug_data.work);
			mutex_unlock(&debug_partition_mutex);
			break;
		default:
			return false;
	}

	return ret;
}
EXPORT_SYMBOL(write_debug_partition);

static bool init_lcd_debug_data(void)
{
	int ret = true, retry = 0;
	struct lcd_debug_t lcd_debug;

	pr_info("%s start\n", __func__);

	memset((void *)&lcd_debug, 0, sizeof(struct lcd_debug_t));

	pr_info("%s lcd_debug size[%ld]\n", __func__, sizeof(struct lcd_debug_t));

	do {
		if (retry++) {
			pr_err("%s : will retry...\n", __func__);
			msleep(1000);
		}

		mutex_lock(&debug_partition_mutex);

		sched_debug_data.value = &lcd_debug;
		sched_debug_data.offset = SEC_DEBUG_LCD_DEBUG_OFFSET;
		sched_debug_data.size = sizeof(struct lcd_debug_t);
		sched_debug_data.direction = PARTITION_WR;

		schedule_work(&sched_debug_data.debug_partition_work);
		wait_for_completion(&sched_debug_data.work);

		mutex_unlock(&debug_partition_mutex);
	} while (sched_debug_data.error);

	pr_info("%s end\n",__func__);

	return ret;
}

static bool init_ap_health_data(void)
{
	int ret = true, retry = 0;

	pr_info("%s start\n",__func__);

	memset((void *)&ap_health_data, 0, sizeof(ap_health_t));

	ap_health_data.header.magic = AP_HEALTH_MAGIC;
	ap_health_data.header.version = AP_HEALTH_VER;
	ap_health_data.header.size = sizeof(ap_health_t);
	ap_health_data.spare_magic1 = AP_HEALTH_MAGIC;
	ap_health_data.spare_magic2 = AP_HEALTH_MAGIC;
	ap_health_data.spare_magic3 = AP_HEALTH_MAGIC;

	pr_info("%s ap_health size[%ld]\n",__func__, sizeof(ap_health_t));

	do {
		if (retry++) {
			pr_err("%s : will retry...\n",__func__);
			msleep(1000);
		}

		mutex_lock(&debug_partition_mutex);

		sched_debug_data.value = &ap_health_data;
		sched_debug_data.offset = SEC_DEBUG_AP_HEALTH_OFFSET;
		sched_debug_data.size = sizeof(ap_health_t);
		sched_debug_data.direction = PARTITION_WR;

		schedule_work(&sched_debug_data.debug_partition_work);
		wait_for_completion(&sched_debug_data.work);

		mutex_unlock(&debug_partition_mutex);
	} while (sched_debug_data.error);

	pr_info("%s end\n",__func__);

	return ret;
}

static int ap_health_initialized;

ap_health_t* ap_health_data_read(void)
{
	if (!driver_initialized)
		return NULL;

	if (ap_health_initialized)
		goto out;

	read_debug_partition(debug_index_ap_health, (void *)&ap_health_data);

	if (ap_health_data.header.magic != AP_HEALTH_MAGIC ||
		ap_health_data.header.version != AP_HEALTH_VER ||
		ap_health_data.header.size != sizeof(ap_health_t) ||
		ap_health_data.spare_magic1 != AP_HEALTH_MAGIC ||
		ap_health_data.spare_magic2 != AP_HEALTH_MAGIC ||
		ap_health_data.spare_magic3 != AP_HEALTH_MAGIC ||
		is_boot_recovery == 1) {
		init_ap_health_data();
		init_lcd_debug_data();
	}

	ap_health_initialized = 1;
out:
	return &ap_health_data;
}
EXPORT_SYMBOL(ap_health_data_read);

int ap_health_data_write(ap_health_t *data)
{
	if (!driver_initialized || !data || !ap_health_initialized)
		return -1;

	data->header.need_write++;

	if (!in_panic) {
		queue_delayed_work(dbg_part_wq, &ap_health_work, 0);
	}

	return 0;
}
EXPORT_SYMBOL(ap_health_data_write);

static BLOCKING_NOTIFIER_HEAD(dbg_partition_notifier_list);

int dbg_partition_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&dbg_partition_notifier_list, nb);
}
EXPORT_SYMBOL(dbg_partition_notifier_register);

static void debug_partition_do_notify(struct work_struct *work)
{
	blocking_notifier_call_chain(&dbg_partition_notifier_list, DBG_PART_DRV_INIT_DONE, NULL);
	return;
}

static int dbg_partition_panic_prepare(struct notifier_block *nb,
		unsigned long event, void *data)
{
	in_panic = 1;
	return NOTIFY_DONE;
}

static struct notifier_block dbg_partition_panic_notifier_block = {
	.notifier_call = dbg_partition_panic_prepare,
};

static int __init sec_debug_partition_init(void)
{
	pr_info("%s: start\n", __func__);

	sched_debug_data.offset = 0;
	sched_debug_data.direction = 0;
	sched_debug_data.size = 0;
	sched_debug_data.value = NULL;

	init_completion(&sched_debug_data.work);
	INIT_WORK(&sched_debug_data.debug_partition_work, debug_partition_operation);
	INIT_DELAYED_WORK(&dbg_partition_notify_work, debug_partition_do_notify);
	INIT_DELAYED_WORK(&ap_health_work, ap_health_work_write_fn);

	dbg_part_wq = create_singlethread_workqueue("glink_lbsrv");
	if (!dbg_part_wq) {
		pr_err("%s: fail to create dbg_part_wq!\n", __func__);
		return -EFAULT;
	}
	atomic_notifier_chain_register(&panic_notifier_list, &dbg_partition_panic_notifier_block);

	driver_initialized = 1;
	schedule_delayed_work(&dbg_partition_notify_work, 2 * HZ);
	pr_info("%s: end\n", __func__);

	return 0;
}

static void __exit sec_debug_partition_exit(void)
{
	driver_initialized = 0;
	cancel_work_sync(&sched_debug_data.debug_partition_work);
	cancel_delayed_work_sync(&dbg_partition_notify_work);
	pr_info("%s: exit\n", __func__);
}

module_init(sec_debug_partition_init);
module_exit(sec_debug_partition_exit);

