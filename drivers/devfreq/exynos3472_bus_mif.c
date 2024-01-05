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
#include <linux/errno.h>
#include <linux/regulator/consumer.h>
#include <linux/reboot.h>
#include <linux/kobject.h>
#include <linux/sched.h>

#include <mach/regs-clock.h>
#include <mach/devfreq.h>
#include <mach/smc.h>
#include <mach/asv-exynos.h>

#include "devfreq_exynos.h"
#include "exynos3472_ppmu.h"
#include "governor.h"

#define MEMCONTROL_MRR_BYTE_SHIFT	(25)
#define MEMCONTROL_MRR_BYTE_MASK	(0x3 << MEMCONTROL_MRR_BYTE_SHIFT)
#define MRSTATUS_THERMAL_LV_SHIFT	(5)
#define MRSTATUS_THERMAL_LV_MASK	(0x7 << MRSTATUS_THERMAL_LV_SHIFT)

static DEFINE_MUTEX(media_mutex);
static unsigned int enabled_fimc_lite;
static unsigned int num_fimd1_layers;

static struct pm_qos_request exynos3472_mif_qos;
static struct pm_qos_request media_mif_qos;

static LIST_HEAD(mif_dvfs_list);

/* NoC list for MIF block */
static LIST_HEAD(mif_noc_list);

enum devfreq_mif_idx {
	LV0 = 0,
	LV1,
	LV2,
	LV3,
	LV4,
	LV_COUNT,
};

enum devfreq_mif_thermal_autorate {
	RATE_ONE = 0x0000005D,
	RATE_HALF = 0x0000002E,
	RATE_QUARTER = 0x00000017,
};

enum devfreq_mif_thermal_channel {
	THERMAL_CHANNEL0,
};

struct devfreq_data_mif {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_mif;
	unsigned long old_volt;

	struct opp *curr_opp;
	void __iomem *base_drex;
	void __iomem *base_pmu_mif;
	void __iomem *base_cmu_dmc;
	cputime64_t last_jiffies;

	bool use_dvfs;

	struct mutex lock;

	struct exynos3472_ppmu_handle *ppmu;
};

cputime64_t devfreq_mif_time_in_state[] = {
	0,
	0,
	0,
	0,
	0,
};

struct devfreq_opp_table devfreq_mif_opp_list[] = {
	{LV0,	533000,	875000},
	{LV1,	400000,	800000},
	{LV2,	266000,	800000},
	{LV3,	200000,	800000},
	{LV4,	 50000,	800000},
};

#if defined(CONFIG_DEVFREQ_GOV_PM_QOS)
static struct devfreq_pm_qos_data exynos3472_devfreq_mif_pm_qos_data = {
	.bytes_per_sec_per_hz = 8,
	.pm_qos_class = PM_QOS_BUS_THROUGHPUT,
};
#endif

#if defined(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)
static struct devfreq_simple_ondemand_data exynos3472_mif_governor_data = {
	.pm_qos_class	= PM_QOS_BUS_THROUGHPUT,
	.upthreshold	= 20,
	.cal_qos_max	= 533000,
};
#endif

struct devfreq_thermal_work {
	struct delayed_work devfreq_mif_thermal_work;
	enum devfreq_mif_thermal_channel channel;
	struct workqueue_struct *work_queue;
	unsigned int polling_period;
	unsigned long max_freq;
};

struct mif_regs_value {
	void __iomem *target_reg;
	unsigned int reg_value;
	unsigned int reg_mask;
	void __iomem *wait_reg;
	unsigned int wait_mask;
};

struct mif_clkdiv_info {
	struct list_head list;
	unsigned int lv_idx;
	struct mif_regs_value div_dmc1;
	unsigned int target_pll;
};

static unsigned int exynos3472_dmc1_div_table[][6] = {
	/* SCLK_DMC, ACLK_DMCD, ACLK_DMCP, DPHY, MPLL_PRE */
	{ 0, 1, 1, 1, 1, 1}, /* 533MHz,   266MHz,  133MHz,  BPLL */
	{ 0, 1, 1, 1, 1, 0}, /* 400MHz,   200MHz,  100MHz,  MPLL */
	{ 0, 1, 1, 1, 2, 0}, /* 266MHz,   133MHz,   66MHz,  MPLL */
	{ 0, 1, 1, 1, 3, 0}, /* 200MHz,   100MHz,   50MHz,  MPLL */
	{ 3, 1, 1, 1, 3, 0}, /*  50MHz,    25MHz,   12.5MHz,  MPLL */
};

