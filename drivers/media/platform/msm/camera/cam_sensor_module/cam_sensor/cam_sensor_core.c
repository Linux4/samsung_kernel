/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <cam_sensor_cmn_header.h>
#include "cam_sensor_core.h"
#include "cam_sensor_util.h"
#include "cam_soc_util.h"
#include "cam_trace.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"

#if defined(CONFIG_GC5035_MACRO_OTP_DD_AUTOLOAD)
#include "gc5035_macro_otp.h"
#endif

#if defined(CONFIG_LEDS_S2MU106_FLASH) || defined(CONFIG_LEDS_S2MU107_FLASH)
extern int muic_afc_set_voltage(int vol);
extern void pdo_ctrl_by_flash(bool mode);
#endif

#if defined(CONFIG_SAMSUNG_REAR_TOF) || defined(CONFIG_SAMSUNG_FRONT_TOF)
struct cam_sensor_ctrl_t *g_s_ctrl_tof;
int check_pd_ready;
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
//#define HWB_FILE_OPERATION 1
uint32_t sec_sensor_position;
uint32_t sec_sensor_clk_size;

static struct cam_hw_param_collector cam_hwparam_collector;
#endif

#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
#include "cam_sensor_mipi.h"

static int disable_adaptive_mipi;
module_param(disable_adaptive_mipi, int, 0644);
#endif

extern uint16_t platformSensorId;

#define STREAM_ON_ADDR_IMX586_S5K4HA    0x0100
#define STREAM_ON_ADDR_IMX316           0x1001
#define STREAM_ON_ADDR_GW3              0x0100
#define STREAM_ON_ADDR_HI847            0x0B00

char tof_freq[10] = "\n";

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
		case SENSOR_ID_S5KGW1P:
		case SENSOR_ID_IMX682:
			ret = 1;
			break;
		default:
			ret = 0;
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
		CAM_DBG(CAM_SENSOR, "[AM_DBG] Write MIPI setting before Stream On setting. mipi_index : %d",
			s_ctrl->mipi_clock_index_new);

 		cur_mipi_sensor_mode = &(s_ctrl->mipi_info[0]);
		memset(&mipi_i2c_list, 0, sizeof(mipi_i2c_list));

		memcpy(&mipi_i2c_list.i2c_settings,
			cur_mipi_sensor_mode->mipi_setting[s_ctrl->mipi_clock_index_new].clk_setting,
			sizeof(struct cam_sensor_i2c_reg_setting));

		CAM_INFO(CAM_SENSOR, "[AM_DBG] Picked MIPI clock : %s",
			cur_mipi_sensor_mode->mipi_setting[s_ctrl->mipi_clock_index_new].str_mipi_clk);

		CAM_DBG(CAM_SENSOR, "[AM_DBG] reg_setting [0x%x 0x%x %d %d]",
			mipi_i2c_list.i2c_settings.reg_setting->reg_addr, mipi_i2c_list.i2c_settings.reg_setting->reg_data,
			mipi_i2c_list.i2c_settings.reg_setting->delay, mipi_i2c_list.i2c_settings.reg_setting->data_mask);
		CAM_DBG(CAM_SENSOR, "[AM_DBG] size [0x%x] addr [%d] data [%d] delay [%d]",
			mipi_i2c_list.i2c_settings.size, mipi_i2c_list.i2c_settings.addr_type,
			mipi_i2c_list.i2c_settings.data_type, mipi_i2c_list.i2c_settings.delay);

		if (mipi_i2c_list.i2c_settings.size > 0)
			rc = camera_io_dev_write(&s_ctrl->io_master_info,
				&(mipi_i2c_list.i2c_settings));
	}

	return rc;
}
#endif

static void cam_sensor_update_req_mgr(
	struct cam_sensor_ctrl_t *s_ctrl,
	struct cam_packet *csl_packet)
{
	struct cam_req_mgr_add_request add_req;

	add_req.link_hdl = s_ctrl->bridge_intf.link_hdl;
	add_req.req_id = csl_packet->header.request_id;
	CAM_DBG(CAM_SENSOR, " Rxed Req Id: %lld",
		csl_packet->header.request_id);
	add_req.dev_hdl = s_ctrl->bridge_intf.device_hdl;
	add_req.skip_before_applying = 0;
	if (s_ctrl->bridge_intf.crm_cb &&
		s_ctrl->bridge_intf.crm_cb->add_req)
		s_ctrl->bridge_intf.crm_cb->add_req(&add_req);

	CAM_DBG(CAM_SENSOR, " add req to req mgr: %lld",
			add_req.req_id);
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
}

static int32_t cam_sensor_i2c_pkt_parse(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg)
{
	int32_t rc = 0;
	uintptr_t generic_ptr;
	struct cam_control *ioctl_ctrl = NULL;
	struct cam_packet *csl_packet = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
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
		goto rel_pkt_buf;
	}

	remain_len -= (size_t)config.offset;
	csl_packet = (struct cam_packet *)(generic_ptr +
		(uint32_t)config.offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_SENSOR, "Invalid packet params");
		rc = -EINVAL;
		goto rel_pkt_buf;

	}

	if ((csl_packet->header.op_code & 0xFFFFFF) !=
		CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG &&
		csl_packet->header.request_id <= s_ctrl->last_flush_req
		&& s_ctrl->last_flush_req != 0) {
		CAM_DBG(CAM_SENSOR,
			"reject request %lld, last request to flush %lld",
			csl_packet->header.request_id, s_ctrl->last_flush_req);
		rc = -EINVAL;
		goto rel_pkt_buf;
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
			goto rel_pkt_buf;

		s_ctrl->streamon_count = s_ctrl->streamon_count + 1;
		i2c_reg_settings = &i2c_data->streamon_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF: {
		if (s_ctrl->streamoff_count > 0)
			goto rel_pkt_buf;

		s_ctrl->streamoff_count = s_ctrl->streamoff_count + 1;
		i2c_reg_settings = &i2c_data->streamoff_settings;
		i2c_reg_settings->request_id = 0;
		i2c_reg_settings->is_settings_valid = 1;
		break;
	}

	case CAM_SENSOR_PACKET_OPCODE_SENSOR_MODE: {
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
		CAM_DBG(CAM_SENSOR, "[adaptive_mipi] SENSOR_MODE : %d", csl_packet->header.request_id);
		s_ctrl->sensor_mode = csl_packet->header.request_id;
#endif
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE)) {
			CAM_WARN(CAM_SENSOR,
				"Rxed Update packets without linking");
			goto rel_pkt_buf;
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
			cam_sensor_update_req_mgr(s_ctrl, csl_packet);
			goto rel_pkt_buf;
		}
		break;
	}
	case CAM_SENSOR_PACKET_OPCODE_SENSOR_NOP: {
		if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) ||
			(s_ctrl->sensor_state == CAM_SENSOR_ACQUIRE)) {
			CAM_WARN(CAM_SENSOR,
				"Rxed NOP packets without linking");
			goto rel_pkt_buf;
		}

		cam_sensor_update_req_mgr(s_ctrl, csl_packet);
		goto rel_pkt_buf;
	}
	default:
		CAM_ERR(CAM_SENSOR, "Invalid Packet Header");
		rc = -EINVAL;
		goto rel_pkt_buf;
	}

	offset = (uint32_t *)&csl_packet->payload;
	offset += csl_packet->cmd_buf_offset / 4;
	cmd_desc = (struct cam_cmd_buf_desc *)(offset);

	rc = cam_sensor_i2c_command_parser(&s_ctrl->io_master_info,
			i2c_reg_settings, cmd_desc, 1);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "Fail parsing I2C Pkt: %d", rc);
		goto rel_pkt_buf;
	}

	if ((csl_packet->header.op_code & 0xFFFFFF) ==
		CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE) {
		i2c_reg_settings->request_id =
			csl_packet->header.request_id;
		cam_sensor_update_req_mgr(s_ctrl, csl_packet);
	}

