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
#include <linux/fb.h>

#include <mach/regs-clock-exynos4415.h>
#include <mach/devfreq.h>
#include <mach/asv-exynos.h>
#include <mach/sec_debug.h>
#ifdef CONFIG_EXYNOS_THERMAL
#include <mach/tmu.h>
#endif

#include "exynos4415_ppmu.h"

#define SAFE_INT_VOLT(x)	(x + 25000)

#define INT_TIMEOUT_VAL		10000
#define COLD_OFFSET			50000

#ifdef CONFIG_EXYNOS_THERMAL
bool int_is_probed;
#endif
static struct device *int_dev;

static unsigned int cold_margin = 0;
static struct pm_qos_request exynos4415_int_qos;
static struct pm_qos_request exynos4415_int_boot_qos;
static struct pm_qos_request exynos4415_int_fb_qos;
cputime64_t int_pre_time;

static LIST_HEAD(int_dvfs_list);

struct busfreq_data_int {
	struct device *dev;
	struct devfreq *devfreq;
	struct opp *curr_opp;
	struct regulator *vdd_int;
	struct exynos4270_ppmu_handle *ppmu;
	struct mutex lock;

	unsigned long old_voltage;
	unsigned long target_voltage;
	unsigned long constraint_voltage;
} *data_int;

enum int_bus_idx {
	LV_0_A,
	LV_1_B,
	LV_0,
	LV_1,
	LV_2,
	LV_3,
	LV_4,
	LV_5,
	LV_END,
};

struct int_bus_opp_table {
	unsigned int idx;
	unsigned long clk;
	unsigned long volt;
	cputime64_t time_in_state;
};

struct int_bus_opp_table int_bus_opp_list[] = {
	{LV_0_A, 269000, 950000, 0},
	{LV_1_B, 268000, 950000, 0},
	{LV_0, 267000, 950000, 0},
	{LV_1, 200000, 950000, 0},
	{LV_2, 190000, 925000, 0},
	{LV_3, 160000, 925000, 0},
	{LV_4, 133000, 925000, 0},
	{LV_5, 100000, 925000, 0},
};

unsigned int exynos4415_int_asv_abb[LV_END] = {0, };

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
	struct int_regs_value cam;
	struct int_regs_value cam1;
	struct int_regs_value isp0;
	struct int_regs_value isp1;
	struct int_regs_value isp0_sub0;
};

static unsigned int exynos4415_int_lbus_div[][2] = {
/* ACLK_GDL, ACLK_GPL */
	{ 3, 1 },		/* LV0_A */
	{ 3, 1 },		/* LV1_B */
	{ 3, 1 },		/* LV0 */
	{ 4, 1 },		/* LV1 */
	{ 4, 1 },		/* LV2 */
	{ 4, 1 },		/* LV3 */
	{ 5, 1 },		/* LV4 */
	{ 7, 1 },		/* LV5 */
};

static unsigned int exynos4415_int_rbus_div[][2] = {
/* ACLK_GDR, ACLK_GPR */
	{ 3, 1 },		/* LV0_A */
	{ 3, 1 },		/* LV1_B */
	{ 3, 1 },		/* LV0 */
	{ 4, 1 },		/* LV1 */
	{ 4, 1 },		/* LV2 */
	{ 4, 1 },		/* LV3 */
	{ 5, 1 },		/* LV4 */
	{ 7, 1 },		/* LV5 */
};

static unsigned int exynos4415_int_top_div[][4] = {
/* ACLK_160, ACLK_200, ACLK_100, ACLK_400 */
	{ 4, 3, 7, 1},		/* LV0_A */
	{ 4, 3, 7, 1},		/* LV1_B */
	{ 4, 3, 7, 1},		/* LV0 */
	{ 5, 4, 7, 1},		/* LV1 */
	{ 5, 4, 7, 1},		/* LV2 */
	{ 5, 4, 7, 2},		/* LV3 */
	{ 5, 5, 7, 5},		/* LV4 */
	{ 7, 7, 7, 7},		/* LV5 */
};

static unsigned int exynos4415_int_acp0_div[][6] = {
/* ACLK_ACP, PCLK_ACP, ACP_DMC, ACP_DMCD, ACP_DMCP ACP_DMC_PRE  */
	{ 3, 1, 1, 1, 1, 0},	/* LV0_A */
	{ 3, 1, 1, 1, 1, 0},	/* LV1_B */
	{ 3, 1, 1, 1, 1, 0},	/* LV0 */
	{ 4, 1, 0, 4, 1, 0},	/* LV1 */
	{ 4, 1, 0, 4, 1, 0},	/* LV2 */
	{ 4, 1, 0, 4, 1, 0},	/* LV3 */
	{ 5, 1, 1, 2, 1, 0},	/* LV4 */
	{ 7, 1, 1, 3, 1, 0},	/* LV5 */
};

static unsigned int exynos4415_int_acp1_div[] = {
/* ACLK_G2D */
	3,			/* LV0_A */
	3,			/* LV1_B */
	3,			/* LV0 */
	4,			/* LV1 */
	4,			/* LV2 */
	4,			/* LV3 */
	5,			/* LV4 */
	7,			/* LV5 */
};

