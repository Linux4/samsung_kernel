/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>

#include <mach/devfreq.h>
#include <mach/regs-clock-exynos3475.h>
#include <mach/regs-pmu-exynos3475.h>
#include <mach/tmu.h>

#include <plat/cpu.h>

#include "devfreq_exynos.h"
#include "governor.h"

static struct workqueue_struct *devfreq_mif_thermal_wq;
struct devfreq_thermal_work devfreq_mif_work = {
	.polling_period = 200,
};
static unsigned int calib_val;

/* =========== INT related struct */
enum devfreq_int_idx {
	INT_LV0,
	INT_LV1,
	INT_LV2,
	INT_LV_COUNT,
};

enum devfreq_int_clk {
	AUD_PLL,
	INT_BUS_PLL,
	MOUT_BUS_PLL_USER,
	MOUT_MEDIA_PLL_USER,
	DOUT_ACLK_BUS0_333,
	MOUT_ACLK_BUS0_333,
	DOUT_ACLK_MFCMSCL_333,
	MOUT_ACLK_MFCMSCL_333,
	DOUT_ACLK_MFCMSCL_200,
	MOUT_ACLK_MFCMSCL_200,
	DOUT_ACLK_IMEM_266,
	DOUT_ACLK_IMEM_200,
	DOUT_ACLK_BUS2_333,
	MOUT_ACLK_BUS2_333,
	DOUT_ACLK_FSYS_200,
	DOUT_ACLK_ISP_300,
	MOUT_ACLK_ISP_300,
	INT_CLK_COUNT,
};

struct devfreq_clk_list devfreq_int_clk[INT_CLK_COUNT] = {
	{"aud_pll",},
	{"bus_pll",},
	{"mout_bus_pll_user",},
	{"mout_media_pll_user",},
	{"dout_aclk_bus0_333",},
	{"mout_aclk_bus0_333",},
	{"dout_aclk_mfcmscl_333",},
	{"mout_aclk_mfcmscl_333",},
	{"dout_aclk_mfcmscl_200",},
	{"mout_aclk_mfcmscl_200",},
	{"dout_aclk_imem_266",},
	{"dout_aclk_imem_200",},
	{"dout_aclk_bus2_333",},
	{"mout_aclk_bus2_333",},
	{"dout_aclk_fsys_200",},
	{"dout_aclk_isp_300",},
	{"mout_aclk_isp_300",},
};

/* ACLK_BUS0_333 */
struct devfreq_clk_state aclk_bus0_333_media_pll[] = {
	{MOUT_ACLK_BUS0_333,	MOUT_MEDIA_PLL_USER},
};

struct devfreq_clk_states aclk_bus0_333_media_pll_list = {
	.state = aclk_bus0_333_media_pll,
	.state_count = ARRAY_SIZE(aclk_bus0_333_media_pll),
};

struct devfreq_clk_state aclk_bus0_333_bus_pll[] = {
	{MOUT_ACLK_BUS0_333,	MOUT_BUS_PLL_USER},
};

struct devfreq_clk_states aclk_bus0_333_bus_pll_list = {
	.state = aclk_bus0_333_bus_pll,
	.state_count = ARRAY_SIZE(aclk_bus0_333_bus_pll),
};

struct devfreq_clk_info dout_aclk_bus0_333[] = {
	{INT_LV0,	333 * MHZ,	0,	&aclk_bus0_333_media_pll_list},
	{INT_LV1,	333 * MHZ,	0,	&aclk_bus0_333_media_pll_list},
	{INT_LV2,	276 * MHZ,	0,	&aclk_bus0_333_bus_pll_list},
};

/* ACLK_MFCMSCL_333 */
struct devfreq_clk_state aclk_mfcmscl_333_media_pll[] = {
	{MOUT_ACLK_MFCMSCL_333,	MOUT_MEDIA_PLL_USER},
};

struct devfreq_clk_states aclk_mfcmscl_333_media_pll_list = {
	.state = aclk_mfcmscl_333_media_pll,
	.state_count = ARRAY_SIZE(aclk_mfcmscl_333_media_pll),
};

struct devfreq_clk_state aclk_mfcmscl_333_bus_pll[] = {
	{MOUT_ACLK_MFCMSCL_333,	MOUT_BUS_PLL_USER},
};

struct devfreq_clk_states aclk_mfcmscl_333_bus_pll_list = {
	.state = aclk_mfcmscl_333_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mfcmscl_333_bus_pll),
};

struct devfreq_clk_info dout_aclk_mfcmscl_333[] = {
	{INT_LV0,	333 * MHZ,	0,	&aclk_mfcmscl_333_media_pll_list},
	{INT_LV1,	333 * MHZ,	0,	&aclk_mfcmscl_333_media_pll_list},
	{INT_LV2,	276 * MHZ,	0,	&aclk_mfcmscl_333_bus_pll_list},
};

/* ACLK_MFCMSCL_200 */
struct devfreq_clk_state aclk_mfcmscl_200_bus_pll[] = {
	{MOUT_ACLK_MFCMSCL_200,	MOUT_BUS_PLL_USER},
};

struct devfreq_clk_states aclk_mfcmscl_200_bus_pll_list = {
	.state = aclk_mfcmscl_200_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mfcmscl_200_bus_pll),
};

struct devfreq_clk_info dout_aclk_mfcmscl_200[] = {
	{INT_LV0,	276 * MHZ,	0,	&aclk_mfcmscl_200_bus_pll_list},
	{INT_LV1,	276 * MHZ,	0,	&aclk_mfcmscl_200_bus_pll_list},
	{INT_LV2,	207 * MHZ,	0,	&aclk_mfcmscl_200_bus_pll_list},
};

