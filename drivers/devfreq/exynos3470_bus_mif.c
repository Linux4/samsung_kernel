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

#include <mach/regs-clock.h>
#include <mach/devfreq.h>
#include <mach/smc.h>
#include <mach/asv-exynos.h>

#include "exynos3470_ppmu.h"

#define LPDDR3_1500		3
#define SET_DREX_TIMING

#define SAFE_MIF_VOLT(x)	(x + 25000)

#undef DVFS_BY_PLL_CHANGE

#ifdef DVFS_BY_PLL_CHANGE
#define exynos4270_mif_set_freq exynos4270_mif_set_pll
#else
#define exynos4270_mif_set_freq exynos4270_mif_set_div
#endif

static void (*exynos4270_set_drex)(unsigned long target_freq);

extern unsigned int pop_type;

static struct device *mif_dev;
static unsigned long suspend_freq = 200000;

static struct pm_qos_request exynos4270_mif_qos;
cputime64_t mif_pre_time;

static LIST_HEAD(mif_dvfs_list);

/* NoC list for MIF block */
static LIST_HEAD(mif_noc_list);

struct busfreq_data_mif {
	struct device *dev;
	struct devfreq *devfreq;
	struct opp *curr_opp;
	struct regulator *vdd_mif;
	struct exynos4270_ppmu_handle *ppmu;
	struct mutex lock;
};

enum mif_bus_idx {
	LV_0 = 0,
	LV_1,
	LV_2,
	LV_3,
	LV_END,
};

struct mif_bus_opp_table {
	unsigned int idx;
	unsigned long clk;
	unsigned long volt;
	cputime64_t time_in_state;
};

struct mif_bus_opp_table mif_bus_opp_list[] = {
	{LV_0, 400000, 1000000, 0},
	{LV_1, 267000,  900000, 0},
	{LV_2, 200000,  900000, 0},
	{LV_3, 100000,  900000, 0},
};

unsigned int exynos4270_mif_asv_abb[LV_END] = {0, };

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
};

static unsigned int exynos4270_dmc1_div_table[][3] = {
	/* SCLK_DMC, ACLK_DMCD, ACLK_DMCP */
	{ 1, 1, 1},	/* 400MHz, 200MHz, 100MHz */
	{ 2, 1, 1},	/* 266MHz, 133MHz,  67MHz */
	{ 3, 1, 1},	/* 200MHz, 100MHz,  50MHz */
	{ 7, 1, 1},	/* 100MHz,  50MHz,  25MHz */
};

#ifdef SET_DREX_TIMING
static unsigned int *exynos4270_dram_parameter;
static unsigned int exynos4270_dram_parameter_rev[] = {
	0x34498691,	/* 400Mhz */
	0x2336544C,	/* 266Mhz */
	0x1A254349,	/* 200Mhz */
	0x0D1321C5,	/* 100Mhz */
};

static unsigned int exynos4270_dram_parameter_rev2[] = {
	0x545A96D3,	/* 400Mhz */
	0x3847748D,	/* 266Mhz */
	0x2A35538A,	/* 200Mhz */
};

static unsigned int exynos4270_dram_data_parameter[] = {
	0x46450306,	/* 400Mhz */
	0x46450306,	/* 266Mhz */
	0x46450306,	/* 200Mhz */
};

static unsigned int exynos4270_dram_power_parameter[] = {
	0x5058033A,	/* 400Mhz */
	0x383B033A,	/* 266Mhz */
	0x282C033A,	/* 200Mhz */
};
#endif

/* Index indicate TOTAL_LAYER_COUNT */
static int mif_bts_media_lock_level[] = {
	LV_3, LV_2, LV_1, LV_0, LV_0, LV_0,
};

static int bts_fimd_layer_count = 0;
static int bts_mixer_layer_count = 0;
static int bts_fimc_enable = false;
static struct pm_qos_request bts_mif_qos;
static DEFINE_MUTEX(bts_media_lock);

extern void exynos5_update_media_int(unsigned int total_layer_count, bool fimc_enable);

