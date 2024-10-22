// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/topology.h>
#include "mediatek-cpufreq-hw_fdvfs.h"

static int fdvfs_support = -1;
static void __iomem *fdvfs_base, *fdvfs_reg, *fdvfs_cci_reg;

static unsigned int ioremap_fdvfs_reg(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, FDVFS_REG);
	if (!res)
		goto no_res;

	fdvfs_reg = ioremap(res->start, resource_size(res));
	if (!fdvfs_reg) {
		pr_info("%s failed to map res[%d] %pR\n", __func__, FDVFS_REG, res);
		goto release_region;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, FDVFS_CCI_REG);
	if (!res)
		goto no_res;

	fdvfs_cci_reg = ioremap(res->start, resource_size(res));
	if (!fdvfs_cci_reg) {
		pr_info("%s failed to map res[%d] %pR\n", __func__, FDVFS_CCI_REG, res);
		goto release_region;
	}

	pr_info("%s: fdvfs reg mapping success.\n", __func__);
	return 1;

release_region:
	release_mem_region(res->start, resource_size(res));
no_res:
	pr_info("%s can't get mem resource.\n", __func__);
	return 0;
}

bool is_fdvfs_support(void)
{
	return fdvfs_support == 1 ? 1 : 0;
}
EXPORT_SYMBOL_GPL(is_fdvfs_support);

int check_fdvfs_support(void)
{
	struct device_node *np;
	struct platform_device *pdev;
	int ret = 0;
	struct resource *res;

	if (fdvfs_support != -1)
		return fdvfs_support == 1 ? 1 : 0;

	np = of_find_node_by_name(NULL, "fdvfs");
	if (!np) {
		pr_info("failed to find node @ %s\n", __func__);
		goto fdvfs_disable;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_info("failed to find pdev @ %s\n", __func__);
		of_node_put(np);
		fdvfs_support = 0;
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, FDVFS_SUPPORT);
	if (!res) {
		ret = -ENODEV;
		goto no_res;
	}

	fdvfs_base = ioremap(res->start, resource_size(res));

	if (!fdvfs_base) {
		pr_info("%s failed to map resource %pR\n", __func__, res);
		ret = -ENOMEM;
		goto release_region;
	}

	if (readl_relaxed(fdvfs_base) == FDVFS_MAGICNUM && ioremap_fdvfs_reg(pdev)) {
		fdvfs_support = 1;
		pr_info("%s: fdvfs supports\n", __func__);
	} else {
		fdvfs_support = 0;
		pr_info("%s: fdvfs not supports\n", __func__);
	}

	of_node_put(np);
	return fdvfs_support;

release_region:
	release_mem_region(res->start, resource_size(res));
no_res:
	pr_info("%s can't get mem resource %d\n", __func__, FDVFS_SUPPORT);
fdvfs_disable:
	pr_info("%s fdvfs is disabled: %d\n", __func__, ret);
	fdvfs_support = 0;
	of_node_put(np);

	return ret;
}

int cpufreq_fdvfs_switch(unsigned int target_f, struct cpufreq_policy *policy)
{
	unsigned int cpu;

	target_f = DIV_ROUND_UP(target_f, FDVFS_FREQU);
	for_each_cpu(cpu, policy->real_cpus) {
		writel_relaxed(target_f, fdvfs_reg + cpu*0x4);
	}

	return 0;
}

int cpufreq_fdvfs_cci_switch(unsigned int target_f)
{

	if (fdvfs_support != 1 || fdvfs_cci_reg == NULL)
		return 0;

	target_f = DIV_ROUND_UP(target_f, FDVFS_FREQU);
	writel_relaxed(target_f, fdvfs_cci_reg);

	return 1;
}
EXPORT_SYMBOL_GPL(cpufreq_fdvfs_cci_switch);

MODULE_AUTHOR("Haohao Yang <Haohao.yang@mediatek.com>");
MODULE_DESCRIPTION("fdvfs apis");
MODULE_LICENSE("GPL");
