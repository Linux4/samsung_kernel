// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "pablo-lib.h"
#include "pablo-irq.h"
#include "pablo-debug.h"

static struct pablo_lib_platform_data *pl_platform_data;
struct pablo_lib_platform_data *pablo_lib_get_platform_data(void)
{
	return pl_platform_data;
}
EXPORT_SYMBOL_GPL(pablo_lib_get_platform_data);

static char *stream_prefix;
char *pablo_lib_get_stream_prefix(int instance)
{
	return &stream_prefix[instance * IS_STR_LEN];
}
EXPORT_SYMBOL_GPL(pablo_lib_get_stream_prefix);

void pablo_lib_set_stream_prefix(int instance, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsnprintf(&stream_prefix[instance * IS_STR_LEN], IS_STR_LEN, fmt, args);
	va_end(args);
}
EXPORT_SYMBOL_GPL(pablo_lib_set_stream_prefix);

static int parse_cpu_data(struct device *dev, struct pablo_lib_platform_data *plpd)
{
	struct device_node *cluster_np = of_get_child_by_name(dev->of_node, "cpus");

	if (cluster_np) {
		of_property_read_u32(cluster_np, "cluster0", &plpd->cpu_cluster[PABLO_CPU_CLUSTER_LIT]);
		of_property_read_u32(cluster_np, "cluster1", &plpd->cpu_cluster[PABLO_CPU_CLUSTER_MID]);
		of_property_read_u32(cluster_np, "cluster2", &plpd->cpu_cluster[PABLO_CPU_CLUSTER_BIG]);
	} else {
		plpd->cpu_cluster[PABLO_CPU_CLUSTER_LIT] = 0;
		plpd->cpu_cluster[PABLO_CPU_CLUSTER_MID] = 4;
		plpd->cpu_cluster[PABLO_CPU_CLUSTER_BIG] = 7;
	}

	dev_info(dev, "cpu_cluster(%d, %d, %d)",
		plpd->cpu_cluster[PABLO_CPU_CLUSTER_LIT],
		plpd->cpu_cluster[PABLO_CPU_CLUSTER_MID],
		plpd->cpu_cluster[PABLO_CPU_CLUSTER_BIG]);

	return 0;
}

static void parse_multi_target_range(struct device *dev, struct pablo_lib_platform_data *plpd)
{
	struct device_node *np = dev->of_node;
	int ret;

	ret = of_property_read_u32_array(np, "multi-target-range", plpd->mtarget_range, PABLO_MTARGET_RMAX);
	if (ret) {
		plpd->mtarget_range[PABLO_MTARGET_RSTART] = 0;
		plpd->mtarget_range[PABLO_MTARGET_REND] = plpd->cpu_cluster[PABLO_CPU_CLUSTER_MID] - 1;
	}

	dev_info(dev, "multi-target-range(%d-%d)", plpd->mtarget_range[PABLO_MTARGET_RSTART],
		plpd->mtarget_range[PABLO_MTARGET_REND]);
}

static int parse_memlog_buf_size_data(struct device *dev, struct pablo_lib_platform_data *plpd)
{
	struct device_node *np = dev->of_node;
	int ret;

	ret = of_property_read_u32_array(np, "memlog_size", plpd->memlog_size, 2);
	if (ret) {
		plpd->memlog_size[PABLO_MEMLOG_DRV] = IS_MEMLOG_PRINTF_DRV_SIZE;
		plpd->memlog_size[PABLO_MEMLOG_DDK] = IS_MEMLOG_PRINTF_DDK_SIZE;
	}

	dev_info(dev, "ret(%d) memlog_size(%#08x, %#08x)", ret,
		plpd->memlog_size[0], plpd->memlog_size[1]);

	return 0;
}

static int pablo_lib_probe(struct platform_device *pdev,
		struct pablo_lib_data *data)
{
	int ret;
	struct device *dev = &pdev->dev;

	pl_platform_data = devm_kzalloc(dev, sizeof(struct pablo_lib_platform_data), GFP_KERNEL);
	if (!pl_platform_data) {
		dev_err(dev, "no memory for platform data\n");
		return -ENOMEM;
	}

	stream_prefix = devm_kcalloc(dev, IS_STREAM_COUNT, sizeof(char) * IS_STR_LEN, GFP_KERNEL);
	if (!stream_prefix) {
		dev_err(dev, "no memory for stream_prefix\n");
		ret = -ENOMEM;
		goto err_alloc_stream_prefix;
	}

	parse_cpu_data(dev, pl_platform_data);
	parse_memlog_buf_size_data(dev, pl_platform_data);
	parse_multi_target_range(dev, pl_platform_data);

	is_debug_probe(dev);

	ret = pablo_irq_probe(dev);
	if (ret)
		return ret;

	dev_info(dev, "%s done\n", __func__);

	return 0;

err_alloc_stream_prefix:
	devm_kfree(dev, pl_platform_data);

	return ret;
}

static struct pablo_lib_data lib_data = {
	.probe = pablo_lib_probe,
};

static const struct of_device_id pablo_lib_of_table[] = {
	{
		.name = "pablo-lib",
		.compatible = "samsung,pablo-lib-v1",
		.data = &lib_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, pablo_lib_of_table);

static int pablo_lib_pdev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id;
	struct pablo_lib_data *data;

	of_id = of_match_device(of_match_ptr(pablo_lib_of_table), dev);
	if (!of_id)
		return -EINVAL;

	data = (struct pablo_lib_data *)of_id->data;

	return data->probe(pdev, data);
}

static int pablo_lib_pdev_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id;
	struct pablo_lib_data *data;

	of_id = of_match_device(of_match_ptr(pablo_lib_of_table), dev);
	if (!of_id)
		return -EINVAL;

	data = (struct pablo_lib_data *)of_id->data;

	return data->remove(pdev, data);
}

static struct platform_driver pablo_lib_driver = {
	.probe = pablo_lib_pdev_probe,
	.remove = pablo_lib_pdev_remove,
	.driver = {
		.name		= "pablo-lib",
		.owner		= THIS_MODULE,
		.of_match_table	= pablo_lib_of_table,
	}
};

#ifdef MODULE
static int pablo_lib_init(void)
{
	int ret;

	ret = platform_driver_register(&pablo_lib_driver);
	if (ret)
		pr_err("%s: platform_driver_register is failed(%d)", __func__, ret);

	return ret;
}
module_init(pablo_lib_init);

void pablo_lib_exit(void)
{
	platform_driver_unregister(&pablo_lib_driver);
}
module_exit(pablo_lib_exit);
#else
static int __init pablo_lib_init(void)
{
	int ret;

	ret = platform_driver_probe(&pablo_lib_driver,
		pablo_lib_pdev_probe);
	if (ret)
		pr_err("%s: platform_driver_probe is failed(%d)", __func__, ret);

	return ret;
}

device_initcall_sync(pablo_lib_init);
#endif

MODULE_DESCRIPTION("Samsung EXYNOS SoC Pablo Lib driver");
MODULE_ALIAS("platform:samsung-pablo-lib");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