void exynos5_update_media_layers(enum devfreq_media_type media_type, unsigned int value)
{
	unsigned int mif_qos = LV_3;
	unsigned int total_layer_count = 0;

	if (samsung_rev() < EXYNOS3470_REV_2_0)
		return;

	mutex_lock(&bts_media_lock);

	switch (media_type) {
		case TYPE_FIMD1:
			bts_fimd_layer_count = value;
			break;
		case TYPE_MIXER:
			bts_mixer_layer_count = value;
			break;
		case TYPE_FIMC_LITE:
			bts_fimc_enable = value;
			break;
		default:
			mutex_unlock(&bts_media_lock);
			return;
	}

	total_layer_count = bts_fimd_layer_count + bts_mixer_layer_count;

	if (total_layer_count > 5) {
		pr_warn("DEVFREQ(MIF) : bts media layer locking doesn't support layer 6\n");
		total_layer_count = 5;
	}

	if (bts_fimc_enable) {
		mif_qos = LV_0;
	} else {
		mif_qos = mif_bts_media_lock_level[total_layer_count];
	}

	pm_qos_update_request(&bts_mif_qos, mif_bus_opp_list[mif_qos].clk);
	exynos5_update_media_int(total_layer_count, bts_fimc_enable);

	mutex_unlock(&bts_media_lock);
}

static void exynos4270_mif_update_state(unsigned int target_freq)
{
	cputime64_t cur_time = get_jiffies_64();
	cputime64_t tmp_cputime;
	unsigned int target_idx = LV_0;
	unsigned int i;

	/*
	 * Find setting value with target frequency
	 */
	for (i = LV_0; i < LV_END; i++) {
		if (mif_bus_opp_list[i].clk == target_freq)
			target_idx = mif_bus_opp_list[i].idx;
	}

	tmp_cputime = cur_time - mif_pre_time;

	mif_bus_opp_list[target_idx].time_in_state =
		mif_bus_opp_list[target_idx].time_in_state + tmp_cputime;

	mif_pre_time = cur_time;
}

static unsigned int exynos4270_mif_set_div(enum mif_bus_idx target_idx)
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

static enum mif_bus_idx exynos4270_find_mif_bus_idx(unsigned long target_freq)
{
	unsigned int i;

	for (i = 0; i < LV_END; i++) {
		if (mif_bus_opp_list[i].clk == target_freq)
			return i;
	}

	return LV_END;
}

#ifdef SET_DREX_TIMING
#define DREX_TIMINGROW		0x34
#define DREX_TIMINGDATA		0x38
#define DREX_TIMINGPOWER	0x3c

#ifndef CONFIG_ARM_TRUSTZONE
static void __iomem *exynos4270_base_drex1;
static void __iomem *exynos4270_base_drex0;
#endif

static void exynos4270_set_drex_rev(unsigned long target_freq)
{
	unsigned target_idx = exynos4270_find_mif_bus_idx(target_freq);
	unsigned int utiming_row_cur, utiming_row_cur_target;

#ifdef CONFIG_ARM_TRUSTZONE
	/* Set DREX PARAMETER_ROW */
	exynos_smc_readsfr(EXYNOS4_PA_DMC0_4X12 + DREX_TIMINGROW, &utiming_row_cur);
	utiming_row_cur_target = (utiming_row_cur | exynos4270_dram_parameter[target_idx]);

	/*
	 * Writing (old value | target value) in TIMINGROW register for META Stability
	 */
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC0_4X12 + DREX_TIMINGROW),
			utiming_row_cur_target, 0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC0_4X12 + DREX_TIMINGROW),
			exynos4270_dram_parameter[target_idx], 0);

	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC1_4X12 + DREX_TIMINGROW),
			utiming_row_cur_target, 0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC1_4X12 + DREX_TIMINGROW),
			exynos4270_dram_parameter[target_idx], 0);
#else
	/* Set DREX PARAMETER_ROW */
	utiming_row_cur = __raw_readl(xynos4270_base_drex0 + DREX_TIMINGROW);
	utiming_row_cur_target = (utiming_row_cur | exynos4270_dram_parameter[target_idx]);

	/*
	 * Writing (old value | target value) in TIMINGROW register for META Stability
	 */
	__raw_writel(utiming_row_cur_target, exynos4270_base_drex0 + DREX_TIMINGROW);
	__raw_writel(exynos4270_dram_parameter[target_idx], exynos4270_base_drex0 + DREX_TIMINGROW);

	__raw_writel(utiming_row_cur_target, exynos4270_base_drex1 + DREX_TIMINGROW);
	__raw_writel(exynos4270_dram_parameter[target_idx], exynos4270_base_drex1 + DREX_TIMINGROW);
