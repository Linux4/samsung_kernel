/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS Power mode
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/module.h>

#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-powermode.h>

struct exynos_powermode_info {
	/*
	 * While system boot, wakeup_mask and idle_ip_mask is intialized with
	 * device tree. These are used by system power mode.
	 */
	unsigned int	num_wakeup_mask;
	unsigned int	*wakeup_mask_offset;
	unsigned int	*wakeup_stat_offset;
	unsigned int	*wakeup_mask[NUM_SYS_POWERDOWN];

	bool vgpio_used;
	void __iomem *vgpio2pmu_base;			/* SYSREG_VGPIO2PMU base */
	unsigned int	vgpio_inten_offset;
	unsigned int	vgpio_wakeup_inten[NUM_SYS_POWERDOWN];


	unsigned int	*usbl2_wakeup_int_en;
};

static struct exynos_powermode_info *pm_info;
extern u32 exynos_eint_wake_mask_array[3];

/******************************************************************************
 *                              System power mode                             *
 ******************************************************************************/
int exynos_system_idle_enter(void)
{
	int ret;

	ret = exynos_prepare_sys_powerdown(SYS_SICD);
	if (ret) {
		pr_info("failed exynos prepare sys powerdown\n");
		return ret;
	}

	exynos_pm_notify(SICD_ENTER);

	return 0;
}

void exynos_system_idle_exit(int cancel)
{
	exynos_pm_notify(SICD_EXIT);

	exynos_wakeup_sys_powerdown(SYS_SICD, cancel);
}

#define PMU_EINT_WAKEUP_MASK	0x60C
#define PMU_EINT_WAKEUP_MASK2	0x61C
static void exynos_set_wakeupmask(enum sys_powerdown mode)
{
	int i;
	u32 wakeup_int_en = 0;

	/* Set external interrupt mask */
	exynos_pmu_write(PMU_EINT_WAKEUP_MASK, exynos_eint_wake_mask_array[0]);
	exynos_pmu_write(PMU_EINT_WAKEUP_MASK2, exynos_eint_wake_mask_array[1]);

	for (i = 0; i < pm_info->num_wakeup_mask; i++) {
		exynos_pmu_write(pm_info->wakeup_stat_offset[i], 0);
		wakeup_int_en = pm_info->wakeup_mask[mode][i];

		if (otg_is_connect() == 2)
			wakeup_int_en &= ~pm_info->usbl2_wakeup_int_en[i];

		exynos_pmu_write(pm_info->wakeup_mask_offset[i], wakeup_int_en);
	}

	if (pm_info->vgpio_used)
		__raw_writel(pm_info->vgpio_wakeup_inten[mode],
				pm_info->vgpio2pmu_base + pm_info->vgpio_inten_offset);
}

int exynos_prepare_sys_powerdown(enum sys_powerdown mode)
{
	int ret;

	exynos_set_wakeupmask(mode);

	ret = cal_pm_enter(mode);
	if (ret)
		pr_err("CAL Fail to set powermode\n");

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_prepare_sys_powerdown);

void exynos_wakeup_sys_powerdown(enum sys_powerdown mode, bool early_wakeup)
{
	if (early_wakeup)
		cal_pm_earlywakeup(mode);
	else
		cal_pm_exit(mode);

	if (pm_info->vgpio_used)
		__raw_writel(0,	pm_info->vgpio2pmu_base + pm_info->vgpio_inten_offset);

}
EXPORT_SYMBOL_GPL(exynos_wakeup_sys_powerdown);

/******************************************************************************
 *                            Driver initialization                           *
 ******************************************************************************/

#define for_each_syspwr_mode(mode)                              \
       for ((mode) = 0; (mode) < NUM_SYS_POWERDOWN; (mode)++)

