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
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/sched.h>

#include <mach/devfreq.h>
#include <mach/asv-exynos.h>
#include <mach/tmu.h>

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
#include <mach/sec_debug.h>
#endif

#include "exynos5260_ppmu.h"
#include "exynos_ppmu2.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(667000)
#define DEVFREQ_POLLING_PERIOD	(0)

#define MIF_VOLT_STEP		(12500)
#define COLD_VOLT_OFFSET	(50000)
#define LIMIT_COLD_VOLTAGE	(1250000)
#define MIN_COLD_VOLTAGE	(950000)

#define TIMING_RFCPB_MASK	(0x3F)

#define DLL_ON_BASE_VOLT	(900000)

#define MRSTATUS_THERMAL_BIT_SHIFT      (7)
#define MRSTATUS_THERMAL_BIT_MASK       (1)
#define MRSTATUS_THERMAL_LV_MASK        (0x7)

enum devfreq_mif_idx {
	LV0,
	LV1,
	LV2,
	LV3,
	LV4,
	LV5,
	LV6,
	LV7,
	LV_COUNT,
};

enum devfreq_mif_clk {
	ACLK_CLK2X_PHY,
	ACLK_MIF_466,
	ACLK_BUS_100,
	ACLK_BUS_200,
	SCLK_BPLL,
	SCLK_MPLL,
	SCLK_CPLL,
	MOUT_MIF_DREX2X,
	FOUT_BPLL,
	CLK_COUNT,
};

enum devfreq_mif_thermal_autorate {
	RATE_ONE = 0x0000005D,
	RATE_HALF = 0x0000002E,
	RATE_QUARTER = 0x00000017,
};

enum devfreq_mif_thermal_channel {
	THERMAL_CHANNEL0,
	THERMAL_CHANNEL1,
};

struct devfreq_data_mif {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_mif;
	unsigned long old_volt;
	unsigned long volt_offset;

	void __iomem *base_drex0;
	void __iomem *base_drex1;
	void __iomem *base_ddrphy0;
	void __iomem *base_ddrphy1;
	void __iomem *base_mif;
	cputime64_t last_jiffies;

	bool use_derate_timing_row;
	bool use_dvfs;
	bool dynamic_self_refresh_enabled;

	struct notifier_block tmu_notifier;

	struct mutex lock;
};

struct devfreq_thermal_work {
	struct delayed_work devfreq_mif_thermal_work;
	enum devfreq_mif_thermal_channel channel;
	struct workqueue_struct *work_queue;
	unsigned int polling_period;
	unsigned long max_freq;
};

struct devfreq_clk_list devfreq_mif_clk[CLK_COUNT] = {
	{"aclk_clk2x_phy",},
	{"aclk_mif_466",},
	{"aclk_bus_100",},
	{"aclk_bus_200",},
	{"sclk_bpll",},
	{"sclk_mpll",},
	{"sclk_cpll",},
	{"mout_mif_drex2x",},
	{"fout_bpll",},
};

cputime64_t devfreq_mif_time_in_state[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};

struct devfreq_opp_table devfreq_mif_opp_list[] = {
	{LV0,	667000,	1000000},
	{LV1,	543000,	1000000},
	{LV2,	413000,	 925000},
	{LV3,	275000,	 875000},
	{LV4,	206000,	 875000},
	{LV5,	165000,	 875000},
	{LV6,	138000,	 875000},
	{LV7,	103000,	 875000},
};

static unsigned int devfreq_mif_asv_abb[LV_COUNT];

struct devfreq_clk_state aclk_clk2x_phy_mem_pll[] = {
	{ACLK_CLK2X_PHY,	MOUT_MIF_DREX2X},
};

struct devfreq_clk_state aclk_clk2x_phy_media_pll[] = {
	{ACLK_CLK2X_PHY,	SCLK_CPLL},
};

struct devfreq_clk_states aclk_clk2x_phy_mem_pll_list = {
	.state = aclk_clk2x_phy_mem_pll,
	.state_count = ARRAY_SIZE(aclk_clk2x_phy_mem_pll),
};

struct devfreq_clk_states aclk_clk2x_phy_media_pll_list = {
	.state = aclk_clk2x_phy_media_pll,
	.state_count = ARRAY_SIZE(aclk_clk2x_phy_media_pll),
};

struct devfreq_clk_info mem_pll_rate[] = {
	{LV0,	667000000,	0,	NULL},
	{LV1,	543000000,	0,	NULL},
	{LV2,	413000000,	0,	NULL},
	{LV3,	275000000,	0,	NULL},
	{LV4,	206000000,	0,	NULL},
	{LV5,	165000000,	0,	NULL},
	{LV6,	138000000,	0,	NULL},
	{LV7,	103000000,	0,	NULL},
};

struct devfreq_clk_info aclk_clk2x_phy[] = {
	{LV0,	667000000,	0,	&aclk_clk2x_phy_media_pll_list},
	{LV1,	543000000,	0,	&aclk_clk2x_phy_mem_pll_list},
	{LV2,	413000000,	0,	&aclk_clk2x_phy_mem_pll_list},
	{LV3,	275000000,	0,	&aclk_clk2x_phy_mem_pll_list},
	{LV4,	206000000,	0,	&aclk_clk2x_phy_mem_pll_list},
	{LV5,	165000000,	0,	&aclk_clk2x_phy_mem_pll_list},
	{LV6,	138000000,	0,	&aclk_clk2x_phy_mem_pll_list},
	{LV7,	103000000,	0,	&aclk_clk2x_phy_mem_pll_list},
};

