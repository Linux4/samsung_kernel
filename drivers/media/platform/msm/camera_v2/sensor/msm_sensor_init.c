/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "MSM-SENSOR-INIT %s:%d " fmt "\n", __func__, __LINE__

/* Header files */
#include <linux/device.h>
#include "msm_sensor_init.h"
#include "msm_sensor_driver.h"
#include "msm_sensor.h"
#include "msm_sd.h"

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
#include "msm_sensor.h"
#endif

/* Logging macro */
#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

extern struct kset *devices_kset;
static struct msm_sensor_init_t *s_init;
struct class *camera_class;
static struct v4l2_file_operations msm_sensor_init_v4l2_subdev_fops;
/* Static function declaration */
static long msm_sensor_init_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg);

/* Static structure declaration */
static struct v4l2_subdev_core_ops msm_sensor_init_subdev_core_ops = {
	.ioctl = msm_sensor_init_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_sensor_init_subdev_ops = {
	.core = &msm_sensor_init_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_sensor_init_internal_ops;

static int msm_sensor_wait_for_probe_done(struct msm_sensor_init_t *s_init)
{
	int rc;
	int tm = 10000;
	if (s_init->module_init_status == 1) {
		CDBG("msm_cam_get_module_init_status -2\n");
		return 0;
	}
	rc = wait_event_timeout(s_init->state_wait,
		(s_init->module_init_status == 1), msecs_to_jiffies(tm));
	if (rc == 0)
		pr_err("%s:%d wait timeout\n", __func__, __LINE__);

	return rc;
}

/* Static function definition */
static int32_t msm_sensor_driver_cmd(struct msm_sensor_init_t *s_init,
	void *arg)
{
	int32_t rc = 0;
	struct sensor_init_cfg_data *cfg = (struct sensor_init_cfg_data *)arg;
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	int bhwb_rc = 0;
#endif

	/* Validate input parameters */
	if (!s_init || !cfg) {
		pr_err("failed: s_init %pK cfg %pK", s_init, cfg);
		return -EINVAL;
	}

	switch (cfg->cfgtype) {
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
		case CFG_SINIT_HWB:
			bhwb_rc = msm_is_sec_file_exist(CAM_HW_ERR_CNT_FILE_PATH, HW_PARAMS_CREATED);
			if (bhwb_rc == 1) {
				msm_is_sec_copy_err_cnt_from_file();
			}
			rc = 0;
			break;
#endif

		case CFG_SINIT_PROBE:
			mutex_lock(&s_init->imutex);
			s_init->module_init_status = 0;
			rc = msm_sensor_driver_probe(cfg->cfg.setting,
				&cfg->probed_info,
				cfg->entity_name);
			mutex_unlock(&s_init->imutex);
			if (rc < 0)
				pr_err("%s failed (non-fatal) rc %d", __func__, rc);
			break;

		case CFG_SINIT_PROBE_DONE:
			s_init->module_init_status = 1;
			wake_up(&s_init->state_wait);
			break;

		case CFG_SINIT_PROBE_WAIT_DONE:
			msm_sensor_wait_for_probe_done(s_init);
			break;

		default:
			pr_err("default");
			break;
	}

	return rc;
}

static long msm_sensor_init_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	long rc = 0;
	struct msm_sensor_init_t *s_init = v4l2_get_subdevdata(sd);
	CDBG("Enter");

	/* Validate input parameters */
	if (!s_init) {
		pr_err("failed: s_init %pK", s_init);
		return -EINVAL;
	}

	switch (cmd) {
	case VIDIOC_MSM_SENSOR_INIT_CFG:
		rc = msm_sensor_driver_cmd(s_init, arg);
		break;

	default:
		pr_err_ratelimited("default\n");
		break;
	}

	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_sensor_init_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	int32_t             rc = 0;
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	struct sensor_init_cfg_data32 *u32 =
		(struct sensor_init_cfg_data32 *)arg;
	struct sensor_init_cfg_data sensor_init_data;

	switch (cmd) {
	case VIDIOC_MSM_SENSOR_INIT_CFG32:
		memset(&sensor_init_data, 0, sizeof(sensor_init_data));
		sensor_init_data.cfgtype = u32->cfgtype;
		sensor_init_data.cfg.setting = compat_ptr(u32->cfg.setting);
		cmd = VIDIOC_MSM_SENSOR_INIT_CFG;
		rc = msm_sensor_init_subdev_ioctl(sd, cmd, &sensor_init_data);
		if (rc < 0) {
			pr_err("%s:%d VIDIOC_MSM_SENSOR_INIT_CFG failed (non-fatal)",
				__func__, __LINE__);
			return rc;
		}
		u32->probed_info = sensor_init_data.probed_info;
		strlcpy(u32->entity_name, sensor_init_data.entity_name,
			sizeof(sensor_init_data.entity_name));
		return 0;
	default:
		return msm_sensor_init_subdev_ioctl(sd, cmd, arg);
	}
}

static long msm_sensor_init_subdev_fops_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_sensor_init_subdev_do_ioctl);
}
#endif

char cam_fw_ver[40] = "NULL NULL\n";//multi module
static ssize_t back_camera_firmware_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] cam_fw_ver : %s\n", cam_fw_ver);
	return snprintf(buf, sizeof(cam_fw_ver), "%s", cam_fw_ver);
}

static ssize_t back_camera_firmware_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(cam_fw_ver, sizeof(cam_fw_ver), "%s", buf);

	return size;
}

static ssize_t back_camera_type_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char cam_type_lsi[] = "SLSI_S5K2L2SA\n";
	char cam_type_sony[] = "SONY_IMX333\n";

  if (cam_fw_ver[4] == 'L') {
    return snprintf(buf, sizeof(cam_type_lsi), "%s", cam_type_lsi);
  } else {
    return snprintf(buf, sizeof(cam_type_sony), "%s", cam_type_sony);
  }
}

static ssize_t front_camera_type_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_SEC_KELLYLTE_PROJECT)
	char cam_type[] = "LSI_S5K5E3YX\n";
#else
	char cam_type[] = "SONY_IMX320\n";
#endif
	return snprintf(buf, sizeof(cam_type), "%s", cam_type);
}

char cam_fw_user_ver[40] = "NULL\n";//multi module
static ssize_t back_camera_firmware_user_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] cam_fw_ver : %s\n", cam_fw_user_ver);
	return snprintf(buf, sizeof(cam_fw_user_ver), "%s", cam_fw_user_ver);
}

static ssize_t back_camera_firmware_user_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(cam_fw_user_ver, sizeof(cam_fw_user_ver), "%s", buf);

	return size;
}

char cam_fw_factory_ver[40] = "NULL\n";//multi module
static ssize_t back_camera_firmware_factory_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] cam_fw_ver : %s\n", cam_fw_factory_ver);
	return snprintf(buf, sizeof(cam_fw_factory_ver), "%s", cam_fw_factory_ver);
}

static ssize_t back_camera_firmware_factory_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(cam_fw_factory_ver, sizeof(cam_fw_factory_ver), "%s", buf);

	return size;
}

char cam_fw_full_ver[40] = "NULL NULL NULL\n";//multi module
static ssize_t back_camera_firmware_full_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] cam_fw_ver : %s\n", cam_fw_full_ver);
	return snprintf(buf, sizeof(cam_fw_full_ver), "%s", cam_fw_full_ver);
}

static ssize_t back_camera_firmware_full_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(cam_fw_full_ver, sizeof(cam_fw_full_ver), "%s", buf);

	return size;
}