#endif
}

static void exynos4270_set_drex_rev2(unsigned long target_freq)
{
	unsigned target_idx = exynos4270_find_mif_bus_idx(target_freq);
	unsigned int utiming_row_cur, utiming_row_cur_target;

#ifdef CONFIG_ARM_TRUSTZONE
	/* Set DREX PARAMETER_ROW */
	exynos_smc_readsfr(EXYNOS4_PA_DMC0_4X12 + DREX_TIMINGROW, &utiming_row_cur);
	utiming_row_cur_target = (utiming_row_cur | exynos4270_dram_parameter[target_idx]);

	/*
	 * Writing (old value | target value) in TIMINGROW register for META Stability
	 */
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC0_4X12 + DREX_TIMINGROW),
			utiming_row_cur_target, 0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC0_4X12 + DREX_TIMINGROW),
			exynos4270_dram_parameter[target_idx], 0);
	/* Writing TIMINGDATA and TIMINGPOWER for parameter */
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC0_4X12 + DREX_TIMINGDATA),
			exynos4270_dram_data_parameter[target_idx], 0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC0_4X12 + DREX_TIMINGPOWER),
			exynos4270_dram_power_parameter[target_idx], 0);

	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC1_4X12 + DREX_TIMINGROW),
			utiming_row_cur_target, 0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC1_4X12 + DREX_TIMINGROW),
			exynos4270_dram_parameter[target_idx], 0);
	/* Writing TIMINGDATA and TIMINGPOWER for parameter */
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC1_4X12 + DREX_TIMINGDATA),
			exynos4270_dram_data_parameter[target_idx], 0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(EXYNOS4_PA_DMC1_4X12 + DREX_TIMINGPOWER),
			exynos4270_dram_power_parameter[target_idx], 0);
#else
	/* Set DREX PARAMETER_ROW */
	utiming_row_cur = __raw_readl(xynos4270_base_drex0 + DREX_TIMINGROW);
	utiming_row_cur_target = (utiming_row_cur | exynos4270_dram_parameter[target_idx]);

	/*
	 * Writing (old value | target value) in TIMINGROW register for META Stability
	 */
	__raw_writel(utiming_row_cur_target, exynos4270_base_drex0 + DREX_TIMINGROW);
	__raw_writel(exynos4270_dram_parameter[target_idx], exynos4270_base_drex0 + DREX_TIMINGROW);
	/* Writing TIMINGDATA and TIMINGPOWER for parameter */
	__raw_writel(exynos4270_dram_data_parameter[target_idx], exynos4270_base_drex0 + DREX_TIMINGDATA);
	__raw_writel(exynos4270_dram_powerpower__parameter[target_idx], exynos4270_base_drex0 + DREX_TIMINGPOWER);

	__raw_writel(utiming_row_cur_target, exynos4270_base_drex1 + DREX_TIMINGROW);
	__raw_writel(exynos4270_dram_parameter[target_idx], exynos4270_base_drex1 + DREX_TIMINGROW);
	/* Writing TIMINGDATA and TIMINGPOWER for parameter */
	__raw_writel(exynos4270_dram_data_parameter[target_idx], exynos4270_base_drex1 + DREX_TIMINGDATA);
	__raw_writel(exynos4270_dram_power_parameter[target_idx], exynos4270_base_drex1 + DREX_TIMINGPOWER);
#endif
}
#endif

