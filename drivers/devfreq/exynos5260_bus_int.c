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
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/sched.h>

#include <mach/devfreq.h>
#include <mach/asv-exynos.h>
#include <mach/pm_domains.h>
#include <mach/regs-clock-exynos5260.h>
#include <mach/tmu.h>

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
#include <mach/sec_debug.h>
#endif

#include "exynos5260_ppmu.h"
#include "exynos_ppmu2.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(333000)
#define DEVFREQ_POLLING_PERIOD	(0)

#define INT_VOLT_STEP		12500
#define COLD_VOLT_OFFSET	50000
#define LIMIT_COLD_VOLTAGE	1250000
#define MIN_COLD_VOLTAGE	950000

enum devfreq_int_idx {
	LV0_ISP_BOOST,
	LV0,
	LV1,
	LV2,
	LV3,
	LV4,
	LV_COUNT,
};

enum devfreq_int_clk {
	ACLK_BUS1D_400,
	ACLK_BUS2D_400,
	ACLK_BUS3D_400,
	ACLK_BUS4D_400,
	PCLK_BUS1P_100,
	PCLK_BUS2P_100,
	PCLK_BUS3P_100,
	PCLK_BUS4P_100,
	ACLK_G2D_333,
	ACLK_MFC_333,
	ACLK_GSCL_333,
	ACLK_GSCL_400,
	ACLK_GSCL_FIMC,
	ACLK_ISP_266,
	ACLK_ISP_400,
	ACLK_FSYS_200,
	ACLK_PERI_66,
	MOUT_ISP1_MEDIA_266,
	SCLK_MEDIATOP_PLL,
	SCLK_BUSTOP_PLL,
	SCLK_MEMTOP_PLL,
	FOUT_BPLL,
	CLK_COUNT,
};

struct devfreq_data_int {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_int;
	unsigned long old_volt;
	unsigned long volt_offset;
	cputime64_t last_jiffies;

	struct notifier_block tmu_notifier;

	bool use_dvfs;

	struct mutex lock;
};

struct devfreq_clk_list devfreq_int_clk[CLK_COUNT] = {
	{"aclk_bus1d_400",},
	{"aclk_bus2d_400",},
	{"aclk_bus3d_400",},
	{"aclk_bus4d_400",},
	{"aclk_bus1p_100",},
	{"aclk_bus2p_100",},
	{"aclk_bus3p_100",},
	{"aclk_bus4p_100",},
	{"aclk_g2d_333",},
	{"aclk_mfc_333",},
	{"aclk_gscl_333",},
	{"aclk_gscl_400",},
	{"aclk_gscl_fimc",},
	{"aclk_isp_266",},
	{"aclk_isp_400",},
	{"aclk_fsys_200",},
	{"aclk_peri_66",},
	{"mout_isp1_media_266",},
	{"sclk_mediatop_pll",},
	{"sclk_bustop_pll",},
	{"sclk_memtop_pll",},
	{"fout_bpll",},
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
	{LV0_ISP_BOOST,	533000, 1025000},
	{LV0,		400000,	 975000},
	{LV1,		333000,	 950000},
	{LV2,		266000,	 900000},
	{LV3,		160000,	 875000},
	{LV4,		100000,	 875000},
};

static unsigned int devfreq_int_asv_abb[LV_COUNT];

struct devfreq_clk_state aclk_isp_266_mem_pll[] = {
	{MOUT_ISP1_MEDIA_266,	SCLK_MEMTOP_PLL},
	{ACLK_ISP_266,		MOUT_ISP1_MEDIA_266},
};

struct devfreq_clk_state aclk_isp_266_bus_pll[] = {
	{ACLK_ISP_266,		SCLK_BUSTOP_PLL},
};

struct devfreq_clk_state aclk_gscl_400_bus_pll[] = {
	{ACLK_GSCL_400,		SCLK_BUSTOP_PLL},
};