static unsigned int exynos4415_int_mfc_div[] = {
/* SCLK_MFC */
	3,			/* LV0_A */
	3,			/* LV1_B */
	3,			/* LV0 */
	3,			/* LV1 */
	3,			/* LV2 */
	4,			/* LV3 */
	5,			/* LV4 */
	7,			/* LV5 */
};

static unsigned int exynos4415_int_cam_div[][6] = {
/* SCLK_CSIS0, SCLK_CSIS1, SCLK_FIMC0, SCLK_FIMC1, SCLK_FIMC2, SCLK_FIMC3 */
	{ 0xf, 0xf, 4, 4, 4, 4},	/* LV0_A */
	{ 0xf, 0xf, 4, 4, 4, 4},	/* LV1_B */
	{ 0xf, 0xf, 4, 4, 4, 4},	/* LV0 */
	{ 0xf, 0xf, 5, 5, 5, 5},	/* LV1 */
	{ 0xf, 0xf, 5, 5, 5, 5},	/* LV2 */
	{ 0xf, 0xf, 5, 5, 5, 5},	/* LV3 */
	{ 0xf, 0xf, 5, 5, 5, 5},	/* LV4 */
	{ 0xf, 0xf, 7, 7, 7, 7},	/* LV5 */
};

static unsigned int exynos4415_int_cam1_div[] = {
/* SCLK_JPEG */
	3,
	3,
	3,
	4,
	4,
	4,
	5,
	7,
};

static unsigned int exynos4415_int_isp0_div[] = {
/* ACLK_ISP0 */
	0,
	0,
	0,
	0,
	2,
	2,
	2,
	2,
};

static unsigned int exynos4415_int_isp1_div[] = {
/* ACLK_ISP1 */
	2,
	2,
	2,
	4,
	7,
	7,
	7,
	7,
};

static unsigned int exynos4415_int_isp0_sub0_div[] = {
/* CSIS1_ISP0_A_300 */
	0,
	0,
	0,
	0,
	0,
	7,
	7,
	7,
};

static struct pm_qos_request exynos4_int_media_qos;

void exynos4_update_district_int_level(unsigned int idx)
{
	unsigned int freq;

	if (!pm_qos_request_active(&exynos4_int_media_qos))
		return;

	freq = int_bus_opp_list[idx].clk;

	pm_qos_update_request(&exynos4_int_media_qos, freq);
}

static void exynos4415_int_update_state(unsigned int target_freq)
{
	cputime64_t cur_time = get_jiffies_64();
	cputime64_t tmp_cputime;
	unsigned int target_idx = LV_0_A;
	unsigned int i;

	/*
	 * Find setting value with target frequency
	 */
	for (i = LV_0_A; i < LV_END; i++) {
		if (int_bus_opp_list[i].clk == target_freq)
			target_idx = int_bus_opp_list[i].idx;
	}

	tmp_cputime = cur_time - int_pre_time;

	int_bus_opp_list[target_idx].time_in_state =
		int_bus_opp_list[target_idx].time_in_state + tmp_cputime;

	int_pre_time = cur_time;
}

static unsigned int exynos4415_int_set_div(enum int_bus_idx target_idx)
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

	tmp = __raw_readl(target_int_clkdiv->acp1.target_reg);
	tmp &= ~target_int_clkdiv->acp1.reg_mask;
	tmp |= target_int_clkdiv->acp1.reg_value;
	__raw_writel(tmp, target_int_clkdiv->acp1.target_reg);

	tmp = __raw_readl(target_int_clkdiv->mfc.target_reg);
	tmp &= ~target_int_clkdiv->mfc.reg_mask;
	tmp |= target_int_clkdiv->mfc.reg_value;
	__raw_writel(tmp, target_int_clkdiv->mfc.target_reg);

	tmp = __raw_readl(target_int_clkdiv->cam.target_reg);
	tmp &= ~target_int_clkdiv->cam.reg_mask;
	tmp |= target_int_clkdiv->cam.reg_value;
	__raw_writel(tmp, target_int_clkdiv->cam.target_reg);

	tmp = __raw_readl(target_int_clkdiv->cam1.target_reg);
	tmp &= ~target_int_clkdiv->cam1.reg_mask;
	tmp |= target_int_clkdiv->cam1.reg_value;
	__raw_writel(tmp, target_int_clkdiv->cam1.target_reg);

	tmp = __raw_readl(target_int_clkdiv->isp0.target_reg);
	tmp &= ~target_int_clkdiv->isp0.reg_mask;
	tmp |= target_int_clkdiv->isp0.reg_value;
	__raw_writel(tmp, target_int_clkdiv->isp0.target_reg);

	tmp = __raw_readl(target_int_clkdiv->isp1.target_reg);
	tmp &= ~target_int_clkdiv->isp1.reg_mask;
	tmp |= target_int_clkdiv->isp1.reg_value;
	__raw_writel(tmp, target_int_clkdiv->isp1.target_reg);

	tmp = __raw_readl(target_int_clkdiv->isp0_sub0.target_reg);
	tmp &= ~target_int_clkdiv->isp0_sub0.reg_mask;
	tmp |= target_int_clkdiv->isp0_sub0.reg_value;
	__raw_writel(tmp, target_int_clkdiv->isp0_sub0.target_reg);

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
		tmp = __raw_readl(target_int_clkdiv->acp1.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : ACP1 DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->acp1.wait_mask);

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->mfc.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : MFC DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->mfc.wait_mask);

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->cam.wait_reg);
		timeout--;

		if (!timeout) {
			pr_err("%s : CAM DIV timeout\n", __func__);
			return -ETIME;
		}

	} while (tmp & target_int_clkdiv->cam.wait_mask);

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->cam1.wait_reg);
		timeout--;
		if (!timeout) {
			pr_err("%s : CAM1 DIV timeout\n", __func__);
			return -ETIME;
		}
	} while (tmp & target_int_clkdiv->cam1.wait_mask);

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->isp0.wait_reg);
		timeout--;
		if (!timeout) {
			pr_err("%s : ISP1 DIV timeout\n", __func__);
			return -ETIME;
		}
	} while (tmp & target_int_clkdiv->isp0.wait_mask);

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->isp1.wait_reg);
		timeout--;
		if (!timeout) {
			pr_err("%s : ISP1 DIV timeout\n", __func__);
			return -ETIME;
		}
	} while (tmp & target_int_clkdiv->isp1.wait_mask);

	timeout = INT_TIMEOUT_VAL;
	do {
		tmp = __raw_readl(target_int_clkdiv->isp0_sub0.wait_reg);
		timeout--;
		if (!timeout) {
			pr_err("%s : ISP0_SUB0 DIV timeout\n", __func__);
			return -ETIME;
		}
	} while (tmp & target_int_clkdiv->isp0_sub0.wait_mask);

	return 0;
}

