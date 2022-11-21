/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>

#include "vertex-log.h"
#include "vertex-graph.h"
#include "vertex-device.h"

void __vertex_fault_handler(struct vertex_device *device)
{
	vertex_enter();
	vertex_leave();
}

static int __attribute__((unused)) vertex_fault_handler(
		struct iommu_domain *domain, struct device *dev,
		unsigned long fault_addr, int fault_flag, void *token)
{
	struct vertex_device *device;

	pr_err("< VERTEX FAULT HANDLER >\n");
	pr_err("Device virtual(0x%lX) is invalid access\n", fault_addr);

	device = dev_get_drvdata(dev);
	vertex_debug_dump_debug_regs();

	__vertex_fault_handler(device);

	return -EINVAL;
}

#if defined(CONFIG_PM_SLEEP)
static int vertex_device_suspend(struct device *dev)
{
	vertex_enter();
	vertex_leave();
	return 0;
}

static int vertex_device_resume(struct device *dev)
{
	vertex_enter();
	vertex_leave();
	return 0;
}
#endif

static int vertex_device_runtime_suspend(struct device *dev)
{
	int ret;
	struct vertex_device *device;

	vertex_enter();
	device = dev_get_drvdata(dev);

	ret = vertex_system_suspend(&device->system);
	if (ret)
		goto p_err;

	vertex_leave();
p_err:
	return ret;
}

static int vertex_device_runtime_resume(struct device *dev)
{
	int ret;
	struct vertex_device *device;

	vertex_enter();
	device = dev_get_drvdata(dev);

	ret = vertex_system_resume(&device->system);
	if (ret)
		goto p_err;

	vertex_leave();
p_err:
	return ret;
}

static const struct dev_pm_ops vertex_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
			vertex_device_suspend,
			vertex_device_resume)
	SET_RUNTIME_PM_OPS(
			vertex_device_runtime_suspend,
			vertex_device_runtime_resume,
			NULL)
};

static int __vertex_device_start(struct vertex_device *device)
{
	int ret;

	vertex_enter();
	if (!test_bit(VERTEX_DEVICE_STATE_OPEN, &device->state)) {
		ret = -EINVAL;
		vertex_err("device was not opend yet (%lx)\n", device->state);
		goto p_err_state;
	}

	if (test_bit(VERTEX_DEVICE_STATE_START, &device->state))
		return 0;

#ifdef TEMP_RT_FRAMEWORK_TEST
	set_bit(VERTEX_DEVICE_STATE_START, &device->state);
	return 0;
#endif
	ret = vertex_system_start(&device->system);
	if (ret)
		goto p_err_system;

	ret = vertex_debug_start(&device->debug);
	if (ret)
		goto p_err_debug;

	set_bit(VERTEX_DEVICE_STATE_START, &device->state);

	vertex_leave();
	return 0;
p_err_debug:
	vertex_system_stop(&device->system);
p_err_system:
p_err_state:
	return ret;
}

static int __vertex_device_stop(struct vertex_device *device)
{
	vertex_enter();
	if (!test_bit(VERTEX_DEVICE_STATE_START, &device->state))
		return 0;

#ifdef TEMP_RT_FRAMEWORK_TEST
	clear_bit(VERTEX_DEVICE_STATE_START, &device->state);
	return 0;
#endif
	vertex_debug_stop(&device->debug);
	vertex_system_stop(&device->system);
	clear_bit(VERTEX_DEVICE_STATE_START, &device->state);

	vertex_leave();
	return 0;
}

static int __vertex_device_power_on(struct vertex_device *device)
{
	int ret;

	vertex_enter();
#if defined(CONFIG_PM)
	ret = pm_runtime_get_sync(device->dev);
	if (ret) {
		vertex_err("Failed to get pm_runtime sync (%d)\n", ret);
		goto p_err;
	}
#else
	ret = vertex_device_runtime_resume(device->dev);
	if (ret)
		goto p_err;
#endif

	vertex_leave();
p_err:
	return ret;
}

static int __vertex_device_power_off(struct vertex_device *device)
{
	int ret;

	vertex_enter();
#if defined(CONFIG_PM)
	ret = pm_runtime_put_sync(device->dev);
	if (ret) {
		vertex_err("Failed to put pm_runtime sync (%d)\n", ret);
		goto p_err;
	}
#else
	ret = vertex_device_runtime_suspend(device->dev);
	if (ret)
		goto p_err;
#endif

	vertex_leave();
p_err:
	return ret;
}

int vertex_device_start(struct vertex_device *device)
{
	int ret;

	vertex_enter();
	ret = __vertex_device_start(device);
	if (ret)
		goto p_err;

	vertex_leave();
p_err:
	return ret;
}

int vertex_device_stop(struct vertex_device *device)
{
	int ret;

	vertex_enter();
	ret = __vertex_device_stop(device);
	if (ret)
		goto p_err;

	vertex_leave();
p_err:
	return ret;
}

