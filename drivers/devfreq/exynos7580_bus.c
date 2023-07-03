/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Seungook yang(swy.yang@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>

#include <mach/asv-exynos.h>
#include <mach/devfreq.h>
#include <mach/regs-clock-exynos7580.h>
#include <mach/regs-pmu-exynos7580.h>
#include <mach/tmu.h>

#include <plat/cpu.h>

#include "devfreq_exynos.h"
#include "governor.h"

static struct delayed_work exynos_devfreq_thermal_work;

/* =========== common function */
int devfreq_get_opp_idx(struct devfreq_opp_table *table,
			unsigned int size,
			unsigned long freq)
{
	int i;

	for (i = 0; i < size; i++) {
		if (table[i].freq == freq)
			return i;
	}

	return -EINVAL;
}

#define MUX_MASK	0x7
static void exynos7580_devfreq_waiting_mux(void __iomem *mux_reg, u32 mux_pos)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(10);

	while (time_before(jiffies, timeout))
		if (((readl(mux_reg) >> mux_pos) & MUX_MASK) != 0x4)
			break;

	if (((readl(mux_reg) >> mux_pos) & MUX_MASK) == 0x4) /* 0x4 : On chainging */
		pr_err("%s: re-parenting mux timed-out\n", __func__);
}

int exynos_devfreq_init_pm_domain(struct devfreq_pm_domain_link pm_domain[], int num_of_pd)
{
	struct platform_device *pdev;
	struct device_node *np;
	int i;

	for_each_compatible_node(np, NULL, "samsung,exynos-pd") {
		struct exynos_pm_domain *pd;

		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);
		pd = platform_get_drvdata(pdev);

		for (i = 0; i < num_of_pd; i++) {
			if (!pm_domain[i].pm_domain_name)
				continue;

			if (!strcmp(pm_domain[i].pm_domain_name, pd->genpd.name))
				pm_domain[i].pm_domain = pd;
		}
	}

	return 0;
}

unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset)
{
	if (voltage > LIMIT_COLD_VOLTAGE)
		return voltage;

	if (voltage + volt_offset > LIMIT_COLD_VOLTAGE)
		return LIMIT_COLD_VOLTAGE;

	return voltage + volt_offset;
}

#ifdef DUMP_DVFS_CLKS
static void exynos7_devfreq_dump_all_clks(struct devfreq_clk_list devfreq_clks[], int clk_count, char *str)
{
	int i;

	for (i = 0; i < clk_count; i++) {
		if (devfreq_clks[i].clk)
			pr_info("%s %2d. clk name: %25s, rate: %4luMHz\n", str,
			i, devfreq_clks[i].clk_name, (clk_get_rate(devfreq_clks[i].clk) + (MHZ-1))/MHZ);
	}
}
#endif

static struct devfreq_data_int *data_int;
static struct devfreq_data_mif *data_mif;
static struct devfreq_data_isp *data_isp;

/* ========== 1. INT related function */
extern struct pm_qos_request min_int_thermal_qos;

enum devfreq_int_idx {
	INT_LV0,
	INT_LV1,
	INT_LV2,
	INT_LV3,
	INT_LV4,
	INT_LV5,
	INT_LV6,
	INT_LV7,
	INT_LV_COUNT,
};

enum devfreq_int_clk {
	MOUT_BUS_PLL_USER,
	MOUT_MEDIA_PLL_USER,
	MOUT_ACLK_BUS0_400,
	MOUT_ACLK_BUS1_400,
	DOUT_ACLK_BUS0_400,
	DOUT_ACLK_BUS1_400,
	DOUT_ACLK_BUS2_400,
	DOUT_ACLK_PERI_66,
	DOUT_ACLK_IMEM_266,
	DOUT_ACLK_IMEM_200,
	MOUT_ACLK_MFCMSCL_400,
	DOUT_ACLK_MFCMSCL_400,
	DOUT_ACLK_MFCMSCL_266,
	DOUT_ACLK_FSYS_200,
	DOUT_ACLK_G3D_400,
	DOUT_PCLK_BUS0_100,
	DOUT_PCLK_BUS1_100,
	DOUT_PCLK_BUS2_100,
	INT_CLK_COUNT,
};

struct devfreq_clk_list devfreq_int_clk[INT_CLK_COUNT] = {
	{"mout_bus_pll_top_user",},
	{"mout_media_pll_top_user",},
	{"mout_aclk_bus0_400",},
	{"mout_aclk_bus1_400",},
	{"dout_aclk_bus0_400",},
	{"dout_aclk_bus1_400",},
	{"dout_aclk_bus2_400",},
	{"dout_aclk_peri_66",},
	{"dout_aclk_imem_266",},
	{"dout_aclk_imem_200",},
	{"mout_aclk_mfcmscl_400",},
	{"dout_aclk_mfcmscl_400",},
	{"dout_aclk_mfcmscl_266",},
	{"dout_aclk_fsys_200",},
	{"dout_aclk_g3d_400",},
	{"dout_pclk_bus0_100",},
	{"dout_pclk_bus1_100",},
	{"dout_pclk_bus2_100",},
};

struct devfreq_opp_table devfreq_int_opp_list[] = {
	{INT_LV0, 400000, 1200000},
	{INT_LV1, 334000, 1200000},
	{INT_LV2, 267000, 1200000},
	{INT_LV3, 200000, 1200000},
	{INT_LV4, 160000, 1200000},
	{INT_LV5, 134000, 1200000},
	{INT_LV6, 111000, 1200000},
	{INT_LV7, 100000, 1200000},
};

/* For ACLK_BUS0_400 */
struct devfreq_clk_state aclk_bus0_400_bus_pll[] = {
	{MOUT_ACLK_BUS0_400, MOUT_BUS_PLL_USER},
};

struct devfreq_clk_states aclk_bus0_400_bus_pll_list = {
	.state = aclk_bus0_400_bus_pll,
	.state_count = ARRAY_SIZE(aclk_bus0_400_bus_pll),
};

struct devfreq_clk_state aclk_bus0_400_media_pll[] = {
	{MOUT_ACLK_BUS0_400, MOUT_MEDIA_PLL_USER},
};

struct devfreq_clk_states aclk_bus0_400_media_pll_list = {
	.state = aclk_bus0_400_media_pll,
	.state_count = ARRAY_SIZE(aclk_bus0_400_media_pll),
};

/* For ACLK_BUS1_400 */
struct devfreq_clk_state aclk_bus1_400_bus_pll[] = {
	{MOUT_ACLK_BUS1_400, MOUT_BUS_PLL_USER},
};

struct devfreq_clk_states aclk_bus1_400_bus_pll_list = {
	.state = aclk_bus1_400_bus_pll,
	.state_count = ARRAY_SIZE(aclk_bus1_400_bus_pll),
};

struct devfreq_clk_state aclk_bus1_400_media_pll[] = {
	{MOUT_ACLK_BUS1_400, MOUT_MEDIA_PLL_USER},
};

struct devfreq_clk_states aclk_bus1_400_media_pll_list = {
	.state = aclk_bus1_400_media_pll,
	.state_count = ARRAY_SIZE(aclk_bus1_400_media_pll),
};

/* For ACLK_MFCMSCL_400 */
struct devfreq_clk_state aclk_mfcmscl_400_bus_pll[] = {
	{MOUT_ACLK_MFCMSCL_400, MOUT_BUS_PLL_USER},
};

struct devfreq_clk_states aclk_mfcmscl_400_bus_pll_list = {
	.state = aclk_mfcmscl_400_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mfcmscl_400_bus_pll),
};

struct devfreq_clk_state aclk_mfcmscl_400_media_pll[] = {
	{MOUT_ACLK_MFCMSCL_400, MOUT_MEDIA_PLL_USER},
};

struct devfreq_clk_states aclk_mfcmscl_400_media_pll_list = {
	.state = aclk_mfcmscl_400_media_pll,
	.state_count = ARRAY_SIZE(aclk_mfcmscl_400_media_pll),
};

