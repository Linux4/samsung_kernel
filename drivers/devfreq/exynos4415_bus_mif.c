/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

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
#include <linux/clk.h>
#include <linux/fb.h>

#include <mach/regs-clock.h>
#include <mach/devfreq.h>
#include <mach/smc.h>
#include <mach/asv-exynos.h>
#include <mach/regs-clock-exynos4415.h>
#ifdef CONFIG_EXYNOS_THERMAL
#include <mach/tmu.h>
#endif
#include <mach/sec_debug.h>

#include <plat/clock.h>

#include "exynos3470_ppmu.h"
#include "governor.h"

#define SAFE_MIF_VOLT(x)	(x + 25000)
#define SET_DREX_TIMING
#define COLD_OFFSET	50000

#ifdef CONFIG_EXYNOS_THERMAL
#define AREF_HOT		0x17
#define AREF_95			0x2E
#define AREF_COLD		0x5D
#endif

#define DREX_PAUSE_SHIFT	(24)
#define DREX_PAUSE_MASK		(1)

#ifdef CONFIG_EXYNOS_THERMAL
bool mif_is_probed;
#endif
static struct device *mif_dev;

static unsigned int cold_margin = 0;
static unsigned int enabled_fimc_lite;
static unsigned int num_mixer_layers;
static unsigned int num_fimd1_layers;

static struct pm_qos_request exynos4415_mif_qos;
static struct pm_qos_request media_mif_qos;
static struct pm_qos_request boot_mif_qos;
static struct pm_qos_request fb_mif_qos;
cputime64_t mif_pre_time;

static LIST_HEAD(mif_dvfs_list);

/* NoC list for MIF block */
static LIST_HEAD(mif_noc_list);
static DEFINE_MUTEX(media_mutex);

struct busfreq_data_mif {
	struct device *dev;
	struct devfreq *devfreq;
	struct opp *curr_opp;
	struct regulator *vdd_mif;
	struct exynos4270_ppmu_handle *ppmu;
	struct mutex lock;

	struct clk *mout_mpll;
	struct clk *mout_bpll;
	struct clk *mout_dmc_bus;
	struct clk *fout_bpll;

	struct clk *clkm_phy;
};

enum mif_bus_idx {
	LV_0 = 0,
	LV_1,
	LV_2,
	LV_3,
	LV_4,
	LV_5,
	LV_6,
	LV_END,
};

struct mif_bus_opp_table {
	unsigned int idx;
	unsigned long clk;
	unsigned long volt;
	cputime64_t time_in_state;
};

enum devfreq_mif_thermal_autorate {
	RATE_ONE = 0x0000005D,
	RATE_HALF = 0x0000002E,
	RATE_QUARTER = 0x00000017,
};

enum devfreq_mif_thermal_channel {
	THERMAL_CHANNEL0,
};

struct mif_bus_opp_table mif_bus_opp_list[] = {
	{LV_0, 543000, 950000, 0},
	{LV_1, 413000, 875000, 0},
	{LV_2, 275000, 850000, 0},
	{LV_3, 206000, 850000, 0},
	{LV_4, 165000, 850000, 0},
	{LV_5, 138000, 850000, 0},
	{LV_6, 103000, 850000, 0},
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
};

static unsigned int exynos4415_dmc1_div_table[][6] = {
	/* the register will be written with (table value-1) */
	/*DMCD_RATIO, DMCP_RATIO, DMC_PRE, DPHY_RATIO, DMC_RATIO */
	{ 2,	2,	1,	2,	2},	/* 543MHz*/
	{ 2,	2,	1,	2,	2},	/* 410MHz*/
	{ 2,	2,	1,	2,	3},	/* 275MHz*/
	{ 2,	2,	1,	2,	4},	/* 206MHz*/
	{ 2,	2,	1,	2,	5},	/* 165MHz*/
	{ 2,	2,	1,	2,	6},	/* 138MHz*/
	{ 2,	2,	1,	2,	8},	/* 103MHz*/
};

#ifdef SET_DREX_TIMING
/* Drex timing parameters for 1GB or 2GB */
static unsigned int exynos4415_dram_parameter_1_2gb[][3] = {
	/* timiningRow, timingData, timingPower */
	{ 0x244764CD, 0x35302509, 0x38270335},	/* 543Mhz */
	{ 0x1B35538A, 0x24202509, 0x2C1D0225},	/* 413Mhz */
	{ 0x19244287, 0x23202509, 0x1C140225},	/* 275Mhz */
	{ 0x192331C6, 0x22202509, 0x180F0225},	/* 206Mhz */
	{ 0x19223185, 0x22202509, 0x140C0225},	/* 165Mhz */
	{ 0x19222144, 0x22202509, 0x100C0225},	/* 138Mhz */
	{ 0x19222103, 0x22202509, 0x100C0225},	/* 103Mhz */
};