static int exynos4270_mif_busfreq_target(struct device *dev,
				      unsigned long *_freq, u32 flags)
{
	int err = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct busfreq_data_mif *data = platform_get_drvdata(pdev);
	struct opp *opp;
	unsigned long freq;
	unsigned long old_freq;
	unsigned long target_volt;

	/* get available opp information */
	rcu_read_lock();
	opp = devfreq_recommended_opp(dev, _freq, flags);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "%s: Invalid OPP.\n", __func__);
		return PTR_ERR(opp);
	}
	freq = opp_get_freq(opp);
	target_volt = opp_get_voltage(opp);
	rcu_read_unlock();

	/* get old opp information */
	rcu_read_lock();
	old_freq = opp_get_freq(data->curr_opp);
	rcu_read_unlock();

	exynos4270_mif_update_state(old_freq);

	if (old_freq == freq)
		return 0;

	mutex_lock(&data->lock);

	/*
	 * If target freq is higher than old freq
	 * after change voltage, setting freq ratio
	 */
	if (old_freq < freq) {
		regulator_set_voltage(data->vdd_mif, target_volt, SAFE_MIF_VOLT(target_volt));
		exynos_set_abb(ID_MIF, exynos4270_mif_asv_abb[exynos4270_find_mif_bus_idx(freq)]);
		exynos4270_set_drex(freq);
		exynos4270_mif_set_div(exynos4270_find_mif_bus_idx(freq));
	} else {
		exynos4270_mif_set_div(exynos4270_find_mif_bus_idx(freq));
		exynos4270_set_drex(freq);
		exynos_set_abb(ID_MIF, exynos4270_mif_asv_abb[exynos4270_find_mif_bus_idx(freq)]);
		regulator_set_voltage(data->vdd_mif, target_volt, SAFE_MIF_VOLT(target_volt));
	}

	data->curr_opp = opp;

	mutex_unlock(&data->lock);

	return err;
}

static struct devfreq_simple_exynos_data exynos3470_mif_governor_data = {
	.urgentthreshold	= 50,
	.upthreshold		= 45,
	.downthreshold		= 30,
	.idlethreshold		= 10,
	.cal_qos_max		= 400000,
	.pm_qos_class		= PM_QOS_BUS_THROUGHPUT,
};

static int exynos4270_mif_bus_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	struct busfreq_data_mif *data = dev_get_drvdata(dev);
	unsigned long busy_data;
	unsigned int mif_ccnt = 0;
	unsigned long mif_pmcnt = 0;
	unsigned int idx = -1;
	int above_idx = 0;
	int below_idx = LV_END - 1;
	int i;

	rcu_read_lock();
	stat->current_frequency = opp_get_freq(data->curr_opp);
	rcu_read_unlock();

	for (i = LV_0; i < LV_END; i++) {
		if (mif_bus_opp_list[i].clk == stat->current_frequency) {
			idx = i;
			above_idx = i - 1;
			below_idx = i + 1;
			break;
		}
	}

	if (idx < 0)
		return -EAGAIN;

	if (above_idx < 0)
		above_idx = 0;
	if (below_idx >= LV_END)
		below_idx = LV_END - 1;

	exynos3470_mif_governor_data.above_freq = mif_bus_opp_list[above_idx].clk;
	exynos3470_mif_governor_data.below_freq = mif_bus_opp_list[below_idx].clk;

	busy_data = exynos4270_ppmu_get_busy(data->ppmu, PPMU_SET_DDR,
					&mif_ccnt, &mif_pmcnt);

	stat->total_time = mif_ccnt;
	stat->busy_time = mif_pmcnt;

	return 0;
}

#if defined(CONFIG_DEVFREQ_GOV_PM_QOS)
static struct devfreq_pm_qos_data exynos4270_devfreq_mif_pm_qos_data = {
	.bytes_per_sec_per_hz = 8,
	.pm_qos_class = PM_QOS_BUS_THROUGHPUT,
};
#endif

static struct devfreq_dev_profile exynos4270_mif_devfreq_profile = {
	.initial_freq	= 400000,
	.polling_ms	= 100,
	.target		= exynos4270_mif_busfreq_target,
	.get_dev_status	= exynos4270_mif_bus_get_dev_status,
};

