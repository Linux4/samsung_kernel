// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <cam_sensor_cmn_header.h>
#include "cam_sensor_core.h"
#include "cam_sensor_util.h"
#include "cam_soc_util.h"
#include "cam_trace.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"
#include "cam_req_mgr_dev.h"
#include "cam_hw_bigdata.h"

#define CAM_SENSOR_PIPELINE_DELAY_MASK        0xFF
#define CAM_SENSOR_MODESWITCH_DELAY_SHIFT     8
#define CAM_SENSOR_WAIT_STREAMON_TIMES   (20)   // 20 * 5 = 100 ms

#if defined(CONFIG_CAMERA_CDR_TEST)
#include "cam_clock_data_recovery.h"
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

#if defined(CONFIG_SAMSUNG_READ_BPC_FROM_OTP)
#include "cam_sensor_bpc.h"
#endif

#if defined(CONFIG_SENSOR_RETENTION)
#include "cam_sensor_retention.h"

static int disable_sensor_retention;
module_param(disable_sensor_retention, int, 0644);

static int check_stream_on = -1;
#endif

#if defined(CONFIG_SAMSUNG_DEBUG_SENSOR_I2C)
static int i2c_debug_cnt;
module_param(i2c_debug_cnt, int, 0644);
/*
adb shell "echo s5khp2 > /sys/module/camera/parameters/debug_sensor_name"
adb shell "echo 10 > /sys/module/camera/parameters/i2c_debug_cnt"
*/

extern int to_dump_when_sof_freeze__sen_id;

void cam_sensor_dbg_regdump(struct cam_sensor_ctrl_t* s_ctrl);
void cam_sensor_dbg_regdump_stream_on_fail(struct cam_sensor_ctrl_t* s_ctrl);
void cam_sensor_parse_reg(
	struct cam_sensor_ctrl_t* s_ctrl,
	struct i2c_settings_list* i2c_list,
	int32_t *debug_sen_id,
	e_sensor_reg_upd_event_type *sen_update_type);
void cam_sensor_dbg_print_vc(struct cam_sensor_ctrl_t* s_ctrl);
void cam_sensor_dbg_print_by_upd_type(struct cam_sensor_ctrl_t* , int32_t);
void cam_sensor_i2c_dump_util(
	struct cam_sensor_ctrl_t* s_ctrl,
	struct i2c_settings_list* i2c_list,
	int i2c_debug_cnt);
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
//#define HWB_FILE_OPERATION 1
uint32_t sec_sensor_position;
uint32_t sec_sensor_clk_size;
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
		case SENSOR_ID_IMX374:
		case SENSOR_ID_IMX754:
		case SENSOR_ID_S5K3K1:
		case SENSOR_ID_S5KGN3:
		case SENSOR_ID_S5K3LU:
		case SENSOR_ID_IMX564:
		case SENSOR_ID_S5KHP2:
		case SENSOR_ID_IMX258:
		case SENSOR_ID_IMX471:
		case SENSOR_ID_S5K2LD:
		case SENSOR_ID_S5K3J1:
			ret = 1;
 			break;
		default:
			ret =0;
			break;
	}

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
		CAM_DBG(CAM_SENSOR, "[AM_DBG] Write MIPI setting before Stream On setting. mipi_index : %d",
			s_ctrl->mipi_clock_index_new);

 		cur_mipi_sensor_mode = &(s_ctrl->mipi_info[0]);
		memset(&mipi_i2c_list, 0, sizeof(mipi_i2c_list));

		memcpy(&mipi_i2c_list.i2c_settings,
			cur_mipi_sensor_mode->mipi_setting[s_ctrl->mipi_clock_index_new].clk_setting,
			sizeof(struct cam_sensor_i2c_reg_setting));

		CAM_INFO(CAM_SENSOR, "[AM_DBG] Picked MIPI clock : %s",
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
	if (s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_HI847_HI1337)
		FRAME_COUNT_REG_ADDR = 0x0732;

	rc = camera_io_dev_read(&s_ctrl->io_master_info, FRAME_COUNT_REG_ADDR,
		frame_cnt, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE, false);
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

#if defined(CONFIG_SOF_FREEZE_FRAME_CNT_READ)
	if (s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_IMX374) {
		rc = gpio_get_value_cansleep(MIPI_SW_SEL_GPIO);
		CAM_INFO(CAM_SENSOR, "[%s]: mipi_sw_sel_gpio value = %d",s_ctrl->sensor_name, rc);
	}
#endif

	CAM_DBG(CAM_SENSOR, "E");

	do {
		rc = cam_sensor_read_frame_count(s_ctrl, &frame_cnt);
		if (rc < 0)
			break;

		if ((frame_cnt != 0xFF) &&	(frame_cnt > 0)) {
			CAM_INFO(CAM_SENSOR, "[CNT_DBG] sensor 0x%x : Stream on, Last frame_cnt 0x%x",
				s_ctrl->sensordata->slave_info.sensor_id, frame_cnt);
			return 0;
		}
		CAM_DBG(CAM_SENSOR, "[CNT_DBG] retry cnt : %d, Stream on, frame_cnt : 0x%x sensor: 0x%x", retry_cnt, frame_cnt,s_ctrl->sensordata->slave_info.sensor_id);
		retry_cnt--;
		usleep_range(5000, 6000);
	} while ((frame_cnt < 0x01 || frame_cnt == 0xFF) && (retry_cnt > 0));

	CAM_INFO(CAM_SENSOR, "[CNT_DBG] wait fail rc %d retry cnt : %d, frame_cnt : 0x%x sensor: 0x%x", rc, retry_cnt, frame_cnt,s_ctrl->sensordata->slave_info.sensor_id);
#if defined(CONFIG_SAMSUNG_DEBUG_SENSOR_I2C)
		cam_sensor_dbg_regdump_stream_on_fail(s_ctrl);
#endif

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
		if ((s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_HI847_HI1337) && ((frame_cnt & 0x01)  == 0x00)) {
			CAM_INFO(CAM_SENSOR, "[CNT_DBG] sensor 0x%x : Stream off, Last frame_cnt 0x%x",
				s_ctrl->sensordata->slave_info.sensor_id, frame_cnt);
			usleep_range(1000, 1010);
			return 0;
		}
		if (frame_cnt == 0xFF) {
			CAM_INFO(CAM_SENSOR, "[CNT_DBG] sensor 0x%x : Stream off, Last frame_cnt 0x%x",
				s_ctrl->sensordata->slave_info.sensor_id, frame_cnt);
			return 0;
		}
		CAM_DBG(CAM_SENSOR, "[CNT_DBG] retry cnt : %d, Stream off, frame_cnt : 0x%x sensor: 0x%x", retry_cnt, frame_cnt,s_ctrl->sensordata->slave_info.sensor_id);
		retry_cnt--;
		usleep_range(5000, 6000);
	} while ((frame_cnt != 0xFF) && (retry_cnt > 0));

	CAM_INFO(CAM_SENSOR, "[CNT_DBG] wait fail rc %d retry cnt : %d, frame_cnt : 0x%x sensor: 0x%x", rc, retry_cnt, frame_cnt,s_ctrl->sensordata->slave_info.sensor_id);

	CAM_DBG(CAM_SENSOR, "X");
	return -1;
}
#endif

#if defined(CONFIG_SAMSUNG_READ_BPC_FROM_OTP)
int cam_sensor_write_bpc_setting(
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
	if (reg_setting.reg_setting != NULL) {
		for (i = 0; i < settings_size; i++) {
			size = settings[i].size;
			memcpy(reg_setting.reg_setting,
				settings[i].reg_setting,
				sizeof(struct cam_sensor_i2c_reg_array) * size);
			reg_setting.size = size;
			reg_setting.addr_type = settings[i].addr_type;
			reg_setting.data_type = settings[i].data_type;
			reg_setting.delay = settings[i].delay;

			rc = camera_io_dev_write(io_master_info, &reg_setting);
			if (rc < 0)
				CAM_ERR(CAM_SENSOR,
					"Failed to random write I2C settings[%d]: %d", i, rc);
		}

		if (reg_setting.reg_setting) {
			kfree(reg_setting.reg_setting);
			reg_setting.reg_setting = NULL;
		}
	}
	else {
		CAM_ERR(CAM_SENSOR,"[BPC] out of memory");
	}

	return rc;
}

static int32_t cam_sensor_is_need_to_read_otp(struct cam_sensor_ctrl_t *s_ctrl, uint32_t * sensor_revision)
{
	char read_otp_reset[5] = "BEEF";
	uint32_t read_value = 0xBEEF;
	int32_t rc = 0;

	rc = camera_io_dev_read(&s_ctrl->io_master_info, SENSOR_REVISION_ADDR, &read_value,
		CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, false);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "Failed to read SENSOR_REVISION_ADDR");

	CAM_INFO(CAM_SENSOR, "[BPC] Revision read_value = 0x%x", read_value);
	*sensor_revision = read_value;

	if (read_value < S5KHP2_SENSOR_REVISION_EVT1) {
		CAM_INFO(CAM_SENSOR, "[BPC] Sensor revision is not EVT1 = 0x%x", read_value);
		return 0;
	}

	if (memcmp(otp_data, read_otp_reset, sizeof(read_otp_reset)) != 0) {
		CAM_INFO(CAM_SENSOR, "[BPC] Sensor is Same");
		return 0;
	}
	return 1;
}

static int32_t cam_sensor_updade_register_to_read_otp(struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;

	rc = cam_sensor_write_bpc_setting(&s_ctrl->io_master_info,
		bpc_sw_reset_settings, ARRAY_SIZE(bpc_sw_reset_settings));
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR," [BPC] Failed to write bpc_sw_reset rc = %d", rc);
		return rc;
	}

	rc = cam_sensor_write_bpc_setting(&s_ctrl->io_master_info,
		bpc_dram_init_setttings, ARRAY_SIZE(bpc_dram_init_setttings));
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "[BPC] Failed to write bpc_dram_init rc = %d", rc);
		return rc;
	}

	rc = cam_sensor_write_bpc_setting(&s_ctrl->io_master_info,
		bpc_configure_otp_addr_setttings, ARRAY_SIZE(bpc_configure_otp_addr_setttings));
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "[BPC] Failed to write bpc_configure_otp_addr rc = %d", rc);
		return rc;
	}

	rc = cam_sensor_write_bpc_setting(&s_ctrl->io_master_info,
		bpc_configure_dram_setttings, ARRAY_SIZE(bpc_configure_dram_setttings));
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "[BPC] Failed to write bpc_configure_dram rc = %d", rc);
		return rc;
	}

	rc = cam_sensor_write_bpc_setting(&s_ctrl->io_master_info,
		bpc_otp_read_settings, ARRAY_SIZE(bpc_otp_read_settings));
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "[BPC] Failed to write bpc_configure_dram rc = %d", rc);
		return rc;
	}
	return rc;
}

int cam_sensor_wait_otp_mode(struct cam_sensor_ctrl_t *s_ctrl)
{
	uint32_t read_value = 0xBEEF;
	uint8_t read_cnt = 0;
	int rc = -1;

	CAM_INFO(CAM_SENSOR, "[BPC] cam_sensor_wait_otp_mode");
	for (read_cnt = 0; read_cnt < SENSOR_BPC_READ_RETRY_CNT; read_cnt++) {

		rc = camera_io_dev_read(&s_ctrl->io_master_info, BPC_OTP_READ_STATUS_ADDR, &read_value,
			CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, false);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed to read BPC_OTP_READ_STATUS_ADDR");
		} else {
			if (read_value == BPC_OTP_READ_STATUS_OK) {
				CAM_INFO(CAM_SENSOR, "[BPC] cam_sensor_wait_otp_mode read_value = %d", read_value);
				return 0;
			} else {
				CAM_WARN(CAM_SENSOR, "[BPC] Fail cam_sensor_wait_otp_mode read_value : 0x%x retry : %d",read_value, read_cnt);
			}

			usleep_range(1000, 1100);
		}
	}

	if (read_cnt == SENSOR_BPC_READ_RETRY_CNT) {
		CAM_ERR(CAM_SENSOR, "[BPC] Fail read_value 0x%x", read_value);
		rc = -1;
	}

	return rc;
}