rel_pkt_buf:
	if (cam_mem_put_cpu_buf(config.packet_handle))
		CAM_WARN(CAM_SENSOR, "Failed in put the buffer: 0x%x",
			config.packet_handle);

	return rc;
}

static int32_t cam_sensor_i2c_modes_util(
	struct cam_sensor_ctrl_t *s_ctrl,
	struct camera_io_master *io_master_info,
	struct i2c_settings_list *i2c_list)
{
	int32_t rc = 0;
	uint32_t i, size;

#if defined(CONFIG_SEC_A72Q_PROJECT)
	if ((i2c_list->i2c_settings.size > 0)
		&& (i2c_list->i2c_settings.reg_setting[0].reg_addr == STREAM_ON_ADDR_IMX586_S5K4HA 
		|| i2c_list->i2c_settings.reg_setting[0].reg_addr == STREAM_ON_ADDR_HI847)
		&& (i2c_list->i2c_settings.reg_setting[0].reg_data == 0x0)) {
		uint32_t frame_cnt = 0;
		int retry_cnt = 20;
		do{
			if (s_ctrl->sensordata->slave_info.sensor_id == HI847_SENSOR_ID) {
				rc = camera_io_dev_read(io_master_info, 0x0732, &frame_cnt,
				CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			}
			else
			{
				rc = camera_io_dev_read(io_master_info, 0x0005, &frame_cnt,
				CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
			}

			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "[CNT_DBG] Failed to read frame_cnt");
				break;
			}

			if (((s_ctrl->sensordata->slave_info.sensor_id == HI847_SENSOR_ID) && ((frame_cnt & 0x01)  == 0x01))
				|| ((s_ctrl->sensordata->slave_info.sensor_id != HI847_SENSOR_ID) && (frame_cnt != 0xFF)
				&& (frame_cnt > 0))) {
	
				if (s_ctrl->sensordata->slave_info.sensor_id == HI847_SENSOR_ID)
					usleep_range(4000, 5000);
				CAM_INFO(CAM_SENSOR, "[CNT_DBG] 0x%x : Last frame_cnt 0x%x",
				s_ctrl->sensordata->slave_info.sensor_id, frame_cnt);
				break;
			}
			CAM_DBG(CAM_SENSOR, "retry cnt : %d, Stream on, frame_cnt : %x", retry_cnt, frame_cnt);
			if(frame_cnt == 0xFF)
				usleep_range(2000, 3000);
			retry_cnt--;
		} while( frame_cnt == 0xFF && retry_cnt>0);
		CAM_INFO(CAM_SENSOR, "Stream ON end retry_cnt = %d frame_cnt %d", retry_cnt, frame_cnt);
	}
#endif

	if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_RANDOM) {
		rc = camera_io_dev_write(io_master_info,
			&(i2c_list->i2c_settings));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to random write I2C settings: %d",
				rc);
			return rc;
		}
// Use below code to wait for Stream OFF by checking sensor frame count register 
#if defined(CONFIG_SEC_A72Q_PROJECT) 
		if ((i2c_list->i2c_settings.size > 0)
			&& (i2c_list->i2c_settings.reg_setting[0].reg_addr == STREAM_ON_ADDR_IMX586_S5K4HA || i2c_list->i2c_settings.reg_setting[0].reg_addr == STREAM_ON_ADDR_IMX316 || i2c_list->i2c_settings.reg_setting[0].reg_addr == STREAM_ON_ADDR_HI847)
			&& (i2c_list->i2c_settings.reg_setting[0].reg_data == 0x0)) {
			uint32_t frame_cnt = 0;
			int retry_cnt = 20;
			CAM_INFO(CAM_SENSOR, "Stream off start retry_cnt = %d", retry_cnt);

			do {
				if (s_ctrl->sensordata->slave_info.sensor_id == HI847_SENSOR_ID)
				{
					rc = camera_io_dev_read(io_master_info, 0x0732,	&frame_cnt,
					CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
				}
				else {
					rc = camera_io_dev_read(io_master_info, 0x0005,	&frame_cnt,
					CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
				}

				if (rc < 0) {
					CAM_ERR(CAM_SENSOR, "[CNT_DBG] Failed to read frame_cnt");
					break;
				}
//Check the HI847 frame count for A72 after Stream OFF
				if (((s_ctrl->sensordata->slave_info.sensor_id == HI847_SENSOR_ID) && ((frame_cnt & 0x01)  == 0x00))
					|| ((s_ctrl->sensordata->slave_info.sensor_id != HI847_SENSOR_ID) && (frame_cnt == 0xFF))) {

					if (s_ctrl->sensordata->slave_info.sensor_id == HI847_SENSOR_ID)
						usleep_range(1000, 1010);
					break;
				}
				CAM_DBG(CAM_SENSOR, "retry cnt : %d, Stream off, frame_cnt : %x", retry_cnt, frame_cnt);
				if (frame_cnt != 0xFF)
				    usleep_range(2000, 3000);
				retry_cnt--;
			} while (frame_cnt != 0xFF && retry_cnt > 0);
			CAM_INFO(CAM_SENSOR, "Stream off end retry_cnt = %d", retry_cnt);
		}
#endif

#if defined(CONFIG_SEC_M42Q_PROJECT)
        if ((i2c_list->i2c_settings.size > 0)
			&& (i2c_list->i2c_settings.reg_setting[0].reg_addr == 0x6010)
			&& (i2c_list->i2c_settings.reg_setting[0].reg_data == 0x0001)){
			CAM_DBG(CAM_SENSOR, "Delay 5 ms after 6010");
            usleep_range(5000,6000);
		}
//Check the GW3 frame count register after Stream ON command 		
		if ((i2c_list->i2c_settings.size > 0)
			&& (i2c_list->i2c_settings.reg_setting[0].reg_addr == STREAM_ON_ADDR_GW3)
			&& (i2c_list->i2c_settings.reg_setting[0].reg_data == 0x0100)) {
			uint32_t frame_cnt = 0;
			int retry_cnt = 30;
			do{
				rc = camera_io_dev_read(io_master_info, 0x0005,	&frame_cnt,
					CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
				CAM_DBG(CAM_SENSOR, "retry cnt : %d, Stream on, frame_cnt : %x", retry_cnt, frame_cnt);
				if(frame_cnt == 0xFF)
					usleep_range(2000, 3000);
				retry_cnt--;
			} while( frame_cnt == 0xFF && retry_cnt>0);
			CAM_INFO(CAM_SENSOR, "Stream ON end retry_cnt = %d frame_cnt %d", retry_cnt, frame_cnt);
		}
#endif
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
	s_ctrl->sensordata->slave_info.version_id =
		probe_info->version_id;
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
		goto rel_pkt_buf;
	}

	if ((len < sizeof(struct cam_packet)) ||
		(pkt->cmd_buf_offset >= (len - sizeof(struct cam_packet)))) {
		CAM_ERR(CAM_SENSOR, "Not enough buf provided");
		rc = -EINVAL;
		goto rel_pkt_buf;
	}

	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *)&pkt->payload + pkt->cmd_buf_offset/4);
	if (cmd_desc == NULL) {
		CAM_ERR(CAM_SENSOR, "command descriptor pos is invalid");
		rc = -EINVAL;
		goto rel_pkt_buf;
	}
	if (pkt->num_cmd_buf != 2) {
		CAM_ERR(CAM_SENSOR, "Expected More Command Buffers : %d",
			 pkt->num_cmd_buf);
		rc = -EINVAL;
		goto rel_pkt_buf;
	}

	for (i = 0; i < pkt->num_cmd_buf; i++) {
		if (!(cmd_desc[i].length))
			continue;
		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			&cmd_buf1, &len);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to parse the command Buffer Header");
			goto rel_pkt_buf;
		}
		if (cmd_desc[i].offset >= len) {
			CAM_ERR(CAM_SENSOR,
				"offset past length of buffer");
			rc = -EINVAL;
			goto rel_pkt_buf;
		}
		remain_len = len - cmd_desc[i].offset;
		if (cmd_desc[i].length > remain_len) {
			CAM_ERR(CAM_SENSOR,
				"Not enough buffer provided for cmd");
			rc = -EINVAL;
			goto rel_pkt_buf;
		}
		cmd_buf = (uint32_t *)cmd_buf1;
		cmd_buf += cmd_desc[i].offset/4;
		ptr = (void *) cmd_buf;

		rc = cam_handle_cmd_buffers_for_probe(ptr, s_ctrl,
			i, cmd_desc[i].length, remain_len);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to parse the command Buffer Header");
			goto rel_cmd_buf;
		}

		if (cam_mem_put_cpu_buf(cmd_desc[i].mem_handle))
			CAM_WARN(CAM_SENSOR,
				"Failed to put command Buffer : 0x%x",
				cmd_desc[i].mem_handle);
	}

	if (cam_mem_put_cpu_buf(handle))
		CAM_WARN(CAM_SENSOR, "Failed to put the command Buffer: 0x%x",
			handle);

	return rc;

