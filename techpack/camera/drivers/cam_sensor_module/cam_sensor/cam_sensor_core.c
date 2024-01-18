// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <cam_sensor_cmn_header.h>
#include "cam_sensor_core.h"
#include "cam_sensor_util.h"
#include "cam_soc_util.h"
#include "cam_trace.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"
#if defined(CONFIG_CAMERA_CDR_TEST)
#include <linux/ktime.h>
extern int cdr_value_exist;
extern uint64_t cdr_start_ts;
#endif

#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
#include "cam_sensor_mipi.h"

static int disable_adaptive_mipi;
module_param(disable_adaptive_mipi, int, 0644);
#endif

#if defined(CONFIG_CAMERA_FRAME_CNT_DBG)
static int frame_cnt_dbg;
module_param(frame_cnt_dbg, int, 0644);

#include "cam_sensor_thread.h"
#include <linux/slab.h>
#endif

#if defined(CONFIG_CAMERA_SSM_I2C_ENV)
struct cam_sensor_ctrl_t *g_s_ctrl_ssm;
#endif

#if defined(CONFIG_SENSOR_RETENTION)
#include "cam_sensor_retention.h"

static int disable_sensor_retention;
module_param(disable_sensor_retention, int, 0644);
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
//#define HWB_FILE_OPERATION 1
uint32_t sec_sensor_position;
uint32_t sec_sensor_clk_size;

static struct cam_hw_param_collector cam_hwparam_collector;
#endif

#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
int32_t cam_check_stream_on(
	struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t ret = 0;
	uint16_t sensor_id = 0;

	if (disable_adaptive_mipi) {
		CAM_INFO(CAM_SENSOR, "Disabled Adaptive MIPI");
		return ret;
	}

#if defined(CONFIG_CAMERA_FRS_DRAM_TEST)
	if (rear_frs_test_mode >= 1) {
		CAM_ERR(CAM_SENSOR, "[FRS_DBG] No DYNAMIC_MIPI, rear_frs_test_mode : %ld", rear_frs_test_mode);
		return ret;
	}
#endif
	sensor_id = s_ctrl->sensordata->slave_info.sensor_id;
	switch (sensor_id) {
		case FRONT_SENSOR_ID_IMX374:
		case SENSOR_ID_IMX555:
		case SENSOR_ID_IMX563:
		case SENSOR_ID_S5KGW2:
		case SENSOR_ID_S5K3J1:
		case SENSOR_ID_S5KGH1:
		case SENSOR_ID_S5KHM3:
		case SENSOR_ID_IMX258:
		case FRONT_SENSOR_ID_IMX471:
#if defined(CONFIG_SEC_Q2Q_PROJECT) || defined(CONFIG_SEC_V2Q_PROJECT)
		case SENSOR_ID_HI1337:
#endif
		case SENSOR_ID_S5KJN1:
		case SENSOR_ID_S5KJN1_1:
#if defined(CONFIG_SEC_M44X_PROJECT)
		case SENSOR_ID_HI1336:
#endif
			ret = 1;
 			break;
		default:
			ret =0;
			break;
	}

#ifdef CONFIG_SEC_FACTORY
	ret = 0;
#endif

	return ret;
}

int cam_sensor_apply_adaptive_mipi_settings(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;
	struct i2c_settings_list mipi_i2c_list;
	uint16_t sensor_id = 0;

	sensor_id = s_ctrl->sensordata->slave_info.sensor_id;

	if (cam_check_stream_on(s_ctrl))
	{
 #if defined(CONFIG_CAMERA_FRS_DRAM_TEST)
		if (rear_frs_test_mode == 0) {
#endif
			cam_mipi_init_setting(s_ctrl);
			cam_mipi_update_info(s_ctrl);
			cam_mipi_get_clock_string(s_ctrl);
#if defined(CONFIG_CAMERA_FRS_DRAM_TEST)
		}
#endif
 	}

	if (cam_check_stream_on(s_ctrl)
		&& s_ctrl->mipi_clock_index_new != INVALID_MIPI_INDEX
		&& s_ctrl->i2c_data.streamon_settings.is_settings_valid) {
		CAM_DBG(CAM_SENSOR, "[CAM_DBG] Write MIPI setting before Stream On setting. mipi_index : %d",
			s_ctrl->mipi_clock_index_new);

 		cur_mipi_sensor_mode = &(s_ctrl->mipi_info[0]);
		memset(&mipi_i2c_list, 0, sizeof(mipi_i2c_list));

		memcpy(&mipi_i2c_list.i2c_settings,
			cur_mipi_sensor_mode->mipi_setting[s_ctrl->mipi_clock_index_new].clk_setting,
			sizeof(struct cam_sensor_i2c_reg_setting));

		CAM_INFO(CAM_SENSOR, "[CAM_DBG] Picked MIPI clock : %s",
			cur_mipi_sensor_mode->mipi_setting[s_ctrl->mipi_clock_index_new].str_mipi_clk);

		if (mipi_i2c_list.i2c_settings.size > 0)
			rc = camera_io_dev_write(&s_ctrl->io_master_info,
				&(mipi_i2c_list.i2c_settings));
	}

	return rc;
}
#endif

#if defined(CONFIG_CAMERA_FRAME_CNT_CHECK)
int cam_sensor_read_frame_count(
	struct cam_sensor_ctrl_t *s_ctrl,
	uint32_t* frame_cnt)
{
	int rc = 0;
	uint32_t FRAME_COUNT_REG_ADDR = 0x0005;
	if (s_ctrl->sensordata->slave_info.sensor_id == HI847_SENSOR_ID)
		FRAME_COUNT_REG_ADDR = 0x0732;

	rc = camera_io_dev_read(&s_ctrl->io_master_info, FRAME_COUNT_REG_ADDR,
		frame_cnt, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "[CNT_DBG] Failed to read frame_cnt");

	return rc;
}

int cam_sensor_wait_stream_on(
	struct cam_sensor_ctrl_t *s_ctrl,
	int retry_cnt)
{
	int rc = 0;
	uint32_t frame_cnt = 0;

#if defined(CONFIG_SEC_Q2Q_PROJECT)
		if (s_ctrl->sensordata->slave_info.sensor_id == FRONT_SENSOR_ID_IMX374) {
			rc = gpio_get_value_cansleep(MIPI_SW_SEL_GPIO);
			CAM_INFO(CAM_SENSOR, "[0x%x]: mipi_sw_sel_gpio value = %d",s_ctrl->sensordata->slave_info.sensor_id, rc);
		}
#endif

	CAM_DBG(CAM_SENSOR, "E");

	do {
		rc = cam_sensor_read_frame_count(s_ctrl, &frame_cnt);
		if (rc < 0)
			break;

		if ((s_ctrl->sensordata->slave_info.sensor_id == HI847_SENSOR_ID) && ((frame_cnt & 0x01)  == 0x01)) {
			usleep_range(4000, 5000);
			CAM_INFO(CAM_SENSOR, "[CNT_DBG] 0x%x : Last frame_cnt 0x%x",
				s_ctrl->sensordata->slave_info.sensor_id, frame_cnt);
			return 0;
		} else if ((frame_cnt != 0xFF) &&	(frame_cnt > 0)) {
			CAM_INFO(CAM_SENSOR, "[CNT_DBG] 0x%x : Last frame_cnt 0x%x",
				s_ctrl->sensordata->slave_info.sensor_id, frame_cnt);
			return 0;
		}
		CAM_INFO(CAM_SENSOR, "[CNT_DBG] retry cnt : %d, Stream off, frame_cnt : 0x%x", retry_cnt, frame_cnt);
		retry_cnt--;
		usleep_range(5000, 6000);
	} while ((frame_cnt < 0x01 || frame_cnt == 0xFF) && (retry_cnt > 0));

	CAM_INFO(CAM_SENSOR, "[CNT_DBG] wait fail rc %d retry cnt : %d, frame_cnt : 0x%x", rc, retry_cnt, frame_cnt);

	CAM_DBG(CAM_SENSOR, "X");

	return -1;
}

int cam_sensor_wait_stream_off(
	struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint32_t frame_cnt = 0;
	int retry_cnt = 30;

	CAM_DBG(CAM_SENSOR, "E");

	usleep_range(2000, 3000);
	do {
		rc = cam_sensor_read_frame_count(s_ctrl, &frame_cnt);
		if (rc < 0)
			break;

		if ((s_ctrl->sensordata->slave_info.sensor_id == HI847_SENSOR_ID) && ((frame_cnt & 0x01)  == 0x00)) {
			usleep_range(1000, 1010);
			return 0;
		} else if (frame_cnt == 0xFF)
			return 0;


		CAM_INFO(CAM_SENSOR, "[CNT_DBG] retry cnt : %d, Stream off, frame_cnt : 0x%x", retry_cnt, frame_cnt);
		retry_cnt--;
		usleep_range(5000, 6000);
	} while ((frame_cnt != 0xFF) && (retry_cnt > 0));

	CAM_INFO(CAM_SENSOR, "[CNT_DBG] wait fail rc %d retry cnt : %d, frame_cnt : 0x%x", rc, retry_cnt, frame_cnt);

	CAM_DBG(CAM_SENSOR, "X");
	return -1;
}
#endif

#if defined(CONFIG_SENSOR_RETENTION)
uint8_t sensor_retention_mode = RETENTION_INIT;
int cam_sensor_retention_calc_checksum(struct cam_sensor_ctrl_t *s_ctrl)
{
	uint32_t read_value = 0xBEEF;
	uint8_t read_cnt = 0;
	int rc = -1;
	uint32_t sensor_id = 0;

	sensor_id = s_ctrl->sensordata->slave_info.sensor_id;

	// Not retention sensor - Always write init settings
	if (sensor_id != RETENTION_SENSOR_ID)
		return rc;

	if (disable_sensor_retention > 0) {
		CAM_INFO(CAM_SENSOR, "[RET_DBG] retention disabled");
		return rc;
	}

	// Retention sensor, but Not retention - write init settings
	if (sensor_retention_mode != RETENTION_ON)
		return rc;

	CAM_INFO(CAM_SENSOR, "[RET_DBG] cam_sensor_retention_calc_checksum");
	for (read_cnt = 0; read_cnt < SENSOR_RETENTION_READ_RETRY_CNT; read_cnt++) {
		// 1. Wait - 6ms delay
		usleep_range(10000, 11000);

		// 2. Check result for checksum test - read addr: 0x19C2
		camera_io_dev_read(&s_ctrl->io_master_info, 0x19C2, &read_value,
			CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD);
		if (read_value == 0x0100) {
			CAM_INFO(CAM_SENSOR, "[RET_DBG] Pass checksum test");
			rc = 0;
			break;
		} else {
			CAM_ERR(CAM_SENSOR, "[RET_DBG] Fail checksum test retry : %d", read_cnt);
		}
	}

	if ((read_cnt == SENSOR_RETENTION_READ_RETRY_CNT) && (read_value != 0x0100)) {
		CAM_ERR(CAM_SENSOR, "[RET_DBG] Fail checksum test! 0x%x", read_value);
		rc = -1;
	}

	return rc;
}

int cam_sensor_write_retention_setting(
	struct camera_io_master *io_master_info,
	struct cam_sensor_i2c_reg_setting* settings,
	uint32_t settings_size)
{
	int32_t rc = 0;
	uint32_t i = 0, size = 0;
	struct cam_sensor_i2c_reg_setting reg_setting;

	for (i = 0; i < settings_size; i++) {
		if (size < settings[i].size)
			size = settings[i].size;
	}

	reg_setting.reg_setting = kmalloc(sizeof(struct cam_sensor_i2c_reg_array) * size, GFP_KERNEL);
	for (i = 0; i < settings_size; i++) {
		size = settings[i].size;
		memcpy(reg_setting.reg_setting,
			settings[i].reg_setting,
			sizeof(struct cam_sensor_i2c_reg_array) * size);
		reg_setting.size = size;
		reg_setting.addr_type = settings[i].addr_type;
		reg_setting.data_type = settings[i].data_type;
		reg_setting.delay = settings[i].delay;
		rc = camera_io_dev_write(io_master_info,
			&reg_setting);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"Failed to random write I2C settings[%d]: %d", i, rc);
	}

	if (reg_setting.reg_setting) {
		kfree(reg_setting.reg_setting);
		reg_setting.reg_setting = NULL;
	}

	return rc;
}