static int cam_bpc_match_crc()
{
	uint32_t calculated_crc = 0x0;
	uint32_t check_sum_crc = 0x0;
	uint32_t cur_otp = 0x0;
	uint32_t i = 0x0;
	CAM_INFO(CAM_SENSOR, "[BPC] cam_bpc_match_crc E");

	for (i = 0 ; i < (BPC_OTP_SIZE_MAX - 4) ; i += 4)
	{
		cur_otp = (otp_data[i] << 24) | (otp_data[i + 1] << 16) | (otp_data[i + 2] << 8) | (otp_data[i + 3]);
		if (i == 0)
		{
			calculated_crc = cur_otp;
		}
		else
		{
			calculated_crc = ~(calculated_crc ^ cur_otp);
		}
		if (cur_otp == BPC_OTP_TERMINATE_CODE_FOR_CRC)
		{
			CAM_INFO(CAM_SENSOR, "[BPC] Meet Terminate code for CRC index = %u cur_otp = 0x%x",i, cur_otp);
			break;
		}
	}

	check_sum_crc = (otp_data[BPC_OTP_SIZE_MAX - 4] << 24) |
					(otp_data[BPC_OTP_SIZE_MAX - 3] << 16) |
					(otp_data[BPC_OTP_SIZE_MAX - 2] << 8) |
					(otp_data[BPC_OTP_SIZE_MAX - 1]);

	CAM_INFO(CAM_SENSOR, "[BPC] previous_crc = 0x%x", calculated_crc);
	CAM_INFO(CAM_SENSOR, "[BPC] check_sum_crc = 0x%x", check_sum_crc);


	if (calculated_crc != check_sum_crc)
	{
		CAM_ERR(CAM_SENSOR, "[BPC] Crc mismatched = calculated crc 0x%x check_sum_crc 0x%x", calculated_crc, check_sum_crc);
		return -EINVAL;
	}

	CAM_INFO(CAM_SENSOR, "[BPC] cam_bpc_match_crc X");
	return 0;
}

int cam_sensor_read_bpc_from_otp(struct cam_sensor_ctrl_t *s_ctrl, uint32_t sensor_revision)
{
	int32_t rc = 0;
	uint32_t addr = 0, size = 0, read_size = 0;
	uint8_t *memptr;
	uint32_t otp_index = 0;
	char read_opt_reset[5] = "BEEF";

	memptr = otp_data;
	CAM_INFO(CAM_SENSOR, "[BPC] E sensor_revision = 0x%x", sensor_revision);

	rc = cam_sensor_updade_register_to_read_otp(s_ctrl);

	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "[BPC] Failed cam_sensor_updade_register_to_read_otp rc = %d", rc);
		return rc;
	}

	if (cam_sensor_wait_otp_mode(s_ctrl) == 0){
		rc = cam_sensor_write_bpc_setting(&s_ctrl->io_master_info,
			bpc_end_sequence_setttings, ARRAY_SIZE(bpc_end_sequence_setttings));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,	"[BPC] Failed to write bpc_configure_dram rc = %d", rc);
			return rc;
		}

		size = BPC_OTP_SIZE_MAX;
		addr = 0;

		while(size > 0) {
			read_size = size;
			if (size > PAGE_SIZE) {
				read_size = PAGE_SIZE;
			}

			rc = camera_io_dev_read_seq(&s_ctrl->io_master_info,
				addr, memptr,
				CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE,
				read_size);
			if (rc < 0) {
				CAM_ERR(CAM_EEPROM, "read failed rc %d",
					rc);
				return rc;
			}
			size -= read_size;
			addr += read_size;
			otp_index += PAGE_SIZE;
			memptr = otp_data + otp_index;
		}
	}
	else{
		CAM_ERR(CAM_SENSOR,	"[BPC] Failed to cam_sensor_wait_otp_mode rc = %d", rc);
	}

	if (sensor_revision >= S5KHP2_SENSOR_SUPPORT_BPC_CRC_SENSOR_REVISION)
	{
		if (cam_bpc_match_crc() < 0)
		{
			CAM_ERR(CAM_SENSOR, "[BPC] CRC error");
			// Need to enable after getting module that is writted CRC checksum
			memcpy(otp_data, read_opt_reset, sizeof(read_opt_reset));
		}
	}

	CAM_INFO(CAM_SENSOR, "[BPC] X");

	return rc;
}
#endif

#if defined(CONFIG_SENSOR_RETENTION)
uint8_t sensor_retention_mode = RETENTION_INIT;

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
	if (reg_setting.reg_setting != NULL) {
		for (i = 0; i < settings_size; i++) {
			size = settings[i].size;
			memcpy(reg_setting.reg_setting,
				settings[i].reg_setting,
				sizeof(struct cam_sensor_i2c_reg_array) * size);
			reg_setting.size = size;
			reg_setting.addr_type = settings[i].addr_type;
			reg_setting.data_type = settings[i].data_type;
			reg_setting.delay = settings[i].delay;


#if defined(CONFIG_SAMSUNG_DEBUG_SENSOR_I2C)
			if (i2c_debug_cnt > 0) {
				int32_t k;

				for (k = 0; k < reg_setting.size && k < i2c_debug_cnt; k++) {
					if (k == 0) {
						CAM_INFO(CAM_SENSOR,
							"[I2C_DBG] ====== size : %d ======",
							reg_setting.size);
					}
					CAM_INFO(CAM_SENSOR,
						"[I2C_DBG] [%d] addr : 0x%04X, data : 0x%04X", k,
						reg_setting.reg_setting[k].reg_addr,
						reg_setting.reg_setting[k].reg_data);
				}
			}
#endif

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
	}
	else {
		CAM_ERR(CAM_SENSOR,"[RET_DBG] out of memory");
	}

	return rc;
}

void cam_sensor_read_retention_status(struct cam_sensor_ctrl_t *s_ctrl, uint32_t expect_value)
{
	uint32_t read_value = 0xBEEF;
	int32_t rc = 0;

	rc = camera_io_dev_read(&s_ctrl->io_master_info, 0x6F12, &read_value,
		CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, false);

	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "read failed rc %d", rc);
	} else {
		if (read_value == expect_value) {
			CAM_INFO(CAM_SENSOR, "[RET_DBG] Pass retention status : 0x%x", expect_value);
		} else {
			CAM_WARN(CAM_SENSOR, "[RET_DBG] Fail retention status (0x%x != 0x%x)", expect_value, read_value);
		}
	}
}

#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)	|| defined(CONFIG_SEC_B5Q_PROJECT)
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

#if defined(CONFIG_SEC_B5Q_PROJECT)
		// 1. Wait - 10ms delay
		usleep_range(10000, 11000);

		// 2. Check result for checksum test - read addr: 0x100E
		rc = camera_io_dev_read(&s_ctrl->io_master_info, 0x100E, &read_value,
			CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, false);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "Failed to read");
#else
		usleep_range(15000, 15100);

		// 1. Check result for retention mode - read addr: 0x010E
		rc = camera_io_dev_read(&s_ctrl->io_master_info, 0x010E, &read_value,
			CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, false);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed to read");
		} else {
			if (read_value == 0x0000) {
				CAM_INFO(CAM_SENSOR, "[RET_DBG] Pass retention mode check");
				rc = 0;
			} else {
				CAM_WARN(CAM_SENSOR, "[RET_DBG] Fail retention mode check retry : %d", read_cnt);
				continue;
			}
		}

		// 2. Check result for checksum test - read addr: 0x19C2
		rc = camera_io_dev_read(&s_ctrl->io_master_info, 0x19C2, &read_value,
			CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, false);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "Failed to read");
#endif

		if (read_value == 0x0100) {
			CAM_INFO(CAM_SENSOR, "[RET_DBG] Pass checksum test");
			rc = 0;
			break;
		} else {
			CAM_ERR(CAM_SENSOR, "[RET_DBG] Fail checksum test (read_value = 0x%x, retry cnt : %d)", read_value, read_cnt);
		}
	}


	if ((read_cnt == SENSOR_RETENTION_READ_RETRY_CNT) && (read_value != 0x0100)) {
		CAM_ERR(CAM_SENSOR, "[RET_DBG] Fail checksum test! 0x%x", read_value);
		rc = -1;
	}

	return rc;
}
#elif defined(CONFIG_SEC_DM3Q_PROJECT)
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

		usleep_range(30000, 31000);

		// Read Checksum Register
		rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
			retention_checksum_settings, ARRAY_SIZE(retention_checksum_settings));
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to retention checksum setting rc = %d", rc);

		rc = camera_io_dev_read(&s_ctrl->io_master_info, 0x6F12, &read_value,
			CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, false);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed to read");
		} else {
			if (read_value == 0x0100) {
				CAM_INFO(CAM_SENSOR, "[RET_DBG] Pass checksum test");
				rc = 0;

				// HW Setting Init
				rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
					retention_hw_init_settings, ARRAY_SIZE(retention_hw_init_settings));
				if (rc < 0)
					CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to HW Setting Init setting rc = %d", rc);

				break;
			} else {
				CAM_WARN(CAM_SENSOR, "[RET_DBG] Fail checksum test (read_value = 0x%x, retry cnt : %d)", read_value, read_cnt);
			}
		}
	}

	if ((read_cnt == SENSOR_RETENTION_READ_RETRY_CNT) && (read_value != 0x01)) {
		CAM_ERR(CAM_SENSOR, "[RET_DBG] Fail checksum test! 0x%x", read_value);
		rc = -1;
	}

	return rc;
}
#endif

#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_DM3Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)\
	|| defined(CONFIG_SEC_B5Q_PROJECT)
void cam_sensor_write_prepare_retention(struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint32_t sensor_id = 0;

	sensor_id = s_ctrl->sensordata->slave_info.sensor_id;

	if(sensor_id != RETENTION_SENSOR_ID)
		return;

	CAM_INFO(CAM_SENSOR, "[RET_DBG] cam_sensor_write_prepare_retention");
	rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
		retention_prepare_settings, ARRAY_SIZE(retention_prepare_settings));
	if (rc < 0)
		CAM_ERR(CAM_SENSOR,	"[RET_DBG] Failed to retention prepare rc = %d", rc);
}

int cam_sensor_wait_retention_mode(struct cam_sensor_ctrl_t *s_ctrl)
{
	uint32_t read_value = 0xBEEF;
	uint8_t read_cnt = 0;
	int rc = -1;
	uint32_t sensor_id = 0;

	sensor_id = s_ctrl->sensordata->slave_info.sensor_id;

	if (sensor_id != RETENTION_SENSOR_ID)
		return 0;

	if (disable_sensor_retention > 0) {
		CAM_INFO(CAM_SENSOR, "[RET_DBG] retention disabled");
		return 0;
	}

	CAM_INFO(CAM_SENSOR, "[RET_DBG] cam_sensor_wait_retention_mode");
	for (read_cnt = 0; read_cnt < SENSOR_RETENTION_READ_RETRY_CNT; read_cnt++) {
		usleep_range(2000, 2100);

		// 1. Check result for retention mode wait - read addr: 0x19C4
		rc =camera_io_dev_read(&s_ctrl->io_master_info, 0x19C4, &read_value,
			CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, false);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Failed to read");
		} else {
			if (read_value == 0x0100) {
				CAM_INFO(CAM_SENSOR, "[RET_DBG] Pass wait retention mode");
				rc = 0;
				break;
			} else {
				CAM_ERR(CAM_SENSOR, "[RET_DBG] Fail wait retention mode (read_value = 0x%x, retry cnt : %d)", read_value, read_cnt);
			}
		}
	}

	if ((read_cnt == SENSOR_RETENTION_READ_RETRY_CNT) && (read_value != 0x0100)) {
		CAM_ERR(CAM_SENSOR, "[RET_DBG] Fail wait retention mode! 0x%x", read_value);
		rc = -1;
	}

	return rc;
}
#endif

