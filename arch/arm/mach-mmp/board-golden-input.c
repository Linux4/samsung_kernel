/*
 * Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio_keys.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/input.h>
#include <linux/input/mxts.h>
#include <linux/input/tc360-touchkey.h>
#include <linux/vibrator.h>
#include <plat/mfp.h>
#include <plat/pxa27x_keypad.h>
#include <mach/pxa988.h>
#include <asm/system_info.h>

#include "common.h"
#include "board-golden.h"



#define GOLDEN_VE_REV_00	1
#define GOLDEN_VE_REV_01	2


#define KP_MKIN0_VOL_DOWN			MFP_CFG(GPIO0, AF1)
#define KP_MKIN1_VOL_UP				MFP_CFG(GPIO2, AF1)
#define GPIO003_TOUCH_KEY_SCL		MFP_CFG(GPIO3, AF0)
#define GPIO004_TOUCH_KEY_SDA		MFP_CFG(GPIO4, AF0)
#define KP_NKIN5_HOME_KEY			MFP_CFG(GPIO13, AF4)
#define GPIO030_TOUCH_KEY_LED_EN	(MFP_CFG(GPIO30, AF0) | MFP_PULL_LOW \
										| MFP_LPM_DRIVE_LOW)
#define CI2C2_TSP_SCL				(MFP_CFG(GPIO87, AF5) | MFP_LPM_FLOAT)
#define CI2C2_TSP_SDA				(MFP_CFG(GPIO88, AF5) | MFP_LPM_FLOAT)
#define GPIO087_TSP_SCL				(MFP_CFG(GPIO87, AF0) | MFP_LPM_FLOAT)
#define GPIO088_TSP_SDA				(MFP_CFG(GPIO88, AF0) | MFP_LPM_FLOAT)
#define GPIO090_TOUCH_KEY_INT		MFP_CFG(GPIO90, AF0)
#define GPIO094_TOUCH_IRQ			MFP_CFG(GPIO94, AF0)
#define GPIO096_MOTOR_EN			(MFP_CFG(GPIO96, AF0) | MFP_PULL_LOW \
										| MFP_LPM_DRIVE_LOW)
#define GPIO102_TOUCH_IO_EN			(MFP_CFG(DF_nCS1_SM_nCS3, AF1) \
										| MFP_PULL_LOW)

static unsigned long __initdata golden_input_pin_config_rev_00[] = {
	KP_MKIN0_VOL_DOWN,
	KP_MKIN1_VOL_UP,
	GPIO003_TOUCH_KEY_SCL,
	GPIO004_TOUCH_KEY_SDA,
	KP_NKIN5_HOME_KEY,
	GPIO030_TOUCH_KEY_LED_EN,
	GPIO087_TSP_SCL,
	GPIO088_TSP_SDA,
	GPIO090_TOUCH_KEY_INT,
	GPIO094_TOUCH_IRQ,
	GPIO096_MOTOR_EN,
	GPIO102_TOUCH_IO_EN,
};

static unsigned long __initdata golden_input_pin_config_rev_01[] = {
#define GPIO007_TOUCH_KEY_EN	(MFP_CFG(GPIO7, AF0) | MFP_PULL_LOW)
	GPIO007_TOUCH_KEY_EN,
};

static unsigned long twsi_i2c_pin_config[] = {
	CI2C2_TSP_SCL,
	CI2C2_TSP_SDA,
};

static unsigned long gpio_i2c_pin_config[] = {
	GPIO087_TSP_SCL,
	GPIO088_TSP_SDA,
};

static inline void change_pin_config(bool onoff)
{
	usleep_range(5000, 8000);

	if (onoff)
		mfp_config(ARRAY_AND_SIZE(twsi_i2c_pin_config));
	else
		mfp_config(ARRAY_AND_SIZE(gpio_i2c_pin_config));

	usleep_range(5000, 8000);
}

/* Matrix keys */
static unsigned int __initdata golden_matrix_key_map[] = {
	KEY(0, 0, KEY_VOLUMEDOWN),
	KEY(1, 0, KEY_VOLUMEUP),
};

