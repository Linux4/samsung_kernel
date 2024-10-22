// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include "apusys_core.h"

/* SMMU device driver for AOV APU */
static int apusys_aov_smmu_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s probed\n", __func__);
	return 0;
}

static int apusys_aov_smmu_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s removed\n", __func__);
	return 0;
}

static const struct of_device_id apusys_aov_smmu_of_match[] = {
	{ .compatible = "mediatek,apusys-aov-smmu-device"},
	{},
};
MODULE_DEVICE_TABLE(of, apusys_aov_smmu_of_match);

static struct platform_driver apusys_aov_smmu_driver = {
	.probe = apusys_aov_smmu_probe,
	.remove = apusys_aov_smmu_remove,
	.driver	= {
		.name = "apusys_aov_smmu",
		.owner = THIS_MODULE,
		.of_match_table = apusys_aov_smmu_of_match,
	},
};

/* SMMU device driver for Secure APU */
static int secure_apu_smmu_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s probed\n", __func__);
	return 0;
}

static int secure_apu_smmu_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s removed\n", __func__);
	return 0;
}

static const struct of_device_id secure_apu_smmu_of_match[] = {
	{ .compatible = "mediatek,secure-apu-smmu-device"},
	{},
};
MODULE_DEVICE_TABLE(of, secure_apu_smmu_of_match);

static struct platform_driver secure_apu_smmu_driver = {
	.probe = secure_apu_smmu_probe,
	.remove = secure_apu_smmu_remove,
	.driver	= {
		.name = "secure_apu_smmu",
		.owner = THIS_MODULE,
		.of_match_table = secure_apu_smmu_of_match,
	},
};

int apu_smmu_device_init(struct apusys_core_info *info)
{
	int ret = 0;

	pr_info("%s apu_smmu init\n", __func__);

	ret = platform_driver_register(&apusys_aov_smmu_driver);
	if (ret)
		pr_info("%s Failed to register apusys_aov_smmu, ret %d\n", __func__, ret);

	ret = platform_driver_register(&secure_apu_smmu_driver);
	if (ret)
		pr_info("%s Failed to register secure_apu_smmu, ret %d\n", __func__, ret);

	return 0;
}

void apu_smmu_device_exit(void)
{
	platform_driver_unregister(&apusys_aov_smmu_driver);
	platform_driver_unregister(&secure_apu_smmu_driver);
}
