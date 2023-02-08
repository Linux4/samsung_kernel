/*
 * Copyright (c) 2010-2012 Samsung Electronics Co., Ltd.
 *
 * I2C6 GPIO configuration.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

struct platform_device; /* don't need the contents */

#include <linux/gpio.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>
#include <plat/cpu.h>

void s3c_i2c6_cfg_gpio(struct platform_device *dev)
{
	if (soc_is_exynos5250())
		s3c_gpio_cfgall_range(EXYNOS5_GPB1(3), 2,
				      S3C_GPIO_SFN(4), S3C_GPIO_PULL_UP);

	else if (soc_is_exynos5260())
		s3c_gpio_cfgall_range(EXYNOS5260_GPB5(4), 2,
				      S3C_GPIO_SFN(2), S3C_GPIO_PULL_UP);

	else if (soc_is_exynos3250())
		s3c_gpio_cfgall_range(EXYNOS3_GPC1(3), 2,
				      S3C_GPIO_SFN(4), S3C_GPIO_PULL_UP);

	else	/* EXYNOS4210, EXYNOS4212, EXYNOS4412, and EXYNOS4415 */
		s3c_gpio_cfgall_range(EXYNOS4_GPC1(3), 2,
				      S3C_GPIO_SFN(4), S3C_GPIO_PULL_UP);
}