int cam_sensor_write_normal_init(struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint32_t sensor_id = 0;

	sensor_id = s_ctrl->sensordata->slave_info.sensor_id;

	if(sensor_id != RETENTION_SENSOR_ID)
		return rc;

	if (disable_sensor_retention > 0) {
		CAM_INFO(CAM_SENSOR, "[RET_DBG] retention disabled");
		return rc;
	}

	if (sensor_retention_mode != RETENTION_INIT)
		return rc;

	CAM_INFO(CAM_SENSOR, "[RET_DBG] E");
#if 1
	if (s_ctrl->i2c_data.init_settings.is_settings_valid &&
		(s_ctrl->i2c_data.init_settings.request_id == 0)) {
		rc = cam_sensor_apply_settings(s_ctrl, 0,
			CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"[RET_DBG] Failed to write init rc = %d", rc);
			goto end;
		}

		CAM_INFO(CAM_SENSOR, "[RET_DBG] stream on");
		rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
			stream_on_settings, ARRAY_SIZE(stream_on_settings));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"[RET_DBG] Failed to write stream on off init rc = %d", rc);
			goto end;
		}
#if defined(CONFIG_CAMERA_FRAME_CNT_CHECK)
		cam_sensor_wait_stream_on(s_ctrl, 1000);
#endif

		CAM_INFO(CAM_SENSOR, "[RET_DBG] stream off");
		rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
			stream_off_settings, ARRAY_SIZE(stream_off_settings));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"[RET_DBG] Failed to write stream on off init rc = %d", rc);
			goto end;
		}
#if defined(CONFIG_CAMERA_FRAME_CNT_CHECK)
		cam_sensor_wait_stream_off(s_ctrl);
#endif
	}
#else
	rc = cam_sensor_write_retention_setting(io_master_info,
		normal_init_setting, ARRAY_SIZE(normal_init_setting));
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR,
				"[RET_DBG] Failed to write normal init rc = %d", rc);
		goto end;
	}
#endif
	sensor_retention_mode = RETENTION_READY_TO_ON;
end:
	CAM_INFO(CAM_SENSOR, "[RET_DBG] X");

	return rc;
}

void cam_sensor_write_enable_crc(struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint32_t sensor_id = 0;

	sensor_id = s_ctrl->sensordata->slave_info.sensor_id;

	if(sensor_id != RETENTION_SENSOR_ID)
		return;

	CAM_INFO(CAM_SENSOR, "[RET_DBG] cam_sensor_write_enable_crc");
	rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
		retention_enable_settings, ARRAY_SIZE(retention_enable_settings));
	if (rc < 0)
		CAM_ERR(CAM_SENSOR,
				"[RET_DBG] Failed to retention enable rc = %d", rc);
}
#endif

#if defined(CONFIG_CAMERA_HYPERLAPSE_300X)
int cam_sensor_apply_hyperlapse_settings(
	struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
#if defined(CONFIG_SEC_P3Q_PROJECT)
		struct cam_sensor_i2c_reg_array i2c_fll_reg_array[] = {
        {0x0104, 0x0101, 0, 0},// GPH on
        {0x0702, 0x0000, 0, 0},// FLL shifter
        {0x0704, 0x0000, 0, 0},// CIT shifter
        {0x0340, 0x181C, 0, 0},// FLL
        {0x0202, 0x0D11, 0, 0},// CIT
        {0x0204, 0x0063, 0, 0},// A gain
        {0x020E, 0x0100, 0, 0},// D gain
        {0x0104, 0x0001, 0, 0},// GPH off
    };
        struct cam_sensor_i2c_reg_array i2c_streamoff_reg_array[] = {
        {0x0100, 0x0000, 0, 0},
    };
        struct cam_sensor_i2c_reg_array i2c_streamon_reg_array[] = {
        {0x0100, 0x0100, 0, 0},
    };
	if (s_ctrl->sensordata->slave_info.sensor_id != SENSOR_ID_S5KHM3)
    {
	    CAM_ERR(CAM_SENSOR, "[ASTRO_DBG] sensor_id != SENSOR_ID_S5KHM3");
        return rc;
    }
#endif

#if defined(CONFIG_SEC_B2Q_PROJECT) 
	 struct cam_sensor_i2c_reg_array i2c_fll_reg_array[] = {
        {0x0104, 0x01, 0, 0}, //group hold on
        {0x3100, 0x00, 0, 0}, //COARSE_SHORT_INT_TIME_SHIFTER
        {0x3101, 0x00, 0, 0}, //FRAME_LENGTH_LINES_SHIFTER
        {0x0340, 0x1F, 0, 0}, //FLL
        {0x0341, 0x42, 0, 0}, //FLL
        {0x0104, 0x00, 0, 0}, // group hold off
    };
    struct cam_sensor_i2c_reg_array i2c_streamoff_reg_array[] = {
        {0x0100, 0x0, 0, 0},
    };
    struct cam_sensor_i2c_reg_array i2c_streamon_reg_array[] = {
        {0x0100, 0x1, 0, 0},
    };
	if (s_ctrl->sensordata->slave_info.sensor_id != SENSOR_ID_IMX563)
    {
	    CAM_ERR(CAM_SENSOR, "[ASTRO_DBG] sensor_id != SENSOR_ID_IMX563");
        return rc;
    }
#endif

#if defined(CONFIG_SEC_O1Q_PROJECT) || defined(CONFIG_SEC_R9Q_PROJECT) || defined(CONFIG_SEC_T2Q_PROJECT) || defined(CONFIG_SEC_Q2Q_PROJECT) || defined(CONFIG_SEC_V2Q_PROJECT)
		struct cam_sensor_i2c_reg_array i2c_fll_reg_array[] = {
        {0x0104, 0x01, 0, 0}, //group hold on
        {0x3100, 0x00, 0, 0}, //COARSE_SHORT_INT_TIME_SHIFTER
        {0x3101, 0x00, 0, 0}, //FRAME_LENGTH_LINES_SHIFTER
        {0x0340, 0x31, 0, 0}, //FLL
        {0x0341, 0xD0, 0, 0}, //FLL
        {0x0104, 0x00, 0, 0}, // group hold off
    };
        struct cam_sensor_i2c_reg_array i2c_streamoff_reg_array[] = {
        {0x0100, 0x0, 0, 0},
    };
        struct cam_sensor_i2c_reg_array i2c_streamon_reg_array[] = {
        {0x0100, 0x01, 0, 0},
    };
	if (s_ctrl->sensordata->slave_info.sensor_id != SENSOR_ID_IMX555)
    {
	    CAM_ERR(CAM_SENSOR, "[ASTRO_DBG] sensor_id != SENSOR_ID_IMX555");
        return rc;
    }
#endif

#if defined(CONFIG_SEC_B2Q_PROJECT) || defined(CONFIG_SEC_O1Q_PROJECT) || defined(CONFIG_SEC_R9Q_PROJECT) || defined(CONFIG_SEC_P3Q_PROJECT) || defined(CONFIG_SEC_T2Q_PROJECT) || defined(CONFIG_SEC_Q2Q_PROJECT) || defined(CONFIG_SEC_V2Q_PROJECT)

	if (s_ctrl->shooting_mode == 16)
	{
		int size = 0;
		struct cam_sensor_i2c_reg_setting reg_fllsetting;
		struct cam_sensor_i2c_reg_setting reg_streamoffsetting;
		struct cam_sensor_i2c_reg_setting reg_streamonsetting;
		size = ARRAY_SIZE(i2c_fll_reg_array);
		CAM_ERR(CAM_SENSOR, "[ASTRO_DBG] write register settings :: StreamOff -> FLL -> StreamOn");
		reg_fllsetting.reg_setting = kmalloc(sizeof(struct cam_sensor_i2c_reg_array) * size, GFP_KERNEL);
		if (reg_fllsetting.reg_setting != NULL) {
			reg_fllsetting.size = size;
			reg_fllsetting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
			if (s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_S5KHM3)
			reg_fllsetting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		    else
			reg_fllsetting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
			reg_fllsetting.delay = 0;
			memcpy(reg_fllsetting.reg_setting, &i2c_fll_reg_array, sizeof(struct cam_sensor_i2c_reg_array) * size);
			CAM_ERR(CAM_SENSOR, "[ASTRO_DBG] fll size = %d", size);
		}
		size = ARRAY_SIZE(i2c_streamoff_reg_array);
		reg_streamoffsetting.reg_setting = kmalloc(sizeof(struct cam_sensor_i2c_reg_array) * size, GFP_KERNEL);
		if (reg_streamoffsetting.reg_setting != NULL) {
			reg_streamoffsetting.size = size;
			reg_streamoffsetting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
			if (s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_S5KHM3)
			reg_fllsetting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		    else
			reg_fllsetting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
			reg_streamoffsetting.delay = 0;
			memcpy(reg_streamoffsetting.reg_setting, &i2c_streamoff_reg_array, sizeof(struct cam_sensor_i2c_reg_array) * size);
			CAM_ERR(CAM_SENSOR, "[ASTRO_DBG] streamoff size = %d", size);
		}
		size = ARRAY_SIZE(i2c_streamon_reg_array);
		reg_streamonsetting.reg_setting = kmalloc(sizeof(struct cam_sensor_i2c_reg_array) * size, GFP_KERNEL);
		if (reg_streamonsetting.reg_setting != NULL) {
			reg_streamonsetting.size = size;
			reg_streamonsetting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
			if (s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_S5KHM3)
			reg_fllsetting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		    else
			reg_fllsetting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
			reg_streamonsetting.delay = 0;
			memcpy(reg_streamonsetting.reg_setting, &i2c_streamon_reg_array, sizeof(struct cam_sensor_i2c_reg_array) * size);
			CAM_ERR(CAM_SENSOR, "[ASTRO_DBG] stream on size = %d", size);
		}
		rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_streamoffsetting);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "[ASTRO_DBG] Failed to write streamoff settings %d", rc);
		cam_sensor_wait_stream_off(s_ctrl);
		rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_fllsetting);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "[ASTRO_DBG] Failed to write fll settings %d", rc);
		rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_streamonsetting);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "[ASTRO_DBG] Failed to write streamon settings %d", rc);
		if (reg_fllsetting.reg_setting) {
			kfree(reg_fllsetting.reg_setting);
			reg_fllsetting.reg_setting = NULL;
		}
		if (reg_streamoffsetting.reg_setting) {
			kfree(reg_streamoffsetting.reg_setting);
			reg_streamoffsetting.reg_setting = NULL;
		}
		if (reg_streamonsetting.reg_setting) {
			kfree(reg_streamonsetting.reg_setting);
			reg_streamonsetting.reg_setting = NULL;
		}
	}
