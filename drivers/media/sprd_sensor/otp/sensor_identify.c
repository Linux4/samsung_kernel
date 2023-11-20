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
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/debugfs.h>
#include <video/sensor_drv_k.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/vmalloc.h>
#include <linux/sprd_mm.h>
#include "../sensor_drv_sprd.h"
#include "sensor_otp.h"
#define SENSOR_I2C_FREQ_400			(0x04 << 5)
#define SENSOR_I2C_REG_16BIT		(0x01<<1)
#define SENSOR_I2C_REG_8BIT			(0x00<<1)
#define DW9714_VCM_SLAVE_ADDR		(0x18>>1)

#if 0
#define SENSOR_PRINT printk
#else
#define SENSOR_PRINT pr_debug
#endif
typedef struct sensor_hwinfo {
	void (*poweron)(struct sensor_file_tag *sensor_file, int is_poweron);
	int (*identify)(struct sensor_file_tag *sensor_file, uint32_t *pid, uint32_t *version);
	const char *name;
	uint16_t i2c_slave_addr;
} sensor_hwinfto_t;

static int Sensor_WriteI2C (uint32_t *fd_handle, uint16_t slave_addr, uint8_t * cmd, uint16_t cmd_length)
{
	struct sensor_i2c_tag i2c_tag;
	i2c_tag.slave_addr = slave_addr;
	i2c_tag.i2c_data   = cmd;
	i2c_tag.i2c_count  = cmd_length;
	SENSOR_PRINT ("sensor i2c write:  addr=0x%x\n", i2c_tag.slave_addr);
	if (0 == sensor_wr_i2c_fromkernel(fd_handle, &i2c_tag)){
		return 0;
	}
	return -1;
}
static int Sensor_ReadI2C(uint32_t *fd_handle, uint16_t reg_addr, uint16_t reg_bits, uint16_t *reg_value)
{
	struct sensor_reg_bits_tag reg;
	reg.reg_addr = reg_addr;
	reg.reg_bits = reg_bits;
	if (0 == sensor_ReadReg_fromkernel(fd_handle, &reg)) {
		*reg_value = reg.reg_value;
		return 0;
	}
	return -1;
}
/*********************************ov5670 begin*******************************************/
int gc0310_mipi_raw_identify(struct sensor_file_tag *sensor_file, uint32_t *pid, uint32_t *version)
{
	/* add new code for gc0310_mipi_raw identify*/
#define GC0310_MIPI_PID_ADDR1     0xf0
#define GC0310_MIPI_PID_ADDR2     0xf1
#define GC0310_MIPI_SENSOR_ID     0xa310
	uint16_t pid_value = 0;
	uint16_t pid_high = 0;
	uint16_t pid_low = 0;
	uint16_t ver_value   = 0x00;
	int i;
	uint32_t *fd_handle = (uint32_t*)sensor_file;

	SENSOR_PRINT("\nSENSOR_gc0310: mipi raw identify\n");
	for (i=0; i<3; i++) {
		if (Sensor_ReadI2C(fd_handle, GC0310_MIPI_PID_ADDR1, SENSOR_I2C_REG_8BIT, &pid_high) < 0) {
			SENSOR_PRINT(KERN_ERR "SENSOR_gc0310: read pid H error");
			return -1;
		}
		if (Sensor_ReadI2C(fd_handle, GC0310_MIPI_PID_ADDR2, SENSOR_I2C_REG_8BIT, &pid_low) < 0) {
			SENSOR_PRINT("SENSOR_gc0310: read pid L error");
			return -1;
		}
		pid_value=pid_high<< 8 | pid_low;
		if (GC0310_MIPI_SENSOR_ID == pid_value) {
			SENSOR_PRINT("SENSOR_gc0310: find gc%x-%x sensor !", pid_high*256 + pid_low, ver_value);
			*pid = (uint32_t)pid_high * 256 + pid_low;
			//*version = ver_value;
			return 0;
		} else {
			SENSOR_PRINT("SENSOR_gc0310: identify fail,pid_h=%02x, pid_l=%02x time %d\n", pid_high, pid_low,i);
		}

	}
	return -1;
}
void gc0310_mipi_raw_poweron(struct sensor_file_tag *sensor_file, int is_poweron)
{
	/* add new code for gc0310_mipi_raw poweron*/

	uint32_t *fd_handle = (uint32_t*)sensor_file;
	if (is_poweron) {
		sensor_k_set_pd_level(fd_handle, 0);
		/*power on*/
		sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_2800MV);
		msleep(1);
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_1800MV);
		sensor_k_set_pd_level(fd_handle, 1);
		msleep(1);
		sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_2800MV);
		msleep(1);
		sensor_k_set_mclk(fd_handle, 24);
		msleep(1);
		sensor_k_set_pd_level(fd_handle, 0);
		msleep(1);
	} else {
		msleep(1);
		sensor_k_set_mclk(fd_handle, 0);
		msleep(1);
		msleep(1);
		sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
		msleep(1);
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_pd_level(fd_handle, 1);
		sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_CLOSED);
	}
	SENSOR_PRINT("gc0310_mipi_raw_poweron(%d)\n", is_poweron);
}

