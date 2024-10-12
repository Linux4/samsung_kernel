/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <cam_sensor_cmn_header.h>
#include "cam_actuator_core.h"
#include "cam_sensor_util.h"
#include "cam_trace.h"
#include "cam_res_mgr_api.h"
#include "cam_common_util.h"
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
#include "cam_ois_core.h"
#include "cam_ois_mcu_stm32g.h"
#endif
#if defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
#include "cam_ois_core.h"
#include "cam_sensor_i2c.h"
#include "cam_ois_rumba_s4.h"
#endif

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
extern struct cam_ois_ctrl_t *g_o_ctrl;
#endif

int32_t cam_actuator_construct_default_power_setting(
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0;

	power_info->power_setting_size = 2;
	power_info->power_setting =
		(struct cam_sensor_power_setting *)
		kzalloc(sizeof(struct cam_sensor_power_setting) * MAX_POWER_CONFIG,
			GFP_KERNEL);
	if (!power_info->power_setting)
		return -ENOMEM;

	power_info->power_setting[0].seq_type = SENSOR_VAF;
	power_info->power_setting[0].seq_val = CAM_VAF;
	power_info->power_setting[0].config_val = 1;
	power_info->power_setting[0].delay = 1;

	power_info->power_setting[1].seq_type = SENSOR_VIO;
	power_info->power_setting[1].seq_val = CAM_VIO;
	power_info->power_setting[1].config_val = 1;
	power_info->power_setting[1].delay = 10;

	power_info->power_down_setting_size = 2;
	power_info->power_down_setting =
		(struct cam_sensor_power_setting *)
		kzalloc(sizeof(struct cam_sensor_power_setting) * MAX_POWER_CONFIG,
			GFP_KERNEL);
	if (!power_info->power_down_setting) {
		rc = -ENOMEM;
		goto free_power_settings;
	}

	power_info->power_down_setting[0].seq_type = SENSOR_VIO;
	power_info->power_down_setting[0].seq_val = CAM_VIO;
	power_info->power_down_setting[0].config_val = 0;

	power_info->power_down_setting[1].seq_type = SENSOR_VAF;
	power_info->power_down_setting[1].seq_val = CAM_VAF;
	power_info->power_down_setting[1].config_val = 0;

	return rc;

free_power_settings:
	kfree(power_info->power_setting);
	power_info->power_setting = NULL;
	power_info->power_setting_size = 0;
	return rc;
}

int32_t cam_actuator_power_up(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	struct cam_hw_soc_info  *soc_info =
		&a_ctrl->soc_info;
	struct cam_actuator_soc_private  *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	if ((power_info->power_setting == NULL) &&
		(power_info->power_down_setting == NULL)) {
		CAM_INFO(CAM_ACTUATOR,
			"Using default power settings");
		rc = cam_actuator_construct_default_power_setting(power_info);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Construct default actuator power setting failed.");
			return rc;
		}
	}

	/* Parse and fill vreg params for power up settings */
	rc = msm_camera_fill_vreg_params(
		&a_ctrl->soc_info,
		power_info->power_setting,
		power_info->power_setting_size);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed to fill vreg params for power up rc:%d", rc);
		return rc;
	}

	/* Parse and fill vreg params for power down settings*/
	rc = msm_camera_fill_vreg_params(
		&a_ctrl->soc_info,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed to fill vreg params power down rc:%d", rc);
		return rc;
	}

	power_info->dev = soc_info->dev;

	CAM_INFO(CAM_ACTUATOR, "POWER UP ABOUT TO CALL");
	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed in actuator power up rc %d", rc);
		return rc;
	}
  
#if defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
	//skip actuator power up and cci1 init , sensor has been power up and ois has init cci1 master 1
	a_ctrl->io_master_info.cci_client->cci_subdev =
	cam_cci_get_subdev(a_ctrl->io_master_info.cci_client->cci_device);	
	return rc;
#endif

	rc = camera_io_init(&a_ctrl->io_master_info);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR, "cci init failed: rc: %d", rc);

	return rc;
}

int32_t cam_actuator_power_down(struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_hw_soc_info *soc_info = &a_ctrl->soc_info;
	struct cam_actuator_soc_private  *soc_private;

	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "failed: a_ctrl %pK", a_ctrl);
		return -EINVAL;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;
	soc_info = &a_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_ACTUATOR, "failed: power_info %pK", power_info);
		return -EINVAL;
	}
	CAM_INFO(CAM_ACTUATOR, "ABOUT TO POWER DOWN at slot:%d", soc_info->index);

#if defined(CONFIG_SAMSUNG_FORCE_DISABLE_REGULATOR)
	rc = cam_sensor_util_power_down(power_info, soc_info, FALSE);
#else
	rc = cam_sensor_util_power_down(power_info, soc_info);
#endif
	if (rc) {
		CAM_ERR(CAM_ACTUATOR, "power down the core is failed:%d", rc);
		return rc;
	}

#if defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
	return rc;
#endif

	camera_io_release(&a_ctrl->io_master_info);

	return rc;
}

static int32_t cam_actuator_i2c_modes_util(
	struct camera_io_master *io_master_info,
	struct i2c_settings_list *i2c_list)
{
	int32_t rc = 0;
	uint32_t i, size;

	if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_RANDOM) {
		rc = camera_io_dev_write(io_master_info,
			&(i2c_list->i2c_settings));
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
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
			CAM_ERR(CAM_ACTUATOR,
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
			CAM_ERR(CAM_ACTUATOR,
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
				CAM_ERR(CAM_ACTUATOR,
					"i2c poll apply setting Fail: %d", rc);
				return rc;
			}
		}
	}

	return rc;
}