int cam_sensor_write_normal_init(struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint32_t sensor_id = 0;

#if defined(CONFIG_SAMSUNG_READ_BPC_FROM_OTP)
	uint32_t sensor_revision = 0;
#endif

	sensor_id = s_ctrl->sensordata->slave_info.sensor_id;

	if(sensor_id != RETENTION_SENSOR_ID)
		return rc;

	if (disable_sensor_retention > 0) {
		CAM_INFO(CAM_SENSOR, "[RET_DBG] retention disabled");
		return rc;
	}

	if (sensor_retention_mode != RETENTION_INIT)
		return rc;

#if defined(CONFIG_SAMSUNG_READ_BPC_FROM_OTP)
	if (cam_sensor_is_need_to_read_otp(s_ctrl, &sensor_revision))
		cam_sensor_read_bpc_from_otp(s_ctrl, sensor_revision);
#endif

	CAM_INFO(CAM_SENSOR, "[RET_DBG] E");
#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_DM3Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)\
	|| defined(CONFIG_SEC_B5Q_PROJECT)
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

#if defined(CONFIG_SEC_B5Q_PROJECT)
		cam_sensor_write_prepare_retention(s_ctrl);
#endif

		rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
			stream_on_settings, ARRAY_SIZE(stream_on_settings));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"[RET_DBG] Failed to write stream on off init rc = %d", rc);
			goto end;
		}

#if defined(CONFIG_CAMERA_FRAME_CNT_CHECK)
		cam_sensor_wait_stream_on(s_ctrl, CAM_SENSOR_WAIT_STREAMON_TIMES);
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

#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
		rc = cam_sensor_wait_retention_mode(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"[RET_DBG] Failed to wait retention mode rc = %d", rc);
		}
#elif defined(CONFIG_SEC_DM3Q_PROJECT)
		CAM_INFO(CAM_SENSOR, "[RET_DBG] Check checksum ready");
		rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
			retention_ready_settings, ARRAY_SIZE(retention_ready_settings));
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to retention checksum setting rc = %d", rc);

		usleep_range(10000, 11000);

		cam_sensor_read_retention_status(s_ctrl, 0x0100);
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

#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
	CAM_INFO(CAM_SENSOR, "[RET_DBG] additional stream on for seamless mode change");
	cam_sensor_write_prepare_retention(s_ctrl);

	rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
		stream_on_settings, ARRAY_SIZE(stream_on_settings));
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to additional stream on for seamless mode change (rc = %d)", rc);

#if defined(CONFIG_CAMERA_FRAME_CNT_CHECK)
	cam_sensor_wait_stream_on(s_ctrl, CAM_SENSOR_WAIT_STREAMON_TIMES);
#endif
#endif

	CAM_INFO(CAM_SENSOR, "[RET_DBG] cam_sensor_write_enable_crc");
	rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
		retention_enable_settings, ARRAY_SIZE(retention_enable_settings));
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to retention enable rc = %d", rc);
}
#endif

#if defined(CONFIG_CAMERA_HYPERLAPSE_300X)
int cam_sensor_apply_hyperlapse_settings(
	struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
	struct cam_sensor_i2c_reg_array i2c_fll_reg_array[] = {
		{0x0104, 0x0101, 0, 0},
		{0x0702, 0x0000, 0, 0},
		{0x0704, 0x0000, 0, 0},
		{0x0340, 0x1AB4, 0, 0},
		{0x0202, 0x0D11, 0, 0},
		{0x0204, 0x0063, 0, 0},
		{0x020E, 0x0100, 0, 0},
		{0x0104, 0x0001, 0, 0},
	};

	struct cam_sensor_i2c_reg_array i2c_streamoff_reg_array[] = {
		{0x0B32, 0x0000, 0, 0},
		{0x0E00, 0x0003, 0, 0},
		{0x0100, 0x0003, 0, 0},
	};

	struct cam_sensor_i2c_reg_array i2c_streamon_reg_array[] = {
		{0x0B32, 0x0000, 0, 0},
		{0x0100, 0x0103, 0, 0},
	};

	if (s_ctrl->sensordata->slave_info.sensor_id != SENSOR_ID_S5KGN3)
	{
		return rc;
	}
#elif defined(CONFIG_SEC_DM3Q_PROJECT)
	struct cam_sensor_i2c_reg_array i2c_fll_reg_array[] = {
		{0x0104, 0x0101, 0, 0},
		{0x0702, 0x0000, 0, 0},
		{0x0704, 0x0000, 0, 0},
		{0x0340, 0x18D0, 0, 0},
		{0x0202, 0x0C26, 0, 0},
		{0x0204, 0x01AD, 0, 0},
		{0x020E, 0x0100, 0, 0},
		{0x0104, 0x0001, 0, 0},
	};

	struct cam_sensor_i2c_reg_array i2c_streamoff_reg_array[] = {
		{0xFCFC, 0x4000, 0, 0},
		{0x0E00, 0x0080, 0, 0},
		{0x0100, 0x0003, 0, 0},
	};

	struct cam_sensor_i2c_reg_array i2c_streamon_reg_array[] = {
		{0x6028, 0x1002, 0, 0},
		{0x602A, 0xCEB4, 0, 0},
		{0x6F12, 0x0000, 0, 0},
		{0x0100, 0x0103, 0, 0},
	};

	if (s_ctrl->sensordata->slave_info.sensor_id != SENSOR_ID_S5KHP2)
	{
		return rc;
	}
#elif defined(CONFIG_SEC_B5Q_PROJECT)
	struct cam_sensor_i2c_reg_array i2c_fll_reg_array[] = {
		{0x0104, 0x0101, 0, 0},
		{0x0702, 0x0000, 0, 0},
		{0x0704, 0x0000, 0, 0},
		{0x0340, 0x2F8E, 0, 0},
		{0x0202, 0x0D11, 0, 0},
		{0x0204, 0x0063, 0, 0},
		{0x020E, 0x0100, 0, 0},
		{0x0104, 0x0001, 0, 0},
	};

	struct cam_sensor_i2c_reg_array i2c_streamoff_reg_array[] = {
		{0x0E0A, 0x0000, 0, 0},
		{0x0100, 0x0000, 0, 0},
	};

	struct cam_sensor_i2c_reg_array i2c_streamon_reg_array[] = {
		{0x0100, 0x0100, 0, 0},
	};

	if (s_ctrl->sensordata->slave_info.sensor_id != SENSOR_ID_S5K2LD)
	{
		return rc;
	}
#endif

#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_DM3Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)\
	|| defined(CONFIG_SEC_B5Q_PROJECT)
	// SHOOTING_MODE_HYPERLAPSE = 16, SHOOTING_MODE_NIGHT = 8, SHOOTING_MODE_SUPER_NIGHT = 31
	if ((s_ctrl->camera_shooting_mode == 16) || (s_ctrl->camera_shooting_mode == 8) || (s_ctrl->camera_shooting_mode == 31))
	{
		struct cam_sensor_i2c_reg_setting reg_fllsetting;
		struct cam_sensor_i2c_reg_setting reg_streamoffsetting;
		struct cam_sensor_i2c_reg_setting reg_streamonsetting;
		int size = ARRAY_SIZE(i2c_fll_reg_array);

		CAM_INFO(CAM_SENSOR, "[RET_DBG] try hyperlapse streamoff setting");
		reg_fllsetting.reg_setting = kmalloc(sizeof(struct cam_sensor_i2c_reg_array) * size, GFP_KERNEL);
		if (reg_fllsetting.reg_setting != NULL) {
			reg_fllsetting.size = size;
			reg_fllsetting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
			reg_fllsetting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
			reg_fllsetting.delay = 0;
			memcpy(reg_fllsetting.reg_setting, &i2c_fll_reg_array, sizeof(struct cam_sensor_i2c_reg_array) * size);
			CAM_INFO(CAM_SENSOR, "[RET_DBG] fll size = %d", size);
		}

		size = ARRAY_SIZE(i2c_streamoff_reg_array);
		reg_streamoffsetting.reg_setting = kmalloc(sizeof(struct cam_sensor_i2c_reg_array) * size, GFP_KERNEL);
		if (reg_streamoffsetting.reg_setting != NULL) {
			reg_streamoffsetting.size = size;
			reg_streamoffsetting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
			reg_streamoffsetting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
			reg_streamoffsetting.delay = 0;
			memcpy(reg_streamoffsetting.reg_setting, &i2c_streamoff_reg_array, sizeof(struct cam_sensor_i2c_reg_array) * size);
			CAM_INFO(CAM_SENSOR, "[RET_DBG] streamoff size = %d", size);
		}

		size = ARRAY_SIZE(i2c_streamon_reg_array);
		reg_streamonsetting.reg_setting = kmalloc(sizeof(struct cam_sensor_i2c_reg_array) * size, GFP_KERNEL);
		if (reg_streamonsetting.reg_setting != NULL) {
			reg_streamonsetting.size = size;
			reg_streamonsetting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
			reg_streamonsetting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
			reg_streamonsetting.delay = 0;
			memcpy(reg_streamonsetting.reg_setting, &i2c_streamon_reg_array, sizeof(struct cam_sensor_i2c_reg_array) * size);
			CAM_INFO(CAM_SENSOR, "[RET_DBG] stream on size = %d", size);
		}

		rc = cam_sensor_wait_stream_on(s_ctrl, CAM_SENSOR_WAIT_STREAMON_TIMES);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "[RET_DBG] failed wait 1");

		rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_streamoffsetting);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to write streamoff settings %d", rc);

		rc = cam_sensor_wait_stream_off(s_ctrl);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "[RET_DBG] failed wait 2");

		rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_fllsetting);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to write fll settings %d", rc);

		rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_streamonsetting);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to write streamon settings %d", rc);

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
			rc = cam_sensor_wait_stream_on(s_ctrl, CAM_SENSOR_WAIT_STREAMON_TIMES);
#endif
#if defined(CONFIG_SENSOR_RETENTION)
			cam_sensor_write_enable_crc(s_ctrl);
