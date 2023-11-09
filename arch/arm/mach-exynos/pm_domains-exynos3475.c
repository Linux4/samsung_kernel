/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/mutex.h>
#include <linux/clk.h>
#include <mach/pm_domains.h>
#include <mach/regs-pmu-exynos3475.h>
#include <mach/regs-clock-exynos3475.h>
#include <mach/exynos-powermode.h>
#include <mach/pm_domains-exynos3475-cal.h>
#include <mach/devfreq.h>
#include <sound/s2803x.h>

enum pd_domain {
	PD_G3D,
	PD_ISP,
	PD_DISPAUD,
	PD_MFCMSCL,
};

struct pd_private {
	const char *name;
	struct exynos_pd_clk *top_clks;
	struct exynos_pd_reg *sys_pwr_regs;
	struct sfr_save *save_list;
	unsigned int nr_top_clks;
	unsigned int nr_sys_pwr_regs;
	unsigned int nr_save_list;
};

static struct pd_private pd_data_list[] = {
	{
		.name = "pd-g3d",
		.sys_pwr_regs = sys_pwr_regs_g3d,
		.save_list = save_list_g3d,
		.nr_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_g3d),
		.nr_save_list = ARRAY_SIZE(save_list_g3d),
	}, {
		.name = "pd-isp",
		.sys_pwr_regs = sys_pwr_regs_isp,
		.save_list = save_list_isp,
		.nr_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_isp),
		.nr_save_list = ARRAY_SIZE(save_list_isp),
	}, {
		.name = "pd-dispaud",
		.sys_pwr_regs = sys_pwr_regs_dispaud,
		.save_list = save_list_dispaud,
		.nr_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_dispaud),
		.nr_save_list = ARRAY_SIZE(save_list_dispaud),
	}, {
		.name = "pd-mfcmscl",
		.sys_pwr_regs = sys_pwr_regs_mscl,
		.save_list = save_list_mscl,
		.nr_sys_pwr_regs = ARRAY_SIZE(sys_pwr_regs_mscl),
		.nr_save_list = ARRAY_SIZE(save_list_mscl),
	},
};

static void exynos_pd_set_sys_pwr_reg(struct exynos_pd_reg *ptr, int nr_regs)
{
	unsigned int val;

	if (!ptr)
		return;

	for (; nr_regs > 0; nr_regs--, ptr++) {
		val = __raw_readl(ptr->reg);
		val &= ~(1 << ptr->bit_offset);
		__raw_writel(val, ptr->reg);
	}
}

static void exynos_pd_save_sfr(struct sfr_save *ptr, int count)
{
	for (; count > 0; count--, ptr++)
		ptr->val = __raw_readl(ptr->reg);
}

static void exynos_pd_restore_sfr(struct sfr_save *ptr, int count)
{
	for (; count > 0; count--, ptr++)
		__raw_writel(ptr->val, ptr->reg);
}

int exynos_pd_clk_get(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data = (struct pd_private *)pd->priv;
	struct exynos_pd_clk *top_clks;
	int i;

	DEBUG_PRINT_INFO("%s: fetch pd top clk\n", pd->name);

	if (!pd_data) {
		pr_err(PM_DOMAIN_PREFIX "pd_data is null\n");
		return -ENODEV;
	}

	if (!pd_data->top_clks) {
		DEBUG_PRINT_INFO("%s: pd clk not defined\n", pd->name);
		return 0;
	}

	top_clks = pd_data->top_clks;
	for (i = 0; i < pd_data->nr_top_clks; i++) {
		top_clks[i].clock = samsung_clk_get_by_reg((unsigned long)top_clks[i].reg, top_clks[i].bit_offset);
		if (!top_clks[i].clock) {
			pr_err(PM_DOMAIN_PREFIX "clk_get of %s's top clock has been failed  i = %d\n",
					pd->name, i);
			return -ENODEV;
		}
	}
	return 0;
}

/****************************************************************
 * G3D
 ****************************************************************/
static int g3d_power_on_pre(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data = (struct pd_private *)pd->priv;
	unsigned int val;

	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	exynos_update_pd_idle_status(pd->idle_ip_index, 0);

	if (pd_data->sys_pwr_regs)
		exynos_pd_set_sys_pwr_reg(pd_data->sys_pwr_regs, pd_data->nr_sys_pwr_regs);

	/* set DUR_SCPRE and DUR_SCALL to 0 */
	val = __raw_readl(EXYNOS_PMU_G3D_DURATION0);
	val &= ~((0xf << 8) | (0xf << 4));
	__raw_writel(val, EXYNOS_PMU_G3D_DURATION0);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);

	return 0;
}

