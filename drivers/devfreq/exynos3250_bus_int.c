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
#include <mach/tmu.h>

#include "exynos3250_ppmu.h"
#include "devfreq_exynos.h"

#define SAFE_INT_VOLT(x)	(x + 25000)
#define INT_TIMEOUT_VAL		10000
#define COLD_VOLT_OFFSET	(50000)
#ifdef CONFIG_EXYNOS_THERMAL
bool int_is_probed = false;
#endif

static struct pm_qos_request exynos3250_int_qos;
struct devfreq_data_int *data_int;
static unsigned int volt_offset;

static LIST_HEAD(int_dvfs_list);

struct devfreq_data_int {
	struct device *dev;
	struct devfreq *devfreq;
	struct regulator *vdd_int;
	unsigned long old_volt;
	cputime64_t last_jiffies;

	bool use_dvfs;

	struct exynos3250_ppmu_handle *ppmu;
	struct notifier_block tmu_notifier;
	struct mutex lock;
};

enum devfreq_int_idx {
	LV0_ISP_BOOST,
	LV0_ISP_PREVIEW,
	LV0,
	LV1,
	LV2,
	LV3,
	LV_COUNT,
};

cputime64_t devfreq_int_time_in_state[] = {
	0,
	0,
	0,
	0,
	0,
	0,
};

struct devfreq_opp_table devfreq_int_opp_list[] = {
	{LV0_ISP_BOOST,	    135000, 1000000},
	{LV0_ISP_PREVIEW,   134000, 1000000},
	{LV0,		    133000, 1000000},
	{LV1,		    100000, 1000000},
	{LV2,		     80000,  900000},
	{LV3,		     50000,  900000},
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

static unsigned int exynos3250_int_lbus_div[][2] = {
/* ACLK_GDL, ACLK_GPL */
	{2, 1},		/* L0 */
	{2, 1},		/* L1 */
	{2, 1},		/* L2 */
	{3, 1},		/* L3 */
	{3, 1},		/* L4 */
	{3, 1},		/* L5 */
};

static unsigned int exynos3250_int_rbus_div[][2] = {
/* ACLK_GDR, ACLK_GPR */
	{2, 1},		/* L0 */
	{2, 1},		/* L1 */
	{2, 1},		/* L2 */
	{3, 1},		/* L3 */
	{3, 1},		/* L4 */
	{3, 1},		/* L5 */
};

static unsigned int exynos3250_int_top_div[][5] = {
/* ACLK_266, ACLK_160, ACLK_200, ACLK_100, ACLK_400 */
	{2, 3, 2, 7, 0},	/* L0 */ /*ACLK266 is only 267MHz*/
	{3, 3, 2, 7, 0},	/* L1 */
	{7, 3, 2, 7, 7},	/* L2 */
	{7, 4, 3, 7, 7},	/* L3 */
	{7, 5, 4, 7, 7},	/* L4 */
	{7, 7, 7, 7, 7},	/* L5 */
};

static unsigned int exynos3250_int_acp0_div[][5] = {
/* ACLK_ACP, PCLK_ACP, ACP_DMC, ACP_DMCD, ACP_DMCP */
	{ 7, 1, 3, 1, 1},	/* LV0 */
	{ 7, 1, 3, 1, 1},	/* LV1 */
	{ 7, 1, 3, 1, 1},	/* LV0 */
	{ 7, 1, 3, 1, 1},	/* LV1 */
	{ 7, 1, 3, 1, 1},	/* LV2 */
	{ 7, 1, 3, 1, 1},	/* LV3 */
};

static unsigned int exynos3250_int_mfc_div[] = {
/* SCLK_MFC */
        1,      /* L0 */
        1,      /* L1 */
        1,      /* L2 */
        2,      /* L3 */
        3,      /* L4 */
        4,      /* L5 */
};

static inline int exynos3250_devfreq_int_get_idx(struct devfreq_opp_table *table,
				unsigned int size,
				unsigned long freq)
{
	int i;

	for (i = 0; i < size; i++) {
		if (table[i].freq == freq)
			return i;
	}

	return -1;
}

static void exynos3250_devfreq_int_update_time(struct devfreq_data_int *data,
						int target_idx)
{
	cputime64_t cur_time = get_jiffies_64();
	cputime64_t diff_time = cur_time - data->last_jiffies;

	devfreq_int_time_in_state[target_idx] += diff_time;
	data->last_jiffies = cur_time;
}

static unsigned int exynos3250_int_set_div(enum devfreq_int_idx target_idx)
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

