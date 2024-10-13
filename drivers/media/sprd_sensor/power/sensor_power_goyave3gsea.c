/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <mach/board.h>
#include <video/sensor_drv_k.h>
#include "../sensor_drv_sprd.h"

static int sensor_sr200pc20m_poweron(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	/* Set default status for main and sub sensor */

	sensor_k_sensor_sel(SENSOR_MAIN); // Select main sensor(sensor SR200PC20)

	/* Power on sequence */

	sensor_k_set_voltage_iovdd(SENSOR_VDD_1800MV); // IO vdd

	udelay(1); // Delay 1us <= 10ms

	sensor_k_set_voltage_avdd(SENSOR_VDD_2800MV); // Anolog vdd

	udelay(1); // Delay 1us <= 10ms

	sensor_k_set_voltage_dvdd(SENSOR_VDD_1800MV); // Core vdd

	mdelay(2); // Delay 2ms >= 1ms

	sensor_k_set_pd_level(1); // Power down invalid for SR200PC20

	mdelay(1); // Delay 1ms >= 1ms

	sensor_k_set_mclk(SENSOR_DEFALUT_MCLK);

	mdelay(30); // Delay 30ms >= 30ms

	sensor_k_set_rst_level(1); // Reset invalid for SR200PC20

	udelay(2); // delay 2us > 16MCLK = 16/24 us

	printk("sensor_sr200pc20m_poweron : OK\n");

	return ret;
}

static int sensor_sr200pc20m_poweroff(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	sensor_k_sensor_sel(SENSOR_MAIN); // Select main sensor(sensor SR200PC20)

	udelay(2); // Delay 2us > 16MCLK = 16/24 us

	sensor_k_set_rst_level(0); // Reset valid for SR200PC20

	udelay(2); // Delay 2us > 16MCLK = 16/24 us

	sensor_k_set_mclk(0); // Disable MCLK

	udelay(1); // Delay 1us >= 0us

	sensor_k_set_pd_level(0); // Power down valid for SR200PC20

	udelay(1); // Delay 1us >= 0us

	sensor_k_set_voltage_dvdd(SENSOR_VDD_CLOSED); // Close core vdd

	udelay(1); // Delay 1us <= 10ms

	sensor_k_set_voltage_avdd(SENSOR_VDD_CLOSED); // Close anolog vdd

	udelay(1); // Delay 1us <= 10ms

	sensor_k_set_voltage_iovdd(SENSOR_VDD_CLOSED); // Close io vdd

	udelay(1);

	printk("sensor_sr200pc20m_poweroff : OK\n");

	return ret;
}

static int sensor_sr200pc20m_sub_poweron(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	/* Set default status for main and sub sensor */

	sensor_k_sensor_sel(SENSOR_SUB); // Select sub sensor(sensor SR130PC20)

	/* Power on sequence */

	sensor_k_set_voltage_iovdd(SENSOR_VDD_1800MV); // IO vdd

	udelay(1); // Delay 1us <= 10ms

	sensor_k_set_voltage_avdd(SENSOR_VDD_2800MV); // Anolog vdd

	udelay(1); // Delay 1us <= 10ms

	sensor_k_set_voltage_dvdd(SENSOR_VDD_1800MV); // Core vdd

	mdelay(2); // Delay 2ms >= 1ms

	sensor_k_set_pd_level(1); // Power down invalid for SR200PC20

	mdelay(1); // Delay 1ms >= 1ms

	sensor_k_set_mclk(SENSOR_DEFALUT_MCLK);

	mdelay(30); // Delay 30ms >= 30ms

	sensor_k_set_rst_level(1); // Reset invalid for SR200PC20

	udelay(2); // delay 2us > 16MCLK = 16/24 us

	printk("sensor_sr200pc20m_sub_poweron : OK\n");

	return ret;
}

static int sensor_sr200pc20m_sub_poweroff(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	sensor_k_sensor_sel(SENSOR_SUB); // Select main sensor(sensor SR200PC20)

	udelay(2); // Delay 2us > 16MCLK = 16/24 us

	sensor_k_set_rst_level(0); // Reset valid for SR200PC20

	udelay(2); // Delay 2us > 16MCLK = 16/24 us

	sensor_k_set_mclk(0); // Disable MCLK

	udelay(1); // Delay 1us >= 0us

	sensor_k_set_pd_level(0); // Power down valid for SR200PC20

	udelay(1); // Delay 1us >= 0us

	sensor_k_set_voltage_dvdd(SENSOR_VDD_CLOSED); // Close core vdd

	udelay(1); // Delay 1us <= 10ms

	sensor_k_set_voltage_avdd(SENSOR_VDD_CLOSED); // Close anolog vdd

	udelay(1); // Delay 1us <= 10ms

	sensor_k_set_voltage_iovdd(SENSOR_VDD_CLOSED); // Close io vdd

	udelay(1);

	printk("sensor_sr200pc20m_sub_poweroff : OK\n");

	return ret;
}

int sensor_power_on(uint8_t sensor_id, struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	if(!main_cfg || !sub_cfg)
	{
		printk("sensor_power_on : main sensor config(0x%x) or front sensor config(0x%x) parameter is error\n", main_cfg, sub_cfg);
	}

	if(SENSOR_MAIN == sensor_id)
	{
		ret = sensor_sr200pc20m_poweron(main_cfg, sub_cfg);
	}
	else
	{
		ret = sensor_sr200pc20m_sub_poweron(main_cfg, sub_cfg);
	}

	return ret;
}

int sensor_power_off(uint8_t sensor_id, struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	if(!main_cfg || !sub_cfg)
	{
		printk("sensor_power_off : main sensor config(0x%x) or front sensor config(0x%x) parameter is error\n", main_cfg, sub_cfg);
	}

	if(SENSOR_MAIN == sensor_id)
	{
		ret = sensor_sr200pc20m_poweroff(main_cfg, sub_cfg);
	}
	else
	{
		ret = sensor_sr200pc20m_sub_poweroff(main_cfg, sub_cfg);
	}

	return ret;
}
