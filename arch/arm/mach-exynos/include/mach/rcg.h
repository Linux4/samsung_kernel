/* linux/arch/arm/mach-exynos/rcg.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *		Sangkyu Kim(skwith.kim@samsung.com)
 *
 * EXYNOS5 - RCG(Root Clock Gating) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/io.h>

enum RCG_DOMAIN {
	RCG_PERI,
	RCG_MIF,
	RCG_MFC,
	RCG_KFC,
	RCG_ISP,
	RCG_GSCL,
	RCG_G3D,
	RCG_G2D,
	RCG_FSYS,
	RCG_EGL,
	RCG_DISP,
	RCG_AUD,
};

struct rcg_domain {
	void __iomem* reg_base;
	unsigned int use_xiu_top;
	unsigned int use_xiu_us;
	unsigned int use_xiu_async;
	unsigned int value_xiu_top;
	unsigned int value_xiu_us;
	unsigned int value_xiu_async;
};

#if defined(CONFIG_EXYNOS_RCG)
int exynos_rcg_enable(enum RCG_DOMAIN domain);
#else
static inline
int exynos_rcg_enable(enum RCG_DOMAIN domain)
{
	return 0;
}
#endif