	tmp = __raw_readl(target_int_clkdiv->mfc.target_reg);
	tmp &= ~target_int_clkdiv->mfc.reg_mask;
	tmp |= target_int_clkdiv->mfc.reg_value;
	__raw_writel(tmp, target_int_clkdiv->mfc.target_reg);

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

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->mfc.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : MFC DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->mfc.wait_mask);

	return 0;
}

static int exynos3250_devfreq_int_target(struct device *dev,
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
	target_volt += volt_offset;
	rcu_read_unlock();

	target_idx = exynos3250_devfreq_int_get_idx(devfreq_int_opp_list,
						ARRAY_SIZE(devfreq_int_opp_list),
						*target_freq);
	old_idx = exynos3250_devfreq_int_get_idx(devfreq_int_opp_list,
						ARRAY_SIZE(devfreq_int_opp_list),
						devfreq_int->previous_freq);
	old_freq = devfreq_int->previous_freq;

	if (target_idx < 0)
		goto out;

	if (old_freq == *target_freq)
		goto out;

	if (old_freq < *target_freq) {
		regulator_set_voltage(int_data->vdd_int, target_volt, target_volt + VOLT_STEP);
		ret = exynos3250_int_set_div(target_idx);
	} else {
		ret = exynos3250_int_set_div(target_idx);
		regulator_set_voltage(int_data->vdd_int, target_volt, target_volt + VOLT_STEP);
	}
out:
	exynos3250_devfreq_int_update_time(int_data, target_idx);

	mutex_unlock(&int_data->lock);

	return ret;
}

static int exynos3250_devfreq_int_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	struct devfreq_data_int *data = dev_get_drvdata(dev);
	unsigned long busy_data;
	unsigned int int_ccnt = 0;
	unsigned long int_pmcnt = 0;

	if (!data_int->use_dvfs)
		return -EAGAIN;

	busy_data = exynos3250_ppmu_get_busy(data->ppmu, PPMU_SET_INT,
					&int_ccnt, &int_pmcnt);

	stat->current_frequency = data->devfreq->previous_freq;
	stat->total_time = int_ccnt;
	stat->busy_time = int_pmcnt;

	return 0;
}

#if defined(CONFIG_DEVFREQ_GOV_PM_QOS)
static struct devfreq_pm_qos_data exynos3250_devfreq_int_pm_qos_data = {
	.bytes_per_sec_per_hz = 8,
	.pm_qos_class = PM_QOS_DEVICE_THROUGHPUT,
};
#endif

#if defined(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)
static struct devfreq_simple_ondemand_data exynos3250_int_governor_data = {
	.pm_qos_class	= PM_QOS_DEVICE_THROUGHPUT,
	.upthreshold	= 15,
	.cal_qos_max = 133000,
};
#endif

static struct devfreq_dev_profile exynos3250_int_devfreq_profile = {
	.initial_freq	= 133000,
	.polling_ms	= 100,
	.target		= exynos3250_devfreq_int_target,
	.get_dev_status	= exynos3250_devfreq_int_get_dev_status,
};