struct devfreq_clk_info aclk_mif_466[] = {
	{LV0,	334000000,	0,	NULL},
	{LV1,	272000000,	0,	NULL},
	{LV2,	207000000,	0,	NULL},
	{LV3,	138000000,	0,	NULL},
	{LV4,	103000000,	0,	NULL},
	{LV5,	 83000000,	0,	NULL},
	{LV6,	 69000000,	0,	NULL},
	{LV7,	 52000000,	0,	NULL},
};

struct devfreq_clk_info aclk_bus_100[] = {
	{LV0,	100000000,	0,	NULL},
	{LV1,	100000000,	0,	NULL},
	{LV2,	 50000000,	0,	NULL},
	{LV3,	 50000000,	0,	NULL},
	{LV4,	 50000000,	0,	NULL},
	{LV5,	 50000000,	0,	NULL},
	{LV6,	 50000000,	0,	NULL},
	{LV7,	 50000000,	0,	NULL},
};

struct devfreq_clk_info aclk_bus_200[] = {
	{LV0,	200000000,	0,	NULL},
	{LV1,	200000000,	0,	NULL},
	{LV2,	100000000,	0,	NULL},
	{LV3,	100000000,	0,	NULL},
	{LV4,	100000000,	0,	NULL},
	{LV5,	100000000,	0,	NULL},
	{LV6,	100000000,	0,	NULL},
	{LV7,	100000000,	0,	NULL},
};

struct devfreq_clk_info *devfreq_clk_mif_info_list[] = {
	aclk_bus_100,
	aclk_bus_200,
};

enum devfreq_mif_clk devfreq_clk_mif_info_idx[] = {
	ACLK_BUS_100,
	ACLK_BUS_200,
};

static struct devfreq_simple_ondemand_data exynos5_devfreq_mif_governor_data = {
	.pm_qos_class		= PM_QOS_BUS_THROUGHPUT,
	.upthreshold		= 95,
	.cal_qos_max		= 667000,
};

static struct exynos_devfreq_platdata exynos5260_qos_mif = {
	.default_qos		= 138000,
};

static struct ppmu_info ppmu_mif[] = {
	{
		.base = (void __iomem *)PPMU_DREX0_S0_ADDR,
	}, {
		.base = (void __iomem *)PPMU_DREX0_S1_ADDR,
	}, {
		.base = (void __iomem *)PPMU_DREX1_S0_ADDR,
	}, {
		.base = (void __iomem *)PPMU_DREX1_S1_ADDR,
	},
};

static struct devfreq_exynos devfreq_mif_exynos = {
	.ppmu_list = ppmu_mif,
	.ppmu_count = ARRAY_SIZE(ppmu_mif),
};

struct timing_parameter {
	unsigned int timing_row;
	unsigned int timing_data;
	unsigned int timing_power;
};

static struct timing_parameter mif_timing_parameter[] = {
	{ /* 667Mhz */
		.timing_row		= 0x2C4885D0,
		.timing_data		= 0x3630064A,
		.timing_power		= 0x442F0335,
	}, { /* 543Mhz */
		.timing_row		= 0x244764CD,
		.timing_data		= 0x3530064A,
		.timing_power		= 0x38270335,
	}, { /* 413Mhz */
		.timing_row		= 0x1B35538A,
		.timing_data		= 0x2420063A,
		.timing_power		= 0x2C1D0225,
	}, { /* 275Mhz */
		.timing_row		= 0x12244287,
		.timing_data		= 0x2320062A,
		.timing_power		= 0x1C140225,
	}, { /* 206Mhz */
		.timing_row		= 0x112331C6,
		.timing_data		= 0x2320062A,
		.timing_power		= 0x180F0225,
	}, { /* 165Mhz */
		.timing_row		= 0x11223185,
		.timing_data		= 0x2320062A,
		.timing_power		= 0x140F0225,
	}, { /* 138Mhz */
		.timing_row		= 0x11222144,
		.timing_data		= 0x2320062A,
		.timing_power		= 0x100F0225,
	}, { /* 103Mhz */
		.timing_row		= 0x11222103,
		.timing_data		= 0x2320062A,
		.timing_power		= 0x100F0225,
	}
};

/* Fimc status, Fimd layer count, Mixer layer count */
unsigned int media_lock_wqxga[2][3][3] = {
	{ /* Fimc disable */
		{LV5,	LV2,	LV2},
		{LV3,	LV2,	LV1},
		{LV1,	LV1,	LV0},
	}, { /* Fimc enable */
		{LV1,	LV1,	LV1},
		{LV1,	LV1,	LV0},
		{LV0,	LV0,	LV0},
	}
};

/* Fimc status, Fimd layer count, Mixer layer count */
unsigned int media_lock_int_wqxga[2][3][3] = {
	{ /* Fimc disable */
		{LV4,	LV2,	LV2},
		{LV4,	LV2,	LV2},
		{LV4,	LV2,	LV2},
	}, { /* Fimc enable */
		{LV0,	LV0,	LV0},
		{LV0,	LV0,	LV0},
		{LV0,	LV0,	LV0},
	}
};

/* Fimc status, Fimd layer count, Mixer layer count */
unsigned int media_lock_fullhd[2][5][3] = {
	{ /* Fimc disable */
		{LV7,	LV2,	LV2},
		{LV7,	LV2,	LV2},
		{LV3,	LV2,	LV2},
		{LV3,	LV2,	LV2},
		{LV3,	LV2,	LV2},
	}, { /* Fimc enable */
		{LV2,	LV1,	LV1},
		{LV2,	LV1,	LV1},
		{LV2,	LV1,	LV0},
		{LV1,	LV1,	LV0},
		{LV1,	LV1,	LV0},
	}
};

/* Fimc status, Mixer layer count */
unsigned int media_lock_int_fullhd[2][3] = {
	{ /* Fimc disable */
		LV5,	LV2,	LV2,
	}, {
		LV1,	LV1,	LV1,
	}
};

