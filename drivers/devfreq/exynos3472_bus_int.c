/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/suspend.h>
#include <linux/opp.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/devfreq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/reboot.h>
#include <linux/kobject.h>
#include <linux/clk.h>

#include <mach/regs-clock.h>
#include <mach/devfreq.h>
#include <mach/asv-exynos.h>

#include "exynos3472_ppmu.h"
#include "devfreq_exynos.h"

#define SAFE_INT_VOLT(x)	(x + 25000)
#define INT_TIMEOUT_VAL		10000
#define INT_COLD_OFFSET	50000

static struct pm_qos_request exynos3472_int_qos;
struct devfreq_data_int *data_int;

static LIST_HEAD(int_dvfs_list);

struct devfreq_data_int {
	struct device *dev;
	struct devfreq *devfreq;
	struct regulator *vdd_int;
	unsigned long old_volt;
	unsigned long volt_offset;
	cputime64_t last_jiffies;

	bool use_dvfs;

	struct exynos3472_ppmu_handle *ppmu;
	struct mutex lock;
};

enum devfreq_int_idx {
	LV0,
	LV1,
	LV2,
	LV_COUNT,
};

cputime64_t devfreq_int_time_in_state[] = {
	0,
	0,
	0,
};

struct devfreq_opp_table devfreq_int_opp_list[] = {
	{LV0,		    400000, 1000000},
	{LV1,		    200000, 1000000},
	{LV2,		    100000,  900000},
};

struct int_regs_value {
	void __iomem *target_reg;
	unsigned int reg_value;
	unsigned int reg_mask;
	void __iomem *wait_reg;
	unsigned int wait_mask;
};

struct int_clkdiv_info {
	struct list_head list;
	unsigned int lv_idx;
	struct int_regs_value lbus;
	struct int_regs_value rbus;
	struct int_regs_value top;
	struct int_regs_value acp0;
	struct int_regs_value acp1;
	struct int_regs_value mfc;
};

static unsigned int exynos3472_int_lbus_div[][2] = {
/* ACLK_GDL, ACLK_GPL */
	{3, 1},
	{4, 1},
	{7, 1},
};

static unsigned int exynos3472_int_rbus_div[][2] = {
/* ACLK_GDR, ACLK_GPR */
	{3, 1},
	{4, 1},
	{7, 1},
};

static unsigned int exynos3472_int_top_div[][5] = {
/* ACLK_266, ACLK_160, ACLK_200, ACLK_100, ACLK_400 */
	{2, 4, 3, 7, 1},
	{4, 7, 4, 7, 3},
	{7, 7, 7, 7, 7},
};

static unsigned int exynos3472_int_acp0_div[][5] = {
/* ACLK_ACP, PCLK_ACP, ACP_DMC, ACP_DMCD, ACP_DMCP */
	{ 3, 1, 1, 1, 1},
	{ 3, 1, 3, 1, 1},
	{ 7, 1, 3, 3, 1},
};

#if 0
static unsigned int exynos3472_int_mfc_div[] = {
/* SCLK_MFC */
        1,      /* L0 */
        1,      /* L1 */
        1,      /* L2 */
        2,      /* L3 */
        3,      /* L4 */
        4,      /* L5 */
};
#endif

static struct pm_qos_request exynos3_int_media_qos;

void exynos3_update_district_int_level(unsigned idx)
{
	if (!pm_qos_request_active(&exynos3_int_media_qos))
		return;
	else
		pm_qos_update_request(&exynos3_int_media_qos, devfreq_int_opp_list[idx].freq);
}

