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
#ifndef _SENSOR_DRV_K_H_
#define _SENSOR_DRV_K_H_

struct sensor_i2c {
unsigned short id;
unsigned short addr;
uint32_t clock;
};

typedef struct sensor_i2c_tag {
	uint8_t  *i2c_data;
	uint16_t i2c_count;
	uint16_t slave_addr;
} SENSOR_I2C_T, *SENSOR_I2C_T_PTR;

typedef struct sensor_reg_tag {
	uint16_t reg_addr;
	uint16_t reg_value;
} SENSOR_REG_T, *SENSOR_REG_T_PTR;

typedef struct sensor_reg_bits_tag {
	uint16_t reg_addr;
	uint16_t reg_value;
	uint32_t reg_bits;
} SENSOR_REG_BITS_T, *SENSOR_REG_BITS_T_PTR;

typedef struct sensor_reg_tab_tag {
	SENSOR_REG_T_PTR sensor_reg_tab_ptr;
	uint32_t reg_count;
	uint32_t reg_bits;
	uint32_t burst_mode;
} SENSOR_REG_TAB_T, *SENSOR_REG_TAB_PTR;

typedef struct sensor_flash_level	 {
	uint32_t low_light;
	uint32_t high_light;
} SENSOR_FLASH_LEVEL_T;

typedef struct sensor_project_func_tag {
	int (*SetFlash)(uint32_t flash_mode);
} SENSOR_PROJECT_FUNC_T;

enum INTERFACE_OP_ID {
	INTERFACE_OPEN = 0,
	INTERFACE_CLOSE
};

enum INTERFACE_TYPE_ID {
	INTERFACE_CCIR = 0,
	INTERFACE_MIPI
};

typedef struct sensor_if_cfg_tag {
	uint32_t is_open;
	uint32_t if_type;
	uint32_t phy_id;
	uint32_t lane_num;
	uint32_t bps_per_lane;
} SENSOR_IF_CFG_T;

typedef struct  {
	uint32_t d_die;
	uint32_t a_die;
} SENSOR_SOCID_T;

typedef struct sensor_power {
	uint32_t af_volt_level;
	uint32_t avdd_volt_level;
	uint32_t dvdd_volt_level;
	uint32_t iovdd_volt_level;
	uint32_t power_down_level;
	uint32_t reset_level;
	uint32_t default_mclk;
} SENSOR_POWER_T;

typedef struct sensor_power_info {
	uint32_t            is_on;
	uint32_t            op_sensor_id;
	struct sensor_power main_sensor;
	struct sensor_power sub_sensor;
} SENSOR_POWER_CFG_T;


#ifndef BOOLEAN
#define BOOLEAN 					char
#endif
#define SENSOR_IOC_MAGIC			'R'

#define SENSOR_IO_PD                _IOW(SENSOR_IOC_MAGIC, 0,  BOOLEAN)
#define SENSOR_IO_SET_AVDD          _IOW(SENSOR_IOC_MAGIC, 1,  uint32_t)
#define SENSOR_IO_SET_DVDD          _IOW(SENSOR_IOC_MAGIC, 2,  uint32_t)
#define SENSOR_IO_SET_IOVDD         _IOW(SENSOR_IOC_MAGIC, 3,  uint32_t)
#define SENSOR_IO_SET_MCLK          _IOW(SENSOR_IOC_MAGIC, 4,  uint32_t)
#define SENSOR_IO_RST               _IOW(SENSOR_IOC_MAGIC, 5,  uint32_t)
#define SENSOR_IO_I2C_INIT          _IOW(SENSOR_IOC_MAGIC, 6,  uint32_t)
#define SENSOR_IO_I2C_DEINIT        _IOW(SENSOR_IOC_MAGIC, 7,  uint32_t)
#define SENSOR_IO_SET_ID            _IOW(SENSOR_IOC_MAGIC, 8,  uint32_t)
#define SENSOR_IO_RST_LEVEL         _IOW(SENSOR_IOC_MAGIC, 9,  uint32_t)
#define SENSOR_IO_I2C_ADDR          _IOW(SENSOR_IOC_MAGIC, 10, uint16_t)
#define SENSOR_IO_I2C_READ          _IOWR(SENSOR_IOC_MAGIC, 11, SENSOR_REG_BITS_T)
#define SENSOR_IO_I2C_WRITE         _IOW(SENSOR_IOC_MAGIC, 12, SENSOR_REG_BITS_T)
#define SENSOR_IO_SET_FLASH         _IOW(SENSOR_IOC_MAGIC, 13, uint32_t)
#define SENSOR_IO_I2C_WRITE_REGS    _IOW(SENSOR_IOC_MAGIC, 14, SENSOR_REG_TAB_T)
#define SENSOR_IO_SET_CAMMOT        _IOW(SENSOR_IOC_MAGIC, 15,  uint32_t)
#define SENSOR_IO_SET_I2CCLOCK      _IOW(SENSOR_IOC_MAGIC, 16,  uint32_t)
#define SENSOR_IO_I2C_WRITE_EXT     _IOW(SENSOR_IOC_MAGIC, 17,  SENSOR_I2C_T)
#define SENSOR_IO_GET_FLASH_LEVEL   _IOWR(SENSOR_IOC_MAGIC, 18,  SENSOR_FLASH_LEVEL_T)
#define SENSOR_IO_IF_CFG            _IOW(SENSOR_IOC_MAGIC, 19,  SENSOR_IF_CFG_T)
#define SENSOR_IO_POWER_CFG         _IOWR(SENSOR_IOC_MAGIC, 20,  SENSOR_POWER_CFG_T)
#define SENSOR_IO_I2C_READ_EXT      _IOWR(SENSOR_IOC_MAGIC, 21,  SENSOR_I2C_T)
#define SENSOR_IO_POWER_ONOFF	_IOW(SENSOR_IOC_MAGIC, 22, uint32_t)
#define SENSOR_IO_GET_SOCID         _IOWR(SENSOR_IOC_MAGIC, 255,  SENSOR_SOCID_T)




#endif //_SENSOR_DRV_K_H_
