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
#define SENSOR_I2C_REG_16BIT		(0x01<<1)
#define SENSOR_I2C_REG_8BIT			(0x00<<1)

#if  1
#define SENSOR_PRINT printk
#else
#define SENSOR_PRINT pr_debug
#endif

static uint32_t front_sensor_pid = 0x0000;

typedef struct sensor_hwinfo {
	void (*poweron)(struct sensor_file_tag *sensor_file, int is_poweron);
	int (*identify)(struct file *filp, uint32_t *pid, uint32_t *version);
	const char *name;
	uint16_t i2c_slave_addr;
} sensor_hwinfto_t;

static int Sensor_ReadI2C(struct file *filp, uint16_t reg_addr, uint16_t reg_bits, uint16_t *reg_value)
{
	struct sensor_reg_bits_tag reg;
	reg.reg_addr = reg_addr;
	reg.reg_bits = reg_bits;
	if (0 == sensor_ReadReg_fromkernel(filp, &reg)) {
		*reg_value = reg.reg_value;
		return 0;
	}
	return -1;
}

/*********************************Sr030pc50 begin*******************************************/
int Sr030pc50_mipi_yuv_Identify(struct file *filp, uint32_t *pid, uint32_t *version)
{
#define SR030PC50_CHIP_ID_ADDR 0x04
#define SR030PC50_CHIP_ID_VALUE 0xB8

	uint32_t i = 0;
	uint16_t chip_id = 0x00;

	SENSOR_PRINT("sensor_sr030pc50: mipi yuv identify.\n");

	for (i = 0; i < 3; i++) {
		if (Sensor_ReadI2C(filp, SR030PC50_CHIP_ID_ADDR, SENSOR_I2C_REG_8BIT, &chip_id) < 0) {
			SENSOR_PRINT(KERN_ERR "sensor_sr030pc50: read pid H error\n");
			return -1;
		}
		if (SR030PC50_CHIP_ID_VALUE == chip_id) {
			SENSOR_PRINT("sensor_sr030pc50: this is sr030pc50 sensor !\n");
			*pid = chip_id;
			return 0;
		} else {
			SENSOR_PRINT(KERN_ERR "sensor_sr030pc50: identify fail, chip_id=0x%4x, count(%d)\n", chip_id, i);
		}
	}

	return -1;
}

void Sr030pc50_mipi_yuv_PowerOn_j1mini3g(struct sensor_file_tag *sensor_file, int is_poweron)
{
	uint32_t *fd_handle = (uint32_t*)sensor_file;

	if (is_poweron) {
		SENSOR_PRINT("sensor_sr030pc50: PowerOn start for Identify !\n");

		sensor_k_set_pd_level(fd_handle, 0);//power down valid for sr030
		sensor_k_set_rst_level(fd_handle, 0);//reset valid for sr030

		mdelay(1);

		/*power on sequence*/

		// VT_IOVDD18 / VT_CORE18 - "vddcamio"
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

		SENSOR_PRINT("sensor_sr030_poweron OK for Identify \n");
	} else {
		SENSOR_PRINT("sensor_sr030pc50: PowerOff start for Identify !\n");

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
		// VT_IOVDD18 / VT_CORE18 - "vddcamio"
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);

		udelay(10);

		SENSOR_PRINT("sensor_sr030_poweroff OK for Identify \n");
	}
}		

