/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>

#include "sprd_adf_adapter.h"

static int sprd_adf_probe(struct platform_device *pdev)
{
	struct sprd_adf_context *adf_context = NULL;

	ADF_DEBUG_WARN("entern\n");
	adf_context = sprd_adf_context_create(pdev);
	if (!adf_context)
		ADF_DEBUG_WARN("sprd_adf_context_create faile\n");

	adf_context->early_suspend.suspend = sprd_adf_adapter_early_suspend;
	adf_context->early_suspend.resume  = sprd_adf_adapter_late_resume;
	adf_context->early_suspend.level   =
			EARLY_SUSPEND_LEVEL_DISABLE_FB + 30;
	register_early_suspend(&adf_context->early_suspend);

	platform_set_drvdata(pdev, adf_context);

	return 0;
}

static int sprd_adf_remove(struct platform_device *pdev)
{
	struct sprd_adf_context *adf_context = NULL;

	ADF_DEBUG_WARN("entern\n");
	adf_context = platform_get_drvdata(pdev);
	sprd_adf_context_destroy(adf_context);

	return 0;
}

static const struct of_device_id sprd_adf_dt_ids[] = {
	{ .compatible = "sprd,sprd-adf", },
	{}
};

static struct platform_driver sprd_adf_driver = {
	.probe = sprd_adf_probe,

	.remove = sprd_adf_remove,
	.shutdown = sprd_adf_adapter_shutdown,
	.driver = {
		.name = "sprd-adf",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sprd_adf_dt_ids),
	},
};

static int __init sprd_adf_init(void)
{
	return platform_driver_register(&sprd_adf_driver);
}

static void __exit sprd_adf_exit(void)
{
	return platform_driver_unregister(&sprd_adf_driver);
}

module_init(sprd_adf_init);
module_exit(sprd_adf_exit);