/* ACLK_IMEM_266 */
struct devfreq_clk_info dout_aclk_imem_266[] = {
	{INT_LV0,	276 * MHZ,	0,	NULL},
	{INT_LV1,	276 * MHZ,	0,	NULL},
	{INT_LV2,	207 * MHZ,	0,	NULL},
};

/* ACLK_IMEM_200 */
struct devfreq_clk_info dout_aclk_imem_200[] = {
	{INT_LV0,	207 * MHZ,	0,	NULL},
	{INT_LV1,	207 * MHZ,	0,	NULL},
	{INT_LV2,	166 * MHZ,	0,	NULL},
};

/* ACLK_BUS2_333 */
struct devfreq_clk_state aclk_bus2_333_bus_pll[] = {
	{MOUT_ACLK_BUS2_333,	MOUT_BUS_PLL_USER},
};

struct devfreq_clk_states aclk_bus2_333_bus_pll_list = {
	.state = aclk_bus2_333_bus_pll,
	.state_count = ARRAY_SIZE(aclk_bus2_333_bus_pll),
};

struct devfreq_clk_info dout_aclk_bus2_333[] = {
	{INT_LV0,	104 * MHZ,	0,	&aclk_bus2_333_bus_pll_list},
	{INT_LV1,	104 * MHZ,	0,	&aclk_bus2_333_bus_pll_list},
	{INT_LV2,	104 * MHZ,	0,	&aclk_bus2_333_bus_pll_list},
};

/* ACLK_FSYS_200 */
struct devfreq_clk_info dout_aclk_fsys_200[] = {
	{INT_LV0,	104 * MHZ,	0,	NULL},
	{INT_LV1,	104 * MHZ,	0,	NULL},
	{INT_LV2,	104 * MHZ,	0,	NULL},
};

/* ACLK_ISP_300 */
struct devfreq_clk_state aclk_isp_300_bus_pll[] = {
	{MOUT_ACLK_ISP_300,	INT_BUS_PLL},
};

struct devfreq_clk_states aclk_isp_300_bus_pll_list = {
	.state = aclk_isp_300_bus_pll,
	.state_count = ARRAY_SIZE(aclk_isp_300_bus_pll),
};

struct devfreq_clk_state aclk_isp_300_aud_pll[] = {
	{MOUT_ACLK_ISP_300,	AUD_PLL},
};

struct devfreq_clk_states aclk_isp_300_aud_pll_list = {
	.state = aclk_isp_300_aud_pll,
	.state_count = ARRAY_SIZE(aclk_isp_300_aud_pll),
};

struct devfreq_clk_info dout_aclk_isp_300[] = {
	{INT_LV0,	295 * MHZ,	0,	&aclk_isp_300_aud_pll_list},
	{INT_LV1,	207 * MHZ,	0,	&aclk_isp_300_bus_pll_list},
	{INT_LV2,	 92 * MHZ,	0,	&aclk_isp_300_bus_pll_list},
};

struct devfreq_opp_table devfreq_int_opp_list[] = {
	{INT_LV0, 334000, 1175000},
	{INT_LV1, 333000, 1125000},
	{INT_LV2, 275000, 1025000},
};

struct devfreq_clk_info *devfreq_clk_int_info_list[] = {
	dout_aclk_bus0_333,
	dout_aclk_mfcmscl_333,
	dout_aclk_mfcmscl_200,
	dout_aclk_imem_266,
	dout_aclk_imem_200,
	dout_aclk_bus2_333,
	dout_aclk_fsys_200,
	dout_aclk_isp_300,
};

enum devfreq_int_clk devfreq_clk_int_info_idx[] = {
	DOUT_ACLK_BUS0_333,
	DOUT_ACLK_MFCMSCL_333,
	DOUT_ACLK_MFCMSCL_200,
	DOUT_ACLK_IMEM_266,
	DOUT_ACLK_IMEM_200,
	DOUT_ACLK_BUS2_333,
	DOUT_ACLK_FSYS_200,
	DOUT_ACLK_ISP_300,
	INT_CLK_COUNT,
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_int_pm_domain[] = {
	{NULL},
	{"pd-mfcmscl"},
	{"pd-mfcmscl"},
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{"pd-isp"},
};

/* =========== MIF related struct */

extern struct pm_qos_request min_mif_thermal_qos;

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
	MEDIA_PLL,
	BUS_PLL,
	DOUT_ACLK_MIF_333,
	MOUT_ACLK_MIF_333,
	DOUT_ACLK_MIF_166,
	DOUT_ACLK_MIF_83,
	MOUT_ACLK_MIF_83,
	DOUT_ACLK_MIF_FIX_50,
	MOUT_ACLK_MIF_FIX_50,
	DOUT_ACLK_DISPAUD_133,
	MOUT_ACLK_DISPAUD_133,
	MIF_CLK_COUNT,
};

struct devfreq_clk_list devfreq_mif_clk[MIF_CLK_COUNT] = {
	{"mem0_pll",},
	{"dout_mif_noddr",},
	{"bus_pll",},
	{"dout_aclk_mif_333",},
	{"mout_aclk_mif_333",},
	{"dout_aclk_mif_166",},
	{"dout_aclk_mif_83",},
	{"mout_aclk_mif_83",},
	{"dout_aclk_mif_fix_50",},
	{"mout_aclk_mif_fix_50",},
	{"dout_aclk_dispaud_133",},
	{"mout_aclk_dispaud_133",},
};

struct devfreq_opp_table devfreq_mif_opp_list[] = {
	{MIF_LV0, 825000, 1200000},
	{MIF_LV1, 728000, 1200000},
	{MIF_LV2, 666000, 1175000},
	{MIF_LV3, 559000, 1100000},
	{MIF_LV4, 413000, 1050000},
	{MIF_LV5, 338000, 1025000},
	{MIF_LV6, 273000, 1025000},
	{MIF_LV7, 200000, 1025000},
};

