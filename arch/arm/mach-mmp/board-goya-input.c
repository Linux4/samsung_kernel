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
#include <linux/input/bt532_ts.h>
#include <linux/vibrator.h>
#include <plat/mfp.h>
#include <plat/pxa27x_keypad.h>
#include <mach/pxa988.h>
#include "common.h"


#define KP_MKIN0_VOL_DOWN		MFP_CFG(GPIO0, AF1)
#define KP_MKIN1_VOL_UP			MFP_CFG(GPIO2, AF1)
#define KP_NKIN5_HOME_KEY		MFP_CFG(GPIO13, AF4)
#define GPIO001_VOL_UP			MFP_CFG(GPIO1, AF0) | MFP_LPM_INPUT | MFP_PULL_HIGH
#define GPIO002_VOL_DOWN		MFP_CFG(GPIO2, AF0) | MFP_LPM_INPUT | MFP_PULL_HIGH
#define GPIO013_HOME_KEY			MFP_CFG(GPIO13, AF0) | MFP_PULL_NONE
#define CI2C2_TSP_SCL			MFP_CFG(GPIO87, AF5) | MFP_LPM_FLOAT | MFP_PULL_NONE
#define CI2C2_TSP_SDA			MFP_CFG(GPIO88, AF5) | MFP_LPM_FLOAT | MFP_PULL_NONE
#define GPIO087_GPIO_87			MFP_CFG(GPIO87, AF0) | MFP_LPM_INPUT | MFP_LPM_FLOAT
#define GPIO088_GPIO_88			MFP_CFG(GPIO88, AF0) | MFP_LPM_INPUT | MFP_LPM_FLOAT
#define GPIO094_TOUCH_IRQ		MFP_CFG(GPIO94, AF0)
#define GPIO096_MOTOR_EN		MFP_CFG(GPIO96, AF0) | MFP_LPM_PULL_LOW | MFP_PULL_LOW

static unsigned long __initdata goya_input_pin_config_bring[] = {
#ifdef CONFIG_KEYBOARD_PXA27x
	KP_MKIN0_VOL_DOWN,
	KP_MKIN1_VOL_UP,
	KP_NKIN5_HOME_KEY,
#else
	GPIO001_VOL_UP,
	GPIO002_VOL_DOWN,
	GPIO013_HOME_KEY,
#endif

	CI2C2_TSP_SCL,
	CI2C2_TSP_SDA,
	GPIO094_TOUCH_IRQ,
	GPIO096_MOTOR_EN,
};


#ifdef CONFIG_KEYBOARD_PXA27x
/* Matrix keys */
static unsigned int __initdata goya_matrix_key_map[] = {
	KEY(0, 0, KEY_VOLUMEDOWN),
	KEY(1, 0, KEY_VOLUMEUP),
};