/*********************************ov5670 begin*******************************************/
int ov5670_mipi_raw_identify(struct sensor_file_tag *sensor_file, uint32_t *pid, uint32_t *version)
{
	/* add new code for ov5670_mipi_raw identify*/
#define ov5670_PID_VALUE    0x56
#define ov5670_PID_ADDR     0x300B
#define ov5670_VER_VALUE    0x70
#define ov5670_VER_ADDR     0x300C

	uint16_t pid_value;
	uint16_t ver_value  = 0x00;
	uint32_t *fd_handle = (uint32_t *)sensor_file;

	SENSOR_PRINT("SENSOR_OV5670: mipi raw identify\n");
	if (Sensor_ReadI2C(fd_handle, ov5670_PID_ADDR, SENSOR_I2C_REG_16BIT, &pid_value) < 0) {
		SENSOR_PRINT("SENSOR_ov5670: read pid error\n");
		return -1;
	}

	if (ov5670_PID_VALUE == pid_value) {
		Sensor_ReadI2C(fd_handle, ov5670_VER_ADDR, SENSOR_I2C_REG_16BIT, &ver_value);
		SENSOR_PRINT("SENSOR_ov5670: find ov%02x%02x sensor!\n", pid_value, ver_value);
		*pid = pid_value;
		*version = ver_value;
		return 0;
	} else {
		SENSOR_PRINT("SENSOR_ov5670: identify fail,pid=%x\n", pid_value);
	}
	return -1;
}

void ov5670_mipi_raw_poweron(struct sensor_file_tag *sensor_file, int is_poweron)
{
	/* add new code for ov5670_mipi_raw poweron*/

	uint32_t *fd_handle = (uint32_t*)sensor_file;
	if (is_poweron) {
		sensor_k_set_mclk(fd_handle, 0);
		sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_rst_level(fd_handle, 0);
		sensor_k_set_pd_level(fd_handle, 0);
		/*power on*/
		sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_2800MV);
		msleep(1);
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_1800MV);
		sensor_k_set_pd_level(fd_handle, 1);
		msleep(1);
		sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_2800MV);
		msleep(1);
		sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_1200MV);
		msleep(1);
		sensor_k_set_rst_level(fd_handle, 1);
		msleep(1);
		sensor_k_set_mclk(fd_handle, 24);
		msleep(10);
	} else {
		msleep(1);
		sensor_k_set_mclk(fd_handle, 0);
		msleep(1);
		sensor_k_set_rst_level(fd_handle, 0);
		msleep(1);
		sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_CLOSED);
		msleep(1);
		sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
		msleep(1);
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_pd_level(fd_handle, 0);
		sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_CLOSED);
	}
	SENSOR_PRINT("SENSOR_OV5670: _ov5670_Power_On(1:on, 0:off): %d\n", is_poweron);
}

/*********************************ov5670 end*******************************************/

