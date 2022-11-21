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

#include <video/sensor_drv_k.h>
#include "../sensor_drv_sprd.h"

#define SR030PC50_I2C_SLAVE_ADDR       0x30
#define GC0310_I2C_SLAVE_ADDR          0x21

static int sensor_hi544_poweron(uint32_t *fd_handle, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)
{
	int ret = 0;

	printk("sensor_hi544_poweron\n");

	/*set default status for main and sub sensor*/
	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_1);//select sub sensor(sensor sr030);
	sensor_k_set_pd_level(fd_handle, 0);//power down valid for sr030
	sensor_k_set_rst_level(fd_handle, 0);//reset valid for sr030

	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_0);//select main sensor(sensor hi544);
	sensor_k_set_pd_level(fd_handle, 0);//power down valid for hi544
	sensor_k_set_rst_level(fd_handle, 0);//reset valid for hi544

	mdelay(1);

	/*power on sequence*/

	// VDDI18 - VDD_CAM_IO_1P8 - "vddcamd"
	sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_1800MV);
	// VDDI18 ~ VDDA28 delay >= 0us
	mdelay(1);
	// VDDA28 - VDD_CAM_A2P8_F - "vddcama"
	sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_2800MV);
	// VDDA28 ~ CORE12 delay : is not defined
	mdelay(1);
	// CORE12 - VDD_CAM_1P2 - "vddcamd"
	sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_1200MV);
	// VDDA28 ~ 5M_XSHUTDOWN delay >= 0us
	mdelay(1);
	// 5M_XSHUTDOWN - CAM_STBY - "main powerdown"
	sensor_k_set_pd_level(fd_handle, 1);
	// 5M_XSHUTDOWN ~ MCLK delay >= 0us
	udelay(500);
	// 5M_MCLK
	sensor_k_set_mclk(fd_handle, 24);
	// 5M_MCLK ~ 5M_RESET delay >= 10ms
	mdelay(12);
	// 5M_Reset
	sensor_k_set_rst_level(fd_handle, 1);
	// 5M_Reset ~ I2C delay > 16MCLK : 16/24 = 2us
	udelay(100);

	printk("sensor_hi544_poweron OK \n");

	return ret;
}

static int sensor_hi544_poweroff(uint32_t *fd_handle, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)
{
	int ret = 0;

	printk("sensor_hi544_poweroff\n");

	/*select main sensor(sensor hi544)*/
	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_0);

	mdelay(1);

	/*power off sequence*/

	// I2C ~ 5M_Reset delay > 16MCLK : 16/24 = 2us
	udelay(2);
	// 5M_Reset
	sensor_k_set_rst_level(fd_handle, 0);
	// 5M_Reset ~ 5M_MCLK delay >= 10ms
	mdelay(12);
	// 5M_MCLK
	sensor_k_set_mclk(fd_handle, 0);
	// 5M_MCLK ~ 5M_XSHUTDOWN delay >= 0us
	udelay(500);
	// 5M_XSHUTDOWN
	sensor_k_set_pd_level(fd_handle, 0);
	// 5M_XSHUTDOWN ~ VDDA28 delay >= 0us
	mdelay(1);
	// CORE12 - VDD_CAM_1P2 - "vddcamd"
	sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_CLOSED);
		// CORE12 ~ VDDA28 delay : is not defined
	mdelay(1);
	// VDDA28 - VDD_CAM_A2P8_F - "vddcama"
	sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
	// VDDA28 ~ VDDI18 delay >= 0us
	mdelay(1);
	// VDDI18 - VDD_CAM_IO_1P8 - "vddcammot"
	sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);	

	udelay(10);


	printk("sensor_hi544_poweroff OK \n");

	return ret;
}