struct timing_parameter {
	unsigned int timing_row;
	unsigned int timing_data;
	unsigned int timing_power;
};

static struct timing_parameter mif_timing_parameter[] = {
	{ /* 533Mhz */
		.timing_row             = 0x2347648D,
		.timing_data            = 0x26200539,
		.timing_power           = 0x38260225,
	}, { /* 400Mhz */
		.timing_row		= 0x1A35538A,
		.timing_data		= 0x26200539,
		.timing_power		= 0x281F0225,
	}, { /* 266Mhz */
		.timing_row             = 0x19244247,
		.timing_data            = 0x26200529,
		.timing_power           = 0x1C1F0225,
	}, { /* 200Mhz */
		.timing_row		= 0x192331C5,
		.timing_data		= 0x26200529,
		.timing_power		= 0x141F0225,
	}, { /*  50Mhz */
		.timing_row		= 0x192220C2,
		.timing_data		= 0x26200529,
		.timing_power		= 0x101F0225,
	}
};

static struct workqueue_struct *devfreq_mif_thermal_wq_ch0;
static struct devfreq_thermal_work devfreq_mif_ch0_work = {
        .channel = THERMAL_CHANNEL0,
        .polling_period = 1000,
};

static bool use_timing_set_0;
static struct devfreq_data_mif *data_mif;

static void exynos3472_devfreq_thermal_event(struct devfreq_thermal_work *work)
{
	if (work->polling_period == 0)
		return;

	queue_delayed_work(work->work_queue,
			&work->devfreq_mif_thermal_work,
			msecs_to_jiffies(work->polling_period));
}

extern void exynos3_update_district_int_level(unsigned int idx);

void exynos3_update_media_layers(enum devfreq_media_type media_type, unsigned int value)
{
	unsigned int num_total_layers, mif_idx = LV3, int_idx = LV2;
	unsigned long media_qos_mif_freq = 0;

	mutex_lock(&media_mutex);

	if (media_type == TYPE_FIMC_LITE)
		enabled_fimc_lite = value;
	else if (media_type == TYPE_FIMD1)
		num_fimd1_layers = value;

	num_total_layers = num_fimd1_layers;

	pr_debug("%s: fimc_lite = %s, num_fimd1_layers = %u, "
			"num_total_layers = %u\n", __func__,
			enabled_fimc_lite ? "enabled" : "disabled",
			num_fimd1_layers, num_total_layers);

	switch (num_total_layers) {
		case NUM_LAYER_5:
			int_idx = LV1;
			mif_idx = LV0;
			break;
		case NUM_LAYER_4:
			int_idx = LV1;
			mif_idx = LV1;
			break;
		case NUM_LAYER_3:
			int_idx = LV1;
			mif_idx = LV2;
			break;
		case NUM_LAYER_2:
			int_idx = LV1;
			mif_idx = LV3;
			break;
		case NUM_LAYER_1:
			int_idx = LV2;
			mif_idx = LV3;
		case NUM_LAYER_0:
			break;
	}

	if(enabled_fimc_lite) {
		int_idx = LV0;
		mif_idx = LV0;
	}

	media_qos_mif_freq = devfreq_mif_opp_list[mif_idx].freq;

	exynos3_update_district_int_level(int_idx);

	if (pm_qos_request_active(&media_mif_qos))
		pm_qos_update_request(&media_mif_qos, media_qos_mif_freq);

	mutex_unlock(&media_mutex);
}
EXPORT_SYMBOL_GPL(exynos3_update_media_layers);

