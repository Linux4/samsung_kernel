/*
 * SAMSUNG ESPRESSO3250 machine file
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/io.h>
#include <linux/memblock.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/i2c.h>
#include <linux/cma.h>
#include <linux/ion.h>

#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <mach/map.h>
#include <mach/memory.h>

#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/regs-serial.h>
#include <plat/clock.h>
#include <plat/iic.h>
#include <plat/adc.h>
#include <plat/watchdog.h>
#include <mach/regs-pmu.h>
#include <mach/gpio.h>
#include <mach/pmu.h>
#include <plat/gpio-cfg.h>

#include "common.h"
#include "board-espresso3250.h"

#define ESPRESSO3472_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define ESPRESSO3472_ULCON_DEFAULT	S3C2410_LCON_CS8

#define ESPRESSO3472_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)


static struct s3c2410_uartcfg espresso3472_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= ESPRESSO3472_UCON_DEFAULT,
		.ulcon		= ESPRESSO3472_ULCON_DEFAULT,
		.ufcon		= ESPRESSO3472_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= ESPRESSO3472_UCON_DEFAULT,
		.ulcon		= ESPRESSO3472_ULCON_DEFAULT,
		.ufcon		= ESPRESSO3472_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= ESPRESSO3472_UCON_DEFAULT,
		.ulcon		= ESPRESSO3472_ULCON_DEFAULT,
		.ufcon		= ESPRESSO3472_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= ESPRESSO3472_UCON_DEFAULT,
		.ulcon		= ESPRESSO3472_ULCON_DEFAULT,
		.ufcon		= ESPRESSO3472_UFCON_DEFAULT,
	},
};

static void __init espresso3472_map_io(void)
{
	exynos_init_io(NULL, 0);
}

static inline void exynos_reserve_mem(void)
{
}

/* ADC */
static struct s3c_adc_platdata espresso3472_adc_data __initdata = {
	.phy_init	= s3c_adc_phy_init,
	.phy_exit	= s3c_adc_phy_exit,
};

#ifdef CONFIG_S3C_DEV_WDT
/* WDT */
static struct s3c_watchdog_platdata smdk3472_watchdog_platform_data = {
	exynos_pmu_wdt_control,
	PMU_WDT_RESET_TYPE0,
};
#endif

static struct platform_device *espresso3472_devices[] __initdata = {
	&s3c_device_adc,
#ifdef CONFIG_S3C_DEV_WDT
	&s3c_device_wdt,
#endif
#ifdef CONFIG_MALI400
	&exynos4_device_g3d,
#endif
};

static void __init espresso3472_machine_init(void)
{

	s3c_adc_set_platdata(&espresso3472_adc_data);
#ifdef CONFIG_S3C_DEV_WDT
	s3c_watchdog_set_platdata(&smdk3472_watchdog_platform_data);
#endif

	exynos3_espresso3250_clock_init();
	exynos3_espresso3250_mmc_init();
	exynos3_espresso3250_power_init();
	exynos3_espresso3250_input_init();
	exynos3_espresso3250_display_init();
	exynos3_espresso3250_usb_init();
	exynos3_espresso3250_media_init();
	exynos3_espresso3250_audio_init();
	exynos3_espresso3250_camera_init();

	platform_add_devices(espresso3472_devices, ARRAY_SIZE(espresso3472_devices));
}

static void __init espresso3472_init_early(void)
{
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(espresso3472_uartcfgs,ARRAY_SIZE(espresso3472_uartcfgs));
}

MACHINE_START(ESPRESSO3250, "ESPRESSO3250")
	.init_irq	= exynos3_init_irq,
	.init_early	= espresso3472_init_early,
	.map_io		= espresso3472_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= espresso3472_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos3_restart,
	.reserve 	= exynos_reserve_mem,
MACHINE_END
