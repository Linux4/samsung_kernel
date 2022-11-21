/*
 * max17048_fuelgauge.h
 * Samsung MAX17048 Fuel Gauge Header
 *
 * Copyright (C) 2014 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MAX17048_FUELGAUGE_H
#define __MAX17048_FUELGAUGE_H __FILE__

#include <linux/battery/sec_battery.h>
#include <linux/of_gpio.h>

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */
#define SEC_FUELGAUGE_I2C_SLAVEADDR (0x6D >> 1)

#define MAX17048_VCELL_MSB	0x02
#define MAX17048_VCELL_LSB	0x03
#define MAX17048_SOC_MSB	0x04
#define MAX17048_SOC_LSB	0x05
#define MAX17048_MODE_MSB	0x06
#define MAX17048_MODE_LSB	0x07
#define MAX17048_VER_MSB	0x08
#define MAX17048_VER_LSB	0x09
#define MAX17048_RCOMP_MSB	0x0C
#define MAX17048_RCOMP_LSB	0x0D
#define MAX17048_OCV_MSB	0x0E
#define MAX17048_OCV_LSB	0x0F
#define MAX17048_CMD_MSB	0xFE
#define MAX17048_CMD_LSB	0xFF

#define RCOMP0_TEMP	20
#define AVER_SAMPLE_CNT		5

struct max17048_fuelgauge_battery_data_t {
	u8 RCOMP0;
	u8 RCOMP_charging;
	int temp_cohot;
	int temp_cocold;
	bool is_using_model_data;
	u8 *type_str;
};

struct max17048_fuelgauge_data {
	struct i2c_client		*client;
	sec_fuelgauge_platform_data_t *pdata;
	struct max17048_fuelgauge_battery_data_t *battery_data;
	struct power_supply		psy_fg;
	struct delayed_work isr_work;

	int cable_type;
	bool is_charging;

	/* HW-dedicated fuel guage info structure
	 * used in individual fuel gauge file only
	 * (ex. dummy_fuelgauge.c)
	 */

	bool is_fuel_alerted;
	struct wake_lock fuel_alert_wake_lock;

	unsigned int capacity_old;	/* only for atomic calculation */
	unsigned int capacity_max;	/* only for dynamic calculation */

	bool initial_update_of_soc;
	struct mutex fg_lock;

	/* register programming */
	int reg_addr;
	u8 reg_data[2];

	int fg_irq;
};

ssize_t max17048_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t max17048_fg_store_attrs(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count);

#define MAX17048_FG_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = max17048_fg_show_attrs,			\
	.store = max17048_fg_store_attrs,			\
}

enum {
	FG_REG = 0,
	FG_DATA,
	FG_REGS,
};

#endif /* __MAX17048_FUELGAUGE_H */