static inline int exynos3472_devfreq_int_get_idx(struct devfreq_opp_table *table,
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

static void exynos3472_devfreq_int_update_time(struct devfreq_data_int *data,
						int target_idx)
{
	cputime64_t cur_time = get_jiffies_64();
	cputime64_t diff_time = cur_time - data->last_jiffies;

	devfreq_int_time_in_state[target_idx] += diff_time;
	data->last_jiffies = cur_time;
}

static unsigned int exynos3472_int_set_div(enum devfreq_int_idx target_idx)
{
	unsigned int tmp;
	unsigned int timeout;
	struct int_clkdiv_info *target_int_clkdiv;

	list_for_each_entry(target_int_clkdiv, &int_dvfs_list, list) {
		if (target_int_clkdiv->lv_idx == target_idx)
			break;
	}
	/*
	 * Setting for DIV
	 */
	tmp = __raw_readl(target_int_clkdiv->lbus.target_reg);
	tmp &= ~target_int_clkdiv->lbus.reg_mask;
	tmp |= target_int_clkdiv->lbus.reg_value;
	__raw_writel(tmp, target_int_clkdiv->lbus.target_reg);

	tmp = __raw_readl(target_int_clkdiv->rbus.target_reg);
	tmp &= ~target_int_clkdiv->rbus.reg_mask;
	tmp |= target_int_clkdiv->rbus.reg_value;
	__raw_writel(tmp, target_int_clkdiv->rbus.target_reg);

	tmp = __raw_readl(target_int_clkdiv->top.target_reg);
	tmp &= ~target_int_clkdiv->top.reg_mask;
	tmp |= target_int_clkdiv->top.reg_value;
	__raw_writel(tmp, target_int_clkdiv->top.target_reg);

	tmp = __raw_readl(target_int_clkdiv->acp0.target_reg);
	tmp &= ~target_int_clkdiv->acp0.reg_mask;
	tmp |= target_int_clkdiv->acp0.reg_value;
	__raw_writel(tmp, target_int_clkdiv->acp0.target_reg);

#if 0
	tmp = __raw_readl(target_int_clkdiv->mfc.target_reg);
	tmp &= ~target_int_clkdiv->mfc.reg_mask;
	tmp |= target_int_clkdiv->mfc.reg_value;
	__raw_writel(tmp, target_int_clkdiv->mfc.target_reg);
#endif

	/*
	 * Wait for divider change done
	 */
	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->lbus.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : Leftbus DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->lbus.wait_mask);

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->rbus.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : Rightbus DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->rbus.wait_mask);

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->top.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : TOP DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->top.wait_mask);

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->acp0.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : ACP0 DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->acp0.wait_mask);

#if 0
	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->mfc.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : MFC DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->mfc.wait_mask);
#endif

	return 0;
}

static int exynos3472_devfreq_int_target(struct device *dev,
				      unsigned long *target_freq,
				      u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_int *int_data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_int = int_data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long target_volt;
	unsigned long old_freq;

	mutex_lock(&int_data->lock);

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(dev, "DEVFREQ(INT) : Invalid OPP to find\n");
		return PTR_ERR(target_opp);
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);

	rcu_read_unlock();

	target_idx = exynos3472_devfreq_int_get_idx(devfreq_int_opp_list,
						ARRAY_SIZE(devfreq_int_opp_list),
						*target_freq);
	old_idx = exynos3472_devfreq_int_get_idx(devfreq_int_opp_list,
						ARRAY_SIZE(devfreq_int_opp_list),
						devfreq_int->previous_freq);
	old_freq = devfreq_int->previous_freq;

	if (target_idx < 0)
		goto out;

	if (old_freq == *target_freq)
		goto out;

	if (old_freq < *target_freq) {
		regulator_set_voltage(int_data->vdd_int, target_volt, SAFE_INT_VOLT(target_volt));
		ret = exynos3472_int_set_div(target_idx);
	} else {
		ret = exynos3472_int_set_div(target_idx);
		regulator_set_voltage(int_data->vdd_int, target_volt, SAFE_INT_VOLT(target_volt));
	}
out:
	exynos3472_devfreq_int_update_time(int_data, target_idx);

	mutex_unlock(&int_data->lock);

	return ret;
}