#endif
	return rc;
}
#endif
#if 1
int cam_sensor_pre_apply_settings(
	struct cam_sensor_ctrl_t *s_ctrl,
	enum cam_sensor_packet_opcodes opcode)
{
	int rc = 0;
	switch (opcode) {
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF: {
#if defined(CONFIG_CAMERA_HYPERLAPSE_300X)
		cam_sensor_apply_hyperlapse_settings(s_ctrl);
#endif
#if defined(CONFIG_CAMERA_FRAME_CNT_CHECK)
			rc = cam_sensor_wait_stream_on(s_ctrl, 10);
#endif
#if defined(CONFIG_SENSOR_RETENTION)
			cam_sensor_write_enable_crc(s_ctrl);
#endif
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON: {
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
			rc = cam_sensor_apply_adaptive_mipi_settings(s_ctrl);
			break;
#endif
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE:
		default:
			return 0;
	}
	return rc;
}

int cam_sensor_post_apply_settings(
	struct cam_sensor_ctrl_t *s_ctrl,
	enum cam_sensor_packet_opcodes opcode)
{
	int rc = 0;
	switch (opcode) {
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF: {
#if defined(CONFIG_CAMERA_FRAME_CNT_CHECK)
			cam_sensor_wait_stream_off(s_ctrl);
#endif
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE:
		default:
			return 0;
	}
	return rc;
}
#endif

static int cam_sensor_update_req_mgr(
	struct cam_sensor_ctrl_t *s_ctrl,
	struct cam_packet *csl_packet)
{
	int rc = 0;
	struct cam_req_mgr_add_request add_req;

	add_req.link_hdl = s_ctrl->bridge_intf.link_hdl;
	add_req.req_id = csl_packet->header.request_id;
	CAM_DBG(CAM_SENSOR, " Rxed Req Id: %llu",
		csl_packet->header.request_id);
	add_req.dev_hdl = s_ctrl->bridge_intf.device_hdl;
	add_req.skip_before_applying = 0;
	add_req.trigger_eof = false;
	if (s_ctrl->bridge_intf.crm_cb &&
		s_ctrl->bridge_intf.crm_cb->add_req) {
		rc = s_ctrl->bridge_intf.crm_cb->add_req(&add_req);
		if (rc) {
			CAM_ERR(CAM_SENSOR,
				"Adding request: %llu failed with request manager rc: %d",
				csl_packet->header.request_id, rc);
			return rc;
		}
	}

	CAM_DBG(CAM_SENSOR, "Successfully add req: %llu to req mgr",
			add_req.req_id);
	return rc;
}

static void cam_sensor_release_stream_rsc(
	struct cam_sensor_ctrl_t *s_ctrl)
{
	struct i2c_settings_array *i2c_set = NULL;
	int rc;

	i2c_set = &(s_ctrl->i2c_data.streamoff_settings);
	if (i2c_set->is_settings_valid == 1) {
		i2c_set->is_settings_valid = -1;
		rc = delete_request(i2c_set);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"failed while deleting Streamoff settings");
	}

	i2c_set = &(s_ctrl->i2c_data.streamon_settings);
	if (i2c_set->is_settings_valid == 1) {
		i2c_set->is_settings_valid = -1;
		rc = delete_request(i2c_set);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"failed while deleting Streamon settings");
	}
}

static void cam_sensor_release_per_frame_resource(
	struct cam_sensor_ctrl_t *s_ctrl)
{
	struct i2c_settings_array *i2c_set = NULL;
	int i, rc;

	if (s_ctrl->i2c_data.per_frame != NULL) {
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			i2c_set = &(s_ctrl->i2c_data.per_frame[i]);
			if (i2c_set->is_settings_valid == 1) {
				i2c_set->is_settings_valid = -1;
				rc = delete_request(i2c_set);
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"delete request: %lld rc: %d",
						i2c_set->request_id, rc);
			}
		}
	}

	if (s_ctrl->i2c_data.frame_skip != NULL) {
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			i2c_set = &(s_ctrl->i2c_data.frame_skip[i]);
			if (i2c_set->is_settings_valid == 1) {
				i2c_set->is_settings_valid = -1;
				rc = delete_request(i2c_set);
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"delete request: %lld rc: %d",
						i2c_set->request_id, rc);
			}
		}
	}
}

static int32_t cam_sensor_i2c_pkt_parse(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg)
{
	int32_t rc = 0;
	uintptr_t generic_ptr;
	struct cam_control *ioctl_ctrl = NULL;
	struct cam_packet *csl_packet = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	struct cam_buf_io_cfg *io_cfg = NULL;
	struct i2c_settings_array *i2c_reg_settings = NULL;
	size_t len_of_buff = 0;
	size_t remain_len = 0;
	uint32_t *offset = NULL;
	struct cam_config_dev_cmd config;
	struct i2c_data_settings *i2c_data = NULL;

	ioctl_ctrl = (struct cam_control *)arg;

	if (ioctl_ctrl->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_SENSOR, "Invalid Handle Type");
		return -EINVAL;
	}

	if (copy_from_user(&config,
		u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(config)))
		return -EFAULT;

	rc = cam_mem_get_cpu_buf(
		config.packet_handle,
		&generic_ptr,
		&len_of_buff);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Failed in getting the packet: %d", rc);
		return rc;
	}

	remain_len = len_of_buff;
	if ((sizeof(struct cam_packet) > len_of_buff) ||
		((size_t)config.offset >= len_of_buff -
		sizeof(struct cam_packet))) {
		CAM_ERR(CAM_SENSOR,
			"Inval cam_packet strut size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), len_of_buff);
		rc = -EINVAL;
		goto end;
	}

	remain_len -= (size_t)config.offset;
	csl_packet = (struct cam_packet *)(generic_ptr +
		(uint32_t)config.offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_SENSOR, "Invalid packet params");
		rc = -EINVAL;
		goto end;
	}
	#if defined(CONFIG_CAMERA_HYPERLAPSE_300X)	
	if((csl_packet->header.op_code & 0xFFFFFF) == CAM_SENSOR_PACKET_OPCODE_SENSOR_SHOOTINGMODE) {
		CAM_DBG(CAM_SENSOR, "[ASTRO_DBG] Shooting_Mode : %d", csl_packet->header.request_id);
		s_ctrl->shooting_mode = csl_packet->header.request_id;
		goto end;
	}
    #endif

	if ((csl_packet->header.op_code & 0xFFFFFF) !=
		CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG &&
		csl_packet->header.request_id <= s_ctrl->last_flush_req
		&& s_ctrl->last_flush_req != 0) {
		CAM_ERR(CAM_SENSOR,
			"reject request %lld, last request to flush %u",
			csl_packet->header.request_id, s_ctrl->last_flush_req);
		rc = -EINVAL;
		goto end;
	}

	if (csl_packet->header.request_id > s_ctrl->last_flush_req)
		s_ctrl->last_flush_req = 0;

	i2c_data = &(s_ctrl->i2c_data);
	CAM_DBG(CAM_SENSOR, "Header OpCode: %d", csl_packet->header.op_code);
	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG: {
		i2c_reg_settings = &i2c_data->init_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG: {
		i2c_reg_settings = &i2c_data->config_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON: {
		if (s_ctrl->streamon_count > 0)
			goto end;

		s_ctrl->streamon_count = s_ctrl->streamon_count + 1;
		i2c_reg_settings = &i2c_data->streamon_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF: {
		if (s_ctrl->streamoff_count > 0)
			goto end;

		s_ctrl->streamoff_count = s_ctrl->streamoff_count + 1;
		i2c_reg_settings = &i2c_data->streamoff_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}

	case CAM_SENSOR_PACKET_OPCODE_SENSOR_MODE: {
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
		CAM_DBG(CAM_SENSOR, "[CAM_DBG] SENSOR_MODE : %d", csl_packet->header.request_id);
		s_ctrl->sensor_mode = csl_packet->header.request_id;
#endif
		break;
	}

	case CAM_SENSOR_PACKET_OPCODE_SENSOR_READ: {
		i2c_reg_settings = &(i2c_data->read_settings);
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;

		CAM_DBG(CAM_SENSOR, "number of IO configs: %d:",
			csl_packet->num_io_configs);
		if (csl_packet->num_io_configs == 0) {
			CAM_ERR(CAM_SENSOR, "No I/O configs to process");
			goto end;
		}

		io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
			&csl_packet->payload +
			csl_packet->io_configs_offset);

		if (io_cfg == NULL) {
			CAM_ERR(CAM_SENSOR, "I/O config is invalid(NULL)");
			goto end;
		}
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE)) {
			CAM_WARN(CAM_SENSOR,
				"Rxed Update packets without linking");
			goto end;
		}

		i2c_reg_settings =
			&i2c_data->per_frame[csl_packet->header.request_id %
				MAX_PER_FRAME_ARRAY];
		CAM_DBG(CAM_SENSOR, "Received Packet: %lld req: %lld",
			csl_packet->header.request_id % MAX_PER_FRAME_ARRAY,
			csl_packet->header.request_id);
		if (i2c_reg_settings->is_settings_valid == 1) {
			CAM_ERR(CAM_SENSOR,
				"Already some pkt in offset req : %lld",
				csl_packet->header.request_id);
			/*
			 * Update req mgr even in case of failure.
			 * This will help not to wait indefinitely
			 * and freeze. If this log is triggered then
			 * fix it.
			 */
			rc = cam_sensor_update_req_mgr(s_ctrl, csl_packet);
			if (rc)
				CAM_ERR(CAM_SENSOR,
					"Failed in adding request to req_mgr");
			goto end;
		}
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_FRAME_SKIP_UPDATE: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE)) {
			CAM_WARN(CAM_SENSOR,
				"Rxed Update packets without linking");
			goto end;
		}

		i2c_reg_settings =
			&i2c_data->frame_skip[csl_packet->header.request_id %
				MAX_PER_FRAME_ARRAY];
		CAM_DBG(CAM_SENSOR, "Received not ready packet: %lld req: %lld",
			csl_packet->header.request_id % MAX_PER_FRAME_ARRAY,
			csl_packet->header.request_id);
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_NOP: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE)) {
			CAM_WARN(CAM_SENSOR,
				"Rxed NOP packets without linking");
			goto end;
		}

		i2c_reg_settings =
			&i2c_data->per_frame[csl_packet->header.request_id %
				MAX_PER_FRAME_ARRAY];
		i2c_reg_settings->request_id = csl_packet->header.request_id;
		i2c_reg_settings->is_settings_valid = 1;

		rc = cam_sensor_update_req_mgr(s_ctrl, csl_packet);
		if (rc)
			CAM_ERR(CAM_SENSOR,
				"Failed in adding request to req_mgr");
		goto end;
	}
	default:
		CAM_ERR(CAM_SENSOR, "Invalid Packet Header");
		rc = -EINVAL;
		goto end;
	}

	offset = (uint32_t *)&csl_packet->payload;
	offset += csl_packet->cmd_buf_offset / 4;
	cmd_desc = (struct cam_cmd_buf_desc *)(offset);

	rc = cam_sensor_i2c_command_parser(&s_ctrl->io_master_info,
			i2c_reg_settings, cmd_desc, 1, io_cfg);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Fail parsing I2C Pkt: %d", rc);
		goto end;
	}

	if ((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE) {
		i2c_reg_settings->request_id =
			csl_packet->header.request_id;
		rc = cam_sensor_update_req_mgr(s_ctrl, csl_packet);
		if (rc) {
			CAM_ERR(CAM_SENSOR,
				"Failed in adding request to req_mgr");
			goto end;
		}
	}

	if ((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_SENSOR_PACKET_OPCODE_SENSOR_FRAME_SKIP_UPDATE) {
		i2c_reg_settings->request_id =
			csl_packet->header.request_id;
	}

end:
	return rc;
}

static int32_t cam_sensor_i2c_modes_util(
	struct camera_io_master *io_master_info,
	struct i2c_settings_list *i2c_list)
{
	int32_t rc = 0;
	uint32_t i, size;

	if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_RANDOM) {
#if 1
		struct cam_sensor_i2c_reg_array *reg_setting;
		uint32_t i2c_size = 0, org_size = 0, offset = 0;

		if (i2c_list->i2c_settings.size >  CCI_I2C_MAX_WRITE) {
			reg_setting = i2c_list->i2c_settings.reg_setting;
			org_size = i2c_list->i2c_settings.size;

			while(offset < org_size) {
				i2c_list->i2c_settings.reg_setting = reg_setting + offset;
				i2c_size = org_size - offset;
				if (i2c_size > CCI_I2C_MAX_WRITE)
					i2c_size = CCI_I2C_MAX_WRITE - 1;
				i2c_list->i2c_settings.size = i2c_size;
				rc = camera_io_dev_write(io_master_info,
					&(i2c_list->i2c_settings));
				if (rc < 0)
					break;
				offset += i2c_size;
			}
			i2c_list->i2c_settings.reg_setting = reg_setting;
			i2c_list->i2c_settings.size = org_size;
		}
		else
#endif
		rc = camera_io_dev_write(io_master_info,
			&(i2c_list->i2c_settings));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to random write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_SEQ) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			0);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to seq write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_BURST) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			1);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to burst write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_POLL) {
		size = i2c_list->i2c_settings.size;
		for (i = 0; i < size; i++) {
			rc = camera_io_dev_poll(
			io_master_info,
			i2c_list->i2c_settings.reg_setting[i].reg_addr,
			i2c_list->i2c_settings.reg_setting[i].reg_data,
			i2c_list->i2c_settings.reg_setting[i].data_mask,
			i2c_list->i2c_settings.addr_type,
			i2c_list->i2c_settings.data_type,
			i2c_list->i2c_settings.reg_setting[i].delay);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"i2c poll apply setting Fail: %d", rc);
				return rc;
			}
		}
	}

	return rc;
}