static struct pxa27x_keypad_platform_data __initdata pxa27x_keypad_pdata = {
	.matrix_key_rows		= 2,
	.matrix_key_cols		= 1,
	.matrix_key_map			= golden_matrix_key_map,
	.matrix_key_map_size	= ARRAY_SIZE(golden_matrix_key_map),
	.debounce_interval		= 30,

	.direct_key_num			= 6,
	.direct_key_map			= {
			KEY_RESERVED,
			KEY_RESERVED,
			KEY_RESERVED,
			KEY_RESERVED,
			KEY_RESERVED,
			KEY_HOME},
	.direct_wakeup_pad		= {
			0,0,0,0,0,
			mfp_to_gpio(KP_NKIN5_HOME_KEY)},
	.direct_key_low_active	= 1,
};

static void __init golden_input_keyboard_init(void)
{
	pxa988_add_keypad(&pxa27x_keypad_pdata);
}


/* touch screen */
#define NUM_XNODE		19
#define NUM_YNODE		11
#define TOUCH_MAX_X		480
#define TOUCH_MAX_Y		800
#define PROJECT_NAME	"I8200N"
//#define CONFIG_VER		"0327"

#define TOUCH_IRQ		mfp_to_gpio(GPIO094_TOUCH_IRQ)
#define TOUCH_IO_EN		MFP_PIN_GPIO102

extern int usb_switch_register_notify(struct notifier_block *nb);

static struct regulator *mxts_vdd_regulator;

static inline bool check_ta_connection()
{
	bool charger_enable;

	if (current_cable_type == POWER_SUPPLY_TYPE_BATTERY)
		charger_enable = false;
	else if (current_cable_type == POWER_SUPPLY_TYPE_MAINS ||
		current_cable_type == POWER_SUPPLY_TYPE_USB)
		charger_enable = true;

	pr_info("%s %s: %s mode\n", MXT_DEV_NAME, __func__,
		charger_enable ? "charging" : "battery");

	return charger_enable;
}

static void mxts_usb_switch_nofify(struct notifier_block *nb, unsigned long val,
			void *dev)
{
	struct mxts_callback *cb = container_of(nb, struct mxts_callback, mxts_nb);

	cb->inform_charger(cb, check_ta_connection());
}

static void mxts_register_callback(struct mxts_callback *cb)
{
	cb->mxts_nb.notifier_call = mxts_usb_switch_nofify;
	usb_switch_register_notify(&cb->mxts_nb);

	cb->inform_charger(cb, check_ta_connection());
}

static int mxts_power_setup(struct device *dev, bool onoff)
{
	int min_uv, max_uv;
	int ret;

	if (onoff) {
		min_uv = max_uv = 3300000;

		mxts_vdd_regulator = regulator_get(dev, "v_tsp_3v3");
		if (IS_ERR(mxts_vdd_regulator)) {
			ret = PTR_ERR(mxts_vdd_regulator);
			pr_err("%s %s: Failed to get mxts_vdd_regulator (%d)\n",
				MXT_DEV_NAME, __func__, ret);
			return ret;
		}

		ret = regulator_set_voltage(mxts_vdd_regulator, min_uv, max_uv);
		if (ret < 0) {
			pr_err("%s %s: Failed to set mxts_vdd_regulator to %d, %d (%d)\n",
				MXT_DEV_NAME, __func__, min_uv, max_uv, ret);
			regulator_put(mxts_vdd_regulator);
			return ret;
		}

		pr_info("%s %s: set mxts_vdd_regulator to %d uV - %d uV (%d)\n",
			MXT_DEV_NAME, __func__, min_uv, max_uv, ret);
	} else {
		regulator_force_disable(mxts_vdd_regulator);
		regulator_put(mxts_vdd_regulator);
	}

	return 0;
}

