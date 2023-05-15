/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/cpu_pm.h>
#include <linux/debug-snapshot.h>
#include <linux/suspend.h>

#include <soc/samsung/exynos-adv-tracer-ipc.h>
#include <soc/samsung/exynos-adv-tracer-ehld.h>
#include <soc/samsung/exynos-ehld.h>
#include <soc/samsung/exynos-pmu.h>

enum ehld_ipc_cmd {
	eEHLD_IPC_CMD_SET_ENABLE,
	eEHLD_IPC_CMD_GET_ENABLE,
	eEHLD_IPC_CMD_SET_INTERVAL,
	eEHLD_IPC_CMD_SET_LOCKUP_WARN_VAL,
	eEHLD_IPC_CMD_SET_LOCKUP_VAL,
	eEHLD_IPC_CMD_NOTI_CPU_ON,
	eEHLD_IPC_CMD_NOTI_CPU_OFF,
	eEHLD_IPC_CMD_LOCKUP_DETECT_WARN,
	eEHLD_IPC_CMD_LOCKUP_DETECT_SW,
	eEHLD_IPC_CMD_LOCKUP_DETECT_HW,
};

struct plugin_ehld_info {
	struct adv_tracer_plugin *ehld_dev;
	struct platform_device *pdev;
	unsigned int enable;
	unsigned int interval;
} plugin_ehld;

int adv_tracer_ehld_get_enable(void)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	cmd.cmd_raw.cmd = eEHLD_IPC_CMD_GET_ENABLE;
	ret = adv_tracer_ipc_send_data(plugin_ehld.ehld_dev->id, (struct adv_tracer_ipc_cmd *)&cmd);
	if (ret < 0) {
		dev_err(&plugin_ehld.pdev->dev, "ehld ipc cannot get enable\n");
		return ret;
	}
	plugin_ehld.enable = cmd.buffer[1];

	dev_info(&plugin_ehld.pdev->dev, "EHLD %sabled\n",
			plugin_ehld.enable ? "en" : "dis");

	return plugin_ehld.enable;
}

int adv_tracer_ehld_set_enable(int en)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	cmd.cmd_raw.cmd = eEHLD_IPC_CMD_SET_ENABLE;
	cmd.buffer[1] = en;
	ret = adv_tracer_ipc_send_data(plugin_ehld.ehld_dev->id, &cmd);
	if (ret < 0) {
		dev_err(&plugin_ehld.pdev->dev, "ehld ipc cannot enable setting\n");
		return ret;
	}
	plugin_ehld.enable = en;
	dev_info(&plugin_ehld.pdev->dev, "set to EHLD %sabled\n",
			plugin_ehld.enable ? "en" : "dis");
	return 0;
}

int adv_tracer_ehld_set_interval(u32 interval)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	if (!interval)
		return -EINVAL;

	cmd.cmd_raw.cmd = eEHLD_IPC_CMD_SET_INTERVAL;
	cmd.buffer[1] = interval;
	ret = adv_tracer_ipc_send_data(plugin_ehld.ehld_dev->id, &cmd);
	if (ret < 0) {
		dev_err(&plugin_ehld.pdev->dev, "ehld ipc can't set the interval\n");
		return ret;
	}
	plugin_ehld.interval = interval;
	dev_info(&plugin_ehld.pdev->dev, "set to interval to %uus\n", interval);

	return 0;
}

int adv_tracer_ehld_set_warn_count(u32 count)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	if (!count)
		return -EINVAL;

	cmd.cmd_raw.cmd = eEHLD_IPC_CMD_SET_LOCKUP_WARN_VAL;
	cmd.buffer[1] = count;
	ret = adv_tracer_ipc_send_data(plugin_ehld.ehld_dev->id, &cmd);
	if (ret < 0) {
		dev_err(&plugin_ehld.pdev->dev, "ehld ipc can't set lockup warning count\n");
		return ret;
	}
	dev_info(&plugin_ehld.pdev->dev, "set warning count for lockup cpu to %u times\n", count);

	return 0;
}

int adv_tracer_ehld_set_lockup_count(u32 count)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	if (!count)
		return 0;

	cmd.cmd_raw.cmd = eEHLD_IPC_CMD_SET_LOCKUP_VAL;
	cmd.buffer[1] = count;
	ret = adv_tracer_ipc_send_data(plugin_ehld.ehld_dev->id, &cmd);
	if (ret < 0) {
		dev_err(&plugin_ehld.pdev->dev, "ehld ipc can't set lockup count\n");
		return ret;
	}
	dev_info(&plugin_ehld.pdev->dev, "set warning count for lockup cpu to %u times\n", count);

	return 0;
}

u32 adv_tracer_ehld_get_interval(void)
{
	return plugin_ehld.interval;
}

int adv_tracer_ehld_noti_cpu_state(int cpu, int en)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	if (!ret)
		return -EINVAL;

	if (en == true)
		cmd.cmd_raw.cmd = eEHLD_IPC_CMD_NOTI_CPU_ON;
	else
		cmd.cmd_raw.cmd = eEHLD_IPC_CMD_NOTI_CPU_OFF;

	cmd.buffer[1] = cpu;

	ret = adv_tracer_ipc_send_data_polling(plugin_ehld.ehld_dev->id, &cmd);
	if (ret < 0) {
		dev_err(&plugin_ehld.pdev->dev, "ehld ipc cannot cmd state\n");
		return ret;
	}

	return 0;
}