static int exynos3472_devfreq_int_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	struct devfreq_data_int *data = dev_get_drvdata(dev);
	unsigned long busy_data;
	unsigned int int_ccnt = 0;
	unsigned long int_pmcnt = 0;

	if (!data_int->use_dvfs)
		return -EAGAIN;

	busy_data = exynos3472_ppmu_get_busy(data->ppmu, PPMU_SET_INT,
					&int_ccnt, &int_pmcnt);

	stat->current_frequency = data->devfreq->previous_freq;

	/* disable ppmu */
	stat->total_time = 1;
	stat->busy_time = 0;

	return 0;
}

#if defined(CONFIG_DEVFREQ_GOV_PM_QOS)
static struct devfreq_pm_qos_data exynos3472_devfreq_int_pm_qos_data = {
	.bytes_per_sec_per_hz = 8,
	.pm_qos_class = PM_QOS_DEVICE_THROUGHPUT,
};
#endif

#if defined(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)
static struct devfreq_simple_ondemand_data exynos3472_int_governor_data = {
	.pm_qos_class	= PM_QOS_DEVICE_THROUGHPUT,
	.upthreshold	= 15,
	.cal_qos_max = 400000,
};
#endif

static struct devfreq_dev_profile exynos3472_int_devfreq_profile = {
	.initial_freq	= 400000,
	.polling_ms	= 100,
	.target		= exynos3472_devfreq_int_target,
	.get_dev_status	= exynos3472_devfreq_int_get_dev_status,
};

