/* Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef S5K3L6_OTP_H
#define S5K3L6_OTP_H

#include "cam_eeprom_dev.h"

static int cam_otp_s5k3l6_read_memory(struct cam_eeprom_ctrl_t *e_ctrl, struct cam_eeprom_memory_block_t *block);

static int cam_otp_s5k3l6_read(struct cam_eeprom_ctrl_t *e_ctrl, uint32_t *OTP_Page, 
										   uint32_t *data_addr, uint8_t *memptr, uint32_t *read_size)
{
	int rc = 0;
	struct cam_sensor_i2c_reg_setting i2c_reg_settings;
	struct cam_sensor_i2c_reg_array i2c_reg_array;
	enum camera_sensor_i2c_type data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	enum camera_sensor_i2c_type addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;

	if (!e_ctrl)
	{
		CAM_ERR(CAM_EEPROM, "e_ctrl is NULL");
		return -EINVAL;
	}
	i2c_reg_settings.addr_type = addr_type;
	i2c_reg_settings.data_type = data_type;
	i2c_reg_settings.size = 1;
	i2c_reg_settings.delay =0;
	i2c_reg_array.delay = 0;

	pr_info("%s:%d current page: %d\n", __func__, __LINE__, *OTP_Page);

	msleep(10);

	// make initial state as per guide
	i2c_reg_array.reg_addr = 0x0A00;
	i2c_reg_array.reg_data = 4;
	i2c_reg_settings.reg_setting = &i2c_reg_array;

	rc = camera_io_dev_write(&(e_ctrl->io_master_info), &i2c_reg_settings);
	if (rc < 0)
	{
		CAM_ERR(CAM_EEPROM,"%s:(%d) write addr failed\n", __func__, __LINE__);
		goto err;
	}

	// set the PAGE
	i2c_reg_array.reg_addr = 0x0A02;
	i2c_reg_array.reg_data = *OTP_Page;
	i2c_reg_settings.reg_setting = &i2c_reg_array;

	rc = camera_io_dev_write(&(e_ctrl->io_master_info), &i2c_reg_settings);
	if (rc < 0)
	{
		CAM_ERR(CAM_EEPROM,"%s:(%d) write addr failed\n", __func__, __LINE__);
		goto err;
	}

	// set read mode of NVM controller interface 1
	i2c_reg_array.reg_addr = 0x0A00;
	i2c_reg_array.reg_data = 1;
	i2c_reg_settings.reg_setting = &i2c_reg_array;

	rc = camera_io_dev_write(&(e_ctrl->io_master_info), &i2c_reg_settings);
	if (rc < 0)
	{
		CAM_ERR(CAM_EEPROM,"%s:(%d) write pluse failed\n", __func__, __LINE__);
		goto err;
	}

	// wait
	udelay(47);

	// read 1 byte
	rc = camera_io_dev_read_seq(&e_ctrl->io_master_info, *data_addr, memptr, addr_type, data_type, 1);
	if (rc < 0)
	{
		CAM_ERR(CAM_EEPROM, "read failed rc %d", rc);
		goto err;
	}
	
	// make initial state
	i2c_reg_array.reg_addr = 0x0A00;
	i2c_reg_array.reg_data = 4;
	i2c_reg_settings.reg_setting = &i2c_reg_array;

	rc = camera_io_dev_write(&(e_ctrl->io_master_info), &i2c_reg_settings);
	if (rc < 0)
	{
		CAM_ERR(CAM_EEPROM,"%s:(%d) write addr failed\n", __func__, __LINE__);
		goto err;
	}

	// disable NVM controller
	i2c_reg_array.reg_addr = 0x0A00;
	i2c_reg_array.reg_data = 0;
	i2c_reg_settings.reg_setting = &i2c_reg_array;

	rc = camera_io_dev_write(&(e_ctrl->io_master_info), &i2c_reg_settings);
	if (rc < 0)
	{
		CAM_ERR(CAM_EEPROM,"%s:(%d) write addr failed\n", __func__, __LINE__);
	}

err:
	return rc;
}

#endif /* S5K3L6_OTP_H */