static unsigned int exynos3472_mif_set_div(enum devfreq_mif_idx target_idx)
{
	unsigned int tmp;
	unsigned int timeout = 1000;
	struct mif_clkdiv_info *target_mif_clkdiv;

	list_for_each_entry(target_mif_clkdiv, &mif_dvfs_list, list) {
		if (target_mif_clkdiv->lv_idx == target_idx)
			break;
	}

	/*
	 * Setting for DMC1
	 */
	tmp = __raw_readl(target_mif_clkdiv->div_dmc1.target_reg);
	tmp &= ~target_mif_clkdiv->div_dmc1.reg_mask;
	tmp |= target_mif_clkdiv->div_dmc1.reg_value;
	__raw_writel(tmp, target_mif_clkdiv->div_dmc1.target_reg);

	/*
	 * Wait for divider change done
	 */
	do {
		tmp = __raw_readl(target_mif_clkdiv->div_dmc1.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : DMC1 DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_mif_clkdiv->div_dmc1.wait_mask);

	return 0;
}

static inline int exynos3472_devfreq_mif_get_idx(struct devfreq_opp_table *table,
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

static bool mif_is_need_pll_change(unsigned int old_value, unsigned int new_value)
{
	return (old_value == new_value) ? false : true;
}

/* MUXmpll_mif setting for using SCLKmpll_mif  */
static void exynos3472_set_sclk_mpll_muxed(void)
{
	struct clk *mout_mpll_mif, *sclk_mpll_mif;
	mout_mpll_mif = clk_get(NULL, "mout_mpll_mif");
	sclk_mpll_mif = clk_get(NULL, "sclk_mpll_mif");
	if (clk_set_parent(mout_mpll_mif, sclk_mpll_mif)) {
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mpll_mif->name, sclk_mpll_mif->name);
	}
}

static void exynos3472_mif_change_pll(enum devfreq_mif_idx target_idx)
{
	struct mif_clkdiv_info *target_mif_clkdiv;
	unsigned int old_pll;
	struct clk *mout_mpll_mif, *mout_bpll, *dout_dmc_pre;

	mout_mpll_mif = clk_get(NULL, "mout_mpll_mif");
	mout_bpll = clk_get(NULL, "mout_bpll");
	dout_dmc_pre = clk_get(NULL, "dout_dmc_pre");

	old_pll = (__raw_readl(EXYNOS3_CLKSRC_DMC) & EXYNOS3_CLKMUX_DMC_BUS_MASK) >> 4;

	list_for_each_entry(target_mif_clkdiv, &mif_dvfs_list, list) {
		if (target_mif_clkdiv->lv_idx == target_idx)
			break;
	}

	if (mif_is_need_pll_change(old_pll,
				target_mif_clkdiv->target_pll)) {
		if (old_pll) {
			printk("BPLL --> MPLL\n");
			if (clk_set_parent(dout_dmc_pre, mout_mpll_mif)) {
				pr_err("Unable to set parent %s of clock %s.\n",
						mout_mpll_mif->name, dout_dmc_pre->name);
			}
		} else {
			printk("MPLL --> BPLL\n");
			if (clk_set_parent(dout_dmc_pre, mout_bpll)) {
				pr_err("Unable to set parent %s of clock %s.\n",
						mout_mpll_mif->name, dout_dmc_pre->name);
			}
		}
	}
}

static void exynos3472_devfreq_pass_sclk_dphy(struct devfreq_data_mif *data, bool pass)
{
	unsigned int tmp;

	tmp = __raw_readl(data->base_cmu_dmc + 0x0800);
	tmp &= ~(0x1 << 2);
	tmp |= ((pass & 0x1) << 2);
	__raw_writel(tmp, data->base_cmu_dmc + 0x0800);
}

static void exynos3472_devfreq_update_timing_set(struct devfreq_data_mif *data)
{
	uint32_t val;
	int ret;

	ret = exynos_smc_readsfr(((uint32_t)EXYNOS3_PA_PMU_MIF + 0x54), &val);
	if (ret)
		pr_err("%s:exynos_smc_read_sfr returns error [ret] = %d\n", __func__, ret);
	use_timing_set_0 = (((val >> 31) & 0x1) == 0);
}

static void exynos3472_devfreq_mif_timing_parameter(struct devfreq_data_mif *data,
						int target_idx)
{
	struct timing_parameter *cur_timing_parameter;

	cur_timing_parameter = &mif_timing_parameter[target_idx];

	if (use_timing_set_0) {
		__raw_writel(cur_timing_parameter->timing_row, data->base_drex + 0xE4);
		__raw_writel(cur_timing_parameter->timing_data, data->base_drex + 0xE8);
		__raw_writel(cur_timing_parameter->timing_power, data->base_drex + 0xEC);

	} else {
		__raw_writel(cur_timing_parameter->timing_row, data->base_drex + 0x34);
		__raw_writel(cur_timing_parameter->timing_data, data->base_drex + 0x38);
		__raw_writel(cur_timing_parameter->timing_power, data->base_drex + 0x3C);
	}
}

static int exynos3472_devfreq_set_dll_voltage(struct devfreq_data_mif *data,
						int target_idx, unsigned long volt_offset)
{
	unsigned long target_volt;
	unsigned int tmp;
	struct opp *target_opp;

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(data->dev, &devfreq_mif_opp_list[target_idx].freq, 0);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(data->dev, "DEVFREQ(MIF) : Invalid OPP to find\n");
		return PTR_ERR(target_opp);
	}
	target_volt = opp_get_voltage(target_opp);
	target_volt += volt_offset;
	rcu_read_unlock();

	if (data->old_volt == target_volt)
		goto out;

	/* dynamic self refresh disable */
	tmp = __raw_readl(data->base_drex + 0x04);
	tmp &= ~(0x1 << 5);
	__raw_writel(tmp, data->base_drex + 0x04);

	/* direct cmd: 0x8 = REFSX(exit from self refresh) */
	__raw_writel(0x08000000, data->base_drex + 0x010);

	regulator_set_voltage(data->vdd_mif, target_volt, target_volt + VOLT_STEP);
	data->old_volt = target_volt;

	/* dynamic self refresh enable */
	tmp = __raw_readl(data->base_drex + 0x04);
	tmp |= (0x1 << 5);
	__raw_writel(tmp, data->base_drex + 0x04);

out:
	return 0;
}

