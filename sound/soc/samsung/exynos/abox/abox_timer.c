// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ALSA SoC - Samsung Abox driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_platform.h>

#include "abox_soc.h"
#include "abox_timer.h"

struct abox_timer_data {
        struct device *dev;
        void __iomem *sfr;
        struct regmap *regmap;
	unsigned int max;
};

static struct device *abox_timer_device;

struct device *abox_timer_get_device(void)
{
	return abox_timer_device;
}

struct platform_driver samsung_abox_timer_driver;

static struct abox_timer_data *abox_timer_get_data(struct device *dev)
{
	if (dev->driver != &samsung_abox_timer_driver)
		return NULL;

	return dev_get_drvdata(dev);
}

int abox_timer_link_device(struct device *dev, struct device *consumer)
{
	struct abox_timer_data *data = abox_timer_get_data(dev);

	device_link_add(consumer, dev, DL_FLAG_PM_RUNTIME);
}

static void abox_timer_runtime_get_sync(struct abox_timer_data *data)
{
	int ret;

	ret = pm_runtime_get_sync(data->dev);
	if (ret < 0)
		abox_err(data->dev, "failed to pm runtime get: %d\n", ret);

	return ret;
}

static void abox_timer_runtime_put(struct abox_timer_data *data)
{
	pm_runtime_put(data->dev);
}

int abox_timer_get_current(struct device *dev, int id, u64 *value)
{
	struct abox_timer_data *data = abox_timer_get_data(dev);
	int ret;

	abox_dbg(dev, "%s(%d)\n", __func__, id);

	ret = pm_runtime_get_sync(dev);
	if (ret < 0)
		abox_err(dev, "failed to pm runtime get: %d\n", ret);

        ret = regmap_raw_read(data->regmap, ABOX_TIMER_CURVALUD_LSB(id), value, sizeof(*value));
	if (ret < 0)
		abox_err(dev, "failed to read current of timer %d: %d\n", id, ret);

	pm_runtime_put(dev);

        return ret;
}

int abox_timer_get_preset(struct device *dev, int id, u64 *value)
{
	struct abox_timer_data *data = abox_timer_get_data(dev);
	int ret;

	abox_dbg(dev, "%s(%d)\n", __func__, id);

	ret = pm_runtime_get_sync(dev);
	if (ret < 0)
		abox_err(dev, "failed to pm runtime get: %d\n", ret);

	ret = regmap_raw_read(data->regmap, ABOX_TIMER_PRESET_LSB(id), value, sizeof(*value));
	if (ret < 0)
		abox_err(dev, "failed to read preset of timer %d: %d\n", id, ret);

	pm_runtime_put(dev);

	return value;
}

int abox_start_timer(struct device *dev, int id)
{
	struct abox_timer_data *data = abox_timer_get_data(dev);
	int ret;

	abox_dbg(dev, "%s(%d)\n", __func__, id);

	ret = pm_runtime_get_sync(dev);
	if (ret < 0)
		abox_err(dev, "failed to pm runtime get: %d\n", ret);

	ret = regmap_write(data->regmap, ABOX_TIMER_CTRL0(id), 1 << ABOX_TIMER_START_L);
	if (ret < 0)
		abox_err(dev, "failed to start timer %d: %d\n", id, ret);

	pm_runtime_put(dev);

	return ret;
}

static bool abox_timer_writeable_reg(struct device *dev, unsigned int reg)
{
	struct abox_timer_data *data = dev_get_drvdata(dev);
	int i;

	if (ABOX_TIMER_CTRL0(i) < reg || reg > ABOX_TIMER_CURVALUD_MSB(data->max))
		return false;

	for (i = 0; i < data->max; i++) {
		if (ABOX_TIMER_CTRL0(i) <= reg && reg <= ABOX_TIMER_CTRL1(i))
			return true;
	}

	return false;
}

static bool abox_timer_readable_reg(struct device *dev, unsigned int reg)
{
	struct abox_timer_data *data = dev_get_drvdata(dev);
	int i;

	if (ABOX_TIMER_CTRL0(i) < reg || reg > ABOX_TIMER_CURVALUD_MSB(data->max))
		return false;

	for (i = 0; i < data->max; i++) {
		if (ABOX_TIMER_CTRL0(i) <= reg && reg <= ABOX_TIMER_CURVALUD_MSB(i))
			return true;
	}

	return false;
}

static bool abox_timer_volatile_reg(struct device *dev, unsigned int reg)
{
	return abox_timer_readable_reg(dev, reg);
}

static struct regmap_config abox_timer_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = ABOX_TIMER_MAX_REGISTERS,
	.writeable_reg = abox_timer_writeable_reg,
	.readable_reg = abox_timer_readable_reg,
	.volatile_reg = abox_timer_volatile_reg,
	.cache_type = REGCACHE_NONE,
	.fast_io = true,
};

static int abox_timer_runtime_suspend(struct device *dev)
{
        struct abox_timer_data *data = dev_get_drvdata(dev);
        struct regmap *regmap = data->regmap;

	abox_dbg(dev, "%s\n", __func__);

	regcache_cache_only(regmap, true);
	regcache_mark_dirty(regmap);

	return 0;
}

static int abox_timer_runtime_resume(struct device *dev)
{
	struct abox_timer_data *data = dev_get_drvdata(dev);
        struct regmap *regmap = data->regmap;

	abox_dbg(dev, "%s\n", __func__);

	regcache_cache_only(regmap, false);
	regcache_sync(regmap);

	return 0;
}

static int samsung_abox_timer_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct abox_timer_data *data;

	abox_dbg(dev, "%s\n", __func__);

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;
	if (of_samsung_property_read_u32(dev, dev->of_node, "max", &data->max) < 0) {
		dev_warn(dev, "No max property. Use 3 by default.\n");
		data->max = 3;
	}

        data->sfr = devm_get_ioremap(pdev, "sfr", NULL, NULL);
	if (IS_ERR(data->sfr))
		return PTR_ERR(data->sfr);

        data->regmap = devm_regmap_init_mmio(dev, data->sfr, &abox_timer_regmap_config);
        if (IS_ERR(data->regmap))
                return PTR_ERR(data->regmap);

        pm_runtime_enable(dev);

	abox_timer_device = dev;

	return 0;
}

static int samsung_abox_timer_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	abox_dbg(dev, "%s\n", __func__);

	pm_runtime_disable(dev);

	return 0;
}

static void samsung_abox_timer_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	abox_dbg(dev, "%s\n", __func__);

	pm_runtime_disable(dev);
}

static const struct of_device_id samsung_abox_timer_match[] = {
	{
		.compatible = "samsung,abox-timer",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_timer_match);

static const struct dev_pm_ops samsung_abox_timer_pm = {
	SET_RUNTIME_PM_OPS(abox_timer_runtime_suspend,
			abox_timer_runtime_resume, NULL)
};

struct platform_driver samsung_abox_timer_driver = {
	.probe  = samsung_abox_timer_probe,
	.remove = samsung_abox_timer_remove,
        .shutdown = samsung_abox_timer_shutdown,
	.driver = {
		.name = "abox-timer",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_timer_match),
                .pm = &samsung_abox_timer_pm,
	},
};