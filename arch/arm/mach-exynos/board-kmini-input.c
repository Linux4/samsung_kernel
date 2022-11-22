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
#include "board-kmini.h"

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5
#include <linux/cyttsp5/cyttsp5_core.h>
#include <linux/cyttsp5/cyttsp5_platform.h>
#endif
#if defined(CONFIG_KEYBOARD_ABOV_TOUCH)
#include <linux/i2c/abov_touchkey.h>
#endif
#ifdef CONFIG_SEC_DEBUG
#include <mach/sec_debug.h>
#endif

extern unsigned int system_rev;
extern unsigned int lpcharge;

#ifdef CONFIG_INPUT_TOUCHSCREEN
bool tsp_connect;
struct tsp_callbacks *tsp_callbacks;
struct tsp_callbacks {
	void (*inform_charger) (struct tsp_callbacks *, bool);
};

void tsp_charger_inform(bool en)
{
	if (tsp_callbacks && tsp_callbacks->inform_charger)
		tsp_callbacks->inform_charger(tsp_callbacks, en);
}

static int __init tsp_connect_check(char *str)
{
	tsp_connect = true;

	return 0;
}
early_param("tspconnect", tsp_connect_check);
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5
#define GPIO_TSP_TA EXYNOS4_GPM1(3)
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

static void cyttsp5_charger_callback(void *cb)
{
	tsp_callbacks = cb;
	pr_debug("[TSP] cyttsp5_charger_callback\n");
}

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
	.ta_gpio = GPIO_TSP_TA,
	.register_cb = cyttsp5_charger_callback,
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

static struct cyttsp5_platform_data _cyttsp5_platform_data = {
	.core_pdata = &_cyttsp5_core_platform_data,
	.mt_pdata = &_cyttsp5_mt_platform_data,
	.loader_pdata = &_cyttsp5_loader_platform_data,
};

void __init cypress_tsp_init(void)
{
	s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_DOWN);
}
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5 */

#if defined(CONFIG_INPUT_TOUCHSCREEN)
static struct i2c_board_info i2c_devs_touch[] = {
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5
	{
		I2C_BOARD_INFO(CYTTSP5_I2C_NAME, CYTTSP5_I2C_TCH_ADR),
		.platform_data = &_cyttsp5_platform_data,
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

	if (tsp_connect == false) {
		printk(KERN_ERR "%s tsp_connect : %d\n", __func__, tsp_connect);
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_DOWN);
		return;
	}

	if (lpcharge) {
		printk(KERN_DEBUG "%s : lpcharge. tsp driver unload\n", __func__);
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_DOWN);
		return;
	}

	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(gpio);

	i2c_devs_touch[0].irq = gpio_to_irq(gpio);
	printk(KERN_ERR "%s touch : %d\n", __func__,
		i2c_devs_touch[0].irq);
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5
	cypress_tsp_init();
	gpio = GPIO_TSP_TA;
	ret = gpio_request(gpio, "TSP_TA");
	if (ret) {
		pr_err("failed to request gpio(TSP_TA)(%d)\n", ret);
	} else {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		gpio_set_value(gpio, 0);
	}
#endif

	s3c_i2c3_set_platdata(NULL);
	i2c_register_board_info(3, i2c_devs_touch, ARRAY_SIZE(i2c_devs_touch));
}
#endif

#if defined(CONFIG_KEYBOARD_ABOV_TOUCH)
static bool abov_key_enabled;
static bool abov_keyled_enabled;
#define GPIO_TK_SCL EXYNOS4_GPM2(4)
#define GPIO_TK_SDA EXYNOS4_GPM3(0)
#define GPIO_TK_RST EXYNOS4_GPF0(6)
#define GPIO_TK_RST_04 EXYNOS4_GPK3(3)
#define GPIO_TK_POWER EXYNOS4_GPF0(6)

#define ABOV_TK_FW_VERSION 0xA
#define ABOV_TK_FW_CHECKSUM_H 0xA2
#define ABOV_TK_FW_CHECKSUM_L 0x50

/* use FPCB 0.3 from hw rev0.2 board*/
#define ABOV_TK_FW_VERSION_02 0x15
#define ABOV_TK_FW_CHECKSUM_H_02 0xB6
#define ABOV_TK_FW_CHECKSUM_L_02 0xCE

#define ABOV_TK_FW_NAME "abov/abov_tk.fw"
#define ABOV_TK_FW_NAME_02 "abov/abov_tk_r02.fw"

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

int touchkey_power_control_regulator(bool on)
{
	struct regulator *regulator;

	if (abov_key_enabled == on) {
		printk(KERN_ERR"%s command error %d\n", __func__, on);
		return 0;
	}

	printk(KERN_DEBUG "[TK] %s %s\n",
		__func__, on ? "on" : "off");

	regulator = regulator_get(NULL, "sensor_2v8");
	if (IS_ERR(regulator))
		return PTR_ERR(regulator);

	if (on)
		regulator_enable(regulator);
	else
		regulator_disable(regulator);
	regulator_put(regulator);

	abov_key_enabled = on;

	return 0;
}

