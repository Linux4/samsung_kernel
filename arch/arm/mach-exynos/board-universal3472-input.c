/* linux/arch/arm/mach-exynos/board-universal3472-input.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>

#include <mach/irqs.h>
#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>

#include "board-universal3472.h"

#define GPIO_POWER_BUTTON	EXYNOS3_GPX2(7)
#define GPIO_VOLUP_BUTTON	EXYNOS3_GPX2(0)
#define GPIO_VOLDOWN_BUTTON	EXYNOS3_GPX2(1)
#define GPIO_HOMEPAGE_BUTTON	EXYNOS3_GPX0(5)

#define GPIO_TSP_VENDOR1	EXYNOS3_GPM4(4)
#define GPIO_TSP_VENDOR2	EXYNOS3_GPM4(5)

#define GPIO_TSP_INT		EXYNOS3_GPX3(5)
#define GPIO_TSP_SDA_18V	EXYNOS3_GPA0(6)
#define GPIO_TSP_SCL_18V	EXYNOS3_GPA0(7)
#define TOUCH_EN1		EXYNOS3_GPE0(6)
#define TOUCH_EN2		EXYNOS3_GPE0(7)

#if defined(CONFIG_TOUCHSCREEN_MMS252)
#include <linux/i2c/mms252.h>
static bool tsp_power_enabled;
struct tsp_callbacks *tsp_callbacks;
struct tsp_callbacks {
	void (*inform_charger)(struct tsp_callbacks *, bool);
};

void tsp_charger_infom(bool en)
{
	if (tsp_callbacks && tsp_callbacks->inform_charger)
		tsp_callbacks->inform_charger(tsp_callbacks, en);
}

static void melfas_register_charger_callback(void *cb)
{
	tsp_callbacks = cb;
	pr_debug("[TSP] melfas_register_lcd_callback\n");
}

int melfas_power(bool on)
{
	struct regulator *regulator_vdd;
	struct regulator *regulator_pullup;

	if (tsp_power_enabled == on)
		return 0;

	printk(KERN_DEBUG "[TSP] %s %s\n",
		__func__, on ? "on" : "off");

	regulator_vdd = regulator_get(NULL, "tsp_vdd_3.3");
	if (IS_ERR(regulator_vdd))
		return PTR_ERR(regulator_vdd);
	regulator_pullup = regulator_get(NULL, "tsp_vdd_1.8");
	if (IS_ERR(regulator_pullup))
		return PTR_ERR(regulator_pullup);

	if (on) {
		regulator_enable(regulator_vdd);
		usleep_range(2500, 3000);
		regulator_enable(regulator_pullup);
	} else {
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_force_disable(regulator_vdd);

		if (regulator_is_enabled(regulator_pullup))
			regulator_disable(regulator_pullup);
		else
			regulator_force_disable(regulator_pullup);
	}
	regulator_put(regulator_vdd);
	regulator_put(regulator_pullup);

	tsp_power_enabled = on;

	return 0;
}

#if TOUCHKEY
static bool tsp_keyled_enabled;
int key_led_control(bool on)
{
	struct regulator *regulator;

	if (tsp_keyled_enabled == on)
		return 0;

	printk(KERN_DEBUG "[TSP] %s %s\n",
		__func__, on ? "on" : "off");

	regulator = regulator_get(NULL, "vtouch_3.3v");
	if (IS_ERR(regulator))
		return PTR_ERR(regulator);

	if (on) {
		regulator_enable(regulator);
	} else {
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
		else
			regulator_force_disable(regulator);
	}
	regulator_put(regulator);

	tsp_keyled_enabled = on;

	return 0;
}
#endif

int is_melfas_vdd_on(void)
{
	static struct regulator *regulator;
	int ret;

	if (!regulator) {
		regulator = regulator_get(NULL, "tsp_vdd_3.3");
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

void melfas_set_touch_i2c(void)
{
	s3c_gpio_cfgpin(GPIO_TSP_SDA_18V, S3C_GPIO_SFN(3));
	s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(GPIO_TSP_SCL_18V, S3C_GPIO_SFN(3));
	s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_UP);
	gpio_free(GPIO_TSP_SDA_18V);
	gpio_free(GPIO_TSP_SCL_18V);
	s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
	/* s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP); */
	s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
}