static int exynos3250_init_int_table(struct devfreq_data_int *data)
{
	unsigned int i;
	unsigned int ret;
	struct int_clkdiv_info *tmp_int_table;

	/* make list for setting value for int DVS */
	for (i = LV0_ISP_BOOST; i < LV_COUNT; i++) {
		tmp_int_table = kzalloc(sizeof(struct int_clkdiv_info), GFP_KERNEL);

		tmp_int_table->lv_idx = i;

		/* Setting for LEFTBUS */
		tmp_int_table->lbus.target_reg = EXYNOS3_CLKDIV_LEFTBUS;
		tmp_int_table->lbus.reg_value = ((exynos3250_int_lbus_div[i][0] << EXYNOS3_CLKDIV_GDL_SHIFT) |\
						 (exynos3250_int_lbus_div[i][1] << EXYNOS3_CLKDIV_GPL_SHIFT));
		tmp_int_table->lbus.reg_mask = (EXYNOS3_CLKDIV_GDL_MASK |\
						EXYNOS3_CLKDIV_STAT_GPL_MASK);
		tmp_int_table->lbus.wait_reg = EXYNOS3_CLKDIV_STAT_LEFTBUS;
		tmp_int_table->lbus.wait_mask = (EXYNOS3_CLKDIV_STAT_GPL_MASK |\
						 EXYNOS3_CLKDIV_STAT_GDL_MASK);

		/* Setting for RIGHTBUS */
		tmp_int_table->rbus.target_reg = EXYNOS3_CLKDIV_RIGHTBUS;
		tmp_int_table->rbus.reg_value = ((exynos3250_int_rbus_div[i][0] << EXYNOS3_CLKDIV_GDR_SHIFT) |\
						 (exynos3250_int_rbus_div[i][1] << EXYNOS3_CLKDIV_GPR_SHIFT));
		tmp_int_table->rbus.reg_mask = (EXYNOS3_CLKDIV_GDR_MASK |\
						EXYNOS3_CLKDIV_STAT_GPR_MASK);
		tmp_int_table->rbus.wait_reg = EXYNOS3_CLKDIV_STAT_RIGHTBUS;
		tmp_int_table->rbus.wait_mask = (EXYNOS3_CLKDIV_STAT_GPR_MASK |\
						 EXYNOS3_CLKDIV_STAT_GDR_MASK);

		/* Setting for TOP */
		tmp_int_table->top.target_reg = EXYNOS3_CLKDIV_TOP;
		tmp_int_table->top.reg_value = ((exynos3250_int_top_div[i][0] << EXYNOS3_CLKDIV_ACLK_266_SHIFT) |
						 (exynos3250_int_top_div[i][1] << EXYNOS3_CLKDIV_ACLK_160_SHIFT) |
						 (exynos3250_int_top_div[i][2] << EXYNOS3_CLKDIV_ACLK_200_SHIFT) |
						 (exynos3250_int_top_div[i][3] << EXYNOS3_CLKDIV_ACLK_100_SHIFT) |
						 (exynos3250_int_top_div[i][4] << EXYNOS3_CLKDIV_ACLK_400_SHIFT));
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

		tmp_int_table->acp0.reg_value = ((exynos3250_int_acp0_div[i][0] << EXYNOS3_CLKDIV_ACP_SHIFT) |
						 (exynos3250_int_acp0_div[i][1] << EXYNOS3_CLKDIV_ACP_PCLK_SHIFT) |
						 (exynos3250_int_acp0_div[i][2] << EXYNOS3_CLKDIV_ACP_DMC_SHIFT) |
						 (exynos3250_int_acp0_div[i][3] << EXYNOS3_CLKDIV_ACP_DMCD_SHIFT) |
						 (exynos3250_int_acp0_div[i][4] << EXYNOS3_CLKDIV_ACP_DMCP_SHIFT));
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

		/* Setting for MFC */
		tmp_int_table->mfc.target_reg = EXYNOS3_CLKDIV_MFC;
		tmp_int_table->mfc.reg_value = (exynos3250_int_mfc_div[i] << EXYNOS3_CLKDIV_MFC_SHIFT);
		tmp_int_table->mfc.reg_mask = EXYNOS3_CLKDIV_MFC_MASK;
		tmp_int_table->mfc.wait_reg = EXYNOS3_CLKDIV_STAT_MFC;
		tmp_int_table->mfc.wait_mask = EXYNOS3_CLKDIV_STAT_MFC_MASK;

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

	for (i = LV0_ISP_BOOST; i < LV_COUNT; i++) {
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
	.default_qos = 133000,
};

static int exynos3250_int_reboot_notifier_call(struct notifier_block *this,
				unsigned long code, void *_cmd)
{
	pm_qos_update_request(&exynos3250_int_qos, devfreq_int_opp_list[LV0].freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos3250_int_reboot_notifier = {
	.notifier_call = exynos3250_int_reboot_notifier_call,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos3250_devfreq_int_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_int *data = container_of(nb, struct devfreq_data_int,
			tmu_notifier);
	int volt;
	int *on = v;

	if (event != TMU_COLD)
		return NOTIFY_OK;

	mutex_lock(&data->lock);
	if (*on) {
		if (volt_offset)
			goto out;
		else
			volt_offset = COLD_VOLT_OFFSET;

		volt = regulator_get_voltage(data->vdd_int);
		volt += COLD_VOLT_OFFSET;
		regulator_set_voltage(data->vdd_int, volt, volt + VOLT_STEP);

	} else {
		if (!volt_offset)
			goto out;
		else
			volt_offset = 0;

		volt = regulator_get_voltage(data->vdd_int);
		volt -= COLD_VOLT_OFFSET;
		regulator_set_voltage(data->vdd_int, volt, volt + VOLT_STEP);
	}

out:
	mutex_unlock(&data->lock);

	return NOTIFY_OK;
}
#endif

static __devinit int exynos3250_busfreq_int_probe(struct platform_device *pdev)
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
	ret = exynos3250_init_int_table(data);
	if (ret)
		goto err_inittable;

	mutex_init(&data->lock);
	platform_set_drvdata(pdev, data);

	data->last_jiffies = get_jiffies_64();
#ifdef CONFIG_EXYNOS_THERMAL
	/* For low temperature compensation when boot time */
	volt_offset = COLD_VOLT_OFFSET;
	data->tmu_notifier.notifier_call = exynos3250_devfreq_int_tmu_notifier;
	exynos_tmu_add_notifier(&data->tmu_notifier);
	int_is_probed = true;
#endif
	data->vdd_int = regulator_get(data->dev, "vdd_int");
	if (IS_ERR(data->vdd_int)) {
		dev_err(data->dev, "Cannot get the regulator \"vdd_int\"\n");
		ret = PTR_ERR(data->vdd_int);
		goto err_regulator;
	}

	/* Init PPMU for INT devfreq */
	data->ppmu = exynos3250_ppmu_get(PPMU_SET_INT);
	if (!data->ppmu)
		goto err_ppmu_get;

#if defined(CONFIG_DEVFREQ_GOV_PM_QOS)
	data->devfreq = devfreq_add_device(data->dev, &exynos3250_int_devfreq_profile,
			&devfreq_pm_qos, &exynos3250_devfreq_int_pm_qos_data);
#endif
#if defined(CONFIG_DEVFREQ_GOV_USERSPACE)
	data->devfreq = devfreq_add_device(data->dev, &exynos3250_int_devfreq_profile,
			&devfreq_userspace, NULL);
#endif
#if defined(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)
	data->devfreq = devfreq_add_device(data->dev, &exynos3250_int_devfreq_profile,
			&devfreq_simple_ondemand, &exynos3250_int_governor_data);
	data->devfreq->max_freq = exynos3250_int_governor_data.cal_qos_max;
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
	pm_qos_add_request(&exynos3250_int_qos, PM_QOS_DEVICE_THROUGHPUT, pdata->default_qos);

	register_reboot_notifier(&exynos3250_int_reboot_notifier);

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

static __devexit int exynos3250_busfreq_int_remove(struct platform_device *pdev)
{
	struct devfreq_data_int *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);
	if (data->vdd_int)
		regulator_put(data->vdd_int);
	kfree(data);

	return 0;
}

static int exynos3250_busfreq_int_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos3250_int_qos))
		pm_qos_update_request(&exynos3250_int_qos, exynos3250_int_devfreq_profile.initial_freq);

	return 0;
}

static int exynos3250_busfreq_int_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos3250_int_qos))
		pm_qos_update_request(&exynos3250_int_qos, pdata->default_qos);

	return 0;
}

static const struct dev_pm_ops exynos3250_busfreq_int_pm = {
	.suspend	= exynos3250_busfreq_int_suspend,
	.resume		= exynos3250_busfreq_int_resume,
};

static struct platform_driver exynos3250_busfreq_int_driver = {
	.probe	= exynos3250_busfreq_int_probe,
	.remove	= __devexit_p(exynos3250_busfreq_int_remove),
	.driver = {
		.name	= "exynos3250-busfreq-int",
		.owner	= THIS_MODULE,
		.pm	= &exynos3250_busfreq_int_pm,
	},
};

static int __init exynos3250_busfreq_int_init(void)
{
	return platform_driver_register(&exynos3250_busfreq_int_driver);
}
late_initcall(exynos3250_busfreq_int_init);

static void __exit exynos3250_busfreq_int_exit(void)
{
	platform_driver_unregister(&exynos3250_busfreq_int_driver);
}
module_exit(exynos3250_busfreq_int_exit);
