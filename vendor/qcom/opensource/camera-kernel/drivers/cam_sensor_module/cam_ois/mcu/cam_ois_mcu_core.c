// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include "cam_ois_core.h"
#include "cam_ois_soc.h"
#include "cam_ois_mcu_stm32g.h"
#include "cam_ois_mcu_thread.h"
#include "cam_ois_mcu_core.h"

int cam_ois_mcu_apply_settings(struct cam_ois_ctrl_t *o_ctrl, struct i2c_settings_list *i2c_list)
{
	int i = 0, rc = 0;
	uint32_t size = 0;

	if ((i2c_list->i2c_settings.size == 1) &&
		(i2c_list->i2c_settings.addr_type == CAMERA_SENSOR_I2C_TYPE_INVALID) &&
		(i2c_list->i2c_settings.data_type == CAMERA_SENSOR_I2C_TYPE_INVALID))
		return 1;

	size = i2c_list->i2c_settings.size;
	for (i = 0; i < size; i++) {
		if (i2c_list->i2c_settings.reg_setting[i].reg_addr == 0xBE) {
			CAM_INFO(CAM_OIS, "set ois driver output seletect (0x%x = 0x%x)", i2c_list->i2c_settings.reg_setting[i].reg_addr, i2c_list->i2c_settings.reg_setting[i].reg_data);
		}

		if (i2c_list->i2c_settings.reg_setting[i].reg_addr == 0x02) {
			rc = cam_ois_set_ois_mode(o_ctrl,
				i2c_list->i2c_settings.reg_setting[i].reg_data);
			return rc;
		}
	}

    return rc;
}

int cam_ois_mcu_get_dev_handle(struct cam_ois_ctrl_t *o_ctrl)
{
	int i = 0, idx = -1;

	if (o_ctrl->bridge_cnt >= MAX_BRIDGE_COUNT) {
		CAM_ERR(CAM_OIS, "Device is already max acquired");
		return -EFAULT;
	}

	for (i = 0; i < MAX_BRIDGE_COUNT; i++) {
		if (o_ctrl->bridge_intf[i].device_hdl == -1) {
			idx = i;
			break;
		}
	}

	if (idx == -1) {
		CAM_ERR(CAM_OIS, "All Device(%d) is already acquired", o_ctrl->bridge_cnt);
		return -EFAULT;
	}

    return idx;
}

static int __cam_ois_mcu_release_dev_handle(struct cam_ois_ctrl_t *o_ctrl, void *arg)
{
	struct cam_control *cmd = (struct cam_control *)arg;
	struct cam_sensor_release_dev ois_rel_dev;
	int i = 0, rc = 0;

	if (!o_ctrl || !arg) {
		CAM_INFO(CAM_OIS, "Invalid argument");
		return -EINVAL;
	}

	if (copy_from_user(&ois_rel_dev, u64_to_user_ptr(cmd->handle),
		sizeof(struct cam_sensor_release_dev)))
		return -EFAULT;

	for (i = 0; i < MAX_BRIDGE_COUNT; i++) {
		if (o_ctrl->bridge_intf[i].device_hdl == -1)
			continue;

		if ((o_ctrl->bridge_intf[i].device_hdl == ois_rel_dev.device_handle) &&
			(o_ctrl->bridge_intf[i].session_hdl == ois_rel_dev.session_handle)) {
			CAM_INFO(CAM_OIS, "Release the device hdl %d", o_ctrl->bridge_intf[i].device_hdl);
			rc = cam_destroy_device_hdl(o_ctrl->bridge_intf[i].device_hdl);
			if (rc < 0)
				CAM_ERR(CAM_OIS, "fail destroying the device hdl");
			o_ctrl->bridge_intf[i].device_hdl = -1;
			o_ctrl->bridge_intf[i].link_hdl = -1;
			o_ctrl->bridge_intf[i].session_hdl = -1;

			if (o_ctrl->bridge_cnt > 0)
				o_ctrl->bridge_cnt--;
			break;
		}
	}

	return 0;
}

int cam_ois_mcu_release_dev_handle(struct cam_ois_ctrl_t *o_ctrl, void *arg)
{
    int rc = 0;

	rc = __cam_ois_mcu_release_dev_handle(o_ctrl, arg);
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "destroying the device hdl");
		return rc;
	}

	if (o_ctrl->bridge_cnt > 0)
		return -EAGAIN;

	cam_ois_thread_destroy(o_ctrl);
	o_ctrl->ois_mode = 0;

	if (o_ctrl->cam_ois_state == CAM_OIS_CONFIG) {
		rc = cam_ois_power_down(o_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_OIS, "OIS Power Down Failed");
            return rc;
		}
	}

    return rc;
}