/* Drex timing parameters for 1.5GB */
static unsigned int exynos4415_dram_parameter_1nhalfgb[][3] = {
	/* timiningRow, timingData, timingPower */
	{ 0x3A4764CD, 0x35302509, 0x38700335},  /* 543Mhz */
	{ 0x2C35538A, 0x24202509, 0x2C550225},  /* 413Mhz */
	{ 0x1D244287, 0x23202509, 0x1C390225},  /* 275Mhz */
	{ 0x192331C6, 0x23202509, 0x182B0225},  /* 206Mhz */
	{ 0x19233185, 0x23202509, 0x14220225},  /* 165Mhz */
	{ 0x19232144, 0x23202509, 0x101F0225},  /* 138Mhz */
	{ 0x19232103, 0x23202509, 0x101F0225},  /* 103Mhz */
};
static unsigned int (*dram_timing_parameter)[3];
#endif

struct devfreq_thermal_work {
	struct delayed_work devfreq_mif_thermal_work;
	enum devfreq_mif_thermal_channel channel;
	struct workqueue_struct *work_queue;
	unsigned int polling_period;
	unsigned long max_freq;
};
static struct workqueue_struct *devfreq_mif_thermal_wq_ch0;
static struct devfreq_thermal_work devfreq_mif_ch0_work = {
	.channel	= THERMAL_CHANNEL0,
	.polling_period	= 1000,
};

static void exynos4415_devfreq_thermal_event(struct devfreq_thermal_work *work)
{
	if (work->polling_period == 0)
		return;

	queue_delayed_work(work->work_queue,
			&work->devfreq_mif_thermal_work,
			msecs_to_jiffies(work->polling_period));
}

extern void exynos4_update_district_int_level(unsigned int idx);

void exynos4_update_media_layers(enum devfreq_media_type media_type,
							unsigned int value)
{
	unsigned int num_total_layers, int_idx = LV_2;
	unsigned long media_qos_mif_freq = mif_bus_opp_list[LV_0].clk;

	mutex_lock(&media_mutex);

	if (media_type == TYPE_FIMC_LITE)
		enabled_fimc_lite = value;
	else if (media_type == TYPE_MIXER)
		num_mixer_layers = value;
	else if (media_type == TYPE_FIMD1)
		num_fimd1_layers = value;

	num_total_layers = num_mixer_layers + num_fimd1_layers;

	switch (num_total_layers) {
		case NUM_LAYER_5:
			int_idx = LV_2;
			media_qos_mif_freq = mif_bus_opp_list[LV_0].clk;
			break;
		case NUM_LAYER_4:
			int_idx = LV_2;
			media_qos_mif_freq = mif_bus_opp_list[LV_0].clk;
			break;
		case NUM_LAYER_3:
			int_idx = LV_3;
			media_qos_mif_freq = mif_bus_opp_list[LV_1].clk;
			break;
		case NUM_LAYER_2:
			int_idx = LV_5;
			media_qos_mif_freq = mif_bus_opp_list[LV_3].clk;
			break;
		case NUM_LAYER_1:
			int_idx = LV_6 + 1;
			media_qos_mif_freq = mif_bus_opp_list[LV_5].clk;
			break;
		case NUM_LAYER_0:
			int_idx = LV_6 + 1;
			media_qos_mif_freq = mif_bus_opp_list[LV_6].clk;
			break;
	}

	if (num_mixer_layers > 0) {
		if(int_idx < LV_5)
			int_idx = LV_5;

		if(media_qos_mif_freq < mif_bus_opp_list[LV_1].clk)
			media_qos_mif_freq = mif_bus_opp_list[LV_1].clk;
	}

	if (enabled_fimc_lite && (num_mixer_layers == 0)) {
		media_qos_mif_freq = mif_bus_opp_list[LV_0].clk;
		exynos4_update_district_int_level(LV_3);
	}

	exynos4_update_district_int_level(int_idx);

	if (pm_qos_request_active(&media_mif_qos))
		pm_qos_update_request(&media_mif_qos, media_qos_mif_freq);

	mutex_unlock(&media_mutex);
}
EXPORT_SYMBOL_GPL(exynos4_update_media_layers);

static void exynos4415_mif_update_state(unsigned int target_freq)
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

static int exynos4415_mif_waiting_pause(void)
{
	int timeout = 1000;

	while (((__raw_readl(EXYNOS4415_CLKDIV_STAT_DMC) >> DREX_PAUSE_SHIFT) & DREX_PAUSE_MASK) != DREX_PAUSE_MASK) {
		if (--timeout <= 0) {
			pr_err("%s : DMC Pause status timeout\n", __func__);
			return -EINVAL;
		}
	};

	return 0;
}

