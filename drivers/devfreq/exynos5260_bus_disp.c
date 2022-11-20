/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Sangkyu Kim(skwith.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/devfreq.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>

#include <mach/tmu.h>
#include <mach/devfreq.h>
#include <mach/asv-exynos.h>

#include "exynos5260_ppmu.h"
#include "exynos_ppmu2.h"
#include "devfreq_exynos.h"
#include "governor.h"

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
#include <mach/sec_debug.h>
#endif

#define DEVFREQ_INITIAL_FREQ	(333000)
#define DEVFREQ_POLLING_PERIOD	(100)

#define DISP_VOLT_STEP		12500
#define COLD_VOLT_OFFSET	50000
#define LIMIT_COLD_VOLTAGE	1250000
#define MIN_COLD_VOLTAGE	950000

enum devfreq_disp_idx {
	LV0,
	LV1,
	LV2,
	LV3,
	LV_COUNT,
};

enum devfreq_disp_clk {
	ACLK_DISP_333,
	ACLK_DISP_222,
	CLK_COUNT,
};

struct devfreq_data_disp {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_disp;
	unsigned long old_volt;
	unsigned long volt_offset;
	cputime64_t last_jiffies;

	struct notifier_block tmu_notifier;

	bool use_dvfs;
	unsigned long devfreq_disp_polling;

	struct mutex lock;
};

struct devfreq_clk_list devfreq_disp_clk[CLK_COUNT] = {
	{"aclk_disp_333",},
	{"aclk_disp_222",},
};

cputime64_t devfreq_disp_time_in_state[] = {
	0,
	0,
	0,
	0,
};

struct devfreq_opp_table devfreq_disp_opp_list[] = {
	{LV0,	333000,	925000},
	{LV1,	222000,	875000},
	{LV2,	167000, 875000},
	{LV3,	111000,	875000},
};

struct devfreq_clk_info aclk_disp_333[] = {
	{LV0,	334000000,	0,	NULL},
	{LV1,	222000000,	0,	NULL},
	{LV2,	167000000,	0,	NULL},
	{LV3,	112000000,	0,	NULL},
};

struct devfreq_clk_info aclk_disp_222[] = {
	{LV0,	222000000,	0,	NULL},
	{LV1,	167000000,	0,	NULL},
	{LV2,	 84000000,	0,	NULL},
	{LV3,	 84000000,	0,	NULL},
};

struct devfreq_clk_info *devfreq_clk_disp_info_list[] = {
	aclk_disp_333,
	aclk_disp_222,
};

enum devfreq_disp_clk devfreq_clk_disp_info_idx[] = {
	ACLK_DISP_333,
	ACLK_DISP_222,
};

static struct devfreq_simple_ondemand_data exynos5_devfreq_disp_governor_data = {
	.pm_qos_class		= PM_QOS_DISPLAY_THROUGHPUT,
	.upthreshold		= 95,
	.cal_qos_max		= 333000,
};

static struct exynos_devfreq_platdata exynos5260_qos_disp = {
	.default_qos		= 111000,
};

static struct ppmu_info ppmu_disp[] = {
	{
		.base = (void __iomem *)PPMU_FIMD0X_ADDR,
	}, {
		.base = (void __iomem *)PPMU_FIMD1X_ADDR,
	},
};

static struct devfreq_exynos devfreq_disp_exynos = {
	.ppmu_list = ppmu_disp,
	.ppmu_count = ARRAY_SIZE(ppmu_disp),
};

static struct pm_qos_request exynos5_disp_qos;
static struct pm_qos_request boot_disp_qos;
static struct pm_qos_request min_disp_thermal_qos;

static struct devfreq_data_disp *data_disp;

static ssize_t disp_show_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t len = 0;
	ssize_t cnt_remain = (ssize_t)(PAGE_SIZE - 1);

	for (i = LV0; i < LV_COUNT; ++i) {
		len += snprintf(buf + len, cnt_remain, "%ld %llu\n",
				devfreq_disp_opp_list[i].freq,
				(unsigned long long)devfreq_disp_time_in_state[i]);
		cnt_remain = (ssize_t)(PAGE_SIZE - len - 1);
	}

	return len;
}