/* MIF_333 */
struct devfreq_clk_state aclk_mif_333_media_pll[] = {
	{MOUT_ACLK_MIF_333, MEDIA_PLL},
};

struct devfreq_clk_states aclk_mif_333_media_pll_list = {
	.state = aclk_mif_333_media_pll,
	.state_count = ARRAY_SIZE(aclk_mif_333_media_pll),
};

struct devfreq_clk_state aclk_mif_333_bus_pll[] = {
	{MOUT_ACLK_MIF_333, BUS_PLL},
};

struct devfreq_clk_states aclk_mif_333_bus_pll_list = {
	.state = aclk_mif_333_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mif_333_bus_pll),
};

struct devfreq_clk_info dout_aclk_mif_333[] = {
	{MIF_LV0,	413 * MHZ,	0,	&aclk_mif_333_bus_pll_list},
	{MIF_LV1,	333 * MHZ,	0,	&aclk_mif_333_media_pll_list},
	{MIF_LV2,	333 * MHZ,	0,	&aclk_mif_333_media_pll_list},
	{MIF_LV3,	276 * MHZ,	0,	&aclk_mif_333_bus_pll_list},
	{MIF_LV4,	207 * MHZ,	0,	&aclk_mif_333_bus_pll_list},
	{MIF_LV5,	166 * MHZ,	0,	&aclk_mif_333_bus_pll_list},
	{MIF_LV6,	138 * MHZ,	0,	&aclk_mif_333_bus_pll_list},
	{MIF_LV7,	104 * MHZ,	0,	&aclk_mif_333_bus_pll_list},
};

/* MIF_166 */
struct devfreq_clk_info dout_aclk_mif_166[] = {
	{MIF_LV0,	207 * MHZ,	0,	NULL},
	{MIF_LV1,	167 * MHZ,	0,	NULL},
	{MIF_LV2,	167 * MHZ,	0,	NULL},
	{MIF_LV3,	138 * MHZ,	0,	NULL},
	{MIF_LV4,	104 * MHZ,	0,	NULL},
	{MIF_LV5,	 83 * MHZ,	0,	NULL},
	{MIF_LV6,	 69 * MHZ,	0,	NULL},
	{MIF_LV7,	 52 * MHZ,	0,	NULL},
};

/* MIF_83 */
struct devfreq_clk_state aclk_mif_83_media_pll[] = {
	{MOUT_ACLK_MIF_83, MEDIA_PLL},
};

struct devfreq_clk_states aclk_mif_83_media_pll_list = {
	.state = aclk_mif_83_media_pll,
	.state_count = ARRAY_SIZE(aclk_mif_83_media_pll),
};

struct devfreq_clk_info dout_aclk_mif_83[] = {
	{MIF_LV0,	 84 * MHZ,	0,	&aclk_mif_83_media_pll_list},
	{MIF_LV1,	 84 * MHZ,	0,	&aclk_mif_83_media_pll_list},
	{MIF_LV2,	 84 * MHZ,	0,	&aclk_mif_83_media_pll_list},
	{MIF_LV3,	 84 * MHZ,	0,	&aclk_mif_83_media_pll_list},
	{MIF_LV4,	 84 * MHZ,	0,	&aclk_mif_83_media_pll_list},
	{MIF_LV5,	 84 * MHZ,	0,	&aclk_mif_83_media_pll_list},
	{MIF_LV6,	 84 * MHZ,	0,	&aclk_mif_83_media_pll_list},
	{MIF_LV7,	 84 * MHZ,	0,	&aclk_mif_83_media_pll_list},
};

/* MIF_FIX_50 */
struct devfreq_clk_state aclk_mif_fix_50_bus_pll[] = {
	{MOUT_ACLK_MIF_FIX_50, BUS_PLL},
};

struct devfreq_clk_states aclk_mif_fix_50_bus_pll_list = {
	.state = aclk_mif_fix_50_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mif_fix_50_bus_pll),
};

struct devfreq_clk_info dout_aclk_mif_fix_50[] = {
	{MIF_LV0,	 52 * MHZ,	0,	&aclk_mif_fix_50_bus_pll_list},
	{MIF_LV1,	 52 * MHZ,	0,	&aclk_mif_fix_50_bus_pll_list},
	{MIF_LV2,	 52 * MHZ,	0,	&aclk_mif_fix_50_bus_pll_list},
	{MIF_LV3,	 52 * MHZ,	0,	&aclk_mif_fix_50_bus_pll_list},
	{MIF_LV4,	 52 * MHZ,	0,	&aclk_mif_fix_50_bus_pll_list},
	{MIF_LV5,	 52 * MHZ,	0,	&aclk_mif_fix_50_bus_pll_list},
	{MIF_LV6,	 52 * MHZ,	0,	&aclk_mif_fix_50_bus_pll_list},
	{MIF_LV7,	 52 * MHZ,	0,	&aclk_mif_fix_50_bus_pll_list},
};

/* ACLK_DISPAUD_133 */
struct devfreq_clk_state aclk_dispaud_133_bus_pll[] = {
	{MOUT_ACLK_DISPAUD_133, BUS_PLL},
};

struct devfreq_clk_states aclk_dispaud_133_bus_pll_list = {
	.state = aclk_dispaud_133_bus_pll,
	.state_count = ARRAY_SIZE(aclk_dispaud_133_bus_pll),
};

struct devfreq_clk_info dout_aclk_dispaud_133[] = {
	{MIF_LV0,	138 * MHZ,	0,	&aclk_dispaud_133_bus_pll_list},
	{MIF_LV1,	138 * MHZ,	0,	&aclk_dispaud_133_bus_pll_list},
	{MIF_LV2,	138 * MHZ,	0,	&aclk_dispaud_133_bus_pll_list},
	{MIF_LV3,	138 * MHZ,	0,	&aclk_dispaud_133_bus_pll_list},
	{MIF_LV4,	104 * MHZ,	0,	&aclk_dispaud_133_bus_pll_list},
	{MIF_LV5,	104 * MHZ,	0,	&aclk_dispaud_133_bus_pll_list},
	{MIF_LV6,	104 * MHZ,	0,	&aclk_dispaud_133_bus_pll_list},
	{MIF_LV7,	104 * MHZ,	0,	&aclk_dispaud_133_bus_pll_list},
};

