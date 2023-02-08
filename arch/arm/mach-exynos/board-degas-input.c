/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio_keys.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/irqs.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <mach/gpio-shannon222ap.h>
#include <linux/regulator/consumer.h>

#include "board-universal222ap.h"
#include "board-degas.h"

#ifdef CONFIG_TOUCHSCREEN_ZINITIX_BT532
#include <linux/i2c/zinitix_bt532_ts.h>

#endif
#ifdef CONFIG_SEC_DEBUG
#include <mach/sec_debug.h>
#endif

#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

extern unsigned int system_rev;

#ifdef CONFIG_INPUT_TOUCHSCREEN
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

#ifdef CONFIG_TOUCHSCREEN_ZINITIX_BT532

static void zinitix_register_charger_callback(void *cb)
{
	tsp_callbacks = cb;
	pr_debug("[TSP] zinitix_register_lcd_callback\n");
}

static int bt532_power(int on){
	struct regulator *regulator_vdd;

//	if (enabled == on)
//		return 0;

	regulator_vdd = regulator_get(NULL, "vdd_tsp_2v8");
	if (IS_ERR(regulator_vdd)) {
		printk(KERN_ERR "[TSP]ts_power_on : tsp_avdd regulator_get failed\n");
		return PTR_ERR(regulator_vdd);
	}

	printk(KERN_ERR "[TSP] %s %s\n", __func__, on ? "on" : "off");

	if (on) {
		regulator_enable(regulator_vdd);

		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
	} else {
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);

		/*
		 * TODO: If there is a case the regulator must be disabled
		 * (e,g firmware update?), consider regulator_force_disable.
		 */
		regulator_disable(regulator_vdd);

		/* TODO: Delay time should be adjusted */
		msleep(10);
	}

//	enabled = on;
	regulator_put(regulator_vdd);

	return 0;
}

static struct bt532_ts_platform_data bt532_ts_pdata = {
	.gpio_int = GPIO_TSP_INT,
	.tsp_power	= bt532_power,
	.x_resolution	= 799,
	.y_resolution	= 1279,
	.page_size	= 128,
	.orientation	= 0,
	.register_cb = zinitix_register_charger_callback,
};


void __init zinitix_tsp_init(void)
{
	s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
}

#endif /* CONFIG_TOUCHSCREEN_ZINITIX_BT532 */

#if defined(CONFIG_INPUT_TOUCHSCREEN)
static struct i2c_board_info i2c_devs_touch[] = {
#ifdef CONFIG_TOUCHSCREEN_ZINITIX_BT532
	{
		I2C_BOARD_INFO(BT532_TS_DEVICE, 0x20),
		.platform_data = &bt532_ts_pdata,
	},
#endif
};

static void __init degas_tsp_init(void)
{
	int gpio;
	int ret;

	/* TSP_INT: XEINT_4 */
	gpio = GPIO_TSP_INT;
	ret = gpio_request(gpio, "TSP_INT");
	if (ret)
		pr_err("failed to request gpio(TSP_INT)(%d)\n", ret);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(gpio);

	i2c_devs_touch[0].irq = gpio_to_irq(gpio);
	printk(KERN_ERR "%s touch : %d\n", __func__,
		i2c_devs_touch[0].irq);
#ifdef CONFIG_TOUCHSCREEN_ZINITIX_BT532
	zinitix_tsp_init();
#endif
}
#endif

static void smdk4270_gpio_keys_config_setup(void)
{
	int irq;
	irq = s5p_register_gpio_interrupt(GPIO_VOL_UP);	/* VOL UP */
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure VOL UP GPIO\n", __func__);
		return;
	}
	irq = s5p_register_gpio_interrupt(GPIO_VOL_DOWN);	/* VOL DOWN */
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure VOL DOWN GPIO\n", __func__);
		return;
	}
	irq = s5p_register_gpio_interrupt(GPIO_HOME_KEY);	/* HOME PAGE */
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure HOME PAGE GPIO\n", __func__);
		return;
	}
	/* set pull up gpio key */
	s3c_gpio_setpull(GPIO_VOL_UP, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_VOL_DOWN, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_HOME_KEY, S3C_GPIO_PULL_UP);
}