static struct workqueue_struct *devfreq_mif_thermal_wq_ch0;
static struct workqueue_struct *devfreq_mif_thermal_wq_ch1;
static struct devfreq_thermal_work devfreq_mif_ch0_work = {
        .channel = THERMAL_CHANNEL0,
        .polling_period = 1000,
};
static struct devfreq_thermal_work devfreq_mif_ch1_work = {
        .channel = THERMAL_CHANNEL1,
        .polling_period = 1000,
};

static struct pm_qos_request exynos5_mif_qos;
static struct pm_qos_request boot_mif_qos;
static struct pm_qos_request min_mif_thermal_qos;
static struct pm_qos_request exynos5_mif_bts_qos;

static bool use_timing_set_0;

static DEFINE_MUTEX(media_mutex);
static unsigned int media_enabled_fimc_lite;
static unsigned int media_enabled_gscl_local;
static unsigned int media_enabled_tv;
static unsigned int media_num_mixer_layer;
static unsigned int media_num_fimd_layer;
static enum devfreq_media_resolution media_resolution;
static struct devfreq_data_mif *data_mif;

extern unsigned int system_rev;

extern void exynos5_update_media_layer_int(int int_qos);

int exynos_mif_cur_level;

static ssize_t mif_show_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t len = 0;
	ssize_t cnt_remain = (ssize_t)(PAGE_SIZE - 1);

	for (i = LV0; i < LV_COUNT; ++i) {
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

	for (i = LV_COUNT - 1; i >= 0; --i) {
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

void exynos5_update_media_layers(enum devfreq_media_type media_type, unsigned int value)
{
	int mif_qos = LV7;
	int int_qos = LV4;

	mutex_lock(&media_mutex);

	switch(media_type) {
	case TYPE_FIMC_LITE:
		media_enabled_fimc_lite = (value == 0 ? 0 : 1);
		break;
	case TYPE_MIXER:
		media_num_mixer_layer = value;
		break;
	case TYPE_FIMD1:
		media_num_fimd_layer = value;
		break;
	case TYPE_TV:
		media_enabled_tv = (value == 0 ? 0 : 1);
		break;
	case TYPE_GSCL_LOCAL:
		media_enabled_gscl_local = (value == 0 ? 0 : 1);
		break;
	case TYPE_RESOLUTION:
		media_resolution = value;
		break;
	}

	if (media_resolution == RESOLUTION_WQXGA) {
		if (2 < media_num_mixer_layer ||
			3 < media_num_fimd_layer) {
			pr_err("DEVFREQ(MIF) : can't support media layer with WQXGA mixer:%u, fimd:%u\n",
				media_num_mixer_layer, media_num_fimd_layer);
			return;
		}

		media_num_fimd_layer--;
		mif_qos = media_lock_wqxga[media_enabled_fimc_lite][media_num_fimd_layer][media_num_mixer_layer];
		int_qos = media_lock_int_wqxga[media_enabled_fimc_lite][media_num_fimd_layer][media_num_mixer_layer];
	} else if (media_resolution == RESOLUTION_FULLHD) {
		if (2 < media_num_mixer_layer ||
			4 < media_num_fimd_layer) {
			pr_err("DEVFREQ(MIF) : can't support media layer with FULLHD mixer:%u, fimd:%u\n",
				media_num_mixer_layer, media_num_fimd_layer);
			return;
		}

		mif_qos = media_lock_fullhd[media_enabled_fimc_lite][media_num_fimd_layer][media_num_mixer_layer];
		int_qos = media_lock_int_fullhd[media_enabled_fimc_lite][media_num_mixer_layer];
	}

	if (pm_qos_request_active(&exynos5_mif_qos))
		pm_qos_update_request(&exynos5_mif_bts_qos, devfreq_mif_opp_list[mif_qos].freq);

	exynos5_update_media_layer_int(int_qos);

	mutex_unlock(&media_mutex);
}

static inline int exynos5_devfreq_mif_get_idx(struct devfreq_opp_table *table,
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

static int exynos5_devfreq_update_timing_set(struct devfreq_data_mif *data)
{
	use_timing_set_0 = (((__raw_readl(data->base_mif + 0x1000) >> 31) & 0x1) == 0);

	return 0;
}

static int exynos5_devfreq_mif_timing_parameter(struct devfreq_data_mif *data,
						int target_idx)
{
	struct timing_parameter *cur_timing_parameter;

	cur_timing_parameter = &mif_timing_parameter[target_idx];

	if (use_timing_set_0) {
		__raw_writel(cur_timing_parameter->timing_row, data->base_drex0 + 0xE4);
		__raw_writel(cur_timing_parameter->timing_data, data->base_drex0 + 0xE8);
		__raw_writel(cur_timing_parameter->timing_power, data->base_drex0 + 0xEC);
		__raw_writel(cur_timing_parameter->timing_row, data->base_drex1 + 0xE4);
		__raw_writel(cur_timing_parameter->timing_data, data->base_drex1 + 0xE8);
		__raw_writel(cur_timing_parameter->timing_power, data->base_drex1 + 0xEC);
	} else {
		__raw_writel(cur_timing_parameter->timing_row, data->base_drex0 + 0x34);
		__raw_writel(cur_timing_parameter->timing_data, data->base_drex0 + 0x38);
		__raw_writel(cur_timing_parameter->timing_power, data->base_drex0 + 0x3C);
		__raw_writel(cur_timing_parameter->timing_row, data->base_drex1 + 0x34);
		__raw_writel(cur_timing_parameter->timing_data, data->base_drex1 + 0x38);
		__raw_writel(cur_timing_parameter->timing_power, data->base_drex1 + 0x3C);
	}

	return 0;
}

static int exynos5_devfreq_mif_change_timing_set(struct devfreq_data_mif *data)
{
	unsigned int tmp;

	if (use_timing_set_0) {
		tmp = __raw_readl(data->base_mif + 0x1000);
		tmp |= (1 << 31);
		__raw_writel(tmp, data->base_mif + 0x1000);
	} else {
		tmp = __raw_readl(data->base_mif + 0x1000);
		tmp &= ~(1 << 31);
		__raw_writel(tmp, data->base_mif + 0x1000);
	}

	use_timing_set_0 = !use_timing_set_0;

	return 0;
}

static int exynos5_devfreq_mif_set_back_pressure(struct devfreq_data_mif *data,
						int target_idx)
{
	bool set_backpressure = false;

	/* if MIF <= 400MHz, DREX Back Pressure Enable && BRB enable */
	if (target_idx >= LV2)
		set_backpressure = true;
	else
		set_backpressure = false;

	if (set_backpressure) {
		__raw_writel(0x1, data->base_drex0 + 0x210);
		__raw_writel(0x1, data->base_drex0 + 0x220);
		__raw_writel(0x1, data->base_drex0 + 0x230);
		__raw_writel(0x1, data->base_drex0 + 0x240);
		__raw_writel(0x33, data->base_drex0 + 0x100);
		__raw_writel(0x1, data->base_drex1 + 0x210);
		__raw_writel(0x1, data->base_drex1 + 0x220);
		__raw_writel(0x1, data->base_drex1 + 0x230);
		__raw_writel(0x1, data->base_drex1 + 0x240);
		__raw_writel(0x33, data->base_drex1 + 0x100);
	} else {
		__raw_writel(0x0, data->base_drex0 + 0x210);
		__raw_writel(0x0, data->base_drex0 + 0x220);
		__raw_writel(0x0, data->base_drex0 + 0x230);
		__raw_writel(0x0, data->base_drex0 + 0x240);
		__raw_writel(0x0, data->base_drex0 + 0x100);
		__raw_writel(0x0, data->base_drex1 + 0x210);
		__raw_writel(0x0, data->base_drex1 + 0x220);
		__raw_writel(0x0, data->base_drex1 + 0x230);
		__raw_writel(0x0, data->base_drex1 + 0x240);
		__raw_writel(0x0, data->base_drex1 + 0x100);
	}

	return 0;
}

static int exynos5_devfreq_mif_set_timeout(struct devfreq_data_mif *data,
					int target_idx)
{
	if (media_resolution == RESOLUTION_FULLHD) {
		if (target_idx >= LV3) {
			__raw_writel(0x00800080, data->base_drex0 + 0xA0);
			__raw_writel(0x00800080, data->base_drex1 + 0xA0);
		} else {
			__raw_writel(0x0FFF0FFF, data->base_drex0 + 0xA0);
			__raw_writel(0x0FFF0FFF, data->base_drex1 + 0xA0);
		}
	} else {
		if (target_idx >= LV4) {
			__raw_writel(0, data->base_drex0 + 0xA0);
			__raw_writel(0, data->base_drex1 + 0xA0);
		} else {
			__raw_writel(0x80, data->base_drex0 + 0xA0);
			__raw_writel(0x80, data->base_drex1 + 0xA0);
		}
	}

	return 0;
}

static int exynos5_devfreq_mif_clk_change_parent(struct devfreq_clk_states *clk_states)
{
	int j;

	if (clk_states == NULL)
		return 0;

	for (j = 0; j < clk_states->state_count; ++j) {
		clk_set_parent(devfreq_mif_clk[clk_states->state[j].clk_idx].clk,
			devfreq_mif_clk[clk_states->state[j].parent_clk_idx].clk);
	}

	return 0;
}

static void exynos5_devfreq_mif_dynamic_self_refresh_pre(struct devfreq_data_mif *data)
{
	data->dynamic_self_refresh_enabled = (__raw_readl(data->base_drex0 + 0x4) & 0x20) ? 1 : 0; /* check default status */

	if(data->dynamic_self_refresh_enabled) {
		__raw_writel(__raw_readl(data->base_drex0 + 0x4) & ~0x20, data->base_drex0 + 0x4); /* DREX0: MemControl.deref_en = 0 */
		__raw_writel(__raw_readl(data->base_drex1 + 0x4) & ~0x20, data->base_drex1 + 0x4); /* DREX1: MemControl.deref_en = 0 */
		__raw_writel(0x08000000, data->base_drex0 + 0x10); /* DREX0 chip0: Exit from self refresh */
		__raw_writel(0x08100000, data->base_drex0 + 0x10); /* DREX0 chip1: Exit from self refresh */
		__raw_writel(0x08000000, data->base_drex1 + 0x10); /* DREX1 chip0: Exit from self refresh */
		__raw_writel(0x08100000, data->base_drex1 + 0x10); /* DREX1 chip1: Exit from self refresh */
	}
}

static void exynos5_devfreq_mif_dynamic_self_refresh_post(struct devfreq_data_mif *data)
{
	if(data->dynamic_self_refresh_enabled) {
		__raw_writel(__raw_readl(data->base_drex0 + 0x4) | 0x20, data->base_drex0 + 0x4); /* DREX0: MemControl.deref_en = 0 */
		__raw_writel(__raw_readl(data->base_drex1 + 0x4) | 0x20, data->base_drex1 + 0x4); /* DREX1: MemControl.deref_en = 0 */
	}
}

static int exynos5_devfreq_mif_set_volt(struct devfreq_data_mif *data,
					unsigned long volt,
					unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	exynos5_devfreq_mif_dynamic_self_refresh_pre(data);
	regulator_set_voltage(data->vdd_mif, volt, volt_range);
	exynos5_devfreq_mif_dynamic_self_refresh_post(data);

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

static int exynos5_devfreq_change_setting(struct devfreq_data_mif *data,
					int target_idx,
					bool to_lower)
{
	unsigned long target_volt;
	struct opp *target_opp;

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(data->dev, &devfreq_mif_opp_list[target_idx].freq, 0);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(data->dev, "DEVFREQ(MIF) : Invalid OPP to find\n");
		return PTR_ERR(target_opp);
	}
	target_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	target_volt = get_limit_voltage(target_volt, data->volt_offset);
#endif
	rcu_read_unlock();

	if (to_lower) {
		set_match_abb(ID_MIF, devfreq_mif_asv_abb[target_idx]);
		exynos5_devfreq_mif_set_volt(data, target_volt, target_volt + VOLT_STEP);
	} else {
		exynos5_devfreq_mif_set_volt(data, target_volt, target_volt + VOLT_STEP);
		set_match_abb(ID_MIF, devfreq_mif_asv_abb[target_idx]);
	}

	return 0;
}

static int exynos5_devfreq_bts_setting(struct devfreq_data_mif *data,
					int target_idx,
					bool to_lower)
{
	if (to_lower) {
		exynos5_devfreq_mif_set_back_pressure(data, target_idx);
		exynos5_devfreq_mif_set_timeout(data, target_idx);
	} else {
		exynos5_devfreq_mif_set_timeout(data, target_idx);
		exynos5_devfreq_mif_set_back_pressure(data, target_idx);
	}

	return 0;
}

static void exynos5_devfreq_mif_update_time(struct devfreq_data_mif *data,
					int target_idx)
{
	cputime64_t cur_time = get_jiffies_64();
	cputime64_t diff_time = cur_time - data->last_jiffies;

	devfreq_mif_time_in_state[target_idx] += diff_time;
	data->last_jiffies = cur_time;
}


static void exynos5_devfreq_set_dll_gate(struct devfreq_data_mif *data,
		bool enable)
{
	unsigned int temp;

	if (enable) {
		temp = __raw_readl(data->base_mif + 0x0300);
		temp &= ~(0x1 << 16);
		__raw_writel(temp, data->base_mif + 0x0300);
	} else {
		temp = __raw_readl(data->base_mif + 0x0300);
		temp |= (0x1 << 16);
		__raw_writel(temp, data->base_mif + 0x0300);

		while (((__raw_readl(data->base_mif + 0x0400) >> 16) & 0x4) == 0x4);
		while ((__raw_readl(data->base_ddrphy0 + 0x34) & 0x5) == 0x5);
		while ((__raw_readl(data->base_ddrphy1 + 0x34) & 0x5) == 0x5);
	}
	while (((__raw_readl(data->base_mif + 0x1004) >> 16) & 0x7) != 0);
}

static void exynos5_devfreq_dynamic_phy_gate(struct devfreq_data_mif *data,
		bool enable)
{
	unsigned int temp;

	if (enable) {
		temp = __raw_readl(data->base_drex0 + 0x0008);
		temp |= (0x1 <<4);
		__raw_writel(temp, data->base_drex0 + 0x0008);
		temp = __raw_readl(data->base_drex1 + 0x0008);
		temp |= (0x1 <<4);		
		__raw_writel(temp, data->base_drex1 + 0x0008);
	} else {
		temp = __raw_readl(data->base_drex0 + 0x0008);
		temp &= ~(0x1 <<4);
		__raw_writel(temp, data->base_drex0 + 0x0008);
		temp = __raw_readl(data->base_drex1 + 0x0008);
		temp &= ~(0x1 <<4);		
		__raw_writel(temp, data->base_drex1 + 0x0008);
	}

}
static int exynos5_devfreq_mif_set_freq(struct devfreq_data_mif *data,
					int target_idx,
					int old_idx)
{
	int i;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
	sec_debug_aux_log(SEC_DEBUG_AUXLOG_CPU_BUS_CLOCK_CHANGE,
		"old:%7d new:%7d (MIF)",
	old_idx, target_idx);
#endif
	if (LV0 != old_idx) {
		exynos5_devfreq_change_setting(data, LV0, false);
		exynos5_devfreq_mif_timing_parameter(data, LV0);
		exynos5_devfreq_mif_change_timing_set(data);
		exynos5_devfreq_mif_clk_change_parent(&aclk_clk2x_phy_media_pll_list);
		exynos5_devfreq_set_dll_gate(data, false);
		exynos5_devfreq_bts_setting(data, LV0, false);		
	}


	/* change clocks except ACLK_CLK2X_PHY */
	if (target_idx < old_idx) {
		if (LV0 != target_idx) {
			clk_set_rate(devfreq_mif_clk[FOUT_BPLL].clk, mem_pll_rate[target_idx].freq);

			exynos5_devfreq_bts_setting(data, target_idx, true);
			exynos5_devfreq_set_dll_gate(data, true);
			exynos5_devfreq_mif_timing_parameter(data, target_idx);
			exynos5_devfreq_mif_change_timing_set(data);
			exynos5_devfreq_mif_clk_change_parent(&aclk_clk2x_phy_mem_pll_list);
			exynos5_devfreq_change_setting(data, target_idx, true);
		}

		for (i = 0; i < ARRAY_SIZE(devfreq_clk_mif_info_list); ++i) {
			clk_info = &devfreq_clk_mif_info_list[i][target_idx];
			clk_states = clk_info->states;

			exynos5_devfreq_mif_clk_change_parent(clk_states);
			if (clk_info->freq != 0)
				clk_set_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk, clk_info->freq);
		}
	} else {
		if (LV0 != target_idx)
			exynos5_devfreq_bts_setting(data, target_idx, true);
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_mif_info_list); ++i) {
			clk_info = &devfreq_clk_mif_info_list[i][target_idx];
			clk_states = clk_info->states;
			exynos5_devfreq_mif_clk_change_parent(clk_states);
			if (clk_info->freq != 0)
				clk_set_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk, clk_info->freq);
		}

		if (LV0 != target_idx) {
			clk_set_rate(devfreq_mif_clk[FOUT_BPLL].clk, mem_pll_rate[target_idx].freq);

			exynos5_devfreq_set_dll_gate(data, true);
			exynos5_devfreq_mif_timing_parameter(data, target_idx);
			exynos5_devfreq_mif_change_timing_set(data);
			exynos5_devfreq_mif_clk_change_parent(&aclk_clk2x_phy_mem_pll_list);
			exynos5_devfreq_change_setting(data, target_idx, true);
		}
	}

	return 0;
}

