/*
 * Copyright (c) 2010-2012 Samsung Electronics Co., Ltd.
 *
 * I2C7 GPIO configuration.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

struct platform_device; /* don't need the contents */

#include <linux/gpio.h>
#include <linux/io.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>
#include <plat/cpu.h>
#include <plat/map-base.h>

#define EXYNOS3472_I2C_MUX (S3C_VA_SYS + 0x228)

void s3c_i2c7_cfg_gpio(struct platform_device *dev)
{
	if (soc_is_exynos5250())
		s3c_gpio_cfgall_range(EXYNOS5_GPB2(2), 2,
				      S3C_GPIO_SFN(3), S3C_GPIO_PULL_UP);

	else if (soc_is_exynos5260())
		s3c_gpio_cfgall_range(EXYNOS5260_GPB5(6), 2,
				      S3C_GPIO_SFN(2), S3C_GPIO_PULL_UP);

	else if (soc_is_exynos3250())
		s3c_gpio_cfgall_range(EXYNOS3_GPD0(2), 2,
				      S3C_GPIO_SFN(3), S3C_GPIO_PULL_UP);

	else	/* EXYNOS4210, EXYNOS4212, EXYNOS4412 and EXYNOS4415 */
		s3c_gpio_cfgall_range(EXYNOS4_GPD0(2), 2,
				      S3C_GPIO_SFN(3), S3C_GPIO_PULL_UP);
}

void s3c_i2c7_cfg_mux()
{
#if 0
	if (soc_is_exynos3472())
		writel(0x0, EXYNOS3472_I2C_MUX);
#endif
}