rel_cmd_buf:
	if (cam_mem_put_cpu_buf(cmd_desc[i].mem_handle))
		CAM_WARN(CAM_SENSOR, "Failed to put command Buffer : 0x%x",
			cmd_desc[i].mem_handle);
rel_pkt_buf:
	if (cam_mem_put_cpu_buf(handle))
		CAM_WARN(CAM_SENSOR, "Failed to put the command Buffer: 0x%x",
			handle);

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

	if ((s_ctrl->sensor_state == CAM_SENSOR_INIT) &&
		(s_ctrl->is_probe_succeed == 0))
		return;

	cam_sensor_release_stream_rsc(s_ctrl);
	cam_sensor_release_per_frame_resource(s_ctrl);

	if (s_ctrl->sensor_state != CAM_SENSOR_INIT)
		cam_sensor_power_down(s_ctrl);

	rc = cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "dhdl already destroyed: rc = %d", rc);
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

#if defined(CONFIG_SAMSUNG_FRONT_TOF) || defined(CONFIG_SAMSUNG_REAR_TOF)
        if(slave_info->sensor_id == TOF_SENSOR_ID_IMX316)
        {
            chipid >>= 4;
        }
#endif
#if defined(CONFIG_SEC_A60Q_PROJECT) || defined(CONFIG_SEC_M40_PROJECT)
	usleep_range(200, 300);
#endif
	CAM_ERR(CAM_SENSOR, "read id: 0x%x expected id 0x%x:",
			 chipid, slave_info->sensor_id);
	if (cam_sensor_id_by_mask(s_ctrl, chipid) != slave_info->sensor_id) {
		CAM_ERR(CAM_SENSOR, "chip id %x does not match %x",
				chipid, slave_info->sensor_id);
		return -ENODEV;
	}
	return rc;
}