char cam_load_fw[25] = "NULL\n";
static ssize_t back_camera_firmware_load_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] cam_load_fw : %s\n", cam_load_fw);
	return snprintf(buf, sizeof(cam_load_fw), "%s", cam_load_fw);
}

static ssize_t back_camera_firmware_load_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(cam_load_fw, sizeof(cam_load_fw), "%s\n", buf);
	return size;
}

#if defined(CONFIG_COMPANION3)
char companion_fw_ver[40] = "NULL NULL NULL\n";
static ssize_t back_companion_firmware_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] companion_fw_ver : %s\n", companion_fw_ver);
	return snprintf(buf, sizeof(companion_fw_ver), "%s", companion_fw_ver);
}

static ssize_t back_companion_firmware_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(companion_fw_ver, sizeof(companion_fw_ver), "%s", buf);

	return size;
}
#endif

char fw_crc[10] = "NN\n";//camera and companion
static ssize_t back_fw_crc_check_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] fw_crc : %s\n", fw_crc);
	return snprintf(buf, sizeof(fw_crc), "%s", fw_crc);
}

static ssize_t back_fw_crc_check_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(fw_crc, sizeof(fw_crc), "%s", buf);

	return size;
}

char cal_crc[37] = "NULL NULL NULL\n";
static ssize_t back_cal_data_check_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] cal_crc : %s\n", cal_crc);
	return snprintf(buf, sizeof(cal_crc), "%s", cal_crc);
}

static ssize_t back_cal_data_check_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(cal_crc, sizeof(cal_crc), "%s", buf);

	return size;
}

char module_info[40] = "NULL\n";
static ssize_t back_module_info_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] module_info : %s\n", module_info);
	return snprintf(buf, sizeof(module_info), "%s", module_info);
}

static ssize_t back_module_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(module_info, sizeof(module_info), "%s", buf);

	return size;
}

char front_module_info[40] = "NULL\n";
static ssize_t front_module_info_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] front_module_info : %s\n", front_module_info);
	return snprintf(buf, sizeof(front_module_info), "%s", front_module_info);
}

static ssize_t front_module_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(front_module_info, sizeof(front_module_info), "%s", buf);

	return size;
}

char isp_core[10];
static ssize_t back_isp_core_check_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
#if 0// Power binning is used
	char cam_isp_core[] = "0.8V\n";
	return snprintf(buf, sizeof(cam_isp_core), "%s", cam_isp_core);
#else
	CDBG("[FW_DBG] isp_core : %s\n", isp_core);
	return snprintf(buf, sizeof(isp_core), "%s\n", isp_core);
#endif
}

static ssize_t back_isp_core_check_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(isp_core, sizeof(isp_core), "%s", buf);

	return size;
}


#define FROM_REAR_AF_CAL_SIZE	 10
int rear_af_cal[FROM_REAR_AF_CAL_SIZE + 1] = {0,};
static ssize_t back_camera_afcal_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_SAMSUNG_MULTI_CAMERA)
	char tempbuf[10];
	char N[] = "N ";

	strncat(buf, "10 ", strlen("10 "));

#ifdef	FROM_REAR_AF_CAL_D10_ADDR
	sprintf(tempbuf, "%d ", rear_af_cal[1]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR_AF_CAL_D20_ADDR
	sprintf(tempbuf, "%d ", rear_af_cal[2]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR_AF_CAL_D30_ADDR
	sprintf(tempbuf, "%d ", rear_af_cal[3]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR_AF_CAL_D40_ADDR
	sprintf(tempbuf, "%d ", rear_af_cal[4]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR_AF_CAL_D50_ADDR
	sprintf(tempbuf, "%d ", rear_af_cal[5]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR_AF_CAL_D60_ADDR
	sprintf(tempbuf, "%d ", rear_af_cal[6]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR_AF_CAL_D70_ADDR
	sprintf(tempbuf, "%d ", rear_af_cal[7]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR_AF_CAL_D80_ADDR
	sprintf(tempbuf, "%d ", rear_af_cal[8]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR_AF_CAL_PAN_ADDR
	sprintf(tempbuf, "%d ", rear_af_cal[9]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

	return strlen(buf);
#else
	return sprintf(buf, "1 %d %d\n", rear_af_cal[0], rear_af_cal[9]);
#endif
}

uint32_t front_af_cal_pan;
uint32_t front_af_cal_macro;
#if defined(CONFIG_SEC_GTS4LLTE_PROJECT) || defined(CONFIG_SEC_GTS4LWIFI_PROJECT)
static ssize_t front_camera_afcal_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] front_af_cal_pan: %d, front_af_cal_macro: %d\n", front_af_cal_pan, front_af_cal_macro);
	return sprintf(buf, "1 NULL NULL\n");
}
#else
static ssize_t front_camera_afcal_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] front_af_cal_pan: %d, front_af_cal_macro: %d\n", front_af_cal_pan, front_af_cal_macro);
	return sprintf(buf, "1 %d %d\n", front_af_cal_macro, front_af_cal_pan);
}
#endif

#if defined(CONFIG_MSM_FRONT_EEPROM)
char front_cam_fw_ver[25] = "NULL NULL\n";
#else
char front_cam_fw_ver[25] = "S5K5E3YX N\n";
#endif
static ssize_t front_camera_firmware_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] front_cam_fw_ver : %s\n", front_cam_fw_ver);
	return snprintf(buf, sizeof(front_cam_fw_ver), "%s", front_cam_fw_ver);
}

static ssize_t front_camera_firmware_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(front_cam_fw_ver, sizeof(front_cam_fw_ver), "%s", buf);

	return size;
}

#if defined(CONFIG_MSM_FRONT_EEPROM)
char front_cam_fw_full_ver[40] = "NULL NULL NULL\n";
#else
char front_cam_fw_full_ver[40] = "S5K5E3YX N N\n";
#endif
static ssize_t front_camera_firmware_full_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] front_cam_fw_full_ver : %s\n", front_cam_fw_full_ver);
	return snprintf(buf, sizeof(front_cam_fw_full_ver), "%s", front_cam_fw_full_ver);
}
static ssize_t front_camera_firmware_full_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(front_cam_fw_full_ver, sizeof(front_cam_fw_full_ver), "%s", buf);
	return size;
}

#if defined(CONFIG_MSM_FRONT_EEPROM)
char front_cam_fw_user_ver[40] = "NULL\n";
#else
char front_cam_fw_user_ver[40] = "OK\n";
#endif
static ssize_t front_camera_firmware_user_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] cam_fw_ver : %s\n", front_cam_fw_user_ver);
	return snprintf(buf, sizeof(front_cam_fw_user_ver), "%s", front_cam_fw_user_ver);
}

static ssize_t front_camera_firmware_user_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(front_cam_fw_user_ver, sizeof(front_cam_fw_user_ver), "%s", buf);

	return size;
}

#if defined(CONFIG_MSM_FRONT_EEPROM)
char front_cam_fw_factory_ver[40] = "NULL\n";
#else
char front_cam_fw_factory_ver[40] = "OK\n";
#endif
static ssize_t front_camera_firmware_factory_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] cam_fw_ver : %s\n", front_cam_fw_factory_ver);
	return snprintf(buf, sizeof(front_cam_fw_factory_ver), "%s", front_cam_fw_factory_ver);
}

static ssize_t front_camera_firmware_factory_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(front_cam_fw_factory_ver, sizeof(front_cam_fw_factory_ver), "%s", buf);

	return size;
}

#if defined (CONFIG_CAMERA_SYSFS_V2)
char rear_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear_camera_info_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] cam_info : %s\n", rear_cam_info);
	return snprintf(buf, sizeof(rear_cam_info), "%s", rear_cam_info);
}