static int exynos5_devfreq_mif_target(struct device *dev,
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

#if defined(CONFIG_MACH_HLLTE)
	if (system_rev < 0x4) {
		return ret;
	}
#endif
	mutex_lock(&mif_data->lock);

	*target_freq = min3(*target_freq,
			devfreq_mif_ch0_work.max_freq,
			devfreq_mif_ch1_work.max_freq);


	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(dev, "DEVFREQ(MIF) : Invalid OPP to find\n");
		mutex_unlock(&mif_data->lock);
		return PTR_ERR(target_opp);
	}

	*target_freq = opp_get_freq(target_opp);
	rcu_read_unlock();

	target_idx = exynos5_devfreq_mif_get_idx(devfreq_mif_opp_list,
						ARRAY_SIZE(devfreq_mif_opp_list),
						*target_freq);
	old_idx = exynos5_devfreq_mif_get_idx(devfreq_mif_opp_list,
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

	exynos5_devfreq_dynamic_phy_gate(mif_data,false);
	exynos5_devfreq_mif_set_freq(mif_data, target_idx, old_idx);
	exynos_mif_cur_level = target_idx;
	exynos5_devfreq_dynamic_phy_gate(mif_data,true);	
out:
	exynos5_devfreq_mif_update_time(mif_data, target_idx);

	mutex_unlock(&mif_data->lock);
	return ret;
}