#if defined(CONFIG_GC5035_MACRO_OTP_DD_AUTOLOAD)
int32_t gc5035_otp_dd_autoload_process(struct cam_sensor_ctrl_t *s_ctrl)
{
	//Temporary Register Settings Holders for Write Operation
	struct cam_sensor_i2c_reg_setting reg_setting;
	struct cam_sensor_i2c_reg_array reg_arr;
	//Store Result of Function
	int rc = 0;
	//Temporary Variable for Reading Different Register Values
	uint32_t reg_val = 0;
	//Variable for Storing Information about Number of Bad Pixels
	uint16_t dd_num = 0;

	uint32_t dd_autoload_step1_size       = sizeof(dd_autoload_step1_reg_arr)/sizeof(struct cam_sensor_i2c_reg_array);
	uint32_t dd_autoload_step3_size       = sizeof(dd_autoload_step3_reg_arr)/sizeof(struct cam_sensor_i2c_reg_array);
	uint32_t dd_autoload_defect_qty1_size = sizeof(dd_autoload_defect_qty1_reg_arr)/sizeof(struct cam_sensor_i2c_reg_array);
	uint32_t dd_autoload_defect_qty2_size = sizeof(dd_autoload_defect_qty2_reg_arr)/sizeof(struct cam_sensor_i2c_reg_array);
	uint32_t dd_autoload_step4_size       = sizeof(dd_autoload_step4_reg_arr)/sizeof(struct cam_sensor_i2c_reg_array);
	uint32_t dd_autoload_step5_size       = sizeof(dd_autoload_step5_reg_arr)/sizeof(struct cam_sensor_i2c_reg_array);
	uint32_t dd_autoload_step7_size       = sizeof(dd_autoload_step7_reg_arr)/sizeof(struct cam_sensor_i2c_reg_array);

	/* Step - 1 - Start */

	memset(&reg_setting, 0, sizeof(reg_setting));
	reg_setting.size        = dd_autoload_step1_size;
	reg_setting.addr_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.reg_setting = dd_autoload_step1_reg_arr;

	rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_setting);

	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 1 failed");
		return rc;
	}
	else
	{
		CAM_DBG(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP Autoload Step 1 Success");
	}

	/* Step - 1 - End */

	/* Step - 2 - Start */

	memset(&reg_setting, 0, sizeof(reg_setting));
	reg_setting.size        = dd_autoload_step3_size;
	reg_setting.addr_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.reg_setting = dd_autoload_step3_reg_arr;

	rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_setting);

	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 2 failed");
		return rc;
	}
	else
	{
		CAM_DBG(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP Autoload Step 2 Success");
	}

	/* Step - 2 - End */

	/* Step - 3 - Start */

	memset(&reg_setting, 0, sizeof(reg_setting));
	reg_setting.size        = dd_autoload_defect_qty1_size;
	reg_setting.addr_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.reg_setting = dd_autoload_defect_qty1_reg_arr;

	rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_setting);

	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 3a failed");
		return rc;
	}
	else
	{
		CAM_DBG(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 3a Success");
	}

	rc = camera_io_dev_read(
			&(s_ctrl->io_master_info),
			DD_AUTOLOAD_TOTAL_NUM_BAD_PIXEL, &reg_val,
			CAMERA_SENSOR_I2C_TYPE_BYTE,
			CAMERA_SENSOR_I2C_TYPE_BYTE);
		if (rc < 0)
		{
			CAM_ERR(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP Read register address fail %d", rc);
			return rc;
		}
		else
		{
			dd_num += reg_val;
			CAM_DBG(CAM_SENSOR,"DPC_DBG GC5035 Macro Sensor OTP Read register address 0x%x, register value 0x%x", DD_AUTOLOAD_TOTAL_NUM_BAD_PIXEL, reg_val);
		}

	memset(&reg_setting, 0, sizeof(reg_setting));
	reg_setting.size        = dd_autoload_defect_qty2_size;
	reg_setting.addr_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.reg_setting = dd_autoload_defect_qty2_reg_arr;

	rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_setting);

	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 3b failed");
		return rc;
	}
	else
	{
		CAM_DBG(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 3b Success");
	}

	rc = camera_io_dev_read(
			&(s_ctrl->io_master_info),
			DD_AUTOLOAD_TOTAL_NUM_BAD_PIXEL, &reg_val,
			CAMERA_SENSOR_I2C_TYPE_BYTE,
			CAMERA_SENSOR_I2C_TYPE_BYTE);
		if (rc < 0)
		{
			CAM_ERR(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP Read register address fail %d", rc);
			return rc;
		}
		else
		{
			dd_num += reg_val;
			CAM_DBG(CAM_SENSOR,"DPC_DBG GC5035 Macro Sensor OTP Read register address 0x%x, register value 0x%x", DD_AUTOLOAD_TOTAL_NUM_BAD_PIXEL, reg_val);
		}

	/* Step - 3 - End */

	/* Step - 4 - Start */

	reg_arr.reg_addr = 0x01;
	reg_arr.reg_data = 0x00;
	reg_setting.reg_setting = &reg_arr;
	reg_setting.size = 1;
	reg_setting.addr_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;

	rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_setting);

	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP MACRO DD Autoload Step 4a DD NUM failed");
		return rc;
	}
	else
	{
		CAM_DBG(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP MACRO DD Autoload Step 4a DD NUM Success");
	}

	reg_arr.reg_addr = 0x02;
	reg_arr.reg_data = dd_num;
	reg_setting.reg_setting = &reg_arr;
	reg_setting.size = 1;
	reg_setting.addr_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;

	rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_setting);

	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 4b DD NUM failed");
		return rc;
	}
	else
	{
		CAM_DBG(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 4b DD NUM Success with dd num 0x%x", dd_num);
	}

	memset(&reg_setting, 0, sizeof(reg_setting));
	reg_setting.size        = dd_autoload_step4_size;
	reg_setting.addr_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.reg_setting = dd_autoload_step4_reg_arr;

	rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_setting);

	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 4 failed");
		return rc;
	}
	else
	{
		CAM_DBG(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 4 Success");
	}

	/* Step - 4 - End */

	/* Step - 5 - Start */

	memset(&reg_setting, 0, sizeof(reg_setting));
	reg_setting.size        = dd_autoload_step5_size;
	reg_setting.addr_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.reg_setting = dd_autoload_step5_reg_arr;

	rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_setting);

	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 5 failed");
		return rc;
	}
	else
	{
		CAM_DBG(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 5 Success");
	}

	/* Step - 5 - End */

	/* Step - 6 - Start */

	msleep(4); //Autoload Process takes 4ms to complete

	/* Step - 6 - End */

	/* Step - 7 - Start */

	memset(&reg_setting, 0, sizeof(reg_setting));
	reg_setting.size        = dd_autoload_step7_size;
	reg_setting.addr_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type   = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.reg_setting = dd_autoload_step7_reg_arr;

	rc = camera_io_dev_write(&s_ctrl->io_master_info, &reg_setting);

	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 7 failed");
		return rc;
	}
	else
	{
		CAM_DBG(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP DD Autoload Step 7 Success");
	}

	/* Step - 7 - End */

	CAM_DBG(CAM_SENSOR, "DPC_DBG GC5035 Macro Sensor OTP Proceed to Resolution/Stream On");

	return rc;
}
#endif

int32_t cam_sensor_driver_cmd(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg)
{
	int rc = 0;
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_sensor_power_ctrl_t *power_info =
		&s_ctrl->sensordata->power_info;

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	struct cam_hw_param *hw_param = NULL;
#endif

#if defined(CONFIG_SEC_A52Q_PROJECT) || defined(CONFIG_SEC_A72Q_PROJECT)
	uint32_t version_id = 0;
	uint16_t sensor_id = 0;
	uint16_t expected_version_id = 0;
#endif

#if defined(CONFIG_GC5035_MACRO_OTP_DD_AUTOLOAD)
		uint16_t autoload_retry_count = 2;
#endif

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
		CAM_INFO(CAM_SENSOR, "Probe command received for %d", s_ctrl->soc_info.index);
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

		platformSensorId = s_ctrl->sensordata->slave_info.sensor_id;
		
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

		/* Power up and probe sensor */
		rc = cam_sensor_power_up(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "power up failed");
			goto free_power_settings;
		}

#if defined(CONFIG_SEC_A52Q_PROJECT) || defined(CONFIG_SEC_A72Q_PROJECT)
		if (s_ctrl->soc_info.index == 0 &&
			s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_S5KGW1) { // check Rear GW1
			sensor_id = s_ctrl->sensordata->slave_info.sensor_id;
			expected_version_id = s_ctrl->sensordata->slave_info.version_id;
			rc = camera_io_dev_read(
				&(s_ctrl->io_master_info),
				0x0002, &version_id,
				CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_WORD);
			version_id>>=8;
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "Read version id fail %d", rc);
			} else {
				CAM_INFO(CAM_SENSOR,
					"Read version id 0x%x,expected_version_id 0x%x", version_id, expected_version_id);
					if (version_id == expected_version_id && version_id == 0XA0)
						CAM_INFO(CAM_SENSOR, "Found A0 - Non OTP GW1P Sensor");
					else if (version_id == expected_version_id && version_id == 0XA1)
						CAM_INFO(CAM_SENSOR, "Found A1 - OTP GW1P Sensor");
					else {
						CAM_INFO(CAM_SENSOR, "Not matched");
						rc = -EINVAL;
						cam_sensor_power_down(s_ctrl);
						goto release_mutex;
				}
			}
		}
		if (((s_ctrl->soc_info.index == 1)||(s_ctrl->soc_info.index == 8)) &&
			s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_S5KGD2) { // check Front GD2
			sensor_id = s_ctrl->sensordata->slave_info.sensor_id;
			expected_version_id = s_ctrl->sensordata->slave_info.version_id;
			rc = camera_io_dev_read(
				&(s_ctrl->io_master_info),
				0x0002, &version_id,
				CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_WORD);
			//version_id>>=8;
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "Read version id fail %d", rc);
			} else {
				CAM_INFO(CAM_SENSOR,
					"Read version id 0x%x,expected_version_id 0x%x", version_id, expected_version_id);
					if (version_id == expected_version_id && version_id == 0X0)
						CAM_INFO(CAM_SENSOR, "Found A0 - Non OTP GD2 Sensor");
					else if (version_id == expected_version_id && version_id == 0XA001)
						CAM_INFO(CAM_SENSOR, "Found A1 - OTP GD2 Sensor");
					else {
						CAM_INFO(CAM_SENSOR, "Not matched");
						rc = -EINVAL;
						cam_sensor_power_down(s_ctrl);
						goto release_mutex;
				}
			}
		}
