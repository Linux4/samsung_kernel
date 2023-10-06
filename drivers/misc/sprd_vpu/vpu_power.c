// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc QOGIRN6PRO VPU power driver
 *
 * Copyright (C) 2019 Unisoc, Inc.
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/types.h>

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "sprd-vpu-pd: " fmt
#define __SPRD_VPU_TIMEOUT            (30 * 1000)

enum {
	PMU_VPU_AUTO_SHUTDOWN = 0,
	PMU_VPU_FORCE_SHUTDOWN,
	PMU_PWR_STATUS,
	PMU_APB_PIXELPLL,
	REG_MAX
};

static char * const vpu_pm_name[REG_MAX] = {
	[PMU_VPU_AUTO_SHUTDOWN] = "pmu_vpu_auto_shutdown",
	[PMU_VPU_FORCE_SHUTDOWN] = "pmu_vpu_force_shutdown",
	[PMU_PWR_STATUS] = "pmu_pwr_status",
	[PMU_APB_PIXELPLL] = "pmu_apb_pixelpll",
};

struct sprd_vpu_pd {
	struct generic_pm_domain gpd;
	struct regmap *regmap[REG_MAX];
	unsigned int reg[REG_MAX];
	unsigned int mask[REG_MAX];
};

static const struct of_device_id vpu_pd_of_match[] = {
	{ .compatible = "sprd,vpu-pd"},
	{ },
};

static int vpu_pw_on(struct generic_pm_domain *domain)
{
	int ret = 0;
	u32 power_state;
	u32 read_count = 0;

	struct sprd_vpu_pd *vpu_pd = container_of(domain, struct sprd_vpu_pd, gpd);

        if (vpu_pd->regmap[PMU_APB_PIXELPLL] != NULL) {
		pr_info("PMU_APB_PIXELPLL pw on \n");
		ret = regmap_update_bits(vpu_pd->regmap[PMU_APB_PIXELPLL],
			vpu_pd->reg[PMU_APB_PIXELPLL],
			vpu_pd->mask[PMU_APB_PIXELPLL],
			vpu_pd->mask[PMU_APB_PIXELPLL]);
		if (ret) {
			pr_err("regmap_update_bits failed %s, %d\n",
				__func__, __LINE__);
			goto pw_on_exit;
		}
		udelay(300);
	}

	if (vpu_pd->regmap[PMU_VPU_AUTO_SHUTDOWN] == NULL) {
		pr_info("skip power on\n");
		ret = -EINVAL;
		goto pw_on_exit;
	}

	ret = regmap_update_bits(vpu_pd->regmap[PMU_VPU_AUTO_SHUTDOWN],
		vpu_pd->reg[PMU_VPU_AUTO_SHUTDOWN],
		vpu_pd->mask[PMU_VPU_AUTO_SHUTDOWN],
		~vpu_pd->mask[PMU_VPU_AUTO_SHUTDOWN]);
	if (ret) {
		pr_err("regmap_update_bits failed %s, %d\n",
			__func__, __LINE__);
		goto pw_on_exit;
	}

	ret = regmap_update_bits(vpu_pd->regmap[PMU_VPU_FORCE_SHUTDOWN],
		vpu_pd->reg[PMU_VPU_FORCE_SHUTDOWN],
		vpu_pd->mask[PMU_VPU_FORCE_SHUTDOWN],
		~vpu_pd->mask[PMU_VPU_FORCE_SHUTDOWN]);
	if (ret) {
		pr_err("regmap_update_bits failed %s, %d\n",
			__func__, __LINE__);
		goto pw_on_exit;
	}

	do {
		udelay(300);
		read_count++;
		regmap_read(vpu_pd->regmap[PMU_PWR_STATUS],
			vpu_pd->reg[PMU_PWR_STATUS], &power_state);
		power_state &= vpu_pd->mask[PMU_PWR_STATUS];
	} while (power_state && read_count < 100);

	if (power_state) {
		pr_err("%s set failed 0x%x\n", __func__,
			power_state);
		return -ETIMEDOUT;
	}

	pr_info("%s: %s OK\n", domain->name, __func__);

pw_on_exit:

	return ret;
}