#endif
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON: {
#if defined(CONFIG_SENSOR_RETENTION)
#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
			cam_sensor_write_prepare_retention(s_ctrl);
#endif
#endif
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
			rc = cam_sensor_apply_adaptive_mipi_settings(s_ctrl);
#endif
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG: {
#if defined(CONFIG_SENSOR_RETENTION)
#if defined(CONFIG_SEC_DM3Q_PROJECT)
			if (sensor_retention_mode != RETENTION_INIT) {
				cam_sensor_write_prepare_retention(s_ctrl);
			}
			break;
#endif
#endif
		}
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
#if defined(CONFIG_SENSOR_RETENTION) && defined(CONFIG_SEC_DM3Q_PROJECT)
	uint32_t sensor_id = s_ctrl->sensordata->slave_info.sensor_id;
#endif

	switch (opcode) {
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF: {
#if defined(CONFIG_CAMERA_FRAME_CNT_CHECK)
			cam_sensor_wait_stream_off(s_ctrl);
#endif
#if defined(CONFIG_SENSOR_RETENTION)
#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
			rc = cam_sensor_wait_retention_mode(s_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"[RET_DBG] Failed to wait retention mode rc = %d", rc);
			}
#elif defined(CONFIG_SEC_DM3Q_PROJECT)
			if (sensor_id == RETENTION_SENSOR_ID) {
				CAM_INFO(CAM_SENSOR, "[RET_DBG] Check retention ready");
				rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
					retention_ready_settings, ARRAY_SIZE(retention_ready_settings));
				if (rc < 0)
					CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to retention checksum setting rc = %d", rc);

				usleep_range(10000, 11000);

				cam_sensor_read_retention_status(s_ctrl, 0x0100);
			}
#endif
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

extern struct completion *cam_sensor_get_i3c_completion(uint32_t index);

static int cam_sensor_notify_v4l2_error_event(
	struct cam_sensor_ctrl_t *s_ctrl,
	uint32_t error_type, uint32_t error_code)
{
	int                        rc = 0;
	struct cam_req_mgr_message req_msg = {0};

	req_msg.session_hdl = s_ctrl->bridge_intf.session_hdl;
	req_msg.u.err_msg.device_hdl = s_ctrl->bridge_intf.device_hdl;
	req_msg.u.err_msg.link_hdl = s_ctrl->bridge_intf.link_hdl;
	req_msg.u.err_msg.error_type = error_type;
	req_msg.u.err_msg.request_id = s_ctrl->last_applied_req;
	req_msg.u.err_msg.resource_size = 0x0;
	req_msg.u.err_msg.error_code = error_code;

	CAM_DBG(CAM_SENSOR,
		"v4l2 error event [type: %u code: %u] for req: %llu on %s",
		error_type, error_code, s_ctrl->last_applied_req,
		s_ctrl->sensor_name);

	rc = cam_req_mgr_notify_message(&req_msg,
		V4L_EVENT_CAM_REQ_MGR_ERROR,
		V4L_EVENT_CAM_REQ_MGR_EVENT);
	if (rc)
		CAM_ERR(CAM_SENSOR,
			"Notifying v4l2 error [type: %u code: %u] failed for req id:%llu on %s",
			error_type, error_code, s_ctrl->last_applied_req,
			s_ctrl->sensor_name);

	return rc;
}

static int cam_sensor_update_req_mgr(
	struct cam_sensor_ctrl_t *s_ctrl,
	struct cam_packet *csl_packet)
{
	int rc = 0;
	struct cam_req_mgr_add_request add_req;

	memset(&add_req, 0, sizeof(add_req));
	add_req.link_hdl = s_ctrl->bridge_intf.link_hdl;
	add_req.req_id = csl_packet->header.request_id;
	CAM_DBG(CAM_SENSOR, " Rxed Req Id: %llu",
		csl_packet->header.request_id);
	add_req.dev_hdl = s_ctrl->bridge_intf.device_hdl;
	if (s_ctrl->bridge_intf.crm_cb &&
		s_ctrl->bridge_intf.crm_cb->add_req) {
		rc = s_ctrl->bridge_intf.crm_cb->add_req(&add_req);
		if (rc) {
			if (rc == -EBADR)
				CAM_INFO(CAM_SENSOR,
					"Adding request: %llu failed with request manager rc: %d, it has been flushed",
					csl_packet->header.request_id, rc);
			else
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
						"delete per frame setting for request: %lld rc: %d",
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
						"delete frame skip setting for request: %lld rc: %d",
						i2c_set->request_id, rc);
			}
		}
	}

	if (s_ctrl->i2c_data.bubble_update != NULL) {
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			i2c_set = &(s_ctrl->i2c_data.bubble_update[i]);
			if (i2c_set->is_settings_valid == 1) {
				i2c_set->is_settings_valid = -1;
				rc = delete_request(i2c_set);
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"delete bubble update setting for request: %lld rc: %d",
						i2c_set->request_id, rc);
			}
		}
	}
}

static int cam_sensor_handle_res_info(struct cam_sensor_res_info *res_info,
	struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint32_t idx = 0;

	if (!s_ctrl || !res_info) {
		CAM_ERR(CAM_SENSOR, "Invalid params: res_info: %s, s_ctrl: %s",
			CAM_IS_NULL_TO_STR(res_info),
			CAM_IS_NULL_TO_STR(s_ctrl));
		return -EINVAL;
	}

	idx = s_ctrl->last_updated_req % MAX_PER_FRAME_ARRAY;

	s_ctrl->sensor_res[idx].res_index = res_info->res_index;
	strscpy(s_ctrl->sensor_res[idx].caps, res_info->caps,
		sizeof(s_ctrl->sensor_res[idx].caps));
	s_ctrl->sensor_res[idx].width = res_info->width;
	s_ctrl->sensor_res[idx].height = res_info->height;
	s_ctrl->sensor_res[idx].fps = res_info->fps;

	if (res_info->num_valid_params > 0) {
		if (res_info->valid_param_mask & CAM_SENSOR_FEATURE_MASK)
			s_ctrl->sensor_res[idx].feature_mask =
				res_info->params[0];
	}

	s_ctrl->is_res_info_updated = true;

	/* If request id is 0, it will be during an initial config/acquire */
	CAM_DBG(CAM_SENSOR,
		"Sensor[%s-%d] Feature: 0x%x updated for request id: %lu, res index: %u, width: 0x%x, height: 0x%x, capability: %s, fps: %u",
		s_ctrl->sensor_name, s_ctrl->soc_info.index,
		s_ctrl->sensor_res[idx].feature_mask,
		s_ctrl->sensor_res[idx].request_id, s_ctrl->sensor_res[idx].res_index,
		s_ctrl->sensor_res[idx].width, s_ctrl->sensor_res[idx].height,
		s_ctrl->sensor_res[idx].caps, s_ctrl->sensor_res[idx].fps);

#if defined(CONFIG_CAMERA_HYPERLAPSE_300X)
	{
		static uint32_t old_shoot_md = 0;
		static uint16_t old_res_idx = 0;
		struct timespec64 curr_ts = { 0, };

		CAM_GET_BOOT_TIMESTAMP(curr_ts);

		s_ctrl->camera_shooting_mode = res_info->shooting_mode;
		if ((s_ctrl->camera_shooting_mode != old_shoot_md) ||
			(s_ctrl->sensor_res[idx].res_index != old_res_idx)) {
			CAM_INFO(CAM_SENSOR, "[SEN_DBG]%s SHOOTING_MODE_%s res_id(%u) %d*%d fps(%d) ts(%llu.%llu)",
				s_ctrl->sensor_name,
				res_info->shooting_mode_name,
				s_ctrl->sensor_res[idx].res_index,
				s_ctrl->sensor_res[idx].width, s_ctrl->sensor_res[idx].height,
				s_ctrl->sensor_res[idx].fps,
				curr_ts.tv_sec, curr_ts.tv_nsec / NSEC_PER_USEC);
			old_shoot_md = s_ctrl->camera_shooting_mode;
			old_res_idx = s_ctrl->sensor_res[idx].res_index;
		}
	}
#endif

	return rc;
}

static int32_t cam_sensor_generic_blob_handler(void *user_data,
	uint32_t blob_type, uint32_t blob_size, uint8_t *blob_data)
{
	int rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl =
		(struct cam_sensor_ctrl_t *) user_data;

	if (!blob_data || !blob_size) {
		CAM_ERR(CAM_SENSOR, "Invalid blob info %pK %u", blob_data,
			blob_size);
		return -EINVAL;
	}

	switch (blob_type) {
	case CAM_SENSOR_GENERIC_BLOB_RES_INFO: {
		struct cam_sensor_res_info *res_info =
			(struct cam_sensor_res_info *) blob_data;

		if (blob_size < sizeof(struct cam_sensor_res_info)) {
			CAM_ERR(CAM_SENSOR, "Invalid blob size expected: 0x%x actual: 0x%x",
				sizeof(struct cam_sensor_res_info), blob_size);
			return -EINVAL;
		}

		rc = cam_sensor_handle_res_info(res_info, s_ctrl);
		break;
	}
	default:
		CAM_WARN(CAM_SENSOR, "Invalid blob type %d", blob_type);
		break;
	}

	return rc;
}

static int32_t cam_sensor_pkt_parse(struct cam_sensor_ctrl_t *s_ctrl,
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
	int64_t prev_updated_req;
	uint32_t cmd_buf_type, idx;
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

	if ((csl_packet->header.op_code & 0xFFFFFF) !=
		CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG &&
		csl_packet->header.request_id <= s_ctrl->last_flush_req
		&& s_ctrl->last_flush_req != 0) {
		CAM_ERR(CAM_SENSOR,
			"reject request %lld, last request to flush %u",
			csl_packet->header.request_id, s_ctrl->last_flush_req);
		rc = -EBADR;
		goto end;
	}

	if (csl_packet->header.request_id > s_ctrl->last_flush_req)
		s_ctrl->last_flush_req = 0;

	prev_updated_req = s_ctrl->last_updated_req;
	s_ctrl->is_res_info_updated = false;

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
#if 1//defined(CONFIG_SAMSUNG_CAMERA_SENSOR_FLIP)
		{
			CAM_DBG(CAM_SENSOR,
				"Replace Streamon settings");
			i2c_reg_settings = &i2c_data->streamon_settings;
			if (i2c_reg_settings->is_settings_valid == 1)
			{
				i2c_reg_settings->is_settings_valid = -1;
				rc = delete_request(i2c_reg_settings);
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"failed while deleting Streamon settings");
			}
		}
#else
			goto end;
#endif

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
		CAM_DBG(CAM_SENSOR, "[AM_DBG] SENSOR_MODE : %d", csl_packet->header.request_id);
		s_ctrl->sensor_mode = csl_packet->header.request_id;
#endif
#if defined(CONFIG_SENSOR_RETENTION)
		if (s_ctrl->sensordata->slave_info.sensor_id == RETENTION_SENSOR_ID) {
			check_stream_on = 0;
		}
#endif
		goto end;
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
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_BUBBLE_UPDATE: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE)) {
			CAM_WARN(CAM_SENSOR,
				"Rxed Update packets without linking");
			goto end;
		}

		i2c_reg_settings =
			&i2c_data->bubble_update[csl_packet->header.request_id %
				MAX_PER_FRAME_ARRAY];
		CAM_DBG(CAM_SENSOR, "Received bubble update packet: %lld req: %lld",
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
		CAM_ERR(CAM_SENSOR, "Invalid Packet Header opcode: %d",
			csl_packet->header.op_code & 0xFFFFFF);
		rc = -EINVAL;
		goto end;
	}

	offset = (uint32_t *)&csl_packet->payload;
	offset += csl_packet->cmd_buf_offset / 4;
	cmd_desc = (struct cam_cmd_buf_desc *)(offset);
	cmd_buf_type = cmd_desc->meta_data;

	switch (cmd_buf_type) {
	case CAM_SENSOR_PACKET_I2C_COMMANDS:
		rc = cam_sensor_i2c_command_parser(&s_ctrl->io_master_info,
				i2c_reg_settings, cmd_desc, 1, io_cfg);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Fail parsing I2C Pkt: %d", rc);
			goto end;
		}
		break;
	case CAM_SENSOR_PACKET_GENERIC_BLOB:
		if ((csl_packet->header.op_code & 0xFFFFFF) !=
			CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG) {
			rc = -EINVAL;
			CAM_ERR(CAM_SENSOR, "Wrong packet opcode sent with blob: %u",
				csl_packet->header.op_code & 0xFFFFFF);
			goto end;
		}

		s_ctrl->last_updated_req = csl_packet->header.request_id;
		idx = s_ctrl->last_updated_req % MAX_PER_FRAME_ARRAY;
		s_ctrl->sensor_res[idx].request_id = csl_packet->header.request_id;

		/**
		 * is_settings_valid is set to false for this case, as generic
		 * blobs are meant to be used to send debugging information
		 * alongside actual configuration settings. As these are sent
		 * as separate packets at present, while sharing the same CONFIG
		 * opcode, setting this to false prevents sensor driver from
		 * applying non-existent configuration and changing s_ctrl
		 * state to CAM_SENSOR_CONFIG
		 */
		i2c_reg_settings->is_settings_valid = 0;

		rc = cam_packet_util_process_generic_cmd_buffer(cmd_desc,
			cam_sensor_generic_blob_handler, s_ctrl);
		if (rc)
			s_ctrl->sensor_res[idx].request_id = 0;

		break;
	}

	/*
	 * If no res info in current request, then we pick previous
	 * resolution info as current resolution info.
	 * Don't copy the sensor resolution info when the request id
	 * is invalid.
	 */
	if ((!s_ctrl->is_res_info_updated) && (csl_packet->header.request_id != 0)) {
		/*
		 * Update the last updated req at two places.
		 * 1# Got generic blob: The req id can be zero for the initial res info updating
		 * 2# Copy previous res info: The req id can't be zero, in case some queue info
		 * are override by slot0.
		 */
		s_ctrl->last_updated_req = csl_packet->header.request_id;
		s_ctrl->sensor_res[s_ctrl->last_updated_req % MAX_PER_FRAME_ARRAY] =
			s_ctrl->sensor_res[prev_updated_req % MAX_PER_FRAME_ARRAY];
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

	if ((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_SENSOR_PACKET_OPCODE_SENSOR_BUBBLE_UPDATE) {
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
			CAM_SENSOR_I2C_WRITE_SEQ);
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
			CAM_SENSOR_I2C_WRITE_BURST);
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

	s_ctrl->sensordata->slave_info.sensor_slave_addr =
		i2c_info->slave_addr;
	return rc;
}

