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

#include <soc/sprd/hardware.h>

#include <soc/sprd/board.h>

#include "../sensor_drv_sprd.h"



static int sensor_s5k4h5yc_poweron(uint32_t *fd_handle, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)
{
	int ret = 0;

	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_1); // Select Sub Sensor
	//sensor_k_set_pd_level(fd_handle, 0); // Power down
	sensor_k_set_rst_level(fd_handle, 0); // Reset
	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_0); // Select Main Sensor
	//sensor_k_set_pd_level(fd_handle, 0); // Power down
	sensor_k_set_rst_level(fd_handle, 0); // Reset
	udelay(100);
	sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_2800MV); // 8M AVDD 2.8V
	udelay(100);
	sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_1200MV); // 8M DVDD 1.2V
	udelay(10);
	sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_2800MV); // AF 2.8V
	mdelay(10); // The minimum timing between AF 2.8V and AF I2C is 15msec
	sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_1800MV); // IO 1.8V (A.K.A Host)
	mdelay(3); // 3msec delay (Min 2msec)
	sensor_k_set_mclk(fd_handle, 24); // MCLK
	udelay(2000);
	sensor_k_set_rst_level(fd_handle, 1); // Reset
	udelay(2000);
	
	printk("s5k4h5yc_poweron OK \n");

	return ret;
}



static int sensor_s5k4h5yc_poweroff(uint32_t *fd_handle, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)

{

	int ret = 0;

	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_1);//select sub sensor

	sensor_k_set_rst_level(fd_handle, 0);//reset
	
	//sensor_k_set_pd_level(fd_handle, 0);//power down

	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_0);//select main sensor
	
	sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_CLOSED);
	
	udelay(150);
	
	sensor_k_set_mclk(fd_handle, 0);// disable mclk

	//sensor_k_set_pd_level(fd_handle, 0);//power down
	
	udelay(100);//delay 6ms < 10ms

	sensor_k_set_rst_level(fd_handle, 0);//reset

	udelay(100);//delay 2us > 16MCLK = 16/24 us
	
	sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);
	
	udelay(200);//delay 6ms < 10ms
	
	sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_CLOSED);
	
	udelay(200);
	
	sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
	
	udelay(200);
	
	

	printk("s5k4h5yc_poweroff OK \n");
return ret;

}



static int sensor_s5k5e3yx_poweron(uint32_t *fd_handle, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)

{

	int ret = 0;
	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_0);//select sub sensor
	
	//sensor_k_set_pd_level(fd_handle, 0);//power down
	
	sensor_k_set_rst_level(fd_handle, 0);//reset
	
	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_1);//select main sensor
	
	sensor_k_set_rst_level(fd_handle, 0);//reset
	

	sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_2800MV);
	udelay(100);
	sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_1200MV);
	udelay(50);
	sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_1800MV);
	
	udelay(10);
	
	sensor_k_set_rst_level(fd_handle, 1);
	
	udelay(300);
	
	sensor_k_set_mclk(fd_handle, 24);
	
	udelay(300);//delay 6ms < 10ms

	printk("s5k5e3yx poweron OK \n");

	return ret;

}



static int sensor_s5k5e3yx_poweroff(uint32_t *fd_handle, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)
{
	int ret = 0;

	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_1);//select main sensor
	sensor_k_set_mclk(fd_handle, 0);// disable mclk
	udelay(10);//delay 2us > 16MCLK = 16/24 us
	//sensor_k_set_pd_level(fd_handle, 0);//power down
	sensor_k_set_rst_level(fd_handle, 0);//reset
	udelay(10);
	sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);
	udelay(10);
	sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_CLOSED);
	udelay(190);
	sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
	udelay(10);

	printk("s5k5e3yx poweroff OK \n");
	return ret;
}



int sensor_power_on(uint32_t *fd_handle, uint32_t sensor_id, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)

{

	int ret = 0;



	if (SENSOR_DEV_0 == sensor_id) {

		ret = sensor_s5k4h5yc_poweron(fd_handle, dev0, dev1, dev2);

	} else if (SENSOR_DEV_1 == sensor_id) {

		ret = sensor_s5k5e3yx_poweron(fd_handle, dev0, dev1, dev2);

	}

	return ret;

}



int sensor_power_off(uint32_t *fd_handle, uint32_t sensor_id, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)

{

	int ret = 0;



	if (SENSOR_DEV_0 == sensor_id) {

		ret = sensor_s5k4h5yc_poweroff(fd_handle, dev0, dev1, dev2);

	} else if (SENSOR_DEV_1 == sensor_id) {

		ret = sensor_s5k5e3yx_poweroff(fd_handle, dev0, dev1, dev2);

	}



	return ret;

}