static int mxts_power_onoff(bool onoff)
{
	if (mxts_vdd_regulator == NULL) {
		pr_info("%s %s: no regulator.\n", MXT_DEV_NAME, __func__);
		return -1;
	}

	if (onoff) {
		regulator_enable(mxts_vdd_regulator);
		gpio_set_value(TOUCH_IO_EN, onoff);
	}
	else {
		gpio_set_value(TOUCH_IO_EN, onoff);
		regulator_disable(mxts_vdd_regulator);
	}

	change_pin_config(onoff);

	pr_info("%s %s: %s\n", MXT_DEV_NAME, __func__, (onoff) ? "on" : "off");

	return 0;
}

static bool mxts_read_chg(void)
{
	return gpio_get_value(TOUCH_IRQ);
}

static struct mxt_platform_data mxt_pdata = {
	.num_xnode		= NUM_XNODE,
	.num_ynode		= NUM_YNODE,
	.max_x			= TOUCH_MAX_X - 1,
	.max_y			= TOUCH_MAX_Y - 1,
	.irqflags		= IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	.read_chg		= mxts_read_chg,
	.power_setup	= mxts_power_setup,
	.power_onoff	= mxts_power_onoff,
	.register_cb	= mxts_register_callback,
	.project_name	= PROJECT_NAME,
//	.config_ver		= CONFIG_VER,
};

static struct i2c_board_info __initdata mxt224s_i2c_devices[] = {
	{
		I2C_BOARD_INFO(MXT_DEV_NAME, 0x4a),
		.platform_data  = &mxt_pdata,
	},
};

static void __init golden_input_tsp_init(void)
{
	int ret = 0;

	ret = gpio_request(TOUCH_IO_EN, "mxt224s_io_en");
	if (ret < 0) {
		pr_err("%s: Request GPIO failed, gpio %d (%d)\n", MXT_DEV_NAME,
			TOUCH_IO_EN, ret);
		return ret;
	}

	gpio_direction_output(TOUCH_IO_EN, 0);

	ret = gpio_request(TOUCH_IRQ, "mxt224s_int");
	if (ret < 0) {
		pr_err("%s: Request GPIO failed, gpio %d (%d)\n", MXT_DEV_NAME,
			TOUCH_IRQ, ret);
		return ret;
	}

	gpio_direction_input(TOUCH_IRQ);
	mxt224s_i2c_devices[0].irq  = gpio_to_irq(TOUCH_IRQ);

	i2c_register_board_info(1, ARRAY_AND_SIZE(mxt224s_i2c_devices));

	return 0;
}


/* touch key */
#define TOUCH_KEY_SCL		mfp_to_gpio(GPIO003_TOUCH_KEY_SCL)
#define TOUCH_KEY_SDA		mfp_to_gpio(GPIO004_TOUCH_KEY_SDA)
#define TOUCH_KEY_EN		mfp_to_gpio(GPIO007_TOUCH_KEY_EN)
#define TOUCH_KEY_LED_EN	mfp_to_gpio(GPIO030_TOUCH_KEY_LED_EN)
#define TOUCH_KEY_INT		mfp_to_gpio(GPIO090_TOUCH_KEY_INT)

static struct regulator *tc360_regulator;
static int tc360_keycodes[]	= {KEY_MENU, KEY_BACK};

static int tc360_power_setup(struct device *dev, bool onoff)
{
	int min_uv, max_uv;
	int ret;

	/* when HW_Rev is 0.0, tsp ic use regulator */
	if (system_rev > GOLDEN_VE_REV_00)
		return 0;

	if (onoff) {
		min_uv = max_uv = 2200000;

		tc360_regulator = regulator_get(dev, "v_touchkey_2v2");
		if (IS_ERR(tc360_regulator)) {
			ret = PTR_ERR(tc360_regulator);
			pr_err("%s %s: Failed to get v_touchkey_2v2 (%d)\n",
				TC360_DEVICE, __func__, ret);
			return ret;
		}

		ret = regulator_set_voltage(tc360_regulator, min_uv, max_uv);
		if (ret < 0) {
			pr_err("%s %s: Failed to set v_touchkey_2v2 to %d, %d (%d)\n",
				TC360_DEVICE, __func__, min_uv, max_uv, ret);
			regulator_put(tc360_regulator);
			return ret;
		}

		{
			unsigned long pin_config[] = {
				(MFP_CFG(GPIO90, AF0) | MFP_PULL_HIGH),};
			mfp_config(pin_config, 1);
		}

		pr_info("%s %s: set v_touchkey_2v2 to %d uV - %d uV (%d)\n",
			TC360_DEVICE, __func__, min_uv, max_uv, ret);
	} else {
		regulator_force_disable(tc360_regulator);
		regulator_put(tc360_regulator);
	}

	return 0;
}