int32_t cam_sensor_update_slave_info(void *probe_info,
	uint32_t cmd, struct cam_sensor_ctrl_t *s_ctrl, uint8_t probe_ver)
{
	int32_t rc = 0;
	struct cam_cmd_probe *sensor_probe_info;
	struct cam_cmd_probe_v2 *sensor_probe_info_v2;

	memset(s_ctrl->sensor_name, 0, CAM_SENSOR_NAME_MAX_SIZE);

	if (probe_ver == CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE) {
		sensor_probe_info = (struct cam_cmd_probe *)probe_info;
		s_ctrl->sensordata->slave_info.sensor_id_reg_addr =
			sensor_probe_info->reg_addr;
		s_ctrl->sensordata->slave_info.sensor_id =
			sensor_probe_info->expected_data;
		s_ctrl->sensordata->slave_info.sensor_id_mask =
			sensor_probe_info->data_mask;
		s_ctrl->pipeline_delay =
			sensor_probe_info->reserved;
		s_ctrl->modeswitch_delay = 0;

		s_ctrl->sensor_probe_addr_type = sensor_probe_info->addr_type;
		s_ctrl->sensor_probe_data_type = sensor_probe_info->data_type;
	} else if (probe_ver == CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE_V2) {
		sensor_probe_info_v2 = (struct cam_cmd_probe_v2 *)probe_info;
		s_ctrl->sensordata->slave_info.sensor_id_reg_addr =
			sensor_probe_info_v2->reg_addr;
		s_ctrl->sensordata->slave_info.sensor_id =
			sensor_probe_info_v2->expected_data;
		s_ctrl->sensordata->slave_info.sensor_id_mask =
			sensor_probe_info_v2->data_mask;
		s_ctrl->pipeline_delay =
			(sensor_probe_info_v2->pipeline_delay &
			CAM_SENSOR_PIPELINE_DELAY_MASK);
		s_ctrl->modeswitch_delay = (sensor_probe_info_v2->pipeline_delay >>
			CAM_SENSOR_MODESWITCH_DELAY_SHIFT);
		s_ctrl->sensor_probe_addr_type =
			sensor_probe_info_v2->addr_type;
		s_ctrl->sensor_probe_data_type =
			sensor_probe_info_v2->data_type;

		memcpy(s_ctrl->sensor_name, sensor_probe_info_v2->sensor_name,
			CAM_SENSOR_NAME_MAX_SIZE-1);
	}

	CAM_DBG(CAM_SENSOR,
		"%s Sensor Addr: 0x%x sensor_id: 0x%x sensor_mask: 0x%x sensor_pipeline_delay:0x%x",
		s_ctrl->sensor_name,
		s_ctrl->sensordata->slave_info.sensor_id_reg_addr,
		s_ctrl->sensordata->slave_info.sensor_id,
		s_ctrl->sensordata->slave_info.sensor_id_mask,
		s_ctrl->pipeline_delay);
	return rc;
}

int32_t cam_handle_cmd_buffers_for_probe(void *cmd_buf,
	struct cam_sensor_ctrl_t *s_ctrl,
	int32_t cmd_buf_num, uint32_t cmd,
	uint32_t cmd_buf_length, size_t remain_len,
	uint32_t probe_ver, struct cam_cmd_buf_desc *cmd_desc)
{
	int32_t rc = 0;
	size_t required_size = 0;

	switch (cmd_buf_num) {
	case 0: {
		struct cam_cmd_i2c_info *i2c_info = NULL;
		void *probe_info;

		if (probe_ver == CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE)
			required_size = sizeof(struct cam_cmd_i2c_info) +
				sizeof(struct cam_cmd_probe);
		else if(probe_ver == CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE_V2)
			required_size = sizeof(struct cam_cmd_i2c_info) +
				sizeof(struct cam_cmd_probe_v2);

		if (remain_len < required_size) {
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
		probe_info = cmd_buf + sizeof(struct cam_cmd_i2c_info);
		rc = cam_sensor_update_slave_info(probe_info, cmd, s_ctrl,
							probe_ver);
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

	case 2: {
		struct i2c_settings_array *i2c_reg_settings = NULL;
		struct i2c_data_settings *i2c_data = NULL;
		struct cam_buf_io_cfg *io_cfg = NULL;

		CAM_DBG(CAM_SENSOR, "reg_bank unlock settings");
		i2c_data = &(s_ctrl->i2c_data);
		i2c_reg_settings = &i2c_data->reg_bank_unlock_settings;
		i2c_reg_settings->request_id = 0;
		rc = cam_sensor_i2c_command_parser(&s_ctrl->io_master_info,
				i2c_reg_settings, cmd_desc, 1, io_cfg);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed in updating reg_bank unlock settings");
			return rc;
		}
	}
		break;
	case 3: {
		struct i2c_settings_array *i2c_reg_settings = NULL;
		struct i2c_data_settings *i2c_data = NULL;
		struct cam_buf_io_cfg *io_cfg = NULL;

		CAM_DBG(CAM_SENSOR, "reg_bank lock settings");
		i2c_data = &(s_ctrl->i2c_data);
		i2c_reg_settings = &i2c_data->reg_bank_lock_settings;
		i2c_reg_settings->request_id = 0;
		rc = cam_sensor_i2c_command_parser(&s_ctrl->io_master_info,
				i2c_reg_settings, cmd_desc, 1, io_cfg);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed in updating reg_bank lock settings");
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

int32_t cam_handle_mem_ptr(uint64_t handle, uint32_t cmd,
	struct cam_sensor_ctrl_t *s_ctrl)
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
	uint32_t probe_ver = 0;

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

	probe_ver = pkt->header.op_code & 0xFFFFFF;
	CAM_DBG(CAM_SENSOR, "Received Header opcode: %u", probe_ver);

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
			i, cmd, cmd_desc[i].length, remain_len, probe_ver, &cmd_desc[i]);

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

	sensor_id &= sensor_id_mask;
	sensor_id_mask &= -sensor_id_mask;
	sensor_id_mask -= 1;
	while (sensor_id_mask) {
		sensor_id_mask >>= 1;
		sensor_id >>= 1;
	}
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

	if(s_ctrl->sensor_state == CAM_SENSOR_START) {
		CAM_INFO(CAM_SENSOR,"STREAM OFF and RETENTION state check before Powerdown to keep sensor in right state");
#if defined(CONFIG_SAMSUNG_DEBUG_SENSOR_I2C)
		if (to_dump_when_sof_freeze__sen_id == s_ctrl->sensordata->slave_info.sensor_id) {
			cam_sensor_dbg_regdump(s_ctrl);
			to_dump_when_sof_freeze__sen_id = 0;
		}
#endif

// Stream OFF start
		if (s_ctrl->i2c_data.streamoff_settings.is_settings_valid &&
			(s_ctrl->i2c_data.streamoff_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
			CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
				"cannot apply streamoff settings for %s",
				s_ctrl->sensor_name);
			}

			CAM_INFO(CAM_SENSOR, "Applied stream off settings");
		}
//Stream OFF end
	}

#if defined(CONFIG_SENSOR_RETENTION)
	if ((s_ctrl->sensordata->slave_info.sensor_id == RETENTION_SENSOR_ID) &&
		(check_stream_on == 0) && (sensor_retention_mode != RETENTION_ON)) {
		CAM_INFO(CAM_SENSOR, "[RET_DBG] additional stream on/off for checksum start!");

#if defined(CONFIG_SEC_DM3Q_PROJECT) || defined(CONFIG_SEC_B5Q_PROJECT)
		rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
			stream_on_settings, ARRAY_SIZE(stream_on_settings));
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to retention additional stream on (rc = %d)", rc);

#if defined(CONFIG_CAMERA_FRAME_CNT_CHECK)
		cam_sensor_wait_stream_on(s_ctrl, CAM_SENSOR_WAIT_STREAMON_TIMES);
#endif
#endif
		cam_sensor_write_enable_crc(s_ctrl);

		rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
			stream_off_settings, ARRAY_SIZE(stream_off_settings));
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to retention additional stream off (rc = %d)", rc);

#if defined(CONFIG_CAMERA_FRAME_CNT_CHECK)
		cam_sensor_wait_stream_off(s_ctrl);
#endif
		CAM_INFO(CAM_SENSOR, "[RET_DBG] additional stream on/off for checksum end!");

#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)	|| defined(CONFIG_SEC_B5Q_PROJECT)
		rc = cam_sensor_wait_retention_mode(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"[RET_DBG] Failed to wait retention mode rc = %d", rc);
		}
#elif defined(CONFIG_SEC_DM3Q_PROJECT)
		CAM_INFO(CAM_SENSOR, "[RET_DBG] Check retention ready");
		rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
			retention_ready_settings, ARRAY_SIZE(retention_ready_settings));
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to retention checksum setting rc = %d", rc);

		usleep_range(10000, 11000);

		cam_sensor_read_retention_status(s_ctrl, 0x0100);
#endif
	}
#endif

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
		CAMERA_SENSOR_I2C_TYPE_WORD, true);

	CAM_INFO(CAM_SENSOR, "%s read id: 0x%x expected id 0x%x:",
		s_ctrl->sensor_name, chipid, slave_info->sensor_id);

	if (cam_sensor_id_by_mask(s_ctrl, chipid) != slave_info->sensor_id) {
		CAM_WARN(CAM_SENSOR, "%s read id: 0x%x expected id 0x%x:",
				s_ctrl->sensor_name, chipid,
				slave_info->sensor_id);
		return -ENODEV;
	}
	return rc;
}

int cam_sensor_stream_off(struct cam_sensor_ctrl_t *s_ctrl)
{
	int               rc = 0;
	struct timespec64 ts;
	uint64_t          ms, sec, min, hrs;

	if (s_ctrl->sensor_state != CAM_SENSOR_START) {
		rc = -EINVAL;
		CAM_WARN(CAM_SENSOR,
			"Not in right state to stop %s state: %d",
			s_ctrl->sensor_name, s_ctrl->sensor_state);
		goto end;
	}

	if (s_ctrl->i2c_data.streamoff_settings.is_settings_valid &&
		(s_ctrl->i2c_data.streamoff_settings.request_id == 0)) {
		rc = cam_sensor_apply_settings(s_ctrl, 0,
			CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"cannot apply streamoff settings for %s",
				s_ctrl->sensor_name);
	}

	cam_sensor_release_per_frame_resource(s_ctrl);
	s_ctrl->last_flush_req = 0;
	s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;
	memset(s_ctrl->sensor_res, 0, sizeof(s_ctrl->sensor_res));

	CAM_GET_TIMESTAMP(ts);
	CAM_CONVERT_TIMESTAMP_FORMAT(ts, hrs, min, sec, ms);

	CAM_INFO(CAM_SENSOR,
		"%llu:%llu:%llu.%llu CAM_STOP_DEV Success for %s sensor_id:0x%x,sensor_slave_addr:0x%x",
		hrs, min, sec, ms,
		s_ctrl->sensor_name,
		s_ctrl->sensordata->slave_info.sensor_id,
		s_ctrl->sensordata->slave_info.sensor_slave_addr);

end:
	return rc;
}