struct devfreq_clk_info aclk_fsys_200[] = {
	{INT_LV0, 100 * MHZ, 0, NULL},
	{INT_LV1, 100 * MHZ, 0, NULL},
	{INT_LV2, 100 * MHZ, 0, NULL},
	{INT_LV3, 100 * MHZ, 0, NULL},
	{INT_LV4, 100 * MHZ, 0, NULL},
	{INT_LV5, 100 * MHZ, 0, NULL},
	{INT_LV6, 100 * MHZ, 0, NULL},
	{INT_LV7, 100 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_imem_266[] = {
	{INT_LV0, 267 * MHZ, 0, NULL},
	{INT_LV1, 267 * MHZ, 0, NULL},
	{INT_LV2, 200 * MHZ, 0, NULL},
	{INT_LV3, 200 * MHZ, 0, NULL},
	{INT_LV4, 160 * MHZ, 0, NULL},
	{INT_LV5, 134 * MHZ, 0, NULL},
	{INT_LV6, 115 * MHZ, 0, NULL},
	{INT_LV7, 100 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_imem_200[] = {
	{INT_LV0, 200 * MHZ, 0, NULL},
	{INT_LV1, 200 * MHZ, 0, NULL},
	{INT_LV2, 160 * MHZ, 0, NULL},
	{INT_LV3, 160 * MHZ, 0, NULL},
	{INT_LV4, 134 * MHZ, 0, NULL},
	{INT_LV5, 115 * MHZ, 0, NULL},
	{INT_LV6, 100 * MHZ, 0, NULL},
	{INT_LV7, 100 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_bus0_400[] = {
	{INT_LV0, 400 * MHZ, 0, &aclk_bus0_400_bus_pll_list},
	{INT_LV1, 400 * MHZ, 0, &aclk_bus0_400_bus_pll_list},
	{INT_LV2, 400 * MHZ, 0, &aclk_bus0_400_bus_pll_list},
	{INT_LV3, 334 * MHZ, 0, &aclk_bus0_400_media_pll_list},
	{INT_LV4, 267 * MHZ, 0, &aclk_bus0_400_bus_pll_list},
	{INT_LV5, 200 * MHZ, 0, &aclk_bus0_400_bus_pll_list},
	{INT_LV6, 160 * MHZ, 0, &aclk_bus0_400_bus_pll_list},
	{INT_LV7, 100 * MHZ, 0, &aclk_bus0_400_bus_pll_list},
};

struct devfreq_clk_info aclk_bus1_400[] = {
	{INT_LV0, 400 * MHZ, 0, &aclk_bus1_400_bus_pll_list},
	{INT_LV1, 400 * MHZ, 0, &aclk_bus1_400_bus_pll_list},
	{INT_LV2, 334 * MHZ, 0, &aclk_bus1_400_media_pll_list},
	{INT_LV3, 267 * MHZ, 0, &aclk_bus1_400_bus_pll_list},
	{INT_LV4, 267 * MHZ, 0, &aclk_bus1_400_bus_pll_list},
	{INT_LV5, 200 * MHZ, 0, &aclk_bus1_400_bus_pll_list},
	{INT_LV6, 160 * MHZ, 0, &aclk_bus1_400_bus_pll_list},
	{INT_LV7, 100 * MHZ, 0, &aclk_bus1_400_bus_pll_list},
};

struct devfreq_clk_info aclk_bus2_400[] = {
	{INT_LV0, 100 * MHZ, 0, NULL},
	{INT_LV1, 100 * MHZ, 0, NULL},
	{INT_LV2, 100 * MHZ, 0, NULL},
	{INT_LV3, 100 * MHZ, 0, NULL},
	{INT_LV4, 100 * MHZ, 0, NULL},
	{INT_LV5, 100 * MHZ, 0, NULL},
	{INT_LV6, 100 * MHZ, 0, NULL},
	{INT_LV7, 100 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_peri_66[] = {
	{INT_LV0, 67 * MHZ, 0, NULL},
	{INT_LV1, 67 * MHZ, 0, NULL},
	{INT_LV2, 67 * MHZ, 0, NULL},
	{INT_LV3, 67 * MHZ, 0, NULL},
	{INT_LV4, 67 * MHZ, 0, NULL},
	{INT_LV5, 67 * MHZ, 0, NULL},
	{INT_LV6, 67 * MHZ, 0, NULL},
	{INT_LV7, 67 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_g3d_400[] = {
	{INT_LV0, 400 * MHZ, 0, NULL},
	{INT_LV1, 400 * MHZ, 0, NULL},
	{INT_LV2, 400 * MHZ, 0, NULL},
	{INT_LV3, 400 * MHZ, 0, NULL},
	{INT_LV4, 267 * MHZ, 0, NULL},
	{INT_LV5, 200 * MHZ, 0, NULL},
	{INT_LV6, 160 * MHZ, 0, NULL},
	{INT_LV7, 134 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_mfcmscl_400[] = {
	{INT_LV0, 267 * MHZ, 0, NULL},
	{INT_LV1, 267 * MHZ, 0, NULL},
	{INT_LV2, 267 * MHZ, 0, NULL},
	{INT_LV3, 200 * MHZ, 0, NULL},
	{INT_LV4, 160 * MHZ, 0, NULL},
	{INT_LV5, 100 * MHZ, 0, NULL},
	{INT_LV6, 100 * MHZ, 0, NULL},
	{INT_LV7, 100 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_mfcmscl_266[] = {
	{INT_LV0, 267 * MHZ, 0, NULL},
	{INT_LV1, 267 * MHZ, 0, NULL},
	{INT_LV2, 267 * MHZ, 0, NULL},
	{INT_LV3, 200 * MHZ, 0, NULL},
	{INT_LV4, 160 * MHZ, 0, NULL},
	{INT_LV5, 100 * MHZ, 0, NULL},
	{INT_LV6, 100 * MHZ, 0, NULL},
	{INT_LV7, 100 * MHZ, 0, NULL},
};

struct devfreq_clk_info pclk_bus0_100[] = {
	{INT_LV0, 100 * MHZ, 0, NULL},
	{INT_LV1, 100 * MHZ, 0, NULL},
	{INT_LV2, 100 * MHZ, 0, NULL},
	{INT_LV3, 84 * MHZ, 0, NULL},
	{INT_LV4, 67 * MHZ, 0, NULL},
	{INT_LV5, 50 * MHZ, 0, NULL},
	{INT_LV6, 40 * MHZ, 0, NULL},
	{INT_LV7, 34 * MHZ, 0, NULL},
};

struct devfreq_clk_info pclk_bus1_100[] = {
	{INT_LV0, 100 * MHZ, 0, NULL},
	{INT_LV1, 100 * MHZ, 0, NULL},
	{INT_LV2, 84 * MHZ, 0, NULL},
	{INT_LV3, 67 * MHZ, 0, NULL},
	{INT_LV4, 54 * MHZ, 0, NULL},
	{INT_LV5, 40 * MHZ, 0, NULL},
	{INT_LV6, 32 * MHZ, 0, NULL},
	{INT_LV7, 20 * MHZ, 0, NULL},
};

struct devfreq_clk_info pclk_bus2_100[] = {
	{INT_LV0, 50 * MHZ, 0, NULL},
	{INT_LV1, 50 * MHZ, 0, NULL},
	{INT_LV2, 50 * MHZ, 0, NULL},
	{INT_LV3, 50 * MHZ, 0, NULL},
	{INT_LV4, 50 * MHZ, 0, NULL},
	{INT_LV5, 50 * MHZ, 0, NULL},
	{INT_LV6, 50 * MHZ, 0, NULL},
	{INT_LV7, 50 * MHZ, 0, NULL},
};

struct devfreq_clk_info *devfreq_clk_int_info_list[] = {
	aclk_bus0_400,
	aclk_bus1_400,
	aclk_bus2_400,
	aclk_peri_66,
	aclk_imem_266,
	aclk_imem_200,
	aclk_mfcmscl_400,
	aclk_mfcmscl_266,
	aclk_fsys_200,
	aclk_g3d_400,
	pclk_bus0_100,
	pclk_bus1_100,
	pclk_bus2_100,
};

enum devfreq_int_clk devfreq_clk_int_info_idx[] = {
	DOUT_ACLK_BUS0_400,
	DOUT_ACLK_BUS1_400,
	DOUT_ACLK_BUS2_400,
	DOUT_ACLK_PERI_66,
	DOUT_ACLK_IMEM_266,
	DOUT_ACLK_IMEM_200,
	DOUT_ACLK_MFCMSCL_400,
	DOUT_ACLK_MFCMSCL_266,
	DOUT_ACLK_FSYS_200,
	DOUT_ACLK_G3D_400,
	DOUT_PCLK_BUS0_100,
	DOUT_PCLK_BUS1_100,
	DOUT_PCLK_BUS2_100,
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_int_pm_domain[] = {
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{"pd-mfcmscl"},
	{"pd-mfcmscl"},
	{NULL},
	{"pd-g3d"},
	{NULL},
	{NULL},
	{NULL},
};

static int exynos7_devfreq_int_set_clk(struct devfreq_data_int *data,
					int target_idx,
					struct clk *clk,
					struct devfreq_clk_info *clk_info)
{
	struct devfreq_clk_states *clk_states = clk_info[target_idx].states;
	int i;

	if (clk_get_rate(clk) > clk_info[target_idx].freq)
		clk_set_rate(clk, clk_info[target_idx].freq);

	if (clk_states) {
		for (i = 0; i < clk_states->state_count; i++) {
			clk_set_parent(devfreq_int_clk[clk_states->state[i].clk_idx].clk,
					devfreq_int_clk[clk_states->state[i].parent_clk_idx].clk);
		}
	}

	clk_set_rate(clk, clk_info[target_idx].freq);

	return 0;
}

void exynos7_int_notify_power_status(const char *pd_name, unsigned int turn_on)
{
	int i, cur_freq_idx;

	if (!turn_on || !data_int || !data_int->use_dvfs)
		return;

	mutex_lock(&data_int->lock);
	cur_freq_idx = devfreq_get_opp_idx(devfreq_int_opp_list,
				ARRAY_SIZE(devfreq_int_opp_list),
				data_int->devfreq->previous_freq);
	if (cur_freq_idx < 0) {
		pr_err("DEVFREQ(INT) : can't find target_idx to apply notify of power\n");
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_int_pm_domain); i++) {
		if (!devfreq_int_pm_domain[i].pm_domain_name)
			continue;

		if (!strcmp(devfreq_int_pm_domain[i].pm_domain_name, pd_name))
			exynos7_devfreq_int_set_clk(data_int,
					cur_freq_idx,
					devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk,
					devfreq_clk_int_info_list[i]);
	}
out:
	mutex_unlock(&data_int->lock);
}
#endif

static int exynos7_devfreq_int_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_int_clk); i++) {
		devfreq_int_clk[i].clk = clk_get(NULL, devfreq_int_clk[i].clk_name);
		if (IS_ERR(devfreq_int_clk[i].clk)) {
			pr_err("DEVFREQ(INT) : %s can't get clock\n", devfreq_int_clk[i].clk_name);
			return PTR_ERR(devfreq_int_clk[i].clk);
		}

		pr_debug("INT clk name: %s, rate: %luMHz\n",
			devfreq_int_clk[i].clk_name, (clk_get_rate(devfreq_int_clk[i].clk) + (MHZ-1))/MHZ);
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_clk_int_info_list); i++)
		clk_prepare_enable(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk);

	return 0;
}

static int exynos7_devfreq_int_set_volt(struct devfreq_data_int *data,
					unsigned long volt,
					unsigned long volt_range)
{
	data->old_volt = volt;
	return regulator_set_voltage(data->vdd_int, volt, volt_range);
}

static int exynos7_devfreq_int_set_freq(struct devfreq_data_int *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;

#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *pm_domain;
#endif

	for (i = 0; i < ARRAY_SIZE(devfreq_clk_int_info_list); i++) {
		clk_info = &devfreq_clk_int_info_list[i][target_idx];
		clk_states = clk_info->states;

#ifdef CONFIG_PM_RUNTIME
		pm_domain = devfreq_int_pm_domain[i].pm_domain;

		if (pm_domain) {
			mutex_lock(&pm_domain->access_lock);
			if ((__raw_readl(pm_domain->base + 0x4) & LOCAL_PWR_CFG) == 0) {
				mutex_unlock(&pm_domain->access_lock);
				continue;
			}
		}
#endif
		if (target_idx > old_idx)
			clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);

		if (clk_states) {
			for (j = 0; j < clk_states->state_count; ++j) {
				clk_set_parent(devfreq_int_clk[clk_states->state[j].clk_idx].clk,
						devfreq_int_clk[clk_states->state[j].parent_clk_idx].clk);
			}
		}

		clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);
		pr_debug("INT clk name: %s, set_rate: %lu, get_rate: %lu\n",
				devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk_name,
				clk_info->freq, clk_get_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk));

#ifdef CONFIG_PM_RUNTIME
		if (pm_domain)
			mutex_unlock(&pm_domain->access_lock);
#endif
	}

#ifdef DUMP_DVFS_CLKS
	exynos7_devfreq_dump_all_clks(devfreq_int_clk, INT_CLK_COUNT, "INT");
#endif
	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
int exynos7_devfreq_int_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_int *data = container_of(nb, struct devfreq_data_int, tmu_notifier);
	unsigned int set_volt;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		mutex_lock(&data->lock);

		if (*on && data->volt_offset)
			goto out;

		if (!*on && !data->volt_offset)
			goto out;

		if (*on) {
			data->volt_offset = COLD_VOLT_OFFSET;
			set_volt = get_limit_voltage(data->old_volt, data->volt_offset);
		} else {
			data->volt_offset = 0;
			set_volt = get_limit_voltage(data->old_volt - COLD_VOLT_OFFSET, data->volt_offset);
		}
		exynos7_devfreq_int_set_volt(data, set_volt, REGULATOR_MAX_MICROVOLT);
out:
		mutex_unlock(&data->lock);
	}

	return NOTIFY_OK;
}
#endif

int exynos7580_devfreq_int_init(void *data)
{
	int ret = 0;

	data_int = (struct devfreq_data_int *)data;
	data_int->max_state = INT_LV_COUNT;

	if (exynos7_devfreq_int_init_clock())
		return -EINVAL;

#ifdef CONFIG_PM_RUNTIME
	if (exynos_devfreq_init_pm_domain(devfreq_int_pm_domain, ARRAY_SIZE(devfreq_int_pm_domain)))
		return -EINVAL;
#endif
	data_int->int_set_volt = exynos7_devfreq_int_set_volt;
	data_int->int_set_freq = exynos7_devfreq_int_set_freq;
#ifdef CONFIG_EXYNOS_THERMAL
	data_int->tmu_notifier.notifier_call = exynos7_devfreq_int_tmu_notifier;
#endif

	return ret;
}
/* end of INT related function */

/* ========== 2. ISP related function */

enum devfreq_isp_idx {
	ISP_LV0,
	ISP_LV1,
	ISP_LV2,
	ISP_LV3,
	ISP_LV4,
	ISP_LV5,
	ISP_LV_COUNT,
};

enum devfreq_isp_clk {
	ISP_PLL,
	MOUT_ACLK_ISP_400_USER,
	MOUT_ACLK_ISP_333_USER,
	MOUT_ACLK_ISP_266_USER,
	DOUT_ISP_PLL_DIV2,
	DOUT_ISP_PLL_DIV3,
	DOUT_ACLK_ISP_400,
	DOUT_ACLK_ISP_333,
	DOUT_ACLK_ISP_266_TOP,
	DOUT_SCLK_CPU_ISP_CLKIN,
	DOUT_SCLK_CPU_ISP_ATCLKIN,
	DOUT_SCLK_CPU_ISP_PCLKDBG,
	DOUT_PCLK_CSI_LINK0_225,
	DOUT_ACLK_LINK_DATA,
	MOUT_ACLK_LINK_DATA_A,
	DOUT_ACLK_CSI_LINK1_75,
	MOUT_ACLK_CSI_LINK1_75_B,
	MOUT_ACLK_CSI_LINK1_75,
	DOUT_PCLK_CSI_LINK1_37,
	DOUT_ACLK_FIMC_ISP_450,
	MOUT_ACLK_FIMC_ISP_450_D,
	MOUT_ACLK_FIMC_ISP_450_C,
	MOUT_ACLK_FIMC_ISP_450_B,
	MOUT_ACLK_FIMC_ISP_450_A,
	DOUT_PCLK_FIMC_ISP_225,
	DOUT_ACLK_FIMC_FD_300,
	MOUT_ACLK_FIMC_FD_300,
	DOUT_PCLK_FIMC_FD_150,
	DOUT_ACLK_ISP_266,
	DOUT_ACLK_ISP_133,
	DOUT_ACLK_ISP_67,
	ISP_CLK_COUNT,
};

struct devfreq_clk_list devfreq_isp_clk[ISP_CLK_COUNT] = {
	{"isp_pll",},
	{"mout_aclk_isp_400_user"},
	{"mout_aclk_isp_333_user"},
	{"mout_aclk_isp_266_user"},
	{"dout_isp_pll_div2"},
	{"dout_isp_pll_div3"},
	{"dout_aclk_isp_400",},
	{"dout_aclk_isp_333",},
	{"dout_aclk_isp_266_top",},
	{"dout_sclk_cpu_isp_clkin",},
	{"dout_sclk_cpu_isp_atclkin",},
	{"dout_sclk_cpu_isp_pclkdbg",},
	{"dout_pclk_csi_link0_225",},
	{"dout_aclk_link_data",},
	{"mout_aclk_link_data_a",},
	{"dout_aclk_csi_link1_75",},
	{"mout_aclk_csi_link1_75_b",},
	{"mout_aclk_csi_link1_75",},
	{"dout_pclk_csi_link1_37",},
	{"dout_aclk_fimc_isp_450",},
	{"mout_aclk_fimc_isp_450_d"},
	{"mout_aclk_fimc_isp_450_c"},
	{"mout_aclk_fimc_isp_450_b"},
	{"mout_aclk_fimc_isp_450_a"},
	{"dout_pclk_fimc_isp_225",},
	{"dout_aclk_fimc_fd_300",},
	{"mout_aclk_fimc_fd_300",},
	{"dout_pclk_fimc_fd_150",},
	{"dout_aclk_isp_266",},
	{"dout_aclk_isp_133",},
	{"dout_aclk_isp_67",},
};

/* aclk_link_data */
struct devfreq_clk_state aclk_link_data_a_isp_400[] = {
	{MOUT_ACLK_LINK_DATA_A, MOUT_ACLK_ISP_400_USER},
};

struct devfreq_clk_states aclk_link_data_a_isp_400_list = {
	.state = aclk_link_data_a_isp_400,
	.state_count = ARRAY_SIZE(aclk_link_data_a_isp_400),
};

/* aclk_fimc_isp_450*/
struct devfreq_clk_state aclk_mif_fimc_bus_pll[] = {
	{MOUT_ACLK_FIMC_ISP_450_D, MOUT_ACLK_ISP_266_USER},
};

struct devfreq_clk_states aclk_mif_fimc_bus_pll_list = {
	.state = aclk_mif_fimc_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mif_fimc_bus_pll),
};

/* aclk_fimc_isp_450*/
struct devfreq_clk_state aclk_mif_fimc_isp_333[] = {
	{MOUT_ACLK_FIMC_ISP_450_B, MOUT_ACLK_ISP_333_USER},
	{MOUT_ACLK_FIMC_ISP_450_C, MOUT_ACLK_FIMC_ISP_450_B},
	{MOUT_ACLK_FIMC_ISP_450_D, MOUT_ACLK_FIMC_ISP_450_C},
};

struct devfreq_clk_states aclk_mif_fimc_isp_333_list = {
	.state = aclk_mif_fimc_isp_333,
	.state_count = ARRAY_SIZE(aclk_mif_fimc_isp_333),
};

/* aclk_fimc_isp_450*/
struct devfreq_clk_state aclk_mif_fimc_isp_pll[] = {
	{MOUT_ACLK_FIMC_ISP_450_A, DOUT_ISP_PLL_DIV2},
	{MOUT_ACLK_FIMC_ISP_450_B, MOUT_ACLK_FIMC_ISP_450_A},
	{MOUT_ACLK_FIMC_ISP_450_C, MOUT_ACLK_FIMC_ISP_450_B},
	{MOUT_ACLK_FIMC_ISP_450_D, MOUT_ACLK_FIMC_ISP_450_C},
};

struct devfreq_clk_states aclk_mif_fimc_isp_pll_list = {
	.state = aclk_mif_fimc_isp_pll,
	.state_count = ARRAY_SIZE(aclk_mif_fimc_isp_pll),
};

/* aclk_csi_link1_7 */
struct devfreq_clk_state aclk_csi_link1_75_isp_400[] = {
	{MOUT_ACLK_CSI_LINK1_75_B, MOUT_ACLK_ISP_400_USER},
};

struct devfreq_clk_states aclk_csi_link1_75_isp_400_list = {
	.state = aclk_csi_link1_75_isp_400,
	.state_count = ARRAY_SIZE(aclk_csi_link1_75_isp_400),
};

/* aclk_csi_link1_7 */
struct devfreq_clk_state aclk_csi_link1_75_isp_266[] = {
	{MOUT_ACLK_CSI_LINK1_75, DOUT_ISP_PLL_DIV3},
	{MOUT_ACLK_CSI_LINK1_75_B, MOUT_ACLK_CSI_LINK1_75},
};

struct devfreq_clk_states aclk_csi_link1_75_isp_266_list = {
	.state = aclk_csi_link1_75_isp_266,
	.state_count = ARRAY_SIZE(aclk_csi_link1_75_isp_266),
};

/* aclk_fimc_fd_300 */
struct devfreq_clk_state aclk_fimc_fd_300_div3[] = {
	{MOUT_ACLK_FIMC_FD_300, DOUT_ISP_PLL_DIV3},
};

struct devfreq_clk_states aclk_fimc_fd_300_div3_list = {
	.state = aclk_fimc_fd_300_div3,
	.state_count = ARRAY_SIZE(aclk_fimc_fd_300_div3),
};

/* aclk_fimc_fd_300 */
struct devfreq_clk_state aclk_fimc_fd_300_isp_266[] = {
	{MOUT_ACLK_FIMC_FD_300, MOUT_ACLK_ISP_266_USER},
};

struct devfreq_clk_states aclk_fimc_fd_300_isp_266_list = {
	.state = aclk_fimc_fd_300_isp_266,
	.state_count = ARRAY_SIZE(aclk_fimc_fd_300_isp_266),
};

struct devfreq_opp_table devfreq_isp_opp_list[] = {
	{ISP_LV0, 530000, 1200000},
	{ISP_LV1, 430000, 1200000},
	{ISP_LV2, 400000, 1200000},
	{ISP_LV3, 334000, 1200000},
	{ISP_LV4, 267000, 1200000},
	{ISP_LV5, 200000, 1200000},
};

struct devfreq_clk_info isp_pll_freq[] = {
	{ISP_LV0, 1060 * MHZ, 0, NULL},
	{ISP_LV1, 1060 * MHZ, 0, NULL},
	{ISP_LV2,  860 * MHZ, 0, NULL},
	{ISP_LV3,  860 * MHZ, 0, NULL},
	{ISP_LV4,  800 * MHZ, 0, NULL},
	{ISP_LV5,  400 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_isp_400[] = {
	{ISP_LV0, 400 * MHZ, 0, NULL},
	{ISP_LV1, 400 * MHZ, 0, NULL},
	{ISP_LV2, 400 * MHZ, 0, NULL},
	{ISP_LV3, 400 * MHZ, 0, NULL},
	{ISP_LV4, 267 * MHZ, 0, NULL},
	{ISP_LV5, 200 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_isp_333[] = {
	{ISP_LV0, 334 * MHZ, 0, NULL},
	{ISP_LV1, 334 * MHZ, 0, NULL},
	{ISP_LV2, 334 * MHZ, 0, NULL},
	{ISP_LV3, 334 * MHZ, 0, NULL},
	{ISP_LV4, 223 * MHZ, 0, NULL},
	{ISP_LV5, 167 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_isp_266_top[] = {
	{ISP_LV0, 267 * MHZ, 0, NULL},
	{ISP_LV1, 267 * MHZ, 0, NULL},
	{ISP_LV2, 267 * MHZ, 0, NULL},
	{ISP_LV3, 267 * MHZ, 0, NULL},
	{ISP_LV4, 200 * MHZ, 0, NULL},
	{ISP_LV5, 160 * MHZ, 0, NULL},
};

struct devfreq_clk_info isp_pll_div2[] = {
	{ISP_LV0, 530 * MHZ, 0, NULL},
	{ISP_LV1, 530 * MHZ, 0, NULL},
	{ISP_LV2, 430 * MHZ, 0, NULL},
	{ISP_LV3, 430 * MHZ, 0, NULL},
	{ISP_LV4, 400 * MHZ, 0, NULL},
	{ISP_LV5, 200 * MHZ, 0, NULL},
};

struct devfreq_clk_info isp_pll_div3[] = {
	{ISP_LV0, 354 * MHZ, 0, NULL},
	{ISP_LV1, 354 * MHZ, 0, NULL},
	{ISP_LV2, 215 * MHZ, 0, NULL},
	{ISP_LV3, 215 * MHZ, 0, NULL},
	{ISP_LV4, 200 * MHZ, 0, NULL},
	{ISP_LV5, 100 * MHZ, 0, NULL},
};

struct devfreq_clk_info sclk_cpu_isp_clkin[] = {
	{ISP_LV0, 530 * MHZ, 0, NULL},
	{ISP_LV1, 530 * MHZ, 0, NULL},
	{ISP_LV2, 430 * MHZ, 0, NULL},
	{ISP_LV3, 430 * MHZ, 0, NULL},
	{ISP_LV4, 400 * MHZ, 0, NULL},
	{ISP_LV5, 200 * MHZ, 0, NULL},
};

struct devfreq_clk_info sclk_cpu_isp_atclkin[] = {
	{ISP_LV0, 265 * MHZ, 0, NULL},
	{ISP_LV1, 265 * MHZ, 0, NULL},
	{ISP_LV2, 215 * MHZ, 0, NULL},
	{ISP_LV3, 215 * MHZ, 0, NULL},
	{ISP_LV4, 200 * MHZ, 0, NULL},
	{ISP_LV5, 100 * MHZ, 0, NULL},
};

struct devfreq_clk_info sclk_cpu_isp_pclkdbg[] = {
	{ISP_LV0, 67 * MHZ, 0, NULL},
	{ISP_LV1, 67 * MHZ, 0, NULL},
	{ISP_LV2, 54 * MHZ, 0, NULL},
	{ISP_LV3, 54 * MHZ, 0, NULL},
	{ISP_LV4, 50 * MHZ, 0, NULL},
	{ISP_LV5, 25 * MHZ, 0, NULL},
};

struct devfreq_clk_info pclk_csi_link0_225[] = {
	{ISP_LV0, 265 * MHZ, 0, NULL},
	{ISP_LV1, 265 * MHZ, 0, NULL},
	{ISP_LV2, 215 * MHZ, 0, NULL},
	{ISP_LV3, 215 * MHZ, 0, NULL},
	{ISP_LV4, 200 * MHZ, 0, NULL},
	{ISP_LV5, 100 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_link_data[] = {
	{ISP_LV0, 400 * MHZ, 0, &aclk_link_data_a_isp_400_list},
	{ISP_LV1, 400 * MHZ, 0, &aclk_link_data_a_isp_400_list},
	{ISP_LV2, 400 * MHZ, 0, &aclk_link_data_a_isp_400_list},
	{ISP_LV3, 400 * MHZ, 0, &aclk_link_data_a_isp_400_list},
	{ISP_LV4, 267 * MHZ, 0, &aclk_link_data_a_isp_400_list},
	{ISP_LV5, 200 * MHZ, 0, &aclk_link_data_a_isp_400_list},
};

struct devfreq_clk_info aclk_csi_link1_75[] = {
	{ISP_LV0, 200 * MHZ, 0, &aclk_csi_link1_75_isp_400_list},
	{ISP_LV1, 200 * MHZ, 0, &aclk_csi_link1_75_isp_400_list},
	{ISP_LV2, 200 * MHZ, 0, &aclk_csi_link1_75_isp_400_list},
	{ISP_LV3, 200 * MHZ, 0, &aclk_csi_link1_75_isp_400_list},
	{ISP_LV4, 200 * MHZ, 0, &aclk_csi_link1_75_isp_266_list},
	{ISP_LV5, 100 * MHZ, 0, &aclk_csi_link1_75_isp_400_list},
};

struct devfreq_clk_info pclk_csi_link1_37[] = {
	{ISP_LV0, 100 * MHZ, 0, NULL},
	{ISP_LV1, 67 * MHZ, 0, NULL},
	{ISP_LV2, 67 * MHZ, 0, NULL},
	{ISP_LV3, 67 * MHZ, 0, NULL},
	{ISP_LV4, 67 * MHZ, 0, NULL},
	{ISP_LV5, 34 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_fimc_isp_450[] = {
	{ISP_LV0, 530 * MHZ, 0, &aclk_mif_fimc_isp_pll_list},
	{ISP_LV1, 334 * MHZ, 0, &aclk_mif_fimc_isp_333_list},
	{ISP_LV2, 430 * MHZ, 0, &aclk_mif_fimc_isp_pll_list},
	{ISP_LV3, 267 * MHZ, 0, &aclk_mif_fimc_bus_pll_list},
	{ISP_LV4, 200 * MHZ, 0, &aclk_mif_fimc_isp_pll_list},
	{ISP_LV5, 160 * MHZ, 0, &aclk_mif_fimc_bus_pll_list},
};

struct devfreq_clk_info pclk_fimc_isp_225[] = {
	{ISP_LV0, 265 * MHZ, 0, NULL},
	{ISP_LV1, 167 * MHZ, 0, NULL},
	{ISP_LV2, 215 * MHZ, 0, NULL},
	{ISP_LV3, 134 * MHZ, 0, NULL},
	{ISP_LV4, 100 * MHZ, 0, NULL},
	{ISP_LV5,  80 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_fimc_fd_300[] = {
	{ISP_LV0, 354 * MHZ, 0, &aclk_fimc_fd_300_div3_list},
	{ISP_LV1, 267 * MHZ, 0, &aclk_fimc_fd_300_isp_266_list},
	{ISP_LV2, 267 * MHZ, 0, &aclk_fimc_fd_300_isp_266_list},
	{ISP_LV3, 267 * MHZ, 0, &aclk_fimc_fd_300_isp_266_list},
	{ISP_LV4, 200 * MHZ, 0, &aclk_fimc_fd_300_div3_list},
	{ISP_LV5, 160 * MHZ, 0, &aclk_fimc_fd_300_isp_266_list},
};

struct devfreq_clk_info pclk_fimc_fd_150[] = {
	{ISP_LV0, 177 * MHZ, 0, NULL},
	{ISP_LV1, 134 * MHZ, 0, NULL},
	{ISP_LV2, 134 * MHZ, 0, NULL},
	{ISP_LV3, 134 * MHZ, 0, NULL},
	{ISP_LV4, 100 * MHZ, 0, NULL},
	{ISP_LV5, 80 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_isp_266[] = {
	{ISP_LV0, 267 * MHZ, 0, NULL},
	{ISP_LV1, 267 * MHZ, 0, NULL},
	{ISP_LV2, 267 * MHZ, 0, NULL},
	{ISP_LV3, 267 * MHZ, 0, NULL},
	{ISP_LV4, 200 * MHZ, 0, NULL},
	{ISP_LV5, 160 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_isp_133[] = {
	{ISP_LV0, 134 * MHZ, 0, NULL},
	{ISP_LV1, 134 * MHZ, 0, NULL},
	{ISP_LV2, 134 * MHZ, 0, NULL},
	{ISP_LV3, 134 * MHZ, 0, NULL},
	{ISP_LV4, 100 * MHZ, 0, NULL},
	{ISP_LV5,  80 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_isp_67[] = {
	{ISP_LV0, 67 * MHZ, 0, NULL},
	{ISP_LV1, 67 * MHZ, 0, NULL},
	{ISP_LV2, 67 * MHZ, 0, NULL},
	{ISP_LV3, 67 * MHZ, 0, NULL},
	{ISP_LV4, 67 * MHZ, 0, NULL},
	{ISP_LV5, 54 * MHZ, 0, NULL},
};

struct devfreq_clk_info *devfreq_clk_isp_info_list[] = {
	aclk_isp_400,
	aclk_isp_333,
	aclk_isp_266_top,
	isp_pll_div2,
	isp_pll_div3,
	sclk_cpu_isp_clkin,
	sclk_cpu_isp_atclkin,
	sclk_cpu_isp_pclkdbg,
	pclk_csi_link0_225,
	aclk_link_data,
	aclk_csi_link1_75,
	pclk_csi_link1_37,
	aclk_fimc_isp_450,
	pclk_fimc_isp_225,
	aclk_fimc_fd_300,
	pclk_fimc_fd_150,
	aclk_isp_266,
	aclk_isp_133,
	aclk_isp_67,
};

enum devfreq_isp_clk devfreq_clk_isp_info_idx[] = {
	DOUT_ACLK_ISP_400,
	DOUT_ACLK_ISP_333,
	DOUT_ACLK_ISP_266_TOP,
	DOUT_ISP_PLL_DIV2,
	DOUT_ISP_PLL_DIV3,
	DOUT_SCLK_CPU_ISP_CLKIN,
	DOUT_SCLK_CPU_ISP_ATCLKIN,
	DOUT_SCLK_CPU_ISP_PCLKDBG,
	DOUT_PCLK_CSI_LINK0_225,
	DOUT_ACLK_LINK_DATA,
	DOUT_ACLK_CSI_LINK1_75,
	DOUT_PCLK_CSI_LINK1_37,
	DOUT_ACLK_FIMC_ISP_450,
	DOUT_PCLK_FIMC_ISP_225,
	DOUT_ACLK_FIMC_FD_300,
	DOUT_PCLK_FIMC_FD_150,
	DOUT_ACLK_ISP_266,
	DOUT_ACLK_ISP_133,
	DOUT_ACLK_ISP_67,
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_isp_pm_domain[] = {
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
	{"pd-isp",},
};

static int exynos7_devfreq_isp_set_clk(struct devfreq_data_isp *data,
					int target_idx,
					struct clk *clk,
					struct devfreq_clk_info *clk_info)
{
	int i;
	struct devfreq_clk_states *clk_states = clk_info[target_idx].states;

	if (clk_get_rate(clk) > clk_info[target_idx].freq)
		clk_set_rate(clk, clk_info[target_idx].freq);

	if (clk_states) {
		for (i = 0; i < clk_states->state_count; i++) {
			clk_set_parent(devfreq_isp_clk[clk_states->state[i].clk_idx].clk,
					devfreq_isp_clk[clk_states->state[i].parent_clk_idx].clk);
		}
	}

	clk_set_rate(clk, clk_info[target_idx].freq);

	return 0;
}

void exynos7_isp_notify_power_status(const char *pd_name, unsigned int turn_on)
{
	int i;
	int cur_freq_idx;

	if (!turn_on || !data_isp || !data_isp->use_dvfs)
		return;

	mutex_lock(&data_isp->lock);
	cur_freq_idx = devfreq_get_opp_idx(devfreq_isp_opp_list,
			ARRAY_SIZE(devfreq_isp_opp_list),
			data_isp->devfreq->previous_freq);
	if (cur_freq_idx < 0) {
		pr_err("DEVFREQ(ISP) : can't find target_idx to apply notify of power\n");
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_isp_pm_domain); i++) {
		if (!devfreq_isp_pm_domain[i].pm_domain_name)
			continue;

		if (!strcmp(devfreq_isp_pm_domain[i].pm_domain_name, pd_name))
			exynos7_devfreq_isp_set_clk(data_isp,
					cur_freq_idx,
					devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk,
					devfreq_clk_isp_info_list[i]);
	}

out:
	mutex_unlock(&data_isp->lock);
}
#endif

static int exynos7_devfreq_isp_set_freq(struct devfreq_data_isp *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;
	struct clk *isp_pll;
#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *pm_domain;

	pm_domain = devfreq_isp_pm_domain[0].pm_domain;
	if (pm_domain) {
		mutex_lock(&pm_domain->access_lock);
		if ((__raw_readl(pm_domain->base + 0x4) & LOCAL_PWR_CFG) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					goto camera_is_off;
		}

		mutex_unlock(&pm_domain->access_lock);
	}

#endif
	/* If both old_idx and target_idx is lower than ISP_LV1,
	 * p, m, s value for ISP_PLL don't need to be changed.
	 */
	if (old_idx <= 1 && target_idx <= 1)
		goto no_change_pms;

	/* Seqeunce to use s/w pll for ISP_PLL
	 * 1. Change OSCCLK as source pll
	 * 2. Set p, m, s value for ISP_PLL
	 * 3. Change ISP_PLL as source pll
	 */
	isp_pll = devfreq_isp_clk[ISP_PLL].clk;

	/* Change OSCCLK as source pll */
	__raw_writel(0x0, EXYNOS7580_MUX_SEL_ISP0);
	exynos7580_devfreq_waiting_mux(EXYNOS7580_MUX_STAT_ISP0, 0);

	/* Set p, m, s value for ISP_PLL
	 * ISP_LV0, ISP_LV1 = 1060MHz
	 * ISP_LV2, ISP_LV3 = 860MHz
	 * ISP_LV4 = 800MHz
	 * ISP_LV5 = 400MHz
	 */
	clk_set_rate(isp_pll, isp_pll_freq[target_idx].freq);

	/* Change ISP_PLL as source pll */
	__raw_writel(0x1, EXYNOS7580_MUX_SEL_ISP0);
	exynos7580_devfreq_waiting_mux(EXYNOS7580_MUX_STAT_ISP0, 0);

no_change_pms:

	for (i = 0; i < ARRAY_SIZE(devfreq_clk_isp_info_list); i++) {
		clk_info = &devfreq_clk_isp_info_list[i][target_idx];
		clk_states = clk_info->states;

		if (target_idx > old_idx)
			clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);

		if (clk_states) {
			for (j = 0; j < clk_states->state_count; ++j)
				clk_set_parent(devfreq_isp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_isp_clk[clk_states->state[j].parent_clk_idx].clk);
		}

		clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);
		pr_debug("ISP clk name: %s, set_rate: %lu, get_rate: %lu\n",
				devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk_name,
				clk_info->freq, clk_get_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk));
	}
#ifdef DUMP_DVFS_CLKS
	exynos7_devfreq_dump_all_clks(devfreq_isp_clk, ISP_CLK_COUNT, "ISP");
#endif

camera_is_off:

	return 0;
}

static int exynos7_devfreq_isp_set_volt(struct devfreq_data_isp *data,
		unsigned long volt,
		unsigned long volt_range)
{
	data->old_volt = volt;
	return regulator_set_voltage(data->vdd_isp_cam0, volt, volt_range);
}

static int exynos7_devfreq_isp_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_isp_clk); i++) {
		devfreq_isp_clk[i].clk = clk_get(NULL, devfreq_isp_clk[i].clk_name);
		if (IS_ERR(devfreq_isp_clk[i].clk)) {
			pr_err("DEVFREQ(ISP) : %s can't get clock\n", devfreq_isp_clk[i].clk_name);
			return PTR_ERR(devfreq_isp_clk[i].clk);
		}
		pr_debug("ISP clk name: %s, rate: %luMhz\n",
				devfreq_isp_clk[i].clk_name, (clk_get_rate(devfreq_isp_clk[i].clk) + (MHZ-1))/MHZ);
	}

	/* aclk_isp_333 has to be enabled */
	clk_prepare_enable(devfreq_isp_clk[devfreq_clk_isp_info_idx[1]].clk);

	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
int exynos7_devfreq_isp_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_isp *data = container_of(nb, struct devfreq_data_isp, tmu_notifier);
	unsigned int set_volt;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		mutex_lock(&data->lock);

		if (*on && data->volt_offset)
			goto out;

		if (!*on && !data->volt_offset)
			goto out;

		if (*on) {
			data->volt_offset = COLD_VOLT_OFFSET;
			set_volt = get_limit_voltage(data->old_volt, data->volt_offset);
		} else {
			data->volt_offset = 0;
			set_volt = get_limit_voltage(data->old_volt - COLD_VOLT_OFFSET, data->volt_offset);
		}

		exynos7_devfreq_isp_set_volt(data, set_volt, REGULATOR_MAX_MICROVOLT);
out:
		mutex_unlock(&data->lock);
	}

	return NOTIFY_OK;
}
#endif

int exynos7580_devfreq_isp_init(void *data)
{
	data_isp = (struct devfreq_data_isp *)data;
	data_isp->max_state = ISP_LV_COUNT;
	if (exynos7_devfreq_isp_init_clock())
		return -EINVAL;

#ifdef CONFIG_PM_RUNTIME
	if (exynos_devfreq_init_pm_domain(devfreq_isp_pm_domain,
					ARRAY_SIZE(devfreq_isp_pm_domain)))
		return -EINVAL;
#endif
	data_isp->isp_set_freq = exynos7_devfreq_isp_set_freq;
	data_isp->isp_set_volt = exynos7_devfreq_isp_set_volt;
#ifdef CONFIG_EXYNOS_THERMAL
	data_isp->tmu_notifier.notifier_call = exynos7_devfreq_isp_tmu_notifier;
#endif

	return 0;
}
int exynos7580_devfreq_isp_deinit(void *data)
{
	return 0;
}
/* end of ISP related function */

/* ========== 3. MIF related function */

enum devfreq_mif_idx {
	MIF_LV0,
	MIF_LV1,
	MIF_LV2,
	MIF_LV3,
	MIF_LV4,
	MIF_LV5,
	MIF_LV6,
	MIF_LV7,
	MIF_LV_COUNT,
};

enum devfreq_mif_clk {
	MEM0_PLL,
	MOUT_MEDIA_PLL_DIV2,
	DOUT_ACLK_MIF_400,
	MOUT_ACLK_MIF_400,
	BUS_PLL,
	DOUT_ACLK_MIF_200,
	DOUT_ACLK_MIF_100,
	MOUT_ACLK_MIF_100,
	DOUT_ACLK_MIF_FIX_100,
	DOUT_ACLK_DISP_200,
	MIF_CLK_COUNT,
};

struct devfreq_clk_list devfreq_mif_clk[MIF_CLK_COUNT] = {
	{"mem0_pll"},
	{"mout_media_pll_div2"},
	{"dout_aclk_mif_400"},
	{"mout_aclk_mif_400"},
	{"bus_pll"},
	{"dout_aclk_mif_200"},
	{"dout_aclk_mif_100"},
	{"mout_aclk_mif_100"},
	{"dout_aclk_mif_fix_100"},
	{"dout_aclk_disp_200"},
};

struct devfreq_opp_table devfreq_mif_opp_list[] = {
	{MIF_LV0, 825000, 1200000},
	{MIF_LV1, 728000, 1200000},
	{MIF_LV2, 667000, 1200000},
	{MIF_LV3, 559000, 1200000},
	{MIF_LV4, 416000, 1200000},
	{MIF_LV5, 338000, 1200000},
	{MIF_LV6, 273000, 1200000},
	{MIF_LV7, 200000, 1200000},
};

struct devfreq_clk_state aclk_mif_400_bus_pll[] = {
	{MOUT_ACLK_MIF_400, BUS_PLL},
};

struct devfreq_clk_states aclk_mif_400_bus_pll_list = {
	.state = aclk_mif_400_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mif_400_bus_pll),
};

struct devfreq_clk_state aclk_mif_400_media_pll[] = {
	{MOUT_ACLK_MIF_400, MOUT_MEDIA_PLL_DIV2},
};

struct devfreq_clk_states aclk_mif_400_media_pll_list = {
	.state = aclk_mif_400_media_pll,
	.state_count = ARRAY_SIZE(aclk_mif_400_media_pll),
};

struct devfreq_clk_state aclk_mif_100_bus_pll[] = {
	{MOUT_ACLK_MIF_100, BUS_PLL},
};

struct devfreq_clk_states aclk_mif_100_bus_pll_list = {
	.state = aclk_mif_100_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mif_100_bus_pll),
};

struct devfreq_clk_state aclk_mif_100_media_pll[] = {
	{MOUT_ACLK_MIF_100, MOUT_MEDIA_PLL_DIV2},
};

struct devfreq_clk_states aclk_mif_100_media_pll_list = {
	.state = aclk_mif_100_media_pll,
	.state_count = ARRAY_SIZE(aclk_mif_100_media_pll),
};

struct devfreq_clk_info sclk_clk_phy[] = {
	{MIF_LV0, 825 * MHZ, 0, NULL},
	{MIF_LV1, 728 * MHZ, 0, NULL},
	{MIF_LV2, 667 * MHZ, 0, NULL},
	{MIF_LV3, 559 * MHZ, 0, NULL},
	{MIF_LV4, 416 * MHZ, 0, NULL},
	{MIF_LV5, 338 * MHZ, 0, NULL},
	{MIF_LV6, 273 * MHZ, 0, NULL},
	{MIF_LV7, 200 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_mif_400[] = {
	{MIF_LV0, 400 * MHZ, 0, &aclk_mif_400_bus_pll_list},
	{MIF_LV1, 334 * MHZ, 0, &aclk_mif_400_media_pll_list},
	{MIF_LV2, 267 * MHZ, 0, &aclk_mif_400_bus_pll_list},
	{MIF_LV3, 223 * MHZ, 0, &aclk_mif_400_media_pll_list},
	{MIF_LV4, 167 * MHZ, 0, &aclk_mif_400_media_pll_list},
	{MIF_LV5, 160 * MHZ, 0, &aclk_mif_400_bus_pll_list},
	{MIF_LV6, 112 * MHZ, 0, &aclk_mif_400_media_pll_list},
	{MIF_LV7, 96 * MHZ, 0, &aclk_mif_400_media_pll_list},
};
struct devfreq_clk_info aclk_mif_200[] = {
	{MIF_LV0, 200 * MHZ, 0, NULL},
	{MIF_LV1, 167 * MHZ, 0, NULL},
	{MIF_LV2, 134 * MHZ, 0, NULL},
	{MIF_LV3, 112 * MHZ, 0, NULL},
	{MIF_LV4, 84 * MHZ, 0, NULL},
	{MIF_LV5, 80 * MHZ, 0, NULL},
	{MIF_LV6, 56 * MHZ, 0, NULL},
	{MIF_LV7, 48 * MHZ, 0, NULL},
};
struct devfreq_clk_info aclk_mif_100[] = {
	{MIF_LV0, 100 * MHZ, 0, &aclk_mif_100_bus_pll_list},
	{MIF_LV1,  84 * MHZ, 0, &aclk_mif_100_media_pll_list},
	{MIF_LV2,  84 * MHZ, 0, &aclk_mif_100_media_pll_list},
	{MIF_LV3,  89 * MHZ, 0, &aclk_mif_100_bus_pll_list},
	{MIF_LV4,  89 * MHZ, 0, &aclk_mif_100_bus_pll_list},
	{MIF_LV5,  67 * MHZ, 0, &aclk_mif_100_media_pll_list},
	{MIF_LV6,  67 * MHZ, 0, &aclk_mif_100_media_pll_list},
	{MIF_LV7,  61 * MHZ, 0, &aclk_mif_100_media_pll_list},
};
struct devfreq_clk_info aclk_mif_fix_100[] = {
	{MIF_LV0, 100 * MHZ, 0, NULL},
	{MIF_LV1, 100 * MHZ, 0, NULL},
	{MIF_LV2, 100 * MHZ, 0, NULL},
	{MIF_LV3, 100 * MHZ, 0, NULL},
	{MIF_LV4, 100 * MHZ, 0, NULL},
	{MIF_LV5, 100 * MHZ, 0, NULL},
	{MIF_LV6, 100 * MHZ, 0, NULL},
	{MIF_LV7, 100 * MHZ, 0, NULL},
};

struct devfreq_clk_info aclk_disp_200[] = {
	{MIF_LV0, 267 * MHZ, 0, NULL},
	{MIF_LV1, 267 * MHZ, 0, NULL},
	{MIF_LV2, 267 * MHZ, 0, NULL},
	{MIF_LV3, 200 * MHZ, 0, NULL},
	{MIF_LV4, 200 * MHZ, 0, NULL},
	{MIF_LV5, 160 * MHZ, 0, NULL},
	{MIF_LV6, 160 * MHZ, 0, NULL},
	{MIF_LV7, 160 * MHZ, 0, NULL},
};

struct devfreq_clk_info *devfreq_clk_mif_info_list[] = {
	aclk_mif_400,
	aclk_mif_200,
	aclk_mif_100,
	aclk_mif_fix_100,
	aclk_disp_200,
};

enum devfreq_mif_clk devfreq_clk_mif_info_idx[] = {
	DOUT_ACLK_MIF_400,
	DOUT_ACLK_MIF_200,
	DOUT_ACLK_MIF_100,
	DOUT_ACLK_MIF_FIX_100,
	DOUT_ACLK_DISP_200,
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_mif_pm_domain[] = {
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{"pd-disp"},
};

static int exynos7_devfreq_mif_set_clk(struct devfreq_data_mif *data,
					int target_idx,
					struct clk *clk,
					struct devfreq_clk_info *clk_info)
{
	int i;
	struct devfreq_clk_states *clk_states = clk_info[target_idx].states;

	if (clk_get_rate(clk) > clk_info[target_idx].freq)
		clk_set_rate(clk, clk_info[target_idx].freq);

	if (clk_states) {
		for (i = 0; i < clk_states->state_count; i++) {
			clk_set_parent(devfreq_mif_clk[clk_states->state[i].clk_idx].clk,
					devfreq_mif_clk[clk_states->state[i].parent_clk_idx].clk);
		}
	}

	clk_set_rate(clk, clk_info[target_idx].freq);

	return 0;
}

void exynos7_mif_notify_power_status(const char *pd_name, unsigned int turn_on)
{
	int i;
	int cur_freq_idx;

	if (!turn_on || !data_mif->use_dvfs)
		return;

	mutex_lock(&data_mif->lock);
	cur_freq_idx = devfreq_get_opp_idx(devfreq_mif_opp_list,
					ARRAY_SIZE(devfreq_mif_opp_list),
					data_mif->devfreq->previous_freq);
	if (cur_freq_idx < 0) {
		pr_err("DEVFREQ(MIF) : can't find target_idx to apply notify of power\n");
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_mif_pm_domain); i++) {
		if (!devfreq_mif_pm_domain[i].pm_domain_name)
			continue;

		if (!strcmp(devfreq_mif_pm_domain[i].pm_domain_name, pd_name))
			exynos7_devfreq_mif_set_clk(data_mif,
					cur_freq_idx,
					devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk,
					devfreq_clk_mif_info_list[i]);
	}
out:
	mutex_unlock(&data_mif->lock);
}
#endif

struct dmc_drex_dfs_mif_table {
	uint32_t drex_timingrfcpb;
	uint32_t drex_timingrow;
	uint32_t drex_timingdata;
	uint32_t drex_timingpower;
	uint32_t drex_rd_fetch;
};

struct dmc_phy_dfs_mif_table {
	uint32_t phy_dvfs_con0_set1;
	uint32_t phy_dvfs_con0_set0;
	uint32_t phy_dvfs_con0_set1_mask;
	uint32_t phy_dvfs_con0_set0_mask;
	uint32_t phy_dvfs_con2_set1;
	uint32_t phy_dvfs_con2_set0;
	uint32_t phy_dvfs_con2_set1_mask;
	uint32_t phy_dvfs_con2_set0_mask;
	uint32_t phy_dvfs_con3_set1;
	uint32_t phy_dvfs_con3_set0;
	uint32_t phy_dvfs_con3_set1_mask;
	uint32_t phy_dvfs_con3_set0_mask;
};

#define DREX_TIMING_PARA(rfcpb, row, data, power, rdfetch) \
{ \
	.drex_timingrfcpb	= rfcpb, \
	.drex_timingrow	= row, \
	.drex_timingdata	= data, \
	.drex_timingpower	= power, \
	.drex_rd_fetch	= rdfetch, \
}

#define PHY_DVFS_CON(con0_set1, con0_set0, con0_set1_mask, con0_set0_mask, \
		con2_set1, con2_set0, con2_set1_mask, con2_set0_mask, \
		con3_set1, con3_set0, con3_set1_mask, con3_set0_mask) \
{ \
	.phy_dvfs_con0_set1	= con0_set1, \
	.phy_dvfs_con0_set0	= con0_set0, \
	.phy_dvfs_con0_set1_mask	= con0_set1_mask, \
	.phy_dvfs_con0_set0_mask	= con0_set0_mask, \
	.phy_dvfs_con2_set1	= con2_set1, \
	.phy_dvfs_con2_set0	= con2_set0, \
	.phy_dvfs_con2_set1_mask	= con2_set1_mask, \
	.phy_dvfs_con2_set0_mask	= con2_set0_mask, \
	.phy_dvfs_con3_set1	= con3_set1, \
	.phy_dvfs_con3_set0	= con3_set0, \
	.phy_dvfs_con3_set1_mask	= con3_set1_mask, \
	.phy_dvfs_con3_set0_mask	= con3_set0_mask, \
}

/* PHY DVFS CON SFR BIT DEFINITION */

/* (0x0)|(1<<31)|(1<<27)|(0x3<<24) */
#define PHY_DVFS_CON0_SET1_MASK	0x8B000000

/* (0x0)|(1<<30)|(1<<27)|(0x3<<24) */
#define PHY_DVFS_CON0_SET0_MASK	0x4B000000

/* (0x0)|(0x1F<<24)|(0xFF<<8) */
#define PHY_DVFS_CON2_SET1_MASK	0x1F00FF00

/* (0x0)|(0x1F<<16)|(0xFF<<0) */
#define PHY_DVFS_CON2_SET0_MASK	0x001F00FF

/* (0x0)|(1<<30)|(1<<29)|(0x7<<23) */
#define PHY_DVFS_CON3_SET1_MASK	0x63800000

/* (0x0)|(1<<31)|(1<<28)|(0x7<<20) */
#define PHY_DVFS_CON3_SET0_MASK	0x90700000

static struct dmc_drex_dfs_mif_table *dfs_drex_mif_table;

static struct dmc_drex_dfs_mif_table dfs_drex_mif_table_samsung_4gb_all_bank[] = {
	/* RfcPb, TimingRow, TimingData, TimingPower, Rdfetch */
	DREX_TIMING_PARA(0x19, 0x36598692, 0x4740185E, 0x4C3A4746, 0x3),
	DREX_TIMING_PARA(0x16, 0x304885D0, 0x3630185E, 0x44333636, 0x2),
	DREX_TIMING_PARA(0x15, 0x2C48758F, 0x3630184E, 0x402F3635, 0x2),
	DREX_TIMING_PARA(0x11, 0x2536648D, 0x3630184E, 0x34283535, 0x2),
	DREX_TIMING_PARA(0xD,  0x1C35538A, 0x2620183E, 0x281F2425, 0x2),
	DREX_TIMING_PARA(0xB,  0x192442C8, 0x2620182E, 0x201F2325, 0x2),
	DREX_TIMING_PARA(0x9,  0x19233247, 0x2620182E, 0x1C1F2325, 0x2),
	DREX_TIMING_PARA(0x6,  0x192331C5, 0x2620182E, 0x141F2225, 0x2),
};

static struct dmc_drex_dfs_mif_table dfs_drex_mif_table_samsung_4gb_per_bank[] = {
	/* RfcPb, TimingRow, TimingData, TimingPower, Rdfetch */
	DREX_TIMING_PARA(0x19, 0x36588652, 0x4740185E, 0x4C3A4746, 0x3),
	DREX_TIMING_PARA(0x16, 0x30478590, 0x3630185E, 0x44333636, 0x2),
	DREX_TIMING_PARA(0x15, 0x2C47754F, 0x3630184E, 0x402F3635, 0x2),
	DREX_TIMING_PARA(0x11, 0x2536644D, 0x3630184E, 0x34283535, 0x2),
	DREX_TIMING_PARA(0xD,  0x1C34534A, 0x2620183E, 0x281F2425, 0x2),
	DREX_TIMING_PARA(0xB,  0x192442C8, 0x2620182E, 0x201F2325, 0x2),
	DREX_TIMING_PARA(0x9,  0x19233247, 0x2620182E, 0x1C1F2325, 0x2),
	DREX_TIMING_PARA(0x6,  0x19223185, 0x2620182E, 0x141F2225, 0x2),
};

static struct dmc_drex_dfs_mif_table *dfs_drex_mif_table_samsung_4gb[] = {
	dfs_drex_mif_table_samsung_4gb_all_bank,
	dfs_drex_mif_table_samsung_4gb_per_bank,
};

static struct dmc_drex_dfs_mif_table dfs_drex_mif_table_samsung_6gb_all_bank[] = {
	/* RfcPb, TimingRow, TimingData, TimingPower, Rdfetch */
	DREX_TIMING_PARA(0x26, 0x57598692, 0x4740185E, 0x4C5B4746, 0x3),
	DREX_TIMING_PARA(0x21, 0x4D4885D0, 0x3630185E, 0x44513636, 0x2),
	DREX_TIMING_PARA(0x1F, 0x4748758F, 0x3630184E, 0x404A3635, 0x2),
	DREX_TIMING_PARA(0x1A, 0x3B36648D, 0x3630184E, 0x343E3535, 0x2),
	DREX_TIMING_PARA(0x13, 0x2C35538A, 0x2620183E, 0x282E2425, 0x2),
	DREX_TIMING_PARA(0x10, 0x242442C8, 0x2620182E, 0x20262325, 0x2),
	DREX_TIMING_PARA(0xD,  0x1D233247, 0x2620182E, 0x1C1F2325, 0x2),
	DREX_TIMING_PARA(0x9,  0x192331C5, 0x2620182E, 0x141F2225, 0x2),
};

static struct dmc_drex_dfs_mif_table dfs_drex_mif_table_samsung_6gb_per_bank[] = {
	/* RfcPb, TimingRow, TimingData, TimingPower, Rdfetch */
	DREX_TIMING_PARA(0x26, 0x57588652, 0x4740185E, 0x4C5B4746, 0x3),
	DREX_TIMING_PARA(0x21, 0x4D478590, 0x3630185E, 0x44513636, 0x2),
	DREX_TIMING_PARA(0x1F, 0x4747754F, 0x3630184E, 0x404A3635, 0x2),
	DREX_TIMING_PARA(0x1A, 0x3B36644D, 0x3630184E, 0x343E3535, 0x2),
	DREX_TIMING_PARA(0x13, 0x2C34534A, 0x2620183E, 0x282E2425, 0x2),
	DREX_TIMING_PARA(0x10, 0x242442C8, 0x2620182E, 0x20262325, 0x2),
	DREX_TIMING_PARA(0xD,  0x1D233247, 0x2620182E, 0x1C1F2325, 0x2),
	DREX_TIMING_PARA(0x9,  0x19223185, 0x2620182E, 0x141F2225, 0x2),
};

static struct dmc_drex_dfs_mif_table *dfs_drex_mif_table_samsung_6gb[] = {
	dfs_drex_mif_table_samsung_6gb_all_bank,
	dfs_drex_mif_table_samsung_6gb_per_bank,
};

static struct dmc_drex_dfs_mif_table dfs_drex_mif_table_samsung_8gb_all_bank[] = {
	/* RfcPb, TimingRow, TimingData, TimingPower, Rdfetch */
	DREX_TIMING_PARA(0x26, 0x57598692, 0x4740185E, 0x4C5B4746, 0x3),
	DREX_TIMING_PARA(0x21, 0x4D4885D0, 0x3630185E, 0x44513636, 0x2),
	DREX_TIMING_PARA(0x1F, 0x4748758F, 0x3630184E, 0x404A3635, 0x2),
	DREX_TIMING_PARA(0x1A, 0x3B36648D, 0x3630184E, 0x343E3535, 0x2),
	DREX_TIMING_PARA(0x13, 0x2C35538A, 0x2620183E, 0x282E2425, 0x2),
	DREX_TIMING_PARA(0x10, 0x242442C8, 0x2620182E, 0x20262325, 0x2),
	DREX_TIMING_PARA(0xD,  0x1D233247, 0x2620182E, 0x1C1F2325, 0x2),
	DREX_TIMING_PARA(0x9,  0x192331C5, 0x2620182E, 0x141F2225, 0x2),
};

static struct dmc_drex_dfs_mif_table dfs_drex_mif_table_samsung_8gb_per_bank[] = {
	/* RfcPb, TimingRow, TimingData, TimingPower, Rdfetch */
	DREX_TIMING_PARA(0x26, 0x57588652, 0x4740185E, 0x4C5B4746, 0x3),
	DREX_TIMING_PARA(0x21, 0x4D478590, 0x3630185E, 0x44513636, 0x2),
	DREX_TIMING_PARA(0x1F, 0x4747754F, 0x3630184E, 0x404A3635, 0x2),
	DREX_TIMING_PARA(0x1A, 0x3B36644D, 0x3630184E, 0x343E3535, 0x2),
	DREX_TIMING_PARA(0x13, 0x2C34534A, 0x2620183E, 0x282E2425, 0x2),
	DREX_TIMING_PARA(0x10, 0x242442C8, 0x2620182E, 0x20262325, 0x2),
	DREX_TIMING_PARA(0xD,  0x1D233247, 0x2620182E, 0x1C1F2325, 0x2),
	DREX_TIMING_PARA(0x9,  0x19223185, 0x2620182E, 0x141F2225, 0x2),
};

static struct dmc_drex_dfs_mif_table *dfs_drex_mif_table_samsung_8gb[] = {
	dfs_drex_mif_table_samsung_8gb_all_bank,
	dfs_drex_mif_table_samsung_8gb_per_bank,
};

static struct dmc_drex_dfs_mif_table dfs_drex_mif_table_hnon_samsung_all_bank[] = {
	/* RfcPb, TimingRow, TimingData, TimingPower, Rdfetch */
	DREX_TIMING_PARA(0x26, 0x57598692, 0x4740185E, 0x545B4746, 0x3),
	DREX_TIMING_PARA(0x21, 0x4D4885D0, 0x3630185E, 0x4C513636, 0x2),
	DREX_TIMING_PARA(0x1F, 0x4748758F, 0x3630184E, 0x444A3635, 0x2),
	DREX_TIMING_PARA(0x1A, 0x3B36648D, 0x3630184E, 0x383E3535, 0x2),
	DREX_TIMING_PARA(0x13, 0x2C35538A, 0x2620183E, 0x2C2E2425, 0x2),
	DREX_TIMING_PARA(0x10, 0x242442C8, 0x2620182E, 0x24262325, 0x2),
	DREX_TIMING_PARA(0xD,  0x1D233247, 0x2620182E, 0x1C1F2325, 0x2),
	DREX_TIMING_PARA(0x9,  0x192331C5, 0x2620182E, 0x141F2225, 0x2),
};

static struct dmc_drex_dfs_mif_table dfs_drex_mif_table_hnon_samsung_per_bank[] = {
	/* RfcPb, TimingRow, TimingData, TimingPower, Rdfetch */
	DREX_TIMING_PARA(0x26, 0x57588652, 0x4740185E, 0x545B4746, 0x3),
	DREX_TIMING_PARA(0x21, 0x4D478590, 0x3630185E, 0x4C513636, 0x2),
	DREX_TIMING_PARA(0x1F, 0x4747754F, 0x3630184E, 0x444A3635, 0x2),
	DREX_TIMING_PARA(0x1A, 0x3B36644D, 0x3630184E, 0x383E3535, 0x2),
	DREX_TIMING_PARA(0x13, 0x2C34534A, 0x2620183E, 0x2C2E2425, 0x2),
	DREX_TIMING_PARA(0x10, 0x242442C8, 0x2620182E, 0x24262325, 0x2),
	DREX_TIMING_PARA(0xD,  0x1D233247, 0x2620182E, 0x1C1F2325, 0x2),
	DREX_TIMING_PARA(0x9,  0x19223185, 0x2620182E, 0x141F2225, 0x2),
};

static struct dmc_drex_dfs_mif_table *dfs_drex_mif_table_hnon_samsung[] = {
	dfs_drex_mif_table_hnon_samsung_all_bank,
	dfs_drex_mif_table_hnon_samsung_per_bank,
};

static struct dmc_drex_dfs_mif_table dfs_drex_mif_table_mnon_samsung_per_bank[] = {
	/* RfcPb, TimingRow, TimingData, TimingPower, Rdfetch */
	DREX_TIMING_PARA(0x19, 0x36588652, 0x4740185E, 0x543A4746, 0x3),
	DREX_TIMING_PARA(0x16, 0x30478590, 0x3630185E, 0x4C333636, 0x2),
	DREX_TIMING_PARA(0x15, 0x2C47754F, 0x3630184E, 0x442F3635, 0x2),
	DREX_TIMING_PARA(0x11, 0x2536644D, 0x3630184E, 0x38283535, 0x2),
	DREX_TIMING_PARA(0xD,  0x1C34534A, 0x2620183E, 0x2C1F2425, 0x2),
	DREX_TIMING_PARA(0xB,  0x192442C8, 0x2620182E, 0x241F2325, 0x2),
	DREX_TIMING_PARA(0x9,  0x19233247, 0x2620182E, 0x1C1F2325, 0x2),
	DREX_TIMING_PARA(0x6,  0x19223185, 0x2620182E, 0x141F2225, 0x2),
};

static struct dmc_drex_dfs_mif_table dfs_drex_mif_table_mnon_samsung_all_bank[] = {
	/* RfcPb, TimingRow, TimingData, TimingPower, Rdfetch */
	DREX_TIMING_PARA(0x19, 0x36598692, 0x4740185E, 0x543A4746, 0x3),
	DREX_TIMING_PARA(0x16, 0x304885D0, 0x3630185E, 0x4C333636, 0x2),
	DREX_TIMING_PARA(0x15, 0x2C48758F, 0x3630184E, 0x442F3635, 0x2),
	DREX_TIMING_PARA(0x11, 0x2536648D, 0x3630184E, 0x38283535, 0x2),
	DREX_TIMING_PARA(0xD,  0x1C35538A, 0x2620183E, 0x2C1F2425, 0x2),
	DREX_TIMING_PARA(0xB,  0x192442C8, 0x2620182E, 0x241F2325, 0x2),
	DREX_TIMING_PARA(0x9,  0x19233247, 0x2620182E, 0x1C1F2325, 0x2),
	DREX_TIMING_PARA(0x6,  0x192331C5, 0x2620182E, 0x141F2225, 0x2),
};

static struct dmc_drex_dfs_mif_table *dfs_drex_mif_table_mnon_samsung[] = {
	dfs_drex_mif_table_mnon_samsung_all_bank,
	dfs_drex_mif_table_mnon_samsung_per_bank,
};

static struct dmc_phy_dfs_mif_table dfs_phy_mif_table[] = {
	/* 825MHz */
	PHY_DVFS_CON(0x8A000000, 0x49000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E002100, 0x000E0021,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	/* 728MHz */
	PHY_DVFS_CON(0x8A000000, 0x49000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E002100, 0x000E0021,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	/* 667MHz */
	PHY_DVFS_CON(0x0A000000, 0x09000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E001100, 0x000E0011,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	/* 559MHz */
	PHY_DVFS_CON(0x8A000000, 0x49000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E001100, 0x000E0011,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	/* Turn the dll off below this */
	/* 416MHz */
	PHY_DVFS_CON(0x0A000000, 0x09000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E000100, 0x000E0001,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	/* 338MHz */
	PHY_DVFS_CON(0x0A000000, 0x09000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E009100, 0x000E0091,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	/* 273MHz */
	PHY_DVFS_CON(0x0A000000, 0x09000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E009100, 0x000E0091,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	/* 200MHz */
	PHY_DVFS_CON(0x0A000000, 0x09000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0E009100, 0x000E0091,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
};

enum devfreq_mif_thermal_autorate {
	RATE_ONE = 0x000C0065,
	RATE_TWO = 0x001800CA,
	RATE_FOUR = 0x00300194,
	RATE_HALF = 0x00060032,
	RATE_QUARTER = 0x00030019,
};

static int exynos7580_devfreq_mif_set_dll(struct devfreq_data_mif *data,
						int target_idx)
{
	uint32_t reg;
	uint32_t value;

	if (target_idx > DLL_LOCK_LV || target_idx == MIF_LV2) {
		/* Write stored lock value to ctrl_force register */
		reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
		reg &= ~(PHY_CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
		if (target_idx == MIF_LV2)
			value = data->lock_value_825;
		else
			value = data->lock_value_416;

		reg |= (value << CTRL_FORCE_SHIFT);
		__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);
	}

	return 0;
}

static int exynos7580_devfreq_mif_timing_set(struct devfreq_data_mif *data,
		int target_idx)
{
	struct dmc_drex_dfs_mif_table *cur_drex_param;
	struct dmc_phy_dfs_mif_table *cur_phy_param;
	uint32_t reg;
	bool timing_set_num;

	cur_drex_param = &dfs_drex_mif_table[target_idx];
	cur_phy_param = &dfs_phy_mif_table[target_idx];

	/* Check what timing_set_num needs to be used */
	timing_set_num = (((__raw_readl(data->base_drex + DREX_PHYSTATUS)
						>> TIMING_SET_SW_SHIFT) & 0x1) == 0);

	if (timing_set_num) {
		/* DREX */
		reg = __raw_readl(data->base_drex + DREX_TIMINGRFCPB);
		reg &= ~(DREX_TIMINGRFCPB_SET1_MASK);
		reg |= ((cur_drex_param->drex_timingrfcpb << 8)
					& DREX_TIMINGRFCPB_SET1_MASK);
		__raw_writel(reg, data->base_drex + DREX_TIMINGRFCPB);
		__raw_writel(cur_drex_param->drex_timingrow,
				data->base_drex + DREX_TIMINGROW_1);
		__raw_writel(cur_drex_param->drex_timingdata,
				data->base_drex + DREX_TIMINGDATA_1);
		__raw_writel(cur_drex_param->drex_timingpower,
				data->base_drex + DREX_TIMINGPOWER_1);
		__raw_writel(cur_drex_param->drex_rd_fetch,
				data->base_drex + DREX_RDFETCH_1);
		reg = __raw_readl(data->base_mif + 0x1004);
		reg |= 0x1;
		__raw_writel(reg, data->base_mif + 0x1004);

		/* PHY */
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON0);
		reg &= ~(cur_phy_param->phy_dvfs_con0_set1_mask);
		reg |= cur_phy_param->phy_dvfs_con0_set1;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON0);

		/* Check whether DLL needs to be turned on or off */
		exynos7580_devfreq_mif_set_dll(data, target_idx);

		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON2);
		reg &= ~(cur_phy_param->phy_dvfs_con2_set1_mask);
		reg |= cur_phy_param->phy_dvfs_con2_set1;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON2);
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON3);
		reg &= ~(cur_phy_param->phy_dvfs_con3_set1_mask);
		reg |= cur_phy_param->phy_dvfs_con3_set1;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON3);
	} else {
		/* DREX */
		reg = __raw_readl(data->base_drex + DREX_TIMINGRFCPB);
		reg &= ~(DREX_TIMINGRFCPB_SET0_MASK);
		reg |= (cur_drex_param->drex_timingrfcpb
					& DREX_TIMINGRFCPB_SET0_MASK);
		__raw_writel(reg, data->base_drex + DREX_TIMINGRFCPB);
		__raw_writel(cur_drex_param->drex_timingrow,
				data->base_drex + DREX_TIMINGROW_0);
		__raw_writel(cur_drex_param->drex_timingdata,
				data->base_drex + DREX_TIMINGDATA_0);
		__raw_writel(cur_drex_param->drex_timingpower,
				data->base_drex + DREX_TIMINGPOWER_0);
		__raw_writel(cur_drex_param->drex_rd_fetch,
				data->base_drex + DREX_RDFETCH_0);
		reg = __raw_readl(data->base_mif + 0x1004);
		reg &= ~0x1;
		__raw_writel(reg, data->base_mif + 0x1004);

		/* PHY */
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON0);
		reg &= ~(cur_phy_param->phy_dvfs_con0_set0_mask);
		reg |= cur_phy_param->phy_dvfs_con0_set0;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON0);

		/* Check whether DLL needs to be turned on or off */
		exynos7580_devfreq_mif_set_dll(data, target_idx);

		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON2);
		reg &= ~(cur_phy_param->phy_dvfs_con2_set0_mask);
		reg |= cur_phy_param->phy_dvfs_con2_set0;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON2);
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON3);
		reg &= ~(cur_phy_param->phy_dvfs_con3_set0_mask);
		reg |= cur_phy_param->phy_dvfs_con3_set0;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON3);
	}
	return 0;
}

int exynos7_devfreq_mif_init_parameter(struct devfreq_data_mif *data)
{
	unsigned int vendor;
	unsigned int density;
	unsigned int bank;

	data->base_drex = ioremap(DREX_BASE, SZ_64K);
	if (!data->base_drex) {
		pr_err("DEVFREQ(MIF) : %s failed to ioremap\n", "base_drex");
		goto out;
	}

	data->base_lpddr_phy = ioremap(PHY_BASE, SZ_64K);
	if (!data->base_lpddr_phy) {
		pr_err("DEVFREQ(MIF) : %s failed to ioremap\n", "base_lpddr_phy");
		goto err_lpddr;
	}

	data->base_mif = ioremap(CMU_MIF_BASE, SZ_64K);
	if (!data->base_mif) {
		pr_err("DEVFREQ(MIF) : %s failed to ioremap\n", "base_mif");
		goto err_mif;
	}

	/* Enable PAUSE */
	__raw_writel(0x1, data->base_mif + CLK_PAUSE);


	vendor = __raw_readl(EXYNOS_PMU_DREX_CALIBRATION1) & 0xff;
	density = __raw_readl(EXYNOS_PMU_DREX_CALIBRATION2) & 0xff;

	__raw_writel(0x9001400, data->base_drex + 0x10);
	bank = (__raw_readl(data->base_drex + 0x4) >> 27) & 0x1;

	/*
	 * Vendor 0xff : Micron 0x6 : Hynix, 0x3 : Elpida, 0x1 : Samsung
	 * Bank 0 : All bank, 1 : Per bank
	 * Density : 0x6 : 4gb, 0x7 : 8gb, 0xE : 6gb
	 */
	if (vendor == 0xff) {
		if (density == 0x7)
			dfs_drex_mif_table = dfs_drex_mif_table_hnon_samsung[bank];
		else
			panic("Wrong MCP density : %x\n", density);
	}else if (vendor == 0x6) {
		dfs_drex_mif_table = dfs_drex_mif_table_hnon_samsung[bank];
	} else if (vendor == 0x3) {
		dfs_drex_mif_table = dfs_drex_mif_table_mnon_samsung[bank];
	} else if (vendor == 0x1) {
		if (density == 0x6)
			dfs_drex_mif_table = dfs_drex_mif_table_samsung_4gb[bank];
		else if (density == 0x7)
			dfs_drex_mif_table = dfs_drex_mif_table_samsung_8gb[bank];
		else if (density == 0xE)
			dfs_drex_mif_table = dfs_drex_mif_table_samsung_6gb[bank];
		else
			panic("Wrong MCP density : %x\n", density);
	} else {
		panic("Wrong MCP vendor : %x\n", vendor);
	}

	return 0;

err_mif:
	iounmap(data->base_lpddr_phy);
err_lpddr:
	iounmap(data->base_drex);
out:
	return -ENOMEM;
}

static int exynos7_devfreq_mif_set_volt(struct devfreq_data_mif *data,
					unsigned long volt,
					unsigned long volt_range)
{
	data->old_volt = volt;
	return regulator_set_voltage(data->vdd_mif, volt, volt_range);
}

static void exynos7580_devfreq_waiting_pause(struct devfreq_data_mif *data)
{
	unsigned int timeout = 1000;

	while ((__raw_readl(data->base_mif + CLK_PAUSE) & CLK_PAUSE_MASK)) {
		if (timeout == 0) {
			pr_err("DEVFREQ(MIF) : timeout to wait pause completion\n");
			return;
		}
		udelay(1);
		timeout--;
	}
}

static int exynos7_devfreq_mif_change_phy(struct devfreq_data_mif *data,
					int target_idx, bool media_pll)
{
	exynos7580_devfreq_mif_timing_set(data, target_idx);

	if (media_pll)
		/* Set MUX_CLK2X_PHY_B, MUX_CLKM_PHY_B as 1 to use MEDIA_PLL */
		__raw_writel(0x100010, EXYNOS7580_MUX_SEL_MIF4);
	else
		/* Set MUX_CLK2X_PHY_B, MUX_CLKM_PHY_C as 0 to use MEM0_PLL */
		__raw_writel(0x0, EXYNOS7580_MUX_SEL_MIF4);

	exynos7580_devfreq_waiting_pause(data);
	exynos7580_devfreq_waiting_mux(data->base_mif + CLK_MUX_STAT_MIF4, 4);
	exynos7580_devfreq_waiting_mux(data->base_mif + CLK_MUX_STAT_MIF4, 20);

	pr_debug("MEDIA_PLL(667MHz) is used for source of MUX_CLK2X_PHY and MUX_CLKM_PHY\n");

	return 0;
}

static int exynos7_devfreq_mif_change_aclk(struct devfreq_data_mif *data,
						int target_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;
#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *pm_domain;
#endif
	uint32_t reg;

	reg = __raw_readl(data->base_mif + CLK_MUX_STAT_MIF5);
	reg &= MUX_MASK;

	for (i = 0; i < ARRAY_SIZE(devfreq_clk_mif_info_list); i++) {
		clk_info = &devfreq_clk_mif_info_list[i][target_idx];
		clk_states = clk_info->states;

#ifdef CONFIG_PM_RUNTIME
		pm_domain = devfreq_mif_pm_domain[i].pm_domain;
		if (pm_domain) {
			mutex_lock(&pm_domain->access_lock);
			if ((__raw_readl(pm_domain->base + 0x4) & LOCAL_PWR_CFG) == 0) {
				mutex_unlock(&pm_domain->access_lock);
				continue;
			}
		}
#endif
		if (reg & 0x2)
			/* If source of MUX_ACLK_MIF is media_pll, setting DIV and MUX orderly */
			clk_set_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk, clk_info->freq);

		if (clk_states) {
			for (j = 0; j < clk_states->state_count; ++j) {
				clk_set_parent(devfreq_mif_clk[clk_states->state[j].clk_idx].clk,
						devfreq_mif_clk[clk_states->state[j].parent_clk_idx].clk);
			}
		}

		/* If source of MUX_ACLK_MIF is bus_pll, setting MUX and DIV orderly */
		clk_set_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk, clk_info->freq);

		pr_debug("MIF clk name: %s, set_rate: %lu, get_rate: %lu\n",
				devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk_name,
				clk_info->freq, clk_get_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk));
#ifdef CONFIG_PM_RUNTIME
		if (pm_domain)
			mutex_unlock(&pm_domain->access_lock);
#endif
	}

	return 0;
}

static void exynos7_devfreq_mif_set_control(struct devfreq_data_mif *data, bool pre)
{
	unsigned int gating, ref;

	/* Get PHY ctrl_ref value */
	ref = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
	ref &= ~(0xF << 1);

	if (pre) {
		/* Disable Clock Gating */
		gating = 0x0;

		/* Set PHY ctrl_ref as 0xE */
		ref |= (0xE << 1);

		ref &= ~(PHY_CTRL_START_POINT_MASK << CTRL_START_POINT_SHIFT);
		ref |= ((data->lock_value_825 >> 2) << CTRL_START_POINT_SHIFT);
	} else {
		/* Enable Clock Gating */
		gating = 0x3FF;

		/* Set PHY ctrl_ref as 0xF */
		ref |= (0xF << 1);
	}

	__raw_writel(gating, data->base_drex + DREX_CGCONTROL);
	__raw_writel(ref, data->base_lpddr_phy + PHY_MDLL_CON0);
}

static int exynos7_devfreq_mif_set_freq(struct devfreq_data_mif *data,
					int target_idx,
					int old_idx)
{
	int pll_safe_idx;
	unsigned int voltage, safe_voltage = 0;
	struct clk *mem0_pll;

	mem0_pll = devfreq_mif_clk[MEM0_PLL].clk;

	pll_safe_idx = data->pll_safe_idx;

	/* Enable PASUE, Disable Clock Gating, ctrl_ref as 0xE */
	exynos7_devfreq_mif_set_control(data, true);

	/* Find the proper voltage to be set */
	if ((pll_safe_idx < old_idx) && (pll_safe_idx < target_idx)) {
		safe_voltage = devfreq_mif_opp_list[pll_safe_idx].volt;
#ifdef CONFIG_EXYNOS_THERMAL
		safe_voltage = get_limit_voltage(safe_voltage, data->volt_offset);
#endif
	}

	voltage = devfreq_mif_opp_list[target_idx].volt;
#ifdef CONFIG_EXYNOS_THERMAL
	voltage = get_limit_voltage(voltage, data->volt_offset);
#endif

	/* Set voltage */
	if (old_idx > target_idx && !safe_voltage) {
		exynos7_devfreq_mif_set_volt(data, voltage, REGULATOR_MAX_MICROVOLT);
		register_match_abb(ID_MIF, devfreq_mif_opp_list[target_idx].freq);
		set_match_abb(ID_MIF, get_match_abb(ID_MIF, devfreq_mif_opp_list[target_idx].freq));
	} else if (safe_voltage) {
		exynos7_devfreq_mif_set_volt(data, safe_voltage, REGULATOR_MAX_MICROVOLT);
		register_match_abb(ID_MIF, devfreq_mif_opp_list[pll_safe_idx].freq);
		set_match_abb(ID_MIF, get_match_abb(ID_MIF, devfreq_mif_opp_list[pll_safe_idx].freq));
	}

	/* old level to safe level */
	if (old_idx < pll_safe_idx) { /* Down */
		exynos7_devfreq_mif_change_aclk(data, pll_safe_idx);
		exynos7_devfreq_mif_change_phy(data, data->pll_safe_idx, true);
	} else if (old_idx > pll_safe_idx) { /* Up */
		exynos7_devfreq_mif_change_phy(data, data->pll_safe_idx, true);
		exynos7_devfreq_mif_change_aclk(data, pll_safe_idx);
	}

	/* set the pll to target level*/
	clk_set_rate(mem0_pll, sclk_clk_phy[target_idx].freq);

	/* safe level to target level */
	if (target_idx > pll_safe_idx) { /* Down */
		exynos7_devfreq_mif_change_aclk(data, target_idx);
		exynos7_devfreq_mif_change_phy(data, target_idx, false);
	} else if (target_idx < pll_safe_idx) { /* Up */
		exynos7_devfreq_mif_change_phy(data, target_idx, false);
		exynos7_devfreq_mif_change_aclk(data, target_idx);
	}

	/* Set voltage */
	if (old_idx < target_idx ||
	    ((old_idx > target_idx) && safe_voltage)) {
		register_match_abb(ID_MIF, devfreq_mif_opp_list[target_idx].freq);
		set_match_abb(ID_MIF, get_match_abb(ID_MIF, devfreq_mif_opp_list[target_idx].freq));
		exynos7_devfreq_mif_set_volt(data, voltage, REGULATOR_MAX_MICROVOLT);
	}

	/* Enable Clock Gating, ctrl_ref as 0xF */
	exynos7_devfreq_mif_set_control(data, false);

#ifdef DUMP_DVFS_CLKS
	exynos7_devfreq_dump_all_clks(devfreq_mif_clk, MIF_CLK_COUNT, "MIF");
#endif
	return 0;
}

static unsigned int mif_thermal_level_cs0;
static unsigned int mif_thermal_level_cs1;

static ssize_t mif_show_templvl_cs0(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", mif_thermal_level_cs0);
}
static ssize_t mif_show_templvl_cs1(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", mif_thermal_level_cs1);
}

static DEVICE_ATTR(templvl_cs0, 0644, mif_show_templvl_cs0, NULL);
static DEVICE_ATTR(templvl_cs1, 0644, mif_show_templvl_cs1, NULL);

static struct attribute *devfreq_mif_sysfs_entries[] = {
	&dev_attr_templvl_cs0.attr,
	&dev_attr_templvl_cs1.attr,
	NULL,
};

struct attribute_group devfreq_mif_attr_group = {
	.name	= "mif_temp_level",
	.attrs	= devfreq_mif_sysfs_entries,
};

static void exynos7_devfreq_swtrip(void)
{
#ifdef CONFIG_EXYNOS_SWTRIP
	char tmustate_string[20];
	char *envp[2];

	snprintf(tmustate_string, sizeof(tmustate_string), "TMUSTATE=%d", 3);
	envp[0] = tmustate_string;
	envp[1] = NULL;
	pr_err("DEVFREQ(MIF) : SW trip by MR4\n");
	kobject_uevent_env(&data_mif->dev->kobj, KOBJ_CHANGE, envp);
#endif
}

static void exynos7_devfreq_thermal_monitor(struct work_struct *work)
{
	unsigned int mrstatus, tmp_thermal_level, max_thermal_level = 0;
	unsigned int timingaref_value = RATE_ONE;
	unsigned int thermal_level_cs0, thermal_level_cs1;
	unsigned int polling_period;
	void __iomem *base_drex;

	base_drex = data_mif->base_drex;

	mutex_lock(&data_mif->lock);

	__raw_writel(0x09001000, base_drex + DREX_DIRECTCMD);
	mrstatus = __raw_readl(base_drex + DREX_MRSTATUS);
	tmp_thermal_level = (mrstatus & DREX_MRSTATUS_THERMAL_LV_MASK);
	if (tmp_thermal_level > max_thermal_level)
		max_thermal_level = tmp_thermal_level;

	thermal_level_cs0 = tmp_thermal_level;
	mif_thermal_level_cs0 = thermal_level_cs0;

	__raw_writel(0x09101000, base_drex + DREX_DIRECTCMD);
	mrstatus = __raw_readl(base_drex + DREX_MRSTATUS);
	tmp_thermal_level = (mrstatus & DREX_MRSTATUS_THERMAL_LV_MASK);
	if (tmp_thermal_level > max_thermal_level)
		max_thermal_level = tmp_thermal_level;

	thermal_level_cs1 = tmp_thermal_level;
	mif_thermal_level_cs1 = thermal_level_cs1;

	mutex_unlock(&data_mif->lock);

	switch (max_thermal_level) {
	case 0:
	case 1:
		timingaref_value = RATE_FOUR;
		polling_period = 500;
		break;
	case 2:
		timingaref_value = RATE_TWO;
		polling_period = 500;
		break;
	case 3:
		timingaref_value = RATE_ONE;
		polling_period = 500;
		break;
	case 4:
		timingaref_value = RATE_HALF;
		polling_period = 300;
		break;
	case 5:
		timingaref_value = RATE_QUARTER;
		polling_period = 100;
		break;

	case 6:
		exynos7_devfreq_swtrip();
		return;
	default:
		pr_err("DEVFREQ(MIF) : can't support memory thermal level\n");
		return;
	}

	__raw_writel(timingaref_value, base_drex + DREX_TIMINGAREF);

	exynos_ss_printk("[MIF]%d,%d;TIMINGAREF:0x%08x",
			thermal_level_cs0, thermal_level_cs1,
			__raw_readl(base_drex + DREX_TIMINGAREF));

	queue_delayed_work(system_freezable_wq, &exynos_devfreq_thermal_work,
			msecs_to_jiffies(polling_period));
}

void exynos7_devfreq_set_tREFI_1x(struct devfreq_data_mif *data)
{
	uint32_t timingaref_value;

	mutex_lock(&data->lock);
	timingaref_value = __raw_readl(data->base_drex + DREX_TIMINGAREF);
	if (timingaref_value == RATE_TWO)
		__raw_writel(RATE_ONE, data->base_drex + DREX_TIMINGAREF);
	mutex_unlock(&data->lock);
}

void exynos7_devfreq_init_thermal(void)
{
	INIT_DELAYED_WORK(&exynos_devfreq_thermal_work,
			exynos7_devfreq_thermal_monitor);

	queue_delayed_work(system_freezable_wq, &exynos_devfreq_thermal_work, msecs_to_jiffies(100));
}

int exynos7_devfreq_mif_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_mif_clk); i++) {
		devfreq_mif_clk[i].clk = clk_get(NULL, devfreq_mif_clk[i].clk_name);
		if (IS_ERR(devfreq_mif_clk[i].clk)) {
			pr_err("DEVFREQ(MIF) : %s can't get clock\n", devfreq_mif_clk[i].clk_name);
			return PTR_ERR(devfreq_mif_clk[i].clk);
		}
		pr_debug("MIF clk name: %s, rate: %luMHz\n",
				devfreq_mif_clk[i].clk_name, (clk_get_rate(devfreq_mif_clk[i].clk) + (MHZ-1))/MHZ);
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_clk_mif_info_list); i++)
		clk_prepare_enable(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk);

	return 0;
}

void exynos7580_devfreq_set_dll_lock_value(struct devfreq_data_mif *data,
						int target_idx)
{
	uint32_t reg;
	uint32_t lock_value;

	/* Sequence to get DLL Lock value
		1. Disable phy_cg_en (PHY clock gating)
		2. Enable ctrl_lock_rden
		3. Disable ctrl_lock_rden
		4. Read ctrl_lock_new
		5. Write ctrl_lock_new to ctrl_force and DREX_CALIBRATION0
		6. Enable phy_cg_en (PHY clock gating)
	*/

	/* 1. Disable PHY Clock Gating */
	reg = __raw_readl(data->base_drex + DREX_CGCONTROL);
	reg &= ~(0x1 << CG_EN_SHIFT);
	__raw_writel(reg, data->base_drex + DREX_CGCONTROL);

	/* 2. Enable ctrl_lock_rden */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
	reg |= (0x1 << CTRL_LOCK_RDEN_SHIFT);
	__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);

	/* 3. Disable ctrl_lock_rden */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
	reg &= ~(0x1 << CTRL_LOCK_RDEN_SHIFT);
	__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);

	/* 4. Read ctrl_lock_new */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON1);
	lock_value = (reg >> CTRL_LOCK_NEW_SHIFT) & PHY_CTRL_LOCK_NEW_MASK;
	data->lock_value_825 = lock_value;

	/* 5. Write ctrl_lock_new to ctrl_force and DREX_CALIBRATION4 */
	__raw_writel(data->lock_value_825, EXYNOS_PMU_DREX_CALIBRATION4);

	/* 6. Enable PHY Clock Gating */
	reg = __raw_readl(data->base_drex + DREX_CGCONTROL);
	reg |= (0x1 << CG_EN_SHIFT);
	__raw_writel(reg, data->base_drex + DREX_CGCONTROL);

	/* Change frequency to 416MHz from 825MHz */
	exynos7_devfreq_mif_set_freq(data, DLL_LOCK_LV, MIF_LV0);

	/* Sequence to get DLL Lock value
		1. Disable phy_cg_en (PHY clock gating)
		2. Enable ctrl_lock_rden
		3. Disable ctrl_lock_rden
		4. Read ctrl_lock_new
		5. Write ctrl_lock_new to ctrl_force and DREX_CALIBRATION0
		6. Enable phy_cg_en (PHY clock gating)
	*/

	/* 1. Disable PHY Clock Gating */
	reg = __raw_readl(data->base_drex + DREX_CGCONTROL);
	reg &= ~(0x1 << CG_EN_SHIFT);
	__raw_writel(reg, data->base_drex + DREX_CGCONTROL);

	/* 2. Enable ctrl_lock_rden */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
	reg |= (0x1 << CTRL_LOCK_RDEN_SHIFT);
	__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);