static int vpu_pw_off(struct generic_pm_domain *domain)
{
	int ret = 0;
	struct sprd_vpu_pd *vpu_pd = container_of(domain, struct sprd_vpu_pd, gpd);

	if (vpu_pd->regmap[PMU_VPU_FORCE_SHUTDOWN] == NULL) {
		pr_info("skip power off\n");
		ret = -EINVAL;
		goto pw_off_exit;
	}

	ret = regmap_update_bits(
		vpu_pd->regmap[PMU_VPU_FORCE_SHUTDOWN],
		vpu_pd->reg[PMU_VPU_FORCE_SHUTDOWN],
		vpu_pd->mask[PMU_VPU_FORCE_SHUTDOWN],
		vpu_pd->mask[PMU_VPU_FORCE_SHUTDOWN]);
	if (ret) {
		pr_err("regmap_update_bits failed %s, %d\n",
			__func__, __LINE__);
		goto pw_off_exit;
	}


        if (vpu_pd->regmap[PMU_APB_PIXELPLL] != NULL) {
		pr_info("PMU_APB_PIXELPLL pw off\n");
		ret = regmap_update_bits(vpu_pd->regmap[PMU_APB_PIXELPLL],
			vpu_pd->reg[PMU_APB_PIXELPLL],
			vpu_pd->mask[PMU_APB_PIXELPLL],
			~vpu_pd->mask[PMU_APB_PIXELPLL]);
		if (ret) {
			pr_err("regmap_update_bits failed %s, %d\n",
				__func__, __LINE__);
			goto pw_off_exit;
		}
	}

	pr_info("%s: %s OK\n", domain->name, __func__);
pw_off_exit:

	return ret;
}

static __init const char *vpu_get_domain_name(struct device_node *node)
{
	const char *name;

	if (of_property_read_string(node, "label", &name) < 0)
		name = kbasename(node->full_name);
	return kstrdup_const(name, GFP_KERNEL);
}

static __init int vpu_pm_init_power_domain(void)
{
	struct device_node *np;
	const struct of_device_id *match;

	for_each_matching_node_and_match(np, vpu_pd_of_match, &match) {

		struct sprd_vpu_pd *pd;
		int i,ret;
		char *pname;
		struct regmap *tregmap;
		uint32_t syscon_args[2];

		pd = kzalloc(sizeof(*pd), GFP_KERNEL);
		if (!pd) {
			of_node_put(np);
			return -ENOMEM;
		}

		pd->gpd.name = vpu_get_domain_name(np);
		if (!pd->gpd.name) {
			kfree(pd);
			of_node_put(np);
			return -ENOMEM;
		}
		pr_info("%s: %s\n", pd->gpd.name, __func__);

		pd->gpd.power_off = vpu_pw_off;
		pd->gpd.power_on = vpu_pw_on;

		for (i = 0; i < ARRAY_SIZE(vpu_pm_name); i++) {
			pname = vpu_pm_name[i];
			tregmap = syscon_regmap_lookup_by_name(np, pname);
			if (IS_ERR(tregmap)) {
				pr_err("Read Vsp Dts %s regmap fail\n",
					pname);
				pd->regmap[i] = NULL;
				pd->reg[i] = 0x0;
				pd->mask[i] = 0x0;
				continue;
			}

			ret = syscon_get_args_by_name(np, pname, 2, syscon_args);
			if (ret != 2) {
				pr_err("Read Vsp Dts %s args fail, ret = %d\n",
					pname, ret);
				continue;
			}
			pd->regmap[i] = tregmap;
			pd->reg[i] = syscon_args[0];
			pd->mask[i] = syscon_args[1];
			pr_info("VPU syscon[%s]%p, offset 0x%x, mask 0x%x\n",
					pname, pd->regmap[i], pd->reg[i], pd->mask[i]);
		}

		pm_genpd_init(&pd->gpd, NULL, true);
		of_genpd_add_provider_simple(np, &pd->gpd);
	}

	/* Assign the child power domains to their parents */
	for_each_matching_node(np, vpu_pd_of_match) {
		struct of_phandle_args child, parent;

		child.np = np;
		child.args_count = 0;

		if (of_parse_phandle_with_args(np, "power-domains",
				"#power-domain-cells", 0,
				&parent) != 0)
			continue;

		if (of_genpd_add_subdomain(&parent, &child))
			pr_warn("%pOF failed to add subdomain: %pOF\n",
				parent.np, child.np);
		else
			pr_info("%pOF has as child subdomain: %pOF.\n",
				parent.np, child.np);
	}

	return 0;
}
core_initcall(vpu_pm_init_power_domain);