int32_t cam_sensor_update_i2c_info(struct cam_cmd_i2c_info *i2c_info,
	struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct cam_sensor_cci_client   *cci_client = NULL;

	if (s_ctrl->io_master_info.master_type == CCI_MASTER) {
		cci_client = s_ctrl->io_master_info.cci_client;
		if (!cci_client) {
			CAM_ERR(CAM_SENSOR, "failed: cci_client %pK",
				cci_client);
			return -EINVAL;
		}
		cci_client->cci_i2c_master = s_ctrl->cci_i2c_master;
		cci_client->sid = i2c_info->slave_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->i2c_freq_mode = i2c_info->i2c_freq_mode;
		CAM_DBG(CAM_SENSOR, " Master: %d sid: %d freq_mode: %d",
			cci_client->cci_i2c_master, i2c_info->slave_addr,
			i2c_info->i2c_freq_mode);
	}
#if 1
	else if (s_ctrl->io_master_info.master_type == I2C_MASTER) {
		struct i2c_client *client = s_ctrl->io_master_info.client;
		if (!client) {
			CAM_ERR(CAM_SENSOR, "failed: i2c client %pK",
				client);
			return -EINVAL;
		}
		client->addr = i2c_info->slave_addr;
		CAM_DBG(CAM_SENSOR, "slave addr 0x%x",
			client->addr);
	}
#endif

	s_ctrl->sensordata->slave_info.sensor_slave_addr =
		i2c_info->slave_addr;
	return rc;
}

int32_t cam_sensor_update_slave_info(struct cam_cmd_probe *probe_info,
	struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;

	s_ctrl->sensordata->slave_info.sensor_id_reg_addr =
		probe_info->reg_addr;
	s_ctrl->sensordata->slave_info.sensor_id =
		probe_info->expected_data;
	s_ctrl->sensordata->slave_info.sensor_id_mask =
		probe_info->data_mask;
	/* Userspace passes the pipeline delay in reserved field */
	s_ctrl->pipeline_delay =
		probe_info->reserved;

	s_ctrl->sensor_probe_addr_type =  probe_info->addr_type;
	s_ctrl->sensor_probe_data_type =  probe_info->data_type;
	CAM_DBG(CAM_SENSOR,
		"Sensor Addr: 0x%x sensor_id: 0x%x sensor_mask: 0x%x sensor_pipeline_delay:0x%x",
		s_ctrl->sensordata->slave_info.sensor_id_reg_addr,
		s_ctrl->sensordata->slave_info.sensor_id,
		s_ctrl->sensordata->slave_info.sensor_id_mask,
		s_ctrl->pipeline_delay);
	return rc;
}

int32_t cam_handle_cmd_buffers_for_probe(void *cmd_buf,
	struct cam_sensor_ctrl_t *s_ctrl,
	int32_t cmd_buf_num, uint32_t cmd_buf_length, size_t remain_len)
{
	int32_t rc = 0;

	switch (cmd_buf_num) {
	case 0: {
		struct cam_cmd_i2c_info *i2c_info = NULL;
		struct cam_cmd_probe *probe_info;

		if (remain_len <
			(sizeof(struct cam_cmd_i2c_info) +
			sizeof(struct cam_cmd_probe))) {
			CAM_ERR(CAM_SENSOR,
				"not enough buffer for cam_cmd_i2c_info");
			return -EINVAL;
		}
		i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
		rc = cam_sensor_update_i2c_info(i2c_info, s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed in Updating the i2c Info");
			return rc;
		}
		probe_info = (struct cam_cmd_probe *)
			(cmd_buf + sizeof(struct cam_cmd_i2c_info));
		rc = cam_sensor_update_slave_info(probe_info, s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Updating the slave Info");
			return rc;
		}
		cmd_buf = probe_info;
	}
		break;
	case 1: {
		rc = cam_sensor_update_power_settings(cmd_buf,
			cmd_buf_length, &s_ctrl->sensordata->power_info,
			remain_len);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed in updating power settings");
			return rc;
		}
	}
		break;
	default:
		CAM_ERR(CAM_SENSOR, "Invalid command buffer");
		break;
	}
	return rc;
}

int32_t cam_handle_mem_ptr(uint64_t handle, struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0, i;
	uint32_t *cmd_buf;
	void *ptr;
	size_t len;
	struct cam_packet *pkt = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	uintptr_t cmd_buf1 = 0;
	uintptr_t packet = 0;
	size_t    remain_len = 0;

	rc = cam_mem_get_cpu_buf(handle,
		&packet, &len);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Failed to get the command Buffer");
		return -EINVAL;
	}

	pkt = (struct cam_packet *)packet;
	if (pkt == NULL) {
		CAM_ERR(CAM_SENSOR, "packet pos is invalid");
		rc = -EINVAL;
		goto end;
	}

	if ((len < sizeof(struct cam_packet)) ||
		(pkt->cmd_buf_offset >= (len - sizeof(struct cam_packet)))) {
		CAM_ERR(CAM_SENSOR, "Not enough buf provided");
		rc = -EINVAL;
		goto end;
	}

	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *)&pkt->payload + pkt->cmd_buf_offset/4);
	if (cmd_desc == NULL) {
		CAM_ERR(CAM_SENSOR, "command descriptor pos is invalid");
		rc = -EINVAL;
		goto end;
	}
#if defined(CONFIG_SENSOR_RETENTION)
	if (pkt->num_cmd_buf > 3)
#else
	if (pkt->num_cmd_buf != 2)
#endif
	{
		CAM_ERR(CAM_SENSOR, "Expected More Command Buffers : %d",
			 pkt->num_cmd_buf);
		rc = -EINVAL;
		goto end;
	}

	for (i = 0; i < pkt->num_cmd_buf; i++) {
		if (!(cmd_desc[i].length))
			continue;
		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			&cmd_buf1, &len);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to parse the command Buffer Header");
			goto end;
		}
		if (cmd_desc[i].offset >= len) {
			CAM_ERR(CAM_SENSOR,
				"offset past length of buffer");
			rc = -EINVAL;
			goto end;
		}
		remain_len = len - cmd_desc[i].offset;
		if (cmd_desc[i].length > remain_len) {
			CAM_ERR(CAM_SENSOR,
				"Not enough buffer provided for cmd");
			rc = -EINVAL;
			goto end;
		}
		cmd_buf = (uint32_t *)cmd_buf1;
		cmd_buf += cmd_desc[i].offset/4;
		ptr = (void *) cmd_buf;

#if defined(CONFIG_SENSOR_RETENTION)
		if (i == 2) {
			struct i2c_settings_array *i2c_reg_settings = NULL;
			CAM_INFO(CAM_SENSOR,
				"[RET_DBG] Receive Init Setting for booting");

			i2c_reg_settings = &s_ctrl->i2c_data.init_settings;
			i2c_reg_settings->request_id = 0;
			i2c_reg_settings->is_settings_valid = 1;

			rc = cam_sensor_i2c_command_parser(&s_ctrl->io_master_info,
					i2c_reg_settings, &cmd_desc[i], 1, NULL);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "Fail parsing I2C Pkt: %d", rc);
				rc = 0;
			}
		}
		else
#endif
		rc = cam_handle_cmd_buffers_for_probe(ptr, s_ctrl,
			i, cmd_desc[i].length, remain_len);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to parse the command Buffer Header");
			goto end;
		}
	}

end:
	return rc;
}

void cam_sensor_query_cap(struct cam_sensor_ctrl_t *s_ctrl,
	struct  cam_sensor_query_cap *query_cap)
{
	query_cap->pos_roll = s_ctrl->sensordata->pos_roll;
	query_cap->pos_pitch = s_ctrl->sensordata->pos_pitch;
	query_cap->pos_yaw = s_ctrl->sensordata->pos_yaw;
	query_cap->secure_camera = 0;
	query_cap->actuator_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_ACTUATOR];
	query_cap->csiphy_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_CSIPHY];
	query_cap->eeprom_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_EEPROM];
	query_cap->flash_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_LED_FLASH];
	query_cap->ois_slot_id =
		s_ctrl->sensordata->subdev_id[SUB_MODULE_OIS];
	query_cap->slot_info =
		s_ctrl->soc_info.index;
}

static uint16_t cam_sensor_id_by_mask(struct cam_sensor_ctrl_t *s_ctrl,
	uint32_t chipid)
{
	uint16_t sensor_id = (uint16_t)(chipid & 0xFFFF);
	int16_t sensor_id_mask = s_ctrl->sensordata->slave_info.sensor_id_mask;

	if (!sensor_id_mask)
		sensor_id_mask = ~sensor_id_mask;

        CAM_ERR(CAM_SENSOR," Sensor id mask prev: %XX",sensor_id_mask);

	sensor_id &= sensor_id_mask;
	sensor_id_mask &= -sensor_id_mask;
	sensor_id_mask -= 1;

        CAM_ERR(CAM_SENSOR," Sensor id mask after operation: %XX",sensor_id_mask);

	while (sensor_id_mask) {
		sensor_id_mask >>= 1;
		sensor_id >>= 1;
	}

        CAM_ERR(CAM_SENSOR," Sensor id final: %XX",sensor_id);

	return sensor_id;
}

void cam_sensor_shutdown(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_sensor_power_ctrl_t *power_info =
		&s_ctrl->sensordata->power_info;
	int rc = 0;

#if defined(CONFIG_CAMERA_FRAME_CNT_DBG)
	cam_sensor_thread_destroy(s_ctrl);
#endif

	if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) &&
		(s_ctrl->is_probe_succeed == 0))
		return;

	cam_sensor_release_stream_rsc(s_ctrl);
	cam_sensor_release_per_frame_resource(s_ctrl);

	if (s_ctrl->sensor_state != CAM_SENSOR_INIT)
		cam_sensor_power_down(s_ctrl);

	if (s_ctrl->bridge_intf.device_hdl != -1) {
		rc = cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"dhdl already destroyed: rc = %d", rc);
	}

	s_ctrl->bridge_intf.device_hdl = -1;
	s_ctrl->bridge_intf.link_hdl = -1;
	s_ctrl->bridge_intf.session_hdl = -1;
	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	power_info->power_setting_size = 0;
	power_info->power_down_setting_size = 0;
	s_ctrl->streamon_count = 0;
	s_ctrl->streamoff_count = 0;
	s_ctrl->is_probe_succeed = 0;
	s_ctrl->last_flush_req = 0;
	s_ctrl->sensor_state = CAM_SENSOR_INIT;
}

int cam_sensor_match_id(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint32_t chipid = 0;
	struct cam_camera_slave_info *slave_info;

	slave_info = &(s_ctrl->sensordata->slave_info);

	if (!slave_info) {
		CAM_ERR(CAM_SENSOR, " failed: %pK",
			 slave_info);
		return -EINVAL;
	}

	rc = camera_io_dev_read(
		&(s_ctrl->io_master_info),
		slave_info->sensor_id_reg_addr,
		&chipid, CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD);
#if defined(CONFIG_SAMSUNG_SUPPORT_MULTI_MODULE)
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "i2c failed, rc = %d", rc);
		return rc;
	}
#endif

        CAM_ERR(CAM_SENSOR, " Read reg addr: %XX, chipid: %XX",
			slave_info->sensor_id_reg_addr,
			chipid);
	CAM_ERR(CAM_SENSOR, "read id: 0x%x expected id 0x%x:",
		chipid, slave_info->sensor_id);
#if defined(CONFIG_SEC_M44X_PROJECT)//TEMP_FIX
    if (s_ctrl->soc_info.index == SEC_WIDE_SENSOR && (cam_sensor_id_by_mask(s_ctrl, chipid) ==
		SENSOR_ID_S5KJN1 || cam_sensor_id_by_mask(s_ctrl, chipid) == SENSOR_ID_S5KJN1_1)) {
        return rc;
    }
#endif

	if (cam_sensor_id_by_mask(s_ctrl, chipid) != slave_info->sensor_id) {
		CAM_ERR(CAM_SENSOR, "read id: 0x%x expected id 0x%x:",
				chipid, slave_info->sensor_id);
		return -ENODEV;
	}
	return rc;
}

