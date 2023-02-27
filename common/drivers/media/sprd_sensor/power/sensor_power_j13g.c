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
#include <linux/kernel.h>
#include <mach/hardware.h>
#include <mach/board.h>
#include "../sensor_drv_sprd.h"

static int sensor_sr541_poweron(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	printk("sensor_sr541_poweron : Start\n");

	int ret = 0;

	sensor_k_sensor_sel(SENSOR_MAIN); // Select main sensor (SR541)

	sensor_k_set_voltage_cammot(SENSOR_VDD_2800MV); // Anolog VDD

	mdelay(2); // 2 msec dealy

	sensor_k_set_voltage_iovdd(SENSOR_VDD_1800MV);

	mdelay(3);

	sensor_k_set_voltage_avdd(SENSOR_VDD_2800MV); // Anolog

	mdelay(3);

	sensor_k_set_voltage_dvdd(SENSOR_VDD_1200MV); // Core VDD

	mdelay(10);

	sensor_k_set_pd_level(1); // Power Down

	mdelay(2);

	sensor_k_set_mclk(24);

	mdelay(30);

	sensor_k_set_rst_level(1); // Reset

	mdelay(3);

	printk("sensor_sr541_poweron : End\n");

	return ret;
}


static int sensor_sr541_poweroff(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	printk("sensor_sr541_poweroff : Start\n");

	sensor_k_sensor_sel(SENSOR_MAIN); // Select main sensor (SR541)
	sensor_k_set_rst_level(0); // Reset valid for SR541

	mdelay(11); // Min 10msec delay

	sensor_k_set_mclk(0); // Disable MCLK

	mdelay(1);

	sensor_k_set_pd_level(0); // Power down valid for SR541

	mdelay(10);

	sensor_k_set_voltage_dvdd(SENSOR_VDD_CLOSED); // Close Core VDD

	mdelay(3);

	sensor_k_set_voltage_avdd(SENSOR_VDD_CLOSED); //Close Anolog VDD

	mdelay(3);

	sensor_k_set_voltage_iovdd(SENSOR_VDD_CLOSED);

	mdelay(3);

	sensor_k_set_voltage_cammot(SENSOR_VDD_CLOSED); //AF monitor

	mdelay(3);

	printk("sensor_sr541_poweroff : End\n");

	return ret;
}


static int sensor_hi255_poweron(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	printk("sensor_hi255_poweron : Start\n");

	sensor_k_sensor_sel(SENSOR_MAIN);

	sensor_k_set_pd_level(0);
	sensor_k_set_rst_level(0);

	sensor_k_sensor_sel(SENSOR_SUB);

	sensor_k_set_pd_level(0);
	sensor_k_set_rst_level(0);

	udelay(1);

	sensor_k_set_voltage_iovdd(SENSOR_VDD_1800MV);
	sensor_k_set_voltage_avdd(SENSOR_VDD_2800MV);

	mdelay(2);

	sensor_k_set_pd_level(1);

	mdelay(2);

	sensor_k_set_mclk(24);

	mdelay(30);

	sensor_k_set_rst_level(1);

	udelay(2);

	printk("sensor_hi255_poweron : End\n");

	return ret;
}

static int sensor_hi255_poweroff(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	printk("sensor_hi255_poweroff : Start\n");

	sensor_k_sensor_sel(SENSOR_SUB);

	udelay(2);

	sensor_k_set_rst_level(0);

	mdelay(10);

	sensor_k_set_mclk(0);

	mdelay(10);

	sensor_k_set_pd_level(0);

	mdelay(10);

	sensor_k_set_voltage_avdd(SENSOR_VDD_CLOSED);

	mdelay(3);

	sensor_k_set_voltage_iovdd(SENSOR_VDD_CLOSED);

	mdelay(3);

	printk("sensor_hi255_poweroff : End\n");

	return ret;
}

int sensor_power_on(uint8_t sensor_id, struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	if (SENSOR_MAIN == sensor_id)
	{
		ret = sensor_sr541_poweron(main_cfg, sub_cfg);
	}
	else
	{
		ret = sensor_hi255_poweron(main_cfg, sub_cfg);
	}
	return ret;
}

int sensor_power_off(uint8_t sensor_id, struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	if (SENSOR_MAIN == sensor_id)
	{
		ret = sensor_sr541_poweroff(main_cfg, sub_cfg);
	}
	else
	{
		ret = sensor_hi255_poweroff(main_cfg, sub_cfg);
	}
	return ret;
}