#endif

		/* Match sensor ID */
		rc = cam_sensor_match_id(s_ctrl);


#if 0
		if (rc < 0) {
			cam_sensor_power_down(s_ctrl);
			msleep(20);
			goto free_power_settings;
		}
#endif

#if defined(CONFIG_SEC_GTA4XLVE_PROJECT)
		if ((rc < 0) &&
			(((s_ctrl->soc_info.index == 0) && (s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_S5K4HA)) ||
			 ((s_ctrl->soc_info.index == 1) && (s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_GC5035)) ||
			 ((s_ctrl->soc_info.index == 8) && (s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_GC5035))))
		{
			CAM_ERR(CAM_SENSOR,
				"Probe failed - slot:%d,slave_addr:0x%x,sensor_id:0x%x",
				s_ctrl->soc_info.index,
				s_ctrl->sensordata->slave_info.sensor_slave_addr,
				s_ctrl->sensordata->slave_info.sensor_id);
			rc = -EINVAL;
			cam_sensor_power_down(s_ctrl);
			msleep(20);
			goto free_power_settings;
		}
#endif

#if defined(CONFIG_SEC_A71_PROJECT)
		if ((rc < 0) &&
			((s_ctrl->soc_info.index == 0) &&
			(s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_S5KGW1)))
		{
			CAM_ERR(CAM_SENSOR,
				"Probe failed - slot:%d,slave_addr:0x%x,sensor_id:0x%x",
				s_ctrl->soc_info.index,
				s_ctrl->sensordata->slave_info.sensor_slave_addr,
				s_ctrl->sensordata->slave_info.sensor_id);
			rc = -EINVAL;
			cam_sensor_power_down(s_ctrl);
			msleep(20);
			goto free_power_settings;
		}
#endif

#if defined(CONFIG_SEC_A52Q_PROJECT) || defined(CONFIG_SEC_A72Q_PROJECT)
		if ((rc < 0) &&
			((s_ctrl->soc_info.index == 0) &&
			(s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_IMX682)))
		{
			CAM_ERR(CAM_SENSOR,
				"Probe failed - slot:%d,slave_addr:0x%x,sensor_id:0x%x",
				s_ctrl->soc_info.index,
				s_ctrl->sensordata->slave_info.sensor_slave_addr,
				s_ctrl->sensordata->slave_info.sensor_id);
			rc = -EINVAL;
			cam_sensor_power_down(s_ctrl);
			msleep(20);
			goto free_power_settings;
		}
		if ((rc < 0) &&
			(((s_ctrl->soc_info.index == 1)||(s_ctrl->soc_info.index == 8)) &&
			(s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_IMX616)))
		{
			CAM_ERR(CAM_SENSOR,
				"Probe failed - slot:%d,slave_addr:0x%x,sensor_id:0x%x",
				s_ctrl->soc_info.index,
				s_ctrl->sensordata->slave_info.sensor_slave_addr,
				s_ctrl->sensordata->slave_info.sensor_id);
			rc = -EINVAL;
			cam_sensor_power_down(s_ctrl);
			msleep(20);
			goto free_power_settings;
		}
		if ((rc < 0) &&
			(((s_ctrl->soc_info.index == 1)||(s_ctrl->soc_info.index == 2)) &&
			(s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_S5K3L6)))
		{
			CAM_ERR(CAM_SENSOR,
				"Probe failed - slot:%d,slave_addr:0x%x,sensor_id:0x%x",
				s_ctrl->soc_info.index,
				s_ctrl->sensordata->slave_info.sensor_slave_addr,
				s_ctrl->sensordata->slave_info.sensor_id);
			rc = -EINVAL;
			cam_sensor_power_down(s_ctrl);
			msleep(20);
			goto free_power_settings;
		}
#endif

#if 1 //For factory module test
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "need to check sensor module : 0x%x",
				s_ctrl->sensordata->slave_info.sensor_id);
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
			if (rc < 0) {
				CAM_ERR(CAM_HWB, "failed rc %d\n", rc);
				if (s_ctrl != NULL) {
					switch (s_ctrl->id) {
					case CAMERA_0:
						if (!msm_is_sec_get_rear_hw_param(&hw_param)) {
							if (hw_param != NULL) {
								CAM_ERR(CAM_HWB, "[R][I2C] Err\n");
								hw_param->i2c_sensor_err_cnt++;
								hw_param->need_update_to_file = TRUE;
							}
						}
						break;

					case CAMERA_1:
						if (!msm_is_sec_get_front_hw_param(&hw_param)) {
							if (hw_param != NULL) {
								CAM_ERR(CAM_HWB, "[F][I2C] Err\n");
								hw_param->i2c_sensor_err_cnt++;
								hw_param->need_update_to_file = TRUE;
							}
						}
						break;

#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
					case CAMERA_2:
						if (!msm_is_sec_get_front2_hw_param(&hw_param)) {
							if (hw_param != NULL) {
								CAM_ERR(CAM_HWB, "[F2][I2C] Err\n");
								hw_param->i2c_sensor_err_cnt++;
								hw_param->need_update_to_file = TRUE;
							}
						}
						break;
#endif

#if defined(CONFIG_SAMSUNG_FRONT_TOP)
					case CAMERA_5:
						if (!msm_is_sec_get_front3_hw_param(&hw_param)) {
							if (hw_param != NULL) {
								CAM_ERR(CAM_HWB, "[F3][I2C] Err\n");
								hw_param->i2c_sensor_err_cnt++;
								hw_param->need_update_to_file = TRUE;
							}
						}
						break;
#endif

#if defined(CONFIG_SAMSUNG_REAR_DUAL) || defined(CONFIG_SAMSUNG_REAR_TRIPLE)
					case CAMERA_3:
						if (!msm_is_sec_get_rear2_hw_param(&hw_param)) {
							if (hw_param != NULL) {
								CAM_ERR(CAM_HWB, "[R2][I2C] Err\n");
								hw_param->i2c_sensor_err_cnt++;
								hw_param->need_update_to_file = TRUE;
							}
						}
						break;
#endif

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
					case CAMERA_4:
						if (!msm_is_sec_get_rear3_hw_param(&hw_param)) {
							if (hw_param != NULL) {
								CAM_ERR(CAM_HWB, "[R3][I2C] Err\n");
								hw_param->i2c_sensor_err_cnt++;
								hw_param->need_update_to_file = TRUE;
							}
						}
						break;
#endif

#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
					case CAMERA_3:
						if (!msm_is_sec_get_iris_hw_param(&hw_param)) {
							if (hw_param != NULL) {
								CAM_ERR(CAM_HWB, "[I][I2C] Err\n");
								hw_param->i2c_sensor_err_cnt++;
								hw_param->need_update_to_file = TRUE;
							}
						}
						break;
#endif

					default:
						CAM_ERR(CAM_HWB, "[NON][I2C] Unsupport\n");
						break;
					}
				}
			}