static int exynos4415_mif_change_mux(struct busfreq_data_mif *data,
						unsigned long target_freq)
{
	if (target_freq == mif_bus_opp_list[LV_0].clk) {
		/* Change parent to BPLL */
		if (clk_set_parent(data->mout_dmc_bus, data->mout_bpll))
			pr_err("Unable to set parent %s of clock %s.\n",
				data->mout_dmc_bus->name, data->mout_bpll->name);
	} else {
		/* Change parent to MPLL */
		if (clk_set_parent(data->mout_dmc_bus, data->mout_mpll))
			pr_err("Unable to set parent %s of clock %s.\n",
				data->mout_dmc_bus->name, data->mout_mpll->name);
	}

	exynos4415_mif_waiting_pause();

	return 0;
}

static unsigned int exynos4415_mif_set_div(enum mif_bus_idx target_idx)
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

	exynos4415_mif_waiting_pause();

	return 0;
}

static enum mif_bus_idx exynos4415_find_mif_bus_idx(unsigned long target_freq)
{
	unsigned int i;

	for (i = 0; i < LV_END; i++) {
		if (mif_bus_opp_list[i].clk == target_freq)
			return i;
	}

	return LV_END;
}

#ifdef SET_DREX_TIMING

#define DREX_DIRECTCMD		0x10
#define DREX_MRSTATUS		0x54

#define DREX_TIMINGAREF		0x30
#define DREX_TIMINGROW		0x34
#define DREX_TIMINGDATA		0x38
#define DREX_TIMINGPOWER	0x3C

#define DRAM_SIZE_1NHALFGB	0x01

#ifndef CONFIG_ARM_TRUSTZONE
static void __iomem *exynos4415_base_drex0;
#endif

static void exynos4415_set_drex(unsigned long target_freq)
{
	unsigned target_idx = exynos4415_find_mif_bus_idx(target_freq);
	unsigned int utiming_row_cur, utiming_row_cur_target;

#ifdef CONFIG_ARM_TRUSTZONE
	/* Read DREX parameters TIMINGROW, TIMINGDATA & TIMINGPOWER */
	exynos_smc_readsfr(EXYNOS4415_PA_DMC + DREX_TIMINGROW,
						&utiming_row_cur);

	/*
	* Writing (old value | target value) in TIMINGROW, TIMINGDATA
	* and TIMINGPOWER for META Stability
	*/
	utiming_row_cur_target =
		(utiming_row_cur | dram_timing_parameter[target_idx][0]);

	exynos_smc(SMC_CMD_REG,
			SMC_REG_ID_SFR_W(EXYNOS4415_PA_DMC + DREX_TIMINGROW),
			utiming_row_cur_target, 0);

	/*
	* Writing target value in TIMINGROW, TIMINGDATA and TIMINGPOWER registers
	* for META Stability
	*/
	exynos_smc(SMC_CMD_REG,
			SMC_REG_ID_SFR_W(EXYNOS4415_PA_DMC + DREX_TIMINGROW),
			dram_timing_parameter[target_idx][0], 0);

#else
	utiming_row_cur = __raw_readl(exynos4415_base_drex0 + DREX_TIMINGROW);
	utiming_row_cur_target =
		(utiming_row_cur | dram_timing_parameter[target_idx][0]);

	/*
	* Writing (old value | target value) in TIMINGROW register for
	* META Stability
	*/
	__raw_writel(utiming_row_cur_target,
				exynos4415_base_drex0 + DREX_TIMINGROW);

	__raw_writel(dram_timing_parameter[target_idx][0],
				exynos4415_base_drex0 + DREX_TIMINGROW);
#endif
}
#endif /*SET_DREX_TIMING*/

