/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "sr200pc20.h"
#include "sr200pc20_yuv.h"

#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_dt_util.h"


#define msm_sensor_driver_WRT_LIST(s_ctrl,A) \
					s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_conf_tbl( \
						s_ctrl->sensor_i2c_client, A, \
						ARRAY_SIZE(A), \
						MSM_CAMERA_I2C_BYTE_DATA); CDBG("REGSEQ ****** %s", #A)
static struct yuv_ctrl sr200pc20_ctrl;
static exif_data_t sr200pc20_exif;

static int32_t streamon = 0;
static int32_t recording = 0;
static int32_t resolution = MSM_SENSOR_RES_FULL;

int32_t sr200pc20_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
int32_t sr200pc20_sensor_native_control(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
//void sr200pc20_get_exif(struct msm_sensor_ctrl_t *s_ctrl);
//int32_t sr200pc20_get_exif_info(struct ioctl_native_cmd * exif_info);
//static int sr200pc20_exif_shutter_speed(struct msm_sensor_ctrl_t *s_ctrl);
//static int sr200pc20_exif_iso(struct msm_sensor_ctrl_t *s_ctrl);

int32_t sr200pc20_set_exposure_compensation(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("CAM-SETTING -- EV is %d", mode);
	switch (mode) {
	case CAMERA_EV_M4:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_M4);
		break;
	case CAMERA_EV_M3:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_M3);
		break;
	case CAMERA_EV_M2:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_M2);
		break;
	case CAMERA_EV_M1:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_M1);
		break;
	case CAMERA_EV_DEFAULT:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_default);
		break;
	case CAMERA_EV_P1:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_P1);
		break;
	case CAMERA_EV_P2:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_P2);
		break;
	case CAMERA_EV_P3:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_P3);
		break;
	case CAMERA_EV_P4:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_brightness_P4);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr200pc20_set_white_balance(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("CAM-SETTING -- WB is %d", mode);
	switch (mode) {
	case CAMERA_WHITE_BALANCE_OFF:
	case CAMERA_WHITE_BALANCE_AUTO:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_WB_Auto);
		break;
	case CAMERA_WHITE_BALANCE_INCANDESCENT:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_WB_Incandescent);
		break;
	case CAMERA_WHITE_BALANCE_FLUORESCENT:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_WB_Fluorescent);
		break;
	case CAMERA_WHITE_BALANCE_DAYLIGHT:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_WB_Daylight);
		break;
	case CAMERA_WHITE_BALANCE_CLOUDY_DAYLIGHT:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_WB_Cloudy);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

static int sr200pc20_exif_shutter_speed(struct msm_sensor_ctrl_t *s_ctrl)
{
	u16 read_value1 = 0;
	u16 read_value2 = 0;
	u16 read_value3 = 0;
	int OPCLK = 24000000;

	/* Exposure Time */
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write(s_ctrl->sensor_i2c_client, 0x03,0x20,MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_read(s_ctrl->sensor_i2c_client, 0x80,&read_value1,MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_read(s_ctrl->sensor_i2c_client, 0x81,&read_value2,MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_read(s_ctrl->sensor_i2c_client, 0x82,&read_value3,MSM_CAMERA_I2C_BYTE_DATA);

	sr200pc20_exif.shutterspeed = OPCLK / ((read_value1 << 19)
			+ (read_value2 << 11) + (read_value3 << 3));
	CDBG("Exposure time = %d\n", sr200pc20_exif.shutterspeed);

	return 0;
}

static int sr200pc20_exif_iso(struct msm_sensor_ctrl_t *s_ctrl)
{
	u16 read_value4 = 0;
	unsigned short gain_value;

	/* ISO*/
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write(s_ctrl->sensor_i2c_client, 0x03,0x20,MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_read(s_ctrl->sensor_i2c_client, 0xb0,&read_value4,MSM_CAMERA_I2C_BYTE_DATA);

	gain_value = (read_value4 / 32) * 1000 +500;
	if (gain_value < 1140)
		sr200pc20_exif.iso = 50;
	else if (gain_value < 2140)
		sr200pc20_exif.iso = 100;
	else if (gain_value < 2640)
		sr200pc20_exif.iso = 200;
	else if (gain_value < 7520)
		sr200pc20_exif.iso = 400;
	else
		sr200pc20_exif.iso = 800;

	CDBG("ISO = %d\n", sr200pc20_exif.iso);

	return 0;
}

int32_t sr200pc20_get_exif_info(struct ioctl_native_cmd * exif_info)
{
	exif_info->value_1 = 1;	// equals 1 to update the exif value in the user level.
	exif_info->value_2 = sr200pc20_exif.iso;
	exif_info->value_3 = sr200pc20_exif.shutterspeed;
	return 0;
}

void sr200pc20_get_exif(struct msm_sensor_ctrl_t *s_ctrl)
{
	CDBG("sr200pc20_get_exif: E");

	/*Exif data*/
	sr200pc20_exif_shutter_speed(s_ctrl);
	sr200pc20_exif_iso(s_ctrl);
	CDBG("exp_time : %d\niso_value : %d\n",sr200pc20_exif.shutterspeed, sr200pc20_exif.iso);
	return;
}

int32_t sr200pc20_set_resolution(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("CAM-SETTING-- resolution is %d", mode);
	switch (mode) {
	case MSM_SENSOR_RES_FULL:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Capture);
		//to get exif data
		sr200pc20_get_exif(s_ctrl);
		break;
	default:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_640x480_Preview);
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
	}
	return rc;
}

int32_t sr200pc20_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	pr_err("CAM-SETTING-- effect is %d", mode);
	switch (mode) {
	case CAMERA_EFFECT_OFF:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Effect_Normal);
		break;
	case CAMERA_EFFECT_MONO:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Effect_Gray);
		break;
	case CAMERA_EFFECT_NEGATIVE:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Effect_Negative);
		break;
	case CAMERA_EFFECT_SEPIA:
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Effect_Sepia);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
	}
	return rc;
}