struct devfreq_clk_state aclk_isp_400_bus_pll[] = {
	{ACLK_ISP_400,		SCLK_BUSTOP_PLL},
};

struct devfreq_clk_states aclk_isp_266_mem_pll_list = {
	.state = aclk_isp_266_mem_pll,
	.state_count = ARRAY_SIZE(aclk_isp_266_mem_pll),
};

struct devfreq_clk_states aclk_isp_266_bus_pll_list = {
	.state = aclk_isp_266_bus_pll,
	.state_count = ARRAY_SIZE(aclk_isp_266_bus_pll),
};

struct devfreq_clk_states aclk_gscl_400_bus_pll_list = {
	.state = aclk_gscl_400_bus_pll,
	.state_count = ARRAY_SIZE(aclk_gscl_400_bus_pll),
};

struct devfreq_clk_states aclk_isp_400_bus_pll_list = {
	.state = aclk_isp_400_bus_pll,
	.state_count = ARRAY_SIZE(aclk_isp_400_bus_pll),
};

struct devfreq_clk_info aclk_bus1d_400[] = {
	{LV0_ISP_BOOST,	400000000,	0,	NULL},
	{LV0,		400000000,	0,	NULL},
	{LV1,		400000000,	0,	NULL},
	{LV2,		267000000,	0,	NULL},
	{LV3,		160000000,	0,	NULL},
	{LV4,		100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_bus2d_400[] = {
	{LV0_ISP_BOOST,	400000000,	0,	NULL},
	{LV0,		400000000,	0,	NULL},
	{LV1,		400000000,	0,	NULL},
	{LV2,		267000000,	0,	NULL},
	{LV3,		160000000,	0,	NULL},
	{LV4,		100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_bus3d_400[] = {
	{LV0_ISP_BOOST,	400000000,	0,	NULL},
	{LV0,		400000000,	0,	NULL},
	{LV1,		400000000,	0,	NULL},
	{LV2,		267000000,	0,	NULL},
	{LV3,		160000000,	0,	NULL},
	{LV4,		100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_bus4d_400[] = {
	{LV0_ISP_BOOST,	400000000,	0,	NULL},
	{LV0,		400000000,	0,	NULL},
	{LV1,		400000000,	0,	NULL},
	{LV2,		267000000,	0,	NULL},
	{LV3,		160000000,	0,	NULL},
	{LV4,		100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_bus1p_100[] = {
	{LV0_ISP_BOOST,	100000000,	0,	NULL},
	{LV0,		100000000,	0,	NULL},
	{LV1,		100000000,	0,	NULL},
	{LV2,		100000000,	0,	NULL},
	{LV3,		 50000000,	0,	NULL},
	{LV4,		 50000000,	0,	NULL},
};

struct devfreq_clk_info aclk_bus2p_100[] = {
	{LV0_ISP_BOOST,	100000000,	0,	NULL},
	{LV0,		100000000,	0,	NULL},
	{LV1,		100000000,	0,	NULL},
	{LV2,		100000000,	0,	NULL},
	{LV3,		 50000000,	0,	NULL},
	{LV4,		 50000000,	0,	NULL},
};

struct devfreq_clk_info aclk_bus3p_100[] = {
	{LV0_ISP_BOOST,	100000000,	0,	NULL},
	{LV0,		100000000,	0,	NULL},
	{LV1,		100000000,	0,	NULL},
	{LV2,		100000000,	0,	NULL},
	{LV3,		 50000000,	0,	NULL},
	{LV4,		 50000000,	0,	NULL},
};

struct devfreq_clk_info aclk_bus4p_100[] = {
	{LV0_ISP_BOOST,	100000000,	0,	NULL},
	{LV0,		100000000,	0,	NULL},
	{LV1,		100000000,	0,	NULL},
	{LV2,		100000000,	0,	NULL},
	{LV3,		 50000000,	0,	NULL},
	{LV4,		 50000000,	0,	NULL},
};

struct devfreq_clk_info aclk_g2d_333[] = {
	{LV0_ISP_BOOST,	334000000,	0,	NULL},
	{LV0,		334000000,	0,	NULL},
	{LV1,		334000000,	0,	NULL},
	{LV2,		223000000,	0,	NULL},
	{LV3,		167000000,	0,	NULL},
	{LV4,		134000000,	0,	NULL},
};

struct devfreq_clk_info aclk_mfc_333[] = {
	{LV0_ISP_BOOST,	334000000,	0,	NULL},
	{LV0,		334000000,	0,	NULL},
	{LV1,		334000000,	0,	NULL},
	{LV2,		223000000,	0,	NULL},
	{LV3,		167000000,	0,	NULL},
	{LV4,		134000000,	0,	NULL},
};

struct devfreq_clk_info aclk_gscl_333[] = {
	{LV0_ISP_BOOST,	334000000,	0,	NULL},
	{LV0,		334000000,	0,	NULL},
	{LV1,		334000000,	0,	NULL},
	{LV2,		223000000,	0,	NULL},
	{LV3,		167000000,	0,	NULL},
	{LV4,		134000000,	0,	NULL},
};

struct devfreq_clk_info aclk_gscl_400[] = {
	{LV0_ISP_BOOST,	400000000,	0,	&aclk_gscl_400_bus_pll_list},
	{LV0,		400000000,	0,	&aclk_gscl_400_bus_pll_list},
	{LV1,		400000000,	0,	&aclk_gscl_400_bus_pll_list},
	{LV2,		267000000,	0,	&aclk_gscl_400_bus_pll_list},
	{LV3,		160000000,	0,	&aclk_gscl_400_bus_pll_list},
	{LV4,		100000000,	0,	&aclk_gscl_400_bus_pll_list},
};

struct devfreq_clk_info aclk_gscl_fimc[] = {
	{LV0_ISP_BOOST,	334000000,	0,	NULL},
	{LV0,		334000000,	0,	NULL},
	{LV1,		334000000,	0,	NULL},
	{LV2,		223000000,	0,	NULL},
	{LV3,		167000000,	0,	NULL},
	{LV4,		134000000,	0,	NULL},
};

struct devfreq_clk_info mem_pll[] = {
	{LV0_ISP_BOOST,	0,		0,	NULL},
	{LV0,		300000000,	0,	NULL},
	{LV1,		0,		0,	NULL},
	{LV2,		0,		0,	NULL},
	{LV3,		0,		0,	NULL},
	{LV4,		0,		0,	NULL},
};

struct devfreq_clk_info aclk_isp_266_pre[] = {
	{LV0_ISP_BOOST,	150000000,	0,	NULL},
	{LV0,		0,		0,	NULL},
	{LV1,		0,		0,	NULL},
	{LV2,		0,		0,	NULL},
	{LV3,		0,		0,	NULL},
	{LV4,		0,		0,	NULL},
};

struct devfreq_clk_info aclk_isp_266[] = {
	{LV0_ISP_BOOST,	400000000,	0,	&aclk_isp_266_bus_pll_list},
	{LV0,		300000000,	0,	&aclk_isp_266_mem_pll_list},
	{LV1,		134000000,	0,	&aclk_isp_266_bus_pll_list},
	{LV2,		100000000,	0,	&aclk_isp_266_bus_pll_list},
	{LV3,		100000000,	0,	&aclk_isp_266_bus_pll_list},
	{LV4,		100000000,	0,	&aclk_isp_266_bus_pll_list},
};

struct devfreq_clk_info aclk_isp_400[] = {
	{LV0_ISP_BOOST,	400000000,	0,	&aclk_isp_400_bus_pll_list},
	{LV0,		400000000,	0,	&aclk_isp_400_bus_pll_list},
	{LV1,		400000000,	0,	&aclk_isp_400_bus_pll_list},
	{LV2,		400000000,	0,	&aclk_isp_400_bus_pll_list},
	{LV3,		160000000,	0,	&aclk_isp_400_bus_pll_list},
	{LV4,		100000000,	0,	&aclk_isp_400_bus_pll_list},
};

struct devfreq_clk_info aclk_fsys_200[] = {
	{LV0_ISP_BOOST,	200000000,	0,	NULL},
	{LV0,		200000000,	0,	NULL},
	{LV1,		200000000,	0,	NULL},
	{LV2,		160000000,	0,	NULL},
	{LV3,		134000000,	0,	NULL},
	{LV4,		100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_peri_66[] = {
	{LV0_ISP_BOOST,	 67000000,	0,	NULL},
	{LV0,		 67000000,	0,	NULL},
	{LV1,		 67000000,	0,	NULL},
	{LV2,		 67000000,	0,	NULL},
	{LV3,		 67000000,	0,	NULL},
	{LV4,		 67000000,	0,	NULL},
};

struct devfreq_clk_info *devfreq_clk_int_info_list[] = {
	aclk_bus1d_400,
	aclk_bus2d_400,
	aclk_bus3d_400,
	aclk_bus4d_400,
	aclk_bus1p_100,
	aclk_bus2p_100,
	aclk_bus3p_100,
	aclk_bus4p_100,
	aclk_g2d_333,
	aclk_mfc_333,
	aclk_gscl_333,
	aclk_gscl_400,
	aclk_gscl_fimc,
	mem_pll,
	aclk_isp_266_pre,
	aclk_isp_266,
	aclk_isp_400,
	aclk_fsys_200,
	aclk_peri_66
};

enum devfreq_int_clk devfreq_clk_int_info_idx[] = {
	ACLK_BUS1D_400,
	ACLK_BUS2D_400,
	ACLK_BUS3D_400,
	ACLK_BUS4D_400,
	PCLK_BUS1P_100,
	PCLK_BUS2P_100,
	PCLK_BUS3P_100,
	PCLK_BUS4P_100,
	ACLK_G2D_333,
	ACLK_MFC_333,
	ACLK_GSCL_333,
	ACLK_GSCL_400,
	ACLK_GSCL_FIMC,
	FOUT_BPLL,
	ACLK_ISP_266,
	ACLK_ISP_266,
	ACLK_ISP_400,
	ACLK_FSYS_200,
	ACLK_PERI_66,
};

static struct devfreq_simple_ondemand_data exynos5_devfreq_int_governor_data = {
	.pm_qos_class		= PM_QOS_DEVICE_THROUGHPUT,
	.upthreshold		= 95,
	.cal_qos_max		= 333000,
};

static struct exynos_devfreq_platdata exynos5260_qos_int = {
	.default_qos		= 100000,
};

static struct ppmu_info ppmu_int[] = {
	{
		.base = (void __iomem *)PPMU_DREX0_S0_ADDR,
	}, {
		.base = (void __iomem *)PPMU_DREX1_S0_ADDR,
	},
};

static struct devfreq_exynos devfreq_int_exynos = {
	.ppmu_list = ppmu_int,
	.ppmu_count = ARRAY_SIZE(ppmu_int),
};

static struct pm_qos_request exynos5_int_qos;
static struct pm_qos_request boot_int_qos;
static struct pm_qos_request min_int_thermal_qos;
static struct pm_qos_request exynos5_int_bts_qos;
struct devfreq_data_int *data_int;

static struct pm_qos_request exynos5_mif_qos;

static ssize_t int_show_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t len = 0;
	ssize_t cnt_remain = (ssize_t)(PAGE_SIZE - 1);

	for (i = LV0_ISP_BOOST; i < LV_COUNT; ++i) {
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

	for (i = LV_COUNT - 1; i >= 0; --i) {
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

static struct attribute_group devfreq_int_attr_group = {
	.name	= "time_in_state",
	.attrs	= devfreq_int_sysfs_entries,
};

void exynos5_update_media_layer_int(int int_qos)
{
	if (pm_qos_request_active(&exynos5_int_bts_qos))
		pm_qos_update_request(&exynos5_int_bts_qos, devfreq_int_opp_list[int_qos].freq);
}

static inline int exynos5_devfreq_int_get_idx(struct devfreq_opp_table *table,
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

static void exynos5_devfreq_int_update_time(struct devfreq_data_int *data,
						int target_idx)
{
	cputime64_t cur_time = get_jiffies_64();
	cputime64_t diff_time = cur_time - data->last_jiffies;

	devfreq_int_time_in_state[target_idx] += diff_time;
	data->last_jiffies = cur_time;
}

static int exynos5_devfreq_int_set_freq(struct devfreq_data_int *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
	sec_debug_aux_log(SEC_DEBUG_AUXLOG_CPU_BUS_CLOCK_CHANGE,
		"old:%7d new:%7d (INT)",
	old_idx,target_idx );
#endif
	if (pm_qos_request_active(&exynos5_mif_qos)) {
		if (target_idx <= LV0) {
			pm_qos_update_request(&exynos5_mif_qos, 667000);
		} else {
			pm_qos_update_request(&exynos5_mif_qos, 0);
		}
	}

	if (target_idx < old_idx) {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_int_info_list); ++i) {
			clk_info = &devfreq_clk_int_info_list[i][target_idx];
			clk_states = clk_info->states;

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_int_clk[clk_states->state[j].clk_idx].clk,
						devfreq_int_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_int_info_list); ++i) {
			clk_info = &devfreq_clk_int_info_list[i][target_idx];
			clk_states = clk_info->states;

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_int_clk[clk_states->state[j].clk_idx].clk,
						devfreq_int_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);
		}
	}

	return 0;
}

static int exynos5_devfreq_int_set_volt(struct devfreq_data_int *data,
					unsigned long volt,
					unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	regulator_set_voltage(data->vdd_int, volt, volt_range);
	data->old_volt = volt;
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

static int exynos5_devfreq_int_target(struct device *dev,
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
#ifdef CONFIG_EXYNOS_THERMAL
	target_volt = get_limit_voltage(target_volt, int_data->volt_offset);
#endif
	rcu_read_unlock();

	target_idx = exynos5_devfreq_int_get_idx(devfreq_int_opp_list,
						ARRAY_SIZE(devfreq_int_opp_list),
						*target_freq);
	old_idx = exynos5_devfreq_int_get_idx(devfreq_int_opp_list,
						ARRAY_SIZE(devfreq_int_opp_list),
						devfreq_int->previous_freq);
	old_freq = devfreq_int->previous_freq;

	if (target_idx < 0)
		goto out;

	if (old_freq == *target_freq)
		goto out;

	if (old_freq < *target_freq) {
		exynos5_devfreq_int_set_volt(int_data, target_volt, target_volt + VOLT_STEP);
		set_match_abb(ID_INT, devfreq_int_asv_abb[target_idx]);
		exynos5_devfreq_int_set_freq(int_data, target_idx, old_idx);
	} else {
		exynos5_devfreq_int_set_freq(int_data, target_idx, old_idx);
		set_match_abb(ID_INT, devfreq_int_asv_abb[target_idx]);
		exynos5_devfreq_int_set_volt(int_data, target_volt, target_volt + VOLT_STEP);
	}
out:
	exynos5_devfreq_int_update_time(int_data, target_idx);

	mutex_unlock(&int_data->lock);
	return ret;
}

static int exynos5_devfreq_int_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_int *data = dev_get_drvdata(dev);

	if (!data_int->use_dvfs)
		return -EAGAIN;

	stat->current_frequency = data->devfreq->previous_freq;
	stat->busy_time = devfreq_int_exynos.val_pmcnt;
	stat->total_time = devfreq_int_exynos.val_ccnt;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_int_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_int_target,
	.get_dev_status	= exynos5_devfreq_int_get_dev_status,
};

static int exynos5_devfreq_int_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_int_clk); ++i) {
		devfreq_int_clk[i].clk = clk_get(NULL, devfreq_int_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_int_clk[i].clk)) {
			pr_err("DEVFREQ(INT) : %s can't get clock\n", devfreq_int_clk[i].clk_name);
			return -EINVAL;
		}
	}

	return 0;
}

static int exynos5_init_int_table(struct device *dev)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;
	unsigned int asv_abb = 0;

	for (i = 0; i < ARRAY_SIZE(devfreq_int_opp_list); ++i) {
		freq = devfreq_int_opp_list[i].freq;
		volt = get_match_volt(ID_INT, freq);
		if (!volt)
			volt = devfreq_int_opp_list[i].volt;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(INT) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(INT) : %uKhz, %uV\n", freq, volt);
		}

		asv_abb = get_match_abb(ID_INT, freq);
		if (!asv_abb)
			devfreq_int_asv_abb[i] = ABB_BYPASS;
		else
			devfreq_int_asv_abb[i] = asv_abb;

		pr_info("DEVFREQ(INT) : %uKhz, ABB %u\n", freq, devfreq_int_asv_abb[i]);
	}

	return 0;
}

static int exynos5_devfreq_int_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	struct devfreq_notifier_block *devfreq_nb;

	devfreq_nb = container_of(nb, struct devfreq_notifier_block, nb);

	mutex_lock(&devfreq_nb->df->lock);
	update_devfreq(devfreq_nb->df);
	mutex_unlock(&devfreq_nb->df->lock);

	return NOTIFY_OK;
}

static int exynos5_devfreq_int_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	pm_qos_update_request(&boot_int_qos, exynos5_devfreq_int_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_int_reboot_notifier = {
	.notifier_call = exynos5_devfreq_int_reboot_notifier,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos5_devfreq_int_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_int *data = container_of(nb, struct devfreq_data_int, tmu_notifier);
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		if (pm_qos_request_active(&min_int_thermal_qos))
			pm_qos_update_request(&min_int_thermal_qos,
					exynos5_devfreq_int_profile.initial_freq);

		if (*on) {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_int);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt, data->volt_offset);
			regulator_set_voltage(data->vdd_int, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		} else {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_int);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt - COLD_VOLT_OFFSET, data->volt_offset);
			regulator_set_voltage(data->vdd_int, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&min_int_thermal_qos))
			pm_qos_update_request(&min_int_thermal_qos,
					exynos5260_qos_int.default_qos);
	}

	return NOTIFY_OK;
}
#endif