struct devfreq_clk_info *devfreq_clk_mif_info_list[] = {
	dout_aclk_mif_333,
	dout_aclk_mif_166,
	dout_aclk_mif_83,
	dout_aclk_mif_fix_50,
	dout_aclk_dispaud_133,
};

enum devfreq_mif_clk devfreq_clk_mif_info_idx[] = {
	DOUT_ACLK_MIF_333,
	DOUT_ACLK_MIF_166,
	DOUT_ACLK_MIF_83,
	DOUT_ACLK_MIF_FIX_50,
	DOUT_ACLK_DISPAUD_133,
};
#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_mif_pm_domain[] = {
	{NULL},
	{NULL},
	{NULL},
	{NULL},
	{"pd-dispaud"},
};
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

#define MUX_MASK	0x3
#define MUX_OFFSET	12
static void exynos3475_devfreq_waiting_mux(void __iomem *mux_reg, u32 mux_set_value)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(10);

	while (time_before(jiffies, timeout))
		if (((readl(mux_reg) >> MUX_OFFSET) & MUX_MASK) == mux_set_value)
			break;

	if (((readl(mux_reg) >> MUX_OFFSET) & MUX_MASK) != mux_set_value)
		pr_err("%s: re-parenting mux timed-out\n", __func__);
}

#ifdef CONFIG_PM_RUNTIME
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
#endif

#ifdef CONFIG_EXYNOS_THERMAL
unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset)
{
	if (voltage > EXYNOS3475_LIMIT_COLD_VOLTAGE)
		return voltage;

	if (voltage + volt_offset > EXYNOS3475_LIMIT_COLD_VOLTAGE)
		return EXYNOS3475_LIMIT_COLD_VOLTAGE;

	return voltage + volt_offset;
}
#endif

#ifdef DUMP_DVFS_CLKS
static void exynos3_devfreq_dump_all_clks(struct devfreq_clk_list devfreq_clks[], int clk_count, char *str)
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
static struct dmc_drex_dfs_mif_table *dfs_drex_mif_table_p;

/* ========== 1. INT related function */

static int exynos3_devfreq_int_set_clk(struct devfreq_data_int *data,
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

void exynos_int_notify_power_status(const char *pd_name, unsigned int turn_on)
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
			exynos3_devfreq_int_set_clk(data_int,
					cur_freq_idx,
					devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk,
					devfreq_clk_int_info_list[i]);
	}
out:
	mutex_unlock(&data_int->lock);
}
#endif

static int exynos3_devfreq_int_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_int_clk); i++) {
		devfreq_int_clk[i].clk = clk_get(NULL, devfreq_int_clk[i].clk_name);
		if (IS_ERR(devfreq_int_clk[i].clk)) {
			pr_err("DEVFREQ(INT) : %s can't get clock\n", devfreq_int_clk[i].clk_name);
			return PTR_ERR(devfreq_int_clk[i].clk);
		}

		pr_info("INT clk name: %s, rate: %luMHz\n",
			devfreq_int_clk[i].clk_name, (clk_get_rate(devfreq_int_clk[i].clk) + (MHZ-1))/MHZ);
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_clk_int_info_list); i++)
		clk_prepare_enable(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk);

	return 0;
}

static int exynos3_devfreq_int_set_volt(struct devfreq_data_int *data,
					unsigned long volt,
					unsigned long volt_range)
{
	if (!data->vdd_int)
		return 0;

	return regulator_set_voltage(data->vdd_int, volt, volt_range);
}

static int exynos3_devfreq_int_set_freq(struct devfreq_data_int *data,
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
	exynos3_devfreq_dump_all_clks(devfreq_int_clk, INT_CLK_COUNT, "INT");
#endif
	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
int exynos3_devfreq_int_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_int *data = container_of(nb, struct devfreq_data_int, tmu_notifier);
	unsigned int set_volt;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		mutex_lock(&data->lock);
		if (*on) {

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(data->old_volt, data->volt_offset);
			if (data->vdd_int)
				regulator_set_voltage(data->vdd_int, set_volt, data->max_support_voltage);

			data->old_volt = (unsigned long) set_volt;
		} else {

			if (data->volt_offset) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(data->old_volt - COLD_VOLT_OFFSET, data->volt_offset);
			if (data->vdd_int)
				regulator_set_voltage(data->vdd_int, set_volt, data->max_support_voltage);

			data->old_volt = (unsigned long) set_volt;

		}
		mutex_unlock(&data->lock);

	}
	return NOTIFY_OK;
}
int exynos3_devfreq_mif_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_mif *data = container_of(nb, struct devfreq_data_mif, tmu_notifier);
	unsigned int set_volt = 0;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		mutex_lock(&data->lock);
		if (*on) {
			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(data->old_volt, data->volt_offset);
			if (data->vdd_mif)
				regulator_set_voltage(data->vdd_mif, set_volt, data->max_support_voltage);

			data->old_volt = (unsigned long) set_volt;
		} else {
			if (data->volt_offset) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(data->old_volt - COLD_VOLT_OFFSET, data->volt_offset);
			if (data->vdd_mif)
				regulator_set_voltage(data->vdd_mif, set_volt, data->max_support_voltage);

			data->old_volt = (unsigned long) set_volt;
		}
		mutex_unlock(&data->lock);
	}
	return NOTIFY_OK;
}
#endif

