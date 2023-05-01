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
#include <linux/i2c/cypress_touchkey.h>
#define MSM_TOUCHKEY_I2C_BUS_ID		16
#endif

#ifdef CONFIG_KEYBOARD_TC360_TOUCHKEY
#include <linux/input/tc360-touchkey.h>
#define MSM_TOUCHKEY_I2C_BUS_ID		16
#endif
#ifdef CONFIG_KEYBOARD_MELFAS_TOUCHKEY
#define MSM_TOUCHKEY_I2C_BUS_ID		16
#endif

#ifdef CONFIG_LEDS_MSM_TKEY
static int tkey_led_gpio;

static struct platform_device msm_device_tkey_leds = {
	.name = "tkey-leds",
	.id = -1,
	.dev.platform_data = &tkey_led_gpio,
};

static struct platform_device *msm_device_gpio_tkey_leds[] __initdata = {
	&msm_device_tkey_leds,
};

void __init input_tkeyled_device_init(void)
{
	platform_add_devices(msm_device_gpio_tkey_leds,
				ARRAY_SIZE(msm_device_gpio_tkey_leds));
}
#endif

#ifdef CONFIG_KEYBOARD_MELFAS_TOUCHKEY

static struct i2c_board_info touchkey_i2c_devices_info[] __initdata = {
	{
		I2C_BOARD_INFO("melfas-touchkey", 0x20),
	},
};

static struct i2c_gpio_platform_data  melfas_touchkey_i2c_gpio_data = {
	.udelay			= 1,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
	.sda_pin			= GPIO_TOUCHKEY_I2C_SDA,
	.scl_pin			= GPIO_TOUCHKEY_I2C_SCL,
};

static struct platform_device touchkey_i2c_gpio_device = {
	.name			= "i2c-gpio",
	.id			= MSM_TOUCHKEY_I2C_BUS_ID,
	.dev.platform_data	= &melfas_touchkey_i2c_gpio_data,
};

static struct platform_device *input_touchkey_devices[] __initdata = {
	&touchkey_i2c_gpio_device,
};


