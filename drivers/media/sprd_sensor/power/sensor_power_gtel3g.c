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

static int sensor_s5k4ecgx_poweron(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	sensor_k_sensor_sel(SENSOR_MAIN);	//select main sensor(sensor S5K4ECGX);
	sensor_k_set_pd_level(0);			//power down valid for hi544
	sensor_k_set_rst_level(0);			//reset valid for hi544
	mdelay(1);

	/*power on sequence*/
	sensor_k_set_voltage_cammot(SENSOR_VDD_2800MV);	//AF monitor
	mdelay(1);
	sensor_k_set_voltage_iovdd(SENSOR_VDD_1800MV);	//IO vdd (VDDIO 1.8V)
	udelay(100);	//delay <2ms
	sensor_k_set_voltage_avdd(SENSOR_VDD_2800MV);		//anolog vdd (AVDD 2.8V)
	mdelay(1);	//delay >0us
	sensor_k_set_mclk(24);
	mdelay(1);	//delay >=0us
	sensor_k_set_voltage_dvdd(SENSOR_VDD_1200MV);		//core vdd (5M CORE 1.2V)
	udelay(1000);	//delay >10us
	sensor_k_set_pd_level(1);						//power down (5Mp STBY)
	udelay(30);//delay > 15us
	sensor_k_set_rst_level(1);						//reset (5Mp RESET)
	udelay(100);//delay > 60us
	printk("sensor_s5k4ecgx_poweron OK \n");
	return ret;
}

static int sensor_s5k4ecgx_poweroff(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	sensor_k_sensor_sel(SENSOR_MAIN);	//select main sensor(sensor S5K4ECGX);

	/*power off sequence*/
	udelay(2);	//delay >0us
	sensor_k_set_rst_level(0);						//reset valid for S5K4ECGX
	udelay(100);	//delay >50us
	sensor_k_set_mclk(0);							// disable mclk
	udelay(100);	//delay >0us
	sensor_k_set_pd_level(0);						// power down (5Mp STBY)
	udelay(100);	//delay >0us
	sensor_k_set_voltage_avdd(SENSOR_VDD_CLOSED);		//close anolog vdd (AVDD 2.8V)
	mdelay(1);	//delay <10ms
	sensor_k_set_voltage_iovdd(SENSOR_VDD_CLOSED);	//close IO vdd (VDDIO 1.8V)
	mdelay(5);	//delay > 0us
	sensor_k_set_voltage_dvdd(SENSOR_VDD_CLOSED);		//close core vdd (5M CORE 1.2V)	
	mdelay(1);	//delay
	sensor_k_set_voltage_cammot(SENSOR_VDD_CLOSED);	//AF monitor
	udelay(10);	//delay

	printk("sensor_s5k4ecgx_poweroff OK \n");
	return ret;

}

static int sensor_sr200pc20m_poweron(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	/* Set default status for main and sub sensor */
	sensor_k_sensor_sel(SENSOR_SUB);	// Select sub sensor(sensor SR200PC20)

	/*power on sequence*/
	sensor_k_set_voltage_iovdd(SENSOR_VDD_1800MV);	// IO vdd (VDDIO 1.8V)
	mdelay(1);	// Delay <10ms
	sensor_k_set_voltage_avdd(SENSOR_VDD_2800MV);		// Anolog vdd (AVDD 2.8V)
	mdelay(1);	// Delay <10ms
	sensor_k_set_voltage_dvdd(SENSOR_VDD_1200MV);		//core vdd (5M CORE 1.2V)
	mdelay(2);	// Delay >1ms
	sensor_k_set_voltage_dvdd(SENSOR_VDD_CLOSED);		//close core vdd (5M CORE 1.2V)
	mdelay(3);	// Delay >1ms
	sensor_k_set_pd_level(1);						// Power down invalid for SR200PC20 (VT Enable)
	mdelay(2);	// Delay >1ms
	sensor_k_set_mclk(SENSOR_DEFALUT_MCLK);			// MCLK
	mdelay(30);	// Delay >30ms
	sensor_k_set_rst_level(1);						// Reset invalid for SR200PC20 (VT RESET)
	udelay(2);	// Delay >2us (16MCLK = 16/24 us )

	printk("sensor_sr200pc20m_poweron : OK\n");

	return ret;
}

static int sensor_sr200pc20m_poweroff(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	sensor_k_sensor_sel(SENSOR_SUB); // Select sub sensor(sensor SR200PC20)

	/*power off sequence*/
	sensor_k_set_rst_level(0);						// Reset valid for SR200PC20
	mdelay(12);	// Delay >16clk
	sensor_k_set_mclk(0);							// Disable MCLK
	mdelay(3);	//Delay >0us
	sensor_k_set_pd_level(0);						// Power down valid for SR200PC20
	mdelay(1);	//Delay >0us
	sensor_k_set_voltage_avdd(SENSOR_VDD_CLOSED);		// Close anolog vdd (AVDD 2.8V)
	udelay(30);	// Delay <10ms
	sensor_k_set_voltage_iovdd(SENSOR_VDD_CLOSED);	// close IO vdd (VDDIO 1.8V)
	udelay(1);

	printk("sensor_sr200pc20m_poweroff : OK\n");

	return ret;
}

int sensor_power_on(uint8_t sensor_id, struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	if (!main_cfg || !sub_cfg) {
		printk("sensor_power_on, para err 0x%x 0x%x \n", main_cfg, sub_cfg);
	}
	if (SENSOR_MAIN == sensor_id) {
		ret = sensor_s5k4ecgx_poweron(main_cfg, sub_cfg);
	} else {
		ret = sensor_sr200pc20m_poweron(main_cfg, sub_cfg);
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
		ret = sensor_s5k4ecgx_poweroff(main_cfg, sub_cfg);
	} else {
		ret = sensor_sr200pc20m_poweroff(main_cfg, sub_cfg);
	}

	return ret;
}