int32_t sr200pc20_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	int32_t rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);

	pr_err("%s ENTER %d \n ",__func__,cdata->cfgtype);

	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		CDBG(" CFG_GET_SENSOR_INFO \n");
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++)
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];


		cdata->cfg.sensor_info.is_mount_angle_valid =
			s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
		cdata->cfg.sensor_info.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		cdata->cfg.sensor_info.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_info.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;

		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);

		pr_err("%s:%d mount angle valid %d value %d\n", __func__,
			__LINE__, cdata->cfg.sensor_info.is_mount_angle_valid,
			cdata->cfg.sensor_info.sensor_mount_angle);

		break;
	case CFG_SET_INIT_SETTING:
		CDBG("CFG_SET_INIT_SETTING writing INIT registers: sr200pc20_Init_Reg \n");
		rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Init_Reg);
		break;
	case CFG_SET_RESOLUTION:
		resolution = *((int32_t  *)cdata->cfg.setting);
		CDBG("CFG_SET_RESOLUTION *** res = %d\n " , resolution);
		if( sr200pc20_ctrl.op_mode == CAMERA_MODE_RECORDING ){
			CDBG("writing *** sr200pc20_24fps_Camcoder\n");
			rc = msm_sensor_driver_WRT_LIST(s_ctrl, sr200pc20_24fps_Camcoder);
			recording = 1;
		}else{
		if(recording == 1){
			CDBG("CFG_SET_RESOLUTION recording START recording =1 *** res = %d\n " , resolution);
			rc = msm_sensor_driver_WRT_LIST(s_ctrl,sr200pc20_Auto_fps);
			recording = 0;
		}else{
			sr200pc20_set_resolution(s_ctrl , resolution );
			CDBG("CFG_SET_RESOLUTION END *** res = %d\n " , resolution);
		}
		}
		break;

	case CFG_SET_STOP_STREAM:
		CDBG(" CFG_SET_STOP_STREAM writing stop stream registers: sr200pc20_stop_stream \n");
		if(streamon == 1){
				rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
					i2c_write_conf_tbl(
					s_ctrl->sensor_i2c_client, sr200pc20_stop_stream,
					ARRAY_SIZE(sr200pc20_stop_stream),
					MSM_CAMERA_I2C_BYTE_DATA);
				streamon = 0;
		}
		break;
	case CFG_SET_START_STREAM:
		CDBG(" CFG_SET_START_STREAM writing start stream registers: sr200pc20_start_stream start   \n");
		streamon = 1;
		CDBG("CFG_SET_START_STREAM : sr200pc20_start_stream rc = %d \n", rc);
		break;
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info sensor_slave_info;
		struct msm_camera_power_ctrl_t *p_ctrl;
		uint16_t size;
		int slave_index = 0;
		if (copy_from_user(&sensor_slave_info,
			(void *)cdata->cfg.setting,
				sizeof(sensor_slave_info))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		/* Update sensor slave address */
		if (sensor_slave_info.slave_addr) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				sensor_slave_info.slave_addr >> 1;
		}

		/* Update sensor address type */
		s_ctrl->sensor_i2c_client->addr_type =
			sensor_slave_info.addr_type;

		/* Update power up / down sequence */
		p_ctrl = &s_ctrl->sensordata->power_info;
		size = sensor_slave_info.power_setting_array.size;
		if (p_ctrl->power_setting_size < size) {
			struct msm_sensor_power_setting *tmp;
			tmp = kmalloc(sizeof(*tmp) * size, GFP_KERNEL);
			if (!tmp) {
				pr_err("%s: failed to alloc mem\n", __func__);
			rc = -ENOMEM;
			break;
			}
			kfree(p_ctrl->power_setting);
			p_ctrl->power_setting = tmp;
		}
		p_ctrl->power_setting_size = size;
		rc = copy_from_user(p_ctrl->power_setting, (void *)
			sensor_slave_info.power_setting_array.power_setting,
			size * sizeof(struct msm_sensor_power_setting));
		if (rc) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(sensor_slave_info.power_setting_array.
				power_setting);
			rc = -EFAULT;
			break;
		}
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.slave_addr);
		CDBG("%s sensor addr type %d\n", __func__,
			sensor_slave_info.addr_type);
		CDBG("%s sensor reg %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id_reg_addr);
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id);
		for (slave_index = 0; slave_index <
			p_ctrl->power_setting_size; slave_index++) {
			CDBG("%s i %d power setting %d %d %ld %d\n", __func__,
				slave_index,
				p_ctrl->power_setting[slave_index].seq_type,
				p_ctrl->power_setting[slave_index].seq_val,
				p_ctrl->power_setting[slave_index].config_val,
				p_ctrl->power_setting[slave_index].delay);
		}
		break;
	}
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		CDBG(" CFG_WRITE_I2C_ARRAY  \n");

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &conf_array);
		kfree(reg_setting);
		break;
	}
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		CDBG("CFG_WRITE_I2C_SEQ_ARRAY  \n");

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_seq_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}

	case CFG_POWER_UP:
		CDBG(" CFG_POWER_UP  \n");
		streamon = 0;
		sr200pc20_ctrl.op_mode = CAMERA_MODE_INIT;
		sr200pc20_ctrl.prev_mode = CAMERA_MODE_INIT;
		sr200pc20_ctrl.settings.metering = CAMERA_CENTER_WEIGHT;
		sr200pc20_ctrl.settings.exposure = CAMERA_EV_DEFAULT;
		sr200pc20_ctrl.settings.wb = CAMERA_WHITE_BALANCE_AUTO;
		sr200pc20_ctrl.settings.iso = CAMERA_ISO_MODE_AUTO;
		sr200pc20_ctrl.settings.effect = CAMERA_EFFECT_OFF;
		sr200pc20_ctrl.settings.scenemode = CAMERA_SCENE_AUTO;
		if (s_ctrl->func_tbl->sensor_power_up) {
                        CDBG("CFG_POWER_UP");
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
                } else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		CDBG("CFG_POWER_DOWN  \n");
		if (s_ctrl->func_tbl->sensor_power_down) {
                        CDBG("CFG_POWER_DOWN");
			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
                } else
			rc = -EFAULT;
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		CDBG("CFG_SET_STOP_STREAM_SETTING  \n");

		if (copy_from_user(stop_setting, (void *)cdata->cfg.setting,
		    sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
		    (void *)reg_setting, stop_setting->size *
		    sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
	}
	default:
		rc = 0;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	CDBG("EXIT \n ");

	return rc;
}

