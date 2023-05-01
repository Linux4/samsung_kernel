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

#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
#include <linux/i2c/cypress_touchkey_234.h>
#endif

#ifdef CONFIG_KEYBOARD_TC360_TOUCHKEY
#include <linux/input/tc360-touchkey.h>
#endif
#define MSM_TOUCHKEY_I2C_BUS_ID		16

void tkey_led_power(bool onoff)
{
	static struct regulator *reg_l36;
	int ret;

	pr_info("%s: %s\n", __func__, onoff ? "on" : "off");

	reg_l36 = regulator_get(NULL, "8917_l36");

	if (IS_ERR(reg_l36))
		pr_err("%s: could not get 8917_l36, rc = %ld\n",
			__func__, PTR_ERR(reg_l36));

	ret = regulator_set_voltage(reg_l36, 3300000, 3300000);

	if (ret) {
		pr_err("%s: unable to set l36 voltage to 3.3V\n",
		__func__);
	}

	if (onoff) {
		if (!regulator_is_enabled(reg_l36)) {
			ret = regulator_enable(reg_l36);
			if (ret) {
				pr_err("%s: enable l36 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%sTKEY_LED 3.3V[l36] on is finished.\n",__func__);			
		} else
			pr_info("%sTKEY_LED 3.3V[l36] is already on.\n",__func__);
	} else {
		if (regulator_is_enabled(reg_l36)) {
			ret = regulator_disable(reg_l36);
			if (ret) {
				pr_err("%s: disable l36 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%sTKEY_LED 3.3V[l36] off is finished.\n",__func__);
		} else
			pr_info("%sTKEY_LED 3.3V[l36] is already off.\n",__func__);	
	}
}

static void tkey_int_request_gpio(void)
{
	int ret;

	pr_info("%s\n",__func__);
	ret = gpio_request(GPIO_TKEY_INT, "gpio_tkey_int");

	if (ret) {
		pr_err("%s: err request Tkey_int: %d\n",
				__func__, ret);
	}
}

#ifdef CONFIG_KEYBOARD_TC360_TOUCHKEY
void tc360_power(int onoff)
{
	static struct regulator *reg_l30;

	int ret = 0;
	
	pr_info("%s: power %s\n", __func__, onoff ? "on" : "off");
	
	if (!reg_l30) {
		reg_l30 = regulator_get(NULL, "8917_l30");
		if (IS_ERR(reg_l30)) {
			pr_err("%s: could not get 8917_l30, rc = %ld\n",
				__func__, PTR_ERR(reg_l30));
			return;
		}
		ret = regulator_set_voltage(reg_l30, 2200000, 2200000);
		if (ret) {
			pr_err("%s: unable to set l30 voltage to 2.2V\n",
				__func__);
			return;
		}
	}

	if (onoff) {
		if (!regulator_is_enabled(reg_l30)) {
			ret = regulator_enable(reg_l30);
			if (ret) {
				pr_err("%s: enable l30 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tkey 2.2V[l30] on is finished.\n", __func__);
		} else
			pr_info("%s: tkey 2.2V[l30] is already on.\n", __func__);
	} else {
		if (regulator_is_enabled(reg_l30)) {
			ret = regulator_disable(reg_l30);
			if (ret) {
				pr_err("%s: disable l30 failed, rc=%d\n",
				__func__, ret);
				return;
			}
			pr_info("%s: tkey 2.2V[l30] off is finished.\n", __func__);
		} else
			pr_info("%s: tkey 2.2V[l30] is already off.\n", __func__);
	}
}
#endif
#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
void cypress_power_onoff(int onoff)
{
	static struct regulator *reg_l30;

	int ret = 0;
	
	pr_info("%s: power %s\n", __func__, onoff ? "on" : "off");
	
	if (!reg_l30) {
		reg_l30 = regulator_get(NULL, "8917_l30");
		if (IS_ERR(reg_l30)) {
			pr_err("%s: could not get 8917_l30, rc = %ld\n",
				__func__, PTR_ERR(reg_l30));
			return;
		}
		ret = regulator_set_voltage(reg_l30, 1800000, 1800000);
		if (ret) {
			pr_err("%s: unable to set l30 voltage to 2.2V\n",
				__func__);
			return;
		}
	}

	if (onoff) {
		if (!regulator_is_enabled(reg_l30)) {
			ret = regulator_enable(reg_l30);
			if (ret) {
				pr_err("%s: enable l30 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tkey 2.2V[l30] on is finished.\n", __func__);
		} else
			pr_info("%s: tkey 2.2V[l30] is already on.\n", __func__);
	} else {
		if (regulator_is_enabled(reg_l30)) {
			ret = regulator_disable(reg_l30);
			if (ret) {
				pr_err("%s: disable l30 failed, rc=%d\n",
				__func__, ret);
				return;
			}
			pr_info("%s: tkey 2.2V[l30] off is finished.\n", __func__);
		} else
			pr_info("%s: tkey 2.2V[l30] is already off.\n", __func__);
	}

}
#endif

#ifdef CONFIG_KEYBOARD_TC360_TOUCHKEY
static int touchkey_keycodes[] = {KEY_MENU, KEY_BACK};

static struct tc360_platform_data tc360_pdata = {
	.gpio_scl = GPIO_TOUCHKEY_I2C_SCL,
	.gpio_sda = GPIO_TOUCHKEY_I2C_SDA,
	.gpio_int = GPIO_TKEY_INT,
	.udelay = 6,
	.num_key = ARRAY_SIZE(touchkey_keycodes),
	.keycodes = touchkey_keycodes,
	.suspend_type = TC360_SUSPEND_WITH_POWER_OFF,
	.power = tc360_power,
	.led_power = tkey_led_power,
	.enable = 1,
};

static struct i2c_board_info touchkey_i2c_devices_info[] __initdata = {
	{
		I2C_BOARD_INFO(TC360_DEVICE, 0x20),
		.platform_data = &tc360_pdata,
		.irq			= MSM_GPIO_TO_INT(GPIO_TKEY_INT),
	},
};
#endif
#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
static const u8 tkey_keycodes[] = {KEY_MENU, KEY_BACK};

static struct cypress_touchkey_platform_data cypress_touchkey_pdata = {
	.gpio_int = GPIO_TKEY_INT,
	.touchkey_led_en = tkey_led_power,
	.touchkey_keycode = tkey_keycodes,
	.power_onoff = cypress_power_onoff,
};

static struct i2c_board_info cypress_touchkey_i2c_devices_info[] __initdata = {
	{
		I2C_BOARD_INFO("cypress_touchkey", 0x20),
		.platform_data = &cypress_touchkey_pdata,
		.irq			= MSM_GPIO_TO_INT(GPIO_TKEY_INT),
	},
};
#endif
static struct i2c_gpio_platform_data  touchkey_i2c_gpio_data = {
	.udelay			= 2,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
	.sda_pin			= GPIO_TOUCHKEY_I2C_SDA,
	.scl_pin			= GPIO_TOUCHKEY_I2C_SCL,
};

static struct platform_device touchkey_i2c_gpio_device = {
	.name			= "i2c-gpio",
	.id			= MSM_TOUCHKEY_I2C_BUS_ID,
	.dev.platform_data	= &touchkey_i2c_gpio_data,
};

static struct platform_device *input_touchkey_devices[] __initdata = {
	&touchkey_i2c_gpio_device,
};

void __init input_touchkey_init(void)
{
	tkey_int_request_gpio();

	gpio_tlmm_config(GPIO_CFG(GPIO_TOUCHKEY_I2C_SDA,
			0, GPIO_CFG_INPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(GPIO_TOUCHKEY_I2C_SCL,
			0, GPIO_CFG_INPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(GPIO_TKEY_INT, 0,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
#if defined(CONFIG_MACH_SERRANO_ATT) ||defined(CONFIG_MACH_SERRANO_VZW) \
	||defined(CONFIG_MACH_SERRANO_SPR) ||defined(CONFIG_MACH_SERRANO_LRA)
		if (system_rev < BOARD_REV07)
	#elif defined(CONFIG_MACH_SERRANO_USC)
		if (system_rev < BOARD_REV05)
	#else
		if (system_rev >= BOARD_REV00)
	#endif
		{
			tkey_led_power(1);
			tc360_power(1);
			i2c_register_board_info(MSM_TOUCHKEY_I2C_BUS_ID,
						touchkey_i2c_devices_info,
						ARRAY_SIZE(touchkey_i2c_devices_info));
		} else {
			cypress_power_onoff(1);
			i2c_register_board_info(MSM_TOUCHKEY_I2C_BUS_ID,
						cypress_touchkey_i2c_devices_info,
						ARRAY_SIZE(cypress_touchkey_i2c_devices_info));
		}
#else
	tkey_led_power(1);
	tc360_power(1);
	i2c_register_board_info(MSM_TOUCHKEY_I2C_BUS_ID,
				touchkey_i2c_devices_info,
				ARRAY_SIZE(touchkey_i2c_devices_info));
	
#endif
	platform_add_devices(input_touchkey_devices,
				ARRAY_SIZE(input_touchkey_devices));	
}

