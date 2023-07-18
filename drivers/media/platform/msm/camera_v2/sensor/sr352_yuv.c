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

#if 0
#define CONFIG_LOAD_FILE  // Enable it for Tunning Binary
#define NO_BURST // Enable no burst mode for Tunning
#endif

#include "sr352.h"
#if defined (CONFIG_MACH_MILLETWIFIUS_OPEN) || \
    defined (CONFIG_MACH_MILLETLTE_VZW) || \
    defined (CONFIG_MACH_MILLETLTE_ATT) || \
    defined (CONFIG_MACH_MILLETLTE_CAN) || \
    defined (CONFIG_MACH_MILLETLTE_TMO)
#include "sr352_yuv_millet_wifi_usa.h"
#elif defined (CONFIG_MACH_MATISSEWIFIUS_OPEN) || \
    defined (CONFIG_MACH_MATISSELTE_VZW) || \
    defined (CONFIG_MACH_MATISSELTE_ATT)
#include "sr352_yuv_matisse_wifi_usa.h"
#elif defined (CONFIG_SEC_MATISSE_PROJECT)
#include "sr352_yuv_matisse.h"
#elif defined (CONFIG_MACH_DEGASLTE_SPR)
#include "sr352_yuv_degas_spr.h"
#else
#include "sr352_yuv.h"
#endif

#if defined(CONFIG_MACH_MILLETWIFIUS_OPEN) || \
    defined (CONFIG_MACH_MATISSEWIFIUS_OPEN) || \
    defined (CONFIG_MACH_MILLETLTE_VZW) || \
    defined (CONFIG_MACH_MILLETLTE_ATT) || \
    defined (CONFIG_MACH_MILLETLTE_CAN) || \
    defined (CONFIG_MACH_MATISSELTE_VZW) || \
    defined (CONFIG_MACH_MATISSELTE_ATT) || \
    defined (CONFIG_MACH_DEGASLTE_SPR) || \
    defined (CONFIG_MACH_MILLETLTE_TMO)
#define ANTIBANDING_60HZ 1
//dummy declaration for 50HZ
//for successful compilation
#define sr352_50hz_setting sr352_60hz_setting
#define sr352_HD_50hz_setting sr352_HD_60hz_setting
#define sr352_AEAWB_Lock_50Hz sr352_AEAWB_Lock_60Hz
#define sr352_AEAWB_Unlock_50Hz sr352_AEAWB_Unlock_60Hz
#elif defined(CONFIG_MACH_MILLETWIFI_OPEN) || \
    defined(CONFIG_MACH_MILLET3G_EUR) || \
    defined(CONFIG_MACH_MATISSE3G_OPEN) || \
    defined(CONFIG_MACH_MATISSEWIFI_OPEN)
extern int back_camera_antibanding_get(void); /*add anti-banding code */
#define ANTIBANDING_60HZ (back_camera_antibanding_get() == 60)
#else
#define ANTIBANDING_60HZ 0
//dummy declaration for 60HZ
//for successful compilation
#define sr352_60hz_setting sr352_50hz_setting
#define sr352_HD_60hz_setting sr352_HD_50hz_setting
#define sr352_AEAWB_Lock_60Hz sr352_AEAWB_Lock_50Hz
#define sr352_AEAWB_Unlock_60Hz sr352_AEAWB_Unlock_50Hz
#endif


#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_dt_util.h"

#ifndef NO_BURST
#define BURST_MODE_BUFFER_MAX_SIZE 255
#define BURST_REG 0x0e
#define DELAY_REG 0xff
uint8_t burst_reg_data[BURST_MODE_BUFFER_MAX_SIZE];
static int32_t sr352_sensor_burst_write (struct msm_sensor_ctrl_t *s_ctrl, struct msm_camera_i2c_reg_conf *reg_settings , int size);
#endif

