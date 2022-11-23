// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2021 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include <linux/samsung/builder_pattern.h>

#include "sec_qc_debug.h"

static int __qc_debug_parse_dt_reboot_notifier_priority(struct builder *bd,
		struct device_node *np)
{
	struct qc_debug_drvdata *drvdata =
			container_of(bd, struct qc_debug_drvdata, bd);
	s32 priority;
	int err;

	err = of_property_read_s32(np, "sec,reboot_notifier-priority",
			&priority);
	if (err)
		return -EINVAL;

	drvdata->reboot_nb.priority = (int)priority;

	return 0;
}

static int __qc_debug_parse_dt_use_store_last_kmsg(struct builder *bd,
		struct device_node *np)
{
	struct qc_debug_drvdata *drvdata =
			container_of(bd, struct qc_debug_drvdata, bd);

	drvdata->use_store_last_kmsg =
			of_property_read_bool(np, "sec,use-store_last_kmsg");

	return 0;
}

static int __qc_debug_parse_dt_use_store_lpm_kmsg(struct builder *bd,
		struct device_node *np)
{
	struct qc_debug_drvdata *drvdata =
			container_of(bd, struct qc_debug_drvdata, bd);

	drvdata->use_store_lpm_kmsg =
			of_property_read_bool(np, "sec,use-store_lpm_kmsg");

	return 0;
}

static int __qc_debug_parse_dt_use_store_onoff_history(struct builder *bd,
		struct device_node *np)
{
	struct qc_debug_drvdata *drvdata =
			container_of(bd, struct qc_debug_drvdata, bd);

	drvdata->use_store_onoff_history =
			of_property_read_bool(np, "sec,use-store_onoff_history");

	return 0;
}

static struct dt_builder __qc_debug_dt_builder[] = {
	DT_BUILDER(__qc_debug_parse_dt_reboot_notifier_priority),
	DT_BUILDER(__qc_debug_parse_dt_use_store_last_kmsg),
	DT_BUILDER(__qc_debug_parse_dt_use_store_lpm_kmsg),
	DT_BUILDER(__qc_debug_parse_dt_use_store_onoff_history)
};

static int __qc_debug_parse_dt(struct builder *bd)
{
	return sec_director_parse_dt(bd, __qc_debug_dt_builder,
			ARRAY_SIZE(__qc_debug_dt_builder));
}

static struct dev_builder __qc_debug_dev_builder[] = {
	DEVICE_BUILDER(__qc_debug_parse_dt, NULL),
	DEVICE_BUILDER(sec_qc_debug_lpm_log_init, NULL),
	DEVICE_BUILDER(sec_qc_debug_reboot_init, sec_qc_debug_reboot_exit),
	DEVICE_BUILDER(sec_qc_force_err_init, sec_qc_force_err_exit),
	DEVICE_BUILDER(sec_qc_boot_stat_init, sec_qc_boot_stat_exit),
};

static int __qc_debug_probe(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct qc_debug_drvdata *drvdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->bd.dev = dev;

	return sec_director_probe_dev(&drvdata->bd, builder, n);
}

static int __qc_debug_remove(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct qc_debug_drvdata *drvdata = platform_get_drvdata(pdev);

	sec_director_destruct_dev(&drvdata->bd, builder, n, n);

	return 0;
}

static int sec_qc_debug_probe(struct platform_device *pdev)
{
	return __qc_debug_probe(pdev, __qc_debug_dev_builder,
			ARRAY_SIZE(__qc_debug_dev_builder));
}

static int sec_qc_debug_remove(struct platform_device *pdev)
{
	return __qc_debug_remove(pdev, __qc_debug_dev_builder,
			ARRAY_SIZE(__qc_debug_dev_builder));
}

static const struct of_device_id sec_qc_debug_match_table[] = {
	{ .compatible = "samsung,qcom-debug" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_qc_debug_match_table);

static struct platform_driver sec_qc_debug_driver = {
	.driver = {
		.name = "samsung,qcom-debug",
		.of_match_table = of_match_ptr(sec_qc_debug_match_table),
	},
	.probe = sec_qc_debug_probe,
	.remove = sec_qc_debug_remove,
};

static int __init sec_qc_debug_init(void)
{
	return platform_driver_register(&sec_qc_debug_driver);
}
module_init(sec_qc_debug_init);

static void __exit sec_qc_debug_exit(void)
{
	platform_driver_unregister(&sec_qc_debug_driver);
}
module_exit(sec_qc_debug_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Common debugging feature for Qualcomm based devices");
MODULE_LICENSE("GPL v2");