int exynos3475_devfreq_int_init(void *data)
{
	int ret = 0;

	data_int = (struct devfreq_data_int *)data;
	data_int->max_state = INT_LV_COUNT;

	if (exynos3_devfreq_int_init_clock())
		return -EINVAL;

#ifdef CONFIG_PM_RUNTIME
	if (exynos_devfreq_init_pm_domain(devfreq_int_pm_domain, ARRAY_SIZE(devfreq_int_pm_domain)))
		return -EINVAL;
#endif
	data_int->int_set_volt = exynos3_devfreq_int_set_volt;
	data_int->int_set_freq = exynos3_devfreq_int_set_freq;
#ifdef CONFIG_EXYNOS_THERMAL
	data_int->tmu_notifier.notifier_call = exynos3_devfreq_int_tmu_notifier;
#endif

	return ret;
}
/* end of INT related function */

/* ========== 2. MIF related function */

static int exynos3_devfreq_mif_set_clk(struct devfreq_data_mif *data,
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

void exynos_mif_notify_power_status(const char *pd_name, unsigned int turn_on)
{
	int i;
	int cur_freq_idx;

	if (!turn_on || !data_mif || !data_mif->use_dvfs)
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
			exynos3_devfreq_mif_set_clk(data_mif,
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

static struct dmc_drex_dfs_mif_table dfs_drex_mif_table_1_5G[] = {
		      /* RfcPb, TimingRow, TimingData, TimingPower, Rdfetch */
	DREX_TIMING_PARA(0x26, 0x57588652, 0x4740167A, 0x4C5B4746, 0x2), /* L0 */
	DREX_TIMING_PARA(0x21, 0x4D478590, 0x3630167A, 0x44513636, 0x2), /* L1 */
	DREX_TIMING_PARA(0x1F, 0x4747754F, 0x3630166A, 0x404A3635, 0x2), /* L2 */
	DREX_TIMING_PARA(0x1A, 0x3B36644D, 0x3630165A, 0x343E3535, 0x2), /* L3 */
	DREX_TIMING_PARA(0x13, 0x2C34534A, 0x2620164A, 0x282E2425, 0x2), /* L4 */
	DREX_TIMING_PARA(0x10, 0x242442C8, 0x2620163A, 0x20262325, 0x2), /* L5 */
	DREX_TIMING_PARA( 0xD, 0x1D233247, 0x2620163A, 0x1C1F2325, 0x2), /* L6 */
	DREX_TIMING_PARA( 0x9, 0x19223185, 0x2620162A, 0x14162225, 0x2), /* L7 */
};

static struct dmc_drex_dfs_mif_table dfs_drex_mif_table_1G[] = {
		      /* RfcPb, TimingRow, TimingData, TimingPower, Rdfetch */
	DREX_TIMING_PARA(0x19, 0x36588652, 0x4740167A, 0x4C3A4746, 0x2), /* L0 */
	DREX_TIMING_PARA(0x16, 0x30478590, 0x3630167A, 0x44333636, 0x2), /* L1 */
	DREX_TIMING_PARA(0x15, 0x2C47754F, 0x3630166A, 0x402F3635, 0x2), /* L2 */
	DREX_TIMING_PARA(0x11, 0x2536644D, 0x3630165A, 0x34283535, 0x2), /* L3 */
	DREX_TIMING_PARA( 0xD, 0x1B34534A, 0x2620164A, 0x281D2425, 0x2), /* L4 */
	DREX_TIMING_PARA( 0xB, 0x192442C8, 0x2620163A, 0x20182325, 0x2), /* L5 */
	DREX_TIMING_PARA( 0x9, 0x19233247, 0x2620163A, 0x1C142325, 0x2), /* L6 */
	DREX_TIMING_PARA( 0x6, 0x19223185, 0x2620162A, 0x140E2225, 0x2), /* L7 */
};

static struct dmc_phy_dfs_mif_table dfs_phy_mif_table[] = {
	PHY_DVFS_CON(0x8A000000, 0x49000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0A002100, 0x000A0021,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x8A000000, 0x49000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0A002100, 0x000A0021,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x8A000000, 0x49000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0A002100, 0x000A0021,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x8A000000, 0x49000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0A002100, 0x000A0021,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	/* Turn the dll off below this */
	PHY_DVFS_CON(0x0A000000, 0x09000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0A001100, 0x000A0011,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x0A000000, 0x09000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0A000100, 0x000A0001,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x0A000000, 0x09000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0A000100, 0x000A0001,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
	PHY_DVFS_CON(0x0A000000, 0x09000000,
			PHY_DVFS_CON0_SET1_MASK, PHY_DVFS_CON0_SET0_MASK,
			0x0A009100, 0x000A0091,
			PHY_DVFS_CON2_SET1_MASK, PHY_DVFS_CON2_SET0_MASK,
			0x63800000, 0x90700000,
			PHY_DVFS_CON3_SET1_MASK, PHY_DVFS_CON3_SET0_MASK),
};

static int exynos3475_devfreq_mif_set_dll(struct devfreq_data_mif *data,
						int target_idx)
{
	uint32_t reg;

	if (target_idx > DLL_LOCK_LV) {
		/* Write stored lock value to ctrl_force register */
		reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
		reg &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
		reg |= (data->dll_lock_value << CTRL_FORCE_SHIFT);
		__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);
	}
	return 0;
}

static int exynos3475_devfreq_mif_timing_set(struct devfreq_data_mif *data,
		int target_idx)
{
	struct dmc_drex_dfs_mif_table *cur_drex_param;
	struct dmc_phy_dfs_mif_table *cur_phy_param;
	uint32_t reg;
	bool timing_set_num;

	cur_drex_param = &dfs_drex_mif_table_p[target_idx];
	cur_phy_param = &dfs_phy_mif_table[target_idx];

	/* Check what timing_set_num needs to be used */
	timing_set_num = (((__raw_readl(data->base_drex + DREX_PHYSTATUS)
						>> TIMING_SET_SW_SHIFT) & TIMING_SET_SW_MASK) == 0);

	if (timing_set_num) {
		/* DREX */
		reg = __raw_readl(data->base_drex + DREX_TIMINGRFCPB);
		reg &= ~(DREX_TIMINGRFCPB_SET1_MASK);
		reg |= ((cur_drex_param->drex_timingrfcpb
				<< DREX_TIMINGRFCPB_SET1_SHIFT)
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

		reg = __raw_readl(EXYNOS3475_DREX_FREQ_CTRL1);
		reg |= 0x1;
		__raw_writel(reg, EXYNOS3475_DREX_FREQ_CTRL1);

		/* PHY_CON0 */
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON0);
		reg &= ~(cur_phy_param->phy_dvfs_con0_set1_mask);
		reg |= cur_phy_param->phy_dvfs_con0_set1;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON0);

		/* Check whether DLL needs to be turned on or off */
		exynos3475_devfreq_mif_set_dll(data, target_idx);

		/* PHY_CON2 */
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON2);
		reg &= ~(cur_phy_param->phy_dvfs_con2_set1_mask);
		reg |= cur_phy_param->phy_dvfs_con2_set1;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON2);

		/* PHY_CON3 */
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
		reg = __raw_readl(EXYNOS3475_DREX_FREQ_CTRL1);
		reg &= ~0x1;
		__raw_writel(reg, EXYNOS3475_DREX_FREQ_CTRL1);

		/* PHY_CON0 */
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON0);
		reg &= ~(cur_phy_param->phy_dvfs_con0_set0_mask);
		reg |= cur_phy_param->phy_dvfs_con0_set0;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON0);

		/* Check whether DLL needs to be turned on or off */
		exynos3475_devfreq_mif_set_dll(data, target_idx);

		/* PHY CON2 */
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON2);
		reg &= ~(cur_phy_param->phy_dvfs_con2_set0_mask);
		reg |= cur_phy_param->phy_dvfs_con2_set0;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON2);

		/* PHY CON3 */
		reg = __raw_readl(data->base_lpddr_phy + PHY_DVFS_CON3);
		reg &= ~(cur_phy_param->phy_dvfs_con3_set0_mask);
		reg |= cur_phy_param->phy_dvfs_con3_set0;
		__raw_writel(reg, data->base_lpddr_phy + PHY_DVFS_CON3);
	}

	return 0;
}

