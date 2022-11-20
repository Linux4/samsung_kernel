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
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c/touchkey_i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/export.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>

#include <mach/irqs.h>
#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>
#include <mach/gpio-exynos.h>

#include "board-universal5260.h"
#include <mach/sec_debug.h>
#ifdef CONFIG_TOUCHSCREEN_FTS
#include <linux/i2c/fts.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_M2
#include <linux/platform_data/mms152_ts.h>
#endif
#ifdef CONFIG_INPUT_WACOM
#include <linux/wacom_i2c.h>
#endif
#ifdef CONFIG_KEYBOARD_TC370L_TOUCHKEY
#include <linux/i2c/tc370l_touchkey.h>
#endif
#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_ZINITIX
#include <linux/i2c/zinitix.h>
#endif

extern unsigned int system_rev;

static struct gpio_keys_button universal5260_button[] = {
	{
		.code = KEY_POWER,
		.gpio = GPIO_POWER_BUTTON,
		.desc = "gpio-keys: KEY_POWER",
		.active_low = 1,
		.wakeup = 1,
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
	}, {
		.code = KEY_VOLUMEDOWN,
		.gpio = GPIO_VOLDOWN_BUTTON,
		.desc = "gpio-keys: KEY_VOLUMEDOWN",
		.active_low = 1,
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
	}, {
		.code = KEY_VOLUMEUP,
		.gpio = GPIO_VOLUP_BUTTON,
		.desc = "gpio-keys: KEY_VOLUMEUP",
		.active_low = 1,
#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
		.wakeup = 1,
#endif
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
	}, {
		.code = KEY_HOMEPAGE,
		.gpio = GPIO_HOME_BUTTON,
		.desc = "gpio-keys: KEY_HOMEPAGE",
		.active_low = 1,
		.wakeup = 1,
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
	},
#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
		{
		.code = KEY_CAMERA_FOCUS,
		.gpio = GPIO_KEY_S1,
		.desc = "gpio-keys: KEY_CAMERA_FOCUS",
		.active_low = 1,
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
	}, {
		.code = KEY_CAMERA,
		.gpio = GPIO_KEY_S2,
		.desc = "gpio-keys: KEY_CAMERA_SHUTTER",
		.active_low = 1,
		.isr_hook = sec_debug_check_crash_key,
		.debounce_interval = 10,
	},
#endif
};
#ifdef CONFIG_TOUCHSCREEN_FTS
struct fts_callbacks *fts_charger_callbacks;
void fts_charger_infom(bool en)
{
	if (fts_charger_callbacks && fts_charger_callbacks->inform_charger)
		fts_charger_callbacks->inform_charger(fts_charger_callbacks, en);
}
static void fts_tsp_register_callback(void *cb)
{
	fts_charger_callbacks = cb;
}

static int fts_power(bool on)
{
	struct regulator *regulator_avdd;
	static bool enabled;

	if (enabled == on)
		return 0;

	regulator_avdd = regulator_get(NULL, "tsp_avdd_3.3v");
	if (IS_ERR(regulator_avdd)) {
		printk(KERN_ERR "[TSP] tsp_avdd_3.3v regulator_get failed\n");
		return PTR_ERR(regulator_avdd);
	}

	printk(KERN_ERR "[TSP] %s %s\n", __func__, on ? "on" : "off");

	if (on) {
		gpio_direction_output(GPIO_TSP_POWER, 1);
		regulator_enable(regulator_avdd);

		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
		s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_NONE);
		s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_NONE);
	} else {
		gpio_direction_output(GPIO_TSP_POWER, 0);
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);

		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_DOWN);
		s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_DOWN);
		s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_DOWN);
	}

	enabled = on;
	regulator_put(regulator_avdd);

	return 0;
}

#define PROJECT_H_LITE	"N750X"
#define H_LITE_FW		"tsp_stm/stm.fw"
#define H_LITE_FW_DE	"tsp_stm/stm_de.fw"

static struct fts_i2c_platform_data fts_platformdata = {
#ifdef CONFIG_TOUCHSCREEN_FACTORY_PLATFORM
	.factory_flatform = true,
#else
	.factory_flatform = false,
#endif
	.max_x = 720,
	.max_y = 1280,
	.max_width = 28,
	.support_hover = true,
	.support_mshover = true,
	.firmware_name = H_LITE_FW_DE,
	.power = fts_power,
	.irq_type = IRQF_TRIGGER_LOW | IRQF_ONESHOT,
	.gpio = GPIO_TSP_INT,
	.register_cb = fts_tsp_register_callback,
	.project_name = PROJECT_H_LITE,
};

static struct i2c_board_info fts_i2c_devs[] = {
	{
		I2C_BOARD_INFO("fts", 0x49),
		.platform_data = &fts_platformdata,
	}
};