static int exynos4270_mif_table(struct busfreq_data_mif *data)
{
	unsigned int i;
	unsigned int ret;
	struct mif_clkdiv_info *tmp_mif_table;

	/* make list for setting value for int DVS */
	for (i = LV_0; i < LV_END; i++) {
		tmp_mif_table = kzalloc(sizeof(struct mif_clkdiv_info), GFP_KERNEL);

		tmp_mif_table->lv_idx = i;

		/* Setting DIV information */
		tmp_mif_table->div_dmc1.target_reg = EXYNOS4270_CLKDIV_DMC1;

		tmp_mif_table->div_dmc1.reg_value = ((exynos4270_dmc1_div_table[i][0] << EXYNOS4270_CLKDIV_DMC1_DMC_SHIFT) |\
						     (exynos4270_dmc1_div_table[i][1] << EXYNOS4270_CLKDIV_DMC1_DMCD_SHIFT) |\
						     (exynos4270_dmc1_div_table[i][2] << EXYNOS4270_CLKDIV_DMC1_DMCP_SHIFT));

		tmp_mif_table->div_dmc1.reg_mask = ((EXYNOS4270_CLKDIV_DMC1_DMC_MASK)|\
						     (EXYNOS4270_CLKDIV_DMC1_DMCP_MASK)|\
						     (EXYNOS4270_CLKDIV_DMC1_DMCD_MASK));

		/* Setting DIV checking information */
		tmp_mif_table->div_dmc1.wait_reg = EXYNOS4270_CLKDIV_DMC1_STAT;

		tmp_mif_table->div_dmc1.wait_mask = (EXYNOS4270_CLKDIV_STAT_DMC_MASK |\
						     EXYNOS4270_CLKDIV_STAT_DMCD_MASK |\
						     EXYNOS4270_CLKDIV_STAT_DMCP_MASK);

		list_add(&tmp_mif_table->list, &mif_dvfs_list);
	}

	/* will add code for ASV information setting function in here */

	for (i = 0; i < ARRAY_SIZE(mif_bus_opp_list); i++) {
		mif_bus_opp_list[i].volt = get_match_volt(ID_MIF, mif_bus_opp_list[i].clk);
		if (mif_bus_opp_list[i].volt == 0) {
			dev_err(data->dev, "Invalid value\n");
			return -EINVAL;
		}

		exynos4270_mif_asv_abb[i] = get_match_abb(ID_MIF, mif_bus_opp_list[i].clk);

		pr_info("MIF %luKhz ASV is %luuV ABB is %d\n", mif_bus_opp_list[i].clk,
						mif_bus_opp_list[i].volt,
						exynos4270_mif_asv_abb[i]);

		ret = opp_add(data->dev, mif_bus_opp_list[i].clk, mif_bus_opp_list[i].volt);

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

	if (!unlikely(mif_dev)) {
		pr_err("%s: device is not probed\n", __func__);
		return -ENODEV;
	}

	rcu_read_lock();
	for (i = 0; i < ARRAY_SIZE(mif_bus_opp_list); i++) {
		opp = opp_find_freq_exact(mif_dev, mif_bus_opp_list[i].clk, true);
		if (!IS_ERR_OR_NULL(opp))
			count += snprintf(&buf[count], PAGE_SIZE-count,"%lu ", opp_get_freq(opp));
	}
	rcu_read_unlock();

	count += snprintf(&buf[count], PAGE_SIZE-count,"\n");
	return count;
}

static DEVICE_ATTR(freq_table, S_IRUGO, show_freq_table, NULL);

static ssize_t mif_show_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t len = 0;
	ssize_t write_cnt = (ssize_t)((PAGE_SIZE / LV_END) - 2);

	for (i = LV_0; i < LV_END; i++)
		len += snprintf(buf + len, write_cnt, "%ld %llu\n", mif_bus_opp_list[i].clk,
				(unsigned long long)mif_bus_opp_list[i].time_in_state);

	return len;
}

static ssize_t mif_show_freq(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;
	ssize_t len = 0;
	ssize_t cnt_remain = (ssize_t)(PAGE_SIZE - 1);

	for (i = LV_END - 1; i >= 0; --i) {
		len += snprintf(buf + len, cnt_remain, "%ld ",
				mif_bus_opp_list[i].clk);
		cnt_remain = (ssize_t)(PAGE_SIZE - len - 1);
	}

	len += snprintf(buf + len, cnt_remain, "\n");

	return len;
}

static DEVICE_ATTR(mif_time_in_state, 0644, mif_show_state, NULL);
static DEVICE_ATTR(available_frequencies, S_IRUGO, mif_show_freq, NULL);
static struct attribute *busfreq_mif_entries[] = {
	&dev_attr_mif_time_in_state.attr,
	NULL,
};
static struct attribute_group busfreq_mif_attr_group = {
	.name	= "time_in_state",
	.attrs	= busfreq_mif_entries,
};

static struct exynos_devfreq_platdata default_qos_mif_pd = {
	.default_qos = 200000,
};

static int exynos4270_mif_reboot_notifier_call(struct notifier_block *this,
					unsigned long code, void *_cmd)
{
	pm_qos_update_request(&exynos4270_mif_qos, mif_bus_opp_list[LV_0].clk);

	return NOTIFY_DONE;
}