int32_t cam_actuator_slaveInfo_pkt_parser(struct cam_actuator_ctrl_t *a_ctrl,
	uint32_t *cmd_buf)
{
	int32_t rc = 0;
	struct cam_cmd_i2c_info *i2c_info;

	if (!a_ctrl || !cmd_buf) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
	if (a_ctrl->io_master_info.master_type == CCI_MASTER) {
		a_ctrl->io_master_info.cci_client->cci_i2c_master =
			a_ctrl->cci_i2c_master;
		a_ctrl->io_master_info.cci_client->i2c_freq_mode =
			i2c_info->i2c_freq_mode;
		if (i2c_info->slave_addr > 0)
			a_ctrl->io_master_info.cci_client->sid =
				i2c_info->slave_addr >> 1;
		CAM_DBG(CAM_ACTUATOR, "Slave addr: 0x%x Freq Mode: %d",
			i2c_info->slave_addr, i2c_info->i2c_freq_mode);
	} else if (a_ctrl->io_master_info.master_type == I2C_MASTER) {
		if (i2c_info->slave_addr > 0)
			a_ctrl->io_master_info.client->addr = i2c_info->slave_addr;
		CAM_DBG(CAM_ACTUATOR, "Slave addr: 0x%x", i2c_info->slave_addr);
	} else {
		CAM_ERR(CAM_ACTUATOR, "Invalid Master type: %d",
			a_ctrl->io_master_info.master_type);
		 rc = -EINVAL;
	}

	return rc;
}

int32_t cam_actuator_apply_settings(struct cam_actuator_ctrl_t *a_ctrl,
	struct i2c_settings_array *i2c_set)
{
	struct i2c_settings_list *i2c_list;
	int32_t rc = 0;
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
	int i = 0;
	int size = 0;
	int position = 0;
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	struct cam_hw_param *hw_param = NULL;
#endif

	if (a_ctrl == NULL || i2c_set == NULL) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	if (i2c_set->is_settings_valid != 1) {
		CAM_ERR(CAM_ACTUATOR, " Invalid settings");
		return -EINVAL;
	}

	list_for_each_entry(i2c_list,
		&(i2c_set->list_head), list) {
		rc = cam_actuator_i2c_modes_util(
			&(a_ctrl->io_master_info),
			i2c_list);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to apply settings: %d",
				rc);
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
			if ((rc < 0) && (i2c_list->i2c_settings.reg_setting->reg_data == 0x0)) {
				if (a_ctrl != NULL) {
					switch (a_ctrl->soc_info.index) {
					case CAMERA_0:
						if (!msm_is_sec_get_rear_hw_param(&hw_param)) {
							if (hw_param != NULL) {
								CAM_ERR(CAM_HWB, "[R][AF] Err\n");
								hw_param->i2c_af_err_cnt++;
								hw_param->need_update_to_file = TRUE;
							}
						}
						break;

					case CAMERA_1:
						if (!msm_is_sec_get_front_hw_param(&hw_param)) {
							if (hw_param != NULL) {
								CAM_ERR(CAM_HWB, "[F][AF] Err\n");
								hw_param->i2c_af_err_cnt++;
								hw_param->need_update_to_file = TRUE;
							}
						}
						break;

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
					case CAMERA_2:
						if (!msm_is_sec_get_rear3_hw_param(&hw_param)) {
							if (hw_param != NULL) {
								CAM_ERR(CAM_HWB, "[R3][AF] Err\n");
								hw_param->i2c_af_err_cnt++;
								hw_param->need_update_to_file = TRUE;
							}
						}
						break;
#endif

#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
					case CAMERA_3:
						if (!msm_is_sec_get_iris_hw_param(&hw_param)) {
							if (hw_param != NULL) {
								CAM_ERR(CAM_HWB, "[I][AF] Err\n");
								hw_param->i2c_af_err_cnt++;
								hw_param->need_update_to_file = TRUE;
							}
						}
						break;
#endif

					default:
						CAM_ERR(CAM_HWB, "[NON][AF] Unsupport\n");
						break;
					}
				}
			}
#endif
		} else {
			CAM_DBG(CAM_ACTUATOR,
				"Success:request ID: %d",
				i2c_set->request_id);
		}
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
		if (a_ctrl->soc_info.index != 1) { //not apply on front case
			size = i2c_list->i2c_settings.size;
			for (i = 0; i < size; i++) {
				if (i2c_list->i2c_settings.reg_setting[i].reg_addr == 0x00) {
					position = i2c_list->i2c_settings.reg_setting[i].reg_data >> 7; //using word data
					CAM_DBG(CAM_ACTUATOR, "Position : %d\n", position);
					break;
				}
			}

			if (g_o_ctrl != NULL) {
				mutex_lock(&(g_o_ctrl->ois_mutex));
				if (position >= 0 && position < 512){
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
					// 1bit right shift af position, because OIS use 8bit af position
					cam_ois_shift_calibration(g_o_ctrl, (position >> 1), a_ctrl->soc_info.index);
#else
					// Rumba OIS uses 9bit af position
					cam_ois_shift_calibration(g_o_ctrl, position, a_ctrl->soc_info.index);
#endif
				}
				else
					CAM_ERR(CAM_ACTUATOR, "Position is invalid %d \n", position);
				mutex_unlock(&(g_o_ctrl->ois_mutex));
			}
		}
#endif
	}

	return rc;
}