static void tc360_power_onoff(bool onoff)
{
	/* when HW_Rev is 0.0, tsp ic use regulator */
	if (system_rev > GOLDEN_VE_REV_00) {
		if (onoff) {
			unsigned long touchkey_pin_config[] = {
				GPIO003_TOUCH_KEY_SCL,
				GPIO004_TOUCH_KEY_SDA,
				GPIO090_TOUCH_KEY_INT,
			};

			mfp_config(ARRAY_AND_SIZE(touchkey_pin_config));

			gpio_set_value(TOUCH_KEY_EN, onoff);
		}
		else {
			unsigned long touchkey_pin_config[] = {
				(GPIO003_TOUCH_KEY_SCL | MFP_PULL_LOW),
				(GPIO004_TOUCH_KEY_SDA | MFP_PULL_LOW),
				(GPIO090_TOUCH_KEY_INT | MFP_PULL_LOW),
			};

			mfp_config(ARRAY_AND_SIZE(touchkey_pin_config));

			gpio_set_value(TOUCH_KEY_EN, onoff);
		}
	}
	else {
		if (tc360_regulator == NULL) {
			pr_info("%s %s: no regulator.\n", TC360_DEVICE, __func__);
			return -1;
		}

		if (onoff)
			regulator_enable(tc360_regulator);
		else
			regulator_disable(tc360_regulator);
	}

	pr_info("%s %s: %s\n", TC360_DEVICE, __func__, (onoff) ? "on" : "off");
}

static void tc360_led_power_onoff(bool onoff)
{
	gpio_set_value(TOUCH_KEY_LED_EN, onoff);

	pr_info("%s %s: %s\n", TC360_DEVICE, __func__, (onoff) ? "on" : "off");
}

static void tc360_pin_configure(bool to_gpios)
{
	/* the below routine is commented out.
	 * because the 'golden' use s/w i2c for tc360 touchkey.
	 */
	if (to_gpios) {
		/*
		nmk_gpio_set_mode(TOUCHKEY_SCL_GOLDEN_BRINGUP,
					NMK_GPIO_ALT_GPIO);
		*/
		gpio_direction_output(TOUCH_KEY_SCL, 1);
		/*
		nmk_gpio_set_mode(TOUCHKEY_SDA_GOLDEN_BRINGUP,
					NMK_GPIO_ALT_GPIO);
		*/
		gpio_direction_output(TOUCH_KEY_SDA, 1);


	} else {

		gpio_direction_output(TOUCH_KEY_SCL, 1);
		/*
		nmk_gpio_set_mode(TOUCHKEY_SCL_GOLDEN_BRINGUP, NMK_GPIO_ALT_C);
		*/
		gpio_direction_output(TOUCH_KEY_SDA, 1);
		/*
		nmk_gpio_set_mode(TOUCHKEY_SDA_GOLDEN_BRINGUP, NMK_GPIO_ALT_C);
		*/
	}

}

static void tc360_int_set_pull(bool to_up)
{
	int ret;
/*	int pull = (to_up) ? NMK_GPIO_PULL_UP : NMK_GPIO_PULL_DOWN;

	ret = nmk_gpio_set_pull(TOUCHKEY_INT_GOLDEN_BRINGUP, pull);
	if (ret < 0)
		printk(KERN_ERR "%s: fail to set pull-%s on interrupt pin\n", __func__,
			(pull == NMK_GPIO_PULL_UP) ? "up" : "down");*/
}

