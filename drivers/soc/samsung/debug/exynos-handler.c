/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Exynos - Support SoC specific handler
 * Author: Hosung Kim <hosung0.kim@samsung.com>
 *         Youngmin Nam <youngmin.nam@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <linux/interrupt.h>
#include <linux/debug-snapshot-helper.h>

#ifdef CONFIG_DEBUG_SNAPSHOT
extern struct dbg_snapshot_helper_ops *dss_soc_ops;
#endif
static struct device *exynos_handler_dev;

struct exynos_handler {
	unsigned int	irq;
	char		name[SZ_128];
	void		*handler;
};

static irqreturn_t exynos_ecc_handler(int irq, void *data)
{
	struct exynos_handler *ecc = (struct exynos_handler *)data;

#ifdef CONFIG_DEBUG_SNAPSHOT
	dss_soc_ops->soc_dump_info(NULL);
#endif
	panic("%s", ecc->name);
	return 0;
}

static int __init exynos_handler_setup(struct device_node *np)
{
	struct exynos_handler *ecc_handler;
	struct property *prop;
	const char *name;
	int err = 0;
	int handler_nr_irq, i;

	handler_nr_irq = of_irq_count(np);
	dev_info(exynos_handler_dev, "%s: handler_nr_irq = %d\n", __func__, handler_nr_irq);

	/* memory alloc for handler */
	if (handler_nr_irq > 0) {
		ecc_handler = kzalloc(sizeof(struct exynos_handler) * handler_nr_irq,
					GFP_KERNEL);
		if (!ecc_handler) {
			dev_err(exynos_handler_dev, "%s: fail to kzalloc\n", __func__);
			err = -ENOMEM;
			goto out;
		}
	}

	/* setup ecc_handler */
	prop = of_find_property(np, "interrupt-names", NULL);
	name = of_prop_next_string(prop, NULL);
	for (i = 0; i < handler_nr_irq; i++, name = of_prop_next_string(prop, name)) {

		if (name == NULL) {
			dev_err(exynos_handler_dev,
				"interrupt name unmatched(index = %d\n", i);
			err = -EINVAL;
			break;
		}

		ecc_handler[i].irq = irq_of_parse_and_map(np, i);
		snprintf(ecc_handler[i].name, sizeof(ecc_handler[i].name) - 1,
									"%s", name);
		ecc_handler[i].handler = (void *)exynos_ecc_handler;

		err = request_irq(ecc_handler[i].irq,
				ecc_handler[i].handler,
				IRQF_NOBALANCING | IRQF_GIC_MULTI_TARGET,
				ecc_handler[i].name, &ecc_handler[i]);
		if (err) {
			dev_err(exynos_handler_dev,
				"unable to request irq%u for ecc handler[%s]\n",
				ecc_handler[i].irq, ecc_handler[i].name);
			break;
		} else {
			dev_info(exynos_handler_dev,
				"Success to request irq%u for ecc handler[%s]\n",
				ecc_handler[i].irq, ecc_handler[i].name);
		}
	}

out:
	of_node_put(np);
	return err;
}

static const struct of_device_id handler_of_match[] __initconst = {
	{ .compatible = "samsung,exynos-handler", .data = exynos_handler_setup},
	{},
};

typedef int (*handler_initcall_t)(const struct device_node *);
static int __init exynos_handler_init(void)
{
	struct device_node *np;
	const struct of_device_id *matched_np;
	handler_initcall_t init_fn;

	np = of_find_matching_node_and_match(NULL, handler_of_match, &matched_np);
	if (!np)
		return -ENODEV;

	exynos_handler_dev = create_empty_device();
	if (!exynos_handler_dev)
		panic("Exynos: create empty device fail\n");
	dev_set_socdata(exynos_handler_dev, "Exynos", "Handler");

	init_fn = (handler_initcall_t)matched_np->data;

	return init_fn(np);
}
subsys_initcall(exynos_handler_init);