static enum int_bus_idx exynos4415_find_int_bus_idx(unsigned long target_freq)
{
	unsigned int i;

	for (i = 0; i < LV_END; i++) {
		if (int_bus_opp_list[i].clk == target_freq)
			return i;
	}

	return LV_END;
}

static int exynos4415_int_busfreq_set_volt(struct busfreq_data_int *data,
						unsigned long volt,
						unsigned long volt_range)
{
	if (data->old_voltage == volt)
		goto out;
	regulator_set_voltage(data->vdd_int, volt, volt_range);
	data->old_voltage = volt;
out:
	return 0;
}

void exynos4415_int_busfreq_voltage_constraint(unsigned long frequency)
{
	struct opp *opp;

	mutex_lock(&data_int->lock);

	rcu_read_lock();
	opp = devfreq_recommended_opp(data_int->dev, &frequency, 0);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(data_int->dev, "%s: Invalid OPP.\n", __func__);
		return;
	}
	data_int->constraint_voltage = opp_get_voltage(opp) + cold_margin;
	rcu_read_unlock();

	if (data_int->target_voltage < data_int->constraint_voltage) {
		exynos4415_int_busfreq_set_volt(data_int, data_int->constraint_voltage,
								 SAFE_INT_VOLT(data_int->constraint_voltage));
	} else {
		exynos4415_int_busfreq_set_volt(data_int, data_int->target_voltage,
								 SAFE_INT_VOLT(data_int->target_voltage));
	}

	mutex_unlock(&data_int->lock);
}

static int exynos4415_int_busfreq_target(struct device *dev,
				      unsigned long *_freq, u32 flags)
{
	int err = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct busfreq_data_int *data = platform_get_drvdata(pdev);
	struct opp *opp;
	unsigned long freq;
	unsigned long old_freq;
	unsigned long target_volt;
	enum int_bus_idx target_index;

	/* get available opp information */
	rcu_read_lock();
	opp = devfreq_recommended_opp(dev, _freq, flags);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "%s: Invalid OPP.\n", __func__);
		return PTR_ERR(opp);
	}

	freq = opp_get_freq(opp);
	target_volt = opp_get_voltage(opp) + cold_margin;
	rcu_read_unlock();

	/* get olg opp information */
	rcu_read_lock();
	old_freq = opp_get_freq(data->curr_opp);
	rcu_read_unlock();

	exynos4415_int_update_state(old_freq);

	if (old_freq == freq)
		return 0;

	sec_debug_aux_log(SEC_DEBUG_AUXLOG_CPU_CLOCK_SWITCH_CHANGE,
		"[INT] %7d <= %7d",
		freq, old_freq);

	target_index = exynos4415_find_int_bus_idx(freq);
	mutex_lock(&data->lock);

	data->target_voltage = target_volt;
	if (target_volt < data->constraint_voltage)
		target_volt = data->constraint_voltage;

	if (old_freq < freq) {
		exynos4415_int_busfreq_set_volt(data, target_volt,
								 SAFE_INT_VOLT(target_volt));
		exynos_set_abb(ID_INT, exynos4415_int_asv_abb[target_index]);
		err = exynos4415_int_set_div(target_index);
	} else {
		err = exynos4415_int_set_div(target_index);
		exynos_set_abb(ID_INT, exynos4415_int_asv_abb[target_index]);
		exynos4415_int_busfreq_set_volt(data, target_volt,
								 SAFE_INT_VOLT(target_volt));
	}

	data->curr_opp = opp;

	mutex_unlock(&data->lock);

	return err;
}

