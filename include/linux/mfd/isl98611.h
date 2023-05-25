/*
 * ISL98611 MFD Driver
 *
  * Copyright 2015 Samsung Electronics
  *
 * Author: Ravikant Sharma(ravikant.s2@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __MFD_ISL98611_H__
#define __MFD_ISL98611_H__

#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/regmap.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>



#define ISL98611_MAX_REGISTERS		0x10

#define ISL98611_NUM_REGULATORS		3

/*
 * struct isl98611_backlight_platform_data
 * @name: Backlight driver name
 * @is_full_strings: set true if two strings are used
 * @pwm_period: Platform specific PWM period value. unit is nano
 */
struct isl98611_backlight_platform_data {
	const char *name;
	bool is_full_strings;

	/* Only valid in case of PWM mode */
	unsigned int pwm_period;

	unsigned	 int gpio_backlight_en;
	u32 en_gpio_flags;

	unsigned	 int gpio_backlight_panel_enp;
	u32 panel_enp_gpio_flags;

	unsigned	 int gpio_backlight_panel_enn;
	u32 panel_enn_gpio_flags;
};

/*
 * struct isl98611_flash_platform_data
 * @name: flash driver name
 * @is_full_strings: set true if two strings are used
 */
struct isl98611_flash_platform_data {
	const char *name;
	bool is_full_strings;

	unsigned int gpio_flash_en;
	u32 flash_en_gpio_flags;

	unsigned int gpio_flash_int;
	u32 flash_int_gpio_flags;
	struct pinctrl *fled_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};

/*
 * struct isl98611_platform_data
 * @en_gpio: GPIO for chip enable pin
 * @lcm_en1_gpio: GPIO for VPOS LDO
 * @lcm_en2_gpio: GPIO for VNEG LDO
 * @regulator_data: Regulator initial data for LCD bias
 * @bl_pdata: Backlight platform data
 */
struct isl98611_platform_data {
	int en_gpio;
	int lcm_en1_gpio;
	int lcm_en2_gpio;
	int i2c_scl_gpio;
	int i2c_sda_gpio;
	struct regulator_init_data *regulator_data[ISL98611_NUM_REGULATORS];
	struct isl98611_backlight_platform_data *bl_pdata;
};

/*
 * struct isl98611
 * @dev: Parent device pointer
 * @regmap: Used for i2c communcation on accessing registers
 * @pdata: LMU platform specific data
 */
struct isl98611 {
	struct device *dev;
	struct regmap *regmap;
	struct isl98611_platform_data *pdata;
	struct i2c_client *client;
	struct mutex isl98611_lock;
};

int isl98611_read_byte(struct isl98611 *isl98611, u8 reg, u8 *read);
int isl98611_write_byte(struct isl98611 *isl98611, u8 reg, u8 data);
int isl98611_update_bits(struct isl98611 *isl98611, u8 reg, u8 mask, u8 data);
void isl98611_gpio_control(int enable)   ;

#endif
