/* linux/arch/arm/mach-exynos/board-universal4415-input.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/export.h>
#include <linux/platform_data/mms_ts.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/interrupt.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>

#include <mach/irqs.h>
#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>

#include "board-universal4415.h"

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

#define GPIO_POWER_BUTTON	EXYNOS4_GPX2(7)
#define GPIO_VOLUP_BUTTON	EXYNOS4_GPX2(2)
#define GPIO_VOLDOWN_BUTTON	EXYNOS4_GPX3(3)
#define GPIO_HOMEPAGE_BUTTON	EXYNOS4_GPX0(1)

static void universal4415_gpio_keys_config_setup(void)
{
	s3c_gpio_cfgpin(GPIO_POWER_BUTTON, S3C_GPIO_SFN(0xf));
	s3c_gpio_cfgpin(GPIO_VOLUP_BUTTON, S3C_GPIO_SFN(0xf));
	s3c_gpio_cfgpin(GPIO_VOLDOWN_BUTTON, S3C_GPIO_SFN(0xf));
	s3c_gpio_cfgpin(GPIO_HOMEPAGE_BUTTON, S3C_GPIO_SFN(0xf));

	s3c_gpio_setpull(GPIO_POWER_BUTTON, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_VOLUP_BUTTON, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_VOLDOWN_BUTTON, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_HOMEPAGE_BUTTON, S3C_GPIO_PULL_UP);
}

static struct gpio_keys_button universal4415_button[] = {
	{
		.code = KEY_POWER,
		.gpio = GPIO_POWER_BUTTON,
		.desc = "gpio-keys: KEY_POWER",
		.active_low = 1,
		.wakeup = 1,
	}, {
		.code = KEY_VOLUMEUP,
		.gpio = GPIO_VOLUP_BUTTON,
		.desc = "gpio-keys: KEY_VOLUMEUP",
		.active_low = 1,
	}, {
		.code = KEY_VOLUMEDOWN,
		.gpio = GPIO_VOLDOWN_BUTTON,
		.desc = "gpio-keys: KEY_VOLUMEDOWN",
		.active_low = 1,
	}, {
		.code = KEY_HOMEPAGE,
		.gpio = GPIO_HOMEPAGE_BUTTON,
		.desc = "gpio-keys: KEY_HOMEPAGE",
		.active_low = 1,
	},
};

static struct gpio_keys_platform_data universal4415_gpiokeys_platform_data = {
	universal4415_button,
	ARRAY_SIZE(universal4415_button),
};


static struct platform_device universal4415_gpio_keys = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data = &universal4415_gpiokeys_platform_data,
	},
};

#define GPIO_TSP_INT		EXYNOS4_GPX0(5)
#define GPIO_TSP_SDA_18V	EXYNOS4_GPA1(2)
#define GPIO_TSP_SCL_18V	EXYNOS4_GPA1(3)

static bool enabled;
int TSP_VDD_18V(bool on)
{
	struct regulator *regulator;

	if (enabled == on)
		return 0;

	regulator = regulator_get(NULL, "tsp_avdd_1.8");
	if (IS_ERR(regulator))
		return PTR_ERR(regulator);

	if (on) {
		regulator_enable(regulator);
		/*printk(KERN_INFO "[TSP] melfas power on\n"); */
	} else {
		/*
		 * TODO: If there is a case the regulator must be disabled
		 * (e,g firmware update?), consider regulator_force_disable.
		 */
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
	}

	enabled = on;
	regulator_put(regulator);

	return 0;
}

int melfas_power(bool on)
{
	struct regulator *regulator;

	if (enabled == on)
		return 0;

	regulator = regulator_get(NULL, "tsp_avdd_3.3v");
	if (IS_ERR(regulator))
		return PTR_ERR(regulator);

	printk(KERN_DEBUG "[TSP] %s %s\n", __func__, on ? "on" : "off");

	if (on) {
		/* Analog-Panel Power */
		regulator_enable(regulator);
		/* IO Logit Power */
		TSP_VDD_18V(true);
	} else {
		/*
		 * TODO: If there is a case the regulator must be disabled
		 * (e,g firmware update?), consider regulator_force_disable.
		 */
		if (regulator_is_enabled(regulator)) {
			regulator_disable(regulator);
			TSP_VDD_18V(false);
		}
	}

	enabled = on;
	regulator_put(regulator);

	return 0;
}

