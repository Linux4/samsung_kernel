// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[lift_to_wake_hub] " fmt

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>

#include <hwmsensor.h>
#include <sensors_io.h>
#include "lift_to_wake.h"
#include "situation.h"

#include <hwmsen_helper.h>

#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "include/scp.h"

static int lift_to_wake_get_data(int *probability, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_LIFT_TO_WAKE, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp = data.time_stamp;
	*probability = data.gesture_data_t.probability;
	return 0;
}
static int lift_to_wake_open_report_data(int open)
{
	int ret = 0;

	pr_debug("%s : enable=%d\n", __func__, open);
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	if (open == 1)
		ret = sensor_set_delay_to_hub(ID_LIFT_TO_WAKE, 120);
#elif defined CONFIG_NANOHUB

#else

#endif
	ret = sensor_enable_to_hub(ID_LIFT_TO_WAKE, open);
	return ret;
}
static int lift_to_wake_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_LIFT_TO_WAKE,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}
//+P230620-03457, liuling3.wt,ADD, 2023/07/07, add flush function
static int lift_to_wake_flush(void)
{
    return sensor_flush_to_hub(ID_LIFT_TO_WAKE);
}
//-P230620-03457, liuling3.wt,ADD, 2023/07/07, add flush function
static int lift_to_wake_recv_data(struct data_unit_t *event,
	void *reserved)
{
	int err = 0;
//+P230620-03457, liuling3.wt,ADD, 2023/07/07, add flush function
	if (event->flush_action == FLUSH_ACTION) {
        err = situation_flush_report(ID_LIFT_TO_WAKE);
		pr_debug("lift_to_wake do not support flush\n");
    } else if (event->flush_action == DATA_ACTION)
//-P230620-03457, liuling3.wt,ADD, 2023/07/07, add flush function
		err = situation_notify_t(ID_LIFT_TO_WAKE,
			(int64_t)event->time_stamp);
	return err;
}

static int lift_to_wake_hub_local_init(void)
{
	struct situation_control_path ctl = {0};
	struct situation_data_path data = {0};
	int err = 0;

	ctl.open_report_data = lift_to_wake_open_report_data;
	ctl.batch = lift_to_wake_batch;
//+P230620-03457, liuling3.wt,ADD, 2023/07/07, add flush function
    ctl.flush = lift_to_wake_flush;
//-P230620-03457, liuling3.wt,ADD, 2023/07/07, add flush function
	ctl.is_support_wake_lock = true;
	err = situation_register_control_path(&ctl, ID_LIFT_TO_WAKE);
	if (err) {
		pr_err("register lift_to_wake control path err\n");
		goto exit;
	}

	data.get_data = lift_to_wake_get_data;
	err = situation_register_data_path(&data, ID_LIFT_TO_WAKE);
	if (err) {
		pr_err("register lift_to_wake data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_LIFT_TO_WAKE,
		lift_to_wake_recv_data);
	if (err) {
		pr_err("SCP_sensorHub_data_registration fail!!\n");
		goto exit_create_attr_failed;
	}
	return 0;
exit:
exit_create_attr_failed:
	return -1;
}
static int lift_to_wake_hub_local_uninit(void)
{
	return 0;
}

static struct situation_init_info lift_to_wake_hub_init_info = {
	.name = "lift_to_wake_hub",
	.init = lift_to_wake_hub_local_init,
	.uninit = lift_to_wake_hub_local_uninit,
};

static int __init lift_to_wake_hub_init(void)
{
	situation_driver_add(&lift_to_wake_hub_init_info, ID_LIFT_TO_WAKE);
	return 0;
}

static void __exit lift_to_wake_hub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(lift_to_wake_hub_init);
module_exit(lift_to_wake_hub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GLANCE_GESTURE_HUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");

