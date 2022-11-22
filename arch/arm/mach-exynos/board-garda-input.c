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
#include <linux/regulator/consumer.h>

#include "board-universal222ap.h"
#include "board-garda.h"

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT432)
#include <linux/i2c/zinitix_ts.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_MMS134S)
#include <linux/i2c/mms134s.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_MMS136)
#include <linux/i2c/mms136.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#endif


#ifdef CONFIG_SEC_DEBUG
#include <mach/sec_debug.h>
#endif

extern unsigned int system_rev;

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

#if 0
static void zxt_ts_int_set_pull(bool to_up)
{

	int ret;

	int pull = (to_up) ? NMK_GPIO_PULL_UP : NMK_GPIO_PULL_DOWN;

	ret = nmk_gpio_set_pull(TSP_INT_CODINA_R0_0, pull);
	if (ret < 0)
		printk(KERN_ERR "%s: fail to set pull xx on interrupt pin\n",
		       __func__);
}

static int zxt_ts_pin_configure(bool to_gpios)
{
	if (to_gpios) {
		nmk_gpio_set_mode(TSP_SCL_CODINA_R0_0, NMK_GPIO_ALT_GPIO);
		gpio_direction_output(TSP_SCL_CODINA_R0_0, 0);

		nmk_gpio_set_mode(TSP_SDA_CODINA_R0_0, NMK_GPIO_ALT_GPIO);
		gpio_direction_output(TSP_SDA_CODINA_R0_0, 0);

	} else {
		gpio_direction_output(TSP_SCL_CODINA_R0_0, 1);
		nmk_gpio_set_mode(TSP_SCL_CODINA_R0_0, NMK_GPIO_ALT_C);

		gpio_direction_output(TSP_SDA_CODINA_R0_0, 1);
		nmk_gpio_set_mode(TSP_SDA_CODINA_R0_0, NMK_GPIO_ALT_C);
	}
	return 0;
}
#endif

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
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
	} else {
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_DOWN);
		if (regulator_is_enabled(regulator_vdd))
			regulator_disable(regulator_vdd);
		else
			regulator_force_disable(regulator_vdd);
	}
	regulator_put(regulator_vdd);

	if (system_rev > 7)
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

#endif

#if defined(CONFIG_TOUCHSCREEN_MMS136)
extern struct class *sec_class;
static bool tsp_power_enabled;

static void melfas_register_charger_callback(void *cb)
{
	tsp_callbacks = cb;
	pr_debug("[TSP] melfas_register_lcd_callback\n");
}

void melfas_vdd_pullup(bool on)
{
	struct regulator *regulator_vdd;

	printk(KERN_DEBUG "[TSP] %s %s\n",
		__func__, on ? "on" : "off");

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
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
	} else {
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_DOWN);
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

int melfas_power_onlyvdd(bool on)
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


int melfas_mux_fw_flash(bool to_gpios)
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

void melfas_set_touch_i2c(void)
{
	s3c_gpio_cfgpin(GPIO_TSP_SDA, S3C_GPIO_SFN(3));
	s3c_gpio_setpull(GPIO_TSP_SDA, S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(GPIO_TSP_SCL, S3C_GPIO_SFN(3));
	s3c_gpio_setpull(GPIO_TSP_SCL, S3C_GPIO_PULL_UP);
	gpio_free(GPIO_TSP_SDA);
	gpio_free(GPIO_TSP_SCL);
	s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
}

void melfas_set_touch_i2c_to_gpio(void)
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

static struct melfas_tsi_platform_data mms_ts_pdata = {
	.max_x = 480,
	.max_y = 800,
	.invert_x = 0,
	.invert_y = 0,
	.gpio_int = GPIO_TSP_INT,
	.gpio_scl = GPIO_TSP_SCL,
	.gpio_sda = GPIO_TSP_SDA,
	.power = melfas_power,
	.power_vdd = melfas_power_onlyvdd,
	.mux_fw_flash = melfas_mux_fw_flash,
	.is_vdd_on = is_melfas_vdd_on,
	.touchkey = true,	
	.keyled = key_led_control,	
	.register_cb = melfas_register_charger_callback,
};
#endif

#if defined(CONFIG_INPUT_TOUCHSCREEN)
static struct i2c_board_info i2c_devs_touch[] = {
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT432)
	{
	 I2C_BOARD_INFO(ZINITIX_TS_NAME, 0x20),
	 .platform_data = &zxt_ts_pdata
	},
#elif defined(CONFIG_TOUCHSCREEN_MMS134S)
	{
	 I2C_BOARD_INFO(MELFAS_TS_NAME, 0x48),
	 .platform_data = &mms_ts_pdata
	},
#elif defined(CONFIG_TOUCHSCREEN_MMS136)
	{
		I2C_BOARD_INFO(MELFAS_TS_NAME, 0x48),
//		.irq		= IRQ_EINT(29),
		.platform_data	= &mms_ts_pdata,
	},
#endif
};

void __init garda_tsp_init(void)
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
	printk(KERN_ERR "%s touch : %d (%u)\n", __func__,
		i2c_devs_touch[0].irq, system_rev);
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_BT432)
	zinitix_power(0);
#endif
#if	defined(CONFIG_TOUCHSCREEN_ZINITIX_BT432) &&\
	defined(CONFIG_TOUCHSCREEN_MMS134S)
	if (system_rev >= 7) {
		strcpy(i2c_devs_touch[0].type, MELFAS_TS_NAME);
		i2c_devs_touch[0].addr = 0x48;
		i2c_devs_touch[0].platform_data = &mms_ts_pdata;
	}
#endif
#if defined(CONFIG_TOUCHSCREEN_MMS136)
	melfas_power(0);
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
	s3c_gpio_setpull(GPIO_VOL_UP, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_VOL_DOWN, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_HOME_KEY, S3C_GPIO_PULL_UP);
}

static struct gpio_keys_button smdk4270_button[] = {
	{
		.code = KEY_POWER,
		.gpio = GPIO_PMIC_ONOB,
		.desc = "gpio-keys: KEY_POWER",
		.active_low = 1,
		.wakeup = 1,
		.debounce_interval = 10,
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
		.wakeup = 1,
		.debounce_interval = 10,
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
#if defined(CONFIG_INPUT_TOUCHSCREEN)
	&s3c_device_i2c3,
#endif
	&smdk4270_gpio_keys,
};

void __init exynos4_smdk4270_input_init(void)
{
#if defined(CONFIG_INPUT_TOUCHSCREEN)
	garda_tsp_init();

	s3c_i2c3_set_platdata(NULL);
	i2c_register_board_info(3, i2c_devs_touch, ARRAY_SIZE(i2c_devs_touch));
#endif
	smdk4270_gpio_keys_config_setup();
	platform_add_devices(smdk4270_input_devices,
			ARRAY_SIZE(smdk4270_input_devices));
}