static int exynos5_devfreq_mif_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_mif *data = dev_get_drvdata(dev);

	if (!data_mif->use_dvfs)
		return -EAGAIN;

	stat->current_frequency = data->devfreq->previous_freq;
	stat->busy_time = devfreq_mif_exynos.val_pmcnt;
	stat->total_time = devfreq_mif_exynos.val_ccnt;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_mif_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_mif_target,
	.get_dev_status	= exynos5_devfreq_mif_get_dev_status,
};

static int exynos5_devfreq_mif_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_mif_clk); ++i) {
		devfreq_mif_clk[i].clk = clk_get(NULL, devfreq_mif_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_mif_clk[i].clk)) {
			pr_err("DEVFREQ(MIF) : %s can't get clock\n", devfreq_mif_clk[i].clk_name);
			return -EINVAL;
		}
	}

	return 0;
}

static int exynos5_init_mif_table(struct device *dev,
				struct devfreq_data_mif *data)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;
	unsigned int asv_abb = 0;

	for (i = 0; i < ARRAY_SIZE(devfreq_mif_opp_list); ++i) {
		freq = devfreq_mif_opp_list[i].freq;

		volt = get_match_volt(ID_MIF, freq);
		if (!volt)
			volt = devfreq_mif_opp_list[i].volt;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(MIF) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(MIF) : %uKhz, %uV\n", freq, volt);
		}

		asv_abb = get_match_abb(ID_MIF, freq);
		if (!asv_abb)
			devfreq_mif_asv_abb[i] = ABB_BYPASS;
		else
			devfreq_mif_asv_abb[i] = asv_abb;

		pr_info("DEVFREQ(MIF) : %uKhz, ABB %u\n", freq, devfreq_mif_asv_abb[i]);
	}

	return 0;
}