static void adv_tracer_ehld_handler(struct adv_tracer_ipc_cmd *cmd, unsigned int len)
{
	switch(cmd->cmd_raw.cmd) {
	case eEHLD_IPC_CMD_LOCKUP_DETECT_WARN:
		dev_err(&plugin_ehld.pdev->dev,
			"CPU%x is Hardlockup Detected Warning - counter:0x%x\n",
			(unsigned int)cmd->buffer[1],
			(unsigned int)cmd->buffer[2]);
			exynos_ehld_do_policy();
			break;
	case eEHLD_IPC_CMD_LOCKUP_DETECT_HW:
		dev_err(&plugin_ehld.pdev->dev,
			"cpu%x is Hardlockup Detected - counter:0x%x, Caused by HW\n",
			(unsigned int)cmd->buffer[1],
			(unsigned int)cmd->buffer[2]);
			exynos_ehld_do_policy();
		break;
	case eEHLD_IPC_CMD_LOCKUP_DETECT_SW:
		dev_err(&plugin_ehld.pdev->dev,
			"cpu%x is Hardlockup Detected - counter:0x%x, Caused by SW Code\n",
			(unsigned int)cmd->buffer[1],
			(unsigned int)cmd->buffer[2]);
			exynos_ehld_do_policy();
		break;
	}
}

static ssize_t ehld_enable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long val = simple_strtoul(buf, NULL, 10);

	adv_tracer_ehld_set_enable(!!val);

	return size;
}

static ssize_t ehld_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, 10, "%sable\n", plugin_ehld.enable ? "en" : "dis");
}

static DEVICE_ATTR_RW(ehld_enable);

static struct attribute *adv_tracer_ehld_sysfs_attrs[] = {
	&dev_attr_ehld_enable.attr,
	NULL,
};
ATTRIBUTE_GROUPS(adv_tracer_ehld_sysfs);

static int adv_tracer_ehld_c2_pm_notifier(struct notifier_block *self,
						unsigned long action, void *v)
{
	if (!plugin_ehld.enable)
		return NOTIFY_OK;

	switch (action) {
	case CPU_PM_ENTER:
		break;
        case CPU_PM_ENTER_FAILED:
        case CPU_PM_EXIT:
		break;
	case CPU_CLUSTER_PM_ENTER:
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		break;
	}
	return NOTIFY_OK;
}

static int adv_tracer_ehld_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	if (!plugin_ehld.enable)
		return NOTIFY_OK;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		break;
	case PM_POST_SUSPEND:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block adv_tracer_ehld_nb = {
	.notifier_call = adv_tracer_ehld_pm_notifier,
};

static struct notifier_block adv_tracer_ehld_c2_pm_nb = {
	.notifier_call = adv_tracer_ehld_c2_pm_notifier,
};

static int adv_tracer_ehld_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct adv_tracer_plugin *ehld = NULL;
	int ret;

	dev_set_socdata(&pdev->dev, "Exynos", "EAT_EHLD");
	ehld = devm_kzalloc(&pdev->dev, sizeof(struct adv_tracer_plugin), GFP_KERNEL);
	if (!ehld) {
		dev_err(&pdev->dev, "can not allocate mem for ehld\n");
		ret = -ENOMEM;
		goto err_ehld_info;
	}

	plugin_ehld.pdev = pdev;
	plugin_ehld.ehld_dev = ehld;

	ret = adv_tracer_ipc_request_channel(node, (ipc_callback)adv_tracer_ehld_handler,
				&ehld->id, &ehld->len);
	if (ret < 0) {
		dev_err(&pdev->dev, "ehld ipc request fail(%d)\n",ret);
		ret = -ENODEV;
		goto err_sysfs_probe;
	}

	platform_set_drvdata(pdev, ehld);
	ret = sysfs_create_groups(&pdev->dev.kobj, adv_tracer_ehld_sysfs_groups);
	if (ret) {
		dev_err(&pdev->dev, "fail to register sysfs.\n");
		return ret;
	}

	/* register pm notifier */
	register_pm_notifier(&adv_tracer_ehld_nb);

	/* register cpu pm notifier for C2 */
	cpu_pm_register_notifier(&adv_tracer_ehld_c2_pm_nb);

	dev_info(&pdev->dev, "%s successful.\n", __func__);
	return 0;

err_sysfs_probe:
	kfree(ehld);
err_ehld_info:
	return ret;
}

static int adv_tracer_ehld_remove(struct platform_device *pdev)
{
	struct adv_tracer_plugin *ehld = platform_get_drvdata(pdev);

	adv_tracer_ipc_release_channel(ehld->id);
	kfree(ehld);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id adv_tracer_ehld_match[] = {
	{
		.compatible = "samsung,exynos-adv-tracer-ehld",
	},
	{},
};

static struct platform_driver adv_tracer_ehld_drv = {
	.probe          = adv_tracer_ehld_probe,
	.remove         = adv_tracer_ehld_remove,
	.driver         = {
		.name   = "exynos-adv-tracer-ehld",
		.owner  = THIS_MODULE,
		.of_match_table = adv_tracer_ehld_match,
	},
};

static int __init adv_tracer_ehld_init(void)
{
	return platform_driver_register(&adv_tracer_ehld_drv);
}
subsys_initcall(adv_tracer_ehld_init);
