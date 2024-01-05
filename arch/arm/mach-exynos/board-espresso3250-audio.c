/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/arizona/pdata.h>
#include <linux/io.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/clock.h>
#include <plat/s5p-clock.h>
#include <plat/clock-clksrc.h>
#include <plat/s3c64xx-spi.h>
#include <mach/spi-clocks.h>

#include <mach/irqs.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>

#include "board-espresso3250.h"

#define GPIO_CODEC_LDOENA	EXYNOS3_GPX3(3)
#define GPIO_CODEC_IRQ		EXYNOS3_GPX3(4)
#define GPIO_CODEC_RESET	EXYNOS3_GPM0(4)

#ifdef CONFIG_S3C64XX_DEV_SPI1
static struct regulator_consumer_supply wm5110_fixed_voltage0_supplies[] = {
	REGULATOR_SUPPLY("AVDD", NULL),
	REGULATOR_SUPPLY("LDOVDD", NULL),
	REGULATOR_SUPPLY("CPVDD", NULL),
	REGULATOR_SUPPLY("MICVDD", NULL),
	REGULATOR_SUPPLY("DBVDD1", NULL),
	REGULATOR_SUPPLY("DBVDD2", NULL),
	REGULATOR_SUPPLY("DBVDD3", NULL),
};

static struct regulator_consumer_supply wm5110_fixed_voltage1_supplies[] = {
	REGULATOR_SUPPLY("SPKVDDL", NULL),
	REGULATOR_SUPPLY("SPKVDDR", NULL),
};

static struct regulator_consumer_supply wm5110_fixed_voltage2_supplies =
	REGULATOR_SUPPLY("DCVDD", NULL);

static struct regulator_init_data wm5110_fixed_voltage0_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm5110_fixed_voltage0_supplies),
	.consumer_supplies	= wm5110_fixed_voltage0_supplies,
};

static struct regulator_init_data wm5110_fixed_voltage1_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm5110_fixed_voltage1_supplies),
	.consumer_supplies	= wm5110_fixed_voltage1_supplies,
};

static struct regulator_init_data wm5110_fixed_voltage2_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm5110_fixed_voltage2_supplies,
};

static struct fixed_voltage_config wm5110_fixed_voltage0_config = {
	.supply_name	= "VCC_1.8V",
	.microvolts	= 1800000,
	.gpio		= -EINVAL,
	.init_data	= &wm5110_fixed_voltage0_init_data,
};

static struct fixed_voltage_config wm5110_fixed_voltage1_config = {
	.supply_name	= "VCC_3.3V",
	.microvolts	= 3300000,
	.gpio		= -EINVAL,
	.init_data	= &wm5110_fixed_voltage1_init_data,
};

static struct fixed_voltage_config wm5110_fixed_voltage2_config = {
	.supply_name	= "VDD_1.2V",
	.microvolts	= 1200000,
	.gpio		= -EINVAL,
	.init_data	= &wm5110_fixed_voltage2_init_data,
};

static struct platform_device wm5110_fixed_voltage0 = {
	.name		= "reg-fixed-voltage",
	.id		= 0,
	.dev		= {
		.platform_data	= &wm5110_fixed_voltage0_config,
	},
};

static struct platform_device wm5110_fixed_voltage1 = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev		= {
		.platform_data	= &wm5110_fixed_voltage1_config,
	},
};

static struct platform_device wm5110_fixed_voltage2 = {
	.name		= "reg-fixed-voltage",
	.id		= 2,
	.dev		= {
		.platform_data	= &wm5110_fixed_voltage2_config,
	},
};

static struct arizona_pdata wm5110_platform_data = {
	.reset = GPIO_CODEC_RESET,
	.ldoena = GPIO_CODEC_LDOENA,

	.irq_base = IRQ_BOARD_CODEC_START,
	.irq_gpio = GPIO_CODEC_IRQ,

	/* configure gpio function */
	.gpio_defaults = {
		[0] = 0xA101,
		[1] = 0xA101,
		[2] = 0xA101,
		[3] = 0xA101,
		[4] = 0x8101,
	},
};

static struct s3c64xx_spi_csinfo spi1_wm5110[] = {
	[0] = {
		.line		= EXYNOS3_GPB(5),
		.set_level	= gpio_set_value,
		.fb_delay	= 0x2,
	},
};

static struct spi_board_info spi1_board_info[] __initdata = {
	{
		.modalias		= "wm5110",
		.platform_data		= &wm5110_platform_data,
		.max_speed_hz		= 20 * 1000 * 1000,
		.bus_num		= 1,
		.chip_select		= 0,
		.mode			= SPI_MODE_0,
		.controller_data	= &spi1_wm5110[0],
	}
};
#endif

#ifdef CONFIG_SND_SOC_DUMMY_CODEC
static struct platform_device exynos_dummy_codec = {
	.name = "dummy-codec",
	.id = -1,
};
#endif

static struct platform_device *espresso3250_audio_devices[] __initdata = {
#ifdef CONFIG_S3C64XX_DEV_SPI1
	&s3c64xx_device_spi1,
	&wm5110_fixed_voltage0,
	&wm5110_fixed_voltage1,
	&wm5110_fixed_voltage2,
#endif
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos4_device_i2s2,
#endif
	&samsung_asoc_dma,
#ifdef CONFIG_SND_SOC_DUMMY_CODEC
	&exynos_dummy_codec,
#endif
};

struct arizona_pdata *espresso3250_get_wm5110_pdata(void)
{
	return &wm5110_platform_data;
}

void __init exynos3_espresso3250_audio_init(void)
{
	clk_set_rate(&clk_fout_epll, 49152000);
	clk_enable(&clk_fout_epll);

	/* XUSBXTI 24MHz via XCLKOUT */
	__raw_writel(0x0900, EXYNOS_PMU_DEBUG);

#ifdef CONFIG_S3C64XX_DEV_SPI1
	/* codec_ldoena is enabled by default */
	s3c_gpio_cfgpin(GPIO_CODEC_LDOENA, S3C_GPIO_SFN(1));
	s5p_gpio_set_pd_cfg(GPIO_CODEC_LDOENA, S5P_GPIO_PD_PREV_STATE);
	gpio_request(GPIO_CODEC_LDOENA, "codec_ldoena");
	gpio_direction_output(GPIO_CODEC_LDOENA, 1);
	gpio_free(GPIO_CODEC_LDOENA);

	/* codec_reset */
	s3c_gpio_cfgpin(GPIO_CODEC_RESET, S3C_GPIO_SFN(1));
	s5p_gpio_set_pd_cfg(GPIO_CODEC_RESET, S5P_GPIO_PD_PREV_STATE);

	/* codec_int */
	s3c_gpio_cfgpin(GPIO_CODEC_IRQ, S3C_GPIO_SFN(0xF));
	s5p_gpio_set_pd_cfg(GPIO_CODEC_IRQ, S5P_GPIO_PD_PREV_STATE);

	if (!exynos_spi_cfg_cs(spi1_wm5110[0].line, 1)) {
		s3c64xx_spi1_set_platdata(&s3c64xx_spi1_pdata,
			EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi1_wm5110));

		spi_register_board_info(spi1_board_info,
			ARRAY_SIZE(spi1_board_info));
	} else {
		pr_err("%s: Error requesting gpio for SPI-CH1 CS\n", __func__);
	}
#endif
	platform_add_devices(espresso3250_audio_devices,
			ARRAY_SIZE(espresso3250_audio_devices));
}
