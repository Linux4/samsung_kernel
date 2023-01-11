/*
 * linux/arch/arm/mm/device_mapping.c
 *
 * Author:	Neil Zhang <zhangwm@marvell.com>
 * Copyright:	(C) 2013 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/of_address.h>

#include <asm/device_mapping.h>

void __iomem *axi_virt_base;
void __iomem *apb_virt_base;

int __init device_mapping_init(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "arm,axi-bus");
	if (!node) {
		pr_err("Failed to find axi-bus\n");
		return -ENODEV;
	}

	axi_virt_base = of_iomap(node, 0);
	if (!axi_virt_base) {
		pr_err("Failed to map axi-bus register\n");
		return -ENOMEM;
	}

	node = of_find_compatible_node(NULL, NULL, "arm,apb-bus");
	if (!node) {
		pr_err("Failed to find apb-bus\n");
		return -ENODEV;
	}

	apb_virt_base = of_iomap(node, 0);
	if (!apb_virt_base) {
		pr_err("Failed to map apb-bus register\n");
		return -ENOMEM;
	}

	return 0;
}
