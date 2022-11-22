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
#include <linux/i2c.h>
#include <linux/export.h>
#if defined(CONFIG_TOUCHSCREEN_FT5X06)
#include <linux/i2c/ft5x06_ts.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI)
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/i2c/synaptics_rmi.h>
#include <linux/interrupt.h>
#endif

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>

#include <mach/irqs.h>
#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>

#include "board-xyref5260.h"

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

#define GPIO_TSP_INT		EXYNOS5260_GPX1(6)
#define GPIO_POWER_BUTTON	EXYNOS5260_GPX2(7)
#define GPIO_VOLUP_BUTTON	EXYNOS5260_GPX2(0)
#define GPIO_VOLDOWN_BUTTON	EXYNOS5260_GPX2(1)

static void xyref5260_gpio_keys_config_setup(void)
{
	s3c_gpio_setpull(GPIO_POWER_BUTTON, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_VOLUP_BUTTON, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_VOLDOWN_BUTTON, S3C_GPIO_PULL_NONE);

}
static struct gpio_keys_button xyref5260_button[] = {
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
	},
};

#if defined(CONFIG_TOUCHSCREEN_FT5X06)
#define GPIO_TSP_RESET		EXYNOS5260_GPA2(2)
#define GPIO_LEVEL_LOW		0

static void exynos5260_touch_init(void)
{
	int gpio;

	printk ("%s\n",__func__);
	/* TOUCH_RESET */
	gpio = GPIO_TSP_RESET;
	if (gpio_request(gpio, "TSP_RESET")) {
		pr_err("%s : TSP_RESET request port error\n", __func__);
	} else {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
		gpio_direction_output(gpio, 0);
		mdelay(10);
		gpio_direction_output(gpio, 1);
		gpio_free(gpio);
	}
}

struct s3c2410_platform_i2c i2c_data_touch  __initdata = {
		.bus_num        = 8,
		.flags          = 0,
		.slave_addr     = 0x70,
		.frequency      = 400*1000,
		.sda_delay      = 100,
};

static struct i2c_board_info i2c_devs_touch[] __initdata = {
	{
		I2C_BOARD_INFO(FT5X0X_NAME, 0x70 >> 1),
		.irq		= IRQ_EINT(14),
	},
};
#endif

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI)
#ifdef CONFIG_SEC_TSP_FACTORY
unsigned int bootmode;
EXPORT_SYMBOL(bootmode);
static int __init bootmode_setup(char *str)
{
	get_option(&str, &bootmode);
	return 1;
}
__setup("bootmode=", bootmode_setup);
#endif

/* Below function is added by synaptics requirement. they want observe
 * the LCD's HSYNC signal in there IC. So we enable the Tout1 pad(on DDI)
 * after power up TSP.
 * I think this is not good solution. because it is not nature, to control
 * LCD's register on TSP driver....
 */
void (*panel_touchkey_on)(void);
void (*panel_touchkey_off)(void);

static void synaptics_enable_sync(bool on)
{
	if (on) {
		if (panel_touchkey_on)
			panel_touchkey_on();
	} else {
		if (panel_touchkey_off)
			panel_touchkey_off();
	}
}

/* Define for DDI multiplication
 * OLED_ID is 0 represent that MAGNA LDI is attatched
 * OLED ID is 1 represent that SDC LDI is attatched
 * From H/W revision (1001) support mulit ddi.
 */
#define REVISION_NEED_TO_SUPPORT_DDI	9
#define DDI_SDC	0
#define DDI_MAGNA	1

static unsigned char synaptics_get_ldi_info(void)
{
	return DDI_SDC;
#if 0
	if (!gpio_get_value(GPIO_OLED_ID))
		return DDI_MAGNA;
	else
		return DDI_SDC;
#endif
}

