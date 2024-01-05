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
#ifdef CONFIG_LEDS_KTD2026
#include <linux/leds-ktd2026.h>
#endif
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <asm/system_info.h>

#include "board-universal4415.h"

#ifdef CONFIG_MACH_MEGA2
#define	GPIO_SVC_LED_PWR_ON		EXYNOS4_GPB(6)
#define	GPIO_SVC_LED_SCL_REV07	EXYNOS4_GPC0(0)
#define	GPIO_SVC_LED_SDA_REV07	EXYNOS4_GPC0(1)
#define	GPIO_SVC_LED_SCL_REV04	EXYNOS4_GPF0(1)
#define	GPIO_SVC_LED_SDA_REV04	EXYNOS4_GPK1(2)

static void __init leds_init(void)
{
	int ret = 0, gpio = GPIO_SVC_LED_PWR_ON;

	ret = gpio_request(gpio, "PEN_RESET_N");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_RESET_N.(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_direction_output(gpio, 0);
}

static int leds_en(bool en)
{
	return gpio_direction_output(GPIO_SVC_LED_PWR_ON, en);
}
#endif	/* CONFIG_MACH_MEGA2 */

/* I2C21 */
static struct i2c_gpio_platform_data gpio_i2c_data21 = {
	.scl_pin = GPIO_SVC_LED_SCL,
	.sda_pin = GPIO_SVC_LED_SDA,
	.udelay = 2,
};

static struct platform_device s3c_device_i2c21 = {
	.name = "i2c-gpio",
	.id = 21,
	.dev.platform_data = &gpio_i2c_data21,
};

#ifdef CONFIG_MACH_MEGA2
#define MAX_CURRENT		30
#else
#define MAX_CURRENT		LED_MAX_CURRENT
#endif

#ifdef CONFIG_LEDS_KTD2026
#define LED_CONF(_name)				\
{										\
	.name = _name,						\
	.brightness = LED_OFF,					\
	.max_brightness = MAX_CURRENT,	\
	.flags = 0,							\
}

static struct ktd2026_led_conf leds_conf[] = {
	LED_CONF("led_r"),
	LED_CONF("led_g"),
	LED_CONF("led_b"),
};

struct ktd2026_led_pdata leds_pdata = {
	.led_conf = leds_conf,
	.leds= ARRAY_SIZE(leds_conf),
};
#endif

/* I2C21 */
static struct i2c_board_info i2c_device21_emul[] __initdata = {
	{
		I2C_BOARD_INFO("ktd2026", 0x30),
#ifdef CONFIG_LEDS_KTD2026
		.platform_data = &leds_pdata,
#endif
	},
};

static struct platform_device *exynos4_i2c21_devices[] __initdata = {
	&s3c_device_i2c21,
};

void __init exynos4_universal4415_led_init(void)
{
#ifdef CONFIG_MACH_MEGA2
	if (0x7 <= system_rev) {
		leds_init();
		leds_pdata.led_en = leds_en;
		gpio_i2c_data21.scl_pin = GPIO_SVC_LED_SCL_REV07;
		gpio_i2c_data21.sda_pin = GPIO_SVC_LED_SDA_REV07;
	} else if (0x4 <= system_rev) {
		gpio_i2c_data21.scl_pin = GPIO_SVC_LED_SCL_REV04;
		gpio_i2c_data21.sda_pin = GPIO_SVC_LED_SDA_REV04;
	}
#endif	/* CONFIG_MACH_MEGA2 */

	s3c_gpio_cfgpin(gpio_i2c_data21.scl_pin, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio_i2c_data21.scl_pin, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio_i2c_data21.scl_pin, S5P_GPIO_DRVSTR_LV1);

	s3c_gpio_cfgpin(gpio_i2c_data21.sda_pin, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio_i2c_data21.sda_pin, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio_i2c_data21.sda_pin, S5P_GPIO_DRVSTR_LV1);

	i2c_register_board_info(21, i2c_device21_emul,
		ARRAY_SIZE(i2c_device21_emul));

	platform_add_devices(exynos4_i2c21_devices,
		ARRAY_SIZE(exynos4_i2c21_devices));
}
