/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS Halt driver
 *
 * Author: Changki Kim <changki.kim@samsung.com>
 * 	   Youngmin Nam <youngmin.nam@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/cpu_pm.h>
#include <linux/debug-snapshot.h>
#include <soc/samsung/exynos-halt.h>

struct exynos_halt_data {
	void __iomem *cti_base[CONFIG_NR_CPUS];
	unsigned int cs_base;
	struct device *dev;
};

static struct exynos_halt_data *exynos_halt_data = NULL;

static void set_cti_break_enable(int cpu)
{
	__raw_writel(0xc5acce55, exynos_halt_data->cti_base[cpu] + 0xfb0);
	__raw_writel(0x1, exynos_halt_data->cti_base[cpu] + 0x140);
	__raw_writel(0x1, exynos_halt_data->cti_base[cpu] + 0x20);
	__raw_writel(0x1, exynos_halt_data->cti_base[cpu] + 0xa0);
	__raw_writel(0x1, exynos_halt_data->cti_base[cpu]);
}

static void set_cti_break_disable(int cpu)
{
	__raw_writel(0x0, exynos_halt_data->cti_base[cpu]);
}

void set_stop_cpu(void)
{
	int cpu;

	if (!exynos_halt_data) {
		pr_err("halt driver is not initialized.\n");
		return;
	}

	local_irq_disable();
	cpu = raw_smp_processor_id();
	dev_info(exynos_halt_data->dev, "cpu%d calls HALT!!\n", cpu);

	__raw_writel(0x1, exynos_halt_data->cti_base[cpu] + 0x14);

	asm(
	"	mov x0, #0x01		\n"
	"	msr oslar_el1, x0	\n"
	"	isb			\n"
	"1:	mrs x1, oslsr_el1	\n"
	"	and x1, x1, #(1<<1)	\n"
	"	cbz x1, 1b		\n"
	"	mrs x0, mdscr_el1	\n"
	"	orr x0, x0, #(1<<14)	\n"
	"	msr mdscr_el1, x0	\n"
	"	isb			\n"
	"2:	mrs x0, mdscr_el1	\n"
	"	and x0, x0, #(1<<14)	\n"
	"	cbz x0, 2b		\n"
	"	msr oslar_el1, xzr	\n"
	"	hlt #0xff	"
	:: );
}

static int exynos_halt_c2_pm_notifier(struct notifier_block *self,
						unsigned long action, void *v)
{
	int cpu = raw_smp_processor_id();

	switch (action) {
	case CPU_PM_ENTER:
		set_cti_break_disable(cpu);
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		set_cti_break_enable(cpu);
		break;
	case CPU_CLUSTER_PM_ENTER:
		set_cti_break_disable(cpu);
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		set_cti_break_enable(cpu);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block exynos_halt_c2_pm_nb = {
	.notifier_call = exynos_halt_c2_pm_notifier,
};

static ssize_t halt_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	set_stop_cpu();
	return size;
}

static DEVICE_ATTR_WO(halt);

static struct attribute *exynos_halt_sysfs_attrs[] = {
	&dev_attr_halt.attr,
	NULL,
};
ATTRIBUTE_GROUPS(exynos_halt_sysfs);

static int exynos_halt_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child;
	struct exynos_halt_data *data = NULL;
	int ret = 0, cpu = 0;
	unsigned int offset, base;
	char name[SZ_16];

	dev_set_socdata(&pdev->dev, "Exynos", "HALT");
	if (dbg_snapshot_get_sjtag_status()) {
		dev_err(&pdev->dev, "Skip init (sjtag enabled)\n");
		goto out_halt_init;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(struct exynos_halt_data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "can not alloc memory\n");
		ret = -ENOMEM;
		goto err_no_free;
	}
	data->dev = &pdev->dev;

	if (of_property_read_u32(np, "cs_base", &base)) {
		dev_err(&pdev->dev, "no coresight base address in dt\n");
		ret = -EINVAL;
		goto err_free;
	}
	data->cs_base = base;

	for_each_possible_cpu(cpu) {
		snprintf(name, sizeof(name), "cpu%d", cpu);
		child = of_get_child_by_name(np, (const char *)name);

		if (!child) {
			dev_err(&pdev->dev, "cpu%d child node missed\n", cpu);
			ret = -EINVAL;
			goto err_free;
		}

		ret = of_property_read_u32(child, "cti-offset", &offset);
		if (ret) {
			dev_err(&pdev->dev, "cpu%d haven't cti-offset\n", cpu);
			goto err_free;
		}

		data->cti_base[cpu] = ioremap(base + offset, SZ_4K);
		if (!data->cti_base[cpu]) {
			dev_err(&pdev->dev, "cpu%d, ioremap fail\n", cpu);
			ret = -ENOMEM;
		}

		dev_info(&pdev->dev,
			"cpu#%d, cs_base:0x%x, cti_base:0x%x, total:0x%x, ioremap:0x%lx\n",
			cpu, base, offset, base + offset, (unsigned long)data->cti_base[cpu]);
	}

	exynos_halt_data = data;

	ret = sysfs_create_groups(&pdev->dev.kobj, exynos_halt_sysfs_groups);
	if (ret) {
		dev_err(&pdev->dev, "fail to register sysfs\n");
		goto err_free;
	}

	for_each_possible_cpu(cpu)
		set_cti_break_enable(cpu);

	/* register cpu pm notifier for C2 */
	cpu_pm_register_notifier(&exynos_halt_c2_pm_nb);

	dev_info(&pdev->dev, "CTI break enabled\n");
	goto out_halt_init;

err_free:
	kfree(data);
	exynos_halt_data = NULL;
err_no_free:
out_halt_init:
	return ret;
}

static const struct of_device_id of_exynos_halt_ids[] = {
	{
		.compatible     = "samsung,exynos-halt",
	},
	{},
};

static struct platform_driver exynos_halt_driver = {
	.driver = {
		.name	= "exynos-halt",
		.of_match_table = of_exynos_halt_ids,
	},
	.probe = exynos_halt_probe,
};

static int __init exynos_halt_init(void)
{
	return platform_driver_register(&exynos_halt_driver);
}
arch_initcall(exynos_halt_init);
