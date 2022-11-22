/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
 
#include "sr030pc50.h"
#include "sr030pc50_regs.h"
#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_dt_util.h"
#if 0
#define CONFIG_LOAD_FILE  // Enable it for Tunning Binary
#endif
#ifdef CONFIG_LOAD_FILE
#define SR030PC50_WRT_LIST(A) \
					sr030pc50_regs_from_sd_tunning(A,s_ctrl,#A);
#else
#define SR030PC50_WRT_LIST(A) \
			s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_conf_tbl( \
				s_ctrl->sensor_i2c_client, A, \
				ARRAY_SIZE(A), \
				MSM_CAMERA_I2C_BYTE_DATA); \
				CDBG("REGSEQ *** %s", #A)
#endif
#define SR030PC50_WRITE_ADDR(A,B) \
			s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write( \
            s_ctrl->sensor_i2c_client,A, B, MSM_CAMERA_I2C_BYTE_DATA)

#define SR030PC50_READ_ADDR(A,B) \
			s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read( \
            s_ctrl->sensor_i2c_client,A, B, MSM_CAMERA_I2C_BYTE_DATA)

#ifdef CONFIG_LOAD_FILE
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

static char *sr030pc50_regs_table;
static int sr030pc50_regs_table_size;
int sr030pc50_regs_from_sd_tunning(struct msm_camera_i2c_reg_conf *settings, struct msm_sensor_ctrl_t *s_ctrl,char * name);
void sr030pc50_regs_table_init(char *filename);
void sr030pc50_regs_table_exit(void);
#endif
static struct sr030pc50_ctrl sr030pc50_ctrl;

int sr030pc50_sensor_match_id(struct msm_camera_i2c_client *sensor_i2c_client,
	struct msm_camera_slave_info *slave_info,
	const char *sensor_name)
{
	int rc = 0;
	uint16_t chipid = 0;
	enum msm_camera_i2c_data_type data_type = MSM_CAMERA_I2C_WORD_DATA;

	if (!sensor_i2c_client || !slave_info || !sensor_name) {
		pr_err("%s:%d failed: %p %p %p\n",
			__func__, __LINE__, sensor_i2c_client, slave_info,
			sensor_name);
		return -EINVAL;
	}
	CDBG("%s sensor_name=%s\n", __func__, sensor_name);

	if (sensor_i2c_client->data_type == MSM_CAMERA_I2C_BYTE_DATA)
		data_type = MSM_CAMERA_I2C_BYTE_DATA;

	rc = sensor_i2c_client->i2c_func_tbl->i2c_read(
		sensor_i2c_client, slave_info->sensor_id_reg_addr,
			&chipid, data_type);

	CDBG("sid = 0x%X sensorid = 0x%X DATA TYPE = %d, chipid = 0x%X\n",
		sensor_i2c_client->cci_client->sid,
		slave_info->sensor_id, sensor_i2c_client->data_type, chipid);

	if (chipid != slave_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
	}
	return 0;
}


int32_t sr030pc50_set_exposure_compensation(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("CAMSET -- EV is %d ", mode);
	switch (mode) {
	case CAMERA_EV_M4:
		rc = SR030PC50_WRT_LIST(sr030pc50_ev_minus_4_regs);
		break;

	case CAMERA_EV_M3:
		rc = SR030PC50_WRT_LIST(sr030pc50_ev_minus_3_regs);
		break;

	case CAMERA_EV_M2:
		rc = SR030PC50_WRT_LIST(sr030pc50_ev_minus_2_regs);
		break;

	case CAMERA_EV_M1:
		rc = SR030PC50_WRT_LIST(sr030pc50_ev_minus_1_regs);
		break;

	case CAMERA_EV_DEFAULT:
		rc = SR030PC50_WRT_LIST(sr030pc50_ev_default_regs);
		break;

	case CAMERA_EV_P1:
		rc = SR030PC50_WRT_LIST(sr030pc50_ev_plus_1_regs);
		break;

	case CAMERA_EV_P2:
		rc = SR030PC50_WRT_LIST(sr030pc50_ev_plus_2_regs);
		break;

	case CAMERA_EV_P3:
		rc = SR030PC50_WRT_LIST(sr030pc50_ev_plus_3_regs);
		break;

	case CAMERA_EV_P4:
		rc = SR030PC50_WRT_LIST(sr030pc50_ev_plus_4_regs);
		break;
	default:
		CDBG("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}
int32_t sr030pc50_set_white_balance(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("CAMSET -- WB is %d", mode);
	switch (mode) {
	case CAMERA_WHITE_BALANCE_OFF:
	case CAMERA_WHITE_BALANCE_AUTO:
		rc = SR030PC50_WRT_LIST(sr030pc50_wb_auto_regs);
		break;
	case CAMERA_WHITE_BALANCE_INCANDESCENT:
		rc = SR030PC50_WRT_LIST(sr030pc50_wb_incandescent_regs);
		break;
	case CAMERA_WHITE_BALANCE_FLUORESCENT:
		rc = SR030PC50_WRT_LIST(sr030pc50_wb_fluorescent_regs);
		break;
	case CAMERA_WHITE_BALANCE_DAYLIGHT:
		rc = SR030PC50_WRT_LIST(sr030pc50_wb_daylight_regs);
		break;
	case CAMERA_WHITE_BALANCE_CLOUDY_DAYLIGHT:
		rc = SR030PC50_WRT_LIST(sr030pc50_wb_cloudy_regs);
		break;
	default:
		CDBG("%s: Setting %d is invalid", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr030pc50_set_resolution(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("CAMSET -- resolution is %d ", mode);
	//SR030PC50_WRT_LIST(sr030pc50_fps_auto_normal_regs);

	return rc;
}

int32_t sr030pc50_set_fps(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("CAMSET -- fps is %d ", mode);
	switch (mode) {
	case 0:
		rc = SR030PC50_WRT_LIST(sr030pc50_fps_auto_normal_regs);
		break;
	case 5:
		rc = SR030PC50_WRT_LIST(sr030pc50_fps_5_regs);
		break;
	case 7:
		rc = SR030PC50_WRT_LIST(sr030pc50_fps_7_regs);
		break;
	case 10:
		rc = SR030PC50_WRT_LIST(sr030pc50_fps_10_regs);
		break;
	case 15:
		rc = SR030PC50_WRT_LIST(sr030pc50_fps_15_regs);
		break;
	case 20:
		rc = SR030PC50_WRT_LIST(sr030pc50_fps_20_regs);
		break;
	case 25:
		rc = SR030PC50_WRT_LIST(sr030pc50_fps_25_regs);
		break;
	case 30:
		rc = SR030PC50_WRT_LIST(sr030pc50_fps_30_regs);
		break;
	default:
		CDBG("%s: Setting %d is invalid ", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr030pc50_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("CAMSET -- effect is %d ", mode);
	switch (mode) {
	case CAMERA_EFFECT_OFF:
		rc = SR030PC50_WRT_LIST(sr030pc50_effect_normal_regs);
		break;

	case CAMERA_EFFECT_MONO:
		rc = SR030PC50_WRT_LIST(sr030pc50_effect_mono_regs);
		break;

	case CAMERA_EFFECT_NEGATIVE:
		rc = SR030PC50_WRT_LIST(sr030pc50_effect_negative_regs);
		break;

	case CAMERA_EFFECT_SEPIA:
		rc = SR030PC50_WRT_LIST(sr030pc50_effect_sepia_regs);
		break;
	default:
		CDBG("%s: Setting %d is invalid ", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr030pc50_get_exposure(struct msm_sensor_ctrl_t *s_ctrl, \
	unsigned int *Expmax, unsigned int *Exptime)
{
	int rc=0;
	unsigned short read_value1 = 0;
	unsigned short read_value2 = 0;
	unsigned short read_value3 = 0;
	unsigned short read_value4 = 0;

	rc = SR030PC50_WRITE_ADDR(0x03, 0x20);
	rc = SR030PC50_READ_ADDR(0x10, &read_value1);
	if(read_value1 & 0x10) {
		rc = SR030PC50_READ_ADDR(0xA0, &read_value2);
		rc = SR030PC50_READ_ADDR(0xA1, &read_value3);
		rc = SR030PC50_READ_ADDR(0xA2, &read_value4);
		*Expmax = (read_value2 << 16) | (read_value3 << 8) | read_value4;
		/*CDBG("read_value = 0x%2x, 0x%2x, 0x%2x : Expmax = 0x%8x", \
		read_value2, read_value3, read_value4, *Expmax); */
	} else {
		rc = SR030PC50_READ_ADDR(0x88, &read_value2);
		rc = SR030PC50_READ_ADDR(0x89, &read_value3);
		rc = SR030PC50_READ_ADDR(0x8A, &read_value4);
		*Expmax = (read_value2 << 16) | (read_value3 << 8) | read_value4;
		/*CDBG("read_value = 0x%2x, 0x%2x, 0x%2x : Expmax = 0x%8x", \
		read_value2, read_value3, read_value4, *Expmax); */
	}
	rc = SR030PC50_READ_ADDR(0x80, &read_value2);
	rc = SR030PC50_READ_ADDR(0x81, &read_value3);
	rc = SR030PC50_READ_ADDR(0x81, &read_value4);
	*Exptime = (read_value2 << 16) | (read_value3 << 8) | read_value4;
	/*CDBG("read_value = 0x%2x, 0x%2x, 0x%2x : Exptime = 0x%8x", \
		read_value2, read_value3, read_value4, *Exptime); */
	return rc;
}

int32_t sr030pc50_set_exif(struct msm_sensor_ctrl_t *s_ctrl )
{
	unsigned short read_value1 = 0;
	unsigned short read_value2 = 0;
	unsigned short read_value3 = 0;
	unsigned short read_value4 = 0;
	unsigned int temp;
	unsigned int gain_value = 0;

	SR030PC50_WRITE_ADDR(0x03, 0x20);
	SR030PC50_READ_ADDR(0x80, &read_value1);
	SR030PC50_READ_ADDR(0x81, &read_value2);
	SR030PC50_READ_ADDR(0x82, &read_value3);

	temp = ((read_value1 << 19)
		+ (read_value2 << 11) + (read_value3 << 3));

	if ( temp != 0)
		sr030pc50_ctrl.exif_shutterspeed = 12000000 / temp;
	else
		sr030pc50_ctrl.exif_shutterspeed = 0;

	SR030PC50_WRITE_ADDR(0x03, 0x20);
	SR030PC50_READ_ADDR(0xb0, &read_value4);

	gain_value = (read_value4 ) * 1000 + 16000;

	if(gain_value < 36032)
		sr030pc50_ctrl.exif_iso = 50;
	else if(gain_value < 48832)
		sr030pc50_ctrl.exif_iso = 100;
	else if(gain_value < 88320)
		sr030pc50_ctrl.exif_iso = 200;
	else
		sr030pc50_ctrl.exif_iso = 400;

	CDBG("shutter_speed = %d, ISO = %d",
		sr030pc50_ctrl.exif_shutterspeed, sr030pc50_ctrl.exif_iso);
	return 0;
}

int32_t sr030pc50_get_exif(struct ioctl_native_cmd * exif_info)
{
	exif_info->value_1 = 1;	// equals 1 to update the exif value in the user level.
	exif_info->value_2 = sr030pc50_ctrl.exif_iso;
	exif_info->value_3 = sr030pc50_ctrl.exif_shutterspeed;
	return 0;
}

int32_t sr030pc50_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	int32_t rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);

	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		CDBG(" CFG_GET_SENSOR_INFO  ");
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++)
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
		CDBG("%s:%d sensor name %s ", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d ", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d ", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);

		break;
	case CFG_SET_INIT_SETTING:
		CDBG("CFG_SET_INIT_SETTING writing INIT registers: sr030pc50_init_regs  ");
#ifdef CONFIG_LOAD_FILE
		sr030pc50_regs_table_init("/data/sr030pc50_regs.h");
		pr_err("/data/sr030pc50_regs.h inside CFG_SET_INIT_SETTING\n");
#endif
		rc = SR030PC50_WRT_LIST(sr030pc50_init_regs);
		break;
	case CFG_SET_RESOLUTION:
		sr030pc50_ctrl.settings.resolution = *((int32_t  *)cdata->cfg.setting);
		CDBG("CFG_SET_RESOLUTION  res = %d  " , sr030pc50_ctrl.settings.resolution);
		break;
	case CFG_SET_STOP_STREAM:
		CDBG(" CFG_SET_STOP_STREAM writing stop stream registers: sr030pc50_stop_stream  ");
		if(sr030pc50_ctrl.streamon == 1){
				//SR030PC50_WRT_LIST(sr030pc50_stop_stream);
				sr030pc50_ctrl.streamon = 0;
		}
		break;
	case CFG_SET_START_STREAM:
		CDBG(" CFG_SET_START_STREAM writing start stream registers: sr030pc50_start_stream start    ");
		sr030pc50_set_resolution(s_ctrl , sr030pc50_ctrl.settings.resolution );
		if (sr030pc50_ctrl.op_mode == CAMERA_MODE_CAPTURE ){
			sr030pc50_set_exif(s_ctrl);
		} else if (sr030pc50_ctrl.op_mode == CAMERA_MODE_RECORDING ){
			sr030pc50_set_fps(s_ctrl, 25);
		} else {
			if (sr030pc50_ctrl.prev_mode == CAMERA_MODE_INIT ) {
				CDBG("Init preview");
				sr030pc50_set_effect( s_ctrl , sr030pc50_ctrl.settings.effect);
				sr030pc50_set_exposure_compensation( s_ctrl , sr030pc50_ctrl.settings.exposure );
			} else if (sr030pc50_ctrl.prev_mode == CAMERA_MODE_RECORDING ){
				unsigned int Expmax = 0;
				unsigned int Exptime = 0;
				CDBG("Return preview after Recording");
				sr030pc50_get_exposure(s_ctrl, &Expmax, &Exptime);
				CDBG("Expmax = %d, Exptime = %d", Expmax, Exptime);
				if (Exptime < Expmax) {
					sr030pc50_set_fps(s_ctrl, 0);
				} else {
					SR030PC50_WRT_LIST(sr030pc50_fps_auto_dark_regs);
				}
			} else {
				CDBG("Return preview after capture");
			}
		}
		sr030pc50_ctrl.streamon = 1;
		CDBG("CFG_SET_START_STREAM : sr030pc50_start_stream rc = %d  ", rc);
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
		CDBG("slave addr = %x, addr type %d, sensor id reg %x, sensor id %x \n", \
			sensor_slave_info.slave_addr, sensor_slave_info.addr_type, \
			sensor_slave_info.sensor_id_info.sensor_id_reg_addr,\
			sensor_slave_info.sensor_id_info.sensor_id);
		for (slave_index = 0; slave_index <
			p_ctrl->power_setting_size; slave_index++) {
			CDBG("%s i %d power setting %d %d %ld %d", __func__,
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

		CDBG(" CFG_WRITE_I2C_ARRAY");

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
		sr030pc50_ctrl.streamon = 0;
		sr030pc50_ctrl.op_mode = CAMERA_MODE_INIT;
		sr030pc50_ctrl.prev_mode = CAMERA_MODE_INIT;
		sr030pc50_ctrl.settings.metering = CAMERA_CENTER_WEIGHT;
		sr030pc50_ctrl.settings.exposure = CAMERA_EV_DEFAULT;
		sr030pc50_ctrl.settings.wb = CAMERA_WHITE_BALANCE_AUTO;
		sr030pc50_ctrl.settings.iso = CAMERA_ISO_MODE_AUTO;
		sr030pc50_ctrl.settings.effect = CAMERA_EFFECT_OFF;
		sr030pc50_ctrl.settings.scenemode = CAMERA_SCENE_AUTO;
		if (s_ctrl->func_tbl->sensor_power_up) {
                        CDBG("CFG_POWER_UP");
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl,
				&s_ctrl->sensordata->power_info,
				s_ctrl->sensor_i2c_client,
				s_ctrl->sensordata->slave_info,
				s_ctrl->sensordata->sensor_name);
                } else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		CDBG("CFG_POWER_DOWN  \n");
		 if (s_ctrl->func_tbl->sensor_power_down) {
                        CDBG("CFG_POWER_DOWN");
			rc = s_ctrl->func_tbl->sensor_power_down(
				s_ctrl,
				&s_ctrl->sensordata->power_info,
				s_ctrl->sensor_device_type,
				s_ctrl->sensor_i2c_client);
                } else
			rc = -EFAULT;
#ifdef CONFIG_LOAD_FILE
			sr030pc50_regs_table_exit();
#endif
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
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}


int32_t sr030pc50_sensor_native_control(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	int32_t rc = 0;
	struct ioctl_native_cmd *cam_info = (struct ioctl_native_cmd *)argp;

	mutex_lock(s_ctrl->msm_sensor_mutex);

	/*CDBG("cam_info values = %d : %d : %d : %d : %d\n", cam_info->mode, cam_info->address, cam_info->value_1, cam_info->value_2 , cam_info->value_3);*/
	switch (cam_info->mode) {
	case EXT_CAM_EV:
		sr030pc50_ctrl.settings.exposure = cam_info->value_1;
		sr030pc50_set_exposure_compensation(s_ctrl, sr030pc50_ctrl.settings.exposure);
		break;
	case EXT_CAM_WB:
		sr030pc50_ctrl.settings.wb = cam_info->value_1;
		rc = sr030pc50_set_white_balance(s_ctrl, sr030pc50_ctrl.settings.wb);
		break;
	case EXT_CAM_EFFECT:
		sr030pc50_ctrl.settings.effect = cam_info->value_1;
		sr030pc50_set_effect(s_ctrl, sr030pc50_ctrl.settings.effect);
		break;
	case EXT_CAM_SENSOR_MODE:
		sr030pc50_ctrl.prev_mode = sr030pc50_ctrl.op_mode;
		sr030pc50_ctrl.op_mode = cam_info->value_1;
		CDBG("EXT_CAM_SENSOR_MODE = %d", sr030pc50_ctrl.op_mode);
	case EXT_CAM_EXIF:
		sr030pc50_get_exif(cam_info);
			if (!copy_to_user((void *)argp,
	                (const void *)&cam_info,
					sizeof(cam_info)))
			pr_err("copy failed");

		break;
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

#ifdef CONFIG_LOAD_FILE
void sr030pc50_regs_table_exit(void)
{
	printk("%s %d", __func__, __LINE__);
	if (sr030pc50_regs_table) {
		vfree(sr030pc50_regs_table);
		sr030pc50_regs_table = NULL;
	}
}

int sr030pc50_regs_from_sd_tunning(struct msm_camera_i2c_reg_conf *settings, struct msm_sensor_ctrl_t *s_ctrl,char * name) {
	char *start, *end, *reg;
	int addr,rc = 0;
	unsigned int  value;
	char reg_buf[5], data_buf1[5];
	*(reg_buf + 4) = '\0';
	*(data_buf1 + 4) = '\0';
	printk("PHANI.....Inside Modified Tuning sr030pc50_regs_from_sd_tunning \n");
	if (settings != NULL){
		pr_err("sr030pc50_regs_from_sd_tunning start address %x start data %x",settings->reg_addr,settings->reg_data);
	}
	if(sr030pc50_regs_table == NULL) {
		pr_err("sr030pc50_regs_table is null ");
		return -1;
	}
	start = strstr(sr030pc50_regs_table, name);
	if (start == NULL){
		return -1;
		}
	end = strstr(start, "};");
	while (1) {
	reg = strstr(start, "{0x");
	if ((reg == NULL) || (reg > end))
		break;
	if (reg != NULL) {
		memcpy(reg_buf, (reg + 1), 4);
		memcpy(data_buf1, (reg + 7), 4);
	if(kstrtoint(reg_buf, 16, &addr))
		pr_err("kstrtoint error");
	if(kstrtoint(data_buf1, 16, &value))
		pr_err("kstrtoint error");
	if (reg)
		start = (reg + 14);
	if (addr == 0xff){
		usleep_range(value * 10, (value* 10) + 10);
		pr_err("delay = %d\n", (int)value*10);
		}
	else{
		rc=s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write(s_ctrl->sensor_i2c_client, addr,
						value,MSM_CAMERA_I2C_BYTE_DATA);
			}
		}
	}
	pr_err("sr030pc50_regs_from_sd_tunning end!");
	return rc;
}
void sr030pc50_regs_table_init(char *filename)
{
	struct file *filp;
	char *dp;
	long lsize;
	loff_t pos;
	int ret;
	mm_segment_t fs = get_fs();
	pr_err("%s %d", __func__, __LINE__);
	set_fs(get_ds());
	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_err("file open error %ld",(long) filp);
		return;
	}
	lsize = filp->f_path.dentry->d_inode->i_size;
	pr_err("size : %ld", lsize);
	dp = vmalloc(lsize);
	if (dp == NULL) {
		pr_err("Out of Memory");
		filp_close(filp, current->files);
	}
	pos = 0;
	memset(dp, 0, lsize);
	ret = vfs_read(filp, (char __user *)dp, lsize, &pos);
	if (ret != lsize) {
		pr_err("Failed to read file ret = %d\n", ret);
		vfree(dp);
		filp_close(filp, current->files);
	}
	filp_close(filp, current->files);
	set_fs(fs);
	pr_err("coming to else part of string compare sr030pc50_regs_table");
	sr030pc50_regs_table = dp;
	sr030pc50_regs_table_size = lsize;
	*((sr030pc50_regs_table + sr030pc50_regs_table_size) - 1) = '\0';
	return;
}
#endif