static struct pxa27x_keypad_platform_data __initdata pxa27x_keypad_pdata = {
	.matrix_key_rows		= 2,
	.matrix_key_cols		= 1,
	.matrix_key_map			= goya_matrix_key_map,
	.matrix_key_map_size	= ARRAY_SIZE(goya_matrix_key_map),
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

static void __init goya_input_keyboard_init(void)
{
	pxa988_add_keypad(&pxa27x_keypad_pdata);
}
#else
struct gpio_keys_button gpio_keys[] =
{
	{
		.code = KEY_VOLUMEUP,       /* input event code (KEY_*, SW_*) */
		.gpio = mfp_to_gpio(GPIO001_VOL_UP),
		.active_low = 1,
		.desc = "key_volumeup",
		.type = EV_KEY,     /* input event type (EV_KEY, EV_SW) */
		.wakeup = 0,
		.debounce_interval = 30,    /* debounce ticks interval in msecs */
		.can_disable = false,
	},
	{
		.code = KEY_VOLUMEDOWN,     /* input event code (KEY_*, SW_*) */
		.gpio = mfp_to_gpio(GPIO002_VOL_DOWN),
		.active_low = 1,
		.desc = "key_volumedown",
		.type = EV_KEY,     /* input event type (EV_KEY, EV_SW) */
		.wakeup = 0,        /* configure the button as a wake-up source */
		.debounce_interval = 30,    /* debounce ticks interval in msecs */
		.can_disable = false,
	},
	{
		.code = KEY_HOME,     /* input event code (KEY_*, SW_*) */
		.gpio = mfp_to_gpio(GPIO013_HOME_KEY),
		.active_low = 1,
		.desc = "key_home",
		.type = EV_KEY,     /* input event type (EV_KEY, EV_SW) */
		.wakeup = 1,        /* configure the button as a wake-up source */
		.debounce_interval = 30,    /* debounce ticks interval in msecs */
		.can_disable = false,
	},

};

struct gpio_keys_platform_data gpio_keys_data =
{
	.buttons = gpio_keys,
	.nbuttons = ARRAY_SIZE(gpio_keys),
};

struct platform_device gpio_keys_device =
{
	.name = "gpio-keys",
	.dev =
	{
		.platform_data = &gpio_keys_data,
	},
};
#include <mach/addr-map.h>
#include <mach/regs-apmu.h>
static void __init goya_input_keyboard_init(void)
{
	u32 kpc;

	/*disable keypad function*/
	kpc = __raw_readl(APB_PHYS_BASE+0x12000);

	__raw_writel(0, APB_PHYS_BASE+0x12000);
	kpc = __raw_readl(APB_PHYS_BASE+0x12000);

	/*close keypad clock*/
	__raw_writel(0, APBC_PXA988_KPC);

	kpc = __raw_readl(APBC_PXA988_KPC);

	/*clear keypad wakeup source*/
	kpc = __raw_readl(APMU_WAKE_CLR);
	__raw_writel(kpc | (1<<3), APMU_WAKE_CLR);

	platform_device_register(&gpio_keys_device);
}
#endif

/* touch screen */
#define TOUCH_IRQ		mfp_to_gpio(GPIO094_TOUCH_IRQ)

static struct regulator *touch_regulator;
extern void i2c1_pin_changed(int gpio);

#define POWER_OFF 0
#define POWER_ON 1
#define POWER_ON_SEQUENCE 2
#define PM_POWER_OFF 3

static int bt532_power(int on)
{
	int ret = 0;
	static u8 is_power_on;
	if (touch_regulator == NULL) {
		touch_regulator = regulator_get(NULL, "v_tsp_3v3");
		if (IS_ERR(touch_regulator)) {
			touch_regulator = NULL;
			pr_info("[TSP]: %s: get touch_regulator error\n",
				__func__);
			return -EIO;
		}
	}
	if ((on == POWER_OFF) || (on == PM_POWER_OFF)) {
		if (is_power_on) {
			is_power_on = 0;
			if(on == POWER_OFF)
				i2c1_pin_changed(1);
			ret = regulator_disable(touch_regulator);
			if (ret) {
				is_power_on = 1;
				pr_err("[TSP]: %s: touch_regulator disable " \
					"failed  (%d)\n", __func__, ret);
				return -EIO;
			}
			msleep(50);
		}
	} else {
		if (!is_power_on) {
			is_power_on = 1;
			regulator_set_voltage(touch_regulator, 2800000,
								2800000);
			ret = regulator_enable(touch_regulator);
			if (ret) {
				is_power_on = 0;
				pr_err("[TSP]: %s: touch_regulator enable "\
					"failed (%d)\n", __func__, ret);
				return -EIO;
			}
			i2c1_pin_changed(0);
		}
	}
	pr_info("[TSP]: %s, expected power[%d], actural power[%d]\n",
		__func__, on, is_power_on);
	return 0;
}

static struct bt532_ts_platform_data bt532_ts_pdata = {
	.tsp_power	= bt532_power,
	.x_resolution	= 599,
	.y_resolution	= 1023,
	.page_size	= 128,
	.orientation	= 0,
};

static struct i2c_board_info __initdata bt532_i2c_devices[] = {
	{
		I2C_BOARD_INFO(BT532_TS_DEVICE, 0x20),
		.platform_data = &bt532_ts_pdata,
	},
};

static void __init goya_input_tsp_init(void)
{
	int ret = 0;

	ret = gpio_request(TOUCH_IRQ, "bt532_int");
	if (ret < 0) {
		pr_err("%s: Request GPIO failed, gpio %d (%d)\n", BT532_TS_DEVICE,
			TOUCH_IRQ, ret);
		return;
	}

	gpio_direction_input(TOUCH_IRQ);
	bt532_ts_pdata.gpio_int = TOUCH_IRQ;
	bt532_i2c_devices[0].irq  = gpio_to_irq(TOUCH_IRQ);

	i2c_register_board_info(1, ARRAY_AND_SIZE(bt532_i2c_devices));
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

static void __init goya_input_vibrator_init(void)
{
	int ret;

	ret = gpio_request(VIB_EN, "vibrator_en");
	if (ret < 0) {
		pr_err("Vibrator: Request GPIO failed, gpio %d (%d)\n", VIB_EN, ret);
		return;
	}

	platform_device_register(&android_vibrator_device);
}

void __init goya_input_init(void)
{
	mfp_config(ARRAY_AND_SIZE(goya_input_pin_config_bring));

	goya_input_keyboard_init();
	goya_input_tsp_init();
	goya_input_vibrator_init();
}