int32_t cam_sensor_driver_cmd(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg)
{
	int rc = 0, pkt_opcode = 0;
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_sensor_power_ctrl_t *power_info =
		&s_ctrl->sensordata->power_info;
	struct timespec64 ts;
	uint64_t ms, sec, min, hrs;

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

#if defined(CONFIG_SAMSUNG_DEBUG_SENSOR_I2C)
	s_ctrl->is_bubble_packet = false;
#endif

	mutex_lock(&(s_ctrl->cam_sensor_mutex));

	switch (cmd->op_code) {
	case CAM_SENSOR_PROBE_CMD: {
#if defined(CONFIG_SENSOR_RETENTION)
		if (s_ctrl->sensordata->slave_info.sensor_id == RETENTION_SENSOR_ID) {
			check_stream_on = -1;
		}
#endif
		if (s_ctrl->is_probe_succeed == 1) {
			CAM_WARN(CAM_SENSOR,
				"Sensor %s already Probed in the slot",
				s_ctrl->sensor_name);
			break;
		}

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
		sec_sensor_position = s_ctrl->id;
#endif

		if (cmd->handle_type ==
			CAM_HANDLE_MEM_HANDLE) {
			rc = cam_handle_mem_ptr(cmd->handle, cmd->op_code,
				s_ctrl);
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
				"Fail in filling vreg params for %s PUP rc %d",
				s_ctrl->sensor_name, rc);
			goto free_power_settings;
		}

		/* Parse and fill vreg params for powerdown settings*/
		rc = msm_camera_fill_vreg_params(
			&s_ctrl->soc_info,
			s_ctrl->sensordata->power_info.power_down_setting,
			s_ctrl->sensordata->power_info.power_down_setting_size);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Fail in filling vreg params for %s PDOWN rc %d",
				s_ctrl->sensor_name, rc);
			goto free_power_settings;
		}

		/* Power up and probe sensor */
		rc = cam_sensor_power_up(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Power up failed for %s sensor_id: 0x%x, slave_addr: 0x%x",
				s_ctrl->sensor_name,
				s_ctrl->sensordata->slave_info.sensor_id,
				s_ctrl->sensordata->slave_info.sensor_slave_addr
				);
			goto free_power_settings;
		}

		if (s_ctrl->i2c_data.reg_bank_unlock_settings.is_settings_valid) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_REG_BANK_UNLOCK);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "REG_bank unlock failed");
				cam_sensor_power_down(s_ctrl);
				goto free_power_settings;
			}
			rc = delete_request(&(s_ctrl->i2c_data.reg_bank_unlock_settings));
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"failed while deleting REG_bank unlock settings");
				cam_sensor_power_down(s_ctrl);
				goto free_power_settings;
			}
		}

		/* Match sensor ID */
		rc = cam_sensor_match_id(s_ctrl);

#if defined(CONFIG_SENSOR_RETENTION)
		if (rc >= 0)
			cam_sensor_write_normal_init(s_ctrl);

		if(s_ctrl->sensordata->slave_info.sensor_id == RETENTION_SENSOR_ID) {
			delete_request(&s_ctrl->i2c_data.init_settings);
		}
#endif

#if 0
		if (rc < 0) {
			CAM_INFO(CAM_SENSOR,
				"Probe failed for %s slot:%d, slave_addr:0x%x, sensor_id:0x%x",
				s_ctrl->sensor_name,
				s_ctrl->soc_info.index,
				s_ctrl->sensordata->slave_info.sensor_slave_addr,
				s_ctrl->sensordata->slave_info.sensor_id);
			cam_sensor_power_down(s_ctrl);
			msleep(20);
			goto free_power_settings;
		}
#endif

		if (s_ctrl->i2c_data.reg_bank_lock_settings.is_settings_valid) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_REG_BANK_LOCK);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "REG_bank lock failed");
				cam_sensor_power_down(s_ctrl);
				goto free_power_settings;
			}
			rc = delete_request(&(s_ctrl->i2c_data.reg_bank_lock_settings));
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"failed while deleting REG_bank lock settings");
				cam_sensor_power_down(s_ctrl);
				goto free_power_settings;
			}
		}

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
		if (rc < 0) {
			CAM_ERR(CAM_UTIL, "[HWB]failed rc %d\n", rc);
			if (s_ctrl != NULL) {
				hw_bigdata_i2c_from_sensor(s_ctrl);
			}
		}
#endif

		rc = cam_sensor_power_down(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Fail in %s sensor Power Down",
				s_ctrl->sensor_name);
			goto free_power_settings;
		}

		/*
		 * Set probe succeeded flag to 1 so that no other camera shall
		 * probed on this slot
		 */
		s_ctrl->is_probe_succeed = 1;
		s_ctrl->sensor_state = CAM_SENSOR_INIT;

		CAM_INFO(CAM_SENSOR,
				"Probe success for %s slot:%d,slave_addr:0x%x,sensor_id:0x%x",
				s_ctrl->sensor_name,
				s_ctrl->soc_info.index,
				s_ctrl->sensordata->slave_info.sensor_slave_addr,
				s_ctrl->sensordata->slave_info.sensor_id);

	}
		break;
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev sensor_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		if ((s_ctrl->is_probe_succeed == 0) ||
			(s_ctrl->sensor_state != CAM_SENSOR_INIT)) {
			CAM_WARN(CAM_SENSOR,
				"Not in right state to aquire %s state: %d",
				s_ctrl->sensor_name, s_ctrl->sensor_state);
			rc = -EINVAL;
			goto release_mutex;
		}

#if defined(CONFIG_SENSOR_RETENTION)
		if (s_ctrl->sensordata->slave_info.sensor_id == RETENTION_SENSOR_ID) {
			check_stream_on = 0;
		}
#endif

		if (s_ctrl->bridge_intf.device_hdl != -1) {
			CAM_ERR(CAM_SENSOR,
				"%s Device is already acquired",
				s_ctrl->sensor_name);
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
		if (sensor_acq_dev.device_handle <= 0) {
			rc = -EFAULT;
			CAM_ERR(CAM_SENSOR, "Can not create device handle");
			goto release_mutex;
		}
		s_ctrl->bridge_intf.device_hdl = sensor_acq_dev.device_handle;
		s_ctrl->bridge_intf.session_hdl = sensor_acq_dev.session_handle;

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
		if (MAX_CAMERAS > s_ctrl->id) {
			sec_sensor_position = s_ctrl->id;
			CAM_INFO(CAM_UTIL, "[HWB]sensor_position: %d", sec_sensor_position);
		}
#endif

		CAM_DBG(CAM_SENSOR, "%s Device Handle: %d",
			s_ctrl->sensor_name, sensor_acq_dev.device_handle);
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&sensor_acq_dev,
			sizeof(struct cam_sensor_acquire_dev))) {
			CAM_ERR(CAM_SENSOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}

		rc = cam_sensor_power_up(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Sensor Power up failed for %s sensor_id:0x%x, slave_addr:0x%x",
				s_ctrl->sensor_name,
				s_ctrl->sensordata->slave_info.sensor_id,
				s_ctrl->sensordata->slave_info.sensor_slave_addr
				);
			goto release_mutex;
		}
#if defined(CONFIG_CAMERA_CDR_TEST)
		if (cam_clock_data_recovery_is_requested()) {
			cam_clock_data_recovery_get_timestamp(CDR_START_TS);
		}
#endif
		s_ctrl->sensor_state = CAM_SENSOR_ACQUIRE;
		s_ctrl->last_flush_req = 0;
		s_ctrl->is_stopped_by_user = false;
		s_ctrl->last_updated_req = 0;
		s_ctrl->last_applied_req = 0;
		memset(s_ctrl->sensor_res, 0, sizeof(s_ctrl->sensor_res));
		CAM_INFO(CAM_SENSOR,
			"CAM_ACQUIRE_DEV Success for %s sensor_id:0x%x,sensor_slave_addr:0x%x",
			s_ctrl->sensor_name,
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
	}
		break;
	case CAM_RELEASE_DEV: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_START)) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
				"Not in right state to release %s state: %d",
				s_ctrl->sensor_name, s_ctrl->sensor_state);
			goto release_mutex;
		}

		if (s_ctrl->bridge_intf.link_hdl != -1) {
			CAM_ERR(CAM_SENSOR,
				"%s Device [%d] still active on link 0x%x",
				s_ctrl->sensor_name,
				s_ctrl->sensor_state,
				s_ctrl->bridge_intf.link_hdl);
			rc = -EAGAIN;
			goto release_mutex;
		}

		hw_bigdata_debug_info();

#if defined(CONFIG_SENSOR_RETENTION)
		if (s_ctrl->sensordata->slave_info.sensor_id == RETENTION_SENSOR_ID &&
			check_stream_on == 0) {
			CAM_INFO(CAM_SENSOR, "[RET_DBG] additional stream on/off for checksum start!");

#if defined(CONFIG_SEC_DM3Q_PROJECT) || defined(CONFIG_SEC_B5Q_PROJECT)
			rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
				stream_on_settings, ARRAY_SIZE(stream_on_settings));
			if (rc < 0)
				CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to retention additional stream on (rc = %d)", rc);

#if defined(CONFIG_CAMERA_FRAME_CNT_CHECK)
			cam_sensor_wait_stream_on(s_ctrl, CAM_SENSOR_WAIT_STREAMON_TIMES);
#endif
#endif
			cam_sensor_write_enable_crc(s_ctrl);

			rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
				stream_off_settings, ARRAY_SIZE(stream_off_settings));
			if (rc < 0)
				CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to retention additional stream off (rc = %d)", rc);

#if defined(CONFIG_CAMERA_FRAME_CNT_CHECK)
			cam_sensor_wait_stream_off(s_ctrl);
#endif
			CAM_INFO(CAM_SENSOR, "[RET_DBG] additional stream on/off for checksum end!");

#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
			rc = cam_sensor_wait_retention_mode(s_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"[RET_DBG] Failed to wait retention mode rc = %d", rc);
			}
#elif defined(CONFIG_SEC_DM3Q_PROJECT)
			CAM_INFO(CAM_SENSOR, "[RET_DBG] Check retention ready");
			rc = cam_sensor_write_retention_setting(&s_ctrl->io_master_info,
				retention_ready_settings, ARRAY_SIZE(retention_ready_settings));
			if (rc < 0)
				CAM_ERR(CAM_SENSOR, "[RET_DBG] Failed to retention checksum setting rc = %d", rc);

			usleep_range(10000, 11000);

			cam_sensor_read_retention_status(s_ctrl, 0x0100);
#endif
		}
#endif

		rc = cam_sensor_power_down(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Sensor Power Down failed for %s sensor_id: 0x%x, slave_addr:0x%x",
				s_ctrl->sensor_name,
				s_ctrl->sensordata->slave_info.sensor_id,
				s_ctrl->sensordata->slave_info.sensor_slave_addr
				);
			goto release_mutex;
		}

		cam_sensor_release_per_frame_resource(s_ctrl);
		cam_sensor_release_stream_rsc(s_ctrl);
		if (s_ctrl->bridge_intf.device_hdl == -1) {
			CAM_ERR(CAM_SENSOR,
				"Invalid Handles: %s link hdl: %d device hdl: %d",
				s_ctrl->sensor_name,
				s_ctrl->bridge_intf.device_hdl,
				s_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR,
				"Failed in destroying %s device hdl",
				s_ctrl->sensor_name);
		s_ctrl->bridge_intf.device_hdl = -1;
		s_ctrl->bridge_intf.link_hdl = -1;
		s_ctrl->bridge_intf.session_hdl = -1;

		s_ctrl->sensor_state = CAM_SENSOR_INIT;
		CAM_INFO(CAM_SENSOR,
			"CAM_RELEASE_DEV Success for %s sensor_id:0x%x, slave_addr:0x%x",
			s_ctrl->sensor_name,
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
		s_ctrl->streamon_count = 0;
		s_ctrl->streamoff_count = 0;
		s_ctrl->last_flush_req = 0;
	}
		break;
	case CAM_QUERY_CAP: {
		struct  cam_sensor_query_cap sensor_cap;

		CAM_DBG(CAM_SENSOR, "%s Sensor Queried", s_ctrl->sensor_name);
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
		struct cam_req_mgr_timer_notify timer;
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_START)) {
			rc = -EINVAL;
			CAM_WARN(CAM_SENSOR,
			"Not in right state to start %s state: %d",
			s_ctrl->sensor_name,
			s_ctrl->sensor_state);
			goto release_mutex;
		}