/*********************************ov13850 begin*******************************************/
void ov13850_mipi_raw_poweron(struct sensor_file_tag *sensor_file, int is_poweron)
{
	uint32_t *fd_handle = (uint32_t*)sensor_file;
	if (is_poweron) {
		uint8_t cmd_val[2];
		/*ensure power off*/
		sensor_k_set_mclk(fd_handle, 0);
		sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_rst_level(fd_handle, 0);
		sensor_k_set_pd_level(fd_handle, 0);
		msleep(3);

		/*power on*/
		sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_2800MV);
		msleep(1);
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_1800MV);
		sensor_k_set_pd_level(fd_handle, 1);
		msleep(1);
		sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_2800MV);
		msleep(1);
		sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_1200MV);
		msleep(10);
		sensor_k_set_rst_level(fd_handle, 1);
		msleep(1);
		sensor_k_set_mclk(fd_handle, 24);
		msleep(10);
		/*dw9174 initial*/
		cmd_val[0] = 0xec;
		cmd_val[1] = 0xa3;
		Sensor_WriteI2C(fd_handle, DW9714_VCM_SLAVE_ADDR,cmd_val, 2);
		cmd_val[0] = 0xa1;
		cmd_val[1] = 0x0e;
		Sensor_WriteI2C(fd_handle, DW9714_VCM_SLAVE_ADDR,cmd_val, 2);
		cmd_val[0] = 0xf2;
		cmd_val[1] = 0x90;
		Sensor_WriteI2C(fd_handle, DW9714_VCM_SLAVE_ADDR,cmd_val, 2);
		cmd_val[0] = 0xdc;
		cmd_val[1] = 0x51;
		Sensor_WriteI2C(fd_handle, DW9714_VCM_SLAVE_ADDR,cmd_val, 2);
	} else {
		/*power off*/
		msleep(1);
		sensor_k_set_mclk(fd_handle, 0);
		msleep(1);
		sensor_k_set_rst_level(fd_handle, 0);
		msleep(1);
		sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_CLOSED);
		msleep(1);
		sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
		msleep(1);
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_pd_level(fd_handle, 0);
		sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_CLOSED);
	}
	SENSOR_PRINT("ov13850_mipi_raw_poweron(%d)", is_poweron);
}

int ov13850_mipi_raw_identify(struct sensor_file_tag *sensor_file, uint32_t *pid, uint32_t *version)
{
#define ov13850_PID_VALUE_H		0xD8
#define ov13850_PID_ADDR_H		0x300A
#define ov13850_PID_VALUE_L		0x50
#define ov13850_PID_ADDR_L		0x300B
#define ov13850_VER_VALUE		0xB1
#define ov13850_VER_ADDR		0x302A

	uint16_t pid_high = 0;
	uint16_t pid_low = 0;
	uint16_t ver_value   = 0x00;

	uint32_t *fd_handle = (uint32_t*)sensor_file;
	SENSOR_PRINT("SENSOR_ov13850: mipi raw identify\n");
	if (Sensor_ReadI2C(fd_handle, ov13850_PID_ADDR_H, SENSOR_I2C_REG_16BIT, &pid_high) < 0) {
		SENSOR_PRINT(KERN_ERR "SENSOR_ov13850: read pid H error");
		return -1;
	}
	if (Sensor_ReadI2C(fd_handle, ov13850_PID_ADDR_L, SENSOR_I2C_REG_16BIT, &pid_low) < 0) {
		SENSOR_PRINT("SENSOR_ov13850: read pid L error");
		return -1;
	}

	if (ov13850_PID_VALUE_H == pid_high && ov13850_PID_VALUE_L == pid_low) {
		Sensor_ReadI2C(fd_handle, ov13850_VER_ADDR, SENSOR_I2C_REG_16BIT|SENSOR_I2C_FREQ_400, &ver_value);
		SENSOR_PRINT("SENSOR_ov13850: find ov%d-%d sensor !", pid_high*256 + pid_low, ver_value);
		*pid = (uint32_t)pid_high * 256 + pid_low;
		*version = ver_value;
		return 0;
	} else {
		SENSOR_PRINT("SENSOR_ov13850: identify fail,pid_h=%02x, pid_l=%02x", pid_high, pid_low);
	}
	return -1;
}
/*********************************ov13850 end*******************************************/

