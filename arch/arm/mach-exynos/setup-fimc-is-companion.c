/* linux/arch/arm/mach-exynos/setup-fimc-sensor.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * FIMC-IS-COMPANION gpio and clock configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <mach/regs-gpio.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <plat/gpio-cfg.h>
#include <plat/map-s5p.h>
#include <plat/cpu.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include <mach/exynos-fimc-is.h>
#include <mach/exynos-fimc-is-sensor.h>

/* Wrapper functions */
int exynos_fimc_is_companion_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}

int exynos_fimc_is_companion_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}

int exynos_fimc_is_companion_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}

int exynos_fimc_is_companion_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}

int exynos_fimc_is_companion_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}
