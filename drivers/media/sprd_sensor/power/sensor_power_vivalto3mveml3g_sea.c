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

	/*set default status for main and sub sensor*/
	sensor_k_sensor_sel(SENSOR_SUB);//select sub sensor(sensor sr030pc50);
	sensor_k_set_pd_level(0);//power down valid for sr030pc50
	sensor_k_set_rst_level(0);//reset valid for sr030pc50

	sensor_k_sensor_sel(SENSOR_MAIN);//select main sensor(sensor sr352);
	sensor_k_set_pd_level(0);//power down valid for sr352
	sensor_k_set_rst_level(0);//reset valid for sr352
	msleep(1);

	/*power on sequence*/
	// Sensor I/O : 1.8V On	
	sensor_k_set_voltage_iovdd(SENSOR_VDD_1800MV);//IO vdd
	msleep(2);	
	// Sensor AVDD : 2.8V On	
	sensor_k_set_voltage_avdd(SENSOR_VDD_2800MV);//anolog vdd
	msleep(2);
	// 3M Core : 1.3V On
	sensor_k_set_voltage_dvdd(SENSOR_VDD_1300MV);//core vdd	
	msleep(2);
	// VT Core : 1.8V On
	sensor_k_sensor_sel(SENSOR_SUB);
	sensor_k_set_voltage_dvdd(SENSOR_VDD_1800MV);
	msleep(4);
	sensor_k_sensor_sel(SENSOR_MAIN);
	//MCLK enable
	sensor_k_set_mclk(24);
	//3M STBY EN
	sensor_k_set_pd_level(1);
	msleep(20);
	//3M RESET
	sensor_k_set_rst_level(1);
	msleep(2);

	//log print
	printk("sensor_sr352_poweron OK \n");
	return ret;
}

static int sensor_sr352_poweroff(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	/*power off sequence*/
	sensor_k_sensor_sel(SENSOR_MAIN);//select main sensor(sensor sr352);
	msleep(2);
	// 3M Reset Disable
	sensor_k_set_rst_level(0);//reset valid for main sensor
	udelay(20);	
	// Mclk Disable	
	sensor_k_set_mclk(0);
	udelay(1);
	// 3M Stby Disable
	sensor_k_set_pd_level(0);//power down
	udelay(1);
	//3M core off
	sensor_k_set_voltage_dvdd(SENSOR_VDD_CLOSED);//close core vdd
	mdelay(1);//delay
	// VT Core : 1.8V Off
	sensor_k_sensor_sel(SENSOR_SUB);
	sensor_k_set_voltage_dvdd(SENSOR_VDD_CLOSED);//close core vdd
	mdelay(1);	
	//AVDD 2.8 V off
	sensor_k_set_voltage_avdd(SENSOR_VDD_CLOSED);
	mdelay(1);	
	//Sensor I/O off
	sensor_k_set_voltage_iovdd(SENSOR_VDD_CLOSED);//IO vdd
	mdelay(1);

	printk("sensor_sr352_poweroff OK \n");
	return ret;
}


static int sensor_sr030pc50_poweron(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	sensor_k_sensor_sel(SENSOR_MAIN);//select main sensor
	sensor_k_set_pd_level(0);//power down valid for main sensor
	sensor_k_set_rst_level(0);//reset valid for main sensor

	sensor_k_sensor_sel(SENSOR_SUB);//select sub sensor
	sensor_k_set_pd_level(0);//power down valid for sub sensor
	sensor_k_set_rst_level(0);//reset valid for sub sensor
	udelay(1);

	/*power on sequence*/
	// Sensor I/O : 1.8V On
	sensor_k_set_voltage_iovdd(SENSOR_VDD_1800MV);//IO vdd
	msleep(2);
	// Sensor AVDD : 2.8VOn
	sensor_k_set_voltage_avdd(SENSOR_VDD_2800MV);//anolog vdd
	msleep(2);
	// 3M Core : 1.3V On	
	sensor_k_sensor_sel(SENSOR_MAIN);//select main sensor	
	sensor_k_set_voltage_dvdd(SENSOR_VDD_1300MV);//core vdd	
	msleep(2);
	// VT Core : 1.8V On
	sensor_k_sensor_sel(SENSOR_SUB);
	sensor_k_set_voltage_dvdd(SENSOR_VDD_1800MV);
	msleep(4);
	// MCLK Enable	
	sensor_k_set_mclk(24);
	// VT STBY Enable
	sensor_k_set_pd_level(1);//power down
	msleep(32);
	//VT Reset Enable
	sensor_k_set_rst_level(1);//reset valid for sub sensor	
	msleep(2);

	printk("sensor_sr030pc50_poweron OK \n");
	return ret;	
}

static int sensor_sr030pc50_poweroff(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	//VT power off sequence
	sensor_k_sensor_sel(SENSOR_SUB);//select sub sensor
	mdelay(2);//delay > 1ms
	//VT reset disable
	sensor_k_set_rst_level(0);//reset valid for sub sensor
	mdelay(2);
	//MCLK disable
	sensor_k_set_mclk(0);
	udelay(2);//delay 2us > 16MCLK = 16/24 us
	//VT STBY disable
	sensor_k_set_pd_level(0);
	msleep(1);
	//MAIN CORE off
	sensor_k_sensor_sel(SENSOR_MAIN);
	sensor_k_set_voltage_dvdd(SENSOR_VDD_CLOSED);
	msleep(1);
	//VT core off
	sensor_k_sensor_sel(SENSOR_SUB);
	sensor_k_set_voltage_dvdd(SENSOR_VDD_CLOSED);
	msleep(1);
	//Sensor Avdd 2.8V off
	sensor_k_set_voltage_avdd(SENSOR_VDD_CLOSED);
	msleep(1);	
	// Sub IO off
	sensor_k_set_voltage_iovdd(SENSOR_VDD_CLOSED);
	msleep(1);

	printk("sensor_sr030pc50_poweroff OK \n");
	return ret;
}

int sensor_power_on(uint8_t sensor_id, struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	if (!main_cfg || !sub_cfg) {
		printk("sensor_power_on, para err 0x%x 0x%x \n", main_cfg, sub_cfg);
	}
	if (SENSOR_MAIN == sensor_id) {
		ret = sensor_sr352_poweron(main_cfg, sub_cfg);
	} else {
		ret = sensor_sr030pc50_poweron(main_cfg, sub_cfg);
	}
	return ret;
}

int sensor_power_off(uint8_t sensor_id, struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	if (!main_cfg || !sub_cfg) {
		printk("sensor_power_off, para err 0x%x 0x%x \n", main_cfg, sub_cfg);
	}

	if (SENSOR_MAIN == sensor_id) {
		ret = sensor_sr352_poweroff(main_cfg, sub_cfg);
	} else {
		ret = sensor_sr030pc50_poweroff(main_cfg, sub_cfg);
	}

	return ret;
}