static int exynos4415_int_bus_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	struct busfreq_data_int *data = dev_get_drvdata(dev);
	unsigned long busy_data;
	unsigned int int_ccnt = 0;
	unsigned long int_pmcnt = 0;

	rcu_read_lock();
	stat->current_frequency = opp_get_freq(data->curr_opp);
	rcu_read_unlock();

	busy_data = exynos4270_ppmu_get_busy(data->ppmu, PPMU_SET_INT,
					&int_ccnt, &int_pmcnt);

	stat->total_time = int_ccnt;
	stat->busy_time = int_pmcnt;

	return 0;
}

#if defined(CONFIG_DEVFREQ_GOV_PM_QOS)
static struct devfreq_pm_qos_data exynos4415_devfreq_int_pm_qos_data = {
	.bytes_per_sec_per_hz = 8,
	.pm_qos_class = PM_QOS_DEVICE_THROUGHPUT,
};
#endif

#if defined(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)
static struct devfreq_simple_ondemand_data exynos4415_int_governor_data = {
	.pm_qos_class	= PM_QOS_DEVICE_THROUGHPUT,
	.upthreshold	= 100,
	.cal_qos_max	= 267000,
};
#endif

static struct devfreq_dev_profile exynos4415_int_devfreq_profile = {
	.initial_freq	= 267000,
	.polling_ms	= 100,
	.target		= exynos4415_int_busfreq_target,
	.get_dev_status	= exynos4415_int_bus_get_dev_status,
};