static ssize_t rear_camera_info_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
//	snprintf(rear_cam_info, sizeof(rear_cam_info), "%s", buf);

	return size;
}

char front_cam_info[150] = "NULL\n";	//camera_info
static ssize_t front_camera_info_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] cam_info : %s\n", front_cam_info);
	return snprintf(buf, sizeof(front_cam_info), "%s", front_cam_info);
}

static ssize_t front_camera_info_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
//	snprintf(front_cam_info, sizeof(front_cam_info), "%s", buf);

	return size;
}
#endif

#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
char iris_cam_fw_ver[40] = "UNKNOWN N\n";
static ssize_t iris_camera_firmware_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] iris_cam_fw_ver : %s\n", cam_fw_ver);
	return snprintf(buf, sizeof(iris_cam_fw_ver), "%s", iris_cam_fw_ver);
}

static ssize_t iris_camera_firmware_store(struct device *dev,
					 struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(iris_cam_fw_ver, sizeof(iris_cam_fw_ver), "%s", buf);
	return size;
}

char iris_cam_fw_full_ver[40] = "UNKNOWN N N\n";
static ssize_t iris_camera_firmware_full_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] iris_cam_fw_full_ver : %s\n", iris_cam_fw_full_ver);
	return snprintf(buf, sizeof(iris_cam_fw_full_ver), "%s", iris_cam_fw_full_ver);
}

static ssize_t iris_camera_firmware_full_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(iris_cam_fw_full_ver, sizeof(iris_cam_fw_full_ver), "%s", buf);
	return size;
}

char iris_cam_fw_user_ver[40] = "OK\n";
static ssize_t iris_camera_firmware_user_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] iris_cam_fw_user_ver : %s\n", iris_cam_fw_user_ver);
	return snprintf(buf, sizeof(iris_cam_fw_user_ver), "%s", iris_cam_fw_user_ver);
}

static ssize_t iris_camera_firmware_user_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(iris_cam_fw_user_ver, sizeof(iris_cam_fw_user_ver), "%s", buf);
	return size;
}

char iris_cam_fw_factory_ver[40] = "OK\n";
static ssize_t iris_camera_firmware_factory_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] iris_cam_fw_factory_ver : %s\n", iris_cam_fw_factory_ver);
	return snprintf(buf, sizeof(iris_cam_fw_factory_ver), "%s", iris_cam_fw_factory_ver);
}

static ssize_t iris_camera_firmware_factory_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(iris_cam_fw_factory_ver, sizeof(iris_cam_fw_factory_ver), "%s", buf);
	return size;
}

#if defined(CONFIG_CAMERA_SYSFS_V2)
char iris_cam_info[150] = "NULL\n";	//camera_info
static ssize_t iris_camera_info_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] iris_cam_info : %s\n", iris_cam_info);
	return snprintf(buf, sizeof(iris_cam_info), "%s", iris_cam_info);
}

static ssize_t iris_camera_info_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
//	snprintf(front_cam_info, sizeof(front_cam_info), "%s", buf);
	return size;
}
#endif
#endif

#define FROM_SENSOR_ID_SIZE 16
char rear_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear_sensorid_exif_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	void * ret = NULL;
	CDBG("[FW_DBG] rear_sensor_id : %s\n", rear_sensor_id);

	ret = memcpy(buf, rear_sensor_id, sizeof(rear_sensor_id));
	if (ret)
		return sizeof(rear_sensor_id);
	return 0;
}

static ssize_t rear_sensorid_exif_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
//	snprintf(rear_sensor_id, sizeof(rear_sensor_id), "%s", buf);

	return size;
}

char front_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t front_sensorid_exif_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	void * ret = NULL;
	CDBG("[FW_DBG] front_sensor_id : %s\n", front_sensor_id);

	ret = memcpy(buf, front_sensor_id, sizeof(front_sensor_id));
	if (ret)
		return sizeof(front_sensor_id);
	return 0;
}

static ssize_t front_sensorid_exif_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
//	snprintf(front_sensor_id, sizeof(front_sensor_id), "%s", buf);

	return size;
}

#define FROM_MTF_SIZE 54
char front_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t front_mtf_exif_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	void * ret = NULL;
	CDBG("[FW_DBG] front_mtf_exif : %s\n", front_mtf_exif);

	ret = memcpy(buf, front_mtf_exif, sizeof(front_mtf_exif));
	if (ret)
		return sizeof(front_mtf_exif);
	return 0;
}

static ssize_t front_mtf_exif_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
//	snprintf(front_mtf_exif, sizeof(front_mtf_exif), "%s", buf);

	return size;
}

char rear_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear_mtf_exif_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	void * ret = NULL;
	CDBG("[FW_DBG] rear_mtf_exif : %s\n", rear_mtf_exif);

	ret = memcpy(buf, rear_mtf_exif, sizeof(rear_mtf_exif));
	if (ret)
		return sizeof(rear_mtf_exif);
	return 0;
}

static ssize_t rear_mtf_exif_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
//	snprintf(rear_mtf_exif, sizeof(rear_mtf_exif), "%s", buf);

	return size;
}

#define FROM_MODULE_ID_SIZE	10
uint8_t rear_module_id[FROM_MODULE_ID_SIZE + 1] = "\0";
static ssize_t back_camera_moduleid_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] rear_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear_module_id[0], rear_module_id[1], rear_module_id[2], rear_module_id[3], rear_module_id[4],
	  rear_module_id[5], rear_module_id[6], rear_module_id[7], rear_module_id[8], rear_module_id[9]);
	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear_module_id[0], rear_module_id[1], rear_module_id[2], rear_module_id[3], rear_module_id[4],
	  rear_module_id[5], rear_module_id[6], rear_module_id[7], rear_module_id[8], rear_module_id[9]);
}

uint8_t front_module_id[FROM_MODULE_ID_SIZE + 1] = "\0";
static ssize_t front_camera_moduleid_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] front_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front_module_id[0], front_module_id[1], front_module_id[2], front_module_id[3], front_module_id[4],
	  front_module_id[5], front_module_id[6], front_module_id[7], front_module_id[8], front_module_id[9]);
	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front_module_id[0], front_module_id[1], front_module_id[2], front_module_id[3], front_module_id[4],
	  front_module_id[5], front_module_id[6], front_module_id[7], front_module_id[8], front_module_id[9]);
}

#if defined (CONFIG_CAMERA_SYSFS_CAMFW_ALL)
char rcam_fw_all_ver[1024] = "NULL \n";
static ssize_t rear_camera_camfw_all_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	pr_err("[FW_DBG] rcam_fw_all_ver : %s\n", rcam_fw_all_ver);
	return sprintf(buf, "%s", rcam_fw_all_ver);
}

static ssize_t rear_camera_camfw_all_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	pr_err("[FW_DBG] buf : %s\n", buf);
	sprintf(rcam_fw_all_ver, "%s", buf);
	return size;
}
#endif

#if defined(CONFIG_COMPANION3)
extern int comp_fac_i2c_check;
extern uint16_t comp_fac_valid_check;
static ssize_t back_camera_comp_ic_check_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	CDBG("%s: comp_fac_i2c_check[%u] comp_fac_valid_check[0x%04x]\n", __func__, comp_fac_i2c_check, comp_fac_valid_check);

	if (comp_fac_i2c_check < 0)
		len = sprintf(buf, "%s\n", "NG_I2C");
	else if (comp_fac_valid_check)
		len = sprintf(buf, "%04X\n", comp_fac_valid_check);
	else
		len = sprintf(buf, "%s\n", "NG");

	return len;
}

