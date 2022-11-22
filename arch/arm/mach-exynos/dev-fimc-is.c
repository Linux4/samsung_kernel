/* linux/arch/arm/plat-s5p/dev-fimc_is.c
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * Base FIMC-IS resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
#include <mach/exynos-fimc-is.h>
#include <mach/exynos-fimc-is-sensor.h>
#else
#include <media/exynos_fimc_is.h>
#endif

#define MCUCTL_BASE	0x180000

#if defined(CONFIG_ARCH_EXYNOS3)
static struct resource exynos3_fimc_is_resource[] = {
	[0] = {
		.start	= EXYNOS4_PA_FIMC_IS,
		.end	= EXYNOS4_PA_FIMC_IS + SZ_2M + SZ_256K + SZ_128K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= EXYNOS3_IRQ_COMB_ARMISP_GIC,
		.end	= EXYNOS3_IRQ_COMB_ARMISP_GIC,
		.flags	= IORESOURCE_IRQ,
	}/*,
	[2] = {
		.start	= EXYNOS3_IRQ_COMB_ISP_GIC,
		.end	= EXYNOS3_IRQ_COMB_ISP_GIC,
		.flags	= IORESOURCE_IRQ,
	},*/
};

struct platform_device exynos3_device_fimc_is = {
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	.name		= FIMC_IS_DEV_NAME,
#else
	.name		= FIMC_IS_MODULE_NAME,
#endif
	.id		= -1,
	.num_resources	= ARRAY_SIZE(exynos3_fimc_is_resource),
	.resource	= exynos3_fimc_is_resource,
};

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
struct platform_device exynos3_device_fimc_is_sensor0 = {
	.name		= FIMC_IS_SENSOR_DEV_NAME,
	.id		= 0,
};

struct platform_device exynos3_device_fimc_is_sensor1 = {
	.name		= FIMC_IS_SENSOR_DEV_NAME,
	.id		= 1,
};

struct exynos_platform_fimc_is exynos_fimc_is_default_data __initdata = {
	.hw_ver		= 15,
};

struct exynos_platform_fimc_is_sensor default_fimc_is_sensor __initdata = {
	.scenario	= SENSOR_SCENARIO_NORMAL,
	.mclk_ch	= 0,
	.csi_ch		= 0,
	.flite_ch	= 0,
	.i2c_ch		= 0,
	.i2c_addr	= 0x20,
	.is_softlanding = 0,
};
void __init exynos_fimc_is_set_platdata(struct exynos_platform_fimc_is *pd)
{
	struct exynos_platform_fimc_is *npd;

	if (!pd)
		pd = (struct exynos_platform_fimc_is *)
				&exynos_fimc_is_default_data;

	npd = kmemdup(pd, sizeof(struct exynos_platform_fimc_is), GFP_KERNEL);

	if (!npd) {
		pr_err("%s: no memory for platform data\n", __func__);
	} else {
		npd->cfg_gpio = exynos3_fimc_is_cfg_gpio;
		npd->clk_cfg = exynos3_fimc_is_cfg_clk;
		npd->clk_on = exynos3_fimc_is_clk_on;
		npd->clk_off = exynos3_fimc_is_clk_off;
		npd->print_clk = exynos3_fimc_is_print_clk;
		npd->print_cfg = exynos3_fimc_is_print_cfg;

		exynos3_device_fimc_is.dev.platform_data = npd;
	}
}
void __init exynos_fimc_is_sensor_set_platdata(
	struct exynos_platform_fimc_is_sensor *pd, struct platform_device *pdev)
{
	struct exynos_platform_fimc_is_sensor *npd;

	if (!pd)
		pd = (struct exynos_platform_fimc_is_sensor *)&default_fimc_is_sensor;

	npd = kmemdup(pd, sizeof(struct exynos_platform_fimc_is_sensor), GFP_KERNEL);
	if (!npd) {
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
		return;
	}

	npd->gpio_cfg = exynos_fimc_is_sensor_pins_cfg;
	npd->iclk_cfg = exynos_fimc_is_sensor_iclk_cfg;
	npd->iclk_on = exynos_fimc_is_sensor_iclk_on;
	npd->iclk_off = exynos_fimc_is_sensor_iclk_off;
	npd->mclk_on = exynos_fimc_is_sensor_mclk_on;
	npd->mclk_off = exynos_fimc_is_sensor_mclk_off;

	pdev->dev.platform_data = npd;
}
#endif
#endif
