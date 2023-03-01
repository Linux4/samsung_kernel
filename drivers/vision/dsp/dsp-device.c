// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>

#include "dsp-log.h"
#include "hardware/dsp-system.h"
#include "dsp-device.h"

struct device *dsp_global_dev;

int dsp_device_npu_start(bool boot, dma_addr_t fw_iova)
{
	int ret;
	struct dsp_device *dspdev;

	dsp_enter();
	dspdev = dev_get_drvdata(dsp_global_dev);
	mutex_lock(&dspdev->lock);
	ret = dsp_system_npu_start(&dspdev->system, boot, fw_iova);
	mutex_unlock(&dspdev->lock);
	dsp_leave();
	return ret;
}

static int __attribute__((unused)) dsp_fault_handler(
		struct iommu_domain *domain, struct device *dev,
		unsigned long fault_addr, int fault_flag, void *token)
{
	struct dsp_device *dspdev;

	dsp_enter();
	dspdev = dev_get_drvdata(dev);

	if (dsp_device_power_active(dspdev)) {
		dsp_err("Invalid access to device virtual(0x%lX)\n",
				fault_addr);
		dsp_system_iovmm_fault_dump(&dspdev->system);
	}

	dsp_leave();
	return 0;
}

#if defined(CONFIG_PM_SLEEP)
static int dsp_device_resume(struct device *dev)
{
	int ret = 0;
	struct dsp_device *dspdev;

	dsp_enter();
	dspdev = dev_get_drvdata(dev);

	if (dsp_device_power_active(dspdev))
		ret = dsp_system_resume(&dspdev->system);

	dsp_leave();
	return ret;
}

static int dsp_device_suspend(struct device *dev)
{
	int ret = 0;
	struct dsp_device *dspdev;

	dsp_enter();
	dspdev = dev_get_drvdata(dev);

	if (dsp_device_power_active(dspdev))
		ret = dsp_system_suspend(&dspdev->system);

	dsp_leave();
	return ret;
}

#endif

static int dsp_device_runtime_resume(struct device *dev)
{
	int ret;
	struct dsp_device *dspdev;

	dsp_enter();
	dspdev = dev_get_drvdata(dev);

	ret = dsp_system_runtime_resume(&dspdev->system);
	if (ret)
		goto p_err_system;

	dsp_leave();
	return 0;
p_err_system:
	return ret;
}

static int dsp_device_runtime_suspend(struct device *dev)
{
	int ret;
	struct dsp_device *dspdev;

	dsp_enter();
	dspdev = dev_get_drvdata(dev);

	ret = dsp_system_runtime_suspend(&dspdev->system);
	if (ret)
		goto p_err_system;

	dsp_leave();
	return 0;
p_err_system:
	return ret;
}

static const struct dev_pm_ops dsp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
			dsp_device_suspend,
			dsp_device_resume)
	SET_RUNTIME_PM_OPS(
			dsp_device_runtime_suspend,
			dsp_device_runtime_resume,
			NULL)
};

int dsp_device_power_active(struct dsp_device *dspdev)
{
	dsp_check();
	return dsp_system_power_active(&dspdev->system);
}

int dsp_device_power_on(struct dsp_device *dspdev)
{
	int ret;

	dsp_enter();
	if (dsp_device_power_active(dspdev)) {
		dsp_warn("Duplicated request to enable power(%u)",
				dspdev->power_count++);
		return 0;
	}

#if defined(CONFIG_PM)
	ret = pm_runtime_get_sync(dspdev->dev);
	if (ret) {
		dsp_err("Failed to get runtime_pm(%d)\n", ret);
		goto p_err;
	}
#else
	ret = dsp_device_runtime_resume(dspdev->dev);
	if (ret)
		goto p_err;
#endif

	dspdev->power_count = 1;
	dsp_leave();
	return 0;
p_err:
	dspdev->power_count = 0;
	return ret;
}

