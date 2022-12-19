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

#include <mach/apm-exynos.h>
#include <mach/asv-exynos.h>
#include <mach/bts.h>
#include <mach/devfreq.h>
#include <mach/regs-clock-exynos7580.h>
#include <mach/regs-pmu-exynos7580.h>
#include <mach/tmu.h>

#include <plat/cpu.h>

#include "exynos7580_ppmu.h"
#include "exynos_ppmu_fw.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define MIF_RESUME_FREQ (416000)

static struct devfreq_simple_exynos_data exynos7_devfreq_mif_governor_data = {
	.pm_qos_class		= PM_QOS_BUS_THROUGHPUT,
	.pm_qos_class_max	= PM_QOS_BUS_THROUGHPUT_MAX,
	.urgentthreshold	= 80,
	.upthreshold		= 70,
	.downthreshold		= 60,
	.idlethreshold		= 50,
	.cal_qos_max		= 825000,
};

static struct exynos_devfreq_platdata exynos7_qos_mif;
static struct devfreq_exynos devfreq_mif_exynos;

static struct pm_qos_request exynos7_mif_qos;
static struct pm_qos_request exynos7_mif_max_qos;
static struct pm_qos_request boot_mif_qos;
struct pm_qos_request min_mif_thermal_qos;

static int exynos7_devfreq_mif_target(struct device *dev,
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

	exynos_ss_freq(ESS_FLAG_MIF, old_freq, ESS_FLAG_IN);
	if (old_freq < *target_freq) {
		if (mif_data->mif_set_freq) {
			mif_data->mif_set_freq(mif_data, target_idx, old_idx);
			exynos7_update_bts_param(target_idx, 1);
		}
	} else {
		if (mif_data->mif_set_freq) {
			exynos7_update_bts_param(target_idx, 1);
			mif_data->mif_set_freq(mif_data, target_idx, old_idx);
		}
	}

	exynos_ss_freq(ESS_FLAG_MIF, *target_freq, ESS_FLAG_OUT);

out:
	mutex_unlock(&mif_data->lock);

	return ret;
}

static int exynos7_devfreq_mif_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_mif *data = dev_get_drvdata(dev);
	int idx, above_idx;
	int below_idx = data->max_state - 1;

	if (!data->use_dvfs)
		return -EAGAIN;

	stat->current_frequency = data->devfreq->previous_freq;

	idx = devfreq_get_opp_idx(devfreq_mif_opp_list, data->max_state,
						stat->current_frequency);

	if (idx < 0)
		return -EAGAIN;

	above_idx = idx - 1;
	below_idx = idx + 1;

	if (above_idx < 0)
		above_idx = 0;

	if (below_idx >= data->max_state)
		below_idx = data->max_state - 1;

	exynos7_devfreq_mif_governor_data.above_freq = devfreq_mif_opp_list[above_idx].freq;
	exynos7_devfreq_mif_governor_data.below_freq = devfreq_mif_opp_list[below_idx].freq;

	stat->busy_time = devfreq_mif_exynos.val_pmcnt;
	stat->total_time = devfreq_mif_exynos.val_ccnt;

	return 0;
}

static struct devfreq_dev_profile exynos7_devfreq_mif_profile = {
	.target		= exynos7_devfreq_mif_target,
	.get_dev_status	= exynos7_devfreq_mif_get_dev_status,
};

static int exynos7_init_mif_table(struct device *dev,
				struct devfreq_data_mif *data)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;

	for (i = 0; i < data->max_state; ++i) {
		freq = devfreq_mif_opp_list[i].freq;
		volt = get_match_volt(ID_MIF, freq);

		if (!volt)
			volt = devfreq_mif_opp_list[i].volt;

		devfreq_mif_opp_list[i].volt = volt;
		exynos7_devfreq_mif_profile.freq_table[i] = freq;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(MIF) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(MIF) : %7uKhz, %7uV\n", freq, volt);
		}
	}

	return 0;
}

static int exynos7_devfreq_mif_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	struct devfreq_notifier_block *devfreq_nb;

	devfreq_nb = container_of(nb, struct devfreq_notifier_block, nb);

	mutex_lock(&devfreq_nb->df->lock);
	update_devfreq(devfreq_nb->df);
	mutex_unlock(&devfreq_nb->df->lock);

	return NOTIFY_OK;
}

