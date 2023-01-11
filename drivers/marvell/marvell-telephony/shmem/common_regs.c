/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/atomic.h>
#include "common_regs.h"

static void __iomem *apmu_virt_addr;
static void __iomem *mpmu_virt_addr;
static void __iomem *ciu_virt_addr;
static void __iomem *apbs_virt_addr;

static atomic_t apmu_user_count = ATOMIC_INIT(0);
static atomic_t mpmu_user_count = ATOMIC_INIT(0);
static atomic_t ciu_user_count = ATOMIC_INIT(0);
static atomic_t apbs_user_count = ATOMIC_INIT(0);

static void __iomem *iomap_register(const char *reg_name)
{
	void __iomem *reg_virt_addr;
	struct device_node *node;

	BUG_ON(!reg_name);
	node = of_find_compatible_node(NULL, NULL, reg_name);
	BUG_ON(!node);
	reg_virt_addr = of_iomap(node, 0);
	BUG_ON(!reg_virt_addr);

	return reg_virt_addr;
}

void __iomem *map_apmu_base_va(void)
{
	if (atomic_inc_return(&apmu_user_count) == 1) {/* first map */
		apmu_virt_addr = iomap_register("marvell,mmp-pmu-apmu");
		if (!apmu_virt_addr) /* map failed */
			atomic_dec(&apmu_user_count);
	}
	return apmu_virt_addr;
}
EXPORT_SYMBOL(map_apmu_base_va);

void __iomem *map_mpmu_base_va(void)
{
	if (atomic_inc_return(&mpmu_user_count) == 1) {/* first map */
		mpmu_virt_addr = iomap_register("marvell,mmp-pmu-mpmu");
		if (!mpmu_virt_addr) /* map failed */
			atomic_dec(&mpmu_user_count);
	}
	return mpmu_virt_addr;
}
EXPORT_SYMBOL(map_mpmu_base_va);

void __iomem *map_ciu_base_va(void)
{
	if (atomic_inc_return(&ciu_user_count) == 1) {/* first map */
		ciu_virt_addr = iomap_register("marvell,mmp-ciu");
		if (!ciu_virt_addr) /* map failed */
			atomic_dec(&ciu_user_count);
	}
	return ciu_virt_addr;
}
EXPORT_SYMBOL(map_ciu_base_va);

void __iomem *map_apbs_base_va(void)
{
	if (atomic_inc_return(&apbs_user_count) == 1) {/* first map */
		apbs_virt_addr = iomap_register("marvell,mmp-apb-spare");
		if (!apbs_virt_addr) /* map failed */
			atomic_dec(&apbs_user_count);
	}
	return apbs_virt_addr;
}
EXPORT_SYMBOL(map_apbs_base_va);

/* use common register functions */

void __iomem *get_apmu_base_va(void)
{
	return apmu_virt_addr;
}
EXPORT_SYMBOL(get_apmu_base_va);

void __iomem *get_mpmu_base_va(void)
{
	return mpmu_virt_addr;
}
EXPORT_SYMBOL(get_mpmu_base_va);

void __iomem *get_ciu_base_va(void)
{
	return ciu_virt_addr;
}
EXPORT_SYMBOL(get_ciu_base_va);

void __iomem *get_apbs_base_va(void)
{
	return apbs_virt_addr;
}
EXPORT_SYMBOL(get_apbs_base_va);

/* unmap common register functions */

void unmap_apmu_base_va(void)
{
	if (atomic_dec_and_test(&apmu_user_count)) {
		iounmap(apmu_virt_addr);
		apmu_virt_addr = NULL;
	}
}
EXPORT_SYMBOL(unmap_apmu_base_va);

void unmap_mpmu_base_va(void)
{
	if (atomic_dec_and_test(&mpmu_user_count)) {
		iounmap(mpmu_virt_addr);
		mpmu_virt_addr = NULL;
	}
}
EXPORT_SYMBOL(unmap_mpmu_base_va);

void unmap_ciu_base_va(void)
{
	if (atomic_dec_and_test(&ciu_user_count)) {
		iounmap(ciu_virt_addr);
		ciu_virt_addr = NULL;
	}
}
EXPORT_SYMBOL(unmap_ciu_base_va);

void unmap_apbs_base_va(void)
{
	if (atomic_dec_and_test(&apbs_user_count)) {
		iounmap(apbs_virt_addr);
		apbs_virt_addr = NULL;
	}
}
EXPORT_SYMBOL(unmap_apbs_base_va);