static int synaptics_power(bool on)
{
	struct regulator *regulator_vdd;
	struct regulator *regulator_avdd;
	static bool enabled;

	if (enabled == on)
		return 0;

	regulator_vdd = regulator_get(NULL, "tsp_avdd_1.8");
	if (IS_ERR(regulator_vdd)) {
		printk(KERN_ERR "[TSP]ts_power_on : tsp_avdd_1.8 regulator_get failed\n");
		return PTR_ERR(regulator_vdd);
	}

	regulator_avdd = regulator_get(NULL, "tsp_avdd_3.3v");
	if (IS_ERR(regulator_avdd)) {
		printk(KERN_ERR "[TSP]ts_power_on : tsp_avdd_3.3v regulator_get failed\n");
		return PTR_ERR(regulator_avdd);
	}

	printk(KERN_ERR "[TSP] %s %s\n", __func__, on ? "on" : "off");

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
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
	}

	enabled = on;
	regulator_put(regulator_vdd);
	regulator_put(regulator_avdd);

	return 0;
}

static int synaptics_gpio_setup(unsigned gpio, bool configure)
{
	int retval = 0;

	if (configure) {
		if (gpio_request(gpio, "TSP_GPIO")) {
			pr_err("%s : TSP_GPIO request port error\n", __func__);
		} else {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_register_gpio_interrupt(gpio);
		}
	} else {
		pr_warn("%s: No way to deconfigure gpio %d.",
		       __func__, gpio);
	}

	return retval;
}

#ifdef NO_0D_WHILE_2D

static unsigned char tm1940_f1a_button_codes[] = {KEY_MENU, KEY_BACK};

static struct synaptics_rmi_f1a_button_map tm1940_f1a_button_map = {
	.nbuttons = ARRAY_SIZE(tm1940_f1a_button_codes),
	.map = tm1940_f1a_button_codes,
};

static int ts_led_power_on(bool on)
{
	struct regulator *regulator;

	if (on) {
		regulator = regulator_get(NULL, "touchkey_led");
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "[TSP_KEY] ts_led_power_on : TK_LED regulator_get failed\n");
			return -EIO;
		}

		regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		regulator = regulator_get(NULL, "touchkey_led");
		if (IS_ERR(regulator)) {
			printk(KERN_ERR "[TSP_KEY] ts_led_power_on : TK_LED regulator_get failed\n");
			return -EIO;
		}

		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);
	}

	return 0;
}
#endif

#define TM1940_ADDR 0x20
#define TM1940_ATTN 130

/* User firmware */
#define FW_IMAGE_NAME_B0_3_4	"tsp_synaptics/synaptics_b0_3_4.fw"
#define FW_IMAGE_NAME_B0_4_0	"tsp_synaptics/synaptics_b0_4_0.fw"
#define FW_IMAGE_NAME_B0_4_3	"tsp_synaptics/synaptics_b0_4_3.fw"
#define FW_IMAGE_NAME_B0_5_1	"tsp_synaptics/synaptics_b0_5_1.fw"

/* Factory firmware */
#define FAC_FWIMAGE_NAME_B0		"tsp_synaptics/synaptics_b0_fac.fw"
#define FAC_FWIMAGE_NAME_B0_5_1		"tsp_synaptics/synaptics_b0_5_1_fac.fw"


#define FW_IMAGE_NAME_B0_H	"tsp_synaptics/synaptics_b0_h.fw"
#define FAC_FWIMAGE_NAME_B0_H	"tsp_synaptics/synaptics_b0_fac.fw"

#define NUM_RX	28
#define NUM_TX	16

static struct synaptics_rmi4_platform_data rmi4_platformdata = {
	.sensor_max_x = 1079,
	.sensor_max_y = 1919,
	.max_touch_width = 28,
	.irq_type = IRQF_TRIGGER_LOW | IRQF_ONESHOT,/*IRQF_TRIGGER_FALLING,*/
	.power = synaptics_power,
	.gpio = GPIO_TSP_INT,
	.gpio_config = synaptics_gpio_setup,
#ifdef NO_0D_WHILE_2D
	.led_power_on = ts_led_power_on,
	.f1a_button_map = &tm1940_f1a_button_map,
#endif
	.firmware_name = NULL,
	.fac_firmware_name = NULL,
	.get_ddi_type = NULL,
	.num_of_rx = NUM_RX,
	.num_of_tx = NUM_TX,
};