void __init input_touchkey_init(void)
{
	int ret;
	printk(KERN_ERR "tkey %s: ln:%d\n", __func__, __LINE__);

		gpio_tlmm_config(GPIO_CFG(GPIO_TOUCHKEY_I2C_SDA,
				0, GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
		gpio_tlmm_config(GPIO_CFG(GPIO_TOUCHKEY_I2C_SCL,
				0, GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);

	ret = gpio_request(GPIO_TKEY_INT, "gpio_tkey_int");

	if (ret) {
		printk(KERN_ERR "%s: err request Tkey_int: %d\n",
				__func__, ret);
	}

	gpio_tlmm_config(GPIO_CFG(GPIO_TKEY_INT, 0,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	printk(KERN_ERR "tkey %s: ln:%d\n", __func__, __LINE__);

	i2c_register_board_info(MSM_TOUCHKEY_I2C_BUS_ID,
				touchkey_i2c_devices_info,
				ARRAY_SIZE(touchkey_i2c_devices_info));
	platform_add_devices(input_touchkey_devices,
				ARRAY_SIZE(input_touchkey_devices));
}
#endif


#ifdef CONFIG_KEYBOARD_TC360_TOUCHKEY

#if defined(CONFIG_MACH_GOLDEN)
void tc360_power(int onoff)
{
	static struct regulator *reg_l36;

	int ret = 0;
	
	pr_info("%s: power %s\n", __func__, onoff ? "on" : "off");

	if(!reg_l36){
		reg_l36 = regulator_get(NULL, "8917_l36");
		if (IS_ERR(reg_l36)) {
			pr_err("%s: could not get 8917_l36, rc = %ld\n",
				__func__, PTR_ERR(reg_l36));
			return;
		}
	
		ret = regulator_set_voltage(reg_l36, 2200000, 2200000);
		if (ret) {
			pr_err("%s: unable to set l30 voltage to 2.2V\n",
				__func__);
			return;
		}
	}

	if (onoff) {
		if (!regulator_is_enabled(reg_l36)) {
			ret = regulator_enable(reg_l36);
			if (ret) {
				pr_err("%s: enable l36 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tkey 2.2V[l36] on is finished.\n", __func__);
		} else
			pr_info("%s: tkey 2.2V[l36] is already on.\n", __func__);
	} else {
		if (regulator_is_enabled(reg_l36)) {
			ret = regulator_disable(reg_l36);
			if (ret) {
				pr_err("%s: disable l36 failed, rc=%d\n",
				__func__, ret);
				return;
			}
			pr_info("%s: tkey 2.2V[l36] off is finished.\n", __func__);
		} else
			pr_info("%s: tkey 2.2V[l36] is already off.\n", __func__);
	}
}

void tc360_led_power(bool onoff)
{
	static struct regulator *reg_l33;
	int ret;

	pr_info("%s: %s\n", __func__, onoff ? "on" : "off");

	if(!reg_l33){
		reg_l33 = regulator_get(NULL, "8917_l33");
		if (IS_ERR(reg_l33)) {
			pr_err("%s: could not get 8917_l33, rc = %ld\n",
				__func__, PTR_ERR(reg_l33));
			return;
		}

		ret = regulator_set_voltage(reg_l33, 3300000, 3300000);
		if (ret) {
			pr_err("%s: unable to set l36 voltage to 3.3V\n",
			__func__);
		}

	}

	if (onoff) {
		if (!regulator_is_enabled(reg_l33)) {
			ret = regulator_enable(reg_l33);
			if (ret) {
				pr_err("%s: enable l33 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%sTKEY_LED 3.3V[l33] on is finished.\n",__func__);			
		} else
			pr_info("%sTKEY_LED 3.3V[l33] is already on.\n",__func__);
	} else {
		if (regulator_is_enabled(reg_l33)) {
			ret = regulator_disable(reg_l33);
			if (ret) {
				pr_err("%s: disable l36 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%sTKEY_LED 3.3V[l33] off is finished.\n",__func__);
		} else
			pr_info("%sTKEY_LED 3.3V[l33] is already off.\n",__func__);	
	}
}

#else

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

void tc360_led_power(bool onoff)
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

#endif

static void tc360_request_gpio(void)
{
	int ret;

	pr_info("%s\n",__func__);
	ret = gpio_request(GPIO_TKEY_INT, "gpio_tkey_int");

	if (ret) {
		pr_err("%s: err request Tkey_int: %d\n",
				__func__, ret);
	}
}

static int tc360_keycodes[] = {KEY_MENU, KEY_BACK};

static struct tc360_platform_data tc360_pdata = {
	.gpio_scl = GPIO_TOUCHKEY_I2C_SCL,
	.gpio_sda = GPIO_TOUCHKEY_I2C_SDA,
	.gpio_int = GPIO_TKEY_INT,
	.udelay = 6,
	.num_key = ARRAY_SIZE(tc360_keycodes),
	.keycodes = tc360_keycodes,
	.suspend_type = TC360_SUSPEND_WITH_POWER_OFF,
	.power = tc360_power,
	.led_power = tc360_led_power,
};

static struct i2c_board_info touchkey_i2c_devices_info[] __initdata = {
	{
		I2C_BOARD_INFO(TC360_DEVICE, 0x20),
		.platform_data = &tc360_pdata,
		.irq			= MSM_GPIO_TO_INT(GPIO_TKEY_INT),
	},
};

static struct i2c_gpio_platform_data  tc360_touchkey_i2c_gpio_data = {
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
	.dev.platform_data	= &tc360_touchkey_i2c_gpio_data,
};

static struct platform_device *input_touchkey_devices[] __initdata = {
	&touchkey_i2c_gpio_device,
};

void __init input_touchkey_init(void)
{
	tc360_request_gpio();

	gpio_tlmm_config(GPIO_CFG(GPIO_TOUCHKEY_I2C_SDA,
			0, GPIO_CFG_INPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(GPIO_TOUCHKEY_I2C_SCL,
			0, GPIO_CFG_INPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);

	#if defined(CONFIG_MACH_GOLDEN)
	gpio_tlmm_config(GPIO_CFG(GPIO_TKEY_INT, 0,
			GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1);
	
	#else
	gpio_tlmm_config(GPIO_CFG(GPIO_TKEY_INT, 0,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	#endif
	tc360_pdata.enable = 1;

	tc360_led_power(1);
	tc360_power(1);

	i2c_register_board_info(MSM_TOUCHKEY_I2C_BUS_ID,
				touchkey_i2c_devices_info,
				ARRAY_SIZE(touchkey_i2c_devices_info));
	
	platform_add_devices(input_touchkey_devices,
				ARRAY_SIZE(input_touchkey_devices));
}
#endif


#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
void cypress_power_onoff(int onoff)
{
	printk(KERN_ERR "%s: power %s\n", __func__, onoff ? "on" : "off");

	#if defined(CONFIG_MACH_EXPRESS)
		if (system_rev > BOARD_REV02) {
	#else
		if (system_rev >= BOARD_REV00) {
	#endif
	static struct regulator *reg_l30;
	static struct regulator *reg_lvs5;
	int ret = 0;
		if (!reg_lvs5) {
			reg_lvs5 = regulator_get(NULL, "8917_lvs5");
			if (IS_ERR(reg_lvs5)) {
				pr_err("%s: could not get 8917_lvs5, rc = %ld\n",
					__func__, PTR_ERR(reg_lvs5));
				return;
			}
		}

		if (onoff) {
			ret = regulator_enable(reg_lvs5);
			if (ret) {
				pr_err("%s: enable lvs5 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 1.8V on is finished.\n", __func__);
		} else {
			if (regulator_is_enabled(reg_lvs5))
				ret = regulator_disable(reg_lvs5);
			else
				printk(KERN_ERR
					"%s: rugulator LVS5(1.8V) is disabled\n",
						__func__);
			if (ret) {
				pr_err("%s: enable lvs5 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 1.8V off is finished.\n", __func__);
		}


		if (!reg_l30) {
			reg_l30 = regulator_get(NULL, "8917_l30");
			if (IS_ERR(reg_l30)) {
				pr_err("%s: could not get 8917_l30, rc = %ld\n",
					__func__, PTR_ERR(reg_l30));
				return;
			}
			ret = regulator_set_voltage(reg_l30, 2800000, 2800000);
			if (ret) {
				pr_err("%s: unable to set l30 voltage to 3.3V\n",
					__func__);
				return;
			}
		}

		if (onoff) {
			ret = regulator_enable(reg_l30);
			if (ret) {
				pr_err("%s: enable l30 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 3.3V on is finished.\n", __func__);
		} else {
			if (regulator_is_enabled(reg_l30))
				ret = regulator_disable(reg_l30);
			else
				printk(KERN_ERR
					"%s: rugulator L30(3.3V) is disabled\n",
						__func__);
			if (ret) {
				pr_err("%s: disable l30 failed, rc=%d\n",
					__func__, ret);
				return;
			}
			pr_info("%s: tsp 3.3V off is finished.\n", __func__);
		}

	} else {

		gpio_direction_output(GPIO_TKEY_LDO_EN, onoff);

	}

	if (onoff)
		msleep(40);
}

void cypress_touchkey_led_en(bool onoff)
{
	printk(KERN_ERR "%s: %s\n", __func__, onoff ? "on" : "off");

	#if defined(CONFIG_MACH_EXPRESS)
		if (system_rev > BOARD_REV01) {
	#else
		if (system_rev >= BOARD_REV00) {
	#endif
		int ret;
		static struct regulator *reg_l33;
		reg_l33 = regulator_get(NULL, "8917_l33");
		if (IS_ERR(reg_l33))
			pr_err("%s: could not get 8917_l33, rc = %ld\n",
				__func__, PTR_ERR(reg_l33));

		ret = regulator_set_voltage(reg_l33, 3300000, 3300000);
		if (ret) {
			pr_err("%s: unable to set l33 voltage to 3.3V\n",
			__func__);
		}

		if (onoff) {
			ret = regulator_enable(reg_l33);
			if (ret)
				pr_err("%s: enable l33 failed, rc=%d\n",
					__func__, ret);

			pr_info("TKEY_LED 3.3V on is finished.\n");
		} else {
			if (regulator_is_enabled(reg_l33))
				ret = regulator_disable(reg_l33);

			if (ret)
				pr_err("%s: disable l33 failed, rc=%d\n",
					__func__, ret);

			pr_info("TKEY_LED 3.3V off is finished.\n");
		}

	} else {
		if (onoff)
			gpio_set_value(GPIO_TKEY_LED, onoff ? 1 : 0);
	}

}

static u8 touchkey_keycode[] = {KEY_MENU, KEY_BACK};

static struct cypress_touchkey_platform_data cypress_touchkey_pdata = {
	.gpio_int = GPIO_TKEY_INT,
	.touchkey_led_en = cypress_touchkey_led_en,
	.touchkey_keycode = touchkey_keycode,
	.power_onoff = cypress_power_onoff,
};

static struct i2c_board_info touchkey_i2c_devices_info[] __initdata = {
	{
		I2C_BOARD_INFO("cypress_touchkey", 0x20),
		.platform_data = &cypress_touchkey_pdata,
		.irq			= MSM_GPIO_TO_INT(GPIO_TKEY_INT),
	},
};

static struct i2c_gpio_platform_data  cypress_touchkey_i2c_gpio_data = {
	.udelay			= 2,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device touchkey_i2c_gpio_device = {
	.name			= "i2c-gpio",
	.id			= MSM_TOUCHKEY_I2C_BUS_ID,
	.dev.platform_data	= &cypress_touchkey_i2c_gpio_data,
};

static struct platform_device *input_touchkey_devices[] __initdata = {
	&touchkey_i2c_gpio_device,
};

void __init input_touchkey_init(void)
{
	int ret;
	#if defined(CONFIG_MACH_EXPRESS)
		if (system_rev > BOARD_REV01) {
	#else
		if (system_rev >= BOARD_REV00) {
	#endif
		ret = gpio_request(GPIO_TKEY_LDO_EN, "touch_ldo_en");
		if (ret)
			printk(KERN_ERR "%s: err request LDO_EN: %d\n",
					__func__, ret);
	}

	#if defined(CONFIG_MACH_EXPRESS)
		if (system_rev > BOARD_REV01) {
	#else
		if (system_rev >= BOARD_REV00) {
	#endif
		gpio_tlmm_config(GPIO_CFG(GPIO_TKEY_I2C_SDA_REV02,
				0, GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
		gpio_tlmm_config(GPIO_CFG(GPIO_TKEY_I2C_SCL_REV02,
				0, GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);

		cypress_touchkey_i2c_gpio_data.sda_pin =
					GPIO_TKEY_I2C_SDA_REV02;
		cypress_touchkey_i2c_gpio_data.scl_pin =
					GPIO_TKEY_I2C_SCL_REV02;


	} else {
		gpio_tlmm_config(GPIO_CFG(GPIO_TKEY_I2C_SDA,
				0, GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
		gpio_tlmm_config(GPIO_CFG(GPIO_TKEY_I2C_SCL,
				0, GPIO_CFG_INPUT,
				GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);

		cypress_touchkey_i2c_gpio_data.sda_pin =
					GPIO_TKEY_I2C_SDA;
		cypress_touchkey_i2c_gpio_data.scl_pin =
					GPIO_TKEY_I2C_SCL;
	}

	ret = gpio_request(GPIO_TKEY_INT, "gpio_tkey_int");

	if (ret) {
		printk(KERN_ERR "%s: err request Tkey_int: %d\n",
				__func__, ret);
	}

	gpio_tlmm_config(GPIO_CFG(GPIO_TKEY_INT, 0,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	gpio_tlmm_config(GPIO_CFG(GPIO_TKEY_LDO_EN, 0, GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	cypress_power_onoff(1);

	i2c_register_board_info(MSM_TOUCHKEY_I2C_BUS_ID,
				touchkey_i2c_devices_info,
				ARRAY_SIZE(touchkey_i2c_devices_info));

	platform_add_devices(input_touchkey_devices,
				ARRAY_SIZE(input_touchkey_devices));
}
#endif