int32_t cam_sensor_driver_cmd(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg)
{
	int rc = 0, pkt_opcode = 0;
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_sensor_power_ctrl_t *power_info =
		&s_ctrl->sensordata->power_info;
	uint8_t retry_count = 0;
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	struct cam_hw_param *hw_param = NULL;
#endif
	int RETRY_CNT = 3;

	if (!s_ctrl || !arg) {
		CAM_ERR(CAM_SENSOR, "s_ctrl is NULL");
		return -EINVAL;
	}

	if (cmd->op_code != CAM_SENSOR_PROBE_CMD) {
		if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
			CAM_ERR(CAM_SENSOR, "Invalid handle type: %d",
				cmd->handle_type);
			return -EINVAL;
		}
	}

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	switch (cmd->op_code) {
	case CAM_SENSOR_PROBE_CMD: {
		if (s_ctrl->is_probe_succeed == 1) {
			CAM_ERR(CAM_SENSOR,
				"Already Sensor Probed in the slot");
			break;
		}
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
		sec_sensor_position = s_ctrl->id;
#endif

		if (cmd->handle_type ==
			CAM_HANDLE_MEM_HANDLE) {
			rc = cam_handle_mem_ptr(cmd->handle, s_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "Get Buffer Handle Failed");
				goto release_mutex;
			}
		} else {
			CAM_ERR(CAM_SENSOR, "Invalid Command Type: %d",
				 cmd->handle_type);
			rc = -EINVAL;
			goto release_mutex;
		}

		/* Parse and fill vreg params for powerup settings */
		rc = msm_camera_fill_vreg_params(
			&s_ctrl->soc_info,
			s_ctrl->sensordata->power_info.power_setting,
			s_ctrl->sensordata->power_info.power_setting_size);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Fail in filling vreg params for PUP rc %d",
				 rc);
			goto free_power_settings;
		}

		/* Parse and fill vreg params for powerdown settings*/
		rc = msm_camera_fill_vreg_params(
			&s_ctrl->soc_info,
			s_ctrl->sensordata->power_info.power_down_setting,
			s_ctrl->sensordata->power_info.power_down_setting_size);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Fail in filling vreg params for PDOWN rc %d",
				 rc);
			goto free_power_settings;
		}

		for(retry_count = 0; retry_count < RETRY_CNT; retry_count++) {
			/* Power up and probe sensor */
			rc = cam_sensor_power_up(s_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "power up failed");
				goto free_power_settings;
			}

			/* Match sensor ID */
			rc = cam_sensor_match_id(s_ctrl);
			if ((rc < 0) && (retry_count < (RETRY_CNT - 1)))  {
				CAM_WARN(CAM_SENSOR, "sensor match ID failed, trying again, retry count = %u", retry_count);
				cam_sensor_power_down(s_ctrl);
				msleep(20);
			}
			else
				break;
		}
#if defined(CONFIG_SENSOR_RETENTION)
		if (rc == 0)
			cam_sensor_write_normal_init(s_ctrl);
#endif

#if 0
		if (rc < 0) {
			cam_sensor_power_down(s_ctrl);
			msleep(20);
			goto free_power_settings;
		}
#endif
#if defined(CONFIG_SAMSUNG_SUPPORT_MULTI_MODULE)
		// Case 1. I2C fail(rc != -ENODEV)
		//		Any sensor tried to probe first will be probed.
		// Case 2. Match id fail(rc == -ENODEV)
		//		probe fail and try other sensor
		if (((rc == -ENODEV) || (rc == -ENOTCONN) || (rc == -EINVAL)) &&
#if defined(CONFIG_SEC_R9Q_PROJECT)
			(((s_ctrl->soc_info.index == SEC_WIDE_SENSOR) &&
			(s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_S5K2LD))  ||
			((s_ctrl->soc_info.index == SEC_ULTRA_WIDE_SENSOR) &&
			(s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_HI1336))))
#elif defined(CONFIG_SEC_Q2Q_PROJECT) || defined(CONFIG_SEC_V2Q_PROJECT)
			((s_ctrl->soc_info.index == SEC_TELE_SENSOR) &&
			(s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_HI1337)))
#else
			((s_ctrl->soc_info.index == SEC_WIDE_SENSOR) ||
			(s_ctrl->soc_info.index == SEC_TELE_SENSOR) ||
			(s_ctrl->soc_info.index == SEC_ULTRA_WIDE_SENSOR)))	
#endif
		{
			cam_sensor_power_down(s_ctrl);
			msleep(20);
			goto free_power_settings;
		}
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
		if (rc < 0) {
			CAM_ERR(CAM_UTIL, "[HWB]failed rc %d\n", rc);
			if (s_ctrl != NULL) {
				switch (s_ctrl->id) {
				case CAMERA_0:
					if (!msm_is_sec_get_rear_hw_param(&hw_param)) {
						if (hw_param != NULL) {
							CAM_ERR(CAM_UTIL, "[HWB][R][I2C] Err\n");
							hw_param->i2c_sensor_err_cnt++;
							hw_param->need_update_to_file = TRUE;
						}
					}
					break;

				case CAMERA_1:
				case CAMERA_12:
					if (!msm_is_sec_get_front_hw_param(&hw_param)) {
						if (hw_param != NULL) {
							CAM_ERR(CAM_UTIL, "[HWB][F][I2C] Err\n");
							hw_param->i2c_sensor_err_cnt++;
							hw_param->need_update_to_file = TRUE;
						}
					}
					break;

#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
				case CAMERA_11:
					if (!msm_is_sec_get_front3_hw_param(&hw_param)) {
						if (hw_param != NULL) {
							CAM_ERR(CAM_UTIL, "[HWB][F3][I2C] Err\n");
							hw_param->i2c_sensor_err_cnt++;
							hw_param->need_update_to_file = TRUE;
						}
					}
					break;
#else
				case CAMERA_11:
					if (!msm_is_sec_get_front2_hw_param(&hw_param)) {
						if (hw_param != NULL) {
							CAM_ERR(CAM_UTIL, "[HWB][F2][I2C] Err\n");
							hw_param->i2c_sensor_err_cnt++;
							hw_param->need_update_to_file = TRUE;
						}
					}
					break;
#endif
#endif

#if defined(CONFIG_SAMSUNG_REAR_DUAL) || defined(CONFIG_SAMSUNG_REAR_TRIPLE)
				case CAMERA_2:
					if (!msm_is_sec_get_rear2_hw_param(&hw_param)) {
						if (hw_param != NULL) {
							CAM_ERR(CAM_UTIL, "[HWB][R2][I2C] Err\n");
							hw_param->i2c_sensor_err_cnt++;
							hw_param->need_update_to_file = TRUE;
						}
					}
					break;
#endif

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
				case CAMERA_3:
					if (!msm_is_sec_get_rear3_hw_param(&hw_param)) {
						if (hw_param != NULL) {
							CAM_ERR(CAM_UTIL, "[HWB][R3][I2C] Err\n");
							hw_param->i2c_sensor_err_cnt++;
							hw_param->need_update_to_file = TRUE;
						}
					}
					break;
#endif

				default:
					CAM_DBG(CAM_UTIL, "[NON][I2C][%d] Unsupport\n", s_ctrl->id);
					break;
				}
			}
		}
#endif

		CAM_INFO(CAM_SENSOR,
			"Probe success,slot:%d,slave_addr:0x%x,sensor_id:0x%x",
			s_ctrl->soc_info.index,
			s_ctrl->sensordata->slave_info.sensor_slave_addr,
			s_ctrl->sensordata->slave_info.sensor_id);

		rc = cam_sensor_power_down(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "fail in Sensor Power Down");
			goto free_power_settings;
		}
		/*
		 * Set probe succeeded flag to 1 so that no other camera shall
		 * probed on this slot
		 */
		s_ctrl->is_probe_succeed = 1;
		s_ctrl->sensor_state = CAM_SENSOR_INIT;
	}
		break;
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev sensor_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		if ((s_ctrl->is_probe_succeed == 0) ||
			(s_ctrl->sensor_state != CAM_SENSOR_INIT)) {
			CAM_WARN(CAM_SENSOR,
				"Not in right state to aquire %d",
				s_ctrl->sensor_state);
			rc = -EINVAL;
			goto release_mutex;
		}

		if (s_ctrl->bridge_intf.device_hdl != -1) {
			CAM_ERR(CAM_SENSOR, "Device is already acquired");
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = copy_from_user(&sensor_acq_dev,
			u64_to_user_ptr(cmd->handle),
			sizeof(sensor_acq_dev));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed Copying from user");
			goto release_mutex;
		}

		bridge_params.session_hdl = sensor_acq_dev.session_handle;
		bridge_params.ops = &s_ctrl->bridge_intf.ops;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = s_ctrl;
		bridge_params.dev_id = CAM_SENSOR;

		sensor_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		s_ctrl->bridge_intf.device_hdl = sensor_acq_dev.device_handle;
		s_ctrl->bridge_intf.session_hdl = sensor_acq_dev.session_handle;

		CAM_DBG(CAM_SENSOR, "Device Handle: %d",
			sensor_acq_dev.device_handle);

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
		if (MAX_CAMERAS > s_ctrl->id) {
			sec_sensor_position = s_ctrl->id;
			CAM_ERR(CAM_UTIL, "[HWB]sensor_position: %d", sec_sensor_position);
		}
#endif

#if defined(CONFIG_CAMERA_SSM_I2C_ENV)
		if (s_ctrl->id == CAMERA_0)
		{
			g_s_ctrl_ssm = s_ctrl;
		}
#endif

		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&sensor_acq_dev,
			sizeof(struct cam_sensor_acquire_dev))) {
			CAM_ERR(CAM_SENSOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
#if 1
		for(retry_count = 0; retry_count < RETRY_CNT; retry_count++) {
			rc = cam_sensor_power_up(s_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "Sensor Power up failed");
				goto release_mutex;
			}

			/* Match sensor ID */
			rc = cam_sensor_match_id(s_ctrl);
			if ((rc < 0) && (retry_count < (RETRY_CNT - 1)))  {
				CAM_WARN(CAM_SENSOR, "sensor match ID failed, trying again, retry count = %u", retry_count);
				cam_sensor_power_down(s_ctrl);
				msleep(20);
			}
			else
				break;
		}

		if (rc < 0) {
			cam_sensor_power_down(s_ctrl);
			msleep(20);
			if (s_ctrl->bridge_intf.device_hdl != -1)
				cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
			s_ctrl->bridge_intf.device_hdl = -1;
			s_ctrl->bridge_intf.link_hdl = -1;
			s_ctrl->bridge_intf.session_hdl = -1;
			goto release_mutex;
		}
#endif

#if defined(CONFIG_CAMERA_CDR_TEST)
		if (cdr_value_exist) {
			cdr_start_ts	= ktime_get();
			cdr_start_ts = cdr_start_ts / 1000 / 1000;
		}
#endif
		s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;
		s_ctrl->last_flush_req = 0;
		CAM_INFO(CAM_SENSOR,
			"CAM_ACQUIRE_DEV Success, sensor_id:0x%x,sensor_slave_addr:0x%x",
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
	}
		break;
	case CAM_RELEASE_DEV: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_START)) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
			"Not in right state to release : %d",
			s_ctrl->sensor_state);
			goto release_mutex;
		}

		if (s_ctrl->bridge_intf.link_hdl != -1) {
			CAM_ERR(CAM_SENSOR,
				"Device [%d] still active on link 0x%x",
				s_ctrl->sensor_state,
				s_ctrl->bridge_intf.link_hdl);
			rc = -EAGAIN;
			goto release_mutex;
		}

		rc = cam_sensor_power_down(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Sensor Power Down failed");
			goto release_mutex;
		}

		cam_sensor_release_per_frame_resource(s_ctrl);
		cam_sensor_release_stream_rsc(s_ctrl);
		if (s_ctrl->bridge_intf.device_hdl == -1) {
			CAM_ERR(CAM_SENSOR,
				"Invalid Handles: link hdl: %d device hdl: %d",
				s_ctrl->bridge_intf.device_hdl,
				s_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"failed in destroying the device hdl");
		s_ctrl->bridge_intf.device_hdl = -1;
		s_ctrl->bridge_intf.link_hdl = -1;
		s_ctrl->bridge_intf.session_hdl = -1;

		s_ctrl->sensor_state = CAM_SENSOR_INIT;
		CAM_INFO(CAM_SENSOR,
			"CAM_RELEASE_DEV Success, sensor_id:0x%x,sensor_slave_addr:0x%x",
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
		s_ctrl->streamon_count = 0;
		s_ctrl->streamoff_count = 0;
		s_ctrl->last_flush_req = 0;
	}
		break;
	case CAM_QUERY_CAP: {
		struct  cam_sensor_query_cap sensor_cap;

		cam_sensor_query_cap(s_ctrl, &sensor_cap);
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&sensor_cap, sizeof(struct  cam_sensor_query_cap))) {
			CAM_ERR(CAM_SENSOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
		break;
	}
	case CAM_START_DEV: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_START)) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
			"Not in right state to start : %d",
			s_ctrl->sensor_state);
			goto release_mutex;
		}