static void exynos3472_devfreq_mif_change_timing_set(struct devfreq_data_mif *data)
{
	unsigned int tmp;
	int ret;

	ret = exynos_smc_readsfr(((uint32_t)EXYNOS3_PA_PMU_MIF + 0x54), &tmp);
	if (ret)
		printk("%s:exynos_smc_read_sfr returns error [ret] = %d\n", __func__, ret);

	if (use_timing_set_0)
		tmp |= (1 << 31);
	else
		tmp &= ~(1 << 31);

	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((uint32_t)EXYNOS3_PA_PMU_MIF + 0x54), tmp, 0);
	if (ret)
		printk("%s:exynos_smc returns error [ret] = %d\n", __func__, ret);


	use_timing_set_0 = !use_timing_set_0;
}

static void exynos3472_devfreq_mif_update_time(struct devfreq_data_mif *data,
					int target_idx)
{
	cputime64_t cur_time = get_jiffies_64();
	cputime64_t diff_time = cur_time - data->last_jiffies;

	devfreq_mif_time_in_state[target_idx] += diff_time;
	data->last_jiffies = cur_time;
}

static int exynos3472_devfreq_mif_set_freq(struct devfreq_data_mif *data,
					int target_idx,
					int old_idx)
{
	exynos3472_devfreq_update_timing_set(data);

	if (target_idx < old_idx) {
		exynos3472_devfreq_set_dll_voltage(data, target_idx, 0);
		exynos3472_devfreq_mif_timing_parameter(data, target_idx);
		exynos3472_devfreq_mif_change_timing_set(data);
		exynos3472_mif_set_div(target_idx);
		exynos3472_mif_change_pll(target_idx);

		if(target_idx == LV0)
		    exynos3472_devfreq_pass_sclk_dphy(data, true);
	} else {
		if(old_idx == LV0)
		    exynos3472_devfreq_pass_sclk_dphy(data, false);

		exynos3472_mif_set_div(target_idx);
		exynos3472_mif_change_pll(target_idx);
		exynos3472_devfreq_mif_timing_parameter(data, target_idx);
		exynos3472_devfreq_mif_change_timing_set(data);
		exynos3472_devfreq_set_dll_voltage(data, target_idx, 0);
	}

	return 0;
}

static int exynos3472_devfreq_mif_target(struct device *dev,
					    unsigned long *target_freq,
					    u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_mif *mif_data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_mif = mif_data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long old_freq;

	mutex_lock(&mif_data->lock);
	*target_freq = min(*target_freq, devfreq_mif_ch0_work.max_freq);

	/* get available opp information */
	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(dev, "DEVFREQ(MIF) : Invalid OPP to find\n");
		return PTR_ERR(target_opp);
	}

	*target_freq = opp_get_freq(target_opp);
	rcu_read_unlock();

	target_idx = exynos3472_devfreq_mif_get_idx(devfreq_mif_opp_list,
						ARRAY_SIZE(devfreq_mif_opp_list),
						*target_freq);
	old_idx = exynos3472_devfreq_mif_get_idx(devfreq_mif_opp_list,
						ARRAY_SIZE(devfreq_mif_opp_list),
						devfreq_mif->previous_freq);
	old_freq = devfreq_mif->previous_freq;

	if (target_idx < 0 ||
		old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq)
		goto out;

	exynos3472_devfreq_mif_set_freq(mif_data, target_idx, old_idx);
