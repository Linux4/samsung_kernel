/* linux/arch/arm/mach-exynos/setup-dp.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Base Samsung Exynos DP configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/io.h>
#include <mach/regs-clock.h>

void s5p_dp_phy_init(void)
{
	u32 reg;

#if defined(CONFIG_SOC_EXYNOS5250)
	reg = __raw_readl(EXYNOS5250_DPTX_PHY_CONTROL);
	reg |= EXYNOS5250_DPTX_PHY_ENABLE;
	__raw_writel(reg, EXYNOS5250_DPTX_PHY_CONTROL);
#elif defined(CONFIG_SOC_EXYNOS5260)
	reg = __raw_readl(EXYNOS5260_DPTX_PHY_CONTROL);
	reg |= EXYNOS5260_DPTX_PHY_ENABLE;
	__raw_writel(reg, EXYNOS5260_DPTX_PHY_CONTROL);
#endif
}

void s5p_dp_phy_exit(void)
{
	u32 reg;

#if defined(CONFIG_SOC_EXYNOS5250)
	reg = __raw_readl(EXYNOS5250_DPTX_PHY_CONTROL);
	reg &= ~EXYNOS5250_DPTX_PHY_ENABLE;
	__raw_writel(reg, EXYNOS5250_DPTX_PHY_CONTROL);
#elif defined(CONFIG_SOC_EXYNOS5260)
	reg = __raw_readl(EXYNOS5260_DPTX_PHY_CONTROL);
	reg &= ~EXYNOS5260_DPTX_PHY_ENABLE;
	__raw_writel(reg, EXYNOS5260_DPTX_PHY_CONTROL);
#endif
}
