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

	sensor_k_sensor_sel(SENSOR_SUB);
	sensor_k_set_pd_level(0);			//power down valid for sr030pc50m
	sensor_k_set_rst_level(0);			//reset valid for sr030pc50m
	sensor_k_sensor_sel(SENSOR_MAIN);	//select main sensor(sensor S5K4ECGX);
	sensor_k_set_pd_level(0);			//power down valid for S5K4ECGX
	sensor_k_set_rst_level(0);			//reset valid for S5K4ECGX
	mdelay(1);

	/*power on sequence*/
	
	// CAM_AF_2P8 (VDDCAMA)
	sensor_k_set_voltage_cammot(SENSOR_VDD_2800MV);	//AF monitor (CAMMOT 2.8V)
	mdelay(1);
	
	// VCAM_IO_1.8V (VDDCAMD), need to delay with AVDD 2.8V < 2ms
	sensor_k_set_voltage_iovdd(SENSOR_VDD_1800MV);	//IO vdd (VDDIO 1.8V)
	udelay(200);

	// VCAM_A_2.8V_F (LDO GPIO CNT: VCAM_A_EN GPIO219), need to delay with MCLK >= 2ms
	sensor_k_set_voltage_avdd(SENSOR_VDD_2800MV);		//anolog vdd (AVDD 2.8V)
	mdelay(3);

	// MCLK, need to delay with 5M Core 1.2V >= 0us
	sensor_k_set_mclk(24);
	udelay(50);

	// VCAM_C_1.2V, (LDO GPIO CNT: 5M_CAM_CORE_EN GPIO217), need to delay with 5MP STBY > 10us
	sensor_k_set_voltage_dvdd(SENSOR_VDD_1200MV);		//core vdd (5M CORE 1.2V)
	mdelay(5);
	
	// 5Mp STBY (5M_CAM_PWDN GPIO188), need to delay with 5MP RESET > 15us
	sensor_k_set_pd_level(1);				//power down (5Mp STBY)
	udelay(30);

	// 5Mp RESET (5M_CAM_RST GPIO186), need to delay with I2C start > 60us
	sensor_k_set_rst_level(1);				//reset (5Mp RESET)
	udelay(100);

	
	printk("sensor_s5k4ecgx_poweron OK \n");

	return ret;
}

static int sensor_s5k4ecgx_poweroff(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	sensor_k_sensor_sel(SENSOR_MAIN);	//select main sensor(sensor S5K4ECGX);

	/*power off sequence*/

	// 5Mp RESET (5M_CAM_RST GPIO186), need to delay with I2C start > 0us
	udelay(2);
	sensor_k_set_rst_level(0);				//reset valid for S5K4ECGX

	// MCLK, need to delay with 5MP RESET > 50us
	udelay(100);
	sensor_k_set_mclk(0);					// disable mclk

	// 5Mp STBY (5M_CAM_PWDN GPIO188), need to delay with MCLK > 0us
	udelay(50);
	sensor_k_set_pd_level(0);				// power down (5Mp STBY)

	// VCAM_IO_1.8V (VDDCAMD), need to delay with 5MP STBY > 0us
	mdelay(1);
	sensor_k_set_voltage_iovdd(SENSOR_VDD_CLOSED);		//close IO vdd (VDDIO 1.8V)

	// VCAM_A_2.8V_F (LDO GPIO CNT: VCAM_A_EN GPIO219), need to delay with VDDIO < 10ms
	mdelay(1);
	sensor_k_set_voltage_avdd(SENSOR_VDD_CLOSED);		//close anolog vdd (AVDD 2.8V)

	// VCAM_C_1.2V, (LDO GPIO CNT: 5M_CAM_CORE_EN GPIO217), need to delay with AVDD > 0us
	mdelay(1);
	sensor_k_set_voltage_dvdd(SENSOR_VDD_CLOSED);		//close core vdd (5M CORE 1.2V)	

	// CAM_AF_2P8 (VDDCAMA)
	mdelay(1);
	sensor_k_set_voltage_cammot(SENSOR_VDD_CLOSED);	//AF monitor

	udelay(10);
	printk("sensor_s5k4ecgx_poweroff OK \n");

	return ret;

}