int exynos3_devfreq_mif_init_parameter(struct devfreq_data_mif *data)
{
	data->base_drex = ioremap(DREX_BASE, SZ_64K);
	if (!data->base_drex) {
		pr_err("DEVFREQ(MIF) : %s failed to ioremap\n", "base_drex");
		goto out;
	}

	data->base_lpddr_phy = ioremap(PHY_BASE, SZ_64K);
	if (!data->base_lpddr_phy) {
		pr_err("DEVFREQ(MIF) : %s failed to ioremap\n", "base_lpddr_phy");
		goto err;
	}

	return 0;

err:
	iounmap(data->base_drex);
out:
	return -ENOMEM;
}

static int exynos3_devfreq_mif_set_volt(struct devfreq_data_mif *data,
					unsigned long volt,
					unsigned long volt_range)
{
	if (!data->vdd_mif)
		return 0;

	return regulator_set_voltage(data->vdd_mif, volt, volt_range);
}

static void exynos3475_devfreq_waiting_pause(struct devfreq_data_mif *data)
{
	unsigned int timeout = 1000;
	while ((__raw_readl(EXYNOS3475_PAUSE) & CLK_PAUSE_MASK)) {
		if (timeout == 0) {
			pr_err("DEVFREQ(MIF) : timeout to wait pause completion\n");
			return;
		}
		udelay(1);
		timeout--;
	}
}

static int exynos3_devfreq_mif_change_phy(struct devfreq_data_mif *data,
					int target_idx, bool bus_pll)
{
	exynos3475_devfreq_mif_timing_set(data, target_idx);

	if (bus_pll)
		/* Set MUX_CLK2X_PHY_B, MUX_CLKM_PHY_B as 1 to use BUS_PLL */
		__raw_writel(0x00013030, EXYNOS3475_DREX_FREQ_CTRL0);
	else
		/* Set MUX_CLK2X_PHY_B, MUX_CLKM_PHY_C as 0 to use MEM0_PLL */
		__raw_writel(0x00010030, EXYNOS3475_DREX_FREQ_CTRL0);

	exynos3475_devfreq_waiting_pause(data);

	if (bus_pll) {
		exynos3475_devfreq_waiting_mux(EXYNOS3475_CLK_STAT_MUX_CLKM_PHY_B, 2);
		exynos3475_devfreq_waiting_mux(EXYNOS3475_CLK_STAT_MUX_CLK2X_PHY_B, 2);
	} else {
		exynos3475_devfreq_waiting_mux(EXYNOS3475_CLK_STAT_MUX_CLKM_PHY_B, 1);
		exynos3475_devfreq_waiting_mux(EXYNOS3475_CLK_STAT_MUX_CLK2X_PHY_B, 1);
	}

	pr_debug("BUS_PLL(400MHz) is used for source of MUX_CLK2X_PHY and MUX_CLKM_PHY\n");

	return 0;
}

