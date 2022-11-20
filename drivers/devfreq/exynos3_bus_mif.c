/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Seungook yang(swy.yang@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/devfreq.h>
#include <linux/exynos-ss.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/reboot.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <mach/asv-exynos.h>
#include <mach/bts.h>
#include <mach/devfreq.h>
#include <mach/regs-clock-exynos3475.h>
#include <mach/regs-pmu-exynos3475.h>
#include <mach/tmu.h>

#include <plat/cpu.h>

#include "devfreq_exynos.h"
#include "governor.h"

#define MIF_RESUME_FREQ (413000)

static struct devfreq_simple_exynos_data exynos3_devfreq_mif_governor_data;
static struct exynos_devfreq_platdata exynos3_qos_mif;

static unsigned long current_freq;
static bool is_freq_changing;

static struct pm_qos_request exynos3_mif_qos;
static struct pm_qos_request exynos3_mif_qos_max;
static struct pm_qos_request boot_mif_qos;
struct pm_qos_request min_mif_thermal_qos;

static unsigned int dsref_cyc;

static int exynos3_devfreq_mif_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_mif *mif_data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_mif = mif_data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long target_volt;
	unsigned long old_freq;

	if(!mif_data->use_dvfs)
		return -EINVAL;

	mutex_lock(&mif_data->lock);

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(dev, "DEVFREQ(MIF) : Invalid OPP to find\n");
		ret = PTR_ERR(target_opp);
		goto out;
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);
	rcu_read_unlock();

	target_idx = devfreq_get_opp_idx(devfreq_mif_opp_list, mif_data->max_state, *target_freq);
	old_idx = devfreq_get_opp_idx(devfreq_mif_opp_list, mif_data->max_state,
						devfreq_mif->previous_freq);
	old_freq = devfreq_mif->previous_freq;

	if (target_idx < 0 || old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq)
		goto out;

	pr_debug("MIF LV_%d(%lu) ================> LV_%d(%lu, volt: %lu)\n",
			old_idx, old_freq, target_idx, *target_freq, target_volt);

	is_freq_changing = true;

	exynos_ss_freq(ESS_FLAG_MIF, old_freq, ESS_FLAG_IN);

	if (old_freq < *target_freq) {
		if (mif_data->mif_set_freq) {
			mif_data->mif_set_freq(mif_data, target_idx, old_idx);
			exynos_update_bts_param(target_idx, 1);
		}
	} else {
		if (mif_data->mif_set_freq) {
			exynos_update_bts_param(target_idx, 1);
			mif_data->mif_set_freq(mif_data, target_idx, old_idx);
		}
	}

	exynos_ss_freq(ESS_FLAG_MIF, *target_freq, ESS_FLAG_OUT);

	current_freq = *target_freq;
	is_freq_changing = false;
out:
	mutex_unlock(&mif_data->lock);

	return ret;
}

static struct devfreq_dev_profile exynos3_devfreq_mif_profile = {
	.target		= exynos3_devfreq_mif_target,
};

static int exynos3_init_mif_table(struct device *dev,
				struct devfreq_data_mif *data)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt = 0;

	for (i = 0; i < data->max_state; ++i) {
		freq = devfreq_mif_opp_list[i].freq;
		volt = get_match_volt(ID_MIF, freq);

		if (!volt)
			volt = devfreq_mif_opp_list[i].volt;

		exynos3_devfreq_mif_profile.freq_table[i] = freq;
		devfreq_mif_opp_list[i].volt = volt;

		ret = opp_add(dev, freq, volt);

		opp_disable(dev, devfreq_mif_opp_list[0].freq);
		opp_disable(dev, devfreq_mif_opp_list[1].freq);

		if (ret) {
			pr_err("DEVFREQ(MIF) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(MIF) : %7uKhz, %7uV\n", freq, volt);
		}
	}

	return 0;
}