int touchkey_power_control_gpio(bool on)
{
	if (abov_key_enabled == on) {
		printk(KERN_ERR"%s command error %d\n", __func__, on);
		return 0;
	}

	printk(KERN_DEBUG "[TK] %s %s\n",
		__func__, on ? "on" : "off");

	if (on)
		gpio_direction_output(GPIO_TK_POWER, 1);
	else
		gpio_direction_output(GPIO_TK_POWER, 0);

	abov_key_enabled = on;

	return 0;
}

static struct abov_touchkey_platform_data abov_tk_pdata = {
	.irq_flag = IRQF_TRIGGER_FALLING,
	.gpio_scl = GPIO_TK_SCL,
	.gpio_sda = GPIO_TK_SDA,
	.gpio_rst = GPIO_TK_RST,
	.gpio_int = GPIO_TOUCH_KEY_INT,
	.keyled = key_led_control,
	.fw_name = ABOV_TK_FW_NAME,
	.fw_version = ABOV_TK_FW_VERSION,
	.checksum_h = ABOV_TK_FW_CHECKSUM_H,
	.checksum_l = ABOV_TK_FW_CHECKSUM_L,
};

static struct abov_touchkey_platform_data abov_tk_pdata_02 = {
	.irq_flag = IRQF_TRIGGER_FALLING,
	.gpio_scl = GPIO_TK_SCL,
	.gpio_sda = GPIO_TK_SDA,
	.gpio_rst = GPIO_TK_RST,
	.gpio_int = GPIO_TOUCH_KEY_INT,
	.keyled = key_led_control,
	.fw_name = ABOV_TK_FW_NAME_02,
	.fw_version = ABOV_TK_FW_VERSION_02,
	.checksum_h = ABOV_TK_FW_CHECKSUM_H_02,
	.checksum_l = ABOV_TK_FW_CHECKSUM_L_02,
};

static struct abov_touchkey_platform_data abov_tk_pdata_04 = {
	.irq_flag = IRQF_TRIGGER_FALLING,
	.gpio_scl = GPIO_TK_SCL,
	.gpio_sda = GPIO_TK_SDA,
	.gpio_rst = GPIO_TK_RST_04,
	.gpio_int = GPIO_TOUCH_KEY_INT,
	.keyled = key_led_control,
	.power = touchkey_power_control_regulator,
	.fw_name = ABOV_TK_FW_NAME_02,
	.fw_version = ABOV_TK_FW_VERSION_02,
	.checksum_h = ABOV_TK_FW_CHECKSUM_H_02,
	.checksum_l = ABOV_TK_FW_CHECKSUM_L_02,
};

static struct abov_touchkey_platform_data abov_tk_pdata_05 = {
	.irq_flag = IRQF_TRIGGER_FALLING,
	.gpio_scl = GPIO_TK_SCL,
	.gpio_sda = GPIO_TK_SDA,
	.gpio_int = GPIO_TOUCH_KEY_INT,
	.keyled = key_led_control,
	.power = touchkey_power_control_gpio,
	.fw_name = ABOV_TK_FW_NAME_02,
	.fw_version = ABOV_TK_FW_VERSION_02,
	.checksum_h = ABOV_TK_FW_CHECKSUM_H_02,
	.checksum_l = ABOV_TK_FW_CHECKSUM_L_02,
};

static struct i2c_gpio_platform_data gpio_i2c_data9 = {
	.sda_pin = GPIO_TK_SDA,
	.scl_pin = GPIO_TK_SCL,
	.udelay = 1,
};

struct platform_device s3c_device_i2c9 = {
	.name = "i2c-gpio",
	.id = 9,
	.dev.platform_data = &gpio_i2c_data9,
};

static struct i2c_board_info i2c_devs9_emul[] = {
	{
		I2C_BOARD_INFO(ABOV_TK_NAME, 0x20),
		.platform_data = &abov_tk_pdata,
	},
};