#if defined(CONFIG_SENSOR_RETENTION)
		if (s_ctrl->sensordata->slave_info.sensor_id == RETENTION_SENSOR_ID) {
			check_stream_on = 1;
		}
#endif

#if defined(CONFIG_CAMERA_FRAME_CNT_DBG)
		// To print frame count,
		// echo 1 > /sys/module/camera/parameters/frame_cnt_dbg
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
					"cannot apply streamon settings for %s",
					s_ctrl->sensor_name);
				goto release_mutex;
			}
		}
		s_ctrl->sensor_state = CAM_SENSOR_START;

		if (s_ctrl->stream_off_after_eof)
			s_ctrl->is_stopped_by_user = false;

		if (s_ctrl->bridge_intf.crm_cb &&
			s_ctrl->bridge_intf.crm_cb->notify_timer) {
			timer.link_hdl = s_ctrl->bridge_intf.link_hdl;
			timer.dev_hdl = s_ctrl->bridge_intf.device_hdl;
			timer.state = true;
			rc = s_ctrl->bridge_intf.crm_cb->notify_timer(&timer);
			if (rc) {
				CAM_ERR(CAM_SENSOR,
					"%s Enable CRM SOF freeze timer failed rc: %d",
					s_ctrl->sensor_name, rc);
				return rc;
			}
		}

		CAM_GET_TIMESTAMP(ts);
		CAM_CONVERT_TIMESTAMP_FORMAT(ts, hrs, min, sec, ms);

		CAM_INFO(CAM_SENSOR,
			"%llu:%llu:%llu.%llu CAM_START_DEV Success for %s sensor_id:0x%x,sensor_slave_addr:0x%x",
			hrs, min, sec, ms,
			s_ctrl->sensor_name,
			s_ctrl->sensordata->slave_info.sensor_id,
			s_ctrl->sensordata->slave_info.sensor_slave_addr);
	}
		break;
	case CAM_STOP_DEV: {
		if (s_ctrl->stream_off_after_eof) {
			s_ctrl->is_stopped_by_user = true;
			CAM_DBG(CAM_SENSOR, "Ignore stop dev cmd for VFPS feature");
			goto release_mutex;
		}

#if defined(CONFIG_CAMERA_FRAME_CNT_DBG)
		cam_sensor_thread_destroy(s_ctrl);
#endif

		rc = cam_sensor_stream_off(s_ctrl);
		if (rc)
			goto release_mutex;
	}
		break;
	case CAM_CONFIG_DEV: {
		rc = cam_sensor_pkt_parse(s_ctrl, arg);
		if (rc < 0) {
			if (rc == -EBADR)
				CAM_INFO(CAM_SENSOR,
					"%s:Failed pkt parse. rc: %d, it has been flushed",
					s_ctrl->sensor_name, rc);
			else
				CAM_ERR(CAM_SENSOR,
					"%s:Failed pkt parse. rc: %d",
					s_ctrl->sensor_name, rc);
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
					"%s: Reapplying the Init settings due to cci hw reset",
					s_ctrl->sensor_name);
				usleep_range(1000, 1010);
				rc = cam_sensor_apply_settings(s_ctrl, 0,
					pkt_opcode);
			}
			s_ctrl->i2c_data.init_settings.request_id = -1;

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
			if (rc < 0) {
				CAM_ERR(CAM_UTIL, "[HWB]failed rc %d\n", rc);
				if (s_ctrl != NULL) {
					hw_bigdata_i2c_from_sensor(s_ctrl);
				}
			}
			hw_bigdata_update_eeprom_error_cnt(s_ctrl);
			hw_bigdata_update_cam_entrance_cnt(s_ctrl);
#endif
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"%s: cannot apply init settings rc= %d",
					s_ctrl->sensor_name, rc);
				delete_request(&s_ctrl->i2c_data.init_settings);
				goto release_mutex;
			}
			rc = delete_request(&s_ctrl->i2c_data.init_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"%s: Fail in deleting the Init settings",
					s_ctrl->sensor_name);
				goto release_mutex;
			}
		}

		if (s_ctrl->i2c_data.config_settings.is_settings_valid &&
			(s_ctrl->i2c_data.config_settings.request_id == 0)) {
			if (s_ctrl->sensor_state == CAM_SENSOR_START) {
				delete_request(&s_ctrl->i2c_data.config_settings);
				CAM_ERR(CAM_SENSOR,
					"%s: get config setting in start state",
					s_ctrl->sensor_name);
					goto release_mutex;
			}

			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG);

			s_ctrl->i2c_data.config_settings.request_id = -1;

			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"%s: cannot apply config settings",
					s_ctrl->sensor_name);
				delete_request(
					&s_ctrl->i2c_data.config_settings);
				goto release_mutex;
			}
			rc = delete_request(&s_ctrl->i2c_data.config_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"%s: Fail in deleting the config settings",
					s_ctrl->sensor_name);
				goto release_mutex;
			}
			s_ctrl->sensor_state = CAM_SENSOR_CONFIG;
		}

		if (s_ctrl->i2c_data.read_settings.is_settings_valid) {
			rc = cam_sensor_i2c_read_data(
				&s_ctrl->i2c_data.read_settings,
				&s_ctrl->io_master_info);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "%s: cannot read data: %d",
					s_ctrl->sensor_name, rc);
				delete_request(&s_ctrl->i2c_data.read_settings);
				goto release_mutex;
			}
			rc = delete_request(
				&s_ctrl->i2c_data.read_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"%s: Fail in deleting the read settings",
					s_ctrl->sensor_name);
				goto release_mutex;
			}
		}
	}
		break;
	default:
		CAM_ERR(CAM_SENSOR, "%s: Invalid Opcode: %d",
			s_ctrl->sensor_name, cmd->op_code);
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
	if (s_ctrl->pipeline_delay >= 1 && s_ctrl->pipeline_delay <= 3) {
		info->p_delay = s_ctrl->pipeline_delay;
		info->m_delay = s_ctrl->modeswitch_delay;
	} else {
		info->p_delay = CAM_PIPELINE_DELAY_2;
		info->m_delay = CAM_MODESWITCH_DELAY_2;
	}
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
int cam_sensor_set_regulator_mode(struct cam_hw_soc_info *soc_info,
	unsigned int mode)
{
	int rc = 0;
	struct regulator *regulator;
	int *pCountFpwm = NULL;

	mutex_lock(&mode_mutex);
	if (soc_info->index == SEC_WIDE_SENSOR) {
#if defined(CONFIG_SEC_B0Q_PROJECT)
		regulator = regulator_get(soc_info->dev, "cam_v_custom1");
#else
		regulator = regulator_get(soc_info->dev, "cam_vdig");
#endif
		pCountFpwm = &countB1FPWM;
	} else {
		CAM_ERR(CAM_SENSOR, "Sensor Index is wrong");
		rc = -EINVAL;
		goto release_mutex;

	}

	if (IS_ERR_OR_NULL(regulator)) {
		CAM_ERR(CAM_SENSOR, "regulator_get fail %d", rc);
		if (regulator != NULL)
			regulator_put(regulator);
		rc = -EINVAL;
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
	CAM_INFO(CAM_SENSOR, "countB1FPWM : %d mode = %d", countB1FPWM, mode);
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
	struct cam_camera_slave_info   *slave_info;
	struct cam_hw_soc_info         *soc_info = &s_ctrl->soc_info;
	struct completion              *i3c_probe_completion = NULL;

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "failed: %pK", s_ctrl);
		return -EINVAL;
	}

#if defined(CONFIG_SENSOR_RETENTION)
	if (s_ctrl->sensordata->slave_info.sensor_id == RETENTION_SENSOR_ID) {
		CAM_INFO(CAM_SENSOR, "[RET_DBG] reset disable_sensor_retention parameter");
		s_ctrl->soc_info.disable_sensor_retention = FALSE;
	}
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	if (s_ctrl != NULL) {
		hw_bigdata_init_mipi_param_sensor(s_ctrl);
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

	if (s_ctrl->aon_camera_id != NOT_AON_CAM) {
		CAM_INFO(CAM_SENSOR,
			"Setup for Main Camera with csiphy index: %d",
			s_ctrl->sensordata->subdev_id[SUB_MODULE_CSIPHY]);
		rc = cam_sensor_util_aon_ops(true,
			s_ctrl->sensordata->subdev_id[SUB_MODULE_CSIPHY]);
		if (rc) {
			CAM_ERR(CAM_SENSOR,
				"Main camera access operation is not successful rc: %d",
				rc);
			return rc;
		}
	}

#if IS_ENABLED(CONFIG_SEC_PM) && (defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT) || defined(CONFIG_SEC_B0Q_PROJECT) || defined(CONFIG_SEC_Q4Q_PROJECT))
	if (soc_info->index == SEC_WIDE_SENSOR)
		cam_sensor_set_regulator_mode(soc_info, REGULATOR_MODE_FAST);
#endif
	if (s_ctrl->io_master_info.master_type == I3C_MASTER)
		i3c_probe_completion = cam_sensor_get_i3c_completion(s_ctrl->soc_info.index);

	rc = cam_sensor_core_power_up(power_info, soc_info, i3c_probe_completion);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "core power up failed:%d", rc);
#if IS_ENABLED(CONFIG_SEC_PM) && (defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT) || defined(CONFIG_SEC_B0Q_PROJECT) || defined(CONFIG_SEC_Q4Q_PROJECT))
		if (soc_info->index == SEC_WIDE_SENSOR)
			cam_sensor_set_regulator_mode(soc_info, REGULATOR_MODE_NORMAL);
#endif
		return rc;
	}

	rc = camera_io_init(&(s_ctrl->io_master_info));
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "cci_init failed: rc: %d", rc);
		goto cci_failure;
	}

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

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "failed: s_ctrl %pK", s_ctrl);
		return -EINVAL;
	}

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	if (s_ctrl != NULL) {
		hw_bigdata_deinit_mipi_param_sensor(s_ctrl);
	}
#endif

	power_info = &s_ctrl->sensordata->power_info;
	soc_info = &s_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_SENSOR, "failed: %s power_info %pK",
			s_ctrl->sensor_name, power_info);
		return -EINVAL;
	}

	rc = cam_sensor_util_power_down(power_info, soc_info);

#if IS_ENABLED(CONFIG_SEC_PM) && (defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT) || defined(CONFIG_SEC_B0Q_PROJECT) || defined(CONFIG_SEC_Q4Q_PROJECT))
	if (soc_info->index == SEC_WIDE_SENSOR)
		cam_sensor_set_regulator_mode(soc_info, REGULATOR_MODE_NORMAL);
#endif

	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "%s core power down failed:%d",
			s_ctrl->sensor_name, rc);
		return rc;
	}

	if (s_ctrl->aon_camera_id != NOT_AON_CAM) {
		CAM_INFO(CAM_SENSOR,
			"Setup for AON FW with csiphy index: %d",
			s_ctrl->sensordata->subdev_id[SUB_MODULE_CSIPHY]);
		rc = cam_sensor_util_aon_ops(false,
			s_ctrl->sensordata->subdev_id[SUB_MODULE_CSIPHY]);
		if (rc) {
			CAM_ERR(CAM_SENSOR,
				"AON FW access operation is not successful rc: %d",
				rc);
			return rc;
		}
	}