static int sensor_sr030_poweron(uint32_t *fd_handle, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)
{
	int ret = 0;

	//printk("sensor_sr030_poweron start\n");

	/*set default status for main and sub sensor*/
	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_0);//select main sensor(sensor hi544);
	sensor_k_set_pd_level(fd_handle, 0);//power down valid for hi544
	sensor_k_set_rst_level(fd_handle, 0);//reset valid for hi544

	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_1);//select sub sensor(sensor sr030);
	sensor_k_set_pd_level(fd_handle, 0);//power down valid for sr030
	sensor_k_set_rst_level(fd_handle, 0);//reset valid for sr030

	mdelay(1);

	/*power on sequence*/


	sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_1800MV);
	// VT_IOVDD18 ~ VT_AVDD28 delay >= 0ms

	mdelay(1);
	// VT_AVDD28 - VDD_CAM_A2P8 - "vddcama"

	sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_2800MV);
	// VT_AVDD28 ~ VGA_MCLK delay >= 2ms

	mdelay(3);

	// VGA_MCLK

	sensor_k_set_mclk(fd_handle, 24);

	// VGA_MCLK ~ VGA_PWDN delay >= 2ms

	mdelay(3);

	// VGA_PWDN

sensor_k_set_pd_level(fd_handle, 1);

	// VGA_PWDN ~ VGA_RESET delay >= 30ms

	mdelay(31);
	// VGA_RESET

	sensor_k_set_rst_level(fd_handle, 1);
	// VGA_RESET delay > 16MCLK : 16/24 = 2us

	udelay(100);

	printk("sensor_sr030_poweron OK \n");

	return ret;
}


static int sensor_sr030_poweroff(uint32_t *fd_handle, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)
{
	int ret = 0;

	//printk("sensor_sr030_poweroff start\n");

	/*select main sensor(sensor sr030)*/
	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_1);

	mdelay(1);

	/*power off sequence*/

	// I2C ~ VGA_Reset delay > 16MCLK : 16/24 = 2us

	udelay(2);
	// VGA_RESET

	sensor_k_set_rst_level(fd_handle, 0);
	// VGA_RESET ~ VGA_MCLK delay >= 10us

	udelay(50);

	// VGA_MCLK

	sensor_k_set_mclk(fd_handle, 0);
	// VGA_MCLK ~ VGA_PWDN delay >= 0ms

	udelay(2);

	// VGA_PWDN

	sensor_k_set_pd_level(fd_handle, 0);
	// VGA_PWDN ~ VT_AVDD28 delay >= 0us

	udelay(2);

	// VT_AVDD28 - VDD_CAM_A2P8 - "vddcama"

	sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
	// VT_AVDD28 ~ VT_IOVDD18 delay >= 0us

	udelay(2);

	// VT_IOVDD18 / VT_CORE18 - "vddcammot"
	sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);

	udelay(10);

	printk("sensor_sr030_poweroff OK\n");


	return ret;
}

static int sensor_gc0310_poweron(uint32_t *fd_handle, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)
{
	int ret = 0;

	//printk("sensor_gc0310_poweron start\n");

	/*set default status for main and sub sensor*/
	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_0);//select main sensor(sensor hi544);
	sensor_k_set_pd_level(fd_handle, 0);//power down valid for hi544
	sensor_k_set_rst_level(fd_handle, 0);//reset valid for hi544

	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_1);//select sub sensor(sensor gc0310);
	sensor_k_set_pd_level(fd_handle, 0);//power down valid for gc0310
	sensor_k_set_rst_level(fd_handle, 0);//reset valid for gc0310

	mdelay(1);


	/*power on sequence*/

	// VT_IO18 / VT_CORE18 - "vddcammot"
	sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_1800MV);
	// VT_IO18 ~ VT_AVDD28 delay >= 0us
	udelay(500);
	// VDDA28 - VDD_CAM_A2P8 - "vddcama"
	sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_2800MV);
	// VDD_CAM_A2P8 ~ VT_MCLK delay >= 0us
	udelay(500);
	// VT_MCLK
	sensor_k_set_mclk(fd_handle, 24);
	// VT_MCLK ~ VT PWDN delay >= 0us
	udelay(100);
	// VT_STBY High
	sensor_k_set_pd_level(fd_handle, 1);
	// VT_STBY High stay >= 1us
	udelay(100);
	// VT_STBY Low
	sensor_k_set_pd_level(fd_handle, 0);
	// VT_STBY ~ I2C >= 30MCLK : 30/24
	udelay(100);

	printk("sensor_gc0310_poweron OK\n");

	return ret;
}

