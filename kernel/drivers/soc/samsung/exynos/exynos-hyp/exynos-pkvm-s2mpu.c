/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS Stage 2 Protection Unit(S2MPU) Module for pKVM
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/kvm_host.h>

#include <soc/samsung/exynos/exynos-hyp.h>


struct s2mpu_drvdata {
	struct device *dev;
	void __iomem *sfrbase;
	bool registered;
};

static int exynos_pkvm_s2mpu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct s2mpu_drvdata *data;
	struct resource *res;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get resource info\n");
		return -ENOENT;
	}

	data->sfrbase = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->sfrbase)) {
		dev_err(dev, "failed to ioremap resource: %ld\n",
				PTR_ERR(data->sfrbase));
		return PTR_ERR(data->sfrbase);
	}

	ret = exynos_pkvm_s2mpu_register(dev, res->start);
	if (ret && ret != -ENODEV) {
		dev_err(dev, "failed to register: %d\n", ret);
		return ret;
	}

	data->registered = (ret != -ENODEV);
	if (!data->registered) {
		dev_info(dev, "pkvm-S2MPU driver disabled, pKVM not enabled\n");
		goto out_free_mem;
	}

	platform_set_drvdata(pdev, data);

	dev_notice(dev, "S2MPU device registration done ret[%d]\n", ret);

	return 0;

out_free_mem:
	kfree(data);

	return 0;
}

static const struct of_device_id exynos_pkvm_s2mpu_of_match[] = {
	{ .compatible = "samsung,pkvm-s2mpu" },
	{},
};

static struct platform_driver exynos_pkvm_s2mpu_driver = {
	.probe = exynos_pkvm_s2mpu_probe,
	.driver = {
		.name = "exynos-pkvm-s2mpu",
		.of_match_table = exynos_pkvm_s2mpu_of_match,
	},
};

module_platform_driver(exynos_pkvm_s2mpu_driver);

MODULE_DESCRIPTION("Exynos Hypervisor S2MPU driver for pKVM");
MODULE_LICENSE("GPL");
