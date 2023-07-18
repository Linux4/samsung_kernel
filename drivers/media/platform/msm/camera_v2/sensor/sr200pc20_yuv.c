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
#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_dt_util.h"

#if defined(CONFIG_SEC_T8_PROJECT)
#include "sr200pc20_yuv_t8.h"
#elif defined(CONFIG_SEC_T10_PROJECT)
#include "sr200pc20_yuv_t10.h"
#endif

#if 0
#define CONFIG_LOAD_FILE  // Enable it for Tuning Binary
#endif

#ifdef CONFIG_LOAD_FILE
#define sr200pc20_WRT_LIST(s_ctrl,A)\
    sr200_regs_from_sd_tuning(A,s_ctrl,#A);
#else
#define sr200pc20_WRT_LIST(s_ctrl,A)\
    s_ctrl->sensor_i2c_client->i2c_func_tbl->\
    i2c_write_conf_tbl(\
    s_ctrl->sensor_i2c_client, A,\
    ARRAY_SIZE(A),\
    MSM_CAMERA_I2C_BYTE_DATA);\
    pr_err("SR200pc20_WRT_LIST - %s\n", #A);
#endif

#ifdef CONFIG_LOAD_FILE
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
static char *sr200_regs_table;
static int sr200_regs_table_size;
int sr200_regs_from_sd_tuning(struct msm_camera_i2c_reg_conf *settings, struct msm_sensor_ctrl_t *s_ctrl,char * name);
void sr200_regs_table_init(char *filename);
void sr200_regs_table_exit(void);
#endif

#define SR200PC20_VALUE_EXPOSURE 1
#define SR200PC20_VALUE_WHITEBALANCE 2
#define SR200PC20_VALUE_EFFECT 4

static struct yuv_ctrl sr200pc20_ctrl;
static exif_data_t sr200pc20_exif;

static int32_t streamon = 0;
//static int32_t recording = 0;
static unsigned int value_mark_ev = 0;
static unsigned int value_mark_wb = 0;
static unsigned int value_mark_effect = 0;

static short scene_mode_chg = 0;

int32_t sr200pc20_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
int32_t sr200pc20_sensor_native_control(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
int sr200pc20_get_exif_data(struct msm_sensor_ctrl_t *s_ctrl, void __user *argp);
void sr200pc20_get_exif(struct msm_sensor_ctrl_t *s_ctrl);
int32_t sr200pc20_get_exif_info(struct ioctl_native_cmd * exif_info);
static int sr200pc20_exif_shutter_speed(struct msm_sensor_ctrl_t *s_ctrl);
static int sr200pc20_exif_iso(struct msm_sensor_ctrl_t *s_ctrl);
static int32_t sr200pc20_set_metering(struct msm_sensor_ctrl_t *s_ctrl, int mode);

int32_t sr200pc20_sensor_match_id(struct msm_camera_i2c_client *sensor_i2c_client,
    struct msm_camera_slave_info *slave_info, const char *sensor_name)
{
    int32_t rc = 0;
    uint16_t chipid = 0;
    if (!sensor_i2c_client || !slave_info || !sensor_name) {
        pr_err("%s:%d failed: %p %p %p\n",__func__, __LINE__,
            sensor_i2c_client, slave_info,sensor_name);
        return -EINVAL;
    }
    CDBG("%s sensor_name =%s sid = 0x%X sensorid = 0x%X DATA TYPE = %d\n",
        __func__, sensor_name, sensor_i2c_client->cci_client->sid,
        slave_info->sensor_id, sensor_i2c_client->data_type);
    rc = sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client,
        slave_info->sensor_id_reg_addr, &chipid, sensor_i2c_client->data_type);
    if (chipid != slave_info->sensor_id) {
        pr_err("%s: chip id %x does not match read id: %x\n",
            __func__, chipid, slave_info->sensor_id);
    }
    return rc;
}

int32_t sr200pc20_set_scene_mode(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
    int32_t rc = 0;
    uint32_t exptime = 0;
    uint32_t expmax = 0;
    uint16_t value = 0;

    CDBG("Scene mode E = %d", mode);
    if (scene_mode_chg == 0) {
        CDBG("scene mode has not changed from %d mode", mode);
        return rc;
    }

    switch (mode) {
    case CAMERA_SCENE_AUTO:
            rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20m_scene_off);
            break;
    case CAMERA_SCENE_LANDSCAPE:
            rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20m_scene_landscape);
            break;
    case CAMERA_SCENE_SPORT:
            pr_err("%s: SPORT mode not supported\n", __func__);
            break;
    case CAMERA_SCENE_PARTY:
            rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20m_scene_party);
            break;
    case CAMERA_SCENE_BEACH:
            pr_err("%s: BEACH mode not supported\n", __func__);
            break;
    case CAMERA_SCENE_SUNSET:
            rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20m_scene_sunset);
            break;
    case CAMERA_SCENE_DAWN:
            rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20m_scene_dawn);
            break;
    case CAMERA_SCENE_FALL:
            rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20m_scene_fall);
            break;
    case CAMERA_SCENE_CANDLE:
            rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20m_scene_candle);
            break;
    case CAMERA_SCENE_FIRE:
            pr_err("%s: FIRE mode not supported\n", __func__);
            break;
    case CAMERA_SCENE_AGAINST_LIGHT:
            rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20m_scene_backlight);
            break;
    case CAMERA_SCENE_NIGHT:
            s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client,
                                                               0x03, 0x20,
                                                               MSM_CAMERA_I2C_BYTE_DATA);
            s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,
                                                               0x80, &value,
                                                               MSM_CAMERA_I2C_BYTE_DATA);
            CDBG("read value=0x%x", value);
            exptime |= value << 16;
            CDBG("after first op exptime =0x%x", exptime);
                 s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,
                                                                   0x81, &value,
                                                                   MSM_CAMERA_I2C_BYTE_DATA);
            CDBG("read value=0x%x", value);
            exptime |= value << 8;
            CDBG("after second op exptime =0x%x", exptime);
                 s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,
                                                                   0x82, &value,
                                                                   MSM_CAMERA_I2C_BYTE_DATA);
            CDBG("read value=0x%x", value);
            exptime |= value;
            CDBG("after third op exptime =0x%x", exptime);

            s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,
                                                              0x88, &value,
                                                              MSM_CAMERA_I2C_BYTE_DATA);
            CDBG("read value=0x%x", value);
            expmax |= value << 16;
            CDBG("after first op expmax =0x%x", expmax);
            s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,
                                                              0x89, &value,
                                                              MSM_CAMERA_I2C_BYTE_DATA);
            CDBG("read value=0x%x", value);
            expmax |= value << 8;
            CDBG("after second op expmax =0x%x", expmax);
            s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(s_ctrl->sensor_i2c_client,
                                                              0x8A, &value,
                                                              MSM_CAMERA_I2C_BYTE_DATA);
            CDBG("read value=0x%x", value);
            expmax |= value;
            CDBG("after third op expmax =0x%x", expmax);
            CDBG("exptime=0x%x expmax=0x%x", exptime, expmax);
            if (exptime < expmax) {
                CDBG("writing sr200pc20m_scene_nightshot_Normal");
                sr200pc20_WRT_LIST(s_ctrl,sr200pc20m_scene_nightshot_Normal);
            } else {
                CDBG("writing sr200pc20m_scene_nightshot_Dark");
                sr200pc20_WRT_LIST(s_ctrl,sr200pc20m_scene_nightshot_Dark);
            }
            break;
    default:
	    pr_err("%s: Setting %d is invalid\n", __func__, mode);
	    rc = 0;
            break;
    }
    scene_mode_chg = 0;
    CDBG("Scene mode X = %d", mode);
    return rc;
}