static int exynos4415_init_int_table(struct busfreq_data_int *data)
{
	unsigned int i;
	unsigned int ret;
	struct int_clkdiv_info *tmp_int_table;

	/* make list for setting value for int DVS */
	for (i = LV_0_A; i < LV_END; i++) {
		tmp_int_table = kzalloc(sizeof(struct int_clkdiv_info), GFP_KERNEL);

		tmp_int_table->lv_idx = i;

		/* Setting for LEFTBUS */
		tmp_int_table->lbus.target_reg = EXYNOS4415_CLKDIV_LEFTBUS;
		tmp_int_table->lbus.reg_value = ((exynos4415_int_lbus_div[i][0] << EXYNOS4_CLKDIV_BUS_GDLR_SHIFT) |\
						 (exynos4415_int_lbus_div[i][1] << EXYNOS4_CLKDIV_BUS_GPLR_SHIFT));
		tmp_int_table->lbus.reg_mask = (EXYNOS4_CLKDIV_BUS_GPLR_MASK|\
						EXYNOS4_CLKDIV_BUS_GDLR_MASK);
		tmp_int_table->lbus.wait_reg =	EXYNOS4415_CLKDIV_STAT_LEFTBUS;
		tmp_int_table->lbus.wait_mask = (EXYNOS4_CLKDIV_STAT_BUS_GPLR_MASK |\
						 EXYNOS4_CLKDIV_STAT_BUS_GDLR_MASK);

		/* Setting for RIGHTBUS */
		tmp_int_table->rbus.target_reg = EXYNOS4415_CLKDIV_RIGHTBUS;
		tmp_int_table->rbus.reg_value = ((exynos4415_int_rbus_div[i][0] << EXYNOS4_CLKDIV_BUS_GDLR_SHIFT) |\
						 (exynos4415_int_rbus_div[i][1] << EXYNOS4_CLKDIV_BUS_GPLR_SHIFT));
		tmp_int_table->rbus.reg_mask = (EXYNOS4_CLKDIV_BUS_GPLR_MASK|\
						EXYNOS4_CLKDIV_BUS_GDLR_MASK);
		tmp_int_table->rbus.wait_reg = EXYNOS4415_CLKDIV_STAT_RIGHTBUS;
		tmp_int_table->rbus.wait_mask = (EXYNOS4_CLKDIV_STAT_BUS_GPLR_MASK |\
						 EXYNOS4_CLKDIV_STAT_BUS_GDLR_MASK);

		/* Setting for TOP */
		tmp_int_table->top.target_reg = EXYNOS4415_CLKDIV_TOP;
		tmp_int_table->top.reg_value = ((exynos4415_int_top_div[i][0] << EXYNOS4_CLKDIV_TOP_ACLK160_SHIFT) |\
						 (exynos4415_int_top_div[i][1] << EXYNOS4415_CLKDIV_TOP_ACLK200_SHIFT) |\
						 (exynos4415_int_top_div[i][2] << EXYNOS4_CLKDIV_TOP_ACLK100_SHIFT) |\
						 (exynos4415_int_top_div[i][3] << EXYNOS4_CLKDIV_TOP_ACLK400_MCUISP_SHIFT));
		tmp_int_table->top.reg_mask = (EXYNOS4_CLKDIV_TOP_ACLK160_MASK |\
						EXYNOS4415_CLKDIV_TOP_ACLK200_MASK |\
						EXYNOS4_CLKDIV_TOP_ACLK100_MASK |\
						EXYNOS4_CLKDIV_TOP_ACLK400_MCUISP_MASK);
		tmp_int_table->top.wait_reg = EXYNOS4415_CLKDIV_STAT_TOP;
		tmp_int_table->top.wait_mask = (EXYNOS4_CLKDIV_STAT_TOP_ACLK160_MASK |\
						 EXYNOS4415_CLKDIV_STAT_TOP_ACLK200_MASK |\
						 EXYNOS4_CLKDIV_STAT_TOP_ACLK100_MASK |\
						 EXYNOS4_CLKDIV_STAT_TOP_ACLK400_MCUISP_MASK);

		/* Setting for ACP0 */
		tmp_int_table->acp0.target_reg = EXYNOS4415_CLKDIV_ACP0;
		tmp_int_table->acp0.reg_value = ((exynos4415_int_acp0_div[i][0] << EXYNOS4415_CLKDIV_ACP_SHIFT) |\
						 (exynos4415_int_acp0_div[i][1] << EXYNOS4415_CLKDIV_ACP_PCLK_SHIFT) |\
						 (exynos4415_int_acp0_div[i][2] << EXYNOS4415_CLKDIV_ACP_DMC_SHIFT) |\
						 (exynos4415_int_acp0_div[i][3] << EXYNOS4415_CLKDIV_ACP_DMCD_SHIFT) |\
						 (exynos4415_int_acp0_div[i][4] << EXYNOS4415_CLKDIV_ACP_DMCP_SHIFT));
		tmp_int_table->acp0.reg_mask = (EXYNOS4415_CLKDIV_ACP_MASK |\
						EXYNOS4415_CLKDIV_ACP_PCLK_MASK |\
						EXYNOS4415_CLKDIV_ACP_DMC_MASK |\
						EXYNOS4415_CLKDIV_ACP_DMCD_MASK |\
						EXYNOS4415_CLKDIV_ACP_DMCP_MASK);
		tmp_int_table->acp0.wait_reg = EXYNOS4415_CLKDIV_STAT_ACP0;
		tmp_int_table->acp0.wait_mask = (EXYNOS4415_CLKDIV_STAT_ACP_MASK |\
						 EXYNOS4415_CLKDIV_STAT_ACP_PCLK_MASK |\
						 EXYNOS4415_CLKDIV_STAT_ACP_DMC_MASK |\
						 EXYNOS4415_CLKDIV_STAT_ACP_DMCD_MASK |\
						 EXYNOS4415_CLKDIV_STAT_ACP_DMCP_MASK);
		/* Setting for ACP1 */
		tmp_int_table->acp1.target_reg = EXYNOS4415_CLKDIV_ACP1;
		tmp_int_table->acp1.reg_value = (exynos4415_int_acp1_div[i] << EXYNOS4415_CLKDIV_ACP_G2D_SHIFT);
		tmp_int_table->acp1.reg_mask = EXYNOS4415_CLKDIV_ACP_G2D_MASK;
		tmp_int_table->acp1.wait_reg = EXYNOS4415_CLKDIV_STAT_ACP1;
		tmp_int_table->acp1.wait_mask = EXYNOS4415_CLKDIV_STAT_ACP_G2D_MASK;

		/* Setting for MFC */
		tmp_int_table->mfc.target_reg = EXYNOS4415_CLKDIV_MFC;
		tmp_int_table->mfc.reg_value = (exynos4415_int_mfc_div[i] << EXYNOS4415_CLKDIV_MFC_SHIFT);
		tmp_int_table->mfc.reg_mask = EXYNOS4415_CLKDIV_MFC_MASK;
		tmp_int_table->mfc.wait_reg = EXYNOS4415_CLKDIV_STAT_MFC;
		tmp_int_table->mfc.wait_mask = EXYNOS4415_CLKDIV_STAT_MFC_MASK;

		/* Setting for CAM */
		tmp_int_table->cam.target_reg = EXYNOS4415_CLKDIV_CAM;
		tmp_int_table->cam.reg_value = ((exynos4415_int_cam_div[i][0] << EXYNOS4415_CLKDIV_CSIS0_SHIFT) |\
						(exynos4415_int_cam_div[i][1] << EXYNOS4415_CLKDIV_CSIS1_SHIFT) |\
						(exynos4415_int_cam_div[i][2] << EXYNOS4_CLKDIV_CAM_FIMC0_SHIFT) |\
						(exynos4415_int_cam_div[i][3] << EXYNOS4_CLKDIV_CAM_FIMC1_SHIFT) |\
						(exynos4415_int_cam_div[i][4] << EXYNOS4_CLKDIV_CAM_FIMC2_SHIFT) |\
						(exynos4415_int_cam_div[i][5] << EXYNOS4_CLKDIV_CAM_FIMC3_SHIFT));
		tmp_int_table->cam.reg_mask = (EXYNOS4415_CLKDIV_CSIS0_MASK |\
					       EXYNOS4415_CLKDIV_CSIS1_MASK |\
					       EXYNOS4_CLKDIV_CAM_FIMC0_MASK |\
					       EXYNOS4_CLKDIV_CAM_FIMC1_MASK |\
					       EXYNOS4_CLKDIV_CAM_FIMC2_MASK |\
					       EXYNOS4_CLKDIV_CAM_FIMC3_MASK);
		tmp_int_table->cam.wait_reg = EXYNOS4415_CLKDIV_STAT_CAM;
		tmp_int_table->cam.wait_mask = (EXYNOS4415_CLKDIV_STAT_CSIS0_MASK |\
						EXYNOS4415_CLKDIV_STAT_CSIS1_MASK |\
						EXYNOS4415_CLKDIV_STAT_FIMC3_MASK |\
						EXYNOS4415_CLKDIV_STAT_FIMC2_MASK |\
						EXYNOS4415_CLKDIV_STAT_FIMC1_MASK |\
						EXYNOS4415_CLKDIV_STAT_FIMC0_MASK);

		/* Setting for JPEG */
		tmp_int_table->cam1.target_reg = EXYNOS4415_CLKDIV_CAM1;
		tmp_int_table->cam1.reg_value = (exynos4415_int_cam1_div[i] << EXYNOS4_CLKDIV_CAM1_JPEG_SHIFT);
		tmp_int_table->cam1.reg_mask = EXYNOS4_CLKDIV_CAM1_JPEG_MASK;
		tmp_int_table->cam1.wait_reg = EXYNOS4415_CLKDIV_STAT_CAM1;
		tmp_int_table->cam1.wait_mask = EXYNOS4_CLKDIV_STAT_CAM1_JPEG_MASK;

		/* Setting for ISP0 */
		tmp_int_table->isp0.target_reg = EXYNOS4415_CLKDIV_ISP0;
		tmp_int_table->isp0.reg_value = (exynos4415_int_isp0_div[i] << EXYNOS4415_CLKDIV_TOP_ISP0_300_SHIFT);
		tmp_int_table->isp0.reg_mask = EXYNOS4415_CLKDIV_TOP_ISP0_300_MASK;
		tmp_int_table->isp0.wait_reg = EXYNOS4415_CLKDIV_STAT_ISP0;
		tmp_int_table->isp0.wait_mask = EXYNOS4415_CLKDIV_STAT_ISP0_300_MASK;

		/* Setting for ISP1 */
		tmp_int_table->isp1.target_reg = EXYNOS4415_CLKDIV_ISP1;
		tmp_int_table->isp1.reg_value = (exynos4415_int_isp1_div[i] << EXYNOS4415_CLKDIV_TOP_ISP1_300_SHIFT);
		tmp_int_table->isp1.reg_mask = EXYNOS4415_CLKDIV_TOP_ISP1_300_MASK;
		tmp_int_table->isp1.wait_reg = EXYNOS4415_CLKDIV_STAT_ISP1;
		tmp_int_table->isp1.wait_mask = EXYNOS4415_CLKDIV_STAT_ISP1_300_MASK;

		/* Setting for ISP0_SUB0 */
		tmp_int_table->isp0_sub0.target_reg = EXYNOS4415_CLKDIV_CMU_ISP0_SUB0;
		tmp_int_table->isp0_sub0.reg_value = (exynos4415_int_isp0_sub0_div[i] << EXYNOS4415_CLKDIV_CMU_ISP0_SUB0_CSIS1_SHIFT);
		tmp_int_table->isp0_sub0.reg_mask = EXYNOS4415_CLKDIV_CMU_ISP0_SUB0_CSIS1_MASK;
		tmp_int_table->isp0_sub0.wait_reg = EXYNOS4415_CLKDIV_CMU_STAT_ISP0_SUB0;
		tmp_int_table->isp0_sub0.wait_mask = EXYNOS4415_CLKDIV_CMU_STAT_ISP0_SUB0_CSIS1_MASK;

		list_add(&tmp_int_table->list, &int_dvfs_list);
	}

	/* will add code for ASV information setting function in here */

	for (i = 0; i < ARRAY_SIZE(int_bus_opp_list); i++) {
		int_bus_opp_list[i].volt = get_match_volt(ID_INT, int_bus_opp_list[i].clk);
		if (int_bus_opp_list[i].volt == 0) {
			dev_err(data->dev, "Invalid value\n");
			return -EINVAL;
		}

		exynos4415_int_asv_abb[i] = get_match_abb(ID_INT, int_bus_opp_list[i].clk);

		pr_info("INT %luKhz ASV is %luuV ABB is %d\n", int_bus_opp_list[i].clk,
							int_bus_opp_list[i].volt,
							exynos4415_int_asv_abb[i]);

		ret = opp_add(data->dev, int_bus_opp_list[i].clk, int_bus_opp_list[i].volt);

		if (ret) {
			dev_err(data->dev, "Fail to add opp entries.\n");
			return ret;
		}
	}

	return 0;
}