static struct notifier_block exynos4270_mif_reboot_notifier = {
	.notifier_call = exynos4270_mif_reboot_notifier_call,
};

static __devinit int exynos4270_busfreq_mif_probe(struct platform_device *pdev)
{
	struct busfreq_data_mif *data;
	struct opp *opp;
	struct device *dev = &pdev->dev;
	struct exynos_devfreq_platdata *pdata;
	unsigned long maxfreq = ULONG_MAX;
	struct opp *target_opp;
	unsigned long freq, volt;
	int err = 0;

	data = kzalloc(sizeof(struct busfreq_data_mif), GFP_KERNEL);

	if (data == NULL) {
		dev_err(dev, "Cannot allocate memory for INT.\n");
		return -ENOMEM;
	}

	if (samsung_rev() >= EXYNOS3470_REV_2_0) {
		if (pop_type == LPDDR3_1500) {
			exynos4270_dram_parameter = exynos4270_dram_parameter_rev2;
			exynos4270_set_drex = exynos4270_set_drex_rev2;
		} else {
			exynos4270_dram_parameter = exynos4270_dram_parameter_rev;
			exynos4270_set_drex = exynos4270_set_drex_rev;
		}
	} else {
		exynos4270_dram_parameter = exynos4270_dram_parameter_rev;
		exynos4270_set_drex = exynos4270_set_drex_rev;
	}

#ifndef CONFIG_ARM_TRUSTZONE
	/* ioremap for drex base address */
	exynos4270_base_drex0 = ioremap(EXYNOS4_PA_DMC0_4X12, SZ_128K);
	exynos4270_base_drex1 = ioremap(EXYNOS4_PA_DMC1_4X12, SZ_128K);
#endif

	data->dev = dev;
	mutex_init(&data->lock);

	/* Setting table for int */
	exynos4270_mif_table(data);

	/* Temporary code to exclude the value mif 100000 */
	opp_disable(dev, 100000);

	rcu_read_lock();
	opp = opp_find_freq_floor(dev, &exynos4270_mif_devfreq_profile.initial_freq);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "Invalid initial frequency %lu kHz.\n",
			       exynos4270_mif_devfreq_profile.initial_freq);
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
	rcu_read_unlock();

	mif_pre_time = get_jiffies_64();

	platform_set_drvdata(pdev, data);

	data->vdd_mif = regulator_get(dev, "vdd_mif");
	if (IS_ERR(data->vdd_mif)) {
		dev_err(dev, "Cannot get the regulator \"vdd_mif\"\n");
		err = PTR_ERR(data->vdd_mif);
		goto err_regulator;
	}

	rcu_read_lock();
	freq = exynos4270_mif_devfreq_profile.initial_freq;
	target_opp = devfreq_recommended_opp(dev, &freq, 0);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(dev, "%s: Invalid OPP for init voltage\n", __func__);
		err = PTR_ERR(target_opp);
		goto err_target_opp;
	}
	volt = opp_get_voltage(target_opp);
	rcu_read_unlock();
	regulator_set_voltage(data->vdd_mif, volt, SAFE_MIF_VOLT(volt));
	exynos_set_abb(ID_MIF, exynos4270_mif_asv_abb[exynos4270_find_mif_bus_idx(freq)]);

	data->ppmu = exynos4270_ppmu_get(PPMU_SET_DDR);
	if (!data->ppmu)
		goto err_ppmu_get;

	data->devfreq = devfreq_add_device(dev, &exynos4270_mif_devfreq_profile, &devfreq_simple_exynos, &exynos3470_mif_governor_data);
	if (IS_ERR(data->devfreq)) {
		err = PTR_ERR(data->devfreq);
		goto err_devfreq_add;
	}

#if defined(CONFIG_DEVFREQ_GOV_PM_QOS)
	data->devfreq = devfreq_add_device(dev, &exynos4270_mif_devfreq_profile, &devfreq_pm_qos, &exynos4270_devfreq_mif_pm_qos_data);
#endif

#if defined(CONFIG_DEVFREQ_GOV_USERSPACE)
	data->devfreq = devfreq_add_device(dev, &exynos4270_mif_devfreq_profile, &devfreq_userspace, NULL);