int32_t sr200pc20_set_exposure_compensation(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	CDBG("CAM-SETTING -- EV is %d", mode);
	switch (mode) {
	case CAMERA_EV_M4:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_brightness_M4);
		break;
	case CAMERA_EV_M3:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_brightness_M3);
		break;
	case CAMERA_EV_M2:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_brightness_M2);
		break;
	case CAMERA_EV_M1:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_brightness_M1);
		break;
	case CAMERA_EV_DEFAULT:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_brightness_default);
		break;
	case CAMERA_EV_P1:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_brightness_P1);
		break;
	case CAMERA_EV_P2:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_brightness_P2);
		break;
	case CAMERA_EV_P3:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_brightness_P3);
		break;
	case CAMERA_EV_P4:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_brightness_P4);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

#if 0
int32_t sr200pc20_set_contrast(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	CDBG("Contrast is %d", mode);
	switch (mode) {
	case CAMERA_CONTRAST_LV0:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_contrast_M4);
		break;
	case CAMERA_CONTRAST_LV1:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_contrast_M3);
		break;
	case CAMERA_CONTRAST_LV2:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_contrast_M2);
		break;
	case CAMERA_CONTRAST_LV3:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_contrast_M1);
		break;
	case CAMERA_CONTRAST_LV4:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_contrast_default);
		break;
	case CAMERA_CONTRAST_LV5:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_contrast_P1);
		break;
	case CAMERA_CONTRAST_LV6:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_contrast_P2);
		break;
	case CAMERA_CONTRAST_LV7:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_contrast_P3);
		break;
	case CAMERA_CONTRAST_LV8:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_contrast_P4);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}