int cam_ois_mcu_create_thread (struct cam_ois_ctrl_t *o_ctrl)
{
	struct cam_ois_thread_msg_t *msg = NULL;
    int rc = 0;

	o_ctrl->ois_mode = 0;

	rc = cam_ois_thread_create(o_ctrl);
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "Failed create OIS thread");
		return rc;
	}

	msg = kmalloc(sizeof(struct cam_ois_thread_msg_t), GFP_KERNEL);
	if (msg == NULL) {
		CAM_ERR(CAM_OIS, "Failed alloc memory for msg, Out of memory");
		return rc;
	}

	memset(msg, 0, sizeof(struct cam_ois_thread_msg_t));
	msg->msg_type = CAM_OIS_THREAD_MSG_START;
	rc = cam_ois_thread_add_msg(o_ctrl, msg);
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "Failed add msg to OIS thread");
		kfree(msg);
		return rc;
	}
	o_ctrl->is_config = true;

    return rc;
}

int cam_ois_mcu_power_down (struct cam_ois_ctrl_t *o_ctrl)
{
	int rc = 0;

	if (!o_ctrl->is_power_up)
		return -EFAULT;

	rc = cam_ois_set_servo_ctrl(o_ctrl, 0);
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "ois servo ctrl off failed");
	}

	msleep(10);

	o_ctrl->is_power_up = false;
	o_ctrl->is_servo_on = false;
	o_ctrl->is_config = false;

	return rc;
}

int cam_ois_mcu_pkt_parser (struct cam_ois_ctrl_t *o_ctrl,
                            struct i2c_settings_array *i2c_reg_settings,
                            struct cam_cmd_buf_desc *cmd_desc)
{
	int rc = 0;

	i2c_reg_settings->is_settings_valid = 1;
	i2c_reg_settings->request_id = 0;

	rc = cam_sensor_i2c_command_parser(
			&o_ctrl->io_master_info,
			i2c_reg_settings,
			cmd_desc, 1, NULL);

	return rc;
}

int cam_ois_mcu_add_msg_apply_settings (struct cam_ois_ctrl_t *o_ctrl,
                                        struct i2c_settings_array *i2c_reg_settings)
{
	struct cam_ois_thread_msg_t *msg = NULL;
	int rc = 0;

	msg = kmalloc(sizeof(struct cam_ois_thread_msg_t), GFP_KERNEL);
	if (msg == NULL) {
		CAM_ERR(CAM_OIS, "Failed alloc memory for msg, Out of memory");
		return -ENOMEM;
	}

	memset(msg, 0, sizeof(struct cam_ois_thread_msg_t));
	msg->i2c_reg_settings = i2c_reg_settings;
	msg->msg_type = CAM_OIS_THREAD_MSG_APPLY_SETTING;

	rc = cam_ois_thread_add_msg(o_ctrl, msg);
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "Failed add msg to OIS thread");
		kfree(msg);
	}

	return rc;
}

static int __cam_ois_mcu_shutdown (struct cam_ois_ctrl_t *o_ctrl, int bridge_index)
{
	int i = bridge_index;
	int rc = 0;

	if (o_ctrl->bridge_intf[i].device_hdl == -1)
		return -EFAULT;

	CAM_INFO(CAM_OIS, "Release the device hdl %d", o_ctrl->bridge_intf[i].device_hdl);

	rc = cam_destroy_device_hdl(o_ctrl->bridge_intf[i].device_hdl);
	if (rc < 0) {
		rc = 0;
		CAM_ERR(CAM_OIS, "fail destroying the device hdl");
	}

	o_ctrl->bridge_intf[i].device_hdl = -1;
	o_ctrl->bridge_intf[i].link_hdl = -1;
	o_ctrl->bridge_intf[i].session_hdl = -1;

	return rc;
}

int cam_ois_mcu_shutdown (struct cam_ois_ctrl_t *o_ctrl)
{
	int i = 0;
	int rc = 0;

	CAM_INFO(CAM_OIS, "cam_ois_shutdown");

	cam_ois_thread_destroy(o_ctrl);
	for (i = MAX_BRIDGE_COUNT - 1; i >= 0; i--) {
		rc = __cam_ois_mcu_shutdown(o_ctrl, i);
		if (rc < 0)
			continue;
	}

	o_ctrl->start_cnt = 0;
	o_ctrl->bridge_cnt = 0;

	return rc;
}

int cam_ois_mcu_start_dev (struct cam_ois_ctrl_t *o_ctrl)
{
	int rc = 0;

	o_ctrl->start_cnt++;

	if (o_ctrl->cam_ois_state != CAM_OIS_CONFIG) {
		CAM_WARN(CAM_OIS, "Not in right state for start : %d",
				o_ctrl->cam_ois_state);
		return -EINVAL;
	}

	o_ctrl->cam_ois_state = CAM_OIS_START;
	return rc;
}

int cam_ois_mcu_stop_dev (struct cam_ois_ctrl_t *o_ctrl)
{
	int rc = 0;

	if (o_ctrl->start_cnt > 0)
		o_ctrl->start_cnt--;

	if (o_ctrl->start_cnt != 0) {
		CAM_WARN(CAM_OIS,
			"Still device running : %d",
			o_ctrl->start_cnt);
		return -EFAULT;
	}

	if (o_ctrl->cam_ois_state != CAM_OIS_START) {
		CAM_WARN(CAM_OIS,
			"Not in right state for stop : %d",
			o_ctrl->cam_ois_state);
		return -EFAULT;
	}

	return rc;
}