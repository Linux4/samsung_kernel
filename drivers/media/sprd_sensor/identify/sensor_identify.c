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


#include "sensor_identify.h"
#define SENSOR_I2C_FREQ_400			(0x04 << 5)
#define SENSOR_I2C_REG_16BIT		(0x01<<1)
#define DW9714_VCM_SLAVE_ADDR		(0x18>>1)
#define SENSOR_I2C_REG_8BIT			(0x00<<1)
#define file_camera_back			"/sys/kernel/debug/hardware_info/parameters/camera_back"
#define file_camera_front			"/sys/kernel/debug/hardware_info/parameters/camera_front"

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

static struct file                     *fd = NULL;

static int Sensor_WriteI2C (uint32_t *fd_handle, uint16_t slave_addr, uint8_t * cmd, uint16_t cmd_length)
{
	struct sensor_i2c_tag i2c_tag;
	i2c_tag.slave_addr = slave_addr;
	i2c_tag.i2c_data   = cmd;
	i2c_tag.i2c_count  = cmd_length;
	SENSOR_PRINT ("sensor i2c write:  addr=0x%x\n", i2c_tag.slave_addr);
	if (0 == sensor_wr_i2c_fromkernel(fd, &i2c_tag)){
		return 0;
	}
	return -1;
}
static int Sensor_ReadI2C(uint32_t *fd_handle, uint16_t reg_addr, uint16_t reg_bits, uint16_t *reg_value)
{
	struct sensor_reg_bits_tag reg;
	reg.reg_addr = reg_addr;
	reg.reg_bits = reg_bits;
	if (0 == sensor_ReadReg_fromkernel(fd, &reg)) {
		*reg_value = reg.reg_value;
		return 0;
	}
	return -1;
}

/*********************************Sr030pc50 begin*******************************************/
int Sr030pc50_mipi_yuv_Identify(struct sensor_file_tag *sensor_file, uint32_t *pid, uint32_t *version)
{
#define SR030PC50_CHIP_ID_ADDR 0x04
#define SR030PC50_CHIP_ID_VALUE 0xB8

	uint32_t i = 0;
	uint16_t chip_id = 0x00;
	uint32_t *fd_handle = (uint32_t*)sensor_file;

	SENSOR_PRINT("sensor_sr030pc50: mipi yuv identify.");

	for (i = 0; i < 3; i++) {
		if (Sensor_ReadI2C(fd_handle, SR030PC50_CHIP_ID_ADDR, SENSOR_I2C_REG_8BIT, &chip_id) < 0) {
			SENSOR_PRINT(KERN_ERR "sensor_sr030pc50: read pid H error");
			return -1;
		}
		if (SR030PC50_CHIP_ID_VALUE == chip_id) {
			SENSOR_PRINT("sensor_sr030pc50: this is sr030pc50 sensor !");
			return 0;
		} else {
			SENSOR_PRINT(KERN_ERR "sensor_sr030pc50: identify fail, chip_id=0x%x", chip_id);
		}
	}

	return -1;
}

void Sr030pc50_mipi_yuv_PowerOn(struct sensor_file_tag *sensor_file, int is_poweron)
{
	uint32_t *fd_handle = (uint32_t*)sensor_file;

	if (is_poweron) {
		sensor_k_set_rst_level(fd_handle, 0);
		msleep(10);
		sensor_k_set_pd_level(fd_handle, 0);
		/*sensor_k_set_mclk(fd_handle, 0);
		
		sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);
		msleep(2);*/
		sensor_k_set_voltage_cammot(fd_handle, SENSOR_VDD_2800MV);

		msleep(10);
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_1800MV);
		msleep(10);
		sensor_k_set_voltage_avdd(fd_handle,SENSOR_VDD_2800MV);
		msleep(10);
		sensor_k_set_mclk(fd_handle, 24);
		msleep(30);
		sensor_k_set_pd_level(fd_handle, 1);
		msleep(10);
		sensor_k_set_rst_level(fd_handle, 1);
		msleep(10);
	} else {
		sensor_k_set_rst_level(fd_handle, 0);
		msleep(10);
		sensor_k_set_mclk(fd_handle, 0);
		sensor_k_set_pd_level(fd_handle, 0);
		sensor_k_set_voltage_avdd(fd_handle, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);
	}
	SENSOR_PRINT("Sr030pc50_mipi_yuv_PowerOn(%d)\n", is_poweron);
}
/*********************************Sr030pc50 begin*******************************************/

/*********************************gc0310 begin*******************************************/
int gc0310_mipi_yuv_identify(struct sensor_file_tag *sensor_file, uint32_t *pid, uint32_t *version)
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

	SENSOR_PRINT("\nSENSOR_gc0310: mipi yuv identify\n");
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
void gc0310_mipi_yuv_poweron(struct sensor_file_tag *sensor_file, int is_poweron)
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
	SENSOR_PRINT("gc0310_mipi_yuv_poweron(%d)\n", is_poweron);
}
/*********************************gc0310 end*******************************************/

#if 0
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
#endif
sensor_hwinfto_t back_cameras[] = {
#if 0
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
#endif
};

sensor_hwinfto_t front_cameras[] = {
#if 0
	{
		.poweron	= ov5648_mipi_raw_poweron,
		.identify 	= ov5648_mipi_raw_identify,
		.name 		= "ov5648_mipi_raw",
		.i2c_slave_addr = 0x36,
	},
#endif

/*	{
		.poweron	= gc0310_mipi_yuv_poweron,
		.identify 	= gc0310_mipi_yuv_identify,
		.name 		= "gc0310_mipi_raw",
		.i2c_slave_addr = 0x21,
	},*/
	{
		.poweron	= Sr030pc50_mipi_yuv_PowerOn,
		.identify	= Sr030pc50_mipi_yuv_Identify,
		.name		= "sr030pc50_yuv",
		//.i2c_slave_addr = 0x21,
		.i2c_slave_addr = 0x30,
	},
};

int write_result(struct dentry *root, const char *filename)
{
	int ret = 0;
	if (root) {
		if(NULL == debugfs_create_file(filename, S_IRUGO, (struct dentry *)root, NULL, NULL)) {
			SENSOR_PRINT("sensor create node error");
			ret = -1;
		}
	} else {
		SENSOR_PRINT("sensor no root dir");
		ret = -1;
	}
	return ret;
}

int sensor_identify (void *data)
{
	return 0;
}