out:
	exynos3472_devfreq_mif_update_time(mif_data, target_idx);

	mutex_unlock(&mif_data->lock);

	return ret;
}

static int exynos3472_mif_bus_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{

	struct devfreq_data_mif *data = dev_get_drvdata(dev);

	unsigned long busy_data;
	unsigned int mif_ccnt = 0;
	unsigned long mif_pmcnt = 0;

	if (!data_mif->use_dvfs)
		return -EAGAIN;

	stat->current_frequency = data->devfreq->previous_freq;

	busy_data = exynos3472_ppmu_get_busy(data->ppmu, PPMU_SET_DDR,
					&mif_ccnt, &mif_pmcnt);

	/* disable ppmu */
	stat->total_time = 1;
	stat->busy_time = 0;

	return 0;
}

static struct devfreq_dev_profile exynos3472_mif_devfreq_profile = {
	.initial_freq	= 533000,
	.polling_ms	= 100,
	.target		= exynos3472_devfreq_mif_target,
	.get_dev_status	= exynos3472_mif_bus_get_dev_status,
};

static int exynos3472_mif_table(struct devfreq_data_mif *data)
{
	unsigned int i;
	unsigned int ret;
	struct mif_clkdiv_info *tmp_mif_table;

	/* make list for setting value for int DVS */
	for (i = LV0; i < LV_COUNT; i++) {
		tmp_mif_table = kzalloc(sizeof(struct mif_clkdiv_info), GFP_KERNEL);
		if (tmp_mif_table == NULL) {
			pr_err("DEVFREQ(MIF) : Failed to allocate mif table\n");
			ret = -ENOMEM;
			goto err_mif_table;
		}

		tmp_mif_table->lv_idx = i;

		/* Setting DIV information */
		tmp_mif_table->div_dmc1.target_reg = EXYNOS3_CLKDIV_DMC1;

		tmp_mif_table->div_dmc1.reg_value = ((exynos3472_dmc1_div_table[i][0] << EXYNOS3_CLKDIV_DMC1_DMC_SHIFT) |\
						     (exynos3472_dmc1_div_table[i][1] << EXYNOS3_CLKDIV_DMC1_DMCD_SHIFT) |\
						     (exynos3472_dmc1_div_table[i][2] << EXYNOS3_CLKDIV_DMC1_DMCP_SHIFT) |\
						     (exynos3472_dmc1_div_table[i][3] << EXYNOS3_CLKDIV_DMC1_DPHY_SHIFT) |\
						     (exynos3472_dmc1_div_table[i][4] << EXYNOS3_CLKDIV_DMC1_DMC_PRE_SHIFT));

		tmp_mif_table->div_dmc1.reg_mask = ((EXYNOS3_CLKDIV_DMC1_DMC_MASK)|\
						     (EXYNOS3_CLKDIV_DMC1_DMCP_MASK)|\
						     (EXYNOS3_CLKDIV_DMC1_DPHY_MASK)|\
						     (EXYNOS3_CLKDIV_DMC1_DMC_PRE_MASK)|\
						     (EXYNOS3_CLKDIV_DMC1_DMCD_MASK));
		tmp_mif_table->target_pll = exynos3472_dmc1_div_table[i][5];

		/* Setting DIV checking information */
		tmp_mif_table->div_dmc1.wait_reg = EXYNOS3_CLKDIV_STAT_DMC0;

		tmp_mif_table->div_dmc1.wait_mask = (EXYNOS3_CLKDIV_STAT_DMC_MASK |\
						     EXYNOS3_CLKDIV_STAT_DMCD_MASK |\
						     EXYNOS3_CLKDIV_STAT_DMC_PRE_MASK |\
						     EXYNOS3_CLKDIV_STAT_DMCP_MASK);


		list_add(&tmp_mif_table->list, &mif_dvfs_list);
	}

	/* will add code for ASV information setting function in here */

	for (i = 0; i < ARRAY_SIZE(devfreq_mif_opp_list); i++) {
		devfreq_mif_opp_list[i].volt = get_match_volt(ID_MIF, devfreq_mif_opp_list[i].freq);
		if (devfreq_mif_opp_list[i].volt == 0) {
			dev_err(data->dev, "Invalid value\n");
			goto err_opp;
		}

		pr_info("MIF %luKhz ASV is %luuV\n", devfreq_mif_opp_list[i].freq,
						devfreq_mif_opp_list[i].volt);

		ret = opp_add(data->dev, devfreq_mif_opp_list[i].freq, devfreq_mif_opp_list[i].volt);

		if (ret) {
			dev_err(data->dev, "Fail to add opp entries.\n");
			goto err_opp;
		}
	}

	return 0;

err_opp:
	kfree(tmp_mif_table);
err_mif_table:
	return ret;
}