static ssize_t disp_show_freq(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;
	ssize_t len = 0;
	ssize_t cnt_remain = (ssize_t)(PAGE_SIZE - 1);

	for (i = LV_COUNT - 1; i >= 0; --i) {
		len += snprintf(buf + len, cnt_remain, "%lu ",
				devfreq_disp_opp_list[i].freq);
		cnt_remain = (ssize_t)(PAGE_SIZE - len - 1);
	}

	len += snprintf(buf + len, cnt_remain, "\n");

	return len;
}

static DEVICE_ATTR(disp_time_in_state, 0644, disp_show_state, NULL);

static DEVICE_ATTR(available_frequencies, S_IRUGO, disp_show_freq, NULL);

static struct attribute *devfreq_disp_sysfs_entries[] = {
	&dev_attr_disp_time_in_state.attr,
	NULL,
};

static struct attribute_group devfreq_disp_attr_group = {
	.name   = "time_in_state",
	.attrs  = devfreq_disp_sysfs_entries,
};

void exynos5_disp_update_pixelclk(unsigned long pixelclk)
{
	int i;

	pixelclk /= 1000;
	for (i = ARRAY_SIZE(devfreq_disp_opp_list) - 1; i >= 0; --i) {
		if (devfreq_disp_opp_list[i].freq > pixelclk) {
			exynos5260_qos_disp.default_qos = devfreq_disp_opp_list[i].freq;
			if (data_disp && data_disp->devfreq)
				data_disp->devfreq->min_freq = devfreq_disp_opp_list[i].freq;
			if (pm_qos_request_active(&exynos5_disp_qos))
				pm_qos_update_request(&exynos5_disp_qos, exynos5260_qos_disp.default_qos);
			break;
		}
	}
}

void exynos5_devfreq_disp_enable(bool enable)
{
	if (data_disp == NULL)
		return;

	data_disp->use_dvfs = enable;
	mutex_lock(&data_disp->devfreq->lock);
	if (enable) {
		data_disp->devfreq->next_polling = data_disp->devfreq_disp_polling;
		devfreq_insert_polling(data_disp->devfreq_disp_polling);
	} else {
		data_disp->devfreq_disp_polling = data_disp->devfreq->next_polling;
		data_disp->devfreq->next_polling = 0;

	}
	mutex_unlock(&data_disp->devfreq->lock);
}

static inline int exynos5_devfreq_disp_get_idx(struct devfreq_opp_table *table,
				unsigned int size,
				unsigned long freq)
{
	int i;

	for (i = 0; i < size; ++i) {
		if (table[i].freq == freq)
			return i;
	}

	return -1;
}

static void exynos5_devfreq_disp_update_time(struct devfreq_data_disp *data,
		int target_idx)
{
	cputime64_t cur_time = get_jiffies_64();
	cputime64_t diff_time = cur_time - data->last_jiffies;

	devfreq_disp_time_in_state[target_idx] += diff_time;
	data->last_jiffies = cur_time;
}

static int exynos5_devfreq_disp_set_freq(struct devfreq_data_disp *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
	sec_debug_aux_log(SEC_DEBUG_AUXLOG_CPU_BUS_CLOCK_CHANGE, 
		"old:%7d new:%7d (DISP)", 
	old_idx,target_idx );
#endif

	if (target_idx < old_idx) {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_disp_info_list); ++i) {
			clk_info = &devfreq_clk_disp_info_list[i][target_idx];
			clk_states = clk_info->states;

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_disp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_disp_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk, clk_info->freq);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_disp_info_list); ++i) {
			clk_info = &devfreq_clk_disp_info_list[i][target_idx];
			clk_states = clk_info->states;

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk, clk_info->freq);

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_disp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_disp_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk, clk_info->freq);
		}
	}

	return 0;
}

static int exynos5_devfreq_disp_set_volt(struct devfreq_data_disp *data,
					unsigned long volt,
					unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	regulator_set_voltage(data->vdd_disp, volt, volt_range);
out:
	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
static unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset)
{
	if (voltage > LIMIT_COLD_VOLTAGE)
		return voltage;

	if (voltage + volt_offset > LIMIT_COLD_VOLTAGE)
		return LIMIT_COLD_VOLTAGE;

	if (volt_offset && (voltage + volt_offset < MIN_COLD_VOLTAGE))
		return MIN_COLD_VOLTAGE;

	return voltage + volt_offset;
}
#endif

