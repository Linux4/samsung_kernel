/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/poll.h>
#include <linux/slab.h>

#include "hts_data.h"
#include "hts_core.h"
#include "hts_devfs.h"
#include "hts_sysfs.h"
#include "hts_vh.h"
#include "hts_ext.h"

static int initialize_hts(struct platform_device *pdev)
{
	int ret;

	ret = hts_external_initialize(pdev);
	if (ret)
		return ret;

	ret = register_hts_vh(pdev);
	if (ret)
		return ret;

	ret = initialize_hts_devfs(pdev);
	if (ret)
		return ret;

	ret = initialize_hts_sysfs(pdev);
	if (ret)
		return ret;

	ret = initialize_hts_core(pdev);
	if (ret)
		return ret;

	return 0;
}

static int hts_parse_dt(struct platform_device *pdev)
{
	struct device_node *np, *hts_reg, *child;
	struct hts_data *data = platform_get_drvdata(pdev);
	const char *buf;

	np = pdev->dev.of_node;
	hts_reg = of_find_node_by_name(np, "hts_reg");
	data->nr_reg = of_get_child_count(hts_reg);
	data->hts_reg = kzalloc(sizeof(struct hts_reg_data) * data->nr_reg, GFP_KERNEL);

	for_each_available_child_of_node(hts_reg, child) {
		int index;
		cpumask_t cpumask;

		if (of_property_read_u32(child, "index", &index)) {
			kfree(data->hts_reg);
			return -EINVAL;
		}
		if (of_property_read_string(child, "disable_cpus", &buf))
			cpumask_setall(&data->hts_reg[index].support_cpu);
		else {
			cpulist_parse(buf, &cpumask);
			cpumask_complement(&data->hts_reg[index].support_cpu, &cpumask);
		}

		dev_info(&pdev->dev, "REG: ectlr%d, support_cpu %*pbl\n", index, cpumask_pr_args(&data->hts_reg[index].support_cpu));
	}

	return 0;
}

static int hts_probe(struct platform_device *pdev)
{
	struct hts_data *data = NULL;
	int ret;

	data = kzalloc(sizeof(struct hts_data), GFP_KERNEL);
	if (data == NULL) {
		pr_err("HTS : Couldn't allocate device data");
		return -ENOMEM;
	}

	spin_lock_init(&data->lock);
	spin_lock_init(&data->req_lock);
	spin_lock_init(&data->app_event_lock);

	data->pdev = pdev;
	platform_set_drvdata(pdev, data);

	ret = hts_parse_dt(pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to parse dt (%d)\n", ret);
		kfree(data);
		data = NULL;
		return ret;
	}

	data->enable_emsmode = true;
	plist_head_init(&data->req_list);
	plist_node_init(&data->req_def_node, REQ_NONE);
	hts_update_request(&data->req_def_node, data, REQ_NONE);

	ret = initialize_hts(pdev);
	if (ret)
		goto err_initialize;

	data->probed = true;
	pr_info("HTS (H/W Tailoring System) has been probed");

	return 0;

err_initialize:
	kfree(data);

	return ret;
}

static const struct of_device_id of_hts_match[] = {
	{ .compatible = "samsung,hts", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_hts_match);

static struct platform_driver hts_driver = {
	.driver = {
		.name = "hts",
		.owner = THIS_MODULE,
		.of_match_table = of_hts_match,
	},
	.probe          = hts_probe,
};

static int __init hts_init(void)
{
	return platform_driver_register(&hts_driver);
}
arch_initcall(hts_init);

static void __exit hts_exit(void)
{
	platform_driver_unregister(&hts_driver);
}
module_exit(hts_exit);

MODULE_LICENSE("GPL");