static ssize_t show_freq_table(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i, count = 0;
	struct opp *opp;

	if (!unlikely(int_dev)) {
		pr_err("%s: device is not probed\n", __func__);
		return -ENODEV;
	}

	rcu_read_lock();
	for (i = 0; i < ARRAY_SIZE(int_bus_opp_list); i++) {
		opp = opp_find_freq_exact(int_dev, int_bus_opp_list[i].clk, true);
		if (!IS_ERR_OR_NULL(opp))
			count += sprintf(&buf[count], "%lu ", opp_get_freq(opp));
	}
	rcu_read_unlock();

	count += sprintf(&buf[count], "\n");
	return count;
}

static DEVICE_ATTR(freq_table, S_IRUGO, show_freq_table, NULL);

static ssize_t int_show_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t len = 0;
	ssize_t write_cnt = (ssize_t)((PAGE_SIZE / LV_END) - 2);

	for (i = LV_0_A; i < LV_END; i++)
		len += snprintf(buf + len, write_cnt, "%ld %llu\n", int_bus_opp_list[i].clk,
				(unsigned long long)int_bus_opp_list[i].time_in_state);

	return len;
}

static DEVICE_ATTR(int_time_in_state, 0644, int_show_state, NULL);
static struct attribute *busfreq_int_entries[] = {
	&dev_attr_int_time_in_state.attr,
	NULL,
};
static struct attribute_group busfreq_int_attr_group = {
	.name	= "time_in_state",
	.attrs	= busfreq_int_entries,
};