#endif
		}
#else
		if (rc < 0) {
			cam_sensor_power_down(s_ctrl);
			msleep(20);
			kfree(pu);
			kfree(pd);
			goto release_mutex;
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

		sensor_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		s_ctrl->bridge_intf.device_hdl = sensor_acq_dev.device_handle;
		s_ctrl->bridge_intf.session_hdl = sensor_acq_dev.session_handle;

#if defined(CONFIG_SAMSUNG_REAR_TOF) || defined(CONFIG_SAMSUNG_FRONT_TOF)
		if(s_ctrl->sensordata->slave_info.sensor_id == 0x316)
		{
			g_s_ctrl_tof = s_ctrl;
			check_pd_ready = 0;
		}
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
		if (sec_sensor_position < s_ctrl->id) {
			sec_sensor_position = s_ctrl->id;
			CAM_ERR(CAM_SENSOR, "sensor_position: %d", sec_sensor_position);
		}
#endif

		CAM_DBG(CAM_SENSOR, "Device Handle: %d",
			sensor_acq_dev.device_handle);
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&sensor_acq_dev,
			sizeof(struct cam_sensor_acquire_dev))) {
			CAM_ERR(CAM_SENSOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}

		rc = cam_sensor_power_up(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "Sensor Power up failed");
			goto release_mutex;
		}

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
#if defined(CONFIG_SAMSUNG_REAR_TOF) || defined(CONFIG_SAMSUNG_FRONT_TOF)
		g_s_ctrl_tof = NULL;
		check_pd_ready = 0;
#endif
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


		if (s_ctrl->i2c_data.streamon_settings.is_settings_valid &&
			(s_ctrl->i2c_data.streamon_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"cannot apply streamon settings");
				goto release_mutex;
			}
		}

#if defined(CONFIG_SAMSUNG_REAR_TOF) || defined(CONFIG_SAMSUNG_FRONT_TOF)
		if (s_ctrl->sensordata->slave_info.sensor_id == TOF_SENSOR_ID_IMX316 &&
			s_ctrl->sensordata->power_info.gpio_num_info &&
			s_ctrl->sensordata->power_info.gpio_num_info->valid[SENSOR_CUSTOM_GPIO1] == 1) {
			gpio_set_value_cansleep(
				s_ctrl->sensordata->power_info.gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1],
				GPIOF_OUT_INIT_HIGH);
		}
#endif
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

#if defined(CONFIG_SEC_A90Q_PROJECT)
		if (s_ctrl->sensordata->slave_info.sensor_id == SENSOR_ID_IMX316
			&& (s_ctrl->soc_info.index == 7 /*Front TOF*/ || s_ctrl->soc_info.index == 8)) {
			scnprintf(tof_freq, sizeof(tof_freq), "0");
			CAM_INFO(CAM_SENSOR, "[TOF_FREQ_DBG] tof_freq : %s", tof_freq);
		}
#endif
		if (s_ctrl->i2c_data.streamoff_settings.is_settings_valid &&
			(s_ctrl->i2c_data.streamoff_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMOFF);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
				"cannot apply streamoff settings");
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

#if defined(CONFIG_GC5035_MACRO_OTP_DD_AUTOLOAD)
init:
#endif			

			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_INITIAL_CONFIG);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"cannot apply init settings");
				goto release_mutex;
			}
#if defined(CONFIG_GC5035_MACRO_OTP_DD_AUTOLOAD)
#if defined (CONFIG_SEC_A52Q_PROJECT)
			if ((s_ctrl->soc_info.index == 4)||(s_ctrl->soc_info.index == 3)) {
#elif defined(CONFIG_SEC_GTA4XLVE_PROJECT)
            if ((s_ctrl->soc_info.index == 1)||(s_ctrl->soc_info.index == 8)) {
#elif defined(CONFIG_SEC_A72Q_PROJECT)
			if (s_ctrl->soc_info.index == 4) {
#endif			
				if(autoload_retry_count > 0)
				{
					rc = gc5035_otp_dd_autoload_process(s_ctrl);
					if(rc < 0){
						CAM_ERR(CAM_SENSOR,"GC5035 OTP DD Autoload Failed");
						autoload_retry_count--;
						goto init;
					}
					else{
						CAM_DBG(CAM_SENSOR,"GC5035 OTP DD Autoload Success");
					}
				}
			}
#endif
			rc = delete_request(&s_ctrl->i2c_data.init_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Fail in deleting the Init settings");
				goto release_mutex;
			}
			s_ctrl->i2c_data.init_settings.request_id = -1;
		}

		if (s_ctrl->i2c_data.config_settings.is_settings_valid &&
			(s_ctrl->i2c_data.config_settings.request_id == 0)) {
			rc = cam_sensor_apply_settings(s_ctrl, 0,
				CAM_SENSOR_PACKET_OPCODE_SENSOR_CONFIG);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"cannot apply config settings");
				goto release_mutex;
			}
			rc = delete_request(&s_ctrl->i2c_data.config_settings);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Fail in deleting the config settings");
				goto release_mutex;
			}
			s_ctrl->sensor_state = CAM_SENSOR_CONFIG;
			s_ctrl->i2c_data.config_settings.request_id = -1;
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

// Added for PLM P191224-07745 (suggestion from sLSI PMIC team)
// Set the PMIC voltage to 5V for Flash operation on Rear Sensor
#if defined(CONFIG_LEDS_S2MU106_FLASH) || defined(CONFIG_LEDS_S2MU107_FLASH)
	if(s_ctrl->soc_info.index == 0 || s_ctrl->soc_info.index == 4)
	{
		pdo_ctrl_by_flash(1);
		muic_afc_set_voltage(5);
	}
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	if (s_ctrl != NULL) {
		switch (s_ctrl->id) {
		case CAMERA_0:
			if (!msm_is_sec_get_rear_hw_param(&hw_param)) {
				if (hw_param != NULL) {
					CAM_DBG(CAM_HWB, "[R][INIT] Init\n");
					hw_param->i2c_chk = FALSE;
					hw_param->mipi_chk = FALSE;
					hw_param->need_update_to_file = FALSE;
					hw_param->comp_chk = FALSE;
				}
			}
			break;

		case CAMERA_1:
			if (!msm_is_sec_get_front_hw_param(&hw_param)) {
				if (hw_param != NULL) {
					CAM_DBG(CAM_HWB, "[F][INIT] Init\n");
					hw_param->i2c_chk = FALSE;
					hw_param->mipi_chk = FALSE;
					hw_param->need_update_to_file = FALSE;
					hw_param->comp_chk = FALSE;
				}
			}
			break;

#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
		case CAMERA_2:
			if (!msm_is_sec_get_front2_hw_param(&hw_param)) {
				if (hw_param != NULL) {
					CAM_DBG(CAM_HWB, "[F2][INIT] Init\n");
					hw_param->i2c_chk = FALSE;
					hw_param->mipi_chk = FALSE;
					hw_param->need_update_to_file = FALSE;
					hw_param->comp_chk = FALSE;
				}
			}
			break;
#endif

#if defined(CONFIG_SAMSUNG_FRONT_TOP)
		case CAMERA_5:
			if (!msm_is_sec_get_front3_hw_param(&hw_param)) {
				if (hw_param != NULL) {
					CAM_DBG(CAM_HWB, "[F3][INIT] Init\n");
					hw_param->i2c_chk = FALSE;
					hw_param->mipi_chk = FALSE;
					hw_param->need_update_to_file = FALSE;
					hw_param->comp_chk = FALSE;
				}
			}
			break;
#endif

#if defined(CONFIG_SAMSUNG_REAR_DUAL) || defined(CONFIG_SAMSUNG_REAR_TRIPLE)
		case CAMERA_3:
			if (!msm_is_sec_get_rear2_hw_param(&hw_param)) {
				if (hw_param != NULL) {
					CAM_DBG(CAM_HWB, "[R2][INIT] Init\n");
					hw_param->i2c_chk = FALSE;
					hw_param->mipi_chk = FALSE;
					hw_param->need_update_to_file = FALSE;
					hw_param->comp_chk = FALSE;

				}
			}
			break;
#endif

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
		case CAMERA_4:
			if (!msm_is_sec_get_rear3_hw_param(&hw_param)) {
				if (hw_param != NULL) {
					CAM_DBG(CAM_HWB, "[R3][INIT] Init\n");
					hw_param->i2c_chk = FALSE;
					hw_param->mipi_chk = FALSE;
					hw_param->need_update_to_file = FALSE;
					hw_param->comp_chk = FALSE;
				}
			}
			break;
#endif

#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
		case CAMERA_3:
			if (!msm_is_sec_get_iris_hw_param(&hw_param)) {
				if (hw_param != NULL) {
					CAM_DBG(CAM_HWB, "[I][INIT] Init\n");
					hw_param->i2c_chk = FALSE;
					hw_param->mipi_chk = FALSE;
					hw_param->need_update_to_file = FALSE;
					hw_param->comp_chk = FALSE;
				}
			}
			break;
#endif

		default:
			CAM_ERR(CAM_HWB, "[NON][INIT] Unsupport\n");
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

	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "power up the core is failed:%d", rc);
		return rc;
	}
