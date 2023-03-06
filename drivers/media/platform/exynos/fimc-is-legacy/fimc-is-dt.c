/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#include <mach/exynos-fimc-is-sensor-legacy.h>
#include <mach/exynos-fimc-is-legacy.h>
#include <media/exynos_mc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#include "fimc-is-dt.h"

#ifdef CONFIG_OF
struct exynos5_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	struct exynos5_platform_fimc_is *pdata;
	struct device_node *np = dev->of_node;
	struct fimc_is_gpio_info *gpio_info;

	if (!np)
		return ERR_PTR(-ENOENT);

	pdata = kzalloc(sizeof(struct exynos5_platform_fimc_is), GFP_KERNEL);
	if (!pdata) {
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	pdata->cfg_gpio = exynos5422_fimc_is_cfg_gpio;
	pdata->clk_cfg = exynos5422_fimc_is_cfg_clk;
	pdata->clk_on = exynos5422_fimc_is_clk_on;
	pdata->clk_off = exynos5422_fimc_is_clk_off;
	pdata->print_cfg = exynos5_fimc_is_print_cfg;
	pdata->sensor_clock_on = exynos5422_fimc_is_sensor_clk_on;
	pdata->sensor_clock_off = exynos5422_fimc_is_sensor_clk_off;

	dev->platform_data = pdata;

	gpio_info = kzalloc(sizeof(struct fimc_is_gpio_info), GFP_KERNEL);
	if (!gpio_info) {
		printk(KERN_ERR "%s: no memory for gpio_info\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	gpio_info->gpio_main_rst = of_get_named_gpio(np, "gpios_main_reset", 0);
	if (!gpio_is_valid(gpio_info->gpio_main_rst)) {
		dev_err(dev, "failed to get main reset gpio\\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_vt_rst = of_get_named_gpio(np, "gpios_vt_reset", 0);
	if (!gpio_is_valid(gpio_info->gpio_vt_rst)) {
		dev_err(dev, "failed to get vt reset gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_vt_en = of_get_named_gpio(np, "gpios_vt_en", 0);
	if (!gpio_is_valid(gpio_info->gpio_vt_en)) {
		dev_err(dev, "failed to get vt reset gpio\n");
		return ERR_PTR(-EINVAL);
	}

	gpio_info->gpio_vt_stby = of_get_named_gpio(np, "gpios_vt_stby", 0);
	if (!gpio_is_valid(gpio_info->gpio_vt_stby)) {
		dev_err(dev, "failed to get vt reset gpio\n");
		return ERR_PTR(-EINVAL);
	}

	pdata->_gpio_info = gpio_info;

	return pdata;
}
#else
struct exynos5_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	return ERR_PTR(-EINVAL);
}
#endif