static ssize_t mif_show_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t len = 0;
	ssize_t cnt_remain = (ssize_t)(PAGE_SIZE - 1);

	for (i = LV0; i < LV_COUNT; i++) {
		len += snprintf(buf + len, cnt_remain, "%ld %llu\n",
				devfreq_mif_opp_list[i].freq,
				(unsigned long long)devfreq_mif_time_in_state[i]);
		cnt_remain = (ssize_t)(PAGE_SIZE - len - 1);
	}

	return len;
}

static ssize_t mif_show_freq(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;
	ssize_t len = 0;
	ssize_t cnt_remain = (ssize_t)(PAGE_SIZE - 1);

	for (i = LV_COUNT - 1; i >= 0; i--) {
		len += snprintf(buf + len, cnt_remain, "%ld ",
				devfreq_mif_opp_list[i].freq);
		cnt_remain = (ssize_t)(PAGE_SIZE - len - 1);
	}

	len += snprintf(buf + len, cnt_remain, "\n");

	return len;
}

static DEVICE_ATTR(mif_time_in_state, 0644, mif_show_state, NULL);

static DEVICE_ATTR(available_frequencies, S_IRUGO, mif_show_freq, NULL);

static struct attribute *devfreq_mif_sysfs_entries[] = {
	&dev_attr_mif_time_in_state.attr,
	NULL,
};
static struct attribute_group devfreq_mif_attr_group = {
	.name	= "time_in_state",
	.attrs	= devfreq_mif_sysfs_entries,
};

static struct exynos_devfreq_platdata default_qos_mif_pd = {
	.default_qos = 200000,
};

