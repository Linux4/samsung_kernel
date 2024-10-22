/*
 * sec_pm_debug.c
 *
 *  Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *  Jonghyeon Cho <jongjaaa.cho.cho@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/sec_pm_debug.h>
#include <linux/pm_wakeup.h>
#include <linux/of.h>
#include <linux/thermal.h>
#include <linux/suspend.h>
#include <linux/rtc.h>

#ifdef KLOG_MODNAME
#undef KLOG_MODNAME
#define KLOG_MODNAME	""
#endif

static struct sec_pm_dbg pm;

static void sec_pm_suspend_marker(char *type)
{
	struct timespec64 ts;
	struct rtc_time tm;

	ktime_get_real_ts64(&ts);
	rtc_time64_to_tm(ts.tv_sec, &tm);

	pr_info("PM: suspend %s %d-%02d-%02d %02d:%02d:%02d.%09lu UTC\n",
		type, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
}

static void sec_pm_debug_print_tz(void)
{
	int i, temp, len = 0, ret = 0;
	struct thermal_zone_device *tz;
	char buf[500] = {0, };

	len += snprintf(buf + len, sizeof(buf) - len, "tz_print");

	for (i = 0; i < pm.tz_cnt; i++) {
		tz = thermal_zone_get_zone_by_name(pm.tz_list[i]);
		if (IS_ERR_OR_NULL(tz)) {
			pr_err("%s: fail to get thermal zone %s\n", __func__, pm.tz_list[i]);
			continue;
		}

		ret = thermal_zone_get_temp(tz, &temp);
		if(ret) {
			pr_err("%s: failed to get thermal zone temperature (%d)\n", __func__, ret);
		}
		len += snprintf(buf + len, sizeof(buf) - len,
				"[%s:%d]", pm.tz_list[i], (int)temp / 100);
	}
	pr_info("%s\n", buf);
}

static void print_wakeup_source_active(
					struct wakeup_source *ws)
{
	unsigned long flags;
	ktime_t total_time;
	unsigned long active_count;
	ktime_t active_time;
	ktime_t prevent_sleep_time;

	spin_lock_irqsave(&ws->lock, flags);

	total_time = ws->total_time;
	prevent_sleep_time = ws->prevent_sleep_time;
	active_count = ws->active_count;

	if (ws->active) {
		ktime_t now = ktime_get();
		active_time = ktime_sub(now, ws->last_time);
		total_time = ktime_add(total_time, active_time);
		if (ws->autosleep_enabled)
			prevent_sleep_time = ktime_add(prevent_sleep_time,
				ktime_sub(now, ws->start_prevent_time));
	} else {
		active_time = ktime_set(0, 0);
	}

	pr_info("%s: active_count(%lu), active_time(%lld), total_time(%lld)\n",
			ws->name, active_count,
			ktime_to_ms(active_time), ktime_to_ms(total_time));

	spin_unlock_irqrestore(&ws->lock, flags);
}

static int wakeup_sources_stats_active(void)
{
	struct wakeup_source *ws;
	int idx;

	pr_info("[SEC_PM] Active wake lock:\n");

	idx = wakeup_sources_read_lock();

	for_each_wakeup_source(ws)
		if (ws->active)
			print_wakeup_source_active(ws);

	wakeup_sources_read_unlock(idx);

	return 0;
}

static void handle_tz_work(struct work_struct *work)
{
	sec_pm_debug_print_tz();
	schedule_delayed_work(&pm.tz_work, msecs_to_jiffies(pm.tz_work_ms));
}

static void handle_ws_work(struct work_struct *work)
{
	wakeup_sources_stats_active();
	schedule_delayed_work(&pm.ws_work, msecs_to_jiffies(pm.ws_work_ms));
}

static int sec_pm_notifier_event(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		sec_pm_suspend_marker("entry");
		break;
	case PM_POST_SUSPEND:
		sec_pm_suspend_marker("exit");
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block sec_pm_notifier_block = {
	.notifier_call = sec_pm_notifier_event,
};

static int sec_pm_debug_parse_dt(struct device_node *np)
{
	int ret = 0, cnt = 0;
	struct property *prop;
	const char *tz;
	u32 val;

	if (!np) {
		pr_err("%s: no device tree\n", __func__);
		ret = -EINVAL;
		goto parse_fail;
	}

	if (!of_property_read_u32(np, "ws_work_ms", &val))
		pm.ws_work_ms = val;
	else
		pm.ws_work_ms = DEFAULT_LOGGING_MS;

	if (!of_property_read_u32(np, "tz_work_ms", &val))
		pm.tz_work_ms = val;
	else
		pm.tz_work_ms = DEFAULT_LOGGING_MS;

	if (of_property_count_strings(np, "thermal_zones") < 0) {
		pr_err("%s: cannot thermal_zones property\n", __func__);
		ret = -EINVAL;
		goto parse_fail;
	}

	of_property_for_each_string(np, "thermal_zones", prop, tz) {
		pm.tz_list[cnt++] = (char *)tz;
	}

	pm.tz_cnt = cnt;

	pr_info("%s: register %d thermal zones\n", __func__, cnt);

parse_fail:
	return ret;
}

static int sec_pm_debug_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("%s: start\n", __func__);

	/* Parse Device Tree */
	ret = sec_pm_debug_parse_dt(pdev->dev.of_node);

	/* Initiate wakeup sorce workqueue */
	INIT_DELAYED_WORK(&pm.ws_work, handle_ws_work);
	if (!schedule_delayed_work(&pm.ws_work, msecs_to_jiffies(5000))) {
		pr_info("%s: Fail to register workqueue\n", __func__);
		ret = -EAGAIN;
		goto probe_fail;
	}

	/* Initiate thermal zone workqueue */
	INIT_DELAYED_WORK(&pm.tz_work, handle_tz_work);
	if (!schedule_delayed_work(&pm.tz_work, msecs_to_jiffies(5000))) {
		pr_info("%s: Fail to register workqueue\n", __func__);
		ret = -EAGAIN;
		goto probe_fail;
	}

	/* Register PM notifier */
	ret = register_pm_notifier(&sec_pm_notifier_block);
	if (ret) {
		pr_info("%s: Fail to register PM notifier(%d)\n", __func__, ret);
		goto probe_fail;
	}

	pr_info("%s: done\n", __func__);

probe_fail:
	return ret;
}

static const struct of_device_id sec_pm_debug_match_table[] = {
	{ .compatible = "samsung,sec_pm_debug" },
	{}
};

static struct platform_driver sec_pm_debug_driver = {
	.driver = {
		.name = "sec_pm_debug",
		.of_match_table = sec_pm_debug_match_table,
	},
	.probe = sec_pm_debug_probe,
};

static int __init sec_pm_debug_init(void)
{
	return platform_driver_register(&sec_pm_debug_driver);
}

static void __exit sec_pm_debug_exit(void)
{
	platform_driver_unregister(&sec_pm_debug_driver);
}

MODULE_AUTHOR("Jonghyeon Cho <jongjaaa.cho@samsung.com>");
MODULE_DESCRIPTION("A driver to debugging the feature related power");
MODULE_LICENSE("GPL");

late_initcall(sec_pm_debug_init);
module_exit(sec_pm_debug_exit);
