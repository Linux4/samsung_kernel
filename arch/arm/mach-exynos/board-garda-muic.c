/*
 * arch/arm/mach-exynos/board-garda-muic.c
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

extern struct muic_platform_data exynos4_muic_pdata;

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

#if defined(CONFIG_MUIC_FSA9480)
#if defined(GPIO_USB_SEL)
static int fsa9480_set_gpio_usb_sel(int uart_sel)
{
	return 0;
}
#endif /* GPIO_USB_SEL */

#if defined(GPIO_UART_SEL)
static int fsa9480_set_gpio_uart_sel(int uart_sel)
{
	const char *mode;

	switch (uart_sel) {
	case MUIC_PATH_UART_AP:
		mode = "AP_UART";
		if (gpio_is_valid(GPIO_UART_SEL))
			gpio_set_value(GPIO_UART_SEL, GPIO_LEVEL_HIGH);
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP_UART";
		if (gpio_is_valid(GPIO_UART_SEL))
			gpio_set_value(GPIO_UART_SEL, GPIO_LEVEL_LOW);
		break;
	default:
		mode = "Error";
		break;
	}

	pr_info("%s: uart_sel(%s)\n", __func__, mode);

	return 0;
}
#endif /* GPIO_UART_SEL */

struct muic_platform_data exynos4_muic_pdata = {
	.irq_gpio		= GPIO_MUIC_IRQ,

#if defined(GPIO_USB_SEL)
	.gpio_usb_sel		= GPIO_USB_SEL,
	.set_gpio_usb_sel	= fsa9480_set_gpio_usb_sel,
#endif /* GPIO_USB_SEL */

#if defined(GPIO_UART_SEL)
	.gpio_uart_sel		= GPIO_UART_SEL,
	.set_gpio_uart_sel	= fsa9480_set_gpio_uart_sel,
#endif /* GPIO_UART_SEL */

	.init_gpio_cb		= muic_init_gpio_cb,
};
#endif /* CONFIG_MUIC_FSA9480 */

#if defined(CONFIG_MUIC_TSU6721)
#if defined(GPIO_USB_SEL)
static int tsu6721_set_gpio_usb_sel(int uart_sel)
{
	return 0;
}
#endif /* GPIO_USB_SEL */

#if defined(GPIO_UART_SEL)
static int tsu6721_set_gpio_uart_sel(int uart_sel)
{
	const char *mode;
	int uart_sel_gpio = exynos4_muic_pdata.gpio_uart_sel;
	int uart_sel_val;
	int ret;

	ret = gpio_request(uart_sel_gpio, "GPIO_UART_SEL");
	if (ret) {
		pr_err("failed to gpio_request GPIO_UART_SEL\n");
		return ret;
	}

	uart_sel_val = gpio_get_value(uart_sel_gpio);

	pr_info("%s: uart_sel(%d), GPIO_UART_SEL(%d)=%c ->", __func__, uart_sel,
			uart_sel_gpio, (uart_sel_val == 0 ? 'L' : 'H'));

	switch (uart_sel) {
	case MUIC_PATH_UART_AP:
		mode = "AP_UART";
		if (gpio_is_valid(uart_sel_gpio))
			gpio_set_value(uart_sel_gpio, GPIO_LEVEL_HIGH);
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP_UART";
		if (gpio_is_valid(uart_sel_gpio))
			gpio_set_value(uart_sel_gpio, GPIO_LEVEL_LOW);
		break;
	default:
		mode = "Error";
		break;
	}

	uart_sel_val = gpio_get_value(uart_sel_gpio);

	gpio_free(uart_sel_gpio);

	pr_info(" %s, GPIO_UART_SEL(%d)=%c\n", mode, uart_sel_gpio,
			(uart_sel_val == 0 ? 'L' : 'H'));

	return 0;
}
#endif /* GPIO_UART_SEL */

#if defined(GPIO_DOC_SWITCH)
static int tsu6721_set_gpio_doc_switch(int val)
{
	int doc_switch_gpio = exynos4_muic_pdata.gpio_doc_switch;
	int doc_switch_val;
	int ret;

	ret = gpio_request(doc_switch_gpio, "GPIO_UART_SEL");
	if (ret) {
		pr_err("failed to gpio_request GPIO_UART_SEL\n");
		return ret;
	}

	doc_switch_val = gpio_get_value(doc_switch_gpio);

	pr_info("%s: doc_switch(%d) --> (%d)", __func__, doc_switch_val, val);

	if (gpio_is_valid(doc_switch_gpio))
			gpio_set_value(doc_switch_gpio, val);

	doc_switch_val = gpio_get_value(doc_switch_gpio);

	gpio_free(doc_switch_gpio);

	pr_info("%s: GPIO_DOC_SWITCH(%d)=%c\n",__func__, doc_switch_gpio,
			(doc_switch_val == 0 ? 'L' : 'H'));

	return 0;
}
#endif /* GPIO_DOC_SWITCH */


struct muic_platform_data exynos4_muic_pdata = {
	.irq_gpio		= GPIO_MUIC_IRQ,

#if defined(GPIO_USB_SEL)
	.gpio_usb_sel		= GPIO_USB_SEL,
	.set_gpio_usb_sel	= tsu6721_set_gpio_usb_sel,
#endif /* GPIO_USB_SEL */

#if defined(GPIO_UART_SEL)
	.gpio_uart_sel		= GPIO_UART_SEL,
	.set_gpio_uart_sel	= tsu6721_set_gpio_uart_sel,
#endif /* GPIO_UART_SEL */

#if defined(GPIO_DOC_SWITCH)
	.gpio_doc_switch = GPIO_DOC_SWITCH,
	.set_gpio_doc_switch = tsu6721_set_gpio_doc_switch,
#endif /* GPIO_DOC_SWITCH*/

	.init_gpio_cb		= muic_init_gpio_cb,
};
#endif /* CONFIG_MUIC_TSU6721 */

#if defined(CONFIG_MUIC_I2C_USE_S3C_DEV_I2C6)
/* I2C6 */
static struct i2c_board_info i2c_devs6_hw[] __initdata = {
#if defined(CONFIG_MUIC_TSU6721)
	{
		I2C_BOARD_INFO(MUIC_DEV_NAME, (0x4A >> 1)),
		.platform_data	= &exynos4_muic_pdata,
	}
#endif /* CONFIG_MUIC_TSU6721 */
};
#endif /* CONFIG_MUIC_I2C_USE_S3C_DEV_I2C6 */

#if defined(CONFIG_MUIC_I2C_USE_I2C17_EMUL)
/* I2C17 */
static struct i2c_board_info i2c_devs17_emul[] __initdata = {
#if defined(CONFIG_MUIC_TSU6721)
	{
		I2C_BOARD_INFO(MUIC_DEV_NAME, (0x4A >> 1)),
		.platform_data	= &exynos4_muic_pdata,
	}
#endif /* CONFIG_MUIC_TSU6721 */
};

static struct i2c_gpio_platform_data gpio_i2c_data17 = {
#if defined(CONFIG_MUIC_TSU6721)
	.sda_pin = GPIO_MUIC_SDA,
	.scl_pin = GPIO_MUIC_SCL,
#endif /* CONFIG_MUIC_TSU6721 */
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
