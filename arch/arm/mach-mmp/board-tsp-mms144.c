/*
 * linux/arch/arm/mach-exynos/midas-tsp.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>

#include <linux/err.h>
#include <linux/gpio.h>

#include <linux/input/mms144_ts.h>
#include <linux/regulator/consumer.h>
#include <mach/mfp-pxa1088-wilcox.h>
#include "common.h"
#include <mach/devices.h>
static bool enabled;

#define TSP_INT	mfp_to_gpio(GPIO094_GPIO_94)
#define TSP_SCL	mfp_to_gpio(GPIO087_GPIO_87)
#define TSP_SDA	mfp_to_gpio(GPIO088_GPIO_88)

struct regulator *regulator_vdd=NULL;

int melfas_power(bool on)
{
	int ret;

	if (enabled == on)
		return 0;

	printk(KERN_DEBUG "[TSP] %s %s\n", __func__, on ? "on" : "off");

	if (regulator_vdd == NULL){

	regulator_vdd = regulator_get(NULL, "v_tsp_3v3");

	if (IS_ERR(regulator_vdd))
			return PTR_ERR(regulator_vdd);

	ret = regulator_set_voltage(regulator_vdd,3000000,3000000);
	printk("[TSP] regulator_set_voltage ret = %d \n", ret);
	if (ret) {
		pr_err("can not set voltage TSP VDD 3.0V, ret=%d\n", ret);
		regulator_vdd = NULL;
		return ret;
	}

	}

	if (on) {
		ret=regulator_enable(regulator_vdd);
		printk("[TSP] 2.85V regulator_enable ret = %d \n", ret);
		if (ret) {
			pr_err("can not enable TSP VDD 3.0V, ret=%d\n", ret);
		return ret;
		}

	} else {
		/*
		 * TODO: If there is a case the regulator must be disabled
		 * (e,g firmware update?), consider regulator_force_disable.
		 */
		//if (regulator_is_enabled(regulator_vdd))
		{
			ret=regulator_disable(regulator_vdd);
			printk("[TSP] 2.85V regulator_disable ret = %d \n", ret);
			if (ret) {
				pr_err("can not disable TSP VDD 3.0V, ret=%d\n", ret);
				return ret;
			}

		}
	}

	enabled = on;

	return 0;
}
//EXPORT_SYMBOL(melfas_power);

int is_melfas_vdd_on(void)
{
	int ret;
	/* 3.3V */
	static struct regulator *regulator;

	if (!regulator) {
		regulator = regulator_get(NULL, "v_tsp_3v3");
		if (IS_ERR(regulator)) {
			ret = PTR_ERR(regulator);
			pr_err("could not get touch, rc = %d\n", ret);
			return ret;
		}
	}

	if (regulator_is_enabled(regulator))
		return 1;
	else
		return 0;
}

int melfas_mux_fw_flash(bool to_gpios)
{
	pr_info("%s:to_gpios=%d\n", __func__, to_gpios);

	/* TOUCH_EN is always an output */
	if (to_gpios) {

	} else {

	}
	return 0;
}

struct tsp_callbacks *charger_callbacks;
struct tsp_callbacks {
	void (*inform_charger)(struct tsp_callbacks *, bool);
};

void tsp_charger_infom(bool en)
{
	if (charger_callbacks && charger_callbacks->inform_charger)
		charger_callbacks->inform_charger(charger_callbacks, en);
}

static void melfas_register_callback(void *cb)
{
	charger_callbacks = cb;
	pr_debug("[TSP] melfas_register_callback\n");
}

#ifdef CONFIG_LCD_FREQ_SWITCH
struct tsp_lcd_callbacks *lcd_callbacks;
struct tsp_lcd_callbacks {
	void (*inform_lcd)(struct tsp_lcd_callbacks *, bool);
};

void tsp_lcd_infom(bool en)
{
	if (lcd_callbacks && lcd_callbacks->inform_lcd)
		lcd_callbacks->inform_lcd(lcd_callbacks, en);
}

static void melfas_register_lcd_callback(void *cb)
{
	lcd_callbacks = cb;
	pr_debug("[TSP] melfas_register_lcd_callback\n");
}
#endif

static struct melfas_tsi_platform_data mms_ts_pdata = {
	.max_x = 480,
	.max_y = 800,
	.invert_x = 0,
	.invert_y = 0,
	.gpio_int = TSP_INT,
	.gpio_scl = NULL,
	.gpio_sda = NULL,
	.power = melfas_power,
	.mux_fw_flash = melfas_mux_fw_flash,
	.is_vdd_on = is_melfas_vdd_on,
	.config_fw_version = "N7100_Me_0910",
	.lcd_type = NULL,
	.register_cb = melfas_register_callback,
#ifdef CONFIG_LCD_FREQ_SWITCH
	.register_lcd_cb = melfas_register_lcd_callback,
#endif
};

static struct pxa_i2c_board_gpio melfas_i2c_gpio[] = {
	{
		.type = MELFAS_TS_NAME,
		.gpio = mfp_to_gpio(GPIO094_GPIO_94),
	},
};
static struct i2c_board_info __initdata melfas_i2c_devices[]  = {
	{
		I2C_BOARD_INFO(MELFAS_TS_NAME, 0x48),
	 	.platform_data = &mms_ts_pdata,
	},
};

void __init board_tsp_init_mms144(void)
{
	printk(KERN_INFO "[TSP] board_tsp_init_mms144() is called\n");

	pxa_init_i2c_gpio_irq(ARRAY_AND_SIZE(melfas_i2c_gpio),
				ARRAY_AND_SIZE(melfas_i2c_devices));	
	i2c_register_board_info(1, &melfas_i2c_devices,ARRAY_SIZE(melfas_i2c_devices));
}