void melfas_set_touch_i2c_to_gpio(void)
{
	int ret;
	s3c_gpio_cfgpin(GPIO_TSP_SDA_18V, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(GPIO_TSP_SCL_18V, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_UP);
	ret = gpio_request(GPIO_TSP_SDA_18V, "GPIO_TSP_SDA");
	if (ret)
		pr_err("failed to request gpio(GPIO_TSP_SDA)\n");
	ret = gpio_request(GPIO_TSP_SCL_18V, "GPIO_TSP_SCL");
	if (ret)
		pr_err("failed to request gpio(GPIO_TSP_SCL)\n");

}

struct s3c2410_platform_i2c i2c_data_melfas __initdata = {
	.bus_num	= 2,
	.flags          = 0,
	.slave_addr     = 0x48,
	.frequency      = 400*1000,
	.sda_delay      = 100,
};

static struct melfas_tsi_platform_data mms_ts_pdata = {
	.max_x = 800,
	.max_y = 1280,
#if 0
	.invert_x = 800,
	.invert_y = 1280,
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
#if TOUCHKEY
	.keyled = key_led_control,
#endif
/*	.set_touch_i2c		= melfas_set_touch_i2c, */
/*	.set_touch_i2c_to_gpio	= melfas_set_touch_i2c_to_gpio, */
	.register_cb = melfas_register_charger_callback,
};

static struct i2c_board_info i2c_devs2[] = {
	{
		I2C_BOARD_INFO(MELFAS_TS_NAME, 0x48),
		.platform_data = &mms_ts_pdata
	},
};

void __init midas_tsp_set_platdata(struct melfas_tsi_platform_data *pdata)
{
	if (!pdata)
		pdata = &mms_ts_pdata;

	i2c_devs2[0].platform_data = pdata;
}

static int get_panel_version(void)
{
	u8 panel_version;

	if (gpio_get_value(GPIO_TSP_VENDOR1))
		panel_version = 1;
	else
		panel_version = 0;
	if (gpio_get_value(GPIO_TSP_VENDOR2))
		panel_version = panel_version | (1 << 1);

	/*
	if (gpio_get_value(GPIO_TSP_VENDOR3))
		panel_version = panel_version | (1 << 2);
	*/
	return (int)panel_version;
}

void __init tab3_tsp_init(u32 system_rev)
{
	int gpio;
	int ret;
	u8 panel;
	printk(KERN_INFO "[TSP] midas_tsp_init() is called\n");

	gpio_request(GPIO_TSP_VENDOR1, "GPIO_TSP_VENDOR1");
	gpio_request(GPIO_TSP_VENDOR2, "GPIO_TSP_VENDOR2");
	//gpio_request(GPIO_TSP_VENDOR3, "GPIO_TSP_VENDOR3");

	gpio_direction_input(GPIO_TSP_VENDOR1);
	gpio_direction_input(GPIO_TSP_VENDOR2);
	//gpio_direction_input(GPIO_TSP_VENDOR3);

	s3c_gpio_setpull(GPIO_TSP_VENDOR1, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_VENDOR2, S3C_GPIO_PULL_NONE);
	//s3c_gpio_setpull(GPIO_TSP_VENDOR3, S3C_GPIO_PULL_NONE);

	/* TSP_INT: XEINT_4 */
	gpio = GPIO_TSP_INT;
	ret = gpio_request(gpio, "TSP_INT");
	if (ret)
		pr_err("failed to request gpio(TSP_INT)\n");
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_NONE);

	s5p_register_gpio_interrupt(gpio);
	i2c_devs2[0].irq = gpio_to_irq(gpio);

	printk(KERN_INFO "%s touch irq : %d (%d)\n",
		__func__, i2c_devs2[0].irq, system_rev);

	panel = get_panel_version();
	printk(KERN_INFO "TSP ID : %d\n", panel);
	if (system_rev < 3) {
		if (panel == 1) /* yongfast */
			mms_ts_pdata.panel = 0x8;
		else if (panel == 2) /* wintec */
			mms_ts_pdata.panel = 0x9;
		else
			mms_ts_pdata.panel = panel;
	} else
		mms_ts_pdata.panel = panel;

	printk(KERN_INFO "touch panel : %d\n",
		mms_ts_pdata.panel);

	s3c_i2c2_set_platdata(&i2c_data_melfas);
	//s3c_i2c2_set_platdata(NULL);
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));
}