#ifdef CONFIG_LOAD_FILE
#define SR352_WRITE_LIST(A) \
					sr352_regs_from_sd_tunning(A,s_ctrl,#A);
#define SR352_WRITE_LIST_BURST(A) \
					sr352_regs_from_sd_tunning(A,s_ctrl,#A);
#else
#define SR352_WRITE_LIST(A) \
					s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_conf_tbl( \
						s_ctrl->sensor_i2c_client, A, \
						ARRAY_SIZE(A), \
						MSM_CAMERA_I2C_BYTE_DATA); CDBG("REGSEQ ****** %s", #A)
#define SR352_WRITE_LIST_BURST(A) \
					 sr352_sensor_burst_write(s_ctrl,A,ARRAY_SIZE(A)); CDBG("REGSEQ ****** BURST %s", #A)
#endif

#ifdef CONFIG_LOAD_FILE
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

static char *sr352_regs_table;
static int sr352_regs_table_size;
int sr352_regs_from_sd_tunning(struct msm_camera_i2c_reg_conf *settings, struct msm_sensor_ctrl_t *s_ctrl,char * name);
void sr352_regs_table_init(char *filename);
void sr352_regs_table_exit(void);
#endif

static struct yuv_ctrl sr352_ctrl;

static int32_t cur_scene_mode_chg = 0;
extern unsigned int system_rev;
unsigned int settings_type = 0;
void sr352_check_hw_revision(void);

void sr352_check_hw_revision()
{
    CDBG(" Hardware revision = %d\n",system_rev);
#if defined(CONFIG_MACH_MILLETLTE_VZW)|| defined (CONFIG_MACH_MILLETLTE_TMO)
            if(system_rev >= 1)
                settings_type = 1;
#elif defined (CONFIG_MACH_MILLETLTE_OPEN) ||  defined (CONFIG_MACH_MILLETLTE_KOR)
            if(system_rev >= 6)
                settings_type = 1;
#elif defined (CONFIG_MACH_MILLETWIFI_OPEN)
            if(system_rev >= 8)
                settings_type = 1;
#elif defined (CONFIG_MACH_MILLETWIFIUS_OPEN)
            if(system_rev >= 8)
                settings_type = 1;
#elif defined (CONFIG_MACH_MILLET3G_EUR)
            if(system_rev >= 7)
                settings_type = 1;
#elif defined (CONFIG_SEC_MATISSE_PROJECT) || \
      defined (CONFIG_MACH_MILLETLTE_ATT) || \
      defined (CONFIG_MACH_MILLETLTE_CAN) || \
      defined(CONFIG_SEC_DEGAS_PROJECT) || \
      defined (CONFIG_MACH_MILLETLTE_TMO)
                settings_type = 1;
#endif
}

// maximum number of chipid and slaveid pairs
// please change accordingly when you add more pairs
#define MAX_NUM_PAIRS 1

// Set of chipid and slaveid pairs not including the latest.
// Order: Latest but one to the oldest
// The latest values will be in the sensor lib file
// index 0 - chipid
// index 1 - slaveid
static uint16_t ids[MAX_NUM_PAIRS][2] = {{0xC2,0x40},};

int sr352_sensor_match_id(struct msm_camera_i2c_client *sensor_i2c_client,
	struct msm_camera_slave_info *slave_info,
	const char *sensor_name)
{
    uint16_t chipid = 0;
    int32_t i = 0;

    if (!sensor_i2c_client || !slave_info || !sensor_name) {
        pr_err("%s:%d failed: %p %p %p\n",__func__, __LINE__,
            sensor_i2c_client, slave_info,sensor_name);
        return -EINVAL;
    }

    sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client,
        slave_info->sensor_id_reg_addr,
        &chipid,
        sensor_i2c_client->data_type);

    if(chipid!=slave_info->sensor_id){
        pr_err("%s: chipid read=%x did not match with chipid=%x",
			__func__, chipid, slave_info->sensor_id);

        for( i=0; i<MAX_NUM_PAIRS; ++i){
            chipid = 0;
            sensor_i2c_client->cci_client->sid = ids[i][1] >> 1;
            sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client,
                slave_info->sensor_id_reg_addr,
                &chipid,
                sensor_i2c_client->data_type);
            if(chipid == ids[i][0]){
                break;
            }
            pr_err("%s: chipid read=%x did not match with chipid=%x",
			__func__, chipid, ids[i][0]);
        }
    }

    CDBG("%s sensor_name =%s slaveid = 0x%X sensorid = 0x%X DATA TYPE = %d\n",
        __func__, sensor_name, sensor_i2c_client->cci_client->sid,
        slave_info->sensor_id, sensor_i2c_client->data_type);

    return 0;
}

int32_t sr352_set_exposure_compensation(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("mode = %d", mode);
	switch (mode) {
	case CAMERA_EV_M4:
		rc = SR352_WRITE_LIST(sr352_brightness_M4);
		break;
	case CAMERA_EV_M3:
		rc = SR352_WRITE_LIST(sr352_brightness_M3);
		break;
	case CAMERA_EV_M2:
		rc = SR352_WRITE_LIST(sr352_brightness_M2);
		break;
	case CAMERA_EV_M1:
		rc = SR352_WRITE_LIST(sr352_brightness_M1);
		break;
	case CAMERA_EV_DEFAULT:
		rc = SR352_WRITE_LIST(sr352_brightness_default);
		break;
	case CAMERA_EV_P1:
		rc = SR352_WRITE_LIST(sr352_brightness_P1);
		break;
	case CAMERA_EV_P2:
		rc = SR352_WRITE_LIST(sr352_brightness_P2);
		break;
	case CAMERA_EV_P3:
		rc = SR352_WRITE_LIST(sr352_brightness_P3);
		break;
	case CAMERA_EV_P4:
		rc = SR352_WRITE_LIST(sr352_brightness_P4);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr352_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("mode = %d", mode);
	switch (mode) {
	case CAMERA_EFFECT_OFF:
		if(settings_type == 1) {
			rc = SR352_WRITE_LIST(sr352_effect_none_01);
		}
		else {
			rc = SR352_WRITE_LIST(sr352_effect_none);
		}
		break;
	case CAMERA_EFFECT_MONO:
		rc = SR352_WRITE_LIST(sr352_effect_gray);
		break;
	case CAMERA_EFFECT_NEGATIVE:
		rc = SR352_WRITE_LIST(sr352_effect_negative);
		break;
	case CAMERA_EFFECT_SEPIA:
		rc = SR352_WRITE_LIST(sr352_effect_sepia);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr352_set_scene_mode(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("Scene mode E = %d", mode);
	if(cur_scene_mode_chg == 0)
	{
		CDBG("Scene mode Not Set = %d", mode);
		return rc;
	}

	if(sr352_ctrl.prev_mode == CAMERA_MODE_INIT
	 && mode == CAMERA_SCENE_AUTO)
	{
		CDBG("Scene mode X Not Set = %d", mode);
		cur_scene_mode_chg = 0;
		return rc;
	}

	switch (mode) {
	case CAMERA_SCENE_AUTO:
		rc = SR352_WRITE_LIST(sr352_Scene_Off);
		break;
	case CAMERA_SCENE_LANDSCAPE:
		rc = SR352_WRITE_LIST(sr352_scene_Landscape);
		break;
	case CAMERA_SCENE_SPORT:
		rc = SR352_WRITE_LIST(sr352_scene_Sports);
		break;
	case CAMERA_SCENE_PARTY:
		rc = SR352_WRITE_LIST(sr352_scene_Party);
		break;
	case CAMERA_SCENE_BEACH:
		rc = SR352_WRITE_LIST(sr352_scene_Beach);
		break;
	case CAMERA_SCENE_SUNSET:
		rc = SR352_WRITE_LIST(sr352_scene_Sunset);
		break;
	case CAMERA_SCENE_DAWN:
		rc = SR352_WRITE_LIST(sr352_scene_Dawn);
		break;
	case CAMERA_SCENE_FALL:
		rc = SR352_WRITE_LIST(sr352_scene_Fall);
		break;
	case CAMERA_SCENE_CANDLE:
		rc = SR352_WRITE_LIST(sr352_scene_Candle);
		break;
	case CAMERA_SCENE_FIRE:
		rc = SR352_WRITE_LIST(sr352_scene_Firework);
		break;
	case CAMERA_SCENE_AGAINST_LIGHT:
		rc = SR352_WRITE_LIST(sr352_scene_Backlight);
		break;
	case CAMERA_SCENE_NIGHT:
		rc = SR352_WRITE_LIST(sr352_scene_Nightshot);
		break;

	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	cur_scene_mode_chg = 0;
	CDBG("Scene mode X = %d", mode);
	return rc;
}


int32_t sr352_set_ae_awb_lock(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
   int32_t rc = 0;
   CDBG("mode = %d", mode);

   if(ANTIBANDING_60HZ) {
      if(mode) {
         rc = SR352_WRITE_LIST(sr352_AEAWB_Lock_60Hz);
      } else {
         rc = SR352_WRITE_LIST(sr352_AEAWB_Unlock_60Hz);
      }
   } else {
      if(mode) {
         rc = SR352_WRITE_LIST(sr352_AEAWB_Lock_50Hz);
      } else {
         rc = SR352_WRITE_LIST(sr352_AEAWB_Unlock_50Hz);
      }
   }
   return rc;
}

int32_t sr352_set_white_balance(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("mode = %d", mode);
	switch (mode) {
	case CAMERA_WHITE_BALANCE_OFF:
	case CAMERA_WHITE_BALANCE_AUTO:
		rc = SR352_WRITE_LIST(sr352_wb_auto);
		break;
	case CAMERA_WHITE_BALANCE_INCANDESCENT:
		rc = SR352_WRITE_LIST(sr352_wb_incandescent);
		break;
	case CAMERA_WHITE_BALANCE_FLUORESCENT:
		rc = SR352_WRITE_LIST(sr352_wb_fluorescent);
		break;
	case CAMERA_WHITE_BALANCE_DAYLIGHT:
		rc = SR352_WRITE_LIST(sr352_wb_sunny);
		break;
	case CAMERA_WHITE_BALANCE_CLOUDY_DAYLIGHT:
		rc = SR352_WRITE_LIST(sr352_wb_cloudy);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr352_set_metering(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("mode = %d", mode);
	switch (mode) {
	case CAMERA_AVERAGE:
		rc = SR352_WRITE_LIST(sr352_metering_matrix);
		break;
	case CAMERA_CENTER_WEIGHT:
		rc = SR352_WRITE_LIST(sr352_metering_center);
		break;
	case CAMERA_SPOT:
		rc = SR352_WRITE_LIST(sr352_metering_spot);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
		rc = 0;
	}
	return rc;
}

int32_t sr352_set_resolution(struct msm_sensor_ctrl_t *s_ctrl, int mode)
{
	int32_t rc = 0;
	CDBG("mode = %d", mode);
	switch (mode) {
	case MSM_SENSOR_RES_FULL:
		if(settings_type == 1) {
			SR352_WRITE_LIST(sr352_Capture_2048_1536_01);
		}
		else {
			SR352_WRITE_LIST(sr352_Capture_2048_1536);
		}
		break;
	case MSM_SENSOR_RES_QTR:
		if(settings_type == 1) {
			SR352_WRITE_LIST(sr352_Enterpreview_1024x768_01);
		}
		else {
			SR352_WRITE_LIST(sr352_Enterpreview_1024x768);
		}
		break;
	case MSM_SENSOR_RES_2:
		if(settings_type == 1) {
			SR352_WRITE_LIST(sr352_Enterpreview_1024x576_01);
		}
		else {
			SR352_WRITE_LIST(sr352_Enterpreview_1024x576);
		}
		break;
	case MSM_SENSOR_RES_3:
		if(settings_type == 1) {
			rc = SR352_WRITE_LIST_BURST(sr352_recording_50Hz_HD_01);
		}
		else {
			rc = SR352_WRITE_LIST_BURST(sr352_recording_50Hz_HD);
		}
                if(ANTIBANDING_60HZ) {
                        rc = SR352_WRITE_LIST(sr352_HD_60hz_setting);
                } else {
                        rc = SR352_WRITE_LIST(sr352_HD_50hz_setting);
		}
		break;
	case MSM_SENSOR_RES_4:
		rc = SR352_WRITE_LIST(sr352_preview_800_480);
		break;
	case MSM_SENSOR_RES_5:
		if(settings_type == 1) {
			SR352_WRITE_LIST(sr352_Enterpreview_640x480_01);
		}
		else {
			SR352_WRITE_LIST(sr352_Enterpreview_640x480);
		}
		break;
	case MSM_SENSOR_RES_6:
		rc = SR352_WRITE_LIST(sr352_preview_320_240);
		break;
	case MSM_SENSOR_RES_7:
		rc = SR352_WRITE_LIST(sr352_preview_176_144);
		break;
	default:
		pr_err("%s: Setting %d is invalid\n", __func__, mode);
	}
	return rc;
}

void sr352_init_camera(struct msm_sensor_ctrl_t *s_ctrl)
{
   int32_t rc = 0;
   if(settings_type == 1) {
      rc = SR352_WRITE_LIST_BURST(sr352_Init_Reg_01);
   } else {
      rc = SR352_WRITE_LIST_BURST(sr352_Init_Reg);
   }
   if(rc <0) {
      pr_err("%s:%d error writing initsettings failed\n", __func__, __LINE__);
   }
   if(ANTIBANDING_60HZ) {
      rc = SR352_WRITE_LIST(sr352_60hz_setting);
   } else {
      rc = SR352_WRITE_LIST(sr352_50hz_setting);
   }
   if(rc <0) {
      pr_err("%s:%d error writing 50hz failed\n", __func__, __LINE__);
   }
}

int32_t sr352_get_exif(struct ioctl_native_cmd * exif_info)
{
	exif_info->value_1 = 1;	// equals 1 to update the exif value in the user level.
	exif_info->value_2 = sr352_ctrl.exif_iso;
	exif_info->value_3 = sr352_ctrl.exif_shutterspeed;
	return 0;
}

int32_t sr352_set_exif(struct msm_sensor_ctrl_t *s_ctrl )
{
	int32_t rc = 0;
	uint16_t read_value0 = 0;
	uint16_t read_value1 = 0;
	uint16_t read_value2 = 0;
	uint16_t read_value3 = 0;
	uint16_t read_value4 = 0;

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
				s_ctrl->sensor_i2c_client,
				0x03,
				0x20,
				MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client, 0xa0,
				&read_value0,
				MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client, 0xa1,
				&read_value1,
				MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client, 0xa2,
				&read_value2,
				MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client, 0xa3,
				&read_value3,
				MSM_CAMERA_I2C_BYTE_DATA);

	sr352_ctrl.exif_shutterspeed = 27000000 / ((read_value0 << 24)
		+ (read_value1 << 16) + (read_value2 << 8) + read_value3);

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
				s_ctrl->sensor_i2c_client,
				0x03,
				0x20,
				MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client, 0x50,
				&read_value4,
				MSM_CAMERA_I2C_BYTE_DATA);

	if (read_value4 < 0x26)
		sr352_ctrl.exif_iso= 50;
	else if (read_value4 < 0x5C)
		sr352_ctrl.exif_iso = 100;
	else if (read_value4 < 0x83)
		sr352_ctrl.exif_iso = 200;
	else
		sr352_ctrl.exif_iso = 400;

	pr_debug("sr352_set_exif: ISO = %d shutter speed = %d",sr352_ctrl.exif_iso,sr352_ctrl.exif_shutterspeed);
	return rc;
}

int32_t sr352_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	int32_t rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);

	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		CDBG(" CFG_GET_SENSOR_INFO");
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++)
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
		CDBG("sensor name %s",
			cdata->cfg.sensor_info.sensor_name);
		CDBG("session id %d",
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("subdev_id[%d] %d", i,
				cdata->cfg.sensor_info.subdev_id[i]);

		break;
	case CFG_SET_INIT_SETTING:
		 CDBG("CFG_SET_INIT_SETTING");
#ifdef CONFIG_LOAD_FILE /* this call should be always called first */
			sr352_regs_table_init("/data/sr352_yuv.h");
			pr_err("/data/sr352_yuv.h inside CFG_SET_INIT_SETTING");
#endif
		 sr352_init_camera(s_ctrl);
	         //Stop stream and start in START_STREAM
		 CDBG("Stop stream after Init");
		 SR352_WRITE_LIST(sr352_stop_stream);
		break;
	case CFG_SET_RESOLUTION:
		if(sr352_ctrl.prev_mode == CAMERA_MODE_RECORDING && sr352_ctrl.settings.resolution == MSM_SENSOR_RES_3)
		{
			sr352_init_camera(s_ctrl);
			//Stop stream and start in START_STREAM
			SR352_WRITE_LIST(sr352_stop_stream);
			CDBG("CFG CFG_SET_RESOLUTION - HD Recording mode off");
			cur_scene_mode_chg = 1;
		}
		else if(sr352_ctrl.prev_mode == CAMERA_MODE_RECORDING && sr352_ctrl.settings.resolution != MSM_SENSOR_RES_3)
		{
			if(settings_type == 1) {
				rc = SR352_WRITE_LIST(sr352_recording_50Hz_modeOff_01);
			}
			else {
				rc = SR352_WRITE_LIST(sr352_recording_50Hz_modeOff);
			}
			rc = SR352_WRITE_LIST(sr352_stop_stream);
			CDBG("CFG CFG_SET_RESOLUTION - Non - HD Recording mode off");
		}
		sr352_ctrl.settings.prev_resolution = sr352_ctrl.settings.resolution;
		sr352_ctrl.settings.resolution = *((int32_t  *)cdata->cfg.setting);
		CDBG("CFG_SET_RESOLUTION  res = %d" , sr352_ctrl.settings.resolution);
		if(sr352_ctrl.streamon == 0)
		{
			switch(sr352_ctrl.op_mode) {
				case CAMERA_MODE_PREVIEW:
				{
					sr352_set_resolution(s_ctrl , sr352_ctrl.settings.resolution);
					if(sr352_ctrl.prev_mode == CAMERA_MODE_RECORDING && sr352_ctrl.settings.prev_resolution == MSM_SENSOR_RES_3)
					{
						sr352_set_ae_awb_lock(s_ctrl, 0);
						msleep(100);
					}
					if(sr352_ctrl.prev_mode == CAMERA_MODE_INIT)
					{
						msleep(100);
					}
				}
				break;
				case CAMERA_MODE_CAPTURE:
				{
					sr352_set_resolution(s_ctrl , sr352_ctrl.settings.resolution);
				}
				break;

				case  CAMERA_MODE_RECORDING:
				{
					if( sr352_ctrl.settings.resolution == MSM_SENSOR_RES_3 )
					{
						CDBG("CFG writing *** sr352_recording_50Hz_HD");
						sr352_set_resolution( s_ctrl , sr352_ctrl.settings.resolution);
					} else {
						CDBG("CFG writing *** sr352_recording_50Hz_30fps");
						sr352_set_resolution( s_ctrl , MSM_SENSOR_RES_5);
						if(settings_type == 1) {
							rc = SR352_WRITE_LIST(sr352_recording_50Hz_30fps_01);
						}
						else {
							rc = SR352_WRITE_LIST(sr352_recording_50Hz_30fps);
						}
						//msleep(100);
					}
				}
				break;
			}
		}
		break;
	case CFG_SET_STOP_STREAM:
		if(sr352_ctrl.streamon == 1) {
			CDBG(" CFG_SET_STOP_STREAM writing stop stream registers: sr352_stop_stream");
			sr352_ctrl.streamon = 0;
		}
		break;
	case CFG_SET_START_STREAM:
	{
		CDBG(" CFG_SET_START_STREAM writing start stream registers: sr352_start_stream start");
		switch(sr352_ctrl.op_mode) {
			case  CAMERA_MODE_PREVIEW:
			{
				CDBG(" CFG_SET_START_STREAM: Preview");
				if(sr352_ctrl.prev_mode != CAMERA_MODE_CAPTURE)
				{
					sr352_set_scene_mode(s_ctrl, sr352_ctrl.settings.scenemode);
					sr352_set_exposure_compensation(s_ctrl, sr352_ctrl.settings.exposure);
					sr352_set_effect(s_ctrl , sr352_ctrl.settings.effect);
					sr352_set_white_balance(s_ctrl , sr352_ctrl.settings.wb);
					sr352_set_metering(s_ctrl, sr352_ctrl.settings.metering );
				}
			}
			break;

			case CAMERA_MODE_CAPTURE:
			{
				CDBG("CFG_SET_START_STREAM: Capture");
				sr352_set_exif(s_ctrl);
			}
			break;

			case CAMERA_MODE_RECORDING:
			{
                                sr352_set_exposure_compensation(s_ctrl, sr352_ctrl.settings.exposure);
                                sr352_set_effect(s_ctrl , sr352_ctrl.settings.effect);
                                sr352_set_white_balance(s_ctrl , sr352_ctrl.settings.wb);
                                sr352_set_metering(s_ctrl, sr352_ctrl.settings.metering );
			}
			break;
		}
		sr352_ctrl.streamon = 1;
		break;
	}
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info sensor_slave_info;
		struct msm_camera_power_ctrl_t *p_ctrl;
		uint16_t size;
		int slave_index = 0;
		CDBG("CFG_SET_SLAVE_INFO");
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
		CDBG("slave_addr = 0x%x, addr_type = %d, sensor_id_reg_addr = 0x%x, sensor_id %x", \
		sensor_slave_info.slave_addr, sensor_slave_info.addr_type, \
		sensor_slave_info.sensor_id_info.sensor_id_reg_addr, sensor_slave_info.sensor_id_info.sensor_id);
		for (slave_index = 0; slave_index <
			p_ctrl->power_setting_size; slave_index++) {
			CDBG("i %d power setting %d %d %ld %d",
				slave_index,
				p_ctrl->power_setting[slave_index].seq_type,
				p_ctrl->power_setting[slave_index].seq_val,
				p_ctrl->power_setting[slave_index].config_val,
				p_ctrl->power_setting[slave_index].delay);
		}
		CDBG("CFG_SET_SLAVE_INFO EXIT");
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

		CDBG("CFG_WRITE_I2C_SEQ_ARRAY");

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
		CDBG(" CFG_POWER_UP");
		sr352_ctrl.streamon = 0;
		sr352_ctrl.op_mode = CAMERA_MODE_INIT;
		sr352_ctrl.prev_mode = CAMERA_MODE_INIT;
		sr352_ctrl.settings.prev_resolution = MSM_SENSOR_RES_FULL;
		sr352_ctrl.settings.resolution = MSM_SENSOR_RES_FULL;
		sr352_check_hw_revision();
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
		CDBG("CFG_POWER_DOWN");
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
			sr352_regs_table_exit();
		#endif
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		CDBG("CFG_SET_STOP_STREAM_SETTING");
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


int32_t sr352_sensor_native_control(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	int32_t rc = 0;
	struct ioctl_native_cmd *cam_info = (struct ioctl_native_cmd *)argp;

	mutex_lock(s_ctrl->msm_sensor_mutex);

	/*CDBG("cam_info values = %d : %d : %d : %d : %d\n", cam_info->mode, cam_info->address, cam_info->value_1, cam_info->value_2 , cam_info->value_3);*/
	switch (cam_info->mode) {
	case EXT_CAM_EV:
		sr352_ctrl.settings.exposure = (cam_info->value_1);
		if(sr352_ctrl.streamon == 1)
			sr352_set_exposure_compensation(s_ctrl, sr352_ctrl.settings.exposure);
		break;
	case EXT_CAM_WB:
		sr352_ctrl.settings.wb = (cam_info->value_1);
		if(sr352_ctrl.streamon == 1)
			sr352_set_white_balance(s_ctrl, sr352_ctrl.settings.wb);
		break;
	case EXT_CAM_METERING:
		sr352_ctrl.settings.metering = (cam_info->value_1);
		if(sr352_ctrl.streamon == 1)
			sr352_set_metering(s_ctrl, sr352_ctrl.settings.metering);
		break;
	case EXT_CAM_EFFECT:
		sr352_ctrl.settings.effect = (cam_info->value_1);
		if(sr352_ctrl.streamon == 1)
			sr352_set_effect(s_ctrl, sr352_ctrl.settings.effect);
		break;
	case EXT_CAM_SCENE_MODE:
		sr352_ctrl.settings.scenemode = (cam_info->value_1);
		cur_scene_mode_chg = 1;
		if(sr352_ctrl.streamon == 1)
			sr352_set_scene_mode(s_ctrl, sr352_ctrl.settings.scenemode);
		break;
	case EXT_CAM_SENSOR_MODE:
		sr352_ctrl.prev_mode =  sr352_ctrl.op_mode;
		sr352_ctrl.op_mode = (cam_info->value_1);
		pr_info("EXT_CAM_SENSOR_MODE = %d", sr352_ctrl.op_mode);
		break;
	case EXT_CAM_EXIF:
		sr352_get_exif(cam_info);
			if (!copy_to_user((void *)argp,
	                (const void *)&cam_info,
					sizeof(cam_info)))
			pr_err("copy failed");

		break;
	case EXT_CAM_SET_AE_AWB:
		sr352_ctrl.settings.aeawblock = cam_info->value_1;
		sr352_set_ae_awb_lock(s_ctrl, sr352_ctrl.settings.aeawblock);
		break;
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

void sr352_set_default_settings(void)
{
	sr352_ctrl.settings.metering = CAMERA_CENTER_WEIGHT;
	sr352_ctrl.settings.exposure = CAMERA_EV_DEFAULT;
	sr352_ctrl.settings.wb = CAMERA_WHITE_BALANCE_AUTO;
	sr352_ctrl.settings.iso = CAMERA_ISO_MODE_AUTO;
	sr352_ctrl.settings.effect = CAMERA_EFFECT_OFF;
	sr352_ctrl.settings.scenemode = CAMERA_SCENE_AUTO;
	sr352_ctrl.settings.aeawblock = 0;
}

#ifndef NO_BURST
int32_t sr352_sensor_burst_write(struct msm_sensor_ctrl_t *s_ctrl, struct msm_camera_i2c_reg_conf *reg_settings , int size)
{
	int i;
	int idx=0;
	int burst_flag = 0;
	int seq_flag = 0;
	int err = -EINVAL;
	unsigned char subaddr;
	unsigned char value;
	struct msm_camera_i2c_reg_conf def_reg_settings[]= { {0x00, 0x00,},};

        // burst write related variables
	struct msm_camera_i2c_burst_reg_array burst_reg_setting = { 0 ,};
	struct msm_camera_i2c_reg_setting conf_array = {
		.reg_setting = (void * ) &burst_reg_setting,
		.size = 1,
		.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
		.data_type = MSM_CAMERA_I2C_BURST_DATA,
	};
	
        // seq write related variables
	struct msm_camera_i2c_seq_reg_array *seq_reg_setting  = NULL;
	struct msm_camera_i2c_seq_reg_setting seq_conf_array = {
		.reg_setting = NULL,
		.size = 0,
		.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
		.delay = 0,
	};
	CDBG("E");
	// allocate memory for seq reg setting
	// I2C_SEQ_REG_DATA_MAX ,defined in msm_cam_sensor.h, is the limit for number of bytes
	// which can be written in one seq_write call
	seq_reg_setting = kzalloc( (size/I2C_SEQ_REG_DATA_MAX + 1) *(sizeof(struct msm_camera_i2c_seq_reg_array)), GFP_KERNEL); // allocate maximum possible
	if (!seq_reg_setting) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		err = -ENOMEM;
		goto on_error;
	}
	seq_conf_array.reg_setting = seq_reg_setting;
	
        // read the register settings array
	for (i = 0; i < size; ++i) {
		if (idx > (BURST_MODE_BUFFER_MAX_SIZE )) {
			pr_err( "[%s:%d]Burst mode buffer overflow! "
				"Byte Count %d\n",
				__func__, __LINE__, i);    		
			err = -EIO;
			goto on_error;
		}

		// the register address
		subaddr = reg_settings[i].reg_addr;

		// the register value
		value = reg_settings[i].reg_data;
		
		if (burst_flag == 0) {
			if ( (subaddr == BURST_REG) && value != 0x00) {
				// set the burst flag since we encountered the burst start reg
				burst_flag = 1;
			}
			
			// reset the byte count
			idx = 0;

			// write the register setting
			def_reg_settings[0].reg_addr = subaddr;
			def_reg_settings[0].reg_data = value;
			err = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client,  def_reg_settings,
				ARRAY_SIZE( def_reg_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
		} else if (burst_flag == 1) {
			if (subaddr == BURST_REG && value == 0x00) {
				// End Burst flag encountered 
						
				if(seq_flag){
					//seq data case
					s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_seq_table(s_ctrl->sensor_i2c_client, &seq_conf_array);
					if (err < 0) {
						pr_err("[%s:%d]Sequential write fail!\n", __func__, __LINE__);
						goto on_error;
					}
				} else{
					//burst data case
					burst_reg_setting.reg_data = burst_reg_data;
					burst_reg_setting.reg_data_size = idx;
					
					err = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(s_ctrl->sensor_i2c_client, &conf_array);
					if (err < 0) {
						pr_err("[%s:%d]Burst write fail!\n", __func__, __LINE__);
						goto on_error;
					}
				}
				
				// reset the byte count and the flags
				idx = 0;
				burst_flag = 0;
				seq_flag = 0;
				
				def_reg_settings[0].reg_addr = subaddr;
				def_reg_settings[0].reg_data = value;
				err = s_ctrl->sensor_i2c_client->i2c_func_tbl->
					i2c_write_conf_tbl(
					s_ctrl->sensor_i2c_client,  def_reg_settings,
					ARRAY_SIZE( def_reg_settings),
					MSM_CAMERA_I2C_BYTE_DATA);
				if (err < 0) {
					pr_err("[%s:%d]Burst write fail!\n", __func__, __LINE__);
					goto on_error;
				}
			} else {
				if (idx == 0) {
					// Zeroth byte in the burst data
					burst_reg_setting.reg_addr = subaddr;
					burst_reg_data[idx++] = value;
				} else {
					if(idx==1 && burst_reg_setting.reg_addr!=subaddr){
						// The burst data was found to be sequenced
						seq_flag = 1;
						seq_reg_setting[0].reg_addr = burst_reg_setting.reg_addr;
						seq_reg_setting[0].reg_data[0] = burst_reg_data[0];
						seq_reg_setting[0].reg_data[1] = value;
						++idx;
					} else {
						if(!seq_flag){
							burst_reg_data[idx++] = value;
						} else{
							// The seq data is split into blocks of I2C_SEQ_REG_DATA_MAX
							if(idx % I2C_SEQ_REG_DATA_MAX == 0){
								seq_reg_setting[idx/I2C_SEQ_REG_DATA_MAX].reg_addr = subaddr;
							}
							seq_reg_setting[idx/I2C_SEQ_REG_DATA_MAX].reg_data[idx%I2C_SEQ_REG_DATA_MAX] = value;
							seq_reg_setting[idx/I2C_SEQ_REG_DATA_MAX].reg_data_size = (idx % I2C_SEQ_REG_DATA_MAX ) + 1;
							seq_conf_array.size = idx/I2C_SEQ_REG_DATA_MAX+1;
							++idx;
						}
					}
				}
			}
		}
	}
	
	on_error:
		if (unlikely(err < 0)) {
			pr_err("[%s:%d] register set failed\n", __func__, __LINE__);
		}
	kfree(seq_reg_setting);
	CDBG("X");
	return err;
}
#endif

#ifdef CONFIG_LOAD_FILE
int sr352_regs_from_sd_tunning(struct msm_camera_i2c_reg_conf *settings, struct msm_sensor_ctrl_t *s_ctrl,char * name) {
	char *start, *end, *reg;
	int addr,rc = 0;
	unsigned int  value;
	char reg_buf[5], data_buf1[5];

	*(reg_buf + 4) = '\0';
	*(data_buf1 + 4) = '\0';

	if (settings != NULL){
		pr_err("sr352_regs_from_sd_tunning start address %x start data %x",settings->reg_addr,settings->reg_data);
	}

	if(sr352_regs_table == NULL) {
		pr_err("sr352_regs_table is null ");
		return -1;
	}
	pr_err("@@@ %s",name);
	start = strstr(sr352_regs_table, name);
	if (start == NULL){
		return -1;
	}
	end = strstr(start, "};");
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

		}
		else{
			rc=s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write(s_ctrl->sensor_i2c_client, addr,
						value,MSM_CAMERA_I2C_BYTE_DATA);
			}
		}
	}
	pr_err("sr352_regs_from_sd_tunning end!");
	return rc;
}

void sr352_regs_table_exit(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (sr352_regs_table) {
		vfree(sr352_regs_table);
		sr352_regs_table = NULL;
	}

}


void sr352_regs_table_init(char *filename)
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

	pr_err("coming to if part of string compare sr352_regs_table");
	sr352_regs_table = dp;
	sr352_regs_table_size = lsize;
	*((sr352_regs_table + sr352_regs_table_size) - 1) = '\0';

	return;
}

#endif
