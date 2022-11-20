/*
 * muic_dt.c
 *
 * Copyright (C) 2014 Samsung Electronics
 * Thomas Ryu <smilesr.ryu@samsung.com>
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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/host_notify.h>

#include <linux/muic/muic.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#if defined(CONFIG_MUIC_UART_SWITCH)
#include <mach/pinctrl-samsung.h>
#endif

#include "muic-internal.h"
#include "muic_dt.h"
#include "muic_vps.h"

extern struct muic_platform_data muic_pdata;

#if defined(GPIO_UART_SEL)
static int muic_gpio_uart_sel;
static bool muic_gpio_uart_ap;
#endif

#if defined(CONFIG_OF)
struct of_device_id muic_i2c_dt_ids[] = {
	{ .compatible = "muic-universal" },
	{ },
};
#endif

/* The supported APS list from dts format as follows.
    + : MUIC SHOULD detect the device type.
         It's not MUIC driver's concern whether it is supported or not.
    - : MUIC SHOULD NOT detect the device type.

"+OTG:GND",
"+MHL:1K",
"+VZW Accessory:28.7K",
"+VZW Incompatible:34K",
"-Smartdock:40.2K",
"-HMT:49.9K",
"-Audiodock:64.9K",
"+USB LANHUB:80.07K",
"+Charging Cable:102K",
"+Universal Multimedia dock:121K",
"+Jig USB Off:255K",
"+Jig USB On:301K",
"+Deskdock:365K",
"+TYPE2 Charger:442K",
"+Jig UART Off:523K",
"+Jig UART On:619K"
"+TA:OPEN",
"+USB:OPEN",
"+CDP:OPEN",
"+Undefined Charging",
*/


#if defined(CONFIG_OF)
int of_update_supported_list(struct i2c_client *i2c,
				struct muic_platform_data *pdata)
{
	struct device_node *np_muic;
	const char *prop, *prop_end;
	char prop_buf[64];
	int i, prop_num;
	int ret = 0;
	int mdev = 0;

	pr_info("%s\n", __func__);

	np_muic = of_find_node_by_path("/muic");
	if (np_muic == NULL)
		return -EINVAL;

	prop_num = of_property_count_strings(np_muic, "muic,support-list");
	if (prop_num < 0) {
		pr_warn("%s:%s No 'support list dt node'[%d]\n",
				MUIC_DEV_NAME, __func__, prop_num);
		ret = prop_num;
		goto err;
	}

	memset(prop_buf, 0x00, sizeof(prop_buf));

	for (i = 0; i < prop_num; i++) {
		ret = of_property_read_string_index(np_muic, "muic,support-list", i,
							&prop);
		if (ret) {
			pr_err("%s:%s Cannot find string at [%d], ret[%d]\n",
					MUIC_DEV_NAME, __func__, i, ret);
			break;
		}

		prop_end = strstr(prop, ":");
		if (!prop_end) {
			pr_err("%s: Wrong prop format. %s\n", __func__, prop);
			break;
		}

		memcpy(prop_buf, prop, prop_end - prop);
		prop_buf[prop_end - prop] = 0x00;

		if (prop_buf[0] == '+') {
			if (vps_name_to_mdev(&prop_buf[1], &mdev))
				vps_update_supported_attr(mdev, true);
		} else if (prop_buf[0] == '-') {
			if (vps_name_to_mdev(&prop_buf[1], &mdev))
#if defined(CONFIG_MUIC_SUPPORT_VZW_ACC)
			{
				if(mdev == ATTACHED_DEV_VZW_INCOMPATIBLE_MUIC || mdev == ATTACHED_DEV_VZW_ACC_MUIC)
					vps_update_supported_attr(mdev, true);
				else
					vps_update_supported_attr(mdev, false);
			}
#else
				vps_update_supported_attr(mdev, false);
#endif

		} else {
			pr_err("%s: %c Undefined prop attribute.\n", __func__, prop_buf[0]);
		}
	}

	prop_num = of_property_count_strings(np_muic, "muic,support-list-variant");
	if (prop_num < 0) {
		pr_warn("%s:%s No support list-variant dt node'[%d]\n",
				MUIC_DEV_NAME, __func__, prop_num);
		ret = prop_num;
		goto err;
	}

	memset(prop_buf, 0x00, sizeof(prop_buf));

	for (i = 0; i < prop_num; i++) {
		ret = of_property_read_string_index(np_muic, "muic,support-list-variant", i,
							&prop);
		if (ret) {
			pr_err("%s:%s Cannot find variant-string at [%d], ret[%d]\n",
					MUIC_DEV_NAME, __func__, i, ret);
			break;
		}

		prop_end = strstr(prop, ":");
		if (!prop_end) {
			pr_err("%s: Wrong prop format. %s\n", __func__, prop);
			break;
		}

		memcpy(prop_buf, prop, prop_end - prop);
		prop_buf[prop_end - prop] = 0x00;

		if (prop_buf[0] == '+') {
			if (vps_name_to_mdev(&prop_buf[1], &mdev))
				vps_update_supported_attr(mdev, true);
		} else if (prop_buf[0] == '-') {
			if (vps_name_to_mdev(&prop_buf[1], &mdev))
				vps_update_supported_attr(mdev, false);
		} else {
			pr_err("%s: %c Undefined prop attribute.\n", __func__, prop_buf[0]);
		}
	}


err:
	of_node_put(np_muic);