	/* 3. Disable ctrl_lock_rden */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
	reg &= ~(0x1 << CTRL_LOCK_RDEN_SHIFT);
	__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);

	/* 4. Read ctrl_lock_new */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON1);
	lock_value = (reg >> CTRL_LOCK_NEW_SHIFT) & PHY_CTRL_LOCK_NEW_MASK;
	data->lock_value_416 = lock_value;

	/* 5. Write ctrl_lock_new to ctrl_force and DREX_CALIBRATION0 */
	__raw_writel(lock_value, EXYNOS_PMU_DREX_CALIBRATION0);

	/* 6. Enable PHY Clock Gating */
	reg = __raw_readl(data->base_drex + DREX_CGCONTROL);
	reg |= (0x1 << CG_EN_SHIFT);
	__raw_writel(reg, data->base_drex + DREX_CGCONTROL);

	/* Change frequency to 825MHz from 416MHz */
	exynos7_devfreq_mif_set_freq(data, MIF_LV0, DLL_LOCK_LV);
}

#ifdef CONFIG_EXYNOS_THERMAL
int exynos7_devfreq_mif_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_mif *data = container_of(nb, struct devfreq_data_mif, tmu_notifier);
	unsigned int set_volt;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		mutex_lock(&data->lock);

		if (*on && data->volt_offset)
			goto out;

		if (!*on && !data->volt_offset)
			goto out;

		if (*on) {
			data->volt_offset = COLD_VOLT_OFFSET;
			set_volt = get_limit_voltage(data->old_volt, data->volt_offset);
		} else {
			data->volt_offset = 0;
			set_volt = get_limit_voltage(data->old_volt - COLD_VOLT_OFFSET, data->volt_offset);
		}

		exynos7_devfreq_mif_set_volt(data, set_volt, REGULATOR_MAX_MICROVOLT);