static int exynos3472_init_int_table(struct devfreq_data_int *data)
{
	unsigned int i;
	unsigned int ret;
	struct int_clkdiv_info *tmp_int_table;

	/* make list for setting value for int DVS */
	for (i = LV0; i < LV_COUNT; i++) {
		tmp_int_table = kzalloc(sizeof(struct int_clkdiv_info), GFP_KERNEL);

		tmp_int_table->lv_idx = i;

		/* Setting for LEFTBUS */
		tmp_int_table->lbus.target_reg = EXYNOS3_CLKDIV_LEFTBUS;
		tmp_int_table->lbus.reg_value = ((exynos3472_int_lbus_div[i][0] << EXYNOS3_CLKDIV_GDL_SHIFT) |\
						 (exynos3472_int_lbus_div[i][1] << EXYNOS3_CLKDIV_GPL_SHIFT));
		tmp_int_table->lbus.reg_mask = (EXYNOS3_CLKDIV_GDL_MASK |\
						EXYNOS3_CLKDIV_STAT_GPL_MASK);
		tmp_int_table->lbus.wait_reg = EXYNOS3_CLKDIV_STAT_LEFTBUS;
		tmp_int_table->lbus.wait_mask = (EXYNOS3_CLKDIV_STAT_GPL_MASK |\
						 EXYNOS3_CLKDIV_STAT_GDL_MASK);

		/* Setting for RIGHTBUS */
		tmp_int_table->rbus.target_reg = EXYNOS3_CLKDIV_RIGHTBUS;
		tmp_int_table->rbus.reg_value = ((exynos3472_int_rbus_div[i][0] << EXYNOS3_CLKDIV_GDR_SHIFT) |\
						 (exynos3472_int_rbus_div[i][1] << EXYNOS3_CLKDIV_GPR_SHIFT));
		tmp_int_table->rbus.reg_mask = (EXYNOS3_CLKDIV_GDR_MASK |\
						EXYNOS3_CLKDIV_STAT_GPR_MASK);
		tmp_int_table->rbus.wait_reg = EXYNOS3_CLKDIV_STAT_RIGHTBUS;
		tmp_int_table->rbus.wait_mask = (EXYNOS3_CLKDIV_STAT_GPR_MASK |\
						 EXYNOS3_CLKDIV_STAT_GDR_MASK);

		/* Setting for TOP */
		tmp_int_table->top.target_reg = EXYNOS3_CLKDIV_TOP;
		tmp_int_table->top.reg_value = ((exynos3472_int_top_div[i][0] << EXYNOS3_CLKDIV_ACLK_266_SHIFT) |
						 (exynos3472_int_top_div[i][1] << EXYNOS3_CLKDIV_ACLK_160_SHIFT) |
						 (exynos3472_int_top_div[i][2] << EXYNOS3_CLKDIV_ACLK_200_SHIFT) |
						 (exynos3472_int_top_div[i][3] << EXYNOS3_CLKDIV_ACLK_100_SHIFT) |
						 (exynos3472_int_top_div[i][4] << EXYNOS3_CLKDIV_ACLK_400_SHIFT));
		tmp_int_table->top.reg_mask = (EXYNOS3_CLKDIV_ACLK_266_MASK |
						EXYNOS3_CLKDIV_ACLK_160_MASK |
						EXYNOS3_CLKDIV_ACLK_200_MASK |
						EXYNOS3_CLKDIV_ACLK_100_MASK |
						EXYNOS3_CLKDIV_ACLK_400_MASK);
		tmp_int_table->top.wait_reg = EXYNOS3_CLKDIV_STAT_TOP;
		tmp_int_table->top.wait_mask = (EXYNOS3_CLKDIV_STAT_ACLK_266_MASK |
						 EXYNOS3_CLKDIV_STAT_ACLK_160_MASK |
						 EXYNOS3_CLKDIV_STAT_ACLK_200_MASK |
						 EXYNOS3_CLKDIV_STAT_ACLK_100_MASK |
						 EXYNOS3_CLKDIV_STAT_ACLK_400_MASK);

		/* Setting for ACP0 */
		tmp_int_table->acp0.target_reg = EXYNOS3_CLKDIV_ACP0;

		tmp_int_table->acp0.reg_value = ((exynos3472_int_acp0_div[i][0] << EXYNOS3_CLKDIV_ACP_SHIFT) |
						 (exynos3472_int_acp0_div[i][1] << EXYNOS3_CLKDIV_ACP_PCLK_SHIFT) |
						 (exynos3472_int_acp0_div[i][2] << EXYNOS3_CLKDIV_ACP_DMC_SHIFT) |
						 (exynos3472_int_acp0_div[i][3] << EXYNOS3_CLKDIV_ACP_DMCD_SHIFT) |
						 (exynos3472_int_acp0_div[i][4] << EXYNOS3_CLKDIV_ACP_DMCP_SHIFT));
		tmp_int_table->acp0.reg_mask = (EXYNOS3_CLKDIV_ACP_MASK |
						EXYNOS3_CLKDIV_ACP_PCLK_MASK |
						EXYNOS3_CLKDIV_ACP_DMC_MASK |
						EXYNOS3_CLKDIV_ACP_DMCD_MASK |
						EXYNOS3_CLKDIV_ACP_DMCP_MASK);
		tmp_int_table->acp0.wait_reg = EXYNOS3_CLKDIV_STAT_ACP0;
		tmp_int_table->acp0.wait_mask = (EXYNOS3_CLKDIV_STAT_ACP_MASK |
						 EXYNOS3_CLKDIV_STAT_ACP_PCLK_MASK |
						 EXYNOS3_CLKDIV_STAT_ACP_DMC_SHIFT |
						 EXYNOS3_CLKDIV_STAT_ACP_DMCD_SHIFT |
						 EXYNOS3_CLKDIV_STAT_ACP_DMCP_SHIFT);

#if 0
		/* Setting for MFC */
		tmp_int_table->mfc.target_reg = EXYNOS3_CLKDIV_MFC;
		tmp_int_table->mfc.reg_value = (exynos3472_int_mfc_div[i] << EXYNOS3_CLKDIV_MFC_SHIFT);
		tmp_int_table->mfc.reg_mask = EXYNOS3_CLKDIV_MFC_MASK;
		tmp_int_table->mfc.wait_reg = EXYNOS3_CLKDIV_STAT_MFC;
		tmp_int_table->mfc.wait_mask = EXYNOS3_CLKDIV_STAT_MFC_MASK;
#endif

		list_add(&tmp_int_table->list, &int_dvfs_list);
	}

	/* will add code for ASV information setting function in here */
	for (i = 0; i < ARRAY_SIZE(devfreq_int_opp_list); i++) {
		devfreq_int_opp_list[i].volt = get_match_volt(ID_INT, devfreq_int_opp_list[i].freq);
		if (devfreq_int_opp_list[i].volt == 0) {
			dev_err(data->dev, "Invalid value\n");
			return -EINVAL;
		}

		pr_info("INT %luKhz ASV is %luuV\n", devfreq_int_opp_list[i].freq,
							devfreq_int_opp_list[i].volt);

		ret = opp_add(data->dev, devfreq_int_opp_list[i].freq, devfreq_int_opp_list[i].volt);

		if (ret) {
			dev_err(data->dev, "Fail to add opp entries.\n");
			return ret;
		}
	}

	return 0;
}


