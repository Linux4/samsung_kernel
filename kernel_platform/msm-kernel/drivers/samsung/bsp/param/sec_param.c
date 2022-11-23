// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2011-2021 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/bsp/sec_param.h>

struct sec_param_drvdata {
	struct builder bd;
	struct sec_param_operations *ops;
};

static struct sec_param_drvdata *sec_param;

static __always_inline bool __param_is_probed(void)
{
	return !!sec_param;
}

static bool __param_get(size_t index, void *value)
{
	struct sec_param_operations *ops = sec_param->ops;

	if (!ops || !ops->read)
		return false;

	return ops->read(index, value);
}

bool sec_param_get(size_t index, void *value)
{
	if (!__param_is_probed())
		return false;

	return __param_get(index, value);
}
EXPORT_SYMBOL(sec_param_get);

static bool __param_set(size_t index, const void *value)
{
	struct sec_param_operations *ops = sec_param->ops;

	if (!ops || !ops->write)
		return false;

	return ops->write(index, value);
}

bool sec_param_set(size_t index, const void *value)
{
	if (!__param_is_probed())
		return false;

	return __param_set(index, value);
}
EXPORT_SYMBOL(sec_param_set);

int sec_param_register_operations(struct sec_param_operations *ops)
{
	if (sec_param->ops) {
		dev_warn(sec_param->bd.dev, "ops is already set (%p)\n",
				sec_param->ops);
		return -EPERM;
	}

	sec_param->ops = ops;

	return 0;
}
EXPORT_SYMBOL(sec_param_register_operations);

void sec_param_unregister_operations(struct sec_param_operations *ops)
{
	if (ops != sec_param->ops) {
		dev_warn(sec_param->bd.dev,
				"%p is not a registered ops.\n", ops);
		return;
	}

	sec_param->ops = NULL;
}
EXPORT_SYMBOL(sec_param_unregister_operations);

static int __param_probe_epilog(struct builder *bd)
{
	struct sec_param_drvdata *drvdata =
			container_of(bd, struct sec_param_drvdata, bd);
	struct device *dev = bd->dev;

	dev_set_drvdata(dev, drvdata);
	sec_param = drvdata;

	return 0;
}

static void __param_remove_prolog(struct builder *bd)
{
	/* FIXME: This is not a graceful exit. */
	sec_param = NULL;
}

static struct dev_builder __param_dev_builder[] = {
	DEVICE_BUILDER(__param_probe_epilog, __param_remove_prolog),
};

static int __param_probe(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct device *dev = &pdev->dev;
	struct sec_param_drvdata *drvdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->bd.dev = dev;

	return sec_director_probe_dev(&drvdata->bd, builder, n);
}

static int __param_remove(struct platform_device *pdev,
		struct dev_builder *builder, ssize_t n)
{
	struct sec_param_drvdata *drvdata = platform_get_drvdata(pdev);

	sec_director_destruct_dev(&drvdata->bd, builder, n, n);

	return 0;
}

static void __param_populate_child(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *parent;
	struct device_node *child;

	parent = pdev->dev.of_node;

	for_each_available_child_of_node(parent, child) {
		struct platform_device *cpdev;

		cpdev = of_platform_device_create(child, NULL, &pdev->dev);
		if (!cpdev) {
			dev_warn(dev, "failed to create %s\n!", child->name);
			of_node_put(child);
		}
	}
}

static int sec_param_probe(struct platform_device *pdev)
{
	int err;

	err = __param_probe(pdev, __param_dev_builder,
			ARRAY_SIZE(__param_dev_builder));
	if (err)
		goto err_probe;

	__param_populate_child(pdev);

err_probe:
	return err;
}

static int sec_param_remove(struct platform_device *pdev)
{
	return __param_remove(pdev, __param_dev_builder,
			ARRAY_SIZE(__param_dev_builder));
}

static const struct of_device_id sec_param_match_table[] = {
	{ .compatible = "samsung,param" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_param_match_table);

static struct platform_driver sec_param_driver = {
	.driver = {
		.name = "samsung,param",
		.of_match_table = of_match_ptr(sec_param_match_table),
	},
	.probe = sec_param_probe,
	.remove = sec_param_remove,
};

static int __init sec_param_init(void)
{
	return platform_driver_register(&sec_param_driver);
}
module_init(sec_param_init);

static void __exit sec_param_exit(void)
{
	platform_driver_unregister(&sec_param_driver);
}
module_exit(sec_param_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SEC PARAM driver");
MODULE_LICENSE("GPL v2");