static void fts_verify_panel_revision(void)
{
	u8 touch_type = (lcdtype & 0xF000) >> 12;

	fts_platformdata.panel_revision = touch_type;
	fts_platformdata.enable_sync = NULL;

	if (fts_platformdata.panel_revision == 0)
		fts_platformdata.firmware_name = H_LITE_FW;

	printk(KERN_ERR "%s: panel_revision: %d, lcdtype: 0x%06X, touch_type: %d, h_sync: %s\n",
		__func__, fts_platformdata.panel_revision,
		lcdtype, touch_type, !fts_platformdata.enable_sync ? "NA" : "OK");
};

void __init fts_tsp_init(void)
{
	int gpio;
	int ret;

	gpio = GPIO_TSP_POWER;
	ret = gpio_request_one(gpio, GPIOF_OUT_INIT_LOW, "GPIO_POWER");
	if (ret)
		printk(KERN_ERR "%s failed to request gpio(TSP_POWER)\n",
			__func__);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);

	gpio_request(GPIO_TSP_INT, "TSP_INT");
	s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_UP);

	fts_i2c_devs[0].irq = IRQ_EINT(14);
	printk(KERN_ERR "%s touch : %d\n", __func__, fts_i2c_devs[0].irq);

	fts_verify_panel_revision();
}
#endif
#ifdef CONFIG_TOUCHSCREEN_ZINITIX
static int zinitix_power(bool on)
{
	struct regulator *regulator_avdd;
	static bool enabled;

	if (enabled == on)
		return 0;

	regulator_avdd = regulator_get(NULL, "tsp_avdd_3.3v");
	if (IS_ERR(regulator_avdd)) {
		printk(KERN_ERR "[TSP] tsp_avdd_3.3v regulator_get failed\n");
		return PTR_ERR(regulator_avdd);
	}

	printk(KERN_ERR "[TSP] %s %s\n", __func__, on ? "on" : "off");

	if (on) {
		gpio_direction_output(GPIO_TSP_POWER, 1);
		regulator_enable(regulator_avdd);

		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
	} else {
		gpio_direction_output(GPIO_TSP_POWER, 0);
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);

		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_DOWN);
	}

	enabled = on;
	regulator_put(regulator_avdd);

	return 0;
}

#define PROJECT_MEGA2	"MEGA2"

static struct bt532_ts_platform_data zinitix_platformdata = {
	.x_resolution	= 720,
	.y_resolution	= 1280,
	.page_size	= 128,
	.orientation	= 0,
	.gpio_int		= GPIO_TSP_INT,
	.power		= zinitix_power,
	.project_name	= PROJECT_MEGA2,
};

static struct i2c_board_info zinitix_i2c_devs[] = {
	{
		I2C_BOARD_INFO(ZINITIX_NAME, 0x20),
		.platform_data = &zinitix_platformdata,
	}
};

void __init zinitix_tsp_init(void)
{
	int gpio;
	int ret;

	gpio = GPIO_TSP_POWER;
	ret = gpio_request_one(gpio, GPIOF_OUT_INIT_LOW, "GPIO_POWER");
	if (ret)
		printk(KERN_ERR "%s failed to request gpio(TSP_POWER)\n",
			__func__);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);

	gpio_request(GPIO_TSP_INT, "TSP_INT");
	s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_UP);

	zinitix_i2c_devs[0].irq = IRQ_EINT(14);
	printk(KERN_ERR "%s touch : %d\n", __func__, zinitix_i2c_devs[0].irq);
}
#endif

#ifdef CONFIG_TOUCHSCREEN_MELFAS_M2
static struct i2c_board_info i2c_devs5_emul[];
/* MELFAS TSP */
static bool enabled;
int TSP_VDD_18V(bool on)
{
	struct regulator *regulator;

	if (enabled == on)
		return 0;

	regulator = regulator_get(NULL, "touch_1.8v");
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
	struct regulator *regulator_vdd;
	struct regulator *regulator_avdd;

	if (enabled == on)
		return 0;

	regulator_vdd = regulator_get(NULL, "touch_1.8v");
	if (IS_ERR(regulator_vdd))
			return PTR_ERR(regulator_vdd);

	regulator_avdd = regulator_get(NULL, "tsp_avdd_3.3v");
	if (IS_ERR(regulator_avdd))
		return PTR_ERR(regulator_avdd);

	printk(KERN_DEBUG "[TSP] %s %s\n", __func__, on ? "on" : "off");

	if (on) {
		regulator_enable(regulator_vdd);
		regulator_enable(regulator_avdd);

		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
	} else {
		/*
		 * TODO: If there is a case the regulator must be disabled
		 * (e,g firmware update?), consider regulator_force_disable.
		 */
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);

		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_DOWN);
	}

	enabled = on;
	regulator_put(regulator_vdd);
	regulator_put(regulator_avdd);

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
		s3c_gpio_cfgpin(GPIO_TSP_SCL_18V, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_NONE);

		gpio_direction_output(GPIO_TSP_SDA_18V, 1);
		gpio_direction_input(GPIO_TSP_SDA_18V);
		s3c_gpio_cfgpin(GPIO_TSP_SDA_18V, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_NONE);
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
	.max_x = 720,
	.max_y = 1280,
	.invert_x = 0,
	.invert_y = 0,
	.gpio_int = GPIO_TSP_INT,
	.gpio_scl = GPIO_TSP_SCL_18V,
	.gpio_sda = GPIO_TSP_SDA_18V,
	.power = melfas_power,
	.mux_fw_flash = melfas_mux_fw_flash,
	.is_vdd_on = is_melfas_vdd_on,
	.config_fw_version = "C115_Me_0121",
	.lcd_type = melfas_get_lcdtype,
	.register_cb = melfas_register_callback,