static int exynos4415_mif_busfreq_target(struct device *dev,
				      unsigned long *_freq, u32 flags)
{
	int err = 0;
	struct platform_device *pdev =
			container_of(dev, struct platform_device, dev);
	struct busfreq_data_mif *data = platform_get_drvdata(pdev);
	struct opp *opp;
	unsigned long freq;
	unsigned long old_freq;
	unsigned long target_volt;

	if (devfreq_mif_ch0_work.max_freq)
		*_freq = min(*_freq, devfreq_mif_ch0_work.max_freq);

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

	exynos4415_mif_update_state(old_freq);


	if (old_freq == freq)
		return 0;

	sec_debug_aux_log(SEC_DEBUG_AUXLOG_CPU_CLOCK_SWITCH_CHANGE,
		"[MIF] %7d <= %7d",
		freq, old_freq);

	mutex_lock(&data->lock);

	/*
	 * If target freq is higher than old freq
	 * after change voltage, setting freq ratio
	 */
	if (old_freq < freq) {
		regulator_set_voltage(data->vdd_mif,
				target_volt + cold_margin, SAFE_MIF_VOLT(target_volt + cold_margin));
#ifdef SET_DREX_TIMING
		exynos4415_set_drex(freq);
#endif
		exynos4415_mif_set_div(exynos4415_find_mif_bus_idx(freq));

		if (freq == mif_bus_opp_list[LV_0].clk) {
			exynos4415_mif_change_mux(data, freq);
			clk_enable(data->clkm_phy);
		}
	} else {
		if (old_freq == mif_bus_opp_list[LV_0].clk) {
			clk_disable(data->clkm_phy);
			exynos4415_mif_change_mux(data, freq);
		}

		exynos4415_mif_set_div(exynos4415_find_mif_bus_idx(freq));
#ifdef SET_DREX_TIMING
		exynos4415_set_drex(freq);
#endif
		regulator_set_voltage(data->vdd_mif,
				target_volt + cold_margin, SAFE_MIF_VOLT(target_volt + cold_margin));
	}

	data->curr_opp = opp;

	mutex_unlock(&data->lock);

	return err;
}

#if defined(CONFIG_DEVFREQ_GOV_PM_QOS)
static struct devfreq_pm_qos_data exynos4415_devfreq_mif_pm_qos_data = {
	.bytes_per_sec_per_hz = 8,
	.pm_qos_class = PM_QOS_BUS_THROUGHPUT,
};
#endif

static struct devfreq_simple_exynos_data exynos4415_mif_governor_data = {
	.pm_qos_class		= PM_QOS_BUS_THROUGHPUT,
	.urgentthreshold	= 45,
	.upthreshold		= 40,
	.downthreshold		= 25,
	.idlethreshold		= 20,
	.cal_qos_max		= 543000,
};

static int exynos4415_mif_bus_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	struct busfreq_data_mif *data = dev_get_drvdata(dev);
	unsigned long busy_data;
	unsigned int mif_ccnt = 0;
	unsigned long mif_pmcnt = 0;
	int idx = -1;
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

	exynos4415_mif_governor_data.above_freq = mif_bus_opp_list[above_idx].clk;
	exynos4415_mif_governor_data.below_freq = mif_bus_opp_list[below_idx].clk;
	busy_data = exynos4270_ppmu_get_busy(data->ppmu, PPMU_SET_DDR,
					&mif_ccnt, &mif_pmcnt);

	stat->total_time = mif_ccnt;
	stat->busy_time = mif_pmcnt;

	return 0;
}

static struct devfreq_dev_profile exynos4415_mif_devfreq_profile = {
	.initial_freq	= 543000,
	.polling_ms	= 100,
	.target		= exynos4415_mif_busfreq_target,
	.get_dev_status	= exynos4415_mif_bus_get_dev_status,
};