#endif

int32_t sr200pc20_set_white_balance(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	CDBG("WB is %d", mode);
	switch (mode) {
	case CAMERA_WHITE_BALANCE_OFF:
	case CAMERA_WHITE_BALANCE_AUTO:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_WB_Auto);
		break;
	case CAMERA_WHITE_BALANCE_INCANDESCENT:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_WB_Incandescent);
		break;
	case CAMERA_WHITE_BALANCE_FLUORESCENT:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_WB_Fluorescent);
		break;
	case CAMERA_WHITE_BALANCE_DAYLIGHT:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_WB_Daylight);
		break;
	case CAMERA_WHITE_BALANCE_CLOUDY_DAYLIGHT:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_WB_Cloudy);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr200pc20_set_resolution(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	switch (mode) {
	case MSM_SENSOR_RES_FULL:
		CDBG("In case MSM_SENSOR_RES_FULL");
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_Capture);
		//to get exif data
		sr200pc20_get_exif(s_ctrl);
		break;
	case MSM_SENSOR_RES_QTR:
		CDBG("In case MSM_SENSOR_RES_QTR");
		if(sr200pc20_ctrl.op_mode == CAMERA_MODE_RECORDING)
		{
			rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20m_recording_800x600_24fps);	
		}
		else
		{
			rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_800x600_Preview);
		}
		rc=0;
		break;
#if 1
	case MSM_SENSOR_RES_2:
		CDBG("In case MSM_SENSOR_RES_2");
		if(sr200pc20_ctrl.op_mode == CAMERA_MODE_RECORDING)
		{
			rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20m_recording_640x480_24fps);	
		}
		else
		{
			rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_640x480_Preview);
		}
		rc=0;
		break;
#endif
	default:
		rc=0;
		break;
	}
	return rc;
}

int32_t sr200pc20_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("E\n");
	switch (mode) {
	case CAMERA_EFFECT_OFF:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_Effect_Normal);
		break;
	case CAMERA_EFFECT_MONO:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_Effect_Gray);
		break;
	case CAMERA_EFFECT_NEGATIVE:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_Effect_Negative);
		break;
	case CAMERA_EFFECT_SEPIA:
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_Effect_Sepia);
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

	CDBG("ENTER \n ");

	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		CDBG("CFG_GET_SENSOR_INFO\n");
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++)
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
		break;
	case CFG_SET_INIT_SETTING:
		CDBG("CFG_SET_INIT_SETTING\n");
#ifdef CONFIG_LOAD_FILE /* this call should be always called first */
		sr200_regs_table_init("/data/sr200pc20_yuv_t8.h");
		pr_err("/data/sr200pc20_yuv_t8.h inside CFG_SET_INIT_SETTING");
