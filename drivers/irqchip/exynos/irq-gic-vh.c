// SPDX-License-Identifier: GPL-2.0-only
/*
 * irq-gic-vh.c - Vendor hook driver for GIC
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 */


#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <trace/hooks/gic.h>
#include <linux/cpumask.h>
#include <linux/of_address.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/irq.h>

#define NR_GIC_CPU_IF 8

static struct cpumask affinity_cpus;

static void set_affinity_gic_vh(void *data, struct irq_data *d,
		const struct cpumask *mask_val, bool force, u8 *gic_cpu_map, void __iomem *reg)
{
	struct cpumask mask;
	u8 cpumap = 0;
	unsigned int cpu;

	if (!cpumask_and(&mask, mask_val, cpu_online_mask)) {
		pr_err("%s: cannot set affinity\n", __func__);
		return;
	}

	if (cpumask_equal(&CPU_MASK_NONE, &affinity_cpus) ||
			!cpumask_equal(&mask, &affinity_cpus))
		return;

	for_each_cpu(cpu, &mask) {
		if (cpu >= min_t(int, NR_GIC_CPU_IF, nr_cpu_ids)) {
			pr_err("%s: cannot get gic_cpu_map - cpu %d\n", __func__, cpu);
			return;
		}
		cpumap |= gic_cpu_map[cpu];
	}

	writeb_relaxed(cpumap, reg);
	irq_data_update_effective_affinity(d, &mask);
}

static int gic_vh_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	const char *name;
	int ret;

	dev_info(dev, "gic-vh probe\n");

	if (of_property_read_string(node, "multitarget-cpus", &name))
		affinity_cpus = CPU_MASK_NONE;
	else
		cpulist_parse(name, &affinity_cpus);

	ret = register_trace_android_vh_gic_set_affinity(set_affinity_gic_vh, NULL);

	if (ret) {
		pr_err("Failed to register trace_android_vh_gic_set_affinity\n");
		return ret;
	}

	return 0;

}

static const struct of_device_id gic_vh_match_table[] = {
	{ .compatible = "arm,gic-vh" },
	{ },
};
MODULE_DEVICE_TABLE(of, gic_vh_match_table);

static struct platform_driver gic_vh_driver = {
	.probe		= gic_vh_probe,
	.driver		= {
		.name	= "gic-vh",
		.of_match_table = gic_vh_match_table,
	},
};

static int __init gic_vh_modinit(void)
{
	return platform_driver_register(&gic_vh_driver);
}

module_init(gic_vh_modinit);

MODULE_DESCRIPTION("Vendor hook driver for GIC");
MODULE_AUTHOR("Hyunki Koo <hyunki00.koo@samsung.com>");
MODULE_AUTHOR("Donghoon Yu <hoony.yu@samsung.com>");
MODULE_AUTHOR("Hajun Sung <hajun.sung@samsung.com>");
MODULE_LICENSE("GPL v2");
