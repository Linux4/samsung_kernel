/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/io.h>
#include <linux/irqchip/arm-gic.h>
#include <mach/hardware.h>

static void __iomem *gic_dist_base_addr;
static void __iomem *gic_cpu_base;

void __iomem *sprd_get_gic_dist_base(void)
{
	gic_dist_base_addr = (void __iomem *)CORE_GIC_DIS_VA;
	return gic_dist_base_addr;
}

void __iomem *sprd_get_gic_cpu_base(void)
{
	gic_cpu_base = (void __iomem *)CORE_GIC_CPU_VA;
	return gic_cpu_base;
}

int sprd_map_gic(void){
	gic_dist_base_addr = (void __iomem *)CORE_GIC_DIS_VA;
	gic_cpu_base = (void __iomem *)CORE_GIC_CPU_VA;
	return 0;
}

/*
 * FIXME: Remove this GIC APIs once common GIG library starts
 * supporting it.
 */
void gic_cpu_enable(unsigned int cpu)
{
	__raw_writel(0xf0, gic_cpu_base + GIC_CPU_PRIMASK);
	__raw_writel(1, gic_cpu_base + GIC_CPU_CTRL);
}

void gic_cpu_disable(unsigned int cpu)
{
	__raw_writel(0, gic_cpu_base + GIC_CPU_CTRL);
}


bool gic_dist_disabled(void)
{
	return !(__raw_readl(gic_dist_base_addr + GIC_DIST_CTRL) & 0x1);
}

void gic_dist_enable(void)
{
	__raw_writel(0x1, gic_dist_base_addr + GIC_DIST_CTRL);
}
void gic_dist_disable(void)
{
	__raw_writel(0, gic_dist_base_addr + GIC_CPU_CTRL);
}

void __iomem *sprd_get_scu_base(void)
{
	return (void __iomem *)SPRD_CORE_BASE;
}