#endif
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_Init_Reg);
#if 0
		rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_stop_stream);
#endif
		break;
	case CFG_SET_RESOLUTION:
		sr200pc20_ctrl.settings.resolution = *((int32_t  *)cdata->cfg.setting);
		CDBG("CFG_SET_RESOLUTION - res = %d\n" , sr200pc20_ctrl.settings.resolution);
		CDBG("sr200pc20_ctrl.op_mode = %d\n " , sr200pc20_ctrl.op_mode);
		break;
	case CFG_SET_STOP_STREAM:
		CDBG("CFG_SET_STOP_STREAM - res = %d\n ", sr200pc20_ctrl.settings.resolution);
		if(streamon == 1){
			CDBG("CFG_SET_STOP_STREAM *** res = %d - streamon == 1\n " , sr200pc20_ctrl.settings.resolution);
#if 0
			CDBG(" CFG_SET_STOP_STREAM writing stop stream registers: sr200pc20_stop_stream \n");
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, sr200pc20_stop_stream,
				ARRAY_SIZE(sr200pc20_stop_stream),
				MSM_CAMERA_I2C_BYTE_DATA);
			//rc = sr200pc20_WRT_LIST(s_ctrl,sr200pc20_stop_stream);
#endif
			rc=0;
			streamon = 0;
		}
		break;
	case CFG_SET_START_STREAM:
		CDBG("CFG_SET_START_STREAM\n");
		CDBG("sr200pc20_ctrl.op_mode = %d,sr200pc20_ctrl.prev_mode = %d \n", sr200pc20_ctrl.op_mode, sr200pc20_ctrl.prev_mode);
                switch (sr200pc20_ctrl.op_mode) {
                case CAMERA_MODE_PREVIEW:
                        if (sr200pc20_ctrl.prev_mode != CAMERA_MODE_CAPTURE) {
                            switch (sr200pc20_ctrl.prev_mode) {
                            case CAMERA_MODE_INIT:
                                    scene_mode_chg = 1;
                                    sr200pc20_set_scene_mode(s_ctrl, sr200pc20_ctrl.settings.scenemode);
                                    sr200pc20_set_effect( s_ctrl , sr200pc20_ctrl.settings.effect);
                                    sr200pc20_set_white_balance( s_ctrl, sr200pc20_ctrl.settings.wb);
                                    sr200pc20_set_exposure_compensation( s_ctrl , sr200pc20_ctrl.settings.exposure );
                                    sr200pc20_set_metering(s_ctrl, sr200pc20_ctrl.settings.metering);
                                    break;
                            case CAMERA_MODE_RECORDING:
                                    sr200pc20_WRT_LIST(s_ctrl,sr200pc20_Init_Reg);
                                    scene_mode_chg = 1;
                                    sr200pc20_set_scene_mode(s_ctrl, sr200pc20_ctrl.settings.scenemode);
                                    sr200pc20_set_effect( s_ctrl , sr200pc20_ctrl.settings.effect);
                                    sr200pc20_set_white_balance( s_ctrl, sr200pc20_ctrl.settings.wb);
                                    sr200pc20_set_exposure_compensation( s_ctrl , sr200pc20_ctrl.settings.exposure);
                                    sr200pc20_set_metering(s_ctrl, sr200pc20_ctrl.settings.metering);
                                    break;
                            case CAMERA_MODE_PREVIEW:
                                    sr200pc20_set_scene_mode(s_ctrl, sr200pc20_ctrl.settings.scenemode);
                                    break;
                            default:
                                    pr_err("Invalid previous mode");
                                    break;
                            }
                        }
                        if (sr200pc20_ctrl.op_mode != sr200pc20_ctrl.prev_mode) {
                            sr200pc20_set_resolution(s_ctrl , sr200pc20_ctrl.settings.resolution);
                        }
                        streamon = 1;
                        break;
                case CAMERA_MODE_CAPTURE:
                case CAMERA_MODE_RECORDING:
                        if (sr200pc20_ctrl.op_mode != sr200pc20_ctrl.prev_mode) {
                            sr200pc20_set_resolution(s_ctrl , sr200pc20_ctrl.settings.resolution );
                        }
                        streamon = 1;
                        break;
                default:
                        pr_err("%s: CFG_SET_START_STREAM - %d INVALID mode\n", __func__, sr200pc20_ctrl.op_mode);
                        break;
                }
		break;
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info sensor_slave_info;
		struct msm_camera_power_ctrl_t *p_ctrl;
		uint16_t size;
		int slave_index = 0;
		CDBG("CFG_SET_SLAVE_INFO\n");
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

		CDBG("CFG_WRITE_I2C_ARRAY\n");

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

		CDBG("CFG_WRITE_I2C_SEQ_ARRAY\n");

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
		CDBG("CFG_POWER_UP\n");
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
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl,
				&s_ctrl->sensordata->power_info,
				s_ctrl->sensor_i2c_client,
				s_ctrl->sensordata->slave_info,
				s_ctrl->sensordata->sensor_name);
                } else
			rc = -EFAULT;
		break;
	case CFG_POWER_DOWN:
		CDBG("CFG_POWER_DOWN\n");
		if (s_ctrl->func_tbl->sensor_power_down) {
			CDBG("CFG_POWER_DOWN");
			rc = s_ctrl->func_tbl->sensor_power_down(
				s_ctrl,
				&s_ctrl->sensordata->power_info,
				s_ctrl->sensor_device_type,
				s_ctrl->sensor_i2c_client);
                } else
			rc = -EFAULT;
		break;
	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		CDBG("CFG_SET_STOP_STREAM_SETTING\n");

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
		break;
	}
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	CDBG("EXIT \n ");
	return 0;
}