#if defined(CONFIG_CAMERA_FRAME_CNT_DBG)
		// To print frame count,
		// echo 1 > /sys/module/cam_sensor_core/parameters/frame_cnt_dbg
		if (frame_cnt_dbg > 0)
		{
			rc = cam_sensor_thread_create(s_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "Failed create sensor thread");
				goto release_mutex;
			}
		}
#endif

		if (s_ctrl->i2c_data.streamon_settings.is_settings_valid &&
			(s_ctrl->i2c_data.streamon_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"cannot apply streamon settings");
#if defined(CONFIG_SEC_ABC)
                sec_abc_send_event("MODULE=camera@ERROR=i2c_fail");
#endif
				goto release_mutex;
			}
		}
		s_ctrl->sensor_state = CAM_SENSOR_START;
		CAM_INFO(CAM_SENSOR,
			"CAM_START_DEV Success, sensor_id:0x%x,sensor_slave_addr:0x%x",
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
	}
		break;
	case CAM_STOP_DEV: {
		if (s_ctrl->sensor_state != CAM_SENSOR_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
			"Not in right state to stop : %d",
			s_ctrl->sensor_state);
			goto release_mutex;
		}

#if defined(CONFIG_CAMERA_FRAME_CNT_DBG)
		cam_sensor_thread_destroy(s_ctrl);
#endif

		if (s_ctrl->i2c_data.streamoff_settings.is_settings_valid &&
			(s_ctrl->i2c_data.streamoff_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
				"cannot apply streamoff settings");
#if defined(CONFIG_SEC_ABC)
                sec_abc_send_event("MODULE=camera@ERROR=i2c_fail");
#endif
			}
		}

		cam_sensor_release_per_frame_resource(s_ctrl);
		s_ctrl->last_flush_req = 0;
		s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;
		CAM_INFO(CAM_SENSOR,
			"CAM_STOP_DEV Success, sensor_id:0x%x,sensor_slave_addr:0x%x",
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
	}
		break;
	case CAM_CONFIG_DEV: {
		rc = cam_sensor_i2c_pkt_parse(s_ctrl, arg);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed i2c pkt parse: %d", rc);
			goto release_mutex;
		}
		if (s_ctrl->i2c_data.init_settings.is_settings_valid &&
			(s_ctrl->i2c_data.init_settings.request_id == 0)) {

			pkt_opcode =
				CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG;
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				pkt_opcode);

			if ((rc == -EAGAIN) &&
			(s_ctrl->io_master_info.master_type == CCI_MASTER)) {
				/* If CCI hardware is resetting we need to wait
				 * for sometime before reapply
				 */
				CAM_WARN(CAM_SENSOR,
					"Reapplying the Init settings due to cci hw reset");
				usleep_range(1000, 1010);
				rc = cam_sensor_apply_settings(s_ctrl, 0,
					pkt_opcode);
			}
			s_ctrl->i2c_data.init_settings.request_id = -1;

			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"cannot apply init settings rc= %d",
					rc);
#if defined(CONFIG_SEC_ABC)
                sec_abc_send_event("MODULE=camera@ERROR=i2c_fail");
#endif
				delete_request(&s_ctrl->i2c_data.init_settings);
				goto release_mutex;
			}
			rc = delete_request(&s_ctrl->i2c_data.init_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Fail in deleting the Init settings");
				goto release_mutex;
			}
		}

		if (s_ctrl->i2c_data.config_settings.is_settings_valid &&
			(s_ctrl->i2c_data.config_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG);

			s_ctrl->i2c_data.config_settings.request_id = -1;

			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"cannot apply config settings");
#if defined(CONFIG_SEC_ABC)
                sec_abc_send_event("MODULE=camera@ERROR=i2c_fail");
#endif
				delete_request(
					&s_ctrl->i2c_data.config_settings);
				goto release_mutex;
			}
			rc = delete_request(&s_ctrl->i2c_data.config_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Fail in deleting the config settings");
				goto release_mutex;
			}
			s_ctrl->sensor_state = CAM_SENSOR_CONFIG;
		}

		if (s_ctrl->i2c_data.read_settings.is_settings_valid) {
			rc = cam_sensor_i2c_read_data(
				&s_ctrl->i2c_data.read_settings,
				&s_ctrl->io_master_info);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "cannot read data: %d", rc);
#if defined(CONFIG_SEC_ABC)
                sec_abc_send_event("MODULE=camera@ERROR=i2c_fail");
#endif
				delete_request(&s_ctrl->i2c_data.read_settings);
				goto release_mutex;
			}
			rc = delete_request(
				&s_ctrl->i2c_data.read_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Fail in deleting the read settings");
				goto release_mutex;
			}
		}
	}
		break;
	default:
		CAM_ERR(CAM_SENSOR, "Invalid Opcode: %d", cmd->op_code);
		rc = -EINVAL;
		goto release_mutex;
	}

release_mutex:
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;

free_power_settings:
	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	power_info->power_down_setting_size = 0;
	power_info->power_setting_size = 0;
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}

int cam_sensor_publish_dev_info(struct cam_req_mgr_device_info *info)
{
	int rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;

	if (!info)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(info->dev_hdl);

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	info->dev_id = CAM_REQ_MGR_DEVICE_SENSOR;
	strlcpy(info->name, CAM_SENSOR_NAME, sizeof(info->name));
	if (s_ctrl->pipeline_delay >= 1 && s_ctrl->pipeline_delay <= 3)
		info->p_delay = s_ctrl->pipeline_delay;
	else
		info->p_delay = 2;
	info->trigger = CAM_TRIGGER_POINT_SOF;

	return rc;
}

int cam_sensor_establish_link(struct cam_req_mgr_core_dev_link_setup *link)
{
	struct cam_sensor_ctrl_t *s_ctrl = NULL;

	if (!link)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(link->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	mutex_lock(&s_ctrl->cam_sensor_mutex);
	if (link->link_enable) {
		s_ctrl->bridge_intf.link_hdl = link->link_hdl;
		s_ctrl->bridge_intf.crm_cb = link->crm_cb;
	} else {
		s_ctrl->bridge_intf.link_hdl = -1;
		s_ctrl->bridge_intf.crm_cb = NULL;
	}
	mutex_unlock(&s_ctrl->cam_sensor_mutex);

	return 0;
}

int cam_sensor_power(struct v4l2_subdev *sd, int on)
{
	struct cam_sensor_ctrl_t *s_ctrl = v4l2_get_subdevdata(sd);

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	if (!on && s_ctrl->sensor_state == CAM_SENSOR_START) {
		cam_sensor_power_down(s_ctrl);
		s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;
	}
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));

	return 0;
}

#if IS_ENABLED(CONFIG_SEC_PM)
static DEFINE_MUTEX(mode_mutex);
static int countB1FPWM = 0;
static int countBB1FPWM = 0;
int cam_sensor_set_regulator_mode(struct cam_hw_soc_info *soc_info,
	unsigned int mode)
{
	int rc = 0;
	struct regulator *regulator;
	int *pCountFpwm = NULL;

	mutex_lock(&mode_mutex);
	if (soc_info->index == SEC_WIDE_SENSOR) {
		regulator = regulator_get(soc_info->dev, "cam_v_custom1");
		pCountFpwm = &countB1FPWM;
	} else if ((soc_info->index == SEC_FRONT_SENSOR) ||
			(soc_info->index == SEC_FRONT_FULL_SENSOR)) {
		regulator = regulator_get(soc_info->dev, "s2mpb02-bb");
		pCountFpwm = &countBB1FPWM;
	} else {
		CAM_ERR(CAM_SENSOR, "Sensor Index is wrong");
		rc = -EINVAL;
		goto release_mutex;

	}

	if (IS_ERR_OR_NULL(regulator)) {
		CAM_ERR(CAM_SENSOR, "regulator_get fail %d", rc);
		rc = PTR_ERR(regulator);
		rc = rc ? rc : -EINVAL;
		goto release_mutex;
	}

	if (mode == REGULATOR_MODE_FAST) {
		if ((*pCountFpwm) == 0)
			rc = regulator_set_mode(regulator, mode);
		(*pCountFpwm)++;
	} else if (mode == REGULATOR_MODE_NORMAL) {
		if ((*pCountFpwm) > 0) {
			(*pCountFpwm)--;
			if ((*pCountFpwm) == 0) {
				rc = regulator_set_mode(regulator, mode);
				usleep_range(1000, 2000);
			}
		}
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid regulator mode: %d", mode);
		rc = -EINVAL;
	}
	CAM_INFO(CAM_SENSOR, "countB1FPWM : %d countBB1FPWM : %d", countB1FPWM, countBB1FPWM);
	if (rc)
		CAM_ERR(CAM_SENSOR, "Failed to configure %d mode: %d", mode, rc);

	if (regulator != NULL)
		regulator_put(regulator);

release_mutex:
	mutex_unlock(&mode_mutex);
	return rc;
}
#endif

int cam_sensor_power_up(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc;
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_camera_slave_info *slave_info;
	struct cam_hw_soc_info *soc_info =
		&s_ctrl->soc_info;
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	struct cam_hw_param *hw_param = NULL;
#endif

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "failed: %pK", s_ctrl);
		return -EINVAL;
	}

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	if (s_ctrl != NULL) {
		switch (s_ctrl->id) {
			case CAMERA_0:
				if (!msm_is_sec_get_rear_hw_param(&hw_param)) {
					if (hw_param != NULL) {
						CAM_DBG(CAM_UTIL, "[R][INIT] Init\n");
						hw_param->i2c_chk = FALSE;
						hw_param->mipi_chk = FALSE;
						hw_param->need_update_to_file = FALSE;
						hw_param->comp_chk = FALSE;
					}
				}
				break;

			case CAMERA_1:
			case CAMERA_12:
				if (!msm_is_sec_get_front_hw_param(&hw_param)) {
					if (hw_param != NULL) {
						CAM_DBG(CAM_UTIL, "[F][INIT] Init\n");
						hw_param->i2c_chk = FALSE;
						hw_param->mipi_chk = FALSE;
						hw_param->need_update_to_file = FALSE;
						hw_param->comp_chk = FALSE;
					}
				}
				break;

#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
			case CAMERA_11:
				if (!msm_is_sec_get_front2_hw_param(&hw_param)) {
					if (hw_param != NULL) {
						CAM_DBG(CAM_UTIL, "[F2][INIT] Init\n");
						hw_param->i2c_chk = FALSE;
						hw_param->mipi_chk = FALSE;
						hw_param->need_update_to_file = FALSE;
						hw_param->comp_chk = FALSE;
					}
				}
				break;
#else
			case CAMERA_11:
				if (!msm_is_sec_get_front3_hw_param(&hw_param)) {
					if (hw_param != NULL) {
						CAM_DBG(CAM_UTIL, "[F3][INIT] Init\n");
						hw_param->i2c_chk = FALSE;
						hw_param->mipi_chk = FALSE;
						hw_param->need_update_to_file = FALSE;
						hw_param->comp_chk = FALSE;
					}
				}
				break;
#endif
#endif

#if defined(CONFIG_SAMSUNG_REAR_DUAL) || defined(CONFIG_SAMSUNG_REAR_TRIPLE)
			case CAMERA_2:
				if (!msm_is_sec_get_rear2_hw_param(&hw_param)) {
					if (hw_param != NULL) {
						CAM_DBG(CAM_UTIL, "[R2][INIT] Init\n");
						hw_param->i2c_chk = FALSE;
						hw_param->mipi_chk = FALSE;
						hw_param->need_update_to_file = FALSE;
						hw_param->comp_chk = FALSE;
					}
				}
				break;
#endif

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
			case CAMERA_3:
				if (!msm_is_sec_get_rear3_hw_param(&hw_param)) {
					if (hw_param != NULL) {
						CAM_DBG(CAM_UTIL, "[R3][INIT] Init\n");
						hw_param->i2c_chk = FALSE;
						hw_param->mipi_chk = FALSE;
						hw_param->need_update_to_file = FALSE;
						hw_param->comp_chk = FALSE;
					}
				}
				break;
#endif

			default:
				CAM_DBG(CAM_UTIL, "[NON][INIT][%d] Unsupport\n", s_ctrl->id);
				break;
		}
	}