static ssize_t back_camera_comp_ic_check_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("%s: buf[0]=%c\n", __func__, buf[0]);
	switch (buf[0]) {
	case '0':
		break;
	case '1':
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}

	return size;
}
#endif

#define SSRM_CAMERA_INFO_SIZE 64
char ssrm_camera_info[SSRM_CAMERA_INFO_SIZE + 1] = "\0";
static ssize_t ssrm_camera_info_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	int rc = 0;
	CDBG("ssrm_camera_info : %s\n", ssrm_camera_info);

	rc = scnprintf(buf, PAGE_SIZE, "%s", ssrm_camera_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t ssrm_camera_info_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("ssrm_camera_info buf : %s\n", buf);
	scnprintf(ssrm_camera_info, sizeof(ssrm_camera_info), "%s", buf);

	return size;
}

#define REAR_PAF_CAL_INFO_SIZE 1024
char rear_paf_cal_data[REAR_PAF_CAL_INFO_SIZE] = {0,};
static ssize_t rear_paf_offset_far_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	int rc = 0;
	rc = snprintf(buf, REAR_PAF_CAL_INFO_SIZE, rear_paf_cal_data, strlen(rear_paf_cal_data));
	if (rc) {
		printk("data size %d\n", rc);
		return rc;
	}
	return 0;
}

static ssize_t rear_paf_offset_far_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	int rc = 0;
	memset(rear_paf_cal_data, 0, REAR_PAF_CAL_INFO_SIZE);
	rc = snprintf(rear_paf_cal_data, REAR_PAF_CAL_INFO_SIZE, "%s", buf);
	if (rc !=  size)
		pr_err("Need to check copied str size!\n");

	return size;
}

#if defined(CONFIG_SAMSUNG_MULTI_CAMERA)
int rear2_af_cal[FROM_REAR_AF_CAL_SIZE + 1] = {0,};
static ssize_t back_camera2_afcal_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char tempbuf[10];
	char N[] = "N ";

	strncat(buf, "10 ", strlen("10 "));

#ifdef	FROM_REAR2_AF_CAL_D10_ADDR
	sprintf(tempbuf, "%d ", rear2_af_cal[1]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR2_AF_CAL_D20_ADDR
	sprintf(tempbuf, "%d ", rear2_af_cal[2]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR2_AF_CAL_D30_ADDR
	sprintf(tempbuf, "%d ", rear2_af_cal[3]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR2_AF_CAL_D40_ADDR
	sprintf(tempbuf, "%d ", rear2_af_cal[4]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR2_AF_CAL_D50_ADDR
	sprintf(tempbuf, "%d ", rear2_af_cal[5]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR2_AF_CAL_D60_ADDR
	sprintf(tempbuf, "%d ", rear2_af_cal[6]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR2_AF_CAL_D70_ADDR
	sprintf(tempbuf, "%d ", rear2_af_cal[7]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR2_AF_CAL_D80_ADDR
	sprintf(tempbuf, "%d ", rear2_af_cal[8]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

#ifdef	FROM_REAR2_AF_CAL_PAN_ADDR
	sprintf(tempbuf, "%d ", rear2_af_cal[9]);
#else
	sprintf(tempbuf, "%s", N);
#endif
	strncat(buf, tempbuf, strlen(tempbuf));

	return strlen(buf);
}

char rear2_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear2_camera_info_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] cam_info : %s\n", rear2_cam_info);
	return snprintf(buf, sizeof(rear2_cam_info), "%s", rear2_cam_info);
}

static ssize_t rear2_camera_info_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
//	snprintf(rear2_cam_info, sizeof(rear2_cam_info), "%s", buf);

	return size;
}

static ssize_t back_camera2_type_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char cam_type[] = "SLSI_S5K3M3\n";
	return snprintf(buf, sizeof(cam_type), "%s", cam_type);
}

char rear2_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear2_mtf_exif_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	void * ret = NULL;
	CDBG("[FW_DBG] rear2_mtf_exif : %s\n", rear2_mtf_exif);

	ret = memcpy(buf, rear2_mtf_exif, sizeof(rear2_mtf_exif));
	if (ret)
		return sizeof(rear2_mtf_exif);
	return 0;
}

static ssize_t rear2_mtf_exif_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
//	snprintf(rear2_mtf_exif, sizeof(rear2_mtf_exif), "%s", buf);

	return size;
}

char rear2_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear2_sensorid_exif_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	void * ret = NULL;
	CDBG("[FW_DBG] rear2_sensor_id : %s\n", rear2_sensor_id);

	ret = memcpy(buf, rear2_sensor_id, sizeof(rear2_sensor_id));
	if (ret)
		return sizeof(rear2_sensor_id);
	return 0;
}

static ssize_t rear2_sensorid_exif_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
//	snprintf(rear2_sensor_id, sizeof(rear2_sensor_id), "%s", buf);

	return size;
}

#define FROM_REAR_DUAL_CAL_SIZE 512
uint8_t rear_dual_cal[FROM_REAR_DUAL_CAL_SIZE + 1] = "\0";
static ssize_t rear_dual_cal_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	void * ret = NULL;
	CDBG("[FW_DBG] rear_dual_cal : %s\n", rear_dual_cal);

	ret = memcpy(buf, rear_dual_cal, sizeof(rear_dual_cal));
	if (ret)
		return sizeof(rear_dual_cal);
	return 0;
}

uint32_t rear_dual_cal_size = FROM_REAR_DUAL_CAL_SIZE;
static ssize_t rear_dual_cal_size_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] rear_dual_cal_size : %d\n", rear_dual_cal_size);
	return sprintf(buf, "%d\n", rear_dual_cal_size);
}

int rear2_dual_tilt_x;
int rear2_dual_tilt_y;
int rear2_dual_tilt_z;
int rear2_dual_tilt_sx;
int rear2_dual_tilt_sy;
int rear2_dual_tilt_range;
int rear2_dual_tilt_max_err;
int rear2_dual_tilt_avg_err;
int rear2_dual_tilt_dll_ver;
static ssize_t rear2_tilt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] rear dual tilt x = %d, y = %d, z = %d, sx = %d, sy = %d, range = %d, max_err = %d, avg_err = %d, dll_ver = %d\n",
		rear2_dual_tilt_x, rear2_dual_tilt_y, rear2_dual_tilt_z, rear2_dual_tilt_sx, rear2_dual_tilt_sy,
		rear2_dual_tilt_range, rear2_dual_tilt_max_err, rear2_dual_tilt_avg_err, rear2_dual_tilt_dll_ver);

	return sprintf(buf, "1 %d %d %d %d %d %d %d %d %d\n", rear2_dual_tilt_x, rear2_dual_tilt_y,
			rear2_dual_tilt_z, rear2_dual_tilt_sx, rear2_dual_tilt_sy, rear2_dual_tilt_range,
			rear2_dual_tilt_max_err, rear2_dual_tilt_avg_err, rear2_dual_tilt_dll_ver);
}
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
static int16_t is_hw_param_valid_module_id(char *moduleid)
{
	int i = 0;
	int32_t moduleid_cnt = 0;
	int16_t rc = HW_PARAMS_MI_VALID;

	if (moduleid == NULL) {
		pr_err("[HWB_DBG] MI_INVALID\n");
		return HW_PARAMS_MI_INVALID;
	}

	for (i = 0;i < FROM_MODULE_ID_SIZE;i++)
	{
		if(moduleid[i] == '\0')
		{
			moduleid_cnt = moduleid_cnt + 1;
		}
		else if ((i < 5)
			&& (!((moduleid[i] > 47 && moduleid[i] < 58)	// 0 to 9
			|| (moduleid[i] > 64 && moduleid[i] < 91))))	// A to Z
		{
			pr_err("[HWB_DBG] MIR_ERR_1\n");
			rc = HW_PARAMS_MIR_ERR_1;
			break;
		}
	}

	if (moduleid_cnt == FROM_MODULE_ID_SIZE)
	{
		pr_err("[HWB_DBG] MIR_ERR_0\n");
		rc = HW_PARAMS_MIR_ERR_0;
	}

	return rc;
}