int32_t sr200pc20_sensor_native_control(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	int32_t rc = 0;
	struct ioctl_native_cmd *cam_info = (struct ioctl_native_cmd *)argp;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("ENTER \n ");
	pr_err("cam_info values = %d : %d : %d : %d : %d\n", cam_info->mode, cam_info->address, cam_info->value_1, cam_info->value_2 , cam_info->value_3);
	switch (cam_info->mode) {
	case EXT_CAM_EV:
		sr200pc20_ctrl.settings.exposure = cam_info->value_1;
		sr200pc20_set_exposure_compensation(s_ctrl, sr200pc20_ctrl.settings.exposure);
		break;
	case EXT_CAM_WB:
		sr200pc20_ctrl.settings.wb = cam_info->value_1;
		sr200pc20_set_white_balance(s_ctrl, sr200pc20_ctrl.settings.wb);
		break;
	case EXT_CAM_EFFECT:
		sr200pc20_ctrl.settings.effect = cam_info->value_1;
		sr200pc20_set_effect(s_ctrl, sr200pc20_ctrl.settings.effect);
		break;
	case EXT_CAM_SENSOR_MODE:
		sr200pc20_ctrl.op_mode = cam_info->value_1;
		pr_err("EXT_CAM_SENSOR_MODE = %d", sr200pc20_ctrl.op_mode);
		break;
	case EXT_CAM_EXIF:
		sr200pc20_get_exif_info(cam_info);
		if (!copy_to_user((void *)argp,
			(const void *)&cam_info,
			sizeof(cam_info)))
		pr_err("copy failed");
		break;
	default:
		rc = 0;
		break;
	}
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	CDBG("EXIT \n ");
	return 0;
}
