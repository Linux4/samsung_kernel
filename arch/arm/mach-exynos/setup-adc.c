/* linux/arch/arm/mach-exynos/setup-adc.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Base Samsung Exynos ADC configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/io.h>
#include <mach/regs-pmu.h>
#include <mach/regs-syscon.h>
#include <plat/cpu.h>

void s3c_adc_phy_init(void)
{
	u32 reg;

	if (soc_is_exynos5250() || soc_is_exynos4415() ||
			soc_is_exynos3470() || soc_is_exynos3250()) {
		reg = __raw_readl(EXYNOS5250_ADC_PHY_CONTROL);
		reg |= EXYNOS5_ADC_PHY_ENABLE;
		__raw_writel(reg, EXYNOS5250_ADC_PHY_CONTROL);
	} else if (soc_is_exynos5410() || soc_is_exynos5420()) {
		reg = __raw_readl(EXYNOS5410_ADC_PHY_CONTROL);
		reg |= EXYNOS5_ADC_PHY_ENABLE;
		__raw_writel(reg, EXYNOS5410_ADC_PHY_CONTROL);
	} else if (soc_is_exynos5260()) {
		/* ADC phy select */
		reg = readl(EXYNOS5260_SYSCON_PERI_PHY_SELECT);
		if (reg & EXYNOS5260_SYSCON_PERI_PHY_SELECT_ISP_ONLY)
			reg &= ~(EXYNOS5260_SYSCON_PERI_PHY_SELECT_ISP_ONLY);
		writel(reg, EXYNOS5260_SYSCON_PERI_PHY_SELECT);

		reg = __raw_readl(EXYNOS5260_ADC_PHY_CONTROL);
		reg |= EXYNOS5_ADC_PHY_ENABLE;
		__raw_writel(reg, EXYNOS5260_ADC_PHY_CONTROL);
	}
}

void s3c_adc_phy_exit(void)
{
	u32 reg;

	if (soc_is_exynos5250() || soc_is_exynos4415() ||
			soc_is_exynos3470() || soc_is_exynos3250()) {
		reg = __raw_readl(EXYNOS5250_ADC_PHY_CONTROL);
		reg &= ~EXYNOS5_ADC_PHY_ENABLE;
		reg |= EXYNOS5_ADC_PHY_ENABLE;
		__raw_writel(reg, EXYNOS5250_ADC_PHY_CONTROL);
	} else if (soc_is_exynos5410() || soc_is_exynos5420()) {
		reg = __raw_readl(EXYNOS5410_ADC_PHY_CONTROL);
		reg &= ~EXYNOS5_ADC_PHY_ENABLE;
		__raw_writel(reg, EXYNOS5410_ADC_PHY_CONTROL);
	} else if (soc_is_exynos5260()) {
		reg = __raw_readl(EXYNOS5260_ADC_PHY_CONTROL);
		reg &= ~EXYNOS5_ADC_PHY_ENABLE;
		__raw_writel(reg, EXYNOS5260_ADC_PHY_CONTROL);
	}
}