static struct gpio_keys_button smdk4270_button[] = {
	{
		.code = KEY_POWER,
		.gpio = GPIO_PMIC_ONOB,
		.desc = "gpio-keys: KEY_POWER",
		.active_low = 1,
		.debounce_interval = 10,
		.wakeup = 1,
#ifdef CONFIG_SEC_DEBUG
		.isr_hook = sec_debug_check_crash_key,
#endif
	}, {
		.code = KEY_VOLUMEUP,
		.gpio = GPIO_VOL_UP,
		.desc = "gpio-keys: KEY_VOLUP",
		.active_low = 1,
		.debounce_interval = 10,
#ifdef CONFIG_SEC_DEBUG
		.isr_hook = sec_debug_check_crash_key,
#endif
	}, {
		.code = KEY_VOLUMEDOWN,
		.gpio = GPIO_VOL_DOWN,
		.desc = "gpio-keys: KEY_VOLDOWN",
		.active_low = 1,
		.debounce_interval = 10,
#ifdef CONFIG_SEC_DEBUG
		.isr_hook = sec_debug_check_crash_key,
#endif
	}, {
		.code = KEY_HOMEPAGE,
		.gpio = GPIO_HOME_KEY,
		.desc = "gpio-keys: KEY_HOMEPAGE",
		.active_low = 1,
		.debounce_interval = 10,
		.wakeup = 1,
	},
};

#ifdef CONFIG_INPUT_BOOSTER
static enum booster_device_type get_booster_device(int code)
{
	switch (code) {
	case KEY_BOOSTER_TOUCH:
		return BOOSTER_DEVICE_TOUCH;
	break;
	default:
		return BOOSTER_DEVICE_NOT_DEFINED;
	break;
	}
}

static const struct dvfs_freq touch_freq_table[BOOSTER_LEVEL_MAX] = {
	[BOOSTER_LEVEL1] = BOOSTER_DVFS_FREQ(1000000,	400000,	200000),
	[BOOSTER_LEVEL2] = BOOSTER_DVFS_FREQ( 800000,	267000,	160000),
	[BOOSTER_LEVEL3] = BOOSTER_DVFS_FREQ(1000000,	400000,	200000),
};

static struct booster_key booster_keys[] = {
	BOOSTER_KEYS("TOUCH", KEY_BOOSTER_TOUCH,
			BOOSTER_DEFAULT_CHG_TIME,
			BOOSTER_DEFAULT_OFF_TIME,
			touch_freq_table),
};

/* Caution : keys, nkeys, get_device_type should be defined */

static struct input_booster_platform_data input_booster_pdata = {
	.keys = booster_keys,
	.nkeys = ARRAY_SIZE(booster_keys),
	.get_device_type = get_booster_device,
};

static struct platform_device universal5260_input_booster = {
	.name = INPUT_BOOSTER_NAME,
	.dev.platform_data = &input_booster_pdata,
};
#endif

static struct gpio_keys_platform_data smdk4270_gpiokeys_platform_data = {
	smdk4270_button,
	ARRAY_SIZE(smdk4270_button),
#ifdef CONFIG_SENSORS_HALL
	.gpio_flip_cover = GPIO_FLIPCOVER,
#endif
};

static struct platform_device smdk4270_gpio_keys = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data = &smdk4270_gpiokeys_platform_data,
	},
};

static struct platform_device *smdk4270_input_devices[] __initdata = {
	&smdk4270_gpio_keys,
#if defined(CONFIG_INPUT_TOUCHSCREEN)
	&s3c_device_i2c3,
#endif
#ifdef CONFIG_INPUT_BOOSTER
	&universal5260_input_booster,
#endif
};

void __init exynos4_smdk4270_input_init(void)
{
	smdk4270_gpio_keys_config_setup();
#if defined(CONFIG_INPUT_TOUCHSCREEN)
	degas_tsp_init();
	s3c_i2c3_set_platdata(NULL);
	i2c_register_board_info(3, i2c_devs_touch, ARRAY_SIZE(i2c_devs_touch));
#endif
	platform_add_devices(smdk4270_input_devices,
		ARRAY_SIZE(smdk4270_input_devices));
}
