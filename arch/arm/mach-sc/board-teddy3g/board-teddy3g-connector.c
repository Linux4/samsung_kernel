/* Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * Based on mach-sc/board-huskytd/board-teddy3g-connector.c
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


#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <mach/board.h>
#include <linux/mfd/rt5033.h>
#include <linux/mfd/rt8973.h>

static struct resource sprd_teddy3g_otg_resource[] = {
	[0] = {
		.start = SPRD_USB_BASE,
		.end = SPRD_USB_BASE + SPRD_USB_SIZE - 1,
		.flags = IORESOURCE_MEM,
	};
	[1] = {
		.start = IRQ_USBD_INT,
		.end = IRQ_USBD_INT,
		.flags = IORESOURCE_IRQ,
	};
};

struct platform_device sprd_teddy3g_otg_device = {
	.name    = "dwc_otg",
	.id	 = 0,
	.num_resources = ARRAY_SIZE(sprd_teddy3g_otg_resource),
	.resource = sprd_teddy3g_otg_resource,
};
	

static struct i2c_gpio_platform_data rt8973_i2cadaptor_data = {
	.sda_pin = GPIO_MUIC_SDA,
	.scl_pin = GPIO_MUIC_SCL,
	.udelay  = 10,
	.timeout = 0,
};

static struct i2c_gpio_platform_data rt5033_i2c_data = {
	.sda_pin = GPIO_IF_PMIC_SDA,
	.scl_pin = GPIO_IF_PMIC_SCL,
	.udelay  = 3,
	.timeout = 100,
};



static struct platform_device rt5033_mfd_device = {
	.name   = "i2c-gpio",
	.id     = 8,
	.dev	= {
		.platform_data = &rt5033_i2c_data,
	}
};

static struct platform_device rt8973_mfd_device_i2cadaptor = {
	.name   = "i2c-gpio",
	.id     = 7,
	.dev	= {
		.platform_data = &rt8973_i2cadaptor_data,
	}
};
static struct rt8973_platform_data rt8973_pdata = {
	.irq_gpio = GPIO_MUIC_IRQ,
	.cable_chg_callback = NULL,
	.usb_callback = NULL,
	.ocp_callback = NULL,
	.otp_callback = NULL,
	.ovp_callback = NULL,
	.usb_callback = NULL,
	.uart_callback = NULL,
	.otg_callback = NULL,
	.jig_callback = NULL,
};

static struct i2c_board_info rtmuic_i2c_boardinfo[] __initdata = {
	{
	I2C_BOARD_INFO("rt8973", 0x28 >> 1),
	.platform_data = &rt8973_pdata,
	},
};


void sp883x_connector_init(void)
{
	i2c_register_board_info(7, rtmuic_i2c_boardinfo,
				ARRAY_SIZE(rtmuic_i2c_boardinfo));

	platform_device_register(&rt5033_mfd_device);
	platform_device_register(&rt8973_mfd_device_i2cadaptor);
	platform_device_register(&sprd_teddy3g_otg_device);
}
