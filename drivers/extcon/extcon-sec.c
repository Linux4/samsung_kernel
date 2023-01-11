/*
 * extcon-sec-common.c - Samsung logical extcon drvier to support USB switches
 *
 * This code support for multi MUIC at one device(project).
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 * Author: Seonggyu Park <seongyu.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/slab.h>
#include <linux/err.h>
#include <linux/notifier.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/extcon.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_qos.h>
#include <linux/sec-common.h>
#include <linux/switch.h>
#include <linux/battery/sec_charger.h>
#include <linux/extcon/extcon-sec.h>

/* Enable print serial log handle by kernel cmd_line */
static bool en_uart = true;
static bool bs0bs = false;

const char *extsec_cable[CABLE_NAME_MAX + 1] = {
	[CABLE_USB] = "USB",
	[CABLE_USB_HOST] = "USB Host",
	[CABLE_TA] = "TA",
	[CABLE_JIG_USB] = "JIG USB",
	[CABLE_JIG_UART] = "JIG UART",
	[CABLE_DESKTOP_DOCK] = "Desktop Dock",
	[CABLE_PPD] = "Power Sharing Cable",
	[CABLE_UNKNOWN] = "Unknown",
	[CABLE_UNKNOWN_VB] = "Unknown with VBUS",
};

struct extcon_sec_platform_data {
	char phy_edev_name[20];
};

struct extcon_sec {
	bool manual_mode;
	bool awake_mode;
	bool dock_mode;
	bool jig_status;

	struct device *dev;
	struct switch_dev dock_dev;
	struct work_struct wq;
	struct notifier_block extsec_nb;
	struct pm_qos_request qos_idle;

	struct extcon_dev *sec_edev;
	struct extcon_dev *phy_edev;
	struct extcon_sec_platform_data *pdata;

	int (*extsec_set_path_fn) (const char *);
	u32 (*extsec_detect_cable_fn) (struct extcon_sec *, u32);
	void (*extsec_dump_fn) (void);
};

#if defined(CONFIG_MACH_J7MLTE) || defined(CONFIG_MACH_J7M3G)
#include <linux/pxa1936_powermode.h>
#include "extcon-sec-j7.h"
#elif defined(CONFIG_MACH_XCOVER3LTE)
#include <linux/pxa1908_powermode.h>
#include "extcon-sec-xcover3.h"
#elif defined(CONFIG_MACH_COREPRIMEVELTE)
#include <linux/pxa1908_powermode.h>
#include "extcon-sec-coreprime.h"
#elif defined(CONFIG_MACH_GRANDPRIMEVELTE)
#include <linux/pxa1908_powermode.h>
#include "extcon-sec-grandprime.h"
#elif defined(CONFIG_MACH_J1ACELTE_LTN)
#include <linux/pxa1908_powermode.h>
#include "extcon-sec-j1acelteltn.h"
#endif

/* In order to support USB HOST Test at SMD Factory process */
BLOCKING_NOTIFIER_HEAD(sec_vbus_notifier);

int vbus_register_notify(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&sec_vbus_notifier, nb);
}

EXPORT_SYMBOL_GPL(vbus_register_notify);

int vbus_unregister_notify(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&sec_vbus_notifier, nb);
}

EXPORT_SYMBOL_GPL(vbus_unregister_notify);

void cutoff_cable_signal(struct extcon_sec *extsec_info)
{
	/*
	 * Cut off UART signals for corrosion prevention.
	 * Accept when this is factory testing or engineer binary
	 * and user binary that battery not inserted device.
	 */
	if (!en_uart && !bs0bs && is_battery_connected() == 1) {
		extsec_info->extsec_set_path_fn("OPEN");
		extsec_info->manual_mode = true;
		dev_info(extsec_info->dev, "PIN Anti-corrosion ON");
	}

	/* hold a wake lock for factory testing apps */
	pm_stay_awake(extsec_info->dev);
	pm_qos_update_request(&extsec_info->qos_idle, PM_QOS_CPUIDLE_BLOCK_AXI);
	extsec_info->awake_mode = true;
	dev_warn(extsec_info->dev, "hold a wake lock");
}

/* Enable & Disable wake lock control by Samsung Factory Testing App */
static ssize_t syssleep_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct extcon_sec *extsec_info = dev_get_drvdata(dev);

	if (!strncmp(buf, "1", 1)) {
		pm_relax(extsec_info->dev);
		pm_qos_update_request(&extsec_info->qos_idle, PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
		extsec_info->awake_mode = false;
		dev_info(extsec_info->dev, "release a wake lock through sysfs");
	} else if (!strncmp(buf, "0", 1)) {
		pm_stay_awake(dev);
		pm_qos_update_request(&extsec_info->qos_idle, PM_QOS_CPUIDLE_BLOCK_AXI);
		extsec_info->awake_mode = true;
		dev_info(extsec_info->dev, "hold a wake lock through sysfs");
	} else
		dev_err(extsec_info->dev, "invalid command!");

	return count;
}