static ssize_t rear_camera_hw_param_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	int16_t moduelid_chk = 0;
	struct cam_hw_param *ec_param = NULL;
	msm_is_sec_get_rear_hw_param(&ec_param);

	if(ec_param != NULL) {
		moduelid_chk = is_hw_param_valid_module_id(rear_module_id);
		switch (moduelid_chk) {
			case HW_PARAMS_MI_VALID:
				rc = sprintf(buf, "\"CAMIR_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR_AF\":\"%d\",\"I2CR_COM\":\"%d\",\"I2CR_OIS\":\"%d\","
					"\"I2CR_SEN\":\"%d\",\"MIPIR_COM\":\"%d\",\"MIPIR_SEN\":\"%d\"\n",
					rear_module_id[0], rear_module_id[1], rear_module_id[2], rear_module_id[3],
					rear_module_id[4], rear_module_id[7], rear_module_id[8], rear_module_id[9],
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
				break;

			case HW_PARAMS_MIR_ERR_1:
				rc = sprintf(buf, "\"CAMIR_ID\":\"MIR_ERR\",\"I2CR_AF\":\"%d\",\"I2CR_COM\":\"%d\",\"I2CR_OIS\":\"%d\","
					"\"I2CR_SEN\":\"%d\",\"MIPIR_COM\":\"%d\",\"MIPIR_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
				break;

			default:
				rc = sprintf(buf, "\"CAMIR_ID\":\"MI_NO\",\"I2CR_AF\":\"%d\",\"I2CR_COM\":\"%d\",\"I2CR_OIS\":\"%d\","
					"\"I2CR_SEN\":\"%d\",\"MIPIR_COM\":\"%d\",\"MIPIR_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
				break;

		}
	}

	return rc;
}

static ssize_t rear_camera_hw_param_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	CDBG("[HWB_DBG][R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		msm_is_sec_get_rear_hw_param(&ec_param);
		if (ec_param != NULL) {
			msm_is_sec_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t front_camera_hw_param_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	int16_t moduelid_chk = 0;
	struct cam_hw_param *ec_param = NULL;
	msm_is_sec_get_front_hw_param(&ec_param);

	if(ec_param != NULL) {
		moduelid_chk = is_hw_param_valid_module_id(front_module_id);
		switch (moduelid_chk) {
			case HW_PARAMS_MI_VALID:
				rc = sprintf(buf, "\"CAMIF_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CF_AF\":\"%d\",\"I2CF_COM\":\"%d\",\"I2CF_OIS\":\"%d\","
					"\"I2CF_SEN\":\"%d\",\"MIPIF_COM\":\"%d\",\"MIPIF_SEN\":\"%d\"\n",
					front_module_id[0], front_module_id[1], front_module_id[2], front_module_id[3],
					front_module_id[4], front_module_id[7], front_module_id[8], front_module_id[9],
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
				break;

			case HW_PARAMS_MIR_ERR_1:
				rc = sprintf(buf, "\"CAMIF_ID\":\"MIR_ERR\",\"I2CF_AF\":\"%d\",\"I2CF_COM\":\"%d\",\"I2CF_OIS\":\"%d\","
					"\"I2CF_SEN\":\"%d\",\"MIPIF_COM\":\"%d\",\"MIPIF_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
				break;

			default:
				rc = sprintf(buf, "\"CAMIF_ID\":\"MI_NO\",\"I2CF_AF\":\"%d\",\"I2CF_COM\":\"%d\",\"I2CF_OIS\":\"%d\","
					"\"I2CF_SEN\":\"%d\",\"MIPIF_COM\":\"%d\",\"MIPIF_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
				break;
		}
	}

	return rc;
}

static ssize_t front_camera_hw_param_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	CDBG("[HWB_DBG][F] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		msm_is_sec_get_front_hw_param(&ec_param);
		if (ec_param != NULL) {
			msm_is_sec_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
uint8_t iris_module_id[FROM_MODULE_ID_SIZE + 1] = "\0";
static ssize_t iris_camera_hw_param_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	int16_t moduelid_chk = 0;
	struct cam_hw_param *ec_param = NULL;
	msm_is_sec_get_iris_hw_param(&ec_param);

	if(ec_param != NULL) {
		moduelid_chk = is_hw_param_valid_module_id(iris_module_id);
		switch (moduelid_chk) {
			case HW_PARAMS_MI_VALID:
				rc = sprintf(buf, "\"CAMII_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CI_AF\":\"%d\",\"I2CI_COM\":\"%d\",\"I2CI_OIS\":\"%d\","
					"\"I2CI_SEN\":\"%d\",\"MIPII_COM\":\"%d\",\"MIPII_SEN\":\"%d\"\n",
					iris_module_id[0], iris_module_id[1], iris_module_id[2], iris_module_id[3],
					iris_module_id[4], iris_module_id[7], iris_module_id[8], iris_module_id[9],
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
				break;

			case HW_PARAMS_MIR_ERR_1:
				rc = sprintf(buf, "\"CAMII_ID\":\"MIR_ERR\",\"I2CI_AF\":\"%d\",\"I2CI_COM\":\"%d\",\"I2CI_OIS\":\"%d\","
					"\"I2CI_SEN\":\"%d\",\"MIPII_COM\":\"%d\",\"MIPII_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
				break;

			default:
				rc = sprintf(buf, "\"CAMII_ID\":\"MI_NO\",\"I2CI_AF\":\"%d\",\"I2CI_COM\":\"%d\",\"I2CI_OIS\":\"%d\","
					"\"I2CI_SEN\":\"%d\",\"MIPII_COM\":\"%d\",\"MIPII_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
				break;
		}
	}

	return rc;
}

static ssize_t iris_camera_hw_param_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	CDBG("[HWB_DBG][I] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		msm_is_sec_get_iris_hw_param(&ec_param);
		if (ec_param != NULL) {
			msm_is_sec_init_err_cnt_file(ec_param);
		}
	}

	return size;
}
#endif

#if defined(CONFIG_SAMSUNG_MULTI_CAMERA)
static ssize_t rear2_camera_hw_param_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	int16_t moduelid_chk = 0;
	struct cam_hw_param *ec_param = NULL;
	msm_is_sec_get_rear2_hw_param(&ec_param);

	if(ec_param != NULL) {
		moduelid_chk = is_hw_param_valid_module_id(rear_module_id);
		switch (moduelid_chk) {
			case HW_PARAMS_MI_VALID:
				rc = sprintf(buf, "\"CAMIR2_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR2_AF\":\"%d\",\"I2CR2_COM\":\"%d\",\"I2CR2_OIS\":\"%d\","
					"\"I2CR2_SEN\":\"%d\",\"MIPIR2_COM\":\"%d\",\"MIPIR2_SEN\":\"%d\"\n",
					rear_module_id[0], rear_module_id[1], rear_module_id[2], rear_module_id[3],
					rear_module_id[4], rear_module_id[7], rear_module_id[8], rear_module_id[9],
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
				break;

			case HW_PARAMS_MIR_ERR_1:
				rc = sprintf(buf, "\"CAMIR2_ID\":\"MIR_ERR\",\"I2CR2_AF\":\"%d\",\"I2CR2_COM\":\"%d\",\"I2CR2_OIS\":\"%d\","
					"\"I2CR2_SEN\":\"%d\",\"MIPIR2_COM\":\"%d\",\"MIPIR2_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
				break;

			default:
				rc = sprintf(buf, "\"CAMIR2_ID\":\"MI_NO\",\"I2CR2_AF\":\"%d\",\"I2CR2_COM\":\"%d\",\"I2CR2_OIS\":\"%d\","
					"\"I2CR2_SEN\":\"%d\",\"MIPIR2_COM\":\"%d\",\"MIPIR2_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt, ec_param->mipi_sensor_err_cnt);
				break;
		}
	}

	return rc;
}

static ssize_t rear2_camera_hw_param_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	CDBG("[HWB_DBG][R2] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		msm_is_sec_get_rear2_hw_param(&ec_param);
		if (ec_param != NULL) {
			msm_is_sec_init_err_cnt_file(ec_param);
		}
	}

	return size;
}
#endif
#endif

#if defined(CONFIG_SEC_KELLYLTE_PROJECT)
char rear_cam_afpos[10] = "NULL\n";	//camera_info
static ssize_t rear_camera_afpos_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[FW_DBG] rear cam afpos : %s\n", rear_cam_afpos);
	return snprintf(buf, sizeof(rear_cam_afpos), "%s", rear_cam_afpos);
}

static ssize_t rear_camera_afpos_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(rear_cam_afpos, sizeof(rear_cam_afpos), "%s", buf);

	return size;
}
#endif

static DEVICE_ATTR(rear_camtype, S_IRUGO, back_camera_type_show, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
    back_camera_firmware_show, back_camera_firmware_store);
static DEVICE_ATTR(rear_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
    back_camera_firmware_user_show, back_camera_firmware_user_store);
static DEVICE_ATTR(rear_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
    back_camera_firmware_factory_show, back_camera_firmware_factory_store);
static DEVICE_ATTR(rear_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
    back_camera_firmware_full_show, back_camera_firmware_full_store);
static DEVICE_ATTR(rear_camfw_load, S_IRUGO|S_IWUSR|S_IWGRP,
    back_camera_firmware_load_show, back_camera_firmware_load_store);
#if defined(CONFIG_COMPANION3)
static DEVICE_ATTR(rear_companionfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
    back_companion_firmware_show, back_companion_firmware_store);
#endif
static DEVICE_ATTR(rear_fwcheck, S_IRUGO|S_IWUSR|S_IWGRP,
    back_fw_crc_check_show, back_fw_crc_check_store);
static DEVICE_ATTR(rear_calcheck, S_IRUGO|S_IWUSR|S_IWGRP,
    back_cal_data_check_show, back_cal_data_check_store);
static DEVICE_ATTR(rear_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
    back_module_info_show, back_module_info_store);
static DEVICE_ATTR(front_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
    front_module_info_show, front_module_info_store);
static DEVICE_ATTR(isp_core, S_IRUGO|S_IWUSR|S_IWGRP,
    back_isp_core_check_show, back_isp_core_check_store);
static DEVICE_ATTR(rear_afcal, S_IRUGO, back_camera_afcal_show, NULL);
static DEVICE_ATTR(front_afcal, S_IRUGO, front_camera_afcal_show, NULL);
static DEVICE_ATTR(front_camtype, S_IRUGO, front_camera_type_show, NULL);
static DEVICE_ATTR(front_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
		front_camera_firmware_show, front_camera_firmware_store);
static DEVICE_ATTR(front_camfw_full, S_IRUGO | S_IWUSR | S_IWGRP,
		front_camera_firmware_full_show, front_camera_firmware_full_store);
static DEVICE_ATTR(front_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
    front_camera_firmware_user_show, front_camera_firmware_user_store);
static DEVICE_ATTR(front_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
    front_camera_firmware_factory_show, front_camera_firmware_factory_store);
#if defined (CONFIG_CAMERA_SYSFS_V2)
static DEVICE_ATTR(rear_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
		rear_camera_info_show, rear_camera_info_store);
static DEVICE_ATTR(front_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
		front_camera_info_show, front_camera_info_store);
#endif
static DEVICE_ATTR(rear_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
		rear_sensorid_exif_show, rear_sensorid_exif_store);
static DEVICE_ATTR(front_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
		front_sensorid_exif_show, front_sensorid_exif_store);
static DEVICE_ATTR(rear_moduleid, S_IRUGO, back_camera_moduleid_show, NULL);
static DEVICE_ATTR(front_moduleid, S_IRUGO, front_camera_moduleid_show, NULL);
#if defined (CONFIG_CAMERA_SYSFS_CAMFW_ALL)
static DEVICE_ATTR(rear_camfw_all,  S_IRUGO|S_IWUSR|S_IWGRP,
  	        rear_camera_camfw_all_show, rear_camera_camfw_all_store);
#endif
static DEVICE_ATTR(front_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
		front_mtf_exif_show, front_mtf_exif_store);
static DEVICE_ATTR(rear_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
		rear_mtf_exif_show, rear_mtf_exif_store);
static DEVICE_ATTR(SVC_rear_module, S_IRUGO, back_camera_moduleid_show, NULL);
static DEVICE_ATTR(SVC_front_module, S_IRUGO, front_camera_moduleid_show, NULL);
static DEVICE_ATTR(ssrm_camera_info, S_IRUGO|S_IWUSR|S_IWGRP,
		ssrm_camera_info_show, ssrm_camera_info_store);

#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
static DEVICE_ATTR(iris_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
		iris_camera_firmware_show, iris_camera_firmware_store);
static DEVICE_ATTR(iris_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
		iris_camera_firmware_user_show, iris_camera_firmware_user_store);
static DEVICE_ATTR(iris_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
		iris_camera_firmware_factory_show, iris_camera_firmware_factory_store);
static DEVICE_ATTR(iris_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
		iris_camera_firmware_full_show, iris_camera_firmware_full_store);
#if defined(CONFIG_CAMERA_SYSFS_V2)
static DEVICE_ATTR(iris_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
		iris_camera_info_show, iris_camera_info_store);
#endif
#endif

#if defined(CONFIG_COMPANION3)
static DEVICE_ATTR(companion_ic_check, S_IRUGO|S_IWUSR|S_IWGRP,
		back_camera_comp_ic_check_show, back_camera_comp_ic_check_store);
#endif

#if defined(CONFIG_SAMSUNG_MULTI_CAMERA)
static DEVICE_ATTR(rear2_afcal, S_IRUGO, back_camera2_afcal_show, NULL);
static DEVICE_ATTR(rear2_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
		rear2_camera_info_show, rear2_camera_info_store);
static DEVICE_ATTR(rear2_camtype, S_IRUGO, back_camera2_type_show, NULL);
static DEVICE_ATTR(rear2_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
		rear2_mtf_exif_show, rear2_mtf_exif_store);
static DEVICE_ATTR(rear2_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
		rear2_sensorid_exif_show, rear2_sensorid_exif_store);
static DEVICE_ATTR(rear_dualcal, S_IRUGO, rear_dual_cal_show, NULL);
static DEVICE_ATTR(rear_dualcal_size, S_IRUGO, rear_dual_cal_size_show, NULL);
static DEVICE_ATTR(rear2_tilt, S_IRUGO, rear2_tilt_show, NULL);


#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
static DEVICE_ATTR(rear_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
		rear_camera_hw_param_show, rear_camera_hw_param_store);
static DEVICE_ATTR(front_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
		front_camera_hw_param_show, front_camera_hw_param_store);
#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
static DEVICE_ATTR(iris_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
		iris_camera_hw_param_show, iris_camera_hw_param_store);
#endif
#if defined(CONFIG_SAMSUNG_MULTI_CAMERA)
static DEVICE_ATTR(rear2_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
		rear2_camera_hw_param_show, rear2_camera_hw_param_store);
#endif
#endif

static DEVICE_ATTR(rear_paf_offset_far, S_IRUGO|S_IWUSR|S_IWGRP,
		rear_paf_offset_far_show, rear_paf_offset_far_store);

#if defined(CONFIG_SEC_KELLYLTE_PROJECT)
static DEVICE_ATTR(rear_afpos, S_IRUGO|S_IWUSR|S_IWGRP,
		rear_camera_afpos_show, rear_camera_afpos_store);
#endif

#if defined(CONFIG_SAMSUNG_MULTI_CAMERA)
char ae_awb_last_value[128] = "0\n";
static ssize_t ae_awb_value_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[syscamera] ae_awb_value_show : %s\n", ae_awb_last_value);
	return snprintf(buf, sizeof(ae_awb_last_value), "%s", ae_awb_last_value);
}

static ssize_t ae_awb_value_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(ae_awb_last_value, sizeof(ae_awb_last_value), "%s", buf);
	return size;
}
static DEVICE_ATTR(ae_awb_value, S_IRUGO|S_IWUSR|S_IWGRP,
		ae_awb_value_show, ae_awb_value_store);

char af_main_position_value[128] = "0\n";
static ssize_t af_main_position_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[syscamera] af_main_position_show : %s\n", af_main_position_value);
	return snprintf(buf, sizeof(af_main_position_value), "%s", af_main_position_value);
}

static ssize_t af_main_position_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(af_main_position_value, sizeof(af_main_position_value), "%s", buf);
	return size;
}
static DEVICE_ATTR(af_main_position, S_IRUGO|S_IWUSR|S_IWGRP,
		af_main_position_show, af_main_position_store);


char af_aux_position_value[128] = "0\n";
static ssize_t af_aux_position_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[syscamera] af_aux_position_show : %s\n", af_aux_position_value);
	return snprintf(buf, sizeof(af_aux_position_value), "%s", af_aux_position_value);
}

static ssize_t af_aux_position_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(af_aux_position_value, sizeof(af_aux_position_value), "%s", buf);
	return size;
}
static DEVICE_ATTR(af_aux_position, S_IRUGO|S_IWUSR|S_IWGRP,
		af_aux_position_show, af_aux_position_store);


char dual_fallback_value[40] = "0\n";
static ssize_t dual_fallback_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	CDBG("[syscamera] dual_fallback_show : %s\n", dual_fallback_value);
	return snprintf(buf, sizeof(dual_fallback_value), "%s", dual_fallback_value);
}

static ssize_t dual_fallback_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	CDBG("[FW_DBG] buf : %s\n", buf);
	snprintf(dual_fallback_value, sizeof(dual_fallback_value), "%s", buf);
	return size;
}
static DEVICE_ATTR(fallback, S_IRUGO|S_IWUSR|S_IWGRP,
		dual_fallback_show, dual_fallback_store);