static int exynos3472_mif_reboot_notifier_call(struct notifier_block *this,
					unsigned long code, void *_cmd)
{
	pm_qos_update_request(&exynos3472_mif_qos, exynos3472_mif_devfreq_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos3472_mif_reboot_notifier = {
	.notifier_call = exynos3472_mif_reboot_notifier_call,
};

static void exynos3472_devfreq_thermal_monitor(struct work_struct *work)
{
	struct delayed_work *d_work = container_of(work, struct delayed_work, work);
	struct devfreq_thermal_work *thermal_work =
		container_of(d_work, struct devfreq_thermal_work, devfreq_mif_thermal_work);
	unsigned int mrstatus, tmp_thermal_level, max_thermal_level = 0;
	unsigned int timingaref_value = RATE_ONE;
	unsigned long max_freq = exynos3472_mif_governor_data.cal_qos_max;
	bool throttling = false;
	unsigned int tmp, reverse_thermal_level;
	void __iomem *base_drex = data_mif->base_drex;

	tmp = __raw_readl(base_drex + 0x4);
	tmp &= ~MEMCONTROL_MRR_BYTE_SHIFT;
	tmp |= 0x1;
	__raw_writel(tmp, base_drex + 0x4);
	__raw_writel(0x09001000, base_drex + 0x10);
	mrstatus = __raw_readl(base_drex + 0x54);
	tmp_thermal_level = ((mrstatus & MRSTATUS_THERMAL_LV_MASK) >> MRSTATUS_THERMAL_LV_SHIFT);
	reverse_thermal_level = ((tmp_thermal_level & 0x1) << 2 )|((tmp_thermal_level & 0x4) >> 2);

	if (reverse_thermal_level > max_thermal_level)
		max_thermal_level = reverse_thermal_level;

	switch (max_thermal_level) {
		case 0:
		case 1:
		case 2:
		case 3:
			timingaref_value = RATE_ONE;
			thermal_work->polling_period = 1000;
			break;
		case 4:
			timingaref_value = RATE_HALF;
			thermal_work->polling_period = 300;
			break;
		case 6:
			throttling = true;
		case 5:
			timingaref_value = RATE_QUARTER;
			thermal_work->polling_period = 100;
			break;
		default:
			pr_err("DEVFREQ(MIF) : can't support memory thermal level\n");

			return;
	}

	if (throttling)
		max_freq = devfreq_mif_opp_list[LV1].freq;
	else
		max_freq = devfreq_mif_opp_list[LV0].freq;

	if (thermal_work->max_freq != max_freq) {
		thermal_work->max_freq = max_freq;
		mutex_lock(&data_mif->devfreq->lock);
		update_devfreq(data_mif->devfreq);
		mutex_unlock(&data_mif->devfreq->lock);
	}

	__raw_writel(timingaref_value, base_drex + 0x30);
	exynos3472_devfreq_thermal_event(thermal_work);
}

static void exynos3472_devfreq_init_thermal(void)
{
	devfreq_mif_thermal_wq_ch0 = create_freezable_workqueue("devfreq_thermal_wq_ch0");

	INIT_DELAYED_WORK(&devfreq_mif_ch0_work.devfreq_mif_thermal_work,
			exynos3472_devfreq_thermal_monitor);

	devfreq_mif_ch0_work.work_queue = devfreq_mif_thermal_wq_ch0;

	exynos3472_devfreq_thermal_event(&devfreq_mif_ch0_work);
}

static __devinit int exynos3472_busfreq_mif_probe(struct platform_device *pdev)
{
	int ret = 0, target_idx;
	struct devfreq_data_mif *data;
	struct exynos_devfreq_platdata *pdata;

	data = kzalloc(sizeof(struct devfreq_data_mif), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	data->base_drex = ioremap(EXYNOS3_PA_DMC, SZ_64K);
	if(!data->base_drex){
		pr_err("DEVFREQ(MIF) : base_drex ioremap is failed \n");
		goto err_drex;
	}

	data->base_pmu_mif = ioremap(EXYNOS3_PA_PMU_MIF, SZ_16K);
	if(!data->base_pmu_mif){
		pr_err("DEVFREQ(MIF) : base_pmu_mif ioremap is failed \n");
		goto err_pmu_mif;
	}

	data->base_cmu_dmc= ioremap(EXYNOS3_PA_CMU_DMC, SZ_16K);
	if(!data->base_cmu_dmc){
		pr_err("DEVFREQ(MIF) : base_pmu_dmc ioremap is failed \n");
		goto err_cmu_dmc;
	}

	data_mif = data;
	data->use_dvfs = false;
	data->dev = &pdev->dev;

	/* Setting table for int */
	exynos3472_mif_table(data);
	devfreq_mif_ch0_work.max_freq = exynos3472_mif_governor_data.cal_qos_max;

	data->last_jiffies = get_jiffies_64();

	mutex_init(&data->lock);
	platform_set_drvdata(pdev, data);

	data->vdd_mif = regulator_get(data->dev, "vdd_mif");
	if (IS_ERR(data->vdd_mif)) {
		pr_err("DEVFREQ(MIF) : Cannot get the regulator \"vdd_mif\"\n");
		ret = PTR_ERR(data->vdd_mif);
		goto err_regulator;
	}
	target_idx = exynos3472_devfreq_mif_get_idx(devfreq_mif_opp_list,
						ARRAY_SIZE(devfreq_mif_opp_list),
						exynos3472_mif_devfreq_profile.initial_freq);
	exynos3472_devfreq_set_dll_voltage(data, target_idx, 0);


	data->ppmu = exynos3472_ppmu_get(PPMU_SET_DDR);
	if (!data->ppmu)
		goto err_ppmu_get;

#if defined(CONFIG_DEVFREQ_GOV_PM_QOS)
	data->devfreq = devfreq_add_device(data->dev, &exynos3472_mif_devfreq_profile, &devfreq_pm_qos, &exynos3472_devfreq_mif_pm_qos_data);
#endif

#if defined(CONFIG_DEVFREQ_GOV_USERSPACE)
	data->devfreq = devfreq_add_device(data->dev, &exynos3472_mif_devfreq_profile, &devfreq_userspace, NULL);
#endif

#if defined(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)
	data->devfreq = devfreq_add_device(data->dev, &exynos3472_mif_devfreq_profile,
					&devfreq_simple_ondemand, &exynos3472_mif_governor_data);
#endif
	if (IS_ERR(data->devfreq)) {
		ret = PTR_ERR(data->devfreq);
		goto err_devfreq_add;
	}

	data->devfreq->max_freq = devfreq_mif_opp_list[LV0].freq;
	devfreq_register_opp_notifier(data->dev, data->devfreq);

	/* Create file for time_in_state */
	ret = sysfs_create_group(&data->devfreq->dev.kobj, &devfreq_mif_attr_group);
	if (ret) {
		pr_err("DEVFREQ(MIF) : Failed create mif attr group sysfs\n");
		goto err_devfreq_add;
	}

	/* Add sysfs for freq_table */
	ret = device_create_file(&data->devfreq->dev, &dev_attr_available_frequencies);
	if (ret) {
		pr_err("DEVFREQ(MIF) : Failed create available_frequencies sysfs\n");
		goto err_devfreq_add;
	}
#ifdef CONFIG_EXYNOS_THERMAL
	exynos3472_devfreq_init_thermal();
#endif
	pdata = data->dev->platform_data;
	if (!pdata)
		pdata = &default_qos_mif_pd;

	exynos3472_set_sclk_mpll_muxed();

	/* Register notify */
	pm_qos_add_request(&exynos3472_mif_qos, PM_QOS_BUS_THROUGHPUT, pdata->default_qos);
	pm_qos_add_request(&media_mif_qos, PM_QOS_BUS_THROUGHPUT, pdata->default_qos);
	pm_qos_update_request_timeout(&exynos3472_mif_qos, exynos3472_mif_devfreq_profile.initial_freq, 40000 * 1000);

	register_reboot_notifier(&exynos3472_mif_reboot_notifier);

	data->use_dvfs = true;

	return 0;

err_devfreq_add:
	devfreq_remove_device(data->devfreq);
err_ppmu_get:
	if (data->vdd_mif)
		regulator_put(data->vdd_mif);
err_regulator:
	platform_set_drvdata(pdev, NULL);
err_cmu_dmc:
	iounmap(data->base_cmu_dmc);
err_pmu_mif:
	iounmap(data->base_pmu_mif);
err_drex:
	iounmap(data->base_drex);
	kfree(data);
err_data:
	return ret;
}

static __devexit int exynos3472_busfreq_mif_remove(struct platform_device *pdev)
{

	struct devfreq_data_mif *data = platform_get_drvdata(pdev);

	if (data->base_drex)
	    iounmap(data->base_drex);
	devfreq_remove_device(data->devfreq);
	if (data->vdd_mif)
		regulator_put(data->vdd_mif);
	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

#define MIF_COLD_OFFSET	50000

static int exynos3472_busfreq_mif_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);
	int target_idx;

	if (pm_qos_request_active(&exynos3472_mif_qos))
		pm_qos_update_request(&exynos3472_mif_qos, exynos3472_mif_devfreq_profile.initial_freq);

	target_idx = exynos3472_devfreq_mif_get_idx(devfreq_mif_opp_list,
						ARRAY_SIZE(devfreq_mif_opp_list),
						exynos3472_mif_devfreq_profile.initial_freq);
	exynos3472_devfreq_set_dll_voltage(data, target_idx, MIF_COLD_OFFSET);

	return 0;
}

static int exynos3472_busfreq_mif_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos3472_mif_qos))
		pm_qos_update_request(&exynos3472_mif_qos, pdata->default_qos);

	return 0;
}

static const struct dev_pm_ops exynos3472_busfreq_mif_pm = {
	.suspend = exynos3472_busfreq_mif_suspend,
	.resume	= exynos3472_busfreq_mif_resume,
};

static struct platform_driver exynos3472_busfreq_mif_driver = {
	.probe	= exynos3472_busfreq_mif_probe,
	.remove	= __devexit_p(exynos3472_busfreq_mif_remove),
	.driver = {
		.name	= "exynos3472-busfreq-mif",
		.owner	= THIS_MODULE,
		.pm	= &exynos3472_busfreq_mif_pm,
	},
};

static int __init exynos3472_busfreq_mif_init(void)
{
	use_timing_set_0 = false;
	return platform_driver_register(&exynos3472_busfreq_mif_driver);
}
late_initcall(exynos3472_busfreq_mif_init);

static void __exit exynos3472_busfreq_mif_exit(void)
{
	platform_driver_unregister(&exynos3472_busfreq_mif_driver);
}
module_exit(exynos3472_busfreq_mif_exit);