#if defined(CONFIG_MCLK_I2C_DELAY)
    usleep_range(11000, 12000); //Add delay for MCLK - I2C TIMING SPEC OUT issue
    CAM_INFO(CAM_SENSOR, "MCLK delay added");
#endif
	rc = camera_io_init(&(s_ctrl->io_master_info));
	if (rc < 0)
		CAM_ERR(CAM_SENSOR, "cci_init failed: rc: %d", rc);

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

// Added for PLM P191224-07745 (suggestion from sLSI PMIC team)
// Re-Set the PMIC voltage
#if defined(CONFIG_LEDS_S2MU106_FLASH) || defined(CONFIG_LEDS_S2MU107_FLASH)
	if(s_ctrl->soc_info.index == 0 || s_ctrl->soc_info.index == 4)
	{
		pdo_ctrl_by_flash(0);
		muic_afc_set_voltage(9);
	}
#endif

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
						CAM_DBG(CAM_HWB, "[R][DEINIT] Update\n");
						msm_is_sec_copy_err_cnt_to_file();
					}
					hw_param->need_update_to_file = FALSE;
				}
			}
			break;

		case CAMERA_1:
			if (!msm_is_sec_get_front_hw_param(&hw_param)) {
				if (hw_param != NULL) {
					hw_param->i2c_chk = FALSE;
					hw_param->mipi_chk = FALSE;
					hw_param->comp_chk = FALSE;

					if (hw_param->need_update_to_file) {
						CAM_DBG(CAM_HWB, "[F][DEINIT] Update\n");
						msm_is_sec_copy_err_cnt_to_file();
					}
					hw_param->need_update_to_file = FALSE;
				}
			}
			break;

#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
		case CAMERA_2:
			if (!msm_is_sec_get_front2_hw_param(&hw_param)) {
				if (hw_param != NULL) {
					hw_param->i2c_chk = FALSE;
					hw_param->mipi_chk = FALSE;
					hw_param->comp_chk = FALSE;

					if (hw_param->need_update_to_file) {
						CAM_DBG(CAM_HWB, "[F2][DEINIT] Update\n");
						msm_is_sec_copy_err_cnt_to_file();
					}
					hw_param->need_update_to_file = FALSE;
				}
			}
			break;
#endif

#if defined(CONFIG_SAMSUNG_FRONT_TOP)
		case CAMERA_5:
			if (!msm_is_sec_get_front3_hw_param(&hw_param)) {
				if (hw_param != NULL) {
					hw_param->i2c_chk = FALSE;
					hw_param->mipi_chk = FALSE;
					hw_param->comp_chk = FALSE;

					if (hw_param->need_update_to_file) {
						CAM_DBG(CAM_HWB, "[F3][DEINIT] Update\n");
						msm_is_sec_copy_err_cnt_to_file();
					}
					hw_param->need_update_to_file = FALSE;
				}
			}
			break;
#endif

#if defined(CONFIG_SAMSUNG_REAR_DUAL) || defined(CONFIG_SAMSUNG_REAR_TRIPLE)
		case CAMERA_3:
			if (!msm_is_sec_get_rear2_hw_param(&hw_param)) {
				if (hw_param != NULL) {
					hw_param->i2c_chk = FALSE;
					hw_param->mipi_chk = FALSE;
					hw_param->comp_chk = FALSE;

					if (hw_param->need_update_to_file) {
						CAM_DBG(CAM_HWB, "[R2][DEINIT] Update\n");
						msm_is_sec_copy_err_cnt_to_file();
					}
					hw_param->need_update_to_file = FALSE;
				}
			}
			break;
#endif

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
		case CAMERA_4:
			if (!msm_is_sec_get_rear3_hw_param(&hw_param)) {
				if (hw_param != NULL) {
					hw_param->i2c_chk = FALSE;
					hw_param->mipi_chk = FALSE;
					hw_param->comp_chk = FALSE;

					if (hw_param->need_update_to_file) {
						CAM_DBG(CAM_HWB, "[R3][DEINIT] Update\n");
						msm_is_sec_copy_err_cnt_to_file();
					}
					hw_param->need_update_to_file = FALSE;
				}
			}
			break;
#endif


#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
		case CAMERA_3:
			if (!msm_is_sec_get_iris_hw_param(&hw_param)) {
				if (hw_param != NULL) {
					hw_param->i2c_chk = FALSE;
					hw_param->mipi_chk = FALSE;
					hw_param->comp_chk = FALSE;

					if (hw_param->need_update_to_file) {
						CAM_DBG(CAM_HWB, "[I][DEINIT] Update\n");
						msm_is_sec_copy_err_cnt_to_file();
					}
					hw_param->need_update_to_file = FALSE;
				}
			}
			break;
#endif

		default:
			CAM_ERR(CAM_HWB, "[NON][DEINIT] Unsupport\n");
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

// Add 200us delay to meet the power off specification iT3 (End of MIPI transfer to MCLK disable and I2C shutdown)
#if defined(CONFIG_MCLK_I2C_DELAY)
    usleep_range(200, 300);
#endif
	rc = cam_sensor_util_power_down(power_info, soc_info);
	
