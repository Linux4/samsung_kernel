/* linux/drivers/devfreq/exynos/exynos7570_bus_mif.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS7570 SoC MIF devfreq driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/workqueue.h>

#ifdef CONFIG_UMTS_MODEM_SS310AP
#include <linux/exynos-modem-ctrl.h>
#endif
#include <soc/samsung/exynos-devfreq.h>
#include <soc/samsung/bts.h>
#include <linux/apm-exynos.h>
#include <soc/samsung/asv-exynos.h>

#include "../../../drivers/soc/samsung/pwrcal/pwrcal.h"
#include "../../../drivers/soc/samsung/pwrcal/S5E7570/S5E7570-vclk.h"
#include "../governor.h"

#ifdef CONFIG_SHARE_MIF_FREQ_INFO
#include <linux/shm_ipc.h>
#endif

#ifdef CONFIG_SOC_EXYNOS7570_DUAL
#define DEVFREQ_MIF_SWITCH_FREQ	(840000)
#else
#define DEVFREQ_MIF_SWITCH_FREQ	(830000)
#endif

static unsigned long origin_suspend_freq = 0;

u32 sw_volt_table;

int is_dll_on(void)
{
	return cal_dfs_ext_ctrl(dvfs_mif, cal_dfs_mif_is_dll_on, 0);
}
EXPORT_SYMBOL_GPL(is_dll_on);

static int exynos7570_devfreq_mif_set_freq_post(struct exynos_devfreq_data *data)
{
#ifdef CONFIG_SHARE_MIF_FREQ_INFO
	shm_set_mif_freq(data->new_freq);
#endif
	return 0;
}

static int exynos7570_devfreq_mif_cmu_dump(struct exynos_devfreq_data *data)
{
	mutex_lock(&data->devfreq->lock);
	cal_vclk_dbg_info(dvfs_mif);
	mutex_unlock(&data->devfreq->lock);

	return 0;
}

static int exynos7570_devfreq_mif_pm_suspend_prepare(struct exynos_devfreq_data *data)
{
#ifdef CONFIG_UMTS_MODEM_SS310AP
	unsigned long chg_suspend_freq = 0;
#endif

	if (!origin_suspend_freq)
		origin_suspend_freq = data->devfreq_profile.suspend_freq;

#ifdef CONFIG_UMTS_MODEM_SS310AP
	chg_suspend_freq = (unsigned long)ss310ap_get_evs_mode_ext();

	if (chg_suspend_freq)
		data->devfreq_profile.suspend_freq = chg_suspend_freq;
	else
		data->devfreq_profile.suspend_freq = origin_suspend_freq;
#endif

	return 0;
}

static int exynos7570_devfreq_cl_dvfs_start(struct device *dev)
{
	int ret = 0;

#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
	ret = exynos_cl_dvfs_start(ID_MIF);
#endif

	return ret;
}

static int exynos7570_devfreq_cl_dvfs_stop(struct device *dev, u32 target_idx)
{
	int ret = 0;

#ifdef CONFIG_EXYNOS_CL_DVFS_MIF
	ret = exynos_cl_dvfs_stop(ID_MIF, target_idx);
#endif

	return ret;
}

static int exynos7570_devfreq_mif_get_switch_freq(struct device *dev, u32 cur_freq,
						u32 new_freq, u32 *switch_freq)
{
	*switch_freq = DEVFREQ_MIF_SWITCH_FREQ;

	return 0;
}

static int exynos7570_devfreq_mif_get_switch_voltage(struct device *dev, u32 cur_freq,
					u32 new_freq, u32 cur_volt, u32 new_volt, u32 *switch_volt)
{
	*switch_volt = sw_volt_table;

	return 0;
}

static int exynos7570_devfreq_mif_get_freq(struct device *dev, u32 *cur_freq, struct clk *clk)
{
	*cur_freq = (u32)clk_get_rate(clk);
	if (*cur_freq == 0) {
		dev_err(dev, "failed get frequency from CAL\n");
		return -EINVAL;
	}

	return 0;
}

static int exynos7570_devfreq_mif_restore_from_switch_freq(struct device *dev,
						struct clk *clk, u32 cur_freq, u32 new_freq)
{
	if (clk_set_rate(clk, new_freq)) {
		dev_err(dev, "failed to set frequency by CAL (%uKhz)\n", new_freq);
		return -EINVAL;
	}

	return 0;
}

static int exynos7570_devfreq_mif_init_freq_table(struct exynos_devfreq_data *data)
{
	u32 max_freq, min_freq, cur_freq;
	unsigned long tmp_max, tmp_min;
	struct dev_pm_opp *target_opp;
	u32 flags = 0;
	int i, ret;

	ret = cal_clk_enable(dvfs_mif);
	if (ret) {
		dev_err(data->dev, "failed to enable MIF\n");
		return -EINVAL;
	}

	max_freq = (u32)cal_dfs_get_max_freq(dvfs_mif);
	if (!max_freq) {
		dev_err(data->dev, "failed get max frequency\n");
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

	min_freq = (u32)cal_dfs_get_min_freq(dvfs_mif);
	if (!min_freq) {
		dev_err(data->dev, "failed get min frequency\n");
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

	cur_freq = clk_get_rate(data->clk);
	dev_info(data->dev, "current frequency: %uKhz\n", cur_freq);

	for (i = 0; i < data->max_state; i++) {
		if (data->opp_list[i].freq > data->max_freq ||
			data->opp_list[i].freq < data->min_freq)
			dev_pm_opp_disable(data->dev, (unsigned long)data->opp_list[i].freq);
	}

	return 0;
}

static int exynos7570_devfreq_mif_get_volt_table(struct device *dev, u32 max_state,
					struct exynos_devfreq_opp_table *opp_list)
{
	struct dvfs_rate_volt mif_rate_volt[max_state];
	int table_size;
	int i;

	table_size = cal_dfs_get_rate_asv_table(dvfs_mif, mif_rate_volt);
	if (!table_size) {
		dev_err(dev, "failed get ASV table\n");
		return -ENODEV;
	}

	if (table_size != max_state) {
		dev_err(dev, "ASV table size is not matched\n");
		return -ENODEV;
	}

	for (i = 0; i < max_state; i++) {
		if (opp_list[i].freq != (u32)(mif_rate_volt[i].rate)) {
			dev_err(dev, "Freq table is not matched(%u:%u)\n",
				opp_list[i].freq, (u32)mif_rate_volt[i].rate);
			return -EINVAL;
		}
		opp_list[i].volt = (u32)mif_rate_volt[i].volt;

		/* Fill switch voltage table */
	}
	if (!sw_volt_table)
		sw_volt_table = (u32)mif_rate_volt[0].volt;

	dev_info(dev, "SW_volt %uuV in freq %uKhz\n",
			sw_volt_table, DEVFREQ_MIF_SWITCH_FREQ);

	return 0;
}