static struct exynos_devfreq_platdata default_qos_int_pd = {
	.default_qos = 100000,
};

static int fb_state_int(struct notifier_block *nb,
			unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;

	if (val != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;
	switch (blank) {
	case FB_BLANK_POWERDOWN:
		if (pm_qos_request_active(&exynos4415_int_fb_qos))
			pm_qos_update_request_timeout(&exynos4415_int_fb_qos,
						200000, 2000 * 1000);
		break;
	case FB_BLANK_UNBLANK:
		pm_qos_update_request_timeout(&exynos4415_int_fb_qos,
			exynos4415_int_devfreq_profile.initial_freq, 2000 * 1000);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block fb_block_int = {
	.notifier_call = fb_state_int,
};

static int exynos4415_int_reboot_notifier_call(struct notifier_block *this,
				unsigned long code, void *_cmd)
{
	pm_qos_update_request(&exynos4415_int_qos, int_bus_opp_list[LV_0].clk);

	return NOTIFY_DONE;
}

static struct notifier_block exynos4415_int_reboot_notifier = {
	.notifier_call = exynos4415_int_reboot_notifier_call,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos4415_bus_int_tmu_notifier(struct notifier_block *notifier,
						unsigned long event, void *v)
{
	int *on = v;

	switch (*on) {
	case TMU_COLD:
		cold_margin = COLD_OFFSET;
		break;
	case TMU_NORMAL:
	default:
		cold_margin = 0;
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos4415_bus_int_tmu_nb = {
	.notifier_call = exynos4415_bus_int_tmu_notifier,
};
#endif

static __devinit int exynos4415_busfreq_int_probe(struct platform_device *pdev)
{
	struct busfreq_data_int *data;
	struct opp *opp;
	struct device *dev = &pdev->dev;
	struct exynos_devfreq_platdata *pdata;
	unsigned long maxfreq = ULONG_MAX;
	unsigned long maxvolt;
	int err = 0;

	data = kzalloc(sizeof(struct busfreq_data_int), GFP_KERNEL);

	if (data == NULL) {
		dev_err(dev, "Cannot allocate memory for INT.\n");
		return -ENOMEM;
	}
	fb_register_client(&fb_block_int);
	data->dev = dev;
	data_int = data;
	mutex_init(&data->lock);

	/* Setting table for int */
	exynos4415_init_int_table(data);

	rcu_read_lock();
	opp = opp_find_freq_floor(dev, &exynos4415_int_devfreq_profile.initial_freq);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "Invalid initial frequency %lu kHz.\n",
			       exynos4415_int_devfreq_profile.initial_freq);
		err = PTR_ERR(opp);
		goto err_opp_add;
	}

	data->curr_opp = opp;

	/* Get max freq information for devfreq */
	opp = opp_find_freq_floor(dev, &maxfreq);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "%s: Invalid OPP.\n", __func__);
		err = PTR_ERR(opp);

		goto err_opp_add;
	}
	maxfreq = opp_get_freq(opp);
	maxvolt = opp_get_voltage(opp);
	rcu_read_unlock();

	int_pre_time = get_jiffies_64();

	platform_set_drvdata(pdev, data);

	data->vdd_int = regulator_get(dev, "vdd_int");
	if (IS_ERR(data->vdd_int)) {
		dev_err(dev, "Cannot get the regulator \"vdd_int\"\n");
		err = PTR_ERR(data->vdd_int);
		goto err_regulator;
	}

#ifdef CONFIG_EXYNOS_THERMAL
	cold_margin = COLD_OFFSET;
#endif
	regulator_set_voltage(data->vdd_int, maxvolt, SAFE_INT_VOLT(maxvolt + cold_margin));

	/* Init PPMU for INT devfreq */
	cold_margin = COLD_OFFSET;
	data->ppmu = exynos4270_ppmu_get(PPMU_SET_INT);
	if (!data->ppmu)
		goto err_ppmu_get;

#if defined(CONFIG_DEVFREQ_GOV_PM_QOS)
	data->devfreq = devfreq_add_device(dev, &exynos4415_int_devfreq_profile,
			&devfreq_pm_qos, &exynos4415_devfreq_int_pm_qos_data);
#endif
#if defined(CONFIG_DEVFREQ_GOV_USERSPACE)
	data->devfreq = devfreq_add_device(dev, &exynos4415_int_devfreq_profile,
			&devfreq_userspace, NULL);
#endif
#if defined(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)
	data->devfreq = devfreq_add_device(dev, &exynos4415_int_devfreq_profile,
			&devfreq_simple_ondemand, &exynos4415_int_governor_data);
#endif
	if (IS_ERR(data->devfreq)) {
		err = PTR_ERR(data->devfreq);
		goto err_devfreq_add;
	}

	devfreq_register_opp_notifier(dev, data->devfreq);

	int_dev = data->dev;

	/* Create file for time_in_state */
	err = sysfs_create_group(&data->devfreq->dev.kobj, &busfreq_int_attr_group);

	/* Add sysfs for freq_table */
	err = device_create_file(&data->devfreq->dev, &dev_attr_freq_table);
	if (err)
		pr_err("%s: Fail to create sysfs file\n", __func__);

	pdata = pdev->dev.platform_data;
	if (!pdata)
		pdata = &default_qos_int_pd;

	/* Register Notify */
	pm_qos_add_request(&exynos4415_int_qos, PM_QOS_DEVICE_THROUGHPUT, pdata->default_qos);
	pm_qos_add_request(&exynos4415_int_fb_qos, PM_QOS_DEVICE_THROUGHPUT, pdata->default_qos);
	pm_qos_add_request(&exynos4415_int_boot_qos, PM_QOS_DEVICE_THROUGHPUT, pdata->default_qos);
	pm_qos_update_request_timeout(&exynos4415_int_boot_qos,
		exynos4415_int_devfreq_profile.initial_freq, 25000 * 1000);
	pm_qos_add_request(&exynos4_int_media_qos, PM_QOS_DEVICE_THROUGHPUT, pdata->default_qos);

	register_reboot_notifier(&exynos4415_int_reboot_notifier);
#ifdef CONFIG_EXYNOS_THERMAL
	/* For low temperature compensation when boot time */
	exynos_tmu_add_notifier(&exynos4415_bus_int_tmu_nb);
	int_is_probed = true;
#endif

	return 0;

err_devfreq_add:
	devfreq_remove_device(data->devfreq);
err_ppmu_get:
	regulator_put(data->vdd_int);
err_regulator:
	platform_set_drvdata(pdev, NULL);
err_opp_add:
	kfree(data);

	return err;
}

static __devexit int exynos4415_busfreq_int_remove(struct platform_device *pdev)
{
	struct busfreq_data_int *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);
	if (data->vdd_int)
		regulator_put(data->vdd_int);
	kfree(data);

	return 0;
}

static int exynos4415_busfreq_int_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct busfreq_data_int *data = platform_get_drvdata(pdev);

	if (pm_qos_request_active(&exynos4415_int_boot_qos)) {
		pm_qos_update_request(&exynos4415_int_boot_qos,
			exynos4415_int_devfreq_profile.initial_freq);
	}

	regulator_set_voltage(data->vdd_int, int_bus_opp_list[0].volt + COLD_OFFSET,
				SAFE_INT_VOLT(int_bus_opp_list[0].volt + COLD_OFFSET));
	return 0;
}

