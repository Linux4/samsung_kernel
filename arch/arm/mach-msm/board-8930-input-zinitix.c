/*
 * Copyright (C) 2011 Google, Inc.
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
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio_event.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <asm/mach-types.h>
#include <linux/regulator/consumer.h>
#include <mach/msm8930-gpio.h>
#include <linux/delay.h>
#include "board-8930.h"
#include "devices-msm8x60.h"
#include <asm/mach-types.h>

#include <linux/platform_data/zinitix_bt_532.h>

#define MSM_8960_GSBI3_QUP_I2C_BUS_ID	3

#if defined(CONFIG_MACH_JASPER2)
static u8 touchkey_keycode[] = {KEY_F3, KEY_BACK, KEY_HOMEPAGE, KEY_MENU};
#endif

void zinitix_register_callback(struct tsp_callbacks *cb)
{
	charger_callbacks = cb;
	pr_err("[TSP] %s\n", __func__);
}

static void zinitix_request_gpio(void)
{
	int ret;
	pr_err("[TSP] request gpio\n");

	ret = gpio_request(GPIO_TOUCH_IRQ, "zinitix_tsp_irq");
	if (ret) {
		pr_err("[TSP]%s: unable to request zinitix_tsp_irq [%d]\n",
				__func__, GPIO_TOUCH_IRQ);
		return;
	}
}

extern unsigned int system_rev;
void zinitix_vdd_on(bool onoff)
{
	int ret = 0;
	static struct regulator *reg_l31;
	static struct regulator *reg_lvs6;
	int volt_i;
	char* volt_c;

	if (system_rev >= 4) {
		volt_i = 3000000;
		volt_c = "3.0V";
	} else {
		volt_i = 3300000;
		volt_c = "3.3V";
	}

	pr_err("%s: power %s\n", __func__, onoff ? "on" : "off");

	if (!reg_lvs6) {
		reg_lvs6 = regulator_get(NULL, "8917_lvs6");
		if (IS_ERR(reg_lvs6)) {
			pr_err("%s: could not get 8917_lvs6, rc = %ld\n",
				__func__, PTR_ERR(reg_lvs6));
			return;
		}
	}

	if (onoff) {
		ret = regulator_enable(reg_lvs6);
		if (ret) {
			pr_err("%s: enable lvs6 failed, rc=%d\n",
				__func__, ret);
			return;
		}
		pr_err("%s: tsp 1.8V on is finished.\n", __func__);
	} else {
		if (regulator_is_enabled(reg_lvs6))
			ret = regulator_disable(reg_lvs6);
		else
			pr_err("%s: rugulator LVS6(1.8V) is disabled\n",
					__func__);

		if (ret) {
			pr_err("%s: enable lvs6 failed, rc=%d\n",
				__func__, ret);
			return;
		}
		pr_err("%s: tsp 1.8V off is finished.\n", __func__);
	}

	if (!reg_l31) {
		reg_l31 = regulator_get(NULL, "8917_l31");
		if (IS_ERR(reg_l31)) {
			pr_err("%s: could not get 8917_l31, rc = %ld\n",
				__func__, PTR_ERR(reg_l31));
			return;
		}
		ret = regulator_set_voltage(reg_l31, volt_i, volt_i);
		if (ret) {
			pr_err("%s: unable to set l31 voltage to %s\n",
				__func__, volt_c);
			return;
		}
	}

	if (onoff) {
		ret = regulator_enable(reg_l31);
		if (ret) {
			pr_err("%s: enable l31 failed, rc=%d\n",
				__func__, ret);
			return;
		}
		pr_err("%s: tsp %s on is finished.\n", __func__, volt_c);
	} else {
		if (regulator_is_enabled(reg_l31))
			ret = regulator_disable(reg_l31);
		else
			pr_err("%s: rugulator is(L31(%s) disabled\n",
					__func__, volt_c);
		if (ret) {
			pr_err("%s: disable l31 failed, rc=%d\n",
				__func__, ret);
			return;
		}
		pr_err("%s: tsp %s off is finished.\n", __func__, volt_c);
	}

	msleep(30);

	return;
}

int is_zinitix_vdd_on(void)
{
	int ret = -1;
	static struct regulator *reg_l31;
	int volt_i;
	char* volt_c;

	if (system_rev >= 4) {
		volt_i = 3000000;
		volt_c = "3.0V";
	} else {
		volt_i = 3300000;
		volt_c = "3.3V";
	}

	if (!reg_l31) {
		reg_l31 = regulator_get(NULL, "8917_l31");
		if (IS_ERR(reg_l31)) {
			pr_err("%s: could not get 8917_l31, rc = %ld\n",
				__func__, PTR_ERR(reg_l31));
			return ret;
		}
		ret = regulator_set_voltage(reg_l31, volt_i, volt_i);
		if (ret) {
			pr_err("%s: unable to set l31 voltage to %s\n",
				__func__, volt_c);
			return ret;
		}
	}
	ret = regulator_is_enabled(reg_l31);

	pr_info("[TSP] %s = %d\n", __func__, ret);

	return ret ? 1 : 0;
}

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT432)
#define TSP_IC_NAME	"BT432"
#elif defined(CONFIG_TOUCHSCREEN_ZINITIX_BT532)
#define TSP_IC_NAME	"BT532"
#else
#define TSP_IC_NAME	""
#endif
#define USE_TOUCHKEY	1

static struct bt532_ts_platform_data bt532_ts_pdata = {
	.gpio_int	= GPIO_TOUCH_IRQ,
	.x_resolution	= 599,
	.y_resolution	= 1023,
	.page_size	= 128,
	.vdd_on		= zinitix_vdd_on,
	.is_vdd_on	= is_zinitix_vdd_on,
	.register_cb	= zinitix_register_callback,
	.orientation	= 0,
};

static struct i2c_board_info zinitix_info[] __initdata = {
	{
		I2C_BOARD_INFO(ZINITIX_NAME, 0x40 >> 1),
		.flags = I2C_CLIENT_WAKE,
		.platform_data = &bt532_ts_pdata,
		.irq = MSM_GPIO_TO_INT(GPIO_TOUCH_IRQ),
	},
};

void __init input_touchscreen_init(void)
{
	zinitix_request_gpio();

	gpio_tlmm_config(GPIO_CFG(GPIO_TOUCH_IRQ, 0,
		GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
#if defined(CONFIG_MACH_LT02)
	pr_err("[TSP] Configuring Vendor GPIO\n");

	gpio_tlmm_config(GPIO_CFG(GPIO_TSP_VENDOR1, 0,
		GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(GPIO_TSP_VENDOR2, 0,
                GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1);
#endif

	i2c_register_board_info(MSM_8960_GSBI3_QUP_I2C_BUS_ID,
				zinitix_info, ARRAY_SIZE(zinitix_info));
}
