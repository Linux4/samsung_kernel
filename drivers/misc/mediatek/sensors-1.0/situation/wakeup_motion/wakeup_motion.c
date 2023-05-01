// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[wakeup_motion] " fmt

#include <hwmsensor.h>
#include "wakeup_motion.h"
#include <situation.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "include/scp.h"

static struct situation_init_info wakeup_motion_init_info;

static int wakeup_motion_get_data(int *probability, int *status)
{
    int err = 0;
    struct data_unit_t data;
    uint64_t time_stamp = 0;

    err = sensor_get_data_from_hub(ID_WAKE_UP_MOTION, &data);
    if (err < 0) {
        pr_err("sensor_get_data_from_hub fail!!\n");
        return -1;
    }
    time_stamp        = data.time_stamp;
    *probability    = data.gesture_data_t.probability;
    return 0;
}
static int wakeup_motion_open_report_data(int open)
{
    int ret = 0;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    if (open == 1)
        ret = sensor_set_delay_to_hub(ID_WAKE_UP_MOTION, 120);
#elif defined CONFIG_NANOHUB

#else

#endif
    ret = sensor_enable_to_hub(ID_WAKE_UP_MOTION, open);
    return ret;
}
static int wakeup_motion_batch(int flag,
    int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
    return sensor_batch_to_hub(ID_WAKE_UP_MOTION,
        flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int wakeup_motion_flush(void)
{
    pr_err("wakeup_motion_flush\n");
    return sensor_flush_to_hub(ID_WAKE_UP_MOTION);
}

static int wakeup_motion_recv_data(struct data_unit_t *event, void *reserved)
{
    int err = 0;

    pr_err("wakeup_motion_recv_data, %d\n", event->flush_action);

    if (event->flush_action == FLUSH_ACTION)
        err = situation_flush_report(ID_WAKE_UP_MOTION);
    else if (event->flush_action == DATA_ACTION)
        err = situation_data_report_t(ID_WAKE_UP_MOTION,
            event->wakeup_event.state, (int64_t)event->time_stamp);
    return err;
}

static int wakeup_motion_local_init(void)
{
    struct situation_control_path ctl = {0};
    struct situation_data_path data = {0};
    int err = 0;

    ctl.open_report_data = wakeup_motion_open_report_data;
    ctl.batch = wakeup_motion_batch;
    ctl.flush = wakeup_motion_flush;
    ctl.is_support_wake_lock = true;
    ctl.is_support_batch = false;
    err = situation_register_control_path(&ctl, ID_WAKE_UP_MOTION);
    if (err) {
        pr_err("register wakeup_motion control path err\n");
        goto exit;
    }

    data.get_data = wakeup_motion_get_data;
    err = situation_register_data_path(&data, ID_WAKE_UP_MOTION);
    if (err) {
        pr_err("register wakeup_motion data path err\n");
        goto exit;
    }
    err = scp_sensorHub_data_registration(ID_WAKE_UP_MOTION,
        wakeup_motion_recv_data);
    if (err) {
        pr_err("SCP_sensorHub_data_registration fail!!\n");
        goto exit;
    }
    return 0;
exit:
    return -1;
}
static int wakeup_motion_local_uninit(void)
{
    return 0;
}

static struct situation_init_info wakeup_motion_init_info = {
    .name = "wakeup_motion_hub",
    .init = wakeup_motion_local_init,
    .uninit = wakeup_motion_local_uninit,
};

static int __init wakeup_motion_init(void)
{
    situation_driver_add(&wakeup_motion_init_info, ID_WAKE_UP_MOTION);
    return 0;
}

static void __exit wakeup_motion_exit(void)
{
    pr_debug("%s\n", __func__);
}

module_init(wakeup_motion_init);
module_exit(wakeup_motion_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WAKEUP MOTION driver");