static int exynos7_devfreq_mif_reboot_notifier(struct notifier_block *nb,
												unsigned long val, void *v)
{
	if (pm_qos_request_active(&boot_mif_qos))
		pm_qos_update_request(&boot_mif_qos, exynos7_devfreq_mif_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos7_mif_reboot_notifier = {
	.notifier_call = exynos7_devfreq_mif_reboot_notifier,
};

extern void exynos7580_devfreq_set_dll_lock_value(struct devfreq_data_mif *, int);
extern struct attribute_group devfreq_mif_attr_group;
static int exynos7_devfreq_mif_probe(struct platform_device *pdev)
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

	if (!exynos_ppmu_is_initialized())
		return -EPROBE_DEFER;

	dev_set_drvdata(&pdev->dev, &exynos7_qos_mif);
	data = devm_kzalloc(&pdev->dev, sizeof(struct devfreq_data_mif), GFP_KERNEL);
	if (!data) {
		pr_err("DEVFREQ(MIF) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	data->dev = &pdev->dev;

	ret = of_property_read_u32(pdev->dev.of_node, "bus_type_mif", &devfreq_mif_exynos.type);
	if (ret < 0) {
		pr_err("%s bus_type_mif error %d\n", __func__, ret);
		goto err_data;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "pm_qos_class", &exynos7_devfreq_mif_governor_data.pm_qos_class);
	if (ret < 0) {
		pr_err("%s pm_qos_class error %d\n", __func__, ret);
		goto err_data;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "urgentthreshold", &exynos7_devfreq_mif_governor_data.urgentthreshold);
	if (ret < 0) {
		pr_err("%s upthreshold error %d\n", __func__, ret);
		goto err_data;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "upthreshold", &exynos7_devfreq_mif_governor_data.upthreshold);
	if (ret < 0) {
		pr_err("%s upthreshold error %d\n", __func__, ret);
		goto err_data;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "downthreshold", &exynos7_devfreq_mif_governor_data.downthreshold);
	if (ret < 0) {
		pr_err("%s downthreshold error %d\n", __func__, ret);
		goto err_data;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "idlethreshold", &exynos7_devfreq_mif_governor_data.idlethreshold);
	if (ret < 0) {
		pr_err("%s idlethreshold error %d\n", __func__, ret);
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

	ret = of_property_read_u32(pdev->dev.of_node, "polling_period", &exynos7_devfreq_mif_profile.polling_ms);
	if (ret < 0) {
		pr_err("%s polling_period error %d\n", __func__, ret);
		goto err_data;
	}

	exynos7_devfreq_mif_governor_data.cal_qos_max = (unsigned long)cal_qos_max;
	exynos7_qos_mif.default_qos = (unsigned long)default_qos;
	exynos7_devfreq_mif_profile.initial_freq = (unsigned long)initial_freq;

	/* Initialize QoS for MIF */
	pm_qos_add_request(&exynos7_mif_qos, PM_QOS_BUS_THROUGHPUT, exynos7_qos_mif.default_qos);
	pm_qos_add_request(&exynos7_mif_max_qos, PM_QOS_BUS_THROUGHPUT_MAX, PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);
	pm_qos_add_request(&min_mif_thermal_qos, PM_QOS_BUS_THROUGHPUT, exynos7_qos_mif.default_qos);
	pm_qos_add_request(&boot_mif_qos, PM_QOS_BUS_THROUGHPUT, exynos7_qos_mif.default_qos);
	pm_qos_update_request_timeout(&boot_mif_qos,
					exynos7_devfreq_mif_profile.initial_freq, 40000 * 1000);

	/* Find proper init function for mif */
	for_each_matching_node_and_match(np, __devfreq_init_of_table, &match) {
		if (!strcmp(np->name, "bus_mif")) {
			init_func = match->data;
			init_func(data);
			break;
		}
	}

	exynos7_devfreq_mif_profile.max_state = data->max_state;
	data->default_qos = exynos7_qos_mif.default_qos;
	data->initial_freq = exynos7_devfreq_mif_profile.initial_freq;
	data->cal_qos_max = exynos7_devfreq_mif_governor_data.cal_qos_max;

	data->use_dvfs = false;
	mutex_init(&data->lock);

	exynos7_devfreq_mif_profile.freq_table = devm_kzalloc(&pdev->dev, sizeof(int) * data->max_state, GFP_KERNEL);
	if (exynos7_devfreq_mif_profile.freq_table == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_data;
	}

	ret = exynos7_init_mif_table(&pdev->dev, data);
	if (ret)
		goto err_data;

	platform_set_drvdata(pdev, data);

	data->vdd_mif = devm_regulator_get(&pdev->dev, "vdd_intmif");

	rcu_read_lock();
	freq = exynos7_devfreq_mif_governor_data.cal_qos_max;
	target_opp = devfreq_recommended_opp(data->dev, &freq, 0);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(data->dev, "DEVFREQ(MIF) : Invalid OPP to set voltagen");
		ret = PTR_ERR(target_opp);
		goto err_data;
	}
	data->old_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	data->old_volt = get_limit_voltage(data->old_volt, data->volt_offset);
#endif

	rcu_read_unlock();

	regulator_set_voltage(data->vdd_mif, data->old_volt, REGULATOR_MAX_MICROVOLT);

	/* Obtain DLL Lock value on 416MHz */
	exynos7580_devfreq_set_dll_lock_value(data, DLL_LOCK_LV);

	data->devfreq = devfreq_add_device(data->dev,
						&exynos7_devfreq_mif_profile,
						"simple_exynos",
						&exynos7_devfreq_mif_governor_data);

	devfreq_nb = devm_kzalloc(&pdev->dev, sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (devfreq_nb == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}

	exynos7_devfreq_init_thermal();

	devfreq_nb->df = data->devfreq;
	devfreq_nb->nb.notifier_call = exynos7_devfreq_mif_notifier;

	exynos_devfreq_register(&devfreq_mif_exynos);
	exynos_ppmu_register_notifier(MIF, &devfreq_nb->nb);

	plat_data = &exynos7_qos_mif;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos7_devfreq_mif_governor_data.cal_qos_max;

	ret = sysfs_create_group(&data->devfreq->dev.kobj, &devfreq_mif_attr_group);

	register_reboot_notifier(&exynos7_mif_reboot_notifier);

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

static int exynos7_devfreq_mif_remove(struct platform_device *pdev)
{
	struct device_node *np;
	const struct of_device_id *match;
	devfreq_deinit_of_fn deinit_func;
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_mif_thermal_qos);
	pm_qos_remove_request(&exynos7_mif_qos);
	pm_qos_remove_request(&boot_mif_qos);

       /* Find proper deinit function for mif */
	for_each_matching_node_and_match(np, __devfreq_deinit_of_table, &match) {
		if (!strcmp(np->name, "bus_mif")) {
			deinit_func = match->data;
			deinit_func(data);
			break;
		}
	}

	platform_set_drvdata(pdev, NULL);

	return 0;
}

extern void exynos7_devfreq_set_tREFI_1x(struct devfreq_data_mif *data);
static int exynos7_devfreq_mif_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);

	exynos7_devfreq_set_tREFI_1x(data);

	pm_qos_update_request(&exynos7_mif_qos, devfreq_mif_opp_list[SUSPEND_LV].freq);
	pm_qos_update_request(&exynos7_mif_max_qos, devfreq_mif_opp_list[SUSPEND_LV].freq);

	return 0;
}

static int exynos7_devfreq_mif_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);
	uint32_t tmp;

	/* Enable auto clock gating after system is resumed */
	/* Enable MIF_NOC_RCG_EN */
	tmp = __raw_readl(EXYNOS7580_VA_SYSREG_MIF + 0x200);
	tmp |= 0xFF;
	__raw_writel(tmp, EXYNOS7580_VA_SYSREG_MIF + 0x200);

	/* Enable MIF_XIU_ASYNC_RCG_EN */
	tmp = __raw_readl(EXYNOS7580_VA_SYSREG_MIF + 0x208);
	tmp |= 0x7E;
	__raw_writel(tmp, EXYNOS7580_VA_SYSREG_MIF + 0x208);

	/* Enable MIF_US_RCG_EN */
	tmp = __raw_readl(EXYNOS7580_VA_SYSREG_MIF + 0x210);
	tmp |= 0x1;
	__raw_writel(tmp, EXYNOS7580_VA_SYSREG_MIF + 0x210);

	pm_qos_update_request(&exynos7_mif_qos, data->default_qos);
	pm_qos_update_request(&exynos7_mif_max_qos, PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);

	return 0;
}

static const struct dev_pm_ops exynos7_devfreq_mif_pm = {
	.suspend	= exynos7_devfreq_mif_suspend,
	.resume		= exynos7_devfreq_mif_resume,
};

static const struct of_device_id exynos7_devfreq_mif_match[] = {
	{ .compatible = "samsung,exynos7-devfreq-mif"
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos7_devfreq_mif_match);

static struct platform_driver exynos7_devfreq_mif_driver = {
	.probe	= exynos7_devfreq_mif_probe,
	.remove	= exynos7_devfreq_mif_remove,
	.driver	= {
		.name	= "exynos7-devfreq-mif",
		.owner	= THIS_MODULE,
		.pm		= &exynos7_devfreq_mif_pm,
		.of_match_table	= of_match_ptr(exynos7_devfreq_mif_match),
	},
};
module_platform_driver(exynos7_devfreq_mif_driver);