#if defined(CONFIG_SENSOR_RETENTION)
	if (s_ctrl->sensordata->slave_info.sensor_id == RETENTION_SENSOR_ID) {
		if (sensor_retention_mode == RETENTION_READY_TO_ON) {
			sensor_retention_mode = RETENTION_ON;
		}
	}
#endif

	if (s_ctrl->bob_pwm_switch) {
		rc = cam_sensor_bob_pwm_mode_switch(soc_info,
			s_ctrl->bob_reg_index, false);
		if (rc) {
			CAM_WARN(CAM_SENSOR,
				"%s BoB PWM setup failed rc: %d",
				s_ctrl->sensor_name, rc);
			rc = 0;
		}
	}

	camera_io_release(&(s_ctrl->io_master_info));

	return rc;
}


int cam_sensor_apply_settings(struct cam_sensor_ctrl_t *s_ctrl,
	int64_t req_id, enum cam_sensor_packet_opcodes opcode)
{
	int rc = 0, offset, i;
	uint64_t top = 0, del_req_id = 0;
	struct i2c_settings_array *i2c_set = NULL;
	struct i2c_settings_list *i2c_list;
#if defined(CONFIG_SAMSUNG_DEBUG_SENSOR_I2C)
	int32_t to_dbg_sen_id = -1;
	e_sensor_reg_upd_event_type sen_upd_evt_type = e_sensor_upd_event_invalid;
#endif

	if (req_id == 0) {
		switch (opcode) {
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON: {
			i2c_set = &s_ctrl->i2c_data.streamon_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG: {
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
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_REG_BANK_UNLOCK: {
			i2c_set = &s_ctrl->i2c_data.reg_bank_unlock_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_REG_BANK_LOCK: {
			i2c_set = &s_ctrl->i2c_data.reg_bank_lock_settings;
			break;
		}
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_FRAME_SKIP_UPDATE:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE:
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE_V2:
		default:
			return 0;
		}
		if (i2c_set->is_settings_valid == 1) {
			cam_sensor_pre_apply_settings(s_ctrl, opcode);
#if defined(CONFIG_SENSOR_RETENTION)
			if ((opcode == CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG) &&
				(cam_sensor_retention_calc_checksum(s_ctrl) >= 0)) {
				CAM_INFO(CAM_SENSOR, "[RET_DBG] Retention ON, Skip write init");
				return 0;
			}
#endif
			list_for_each_entry(i2c_list,
				&(i2c_set->list_head), list) {
#if defined(CONFIG_SAMSUNG_DEBUG_SENSOR_I2C)
				if (debug_sensor_name[0]!='\0') {
					cam_sensor_i2c_dump_util(s_ctrl, i2c_list, i2c_debug_cnt);
				}
				cam_sensor_parse_reg(s_ctrl, i2c_list,
					&to_dbg_sen_id,
					&sen_upd_evt_type);
#endif
				rc = cam_sensor_i2c_modes_util(
					&(s_ctrl->io_master_info),
					i2c_list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
						"Failed to apply settings: %d",
						rc);
					return rc;
				}
			}

#if 1
			cam_sensor_post_apply_settings(s_ctrl, opcode);
#endif
		}
	} else if (req_id > 0) {
		offset = req_id % MAX_PER_FRAME_ARRAY;

		if (opcode == CAM_SENSOR_PACKET_OPCODE_SENSOR_FRAME_SKIP_UPDATE)
			i2c_set = s_ctrl->i2c_data.frame_skip;
		else if (opcode == CAM_SENSOR_PACKET_OPCODE_SENSOR_BUBBLE_UPDATE) {
#if defined(CONFIG_SAMSUNG_DEBUG_SENSOR_I2C)
			s_ctrl->is_bubble_packet = true;
#endif
			i2c_set = s_ctrl->i2c_data.bubble_update;
			/*
			 * If bubble update isn't valid, then we just use
			 * per frame update.
			 */
			if (!(i2c_set[offset].is_settings_valid == 1) &&
				(i2c_set[offset].request_id == req_id))
				i2c_set = s_ctrl->i2c_data.per_frame;
		} else
			i2c_set = s_ctrl->i2c_data.per_frame;

		if (i2c_set[offset].is_settings_valid == 1 &&
			i2c_set[offset].request_id == req_id) {
			list_for_each_entry(i2c_list,
				&(i2c_set[offset].list_head), list) {
#if defined(CONFIG_SAMSUNG_DEBUG_SENSOR_I2C)
				if (debug_sensor_name[0] != '\0') {
					cam_sensor_i2c_dump_util(s_ctrl, i2c_list, i2c_debug_cnt);
				}
				cam_sensor_parse_reg(s_ctrl, i2c_list,
					&to_dbg_sen_id,
					&sen_upd_evt_type);
#endif
				rc = cam_sensor_i2c_modes_util(
					&(s_ctrl->io_master_info),
					i2c_list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
						"Failed to apply settings: %d",
						rc);
					return rc;
				}
			}
			CAM_DBG(CAM_SENSOR, "applied req_id: %llu", req_id);
		} else {
			CAM_DBG(CAM_SENSOR,
				"Invalid/NOP request to apply: %lld", req_id);
		}

		s_ctrl->last_applied_req = req_id;
		CAM_DBG(CAM_REQ,
			"Sensor[%d] updating last_applied [req id: %lld last_applied: %lld] with opcode:%d",
			s_ctrl->soc_info.index, req_id, s_ctrl->last_applied_req, opcode);

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

		/*
		 * If the op code is bubble update, then we also need to delete
		 * req for per frame update, vice versa.
		 */
		if (opcode == CAM_SENSOR_PACKET_OPCODE_SENSOR_BUBBLE_UPDATE)
			i2c_set = s_ctrl->i2c_data.per_frame;
		else if (opcode == CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE)
			i2c_set = s_ctrl->i2c_data.bubble_update;
		else
			i2c_set = NULL;

		if (i2c_set) {
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
	}

#if defined(CONFIG_SAMSUNG_DEBUG_SENSOR_I2C)
	if ((to_dbg_sen_id == s_ctrl->sensordata->slave_info.sensor_id) &&
		(sen_upd_evt_type != e_sensor_upd_event_invalid))
		cam_sensor_dbg_print_by_upd_type(s_ctrl, sen_upd_evt_type);
#endif

	return rc;
}

int32_t cam_sensor_apply_request(struct cam_req_mgr_apply_request *apply)
{
	int32_t rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;
	int32_t curr_idx, last_applied_idx;
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

	if ((apply->recovery) && (apply->request_id > 0)) {
		curr_idx = apply->request_id % MAX_PER_FRAME_ARRAY;
		last_applied_idx = s_ctrl->last_applied_req % MAX_PER_FRAME_ARRAY;

		/*
		 * If the sensor resolution index in current req isn't same with
		 * last applied index, we should apply bubble update.
		 */

		if ((s_ctrl->sensor_res[curr_idx].res_index !=
			s_ctrl->sensor_res[last_applied_idx].res_index) ||
			(s_ctrl->sensor_res[curr_idx].feature_mask !=
			s_ctrl->sensor_res[last_applied_idx].feature_mask)) {
			opcode = CAM_SENSOR_PACKET_OPCODE_SENSOR_BUBBLE_UPDATE;
			CAM_INFO(CAM_REQ,
				"Sensor[%d] update req id: %lld [last_applied: %lld] with opcode:%d recovery: %d last_applied_res_idx: %u current_res_idx: %u",
				s_ctrl->soc_info.index, apply->request_id,
				s_ctrl->last_applied_req, opcode, apply->recovery,
				s_ctrl->sensor_res[last_applied_idx].res_index,
				s_ctrl->sensor_res[curr_idx].res_index);
		}
	}

	CAM_DBG(CAM_REQ,
		"Sensor[%d] update req id: %lld [last_applied: %lld] with opcode:%d recovery: %d",
		s_ctrl->soc_info.index, apply->request_id,
		s_ctrl->last_applied_req, opcode, apply->recovery);
	trace_cam_apply_req("Sensor", s_ctrl->soc_info.index, apply->request_id, apply->link_hdl);

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

	CAM_INFO(CAM_REQ, " Sensor[%d] handle frame skip for req id: %lld",
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

		/*
		 * Sensor can't get EOF event during flush if we do the flush
		 * before EOF, so we need to stream off the sensor during flush
		 * for VFPS usecase.
		 */
		if (s_ctrl->stream_off_after_eof && s_ctrl->is_stopped_by_user) {
			cam_sensor_stream_off(s_ctrl);
			s_ctrl->is_stopped_by_user = false;
		}
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

int cam_sensor_process_evt(struct cam_req_mgr_link_evt_data *evt_data)
{
	int                       rc = 0;
	struct cam_sensor_ctrl_t *s_ctrl = NULL;
#if defined(CONFIG_SOF_FREEZE_FRAME_CNT_READ)
	uint32_t frame_cnt = 0;
#endif

	if (!evt_data)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(evt_data->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}

	CAM_DBG(CAM_SENSOR, "Received evt:%d", evt_data->evt_type);

	mutex_lock(&(s_ctrl->cam_sensor_mutex));

	switch (evt_data->evt_type) {
	case CAM_REQ_MGR_LINK_EVT_EOF:
		if (s_ctrl->stream_off_after_eof) {
			rc = cam_sensor_stream_off(s_ctrl);
			if (rc) {
				CAM_ERR(CAM_SENSOR, "Failed to stream off %s",
					s_ctrl->sensor_name);

				cam_sensor_notify_v4l2_error_event(s_ctrl,
					CAM_REQ_MGR_ERROR_TYPE_FULL_RECOVERY,
					CAM_REQ_MGR_SENSOR_STREAM_OFF_FAILED);
			}
		}
		break;
	case CAM_REQ_MGR_LINK_EVT_UPDATE_PROPERTIES:
		if (evt_data->u.properties_mask &
			CAM_LINK_PROPERTY_SENSOR_STANDBY_AFTER_EOF)
			s_ctrl->stream_off_after_eof = true;
		else
			s_ctrl->stream_off_after_eof = false;

		CAM_DBG(CAM_SENSOR, "sensor %s stream off after eof:%s",
			s_ctrl->sensor_name,
			CAM_BOOL_TO_YESNO(s_ctrl->stream_off_after_eof));
		break;
#if defined(CONFIG_SAMSUNG_DEBUG_SENSOR_I2C)
	case CAM_REQ_MGR_LINK_EVT_SOF_FREEZE:
	case CAM_REQ_MGR_LINK_EVT_ERR:
#if defined(CONFIG_SOF_FREEZE_FRAME_CNT_READ)
		rc = cam_sensor_read_frame_count(s_ctrl, &frame_cnt);
		if (rc >= 0)
			CAM_INFO(CAM_SENSOR, "[CNT_DBG][%s]: frame_cnt 0x%x",s_ctrl->sensor_name, frame_cnt);

		if (s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_IMX374) {
			rc = gpio_get_value_cansleep(MIPI_SW_SEL_GPIO);
			CAM_INFO(CAM_SENSOR, "[FREEZE_DBG][%s]: mipi_sw_sel_gpio value = %d",s_ctrl->sensor_name, rc);
		}
#endif

		CAM_INFO(CAM_SENSOR, "[FREEZE_DBG][%s] sof freeze proc_evt %d", s_ctrl->sensor_name,
			evt_data->evt_type);
		to_dump_when_sof_freeze__sen_id = s_ctrl->sensordata->slave_info.sensor_id;
		break;
#endif
	default:
		/* No handling */
		break;
	}

	mutex_unlock(&(s_ctrl->cam_sensor_mutex));

	return rc;
}

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
int msm_is_sec_get_sensor_position(uint32_t *cam_position)
{
	*cam_position = sec_sensor_position;
	return 0;
}

int msm_is_sec_get_sensor_comp_mode(uint32_t **sensor_clk_size)
{
	*sensor_clk_size = &sec_sensor_clk_size;
	return 0;
}
#endif
