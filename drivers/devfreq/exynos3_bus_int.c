/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
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
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/reboot.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include <mach/asv-exynos.h>
#include <mach/pm_domains.h>
#include <mach/tmu.h>
#include <mach/regs-clock-exynos3475.h>

#include "devfreq_exynos.h"
#include "governor.h"

static struct devfreq_simple_ondemand_data exynos3_devfreq_int_governor_data;
static struct exynos_devfreq_platdata exynos3_qos_int;

static struct pm_qos_request exynos3_int_qos;
static struct pm_qos_request boot_int_qos;

static int exynos3_devfreq_int_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_int *data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_int = data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long target_volt;
	unsigned long old_freq;

	if(!data->use_dvfs)
		return -EINVAL;

	mutex_lock(&data->lock);

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(dev, "DEVFREQ(INT) : Invalid OPP to find\n");
		ret = PTR_ERR(target_opp);
		goto out;
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	target_volt = get_limit_voltage(target_volt, data->volt_offset);
#endif
	/* just want to save voltage before apply constraint with isp */
	data->target_volt = target_volt;
	rcu_read_unlock();

	target_idx = devfreq_get_opp_idx(devfreq_int_opp_list, data->max_state,
						*target_freq);
	old_idx = devfreq_get_opp_idx(devfreq_int_opp_list, data->max_state,
						devfreq_int->previous_freq);

	old_freq = devfreq_int->previous_freq;

	if (target_idx < 0 || old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq)
		goto out;

	pr_debug("INT LV_%d(%lu) ================> LV_%d(%lu, volt: %lu)\n",
			old_idx, old_freq, target_idx, *target_freq, target_volt);

	exynos_ss_freq(ESS_FLAG_INT, old_freq, ESS_FLAG_IN);
	if (old_freq < *target_freq) {
		if (data->int_set_volt)
			data->int_set_volt(data, target_volt, data->max_support_voltage);
		if (data->int_set_freq)
			data->int_set_freq(data, target_idx, old_idx);
	} else {
		if (data->int_set_freq)
			data->int_set_freq(data, target_idx, old_idx);
		if (data->int_set_volt)
			data->int_set_volt(data, target_volt, data->max_support_voltage);
	}
	data->old_volt = target_volt;
	exynos_ss_freq(ESS_FLAG_INT, *target_freq, ESS_FLAG_OUT);
out:
	mutex_unlock(&data->lock);

	return ret;
}

static struct devfreq_dev_profile exynos3_devfreq_int_profile = {
	.target		= exynos3_devfreq_int_target,
};

static int exynos3_init_int_table(struct device *dev,
				struct devfreq_data_int *data)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt = 0;

	exynos3_devfreq_int_profile.max_state = data->max_state;

	for (i = 0; i < data->max_state; i++) {
		freq = devfreq_int_opp_list[i].freq;
		volt = get_match_volt(ID_INT, freq);

		if (!volt)
			volt = devfreq_int_opp_list[i].volt;

		exynos3_devfreq_int_profile.freq_table[i] = freq;
		devfreq_int_opp_list[i].volt = volt;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(INT) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(INT) : %7uKhz, %7uV\n", freq, volt);
		}
	}

	return 0;
}