int32_t sr200pc20_sensor_native_control(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct ioctl_native_cmd *cam_info = (struct ioctl_native_cmd *)argp;

	mutex_lock(s_ctrl->msm_sensor_mutex);

	CDBG("ENTER \n ");
	CDBG("cam_info values = %d : %d : %d : %d : %d\n", cam_info->mode, cam_info->address, cam_info->value_1, cam_info->value_2 , cam_info->value_3);
	/*Hal set more than 2 parms one time*/
	if ((cam_info->value_2 != 0 )&&
		(cam_info->value_2 != SR200PC20_VALUE_EXPOSURE )&&
		(cam_info->value_2 != SR200PC20_VALUE_WHITEBALANCE )&&
		(cam_info->value_2 != SR200PC20_VALUE_EFFECT )){
		if(cam_info->value_2 & SR200PC20_VALUE_EXPOSURE){
		    sr200pc20_ctrl.settings.exposure=value_mark_ev;
                    if (streamon == 1) {
                        sr200pc20_set_exposure_compensation(s_ctrl, value_mark_ev);
                    }
		}
		 /*Hal set WB and Effect together */
		if ((cam_info->value_2 & (SR200PC20_VALUE_WHITEBALANCE | SR200PC20_VALUE_EFFECT)) == 6){
			sr200pc20_ctrl.settings.effect = cam_info->value_3;
			value_mark_effect = sr200pc20_ctrl.settings.effect;
                        if (streamon == 1) {
			    sr200pc20_set_effect(s_ctrl, sr200pc20_ctrl.settings.effect);
                        }
			sr200pc20_ctrl.settings.wb = cam_info->value_1;
			value_mark_wb = sr200pc20_ctrl.settings.wb ;
                        if (streamon == 1) {
			    sr200pc20_set_white_balance(s_ctrl, sr200pc20_ctrl.settings.wb);
                        }
		}else{
                        if(cam_info->value_2 & SR200PC20_VALUE_EFFECT){
                                sr200pc20_ctrl.settings.effect=value_mark_effect;
                                if (streamon == 1) {
                                    sr200pc20_set_effect(s_ctrl, value_mark_effect);
                                }
                        }
			if(cam_info->value_2 & SR200PC20_VALUE_WHITEBALANCE){
				sr200pc20_ctrl.settings.wb=value_mark_wb;
                                if (streamon == 1) {
                                    sr200pc20_set_white_balance(s_ctrl, value_mark_wb);
                                }
			}
		}
	}else{
		switch (cam_info->mode) {
		case EXT_CAM_EV:
			sr200pc20_ctrl.settings.exposure = cam_info->value_1;
			value_mark_ev = sr200pc20_ctrl.settings.exposure;
                        if (streamon == 1) {
			    sr200pc20_set_exposure_compensation(s_ctrl, sr200pc20_ctrl.settings.exposure);
                        }
			break;
		case EXT_CAM_WB:
			sr200pc20_ctrl.settings.wb = cam_info->value_1;
			value_mark_wb = sr200pc20_ctrl.settings.wb;
                        if (streamon == 1) {
                            sr200pc20_set_white_balance(s_ctrl, sr200pc20_ctrl.settings.wb);
                        }
			break;
		case EXT_CAM_EFFECT:
			sr200pc20_ctrl.settings.effect = cam_info->value_1;
			value_mark_effect = sr200pc20_ctrl.settings.effect;
                        if (streamon == 1) {
			    sr200pc20_set_effect(s_ctrl, sr200pc20_ctrl.settings.effect);
                        }
			break;
		case EXT_CAM_SENSOR_MODE:
			sr200pc20_ctrl.prev_mode =  sr200pc20_ctrl.op_mode;
			sr200pc20_ctrl.op_mode = cam_info->value_1;
                        CDBG("EXT_CAM_SENSOR_MODE prev_mode=%d changed to op_mode=%d", sr200pc20_ctrl.prev_mode, cam_info->value_1);
			break;
		case EXT_CAM_EXIF:
			sr200pc20_get_exif_info(cam_info);
			if (!copy_to_user((void *)argp,
				(const void *)&cam_info,
				sizeof(cam_info)))
			pr_err("copy failed");
		break;
		case EXT_CAM_CONTRAST:
		#if 0
			sr200pc20_ctrl.settings.contrast = cam_info->value_1;
			sr200pc20_set_contrast(s_ctrl, sr200pc20_ctrl.settings.contrast);
		#endif
			break;
                case EXT_CAM_SCENE_MODE:
                        CDBG("EXT_CAM_SCENE_MODE value=%d", cam_info->value_1);
                        scene_mode_chg = 1;
                        sr200pc20_ctrl.settings.scenemode = cam_info->value_1;
                        if (streamon == 1) {
                            sr200pc20_set_scene_mode(s_ctrl, sr200pc20_ctrl.settings.scenemode);
                        }
                        break;
                case EXT_CAM_METERING:
                        sr200pc20_ctrl.settings.metering = cam_info->value_1;
                        if (streamon == 1) {
                            sr200pc20_set_metering(s_ctrl, sr200pc20_ctrl.settings.metering);
                        }
		default:
			break;
		}
	}
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	CDBG("EXIT \n ");
	return 0;
}