static int exynos5_devfreq_mif_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	struct devfreq_notifier_block *devfreq_nb;

	devfreq_nb = container_of(nb, struct devfreq_notifier_block, nb);

	mutex_lock(&devfreq_nb->df->lock);
	update_devfreq(devfreq_nb->df);
	mutex_unlock(&devfreq_nb->df->lock);

	return NOTIFY_OK;
}

static int exynos5_devfreq_mif_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	pm_qos_update_request(&boot_mif_qos, exynos5_devfreq_mif_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_mif_reboot_notifier = {
	.notifier_call = exynos5_devfreq_mif_reboot_notifier,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos5_devfreq_mif_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_mif *data = container_of(nb, struct devfreq_data_mif,
			tmu_notifier);
	unsigned int *on = v;

	switch (event) {
	case MEM_TH_LV1:
		data->use_derate_timing_row = false;
		return NOTIFY_OK;
	case MEM_TH_LV2:
	case MEM_TH_LV3:
		data->use_derate_timing_row = true;
		return NOTIFY_OK;
	}

	if (event == TMU_COLD) {
		unsigned int prev_volt, set_volt;
		if (pm_qos_request_active(&min_mif_thermal_qos))
			pm_qos_update_request(&min_mif_thermal_qos,
					exynos5_devfreq_mif_profile.initial_freq);

		if (*on) {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_mif);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt, data->volt_offset);
			if (prev_volt != set_volt)
				exynos5_devfreq_mif_dynamic_self_refresh_pre(data);
			regulator_set_voltage(data->vdd_mif, set_volt, set_volt + VOLT_STEP);
			if (prev_volt != set_volt)
				exynos5_devfreq_mif_dynamic_self_refresh_post(data);

			mutex_unlock(&data->lock);
		} else {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_mif);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt - COLD_VOLT_OFFSET, data->volt_offset);
			if (prev_volt != set_volt)
				exynos5_devfreq_mif_dynamic_self_refresh_pre(data);
			regulator_set_voltage(data->vdd_mif, set_volt, set_volt + VOLT_STEP);
			if (prev_volt != set_volt)
				exynos5_devfreq_mif_dynamic_self_refresh_post(data);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&min_mif_thermal_qos))
			pm_qos_update_request(&min_mif_thermal_qos,
					exynos5260_qos_mif.default_qos);
	}

	return NOTIFY_OK;
}
#endif

