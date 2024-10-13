/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "esca_drv.h"
#include "fw_header/framework.h"

struct esca_info *exynos_esca[NR_ESCA_LAYERS];

static void esca_enter_wfi(u32 esca_layer)
{
	struct ipc_config config;
	int ret = 0;
	unsigned int cmd[4] = {0, };

	config.cmd = cmd;
	config.response = true;
	config.indirection = false;
	config.cmd[0] = 1 << ESCA_IPC_PROTOCOL_STOP;

	ret = esca_send_data(exynos_esca[esca_layer]->dev->of_node, ESCA_IPC_PROTOCOL_STOP, &config);

	config.cmd = NULL;

	if (ret) {
		pr_err("[ESCA][%d] enter wfi fail!!\n", esca_layer);
	} else {
		pr_err("[ESCA][%d] wfi done\n", esca_layer);
	}
}

void exynos_esca_reboot(void)
{
	if (ESCA_MBOX__APP != -1)
		esca_enter_wfi(ESCA_MBOX__APP);
	if (ESCA_MBOX__PHY != -1)
		esca_enter_wfi(ESCA_MBOX__PHY);
}
EXPORT_SYMBOL_GPL(exynos_esca_reboot);

void exynos_esca_force_apm_wdt_reset(void)
{
	struct ipc_config config;
	int ret = 0;
	unsigned int cmd[4] = {0, };

	config.cmd = cmd;
	config.response = true;
	config.indirection = false;
	config.cmd[0] = (1 << ESCA_IPC_PROTOCOL_STOP_WDT);

	ret = esca_send_data(exynos_esca[ESCA_MBOX__PHY]->dev->of_node, ESCA_IPC_PROTOCOL_STOP_WDT, &config);

	pr_err("ESCA force WDT reset. (ret: %d)\n", ret);
}
EXPORT_SYMBOL_GPL(exynos_esca_force_apm_wdt_reset);

int esca_send_data(struct device_node *node, unsigned int check_id,
		struct ipc_config *config)
{
	unsigned int channel_num, size;
	int ret = 0;
	int timeout_flag;
	unsigned int id = 0;
	static int ipc_done;

	if (!esca_ipc_request_channel(node, NULL,
				&channel_num, &size)) {
		ipc_done = -1;

		ret = esca_ipc_send_data(channel_num, config);

		if (config->cmd[0] & (1 << ESCA_IPC_TASK_PROF_START)) {
			ipc_done = ESCA_IPC_TASK_PROF_START;
		} else if (config->cmd[0] & (1 << ESCA_IPC_TASK_PROF_END)) {
			ipc_done = ESCA_IPC_TASK_PROF_END;
		} else if (config->cmd[0] & (1 << ESCA_IPC_PROTOCOL_STOP)) {
			ipc_done = ESCA_IPC_PROTOCOL_STOP;
		} else if (config->cmd[0] & (1 << ESCA_IPC_PROTOCOL_TEST)) {
			id = config->cmd[0] & ESCA_IPC_PROTOCOL_IDX;
			id = id	>> ESCA_IPC_PROTOCOL_ID;
			ipc_done = id;
		} else {
			id = config->cmd[0] & ESCA_IPC_PROTOCOL_IDX;
			id = id	>> ESCA_IPC_PROTOCOL_ID;
			ipc_done = id;
		}

		/* Response interrupt waiting */
		UNTIL_EQUAL(ipc_done, check_id, timeout_flag);

		if (timeout_flag)
			ret = -ETIMEDOUT;

		esca_ipc_release_channel(node, channel_num);
	} else {
		pr_err("%s ipc request_channel fail, id:%u, size:%u\n",
				__func__, channel_num, size);
		ret = -EBUSY;
	}

	return ret;
}