void dsp_device_power_off(struct dsp_device *dspdev)
{
	dsp_enter();
	if (!dspdev->power_count) {
		dsp_warn("power is already disabled\n");
	} else if (dspdev->power_count == 1) {
#if defined(CONFIG_PM)
		pm_runtime_put_sync(dspdev->dev);
#else
		dsp_device_runtime_suspend(dspdev->dev);
#endif
		dspdev->power_count = 0;
	} else {
		dsp_warn("The power reference count is cleared(%u)\n",
				dspdev->power_count--);
	}
	dsp_leave();
}

int dsp_device_start(struct dsp_device *dspdev)
{
	int ret;

	dsp_enter();
	mutex_lock(&dspdev->lock);
	if (!dspdev->open_count) {
		ret = -ENOSTR;
		dsp_err("device is not opened\n");
		goto p_err_open;
	}

	if (dspdev->start_count) {
		dspdev->start_count++;
		dsp_info("start count is incresed(%u/%u)\n",
				dspdev->open_count, dspdev->start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	ret = dsp_system_start(&dspdev->system);
	if (ret)
		goto p_err_system;

	ret = dsp_device_power_on(dspdev);
	if (ret)
		goto p_err_power;

	ret = dsp_system_boot(&dspdev->system);
	if (ret)
		goto p_err_boot;

	dspdev->start_count = 1;
	dsp_info("device is started(%u/%u)\n",
			dspdev->open_count, dspdev->start_count);
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return 0;
p_err_boot:
	dsp_device_power_off(dspdev);
p_err_power:
	dsp_system_stop(&dspdev->system);
p_err_system:
p_err_open:
	mutex_unlock(&dspdev->lock);
	return ret;
}

static int __dsp_device_stop(struct dsp_device *dspdev)
{
	dsp_enter();
	dsp_system_reset(&dspdev->system);
	dsp_device_power_off(dspdev);
	dsp_system_stop(&dspdev->system);
	dspdev->start_count = 0;
	dsp_leave();
	return 0;
}

int dsp_device_stop(struct dsp_device *dspdev, unsigned int count)
{
	dsp_enter();
	mutex_lock(&dspdev->lock);
	if (!dspdev->start_count) {
		dsp_warn("device already stopped(%u/%u)\n",
				dspdev->open_count, dspdev->start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	if (dspdev->start_count > count) {
		dspdev->start_count -= count;
		dsp_info("start count is decreased(%u/%u)\n",
				dspdev->open_count, dspdev->start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	__dsp_device_stop(dspdev);

	dsp_info("device stopped(%u/%u)\n",
			dspdev->open_count, dspdev->start_count);
	mutex_unlock(&dspdev->lock);
	dsp_leave();
	return 0;
}

int dsp_device_open(struct dsp_device *dspdev)
{
	int ret;

	dsp_enter();
	mutex_lock(&dspdev->lock);
	if (dspdev->open_count) {
		dspdev->open_count++;
		dsp_info("open count is incresed(%u/%u)\n",
				dspdev->open_count, dspdev->start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	ret = dsp_debug_open(&dspdev->debug);
	if (ret)
		goto p_err_debug;

	ret = dsp_system_open(&dspdev->system);
	if (ret)
		goto p_err_system;

	dspdev->open_count = 1;
	dspdev->start_count = 0;
	dsp_info("device is opened(%u/%u)\n",
			dspdev->open_count, dspdev->start_count);
	mutex_unlock(&dspdev->lock);

	dsp_leave();
	return 0;
p_err_system:
	dsp_debug_close(&dspdev->debug);
p_err_debug:
	mutex_unlock(&dspdev->lock);
	return ret;
}

int dsp_device_close(struct dsp_device *dspdev)
{
	dsp_enter();
	mutex_lock(&dspdev->lock);
	if (!dspdev->open_count) {
		dsp_warn("device is already closed(%u/%u)\n",
				dspdev->open_count, dspdev->start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	if (dspdev->open_count != 1) {
		dspdev->open_count--;
		dsp_info("open count is decreased(%u/%u)\n",
				dspdev->open_count, dspdev->start_count);
		mutex_unlock(&dspdev->lock);
		return 0;
	}

	if (dspdev->start_count) {
		dsp_warn("device will forcibly stop(%u/%u)\n",
				dspdev->open_count, dspdev->start_count);
		dspdev->start_count = 1;
		__dsp_device_stop(dspdev);
	}

	dsp_system_close(&dspdev->system);
	dsp_debug_close(&dspdev->debug);

	dspdev->open_count = 0;
	dsp_info("device is closed(%u/%u)\n",
			dspdev->open_count, dspdev->start_count);
	mutex_unlock(&dspdev->lock);
	dsp_leave();
	return 0;
}

static int dsp_device_probe(struct platform_device *pdev)
{
	int ret;
	struct dsp_device *dspdev;

	dsp_enter();
	dsp_global_dev = &pdev->dev;
	dev_set_socdata(&pdev->dev, "Exynos", "DSP");

	dspdev = devm_kzalloc(&pdev->dev, sizeof(*dspdev), GFP_KERNEL);
	if (!dspdev) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc dsp_device\n");
		goto p_exit;
	}

	get_device(&pdev->dev);
	dev_set_drvdata(&pdev->dev, dspdev);
	dspdev->dev = &pdev->dev;

	ret = dsp_debug_probe(dspdev);
	if (ret)
		goto p_err_debug;

	ret = dsp_system_probe(dspdev);
	if (ret)
		goto p_err_system;

	ret = dsp_core_probe(dspdev);
	if (ret)
		goto p_err_core;

	iovmm_set_fault_handler(dspdev->dev, dsp_fault_handler, NULL);
	ret = iovmm_activate(dspdev->dev);
	if (ret) {
		dsp_err("Failed to activate iovmm(%d)\n", ret);
		goto p_err_iovmm;
	}

	mutex_init(&dspdev->lock);
	dspdev->open_count = 0;

	dsp_leave();
	dsp_info("dsp initialization completed\n");
	return 0;
p_err_iovmm:
	dsp_core_remove(&dspdev->core);
p_err_core:
	dsp_system_remove(&dspdev->system);
p_err_system:
	dsp_debug_remove(&dspdev->debug);
p_err_debug:
	put_device(&pdev->dev);
	devm_kfree(&pdev->dev, dspdev);
p_exit:
	return ret;
}

static int dsp_device_remove(struct platform_device *pdev)
{
	struct dsp_device *dspdev;

	dsp_enter();
	dspdev = dev_get_drvdata(&pdev->dev);

	iovmm_deactivate(dspdev->dev);
	dsp_core_remove(&dspdev->core);
	dsp_system_remove(&dspdev->system);
	dsp_debug_remove(&dspdev->debug);
	put_device(dspdev->dev);
	devm_kfree(dspdev->dev, dspdev);
	dsp_leave();
	return 0;
}

static void dsp_device_shutdown(struct platform_device *pdev)
{
	struct dsp_device *dspdev;

	dsp_enter();
	dspdev = dev_get_drvdata(&pdev->dev);
	dsp_leave();
}

#if defined(CONFIG_OF)
static const struct of_device_id exynos_dsp_match[] = {
	{
		.compatible = "samsung,exynos-dsp",
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_dsp_match);
#endif

static struct platform_driver dsp_driver = {
	.probe          = dsp_device_probe,
	.remove         = dsp_device_remove,
	.shutdown       = dsp_device_shutdown,
	.driver = {
		.name   = "exynos-dsp",
		.owner  = THIS_MODULE,
		.pm     = &dsp_pm_ops,
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(exynos_dsp_match)
#endif
	}
};

static int __init dsp_device_init(void)
{
	int ret;

	dsp_enter();
	ret = platform_driver_register(&dsp_driver);
	if (ret)
		pr_err("[Exynos][DSP] Failed to registe platform driver(%d)\n",
				ret);

	dsp_leave();
	return ret;
}

static void __exit dsp_device_exit(void)
{
	dsp_enter();
	platform_driver_unregister(&dsp_driver);
	dsp_leave();
}

#if defined(MODULE)
module_init(dsp_device_init);
#else
late_initcall(dsp_device_init);
#endif
module_exit(dsp_device_exit);

MODULE_AUTHOR("@samsung.com>");
MODULE_DESCRIPTION("Exynos dsp driver");
MODULE_LICENSE("GPL");
