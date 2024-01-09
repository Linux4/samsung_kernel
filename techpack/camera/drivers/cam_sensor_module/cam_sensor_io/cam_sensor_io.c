// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include "cam_sensor_io.h"
#include "cam_sensor_i2c.h"

#if defined(CONFIG_CAMERA_CDR_TEST)
#include <linux/ktime.h>
extern char cdr_result[40];
extern uint64_t cdr_start_ts;
extern uint64_t cdr_end_ts;
static void cam_io_cdr_store_result()
{
	cdr_end_ts	= ktime_get();
	cdr_end_ts = cdr_end_ts / 1000 / 1000;
	sprintf(cdr_result, "%d,%lld\n", 1, cdr_end_ts-cdr_start_ts);
	CAM_INFO(CAM_CSIPHY, "[CDR_DBG] i2c_fail, time(ms): %llu", cdr_end_ts-cdr_start_ts);
}
#endif
int32_t camera_io_dev_poll(struct camera_io_master *io_master_info,
	uint32_t addr, uint16_t data, uint32_t data_mask,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	uint32_t delay_ms)
{
	int16_t mask = data_mask & 0xFF;
	int32_t rc = 0;

	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		rc = -EINVAL;
	}

	if (io_master_info->master_type == CCI_MASTER) {
		rc = cam_cci_i2c_poll(io_master_info->cci_client,
			addr, data, mask, data_type, addr_type, delay_ms);
	} else if (io_master_info->master_type == I2C_MASTER) {
		rc = cam_qup_i2c_poll(io_master_info->client,
			addr, data, data_mask, addr_type, data_type,
			delay_ms);
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Comm. Master:%d",
			io_master_info->master_type);
		rc = -EINVAL;
	}

#if defined(CONFIG_CAMERA_CDR_TEST)
	if (rc < 0)
		cam_io_cdr_store_result();
#endif

	return rc;
}

int32_t camera_io_dev_erase(struct camera_io_master *io_master_info,
	uint32_t addr, uint32_t size)
{
	int rc = 0;

	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		rc = -EINVAL;
	}

	if (size == 0)
		return rc;

	if (io_master_info->master_type == SPI_MASTER) {
		CAM_DBG(CAM_SENSOR, "Calling SPI Erase");
		rc = cam_spi_erase(io_master_info, addr,
			CAMERA_SENSOR_I2C_TYPE_WORD, size);
	} else if (io_master_info->master_type == I2C_MASTER ||
		io_master_info->master_type == CCI_MASTER) {
		CAM_ERR(CAM_SENSOR, "Erase not supported on master :%d",
			io_master_info->master_type);
		rc = -EINVAL;
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Comm. Master:%d",
			io_master_info->master_type);
		rc = -EINVAL;
	}

#if defined(CONFIG_CAMERA_CDR_TEST)
	if (rc < 0)
		cam_io_cdr_store_result();
#endif

	return rc;
}

int32_t camera_io_dev_read(struct camera_io_master *io_master_info,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type)
{
	int32_t rc = 0;

	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		rc = -EINVAL;
	}

	if (io_master_info->master_type == CCI_MASTER) {
		rc = cam_cci_i2c_read(io_master_info->cci_client,
			addr, data, addr_type, data_type);
	} else if (io_master_info->master_type == I2C_MASTER) {
		rc = cam_qup_i2c_read(io_master_info->client,
			addr, data, addr_type, data_type);
	} else if (io_master_info->master_type == SPI_MASTER) {
		rc = cam_spi_read(io_master_info,
			addr, data, addr_type, data_type);
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Comm. Master:%d",
			io_master_info->master_type);
		rc = -EINVAL;
	}

#if defined(CONFIG_CAMERA_CDR_TEST)
	if (rc < 0)
		cam_io_cdr_store_result();
#endif

	return rc;
}

int32_t camera_io_dev_read_seq(struct camera_io_master *io_master_info,
	uint32_t addr, uint8_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type, int32_t num_bytes)
{
	int32_t rc = 0;
	if (io_master_info->master_type == CCI_MASTER) {
		rc = cam_camera_cci_i2c_read_seq(io_master_info->cci_client,
			addr, data, addr_type, data_type, num_bytes);
	} else if (io_master_info->master_type == I2C_MASTER) {
		rc = cam_qup_i2c_read_seq(io_master_info->client,
			addr, data, addr_type, num_bytes);
	} else if (io_master_info->master_type == SPI_MASTER) {
		rc = cam_spi_read_seq(io_master_info,
			addr, data, addr_type, num_bytes);
	} else if (io_master_info->master_type == SPI_MASTER) {
		rc = cam_spi_write_seq(io_master_info,
			addr, data, addr_type, num_bytes);
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Comm. Master:%d",
			io_master_info->master_type);
		rc = -EINVAL;
	}

