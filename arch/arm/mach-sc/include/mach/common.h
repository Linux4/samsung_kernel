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

#ifndef __ASM_ARCH_SPRD_COMMON_H
#define __ASM_ARCH_SPRD_COMMON_H

#define INTC_DYNAMIC_CLK_GATE_EN (1<<6)
#define SCU_DYNAMIC_CLK_GATE_EN  (1<<5)

extern void __iomem *sprd_get_gic_dist_base(void);
extern void __iomem *sprd_get_gic_cpu_base(void);


extern void gic_cpu_enable(unsigned int cpu);
extern void gic_cpu_disable(unsigned int cpu);
extern void gic_dist_enable(void);
extern void gic_dist_disable(void);


extern void sprd_pm_cpu_enter_lowpower(unsigned int cpu);
extern void __iomem *sprd_get_scu_base(void);
#endif