int32_t cam_actuator_apply_request(struct cam_req_mgr_apply_request *apply)
{
	int32_t rc = 0, request_id, del_req_id;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;

	if (!apply) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Input Args");
		return -EINVAL;
	}

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(apply->dev_hdl);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}
	request_id = apply->request_id % MAX_PER_FRAME_ARRAY;

	trace_cam_apply_req("Actuator", apply->request_id);

	CAM_DBG(CAM_ACTUATOR, "Request Id: %lld", apply->request_id);
	mutex_lock(&(a_ctrl->actuator_mutex));
	if ((apply->request_id ==
		a_ctrl->i2c_data.per_frame[request_id].request_id) &&
		(a_ctrl->i2c_data.per_frame[request_id].is_settings_valid)
		== 1) {
		rc = cam_actuator_apply_settings(a_ctrl,
			&a_ctrl->i2c_data.per_frame[request_id]);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in applying the request: %lld\n",
				apply->request_id);
			goto release_mutex;
		}
	}
	del_req_id = (request_id +
		MAX_PER_FRAME_ARRAY - MAX_SYSTEM_PIPELINE_DELAY) %
		MAX_PER_FRAME_ARRAY;

	if (apply->request_id >
		a_ctrl->i2c_data.per_frame[del_req_id].request_id) {
		a_ctrl->i2c_data.per_frame[del_req_id].request_id = 0;
		rc = delete_request(&a_ctrl->i2c_data.per_frame[del_req_id]);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Fail deleting the req: %d err: %d\n",
				del_req_id, rc);
			goto release_mutex;
		}
	} else {
		CAM_DBG(CAM_ACTUATOR, "No Valid Req to clean Up");
	}

release_mutex:
	mutex_unlock(&(a_ctrl->actuator_mutex));
	return rc;
}

int32_t cam_actuator_establish_link(
	struct cam_req_mgr_core_dev_link_setup *link)
{
	struct cam_actuator_ctrl_t *a_ctrl = NULL;

	if (!link) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(link->dev_hdl);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}

	mutex_lock(&(a_ctrl->actuator_mutex));
	if (link->link_enable) {
		a_ctrl->bridge_intf.link_hdl = link->link_hdl;
		a_ctrl->bridge_intf.crm_cb = link->crm_cb;
	} else {
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.crm_cb = NULL;
	}
	mutex_unlock(&(a_ctrl->actuator_mutex));

	return 0;
}

static void cam_actuator_update_req_mgr(
	struct cam_actuator_ctrl_t *a_ctrl,
	struct cam_packet *csl_packet)
{
	struct cam_req_mgr_add_request add_req;

	add_req.link_hdl = a_ctrl->bridge_intf.link_hdl;
	add_req.req_id = csl_packet->header.request_id;
	add_req.dev_hdl = a_ctrl->bridge_intf.device_hdl;
	add_req.skip_before_applying = 0;

	if (a_ctrl->bridge_intf.crm_cb &&
		a_ctrl->bridge_intf.crm_cb->add_req) {
		a_ctrl->bridge_intf.crm_cb->add_req(&add_req);
		CAM_DBG(CAM_ACTUATOR, "Request Id: %lld added to CRM",
			add_req.req_id);
	} else {
		CAM_ERR(CAM_ACTUATOR, "Can't add Request ID: %lld to CRM",
			csl_packet->header.request_id);
	}
}

int32_t cam_actuator_publish_dev_info(struct cam_req_mgr_device_info *info)
{
	if (!info) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	info->dev_id = CAM_REQ_MGR_DEVICE_ACTUATOR;
	strlcpy(info->name, CAM_ACTUATOR_NAME, sizeof(info->name));
	info->p_delay = 1;
	info->trigger = CAM_TRIGGER_POINT_SOF;

	return 0;
}