int is_melfas_vdd_on(void)
{
	int ret;
	/* 3.3V */
	static struct regulator *regulator;

	if (!regulator) {
		regulator = regulator_get(NULL, "tsp_avdd_3.3v");
		if (IS_ERR(regulator)) {
			ret = PTR_ERR(regulator);
			pr_err("could not get touch, rc = %d\n", ret);
			return ret;
		}
/*
		ret = regulator_set_voltage(regulator, 3300000, 3300000);
		if (ret) {
			pr_err("%s: unable to set ldo17 voltage to 3.3V\n",
			       __func__);
			return ret;
		} */
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
		if (gpio_request(GPIO_TSP_SCL_18V, "GPIO_TSP_SCL"))
			pr_err("failed to request gpio(GPIO_TSP_SCL)\n");
		if (gpio_request(GPIO_TSP_SDA_18V, "GPIO_TSP_SDA"))
			pr_err("failed to request gpio(GPIO_TSP_SDA)\n");

		gpio_direction_output(GPIO_TSP_INT, 0);
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);

		gpio_direction_output(GPIO_TSP_SCL_18V, 0);
		s3c_gpio_cfgpin(GPIO_TSP_SCL_18V, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_NONE);

		gpio_direction_output(GPIO_TSP_SDA_18V, 0);
		s3c_gpio_cfgpin(GPIO_TSP_SDA_18V, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_NONE);

	} else {
		gpio_direction_output(GPIO_TSP_INT, 1);
		gpio_direction_input(GPIO_TSP_INT);
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
		/*s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT); */
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
		/*S3C_GPIO_PULL_UP */

		gpio_direction_output(GPIO_TSP_SCL_18V, 1);
		gpio_direction_input(GPIO_TSP_SCL_18V);
		s3c_gpio_cfgpin(GPIO_TSP_SCL_18V, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_NONE);

		gpio_direction_output(GPIO_TSP_SDA_18V, 1);
		gpio_direction_input(GPIO_TSP_SDA_18V);
		s3c_gpio_cfgpin(GPIO_TSP_SDA_18V, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_NONE);

		gpio_free(GPIO_TSP_SCL_18V);
		gpio_free(GPIO_TSP_SDA_18V);
	}
	return 0;
}

int get_lcd_type;
void __init midas_tsp_set_lcdtype(int lcd_type)
{
	get_lcd_type = lcd_type;
}

int melfas_get_lcdtype(void)
{
	return get_lcd_type;
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

static struct melfas_tsi_platform_data mms_ts_pdata = {
	.max_x = 720,
	.max_y = 1280,
#if defined(CONFIG_MACH_SUPERIOR_KOR_SKT)
	.invert_x = 1,
	.invert_y = 1,
#else
	.invert_x = 0,
	.invert_y = 0,
#endif
	.gpio_int = GPIO_TSP_INT,
	.gpio_scl = GPIO_TSP_SCL_18V,
	.gpio_sda = GPIO_TSP_SDA_18V,
	.power = melfas_power,
	.mux_fw_flash = melfas_mux_fw_flash,
	.is_vdd_on = is_melfas_vdd_on,
	.config_fw_version = "I9300_Me_0507",
	.lcd_type = melfas_get_lcdtype,
	.register_cb = melfas_register_callback,
};

static struct i2c_board_info i2c_devs3[] = {
	{
	I2C_BOARD_INFO(MELFAS_TS_NAME, 0x48),
	.platform_data = &mms_ts_pdata},
};

static struct platform_device *universal4415_input_devices[] __initdata = {
        &s3c_device_i2c3,
	&universal4415_gpio_keys,
};

void __init melfas_tsp_init(void)
{
	int gpio;
	int ret;

	/* TSP_INT: XEINT_4 */
	gpio = GPIO_TSP_INT;
	ret = gpio_request(gpio, "TSP_INT");
	if (ret)
		pr_err("melfas-ts : failed to request gpio(TSP_INT)");
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);

	/*s5p_register_gpio_interrupt(gpio);*/
	i2c_devs3[0].irq = gpio_to_irq(gpio);

	s3c_i2c3_set_platdata(NULL);
	i2c_register_board_info(3, i2c_devs3, ARRAY_SIZE(i2c_devs3));

	pr_info("melfas-ts : %s touch : %d\n", __func__, i2c_devs3[0].irq);
}

void __init exynos4_universal4415_input_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");
	melfas_tsp_init();
	universal4415_gpio_keys_config_setup();
	platform_add_devices(universal4415_input_devices,
			ARRAY_SIZE(universal4415_input_devices));
}