static int esca_drv_dt_parse(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np;
	const __be32 *prop;
	unsigned int len, val;
	struct resource *res;
	struct esca_info *esca;

	np = pdev->dev.of_node;
	if (!np) {
		dev_err(&pdev->dev, "driver doesnt support non-dt devices\n");
		return -ENODEV;
	}

	prop = of_get_property(np, "esca-layer", &len);
	if (prop) {
		val = be32_to_cpup(prop);
		if ((val >= NR_ESCA_LAYERS) ||
				(val < NR_ESCA_LAYERS && exynos_esca[val])) {
			dev_err(&pdev->dev, "Invalid ESCA layer value\n");
			return -EINVAL;
		}
	} else {
		dev_err(&pdev->dev, "Missing ESCA layer value\n");
		return -ENODEV;
	}

	esca = devm_kzalloc(&pdev->dev, sizeof(struct esca_info), GFP_KERNEL);
	if (IS_ERR(esca))
		return PTR_ERR(esca);
	if (!esca)
		return -ENOMEM;

	exynos_esca[val] = esca;
	esca->layer = val;

	prop = of_get_property(np, "num-cores", &len);
	if (prop) {
		val = be32_to_cpup(prop);
		if (val <= 0) {
			dev_err(&pdev->dev, "Invalid ESCA num-cores value\n");
			return -EINVAL;
		}
		esca->num_cores = val;
	} else {
		dev_err(&pdev->dev, "Missing ESCA num-cores value\n");
		return -ENODEV;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sram");
	if (res) {
		esca->sram_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(esca->sram_base)) {
			dev_err(&pdev->dev, "SRAM ioremap error\n");
			return -ENOMEM;
		}

		prop = of_get_property(np, "initdata-base", &len);
		if (prop) {
			val = be32_to_cpup(prop);
			if (val >= resource_size(res)) {
				dev_err(&pdev->dev, "Invalid initdata-base.\n");
				return -EINVAL;
			}
			esca->initdata_base = val;
			esca->initdata =
				(struct esca_framework *)(esca->sram_base + val);
		} else {
			dev_err(&pdev->dev, "Missing initdata-base.\n");
			return -ENODEV;
		}

		prop = of_get_property(np, "fvmap_offset", &len);
		if (prop) {
			val = be32_to_cpup(prop);
			if (val >= resource_size(res)) {
				dev_err(&pdev->dev, "Invalid fvmap_offset.\n");
				return -EINVAL;
			}
			fvmap_base_address = esca->sram_base + val;
		}
	} else {
		dev_err(&pdev->dev, "Missing SRAM base\n");
		return -ENODEV;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "timer_apm");
	if (res) {
		esca->timer_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(esca->timer_base))
			dev_info(&pdev->dev, "could not find timer base addr \n");
	}

	prop = of_get_property(np, "board-info", &len);
	if (prop)
		esca->initdata->board_info = be32_to_cpup(prop);

	esca->dev = &pdev->dev;

	return ret;
}

static int esca_suspend(struct device *dev)
{
	/* Suspend callback function might be registered if necessary */
	esca_ktime_sram_sync();
	return 0;
}

static int esca_resume(struct device *dev)
{
	/* Resume callback function might be registered if necessary */
	esca_ktime_sram_sync();
	return 0;
}

static int esca_probe(struct platform_device *pdev)
{
	int ret = 0;

	dev_info(&pdev->dev, "%s+++\n", __func__);

	ret = esca_drv_dt_parse(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "DRV DT parse failed\n");
		return ret;
	}

	ret = esca_ipc_init(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "IPC init failed\n");
		return ret;
	}
	
	esca_dbg_init(pdev);

	dev_info(&pdev->dev, "%s---\n", __func__);

	return ret;
}

static int esca_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct dev_pm_ops esca_pm_ops = {
	.suspend	= esca_suspend,
	.resume		= esca_resume,
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
		.pm	= &esca_pm_ops,
	},
};

static int exynos_esca_init(void)
{
	platform_driver_register(&samsung_esca_driver);

	return 0;
}
postcore_initcall_sync(exynos_esca_init);

static void exynos_esca_exit(void)
{
	platform_driver_unregister(&samsung_esca_driver);
}
module_exit(exynos_esca_exit);

MODULE_LICENSE("GPL");
