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

#if defined(CONFIG_ARCH_EXYNOS4)
#if defined(CONFIG_VIDEO_EXYNOS_FIMC_IS)
static struct resource exynos4_fimc_is_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS4_PA_FIMC_IS + MCUCTL_BASE, SZ_64K),
	[1] = DEFINE_RES_IRQ(EXYNOS4_IRQ_ARMISP_GIC),
	[2] = DEFINE_RES_IRQ(EXYNOS4_IRQ_ISP_GIC),
};

struct platform_device exynos4_device_fimc_is = {
	.name		= FIMC_IS_DEV_NAME,
	.id		= -1,
	.num_resources	= ARRAY_SIZE(exynos4_fimc_is_resource),
	.resource	= exynos4_fimc_is_resource,
};

struct platform_device exynos_device_fimc_is_sensor0 = {
	.name		= FIMC_IS_SENSOR_DEV_NAME,
	.id		= 0,
};

struct platform_device exynos_device_fimc_is_sensor1 = {
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
#if defined(CONFIG_SOC_EXYNOS3470)
		npd->cfg_gpio = exynos3470_fimc_is_cfg_gpio;
		npd->clk_cfg = exynos3470_fimc_is_cfg_clk;
		npd->clk_on = exynos3470_fimc_is_clk_on;
		npd->clk_off = exynos3470_fimc_is_clk_off;
		npd->print_clk = exynos3470_fimc_is_print_clk;
		npd->print_cfg = exynos3470_fimc_is_print_cfg;
		npd->gate_info->user_clk_gate = exynos3470_fimc_is_set_user_clk_gate;
		npd->gate_info->clk_on_off = exynos3470_fimc_is_clk_gate;
#endif
		exynos4_device_fimc_is.dev.platform_data = npd;
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
#else
static struct resource exynos4_fimc_is_resource[] = {
	[0] = {
		.start	= EXYNOS4_PA_FIMC_IS,
		.end	= EXYNOS4_PA_FIMC_IS + SZ_2M + SZ_256K + SZ_128K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= EXYNOS4_IRQ_ARMISP_GIC,
		.end	= EXYNOS4_IRQ_ARMISP_GIC,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= EXYNOS4_IRQ_ISP_GIC,
		.end	= EXYNOS4_IRQ_ISP_GIC,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device exynos4_device_fimc_is = {
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	.name		= FIMC_IS_DEV_NAME,
#else
	.name		= FIMC_IS_MODULE_NAME,
#endif
	.id		= -1,
	.num_resources	= ARRAY_SIZE(exynos4_fimc_is_resource),
	.resource	= exynos4_fimc_is_resource,
};

struct fimc_is_platform_data exynos4_fimc_is_default_data __initdata;

void __init exynos4_fimc_is_set_platdata(struct fimc_is_platform_data *pd)
{
	struct fimc_is_platform_data *npd;

	if (!pd)
		pd = (struct fimc_is_platform_data *)
					&exynos4_fimc_is_default_data;

	npd = kmemdup(pd, sizeof(struct fimc_is_platform_data), GFP_KERNEL);

	if (!npd) {
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
	} else {
		if (!npd->cfg_gpio)
			npd->cfg_gpio = exynos4_fimc_is_cfg_gpio;
		if (!npd->clk_cfg)
			npd->clk_cfg = exynos4_fimc_is_cfg_clk;
		if (!npd->clk_on)
			npd->clk_on = exynos4_fimc_is_clk_on;
		if (!npd->clk_off)
			npd->clk_off = exynos4_fimc_is_clk_off;
		if (!npd->clk_get)
			npd->clk_get = exynos4_fimc_is_clk_get;
		if (!npd->clk_put)
			npd->clk_put = exynos4_fimc_is_clk_put;

		exynos4_device_fimc_is.dev.platform_data = npd;
	}
}
#endif
#elif defined(CONFIG_ARCH_EXYNOS5)
static struct resource exynos5_fimc_is_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS5_PA_FIMC_IS + MCUCTL_BASE, SZ_64K),
	[1] = DEFINE_RES_IRQ(IRQ_ARMISP_GIC),
	[2] = DEFINE_RES_IRQ(IRQ_ISP_GIC),
};

struct platform_device exynos5_device_fimc_is = {
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	.name		= FIMC_IS_DEV_NAME,
#else
	.name		= FIMC_IS_MODULE_NAME,
#endif
	.id		= -1,
	.num_resources	= ARRAY_SIZE(exynos5_fimc_is_resource),
	.resource	= exynos5_fimc_is_resource,
};

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
struct platform_device exynos_device_fimc_is_sensor0 = {
	.name		= FIMC_IS_SENSOR_DEV_NAME,
	.id		= 0,
};

struct platform_device exynos_device_fimc_is_sensor1 = {
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
};

void __init exynos_fimc_is_set_platdata(struct exynos_platform_fimc_is *pd)
{
	struct exynos_platform_fimc_is *npd;

	if (!pd)
		pd = (struct exynos_platform_fimc_is *)&exynos_fimc_is_default_data;

	npd = kmemdup(pd, sizeof(struct exynos_platform_fimc_is), GFP_KERNEL);
	if (!npd) {
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
		return;
	}

#if defined(CONFIG_SOC_EXYNOS5260)
	npd->cfg_gpio = exynos5260_fimc_is_cfg_gpio;
	npd->clk_cfg = exynos5260_fimc_is_cfg_clk;
	npd->clk_on = exynos5260_fimc_is_clk_on;
	npd->clk_off = exynos5260_fimc_is_clk_off;
	npd->print_clk = exynos5260_fimc_is_print_clk;
	npd->print_cfg = exynos5260_fimc_is_print_cfg;
	npd->gate_info->user_clk_gate = exynos5260_fimc_is_set_user_clk_gate;
	npd->gate_info->clk_on_off = exynos5260_fimc_is_clk_gate;
#endif
	exynos5_device_fimc_is.dev.platform_data = npd;
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
#else
struct exynos5_platform_fimc_is exynos5_fimc_is_default_data __initdata = {
	.hw_ver = 15,
};

void __init exynos5_fimc_is_set_platdata(struct exynos5_platform_fimc_is *pd)
{
	struct exynos5_platform_fimc_is *npd;

	if (!pd)
		pd = (struct exynos5_platform_fimc_is *) &exynos5_fimc_is_default_data;

	npd = kmemdup(pd, sizeof(struct exynos5_platform_fimc_is), GFP_KERNEL);
	if (!npd) {
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
		return;
	}

#if defined(CONFIG_SOC_EXYNOS5250)
	npd->cfg_gpio = exynos5_fimc_is_cfg_gpio;
	npd->clk_cfg = exynos5250_fimc_is_cfg_clk;
	npd->clk_on = exynos5250_fimc_is_clk_on;
	npd->clk_off = exynos5250_fimc_is_clk_off;
	npd->sensor_power_on = exynos5_fimc_is_sensor_power_on;
	npd->sensor_power_off = exynos5_fimc_is_sensor_power_off;
	npd->print_cfg = exynos5_fimc_is_print_cfg;
#elif defined(CONFIG_SOC_EXYNOS5410)
	npd->cfg_gpio = exynos5_fimc_is_cfg_gpio;
	npd->clk_cfg = exynos5410_fimc_is_cfg_clk;
	npd->clk_on = exynos5410_fimc_is_clk_on;
	npd->clk_off = exynos5410_fimc_is_clk_off;
	npd->sensor_clock_on = exynos5410_fimc_is_sensor_clk_on;
	npd->sensor_clock_off = exynos5410_fimc_is_sensor_clk_off;
	npd->sensor_power_on = exynos5_fimc_is_sensor_power_on;
	npd->sensor_power_off = exynos5_fimc_is_sensor_power_off;
	npd->print_cfg = exynos5_fimc_is_print_cfg;
#elif defined(CONFIG_SOC_EXYNOS5420)
	npd->cfg_gpio = exynos5_fimc_is_cfg_gpio;
	npd->clk_cfg = exynos5420_fimc_is_cfg_clk;
	npd->clk_on = exynos5420_fimc_is_clk_on;
	npd->clk_off = exynos5420_fimc_is_clk_off;
	npd->sensor_clock_on = exynos5420_fimc_is_sensor_clk_on;
	npd->sensor_clock_off = exynos5420_fimc_is_sensor_clk_off;
	npd->sensor_power_on = exynos5_fimc_is_sensor_power_on;
	npd->sensor_power_off = exynos5_fimc_is_sensor_power_off;
	npd->print_cfg = exynos5_fimc_is_print_cfg;
#endif

	exynos5_device_fimc_is.dev.platform_data = npd;
}
#endif
#elif defined(CONFIG_ARCH_EXYNOS3)
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
