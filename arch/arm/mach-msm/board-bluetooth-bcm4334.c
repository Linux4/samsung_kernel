/*
 * Bluetooth Broadcom GPIO and Low Power Mode control
 *
 *  Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *  Copyright (C) 2011 Google, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/wakelock.h>

#include <asm/mach-types.h>

#include <mach/msm8960-gpio.h>
#include "board-8960.h"
#include "devices.h"

#define BT_UART_CFG
#define BT_LPM_ENABLE

static struct rfkill *bt_rfkill;

#ifdef BT_UART_CFG
static unsigned bt_uart_on_table[] = {
	GPIO_CFG(GPIO_BT_UART_RTS, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
		 GPIO_CFG_2MA),
	GPIO_CFG(GPIO_BT_UART_CTS, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL,
		 GPIO_CFG_2MA),
	GPIO_CFG(GPIO_BT_UART_RXD, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL,
		 GPIO_CFG_2MA),
	GPIO_CFG(GPIO_BT_UART_TXD, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
		 GPIO_CFG_2MA),
};

static unsigned bt_uart_off_table[] = {
	GPIO_CFG(GPIO_BT_UART_RTS, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP,
		 GPIO_CFG_2MA),
	GPIO_CFG(GPIO_BT_UART_CTS, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP,
		 GPIO_CFG_2MA),
	GPIO_CFG(GPIO_BT_UART_RXD, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP,
		 GPIO_CFG_2MA),
	GPIO_CFG(GPIO_BT_UART_TXD, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP,
		 GPIO_CFG_2MA),
};
#endif

static int bcm4334_bt_rfkill_set_power(void *data, bool blocked)
{
	/* rfkill_ops callback. Turn transmitter on when blocked is false */
	int pin, rc = 0;

	if (!blocked) {
		pr_info("[BT] Bluetooth Power On.\n");
#ifdef BT_UART_CFG
	for (pin = 0; pin < ARRAY_SIZE(bt_uart_on_table); pin++) {
			rc = gpio_tlmm_config(bt_uart_on_table[pin],
					      GPIO_CFG_ENABLE);
		if (rc < 0)
				pr_err("%s: gpio_tlmm_config(%#x)=%d\n",
				       __func__, bt_uart_on_table[pin], rc);
	}
#endif
		gpio_set_value(gpio_rev(BT_EN), 1);
		msleep(50);
	} else {
#ifdef BT_UART_CFG
	for (pin = 0; pin < ARRAY_SIZE(bt_uart_off_table); pin++) {
			rc = gpio_tlmm_config(bt_uart_off_table[pin],
					      GPIO_CFG_ENABLE);
		if (rc < 0)
				pr_err("%s: gpio_tlmm_config(%#x)=%d\n",
				       __func__, bt_uart_off_table[pin], rc);
	}
#endif
		pr_info("[BT] Bluetooth Power Off.\n");
		gpio_set_value(gpio_rev(BT_EN), 0);
	}
	return 0;
}

static const struct rfkill_ops bcm4334_bt_rfkill_ops = {
	.set_block = bcm4334_bt_rfkill_set_power,
};

static int bcm4334_bluetooth_probe(struct platform_device *pdev)
{
	int rc = 0;

	rc = gpio_request(gpio_rev(BT_EN), "bcm4334_bten_gpio");
	if (unlikely(rc)) {
		pr_err("[BT] GPIO_BT_EN request failed.\n");
		return rc;
	}

	gpio_tlmm_config(GPIO_CFG(GPIO_BT_UART_RTS, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(GPIO_BT_UART_CTS, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(GPIO_BT_UART_RXD, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(GPIO_BT_UART_TXD, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	gpio_tlmm_config(GPIO_CFG(gpio_rev(BT_EN), 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA), GPIO_CFG_ENABLE);

	gpio_direction_output(gpio_rev(BT_EN), 0);

	bt_rfkill = rfkill_alloc("bcm4334 Bluetooth", &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &bcm4334_bt_rfkill_ops,
				NULL);

	if (unlikely(!bt_rfkill)) {
		pr_err("[BT] bt_rfkill alloc failed.\n");
		gpio_free(gpio_rev(BT_EN));
		return -ENOMEM;
	}

	rfkill_init_sw_state(bt_rfkill, 0);

	rc = rfkill_register(bt_rfkill);

	if (unlikely(rc)) {
		pr_err("[BT] bt_rfkill register failed.\n");
		rfkill_destroy(bt_rfkill);
		gpio_free(gpio_rev(BT_EN));
		return -1;
	}

	rfkill_set_sw_state(bt_rfkill, true);

	return rc;
}

static int bcm4334_bluetooth_remove(struct platform_device *pdev)
{
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);

	gpio_free(gpio_rev(BT_EN));

	return 0;
}

static struct platform_driver bcm4334_bluetooth_platform_driver = {
	.probe = bcm4334_bluetooth_probe,
	.remove = bcm4334_bluetooth_remove,
	.driver = {
		   .name = "bcm4334_bluetooth",
		   .owner = THIS_MODULE,
		   },
};

#ifdef BT_LPM_ENABLE
static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= -1,
		.end	= -1,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= -1,
		.end	= -1,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
		.start	= -1,
		.end	= -1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm_bluesleep_device = {
	.name = "bluesleep_bcm",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};

static void gpio_rev_init(void)
{
	bluesleep_resources[0].start = gpio_rev(BT_HOST_WAKE);
	bluesleep_resources[0].end = gpio_rev(BT_HOST_WAKE);
	bluesleep_resources[1].start = gpio_rev(BT_WAKE);
	bluesleep_resources[1].end = gpio_rev(BT_WAKE);
	bluesleep_resources[2].start = MSM_GPIO_TO_INT(gpio_rev(BT_HOST_WAKE));
	bluesleep_resources[2].end = MSM_GPIO_TO_INT(gpio_rev(BT_HOST_WAKE));
}
#endif

#ifdef BT_LPM_ENABLE
extern void bluesleep_setup_uart_port(struct platform_device *uart_dev);
#endif

static int __init bcm4334_bluetooth_init(void)
{
#ifdef BT_LPM_ENABLE
	gpio_rev_init();
	platform_device_register(&msm_bluesleep_device);
        bluesleep_setup_uart_port(&msm_device_uart_dm6);
#endif
	return platform_driver_register(&bcm4334_bluetooth_platform_driver);
}

static void __exit bcm4334_bluetooth_exit(void)
{
#ifdef BT_LPM_ENABLE
	platform_device_unregister(&msm_bluesleep_device);
#endif
	platform_driver_unregister(&bcm4334_bluetooth_platform_driver);
}

module_init(bcm4334_bluetooth_init);
module_exit(bcm4334_bluetooth_exit);

MODULE_ALIAS("platform:bcm4334");
MODULE_DESCRIPTION("bcm4334_bluetooth");
MODULE_LICENSE("GPL");