#endif

int svc_cheating_prevent_device_file_create(struct kobject **obj)
{
	struct kernfs_node *SVC_sd;
	struct kobject *data;
	struct kobject *Camera;

	/* To find SVC kobject */
	SVC_sd = sysfs_get_dirent(devices_kset->kobj.sd, "svc");
	if (IS_ERR_OR_NULL(SVC_sd)) {
		/* try to create SVC kobject */
		data = kobject_create_and_add("svc", &devices_kset->kobj);
		if (IS_ERR_OR_NULL(data))
		        pr_info("Failed to create sys/devices/svc already exist SVC : 0x%p\n", data);
		else
			pr_info("Success to create sys/devices/svc SVC : 0x%p\n", data);
	} else {
		data = (struct kobject *)SVC_sd->priv;
		pr_info("Success to find SVC_sd : 0x%p SVC : 0x%p\n", SVC_sd, data);
	}

	Camera = kobject_create_and_add("Camera", data);
	if (IS_ERR_OR_NULL(Camera))
	        pr_info("Failed to create sys/devices/svc/Camera : 0x%p\n", Camera);
	else
		pr_info("Success to create sys/devices/svc/Camera : 0x%p\n", Camera);


	*obj = Camera;
	return 0;
}

static int __init msm_sensor_init_module(void)
{
	struct device         *cam_dev_back;
	struct device         *cam_dev_front;
#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
	struct device         *cam_dev_iris;
#endif
#if defined(CONFIG_SAMSUNG_MULTI_CAMERA)
	struct device         *cam_dev_af;
	struct device         *cam_dev_dual;
#endif
	struct kobject *SVC = 0;
	int ret = 0;

	svc_cheating_prevent_device_file_create(&SVC);

	camera_class = class_create(THIS_MODULE, "camera");
	if (IS_ERR(camera_class))
	    pr_err("failed to create device cam_dev_rear!\n");

	/* Allocate memory for msm_sensor_init control structure */
	s_init = kzalloc(sizeof(struct msm_sensor_init_t), GFP_KERNEL);
	if (!s_init) {
		class_destroy(camera_class);
		pr_err("failed: no memory s_init %pK", NULL);
		return -ENOMEM;
	}

	CDBG("MSM_SENSOR_INIT_MODULE %pK", NULL);

	/* Initialize mutex */
	mutex_init(&s_init->imutex);

	/* Create /dev/v4l-subdevX for msm_sensor_init */
	v4l2_subdev_init(&s_init->msm_sd.sd, &msm_sensor_init_subdev_ops);
	snprintf(s_init->msm_sd.sd.name, sizeof(s_init->msm_sd.sd.name), "%s",
		"msm_sensor_init");
	v4l2_set_subdevdata(&s_init->msm_sd.sd, s_init);
	s_init->msm_sd.sd.internal_ops = &msm_sensor_init_internal_ops;
	s_init->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ret = media_entity_init(&s_init->msm_sd.sd.entity, 0, NULL, 0);
	if (ret < 0) {
		pr_err("Failed to media entity init!\n");
		goto error;
	}
	s_init->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_init->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_SENSOR_INIT;
	s_init->msm_sd.sd.entity.name = s_init->msm_sd.sd.name;
	s_init->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x6;
	ret = msm_sd_register(&s_init->msm_sd);
	if (ret) {
		CDBG("%s: msm_sd_register error = %d\n", __func__, ret);
		goto msm_sd_register_fail;
	}
	msm_cam_copy_v4l2_subdev_fops(&msm_sensor_init_v4l2_subdev_fops);
#ifdef CONFIG_COMPAT
	msm_sensor_init_v4l2_subdev_fops.compat_ioctl32 =
		msm_sensor_init_subdev_fops_ioctl;
#endif
	s_init->msm_sd.sd.devnode->fops =
		&msm_sensor_init_v4l2_subdev_fops;

	init_waitqueue_head(&s_init->state_wait);

	cam_dev_back = device_create(camera_class, NULL,
		1, NULL, "rear");
	if (IS_ERR(cam_dev_back)) {
		printk("Failed to create cam_dev_back device!\n");
		ret = -ENODEV;
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_back, &dev_attr_rear_camtype) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_camtype.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_back, &dev_attr_rear_camfw) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_camfw.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_back, &dev_attr_rear_checkfw_user) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_checkfw_user.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_back, &dev_attr_rear_checkfw_factory) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_checkfw_factory.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_back, &dev_attr_rear_camfw_full) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_camfw_full.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_back, &dev_attr_rear_camfw_load) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_camfw_load.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
#if defined(CONFIG_COMPANION3)
	if (device_create_file(cam_dev_back, &dev_attr_rear_companionfw_full) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_companionfw_full.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
#endif
	if (device_create_file(cam_dev_back, &dev_attr_rear_fwcheck) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_fwcheck.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_back, &dev_attr_rear_calcheck) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_calcheck.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_back, &dev_attr_rear_moduleinfo) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_moduleinfo.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_back, &dev_attr_isp_core) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_isp_core.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