static DEVICE_ATTR_WO(syssleep);

static ssize_t attached_dev_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct extcon_sec *extsec_info = dev_get_drvdata(dev);
	unsigned cable_idx;

	for (cable_idx = 0; cable_idx < CABLE_END; cable_idx++) {
		if (extcon_get_cable_state_(extsec_info->sec_edev, cable_idx))
			break;
	}

	return sprintf(buf, "%s\n",
		       extsec_info->sec_edev->supported_cable[cable_idx]);
}

static DEVICE_ATTR_RO(attached_dev);

static ssize_t adc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct extcon_sec *extsec_info = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", extsec_info->jig_status ? "1C" : "0");
}

static DEVICE_ATTR_RO(adc);

static ssize_t uart_en_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", bs0bs ? "1" : "0");
}

static ssize_t uart_en_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct extcon_sec *extsec_info = dev_get_drvdata(dev);

	if (!strncmp(buf, "1", 1)) {
		bs0bs = true;
		dev_info(extsec_info->dev, "Entered *#0*#\n");
	} else if (!strncmp(buf, "0", 1)) {
		bs0bs = false;
		dev_info(extsec_info->dev, "Exited *#0*#\n");;
	} else
		dev_err(extsec_info->dev, "invalid command!");

	return count;
}

static DEVICE_ATTR_RW(uart_en);

static ssize_t dump_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct extcon_sec *extsec_info = dev_get_drvdata(dev);

	if (extsec_info->extsec_dump_fn)
		extsec_info->extsec_dump_fn();

	return 0;
}
static DEVICE_ATTR_RO(dump);

static struct attribute *extsec_attrs[] = {
	&dev_attr_syssleep.attr,
	&dev_attr_attached_dev.attr,
	&dev_attr_adc.attr,
	&dev_attr_uart_en.attr,
	&dev_attr_dump.attr,
	NULL
};

ATTRIBUTE_GROUPS(extsec);