static int exynos3_devfreq_mif_change_aclk(struct devfreq_data_mif *data,
						int target_idx, int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;
#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *pm_domain;
#endif
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
		if (target_idx > old_idx)
			/* If source of MUX_ACLK_MIF is media_pll, setting MUX and DIV orderly */
			clk_set_rate(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk, clk_info->freq);

		if (clk_states) {
			for (j = 0; j < clk_states->state_count; ++j) {
				clk_set_parent(devfreq_mif_clk[clk_states->state[j].clk_idx].clk,
						devfreq_mif_clk[clk_states->state[j].parent_clk_idx].clk);
			}
		}

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

static int exynos3_devfreq_mif_change_sclk(struct devfreq_data_mif *data,
						int target_idx, int old_idx)
{
	struct clk *mem0_pll;

	mem0_pll = devfreq_mif_clk[MEM0_PLL].clk;

	/* Set DREX and PHY value for Switch_PLL(BUS_PLL) */
	exynos3_devfreq_mif_change_phy(data, data->pll_safe_idx, true);

	if (target_idx == data->pll_safe_idx)
		return 0;

	/* Change p, m, s value for mem0_pll */
	clk_set_rate(mem0_pll, (devfreq_mif_opp_list[target_idx].freq * 2 * 1000));

	/* Set DREX and PHY value for MEM0_PLL */
	exynos3_devfreq_mif_change_phy(data, target_idx, false);

	return 0;
}

static int exynos3_devfreq_mif_set_freq(struct devfreq_data_mif *data,
					int target_idx,
					int old_idx)
{
	int pll_safe_idx;
	unsigned int voltage, safe_voltage = 0;
	uint32_t reg;

	pll_safe_idx = data->pll_safe_idx;
	/* Enable PAUSE */
	__raw_writel(0x1, EXYNOS3475_PAUSE);

	/* Disable Clock Gating */
	__raw_writel(0x0, data->base_drex + DREX_CGCONTROL);

	/* Set PHY ctrl_ref as 0xE */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
	reg &= ~(MDLL_CON0_CTRL_REF_MASK);
	reg |= (0xE << 1);
	__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);

	/* Find the proper voltage to be set */
	if ((pll_safe_idx <= old_idx) && (pll_safe_idx <= target_idx)) {
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
	if (old_idx > target_idx && !safe_voltage)
		exynos3_devfreq_mif_set_volt(data, voltage, data->max_support_voltage);
	else if (safe_voltage)
		exynos3_devfreq_mif_set_volt(data, safe_voltage, data->max_support_voltage);

	if (target_idx < old_idx) {
		/* Set sclk frequency */
		exynos3_devfreq_mif_change_sclk(data, target_idx, old_idx);

		/* Set aclk frequency */
		exynos3_devfreq_mif_change_aclk(data, target_idx, old_idx);
	} else {
		/* Set aclk frequency */
		exynos3_devfreq_mif_change_aclk(data, target_idx, old_idx);

		/* Set sclk frequency */
		exynos3_devfreq_mif_change_sclk(data, target_idx, old_idx);
	}

	/* Set voltage */
	if (old_idx < target_idx ||
	    ((old_idx > target_idx) && safe_voltage))
		exynos3_devfreq_mif_set_volt(data, voltage, data->max_support_voltage);

	/* Enable Clock Gating */
	__raw_writel(0x3FF, data->base_drex + DREX_CGCONTROL);

	/* Set PHY ctrl_ref as 0xF */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
	reg |= (0xF << 1);
	__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);

#ifdef DUMP_DVFS_CLKS
	exynos3_devfreq_dump_all_clks(devfreq_mif_clk, MIF_CLK_COUNT, "MIF");
#endif
	/* update mif old voltage */
	data->old_volt = voltage;

	return 0;
}

enum devfreq_mif_thermal_autorate {
	RATE_ONE = 0x000C0065,
	RATE_HALF = 0x00060032,
	RATE_QUARTER = 0x00030019,
};

static void exynos3_devfreq_swtrip(void)
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

static void exynos3_devfreq_thermal_event(struct devfreq_thermal_work *work)
{
	if (work->polling_period == 0)
		return;

	queue_delayed_work(work->work_queue,
			&work->devfreq_mif_thermal_work,
			msecs_to_jiffies(work->polling_period));
}

static void exynos3_devfreq_thermal_monitor(struct work_struct *work)
{
	struct delayed_work *d_work = container_of(work, struct delayed_work, work);
	struct devfreq_thermal_work *thermal_work =
			container_of(d_work, struct devfreq_thermal_work, devfreq_mif_thermal_work);
	unsigned int mrstatus, tmp_thermal_level, max_thermal_level = 0;
	unsigned int timingaref_value = RATE_ONE;
	void __iomem *base_drex;

	base_drex = data_mif->base_drex;

	mutex_lock(&data_mif->lock);

	__raw_writel(0x09001000, base_drex + DREX_DIRECTCMD);
	mrstatus = __raw_readl(base_drex + DREX_MRSTATUS);
	tmp_thermal_level = (mrstatus & DREX_MRSTATUS_THERMAL_LV_MASK);
	if (tmp_thermal_level > max_thermal_level)
		max_thermal_level = tmp_thermal_level;

	thermal_work->thermal_level_cs0 = tmp_thermal_level;

	/* if calib val is 0x7, dram has only one bank */
	if (calib_val != 0x7) {
		__raw_writel(0x09101000, base_drex + DREX_DIRECTCMD);
		mrstatus = __raw_readl(base_drex + DREX_MRSTATUS);
		tmp_thermal_level = (mrstatus & DREX_MRSTATUS_THERMAL_LV_MASK);
		if (tmp_thermal_level > max_thermal_level)
			max_thermal_level = tmp_thermal_level;
		thermal_work->thermal_level_cs1 = tmp_thermal_level;
	}

	mutex_unlock(&data_mif->lock);

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
	case 5:
		timingaref_value = RATE_QUARTER;
		thermal_work->polling_period = 100;
		break;

	case 6:
		exynos3_devfreq_swtrip();
		return;
	default:
		pr_err("DEVFREQ(MIF) : can't support memory thermal level\n");
		return;
	}

	__raw_writel(timingaref_value, base_drex + DREX_TIMINGAREF);

	exynos_ss_printk("[MIF]%d,%d;VT_MON_CON:0x%08x;TIMINGAREF:0x%08x\n",
			thermal_work->thermal_level_cs0, thermal_work->thermal_level_cs1,
			__raw_readl(base_drex + DREX_TIMINGAREF));

	exynos3_devfreq_thermal_event(thermal_work);
}