/*********************************ov5648 begin*******************************************/
void ov5648_mipi_raw_poweron(struct sensor_file_tag *sensor_file, int is_poweron)
{
	uint32_t *fd_handle = (uint32_t*)sensor_file;
	if (is_poweron) {
		uint8_t cmd_val[6];
		/*ensure power off*/
		sensor_k_set_mclk((uint32_t*)fd_handle, 0);
		sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_rst_level(fd_handle, 0);
		sensor_k_set_pd_level(fd_handle, 0);
		msleep(2);
		/* power on*/
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_1800MV);
		msleep(1);
		sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_2800MV);
		msleep(5);
		sensor_k_set_pd_level(fd_handle, 1);
		msleep(1);
		sensor_k_set_rst_level(fd_handle, 1);
		msleep(2);
#ifdef CONFIG_DCAM_SENSOR_DEV_2_SUPPORT
		/*for jiaotu board*/
		sensor_k_set_mipi_level(fd_handle, 0);
#endif
		sensor_k_set_mclk(fd_handle, 24);
		msleep(2);
		sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_2800MV);
		msleep(10);
		/*dw9174 initial*/
		cmd_val[0] = 0xec;
		cmd_val[1] = 0xa3;
		cmd_val[2] = 0xf2;
		cmd_val[3] = 0x00;
		cmd_val[4] = 0xdc;
		cmd_val[5] = 0x51;
		Sensor_WriteI2C(fd_handle, DW9714_VCM_SLAVE_ADDR,cmd_val, 6);
	} else {
		/*power off*/
		msleep(1);
		sensor_k_set_rst_level(fd_handle, 0);
		sensor_k_set_pd_level(fd_handle, 0);
		msleep(1);
		sensor_k_set_mclk(fd_handle, 0);
		msleep(1);
		sensor_k_set_voltage_dvdd(fd_handle, SENSOR_VDD_CLOSED);
		msleep(1);
		sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
		msleep(1);
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_CLOSED);
	}
}

int ov5648_mipi_raw_identify(struct sensor_file_tag *sensor_file, uint32_t *pid,  uint32_t *version)
{
#define ov5648_PID_VALUE    0x56
#define ov5648_PID_ADDR     0x300A
#define ov5648_VER_VALUE    0x48
#define ov5648_VER_ADDR     0x300B


	uint16_t pid_value;
	uint16_t ver_value  = 0x00;
	uint32_t *fd_handle = (uint32_t *)sensor_file;
	SENSOR_PRINT("SENSOR_ov5648: mipi raw identify\n");
	msleep(2);
	if (Sensor_ReadI2C(fd_handle, ov5648_PID_ADDR, SENSOR_I2C_REG_16BIT, &pid_value) < 0) {
		SENSOR_PRINT("SENSOR_ov5648: read pid error\n");
		return -1;
	}

	if (ov5648_PID_VALUE == pid_value) {
		Sensor_ReadI2C(fd_handle, ov5648_VER_ADDR, SENSOR_I2C_REG_16BIT, &ver_value);
		SENSOR_PRINT("SENSOR_ov5648: find ov%02x%02x sensor!\n", pid_value, ver_value);
		*pid = pid_value;
		*version = ver_value;
		return 0;
	} else {
		SENSOR_PRINT("SENSOR_ov5648: identify fail,pid=%x\n", pid_value);
	}
	return -1;
}
/*********************************ov5648 end*******************************************/

sensor_hwinfto_t back_cameras[] = {
	{
		.poweron	= ov13850_mipi_raw_poweron,
		.identify 	= ov13850_mipi_raw_identify,
		.name 		= "ov13850_mipi_raw",
		.i2c_slave_addr = 0x10,
	},
	{
		.poweron	= ov5670_mipi_raw_poweron,
		.identify 	= ov5670_mipi_raw_identify,
		.name 		= "ov5670_mipi_raw",
		.i2c_slave_addr = 0x10,
	},

};

