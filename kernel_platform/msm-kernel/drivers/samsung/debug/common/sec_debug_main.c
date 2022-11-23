// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2017-2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/of_early_populate.h>
#include <linux/samsung/debug/sec_debug.h>

#include "sec_debug.h"

static unsigned int sec_dbg_level __ro_after_init;
module_param_named(debug_level, sec_dbg_level, uint, 0440);

static unsigned int sec_dbg_force_upload __ro_after_init;
module_param_named(force_upload, sec_dbg_force_upload, uint, 0440);

static unsigned int enable __read_mostly = 1;
module_param_named(enable, enable, uint, 0644);

unsigned int sec_debug_level(void)
{
	return sec_dbg_level;
}
EXPORT_SYMBOL(sec_debug_level);

bool sec_debug_is_enabled(void)
{
	if (IS_ENABLED(CONFIG_UML))
		return true;

	switch (sec_dbg_level) {
	case SEC_DEBUG_LEVEL_LOW:
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	case SEC_DEBUG_LEVEL_MID:
#endif
		return !!sec_dbg_force_upload;
	}

	return !!enable;
}
EXPORT_SYMBOL(sec_debug_is_enabled);

static int __debug_parse_dt_panic_notifier_priority(struct builder *bd,
		struct device_node *np)
{
	struct sec_debug_drvdata *drvdata =
			container_of(bd, struct sec_debug_drvdata, bd);
	struct notifier_block *nb = &drvdata->nb_panic;
	s32 priority;
	int err;

	err = of_property_read_s32(np, "sec,panic_notifier-priority",
			&priority);
	if (err)
		return -EINVAL;

	nb->priority = (int)priority;

	return 0;
}

static struct dt_builder __debug_dt_builder[] = {
	DT_BUILDER(__debug_parse_dt_panic_notifier_priority),
};

static int __debug_parse_dt(struct builder *bd)
{
	return sec_director_parse_dt(bd, __debug_dt_builder,
			ARRAY_SIZE(__debug_dt_builder));
}

static int sec_debug_panic_notifer_handler(struct notifier_block *nb,
		unsigned long l, void *msg)
{
	sec_debug_show_stat((const char *)msg);

	return NOTIFY_OK;
}

static int __debug_register_panic_notifier(struct builder *bd)
{
	struct sec_debug_drvdata *drvdata =
			container_of(bd, struct sec_debug_drvdata, bd);
	struct notifier_block *nb = &drvdata->nb_panic;

	nb->notifier_call = sec_debug_panic_notifer_handler;

	return atomic_notifier_chain_register(&panic_notifier_list, nb);
}

static void __debug_unregister_panic_notifier(struct builder *bd)
{
	struct sec_debug_drvdata *drvdata =
			container_of(bd, struct sec_debug_drvdata, bd);
	struct notifier_block *nb = &drvdata->nb_panic;

	atomic_notifier_chain_unregister(&panic_notifier_list, nb);
}

static int __debug_probe_epilog(struct builder *bd)
{
	struct sec_debug_drvdata *drvdata =
			container_of(bd, struct sec_debug_drvdata, bd);
	struct device *dev = bd->dev;

	dev_set_drvdata(dev, drvdata);

	return 0;
}

static int __debug_probe(struct platform_device *pdev,
		const struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct sec_debug_drvdata *drvdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->bd.dev = dev;

	return sec_director_probe_dev(&drvdata->bd, builder, n);
}

static int __debug_remove(struct platform_device *pdev,
		const struct dev_builder *builder, ssize_t n)
{
	struct sec_debug_drvdata *drvdata = platform_get_drvdata(pdev);

	sec_director_destruct_dev(&drvdata->bd, builder, n, n);

	return 0;
}

static struct dev_builder __debug_dev_builder[] = {
	DEVICE_BUILDER(__debug_parse_dt, NULL),
	DEVICE_BUILDER(sec_user_fault_init, sec_user_fault_exit),
	DEVICE_BUILDER(sec_ap_serial_sysfs_init, sec_ap_serial_sysfs_exit),
	DEVICE_BUILDER(sec_force_err_init, sec_force_err_exit),
	DEVICE_BUILDER(sec_debug_node_init_dump_sink, NULL),
	DEVICE_BUILDER(__debug_register_panic_notifier,
		       __debug_unregister_panic_notifier),
	DEVICE_BUILDER(__debug_probe_epilog, NULL),
};

static int sec_debug_probe(struct platform_device *pdev)
{
	return __debug_probe(pdev, __debug_dev_builder,
			ARRAY_SIZE(__debug_dev_builder));
}

static int sec_debug_remove(struct platform_device *pdev)
{
	return __debug_remove(pdev, __debug_dev_builder,
			ARRAY_SIZE(__debug_dev_builder));
}

static const struct of_device_id sec_debug_match_table[] = {
	{ .compatible = "samsung,sec_debug" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_debug_match_table);

static struct platform_driver sec_debug_driver = {
	.driver = {
		.name = "samsung,sec_debug",
		.of_match_table = of_match_ptr(sec_debug_match_table),
	},
	.probe = sec_debug_probe,
	.remove = sec_debug_remove,
};

static int __init sec_debug_init(void)
{
	int err;

	err = platform_driver_register(&sec_debug_driver);
	if (err)
		return err;

	err = __of_platform_early_populate_init(sec_debug_match_table);
	if (err)
		return err;

	return 0;
}
core_initcall(sec_debug_init);

static void __exit sec_debug_exit(void)
{
	platform_driver_unregister(&sec_debug_driver);
}
module_exit(sec_debug_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("TN Debugging Feature");
MODULE_LICENSE("GPL v2");
