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

#include <linux/input.h>
#include <linux/i2c.h>
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
#include "board-kmini.h"

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5
#include <linux/cyttsp5/cyttsp5_bus.h>
#include <linux/cyttsp5/cyttsp5_core.h>
#include <linux/cyttsp5/cyttsp5_btn.h>
#include <linux/cyttsp5/cyttsp5_mt.h>
#include <linux/cyttsp5/cyttsp5_platform.h>
#endif
#if defined(CONFIG_KEYBOARD_ABOV_TOUCH)
#include <linux/i2c/abov_touchkey.h>
#endif
#ifdef CONFIG_SEC_DEBUG
#include <mach/sec_debug.h>
#endif

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

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5
#define CYTTSP5_I2C_NAME "cyttsp5_i2c_adapter"
#define CYTTSP5_I2C_TCH_ADR 0x24

#define CYTTSP5_HID_DESC_REGISTER 1

#define CY_MAXX 720
#define CY_MAXY 1280
#define CY_MINX 0
#define CY_MINY 0

#define CY_ABS_MIN_X CY_MINX
#define CY_ABS_MIN_Y CY_MINY
#define CY_ABS_MAX_X CY_MAXX
#define CY_ABS_MAX_Y CY_MAXY
#define CY_ABS_MIN_P 0
#define CY_ABS_MAX_P 255
#define CY_ABS_MIN_W 0
#define CY_ABS_MAX_W 255

#define CY_ABS_MIN_T 0

#define CY_ABS_MAX_T 15

#define CY_IGNORE_VALUE 0xFFFF

static struct cyttsp5_core_platform_data _cyttsp5_core_platform_data = {
	.irq_gpio = GPIO_TSP_INT,
	.hid_desc_register = CYTTSP5_HID_DESC_REGISTER,
	.xres = cyttsp5_xres,
	.init = cyttsp5_init,
	.power = cyttsp5_power,
	.irq_stat = cyttsp5_irq_stat,
	.sett = {
		NULL,	/* Reserved */
		NULL,	/* Command Registers */
		NULL,	/* Touch Report */
		NULL,	/* Cypress Data Record */
		NULL,	/* Test Record */
		NULL,	/* Panel Configuration Record */
		NULL,	/* &cyttsp5_sett_param_regs, */
		NULL,	/* &cyttsp5_sett_param_size, */
		NULL,	/* Reserved */
		NULL,	/* Reserved */
		NULL,	/* Operational Configuration Record */
		NULL, /* &cyttsp5_sett_ddata, *//* Design Data Record */
		NULL, /* &cyttsp5_sett_mdata, *//* Manufacturing Data Record */
		NULL,	/* Config and Test Registers */
		NULL, /* &cyttsp5_sett_btn_keys, */	/* button-to-keycode table */
	},
	.loader_pdata = &_cyttsp5_loader_platform_data,
	.flags = CY_FLAG_CORE_POWEROFF_ON_SLEEP,
};

static struct cyttsp5_core_info cyttsp5_core_info __initdata = {
	.name = CYTTSP5_CORE_NAME,
	.id = "main_ttsp_core",
	.adap_id = CYTTSP5_I2C_NAME,
	.platform_data = &_cyttsp5_core_platform_data,
};

static const uint16_t cyttsp5_abs[] = {
	ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
	ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
	ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
	CY_IGNORE_VALUE, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
	ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0,
	ABS_MT_TOUCH_MINOR, 0, 255, 0, 0,
	ABS_MT_ORIENTATION, -128, 127, 0, 0,
	ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0,
};

struct touch_framework cyttsp5_framework = {
	.abs = (uint16_t *)&cyttsp5_abs[0],
	.size = ARRAY_SIZE(cyttsp5_abs),
	.enable_vkeys = 0,
};

static struct cyttsp5_mt_platform_data _cyttsp5_mt_platform_data = {
	.frmwrk = &cyttsp5_framework,
	.flags = CY_MT_FLAG_NONE,
	.inp_dev_name = "sec_touchscreen",
};

static struct cyttsp5_device_info cyttsp5_mt_info __initdata = {
	.name = CYTTSP5_MT_NAME,
	.core_id = "main_ttsp_core",
	.platform_data = &_cyttsp5_mt_platform_data,
};

void __init cypress_tsp_init(void)
{
	cyttsp5_register_core_device(&cyttsp5_core_info);
	cyttsp5_register_device(&cyttsp5_mt_info);
}
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5 */

