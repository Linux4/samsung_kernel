/* arch/arm/mach-exynos/include/mach/regs-syscon.h
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5 SYSCON configutation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_REGS_SYSCON_H
#define __ASM_ARCH_REGS_SYSCON_H __FILE__

#define EXYNOS5260_SYSCON_PERI(x)        		(EXYNOS5260_VA_SYSCON_PERI + (x))

#define EXYNOS5260_SYSCON_PERI_PHY_SELECT               EXYNOS5260_SYSCON_PERI(0x8)
#define EXYNOS5260_SYSCON_PERI_PHY_SELECT_ISP_ONLY      (0x1 << 0)

#endif /* __ASM_ARCH_REGS_SYSCON_H */