static int sensor_db8221a_poweron(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	sensor_k_sensor_sel(SENSOR_MAIN);//select main sensor
	sensor_k_set_pd_level(0);//power down valid for main sensor
	sensor_k_set_rst_level(0);//reset valid for main sensor
	sensor_k_sensor_sel(SENSOR_SUB);//select main sensor
	sensor_k_set_pd_level(0);//power down valid for main sensor
	sensor_k_set_rst_level(0);//reset valid for main sensor
	udelay(1);

	// VCAM_A_2.8V_F (LDO GPIO CNT: VCAM_A_EN GPIO219), need to delay with VDDIO >= 10ms
	sensor_k_set_voltage_avdd(SENSOR_VDD_2800MV); // Analog VDD 2.8V
	mdelay(15);

	// VCAM_IO_1.8V (VDDCAMD), need to delay with 5M Core VDD >= 0us
	sensor_k_set_voltage_iovdd(SENSOR_VDD_1800MV); // IO VDD 1.8V
	mdelay(1);

	// 5Mp Core VDD, pulse width is > 1ms, need to delay with MCLK >= 0us
	sensor_k_sensor_sel(SENSOR_MAIN);
	sensor_k_set_voltage_dvdd(SENSOR_VDD_1200MV); // 5M Core VDD 1.2V
	mdelay(1);
	sensor_k_set_voltage_dvdd(SENSOR_VDD_CLOSED); // 5M Core VDD 1.2V
	mdelay(1);

	sensor_k_sensor_sel(SENSOR_SUB); //select main sensor
	// MCLK, need to delay with VT Enable > 0us
	sensor_k_set_mclk(24);
	udelay(300);

	// VT Enable (VGA_CAM_STBY GPIO238), need to delay with VT Reset >= 20us
	sensor_k_set_pd_level(1);
	mdelay(1);

	// VT RESET (VGA_CAM_RST GPIO187), need to delay with I2C start >= 70000MCLK (70000MCLK = 70000MCLK/24MHz = 2.9 ms)
	sensor_k_set_rst_level(1);
	mdelay(5);
	
	printk("db8221a_poweron OK \n");
	return ret;
}

static int sensor_db8221a_poweroff(struct sensor_power *main_cfg, struct sensor_power *sub_cfg)
{
	int ret = 0;

	sensor_k_sensor_sel(SENSOR_SUB);//select sub sensor

	// VT RESET (VGA_CAM_RST GPIO187), need to delay with I2C start >= 16MCLK (16MCLK = 16/24 us = 2/3 us)
	mdelay(1);
	sensor_k_set_rst_level(0);
	
	// VT Enable (VGA_CAM_STBY GPIO238), need to delay with VT RESET >= 10us
	udelay(50);
	sensor_k_set_pd_level(0);

	// MCLK, need to delay with MCLK >= 0us
	udelay(50);
	sensor_k_set_mclk(0);

	// VCAM_A_2.8V_F (LDO GPIO CNT: VCAM_A_EN GPIO219), need to delay with VT Enable > 0ms
	mdelay(1);
	sensor_k_set_voltage_avdd(SENSOR_VDD_CLOSED);

	// VCAM_IO_1.8V (VDDCAMD), need to delay with AVDD > 0ms
	mdelay(1);
	sensor_k_set_voltage_iovdd(SENSOR_VDD_CLOSED);

	udelay(1);
	printk("sensor_db8221a_poweroff: OK \n");
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
		ret = sensor_db8221a_poweron(main_cfg, sub_cfg);
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
		ret = sensor_db8221a_poweroff(main_cfg, sub_cfg);
	}

	return ret;
}