void exynos3_devfreq_init_thermal(void)
{
	devfreq_mif_thermal_wq = create_freezable_workqueue("devfreq_thermal_wq_ch0");

	INIT_DELAYED_WORK(&devfreq_mif_work.devfreq_mif_thermal_work,
			exynos3_devfreq_thermal_monitor);

	devfreq_mif_work.work_queue = devfreq_mif_thermal_wq;

	exynos3_devfreq_thermal_event(&devfreq_mif_work);
}

int exynos3_devfreq_mif_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_mif_clk); i++) {
		devfreq_mif_clk[i].clk = clk_get(NULL, devfreq_mif_clk[i].clk_name);
		if (IS_ERR(devfreq_mif_clk[i].clk)) {
			pr_err("DEVFREQ(MIF) : %s can't get clock\n", devfreq_mif_clk[i].clk_name);
			return PTR_ERR(devfreq_mif_clk[i].clk);
		}
		pr_info("MIF clk name: %s, rate: %luMHz\n",
				devfreq_mif_clk[i].clk_name, (clk_get_rate(devfreq_mif_clk[i].clk) + (MHZ-1))/MHZ);
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_clk_mif_info_list); i++)
		clk_prepare_enable(devfreq_mif_clk[devfreq_clk_mif_info_idx[i]].clk);

	return 0;
}

static void exynos3475_devfreq_get_dll_lock_value(struct devfreq_data_mif *data,
		int target_idx)
{
	uint32_t reg;
	uint32_t lock_value;

	/* Sequence to get DLL Lock value
	   1. Disable phy_cg_en (PHY clock gating)
	   2. Enable ctrl_lock_rden
	   3. Disable ctrl_lock_rden
	   4. Read ctrl_lock_new
	   5. Write ctrl_lock_new to ctrl_force
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
	data->dll_lock_value = lock_value;

	/* 5. Write ctrl_lock_new to ctrl_force */
	reg = __raw_readl(data->base_lpddr_phy + PHY_MDLL_CON0);
	reg &= ~(CTRL_FORCE_MASK << CTRL_FORCE_SHIFT);
	reg |= (lock_value << CTRL_FORCE_SHIFT);
	__raw_writel(reg, data->base_lpddr_phy + PHY_MDLL_CON0);

	/* 6. Enable PHY Clock Gating */
	reg = __raw_readl(data->base_drex + DREX_CGCONTROL);
	reg |= (0x1 << CG_EN_SHIFT);
	__raw_writel(reg, data->base_drex + DREX_CGCONTROL);

}

void exynos3475_devfreq_set_dll_lock_value(struct devfreq_data_mif *data,
		int target_idx)
{
	/* temporary save dll_lock_value for switching pll */
	exynos3475_devfreq_get_dll_lock_value(data, target_idx);

	/* Change frequency to 559MHz from 666MHz */
	exynos3_devfreq_mif_set_freq(data, DLL_LOCK_LV, MIF_LV2);

	/* save dll_lock_value  */
	exynos3475_devfreq_get_dll_lock_value(data, target_idx);

	/* Change frequency to 666MHz from 559MHz */
	exynos3_devfreq_mif_set_freq(data, MIF_LV2, DLL_LOCK_LV);
}

int exynos3475_devfreq_mif_init(void *data)
{
	data_mif = (struct devfreq_data_mif *)data;
	data_mif->max_state = MIF_LV_COUNT;

	/* if EXYNOS_PMU_DREX_CALIBRATION1 is 0xE, mem size is 1.5GB */
	calib_val = __raw_readl(EXYNOS_PMU_DREX_CALIBRATION1);
	if (calib_val == 0x6) {
		dfs_drex_mif_table_p = dfs_drex_mif_table_1G;
		pr_info("DEVFREQ(MIF): (%x) mem size is 1GB\n", calib_val);
	} else {
		dfs_drex_mif_table_p = dfs_drex_mif_table_1_5G;
		pr_info("DEVFREQ(MIF): (%x) mem size is 1.5GB\n", calib_val);
	}

	data_mif->mif_set_freq = exynos3_devfreq_mif_set_freq;
	data_mif->mif_set_volt = NULL;

	exynos3_devfreq_mif_init_clock();
	exynos3_devfreq_mif_init_parameter(data_mif);
	data_mif->pll_safe_idx = MIF_LV4;

#ifdef CONFIG_PM_RUNTIME
	if (exynos_devfreq_init_pm_domain(devfreq_mif_pm_domain,
				ARRAY_SIZE(devfreq_mif_pm_domain)))
		return -EINVAL;
#endif
#ifdef CONFIG_EXYNOS_THERMAL
	data_mif->tmu_notifier.notifier_call = exynos3_devfreq_mif_tmu_notifier;
#endif

	return 0;
}

int exynos3475_devfreq_mif_deinit(void *data)
{
	flush_workqueue(devfreq_mif_thermal_wq);
	destroy_workqueue(devfreq_mif_thermal_wq);

	return 0;
}
/* end of MIF related function */
DEVFREQ_INIT_OF_DECLARE(exynos3475_devfreq_int_init, "samsung,exynos3-devfreq-int", exynos3475_devfreq_int_init);
DEVFREQ_INIT_OF_DECLARE(exynos3475_devfreq_mif_init, "samsung,exynos3-devfreq-mif", exynos3475_devfreq_mif_init);
DEVFREQ_DEINIT_OF_DECLARE(exynos3475_devfreq_deinit, "samsung,exynos3-devfreq-mif", exynos3475_devfreq_mif_deinit);