#if defined(CONFIG_INPUT_TOUCHSCREEN)
static struct i2c_board_info i2c_devs_touch[] = {
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5
	{
		I2C_BOARD_INFO(CYTTSP5_I2C_NAME, CYTTSP5_I2C_TCH_ADR),
		.platform_data = CYTTSP5_I2C_NAME,
	},
#endif
};

void __init kmini_tsp_init(void)
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
	printk(KERN_ERR "%s touch : %d \n", __func__,
		i2c_devs_touch[0].irq);
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5
	cypress_tsp_init();
#endif
}
#endif

#if defined(CONFIG_KEYBOARD_ABOV_TOUCH)
static bool abov_keyled_enabled;
int key_led_control(bool on)
{
	struct regulator *regulator;

	if (abov_keyled_enabled == on)
		return 0;

	printk(KERN_DEBUG "[TK] %s %s\n",
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

	abov_keyled_enabled = on;

	return 0;
}

static struct abov_touchkey_platform_data abov_tk_pdata = {
	.keyled = key_led_control,
};

static struct i2c_board_info i2c_devs_tk[] = {
	{
		I2C_BOARD_INFO(ABOV_TK_NAME, 0x20),
		.platform_data = &abov_tk_pdata,
	},
};

void __init kmini_touchkey_init(void)
{
	int gpio, ret;

	gpio = GPIO_TOUCH_KEY_INT;
	ret = gpio_request(gpio, "TK_INT");
	if (ret)
		pr_err("failed to request gpio(TK_INT)(%d)\n", ret);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(gpio);

	i2c_devs_tk[0].irq = gpio_to_irq(gpio);
	printk(KERN_ERR "%s touch : %d \n", __func__,
		i2c_devs_tk[0].irq);
};
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
#ifdef CONFIG_MACH_KMINI
	s3c_gpio_setpull(GPIO_VOL_UP, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_VOL_DOWN, S3C_GPIO_PULL_NONE);
#else
	s3c_gpio_setpull(GPIO_VOL_UP, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_VOL_DOWN, S3C_GPIO_PULL_UP);
#endif
	s3c_gpio_setpull(GPIO_HOME_KEY, S3C_GPIO_PULL_UP);
}

static struct gpio_keys_button smdk4270_button[] = {
	{
		.code = KEY_POWER,
		.gpio = GPIO_PMIC_ONOB,
		.desc = "gpio-keys: KEY_POWER",
		.active_low = 1,
		.wakeup = 1,
#ifdef CONFIG_SEC_DEBUG
		.isr_hook = sec_debug_check_crash_key,
#endif
	}, {
		.code = KEY_VOLUMEUP,
		.gpio = GPIO_VOL_UP,
		.desc = "gpio-keys: KEY_VOLUP",
		.active_low = 1,
#ifdef CONFIG_SEC_DEBUG
		.isr_hook = sec_debug_check_crash_key,
#endif
	}, {
		.code = KEY_VOLUMEDOWN,
		.gpio = GPIO_VOL_DOWN,
		.desc = "gpio-keys: KEY_VOLDOWN",
		.active_low = 1,
#ifdef CONFIG_SEC_DEBUG
		.isr_hook = sec_debug_check_crash_key,
#endif
	}, {
		.code = KEY_HOMEPAGE,
		.gpio = GPIO_HOME_KEY,
		.desc = "gpio-keys: KEY_HOMEPAGE",
		.active_low = 1,
		.wakeup = 1,
	},
};

static struct gpio_keys_platform_data smdk4270_gpiokeys_platform_data = {
	smdk4270_button,
	ARRAY_SIZE(smdk4270_button),
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
#if defined(CONFIG_KEYBOARD_ABOV_TOUCH)
	&s3c_device_i2c4,
#endif
};

void __init exynos4_smdk4270_input_init(void)
{
	smdk4270_gpio_keys_config_setup();
#if defined(CONFIG_INPUT_TOUCHSCREEN)
	kmini_tsp_init();
	s3c_i2c3_set_platdata(NULL);
	i2c_register_board_info(3, i2c_devs_touch, ARRAY_SIZE(i2c_devs_touch));
#endif
#if defined(CONFIG_KEYBOARD_ABOV_TOUCH)
	kmini_touchkey_init();
	s3c_i2c4_set_platdata(NULL);
	i2c_register_board_info(4, i2c_devs_tk, ARRAY_SIZE(i2c_devs_tk));
#endif
	platform_add_devices(smdk4270_input_devices,
			ARRAY_SIZE(smdk4270_input_devices));
}