sensor_hwinfto_t front_cameras[] = {
	{
		.poweron	= ov5648_mipi_raw_poweron,
		.identify 	= ov5648_mipi_raw_identify,
		.name 		= "ov5648_mipi_raw",
		.i2c_slave_addr = 0x36,
	},
	{
		.poweron	= gc0310_mipi_raw_poweron,
		.identify 	= gc0310_mipi_raw_identify,
		.name 		= "gc0310_mipi_raw",
		.i2c_slave_addr = 0x21,
	},
};

void write_result(struct dentry *root, const char *filename)
{
	if (root) {
		if(NULL == debugfs_create_file(filename, S_IRUGO, (struct dentry *)root, NULL, NULL)) {
			SENSOR_PRINT("sensor create node error");
		}
	} else {
		SENSOR_PRINT("sensor no root dir");
	}
}

int sensor_identify (void *data)
{
#ifdef CONFIG_CAMERA_SENSOR_FACTORY_TEST
	int i;
	struct sensor_module_tab_tag    *p_mod;
	struct sensor_file_tag          *fd_handle;
	struct file                     *fd = NULL;
	uint32_t pid, version;
	struct dentry *root;
	(void)(data);

	root = debugfs_create_dir("hardware_info", NULL);
	if (root) {
		root =  debugfs_create_dir("parameters", root);
	}

	/*wait dev/sprd_sensor created*/
	for (i = 0; i < 10; i++) {
		fd = filp_open ("/dev/sprd_sensor", O_RDWR, 0);
		if (IS_ERR (fd)) {
			msleep(2000);
		} else {
			break;
		}
	}
	if (IS_ERR (fd)) {
		SENSOR_PRINT("sensor: open /dev/sprd_sensor error");
		return -1;
	}
	fd_handle = (struct sensor_file_tag *)fd->private_data;
	p_mod     = fd_handle->module_data;
	if (NULL==fd_handle || NULL==p_mod) {
		SENSOR_PRINT("sensor: thread error");
		return -1;
	}

	/* back camera identify*/
	sensor_k_sensor_sel((uint32_t*)fd_handle, SENSOR_DEV_0);
	sensor_k_set_id(fd, SENSOR_DEV_0);
	for (i=0; i< sizeof(back_cameras)/sizeof(back_cameras[0]); i++) {
		sensor_hwinfto_t *hw = &back_cameras[i];
		hw->poweron(fd_handle, 1);

		sensor_k_set_i2c_addr(fd, hw->i2c_slave_addr);
		sensor_set_i2c_clk_fromkernel((uint32_t*)fd_handle, 400000, 0);

		if (0 == hw->identify(fd_handle, &pid, &version)) {
			SENSOR_PRINT ("config sensor %s ok\n", hw->name);
			hw->poweron(fd_handle, 0);
			write_result(root, "camera_back");
			break;
		} else {
			hw->poweron(fd_handle, 0);
		}
	}
	/* front camera identify*/
	sensor_k_sensor_sel((uint32_t*)fd_handle, SENSOR_DEV_1);
	sensor_k_set_id(fd, SENSOR_DEV_1);
	for (i=0; i< sizeof(front_cameras)/sizeof(front_cameras[0]); i++) {
		sensor_hwinfto_t *hw = &front_cameras[i];
		hw->poweron(fd_handle, 1);

		sensor_k_set_i2c_addr(fd, hw->i2c_slave_addr);
		sensor_set_i2c_clk_fromkernel((uint32_t*)fd_handle, 400000, 1);

		if (0 == hw->identify(fd_handle, &pid, &version)) {
			SENSOR_PRINT ("config sensor %s ok\n", hw->name);
			hw->poweron(fd_handle, 0);
			write_result(root, "camera_front");
			break;
		} else {
			hw->poweron(fd_handle, 0);
		}
	}

	sensor_k_sensor_sel((uint32_t*)fd_handle, SENSOR_DEV_0);
	sensor_k_set_id(fd, SENSOR_DEV_0);

	filp_close(fd, NULL);

#endif

	return 0;
}