	return ret;
}


int of_muic_dt(struct i2c_client *i2c, struct muic_platform_data *pdata)
{
	struct device_node *np_muic = i2c->dev.of_node;
	muic_data_t *pmuic = i2c_get_clientdata(i2c);
	int ret=0;

	if(!np_muic)
		return -EINVAL;

	ret = of_property_read_string(np_muic,
		"muic-universal,chip_name", (char const **)&pmuic->chip_name);
	if (ret)
		pr_info("%s: Vendor is Empty\n", __func__);
	else
		pr_info("%s: chip_name is %s\n", __func__, pmuic->chip_name);


	pdata->irq_gpio = of_get_named_gpio(np_muic, "muic-universal,irq-gpio", 0);
	pr_info("%s: irq-gpio: %u\n", __func__, pdata->irq_gpio);

#if defined(GPIO_UART_SEL)
	muic_gpio_uart_sel = of_get_named_gpio(np_muic, "muic-universal,uart-gpio", 0);
	ret = gpio_is_valid(muic_gpio_uart_sel);
	if (!ret) {
		pr_err("GPIO_UART_SEL is not valid!!!\n");
		pmuic->gpio_uart_sel = muic_gpio_uart_sel = 0;
		return ret;
	} else {
		ret = gpio_request(muic_gpio_uart_sel, "GPIO_UART_SEL");
		if (ret) {
			pr_err("failed to gpio_request GPIO_UART_SEL\n");
			return ret;
		}

		muic_gpio_uart_ap = of_property_read_bool(np_muic, "muic-universal,uart-ap");

		pr_info("%s: uart-gpio : %d\n", __func__, muic_gpio_uart_sel);
		pr_info("%s: uart-ap : %d\n", __func__, muic_gpio_uart_ap);
	}

	pmuic->gpio_uart_sel = muic_gpio_uart_sel;
#endif

#if defined(CONFIG_MUIC_UART_SWITCH)
	pdata->uart_addr =
		(const char *)of_get_property(np_muic, "muic-universal,uart_addr", NULL);
	pdata->uart_txd =
		(const char *)of_get_property(np_muic, "muic-universal,uart_txd", NULL);
	pdata->uart_rxd =
		(const char *)of_get_property(np_muic, "muic-universal,uart_rxd", NULL);

	pr_info("%s: uart_addr : %s\n", __func__, pdata->uart_addr);
	pr_info("%s: uart_txd : %s\n", __func__, pdata->uart_txd);
	pr_info("%s: uart_rxd : %s\n", __func__, pdata->uart_rxd);
#endif

	return 0;
}
#endif

int muic_set_gpio_uart_sel(int uart_sel)
{
	const char *mode;
#if !defined(CONFIG_MUIC_UART_SWITCH)
	int uart_sel_gpio = muic_gpio_uart_sel;

	if (!uart_sel_gpio) {
		pr_err("%s: No UART gpio defined.\n", __func__);
		return 0;
	}

	switch (uart_sel) {
	case MUIC_PATH_UART_AP:
		mode = "AP_UART";
		if (gpio_is_valid(uart_sel_gpio))
			gpio_direction_output(uart_sel_gpio, muic_gpio_uart_ap);
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP_UART";
		if (gpio_is_valid(uart_sel_gpio))
			gpio_direction_output(uart_sel_gpio, !muic_gpio_uart_ap);
		break;
	default:
		mode = "Error";
		break;
	}

	pr_info("%s: uart_sel(%d), GPIO_UART_SEL(%d)=%s", __func__, uart_sel,
			uart_sel_gpio, mode);
#else
	switch (uart_sel) {
	case MUIC_PATH_UART_AP:
		mode = "AP_UART";
		pin_config_set(muic_pdata.uart_addr, muic_pdata.uart_rxd,
					PINCFG_PACK(PINCFG_TYPE_FUNC, 0x2));
		pin_config_set(muic_pdata.uart_addr, muic_pdata.uart_txd,
					PINCFG_PACK(PINCFG_TYPE_FUNC, 0x2));
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP_UART";
		pin_config_set(muic_pdata.uart_addr, muic_pdata.uart_rxd,
					PINCFG_PACK(PINCFG_TYPE_FUNC, 0x3));
		pin_config_set(muic_pdata.uart_addr, muic_pdata.uart_txd,
					PINCFG_PACK(PINCFG_TYPE_FUNC, 0x3));
		break;
	default:
		mode = "Error";
		break;
	}

	printk(KERN_DEBUG "[muic] %s %s\n", __func__, mode);
#endif

	return 0;
}

