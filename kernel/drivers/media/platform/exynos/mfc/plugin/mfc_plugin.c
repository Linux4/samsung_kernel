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
#include <linux/iommu.h>

#include "mfc_plugin_ops.h"
#include "mfc_plugin_isr.h"

#include "mfc_plugin_hwlock.h"

#include "base/mfc_sched.h"
#include "base/mfc_common.h"

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

	core->irq = platform_get_irq(pdev, 0);
	if (core->irq < 0) {
		dev_err(&pdev->dev, "failed to get irq\n");
		goto err_res_irq;
	}

	ret = devm_request_irq(&pdev->dev, core->irq, mfc_plugin_irq, 0, pdev->name, core);
	if (ret) {
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

int mfc_plugin_sysmmu_fault_handler(struct iommu_fault *fault, void *param)
{
	struct mfc_core *core = (struct mfc_core *)param;

	mfc_core_err("MFC-%d SysMMU PAGE FAULT at %#llx, fault_status: %#x\n",
			core->id, fault->event.addr, fault->event.reason);
	MFC_TRACE_CORE("MFC-%d SysMMU PAGE FAULT at %#llx\n",
			core->id, fault->event.addr);

	call_dop(core, dump_info, core);

	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
static int __mfc_plugin_s2mpu_fault_notifier(struct s2mpu_notifier_block *nb_block,
					     struct s2mpu_notifier_info *nb_info)
{
	struct mfc_core *core;

	if (strncmp("MFC2", nb_info->subsystem, strlen("MFC2")) == 0) {
		core = container_of(nb_block, struct mfc_core, mfc_s2mpu_nb);
	} else if (strncmp("TZMP2_MFC2", nb_info->subsystem, strlen("TZMP2_MFC2")) == 0) {
		core = container_of(nb_block, struct mfc_core, mfc_s2mpu_nb_drm);
	} else if (strncmp("FGN", nb_info->subsystem, strlen("FGN")) == 0) {
		core = container_of(nb_block, struct mfc_core, mfc_s2mpu_nb);
	} else if (strncmp("TZMP2_FGN", nb_info->subsystem, strlen("TZMP2_FGN")) == 0) {
		core = container_of(nb_block, struct mfc_core, mfc_s2mpu_nb_drm);
	} else {
		pr_err("%s: It is not MFC block(name: %s)\n",
		       __func__, nb_block->subsystem);
		return S2MPU_NOTIFY_OK;
	}

	dev_err(core->device, "__mfc_core_s2mpu_fault_notifier: +\n");
	call_dop(core, dump_info, core);
	dev_err(core->device, "__mfc_core_s2mpu_fault_notifier: -\n");

	return S2MPU_NOTIFY_BAD;
}
#endif

static int mfc_plugin_probe(struct platform_device *pdev)
{
	struct mfc_core *core;
	struct mfc_dev *dev;
	int ret = -ENOENT;
	int i;
	const char *susbsystem_name;

	dev_info(&pdev->dev, "%s is called\n", __func__);

	core = devm_kzalloc(&pdev->dev, sizeof(struct mfc_core), GFP_KERNEL);
	if (!core) {
		dev_err(&pdev->dev, "Not enough memory for MFC device\n");
		return -ENOMEM;
	}

	core->device = &pdev->dev;

	/* set core id */
	of_property_read_u32(core->device->of_node, "id", &core->id);

	if (of_property_read_string(core->device->of_node, "subsystem", &susbsystem_name))
		snprintf(core->name, sizeof(core->name), "MFC%d", core->id);
	else
		snprintf(core->name, sizeof(core->name), "%s", susbsystem_name);

	pr_info("mfc subsystem:%s", core->name);
	platform_set_drvdata(pdev, core);
	dev = dev_get_drvdata(pdev->dev.parent);
	dev->plugin = core;
	dev->num_subsystem++;
	core->dev = dev;
	core->core_ops = &mfc_plugin_ops;

	ret = __mfc_plugin_register_resource(pdev, core);
	if (ret)
		goto err_butler_wq;

	if (dev->pdata->support_fg_shadow) {
		core->fg_q_handle = devm_kzalloc(&pdev->dev, sizeof(fg_queue_handle), GFP_KERNEL);
		if (!core->fg_q_handle) {
			dev_err(&pdev->dev, "Failed to allocate fg_q_handle\n");
			ret = -ENOMEM;
			goto err_butler_wq;
		}
		spin_lock_init(&core->fg_q_handle->lock);
		core->fg_q_enable = true;

		for (i = 0; i < MFC_FG_NUM_SHADOW_REGS; i++)
			core->fg_q_handle->ctx_num_table[i] = -1;
	}

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

	ret = iommu_register_device_fault_handler(core->device,
			mfc_plugin_sysmmu_fault_handler, core);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to register sysmmu fault handler(%d)\n", ret);
		goto err_register_fault_handler;
	}

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
	core->mfc_s2mpu_nb.subsystem = core->name;
	core->mfc_s2mpu_nb.notifier_call = __mfc_plugin_s2mpu_fault_notifier;
	exynos_s2mpu_notifier_call_register(&core->mfc_s2mpu_nb);

	/*
	 * Even though plugin core don't have firmware,
	 * save the name of drm into drm_fw_buf.
	 * The drm name should be "TZMP2_MFC%d" for S2MPU
	 */
	snprintf(core->drm_fw_buf.name, MFC_NUM_SPECIAL_BUF_NAME, "TZMP2_%s", core->name);
	core->mfc_s2mpu_nb_drm.subsystem = core->drm_fw_buf.name;
	core->mfc_s2mpu_nb_drm.notifier_call = __mfc_plugin_s2mpu_fault_notifier;
	exynos_s2mpu_notifier_call_register(&core->mfc_s2mpu_nb_drm);
#endif

	mfc_core_info("%s is completed\n", core->name);

	return 0;

err_register_fault_handler:
	destroy_workqueue(core->butler_wq);
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

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int mfc_plugin_suspend(struct device *device)
{
	struct mfc_core *core = platform_get_drvdata(to_platform_device(device));

	if (!core) {
		dev_err(device, "no mfc device to run\n");
		return -EINVAL;
	}

	mfc_core_debug(2, "MFC suspend will be handled by main driver\n");

	return 0;
}

static int mfc_plugin_resume(struct device *device)
{
	struct mfc_core *core = platform_get_drvdata(to_platform_device(device));

	if (!core) {
		dev_err(device, "no mfc core to run\n");
		return -EINVAL;
	}

	mfc_core_debug(2, "MFC resume will be handled by main driver\n");

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_PM)
static int mfc_plugin_runtime_suspend(struct device *device)
{
	struct mfc_core *core = platform_get_drvdata(to_platform_device(device));

	mfc_core_debug(3, "mfc runtime suspend\n");

	return 0;
}

static int mfc_plugin_runtime_resume(struct device *device)
{
	struct mfc_core *core = platform_get_drvdata(to_platform_device(device));

	mfc_core_debug(3, "mfc runtime resume\n");

	return 0;
}
#endif

static const struct dev_pm_ops mfc_plugin_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mfc_plugin_suspend, mfc_plugin_resume)
	SET_RUNTIME_PM_OPS(
			mfc_plugin_runtime_suspend,
			mfc_plugin_runtime_resume,
			NULL
	)
};

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
		.pm	= &mfc_plugin_pm_ops,
		.of_match_table = of_match_ptr(exynos_mfc_plugin_match),
	},
};