static void extsec_work_cb(struct work_struct *wq)
{
	struct extcon_sec *extsec_info =
	    container_of(wq, struct extcon_sec, wq);
	static u32 prev_cable_idx;
	bool attach;
	u32 cable_idx = CABLE_UNKNOWN, phy_cable_idx = 0;
	u32 phy_state = extcon_get_state(extsec_info->phy_edev);

	if (!phy_state) {
		attach = false;
		cable_idx = prev_cable_idx;
		if (extsec_info->manual_mode) {
			extsec_info->extsec_set_path_fn("AUTO");
			extsec_info->manual_mode = false;
			dev_info(extsec_info->dev, "Set Automatic again");
		}
		if (extsec_info->awake_mode) {
			pm_relax(extsec_info->dev);
			pm_qos_update_request(&extsec_info->qos_idle,
					      PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
			extsec_info->awake_mode = false;
			dev_warn(extsec_info->dev, "release a wake lock");
		}
		if (extsec_info->dock_mode) {
			switch_set_state(&extsec_info->dock_dev, false);
			extsec_info->dock_mode = false;
			dev_info(extsec_info->dev, "Report Dock Detach");
		}
		if (extsec_info->jig_status)
			extsec_info->jig_status = false;
	} else {
		attach = true;
		while (phy_cable_idx < extsec_info->phy_edev->max_supported &&
		       !(phy_state & (1 << phy_cable_idx)))
			phy_cable_idx++;

		cable_idx = extsec_info->extsec_detect_cable_fn(extsec_info,
								phy_cable_idx);
		if (cable_idx == CABLE_JIG_UART)
			extsec_info->jig_status = true;

		prev_cable_idx = cable_idx;
	}
	dev_info(extsec_info->dev, "%s connector %s\n", extsec_cable[cable_idx],
		 attach ? "attached" : "detached");
	extcon_set_cable_state_(extsec_info->sec_edev, cable_idx, attach);
};

static int extsec_notifier(struct notifier_block *nb,
			   unsigned long state, void *phy_edev)
{
	struct extcon_sec *extsec_info =
	    container_of(nb, struct extcon_sec, extsec_nb);

	schedule_work(&extsec_info->wq);

	return NOTIFY_OK;
}

static int __init extcon_sec_probe(struct platform_device *pdev)
{
	int ret;
	struct extcon_sec *extsec_info;
	struct device *sysfs_switch_dev;

	extsec_info = kzalloc(sizeof(struct extcon_sec), GFP_KERNEL);
	if (!extsec_info) {
		dev_err(&pdev->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	extsec_info->dev = &pdev->dev;
	extsec_info->extsec_nb.notifier_call = extsec_notifier;

	extsec_info->sec_edev = devm_extcon_dev_allocate(extsec_info->dev,
							 extsec_cable);
	if (!extsec_info->sec_edev) {
		ret = PTR_ERR(extsec_info->sec_edev);
		dev_err(extsec_info->dev,
			"failed to allocate memory for extcon\n");
		goto extcon_allocate_failed;
	}
	extsec_info->sec_edev->name = EXTCON_SEC_NAME;

	/* For AT Command FactoryTest */
	extsec_info->qos_idle.name = "extcon-SEC";
	pm_qos_add_request(&extsec_info->qos_idle, PM_QOS_CPUIDLE_BLOCK,
			   PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);

	/* Register extcon device */
	ret = devm_extcon_dev_register(extsec_info->dev, extsec_info->sec_edev);
	if (IS_ERR_VALUE(ret)) {
		dev_err(extsec_info->dev, "failed to register extcon device\n");
		goto extcon_register_failed;
	}

	/* Register MUIC sysfs nodes */
	if (!sec_class)
		sec_class = class_create(THIS_MODULE, "sec");

	sysfs_switch_dev = device_create_with_groups(sec_class, NULL, 0,
						     extsec_info, extsec_groups,
						     "switch");
	if (!sysfs_switch_dev) {
		dev_err(extsec_info->dev, "failed to create device files!\n");
		ret = -ENOMEM;
		goto device_create_failed;
	}

	/* DeskTop Dock  */
	extsec_info->dock_dev.name = "dock";
	ret = switch_dev_register(&extsec_info->dock_dev);
	if (IS_ERR_VALUE(ret)) {
		dev_err(extsec_info->dev, "failed to register dock device\n");
		goto device_create_failed;
	}

	extsec_info->manual_mode = false;
	extsec_info->awake_mode = false;
	extsec_info->dock_mode = false;

	ret = device_init_wakeup(extsec_info->dev, true);
	if (IS_ERR_VALUE(ret)) {
		dev_err(extsec_info->dev,
			"failed to wakeup source register!\n");
		goto set_wakeup_failed;
	}

	ret = extcon_sec_set_board(extsec_info);
	if (IS_ERR_VALUE(ret)) {
		dev_err(extsec_info->dev, "failed to set board data!\n");
		goto extcon_sec_set_board_failed;
	}

	extsec_info->phy_edev =
	    extcon_get_extcon_dev(extsec_info->pdata->phy_edev_name);
	if (!extsec_info->phy_edev) {
		dev_err(extsec_info->dev, "failed to get extcon dev!\n");
		ret = -ENXIO;
		goto extcon_get_extcon_dev_failed;
	}

	extcon_register_notifier(extsec_info->phy_edev,
				 &extsec_info->extsec_nb);

	INIT_WORK(&extsec_info->wq, extsec_work_cb);

	dev_info(extsec_info->dev, "connected to %s",
		 extsec_info->pdata->phy_edev_name);
	return 0;

extcon_get_extcon_dev_failed:
	kfree(extsec_info->pdata);
extcon_sec_set_board_failed:
	device_destroy(sec_class, 0);
set_wakeup_failed:
device_create_failed:
	devm_extcon_dev_unregister(extsec_info->dev, extsec_info->sec_edev);
extcon_register_failed:
	devm_extcon_dev_free(extsec_info->dev, extsec_info->sec_edev);
extcon_allocate_failed:
	kfree(extsec_info);
	return ret;
}

#if defined(CONFIG_OF)
static const struct of_device_id extcon_sec_dt_ids[] = {
	{.compatible = "samsung,extcon-sec"},
	{},
};

MODULE_DEVICE_TABLE(of, extcon_sec_dt_ids);
#endif

static const struct platform_device_id extcon_sec_id[] = {
	{"extcon-sec", 0},
	{}
};

MODULE_DEVICE_TABLE(platform, extcon_sec_id);

static struct platform_driver extcon_sec_driver = {
	.driver = {
		   .name = "extcon-sec",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(extcon_sec_dt_ids),
		   },
	.probe = extcon_sec_probe,
	.id_table = extcon_sec_id,
};

module_platform_driver(extcon_sec_driver);

static int __init extcon_sec_set_en_uart(char *str)
{
	int n;

	if (!get_option(&str, &n))
		return 1;
	en_uart = (n == 1) ? true : false;
	pr_info("%s = %d\n", __func__, en_uart);

	return 0;
}

__setup("uart_swc_en=", extcon_sec_set_en_uart);

MODULE_DESCRIPTION("Samsung logical external connector driver");
MODULE_AUTHOR("Seonggyu Park <seongyu.park@samsung.com>");
MODULE_LICENSE("GPL");