#if defined(CONFIG_CAMERA_CDR_TEST)
	if (rc < 0)
		cam_io_cdr_store_result();
#endif

	return rc;
}

int32_t camera_io_dev_write(struct camera_io_master *io_master_info,
	struct cam_sensor_i2c_reg_setting *write_setting)
{
	int32_t rc = 0;

	if (!write_setting || !io_master_info) {
		CAM_ERR(CAM_SENSOR,
			"Input parameters not valid ws: %pK ioinfo: %pK",
			write_setting, io_master_info);
		rc = -EINVAL;
	}

	if (!write_setting->reg_setting) {
		CAM_ERR(CAM_SENSOR, "Invalid Register Settings");
		rc = -EINVAL;
	}

	if (io_master_info->master_type == CCI_MASTER) {
		rc = cam_cci_i2c_write_table(io_master_info,
			write_setting);
	} else if (io_master_info->master_type == I2C_MASTER) {
		rc = cam_qup_i2c_write_table(io_master_info,
			write_setting);
	} else if (io_master_info->master_type == SPI_MASTER) {
		rc = cam_spi_write_table(io_master_info,
			write_setting);
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Comm. Master:%d",
			io_master_info->master_type);
		rc = -EINVAL;
	}

#if defined(CONFIG_CAMERA_CDR_TEST)
	if (rc < 0)
		cam_io_cdr_store_result();
#endif

	return rc;
}

int32_t camera_io_dev_write_continuous(struct camera_io_master *io_master_info,
	struct cam_sensor_i2c_reg_setting *write_setting,
	uint8_t cam_sensor_i2c_write_flag)
{
	int32_t rc = 0;

	if (!write_setting || !io_master_info) {
		CAM_ERR(CAM_SENSOR,
			"Input parameters not valid ws: %pK ioinfo: %pK",
			write_setting, io_master_info);
		rc = -EINVAL;
	}

	if (!write_setting->reg_setting) {
		CAM_ERR(CAM_SENSOR, "Invalid Register Settings");
		rc = -EINVAL;
	}

	if (io_master_info->master_type == CCI_MASTER) {
		rc = cam_cci_i2c_write_continuous_table(io_master_info,
			write_setting, cam_sensor_i2c_write_flag);
	} else if (io_master_info->master_type == I2C_MASTER) {
		rc = cam_qup_i2c_write_continuous_table(io_master_info,
			write_setting, cam_sensor_i2c_write_flag);
	} else if (io_master_info->master_type == SPI_MASTER) {
		rc = cam_spi_write_table(io_master_info,
			write_setting);
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid Comm. Master:%d",
			io_master_info->master_type);
		rc = -EINVAL;
	}

#if defined(CONFIG_CAMERA_CDR_TEST)
	if (rc < 0)
		cam_io_cdr_store_result();
#endif

	return rc;
}

int32_t camera_io_init(struct camera_io_master *io_master_info)
{
	int32_t rc = -EINVAL;

	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		rc = -EINVAL;
	}

	if (io_master_info->master_type == CCI_MASTER) {
		io_master_info->cci_client->cci_subdev =
		cam_cci_get_subdev(io_master_info->cci_client->cci_device);
		rc = cam_sensor_cci_i2c_util(io_master_info->cci_client,
			MSM_CCI_INIT);
	} else if ((io_master_info->master_type == I2C_MASTER) ||
			(io_master_info->master_type == SPI_MASTER)) {
		rc = 0;
	}

#if defined(CONFIG_CAMERA_CDR_TEST)
	if (rc < 0)
		cam_io_cdr_store_result();
#endif

	return rc;
}

int32_t camera_io_release(struct camera_io_master *io_master_info)
{
	int32_t rc = -EINVAL;

	if (!io_master_info) {
		CAM_ERR(CAM_SENSOR, "Invalid Args");
		rc = -EINVAL;
	}

	if (io_master_info->master_type == CCI_MASTER) {
		rc = cam_sensor_cci_i2c_util(io_master_info->cci_client,
			MSM_CCI_RELEASE);
	} else if ((io_master_info->master_type == I2C_MASTER) ||
			(io_master_info->master_type == SPI_MASTER)) {
		rc = 0;
	}

#if defined(CONFIG_CAMERA_CDR_TEST)
	if (rc < 0)
		cam_io_cdr_store_result();
#endif

	return rc;
}
