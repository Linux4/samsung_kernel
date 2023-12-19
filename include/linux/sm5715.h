/*
 *  sm5715.h - mfd driver for SM5715.
 *
 *  Copyright (C) 2019 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SM5715_H__
#define __SM5715_H__
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define MFD_DEV_NAME "sm5715"

struct sm5715_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	bool wakeup;
};
#if IS_ENABLED(CONFIG_LEDS_SM5715)
/* For SM5715 Flash LED */
enum sm5715_fled_mode {
	SM5715_FLED_MODE_OFF = 1,
	SM5715_FLED_MODE_MAIN_FLASH,
	SM5715_FLED_MODE_TORCH_FLASH,
	SM5715_FLED_MODE_PREPARE_FLASH,
	SM5715_FLED_MODE_CLOSE_FLASH,
	SM5715_FLED_MODE_PRE_FLASH,
};

/* For SM5715 FLEDCNTL1(0x41) FLEDEN[1:0]*/
enum {
	FLEDEN_DISABLE   = 0x0,
	FLEDEN_TORCH_ON  = 0x1,
	FLEDEN_FLASH_ON  = 0x2,
	FLEDEN_EXTERNAL  = 0x3,
};

struct sm5715_fled_platform_data {
	struct {
		const char *name;
		u8 flash_brightness;
		u8 preflash_brightness;
		u8 torch_brightness;
		u8 timeout;
		u8 factory_current;

		int fen_pin;            /* GPIO-pin for Flash */
		int men_pin;            /* GPIO-pin for Torch */

		bool used_gpio_ctrl;
		int sysfs_input_data;

		bool en_prepare_fled;	/* prepare_flash function */
		bool en_fled;
		bool en_mled;
	} led;
};

struct sm5715_fled_data {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex fled_mutex;

	struct sm5715_fled_platform_data *pdata;
	struct device *rear_fled_dev;

	int input_voltage;
};

#if defined(CONFIG_SEC_M44X_PROJECT)
extern ssize_t sm5714_rear_flash_store(const char *buf);
extern ssize_t sm5714_rear_flash_show(char *buf);
#endif
extern int32_t sm5715_fled_mode_ctrl(int state, uint32_t brightness);
#endif
#endif /* __SM5715_H__ */