int32_t cam_actuator_i2c_pkt_parse(struct cam_actuator_ctrl_t *a_ctrl,
	void *arg)
{
	int32_t  rc = 0;
	int32_t  i = 0;
	uint32_t total_cmd_buf_in_bytes = 0;
	size_t   len_of_buff = 0;
	uint32_t *offset = NULL;
	uint32_t *cmd_buf = NULL;
	uintptr_t generic_ptr;
	int32_t  SLEEP_MS = 10;
	struct common_header      *cmm_hdr = NULL;
	struct cam_control        *ioctl_ctrl = NULL;
	struct cam_packet         *csl_packet = NULL;
	struct cam_config_dev_cmd config;
	struct i2c_data_settings  *i2c_data = NULL;
	struct i2c_settings_array *i2c_reg_settings = NULL;
	struct cam_cmd_buf_desc   *cmd_desc = NULL;
	struct cam_actuator_soc_private *soc_private = NULL;
	struct cam_sensor_power_ctrl_t  *power_info = NULL;
#if defined(CONFIG_SAMSUNG_ACTUATOR_DW9808)// Actuator DW9808 specific changes
	static int32_t dw9808_init_apply_setting;
#endif

	if (!a_ctrl || !arg) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;

	power_info = &soc_private->power_info;

	ioctl_ctrl = (struct cam_control *)arg;
	if (copy_from_user(&config,
		u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(config)))
		return -EFAULT;
	rc = cam_mem_get_cpu_buf(config.packet_handle,
		&generic_ptr, &len_of_buff);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "Error in converting command Handle %d",
			rc);
		return rc;
	}

	if (config.offset > len_of_buff) {
		CAM_ERR(CAM_ACTUATOR,
			"offset is out of bounds: offset: %lld len: %zu",
			config.offset, len_of_buff);
		return -EINVAL;
	}

	csl_packet =
		(struct cam_packet *)(generic_ptr + (uint32_t)config.offset);
	CAM_DBG(CAM_ACTUATOR, "Pkt opcode: %d", csl_packet->header.op_code);

	if ((csl_packet->header.op_code & 0xFFFFFF) !=
		CAM_ACTUATOR_PACKET_OPCODE_INIT &&
		csl_packet->header.request_id <= a_ctrl->last_flush_req
		&& a_ctrl->last_flush_req != 0) {
		CAM_DBG(CAM_ACTUATOR,
			"reject request %lld, last request to flush %lld",
			csl_packet->header.request_id, a_ctrl->last_flush_req);
		return -EINVAL;
	}

	if (csl_packet->header.request_id > a_ctrl->last_flush_req)
		a_ctrl->last_flush_req = 0;

	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_ACTUATOR_PACKET_OPCODE_INIT:
		offset = (uint32_t *)&csl_packet->payload;
		offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);

		/* Loop through multiple command buffers */
		for (i = 0; i < csl_packet->num_cmd_buf; i++) {
			total_cmd_buf_in_bytes = cmd_desc[i].length;
			if (!total_cmd_buf_in_bytes)
				continue;
			rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
					&generic_ptr, &len_of_buff);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR, "Failed to get cpu buf");
				return rc;
			}
			cmd_buf = (uint32_t *)generic_ptr;
			if (!cmd_buf) {
				CAM_ERR(CAM_ACTUATOR, "invalid cmd buf");
				return -EINVAL;
			}
			cmd_buf += cmd_desc[i].offset / sizeof(uint32_t);
			cmm_hdr = (struct common_header *)cmd_buf;

			switch (cmm_hdr->cmd_type) {
			case CAMERA_SENSOR_CMD_TYPE_I2C_INFO:
				CAM_DBG(CAM_ACTUATOR,
					"Received slave info buffer");
				rc = cam_actuator_slaveInfo_pkt_parser(
					a_ctrl, cmd_buf);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
					"Failed to parse slave info: %d", rc);
					return rc;
				}
				break;
			case CAMERA_SENSOR_CMD_TYPE_PWR_UP:
			case CAMERA_SENSOR_CMD_TYPE_PWR_DOWN:
				CAM_DBG(CAM_ACTUATOR,
					"Received power settings buffer");
				rc = cam_sensor_update_power_settings(
					cmd_buf,
					total_cmd_buf_in_bytes,
					power_info);
				if (rc) {
					CAM_ERR(CAM_ACTUATOR,
					"Failed:parse power settings: %d",
					rc);
					return rc;
				}
				break;
			default:
				CAM_DBG(CAM_ACTUATOR,
					"Received initSettings buffer");
				i2c_data = &(a_ctrl->i2c_data);
				i2c_reg_settings =
					&i2c_data->init_settings;

				i2c_reg_settings->request_id = 0;
				i2c_reg_settings->is_settings_valid = 1;
				rc = cam_sensor_i2c_command_parser(
					&a_ctrl->io_master_info,
					i2c_reg_settings,
					&cmd_desc[i], 1);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
					"Failed:parse init settings: %d",
					rc);
					return rc;
				}
				break;
			}
		}

		if (a_ctrl->cam_act_state == CAM_ACTUATOR_ACQUIRE) {
			rc = cam_actuator_power_up(a_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					" Actuator Power up failed");
				return rc;
			}
			a_ctrl->cam_act_state = CAM_ACTUATOR_CONFIG;
#if defined(CONFIG_SAMSUNG_ACTUATOR_DW9808)// Actuator DW9808 specific changes
			dw9808_init_apply_setting = 1;
#endif
		}
		/* Actuator i2c can start at least 10 ms after power on according to spec. */
		if (SLEEP_MS > 20) // due to deviation of msleep
			msleep(SLEEP_MS);
		else if (SLEEP_MS)
			usleep_range(SLEEP_MS * 1000, (SLEEP_MS * 1000) + 1000);

#if defined(CONFIG_SAMSUNG_ACTUATOR_DW9808)// Actuator DW9808 specific changes
		if(dw9808_init_apply_setting) {
			rc = cam_actuator_init(a_ctrl,
				&a_ctrl->i2c_data.init_settings);
			dw9808_init_apply_setting = 0;
		} else {
			rc = cam_actuator_apply_settings(a_ctrl,
				&a_ctrl->i2c_data.init_settings);
		}
#else
		rc = cam_actuator_apply_settings(a_ctrl,
			&a_ctrl->i2c_data.init_settings);
#endif

		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "Cannot apply Init settings");
			return rc;
		}

		/* Delete the request even if the apply is failed */
		rc = delete_request(&a_ctrl->i2c_data.init_settings);
		if (rc < 0) {
			CAM_WARN(CAM_ACTUATOR,
				"Fail in deleting the Init settings");
			rc = 0;
		}
		break;
	case CAM_ACTUATOR_PACKET_AUTO_MOVE_LENS:
		if (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Not in right state to move lens: %d",
				a_ctrl->cam_act_state);
			return rc;
		}
		a_ctrl->setting_apply_state = ACT_APPLY_SETTINGS_NOW;

		i2c_data = &(a_ctrl->i2c_data);
		i2c_reg_settings = &i2c_data->init_settings;

		i2c_data->init_settings.request_id =
			csl_packet->header.request_id;
		i2c_reg_settings->is_settings_valid = 1;
		offset = (uint32_t *)&csl_packet->payload;
		offset += csl_packet->cmd_buf_offset / sizeof(uint32_t);
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_sensor_i2c_command_parser(
			&a_ctrl->io_master_info,
			i2c_reg_settings,
			cmd_desc, 1);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Auto move lens parsing failed: %d", rc);
			return rc;
		}
		cam_actuator_update_req_mgr(a_ctrl, csl_packet);
		break;
	case CAM_ACTUATOR_PACKET_MANUAL_MOVE_LENS:
		if (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Not in right state to move lens: %d",
				a_ctrl->cam_act_state);
			return rc;
		}

		a_ctrl->setting_apply_state = ACT_APPLY_SETTINGS_LATER;
		i2c_data = &(a_ctrl->i2c_data);
		i2c_reg_settings = &i2c_data->per_frame[
			csl_packet->header.request_id % MAX_PER_FRAME_ARRAY];

		 i2c_reg_settings->request_id =
			csl_packet->header.request_id;
		i2c_reg_settings->is_settings_valid = 1;
		offset = (uint32_t *)&csl_packet->payload;
		offset += csl_packet->cmd_buf_offset / sizeof(uint32_t);
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_sensor_i2c_command_parser(
			&a_ctrl->io_master_info,
			i2c_reg_settings,
			cmd_desc, 1);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Manual move lens parsing failed: %d", rc);
			return rc;
		}

		cam_actuator_update_req_mgr(a_ctrl, csl_packet);
		break;
	case CAM_PKT_NOP_OPCODE:
		if (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG) {
			CAM_WARN(CAM_ACTUATOR,
				"Received NOP packets in invalid state: %d",
				a_ctrl->cam_act_state);
			return -EINVAL;
		}

		cam_actuator_update_req_mgr(a_ctrl, csl_packet);
		break;
	}

	return rc;
}

