/* linux/arch/arm/mach-exynos/board-universal3250-input.c
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

#include "board-universal3250.h"

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

#define GPIO_TSP_INT		EXYNOS3_GPX3(5)
#define GPIO_TSP_SDA		EXYNOS3_GPA0(6)
#define GPIO_TSP_SCL		EXYNOS3_GPA0(7)

#define GPIO_POWER_BUTTON	EXYNOS3_GPX2(7)
#define GPIO_VOLUP_BUTTON	EXYNOS3_GPX3(2)
#define GPIO_VOLDOWN_BUTTON	EXYNOS3_GPX3(3)
#define GPIO_HOMEPAGE_BUTTON	EXYNOS3_GPX3(4)

#if defined(CONFIG_TOUCHSCREEN_MMS134S)
#include <linux/i2c/mms134s.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_MELFAS_W)
#include <linux/i2c/mms_ts_w.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_MMS134S)
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
static void melfas_register_charger_callback(void *cb)
{
	tsp_callbacks = cb;
	pr_debug("[TSP] melfas_register_lcd_callback\n");
}

void melfas_vdd_pullup(bool on)
{
	struct regulator *regulator_vdd;

	regulator_vdd = regulator_get(NULL, "vtsp_1v8");
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

	regulator_vdd = regulator_get(NULL, "vtsp_a3v0");
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

	i2c_devs3[0].irq = gpio_to_irq(gpio);
	printk(KERN_ERR "%s touch : %d\n", __func__, i2c_devs3[0].irq);
}

#endif

#if defined(CONFIG_TOUCHSCREEN_MELFAS_W)
static bool enabled;
int melfas_power(int on)
{
	struct regulator *regulator_pwr;
	struct regulator *regulator_vdd;
	int ret = 0;

	if (enabled == on) {
		pr_err("melfas-ts : %s same state!", __func__);
		return 0;
	}

	regulator_pwr = regulator_get(NULL, "tsp_vdd_3.3v");
	regulator_vdd = regulator_get(NULL, "tsp_vdd_1.8v");

	if (IS_ERR(regulator_pwr)) {
		pr_err("melfas-ts : %s regulator_pwr error!", __func__);
		return PTR_ERR(regulator_pwr);
	}
	if (IS_ERR(regulator_vdd)) {
		pr_err("melfas-ts : %s regulator_vdd error!", __func__);
		return PTR_ERR(regulator_vdd);
	}

	if (on) {
		regulator_enable(regulator_pwr);
		usleep_range(2500, 3000);
		regulator_enable(regulator_vdd);
	} else {
		if (regulator_is_enabled(regulator_pwr))
			regulator_disable(regulator_pwr);
		else
			regulator_force_disable(regulator_pwr);

		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_force_disable(regulator_vdd);
	}

	if (regulator_is_enabled(regulator_pwr) == !!on &&
		regulator_is_enabled(regulator_vdd) == !!on) {
		pr_info("melfas-ts : %s %s", __func__, !!on ? "ON" : "OFF");
		enabled = on;
	} else {
		pr_err("melfas-ts : regulator_is_enabled value error!");
		ret = -1;
	}

	regulator_put(regulator_vdd);
	regulator_put(regulator_pwr);

	return ret;
}

int melfas_power_vdd(int on)
{
	struct regulator *regulator_vdd;
	int ret = 0;

	regulator_vdd = regulator_get(NULL, "tsp_vdd_1.8v");

	if (IS_ERR(regulator_vdd)) {
		pr_err("melfas-ts : %s regulator_vdd error!", __func__);
		return PTR_ERR(regulator_vdd);
	}

	if (on) {
		regulator_enable(regulator_vdd);
	} else {
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_force_disable(regulator_vdd);
	}

	pr_info("melfas-ts : %s %s", __func__, !!on ? "ON" : "OFF");
	regulator_put(regulator_vdd);

	return ret;
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
	pr_info("melfas-ts : melfas_register_callback");
}

static struct melfas_tsi_platform_data mms_ts_pdata = {
	.max_x = 320,
	.max_y = 320,
	.invert_x = 0,
	.invert_y = 0,
	.gpio_int = GPIO_TSP_INT,
	.gpio_scl = GPIO_TSP_SCL,
	.gpio_sda = GPIO_TSP_SDA,
	.power = melfas_power,
	.power_vdd = melfas_power_vdd,
	.tsp_vendor = "MELFAS",
	.tsp_ic	= "MMS128S",
	.tsp_tx = 7,	/* TX_NUM (Reg Addr : 0xEF) */
	.tsp_rx = 7,	/* RX_NUM (Reg Addr : 0xEE) */
	.config_fw_version = "V700_ME_0910",
	.register_cb = melfas_register_callback,
};

static struct i2c_board_info i2c_devs3[] = {
	{
	 I2C_BOARD_INFO(MELFAS_TS_NAME, 0x48),
	 .platform_data = &mms_ts_pdata},
};

void __init midas_tsp_set_platdata(struct melfas_tsi_platform_data *pdata)
{
	if (!pdata)
		pdata = &mms_ts_pdata;

	i2c_devs3[0].platform_data = pdata;
}

void __init midas_tsp_init(void)
{
	int gpio;
	int ret;
//	pr_info("melfas-ts : W TSP init() is called : [%d]", system_rev);
	pr_info("melfas-ts : W TSP init() is called!");

	gpio = mms_ts_pdata.gpio_int;
	ret = gpio_request(gpio, "TSP_INT");
	if (ret)
		pr_err("melfas-ts : failed to request gpio(TSP_INT)");

	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SDA, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_TSP_SCL, S3C_GPIO_PULL_NONE);

	s5p_register_gpio_interrupt(gpio);
	i2c_devs3[0].irq = gpio_to_irq(gpio);

	pr_info("melfas-ts : %s touch : %d\n", __func__, i2c_devs3[0].irq);
}
#endif

static void universal3250_gpio_keys_config_setup(void)
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

static struct gpio_keys_button universal3250_button[] = {
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

static struct gpio_keys_platform_data universal3250_gpiokeys_platform_data = {
	universal3250_button,
	ARRAY_SIZE(universal3250_button),
};


static struct platform_device universal3250_gpio_keys = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data = &universal3250_gpiokeys_platform_data,
	},
};

static struct platform_device *universal3250_input_devices[] __initdata = {
	&s3c_device_i2c2,
	&universal3250_gpio_keys,
};


void __init exynos3_universal3250_input_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");

#if defined(CONFIG_TOUCHSCREEN_MELFAS_W)
	midas_tsp_init();
#endif

#if defined(CONFIG_TOUCHSCREEN_MMS134S)
	garda_tsp_init(0);
#endif

	s3c_i2c2_set_platdata(NULL);
	i2c_register_board_info(2, i2c_devs3, ARRAY_SIZE(i2c_devs3));

	universal3250_gpio_keys_config_setup();
	platform_add_devices(universal3250_input_devices,
			ARRAY_SIZE(universal3250_input_devices));
}
