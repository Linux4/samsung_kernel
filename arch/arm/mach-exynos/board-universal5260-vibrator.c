/*
 * Copyright (C) 2012 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#ifdef CONFIG_MFD_MAX77804K
#include <linux/mfd/max77803.h>
#include <linux/mfd/max77804k-private.h>
#else
#ifdef CONFIG_MFD_MAX77803
#include <linux/mfd/max77803.h>
#include <linux/mfd/max77803-private.h>
#endif
#endif
#include <mach/gpio-exynos.h>

#include "board-universal5260.h"

void __init exynos5_universal5260_vibrator_init(void)
{
	int ret;
	ret = gpio_request(GPIO_VIBTONE_PWM, "motor_pwm");
	if (ret) {
		printk(KERN_ERR "%s gpio_request error %d\n",
				__func__, ret);
		goto init_fail;
	}

	ret = s3c_gpio_cfgpin(GPIO_VIBTONE_PWM, S3C_GPIO_SFN(0x2));
	if (ret) {
		printk(KERN_ERR "%s s3c_gpio_cfgpin error %d\n",
				__func__, ret);
		goto init_fail;
	}

	ret = platform_device_register(&s3c_device_timer[0]);

	if (ret)
		printk(KERN_ERR "%s failed platform_device_register with error %d\n",
				__func__, ret);

init_fail:
	return;
}