void __init kmini_touchkey_init(void)
{
	int gpio_int, gpio_rst, gpio_pwr;
	int ret;

	printk(KERN_INFO "%s, systme_rev : %d\n", __func__, system_rev);

	gpio_int = GPIO_TOUCH_KEY_INT;
	ret = gpio_request(gpio_int, "TK_INT");
	if (ret)
		pr_err("failed to request gpio(TK_INT)(%d)\n", ret);

	if (system_rev == 5) {
		gpio_rst = GPIO_TK_RST_04;
		abov_tk_pdata_02.gpio_rst = GPIO_TK_RST_04;
	} else
		gpio_rst = GPIO_TK_RST;

	if (system_rev >= 6) {
		gpio_pwr = GPIO_TK_POWER;
		ret = gpio_request(gpio_pwr, "TK_PWR");
		if (ret)
			pr_err("failed to request gpio(TK_PWR)(%d)\n", ret);
	} else {
		ret = gpio_request(gpio_rst, "TK_RST");
		if (ret)
			pr_err("failed to request gpio(TK_RST)(%d)\n", ret);
	}

	if (lpcharge) {
		printk(KERN_DEBUG "%s : lpcharge. touchkey driver unload\n", __func__);
		s3c_gpio_cfgpin(gpio_int, S3C_GPIO_INPUT);
		s3c_gpio_setpull(gpio_int, S3C_GPIO_PULL_DOWN);
		if (system_rev >= 6) {
			s3c_gpio_cfgpin(gpio_pwr, S3C_GPIO_OUTPUT);
			s3c_gpio_setpull(gpio_pwr, S3C_GPIO_PULL_NONE);
			gpio_direction_output(gpio_pwr, 0);
		} else {
			s3c_gpio_cfgpin(gpio_rst, S3C_GPIO_OUTPUT);
			s3c_gpio_setpull(gpio_rst, S3C_GPIO_PULL_NONE);
			gpio_direction_output(gpio_rst, 1);
		}
		return;
	}

#ifdef CONFIG_INPUT_TOUCHSCREEN
	if (tsp_connect == false) {
		printk(KERN_ERR "%s tsp_connect : %d\n", __func__, tsp_connect);
		s3c_gpio_cfgpin(gpio_int, S3C_GPIO_INPUT);
		s3c_gpio_setpull(gpio_int, S3C_GPIO_PULL_DOWN);
		if (system_rev >= 6) {
			s3c_gpio_cfgpin(gpio_pwr, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio_pwr, S3C_GPIO_PULL_DOWN);
		} else {
			s3c_gpio_cfgpin(gpio_rst, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio_rst, S3C_GPIO_PULL_DOWN);
		}
		return;
	}
#endif

	s3c_gpio_cfgpin(gpio_int, S3C_GPIO_INPUT);
	if (system_rev >= 1)
		s3c_gpio_setpull(gpio_int, S3C_GPIO_PULL_UP);
	else
		s3c_gpio_setpull(gpio_int, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(gpio_int);

	i2c_devs9_emul[0].irq = gpio_to_irq(gpio_int);
	printk(KERN_ERR "%s touch : %d \n", __func__,
		i2c_devs9_emul[0].irq);

	if (system_rev >= 6) {
		s3c_gpio_cfgpin(gpio_pwr, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(gpio_pwr, S3C_GPIO_PULL_NONE);
		gpio_direction_output(gpio_pwr, 0);
	} else {
		s3c_gpio_cfgpin(gpio_rst, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(gpio_rst, S3C_GPIO_PULL_NONE);
		gpio_direction_output(gpio_rst, 1);
	}

	if (system_rev >= 6)
		i2c_devs9_emul[0].platform_data = &abov_tk_pdata_05;
	else if (system_rev == 5)
		i2c_devs9_emul[0].platform_data = &abov_tk_pdata_04;
	else if ((system_rev < 5) && (system_rev >= 2))
		i2c_devs9_emul[0].platform_data = &abov_tk_pdata_02;

	i2c_register_board_info(9, i2c_devs9_emul,
		ARRAY_SIZE(i2c_devs9_emul));
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

static struct gpio_keys_platform_data smdk4270_gpiokeys_platform_data = {
	smdk4270_button,
	ARRAY_SIZE(smdk4270_button),
#ifdef CONFIG_SENSORS_HALL
	.gpio_flip_cover = GPIO_HALL_SENSOR_INT,
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
#if defined(CONFIG_KEYBOARD_ABOV_TOUCH)
	&s3c_device_i2c9,
#endif
};

void __init exynos4_smdk4270_input_init(void)
{
	smdk4270_gpio_keys_config_setup();
#if defined(CONFIG_INPUT_TOUCHSCREEN)
	kmini_tsp_init();
#endif
#if defined(CONFIG_KEYBOARD_ABOV_TOUCH)
	kmini_touchkey_init();
#endif
#ifdef CONFIG_SENSORS_HALL
		s3c_gpio_setpull(GPIO_HALL_SENSOR_INT, S3C_GPIO_PULL_NONE);
		gpio_request(GPIO_HALL_SENSOR_INT, "GPIO_HALL_SENSOR_INT");
		s3c_gpio_cfgpin(GPIO_HALL_SENSOR_INT, S3C_GPIO_SFN(0xf));
		s5p_register_gpio_interrupt(GPIO_HALL_SENSOR_INT);
		gpio_direction_input(GPIO_HALL_SENSOR_INT);
#endif

	platform_add_devices(smdk4270_input_devices,
		ARRAY_SIZE(smdk4270_input_devices));
}