static int exynos4415_mif_table(struct busfreq_data_mif *data)
{
	unsigned int i;
	unsigned int ret, tmp;
	struct mif_clkdiv_info *tmp_mif_table;

	/* Enable pause function for DREX2 DVFS */
	tmp = __raw_readl(EXYNOS4415_CLK_DMC_PAUSE_CTR);
	tmp |= EXYNOS4415_DMC_PAUSE_ENABLE;
	__raw_writel(tmp, EXYNOS4415_CLK_DMC_PAUSE_CTR);

	/* make list for setting value for int DVS */
	for (i = LV_0; i < LV_END; i++) {
		tmp_mif_table =
			kzalloc(sizeof(struct mif_clkdiv_info), GFP_KERNEL);

		tmp_mif_table->lv_idx = i;

		/* Setting DIV information */
		tmp_mif_table->div_dmc1.target_reg = EXYNOS4415_CLKDIV_DMC1;

		tmp_mif_table->div_dmc1.reg_value =
			(((exynos4415_dmc1_div_table[i][0] - 1)
					<< EXYNOS4415_CLKDIV_DMC1_DMCD_SHIFT) |\
			((exynos4415_dmc1_div_table[i][1] - 1)
					<< EXYNOS4415_CLKDIV_DMC1_DMCP_SHIFT) |\
			((exynos4415_dmc1_div_table[i][2] - 1)
					<< EXYNOS4415_CLKDIV_DMC1_DMCPRE_SHIFT) |\
			((exynos4415_dmc1_div_table[i][3] - 1)
					<< EXYNOS4415_CLKDIV_DMC1_DMCPHY_SHIFT) |\
			((exynos4415_dmc1_div_table[i][4] - 1)
					<< EXYNOS4415_CLKDIV_DMC1_DMC_SHIFT));

		tmp_mif_table->div_dmc1.reg_mask =
				((EXYNOS4415_CLKDIV_DMC1_DMCD_MASK)|\
				(EXYNOS4415_CLKDIV_DMC1_DMCP_MASK)|\
				(EXYNOS4415_CLKDIV_DMC1_DMCPRE_MASK)|\
				(EXYNOS4415_CLKDIV_DMC1_DMCPHY_MASK)|\
				(EXYNOS4415_CLKDIV_DMC1_DMC_MASK));

		/* Setting DIV checking information */
		tmp_mif_table->div_dmc1.wait_reg = EXYNOS4415_CLKDIV_STAT_DMC;

		tmp_mif_table->div_dmc1.wait_mask =
				(EXYNOS4415_CLKDIV_STAT_DMC_MASK |\
				EXYNOS4415_CLKDIV_STAT_DMCD_MASK |\
				EXYNOS4415_CLKDIV_STAT_DMCP_MASK |\
				EXYNOS4415_CLKDIV_STAT_DMCPRE_MASK);

		list_add(&tmp_mif_table->list, &mif_dvfs_list);
	}

	/* will add code for ASV information setting function in here */

	for (i = 0; i < ARRAY_SIZE(mif_bus_opp_list); i++) {
		mif_bus_opp_list[i].volt = get_match_volt(ID_MIF, mif_bus_opp_list[i].clk);
		if (mif_bus_opp_list[i].volt == 0) {
			dev_err(data->dev, "Invalid value \n");
			return -EINVAL;
		}

		pr_info("MIF %luKhz ASV is %luuV\n", mif_bus_opp_list[i].clk,
						mif_bus_opp_list[i].volt);

		ret = opp_add(data->dev, mif_bus_opp_list[i].clk,
						mif_bus_opp_list[i].volt);

		if (ret) {
			dev_err(data->dev, "Fail to add opp entries.\n");
			return ret;
		}
	}

	return 0;
}

static ssize_t show_freq_table(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i, count = 0;
	struct opp *opp;

	if (!unlikely(mif_dev)) {
		pr_err("%s: device is not probed\n", __func__);
		return -ENODEV;
	}

	rcu_read_lock();
	for (i = 0; i < ARRAY_SIZE(mif_bus_opp_list); i++) {
		opp = opp_find_freq_exact(mif_dev,
					mif_bus_opp_list[i].clk, true);
		if (!IS_ERR_OR_NULL(opp))
			count += sprintf(&buf[count], "%lu ", opp_get_freq(opp));
	}
	rcu_read_unlock();

	count += sprintf(&buf[count], "\n");
	return count;
}

static DEVICE_ATTR(freq_table, S_IRUGO, show_freq_table, NULL);

static ssize_t mif_show_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t len = 0;
	ssize_t write_cnt = (ssize_t)((PAGE_SIZE / LV_END) - 2);

	for (i = LV_0; i < LV_END; i++)
		len += snprintf(buf + len, write_cnt, "%ld %llu\n",
				mif_bus_opp_list[i].clk,
				(unsigned long long)mif_bus_opp_list[i].time_in_state);

	return len;
}

static DEVICE_ATTR(mif_time_in_state, 0644, mif_show_state, NULL);
static struct attribute *busfreq_mif_entries[] = {
	&dev_attr_mif_time_in_state.attr,
	NULL,
};
static struct attribute_group busfreq_mif_attr_group = {
	.name	= "time_in_state",
	.attrs	= busfreq_mif_entries,
};

static struct exynos_devfreq_platdata default_qos_mif_pd = {
	.default_qos = 103000,
};