static int exynos5_devfreq_int_probe(struct platform_device *pdev)
{
	int ret = 0;
	int index;
	struct devfreq_data_int *data;
	struct devfreq_notifier_block *devfreq_nb;
	struct exynos_devfreq_platdata *plat_data;
	struct opp *target_opp;
        unsigned long freq;

	if (exynos5_devfreq_int_init_clock()) {
		ret = -EINVAL;
		goto err_data;
	}

	data = kzalloc(sizeof(struct devfreq_data_int), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	data_int = data;
	data->use_dvfs = false;

	ret = exynos5_init_int_table(&pdev->dev);
	if (ret)
		goto err_inittable;

	mutex_init(&data->lock);
	platform_set_drvdata(pdev, data);

	data->last_jiffies = get_jiffies_64();
	/* For low temperature compensation when boot time */
	data->volt_offset = COLD_VOLT_OFFSET;
	data->dev = &pdev->dev;
	data->vdd_int = regulator_get(NULL, "vdd_int");

	rcu_read_lock();
	freq = exynos5_devfreq_int_profile.initial_freq;
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
	regulator_set_voltage(data->vdd_int, data->old_volt, data->old_volt + VOLT_STEP);
	index = exynos5_devfreq_int_get_idx(devfreq_int_opp_list,
			ARRAY_SIZE(devfreq_int_opp_list),
			freq);
	if (index < 0) {
		pr_err("DEVFREQ(MIF) : Failed to find index abb\n");
		ret = -EINVAL;
		goto err_nb;
	}
	set_match_abb(ID_INT, devfreq_int_asv_abb[index]);

	data->devfreq = devfreq_add_device(data->dev,
						&exynos5_devfreq_int_profile,
						&devfreq_simple_ondemand,
						&exynos5_devfreq_int_governor_data);

	devfreq_nb = kzalloc(sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (devfreq_nb == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}

	devfreq_nb->df = data->devfreq;
	devfreq_nb->nb.notifier_call = exynos5_devfreq_int_notifier;

	exynos5260_devfreq_register(&devfreq_int_exynos);
	exynos5260_ppmu_register_notifier(INT, &devfreq_nb->nb);

	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = devfreq_int_opp_list[LV0_ISP_BOOST].freq;
	pm_qos_add_request(&exynos5_mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&exynos5_int_qos, PM_QOS_DEVICE_THROUGHPUT, plat_data->default_qos);
	pm_qos_add_request(&min_int_thermal_qos, PM_QOS_DEVICE_THROUGHPUT, plat_data->default_qos);
	pm_qos_add_request(&boot_int_qos, PM_QOS_DEVICE_THROUGHPUT, plat_data->default_qos);
	pm_qos_add_request(&exynos5_int_bts_qos, PM_QOS_DEVICE_THROUGHPUT, 0);

	register_reboot_notifier(&exynos5_int_reboot_notifier);

	ret = sysfs_create_group(&data->devfreq->dev.kobj, &devfreq_int_attr_group);
	if (ret) {
		pr_err("DEVFREQ(INT) : Failed create time_in_state sysfs\n");
		goto err_nb;
	}

	ret = device_create_file(&data->devfreq->dev, &dev_attr_available_frequencies);
	if (ret) {
		pr_err("DEVFREQ(INT) : Failed create available_frequencies sysfs\n");
		goto err_nb;
	}

#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos5_devfreq_int_tmu_notifier;
	exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data_int->use_dvfs = true;

	return ret;
err_nb:
	devfreq_remove_device(data->devfreq);
err_inittable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_int_remove(struct platform_device *pdev)
{
	struct devfreq_data_int *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_int_thermal_qos);
	pm_qos_remove_request(&exynos5_int_qos);
	pm_qos_remove_request(&boot_int_qos);
	pm_qos_remove_request(&exynos5_int_bts_qos);

	regulator_put(data->vdd_int);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_int_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos5_int_qos))
		pm_qos_update_request(&exynos5_int_qos, exynos5_devfreq_int_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_int_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos5_int_qos))
		pm_qos_update_request(&exynos5_int_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_int_pm = {
	.suspend	= exynos5_devfreq_int_suspend,
	.resume		= exynos5_devfreq_int_resume,
};

static struct platform_driver exynos5_devfreq_int_driver = {
	.probe	= exynos5_devfreq_int_probe,
	.remove	= exynos5_devfreq_int_remove,
	.driver	= {
		.name	= "exynos5-devfreq-int",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_int_pm,
	},
};

static struct platform_device exynos5_devfreq_int_device = {
	.name	= "exynos5-devfreq-int",
	.id	= -1,
};

static int __init exynos5_devfreq_int_init(void)
{
	int ret;

	exynos5_devfreq_int_device.dev.platform_data = &exynos5260_qos_int;

	ret = platform_device_register(&exynos5_devfreq_int_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_int_driver);
}
late_initcall(exynos5_devfreq_int_init);

static void __exit exynos5_devfreq_int_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_int_driver);
	platform_device_unregister(&exynos5_devfreq_int_device);
}
module_exit(exynos5_devfreq_int_exit);
