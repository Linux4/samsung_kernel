/*
 * arch/arm/mach-exynos/board-kmini-muic.c
 *
 * c source file supporting MUIC board specific register
 *
 * Copyright (C) 2013 Samsung Electronics
 * Seung-Jin Hahn <sjin.hahn@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#if defined(CONFIG_MUIC_I2C_USE_S3C_DEV_I2C6)
#include <plat/devs.h>
#include <plat/iic.h>
#endif

#include <linux/muic.h>

#include "board-universal222ap.h"

static struct muic_platform_data exynos4_muic_pdata;

static int muic_init_gpio_cb(int switch_sel)
{
	struct muic_platform_data *pdata = &exynos4_muic_pdata;
	const char *usb_mode;
	const char *uart_mode;
	int ret = 0;

	pr_info("%s\n", __func__);

	if (switch_sel & SWITCH_SEL_USB_MASK) {
		pdata->usb_path = MUIC_PATH_USB_AP;
		usb_mode = "PDA";
	} else {
		pdata->usb_path = MUIC_PATH_USB_CP;
		usb_mode = "MODEM";
	}

	if (pdata->set_gpio_usb_sel)
		ret = pdata->set_gpio_usb_sel(pdata->uart_path);

	if (switch_sel & SWITCH_SEL_UART_MASK) {
		pdata->uart_path = MUIC_PATH_UART_AP;
		uart_mode = "AP";
	} else {
		pdata->uart_path = MUIC_PATH_UART_CP;
		uart_mode = "CP";
	}

	if (pdata->set_gpio_uart_sel)
		ret = pdata->set_gpio_uart_sel(pdata->uart_path);

	pr_info("%s: usb_path(%s), uart_path(%s)\n", __func__, usb_mode,
			uart_mode);

	return ret;
}


static struct muic_platform_data exynos4_muic_pdata = {
	.irq_gpio		= GPIO_MUIC_IRQ,

#if defined(GPIO_USB_SEL)
	.gpio_usb_sel		= GPIO_USB_SEL,
#endif /* GPIO_USB_SEL */

#if defined(GPIO_UART_SEL)
	.gpio_uart_sel		= GPIO_UART_SEL,
#endif /* GPIO_UART_SEL */

#if defined(GPIO_DOC_SWITCH)
	.gpio_doc_switch = GPIO_DOC_SWITCH,
#endif /* GPIO_DOC_SWITCH*/

	.init_gpio_cb		= muic_init_gpio_cb,
};

#if defined(CONFIG_MUIC_I2C_USE_S3C_DEV_I2C6)
/* I2C6 */
static struct i2c_board_info i2c_devs6_hw[] __initdata = {
	{
		I2C_BOARD_INFO(MUIC_DEV_NAME, (0x4A >> 1)),
		.platform_data	= &exynos4_muic_pdata,
	}
};
#endif /* CONFIG_MUIC_I2C_USE_S3C_DEV_I2C6 */

#if defined(CONFIG_MUIC_I2C_USE_I2C17_EMUL)
/* I2C17 */
static struct i2c_board_info i2c_devs17_emul[] __initdata = {
	{
		I2C_BOARD_INFO(MUIC_DEV_NAME, (0x4A >> 1)),
		.platform_data	= &exynos4_muic_pdata,
	}
};

static struct i2c_gpio_platform_data gpio_i2c_data17 = {
	.sda_pin = GPIO_MUIC_SDA,
	.scl_pin = GPIO_MUIC_SCL,
};

struct platform_device s3c_device_i2c17 = {
	.name = "i2c-gpio",
	.id = 17,
	.dev.platform_data = &gpio_i2c_data17,
};
#endif /* CONFIG_MUIC_I2C_USE_I2C17_EMUL */

static struct platform_device *exynos4_muic_device[] __initdata = {
#if defined(CONFIG_MUIC_I2C_USE_S3C_DEV_I2C6) && defined(CONFIG_SOC_EXYNOS4270)
	&s3c_device_i2c6,
#endif /* CONFIG_MUIC_I2C_USE_S3C_DEV_I2C6 */

#if defined(CONFIG_MUIC_I2C_USE_I2C17_EMUL)
	&s3c_device_i2c17,
#endif /* CONFIG_MUIC_I2C_USE_I2C17_EMUL */
};

void __init exynos4_smdk4270_muic_init(void)
{
#if defined(CONFIG_MUIC_I2C_USE_S3C_DEV_I2C6) && defined(CONFIG_SOC_EXYNOS4270)
	s3c_i2c6_set_platdata(NULL);

	i2c_register_board_info(6, i2c_devs6_hw,
				ARRAY_SIZE(i2c_devs6_hw));
#endif /* CONFIG_MUIC_I2C_USE_S3C_DEV_I2C6 */

#if defined(CONFIG_MUIC_I2C_USE_I2C17_EMUL)
	i2c_register_board_info(17, i2c_devs17_emul,
				ARRAY_SIZE(i2c_devs17_emul));
#endif /* CONFIG_MUIC_I2C_USE_I2C17_EMUL */

	platform_add_devices(exynos4_muic_device,
			ARRAY_SIZE(exynos4_muic_device));
}