#endif

	power_info = &s_ctrl->sensordata->power_info;
	slave_info = &(s_ctrl->sensordata->slave_info);

	if (!power_info || !slave_info) {
		CAM_ERR(CAM_SENSOR, "failed: %pK %pK", power_info, slave_info);
		return -EINVAL;
	}

	if (s_ctrl->bob_pwm_switch) {
		rc = cam_sensor_bob_pwm_mode_switch(soc_info,
			s_ctrl->bob_reg_index, true);
		if (rc) {
			CAM_WARN(CAM_SENSOR,
			"BoB PWM setup failed rc: %d", rc);
			rc = 0;
		}
	}

#if IS_ENABLED(CONFIG_SEC_PM) && (defined(CONFIG_SEC_P3Q_PROJECT) || defined(CONFIG_SEC_O3Q_PROJECT))
	if ((soc_info->index == SEC_WIDE_SENSOR) ||
		(soc_info->index == SEC_FRONT_SENSOR) ||
		(soc_info->index == SEC_FRONT_FULL_SENSOR))
		cam_sensor_set_regulator_mode(soc_info, REGULATOR_MODE_FAST);
#endif

	CAM_INFO(CAM_SENSOR, "sensor[%d] powerup",
		s_ctrl->soc_info.index);

	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "power up the core is failed:%d", rc);

#if IS_ENABLED(CONFIG_SEC_PM) && (defined(CONFIG_SEC_P3Q_PROJECT) || defined(CONFIG_SEC_O3Q_PROJECT))
		if ((soc_info->index == SEC_WIDE_SENSOR) ||
			(soc_info->index == SEC_FRONT_SENSOR) ||
			(soc_info->index == SEC_FRONT_FULL_SENSOR))
			cam_sensor_set_regulator_mode(soc_info, REGULATOR_MODE_NORMAL);
#endif
		return rc;
	}

	rc = camera_io_init(&(s_ctrl->io_master_info));
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "cci_init failed: rc: %d", rc);
		goto cci_failure;
	}

	CAM_INFO(CAM_SENSOR, "sensor[%d] cci init done",
		s_ctrl->soc_info.index);

	return rc;
cci_failure:
	if (cam_sensor_util_power_down(power_info, soc_info))
		CAM_ERR(CAM_SENSOR, "power down failure");

	return rc;
}

int cam_sensor_power_down(struct cam_sensor_ctrl_t *s_ctrl)
{
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_hw_soc_info *soc_info;
	int rc = 0;
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	struct cam_hw_param *hw_param = NULL;
#endif

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "failed: s_ctrl %pK", s_ctrl);
		return -EINVAL;
	}

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	if (s_ctrl != NULL) {
		switch (s_ctrl->id) {
			case CAMERA_0:
				if (!msm_is_sec_get_rear_hw_param(&hw_param)) {
					if (hw_param != NULL) {
						hw_param->i2c_chk = FALSE;
						hw_param->mipi_chk = FALSE;
						hw_param->comp_chk = FALSE;

						if (hw_param->need_update_to_file) {
						CAM_DBG(CAM_UTIL, "[R][DEINIT] Update\n");
						msm_is_sec_copy_err_cnt_to_file();
						}
						hw_param->need_update_to_file = FALSE;
					}
				}
				break;

			case CAMERA_1:
			case CAMERA_12:
				if (!msm_is_sec_get_front_hw_param(&hw_param)) {
					if (hw_param != NULL) {
						hw_param->i2c_chk = FALSE;
						hw_param->mipi_chk = FALSE;
						hw_param->comp_chk = FALSE;

						if (hw_param->need_update_to_file) {
							CAM_DBG(CAM_UTIL, "[F][DEINIT] Update\n");
							msm_is_sec_copy_err_cnt_to_file();
						}
							hw_param->need_update_to_file = FALSE;
					}
				}
				break;

#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
			case CAMERA_11:
				if (!msm_is_sec_get_front3_hw_param(&hw_param)) {
					if (hw_param != NULL) {
						hw_param->i2c_chk = FALSE;
						hw_param->mipi_chk = FALSE;
						hw_param->comp_chk = FALSE;

						if (hw_param->need_update_to_file) {
							CAM_DBG(CAM_UTIL, "[F3][DEINIT] Update\n");
							msm_is_sec_copy_err_cnt_to_file();
						}
						hw_param->need_update_to_file = FALSE;
					}
				}
				break;
#else
			case CAMERA_11:
				if (!msm_is_sec_get_front2_hw_param(&hw_param)) {
					if (hw_param != NULL) {
						hw_param->i2c_chk = FALSE;
						hw_param->mipi_chk = FALSE;
						hw_param->comp_chk = FALSE;

						if (hw_param->need_update_to_file) {
							CAM_DBG(CAM_UTIL, "[F2][DEINIT] Update\n");
							msm_is_sec_copy_err_cnt_to_file();
						}
						hw_param->need_update_to_file = FALSE;
					}
				}
				break;
#endif
#endif

#if defined(CONFIG_SAMSUNG_REAR_DUAL) || defined(CONFIG_SAMSUNG_REAR_TRIPLE)
			case CAMERA_2:
				if (!msm_is_sec_get_rear2_hw_param(&hw_param)) {
					if (hw_param != NULL) {
						hw_param->i2c_chk = FALSE;
						hw_param->mipi_chk = FALSE;
						hw_param->comp_chk = FALSE;

						if (hw_param->need_update_to_file) {
							CAM_DBG(CAM_UTIL, "[R2][DEINIT] Update\n");
							msm_is_sec_copy_err_cnt_to_file();
						}
						hw_param->need_update_to_file = FALSE;
					}
				}
				break;
#endif

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
			case CAMERA_3:
				if (!msm_is_sec_get_rear3_hw_param(&hw_param)) {
					if (hw_param != NULL) {
						hw_param->i2c_chk = FALSE;
						hw_param->mipi_chk = FALSE;
						hw_param->comp_chk = FALSE;

						if (hw_param->need_update_to_file) {
							CAM_DBG(CAM_UTIL, "[R3][DEINIT] Update\n");
							msm_is_sec_copy_err_cnt_to_file();
						}
						hw_param->need_update_to_file = FALSE;
					}
				}
				break;
#endif

			default:
				CAM_DBG(CAM_UTIL, "[NON][DEINIT][%d] Unsupport\n", s_ctrl->id);
				break;
		}
	}
#endif

	power_info = &s_ctrl->sensordata->power_info;
	soc_info = &s_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_SENSOR, "failed: power_info %pK", power_info);
		return -EINVAL;
	}

// Add 1000us delay to meet the power off specification iT3 (End of MIPI transfer to MCLK disable and I2C shutdown)	
#if defined(CONFIG_SEC_M44X_PROJECT)
    if ((soc_info->index == SEC_FRONT_SENSOR) || (soc_info->index == SEC_FRONT_FULL_SENSOR))
	{
		msleep(10);
	}	
#endif
	
	rc = cam_sensor_util_power_down(power_info, soc_info);

#if IS_ENABLED(CONFIG_SEC_PM) && (defined(CONFIG_SEC_P3Q_PROJECT) || defined(CONFIG_SEC_O3Q_PROJECT))
	if ((soc_info->index == SEC_WIDE_SENSOR) ||
		(soc_info->index == SEC_FRONT_SENSOR) ||
		(soc_info->index == SEC_FRONT_FULL_SENSOR))
		cam_sensor_set_regulator_mode(soc_info, REGULATOR_MODE_NORMAL);
#endif

#if defined(CONFIG_SENSOR_RETENTION)
	if (s_ctrl->sensordata->slave_info.sensor_id == RETENTION_SENSOR_ID) {
		if (sensor_retention_mode == RETENTION_READY_TO_ON) {
			sensor_retention_mode = RETENTION_ON;
		}
	}
#endif

	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "power down the core is failed:%d", rc);
		return rc;
	}

	if (s_ctrl->bob_pwm_switch) {
		rc = cam_sensor_bob_pwm_mode_switch(soc_info,
			s_ctrl->bob_reg_index, false);
		if (rc) {
			CAM_WARN(CAM_SENSOR,
				"BoB PWM setup failed rc: %d", rc);
			rc = 0;
		}
	}

	camera_io_release(&(s_ctrl->io_master_info));

	return rc;
}

int cam_sensor_apply_settings(struct cam_sensor_ctrl_t *s_ctrl,
	uint64_t req_id, enum cam_sensor_packet_opcodes opcode)
{
	int rc = 0, offset, i;
	uint64_t top = 0, del_req_id = 0;
	struct i2c_settings_array *i2c_set = NULL;
	struct i2c_settings_list *i2c_list;

	if (req_id == 0) {
		switch (opcode) {
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON: {
			i2c_set = &s_ctrl->i2c_data.streamon_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG: {
			CAM_INFO(CAM_SENSOR, "sensor[%d] init start",
				s_ctrl->soc_info.index);

			i2c_set = &s_ctrl->i2c_data.init_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG: {
			i2c_set = &s_ctrl->i2c_data.config_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF: {
			i2c_set = &s_ctrl->i2c_data.streamoff_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_FRAME_SKIP_UPDATE:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE:
		default:
			return 0;
		}
		if (i2c_set->is_settings_valid == 1) {
#if 1
			cam_sensor_pre_apply_settings(s_ctrl, opcode);
#if defined(CONFIG_SENSOR_RETENTION)
			if ((opcode == CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG) &&
				(cam_sensor_retention_calc_checksum(s_ctrl) >= 0)) {
				CAM_INFO(CAM_SENSOR, "[RET_DEB] Retention ON, Skip write init");
				return 0;
			}
#endif
#endif
			list_for_each_entry(i2c_list,
				&(i2c_set->list_head), list) {
				rc = cam_sensor_i2c_modes_util(
					&(s_ctrl->io_master_info),
					i2c_list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
						"Failed to apply settings: %d Sensor=0x%x",
						rc, s_ctrl->sensordata->slave_info.sensor_id);
					return rc;
				}
			}
#if 1
			cam_sensor_post_apply_settings(s_ctrl, opcode);
#endif
		}

		if (opcode == CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG) {
			CAM_INFO(CAM_SENSOR, "sensor[%d] init done",
				s_ctrl->soc_info.index);
		}
	} else if (req_id > 0) {
		offset = req_id % MAX_PER_FRAME_ARRAY;

		if (opcode == CAM_SENSOR_PACKET_OPCODE_SENSOR_FRAME_SKIP_UPDATE)
			i2c_set = s_ctrl->i2c_data.frame_skip;
		else
			i2c_set = s_ctrl->i2c_data.per_frame;

		if (i2c_set[offset].is_settings_valid == 1 &&
			i2c_set[offset].request_id == req_id) {
			list_for_each_entry(i2c_list,
				&(i2c_set[offset].list_head), list) {
				rc = cam_sensor_i2c_modes_util(
					&(s_ctrl->io_master_info),
					i2c_list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
						"Failed to apply settings: %d Sensor=0x%x",
						rc, s_ctrl->sensordata->slave_info.sensor_id);
					return rc;
				}
			}
			CAM_DBG(CAM_SENSOR, "applied req_id: %llu", req_id);
		} else {
			CAM_DBG(CAM_SENSOR,
				"Invalid/NOP request to apply: %lld", req_id);
		}

		/* Change the logic dynamically */
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			if ((req_id >=
				i2c_set[i].request_id) &&
				(top <
				i2c_set[i].request_id) &&
				(i2c_set[i].is_settings_valid
					== 1)) {
				del_req_id = top;
				top = i2c_set[i].request_id;
			}
		}

		if (top < req_id) {
			if ((((top % MAX_PER_FRAME_ARRAY) - (req_id %
				MAX_PER_FRAME_ARRAY)) >= BATCH_SIZE_MAX) ||
				(((top % MAX_PER_FRAME_ARRAY) - (req_id %
				MAX_PER_FRAME_ARRAY)) <= -BATCH_SIZE_MAX))
				del_req_id = req_id;
		}

		if (!del_req_id)
			return rc;

		CAM_DBG(CAM_SENSOR, "top: %llu, del_req_id:%llu",
			top, del_req_id);

		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			if ((del_req_id >
				 i2c_set[i].request_id) && (
				 i2c_set[i].is_settings_valid
					== 1)) {
				i2c_set[i].request_id = 0;
				rc = delete_request(
					&(i2c_set[i]));
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"Delete request Fail:%lld rc:%d",
						del_req_id, rc);
			}
		}
	}

	return rc;
}