void cam_actuator_shutdown(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	struct cam_actuator_soc_private  *soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info =
		&soc_private->power_info;

	if (a_ctrl->cam_act_state == CAM_ACTUATOR_INIT)
		return;

	if (a_ctrl->cam_act_state >= CAM_ACTUATOR_CONFIG) {
		rc = cam_actuator_power_down(a_ctrl);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "Actuator Power down failed");
		a_ctrl->cam_act_state = CAM_ACTUATOR_ACQUIRE;
	}

	if (a_ctrl->cam_act_state >= CAM_ACTUATOR_ACQUIRE) {
		rc = cam_destroy_device_hdl(a_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "destroying  dhdl failed");
		a_ctrl->bridge_intf.device_hdl = -1;
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.session_hdl = -1;
	}

	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	power_info->power_setting_size = 0;
	power_info->power_down_setting_size = 0;

	a_ctrl->cam_act_state = CAM_ACTUATOR_INIT;
}

int32_t cam_actuator_driver_cmd(struct cam_actuator_ctrl_t *a_ctrl,
	void *arg)
{
	int rc = 0;
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_actuator_soc_private *soc_private = NULL;
	struct cam_sensor_power_ctrl_t  *power_info = NULL;

	if (!a_ctrl || !cmd) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;

	power_info = &soc_private->power_info;

	if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_ACTUATOR, "Invalid handle type: %d",
			cmd->handle_type);
		return -EINVAL;
	}

	CAM_DBG(CAM_ACTUATOR, "Opcode to Actuator: %d", cmd->op_code);

	mutex_lock(&(a_ctrl->actuator_mutex));
	switch (cmd->op_code) {
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev actuator_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		CAM_INFO(CAM_ACTUATOR, "CAM_ACQUIRE_DEV");
		if (a_ctrl->bridge_intf.device_hdl != -1) {
			CAM_ERR(CAM_ACTUATOR, "Device is already acquired");
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = copy_from_user(&actuator_acq_dev,
			u64_to_user_ptr(cmd->handle),
			sizeof(actuator_acq_dev));
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "Failed Copying from user\n");
			goto release_mutex;
		}

		bridge_params.session_hdl = actuator_acq_dev.session_handle;
		bridge_params.ops = &a_ctrl->bridge_intf.ops;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = a_ctrl;

		bridge_params.dev_id = CAM_ACTUATOR;

		actuator_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		a_ctrl->bridge_intf.device_hdl = actuator_acq_dev.device_handle;
		a_ctrl->bridge_intf.session_hdl =
			actuator_acq_dev.session_handle;

		CAM_DBG(CAM_ACTUATOR, "Device Handle: %d",
			actuator_acq_dev.device_handle);
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&actuator_acq_dev,
			sizeof(struct cam_sensor_acquire_dev))) {
			CAM_ERR(CAM_ACTUATOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}

		a_ctrl->cam_act_state = CAM_ACTUATOR_ACQUIRE;
	}
		break;
	case CAM_RELEASE_DEV: {
		CAM_INFO(CAM_ACTUATOR, "CAM_RELEASE_DEV");
		if (a_ctrl->cam_act_state == CAM_ACTUATOR_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Cant release actuator: in start state");
			goto release_mutex;
		}

		if (a_ctrl->cam_act_state == CAM_ACTUATOR_CONFIG) {
			rc = cam_actuator_power_down(a_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"Actuator Power down failed");
				goto release_mutex;
			}
		}

		if (a_ctrl->bridge_intf.device_hdl == -1) {
			CAM_ERR(CAM_ACTUATOR, "link hdl: %d device hdl: %d",
				a_ctrl->bridge_intf.device_hdl,
				a_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}

		if (a_ctrl->bridge_intf.link_hdl != -1) {
			CAM_ERR(CAM_ACTUATOR,
				"Device [%d] still active on link 0x%x",
				a_ctrl->cam_act_state,
				a_ctrl->bridge_intf.link_hdl);
			rc = -EAGAIN;
			goto release_mutex;
		}

		rc = cam_destroy_device_hdl(a_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "destroying the device hdl");
		a_ctrl->bridge_intf.device_hdl = -1;
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.session_hdl = -1;
		a_ctrl->cam_act_state = CAM_ACTUATOR_INIT;
		a_ctrl->last_flush_req = 0;
		kfree(power_info->power_setting);
		kfree(power_info->power_down_setting);
		power_info->power_setting = NULL;
		power_info->power_down_setting = NULL;
		power_info->power_down_setting_size = 0;
		power_info->power_setting_size = 0;
	}
		break;
	case CAM_QUERY_CAP: {
		struct cam_actuator_query_cap actuator_cap = {0};

		actuator_cap.slot_info = a_ctrl->soc_info.index;
		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&actuator_cap,
			sizeof(struct cam_actuator_query_cap))) {
			CAM_ERR(CAM_ACTUATOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
	}
		break;
	case CAM_START_DEV: {
		CAM_INFO(CAM_ACTUATOR, "CAM_START_DEV");
		if (a_ctrl->cam_act_state != CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
			"Not in right state to start : %d",
			a_ctrl->cam_act_state);
			goto release_mutex;
		}
#if defined(CONFIG_SAMSUNG_ACTUATOR_AK7377)
		if (a_ctrl->soc_info.index == CAMERA_2) { // Apply only to TELE
			cam_actuator_to_wakeup(a_ctrl);
		}
#endif
		a_ctrl->cam_act_state = CAM_ACTUATOR_START;
		a_ctrl->last_flush_req = 0;
	}
		break;
	case CAM_STOP_DEV: {
		struct i2c_settings_array *i2c_set = NULL;
		int i;
		CAM_INFO(CAM_ACTUATOR, "CAM_STOP_DEV");
		if (a_ctrl->cam_act_state != CAM_ACTUATOR_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
			"Not in right state to stop : %d",
			a_ctrl->cam_act_state);
			goto release_mutex;
		}
