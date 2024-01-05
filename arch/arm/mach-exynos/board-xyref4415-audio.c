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
#include <plat/clock.h>
#include <plat/s5p-clock.h>
#include <plat/clock-clksrc.h>

#include <mach/map.h>
#include <mach/regs-pmu.h>
#include <mach/regs-clock-exynos4415.h>
#include <mach/regs-audss.h>

#include "board-xyref4415.h"


#if defined(CONFIG_SND_SOC_ES515) || defined(CONFIG_SND_SOC_ES515_MODULE)
static struct esxxx_platform_data xyref_esxxx_data = {
	.reset_gpio = EXYNOS4_GPY4(6), /* gpy4(6) */
	.wakeup_gpio = EXYNOS4_GPY4(7), /* gpy4(7) */
	.int_gpio = EXYNOS4_GPX0(5), /* gpx0(5) */
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

static struct platform_device *xyref4415_audio_devices[] __initdata = {
	&exynos_device_audss,
	&s3c_device_i2c1,
#ifdef CONFIG_SND_SAMSUNG_ALP
	&exynos4_device_srp,
#endif
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos4_device_i2s0,
#endif
#ifdef CONFIG_SND_SAMSUNG_PCM
	&exynos4_device_pcm0,
	&exynos_smdk_pcm,
#endif
#if defined(CONFIG_SND_SAMSUNG_SPDIF) || defined(CONFIG_SND_SAMSUNG_AUX_SPDIF)
	&exynos4_device_spdif,
#endif
	&samsung_asoc_dma,
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
	&samsung_asoc_idma,
#endif
#ifdef CONFIG_SND_SOC_DUMMY_CODEC
	&exynos_dummy_codec,
#endif
};

void __init exynos4_xyref4415_audio_init(void)
{
	writel(0x717, EXYNOS_CLKDIV_AUDSS);
	clk_set_rate(&clk_fout_epll, 786432000);

	s3c_i2c1_set_platdata(NULL);
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	platform_add_devices(xyref4415_audio_devices,
			ARRAY_SIZE(xyref4415_audio_devices));

	/* set codec_int pin as EINT */
	s3c_gpio_cfgpin(EXYNOS4_GPX0(5), S3C_GPIO_SFN(0xF));

	/* gpio power-down config */
	s5p_gpio_set_pd_cfg(EXYNOS4_GPY4(6), S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_cfg(EXYNOS4_GPY4(7), S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_cfg(EXYNOS4_GPX0(5), S5P_GPIO_PD_PREV_STATE);

	/* set clock for clkout. this clock is used to be the
	   clock source for es515 audio codec */
	writel(0x1090B, EXYNOS4415_CLKOUT_CMU_TOP);	/* ACLK_200 / 10 */
	writel(0x0100, EXYNOS_PMU_DEBUG);		/* CLKOUT from CMU_TOP */
}
