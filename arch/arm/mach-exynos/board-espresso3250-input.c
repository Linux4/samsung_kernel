/* linux/arch/arm/mach-exynos/board-espresso3250-input.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
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

#include "board-espresso3250.h"

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

#define GPIO_TSP_INT		EXYNOS3_GPX0(5)
#define GPIO_TSP_SDA		EXYNOS3_GPB(2)
#define GPIO_TSP_SCL		EXYNOS3_GPB(3)

#define GPIO_POWER_BUTTON	EXYNOS3_GPX2(7)
#define GPIO_VOLUP_BUTTON	EXYNOS3_GPX0(0)
#define GPIO_VOLDOWN_BUTTON	EXYNOS3_GPX1(7)
#define GPIO_HOMEPAGE_BUTTON	EXYNOS3_GPX0(3)

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT432)
#include <linux/i2c/zinitix_ts.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_MMS134S)
#include <linux/i2c/mms134s.h>
#endif

#ifdef CONFIG_INPUT_TOUCHSCREEN
static bool tsp_power_enabled;
struct tsp_callbacks *tsp_callbacks;
struct tsp_callbacks {
	void (*inform_charger) (struct tsp_callbacks *, bool);
};

void tsp_charger_infom(bool en)
{
	if (tsp_callbacks && tsp_callbacks->inform_charger)
		tsp_callbacks->inform_charger(tsp_callbacks, en);
}
#endif

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT432)
static void zinitix_register_charger_callback(void *cb)
{
	tsp_callbacks = cb;
	pr_debug("[TSP] zinitix_register_lcd_callback\n");
}

int zinitix_power(bool on)
{
	struct regulator *regulator_vdd;

	if (tsp_power_enabled == on)
		return 0;

	pr_debug("[TSP] %s %s\n", __func__, on ? "on" : "off");

	regulator_vdd = regulator_get(NULL, "vtsp_a3v0");
	if (IS_ERR(regulator_vdd))
		return PTR_ERR(regulator_vdd);

	if (on) {
		regulator_enable(regulator_vdd);
	} else {
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_disable(regulator_vdd);
	}
	regulator_put(regulator_vdd);

	tsp_power_enabled = on;

	return 0;
}

int is_zinitix_vdd_on(void)
{
	static struct regulator *regulator;
	int ret;

	if (!regulator) {
		regulator = regulator_get(NULL, "vtsp_a3v0");
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

int zinitix_mux_fw_flash(bool to_gpios)
{
	pr_info("%s:to_gpios=%d\n", __func__, to_gpios);

	/* TOUCH_EN is always an output */
	if (to_gpios) {
		if (gpio_request(GPIO_TSP_SCL, "GPIO_TSP_SCL"))
			pr_err("failed to request gpio(GPIO_TSP_SCL)\n");
		if (gpio_request(GPIO_TSP_SDA, "GPIO_TSP_SDA"))
			pr_err("failed to request gpio(GPIO_TSP_SDA)\n");

		gpio_direction_output(GPIO_TSP_INT, 0);
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);

		gpio_direction_output(GPIO_TSP_SCL, 0);
		s3c_gpio_cfgpin(GPIO_TSP_SCL, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_SCL, S3C_GPIO_PULL_NONE);

		gpio_direction_output(GPIO_TSP_SDA, 0);
		s3c_gpio_cfgpin(GPIO_TSP_SDA, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_SDA, S3C_GPIO_PULL_NONE);

	} else {
		gpio_direction_output(GPIO_TSP_INT, 1);
		gpio_direction_input(GPIO_TSP_INT);
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
		/*S3C_GPIO_PULL_UP */

		gpio_direction_output(GPIO_TSP_SCL, 1);
		gpio_direction_input(GPIO_TSP_SCL);
		s3c_gpio_cfgpin(GPIO_TSP_SCL, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(GPIO_TSP_SCL, S3C_GPIO_PULL_NONE);

		gpio_direction_output(GPIO_TSP_SDA, 1);
		gpio_direction_input(GPIO_TSP_SDA);
		s3c_gpio_cfgpin(GPIO_TSP_SDA, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(GPIO_TSP_SDA, S3C_GPIO_PULL_NONE);

		gpio_free(GPIO_TSP_SCL);
		gpio_free(GPIO_TSP_SDA);
	}
	return 0;
}

void zinitix_set_touch_i2c(void)
{
	s3c_gpio_cfgpin(GPIO_TSP_SDA, S3C_GPIO_SFN(3));
	s3c_gpio_setpull(GPIO_TSP_SDA, S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(GPIO_TSP_SCL, S3C_GPIO_SFN(3));
	s3c_gpio_setpull(GPIO_TSP_SCL, S3C_GPIO_PULL_UP);
	gpio_free(GPIO_TSP_SDA);
	gpio_free(GPIO_TSP_SCL);
	s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
	/* s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP); */
	s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
}

void zinitix_set_touch_i2c_to_gpio(void)
{
	int ret;
	s3c_gpio_cfgpin(GPIO_TSP_SDA, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TSP_SDA, S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(GPIO_TSP_SCL, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TSP_SCL, S3C_GPIO_PULL_UP);
	ret = gpio_request(GPIO_TSP_SDA, "GPIO_TSP_SDA");
	if (ret)
		pr_err("failed to request gpio(GPIO_TSP_SDA)\n");
	ret = gpio_request(GPIO_TSP_SCL, "GPIO_TSP_SCL");
	if (ret)
		pr_err("failed to request gpio(GPIO_TSP_SCL)\n");

}

struct i2c_client *bt404_i2c_client;

void put_isp_i2c_client(struct i2c_client *client)
{
	bt404_i2c_client = client;
}

struct i2c_client *get_isp_i2c_client(void)
{
	return bt404_i2c_client;
}

static struct zxt_ts_platform_data zxt_ts_pdata = {
	.gpio_int = GPIO_TSP_INT,
	.gpio_scl = GPIO_TSP_SCL,
	.gpio_sda = GPIO_TSP_SDA,
	.x_resolution = 800,
	.y_resolution = 1280,
	.orientation = 0,
	.power = zinitix_power,
	/*.mux_fw_flash = zinitix_mux_fw_flash,
	   .is_vdd_on = is_zinitix_vdd_on, */
	/*      .set_touch_i2c          = zinitix_set_touch_i2c, */
	/*      .set_touch_i2c_to_gpio  = zinitix_set_touch_i2c_to_gpio, */
	/*      .lcd_type = zinitix_get_lcdtype, */
	.register_cb = zinitix_register_charger_callback,
#ifdef CONFIG_LCD_FREQ_SWITCH
	.register_lcd_cb = zinitix_register_lcd_callback,
#endif
};

static struct i2c_board_info i2c_devs_touch[] = {
	{
	 I2C_BOARD_INFO(ZINITIX_TS_NAME, 0x20),
	 .platform_data = &zxt_ts_pdata},
};

void __init garda_tsp_set_platdata(struct zxt_ts_platform_data *pdata)
{
	if (!pdata)
		pdata = &zxt_ts_pdata;

	i2c_devs_touch[0].platform_data = pdata;
}
#endif

#if defined(CONFIG_TOUCHSCREEN_MMS134S)
static void melfas_register_charger_callback(void *cb)
{
	tsp_callbacks = cb;
	pr_debug("[TSP] melfas_register_lcd_callback\n");
}

void melfas_vdd_pullup(bool on)
{
	struct regulator *regulator_vdd;

	regulator_vdd = regulator_get(NULL, "tsp_vdd_1.8v");
	if (IS_ERR(regulator_vdd))
		return;

	if (on) {
		regulator_enable(regulator_vdd);
	} else {
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_force_disable(regulator_vdd);
	}
	regulator_put(regulator_vdd);
}

int melfas_power(bool on)
{
	struct regulator *regulator_vdd;

	if (tsp_power_enabled == on)
		return 0;

	printk(KERN_DEBUG "[TSP] %s %s\n",
		__func__, on ? "on" : "off");

	regulator_vdd = regulator_get(NULL, "tsp_vdd_3.3v");
	if (IS_ERR(regulator_vdd))
		return PTR_ERR(regulator_vdd);

	if (on) {
		regulator_enable(regulator_vdd);
		usleep_range(2500, 3000);
	} else {
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_force_disable(regulator_vdd);
	}
	regulator_put(regulator_vdd);

	melfas_vdd_pullup(on);

	tsp_power_enabled = on;

	return 0;
}

static bool tsp_keyled_enabled;
int key_led_control(bool on)
{
	struct regulator *regulator;

	if (tsp_keyled_enabled == on)
		return 0;

	printk(KERN_DEBUG "[TSP] %s %s\n",
		__func__, on ? "on" : "off");

	regulator = regulator_get(NULL, "key_led_3v3");
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

int is_melfas_vdd_on(void)
{
	static struct regulator *regulator;
	int ret;

	if (!regulator) {
		regulator = regulator_get(NULL, "tsp_vdd_3.3v");
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

static struct melfas_tsi_platform_data mms_ts_pdata = {
	.max_x = 480,
	.max_y = 800,
	.gpio_int = GPIO_TSP_INT,
	.gpio_scl = GPIO_TSP_SCL,
	.gpio_sda = GPIO_TSP_SDA,
	.power = melfas_power,
	.is_vdd_on = is_melfas_vdd_on,
	.touchkey = true,
	.keyled = key_led_control,
	.register_cb = melfas_register_charger_callback,
};

static struct i2c_board_info i2c_devs3[] = {
	{
		I2C_BOARD_INFO(MELFAS_TS_NAME, 0x48),
		.platform_data = &mms_ts_pdata
	},
};

void __init midas_tsp_set_platdata(struct melfas_tsi_platform_data *pdata)
{
	if (!pdata)
		pdata = &mms_ts_pdata;

	i2c_devs3[0].platform_data = pdata;
}
#endif

#ifdef CONFIG_INPUT_TOUCHSCREEN
void __init garda_tsp_init(u32 system_rev)
{
	int gpio;
	int ret;

	printk(KERN_ERR "tsp:%s called\n", __func__);

	gpio = GPIO_TSP_INT;
	ret = gpio_request(gpio, "TSP_INT");
	if (ret)
		pr_err("failed to request gpio(TSP_INT)(%d)\n", ret);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SDA, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SCL, S3C_GPIO_PULL_NONE);

	s5p_register_gpio_interrupt(gpio);

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT432)
	i2c_devs_touch[0].irq = gpio_to_irq(gpio);
	printk(KERN_ERR "%s touch : %d\n", __func__, i2c_devs_touch[0].irq);
	zinitix_power(0);
#endif
#if defined(CONFIG_TOUCHSCREEN_MMS134S)
	i2c_devs3[0].irq = gpio_to_irq(gpio);
	printk(KERN_ERR "%s touch : %d\n", __func__, i2c_devs3[0].irq);
#endif
}
#endif

static void espresso3250_gpio_keys_config_setup(void)
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

static struct gpio_keys_button espresso3250_button[] = {
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

static struct gpio_keys_platform_data espresso3250_gpiokeys_platform_data = {
	espresso3250_button,
	ARRAY_SIZE(espresso3250_button),
};

static struct platform_device espresso3250_gpio_keys = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data = &espresso3250_gpiokeys_platform_data,
	},
};

static struct platform_device *espresso3250_input_devices[] __initdata = {
	&s3c_device_i2c5,
	&espresso3250_gpio_keys,
};

void __init exynos3_espresso3250_input_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");
#if defined(CONFIG_INPUT_TOUCHSCREEN)
	garda_tsp_init(0);

	s3c_i2c5_set_platdata(NULL);
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT432)
	i2c_register_board_info(5, i2c_devs_touch, ARRAY_SIZE(i2c_devs_touch));

#elif defined(CONFIG_TOUCHSCREEN_MMS134S)
	i2c_register_board_info(5, i2c_devs3, ARRAY_SIZE(i2c_devs3));
#endif
#endif

	espresso3250_gpio_keys_config_setup();
	platform_add_devices(espresso3250_input_devices,
			ARRAY_SIZE(espresso3250_input_devices));
}