#if defined(CONFIG_SAMSUNG_ACTUATOR_AK7377)
		if (a_ctrl->soc_info.index == CAMERA_2) { // Apply only to TELE
			cam_actuator_to_sleep(a_ctrl);
		}
#endif
		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			i2c_set = &(a_ctrl->i2c_data.per_frame[i]);

			if (i2c_set->is_settings_valid == 1) {
				rc = delete_request(i2c_set);
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"delete request: %lld rc: %d",
						i2c_set->request_id, rc);
			}
		}
		a_ctrl->last_flush_req = 0;
		a_ctrl->cam_act_state = CAM_ACTUATOR_CONFIG;
	}
		break;
	case CAM_CONFIG_DEV: {
		CAM_DBG(CAM_ACTUATOR, "CAM_CONFIG_DEV");
		a_ctrl->setting_apply_state =
			ACT_APPLY_SETTINGS_LATER;
		rc = cam_actuator_i2c_pkt_parse(a_ctrl, arg);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "Failed in actuator Parsing");
			goto release_mutex;
		}

		if (a_ctrl->setting_apply_state ==
			ACT_APPLY_SETTINGS_NOW) {
			rc = cam_actuator_apply_settings(a_ctrl,
				&a_ctrl->i2c_data.init_settings);
			if (rc < 0)
				CAM_ERR(CAM_ACTUATOR,
					"Cannot apply Update settings");

			/* Delete the request even if the apply is failed */
			rc = delete_request(&a_ctrl->i2c_data.init_settings);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"Failed in Deleting the Init Pkt: %d",
					rc);
				goto release_mutex;
			}
		}
	}
		break;
	default:
		CAM_ERR(CAM_ACTUATOR, "Invalid Opcode %d", cmd->op_code);
	}

release_mutex:
	mutex_unlock(&(a_ctrl->actuator_mutex));

	return rc;
}