#endif

	data->devfreq->max_freq = maxfreq;
	devfreq_register_opp_notifier(dev, data->devfreq);

	mif_dev = data->dev;

	/* Create file for time_in_state */
	err = sysfs_create_group(&data->devfreq->dev.kobj, &busfreq_mif_attr_group);
	if (err) {
		pr_err("DEVFREQ(MIF) : Failed create time_in_state sysfs\n");
		goto err_sysfs_create;
	}

	err = device_create_file(&data->devfreq->dev, &dev_attr_available_frequencies);
	if (err) {
		pr_err("DEVFREQ(MIF) : Failed create available_frequencies sysfs\n");
		goto err_dev_create;
	}

	/* Add sysfs for freq_table */
	err = device_create_file(&data->devfreq->dev, &dev_attr_freq_table);
	if (err)
		pr_err("%s: Fail to create sysfs file\n", __func__);

	pdata = pdev->dev.platform_data;
	if (!pdata)
		pdata = &default_qos_mif_pd;

	/* Register notify */
	pm_qos_add_request(&exynos4270_mif_qos, PM_QOS_BUS_THROUGHPUT, pdata->default_qos);
	pm_qos_update_request_timeout(&exynos4270_mif_qos, exynos4270_mif_devfreq_profile.initial_freq, 40000 * 1000);

	register_reboot_notifier(&exynos4270_mif_reboot_notifier);

	return 0;

err_dev_create:
	sysfs_remove_group(&data->devfreq->dev.kobj, &busfreq_mif_attr_group);
err_sysfs_create:
	devfreq_unregister_opp_notifier(dev, data->devfreq);
	devfreq_remove_device(data->devfreq);
err_devfreq_add:
	exynos4270_ppmu_put(data->ppmu);
err_ppmu_get:
err_target_opp:
	regulator_put(data->vdd_mif);
err_regulator:
	platform_set_drvdata(pdev, NULL);
err_opp_add:
	mutex_destroy(&data->lock);
	kfree(data);

	return err;
}

static int __init exynos4270_busfreq_init_mif_qos(void)
{
	pm_qos_add_request(&bts_mif_qos, PM_QOS_BUS_THROUGHPUT, 0);

	return 0;
}
device_initcall(exynos4270_busfreq_init_mif_qos);

static __devexit int exynos4270_busfreq_mif_remove(struct platform_device *pdev)
{
	struct busfreq_data_mif *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);
	if (data->vdd_mif)
		regulator_put(data->vdd_mif);
	kfree(data);

	return 0;
}

#define MIF_COLD_OFFSET	25000

static int exynos4270_busfreq_mif_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct busfreq_data_mif *data = platform_get_drvdata(pdev);
	unsigned int temp_volt;

	exynos4270_mif_busfreq_target(dev, &suspend_freq, 0);

	temp_volt = get_match_volt(ID_MIF, mif_bus_opp_list[2].clk);
	regulator_set_voltage(data->vdd_mif, (temp_volt + MIF_COLD_OFFSET),
				SAFE_MIF_VOLT(temp_volt + MIF_COLD_OFFSET));

	return 0;
}

static int exynos4270_busfreq_mif_resume(struct device *dev)
{
	if (pm_qos_request_active(&exynos4270_mif_qos))
		pm_qos_update_request(&exynos4270_mif_qos, suspend_freq);
	return 0;
}

static const struct dev_pm_ops exynos4270_busfreq_mif_pm = {
	.suspend = exynos4270_busfreq_mif_suspend,
	.resume	= exynos4270_busfreq_mif_resume,
};

static struct platform_driver exynos4270_busfreq_mif_driver = {
	.probe	= exynos4270_busfreq_mif_probe,
	.remove	= __devexit_p(exynos4270_busfreq_mif_remove),
	.driver = {
		.name	= "exynos4270-busfreq-mif",
		.owner	= THIS_MODULE,
		.pm	= &exynos4270_busfreq_mif_pm,
	},
};

static int __init exynos4270_busfreq_mif_init(void)
{
	return platform_driver_register(&exynos4270_busfreq_mif_driver);
}
late_initcall(exynos4270_busfreq_mif_init);

static void __exit exynos4270_busfreq_mif_exit(void)
{
	platform_driver_unregister(&exynos4270_busfreq_mif_driver);
}
module_exit(exynos4270_busfreq_mif_exit);