out:
		mutex_unlock(&data->lock);
	}

	return NOTIFY_OK;
}
#endif

int exynos7580_devfreq_mif_init(void *data)
{
	data_mif = (struct devfreq_data_mif *)data;

	data_mif->max_state = MIF_LV_COUNT;

	data_mif->mif_set_freq = exynos7_devfreq_mif_set_freq;
	data_mif->mif_set_volt = NULL;

	exynos7_devfreq_mif_init_clock();
	exynos7_devfreq_mif_init_parameter(data_mif);
	data_mif->pll_safe_idx = MIF_LV2;
#ifdef CONFIG_EXYNOS_THERMAL
	data_mif->tmu_notifier.notifier_call = exynos7_devfreq_mif_tmu_notifier;
#endif

#ifdef CONFIG_PM_RUNTIME
	if (exynos_devfreq_init_pm_domain(devfreq_mif_pm_domain,
				ARRAY_SIZE(devfreq_mif_pm_domain)))
		return -EINVAL;
#endif

	return 0;
}

int exynos7580_devfreq_mif_deinit(void *data)
{
	return 0;
}
/* end of MIF related function */
DEVFREQ_INIT_OF_DECLARE(exynos7580_devfreq_int_init, "samsung,exynos7-devfreq-int", exynos7580_devfreq_int_init);
DEVFREQ_INIT_OF_DECLARE(exynos7580_devfreq_mif_init, "samsung,exynos7-devfreq-mif", exynos7580_devfreq_mif_init);
DEVFREQ_INIT_OF_DECLARE(exynos7580_devfreq_isp_init, "samsung,exynos7-devfreq-isp", exynos7580_devfreq_isp_init);
DEVFREQ_DEINIT_OF_DECLARE(exynos7580_devfreq_deinit, "samsung,exynos7-devfreq-mif", exynos7580_devfreq_mif_deinit);