int32_t cam_actuator_flush_request(struct cam_req_mgr_flush_request *flush_req)
{
	int32_t rc = 0, i;
	uint32_t cancel_req_id_found = 0;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;
	struct i2c_settings_array *i2c_set = NULL;

	if (!flush_req)
		return -EINVAL;

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(flush_req->dev_hdl);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}

	if (a_ctrl->i2c_data.per_frame == NULL) {
		CAM_ERR(CAM_ACTUATOR, "i2c frame data is NULL");
		return -EINVAL;
	}

	mutex_lock(&(a_ctrl->actuator_mutex));
	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_ALL) {
		a_ctrl->last_flush_req = flush_req->req_id;
		CAM_DBG(CAM_ACTUATOR, "last reqest to flush is %lld",
			flush_req->req_id);
	}

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		i2c_set = &(a_ctrl->i2c_data.per_frame[i]);

		if ((flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ)
				&& (i2c_set->request_id != flush_req->req_id))
			continue;

		if (i2c_set->is_settings_valid == 1) {
			rc = delete_request(i2c_set);
			if (rc < 0)
				CAM_ERR(CAM_ACTUATOR,
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
		CAM_DBG(CAM_ACTUATOR,
			"Flush request id:%lld not found in the pending list",
			flush_req->req_id);
	mutex_unlock(&(a_ctrl->actuator_mutex));
	return rc;
}

/* Actuator DW9808 specific changes */
#if defined(CONFIG_SAMSUNG_ACTUATOR_DW9808)
int16_t cam_actuator_init(struct cam_actuator_ctrl_t *a_ctrl,
	struct i2c_settings_array *i2c_set)
{
	struct cam_sensor_i2c_reg_setting reg_setting;
	int rc = 0;
	int size = 0;
	CAM_INFO(CAM_ACTUATOR, " Entry ");

	if (a_ctrl == NULL || i2c_set == NULL ) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	if (i2c_set->is_settings_valid != 1) {
		CAM_ERR(CAM_ACTUATOR, " Invalid settings");
		return -EINVAL;
	}

	memset(&reg_setting, 0, sizeof(reg_setting));

	reg_setting.reg_setting = kmalloc(sizeof(struct cam_sensor_i2c_reg_array) * 4, GFP_KERNEL);
	if (!reg_setting.reg_setting) {
		return -ENOMEM;
	}
	memset(reg_setting.reg_setting, 0, sizeof(struct cam_sensor_i2c_reg_array));

	reg_setting.reg_setting[size].reg_addr = 0x02;
	reg_setting.reg_setting[size].reg_data = 0x01;
	size++;

	reg_setting.reg_setting[size].reg_addr = 0x02;
	reg_setting.reg_setting[size].reg_data = 0x00;
	size++;

	reg_setting.size = size;
	reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;

	rc = camera_io_dev_write(&a_ctrl->io_master_info, &reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write 0x01 I2C settings: %d",
			rc);
	usleep_range(5000, 5000);

	memset(reg_setting.reg_setting, 0, sizeof(struct cam_sensor_i2c_reg_array));
	size = 0;
	rc = 0;

	reg_setting.reg_setting[size].reg_addr = 0x06;
	reg_setting.reg_setting[size].reg_data = 0x611F;
	size++;

	reg_setting.size = size;
	reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	rc = camera_io_dev_write(&a_ctrl->io_master_info, &reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write 0x611F I2C settings: %d",
			rc);
	usleep_range(1000, 1000);

	memset(reg_setting.reg_setting, 0, sizeof(struct cam_sensor_i2c_reg_array));
	size = 0;
	rc = 0;

	reg_setting.reg_setting[size].reg_addr = 0x02;
	reg_setting.reg_setting[size].reg_data = 0x02;
	size++;

	reg_setting.size = size;
	reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	rc = camera_io_dev_write(&a_ctrl->io_master_info, &reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write 0x00FA I2C settings: %d",
			rc);
	usleep_range(1000, 1000);

	memset(reg_setting.reg_setting, 0, sizeof(struct cam_sensor_i2c_reg_array));
	size = 0;
	rc = 0;

	reg_setting.reg_setting[size].reg_addr = 0x03;
	reg_setting.reg_setting[size].reg_data = 0x012B;
	size++;

	reg_setting.size = size;
	reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	rc = camera_io_dev_write(&a_ctrl->io_master_info, &reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write 0x012B I2C settings: %d",
			rc);
	usleep_range(10000, 11000);


	if (reg_setting.reg_setting) {
		kfree(reg_setting.reg_setting);
		reg_setting.reg_setting = NULL;
	}
	CAM_INFO(CAM_ACTUATOR, " Exit ");

	return rc;
}
#endif

#if defined(CONFIG_SAMSUNG_ACTUATOR_AK7377)
/***** To support soft landing AK7377 *****/
int16_t cam_actuator_to_sleep(struct cam_actuator_ctrl_t *a_ctrl)
{
	struct cam_sensor_i2c_reg_setting reg_setting;
	int rc = 0;
	int size = 0;
	CAM_INFO(CAM_ACTUATOR, " Entry ");

	memset(&reg_setting, 0, sizeof(reg_setting));
	if (a_ctrl == NULL) {
		CAM_ERR(CAM_ACTUATOR, "failed. a_ctrl is NULL");
		return -EINVAL;
	}

	reg_setting.reg_setting = kmalloc(sizeof(struct cam_sensor_i2c_reg_array) * 4, GFP_KERNEL);
	if (!reg_setting.reg_setting) {
		return -ENOMEM;
	}
	memset(reg_setting.reg_setting, 0, sizeof(struct cam_sensor_i2c_reg_array));

	/* SET Position - 0x3200 */
	reg_setting.reg_setting[size].reg_addr = 0x00;
	reg_setting.reg_setting[size].reg_data = 0x3200;
	size++;

	reg_setting.size = size;
	reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;

	rc = camera_io_dev_write(&a_ctrl->io_master_info, &reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write 0x3200 I2C settings: %d",
			rc);
	msleep(20);

	memset(reg_setting.reg_setting, 0, sizeof(struct cam_sensor_i2c_reg_array));
	size = 0;
	rc = 0;

	/* SET Position - 0x0000 */
	reg_setting.reg_setting[size].reg_addr = 0x00;
	reg_setting.reg_setting[size].reg_data = 0x0000;
	size++;

	reg_setting.size = size;
	reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;

	rc = camera_io_dev_write(&a_ctrl->io_master_info, &reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write 0x0000 I2C settings: %d",
			rc);
	msleep(40);

	memset(reg_setting.reg_setting, 0, sizeof(struct cam_sensor_i2c_reg_array));
	size = 0;
	rc = 0;

	/* SET Standby Mode */
	reg_setting.reg_setting[size].reg_addr = 0x02;
	reg_setting.reg_setting[size].reg_data = 0x40;
	size++;


	reg_setting.size = size;
	reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	rc = camera_io_dev_write(&a_ctrl->io_master_info, &reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write 0x40 I2C settings: %d",
			rc);
	usleep_range(1000, 1000);
	memset(reg_setting.reg_setting, 0, sizeof(struct cam_sensor_i2c_reg_array));
	size = 0;
	rc = 0;

	/* SET Sleep Mode */
	reg_setting.reg_setting[size].reg_addr = 0x02;
	reg_setting.reg_setting[size].reg_data = 0x20;
	size++;

	reg_setting.size = size;
	reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;

	rc = camera_io_dev_write(&a_ctrl->io_master_info,
		&reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write 0x20 I2C settings: %d",
			rc);

	if (reg_setting.reg_setting) {
		kfree(reg_setting.reg_setting);
		reg_setting.reg_setting = NULL;
	}
	CAM_INFO(CAM_ACTUATOR, " Exit ");

	return rc;
}



/***** To wake up actuator after soft landing AK7377 *****/
int16_t cam_actuator_to_wakeup(struct cam_actuator_ctrl_t *a_ctrl)
{
	struct cam_sensor_i2c_reg_setting reg_setting;
	int rc = 0;
	int size = 0;

	memset(&reg_setting, 0, sizeof(reg_setting));
	if (a_ctrl == NULL) {
		CAM_ERR(CAM_ACTUATOR, "failed. a_ctrl is NULL");
		return -EINVAL;
	}

	reg_setting.reg_setting = kmalloc(sizeof(struct cam_sensor_i2c_reg_array) * 4, GFP_KERNEL);
	if (!reg_setting.reg_setting) {
		return -ENOMEM;
	}
	memset(reg_setting.reg_setting, 0, sizeof(struct cam_sensor_i2c_reg_array));

	/* SET Standby Mode */
	reg_setting.reg_setting[size].reg_addr = 0x02;
	reg_setting.reg_setting[size].reg_data = 0x40;
	size++;

	reg_setting.size = size;
	reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	rc = camera_io_dev_write(&a_ctrl->io_master_info, &reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write 0x40 I2C settings: %d",
			rc);
	usleep_range(1000, 1000);

	memset(reg_setting.reg_setting, 0, sizeof(struct cam_sensor_i2c_reg_array));
	size = 0;
	rc = 0;

	/* SET Active Mode */
	reg_setting.reg_setting[size].reg_addr = 0x02;
	reg_setting.reg_setting[size].reg_data = 0x00;
	size++;

	reg_setting.size = size;
	reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;

	rc = camera_io_dev_write(&a_ctrl->io_master_info,
		&reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write 0x00 I2C settings: %d",
			rc);

	if (reg_setting.reg_setting) {
		kfree(reg_setting.reg_setting);
		reg_setting.reg_setting = NULL;
	}
	return rc;
}
#endif

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
/***** for only ois selftest , set the actuator initial position to 256 *****/
int16_t cam_actuator_move_for_ois_test(struct cam_actuator_ctrl_t *a_ctrl)
{
	struct cam_sensor_i2c_reg_setting reg_setting;
	int rc = 0;
	int size = 0;

	memset(&reg_setting, 0, sizeof(reg_setting));
	if (a_ctrl == NULL) {
		CAM_ERR(CAM_ACTUATOR, "failed. a_ctrl is NULL");
		return -EINVAL;
	}

	reg_setting.reg_setting = kmalloc(sizeof(struct cam_sensor_i2c_reg_array) * 4, GFP_KERNEL);
	if (!reg_setting.reg_setting) {
		return -ENOMEM;
	}
	memset(reg_setting.reg_setting, 0, sizeof(struct cam_sensor_i2c_reg_array));

#if defined(CONFIG_SEC_D2XQ_PROJECT) || defined(CONFIG_SEC_D2Q_PROJECT) || defined(CONFIG_SEC_D1Q_PROJECT)\
	|| defined(CONFIG_SEC_D2XQ2_PROJECT)
	/* Init setting for ak7377 */
	/* SET Standby Mode */
	reg_setting.reg_setting[size].reg_addr = 0x02;
	reg_setting.reg_setting[size].reg_data = 0x40;
	reg_setting.reg_setting[size].delay = 2000;
	size++;
#endif

	/* SET Position MSB - 0x00 */
	reg_setting.reg_setting[size].reg_addr = 0x00;
	reg_setting.reg_setting[size].reg_data = 0x80;
	size++;

	/* SET Position LSB - 0x00 */
	reg_setting.reg_setting[size].reg_addr = 0x01;
	reg_setting.reg_setting[size].reg_data = 0x00;
	size++;

	/* SET Active Mode */
	reg_setting.reg_setting[size].reg_addr = 0x02;
	reg_setting.reg_setting[size].reg_data = 0x00;
	size++;

	reg_setting.size = size;
	reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;

	rc = camera_io_dev_write(&a_ctrl->io_master_info,
		&reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write I2C settings: %d",
			rc);

	if (reg_setting.reg_setting) {
		kfree(reg_setting.reg_setting);
		reg_setting.reg_setting = NULL;
	}

	return rc;
}
#endif

#if defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
/***** for only ois selftest , set the actuator initial position to 256 *****/
int16_t cam_actuator_move_for_ois_test(struct cam_actuator_ctrl_t *a_ctrl)
{
	struct cam_sensor_i2c_reg_setting reg_setting;
	struct cam_sensor_i2c_reg_array reg_arr;
	int rc = 0;

	memset(&reg_setting, 0, sizeof(reg_setting));
	memset(&reg_arr, 0, sizeof(reg_arr));
	if (a_ctrl == NULL) {
		CAM_ERR(CAM_ACTUATOR, "failed. a_ctrl is NULL");
		return -EINVAL;
	}

	reg_setting.size = 1;
	reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	reg_setting.reg_setting = &reg_arr;

	/* SET Position MSB - 0x00 */
	reg_arr.reg_addr = 0x00;
	reg_arr.reg_data = 0x80;
	rc = camera_io_dev_write(&a_ctrl->io_master_info,
		&reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write I2C settings: %d",
			rc);

	/* SET Position LSB - 0x00 */
	reg_arr.reg_addr = 0x01;
	reg_arr.reg_data = 0x00;
	rc = camera_io_dev_write(&a_ctrl->io_master_info,
		&reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write I2C settings: %d",
			rc);

	/* SET Active Mode */
	reg_arr.reg_addr = 0x02;
	reg_arr.reg_data = 0x00;
	rc = camera_io_dev_write(&a_ctrl->io_master_info,
		&reg_setting);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR,
			"Failed to random write I2C settings: %d",
			rc);

	return rc;
}
#endif