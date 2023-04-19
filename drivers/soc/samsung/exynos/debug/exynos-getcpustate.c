// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Interface to get the cpu power state
 *
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/io.h>

#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/exynos/exynos-getcpustate.h>


enum cpu_power_management_type {
	ePM_BY_PMU,
	ePM_BY_DSU,
	ePM_BY_CNT,
};

#define PMU_CORE_STATUS_MASK		(0x00000001)
#define PMU_CORE_STATUS_ON		(0x00000001)
#define PMU_CORE_STATUS_OFF		(0x00000000)

struct getcpustate_pmu_data {
	unsigned int *offset;
};

#define DSU_CORE_PPUHWSTAT_MASK		(0x0000ffff)
#define DSU_CORE_PPUHWSTAT_OFF		(0x0001)
#define DSU_CORE_PPUHWSTAT_OFF_EMU	(0x0002)
#define DSU_CORE_PPUHWSTAT_FULL_RET	(0x0020)
#define DSU_CORE_PPUHWSTAT_FUNC_RET	(0x0080)
#define DSU_CORE_PPUHWSTAT_ON		(0x0100)
#define DSU_CORE_PPUHWSTAT_WARM_RST	(0x0200)
#define DSU_CORE_PPUHWSTAT_DBG_RECOV	(0x0400)

struct getcpustate_dsu_data {
	void __iomem *base;
	unsigned int *offset;
	unsigned int *lsb;
};

struct getcpustate_descriptor {
	enum cpu_power_management_type type;
	int num_cpu;
	union {
		struct getcpustate_pmu_data pmu_data;
		struct getcpustate_dsu_data dsu_data;
	};
};

static struct getcpustate_descriptor *getcpustate_desc;

static bool is_pmu_cpu_power_on(int cpu)
{
	unsigned int reg;

	if (exynos_pmu_read(getcpustate_desc->pmu_data.offset[cpu], &reg)) {
		pr_err("%s: pmu read failed\n", __func__);
		return false;
	}

	if ((reg & PMU_CORE_STATUS_MASK) == PMU_CORE_STATUS_ON)
		return true;
	else
		return false;
}

static unsigned int get_core_hwppustat(int cpu)
{
	unsigned int reg;

	reg = readl(getcpustate_desc->dsu_data.base + getcpustate_desc->dsu_data.offset[cpu]);
	return (reg >> getcpustate_desc->dsu_data.lsb[cpu]) & DSU_CORE_PPUHWSTAT_MASK;
}

static bool is_dsu_cpu_power_on(int cpu)
{
	unsigned int hwppustat;

	hwppustat = get_core_hwppustat(cpu);
	if (hwppustat != DSU_CORE_PPUHWSTAT_OFF)
		return true;

	return false;
}

static bool is_getcpustate_valid(int cpu)
{
	bool is_valid;

	if (!getcpustate_desc) {
		pr_err("%s: driver is not probed\n", __func__);
		return false;
	}

	if (cpu < 0 || cpu >= getcpustate_desc->num_cpu) {
		pr_err("%s: Invalid CPU(%d)\n", __func__, cpu);
		return false;
	}

	switch (getcpustate_desc->type) {
	case ePM_BY_PMU:
	case ePM_BY_DSU:
		is_valid = true;
		break;
	default:
		pr_err("%s: Invalid PM type\n", __func__);
		is_valid = false;
		break;
	}

	return is_valid;
}

static const char *get_pmu_cpu_power_state(int cpu)
{
	if (is_pmu_cpu_power_on(cpu))
		return "ON";
	else
		return "OFF";
}

static const char *get_dsu_cpu_power_state(int cpu)
{
	unsigned int hwppustat = get_core_hwppustat(cpu);
	const char *state;

	switch (hwppustat) {
	case DSU_CORE_PPUHWSTAT_OFF:
		state = "OFF";
		break;
	case DSU_CORE_PPUHWSTAT_OFF_EMU:
		state = "OFF_EMU";
		break;
	case DSU_CORE_PPUHWSTAT_FULL_RET:
		state = "FULL_RET";
		break;
	case DSU_CORE_PPUHWSTAT_FUNC_RET:
		state = "FUNC_RET";
		break;
	case DSU_CORE_PPUHWSTAT_ON:
		state = "ON";
		break;
	case DSU_CORE_PPUHWSTAT_WARM_RST:
		state = "WARM_RST";
		break;
	case DSU_CORE_PPUHWSTAT_DBG_RECOV:
		state = "DBG_RECOV";
		break;
	default:
		state = "INVALID";
		break;
	}

	return state;
}

bool is_exynos_cpu_power_on(int cpu)
{
	bool is_on;

	if (!is_getcpustate_valid(cpu))
		return false;

	if (getcpustate_desc->type == ePM_BY_PMU)
		is_on = is_pmu_cpu_power_on(cpu);
	else
		is_on = is_dsu_cpu_power_on(cpu);

	return is_on;
}
EXPORT_SYMBOL_GPL(is_exynos_cpu_power_on);