static ssize_t int_show_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t len = 0;
	ssize_t cnt_remain = (ssize_t)(PAGE_SIZE - 1);

	for (i = LV0; i < LV_COUNT; i++) {
		len += snprintf(buf + len, cnt_remain, "%ld %llu\n",
			devfreq_int_opp_list[i].freq,
			(unsigned long long)devfreq_int_time_in_state[i]);
		cnt_remain = (ssize_t)(PAGE_SIZE - len - 1);
	}

	return len;
}

static ssize_t int_show_freq(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;
	ssize_t len = 0;
	ssize_t cnt_remain = (ssize_t)(PAGE_SIZE - 1);

	for (i = LV_COUNT - 1; i >= 0; i--) {
		len += snprintf(buf + len, cnt_remain, "%lu ",
				devfreq_int_opp_list[i].freq);
		cnt_remain = (ssize_t)(PAGE_SIZE - len - 1);
	}

	len += snprintf(buf + len, cnt_remain, "\n");

	return len;
}

static DEVICE_ATTR(int_time_in_state, 0644, int_show_state, NULL);
static DEVICE_ATTR(available_frequencies, S_IRUGO, int_show_freq, NULL);

static struct attribute *devfreq_int_sysfs_entries[] = {
	&dev_attr_int_time_in_state.attr,
	NULL,
};
static struct attribute_group busfreq_int_attr_group = {
	.name	= "time_in_state",
	.attrs	= devfreq_int_sysfs_entries,
};

static struct exynos_devfreq_platdata default_qos_int_pd = {
	.default_qos = 100000,
};