static int sensor_gc0310_poweroff(uint32_t *fd_handle, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)
{
	int ret = 0;

	//printk("sensor_gc0310_poweroff start\n");

	/*select main sensor(sensor gc0310)*/
	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_1);
	/*power off sequence*/

	// I2C ~ VT_STBY High delay >= 30MCLK : 30/24
	mdelay(1);
	// VT_STBY High
	sensor_k_set_pd_level(fd_handle, 1);
	// VT_STBY High ~ VT_MCLK delay >= 0us
	udelay(100);
	// VT_MCLK
	sensor_k_set_mclk(fd_handle, 0);
	// VT_MCLK ~ VDD_CAM_A2P8 dealy >= 0us
	udelay(100);
	// VDDA28 - VDD_CAM_A2P8 - "vddcama"
	sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
	// VDD28 ~ VDDIO18 delay >= 0us
	udelay(500);
	// VT_IO18 / VT_CORE18 - "vddcammot"
	sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);
	// VT_IO18 ~ VT_STBY LOW delay >= 0us
	udelay(500);
	sensor_k_set_pd_level(fd_handle, 0);

	udelay(10);

	printk("sensor_gc0310_poweroff OK\n");


	return ret;
}

int sensor_power_init(uint32_t *fd_handle, uint32_t sensor_id, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)
{
	int ret = 0;

	//printk("sensor_power_initial\n");
	
	/*set default status for main and sub sensor*/
	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_0);//select main sensor(sensor hi544);
	sensor_k_set_rst_level(fd_handle, 0);//reset valid for hi544
	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_1);//select sub sensor(sensor sr030);

	sensor_k_set_rst_level(fd_handle, 0);//reset valid for sr030

	mdelay(1);

	sensor_k_set_mclk(fd_handle, 0);

	mdelay(1);

	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_0);

	sensor_k_set_pd_level(fd_handle, 0);//reset valid for hi544
	sensor_k_sensor_sel(fd_handle, SENSOR_DEV_1);//select sub sensor(sensor sr030);

	sensor_k_set_pd_level(fd_handle, 0);//power down valid for sr030

	mdelay(1);

	sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_CLOSED);

	mdelay(1);

	sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);

	mdelay(1);

	sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);

	mdelay(1);


	return ret;
}

int sensor_power_on(uint32_t *fd_handle, uint32_t sensor_id, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)

{
	int ret = 0;
	unsigned short i2c_slave_addr = 0x00;

	i2c_slave_addr = sensor_k_get_sensor_i2c_addr(fd_handle, sensor_id);

	//printk("sensor_power i2c_slave_addr = %x\n", i2c_slave_addr);

	if (SENSOR_DEV_0 == sensor_id) {
		ret = sensor_hi544_poweron(fd_handle, dev0, dev1, dev2);
	} else if (SENSOR_DEV_1 == sensor_id) {
		if (GC0310_I2C_SLAVE_ADDR == i2c_slave_addr) {
			ret = sensor_gc0310_poweron(fd_handle, dev0, dev1, dev2);

		} else if (SR030PC50_I2C_SLAVE_ADDR == i2c_slave_addr) {
		ret = sensor_sr030_poweron(fd_handle, dev0, dev1, dev2);

		} else {
			ret = sensor_power_init(fd_handle, sensor_id, dev0, dev1, dev2);

	}
	}

	return ret;
}

int sensor_power_off(uint32_t *fd_handle, uint32_t sensor_id, struct sensor_power *dev0, struct sensor_power *dev1, struct sensor_power *dev2)

{
	int ret = 0;
	unsigned short i2c_slave_addr = 0x00;


	i2c_slave_addr = sensor_k_get_sensor_i2c_addr(fd_handle, sensor_id);

	//printk("sensor_power i2c_slave_addr = %x\n", i2c_slave_addr);


	if (SENSOR_DEV_0 == sensor_id) {

		ret = sensor_hi544_poweroff(fd_handle, dev0, dev1, dev2);

	} else if (SENSOR_DEV_1 == sensor_id) {

		if (GC0310_I2C_SLAVE_ADDR == i2c_slave_addr) {

		ret = sensor_gc0310_poweroff(fd_handle, dev0, dev1, dev2);

		} else if (SR030PC50_I2C_SLAVE_ADDR == i2c_slave_addr) {

			ret = sensor_sr030_poweroff(fd_handle, dev0, dev1, dev2);

		} else {

			ret = sensor_power_init(fd_handle, sensor_id, dev0, dev1, dev2);

		}

	}



	return ret;

}