#if defined (CONFIG_CAMERA_SYSFS_V2)
	if (device_create_file(cam_dev_back, &dev_attr_rear_caminfo) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_caminfo.attr.name);
		goto device_create_fail;
	}
#endif
	if (device_create_file(cam_dev_back, &dev_attr_rear_afcal) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_afcal.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_back, &dev_attr_rear_sensorid_exif) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_sensorid_exif.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_back, &dev_attr_rear_moduleid) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_moduleid.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_back, &dev_attr_rear_mtf_exif) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_mtf_exif.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_back, &dev_attr_ssrm_camera_info) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_ssrm_camera_info.attr.name);
	}

	if (device_create_file(cam_dev_back, &dev_attr_rear_paf_offset_far) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_paf_offset_far.attr.name);
	}

	if (sysfs_create_file(SVC, &dev_attr_SVC_rear_module.attr) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_SVC_rear_module.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

#if defined(CONFIG_SEC_KELLYLTE_PROJECT)
	if (device_create_file(cam_dev_back, &dev_attr_rear_afpos) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_afpos.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
#endif

	cam_dev_front = device_create(camera_class, NULL,
		2, NULL, "front");
	if (IS_ERR(cam_dev_front)) {
		printk("Failed to create cam_dev_front device!");
		ret = -ENODEV;
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_front, &dev_attr_front_camtype) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_front_camtype.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_front, &dev_attr_front_camfw) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_front_camfw.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_front, &dev_attr_front_camfw_full) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_front_camfw_full.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_front, &dev_attr_front_checkfw_user) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_front_checkfw_user.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_front, &dev_attr_front_checkfw_factory) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_front_checkfw_factory.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_front, &dev_attr_front_moduleinfo) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_front_moduleinfo.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