static int g3d_power_on_post(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data = (struct pd_private *)pd->priv;

	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	if (pd_data->save_list)
		exynos_pd_restore_sfr(pd_data->save_list, pd_data->nr_save_list);

	exynos_mif_notify_power_status(pd->name, 1);
	exynos_int_notify_power_status(pd->name, 1);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);

	return 0;
}

static int g3d_power_off_pre(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data = (struct pd_private *)pd->priv;
	unsigned int val;

	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	if (pd_data->save_list)
		exynos_pd_save_sfr(pd_data->save_list, pd_data->nr_save_list);

	if (pd_data->sys_pwr_regs)
		exynos_pd_set_sys_pwr_reg(pd_data->sys_pwr_regs, pd_data->nr_sys_pwr_regs);

	/* set DUR_SCPRE and DUR_SCALL to 0 */
	val = __raw_readl(EXYNOS_PMU_G3D_DURATION0);
	val &= ~((0xf << 8) | (0xf << 4));
	__raw_writel(val, EXYNOS_PMU_G3D_DURATION0);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);

	return 0;
}

static int g3d_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	exynos_update_pd_idle_status(pd->idle_ip_index, 1);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);
	return 0;
}

/****************************************************************
 * ISP
 ****************************************************************/
static int isp_power_on_pre(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data = (struct pd_private *)pd->priv;

	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	exynos_update_pd_idle_status(pd->idle_ip_index, 0);

	if (pd_data->sys_pwr_regs)
		exynos_pd_set_sys_pwr_reg(pd_data->sys_pwr_regs, pd_data->nr_sys_pwr_regs);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);

	return 0;
}

static int isp_power_on_post(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data = (struct pd_private *)pd->priv;

	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	if (pd_data->save_list)
		exynos_pd_restore_sfr(pd_data->save_list, pd_data->nr_save_list);

	exynos_mif_notify_power_status(pd->name, 1);
	exynos_int_notify_power_status(pd->name, 1);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);

	return 0;
}

static int isp_power_off_pre(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data = (struct pd_private *)pd->priv;
	unsigned int val;

	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	if (pd_data->save_list)
		exynos_pd_save_sfr(pd_data->save_list, pd_data->nr_save_list);

	if (pd_data->sys_pwr_regs)
		exynos_pd_set_sys_pwr_reg(pd_data->sys_pwr_regs, pd_data->nr_sys_pwr_regs);

	/* disable FIMC_FD LPI */
	val = __raw_readl(EXYNOS_PMU_LPI_MASK_ISP_BUSMASTER);
	val |= DIS_LPI_FIMC_FD;
	__raw_writel(val, EXYNOS_PMU_LPI_MASK_ISP_BUSMASTER);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);

	return 0;
}

static int isp_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	exynos_update_pd_idle_status(pd->idle_ip_index, 1);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);
	return 0;
}
/****************************************************************
 * DISPAUD
 ****************************************************************/
static int dispaud_power_on_pre(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data = (struct pd_private *)pd->priv;

	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	exynos_update_pd_idle_status(pd->idle_ip_index, 0);

	if (pd_data->sys_pwr_regs)
		exynos_pd_set_sys_pwr_reg(pd_data->sys_pwr_regs, pd_data->nr_sys_pwr_regs);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);

	return 0;
}

static int dispaud_power_on_post(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data = (struct pd_private *)pd->priv;
	unsigned int val;

	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	/* release AUD PAD retention */
	val = __raw_readl(EXYNOS_PMU_PAD_RETENTION_AUD_OPTION);
	val |= BIT(28);
	__raw_writel(val, EXYNOS_PMU_PAD_RETENTION_AUD_OPTION);

	if (pd_data->save_list)
		exynos_pd_restore_sfr(pd_data->save_list, pd_data->nr_save_list);

	exynos_mif_notify_power_status(pd->name, 1);
	exynos_int_notify_power_status(pd->name, 1);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);

	return 0;
}

static int dispaud_power_off_pre(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data = (struct pd_private *)pd->priv;

	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	if (pd_data->save_list)
		exynos_pd_save_sfr(pd_data->save_list, pd_data->nr_save_list);

	if (pd_data->sys_pwr_regs)
		exynos_pd_set_sys_pwr_reg(pd_data->sys_pwr_regs, pd_data->nr_sys_pwr_regs);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);

	return 0;
}

static int dispaud_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	exynos_update_pd_idle_status(pd->idle_ip_index, 1);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);
	return 0;
}

