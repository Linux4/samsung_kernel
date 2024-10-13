/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/pm_runtime.h>

#include "npu-core.h"
#include "common/npu-log.h"
#include "npu-device.h"

#if IS_ENABLED(CONFIG_NPU_CORE_DRIVER)
static int g_npu_core_num;
#endif
static int g_npu_core_always_num;
static int g_npu_core_inuse_num;
static struct npu_core *g_npu_core_list[NPU_CORE_MAX_NUM];

#if IS_ENABLED(CONFIG_NPU_CORE_DRIVER)
static int __npu_core_runtime_suspend(struct device *dev)
{
	struct npu_core *core;

	BUG_ON(!dev);
	npu_dbg("start\n");

	core = dev_get_drvdata(dev);

	npu_clk_disable_unprepare(&core->clks);

	npu_dbg("end\n");
	return 0;
}

static int __npu_core_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct npu_core *core;

	BUG_ON(!dev);
	npu_dbg("start\n");

	core = dev_get_drvdata(dev);

	npu_dbg("clk\n");
	ret = npu_clk_prepare_enable(&core->clks);
	if (ret) {
		npu_err("fail(%d) in npu_clk_prepare_enable\n", ret);
		return ret;
	}

	npu_dbg("complete:%d\n", ret);
	return ret;
}
#endif

#if IS_ENABLED(CONFIG_NPU_CORE_DRIVER)
int npu_core_on(struct npu_system *system)
{
	int ret = 0;
	int i;
	bool active;
	struct npu_core *core;

	BUG_ON(!system);

	for (i = g_npu_core_always_num; i < g_npu_core_inuse_num; i++) {
		core = g_npu_core_list[i];
		active = pm_runtime_active(core->dev);
		npu_info("%d %d\n", i, core->id);
		ret += pm_runtime_get_sync(core->dev);
		if (ret)
			npu_err("fail(%d) in pm_runtime_get_sync\n", ret);

		pm_runtime_set_active(core->dev);

		if (!active && pm_runtime_active(core->dev)) {
			npu_info("core\n");
			if (ret)
				npu_err("fail(%d) in npu_soc_core_on\n", ret);
		}
	}
	npu_dbg("complete\n");
	return ret;
}

int npu_core_off(struct npu_system *system)
{
	int ret = 0;
	int i;
	struct npu_core *core;

	BUG_ON(!system);

	for (i = g_npu_core_inuse_num - 1; i >= g_npu_core_always_num; i--) {
		core = g_npu_core_list[i];
		npu_info("%d %d\n", i, core->id);

		ret += pm_runtime_put_sync(core->dev);
		if (ret)
			npu_err("fail(%d) in pm_runtime_put_sync\n", ret);

		BUG_ON(ret < 0);
	}
	npu_dbg("complete\n");
	return ret;
}
#endif

int npu_core_clock_on(struct npu_system *system)
{
	int ret = 0;
	int i;
	struct npu_core *core;

	BUG_ON(!system);

	for (i = g_npu_core_always_num; i < g_npu_core_inuse_num; i++) {
		core = g_npu_core_list[i];
		ret = npu_clk_prepare_enable(&core->clks);
		if (ret)
			npu_err("fail(%d) in npu_clk_prepare_enable\n", ret);
	}
	npu_dbg("complete\n");
	return ret;
}

int npu_core_clock_off(struct npu_system *system)
{
	int i;
	struct npu_core *core;

	BUG_ON(!system);

	for (i = g_npu_core_inuse_num - 1; i >= g_npu_core_always_num; i--) {
		core = g_npu_core_list[i];
		npu_clk_disable_unprepare(&core->clks);
	}
	npu_dbg("complete\n");
	return 0;
}

#if IS_ENABLED(CONFIG_NPU_CORE_DRIVER)
int npu_core_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	struct npu_core *core;

	BUG_ON(!pdev);

	dev = &pdev->dev;

	core = devm_kzalloc(dev, sizeof(*core), GFP_KERNEL);
	if (!core) {
		probe_err("fail in devm_kzalloc\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	core->dev = dev;

	ret = of_property_read_u32(dev->of_node, "samsung,npucore-id", &core->id);
	if (ret) {
		probe_err("fail(%d) in of_property_read_u32\n", ret);
		goto err_exit;
	}

	ret = npu_clk_get(&core->clks, dev);
	if (ret) {
		probe_err("fail(%d) in npu_clk_get\n", ret);
		goto err_exit;
	}

	pm_runtime_enable(dev);
	dev_set_drvdata(dev, core);

	// TODO: do NOT use global variable for core list
	g_npu_core_list[g_npu_core_num++] = core;
	g_npu_core_always_num = 1;
	g_npu_core_inuse_num = 2;

	probe_info("npu core %d registerd in %s\n", core->id, __func__);

	goto ok_exit;
err_exit:
	probe_err("ret(%d)\n", ret);
ok_exit:
	return ret;

}

#if IS_ENABLED(CONFIG_PM_SLEEP)
int npu_core_suspend(struct device *dev)
{
	npu_info("called\n");
	return 0;
}

int npu_core_resume(struct device *dev)
{
	npu_info("called\n");
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_PM)
int npu_core_runtime_suspend(struct device *dev)
{
	return __npu_core_runtime_suspend(dev);
}

int npu_core_runtime_resume(struct device *dev)
{
	return __npu_core_runtime_resume(dev);
}
#endif

int npu_core_remove(struct platform_device *pdev)
{
	struct device *dev;
	struct npu_core *core;

	BUG_ON(!pdev);

	dev = &pdev->dev;
	core = dev_get_drvdata(dev);

	pm_runtime_disable(dev);

	npu_clk_put(&core->clks, dev);

	probe_info("completed in %s\n", __func__);
	return 0;
}
#endif