#if defined (CONFIG_CAMERA_SYSFS_V2)
	if (device_create_file(cam_dev_front, &dev_attr_front_caminfo) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_front_caminfo.attr.name);
		goto device_create_fail;
	}
#endif
	if (device_create_file(cam_dev_front, &dev_attr_front_afcal) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_front_afcal.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_front, &dev_attr_front_sensorid_exif) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_front_sensorid_exif.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_front, &dev_attr_front_moduleid) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_front_moduleid.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
#if defined (CONFIG_CAMERA_SYSFS_CAMFW_ALL)
	if (device_create_file(cam_dev_back, &dev_attr_rear_camfw_all) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_camfw_all.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
#endif
	if (device_create_file(cam_dev_front, &dev_attr_front_mtf_exif) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_front_mtf_exif.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (sysfs_create_file(SVC, &dev_attr_SVC_front_module.attr) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_SVC_front_module.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
	cam_dev_iris = device_create(camera_class, NULL,
		2, NULL, "secure");
	if (IS_ERR(cam_dev_iris)) {
		printk("Failed to create cam_dev_iris device!");
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_iris, &dev_attr_iris_camfw) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_iris_camfw.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_iris, &dev_attr_iris_camfw_full) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_iris_camfw_full.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_iris, &dev_attr_iris_checkfw_user) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_iris_checkfw_user.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_iris, &dev_attr_iris_checkfw_factory) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_iris_checkfw_factory.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
#if defined (CONFIG_CAMERA_SYSFS_V2)
	if (device_create_file(cam_dev_iris, &dev_attr_iris_caminfo) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_iris_caminfo.attr.name);
		goto device_create_fail;
	}
#endif
#endif
#if defined(CONFIG_COMPANION3)
        if (device_create_file(cam_dev_back, &dev_attr_companion_ic_check) < 0) {
            printk("Failed to create device file!(%s)!\n",
                dev_attr_companion_ic_check.attr.name);
            goto device_create_fail;
        }
#endif

#if defined(CONFIG_SAMSUNG_MULTI_CAMERA)
	if (device_create_file(cam_dev_back, &dev_attr_rear2_afcal) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear2_afcal.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_back, &dev_attr_rear2_tilt) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear2_tilt.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_back, &dev_attr_rear2_caminfo) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear2_caminfo.attr.name);
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_back, &dev_attr_rear2_camtype) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear2_camtype.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_back, &dev_attr_rear2_mtf_exif) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear2_mtf_exif.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_back, &dev_attr_rear2_sensorid_exif) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear2_sensorid_exif.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_back, &dev_attr_rear_dualcal) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_dualcal.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_back, &dev_attr_rear_dualcal_size) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_dualcal_size.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	if (device_create_file(cam_dev_back, &dev_attr_rear_hwparam) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear_hwparam.attr.name);
	}

	if (device_create_file(cam_dev_front, &dev_attr_front_hwparam) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_front_hwparam.attr.name);
	}

#if defined(CONFIG_SAMSUNG_SECURE_CAMERA)
	if (device_create_file(cam_dev_iris, &dev_attr_iris_hwparam) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_iris_hwparam.attr.name);
	}
#endif

#if defined(CONFIG_SAMSUNG_MULTI_CAMERA)
	if (device_create_file(cam_dev_back, &dev_attr_rear2_hwparam) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_rear2_hwparam.attr.name);
	}
#endif
#if defined(CONFIG_SAMSUNG_MULTI_CAMERA)
	cam_dev_af = device_create(camera_class, NULL,
		1, NULL, "af");
	if (IS_ERR(cam_dev_af)) {
		printk("Failed to create cam_dev_af device!\n");
		ret = -ENODEV;
		goto device_create_fail;
	}
	if (device_create_file(cam_dev_af, &dev_attr_ae_awb_value) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_ae_awb_value.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_af, &dev_attr_af_main_position) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_af_main_position.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

	if (device_create_file(cam_dev_af, &dev_attr_af_aux_position) < 0) {
		printk("Failed to create device file!(%s)!\n",
			dev_attr_af_aux_position.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
	}

    cam_dev_dual = device_create(camera_class, NULL,
	    1, NULL, "dual");
    if (IS_ERR(cam_dev_dual)) {
	    printk("Failed to create cam_dev_dual device!\n");
		ret = -ENODEV;
		goto device_create_fail;
    }
    if (device_create_file(cam_dev_dual, &dev_attr_fallback) < 0) {
	    printk("Failed to create device file!(%s)!\n",
		    dev_attr_fallback.attr.name);
		ret = -ENODEV;
		goto device_create_fail;
    }
#endif
#endif

	pr_warn("MSM_SENSOR_INIT_MODULE : X");
	return 0;

device_create_fail:
	msm_sd_unregister(&s_init->msm_sd);
msm_sd_register_fail:
	media_entity_cleanup(&s_init->msm_sd.sd.entity);
error:
	mutex_destroy(&s_init->imutex);
	kfree(s_init);
	class_destroy(camera_class);
	return ret;
}

static void __exit msm_sensor_exit_module(void)
{
	msm_sd_unregister(&s_init->msm_sd);
	mutex_destroy(&s_init->imutex);
	kfree(s_init);
	return;
}

module_init(msm_sensor_init_module);
module_exit(msm_sensor_exit_module);
MODULE_DESCRIPTION("msm_sensor_init");
MODULE_LICENSE("GPL v2");