void sr200pc20_set_default_settings(void)
{
	sr200pc20_ctrl.settings.metering = CAMERA_CENTER_WEIGHT;
	sr200pc20_ctrl.settings.exposure = CAMERA_EV_DEFAULT;
	sr200pc20_ctrl.settings.wb = CAMERA_WHITE_BALANCE_AUTO;
	sr200pc20_ctrl.settings.iso = CAMERA_ISO_MODE_AUTO;
	sr200pc20_ctrl.settings.effect = CAMERA_EFFECT_OFF;
	sr200pc20_ctrl.settings.scenemode = CAMERA_SCENE_AUTO;
	sr200pc20_ctrl.settings.resolution = MSM_SENSOR_RES_FULL;
}


int sr200pc20_get_exif_data(struct msm_sensor_ctrl_t *s_ctrl,
			void __user *argp)
{
	*((exif_data_t *)argp) = sr200pc20_exif;
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
int32_t sr200pc20_get_exif_info(struct ioctl_native_cmd * exif_info)
{
	exif_info->value_1 = 1;	// equals 1 to update the exif value in the user level.
	exif_info->value_2 = sr200pc20_exif.iso;
	exif_info->value_3 = sr200pc20_exif.shutterspeed;
	return 0;
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
	u16 gain_value;

	/* ISO*/
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
		i2c_write(s_ctrl->sensor_i2c_client, 0x03,0x20,MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->
		i2c_read(s_ctrl->sensor_i2c_client, 0xb0,&read_value4,MSM_CAMERA_I2C_BYTE_DATA);

	gain_value = (u16)(((read_value4 * 1000L)>>5) + 500);

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

static int32_t sr200pc20_set_metering(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("mode = %d", mode);
	switch (mode) {
	case CAMERA_AVERAGE:
		rc = sr200pc20_WRT_LIST(s_ctrl, sr200pc20m_metering_matrix);
		break;
	case CAMERA_CENTER_WEIGHT:
		rc = sr200pc20_WRT_LIST(s_ctrl, sr200pc20m_metering_center);
		break;
	case CAMERA_SPOT:
		rc = sr200pc20_WRT_LIST(s_ctrl, sr200pc20m_metering_spot);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

#ifdef CONFIG_LOAD_FILE
int sr200_regs_from_sd_tuning(struct msm_camera_i2c_reg_conf *settings, struct msm_sensor_ctrl_t *s_ctrl,char * name)
{
    char *start, *end, *reg;
    int addr,rc = 0;
    unsigned int  value;
    char reg_buf[5], data_buf1[5];

    *(reg_buf + 4) = '\0';
    *(data_buf1 + 4) = '\0';

    if (settings != NULL){
        pr_err("sr200_regs_from_sd_tunning start address %x start data %x",
               settings->reg_addr,settings->reg_data);
    }

    if(sr200_regs_table == NULL) {
        pr_err("sr200_regs_table is null ");
        return -1;
    }
    pr_err("@@@ %s",name);
    start = strstr(sr200_regs_table, name);
    if (start == NULL){
        return -1;
    }
    end = strstr(start, "};");
    CDBG("Writing %s registers", name);
    while (1) {
        /* Find Address */
        reg = strstr(start, "{0x");
        if ((reg == NULL) || (reg > end))
            break;
        /* Write Value to Address */
        if (reg != NULL) {
            memcpy(reg_buf, (reg + 1), 4);
            memcpy(data_buf1, (reg + 7), 4);

            if(kstrtoint(reg_buf, 16, &addr))
                pr_err("kstrtoint error .Please Align contents of the Header file!!") ;

            if(kstrtoint(data_buf1, 16, &value))
                pr_err("kstrtoint error .Please Align contents of the Header file!!");

            if (reg)
                start = (reg + 14);

            if (addr == 0xff){
                usleep_range(value * 10, (value* 10) + 10);
                pr_err("delay = %d\n", (int)value*10);
            } else{
                rc=s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(s_ctrl->sensor_i2c_client, addr,
						                      value,MSM_CAMERA_I2C_BYTE_DATA);
            }
        }
    }
    pr_err("sr200_regs_from_sd_tunning end!");
    return rc;
}

void sr200_regs_table_exit(void)
{
    pr_info("%s:%d\n", __func__, __LINE__);
    if (sr200_regs_table) {
        vfree(sr200_regs_table);
        sr200_regs_table = NULL;
    }
}

void sr200_regs_table_init(char *filename)
{
    struct file *filp;
    char *dp;
    long lsize;
    loff_t pos;
    int ret;

    /*Get the current address space */
    mm_segment_t fs = get_fs();
    pr_err("%s %d", __func__, __LINE__);

    /*Set the current segment to kernel data segment */
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

    /*close the file*/
    filp_close(filp, current->files);

    /*restore the previous address space*/
    set_fs(fs);

    pr_err("coming to if part of string compare sr200_regs_table");
    sr200_regs_table = dp;
    sr200_regs_table_size = lsize;
    *((sr200_regs_table + sr200_regs_table_size) - 1) = '\0';

    return;
}
#endif
