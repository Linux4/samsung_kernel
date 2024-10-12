/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include "mfc_plugin_ops.h"
#include "mfc_plugin_isr.h"

#include "mfc_plugin_hwlock.h"

#include "../base/mfc_sched.h"
#include "../base/mfc_common.h"

#define MFC_PLUGIN_NAME			"mfc_plugin"

void mfc_plugin_butler_worker(struct work_struct *work)
{
	struct mfc_core *core;

	core = container_of(work, struct mfc_core, butler_work);

	mfc_plugin_try_run(core);
}

static const struct mfc_core_ops mfc_plugin_ops = {
	.instance_init = mfc_plugin_instance_init,
	.instance_deinit = mfc_plugin_instance_deinit,
	.request_work = mfc_plugin_request_work,
	.instance_open = NULL,
	.instance_cache_flush = NULL,
	.instance_move_to = NULL,
	.instance_move_from = NULL,
	.instance_csd_parsing = NULL,
	.instance_dpb_flush = NULL,
	.instance_init_buf = NULL,
	.instance_q_flush = NULL,
	.instance_finishing = NULL,
};

static int __mfc_plugin_register_resource(struct platform_device *pdev, struct mfc_core *core)
{
	struct resource *res;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get memory region resource\n");
		return -ENOENT;
	}
	core->mfc_mem = request_mem_region(res->start, resource_size(res), pdev->name);
	if (core->mfc_mem == NULL) {
		dev_err(&pdev->dev, "failed to get memory region\n");
		return -ENOENT;
	}
	core->regs_base = ioremap(core->mfc_mem->start, resource_size(core->mfc_mem));
	if (core->regs_base == NULL) {
		dev_err(&pdev->dev, "failed to ioremap address region\n");
		goto err_ioremap;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get irq resource\n");
		goto err_res_irq;
	}
	core->irq = res->start;
	ret = devm_request_irq(&pdev->dev, core->irq, mfc_plugin_irq, 0, pdev->name, core);
	if (ret != 0) {
		dev_err(&pdev->dev, "failed to install irq (%d)\n", ret);
		goto err_res_irq;
	}

	return 0;

err_res_irq:
	iounmap(core->regs_base);

err_ioremap:
	release_mem_region(core->mfc_mem->start, resource_size(core->mfc_mem));
	return -ENOENT;
}

static int mfc_plugin_probe(struct platform_device *pdev)
{
	struct mfc_core *core;
	struct mfc_dev *dev;
	int ret = -ENOENT;

	dev_info(&pdev->dev, "%s is called\n", __func__);

	core = devm_kzalloc(&pdev->dev, sizeof(struct mfc_core), GFP_KERNEL);
	if (!core) {
		dev_err(&pdev->dev, "Not enough memory for MFC device\n");
		return -ENOMEM;
	}

	core->device = &pdev->dev;

	/* set core id */
	of_property_read_u32(core->device->of_node, "id", &core->id);
	snprintf(core->name, sizeof(core->name), "MFC%d", core->id);

	platform_set_drvdata(pdev, core);
	dev = dev_get_drvdata(pdev->dev.parent);
	dev->plugin = core;
	dev->num_subsystem++;
	core->dev = dev;
	core->core_ops = &mfc_plugin_ops;

	ret = __mfc_plugin_register_resource(pdev, core);
	if (ret)
		goto err_butler_wq;

	/* scheduler */
	mfc_sched_init_plugin(core);

	/* interrupt wait queue */
	init_waitqueue_head(&core->cmd_wq);

	/* hwlock */
	mfc_plugin_init_hwlock(core);

	/* plug-in butler worker */
	core->butler_wq = alloc_workqueue("mfc_plugin/butler", WQ_UNBOUND
					| WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	if (core->butler_wq == NULL) {
		dev_err(&pdev->dev, "failed to create workqueue for butler\n");
		goto err_butler_wq;
	}
	INIT_WORK(&core->butler_work, mfc_plugin_butler_worker);

	core->dump_ops = &mfc_plugin_dump_ops;

	mfc_core_info("%s is completed\n", core->name);

	return 0;

err_butler_wq:
	return ret;
}

static int mfc_plugin_remove(struct platform_device *pdev)
{
	struct mfc_core *core = platform_get_drvdata(pdev);

	mfc_core_info("++%s remove\n", core->name);

	iounmap(core->regs_base);
	release_mem_region(core->mfc_mem->start, resource_size(core->mfc_mem));

	flush_workqueue(core->butler_wq);
	destroy_workqueue(core->butler_wq);

	mfc_core_info("--%s remove\n", core->name);
	return 0;
}

static const struct of_device_id exynos_mfc_plugin_match[] = {
	{
		.compatible = "samsung,exynos-mfc_plugin",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_mfc_plugin_match);

struct platform_driver mfc_plugin_driver = {
	.probe		= mfc_plugin_probe,
	.remove		= mfc_plugin_remove,
	.driver	= {
		.name	= MFC_PLUGIN_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_mfc_plugin_match),
	},
};
