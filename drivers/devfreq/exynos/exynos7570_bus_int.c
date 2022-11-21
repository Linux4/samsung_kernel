/* linux/drivers/devfreq/exynos/exynos7570_bus_int.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Samsung EXYNOS7570 SoC INT devfreq driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/clk.h>

#include <soc/samsung/exynos-devfreq.h>

#include "../../../drivers/soc/samsung/pwrcal/pwrcal.h"
#include "../../../drivers/soc/samsung/pwrcal/S5E7570/S5E7570-vclk.h"
#include "../governor.h"

static int exynos7570_devfreq_int_cmu_dump(struct exynos_devfreq_data *data)
{
	cal_vclk_dbg_info(dvfs_int);

	return 0;
}

static int exynos7570_devfreq_int_reboot(struct exynos_devfreq_data *data)
{
	data->max_freq = data->reboot_freq;
	data->devfreq->max_freq = data->max_freq;

	mutex_lock(&data->devfreq->lock);
	update_devfreq(data->devfreq);
	mutex_unlock(&data->devfreq->lock);

	return 0;
}

static int exynos7570_devfreq_int_get_freq(struct device *dev, u32 *cur_freq, struct clk *clk)
{
	*cur_freq = (u32)clk_get_rate(clk);
	if (*cur_freq == 0) {
		dev_err(dev, "failed to get frequency from CAL\n");
		return -EINVAL;
	}

	return 0;
}

static int exynos7570_devfreq_int_set_freq(struct device *dev, u32 new_freq, struct clk *clk)
{
	if (clk_set_rate(clk, (unsigned long)new_freq)) {
		dev_err(dev, "failed to set frequency via CAL (%uKhz)\n", new_freq);
		return -EINVAL;
	}

	return 0;
}

static int exynos7570_devfreq_int_init_freq_table(struct exynos_devfreq_data *data)
{
	u32 max_freq, min_freq;
	unsigned long tmp_max, tmp_min;
	struct dev_pm_opp *target_opp;
	u32 flags = 0;
	int i;

	max_freq = (u32)cal_dfs_get_max_freq(dvfs_int);
	if (!max_freq) {
		dev_err(data->dev, "failed to get max frequency\n");
		return -EINVAL;
	}

	dev_info(data->dev, "max_freq: %uKhz, get_max_freq: %uKhz\n",
			data->max_freq, max_freq);

	if (max_freq < data->max_freq) {
		rcu_read_lock();
		flags |= DEVFREQ_FLAG_LEAST_UPPER_BOUND;
		tmp_max = (unsigned long)max_freq;
		target_opp = devfreq_recommended_opp(data->dev, &tmp_max, flags);
		if (IS_ERR(target_opp)) {
			rcu_read_unlock();
			dev_err(data->dev, "not found valid OPP for max_freq\n");
			return PTR_ERR(target_opp);
		}

		data->max_freq = dev_pm_opp_get_freq(target_opp);
		rcu_read_unlock();
	}

	min_freq = (u32)cal_dfs_get_min_freq(dvfs_int);
	if (!min_freq) {
		dev_err(data->dev, "failed to get min frequency\n");
		return -EINVAL;
	}

	dev_info(data->dev, "min_freq: %uKhz, get_min_freq: %uKhz\n",
			data->min_freq, min_freq);

	if (min_freq > data->min_freq) {
		rcu_read_lock();
		flags &= ~DEVFREQ_FLAG_LEAST_UPPER_BOUND;
		tmp_min = (unsigned long)min_freq;
		target_opp = devfreq_recommended_opp(data->dev, &tmp_min, flags);
		if (IS_ERR(target_opp)) {
			rcu_read_unlock();
			dev_err(data->dev, "not found valid OPP for min_freq\n");
			return PTR_ERR(target_opp);
		}

		data->min_freq = dev_pm_opp_get_freq(target_opp);
		rcu_read_unlock();
	}

	dev_info(data->dev, "min_freq: %uKhz, max_freq: %uKhz\n",
			data->min_freq, data->max_freq);

	for (i = 0; i < data->max_state; i++) {
		if (data->opp_list[i].freq > data->max_freq ||
			data->opp_list[i].freq < data->min_freq)
			dev_pm_opp_disable(data->dev, (unsigned long)data->opp_list[i].freq);
	}

	return 0;
}

static int exynos7570_devfreq_int_get_volt_table(struct device *dev, u32 max_state,
					struct exynos_devfreq_opp_table *opp_list)
{
	struct dvfs_rate_volt int_rate_volt[max_state];
	int table_size;
	int i;

	table_size = cal_dfs_get_rate_asv_table(dvfs_int, int_rate_volt);
	if (!table_size) {
		dev_err(dev, "failed to get ASV table\n");
		return -ENODEV;
	}

	if (table_size != max_state) {
		dev_err(dev, "ASV table size is not matched\n");
		return -ENODEV;
	}

	for (i = 0; i < max_state; i++) {
		if (opp_list[i].freq != (u32)(int_rate_volt[i].rate)) {
			dev_err(dev, "Freq table is not matched(%u:%u)\n",
				opp_list[i].freq, (u32)int_rate_volt[i].rate);
			return -EINVAL;
		}
		opp_list[i].volt= (u32)int_rate_volt[i].volt;
	}

	return 0;
}

static int exynos7570_devfreq_int_init(struct exynos_devfreq_data *data)
{
	data->clk = clk_get(data->dev, "dvfs_int");
	if (IS_ERR_OR_NULL(data->clk)) {
		dev_err(data->dev, "failed get dvfs vclk\n");
		return -ENODEV;
	}

	return 0;
}

static int exynos7570_devfreq_int_exit(struct exynos_devfreq_data *data)
{
	clk_put(data->clk);

	return 0;
}

static int __init exynos7570_devfreq_int_init_prepare(struct exynos_devfreq_data *data)
{
	data->ops.init = exynos7570_devfreq_int_init;
	data->ops.exit = exynos7570_devfreq_int_exit;
	data->ops.get_volt_table = exynos7570_devfreq_int_get_volt_table;
	data->ops.get_freq = exynos7570_devfreq_int_get_freq;
	data->ops.set_freq = exynos7570_devfreq_int_set_freq;
	data->ops.init_freq_table = exynos7570_devfreq_int_init_freq_table;
	data->ops.reboot = exynos7570_devfreq_int_reboot;
	data->ops.cmu_dump = exynos7570_devfreq_int_cmu_dump;

	return 0;
}

static int __init exynos7570_devfreq_int_initcall(void)
{
	if (register_exynos_devfreq_init_prepare(DEVFREQ_INT,
				exynos7570_devfreq_int_init_prepare))
		return -EINVAL;

	return 0;
}

fs_initcall(exynos7570_devfreq_int_initcall);
