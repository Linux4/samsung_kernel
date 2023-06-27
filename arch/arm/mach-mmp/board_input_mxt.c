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
#include <linux/delay.h>
#include <mach/gpio.h>
#include <linux/regulator/machine.h>
#include <mach/devices.h>
#include <mach/mfp-pxa1088-delos.h>


#include "common.h"


#include <linux/input/mxts.h>

#if defined(CONFIG_MACH_GOLDEN)

#define TOUCH_MAX_X	480
#define TOUCH_MAX_Y	800
#define NUM_XNODE 24
#define NUM_YNODE 14
#define PROJECT_NAME	"224S"
#define CONFIG_VER		"0327"

#elif defined(CONFIG_MACH_GOYA)

#define TOUCH_MAX_X	480
#define TOUCH_MAX_Y	800
#define NUM_XNODE 24
#define NUM_YNODE 14
#define PROJECT_NAME	"224S"
#define CONFIG_VER		"0327"
#else
#define TOUCH_MAX_X	540
#define TOUCH_MAX_Y	960
#define NUM_XNODE 24
#define NUM_YNODE 14
#define PROJECT_NAME	"336S"
#define CONFIG_VER		"0327"
#endif

#if 0
#define TSP_SDA	mfp_to_gpio(GPIO017_GPIO_17)
#define TSP_SCL	mfp_to_gpio(GPIO016_GPIO_16)
#else
#define TSP_SDA	mfp_to_gpio(GPIO088_GPIO_88)
#define TSP_SCL	mfp_to_gpio(GPIO087_GPIO_87)
#endif
#define TSP_INT	mfp_to_gpio(GPIO094_GPIO_94)
#define KEY_LED_GPIO mfp_to_gpio(GPIO096_GPIO_96)

#define TOUCH_ON  1
#define TOUCH_OFF 0



static struct regulator *touch_regulator_3v3 =  NULL;


void mxts_register_callback(struct tsp_callbacks *cb)
{
	charger_callbacks = cb;
	pr_debug("[TSP]mxts_register_callback\n");
}

static void mxts_power_onoff(int en)
{
	int ret=0;
	static u8 is_power_on = 0;
	printk("[TSP] %s, %d\n", __func__, en );

	if(touch_regulator_3v3 == NULL) {
		touch_regulator_3v3 = regulator_get(NULL,"v_tsp_3v3");
		if (IS_ERR(touch_regulator_3v3)) {
			touch_regulator_3v3 = NULL;
			printk("get touch_regulator_3v0 regulator error\n");
			return;
		}
	}

	if(en == TOUCH_ON)
	{
		if (!is_power_on)
		{
			is_power_on = 1;
			regulator_set_voltage(touch_regulator_3v3, 3300000, 3300000);

			ret = regulator_enable(touch_regulator_3v3);
			if (ret)
			{
				is_power_on = 0;
				printk(KERN_ERR "%s: touch_regulator_3v0 enable failed (%d)\n",__func__, ret);
			}
		}
	}
	else
	{
		if (is_power_on)
		{
			is_power_on = 0;
			ret = regulator_disable(touch_regulator_3v3);
			if (ret) {
				is_power_on = 1;
				printk(KERN_ERR "%s: touch_regulator_3v0 disable failed (%d)\n",__func__, ret);
			}
		}
	}
	pr_info("[TSP] %s, expected power[%d], actural power[%d]\n", __func__, en, is_power_on);

	msleep(30);

	return;
}

static int mxts_power_on(void)
{
	mxts_power_onoff(1);

	return 0;
}

static int mxts_power_off(void)
{
	mxts_power_onoff(0);

	return 0;
}

#if ENABLE_TOUCH_KEY
static int mxts_led_power_onoff (int en)
{
	if (gpio_request(KEY_LED_GPIO, "key_led_gpio")) {
		printk(KERN_ERR "Request GPIO failed," "gpio: %d \n", KEY_LED_GPIO);
	}

	if(en == TOUCH_ON)
		gpio_direction_output(KEY_LED_GPIO, 1);
	else
		gpio_direction_output(KEY_LED_GPIO, 0);

}

static int mxts_led_power_on(void)
{
	mxts_led_power_onoff(1);

	return 0;
}

static int mxts_led_power_off(void)
{
	mxts_led_power_onoff(0);

	return 0;
}
#endif

static bool mxts_read_chg(void)
{
	return gpio_get_value(TSP_INT);
}

static struct i2c_gpio_platform_data i2c_mxts_bus_data = {
	.sda_pin = TSP_SDA,
	.scl_pin = TSP_SCL,
	.udelay  = 3,
	.timeout = 100,
};
static struct platform_device i2c_mxts_bus_device = {
	.name		= "i2c-gpio",
	.id		= 4,
	.dev = {
		.platform_data = &i2c_mxts_bus_data,
	}
};

struct mxt_platform_data mxts_data = {
	.num_xnode = NUM_XNODE,
	.num_ynode = NUM_YNODE,
	.max_x = TOUCH_MAX_X - 1,
	.max_y = TOUCH_MAX_Y - 1,
	.irqflags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	.read_chg = mxts_read_chg,
	.power_on = mxts_power_on,
	.power_off = mxts_power_off,
	.register_cb = mxts_register_callback,
	.project_name = PROJECT_NAME,
	.config_ver = CONFIG_VER,
#if ENABLE_TOUCH_KEY
	.led_power_on = mxts_led_power_on,
	.led_power_off = mxts_led_power_off,
#endif
};

static struct i2c_board_info __initdata mxts_info[]  = {
	{
		I2C_BOARD_INFO(MXT_DEV_NAME, 0x4a),
		.platform_data = &mxts_data,
	},
};

static struct pxa_i2c_board_gpio mxts_info_gpio[] = {
	{
		.type = MXT_DEV_NAME,
		.gpio = TSP_INT,
	},
};


void __init input_touchscreen_init(void)
{
	int ret = 0;

	pr_info("add i2c device: TOUCHSCREEN ATMEL\n");

	ret = gpio_request(TSP_INT, "mxt_tsp_int");
	if(!ret)
	{
		gpio_direction_input(TSP_INT);
	}
	else
	{
		printk("gpio request fail!\n");
	}

	//mxts_power_on();

	platform_device_register(&i2c_mxts_bus_device);
	i2c_register_board_info(4, ARRAY_AND_SIZE(mxts_info));

	pxa_init_i2c_gpio_irq(ARRAY_AND_SIZE(mxts_info_gpio),
				ARRAY_AND_SIZE(mxts_info));
}