static int exynos7570_devfreq_mif_init(struct exynos_devfreq_data *data)
{
	data->clk = clk_get(data->dev, "dvfs_mif");
	if (IS_ERR_OR_NULL(data->clk)) {
		dev_err(data->dev, "failed get dvfs vclk\n");
		return -ENODEV;
	}

	data->sw_clk = clk_get(data->dev, "dvfs_mif_sw");
	if (IS_ERR_OR_NULL(data->sw_clk)) {
		dev_err(data->dev, "failed get dvfs sw vclk\n");
		clk_put(data->clk);
		return -ENODEV;
	}

	return 0;
}

static int exynos7570_devfreq_mif_exit(struct exynos_devfreq_data *data)
{
	clk_put(data->sw_clk);
	clk_put(data->clk);

	return 0;
}

static int __init exynos7570_devfreq_mif_init_prepare(struct exynos_devfreq_data *data)
{
	data->ops.init = exynos7570_devfreq_mif_init;
	data->ops.exit = exynos7570_devfreq_mif_exit;
	data->ops.get_volt_table = exynos7570_devfreq_mif_get_volt_table;
	data->ops.get_switch_freq = exynos7570_devfreq_mif_get_switch_freq;
	data->ops.get_switch_voltage = exynos7570_devfreq_mif_get_switch_voltage;
	data->ops.get_freq = exynos7570_devfreq_mif_get_freq;
	data->ops.restore_from_switch_freq = exynos7570_devfreq_mif_restore_from_switch_freq;
	data->ops.init_freq_table = exynos7570_devfreq_mif_init_freq_table;
	data->ops.cl_dvfs_start = exynos7570_devfreq_cl_dvfs_start;
	data->ops.cl_dvfs_stop = exynos7570_devfreq_cl_dvfs_stop;
	data->ops.pm_suspend_prepare = exynos7570_devfreq_mif_pm_suspend_prepare;
	data->ops.cmu_dump = exynos7570_devfreq_mif_cmu_dump;
	data->ops.set_freq_post = exynos7570_devfreq_mif_set_freq_post;

	return 0;
}

static int __init exynos7570_devfreq_mif_initcall(void)
{
	if (register_exynos_devfreq_init_prepare(DEVFREQ_MIF,
				exynos7570_devfreq_mif_init_prepare))
		return -EINVAL;

	return 0;
}
fs_initcall(exynos7570_devfreq_mif_initcall);