static int exynos5_devfreq_disp_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_disp *disp_data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_disp = disp_data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long target_volt;
	unsigned long old_freq;

	mutex_lock(&disp_data->lock);

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(dev, "DEVFREQ(DISP) : Invalid OPP to find\n");
		return PTR_ERR(target_opp);
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);
	rcu_read_unlock();
	target_idx = exynos5_devfreq_disp_get_idx(devfreq_disp_opp_list,
						ARRAY_SIZE(devfreq_disp_opp_list),
						*target_freq);
	old_idx = exynos5_devfreq_disp_get_idx(devfreq_disp_opp_list,
						ARRAY_SIZE(devfreq_disp_opp_list),
						devfreq_disp->previous_freq);
	old_freq = devfreq_disp->previous_freq;

	if (target_idx < 0)
		goto out;

	if (old_freq == *target_freq)
		goto out;

	if (old_freq < *target_freq) {
		exynos5_devfreq_disp_set_volt(disp_data, target_volt, target_volt + VOLT_STEP);
		exynos5_devfreq_disp_set_freq(disp_data, target_idx, old_idx);
	} else {
		exynos5_devfreq_disp_set_freq(disp_data, target_idx, old_idx);
		exynos5_devfreq_disp_set_volt(disp_data, target_volt, target_volt + VOLT_STEP);
	}
out:
	exynos5_devfreq_disp_update_time(disp_data, target_idx);

	if (disp_data->use_dvfs)
		ppmu_reset_total(devfreq_disp_exynos.ppmu_list,
				devfreq_disp_exynos.ppmu_count);

	mutex_unlock(&disp_data->lock);
	return ret;
}

static int exynos5_devfreq_disp_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_disp *data = dev_get_drvdata(dev);

	if (!data->use_dvfs)
		return -EAGAIN;

	if (ppmu_count_total(devfreq_disp_exynos.ppmu_list,
			devfreq_disp_exynos.ppmu_count,
			&devfreq_disp_exynos.val_ccnt,
			&devfreq_disp_exynos.val_pmcnt)) {
		pr_err("DEVFREQ(DISP) : can't calculate bus status\n");
		return -EINVAL;
	}

	stat->current_frequency = data->devfreq->previous_freq;
	stat->busy_time = devfreq_disp_exynos.val_pmcnt;
	stat->total_time = devfreq_disp_exynos.val_ccnt;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_disp_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_disp_target,
	.get_dev_status	= exynos5_devfreq_disp_get_dev_status,
};

static int exynos5_devfreq_disp_init_ppmu(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ppmu_disp); ++i) {
		if (ppmu_init(&ppmu_disp[i]))
			return -EINVAL;
	}

	return 0;
}

static int exynos5_devfreq_disp_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_disp_clk); ++i) {
		devfreq_disp_clk[i].clk = clk_get(NULL, devfreq_disp_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_disp_clk[i].clk)) {
			pr_err("DEVFREQ(DISP) : %s can't get clock\n", devfreq_disp_clk[i].clk_name);
			return -EINVAL;
		}
	}

	return 0;
}

static int exynos5_init_disp_table(struct device *dev)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;

	for (i = 0; i < ARRAY_SIZE(devfreq_disp_opp_list); ++i) {
		freq = devfreq_disp_opp_list[i].freq;
		volt = get_match_volt(ID_DISP, freq);
		if (!volt)
			volt = devfreq_disp_opp_list[i].volt;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(DISP) : Failed to add opp entries %uKhzi %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(DISP) : %uKhz %u\n", freq, volt);
		}
	}

	return 0;
}

static int exynos5_devfreq_disp_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	pm_qos_update_request(&boot_disp_qos, exynos5_devfreq_disp_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_disp_reboot_notifier = {
	.notifier_call = exynos5_devfreq_disp_reboot_notifier,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos5_devfreq_disp_tmu_notifier(struct notifier_block *nb, unsigned long event,
		void *v)
{
	struct devfreq_data_disp *data = container_of(nb, struct devfreq_data_disp, tmu_notifier);
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		if (pm_qos_request_active(&min_disp_thermal_qos))
			pm_qos_update_request(&min_disp_thermal_qos,
					exynos5_devfreq_disp_profile.initial_freq);

		if (*on) {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_disp);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt, data->volt_offset);
			regulator_set_voltage(data->vdd_disp, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		} else {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_disp);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt - COLD_VOLT_OFFSET, data->volt_offset);
			regulator_set_voltage(data->vdd_disp, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&min_disp_thermal_qos))
			pm_qos_update_request(&min_disp_thermal_qos,
					exynos5260_qos_disp.default_qos);
	}

	return NOTIFY_OK;
}
#endif