#ifdef CONFIG_LCD_FREQ_SWITCH
	.register_lcd_cb = melfas_register_lcd_callback,
#endif
};

static struct i2c_board_info i2c_devs5[] = {
	{
	 I2C_BOARD_INFO(MELFAS_TS_NAME, 0x48),
	 .platform_data = &mms_ts_pdata},
};

void __init midas_tsp_init(void)
{
	int gpio;
	int ret;
	printk(KERN_INFO "[TSP] midas_tsp_init() is called\n");

	/* TSP_INT: XEINT_4 */
	gpio = GPIO_TSP_INT;
	ret = gpio_request(gpio, "TSP_INT");
	if (ret)
		pr_err("failed to request gpio(TSP_INT)\n");
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);

	if (gpio_request(GPIO_TSP_SCL_18V, "GPIO_TSP_SCL"))
		pr_err("failed to request gpio(GPIO_TSP_SCL)\n");

	if (gpio_request(GPIO_TSP_SDA_18V, "GPIO_TSP_SDA"))
		pr_err("failed to request gpio(GPIO_TSP_SDA)\n");

	s5p_register_gpio_interrupt(gpio);
	i2c_devs5[0].irq = gpio_to_irq(gpio);

	printk(KERN_INFO "%s touch : %d\n", __func__, i2c_devs5[0].irq);

}
#endif /* CONFIG_TOUCHSCREEN_MELFAS_M2 */

#ifdef CONFIG_KEYBOARD_CYPRESS_TOUCH
static struct i2c_board_info i2c_devs10_emul[];