static struct i2c_board_info synaptics_i2c_devs0[] = {
	{
		I2C_BOARD_INFO("synaptics_rmi4_i2c", 0x20),
		.platform_data = &rmi4_platformdata,
	}
};

static void synaptics_verify_panel_revision(void)
{
	unsigned int lcdtype = 4227143;

	u8 el_type = (lcdtype & 0x300) >> 8;
	u8 touch_type = (lcdtype & 0xF000) >> 12;

	rmi4_platformdata.firmware_name = FW_IMAGE_NAME_B0_H;
	rmi4_platformdata.fac_firmware_name = FAC_FWIMAGE_NAME_B0_H;

	rmi4_platformdata.panel_revision = touch_type;

	/* ID2 of LDI : from 0x25 of ID2, TOUT1 pin is connected
	 * to TSP IC to observe HSYNC signal on TSP IC.
	 * So added enable_sync function added in platform data.
	 */
	rmi4_platformdata.enable_sync = &synaptics_enable_sync;

#ifdef CONFIG_SEC_TSP_FACTORY
	rmi4_platformdata.bootmode = bootmode;
	printk(KERN_ERR "%s: bootmode: %d\n", __func__, bootmode);
#endif

	/*if (system_rev >= REVISION_NEED_TO_SUPPORT_DDI)*/
	rmi4_platformdata.get_ddi_type = &synaptics_get_ldi_info,

	printk(KERN_ERR "%s: panel_revision: %d, lcdtype: 0x%06X, touch_type: %d, el_type: %s, h_sync: %s, ddi_type: %s\n",
		__func__, rmi4_platformdata.panel_revision,
		lcdtype, touch_type, el_type ? "M4+" : "M4",
		!rmi4_platformdata.enable_sync ? "NA" : "OK",
		!rmi4_platformdata.get_ddi_type ? "SDC" :
		rmi4_platformdata.get_ddi_type() ? "MAGNA" : "SDC");
};

void __init synaptics_tsp_init(void)
{
	if (gpio_request(GPIO_TSP_INT, "TSP_INT")) {
		pr_err("%s : TSP_INT request port error\n", __func__);

	} else {
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_UP);
		s5p_register_gpio_interrupt(GPIO_TSP_INT);
	}

	rmi4_platformdata.gpio = GPIO_TSP_INT;
	synaptics_i2c_devs0[0].irq = gpio_to_irq(GPIO_TSP_INT);

	synaptics_verify_panel_revision();

	s3c_i2c4_set_platdata(NULL);
	i2c_register_board_info(8, synaptics_i2c_devs0,
		 ARRAY_SIZE(synaptics_i2c_devs0));

	printk(KERN_ERR "%s touch : %d\n",
		__func__, synaptics_i2c_devs0[0].irq);
}
#endif

static struct gpio_keys_platform_data xyref5260_gpiokeys_platform_data = {
	xyref5260_button,
	ARRAY_SIZE(xyref5260_button),
};


static struct platform_device xyref5260_gpio_keys = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data = &xyref5260_gpiokeys_platform_data,
	},
};

static struct platform_device *xyref5260_input_devices[] __initdata = {
	&s3c_device_i2c4,
	&xyref5260_gpio_keys,
};

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI)
void __init tsp_check_and_init(void)
{
	/* We do not need to check Ic vendor anymore */
	synaptics_tsp_init();
}
#endif

void __init exynos5_xyref5260_input_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");
#if defined(CONFIG_TOUCHSCREEN_FT5X06)
	s3c_i2c4_set_platdata(&i2c_data_touch);
	i2c_register_board_info(8, i2c_devs_touch, ARRAY_SIZE(i2c_devs_touch));
	exynos5260_touch_init();
#endif
#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI)
	tsp_check_and_init();
#endif
	xyref5260_gpio_keys_config_setup();
	platform_add_devices(xyref5260_input_devices,
			ARRAY_SIZE(xyref5260_input_devices));
}