static bool dispaud_power_on_active_wakeup(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	if (pd->cb->active_wakeup) {
		pr_info(PM_DOMAIN_PREFIX "skip %s power on\n", pd->name);
		pd->cb->active_wakeup = false;
		return true;
	}

	DEBUG_PRINT_INFO("%s ... done\n", __func__);
	return false;
}

static bool dispaud_power_off_active_wakeup(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	if (is_cp_aud_enabled()) {
		pr_info(PM_DOMAIN_PREFIX "skip %s power off\n", pd->name);
		pd->cb->active_wakeup = true;
		return true;
	}

	DEBUG_PRINT_INFO("%s ... done\n", __func__);
	return false;
}

/****************************************************************
 * MFCMSCL
 ****************************************************************/
static int mfcmscl_power_on_pre(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data = (struct pd_private *)pd->priv;

	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	exynos_update_pd_idle_status(pd->idle_ip_index, 0);

	if (pd_data->sys_pwr_regs)
		exynos_pd_set_sys_pwr_reg(pd_data->sys_pwr_regs, pd_data->nr_sys_pwr_regs);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);

	return 0;
}

static int mfcmscl_power_on_post(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data = (struct pd_private *)pd->priv;

	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	if (pd_data->save_list)
		exynos_pd_restore_sfr(pd_data->save_list, pd_data->nr_save_list);

	exynos_mif_notify_power_status(pd->name, 1);
	exynos_int_notify_power_status(pd->name, 1);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);

	return 0;
}

static int mfcmscl_power_off_pre(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data = (struct pd_private *)pd->priv;

	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	if (pd_data->save_list)
		exynos_pd_save_sfr(pd_data->save_list, pd_data->nr_save_list);

	if (pd_data->sys_pwr_regs)
		exynos_pd_set_sys_pwr_reg(pd_data->sys_pwr_regs, pd_data->nr_sys_pwr_regs);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);

	return 0;
}

static int mfcmscl_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %s\n", __func__, pd->name);

	exynos_update_pd_idle_status(pd->idle_ip_index, 1);

	DEBUG_PRINT_INFO("%s ... done\n", __func__);
	return 0;
}
/****************************************************************/

static struct exynos_pd_callback pd_callback_list[] = {
	{
		.name = "pd-g3d",
		.on_pre = g3d_power_on_pre,
		.on_post = g3d_power_on_post,
		.off_pre = g3d_power_off_pre,
		.off_post = g3d_power_off_post,
	}, {
		.name = "pd-isp",
		.on_pre = isp_power_on_pre,
		.on_post = isp_power_on_post,
		.off_pre = isp_power_off_pre,
		.off_post = isp_power_off_post,
	}, {
		.name = "pd-dispaud",
		.on_pre = dispaud_power_on_pre,
		.on_post = dispaud_power_on_post,
		.off_pre = dispaud_power_off_pre,
		.off_post = dispaud_power_off_post,
		.on_active = dispaud_power_on_active_wakeup,
		.off_active = dispaud_power_off_active_wakeup,
	}, {
		.name = "pd-mfcmscl",
		.on_pre = mfcmscl_power_on_pre,
		.on_post = mfcmscl_power_on_post,
		.off_pre = mfcmscl_power_off_pre,
		.off_post = mfcmscl_power_off_post,
	},
};

struct exynos_pd_callback *exynos_pd_find_callback(struct exynos_pm_domain *pd)
{
	struct exynos_pd_callback *cb;
	int i;

	/* find callback function for power domain */
	for (i = 0; i < ARRAY_SIZE(pd_callback_list); i++) {
		cb = &pd_callback_list[i];

		if (strcmp(cb->name, pd->name))
			continue;

		DEBUG_PRINT_INFO("%s: found callback function\n", pd->name);
		break;
	}

	if (i == ARRAY_SIZE(pd_callback_list)) {
		DEBUG_PRINT_INFO("%s: can't found pd_data\n", pd->name);
		return NULL;
	}

	pd->cb = cb;
	return cb;
}

void *exynos_pd_find_data(struct exynos_pm_domain *pd)
{
	struct pd_private *pd_data;
	int i;

	/* find pd_data for each power domain */
	for (i = 0; i < ARRAY_SIZE(pd_data_list); i++) {
		pd_data = &pd_data_list[i];

		if (!strcmp(pd_data->name, pd->name))
			break;
	}

	if (i == ARRAY_SIZE(pd_data_list)) {
		DEBUG_PRINT_INFO("%s: can't found pd_data\n", pd->name);
		return NULL;
	}

	return (void *)pd_data;
}