void Sr030pc50_mipi_yuv_PowerOn(struct sensor_file_tag *sensor_file, int is_poweron)
{
	uint32_t *fd_handle = (uint32_t*)sensor_file;

	if (is_poweron) {
		sensor_k_set_rst_level(fd_handle, 0);
		msleep(10);
		sensor_k_set_pd_level(fd_handle, 0);

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
int gc0310_mipi_yuv_identify(struct file *filp, uint32_t *pid, uint32_t *version)
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

	SENSOR_PRINT("\nSENSOR_gc0310: mipi yuv identify\n");
	for (i=0; i<3; i++) {
		if (Sensor_ReadI2C(filp, GC0310_MIPI_PID_ADDR1, SENSOR_I2C_REG_8BIT, &pid_high) < 0) {
			SENSOR_PRINT(KERN_ERR "SENSOR_gc0310: read pid H error\n");
			return -1;
		}
		if (Sensor_ReadI2C(filp, GC0310_MIPI_PID_ADDR2, SENSOR_I2C_REG_8BIT, &pid_low) < 0) {
			SENSOR_PRINT("SENSOR_gc0310: read pid L error\n");
			return -1;
		}
		pid_value = ((pid_high << 8) | pid_low);
		if (GC0310_MIPI_SENSOR_ID == pid_value) {
			SENSOR_PRINT("SENSOR_gc0310: find gc0x%4x-0x%2x sensor !\n", ((pid_high*256) + pid_low), ver_value);
			*pid = (((uint32_t)pid_high * 256) + pid_low);
			//*version = ver_value;
			return 0;
		} else {
			SENSOR_PRINT("SENSOR_gc0310: identify fail, pid_h=%02x, pid_l=%02x, count(%d)\n", pid_high, pid_low, i);
		}

	}
	return -1;
}

void gc0310_mipi_yuv_poweron_j1mini3g(struct sensor_file_tag *sensor_file, int is_poweron)
{
	uint32_t *fd_handle = (uint32_t*)sensor_file;

	if (is_poweron) {
		SENSOR_PRINT("sensor_gc0310: PowerOn start for Identify !\n");
		
		sensor_k_set_pd_level(fd_handle, 0);//power down valid for gc0310
		sensor_k_set_rst_level(fd_handle, 0);//reset valid for gc0310

		mdelay(1);


		/*power on sequence*/

		// VT_IO18 / VT_CORE18 - "vddcamio"
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

		SENSOR_PRINT("sensor_gc0310_poweron OK for Identify \n");
	} else {
		SENSOR_PRINT("sensor_gc0310: PowerOn start for Identify !\n");

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
		// VT_IO18 / VT_CORE18 - "vddcamio"
		sensor_k_set_voltage_iovdd(fd_handle, SENSOR_VDD_CLOSED);
		// VT_IO18 ~ VT_STBY LOW delay >= 0us
		udelay(500);
		sensor_k_set_pd_level(fd_handle, 0);

		udelay(10);

		SENSOR_PRINT("sensor_gc0310_poweroff OK for Identify \n");
	}
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

sensor_hwinfto_t front_cameras[] = {
	{
#if defined(CONFIG_MACH_J1MINI3G)
		.poweron	= Sr030pc50_mipi_yuv_PowerOn_j1mini3g,
#else
		.poweron	= Sr030pc50_mipi_yuv_PowerOn,
#endif
		.identify	= Sr030pc50_mipi_yuv_Identify,
		.name		= "sr030pc50_yuv",
		.i2c_slave_addr = 0x30,
	},
	{
#if defined(CONFIG_MACH_J1MINI3G)
		.poweron	= gc0310_mipi_yuv_poweron_j1mini3g,
#else
		.poweron	= gc0310_mipi_yuv_poweron,
#endif
		.identify 	= gc0310_mipi_yuv_identify,
		.name 		= "gc0310_mipi_yuv",
		.i2c_slave_addr = 0x21,
	},
};

int write_result(struct dentry *root, const char *filename, uint32_t *pid)
{
	int ret = 0;
	if (root) {
		if(NULL == debugfs_create_x32(filename, S_IRUGO, (struct dentry *)root, pid)) {
			SENSOR_PRINT("sensor create node error\n");
			ret = -1;
		}
	} else {
		SENSOR_PRINT("sensor no root dir\n");
		ret = -1;
	}
	return ret;
}

int sensor_identify (void *data)
{
	int i = 0 , ret = 0;
	struct sensor_module_tab_tag    *p_mod;
	struct sensor_file_tag          *fd_handle;
	struct file *fd = NULL, *back_sensor_filp = NULL, *front_sensor_filp = NULL;
	uint32_t pid = 0, version = 0;
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
		SENSOR_PRINT("sensor: open /dev/sprd_sensor error\n");
		return -1;
	}

	fd_handle = (struct sensor_file_tag *)fd->private_data;
	if (NULL == fd_handle) {
		SENSOR_PRINT("sensor: thread error\n");
		return -1;
	}

	p_mod = fd_handle->module_data;
	if (NULL == p_mod) {
		SENSOR_PRINT("sensor: thread error\n");
		return -1;
	}

	/* front camera identify*/
	sensor_k_sensor_sel((uint32_t*)fd_handle, SENSOR_DEV_1);
	sensor_k_set_id(fd, SENSOR_DEV_1);

	for (i=0; i< sizeof(front_cameras)/sizeof(front_cameras[0]); i++) {
		sensor_hwinfto_t *hw = &front_cameras[i];
		hw->poweron(fd_handle, 1);
		sensor_k_set_i2c_addr(fd, hw->i2c_slave_addr);
		sensor_set_i2c_clk_fromkernel(fd, 400000, 1);

		if (0 == hw->identify(fd, &pid, &version)) {
			printk ("config sensor %s ok pid %p\n", hw->name, pid);
			hw->poweron(fd_handle, 0);
			ret = write_result(root, "camera_front", &pid);
			front_sensor_pid = pid;
			break;
		} else {
			hw->poweron(fd_handle, 0);
		}
	}

	sensor_k_sensor_sel((uint32_t*)fd_handle, SENSOR_DEV_0);
	sensor_k_set_id(fd, SENSOR_DEV_0);

	filp_close(fd, NULL);

	return 0;
}

void sensor_get_front_sensor_pid(uint32_t *read_sensor_pid)
{
	if(read_sensor_pid) {
		*read_sensor_pid = front_sensor_pid;
	}
}