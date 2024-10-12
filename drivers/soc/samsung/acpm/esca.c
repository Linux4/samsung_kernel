/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/reset/exynos/exynos-reset.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include <soc/samsung/exynos/exynos-soc.h>
#include <linux/sched/clock.h>
#include <linux/module.h>

#include "acpm.h"
#include "acpm_ipc.h"
#include "fw_header/framework.h"

static struct acpm_info *exynos_esca;

static int esca_send_data(struct device_node *node, unsigned int check_id,
		struct ipc_config *config);

static int debug_log_level_get(void *data, unsigned long long *val)
{

	return 0;
}

static int debug_log_level_set(void *data, unsigned long long val)
{

	return 0;
}

static int debug_ipc_loopback_test_get(void *data, unsigned long long *val)
{
	struct ipc_config config;
	int ret = 0;
	unsigned int cmd[4] = {0, };
	static unsigned long long ipc_time_start;
	static unsigned long long ipc_time_end;

	config.cmd = cmd;
	config.cmd[0] = (1 << ACPM_IPC_PROTOCOL_TEST);
	config.cmd[0] |= 0x3 << ACPM_IPC_PROTOCOL_ID;

	config.response = true;
	config.indirection = false;

	ipc_time_start = sched_clock();
	ret = esca_send_data(exynos_esca->dev->of_node, 3, &config);
	ipc_time_end = sched_clock();

	if (!ret)
		*val = ipc_time_end - ipc_time_start;
	else
		*val = 0;

	config.cmd = NULL;

	return 0;
}

static int debug_ipc_loopback_test_set(void *data, unsigned long long val)
{

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_log_level_fops,
		debug_log_level_get, debug_log_level_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(debug_ipc_loopback_test_fops,
		debug_ipc_loopback_test_get, debug_ipc_loopback_test_set, "%llu\n");

static void esca_debugfs_init(struct acpm_info *esca)
{
	struct dentry *den;

	den = debugfs_create_dir("esca", NULL);
	debugfs_create_file("ipc_loopback_test", 0644, den, esca, &debug_ipc_loopback_test_fops);
	debugfs_create_file("log_level", 0644, den, NULL, &debug_log_level_fops);
}

void esca_enter_wfi(void)
{
	struct ipc_config config;
	int ret = 0;
	unsigned int cmd[4] = {0, };

	if (exynos_esca->enter_wfi)
		return;

	config.cmd = cmd;
	config.response = true;
	config.indirection = false;
	config.cmd[0] = 1 << ACPM_IPC_PROTOCOL_STOP;

	ret = esca_send_data(exynos_esca->dev->of_node, ACPM_IPC_PROTOCOL_STOP, &config);

	config.cmd = NULL;

	if (ret) {
		pr_err("[ESCA] esca enter wfi fail!!\n");
	} else {
		pr_err("[ESCA] wfi done\n");
		exynos_esca->enter_wfi++;
	}
}

void exynos_esca_force_apm_wdt_reset(void)
{
	struct ipc_config config;
	int ret = 0;
	unsigned int cmd[4] = {0, };

	config.cmd = cmd;
	config.response = true;
	config.indirection = false;
	config.cmd[0] = (1 << ACPM_IPC_PROTOCOL_STOP_WDT);

	ret = esca_send_data(exynos_esca->dev->of_node, ACPM_IPC_PROTOCOL_STOP_WDT, &config);

	pr_err("ACPM force WDT reset. (ret: %d)\n", ret);
}
EXPORT_SYMBOL_GPL(exynos_esca_force_apm_wdt_reset);

static int esca_send_data(struct device_node *node, unsigned int check_id,
		struct ipc_config *config)
{
	unsigned int channel_num, size;
	int ret = 0;
	int timeout_flag;
	unsigned int id = 0;
	static int ipc_done;

	ret = esca_ipc_request_channel(node, NULL, &channel_num, &size);
	if (!ret) {
		ipc_done = -1;

		ret = esca_ipc_send_data(channel_num, config);

		if (config->cmd[0] & (1 << ACPM_IPC_PROTOCOL_STOP)) {
			ipc_done = ACPM_IPC_PROTOCOL_STOP;
		} else if (config->cmd[0] & (1 << ACPM_IPC_PROTOCOL_TEST)) {
			id = config->cmd[0] & ACPM_IPC_PROTOCOL_IDX;
			id = id	>> ACPM_IPC_PROTOCOL_ID;
			ipc_done = id;
		} else {
			id = config->cmd[0] & ACPM_IPC_PROTOCOL_IDX;
			id = id	>> ACPM_IPC_PROTOCOL_ID;
			ipc_done = id;
		}

		/* Response interrupt waiting */
		UNTIL_EQUAL(ipc_done, check_id, timeout_flag);

		if (timeout_flag)
			ret = -ETIMEDOUT;

		esca_ipc_release_channel(node, channel_num);
	} else {
		pr_err("%s ipc request_channel fail %d, id:%u, size:%u\n",
				__func__, ret, channel_num, size);
		ret = -EBUSY;
	}

	return ret;
}

static int esca_probe(struct platform_device *pdev)
{
	struct acpm_info *esca;
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;

	dev_info(&pdev->dev, "esca probe\n");

	if (!node) {
		dev_err(&pdev->dev, "driver doesnt support"
				"non-dt devices\n");
		return -ENODEV;
	}

	esca = devm_kzalloc(&pdev->dev,
			sizeof(struct acpm_info), GFP_KERNEL);
	if (IS_ERR(esca))
		return PTR_ERR(esca);
	if (!esca)
		return -ENOMEM;

	esca->dev = &pdev->dev;
	exynos_esca = esca;

	esca_debugfs_init(esca);

	dev_info(&pdev->dev, "esca probe done.\n");

	return ret;
}

static int esca_remove(struct platform_device *pdev)
{
	return 0;
}

static struct acpm_ipc_drvdata esca_drvdata = {
	.is_mailbox_master = false,
	.is_apm = false,
	.is_asm = true,
};

static const struct of_device_id esca_ipc_match[] = {
	{
		.compatible = "samsung,exynos-esca-ipc",
		.data = &esca_drvdata,
	},
	{},
};
MODULE_DEVICE_TABLE(of, esca_ipc_match);

static struct platform_driver samsung_esca_ipc_driver = {
	.probe	= acpm_ipc_probe,
	.remove	= acpm_ipc_remove,
	.driver	= {
		.name = "exynos-esca-ipc",
		.owner	= THIS_MODULE,
		.of_match_table	= esca_ipc_match,
	},
};

static const struct of_device_id esca_match[] = {
	{ .compatible = "samsung,exynos-esca" },
	{},
};
MODULE_DEVICE_TABLE(of, esca_match);

static struct platform_driver samsung_esca_driver = {
	.probe	= esca_probe,
	.remove	= esca_remove,
	.driver	= {
		.name = "exynos-esca",
		.owner	= THIS_MODULE,
		.of_match_table	= esca_match,
	},
};

static int exynos_esca_init(void)
{
	platform_driver_register(&samsung_esca_ipc_driver);
	platform_driver_register(&samsung_esca_driver);
	return 0;
}
postcore_initcall_sync(exynos_esca_init);

static void exynos_esca_exit(void)
{
	platform_driver_unregister(&samsung_esca_driver);
	platform_driver_unregister(&samsung_esca_ipc_driver);
}
module_exit(exynos_esca_exit);

MODULE_LICENSE("GPL");
