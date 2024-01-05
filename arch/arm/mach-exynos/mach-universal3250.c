/*
 * SAMSUNG UNIVERSAL3250 machine file
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
#include <linux/gpio.h>
#include <linux/spi/spi.h>

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
#include <plat/s3c64xx-spi.h>
#include <mach/spi-clocks.h>

#include "common.h"
#include "board-universal3250.h"

#define UNIVERSAL3250_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define UNIVERSAL3250_ULCON_DEFAULT	S3C2410_LCON_CS8

#define UNIVERSAL3250_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)


static struct s3c2410_uartcfg universal3250_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= UNIVERSAL3250_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3250_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3250_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= UNIVERSAL3250_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3250_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3250_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= UNIVERSAL3250_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3250_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3250_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= UNIVERSAL3250_UCON_DEFAULT,
		.ulcon		= UNIVERSAL3250_ULCON_DEFAULT,
		.ufcon		= UNIVERSAL3250_UFCON_DEFAULT,
	},
};

#ifdef CONFIG_S3C64XX_DEV_SPI0
static struct s3c64xx_spi_csinfo spi0_csi[] = {
	[0] = {
		.line		= EXYNOS3_GPB(1),
		.set_level	= gpio_set_value,
		.fb_delay	= 0x2,
	},
};
#endif

#ifdef CONFIG_S3C64XX_DEV_SPI1
static struct s3c64xx_spi_csinfo spi1_csi[] = {
	[0] = {
		.line		= EXYNOS3_GPB(5),
		.set_level	= gpio_set_value,
		.fb_delay	= 0x2,
	},
};
#endif

#ifdef CONFIG_S3C64XX_DEV_SPI0
static struct spi_board_info spi0_board_info[1] __initdata = {
	{
		.modalias		= "spidev",
		.platform_data		= NULL,
		.max_speed_hz		= 10 * 1000 * 1000,
		.bus_num		= 0,
		.chip_select		= 0,
		.mode			= SPI_MODE_0,
		.controller_data	= &spi0_csi[0],
	}
};
#endif

#ifdef CONFIG_S3C64XX_DEV_SPI1
static struct spi_board_info spi1_board_info[1] __initdata = {
	{
		.modalias		= "spidev",
		.platform_data		= NULL,
		.max_speed_hz		= 10 * 1000 * 1000,
		.bus_num		= 1,
		.chip_select		= 0,
		.mode			= SPI_MODE_0,
		.controller_data	= &spi1_csi[0],
	}
};
#endif

static void __init universal3250_map_io(void)
{
	exynos_init_io(NULL, 0);
}

static inline void exynos_reserve_mem(void)
{
}

/* ADC */
static struct s3c_adc_platdata espresso3250_adc_data __initdata = {
	.phy_init	= s3c_adc_phy_init,
	.phy_exit	= s3c_adc_phy_exit,
};

#ifdef CONFIG_S3C_DEV_WDT
/* WDT */
static struct s3c_watchdog_platdata smdk5410_watchdog_platform_data = {
	exynos_pmu_wdt_control,
	PMU_WDT_RESET_TYPE0,
};
#endif

static struct platform_device *universal3250_devices[] __initdata = {
	&s3c_device_adc,
#ifdef CONFIG_S3C_DEV_WDT
	&s3c_device_wdt,
#endif
#ifdef CONFIG_MALI400
	&exynos4_device_g3d,
#endif
#ifdef CONFIG_S3C64XX_DEV_SPI0
	&s3c64xx_device_spi0,
#endif
#ifdef CONFIG_S3C64XX_DEV_SPI1
	&s3c64xx_device_spi1,
#endif
};

static void __init universal3250_machine_init(void)
{

	s3c_adc_set_platdata(&espresso3250_adc_data);
#ifdef CONFIG_S3C_DEV_WDT
	s3c_watchdog_set_platdata(&smdk5410_watchdog_platform_data);
#endif

	exynos3_universal3250_clock_init();
	exynos3_universal3250_mmc_init();
	exynos3_universal3250_power_init();
	exynos3_universal3250_input_init();
	exynos3_universal3250_display_init();
	exynos3_universal3250_usb_init();
	exynos3_universal3250_media_init();

	platform_add_devices(universal3250_devices, ARRAY_SIZE(universal3250_devices));

#ifdef CONFIG_S3C64XX_DEV_SPI0
	if (!exynos_spi_cfg_cs(spi0_csi[0].line, 0)) {
		s3c64xx_spi0_set_platdata(&s3c64xx_spi0_pdata,
				EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi0_csi));

		spi_register_board_info(spi0_board_info,
				ARRAY_SIZE(spi0_board_info));
	} else {
		pr_err("%s: Error requesting gpio for SPI0 CS\n", __func__);
	}
#endif

#ifdef CONFIG_S3C64XX_DEV_SPI1
	if (!exynos_spi_cfg_cs(spi1_csi[0].line, 1)) {
		s3c64xx_spi1_set_platdata(&s3c64xx_spi1_pdata,
				EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi1_csi));

		spi_register_board_info(spi1_board_info,
				ARRAY_SIZE(spi1_board_info));
	} else {
		pr_err("%s: Error requesting gpio for SPI1 CS\n", __func__);
	}
#endif
	exynos3_universal3250_gpio_init();
}

static void __init universal3250_init_early(void)
{
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(universal3250_uartcfgs, ARRAY_SIZE(universal3250_uartcfgs));
}

MACHINE_START(UNIVERSAL3250, "UNIVERSAL3250")
	.init_irq	= exynos3_init_irq,
	.init_early	= universal3250_init_early,
	.map_io		= universal3250_map_io,
	.handle_irq	= gic_handle_irq,
	.init_machine	= universal3250_machine_init,
	.timer		= &exynos4_timer,
	.restart	= exynos3_restart,
	.reserve 	= exynos_reserve_mem,
MACHINE_END