static int exynos3_devfreq_int_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	pm_qos_update_request(&boot_int_qos, exynos3_devfreq_int_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos3_int_reboot_notifier = {
	.notifier_call = exynos3_devfreq_int_reboot_notifier,
};

static int exynos3_devfreq_int_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np;
	const struct of_device_id *match;
	devfreq_init_of_fn init_func;
	struct devfreq_data_int *data;
	struct devfreq_notifier_block *devfreq_nb;
	struct exynos_devfreq_platdata *plat_data;
	struct opp *target_opp;
	unsigned long freq;
	u64 cal_qos_max;
	u64 default_qos;
	u64 initial_freq;

	dev_set_drvdata(&pdev->dev, &exynos3_qos_int);
	data = devm_kzalloc(&pdev->dev, sizeof(struct devfreq_data_int), GFP_KERNEL);
	if (!data) {
		pr_err("DEVFREQ(INT) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	data->use_dvfs = false;

	ret = of_property_read_u32(pdev->dev.of_node, "pm_qos_class", &exynos3_devfreq_int_governor_data.pm_qos_class);
	if (ret < 0) {
		pr_err("%s pm_qos_class error %d\n", __func__, ret);
		goto err_data;
	}

	ret = of_property_read_u64(pdev->dev.of_node, "cal_qos_max", &cal_qos_max);
	if (ret < 0) {
		pr_err("%s cal_qos_max error %d\n", __func__, ret);
		goto err_data;
	}

	ret = of_property_read_u64(pdev->dev.of_node, "default_qos", &default_qos);
	if (ret < 0) {
		pr_err("%s default_qos error %d\n", __func__, ret);
		goto err_data;
	}

	ret = of_property_read_u64(pdev->dev.of_node, "initial_freq", &initial_freq);
	if (ret < 0) {
		pr_err("%s initial_freq error %d\n", __func__, ret);
		goto err_data;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "polling_period", &exynos3_devfreq_int_profile.polling_ms);
	if (ret < 0) {
		pr_err("%s polling_period error %d\n", __func__, ret);
		goto err_data;
	}

	exynos3_devfreq_int_governor_data.cal_qos_max = (unsigned long)cal_qos_max;
	exynos3_qos_int.default_qos = (unsigned long)default_qos;
	exynos3_devfreq_int_profile.initial_freq = (unsigned long)initial_freq;

	/* Initialize QoS for INT */
	pm_qos_add_request(&exynos3_int_qos, PM_QOS_DEVICE_THROUGHPUT, exynos3_qos_int.default_qos);
	pm_qos_add_request(&boot_int_qos, PM_QOS_DEVICE_THROUGHPUT, exynos3_qos_int.default_qos);
	pm_qos_update_request_timeout(&exynos3_int_qos,
					exynos3_devfreq_int_profile.initial_freq, 40000 * 1000);

	/* Find proper init function for int */
	for_each_matching_node_and_match(np, __devfreq_init_of_table, &match) {
		if (!strcmp(np->name, "bus_int")) {
			init_func = match->data;
			init_func(data);
			break;
		}
	}

	data->initial_freq = exynos3_devfreq_int_profile.initial_freq;
	exynos3_devfreq_int_profile.freq_table = devm_kzalloc(&pdev->dev, sizeof(int) * data->max_state, GFP_KERNEL);
	if (!exynos3_devfreq_int_profile.freq_table) {
		pr_err("DEVFREQ(INT) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_data;
	}

	ret = exynos3_init_int_table(&pdev->dev, data);
	if (ret)
		goto err_data;

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);

	data->volt_offset = 0;
	data->dev = &pdev->dev;

	data->vdd_int = devm_regulator_get(&pdev->dev, "vdd_core");
	if (IS_ERR(data->vdd_int)) {
		pr_warn("DEVFREQ(INT) : Failed get regulator (%ld)\n", IS_ERR(data->vdd_int));
		data->vdd_int = NULL;
	}

	if (data->vdd_int)
		data->max_support_voltage = regulator_get_max_support_voltage(data->vdd_int);

	if (data->max_support_voltage <= 0)
		data->max_support_voltage = REGULATOR_MAX_MICROVOLT;

	rcu_read_lock();
	freq = exynos3_devfreq_int_governor_data.cal_qos_max;
	target_opp = devfreq_recommended_opp(data->dev, &freq, 0);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(data->dev, "DEVFREQ(INT) : Invalid OPP to set voltagen");
		ret = PTR_ERR(target_opp);
		goto err_data;
	}
	data->old_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	data->old_volt = get_limit_voltage(data->old_volt, data->volt_offset);
#endif
	rcu_read_unlock();

	if (data->vdd_int)
		regulator_set_voltage(data->vdd_int, data->old_volt, data->max_support_voltage);

	data->devfreq = devfreq_add_device(data->dev,
						&exynos3_devfreq_int_profile,
						"simple_ondemand",
						&exynos3_devfreq_int_governor_data);

	devfreq_nb = devm_kzalloc(&pdev->dev, sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (!devfreq_nb) {
		pr_err("DEVFREQ(INT) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}
	devfreq_nb->df = data->devfreq;

	plat_data = &exynos3_qos_int;
	data->default_qos = plat_data->default_qos;
	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = devfreq_int_opp_list[0].freq;
	register_reboot_notifier(&exynos3_int_reboot_notifier);

#ifdef CONFIG_EXYNOS_THERMAL
       exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data->use_dvfs = true;

	return ret;
err_nb:
	devfreq_remove_device(data->devfreq);
err_data:
	return ret;
}

static int exynos3_devfreq_int_remove(struct platform_device *pdev)
{
	struct devfreq_data_int *data = platform_get_drvdata(pdev);
	devfreq_remove_device(data->devfreq);
	pm_qos_remove_request(&exynos3_int_qos);
	pm_qos_remove_request(&boot_int_qos);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos3_devfreq_int_suspend(struct device *dev)
{
	pm_qos_update_request(&exynos3_int_qos, exynos3_devfreq_int_profile.initial_freq);

	return 0;
}

static int exynos3_devfreq_int_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_int *data = platform_get_drvdata(pdev);

	pm_qos_update_request(&exynos3_int_qos, data->default_qos);

	return 0;
}

static const struct dev_pm_ops exynos3_devfreq_int_pm = {
	.suspend	= exynos3_devfreq_int_suspend,
	.resume		= exynos3_devfreq_int_resume,
};

static const struct of_device_id exynos3_devfreq_int_match[] = {
	{ .compatible = "samsung,exynos3-devfreq-int"
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos3_devfreq_int_match);

static struct platform_driver exynos3_devfreq_int_driver = {
	.probe	= exynos3_devfreq_int_probe,
	.remove	= exynos3_devfreq_int_remove,
	.driver	= {
		.name	= "exynos3-devfreq-int",
		.owner	= THIS_MODULE,
		.pm		= &exynos3_devfreq_int_pm,
		.of_match_table	= of_match_ptr(exynos3_devfreq_int_match),
	},
};
module_platform_driver(exynos3_devfreq_int_driver);