static void touchkey_init_hw(void)
{
#ifndef LED_LDO_WITH_REGULATOR
	gpio_request(GPIO_3_TOUCH_EN, "gpio_3_touch_en");
#endif
#ifndef LDO_WITH_REGULATOR
	gpio_request_one(GPIO_VTOUCH_LDO_EN, GPIOF_OUT_INIT_LOW, "gpio_vtouch_ldo_en");
	s3c_gpio_setpull(GPIO_VTOUCH_LDO_EN, S3C_GPIO_PULL_DOWN);
#endif
	gpio_request(GPIO_2TOUCH_INT, "2_TOUCH_INT");
	s3c_gpio_setpull(GPIO_2TOUCH_INT, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(GPIO_2TOUCH_INT);
	gpio_direction_input(GPIO_2TOUCH_INT);

	i2c_devs10_emul[0].irq = gpio_to_irq(GPIO_2TOUCH_INT);
	irq_set_irq_type(gpio_to_irq(GPIO_2TOUCH_INT), IRQF_TRIGGER_FALLING);
	s3c_gpio_cfgpin(GPIO_2TOUCH_INT, S3C_GPIO_SFN(0xf));

#ifdef GPIO_2TOUCH_RST
	gpio_request(GPIO_2TOUCH_RST, "2_TOUCH_RST");
	gpio_direction_output(GPIO_2TOUCH_RST, 0);
	s3c_gpio_setpull(GPIO_2TOUCH_RST, S3C_GPIO_PULL_NONE);
#endif
#ifdef GPIO_2_TOUCH_ID
	gpio_request(GPIO_2_TOUCH_ID, "2_TOUCH_ID");
	gpio_direction_input(GPIO_2_TOUCH_ID);
	s3c_gpio_setpull(GPIO_2_TOUCH_ID, S3C_GPIO_PULL_NONE);
#endif

	s3c_gpio_setpull(GPIO_2TOUCH_SCL, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(GPIO_2TOUCH_SDA, S3C_GPIO_PULL_DOWN);
}

static int touchkey_suspend(void)
{
#ifdef LDO_WITH_REGULATOR
	struct regulator *regulator;

	regulator = regulator_get(NULL, TK_REGULATOR_NAME);
	if (IS_ERR(regulator)) {
		printk(KERN_ERR
		"[Touchkey] touchkey_suspend : TK regulator_get failed\n");
		return -EIO;
	}

	if (regulator_is_enabled(regulator))
		regulator_disable(regulator);

	regulator_put(regulator);
#else
	gpio_direction_output(GPIO_VTOUCH_LDO_EN, 0);
#endif
	s3c_gpio_setpull(GPIO_2TOUCH_SCL, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(GPIO_2TOUCH_SDA, S3C_GPIO_PULL_DOWN);

	return 1;
}

static int touchkey_resume(void)
{
#ifdef LDO_WITH_REGULATOR
	struct regulator *regulator;

	regulator = regulator_get(NULL, TK_REGULATOR_NAME);
	if (IS_ERR(regulator)) {
		printk(KERN_ERR
		"[Touchkey] touchkey_resume : TK regulator_get failed\n");
		return -EIO;
	}

	regulator_enable(regulator);
	regulator_put(regulator);
#else
	gpio_direction_output(GPIO_VTOUCH_LDO_EN, 1);
#endif
	s3c_gpio_setpull(GPIO_2TOUCH_SCL, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_2TOUCH_SDA, S3C_GPIO_PULL_NONE);
#ifdef GPIO_2TOUCH_RST
	s3c_gpio_setpull(GPIO_2TOUCH_RST, S3C_GPIO_PULL_NONE);
#endif

	return 1;
}

static int touchkey_power_on(bool on)
{
	int ret;

	if (on) {
		gpio_direction_output(GPIO_2TOUCH_INT, 1);
		irq_set_irq_type(gpio_to_irq(GPIO_2TOUCH_INT),
			IRQF_TRIGGER_FALLING);
		s3c_gpio_cfgpin(GPIO_2TOUCH_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_2TOUCH_INT, S3C_GPIO_PULL_NONE);
	} else {
		gpio_direction_input(GPIO_2TOUCH_INT);
#ifdef GPIO_2TOUCH_RST
		gpio_direction_output(GPIO_2TOUCH_RST, 0);
		s3c_gpio_setpull(GPIO_2TOUCH_RST, S3C_GPIO_PULL_DOWN);
#endif
#ifdef GPIO_2_TOUCH_ID
		s3c_gpio_setpull(GPIO_2_TOUCH_ID, S3C_GPIO_PULL_DOWN);
#endif
	}

	if (on)
		ret = touchkey_resume();
	else
		ret = touchkey_suspend();

	return ret;
}

static int touchkey_led_power_on(bool on)
{
#ifdef LED_LDO_WITH_REGULATOR
	struct regulator *regulator;

	regulator = regulator_get(NULL, TK_LED_REGULATOR_NAME);
	if (IS_ERR(regulator)) {
		printk(KERN_ERR
		"[Touchkey] touchkey_led_power_on : TK_LED regulator_get failed\n");
		return -EIO;
	}

	if (on) {
		regulator_enable(regulator);
	} else {
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
	}
	regulator_put(regulator);
#else
	if (on)
		gpio_direction_output(GPIO_3_TOUCH_EN, 1);
	else
		gpio_direction_output(GPIO_3_TOUCH_EN, 0);
#endif
	return 1;
}

#ifdef GPIO_2TOUCH_RST
static int touchkey_rst_reset(void)
{
	gpio_direction_output(GPIO_2TOUCH_RST, 1);
	usleep_range(1000, 1000);
	gpio_direction_output(GPIO_2TOUCH_RST, 0);
	
	return 0;
}
#endif

static struct touchkey_platform_data touchkey_pdata = {
	.gpio_sda = GPIO_2TOUCH_SDA,
	.gpio_scl = GPIO_2TOUCH_SCL,
	.gpio_int = GPIO_2TOUCH_INT,
	.init_platform_hw = touchkey_init_hw,
	.suspend = touchkey_suspend,
	.resume = touchkey_resume,
	.power_on = touchkey_power_on,
	.led_power_on = touchkey_led_power_on,
/*	.register_cb = touchkey_register_callback, */
#ifdef GPIO_2TOUCH_RST
	.gpio_rst = GPIO_2TOUCH_RST,
	.rst_reset = touchkey_rst_reset,
#endif
};

/* I2C10 */
static struct i2c_board_info i2c_devs10_emul[] = {
	{
		I2C_BOARD_INFO("sec_touchkey", 0x20),
		.platform_data = &touchkey_pdata,
	},
};
#endif /*CONFIG_KEYBOARD_CYPRESS_TOUCH*/

#ifdef CONFIG_KEYBOARD_TC370L_TOUCHKEY
static bool tc370_power_enabled;
static bool tc370_keyled_enabled;

int tc370_keycode[] = { 0,
	KEY_RECENT, KEY_BACK
};

int touchkey_power(bool on)
{
	struct regulator *regulator;

	if (tc370_power_enabled == on)
		return 0;

	printk(KERN_DEBUG "[TK] %s %s\n",
		__func__, on ? "on" : "off");

	regulator = regulator_get(NULL, "vtouch_1.8v");
	if (IS_ERR(regulator))
		return PTR_ERR(regulator);
	if (on) {
		regulator_enable(regulator);
		s3c_gpio_setpull(GPIO_2TOUCH_SCL, S3C_GPIO_PULL_NONE);
		s3c_gpio_setpull(GPIO_2TOUCH_SDA, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_2TOUCH_INT, S3C_GPIO_SFN(0xf));
		if (system_rev == 2 || system_rev == 3)
			s3c_gpio_setpull(GPIO_2TOUCH_INT, S3C_GPIO_PULL_UP);
		else
			s3c_gpio_setpull(GPIO_2TOUCH_INT, S3C_GPIO_PULL_NONE);
	} else {
		s3c_gpio_cfgpin(GPIO_2TOUCH_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_2TOUCH_INT, S3C_GPIO_PULL_DOWN);
		s3c_gpio_setpull(GPIO_2TOUCH_SCL, S3C_GPIO_PULL_DOWN);
		s3c_gpio_setpull(GPIO_2TOUCH_SDA, S3C_GPIO_PULL_DOWN);
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
		else
			regulator_force_disable(regulator);
	}
	regulator_put(regulator);

	tc370_power_enabled = on;

	return 0;
}

int touchkey_power_isp(bool on)
{
	struct regulator *regulator;

	if (tc370_power_enabled == on)
		return 0;

	printk(KERN_DEBUG "[TK] %s %s\n",
		__func__, on ? "on" : "off");

	regulator = regulator_get(NULL, "vtouch_1.8v");
	if (IS_ERR(regulator))
		return PTR_ERR(regulator);
	if (on) {
		regulator_enable(regulator);
		s3c_gpio_setpull(GPIO_2TOUCH_SCL, S3C_GPIO_PULL_DOWN);
		s3c_gpio_setpull(GPIO_2TOUCH_SDA, S3C_GPIO_PULL_DOWN);
		s3c_gpio_setpull(GPIO_2TOUCH_INT, S3C_GPIO_PULL_DOWN);
	} else {
		s3c_gpio_cfgpin(GPIO_2TOUCH_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_2TOUCH_INT, S3C_GPIO_PULL_DOWN);
		s3c_gpio_setpull(GPIO_2TOUCH_SCL, S3C_GPIO_PULL_DOWN);
		s3c_gpio_setpull(GPIO_2TOUCH_SDA, S3C_GPIO_PULL_DOWN);
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
		else
			regulator_force_disable(regulator);
	}
	regulator_put(regulator);

	tc370_power_enabled = on;

	return 0;
}

int key_led_control(bool on)
{
	struct regulator *regulator;

	if (tc370_keyled_enabled == on)
		return 0;

	printk(KERN_INFO "[TK] %s %s\n",
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

	tc370_keyled_enabled = on;

	return 0;
}

static struct tc370_platform_data tc370_touchkey_pdata = {
	.gpio_int = GPIO_2TOUCH_INT,
	.gpio_sda = GPIO_2TOUCH_SDA,
	.gpio_scl = GPIO_2TOUCH_SCL,
	.key_num = ARRAY_SIZE(tc370_keycode),
	.keycode = tc370_keycode,
	.power = touchkey_power,
	.power_isp = touchkey_power_isp,
	.keyled = key_led_control,
	.fw_name = "coreriver/tc370_m2.fw",
};

static struct i2c_board_info touchkey_i2c_data[] = {
	{
		I2C_BOARD_INFO(TC370_NAME, 0x20),
		.platform_data = &tc370_touchkey_pdata,
	},
};

extern unsigned int lcdtype;

static void tc370_touchkey_init(void)
{
	gpio_request(GPIO_2TOUCH_INT, "TOUCH_INT");
	s3c_gpio_setpull(GPIO_2TOUCH_INT, S3C_GPIO_PULL_UP);
	s5p_register_gpio_interrupt(GPIO_2TOUCH_INT);
	gpio_direction_input(GPIO_2TOUCH_INT);

	touchkey_i2c_data[0].irq = gpio_to_irq(GPIO_2TOUCH_INT);
	irq_set_irq_type(gpio_to_irq(GPIO_2TOUCH_INT), IRQF_TRIGGER_FALLING);
	s3c_gpio_cfgpin(GPIO_2TOUCH_INT, S3C_GPIO_SFN(0xf));

	printk(KERN_ERR "%s touchkey : %d\n",
		__func__, touchkey_i2c_data[0].irq);

	s3c_gpio_setpull(GPIO_2TOUCH_SCL, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(GPIO_2TOUCH_SDA, S3C_GPIO_PULL_DOWN);

	if (lcdtype != 0)
		tc370_touchkey_pdata.panel_connect = true;
	else
		tc370_touchkey_pdata.panel_connect = false;

}
#endif /* CONFIG_KEYBOARD_TC370L_TOUCHKEY */

#if defined(CONFIG_KEYBOARD_CYPRESS_TOUCH) || defined(CONFIG_KEYBOARD_TC370L_TOUCHKEY)
static struct i2c_gpio_platform_data gpio_i2c_data10 = {
	.sda_pin = GPIO_2TOUCH_SDA,
	.scl_pin = GPIO_2TOUCH_SCL,
	.udelay = 1,
};

struct platform_device s3c_device_i2c10 = {
	.name = "i2c-gpio",
	.id = 10,
	.dev.platform_data = &gpio_i2c_data10,
};

static void touchkey_init(void)
{
	printk(KERN_INFO"%s, system_rev : %d\n", __func__, system_rev);

#if defined(CONFIG_KEYBOARD_TC370L_TOUCHKEY)
	tc370_touchkey_init();
	i2c_register_board_info(10, touchkey_i2c_data,
		ARRAY_SIZE(touchkey_i2c_data));
#endif

#if defined(CONFIG_KEYBOARD_CYPRESS_TOUCH)
	touchkey_init_hw();
	i2c_register_board_info(10, i2c_devs10_emul,
		ARRAY_SIZE(i2c_devs10_emul));
#endif
}
#endif

#ifdef CONFIG_INPUT_WACOM
static struct wacom_g5_callbacks *wacom_callbacks;
static bool wacom_power_enabled;

int wacom_power(bool on)
{
#ifdef GPIO_PEN_LDO_EN
	gpio_direction_output(GPIO_PEN_LDO_EN, on);
#else
	struct regulator *regulator_vdd;

	if (wacom_power_enabled == on)
		return 0;

	regulator_vdd = regulator_get(NULL, "wacom_3.0v");
	if (IS_ERR(regulator_vdd)) {
		printk(KERN_ERR"epen: %s reg get err\n", __func__);
		return PTR_ERR(regulator_vdd);
	}

	if (on) {
		regulator_enable(regulator_vdd);
	} else {
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
	}
	regulator_put(regulator_vdd);

	wacom_power_enabled = on;
#endif
	return 0;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static int wacom_early_suspend_hw(void)
{
#ifdef GPIO_PEN_RESET_N_18V
	gpio_direction_output(GPIO_PEN_RESET_N_18V, 0);
#endif
	wacom_power(0);

	return 0;
}

static int wacom_late_resume_hw(void)
{
#ifdef GPIO_PEN_RESET_N_18V
	gpio_direction_output(GPIO_PEN_RESET_N_18V, 1);
#endif
	gpio_direction_output(GPIO_PEN_PDCT_18V, 1);
	wacom_power(1);
	msleep(100);
	gpio_direction_input(GPIO_PEN_PDCT_18V);
	return 0;
}
#endif

static int wacom_suspend_hw(void)
{
#ifdef GPIO_PEN_RESET_N_18V
	gpio_direction_output(GPIO_PEN_RESET_N_18V, 0);
	s3c_gpio_setpull(GPIO_PEN_RESET_N_18V, S3C_GPIO_PULL_DOWN);
#endif
	wacom_power(0);

	s3c_gpio_setpull(GPIO_PEN_PDCT_18V, S3C_GPIO_PULL_DOWN);

	s3c_gpio_cfgpin(GPIO_PEN_IRQ_18V, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_PEN_IRQ_18V, S3C_GPIO_PULL_DOWN);

	return 0;
}

static int wacom_resume_hw(void)
{
	gpio_direction_output(GPIO_PEN_PDCT_18V, 1);
	wacom_power(1);
	msleep(100);
#ifdef GPIO_PEN_RESET_N_18V
	gpio_direction_output(GPIO_PEN_RESET_N_18V, 1);
	s3c_gpio_setpull(GPIO_PEN_RESET_N_18V, S3C_GPIO_PULL_UP);
#endif
	gpio_direction_input(GPIO_PEN_PDCT_18V);
	s3c_gpio_setpull(GPIO_PEN_PDCT_18V, S3C_GPIO_PULL_UP);

	s3c_gpio_cfgpin(GPIO_PEN_IRQ_18V, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_PEN_IRQ_18V, S3C_GPIO_PULL_NONE);

	return 0;
}

static int wacom_reset_hw(void)
{
	wacom_suspend_hw();
	msleep(100);
	wacom_resume_hw();

	return 0;
}

static void wacom_register_callbacks(struct wacom_g5_callbacks *cb)
{
	wacom_callbacks = cb;
};

static void wacom_compulsory_flash_mode(bool en)
{
	gpio_direction_output(GPIO_PEN_FWE1_18V, en);
}

static int wacom_get_irq_state(void)
{
	return gpio_get_value(GPIO_PEN_IRQ_18V);
}

static struct wacom_g5_platform_data wacom_platform_data = {
	.x_invert = WACOM_X_INVERT,
	.y_invert = WACOM_Y_INVERT,
	.xy_switch = WACOM_XY_SWITCH,
	.min_x = 0,
	.max_x = WACOM_MAX_COORD_X,
	.min_y = 0,
	.max_y = WACOM_MAX_COORD_Y,
	.min_pressure = 0,
	.max_pressure = WACOM_MAX_PRESSURE,
	.gpio_pendct = GPIO_PEN_PDCT_18V,
	/*.init_platform_hw = wacom_init,*/
	/*      .exit_platform_hw =,    */
	.suspend_platform_hw = wacom_suspend_hw,
	.resume_platform_hw = wacom_resume_hw,
#ifdef CONFIG_HAS_EARLYSUSPEND
	.early_suspend_platform_hw = wacom_early_suspend_hw,
	.late_resume_platform_hw = wacom_late_resume_hw,
#endif
	.reset_platform_hw = wacom_reset_hw,
	.register_cb = wacom_register_callbacks,
	.compulsory_flash_mode = wacom_compulsory_flash_mode,
	.gpio_pen_insert = GPIO_WACOM_SENSE,
	.get_irq_state = wacom_get_irq_state,
};

/* I2C */
static struct i2c_board_info wacom_i2c_devs[] __initdata = {
	{
		I2C_BOARD_INFO("wacom_g5sp_i2c", WACOM_I2C_ADDR),
		.platform_data = &wacom_platform_data,
	},
};

#define WACOM_SET_I2C(ch, pdata, i2c_info)	\
	do {		\
	s3c_i2c##ch##_set_platdata(pdata);	\
	i2c_register_board_info(ch, i2c_info,	\
	ARRAY_SIZE(i2c_info));	\
	platform_device_register(&s3c_device_i2c##ch);	\
	} while (0);

void __init wacom_init(void)
{
	int gpio;
	int ret;

#ifdef GPIO_PEN_RESET_N_18V
	/*Reset*/
	gpio = GPIO_PEN_RESET_N_18V;
	ret = gpio_request(gpio, "PEN_RESET_N");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_RESET_N(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0x1));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);
	gpio_direction_output(gpio, 0);
#endif

	/*SLP & FWE1*/
	gpio = GPIO_PEN_FWE1_18V;
	ret = gpio_request(gpio, "PEN_FWE1");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_FWE1(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0x1));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);
	gpio_direction_output(gpio, 0);

	/*PDCT*/
	gpio = GPIO_PEN_PDCT_18V;
	ret = gpio_request(gpio, "PEN_PDCT");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_PDCT(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	s5p_register_gpio_interrupt(gpio);
	gpio_direction_input(gpio);

	irq_set_irq_type(gpio_to_irq(gpio), IRQ_TYPE_EDGE_BOTH);

	/*IRQ*/
	gpio = GPIO_PEN_IRQ_18V;
	ret = gpio_request(gpio, "PEN_IRQ");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_IRQ(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_register_gpio_interrupt(gpio);
	gpio_direction_input(gpio);

	wacom_i2c_devs[0].irq = gpio_to_irq(gpio);
	irq_set_irq_type(wacom_i2c_devs[0].irq, IRQ_TYPE_EDGE_RISING);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));

	/*LDO_EN*/
#ifdef GPIO_PEN_LDO_EN
	gpio = GPIO_PEN_LDO_EN;
	ret = gpio_request(gpio, "PEN_LDO_EN");
	if (ret) {
		printk(KERN_ERR "epen:failed to request PEN_LDO_EN.(%d)\n",
			ret);
		return ;
	}
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	gpio_direction_output(gpio, 0);
#else
	wacom_power(0);
#endif

	if (lpcharge)
		printk(KERN_DEBUG"epen:disable wacom for lpm\n");
	else {
		s3c_i2c2_set_platdata(NULL);
		i2c_register_board_info(6, wacom_i2c_devs, ARRAY_SIZE(wacom_i2c_devs));
	}

	printk(KERN_INFO "epen:init\n");
}
#endif

static struct gpio_keys_platform_data universal5260_gpiokeys_platform_data = {
	universal5260_button,
	ARRAY_SIZE(universal5260_button),
#ifdef CONFIG_SENSORS_HALL
	.gpio_flip_cover = GPIO_HALL_SENSOR_INT,
#endif
};


static struct platform_device universal5260_gpio_keys = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data = &universal5260_gpiokeys_platform_data,
	},
};

#ifdef CONFIG_INPUT_BOOSTER
static enum booster_device_type get_booster_device(int code)
{
	switch (code) {
	case KEY_HOMEPAGE:
		return BOOSTER_DEVICE_KEY;
	break;
	case KEY_MENU:
	case KEY_BACK:
		return BOOSTER_DEVICE_TOUCHKEY;
	break;
	case KEY_BOOSTER_TOUCH:
		return BOOSTER_DEVICE_TOUCH;
	break;
	case KEY_BOOSTER_PEN:
		return BOOSTER_DEVICE_PEN;
	break;
	default:
		return BOOSTER_DEVICE_NOT_DEFINED;
	break;
	}
}

static const struct dvfs_freq key_freq_table[BOOSTER_LEVEL_MAX] = {
	[BOOSTER_LEVEL1] = BOOSTER_DVFS_FREQ(1300000,	543000,	266000),
};

static const struct dvfs_freq touchkey_freq_table[BOOSTER_LEVEL_MAX] = {
	[BOOSTER_LEVEL1] = BOOSTER_DVFS_FREQ(1300000,	543000,	266000),
	[BOOSTER_LEVEL2] = BOOSTER_DVFS_FREQ(1300000,	543000,	160000),
};

static const struct dvfs_freq touch_freq_table[BOOSTER_LEVEL_MAX] = {
	[BOOSTER_LEVEL1] = BOOSTER_DVFS_FREQ(1300000,	543000,	266000),
	[BOOSTER_LEVEL2] = BOOSTER_DVFS_FREQ(1300000,	543000,	160000),
	[BOOSTER_LEVEL3] = BOOSTER_DVFS_FREQ(1300000,	206000,	160000),
	[BOOSTER_LEVEL4] = BOOSTER_DVFS_FREQ(1300000,	413000,	160000),
};

static struct booster_key booster_keys[] = {
	BOOSTER_KEYS("HOMEPAGE", KEY_HOMEPAGE,
			BOOSTER_DEFAULT_ON_TIME,
			BOOSTER_DEFAULT_OFF_TIME,
			key_freq_table),
	BOOSTER_KEYS("MENU", KEY_MENU,
			BOOSTER_DEFAULT_ON_TIME,
			BOOSTER_DEFAULT_OFF_TIME,
			touchkey_freq_table),
	BOOSTER_KEYS("BACK", KEY_BACK,
			BOOSTER_DEFAULT_ON_TIME,
			BOOSTER_DEFAULT_OFF_TIME,
			touchkey_freq_table),
	BOOSTER_KEYS("TOUCH", KEY_BOOSTER_TOUCH,
			BOOSTER_DEFAULT_CHG_TIME,
			BOOSTER_DEFAULT_OFF_TIME,
			touch_freq_table),
	BOOSTER_KEYS("PEN", KEY_BOOSTER_PEN,
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

static struct platform_device *universal5260_input_devices[] __initdata = {
	&s3c_device_i2c1,
	&universal5260_gpio_keys,
#if defined(CONFIG_KEYBOARD_CYPRESS_TOUCH) || defined(CONFIG_KEYBOARD_TC370L_TOUCHKEY)
	&s3c_device_i2c10,
#endif
#ifdef CONFIG_INPUT_WACOM
	&s3c_device_i2c2,
#endif
#ifdef CONFIG_INPUT_BOOSTER
	&universal5260_input_booster,
#endif
};

struct exynos5_platform_i2c hs_i2c0_data __initdata = {
	.bus_number = 0,
	.operation_mode = HSI2C_POLLING,
	.speed_mode = HSI2C_FAST_SPD,
	.fast_speed = 400000,
	.high_speed = 2500000,
	.cfg_gpio = NULL,
};

void __init tsp_check_and_init(void)
{

#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
	midas_tsp_init();
//	s3c_i2c1_set_platdata(&touch_i2c5_platdata);
	s3c_i2c1_set_platdata(NULL);
	i2c_register_board_info(5, i2c_devs5, ARRAY_SIZE(i2c_devs5));
#elif defined(CONFIG_TOUCHSCREEN_ZINITIX)
	zinitix_tsp_init();
	s3c_i2c1_set_platdata(NULL);
	i2c_register_board_info(5, zinitix_i2c_devs, ARRAY_SIZE(zinitix_i2c_devs));
#else
	fts_tsp_init();

	if (system_rev >= 0x1) {
		s3c_i2c1_set_platdata(NULL);
		i2c_register_board_info(5, fts_i2c_devs, ARRAY_SIZE(fts_i2c_devs));
	}
	else {
		universal5260_input_devices[0] = &exynos5_device_hs_i2c0;
		exynos5_hs_i2c0_set_platdata(&hs_i2c0_data);
		i2c_register_board_info(0, fts_i2c_devs, ARRAY_SIZE(fts_i2c_devs));
	}
#endif	
}

void __init exynos5_universal5260_input_init(void)
{
	int state = (gpio_get_value_cansleep(GPIO_POWER_BUTTON) ? 1 : 0) ^ 1;

	printk(KERN_INFO "gpio-keys : PWR, state[%d]\n", state);

	tsp_check_and_init();

#if defined(CONFIG_KEYBOARD_CYPRESS_TOUCH) || defined(CONFIG_KEYBOARD_TC370L_TOUCHKEY)
	touchkey_init();
#endif

#ifdef CONFIG_INPUT_WACOM
	wacom_init();
#endif
#ifdef CONFIG_SENSORS_HALL
	s3c_gpio_setpull(GPIO_HALL_SENSOR_INT, S3C_GPIO_PULL_UP);
	gpio_request(GPIO_HALL_SENSOR_INT, "GPIO_HALL_SENSOR_INT");
	s3c_gpio_cfgpin(GPIO_HALL_SENSOR_INT, S3C_GPIO_SFN(0xf));
	s5p_register_gpio_interrupt(GPIO_HALL_SENSOR_INT);
	gpio_direction_input(GPIO_HALL_SENSOR_INT);
#endif
#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
	s3c_gpio_setpull(GPIO_KEY_S1, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_KEY_S2, S3C_GPIO_PULL_UP);
	s5p_register_gpio_interrupt(GPIO_KEY_S1);
	s5p_register_gpio_interrupt(GPIO_KEY_S2);
#endif
	platform_add_devices(universal5260_input_devices,
			ARRAY_SIZE(universal5260_input_devices));
}