static int exynos3_devfreq_mif_reboot_notifier(struct notifier_block *nb,
		unsigned long val, void *v)
{
	if (pm_qos_request_active(&boot_mif_qos))
		pm_qos_update_request(&boot_mif_qos, exynos3_devfreq_mif_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos3_mif_reboot_notifier = {
	.notifier_call = exynos3_devfreq_mif_reboot_notifier,
};

extern void exynos3475_devfreq_set_dll_lock_value(struct devfreq_data_mif *, int);
extern struct attribute_group devfreq_mif_attr_group;
static int exynos3_devfreq_mif_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np;
	const struct of_device_id *match;
	devfreq_init_of_fn init_func;
	struct devfreq_data_mif *data;
	struct devfreq_notifier_block *devfreq_nb;
	struct exynos_devfreq_platdata *plat_data;
	struct opp *target_opp;
	unsigned long freq;
	u64 cal_qos_max;
	u64 default_qos;
	u64 initial_freq;

	dev_set_drvdata(&pdev->dev, &exynos3_qos_mif);
	data = devm_kzalloc(&pdev->dev, sizeof(struct devfreq_data_mif), GFP_KERNEL);
	if (!data) {
		pr_err("DEVFREQ(MIF) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	data->use_dvfs = false;

	data->dev = &pdev->dev;

	ret = of_property_read_u32(pdev->dev.of_node, "pm_qos_class", &exynos3_devfreq_mif_governor_data.pm_qos_class);
	if (ret < 0) {
		pr_err("%s pm_qos_class error %d\n", __func__, ret);
		goto err_freqtable;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "pm_qos_class_max", &exynos3_devfreq_mif_governor_data.pm_qos_class_max);
	if (ret < 0) {
		pr_err("%s pm_qos_class_max error %d\n", __func__, ret);
		goto err_freqtable;
	}

	ret = of_property_read_u64(pdev->dev.of_node, "cal_qos_max", &cal_qos_max);
	if (ret < 0) {
		pr_err("%s cal_qos_max error %d\n", __func__, ret);
		goto err_freqtable;
	}

	ret = of_property_read_u64(pdev->dev.of_node, "default_qos", &default_qos);
	if (ret < 0) {
		pr_err("%s default_qos error %d\n", __func__, ret);
		goto err_freqtable;
	}

	ret = of_property_read_u64(pdev->dev.of_node, "initial_freq", &initial_freq);
	if (ret < 0) {
		pr_err("%s initial_freq error %d\n", __func__, ret);
		goto err_freqtable;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "polling_period", &exynos3_devfreq_mif_profile.polling_ms);
	if (ret < 0) {
		pr_err("%s polling_period error %d\n", __func__, ret);
		goto err_freqtable;
	}

	exynos3_devfreq_mif_governor_data.cal_qos_max = (unsigned long)cal_qos_max;
	exynos3_qos_mif.default_qos = (unsigned long)default_qos;
	exynos3_devfreq_mif_profile.initial_freq = (unsigned long)initial_freq;

	/* Initialize QoS for MIF */
	pm_qos_add_request(&exynos3_mif_qos, PM_QOS_BUS_THROUGHPUT, exynos3_qos_mif.default_qos);
	pm_qos_add_request(&min_mif_thermal_qos, PM_QOS_BUS_THROUGHPUT, exynos3_qos_mif.default_qos);
	pm_qos_add_request(&boot_mif_qos, PM_QOS_BUS_THROUGHPUT, exynos3_qos_mif.default_qos);
	pm_qos_add_request(&exynos3_mif_qos_max, PM_QOS_BUS_THROUGHPUT_MAX, cal_qos_max);
	pm_qos_update_request_timeout(&boot_mif_qos,
					exynos3_devfreq_mif_profile.initial_freq, 40000 * 1000);

	/* Find proper init function for mif */
	for_each_matching_node_and_match(np, __devfreq_init_of_table, &match) {
		if (!strcmp(np->name, "bus_mif")) {
			init_func = match->data;
			init_func(data);
			break;
		}
	}

	exynos3_devfreq_mif_profile.max_state = data->max_state;
	data->default_qos = exynos3_qos_mif.default_qos;
	data->initial_freq = exynos3_devfreq_mif_profile.initial_freq;
	data->cal_qos_max = exynos3_devfreq_mif_governor_data.cal_qos_max;

	data->use_dvfs = false;
	mutex_init(&data->lock);

	exynos3_devfreq_mif_profile.freq_table = devm_kzalloc(&pdev->dev, sizeof(int) * data->max_state, GFP_KERNEL);
	if (exynos3_devfreq_mif_profile.freq_table == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos3_init_mif_table(&pdev->dev, data);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);

	data->vdd_mif = devm_regulator_get(&pdev->dev, "vdd_core");
	if (IS_ERR(data->vdd_mif)) {
		pr_warn("DEVFREQ(MIF) : Failed get regulator (%ld)\n", IS_ERR(data->vdd_mif));
		data->vdd_mif = NULL;
	}

	if (data->vdd_mif)
		data->max_support_voltage = regulator_get_max_support_voltage(data->vdd_mif);

	if (data->max_support_voltage <= 0)
		data->max_support_voltage = REGULATOR_MAX_MICROVOLT;

	rcu_read_lock();
	freq = exynos3_devfreq_mif_governor_data.cal_qos_max;
	target_opp = devfreq_recommended_opp(data->dev, &freq, 0);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(data->dev, "DEVFREQ(MIF) : Invalid OPP to set voltagen");
		ret = PTR_ERR(target_opp);
		goto err_inittable;
	}
	data->old_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	data->old_volt = get_limit_voltage(data->old_volt, data->volt_offset);
#endif

	rcu_read_unlock();

	if (data->vdd_mif)
		regulator_set_voltage(data->vdd_mif, data->old_volt, data->max_support_voltage);

	/* Obtain DLL Lock value on 416MHz */
	exynos3475_devfreq_set_dll_lock_value(data, DLL_LOCK_LV);

	data->devfreq = devfreq_add_device(data->dev,
						&exynos3_devfreq_mif_profile,
						"simple_exynos",
						&exynos3_devfreq_mif_governor_data);

	devfreq_nb = devm_kzalloc(&pdev->dev, sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (devfreq_nb == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}

	exynos3_devfreq_init_thermal();

	devfreq_nb->df = data->devfreq;

	plat_data = &exynos3_qos_mif;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos3_devfreq_mif_governor_data.cal_qos_max;

	register_reboot_notifier(&exynos3_mif_reboot_notifier);

#ifdef CONFIG_EXYNOS_THERMAL
       exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data->use_dvfs = true;

	return ret;
err_nb:
	devfreq_remove_device(data->devfreq);
err_inittable:
	kfree(exynos3_devfreq_mif_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos3_devfreq_mif_remove(struct platform_device *pdev)
{
	struct device_node *np;
	const struct of_device_id *match;
	devfreq_deinit_of_fn deinit_func;
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_mif_thermal_qos);
	pm_qos_remove_request(&exynos3_mif_qos);
	pm_qos_remove_request(&exynos3_mif_qos_max);
	pm_qos_remove_request(&boot_mif_qos);

       /* Find proper deinit function for mif */
	for_each_matching_node_and_match(np, __devfreq_deinit_of_table, &match) {
		if (!strcmp(np->name, "bus_mif")) {
			deinit_func = match->data;
			deinit_func(data);
			break;
		}
	}

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos3_devfreq_mif_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);
	uint32_t reg, tmp;

	pm_qos_update_request(&exynos3_mif_qos, MIF_RESUME_FREQ);

	/* enable ABR */
	reg = __raw_readl(data->base_drex + MEMCONTROL);
	reg &= ~(PB_REF_EN_MASK);
	__raw_writel(reg, data->base_drex + MEMCONTROL);

	/* reduce dsref_cyc value about 1/2 times */
	reg = dsref_cyc = __raw_readl(data->base_drex + PWRDNCONFIG);
	tmp = ((reg >> DSREF_CYC_SHIFT) / 2) << DSREF_CYC_SHIFT;
	reg &= ~(DSREF_CYC_MASK << DSREF_CYC_SHIFT);
	reg |= tmp;
	__raw_writel(reg, data->base_drex + PWRDNCONFIG);

	return 0;
}

static int exynos3_devfreq_mif_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_mif = data->devfreq;
	uint32_t reg;

	/* restore dsref_cyc */
	__raw_writel(dsref_cyc, data->base_drex + PWRDNCONFIG);

	/* enable PBR */
	reg = __raw_readl(data->base_drex + MEMCONTROL);
	reg |= PB_REF_EN_MASK;
	__raw_writel(reg, data->base_drex + MEMCONTROL);

	pm_qos_update_request(&exynos3_mif_qos_max, devfreq_mif->max_freq);
	pm_qos_update_request(&exynos3_mif_qos, devfreq_mif->min_freq);

	return 0;
}

static const struct dev_pm_ops exynos3_devfreq_mif_pm = {
	.suspend	= exynos3_devfreq_mif_suspend,
	.resume		= exynos3_devfreq_mif_resume,
};

static const struct of_device_id exynos3_devfreq_mif_match[] = {
	{ .compatible = "samsung,exynos3-devfreq-mif"
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos3_devfreq_mif_match);

static struct platform_driver exynos3_devfreq_mif_driver = {
	.probe	= exynos3_devfreq_mif_probe,
	.remove	= exynos3_devfreq_mif_remove,
	.driver	= {
		.name	= "exynos3-devfreq-mif",
		.owner	= THIS_MODULE,
		.pm		= &exynos3_devfreq_mif_pm,
		.of_match_table	= of_match_ptr(exynos3_devfreq_mif_match),
	},
};
module_platform_driver(exynos3_devfreq_mif_driver);