int vertex_device_open(struct vertex_device *device)
{
	int ret;

	vertex_enter();
	if (test_bit(VERTEX_DEVICE_STATE_OPEN, &device->state)) {
		ret = -EINVAL;
		vertex_warn("device was already opened\n");
		goto p_err_state;
	}

#ifdef TEMP_RT_FRAMEWORK_TEST
	device->system.graphmgr.current_model = NULL;
	set_bit(VERTEX_DEVICE_STATE_OPEN, &device->state);
	return 0;
#endif

	ret = vertex_system_open(&device->system);
	if (ret)
		goto p_err_system;

	ret = vertex_debug_open(&device->debug);
	if (ret)
		goto p_err_debug;

	ret = __vertex_device_power_on(device);
	if (ret)
		goto p_err_power;

	ret = vertex_system_fw_bootup(&device->system);
	if (ret)
		goto p_err_boot;

	set_bit(VERTEX_DEVICE_STATE_OPEN, &device->state);

	vertex_leave();
	return 0;
p_err_boot:
	__vertex_device_power_off(device);
p_err_power:
	vertex_debug_close(&device->debug);
p_err_debug:
	vertex_system_close(&device->system);
p_err_system:
p_err_state:
	return ret;
}

int vertex_device_close(struct vertex_device *device)
{
	vertex_enter();
	if (!test_bit(VERTEX_DEVICE_STATE_OPEN, &device->state)) {
		vertex_warn("device is already closed\n");
		goto p_err;
	}

	if (test_bit(VERTEX_DEVICE_STATE_START, &device->state))
		__vertex_device_stop(device);

#ifdef TEMP_RT_FRAMEWORK_TEST
	clear_bit(VERTEX_DEVICE_STATE_OPEN, &device->state);
	return 0;
#endif
	__vertex_device_power_off(device);
	vertex_debug_close(&device->debug);
	vertex_system_close(&device->system);
	clear_bit(VERTEX_DEVICE_STATE_OPEN, &device->state);

	vertex_leave();
p_err:
	return 0;
}

static int vertex_device_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev;
	struct vertex_device *device;

	vertex_enter();
	dev = &pdev->dev;

	device = devm_kzalloc(dev, sizeof(*device), GFP_KERNEL);
	if (!device) {
		ret = -ENOMEM;
		vertex_err("Fail to alloc device structure\n");
		goto p_err_alloc;
	}

	get_device(dev);
	device->dev = dev;
	dev_set_drvdata(dev, device);

	ret = vertex_system_probe(device);
	if (ret)
		goto p_err_system;

	ret = vertex_core_probe(device);
	if (ret)
		goto p_err_core;

	ret = vertex_debug_probe(device);
	if (ret)
		goto p_err_debug;

	iovmm_set_fault_handler(dev, vertex_fault_handler, NULL);
	pm_runtime_enable(dev);

	vertex_leave();
	vertex_info("vertex device is initilized\n");
	return 0;
p_err_debug:
	vertex_core_remove(&device->core);
p_err_core:
	vertex_system_remove(&device->system);
p_err_system:
	devm_kfree(dev, device);
p_err_alloc:
	vertex_err("vertex device is not registered (%d)\n", ret);
	return ret;
}

static int vertex_device_remove(struct platform_device *pdev)
{
	struct vertex_device *device;

	vertex_enter();
	device = dev_get_drvdata(&pdev->dev);

	vertex_debug_remove(&device->debug);
	vertex_core_remove(&device->core);
	vertex_system_remove(&device->system);
	devm_kfree(device->dev, device);
	vertex_leave();
	return 0;
}

static void vertex_device_shutdown(struct platform_device *pdev)
{
	struct vertex_device *device;

	vertex_enter();
	device = dev_get_drvdata(&pdev->dev);
	vertex_leave();
}

#if defined(CONFIG_OF)
static const struct of_device_id exynos_vertex_match[] = {
	{
		.compatible = "samsung,exynos-vipx-vertex",
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_vertex_match);
#endif

static struct platform_driver vertex_driver = {
	.probe		= vertex_device_probe,
	.remove		= vertex_device_remove,
	.shutdown	= vertex_device_shutdown,
	.driver = {
		.name	= "exynos-vipx-vertex",
		.owner	= THIS_MODULE,
		.pm	= &vertex_pm_ops,
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(exynos_vertex_match)
#endif
	}
};

int __init vertex_device_init(void)
{
	int ret;

	vertex_enter();
	ret = platform_driver_register(&vertex_driver);
	if (ret)
		vertex_err("platform driver for vertex is not registered(%d)\n",
				ret);

	vertex_leave();
	return ret;
}

void __exit vertex_device_exit(void)
{
	vertex_enter();
	platform_driver_unregister(&vertex_driver);
	vertex_leave();
}

MODULE_AUTHOR("@samsung.com>");
MODULE_DESCRIPTION("Exynos VIPx temp driver");
MODULE_LICENSE("GPL");
