/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>

#include "board-universal222ap.h"

#define GPIO_KMINI_GPIO_I2C_SCL	EXYNOS4_GPM4(5)
#define GPIO_KMINI_GPIO_I2C_SDA	EXYNOS4_GPM4(4)

/* I2C21 */
static struct i2c_gpio_platform_data gpio_i2c_data21 = {
	.scl_pin = GPIO_KMINI_GPIO_I2C_SCL,
	.sda_pin = GPIO_KMINI_GPIO_I2C_SDA,
	.udelay = 2,
};

static struct platform_device s3c_device_i2c21 = {
	.name = "i2c-gpio",
	.id = 21,
	.dev.platform_data = &gpio_i2c_data21,
};

/* I2C21 */
static struct i2c_board_info i2c_device21_emul[] __initdata = {
	{
		I2C_BOARD_INFO("ktd2026", 0x30),
	},
	{
		I2C_BOARD_INFO("mc96fr116c", 0xA0 >> 1),
	},
};

static struct platform_device *kmini_i2c21_devices[] __initdata = {
	&s3c_device_i2c21,
};

int board_universal_ss222ap_add_platdata_gpio_i2c(int index,
		void *platform_data)
{
	if (ARRAY_SIZE(i2c_device21_emul) <= index)
		return -EINVAL;

	i2c_device21_emul[index].platform_data = platform_data;

	return 0;
}

void board_universal_ss222ap_init_gpio_i2c(void)
{
	s3c_gpio_cfgpin(gpio_i2c_data21.scl_pin, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio_i2c_data21.scl_pin, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio_i2c_data21.scl_pin, S5P_GPIO_DRVSTR_LV1);

	s3c_gpio_cfgpin(gpio_i2c_data21.sda_pin, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio_i2c_data21.sda_pin, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio_i2c_data21.sda_pin, S5P_GPIO_DRVSTR_LV1);

	i2c_register_board_info(21, i2c_device21_emul,
		ARRAY_SIZE(i2c_device21_emul));

	platform_add_devices(kmini_i2c21_devices,
		ARRAY_SIZE(kmini_i2c21_devices));
}