static int alloc_wakeup_mask(int num_wakeup_mask)
{
	unsigned int mode;

	pm_info->wakeup_mask_offset = kzalloc(sizeof(unsigned int)
				* num_wakeup_mask, GFP_KERNEL);
	if (!pm_info->wakeup_mask_offset)
		return -ENOMEM;

	pm_info->wakeup_stat_offset = kzalloc(sizeof(unsigned int)
				* num_wakeup_mask, GFP_KERNEL);
	if (!pm_info->wakeup_stat_offset)
		return -ENOMEM;

	for_each_syspwr_mode(mode) {
		pm_info->wakeup_mask[mode] = kzalloc(sizeof(unsigned int)
				* num_wakeup_mask, GFP_KERNEL);

		if (!pm_info->wakeup_mask[mode])
			goto free_reg_offset;
	}

	pm_info->usbl2_wakeup_int_en = kzalloc(sizeof(unsigned int)
				* num_wakeup_mask, GFP_KERNEL);
	if (!pm_info->usbl2_wakeup_int_en)
		return -ENOMEM;

	return 0;

free_reg_offset:
	for_each_syspwr_mode(mode)
		if (pm_info->wakeup_mask[mode])
			kfree(pm_info->wakeup_mask[mode]);

	kfree(pm_info->wakeup_mask_offset);
	kfree(pm_info->wakeup_stat_offset);

	return -ENOMEM;
}

static int parsing_dt_wakeup_mask(struct device_node *np)
{
	int ret;
	struct device_node *root, *child;
	unsigned int mode, mask_index = 0;
	struct property *prop;
	const __be32 *cur;
	u32 val;

	root = of_find_node_by_name(np, "wakeup-masks");
	pm_info->num_wakeup_mask = of_get_child_count(root);

	ret = alloc_wakeup_mask(pm_info->num_wakeup_mask);
	if (ret)
		return ret;

	for_each_child_of_node(root, child) {
		for_each_syspwr_mode(mode) {
			ret = of_property_read_u32_index(child, "mask",
				mode, &pm_info->wakeup_mask[mode][mask_index]);
			if (ret)
				return ret;
		}

		ret = of_property_read_u32(child, "mask-offset",
				&pm_info->wakeup_mask_offset[mask_index]);
		if (ret)
			return ret;

		ret = of_property_read_u32(child, "stat-offset",
				&pm_info->wakeup_stat_offset[mask_index]);
		if (ret)
			return ret;

		of_property_for_each_u32(child, "usbl2_wakeup_int_en", prop, cur, val) {
			pm_info->usbl2_wakeup_int_en[mask_index] |= (unsigned int)BIT(val);
		}

		mask_index++;
	}

	root = of_find_node_by_name(np, "vgpio-wakeup-inten");

	if (root) {
		pm_info->vgpio_used = true;
		pm_info->vgpio2pmu_base = of_iomap(np, 0);
		for_each_syspwr_mode(mode) {
			ret = of_property_read_u32_index(root, "mask",
					mode, &pm_info->vgpio_wakeup_inten[mode]);
			if (ret)
				return ret;
		}

		ret = of_property_read_u32(root, "inten-offset",
				&pm_info->vgpio_inten_offset);
		if (ret)
			return ret;
	} else {
		pm_info->vgpio_used = false;
		pr_info("%s: VGPIO WAKEKUP INTEN will not be used\n", __func__);
	}
	return 0;
}

static int exynos_powermode_init(void)
{
	struct device_node *np;
	int ret;

	pm_info = kzalloc(sizeof(struct exynos_powermode_info), GFP_KERNEL);
	if (pm_info == NULL) {
		pr_err("%s: failed to allocate exynos_powermode_info\n", __func__);
		return -ENOMEM;
	}

	np = of_find_node_by_name(NULL, "exynos-powermode");
	ret = parsing_dt_wakeup_mask(np);
	if (ret)
		pr_warn("Fail to initialize the wakeup mask with err = %d\n", ret);

	return 0;
}
arch_initcall(exynos_powermode_init);
MODULE_LICENSE("GPL");
