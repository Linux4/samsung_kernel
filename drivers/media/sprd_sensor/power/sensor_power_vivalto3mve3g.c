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

static int sensor_sr352_poweron(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	sensor_k_sensor_sel(SENSOR_MAIN);//select main sensor(sensor hi544);
	sensor_k_set_pd_level(0);//power down valid for hi544
	sensor_k_set_rst_level(0);//reset valid for hi544
	udelay(1);
	sensor_k_set_voltage_iovdd(SENSOR_VDD_1800MV);//IO vdd
	udelay(1);
	sensor_k_set_voltage_avdd(SENSOR_VDD_2800MV);
	udelay(1);
	sensor_k_set_voltage_dvdd(SENSOR_VDD_1300MV);
	mdelay(3);
	sensor_k_set_mclk(SENSOR_DEFALUT_MCLK);
	mdelay(3);    
	sensor_k_set_pd_level(1);
	mdelay(10);
	sensor_k_set_rst_level(1);
	udelay(2);//delay 2us > 16MCLK = 16/24 us
	printk("sr352_poweron OK \n");

	return ret;
}

static int sensor_sr352_poweroff(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	sensor_k_sensor_sel(SENSOR_MAIN);//select main sensor(sensor hi255);
	udelay(2);//delay 2us > 16MCLK = 16/24 us
	sensor_k_set_rst_level(0);//reset valid for hi255	
	mdelay(10);//delay 2us > 16MCLK = 16/24 us
	sensor_k_set_mclk(0);// disable mclk
	udelay(1);//delay 1us > 0ns
	sensor_k_set_pd_level(0);//power down valid for hi255
	udelay(1);
	sensor_k_set_voltage_dvdd(SENSOR_VDD_CLOSED);
	udelay(1);
	sensor_k_set_voltage_avdd(SENSOR_VDD_CLOSED);
	udelay(1);
	sensor_k_set_voltage_iovdd(SENSOR_VDD_CLOSED);
	udelay(1);
	printk("sr352_poweroff OK \n");

	return ret;
}

int sensor_power_on(uint8_t sensor_id, struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	if (!main_cfg /*|| !sub_cfg*/) {
		printk("sensor_power_on, para err 0x%x 0x%x \n", main_cfg, sub_cfg);
	}
	if (SENSOR_MAIN == sensor_id) {
		ret = sensor_sr352_poweron(main_cfg, sub_cfg);
	} else {
		//ret = sensor_hi255_poweron(main_cfg, sub_cfg);
	}
	return ret;
}

int sensor_power_off(uint8_t sensor_id, struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	if (!main_cfg /*|| !sub_cfg*/) {
		printk("sensor_power_off, para err 0x%x 0x%x \n", main_cfg, sub_cfg);
	}

	if (SENSOR_MAIN == sensor_id) {
		ret = sensor_sr352_poweroff(main_cfg, sub_cfg);
	} else {
		//ret = sensor_hi255_poweroff(main_cfg, sub_cfg);
	}

	return ret;
}