static int exynos3472_int_reboot_notifier_call(struct notifier_block *this,
				unsigned long code, void *_cmd)
{
	pm_qos_update_request(&exynos3472_int_qos, devfreq_int_opp_list[LV0].freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos3472_int_reboot_notifier = {
	.notifier_call = exynos3472_int_reboot_notifier_call,
};

static __devinit int exynos3472_busfreq_int_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_int *data;
	struct exynos_devfreq_platdata *pdata;

	data = kzalloc(sizeof(struct devfreq_data_int), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	data_int = data;
	data->use_dvfs = false;
	data->dev = &pdev->dev;

	/* Setting table for int */
	ret = exynos3472_init_int_table(data);
	if (ret)
		goto err_inittable;

	mutex_init(&data->lock);
	platform_set_drvdata(pdev, data);

	data->last_jiffies = get_jiffies_64();
	data->volt_offset = INT_COLD_OFFSET;

	data->vdd_int = regulator_get(data->dev, "vdd_int");
	if (IS_ERR(data->vdd_int)) {
		dev_err(data->dev, "Cannot get the regulator \"vdd_int\"\n");
		ret = PTR_ERR(data->vdd_int);
		goto err_regulator;
	}

	/* Init PPMU for INT devfreq */
	data->ppmu = exynos3472_ppmu_get(PPMU_SET_INT);
	if (!data->ppmu)
		goto err_ppmu_get;

#if defined(CONFIG_DEVFREQ_GOV_PM_QOS)
	data->devfreq = devfreq_add_device(data->dev, &exynos3472_int_devfreq_profile,
			&devfreq_pm_qos, &exynos3472_devfreq_int_pm_qos_data);
#endif
#if defined(CONFIG_DEVFREQ_GOV_USERSPACE)
	data->devfreq = devfreq_add_device(data->dev, &exynos3472_int_devfreq_profile,
			&devfreq_userspace, NULL);
#endif
#if defined(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)
	data->devfreq = devfreq_add_device(data->dev, &exynos3472_int_devfreq_profile,
			&devfreq_simple_ondemand, &exynos3472_int_governor_data);
	data->devfreq->max_freq = devfreq_int_opp_list[LV0].freq;
#endif
	if (IS_ERR(data->devfreq)) {
		ret = PTR_ERR(data->devfreq);
		goto err_devfreq_add;
	}

	devfreq_register_opp_notifier(data->dev, data->devfreq);

	/* Create file for time_in_state */
	ret = sysfs_create_group(&data->devfreq->dev.kobj, &busfreq_int_attr_group);

	/* Add sysfs for freq_table */
	ret = device_create_file(&data->devfreq->dev, &dev_attr_available_frequencies);
	if (ret)
		pr_err("%s: Fail to create sysfs file\n", __func__);

	pdata = pdev->dev.platform_data;
	if (!pdata)
		pdata = &default_qos_int_pd;

	/* Register Notify */
	pm_qos_add_request(&exynos3472_int_qos, PM_QOS_DEVICE_THROUGHPUT, pdata->default_qos);
	pm_qos_add_request(&exynos3_int_media_qos, PM_QOS_DEVICE_THROUGHPUT, pdata->default_qos);
	register_reboot_notifier(&exynos3472_int_reboot_notifier);

	data_int->use_dvfs = true;

	return ret;

err_devfreq_add:
	devfreq_remove_device(data->devfreq);
err_ppmu_get:
	if (data->vdd_int)
		regulator_put(data->vdd_int);
err_regulator:
	platform_set_drvdata(pdev, NULL);
err_inittable:
	kfree(data);
err_data:
	return ret;
}

static __devexit int exynos3472_busfreq_int_remove(struct platform_device *pdev)
{
	struct devfreq_data_int *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);
	if (data->vdd_int)
		regulator_put(data->vdd_int);
	kfree(data);

	return 0;
}

static int exynos3472_busfreq_int_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_int *data = platform_get_drvdata(pdev);
	unsigned int temp_volt;

	if (pm_qos_request_active(&exynos3472_int_qos))
		pm_qos_update_request(&exynos3472_int_qos, exynos3472_int_devfreq_profile.initial_freq);

	temp_volt = get_match_volt(ID_INT, devfreq_int_opp_list[0].freq);
	regulator_set_voltage(data->vdd_int, (temp_volt + INT_COLD_OFFSET),
				SAFE_INT_VOLT(temp_volt + INT_COLD_OFFSET));

	return 0;
}

static int exynos3472_busfreq_int_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos3472_int_qos))
		pm_qos_update_request(&exynos3472_int_qos, pdata->default_qos);

	return 0;
}

static const struct dev_pm_ops exynos3472_busfreq_int_pm = {
	.suspend	= exynos3472_busfreq_int_suspend,
	.resume		= exynos3472_busfreq_int_resume,
};

static struct platform_driver exynos3472_busfreq_int_driver = {
	.probe	= exynos3472_busfreq_int_probe,
	.remove	= __devexit_p(exynos3472_busfreq_int_remove),
	.driver = {
		.name	= "exynos3472-busfreq-int",
		.owner	= THIS_MODULE,
		.pm	= &exynos3472_busfreq_int_pm,
	},
};

static int __init exynos3472_busfreq_int_init(void)
{
	return platform_driver_register(&exynos3472_busfreq_int_driver);
}
late_initcall(exynos3472_busfreq_int_init);

static void __exit exynos3472_busfreq_int_exit(void)
{
	platform_driver_unregister(&exynos3472_busfreq_int_driver);
}
module_exit(exynos3472_busfreq_int_exit);