static int exynos4415_busfreq_int_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct busfreq_data_int *data = platform_get_drvdata(pdev);
	regulator_set_voltage(data->vdd_int, int_bus_opp_list[LV_0].volt,
				SAFE_INT_VOLT(int_bus_opp_list[LV_0].volt));

	if (pm_qos_request_active(&exynos4415_int_boot_qos)) {
		pm_qos_update_request_timeout(&exynos4415_int_boot_qos,
			exynos4415_int_devfreq_profile.initial_freq, 2000 * 1000);
	}
	return 0;
}

static const struct dev_pm_ops exynos4415_busfreq_int_pm = {
	.suspend	= exynos4415_busfreq_int_suspend,
	.resume		= exynos4415_busfreq_int_resume,
};

static struct platform_driver exynos4415_busfreq_int_driver = {
	.probe	= exynos4415_busfreq_int_probe,
	.remove	= __devexit_p(exynos4415_busfreq_int_remove),
	.driver = {
		.name	= "exynos4415-busfreq-int",
		.owner	= THIS_MODULE,
		.pm	= &exynos4415_busfreq_int_pm,
	},
};

static int __init exynos4415_busfreq_int_init(void)
{
	return platform_driver_register(&exynos4415_busfreq_int_driver);
}
device_initcall(exynos4415_busfreq_int_init);

static void __exit exynos4415_busfreq_int_exit(void)
{
	platform_driver_unregister(&exynos4415_busfreq_int_driver);
}
module_exit(exynos4415_busfreq_int_exit);

