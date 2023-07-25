/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/cpu_pm.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/suspend.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/alarmtimer.h>
#include <linux/sprd_sip_svc.h>
#include <linux/sprd-debugstat.h>

static const char subsys_name[8] = "AP";

static struct sprd_sip_svc_pwr_ops *pwr_ops;
struct subsys_sleep_info *ap_sleep_info;

static struct subsys_sleep_info *ap_sleep_info_get(void *data)
{
	const struct cpumask *cpu = cpu_online_mask;
	u32 major, intc_num, intc_bit;

	ap_sleep_info->total_duration = ktime_get_boot_fast_ns() / 1000000000;

	ap_sleep_info->active_core = (uint32_t)(cpu->bits[0]);

	pwr_ops->get_wakeup_source(&major, NULL, NULL);
	intc_num = (major >> 16) & 0xFFFF;
	intc_bit = major & 0xFFFF;

	ap_sleep_info->wakeup_reason = (intc_num << 5) + intc_bit;

	return ap_sleep_info;
}

#ifdef CONFIG_PM_SLEEP
static int sprd_debugstat_suspend(struct device *dev)
{
	ap_sleep_info->last_sleep_duration = (u32)(ktime_get_boot_fast_ns() / 1000000000);

	return 0;
}

static int sprd_debugstat_resume(struct device *dev)
{
	u32 sleep, wakeup;

	ap_sleep_info->last_wakeup_duration = (u32)(ktime_get_boot_fast_ns() / 1000000000);

	sleep = ap_sleep_info->last_sleep_duration;
	wakeup = ap_sleep_info->last_wakeup_duration;

	ap_sleep_info->sleep_duration_total += wakeup - sleep;

	return 0;
}
#endif

/**
 * sprd_debugstat_driver_probe - add the debug stat driver
 */
static int sprd_debugstat_driver_probe(struct platform_device *pdev)
{
	struct sprd_sip_svc_handle *sip;
	struct device *dev;
	int ret;

	pr_info("%s: Init debug stat driver\n", __func__);

	dev = &pdev->dev;
	if (!pdev) {
		pr_err("%s: Get debug device error\n", __func__);
		return -EINVAL;
	}

	sip = sprd_sip_svc_get_handle();
	if (!sip) {
		pr_err("%s: Get wakeup sip error\n", __func__);
		return -EINVAL;
	}

	pwr_ops = &sip->pwr_ops;

	ap_sleep_info = devm_kzalloc(dev, sizeof(struct subsys_sleep_info), GFP_KERNEL);
	if (!ap_sleep_info) {
		pr_err("%s: Sleep info alloc error\n", __func__);
		return -ENOMEM;
	}

	strcpy(ap_sleep_info->subsystem_name, subsys_name);
	ap_sleep_info->current_status = 1;
	ap_sleep_info->irq_to_ap_count = 0;

	ret = stat_info_register("ap_sys", ap_sleep_info_get, NULL);
	if (ret) {
		pr_err("%s: Register ap sleep info get error\n", __func__);
		kfree(ap_sleep_info);
		return -EINVAL;
	}

	return 0;
}

/**
 * sprd_debugstat_driver_remove - remove the debug log driver
 */
static int sprd_debugstat_driver_remove(struct platform_device *pdev)
{
	int ret;

	ret = stat_info_unregister("ap_sys");

	return ret;
}

static const struct dev_pm_ops sprd_debugstat_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sprd_debugstat_suspend, sprd_debugstat_resume)
};

static const struct of_device_id sprd_debugstat_of_match[] = {
	{.compatible = "sprd,debugstat-sharkl6",},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_debugstat_of_match);

static struct platform_driver sprd_debugstat_driver = {
	.driver = {
		.name = "sprd-debugstat",
		.of_match_table = sprd_debugstat_of_match,
		.pm = &sprd_debugstat_pm_ops,
	},
	.probe = sprd_debugstat_driver_probe,
	.remove = sprd_debugstat_driver_remove,
};

module_platform_driver(sprd_debugstat_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("sprd debug stat driver");