static int exynos5_devfreq_disp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_disp *data;
	struct exynos_devfreq_platdata *plat_data;

	if (exynos5_devfreq_disp_init_ppmu()) {
		ret = -EINVAL;
		goto err_data;
	}

	if (exynos5_devfreq_disp_init_clock()) {
		ret = -EINVAL;
		goto err_data;
	}

	data = kzalloc(sizeof(struct devfreq_data_disp), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(DISP) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	ret = exynos5_init_disp_table(&pdev->dev);
	if (ret)
		goto err_inittable;

	mutex_init(&data->lock);
	platform_set_drvdata(pdev, data);
	data_disp = data;

	data->last_jiffies = get_jiffies_64();
	/* For low temperature compensation when boot time */
	data->volt_offset = COLD_VOLT_OFFSET;
	data->dev = &pdev->dev;
	data->vdd_disp = regulator_get(NULL, "vdd_disp");
	data->devfreq = devfreq_add_device(data->dev,
						&exynos5_devfreq_disp_profile,
						&devfreq_simple_ondemand,
						&exynos5_devfreq_disp_governor_data);

	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos5_devfreq_disp_governor_data.cal_qos_max;
	pm_qos_add_request(&exynos5_disp_qos, PM_QOS_DISPLAY_THROUGHPUT, plat_data->default_qos);
	pm_qos_add_request(&min_disp_thermal_qos, PM_QOS_DISPLAY_THROUGHPUT, plat_data->default_qos);
	pm_qos_add_request(&boot_disp_qos, PM_QOS_DISPLAY_THROUGHPUT, plat_data->default_qos);

	register_reboot_notifier(&exynos5_disp_reboot_notifier);

	ret = sysfs_create_group(&data->devfreq->dev.kobj, &devfreq_disp_attr_group);
	if (ret) {
		pr_err("DEVFREQ(DISP) : Failed create time_in_state sysfs\n");
		goto err_inittable;
	}

	ret = device_create_file(&data->devfreq->dev, &dev_attr_available_frequencies);
	if (ret) {
		pr_err("DEVFREQ(DISP) : Failed create available_frequencies sysfs\n");
		goto err_inittable;
	}

#ifdef CONFIG_EXYNOS_THERMAL
        data->tmu_notifier.notifier_call = exynos5_devfreq_disp_tmu_notifier;
        exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data->use_dvfs = true;

	return ret;

err_inittable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_disp_remove(struct platform_device *pdev)
{
	struct devfreq_data_disp *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_disp_thermal_qos);
	pm_qos_remove_request(&exynos5_disp_qos);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_disp_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos5_disp_qos))
		pm_qos_update_request(&exynos5_disp_qos, exynos5_devfreq_disp_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_disp_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos5_disp_qos))
		pm_qos_update_request(&exynos5_disp_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_disp_pm = {
	.suspend	= exynos5_devfreq_disp_suspend,
	.resume		= exynos5_devfreq_disp_resume,
};

static struct platform_driver exynos5_devfreq_disp_driver = {
	.probe	= exynos5_devfreq_disp_probe,
	.remove	= exynos5_devfreq_disp_remove,
	.driver	= {
		.name	= "exynos5-devfreq-disp",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_disp_pm,
	},
};

static struct platform_device exynos5_devfreq_disp_device = {
	.name	= "exynos5-devfreq-disp",
	.id	= -1,
};

static int __init exynos5_devfreq_disp_init(void)
{
	int ret;

	exynos5_devfreq_disp_device.dev.platform_data = &exynos5260_qos_disp;

	ret = platform_device_register(&exynos5_devfreq_disp_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_disp_driver);
}
late_initcall(exynos5_devfreq_disp_init);

static void __exit exynos5_devfreq_disp_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_disp_driver);
	platform_device_unregister(&exynos5_devfreq_disp_device);
}
module_exit(exynos5_devfreq_disp_exit);
