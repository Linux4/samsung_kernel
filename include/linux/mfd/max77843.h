/*
 * max77843.h - Driver for the Maxim 77843
 *
 *  Copyright (C) 2011 Samsung Electrnoics
 *  Seoyoung Jeong <seo0.jeong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max8997.h
 *
 * MAX77843 has Flash LED, SVC LED, Haptic, MUIC devices.
 * The devices share the same I2C bus and included in
 * this mfd driver.
 */

#ifndef __MAX77843_H__
#define __MAX77843_H__
#include <linux/platform_device.h>
#include <linux/regmap.h>

#if defined(CONFIG_CHARGER_MAX77843) && defined(CONFIG_FUELGAUGE_MAX77843)
#include <linux/battery/sec_charger.h>
#include <linux/battery/sec_fuelgauge.h>
#endif

#define MFD_DEV_NAME "max77843"
#define M2SH(m) ((m) & 0x0F ? ((m) & 0x03 ? ((m) & 0x01 ? 0 : 1) : ((m) & 0x04 ? 2 : 3)) : \
		((m) & 0x30 ? ((m) & 0x10 ? 4 : 5) : ((m) & 0x40 ? 6 : 7)))

struct max77843_haptic_platform_data {
	u16 max_timeout;
	u16 duty;
	u16 period;
	u16 reg2;
	char *regulator_name;
	unsigned int pwm_id;

	void (*init_hw) (void);
	void (*motor_en) (bool);
};

struct max77843_regulator_data {
	int id;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

struct max77843_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct muic_platform_data *muic_pdata;

#if defined(CONFIG_CHARGER_MAX77843) && defined(CONFIG_FUELGAUGE_MAX77843)
	sec_battery_platform_data_t *charger_data;
	sec_battery_platform_data_t *fuelgauge_data;
#endif

	int num_regulators;
	struct max77843_regulator_data *regulators;
	/* haptic motor data */
	struct max77843_haptic_platform_data *haptic_data;
	struct mfd_cell *sub_devices;
	int num_subdevs;
};

struct max77843
{
	struct regmap *regmap;
};

#endif /* __MAX77843_H__ */