const char *get_exynos_cpu_power_state(int cpu)
{
	const char *state;

	if (!is_getcpustate_valid(cpu))
		return "Not Spported";

	if (getcpustate_desc->type == ePM_BY_PMU)
		state = get_pmu_cpu_power_state(cpu);
	else
		state = get_dsu_cpu_power_state(cpu);

	return state;
}
EXPORT_SYMBOL_GPL(get_exynos_cpu_power_state);

static int init_pmu_getcpustate(struct platform_device *pdev, struct getcpustate_descriptor *desc)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;

	desc->type = ePM_BY_PMU;

	desc->num_cpu = of_property_count_u32_elems(np, "core_pmustatus_offset");
	if (desc->num_cpu <= 0) {
		dev_err(dev, "core_pmustatus_offset is invalid\n");
		return -EINVAL;
	}

	desc->pmu_data.offset = devm_kzalloc(dev,
					     desc->num_cpu * sizeof(*desc->pmu_data.offset),
					     GFP_KERNEL);
	if (!desc->pmu_data.offset) {
		dev_err(dev, "%s: alloc failed\n", __func__);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(np, "core_pmustatus_offset",
				   desc->pmu_data.offset, desc->num_cpu)) {
		dev_err(dev, "%s: array read failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int init_dsu_getcpustate(struct platform_device *pdev, struct getcpustate_descriptor *desc)
{

	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;

	desc->type = ePM_BY_DSU;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dsu_base");
	if (!res) {
		dev_err(dev, "%s: resource is invalid\n", __func__);
		return -ENOMEM;
	}
	desc->dsu_data.base = devm_ioremap_resource(dev, res);
	if (IS_ERR(desc->dsu_data.base)) {
		dev_err(dev, "%s: ioremap failed\n", __func__);
		return -ENOMEM;
	}

	desc->num_cpu = of_property_count_u32_elems(np, "core_ppuhwstat_offset");
	if (desc->num_cpu <= 0) {
		dev_err(dev, "core_ppuhwstat_offset is invalid\n");
		return -EINVAL;
	}

	if (desc->num_cpu != of_property_count_u32_elems(np, "core_ppuhwstat_lsb")) {
		dev_err(dev, "core_ppuhwstat_lsb is invalid\n");
		return -EINVAL;
	}

	desc->dsu_data.offset = devm_kzalloc(dev,
					     desc->num_cpu * sizeof(*desc->dsu_data.offset),
					     GFP_KERNEL);
	desc->dsu_data.lsb = devm_kzalloc(dev,
					  desc->num_cpu * sizeof(*desc->dsu_data.lsb),
					  GFP_KERNEL);
	if (!desc->dsu_data.offset || !desc->dsu_data.lsb) {
		dev_err(dev, "%s: alloc failed\n", __func__);
		return -ENOMEM;
	}

	if (of_property_read_u32_array(np, "core_ppuhwstat_offset",
					desc->dsu_data.offset, desc->num_cpu)) {
		dev_err(dev, "core_ppuhwstat_offset read failed\n");
		return -EINVAL;
	}

	if (of_property_read_u32_array(np, "core_ppuhwstat_lsb",
					desc->dsu_data.lsb, desc->num_cpu)) {
		dev_err(dev, "core_ppuhwstat_lsb read failed\n");
		return -EINVAL;
	}

	return 0;
}

static bool is_controlled_by_dsu(struct device_node *np)
{
	if (of_get_property(np, "control_by_dsu", NULL) == NULL)
		return false;
	else
		return true;
}

static int getcpustate_probe(struct platform_device *pdev)
{
	int ret;

	getcpustate_desc = devm_kzalloc(&pdev->dev,
					sizeof(struct getcpustate_descriptor), GFP_KERNEL);
	if (!getcpustate_desc) {
		dev_err(&pdev->dev, "%s: memory alloc failed\n", __func__);
		return -ENOMEM;
	}

	if (is_controlled_by_dsu(pdev->dev.of_node))
		ret = init_dsu_getcpustate(pdev, getcpustate_desc);
	else
		ret = init_pmu_getcpustate(pdev, getcpustate_desc);

	if (!ret)
		dev_info(&pdev->dev, "probed\n");
	else
		getcpustate_desc = NULL;

	return ret;
}

static int getcpustate_remove(struct platform_device *pdev)
{
	getcpustate_desc = NULL;
	return 0;
}

static const struct of_device_id getcpustate_dt_match[] = {
	{ .compatible	= "samsung,exynos-getcpustate", },
	{},
};
MODULE_DEVICE_TABLE(of, getcpustate_dt_match);

static struct platform_driver exynos_getcpustate_driver = {
	.probe = getcpustate_probe,
	.remove = getcpustate_remove,
	.driver = {
		.name = "getcpustate",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(getcpustate_dt_match),
	}
};
module_platform_driver(exynos_getcpustate_driver);

MODULE_DESCRIPTION("Exynos Inteface for getting cpu power state");
MODULE_LICENSE("GPL v2");