#if defined(CONFIG_MCLK_I2C_DELAY_FOR_CAM_POWERDOWN)
    msleep(6);
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
	int64_t req_id, enum cam_sensor_packet_opcodes opcode)
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
		case CAM_SENSOR_PACKET_OPCODE_SENSOR_PROBE:
		default:
			return 0;
		}
		if (i2c_set->is_settings_valid == 1) {
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
			if (opcode == CAM_SENSOR_PACKET_OPCODE_SENSOR_STREAMON) {
				cam_sensor_apply_adaptive_mipi_settings(s_ctrl);
			}
#endif
			list_for_each_entry(i2c_list,
				&(i2c_set->list_head), list) {
				rc = cam_sensor_i2c_modes_util(
					s_ctrl,
					&(s_ctrl->io_master_info),
					i2c_list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
						"Failed to apply settings: %d",
						rc);
					return rc;
				}
			}
		}
	} else {
		offset = req_id % MAX_PER_FRAME_ARRAY;
		i2c_set = &(s_ctrl->i2c_data.per_frame[offset]);
		if (i2c_set->is_settings_valid == 1 &&
			i2c_set->request_id == req_id) {
			list_for_each_entry(i2c_list,
				&(i2c_set->list_head), list) {
				rc = cam_sensor_i2c_modes_util(
					s_ctrl,
					&(s_ctrl->io_master_info),
					i2c_list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
						"Failed to apply settings: %d",
						rc);
					return rc;
				}
			}
		} else {
			CAM_DBG(CAM_SENSOR,
				"Invalid/NOP request to apply: %lld", req_id);
		}

		/* Change the logic dynamically */
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			if ((req_id >=
				s_ctrl->i2c_data.per_frame[i].request_id) &&
				(top <
				s_ctrl->i2c_data.per_frame[i].request_id) &&
				(s_ctrl->i2c_data.per_frame[i].
				is_settings_valid == 1)) {
				del_req_id = top;
				top = s_ctrl->i2c_data.per_frame[i].request_id;
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
				 s_ctrl->i2c_data.per_frame[i].request_id) && (
				 s_ctrl->i2c_data.per_frame[i].is_settings_valid
					== 1)) {
				s_ctrl->i2c_data.per_frame[i].request_id = 0;
				rc = delete_request(
					&(s_ctrl->i2c_data.per_frame[i]));
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

	if (!apply)
		return -EINVAL;

	s_ctrl = (struct cam_sensor_ctrl_t *)
		cam_get_device_priv(apply->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "Device data is NULL");
		return -EINVAL;
	}
	CAM_DBG(CAM_REQ, " Sensor update req id: %lld", apply->request_id);
	trace_cam_apply_req("Sensor", apply->request_id);
	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	rc = cam_sensor_apply_settings(s_ctrl, apply->request_id,
		CAM_SENSOR_PACKET_OPCODE_SENSOR_UPDATE);
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
	if (s_ctrl->sensor_state != CAM_SENSOR_START ||
		s_ctrl->sensor_state != CAM_SENSOR_CONFIG) {
		mutex_unlock(&(s_ctrl->cam_sensor_mutex));
		return rc;
	}

	if (s_ctrl->i2c_data.per_frame == NULL) {
		CAM_ERR(CAM_SENSOR, "i2c frame data is NULL");
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
	CAM_INFO(CAM_HWB, "All_Init_Cnt\n");
	memset(&cam_hwparam_collector, 0, sizeof(struct cam_hw_param_collector));
}

void msm_is_sec_init_err_cnt_file(struct cam_hw_param *hw_param)
{
	if (hw_param != NULL) {
		CAM_INFO(CAM_HWB, "Init_Cnt\n");

		memset(hw_param, 0, sizeof(struct cam_hw_param));
		msm_is_sec_copy_err_cnt_to_file();
	} else {
		CAM_INFO(CAM_HWB, "NULL\n");
	}
}

void msm_is_sec_dbg_check(void)
{
	CAM_INFO(CAM_HWB, "Dbg E\n");
	CAM_INFO(CAM_HWB, "Dbg X\n");
}

void msm_is_sec_copy_err_cnt_to_file(void)
{
#if defined(HWB_FILE_OPERATION)
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nwrite = 0;
	int old_mask = 0;

	CAM_INFO(CAM_HWB, "To_F\n");

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	old_mask = sys_umask(0);

	fp = filp_open(CAM_HW_ERR_CNT_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0660);
	if (IS_ERR_OR_NULL(fp)) {
		CAM_ERR(CAM_HWB, "[To_F] Err\n");
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
		CAM_INFO(CAM_HWB, "From_F\n");
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(CAM_HW_ERR_CNT_FILE_PATH, O_RDONLY, 0660);
		if (IS_ERR_OR_NULL(fp)) {
			CAM_ERR(CAM_HWB, "[From_F] Err\n");
			set_fs(old_fs);
			return;
		}

		nread = vfs_read(fp, (char *)&cam_hwparam_collector, sizeof(struct cam_hw_param_collector), &fp->f_pos);

		filp_close(fp, NULL);
		fp = NULL;
		set_fs(old_fs);
	} else {
		CAM_INFO(CAM_HWB, "NoEx_F\n");
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
		CAM_INFO(CAM_HWB, "Ex_F\n");
		ret = 1;
	} else {
		switch (chktype) {
		case HW_PARAMS_CREATED:
			CAM_INFO(CAM_HWB, "Ex_Cr\n");
			msm_is_sec_init_all_cnt();

			old_mask = sys_umask(0);

			fp = filp_open(CAM_HW_ERR_CNT_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0660);
			if (IS_ERR_OR_NULL(fp)) {
				CAM_ERR(CAM_HWB, "[Ex_F] ERROR\n");
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
			CAM_INFO(CAM_HWB, "Ex_NoCr\n");
			ret = 0;
			break;

		default:
			CAM_INFO(CAM_HWB, "Ex_Err\n");
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

#if defined(CONFIG_SAMSUNG_REAR_TOF) || defined(CONFIG_SAMSUNG_FRONT_TOF)
void cam_sensor_tof_i2c_read(uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type)
{
	int rc = 0;

	if (g_s_ctrl_tof)
	{
		rc = camera_io_dev_read(&g_s_ctrl_tof->io_master_info, addr,
			data, addr_type, data_type);
		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "Failed to read 0x%x", addr);

		CAM_INFO(CAM_SENSOR, "[TOF_I2C] tof_i2c_read, addr : 0x%x, data : 0x%x", addr, *data);
	}
	else
	{
		CAM_ERR(CAM_SENSOR, "tof i2c is not ready!");
	}
}

void cam_sensor_tof_i2c_write(uint32_t addr, uint32_t data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type)
{
	int rc = 0;

	struct cam_sensor_i2c_reg_setting  i2c_reg_settings;
	struct cam_sensor_i2c_reg_array    i2c_reg_array;

	CAM_DBG(CAM_SENSOR, "[TOF_I2C] tof_i2c_write, addr : 0x%x, data : 0x%x", addr, data);

	if (g_s_ctrl_tof)
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

		rc = camera_io_dev_write(&g_s_ctrl_tof->io_master_info,
			&i2c_reg_settings);

		if (rc < 0)
			CAM_ERR(CAM_SENSOR, "Failed to i2c write");
	}
	else
	{
		CAM_ERR(CAM_SENSOR, "tof i2c is not ready!");
	}
}
#endif