static int exynos5_devfreq_init_drex(struct devfreq_data_mif *data)
{
	unsigned int temp;

	temp = __raw_readl(data->base_mif + 0x1004);
	temp |= 0x1;
	__raw_writel(temp, data->base_mif + 0x1004);

	return 0;
}

static int exynos5_devfreq_init_alloc(struct devfreq_data_mif *data)
{
	data->base_drex0 = ioremap(0x10C20000, SZ_64K);
	data->base_drex1 = ioremap(0x10C30000, SZ_64K);
	data->base_mif = ioremap(0x10CE0000, SZ_8K);
	data->base_ddrphy0 = ioremap(0x10C00000, SZ_4K);
	data->base_ddrphy1 = ioremap(0x10C10000, SZ_4K);

	exynos5_devfreq_init_drex(data);

	return 0;
}

static void exynos5_devfreq_thermal_event(struct devfreq_thermal_work *work)
{
	if (work->polling_period == 0)
		return;

	queue_delayed_work(work->work_queue,
			&work->devfreq_mif_thermal_work,
			msecs_to_jiffies(work->polling_period));
}

static void exynos5_devfreq_thermal_monitor(struct work_struct *work)
{
	struct delayed_work *d_work = container_of(work, struct delayed_work, work);
	struct devfreq_thermal_work *thermal_work =
		container_of(d_work, struct devfreq_thermal_work, devfreq_mif_thermal_work);
	unsigned int mrstatus, tmp_thermal_level, max_thermal_level = 0;
	unsigned int timingaref_value = RATE_HALF;
	unsigned long max_freq = exynos5_devfreq_mif_governor_data.cal_qos_max;
	bool throttling = false;
	void __iomem *base_drex = NULL;

	if (thermal_work->channel == THERMAL_CHANNEL0) {
		base_drex = data_mif->base_drex0;
	} else if (thermal_work->channel == THERMAL_CHANNEL1) {
		base_drex = data_mif->base_drex1;
	}

	__raw_writel(0x09001000, base_drex + 0x10);
	mrstatus = __raw_readl(base_drex + 0x54);
	tmp_thermal_level = (mrstatus & MRSTATUS_THERMAL_LV_MASK);
	if (tmp_thermal_level > max_thermal_level)
		max_thermal_level = tmp_thermal_level;

	__raw_writel(0x09101000, base_drex + 0x10);
	mrstatus = __raw_readl(base_drex + 0x54);
	tmp_thermal_level = (mrstatus & MRSTATUS_THERMAL_LV_MASK);
	if (tmp_thermal_level > max_thermal_level)
		max_thermal_level = tmp_thermal_level;

	switch (max_thermal_level) {
		case 0:
		case 1:
		case 2:
		case 3:
			timingaref_value = RATE_HALF;
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
		max_freq = devfreq_mif_opp_list[LV4].freq;
	else
		max_freq = exynos5_devfreq_mif_governor_data.cal_qos_max;

	if (thermal_work->max_freq != max_freq) {
		thermal_work->max_freq = max_freq;
		mutex_lock(&data_mif->devfreq->lock);
		update_devfreq(data_mif->devfreq);
		mutex_unlock(&data_mif->devfreq->lock);
	}

	__raw_writel(timingaref_value, base_drex + 0x30);
	exynos5_devfreq_thermal_event(thermal_work);
}

static void exynos5_devfreq_init_thermal(void)
{
	devfreq_mif_thermal_wq_ch0 = create_freezable_workqueue("devfreq_thermal_wq_ch0");
	devfreq_mif_thermal_wq_ch1 = create_freezable_workqueue("devfreq_thermal_wq_ch1");

	INIT_DELAYED_WORK(&devfreq_mif_ch0_work.devfreq_mif_thermal_work,
			exynos5_devfreq_thermal_monitor);
	INIT_DELAYED_WORK(&devfreq_mif_ch1_work.devfreq_mif_thermal_work,
			exynos5_devfreq_thermal_monitor);

	devfreq_mif_ch0_work.work_queue = devfreq_mif_thermal_wq_ch0;
	devfreq_mif_ch1_work.work_queue = devfreq_mif_thermal_wq_ch1;

	exynos5_devfreq_thermal_event(&devfreq_mif_ch0_work);
	exynos5_devfreq_thermal_event(&devfreq_mif_ch1_work);
}
static int exynos5_devfreq_mif_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_mif *data;
	struct devfreq_notifier_block *devfreq_nb;
	struct exynos_devfreq_platdata *plat_data;
	int index = -1;
	struct opp *target_opp;
	unsigned long freq;

	if (exynos5_devfreq_mif_init_clock()) {
		ret = -EINVAL;
		goto err_data;
	}
#if defined(CONFIG_MACH_HLLTE)
	if (system_rev >= 0x4) {
		clk_set_parent(devfreq_mif_clk[MOUT_MIF_DREX2X].clk, devfreq_mif_clk[SCLK_BPLL].clk);
	}
#else
	clk_set_parent(devfreq_mif_clk[MOUT_MIF_DREX2X].clk, devfreq_mif_clk[SCLK_BPLL].clk);
#endif
	data = kzalloc(sizeof(struct devfreq_data_mif), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	data_mif = data;
	data->use_dvfs = false;

	ret = exynos5_init_mif_table(&pdev->dev, data);
	if (ret)
		goto err_inittable;

	exynos5_devfreq_init_alloc(data);
	exynos5_devfreq_update_timing_set(data);

	mutex_init(&data->lock);
	platform_set_drvdata(pdev, data);

	devfreq_mif_ch0_work.max_freq = exynos5_devfreq_mif_governor_data.cal_qos_max;
	devfreq_mif_ch1_work.max_freq = exynos5_devfreq_mif_governor_data.cal_qos_max;

	data->last_jiffies = get_jiffies_64();
	/* For low temperature compensation when boot time */
	data->volt_offset = COLD_VOLT_OFFSET;
	data->dev = &pdev->dev;
	data->vdd_mif = regulator_get(NULL, "vdd_mif");

	rcu_read_lock();
	freq = exynos5_devfreq_mif_profile.initial_freq;
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
	regulator_set_voltage(data->vdd_mif, data->old_volt, data->old_volt + VOLT_STEP);
	index = exynos5_devfreq_mif_get_idx(devfreq_mif_opp_list,
			ARRAY_SIZE(devfreq_mif_opp_list),
			freq);
	if (index < 0) {
		pr_err("DEVFREQ(MIF) : Failed to find index abb\n");
		ret = -EINVAL;
		goto err_nb;
	}
	set_match_abb(ID_MIF, devfreq_mif_asv_abb[index]);

	data->devfreq = devfreq_add_device(data->dev,
						&exynos5_devfreq_mif_profile,
						&devfreq_simple_ondemand,
						&exynos5_devfreq_mif_governor_data);

	exynos5_devfreq_init_thermal();

	devfreq_nb = kzalloc(sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (devfreq_nb == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}

	devfreq_nb->df = data->devfreq;
	devfreq_nb->nb.notifier_call = exynos5_devfreq_mif_notifier;

	exynos5260_devfreq_register(&devfreq_mif_exynos);
	exynos5260_ppmu_register_notifier(MIF, &devfreq_nb->nb);

	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos5_devfreq_mif_governor_data.cal_qos_max;
	pm_qos_add_request(&exynos5_mif_qos, PM_QOS_BUS_THROUGHPUT, plat_data->default_qos);
	pm_qos_add_request(&min_mif_thermal_qos, PM_QOS_BUS_THROUGHPUT, plat_data->default_qos);
	pm_qos_add_request(&boot_mif_qos, PM_QOS_BUS_THROUGHPUT, plat_data->default_qos);
	pm_qos_add_request(&exynos5_mif_bts_qos, PM_QOS_BUS_THROUGHPUT, plat_data->default_qos);
	pm_qos_update_request_timeout(&boot_mif_qos,
					exynos5_devfreq_mif_profile.initial_freq, 40000 * 1000);

	register_reboot_notifier(&exynos5_mif_reboot_notifier);

	ret = sysfs_create_group(&data->devfreq->dev.kobj, &devfreq_mif_attr_group);
	if (ret) {
		pr_err("DEVFREQ(MIF) : Failed create time_in_state sysfs\n");
		goto err_nb;
	}

	ret = device_create_file(&data->devfreq->dev, &dev_attr_available_frequencies);
	if (ret) {
		pr_err("DEVFREQ(MIF) : Failed create available_frequencies sysfs\n");
		goto err_nb;
	}

#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos5_devfreq_mif_tmu_notifier;
	exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data->use_dvfs = true;

	return ret;
err_nb:
	devfreq_remove_device(data->devfreq);
err_inittable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_mif_remove(struct platform_device *pdev)
{
	struct devfreq_data_mif *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_mif_thermal_qos);
	pm_qos_remove_request(&exynos5_mif_qos);
	pm_qos_remove_request(&boot_mif_qos);
	pm_qos_remove_request(&exynos5_mif_bts_qos);

	regulator_put(data->vdd_mif);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_mif_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos5_mif_qos))
		pm_qos_update_request(&exynos5_mif_qos, exynos5_devfreq_mif_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_mif_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	exynos5_devfreq_init_drex(data_mif);

	if (pm_qos_request_active(&exynos5_mif_qos))
		pm_qos_update_request(&exynos5_mif_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_mif_pm = {
	.suspend	= exynos5_devfreq_mif_suspend,
	.resume		= exynos5_devfreq_mif_resume,
};

static struct platform_driver exynos5_devfreq_mif_driver = {
	.probe	= exynos5_devfreq_mif_probe,
	.remove	= exynos5_devfreq_mif_remove,
	.driver	= {
		.name	= "exynos5-devfreq-mif",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_mif_pm,
	},
};

static struct platform_device exynos5_devfreq_mif_device = {
	.name	= "exynos5-devfreq-mif",
	.id	= -1,
};

static int __init exynos5_devfreq_mif_init(void)
{
	int ret;

	exynos_mif_cur_level = -1;
	use_timing_set_0 = false;
	exynos5_devfreq_mif_device.dev.platform_data = &exynos5260_qos_mif;

	ret = platform_device_register(&exynos5_devfreq_mif_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_mif_driver);
}
late_initcall(exynos5_devfreq_mif_init);

static void __exit exynos5_devfreq_mif_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_mif_driver);
	platform_device_unregister(&exynos5_devfreq_mif_device);
}
module_exit(exynos5_devfreq_mif_exit);
