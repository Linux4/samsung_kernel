/*
 *  sm5703.h
 *  Samsung mfd core driver header file for the SM5703.
 *
 *  Copyright (C) 2016 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SM5703_H__
#define __SM5703_H__
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/battery/sec_charging_common.h>

#define MFD_DEV_NAME "sm5703"

#if defined(CONFIG_SS_VIBRATOR)

struct sm5703_haptic_platform_data
{
    int mode;
    int divisor;    /* PWM Frequency Divisor. 32, 64, 128 or 256 */
};

#endif

struct sm5703_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	bool wakeup;

    struct muic_platform_data *muic_pdata;
    sec_charger_platform_data_t *charger_data;
	sec_fuelgauge_platform_data_t *fuelgauge_data;

#if defined(CONFIG_SS_VIBRATOR)
	/* haptic motor data */
	struct sm5703_haptic_platform_data *haptic_data;
#endif
	struct mfd_cell *sub_devices;
	int num_subdevs;
};

#endif /* __SM5703_H__ */