#endif
static void universal3472_gpio_keys_config_setup(void)
{
	int irq;

	s3c_gpio_cfgpin(GPIO_VOLUP_BUTTON, S3C_GPIO_SFN(0xf));
	s3c_gpio_cfgpin(GPIO_VOLDOWN_BUTTON, S3C_GPIO_SFN(0xf));
	s3c_gpio_cfgpin(GPIO_HOMEPAGE_BUTTON, S3C_GPIO_SFN(0xf));
	s3c_gpio_cfgpin(GPIO_POWER_BUTTON, S3C_GPIO_SFN(0xf));

	s3c_gpio_setpull(GPIO_VOLUP_BUTTON, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_VOLDOWN_BUTTON, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_HOMEPAGE_BUTTON, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_POWER_BUTTON, S3C_GPIO_PULL_UP);

	irq = s5p_register_gpio_interrupt(GPIO_VOLUP_BUTTON);
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure VOL UP GPIO\n", __func__);
		return;
	}
	irq = s5p_register_gpio_interrupt(GPIO_VOLDOWN_BUTTON);
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure VOL DOWN GPIO\n", __func__);
		return;
	}
	irq = s5p_register_gpio_interrupt(GPIO_HOMEPAGE_BUTTON);
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure HOMEPAGE GPIO\n", __func__);
		return;
	}
	irq = s5p_register_gpio_interrupt(GPIO_POWER_BUTTON);
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure POWER GPIO\n", __func__);
		return;
	}
}

static struct gpio_keys_button universal3472_button[] = {
	{
		.code = KEY_POWER,
		.gpio = GPIO_POWER_BUTTON,
		.desc = "gpio-keys: KEY_POWER",
		.active_low = 1,
		.wakeup = 1,
	}, {
		.code = KEY_VOLUMEDOWN,
		.gpio = GPIO_VOLDOWN_BUTTON,
		.desc = "gpio-keys: KEY_VOLUMEDOWN",
		.active_low = 1,
	}, {
		.code = KEY_VOLUMEUP,
		.gpio = GPIO_VOLUP_BUTTON,
		.desc = "gpio-keys: KEY_VOLUMEUP",
		.active_low = 1,
	}, {
		.code = KEY_HOMEPAGE,
		.gpio = GPIO_HOMEPAGE_BUTTON,
		.desc = "gpio-keys: KEY_HOMEPAGE",
		.active_low = 1,
	},
};

static struct gpio_keys_platform_data universal3472_gpiokeys_platform_data = {
	universal3472_button,
	ARRAY_SIZE(universal3472_button),
};


static struct platform_device universal3472_gpio_keys = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data = &universal3472_gpiokeys_platform_data,
	},
};

static struct platform_device *universal3472_input_devices[] __initdata = {
	&universal3472_gpio_keys,
	&s3c_device_i2c2,
};


void __init exynos3_universal3472_input_init(void)
{
#if defined(CONFIG_TOUCHSCREEN_MMS252)
	s3c_gpio_cfgpin(TOUCH_EN1, S3C_GPIO_SFN(0x1));
	s3c_gpio_setpull(TOUCH_EN1, S3C_GPIO_PULL_NONE);
	gpio_set_value(TOUCH_EN1, 0x1);

	s3c_gpio_cfgpin(TOUCH_EN2, S3C_GPIO_SFN(0x1));
	s3c_gpio_setpull(TOUCH_EN2, S3C_GPIO_PULL_NONE);
	gpio_set_value(TOUCH_EN2, 0x1);

	midas_tsp_set_platdata(&mms_ts_pdata);
	tab3_tsp_init(3);
#endif
	universal3472_gpio_keys_config_setup();
	platform_add_devices(universal3472_input_devices,
			ARRAY_SIZE(universal3472_input_devices));
}