int32_t cam_sensor_apply_request(struct cam_req_mgr_apply_request *apply)
{
	int32_t rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;
	enum cam_sensor_packet_opcodes opcode =
		CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE;

	if (!apply)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(apply->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	CAM_DBG(CAM_REQ, " Sensor[%d] update req id: %lld",
		s_ctrl->soc_info.index, apply->request_id);
	trace_cam_apply_req("Sensor", apply->request_id);
	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	rc = cam_sensor_apply_settings(s_ctrl, apply->request_id,
		opcode);
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}

int32_t cam_sensor_notify_frame_skip(struct cam_req_mgr_apply_request *apply)
{
	int32_t rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;
	enum cam_sensor_packet_opcodes opcode =
		CAM_SENSOR_PACKET_OPCODE_SENSOR_FRAME_SKIP_UPDATE;

	if (!apply)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(apply->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	CAM_DBG(CAM_REQ, " Sensor[%d] handle frame skip for req id: %lld",
		s_ctrl->soc_info.index, apply->request_id);
	trace_cam_notify_frame_skip("Sensor", apply->request_id);
	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	rc = cam_sensor_apply_settings(s_ctrl, apply->request_id,
		opcode);
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}

int32_t cam_sensor_flush_request(struct cam_req_mgr_flush_request *flush_req)
{
	int32_t rc = 0, i;
	uint32_t cancel_req_id_found = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;
	struct i2c_settings_array *i2c_set = NULL;

	if (!flush_req)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(flush_req->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	if ((s_ctrl->sensor_state != CAM_SENSOR_START) &&
		(s_ctrl->sensor_state != CAM_SENSOR_CONFIG)) {
		mutex_unlock(&(s_ctrl->cam_sensor_mutex));
		return rc;
	}

	if (s_ctrl->i2c_data.per_frame == NULL) {
		CAM_ERR(CAM_SENSOR, "i2c frame data is NULL");
		mutex_unlock(&(s_ctrl->cam_sensor_mutex));
		return -EINVAL;
	}

	if (s_ctrl->i2c_data.frame_skip == NULL) {
		CAM_ERR(CAM_SENSOR, "i2c not ready data is NULL");
		mutex_unlock(&(s_ctrl->cam_sensor_mutex));
		return -EINVAL;
	}

	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_ALL) {
		s_ctrl->last_flush_req = flush_req->req_id;
		CAM_DBG(CAM_SENSOR, "last reqest to flush is %lld",
			flush_req->req_id);
	}

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		i2c_set = &(s_ctrl->i2c_data.per_frame[i]);

		if ((flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ)
				&& (i2c_set->request_id != flush_req->req_id))
			continue;

		if (i2c_set->is_settings_valid == 1) {
			rc = delete_request(i2c_set);
			if (rc < 0)
				CAM_ERR(CAM_SENSOR,
					"delete request: %lld rc: %d",
					i2c_set->request_id, rc);

			if (flush_req->type ==
				CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
				cancel_req_id_found = 1;
				break;
			}
		}
	}

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		i2c_set = &(s_ctrl->i2c_data.frame_skip[i]);

		if ((flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ)
				&& (i2c_set->request_id != flush_req->req_id))
			continue;

		if (i2c_set->is_settings_valid == 1) {
			rc = delete_request(i2c_set);
			if (rc < 0)
				CAM_ERR(CAM_SENSOR,
					"delete request for not ready packet: %lld rc: %d",
					i2c_set->request_id, rc);

			if (flush_req->type ==
				CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
				cancel_req_id_found = 1;
				break;
			}
		}
	}

	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ &&
		!cancel_req_id_found)
		CAM_DBG(CAM_SENSOR,
			"Flush request id:%lld not found in the pending list",
			flush_req->req_id);
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
void msm_is_sec_init_all_cnt(void)
{
	CAM_INFO(CAM_UTIL, "All_Init_Cnt\n");
	memset(&cam_hwparam_collector, 0, sizeof(struct cam_hw_param_collector));
}

void msm_is_sec_init_err_cnt_file(struct cam_hw_param *hw_param)
{
	if (hw_param != NULL) {
		CAM_INFO(CAM_UTIL, "Init_Cnt\n");

		memset(hw_param, 0, sizeof(struct cam_hw_param));
		msm_is_sec_copy_err_cnt_to_file();
	} else {
		CAM_INFO(CAM_UTIL, "NULL\n");
	}
}

void msm_is_sec_dbg_check(void)
{
	CAM_INFO(CAM_UTIL, "Dbg E\n");
	CAM_INFO(CAM_UTIL, "Dbg X\n");
}

void msm_is_sec_copy_err_cnt_to_file(void)
{
#if defined(HWB_FILE_OPERATION)
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nwrite = 0;
	int old_mask = 0;

	CAM_INFO(CAM_UTIL, "To_F\n");

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	old_mask = sys_umask(0);

	fp = filp_open(CAM_HW_ERR_CNT_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0660);
	if (IS_ERR_OR_NULL(fp)) {
		CAM_ERR(CAM_UTIL, "[HWB][To_F] Err\n");
		sys_umask(old_mask);
		set_fs(old_fs);
		return;
	}

	nwrite = vfs_write(fp, (char *)&cam_hwparam_collector, sizeof(struct cam_hw_param_collector), &fp->f_pos);

	filp_close(fp, NULL);
	fp = NULL;
	sys_umask(old_mask);
	set_fs(old_fs);
#endif
}

void msm_is_sec_copy_err_cnt_from_file(void)
{
#if defined(HWB_FILE_OPERATION)
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nread = 0;
	int ret = 0;

	ret = msm_is_sec_file_exist(CAM_HW_ERR_CNT_FILE_PATH, HW_PARAMS_NOT_CREATED);
	if (ret == 1) {
		CAM_INFO(CAM_UTIL, "From_F\n");
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(CAM_HW_ERR_CNT_FILE_PATH, O_RDONLY, 0660);
		if (IS_ERR_OR_NULL(fp)) {
			CAM_ERR(CAM_UTIL, "[HWB][From_F] Err\n");
			set_fs(old_fs);
			return;
		}

		nread = vfs_read(fp, (char *)&cam_hwparam_collector, sizeof(struct cam_hw_param_collector), &fp->f_pos);

		filp_close(fp, NULL);
		fp = NULL;
		set_fs(old_fs);
	} else {
		CAM_INFO(CAM_UTIL, "NoEx_F\n");
	}
#endif
}

int msm_is_sec_file_exist(char *filename, hw_params_check_type chktype)
{
	int ret = 0;
#if defined(HWB_FILE_OPERATION)
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nwrite = 0;
	int old_mask = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (sys_access(filename, 0) == 0) {
		CAM_INFO(CAM_UTIL, "Ex_F\n");
		ret = 1;
	} else {
		switch (chktype) {
		case HW_PARAMS_CREATED:
			CAM_INFO(CAM_UTIL, "Ex_Cr\n");
			msm_is_sec_init_all_cnt();

			old_mask = sys_umask(0);

			fp = filp_open(CAM_HW_ERR_CNT_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0660);
			if (IS_ERR_OR_NULL(fp)) {
				CAM_ERR(CAM_UTIL, "[HWB][Ex_F] ERROR\n");
				ret = 0;
			} else {
				nwrite = vfs_write(fp, (char *)&cam_hwparam_collector, sizeof(struct cam_hw_param_collector), &fp->f_pos);

				filp_close(fp, current->files);
				fp = NULL;
				ret = 2;
			}
			sys_umask(old_mask);
			break;

		case HW_PARAMS_NOT_CREATED:
			CAM_INFO(CAM_UTIL, "Ex_NoCr\n");
			ret = 0;
			break;

		default:
			CAM_INFO(CAM_UTIL, "Ex_Err\n");
			ret = 0;
			break;
		}
	}

	set_fs(old_fs);
#endif

	return ret;
}

int msm_is_sec_get_sensor_position(uint32_t **cam_position)
{
	*cam_position = &sec_sensor_position;
	return 0;
}

int msm_is_sec_get_sensor_comp_mode(uint32_t **sensor_clk_size)
{
	*sensor_clk_size = &sec_sensor_clk_size;
	return 0;
}

int msm_is_sec_get_rear_hw_param(struct cam_hw_param **hw_param)
{
	*hw_param = &cam_hwparam_collector.rear_hwparam;
	return 0;
}

int msm_is_sec_get_front_hw_param(struct cam_hw_param **hw_param)
{
	*hw_param = &cam_hwparam_collector.front_hwparam;
	return 0;
}

int msm_is_sec_get_iris_hw_param(struct cam_hw_param **hw_param)
{
	*hw_param = &cam_hwparam_collector.iris_hwparam;
	return 0;
}

int msm_is_sec_get_rear2_hw_param(struct cam_hw_param **hw_param)
{
	*hw_param = &cam_hwparam_collector.rear2_hwparam;
	return 0;
}

int msm_is_sec_get_front2_hw_param(struct cam_hw_param **hw_param)
{
	*hw_param = &cam_hwparam_collector.front2_hwparam;
	return 0;
}

int msm_is_sec_get_front3_hw_param(struct cam_hw_param **hw_param)
{
	*hw_param = &cam_hwparam_collector.front3_hwparam;
	return 0;
}

int msm_is_sec_get_rear3_hw_param(struct cam_hw_param **hw_param)
{
	*hw_param = &cam_hwparam_collector.rear3_hwparam;
	return 0;
}
#endif

#if defined(CONFIG_CAMERA_SSM_I2C_ENV)
void cam_sensor_ssm_i2c_read(uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type)
{
	int rc = 0;

	if (g_s_ctrl_ssm)
	{
		rc = camera_io_dev_read(&g_s_ctrl_ssm->io_master_info, addr,
			data, addr_type, data_type);

		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "Failed to read 0x%x", addr);

		CAM_ERR(CAM_SENSOR, "[SSM_I2C] ssm_i2c_read, addr : 0x%x, data : 0x%x", addr, *data);
	}
	else
	{
		CAM_ERR(CAM_SENSOR, "ssm i2c is not ready!");
	}
}

void cam_sensor_ssm_i2c_write(uint32_t addr, uint32_t data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type)
{
	int rc = 0;

	struct cam_sensor_i2c_reg_setting  i2c_reg_settings;
	struct cam_sensor_i2c_reg_array    i2c_reg_array;

	CAM_INFO(CAM_SENSOR, "[SSM_I2C] ssm_i2c_write, addr : 0x%x, data : 0x%x", addr, data);

	if (g_s_ctrl_ssm)
	{
		i2c_reg_settings.addr_type = addr_type;
		i2c_reg_settings.data_type = data_type;
		i2c_reg_settings.size = 1;
		i2c_reg_settings.delay = 0;
		i2c_reg_array.reg_addr = addr;
		i2c_reg_array.reg_data = data;
		i2c_reg_array.delay = 0;
		i2c_reg_array.data_mask = 0x0;
		i2c_reg_settings.reg_setting = &i2c_reg_array;

		rc = camera_io_dev_write(&g_s_ctrl_ssm->io_master_info,
			&i2c_reg_settings);

		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "Failed to i2c write");

	}
	else
	{
		CAM_ERR(CAM_SENSOR, "ssm i2c is not ready!");
	}
}
#endif