static int fb_state_mif(struct notifier_block *nb,
			unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;

	if (val != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;
	switch (blank) {
	case FB_BLANK_POWERDOWN:
		pm_qos_update_request_timeout(&fb_mif_qos,
			exynos4415_mif_devfreq_profile.initial_freq,
			2000 * 1000);
		break;
	case FB_BLANK_UNBLANK:
		pm_qos_update_request_timeout(&fb_mif_qos,
			exynos4415_mif_devfreq_profile.initial_freq,
			2000 * 1000);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block fb_block_mif = {
	.notifier_call = fb_state_mif,
};

static int exynos4415_mif_reboot_notifier_call(struct notifier_block *this,
					unsigned long code, void *_cmd)
{
	pm_qos_update_request(&exynos4415_mif_qos, mif_bus_opp_list[LV_0].clk);

	return NOTIFY_DONE;
}

static struct notifier_block exynos4415_mif_reboot_notifier = {
	.notifier_call = exynos4415_mif_reboot_notifier_call,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos4415_bus_mif_tmu_notifier(struct notifier_block *notifier,
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

static struct notifier_block exynos4415_bus_mif_tmu_nb = {
	.notifier_call = exynos4415_bus_mif_tmu_notifier,
};
#endif

static void exynos4415_devfreq_thermal_monitor(struct work_struct *work)
{
	bool throttling = false;
	unsigned int mrstatus;
	unsigned int timingaref_value = RATE_ONE;

	unsigned long max_freq = exynos4415_mif_governor_data.cal_qos_max;

	struct delayed_work *d_work =
			container_of(work, struct delayed_work, work);
	struct devfreq_thermal_work *thermal_work =
			container_of(d_work, struct devfreq_thermal_work,
						devfreq_mif_thermal_work);

	struct platform_device *pdev =
			container_of(mif_dev, struct platform_device, dev);
	struct busfreq_data_mif *data = platform_get_drvdata(pdev);

	mutex_lock(&data->lock);
#ifndef CONFIG_ARM_TRUSTZONE
	__raw_writel(0x09001000, exynos4415_base_drex0 + DREX_DIRECTCMD);
	mrstatus = __raw_readl(exynos4415_base_drex0 + DREX_MRSTATUS);
#else
	exynos_smc(SMC_CMD_REG,
			SMC_REG_ID_SFR_W(EXYNOS4415_PA_DMC + DREX_DIRECTCMD),
			0x09001000, 0);
	exynos_smc_readsfr(EXYNOS4415_PA_DMC + DREX_MRSTATUS, &mrstatus);
#endif
	mutex_unlock(&data->lock);

	switch (mrstatus & 0x7) {
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
		max_freq = mif_bus_opp_list[LV_3].clk;
	else
		max_freq = exynos4415_mif_governor_data.cal_qos_max;

	if (thermal_work->max_freq != max_freq) {
		thermal_work->max_freq = max_freq;
		mutex_lock(&data->devfreq->lock);
		update_devfreq(data->devfreq);
		mutex_unlock(&data->devfreq->lock);
	}

#ifndef CONFIG_ARM_TRUSTZONE
	__raw_writel(timingaref_value,
				exynos4415_base_drex0 + DREX_TIMINGAREF);
#else
	exynos_smc(SMC_CMD_REG,
			SMC_REG_ID_SFR_W(EXYNOS4415_PA_DMC + DREX_TIMINGAREF),
			timingaref_value, 0);
#endif
	exynos4415_devfreq_thermal_event(thermal_work);
}

static void exynos4415_devfreq_init_thermal(void)
{
	devfreq_mif_thermal_wq_ch0 =
			create_freezable_workqueue("devfreq_thermal_wq_ch0");

	INIT_DELAYED_WORK(&devfreq_mif_ch0_work.devfreq_mif_thermal_work,
			exynos4415_devfreq_thermal_monitor);

	devfreq_mif_ch0_work.work_queue = devfreq_mif_thermal_wq_ch0;

	exynos4415_devfreq_thermal_event(&devfreq_mif_ch0_work);
}

static __devinit int exynos4415_busfreq_mif_probe(struct platform_device *pdev)
{
	struct busfreq_data_mif *data;
	struct opp *opp;
	struct device *dev = &pdev->dev;
	struct exynos_devfreq_platdata *pdata;
	unsigned long maxfreq = ULONG_MAX;
	int err = 0;

	fb_register_client(&fb_block_mif);

	data = kzalloc(sizeof(struct busfreq_data_mif), GFP_KERNEL);

	if (data == NULL) {
		dev_err(dev, "Cannot allocate memory for INT.\n");
		return -ENOMEM;
	}

#ifdef SET_DREX_TIMING
#ifndef CONFIG_ARM_TRUSTZONE
	/* ioremap for drex base address */
	exynos4415_base_drex0 = ioremap(EXYNOS4415_PA_DMC, SZ_128K);
	if (!exynos4415_base_drex0) {
		dev_err(dev, "Error while Mapping DMC base address...\n");
		kfree(data);
		return -ENOMEM;
	}
#endif
	/* Get DDR size for deciding DREX timing parameters */
	if (exynos4415_get_ddrsize() == DRAM_SIZE_1NHALFGB)
		dram_timing_parameter = &exynos4415_dram_parameter_1nhalfgb[0];
	else
		dram_timing_parameter = &exynos4415_dram_parameter_1_2gb[0];
#endif

	data->dev = dev;
	mutex_init(&data->lock);

	/* Setting table for int */
	err = exynos4415_mif_table(data);
	if (err) {
		dev_err(dev, "%s: mif table initialization failed.\n", __func__);
		goto err_init_table;
	}

#if 0 //XXX
	/* Temporary code to exclude the value mif 100000 */
	opp_disable(dev, 100000);
#endif

	data->mout_dmc_bus = clk_get(dev, "mout_dmc_bus");
	if (IS_ERR(data->mout_dmc_bus)) {
		dev_err(dev, "Cannot get clock \"mout_dmc_bus\"\n");
		err = PTR_ERR(data->mout_dmc_bus);
		goto err_mout_dmc_bus;
	}

	data->mout_mpll = clk_get(dev, "mout_mpll");
	if (IS_ERR(data->mout_mpll)) {
		dev_err(dev, "Cannot get clock \"mout_mpll\"\n");
		err = PTR_ERR(data->mout_mpll);
		goto err_mout_mpll;
	}

	data->mout_bpll = clk_get(dev, "mout_bpll");
	if (IS_ERR(data->mout_bpll)) {
		dev_err(dev, "Cannot get clock \"mout_bpll\"\n");
		err = PTR_ERR(data->mout_bpll);
		goto err_mout_bpll;
	}

	if (clk_set_parent(data->mout_dmc_bus, data->mout_bpll)) {
		dev_err(dev, "Cannot Set Parent DMC of %s as %s\n",
				data->mout_dmc_bus->name,
				data->mout_bpll->name);
		goto err_set_parent_dmc;
	}

	data->fout_bpll = clk_get(dev, "fout_bpll");
	if (IS_ERR(data->fout_bpll)) {
		dev_err(dev, "Cannot get clock \"fout_bpll\"\n");
		err = PTR_ERR(data->fout_bpll);
		goto err_fout_bpll;
	}

	data->clkm_phy = clk_get(dev, "sclk_dphy");
	if (IS_ERR(data->clkm_phy)) {
		dev_err(dev, "Cannot get clock \"sclk_dphy\"\n");
		err = PTR_ERR(data->clkm_phy);
		goto err_clkm_phy;
	}

	if (clk_set_parent(data->clkm_phy, data->mout_bpll)) {
		dev_err(dev, "Cannot Set Parent PHY of %s as %s\n",
				data->clkm_phy->name, data->mout_bpll->name);
		goto err_set_parent_phy;
	}
	clk_enable(data->clkm_phy);

	rcu_read_lock();
	opp = opp_find_freq_floor(dev,
				&exynos4415_mif_devfreq_profile.initial_freq);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "Invalid initial frequency %lu kHz.\n",
			       exynos4415_mif_devfreq_profile.initial_freq);
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

	/* Initialize thermal_work max_freq to LV_0 Frequency */
	devfreq_mif_ch0_work.max_freq = mif_bus_opp_list[LV_0].clk;

	data->ppmu = exynos4270_ppmu_get(PPMU_SET_DDR);
	if (!data->ppmu)
		goto err_ppmu_get;

#if defined(CONFIG_DEVFREQ_GOV_PM_QOS)
	data->devfreq =
		devfreq_add_device(dev, &exynos4415_mif_devfreq_profile,
					&devfreq_pm_qos,
					&exynos4415_devfreq_mif_pm_qos_data);
#endif

#if defined(CONFIG_DEVFREQ_GOV_USERSPACE)
	data->devfreq =
		devfreq_add_device(dev, &exynos4415_mif_devfreq_profile,
				&devfreq_userspace, NULL);
#endif

#if defined(CONFIG_DEVFREQ_GOV_SIMPLE_EXYNOS)
	data->devfreq =
		devfreq_add_device(dev, &exynos4415_mif_devfreq_profile,
					&devfreq_simple_exynos,
					&exynos4415_mif_governor_data);
#endif
	if (IS_ERR(data->devfreq)) {
		err = PTR_ERR(data->devfreq);
		goto err_devfreq_add;
	}

	data->devfreq->max_freq = maxfreq;
	devfreq_register_opp_notifier(dev, data->devfreq);

	mif_dev = data->dev;

	/* Create file for time_in_state */
	err = sysfs_create_group(&data->devfreq->dev.kobj,
			&busfreq_mif_attr_group);

	/* Add sysfs for freq_table */
	err = device_create_file(&data->devfreq->dev, &dev_attr_freq_table);
	if (err)
		pr_err("%s: Fail to create sysfs file\n", __func__);

	exynos4415_devfreq_init_thermal();

	pdata = pdev->dev.platform_data;
	if (!pdata)
		pdata = &default_qos_mif_pd;

	/* Register notify */
	pm_qos_add_request(&exynos4415_mif_qos, PM_QOS_BUS_THROUGHPUT,
							pdata->default_qos);
	pm_qos_add_request(&fb_mif_qos, PM_QOS_BUS_THROUGHPUT,
							pdata->default_qos);
	pm_qos_add_request(&boot_mif_qos, PM_QOS_BUS_THROUGHPUT,
							pdata->default_qos);
	pm_qos_add_request(&media_mif_qos, PM_QOS_BUS_THROUGHPUT,
							pdata->default_qos);

	pm_qos_update_request_timeout(&boot_mif_qos,
		exynos4415_mif_devfreq_profile.initial_freq, 25000 * 1000);

	register_reboot_notifier(&exynos4415_mif_reboot_notifier);
#ifdef CONFIG_EXYNOS_THERMAL
	/* For low temperature compensation when boot time */
	cold_margin = COLD_OFFSET;

	exynos_tmu_add_notifier(&exynos4415_bus_mif_tmu_nb);
	mif_is_probed = true;
#endif
	printk(KERN_INFO "EXYNOS4 busfreq driver-MIF Initialized\n");
	return 0;

err_devfreq_add:
	devfreq_remove_device(data->devfreq);
err_ppmu_get:
	if (data->vdd_mif)
		regulator_put(data->vdd_mif);
err_regulator:
	platform_set_drvdata(pdev, NULL);
err_opp_add:
	clk_disable(data->clkm_phy);
err_set_parent_phy:
	clk_put(data->clkm_phy);
err_clkm_phy:
	clk_put(data->fout_bpll);
err_fout_bpll:
err_set_parent_dmc:
	clk_put(data->mout_bpll);
err_mout_bpll:
	clk_put(data->mout_mpll);
err_mout_mpll:
	clk_put(data->mout_dmc_bus);
err_mout_dmc_bus:
err_init_table:
	kfree(data);
	return err;
}

static __devexit int exynos4415_busfreq_mif_remove(struct platform_device *pdev)
{
	struct busfreq_data_mif *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);
	if (data->vdd_mif)
		regulator_put(data->vdd_mif);
	kfree(data);

	return 0;
}

static int exynos4415_busfreq_mif_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev,
				struct platform_device, dev);
	struct busfreq_data_mif *data = platform_get_drvdata(pdev);
	unsigned long suspend_freq = mif_bus_opp_list[0].clk;

	exynos4415_mif_busfreq_target(dev, &suspend_freq, 0);
	regulator_set_voltage(data->vdd_mif, mif_bus_opp_list[LV_0].volt + COLD_OFFSET,
				SAFE_MIF_VOLT(mif_bus_opp_list[LV_0].volt + COLD_OFFSET));
	return 0;
}

static int exynos4415_busfreq_mif_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev,
				struct platform_device, dev);
	struct busfreq_data_mif *data = platform_get_drvdata(pdev);
	int tmp;

	/* Enable pause function for DREX2 DVFS */
	tmp = __raw_readl(EXYNOS4415_CLK_DMC_PAUSE_CTR);
	tmp |= EXYNOS4415_DMC_PAUSE_ENABLE;
	__raw_writel(tmp, EXYNOS4415_CLK_DMC_PAUSE_CTR);

	regulator_set_voltage(data->vdd_mif, mif_bus_opp_list[LV_0].volt,
				SAFE_MIF_VOLT(mif_bus_opp_list[LV_0].volt));

	if (pm_qos_request_active(&exynos4415_mif_qos)) {
		pm_qos_update_request(&exynos4415_mif_qos,
					mif_bus_opp_list[LV_3].clk);
		pm_qos_update_request_timeout(&boot_mif_qos,
				exynos4415_mif_devfreq_profile.initial_freq,
				2000 * 1000);
	}
	return 0;
}

static const struct dev_pm_ops exynos4415_busfreq_mif_pm = {
	.suspend = exynos4415_busfreq_mif_suspend,
	.resume	= exynos4415_busfreq_mif_resume,
};

static struct platform_driver exynos4415_busfreq_mif_driver = {
	.probe	= exynos4415_busfreq_mif_probe,
	.remove	= __devexit_p(exynos4415_busfreq_mif_remove),
	.driver = {
		.name	= "exynos4415-busfreq-mif",
		.owner	= THIS_MODULE,
		.pm	= &exynos4415_busfreq_mif_pm,
	},
};

static int __init exynos4415_busfreq_mif_init(void)
{
	return platform_driver_register(&exynos4415_busfreq_mif_driver);
}
late_initcall(exynos4415_busfreq_mif_init);

static void __exit exynos4415_busfreq_mif_exit(void)
{
	platform_driver_unregister(&exynos4415_busfreq_mif_driver);
}
module_exit(exynos4415_busfreq_mif_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EXYNOS4 busfreq driver with devfreq framework");
