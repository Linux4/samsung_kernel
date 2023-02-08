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
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>

#if defined(CONFIG_SND_SOC_ES515) || defined(CONFIG_SND_SOC_ES515_MODULE)
#include <linux/i2c/esxxx.h>
#endif
#include <linux/io.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>

#include <mach/hs-iic.h>
#include <mach/map.h>

#include "board-xyref5260.h"

#define NOT_USE_24MHZ

#if defined(CONFIG_SND_SOC_ES515) || defined(CONFIG_SND_SOC_ES515_MODULE)
static struct esxxx_platform_data xyref_esxxx_data = {
	.reset_gpio = EXYNOS5260_GPD0(5), /* gpd0(5) */
	.wakeup_gpio = EXYNOS5260_GPD0(6), /* gpd0(6) */
	.int_gpio = EXYNOS5260_GPX0(5), /* gpx0(5) */
};
#endif

static struct i2c_board_info __initdata i2c_devs1[] = {
#if defined(CONFIG_SND_SOC_ES515) || defined(CONFIG_SND_SOC_ES515_MODULE)
{
	I2C_BOARD_INFO("es515", 0x3e),
	.platform_data = &xyref_esxxx_data,
},
#endif
};

#ifdef CONFIG_SND_SAMSUNG_PCM
static struct platform_device exynos_smdk_pcm = {
	.name = "samsung-smdk-pcm",
	.id = -1,
};
#endif

#ifdef CONFIG_SND_SOC_DUMMY_CODEC
static struct platform_device exynos_dummy_codec = {
	.name = "dummy-codec",
	.id = -1,
};
#endif

static struct platform_device *xyref5260_audio_devices[] __initdata = {
	&exynos5_device_lpass,
	&exynos5_device_hs_i2c1,
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos5_device_i2s0,
#endif
#ifdef CONFIG_SND_SAMSUNG_AUX_HDMI
	&exynos5_device_i2s1,
#endif
#ifdef CONFIG_SND_SAMSUNG_PCM
	&exynos5_device_pcm0,
	&exynos_smdk_pcm,
#endif
#if defined(CONFIG_SND_SAMSUNG_SPDIF) || defined(CONFIG_SND_SAMSUNG_AUX_SPDIF)
	&exynos5_device_spdif,
#endif
	&samsung_asoc_dma,
#ifdef CONFIG_SND_SOC_DUMMY_CODEC
	&exynos_dummy_codec,
#endif
};

static void exynos_cfg_hs_i2c1_gpio(struct platform_device *pdev)
{
	s3c_gpio_cfgpin_range(EXYNOS5260_GPB3(2), 2,  S3C_GPIO_SFN(2));
}


struct exynos5_platform_i2c hs_i2c1_data __initdata = {
	.bus_number = 1,
	.operation_mode = HSI2C_POLLING,
	.speed_mode = HSI2C_FAST_SPD,
	.fast_speed = 400000,
	.high_speed = 2500000,
	.cfg_gpio = exynos_cfg_hs_i2c1_gpio,
};

void __init exynos5_xyref5260_audio_init(void)
{
	u32 tmp;
	exynos5_hs_i2c1_set_platdata(&hs_i2c1_data);
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	platform_add_devices(xyref5260_audio_devices,
			ARRAY_SIZE(xyref5260_audio_devices));

	/* set codec_int pin as EINT */
	s3c_gpio_cfgpin(EXYNOS5260_GPX0(5), S3C_GPIO_SFN(0xF));

	/* gpio power-down config */
	s5p_gpio_set_pd_cfg(EXYNOS5260_GPD0(5), S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_cfg(EXYNOS5260_GPD0(6), S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_cfg(EXYNOS5260_GPX0(5), S5P_GPIO_PD_PREV_STATE);

	/* set clock for clkout. this clock is used to be the
	   clock source for es515 audio codec */
#ifdef NOT_USE_24MHZ
	/* to make 20Mhz, it sets clkout as AUD_PLL divided by 9 */
	tmp = readl(EXYNOS5260_VA_CMU_TOP + 0xC00);
	tmp &= ~(0x13f1f);
	tmp |= 0x10802;
	writel(tmp, EXYNOS5260_VA_CMU_TOP + 0xC00);
#endif

	tmp = readl(EXYNOS5260_VA_PMU + 0xa00);
	tmp &= ~(0x1f00);

#ifdef NOT_USE_24MHZ
	tmp |= (0x7 << 8);
#else
	tmp |= (0x1 <<12);
#endif
	writel (tmp, EXYNOS5260_VA_PMU + 0xa00);

	writel (readl(S5P_VA_AUDSS + 0x58) | 0x2, S5P_VA_AUDSS + 0x58);
}
