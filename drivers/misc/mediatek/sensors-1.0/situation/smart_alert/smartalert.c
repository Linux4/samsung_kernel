// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[smartalert] " fmt

#include <hwmsensor.h>
#include "smartalert.h"
#include <situation.h>
#include <SCP_sensorHub.h>
#include <linux/notifier.h>
#include "include/scp.h"

static struct situation_init_info smartalert_init_info;

static int smart_alert_get_data(int *probability, int *status)
{
    int err = 0;
    struct data_unit_t data;
    uint64_t time_stamp = 0;

    err = sensor_get_data_from_hub(ID_SMART_ALERT, &data);
    if (err < 0) {
        pr_err("sensor_get_data_from_hub fail!!\n");
        return -1;
    }
    time_stamp        = data.time_stamp;
    *probability    = data.gesture_data_t.probability;
    return 0;
}
static int smart_alert_open_report_data(int open)
{
    int ret = 0;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
    if (open == 1)
        ret = sensor_set_delay_to_hub(ID_SMART_ALERT, 120);
#elif defined CONFIG_NANOHUB

#else

#endif
    ret = sensor_enable_to_hub(ID_SMART_ALERT, open);
    return ret;
}
static int smart_alert_batch(int flag,
    int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
    return sensor_batch_to_hub(ID_SMART_ALERT,
        flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int smart_alert_flush(void)
{
    return sensor_flush_to_hub(ID_SMART_ALERT);
}

static int smart_alert_recv_data(struct data_unit_t *event, void *reserved)
{
    int err = 0;

    pr_err("smart_alert_recv_data, flush_action:%d\n", event->flush_action);

    if (event->flush_action == FLUSH_ACTION)
        err = situation_flush_report(ID_SMART_ALERT);
    else if (event->flush_action == DATA_ACTION)
        err = situation_data_report_t(ID_SMART_ALERT,
            event->alert_event.state, (int64_t)event->time_stamp);

    pr_err("smart_alert_recv_data, err:%d\n", err);
    return err;
}

static int smartalert_local_init(void)
{
    struct situation_control_path ctl = {0};
    struct situation_data_path data = {0};
    int err = 0;

    ctl.open_report_data = smart_alert_open_report_data;
    ctl.batch = smart_alert_batch;
    ctl.flush = smart_alert_flush;
    ctl.is_support_wake_lock = true;
    ctl.is_support_batch = false;
    err = situation_register_control_path(&ctl, ID_SMART_ALERT);
    if (err) {
        pr_err("register smart_alert control path err\n");
        goto exit;
    }

    data.get_data = smart_alert_get_data;
    err = situation_register_data_path(&data, ID_SMART_ALERT);
    if (err) {
        pr_err("register smart_alert data path err\n");
        goto exit;
    }
    err = scp_sensorHub_data_registration(ID_SMART_ALERT,
        smart_alert_recv_data);
    if (err) {
        pr_err("SCP_sensorHub_data_registration fail!!\n");
        goto exit;
    }
    return 0;
exit:
    return -1;
}
static int smartalert_local_uninit(void)
{
    return 0;
}

static struct situation_init_info smartalert_init_info = {
    .name = "smart_alert",
    .init = smartalert_local_init,
    .uninit = smartalert_local_uninit,
};

static int __init smartalert_init(void)
{
    situation_driver_add(&smartalert_init_info, ID_SMART_ALERT);
    return 0;
}

static void __exit smartalert_exit(void)
{
    pr_debug("%s\n", __func__);
}

module_init(smartalert_init);
module_exit(smartalert_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SMART ALERT HUB driver");