struct tc360_platform_data tc360_pdata = {
	.gpio_scl = TOUCH_KEY_SCL,
	.gpio_sda = TOUCH_KEY_SDA,
	.gpio_int = TOUCH_KEY_INT,
	//.gpio_en = TOUCHKEY_EN_GOLDEN_BRINGUP,
	.udelay = 6,
	.num_key = ARRAY_SIZE(tc360_keycodes),
	.keycodes = tc360_keycodes,
	.suspend_type = TC360_SUSPEND_WITH_POWER_OFF,
	.setup_power = tc360_power_setup,
	.power = tc360_power_onoff,
	.led_power = tc360_led_power_onoff,
	.pin_configure = tc360_pin_configure,
	.int_set_pull = tc360_int_set_pull,
	//.touchscreen_is_pressed = &touchscreen_is_pressed,
};

static struct i2c_board_info __initdata tc360_i2c_devices[] = {
	{
		I2C_BOARD_INFO(TC360_DEVICE, 0x20),
		.platform_data  = &tc360_pdata,
	},
};

static struct i2c_gpio_platform_data tc360_gpio_data = {
	.sda_pin	= TOUCH_KEY_SDA,
	.scl_pin	= TOUCH_KEY_SCL,
	.udelay		= 5,
	.timeout	= 100,
};

static struct platform_device tc360_gpio_device = {
	.name   = "i2c-gpio",
	.id		= 4,
	.dev    = {
		.platform_data = &tc360_gpio_data,
	}
};

static void __init golden_input_tk_init()
{
	int ret;

	if (system_rev > GOLDEN_VE_REV_00) {
		ret = gpio_request(TOUCH_KEY_EN, "touchkey_en");
		if (ret < 0) {
			pr_err("%s: Request GPIO failed, gpio %d (%d)\n", TC360_DEVICE,
				TOUCH_KEY_EN, ret);
			return ret;
		}

		gpio_direction_output(TOUCH_KEY_EN, 0);
	}

	ret = gpio_request(TOUCH_KEY_LED_EN, "touchkey_led_en");
	if (ret < 0) {
		pr_err("%s: Request GPIO failed, gpio %d (%d)\n", TC360_DEVICE,
			TOUCH_KEY_LED_EN, ret);
		return ret;
	}

	gpio_direction_output(TOUCH_KEY_LED_EN, 0);

	ret = gpio_request(TOUCH_KEY_INT, "touchkey_int");
	if (ret < 0) {
		pr_err("%s: Request GPIO failed, gpio %d (%d)\n", TC360_DEVICE,
			TOUCH_KEY_INT, ret);
		return ret;
	}

	gpio_direction_input(TOUCH_KEY_INT);
	tc360_i2c_devices[0].irq = gpio_to_irq(TOUCH_KEY_INT);

	i2c_register_board_info(4, ARRAY_AND_SIZE(tc360_i2c_devices));

	//tc360_pdata.exit_flag = (u8)hats_state;

	platform_device_register(&tc360_gpio_device);

	return 0;
}

/* Vibrator */
#define VIB_EN		mfp_to_gpio(GPIO096_MOTOR_EN)

static struct vib_info vib_info = {
	.gpio		= VIB_EN,
};

static struct platform_device android_vibrator_device = {
	.name	= "android-vibrator",
	.dev	= {
		.platform_data = &vib_info,
	},
};

static void __init golden_input_vibrator_init(void)
{
	platform_device_register(&android_vibrator_device);
}

extern unsigned int lpcharge;
extern int get_panel_id();

void __init golden_input_init(void)
{
	mfp_config(ARRAY_AND_SIZE(golden_input_pin_config_rev_00));

	if (system_rev > GOLDEN_VE_REV_00)
		mfp_config(ARRAY_AND_SIZE(golden_input_pin_config_rev_01));

	golden_input_keyboard_init();

	if (get_panel_id() && !lpcharge) {
		golden_input_tsp_init();
		golden_input_tk_init();
	}

	golden_input_vibrator_init();
}