/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "ssp_scontext.h"
#include "ssp_cmd_define.h"
#include "ssp_comm.h"
#include "ssp_sysfs.h"
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>

void ssp_scontext_log(const char *func_name,
                              const char *data, int length)
{
	char buf[6];
	char *log_str;
	int log_size;
	int i;

	if (likely(length <= BIG_DATA_SIZE)) {
		log_size = length;
	} else {
		log_size = PRINT_TRUNCATE * 2 + 1;
	}

	log_size = sizeof(buf) * log_size + 1;
	log_str = kzalloc(log_size, GFP_ATOMIC);
	if (unlikely(!log_str)) {
		ssp_errf("allocate memory for data log err");
		return;
	}

	for (i = 0; i < length; i++) {
		if (length < BIG_DATA_SIZE ||
		    i < PRINT_TRUNCATE || i >= length - PRINT_TRUNCATE) {
			snprintf(buf, sizeof(buf), "0x%x", (signed char)data[i]);
			strlcat(log_str, buf, log_size);

			if (i < length - 1) {
				strlcat(log_str, ", ", log_size);
			}
		}
		if (length > BIG_DATA_SIZE && i == PRINT_TRUNCATE) {
			strlcat(log_str, "..., ", log_size);
		}
	}

	ssp_info("%s(%d): %s", func_name, length, log_str);
	kfree(log_str);
}

static int ssp_scontext_send_cmd(struct ssp_data *data,
                                  const char *buf, int count)
{
	int ret = 0;

	if (buf[2] < SCONTEXT_AP_STATUS_WAKEUP ||
	    buf[2] >= SCONTEXT_AP_STATUS_CALL_ACTIVE) {
		ssp_errf("INST_LIB_NOTI err(%d)", buf[2]);
		return -EINVAL;
	}

	ret = ssp_send_status(data, buf[2]);

	if (buf[2] == SCONTEXT_AP_STATUS_WAKEUP ||
	    buf[2] == SCONTEXT_AP_STATUS_SLEEP) {
		data->last_ap_status = buf[2];
	}

	if (buf[2] == SCONTEXT_AP_STATUS_SUSPEND ||
	    buf[2] == SCONTEXT_AP_STATUS_RESUME) {
		data->last_resume_status = buf[2];
	}

	return ret;
}


#define SCONTEXT_VALUE_CURRENTSYSTEMTIME                      0x0E
#define SCONTEXT_VALUE_PEDOMETER_USERHEIGHT           0x12
#define SCONTEXT_VALUE_PEDOMETER_USERWEIGHT           0x13
#define SCONTEXT_VALUE_PEDOMETER_USERGENDER           0x14
#define SCONTEXT_VALUE_PEDOMETER_INFOUPDATETIME       0x15

int convert_scontext_putvalue_subcmd(int subcmd)
{
	int ret = -1;
	switch (subcmd) {
	case SCONTEXT_VALUE_CURRENTSYSTEMTIME :
		ret = CURRENT_SYSTEM_TIME;
		break;
	case SCONTEXT_VALUE_PEDOMETER_USERHEIGHT :
		ret = PEDOMETER_USERHEIGHT;
		break;
	case SCONTEXT_VALUE_PEDOMETER_USERWEIGHT:
		ret = PEDOMETER_USERWEIGHT;
		break;
	case SCONTEXT_VALUE_PEDOMETER_USERGENDER:
		ret = PEDOMETER_USERGENDER;
		break;
	case SCONTEXT_VALUE_PEDOMETER_INFOUPDATETIME:
		ret = PEDOMETER_INFOUPDATETIME;
		break;
	default:
		ret = ERROR;
	}

	return ret;
}

int convert_scontext_getvalue_subcmd(int subcmd)
{
	int ret = -1;
	switch (subcmd) {
	case SCONTEXT_VALUE_CURRENTSTATUS :
		ret = LIBRARY_CURRENTSTATUS;
		break;
	case SCONTEXT_VALUE_CURRENTSTATUS_BATCH :
		ret = LIBRARY_CURRENTSTATUS_BATCH;
		break;
	case SCONTEXT_VALUE_VERSIONINFO:
		ret = LIBRARY_VERSIONINFO;
		break;
	default:
		ret = ERROR;
	}

	return ret;
}


#define SS_SENSOR_TYPE_CALL_POSE                  (SS_SENSOR_TYPE_BASE + 2)
#define SS_SENSOR_TYPE_PEDOMETER                  (SS_SENSOR_TYPE_BASE + 3)
#define SS_SENSOR_TYPE_MOTION                     (SS_SENSOR_TYPE_BASE + 4)
#define SS_SENSOR_TYPE_GESTURE_APPROACH           (SS_SENSOR_TYPE_BASE + 5)
#define SS_SENSOR_TYPE_STEP_COUNT_ALERT           (SS_SENSOR_TYPE_BASE + 6)
#define SS_SENSOR_TYPE_AUTO_ROTATION              (SS_SENSOR_TYPE_BASE + 7)
#define SS_SENSOR_TYPE_MOVEMENT                   (SS_SENSOR_TYPE_BASE + 8)
#define SS_SENSOR_TYPE_MOVEMENT_FOR_POSITIONING   (SS_SENSOR_TYPE_BASE + 9)
#define SS_SENSOR_TYPE_DIRECT_CALL                (SS_SENSOR_TYPE_BASE + 10)
#define SS_SENSOR_TYPE_STOP_ALERT                 (SS_SENSOR_TYPE_BASE + 11)
#define SS_SENSOR_TYPE_ENVIRONMENT_SENSOR         (SS_SENSOR_TYPE_BASE + 12)
#define SS_SENSOR_TYPE_SHAKE_MOTION               (SS_SENSOR_TYPE_BASE + 13)
#define SS_SENSOR_TYPE_FLIP_COVER_ACTION          (SS_SENSOR_TYPE_BASE + 14)
#define SS_SENSOR_TYPE_GYRO_TEMPERATURE           (SS_SENSOR_TYPE_BASE + 15)
#define SS_SENSOR_TYPE_PUT_DOWN_MOTION            (SS_SENSOR_TYPE_BASE + 16)
#define SS_SENSOR_TYPE_BOUNCE_SHORT_MOTION        (SS_SENSOR_TYPE_BASE + 18)
#define SS_SENSOR_TYPE_BOUNCE_LONG_MOTION         (SS_SENSOR_TYPE_BASE + 20)
#define SS_SENSOR_TYPE_WRIST_UP_MOTION            (SS_SENSOR_TYPE_BASE + 19)
#define SS_SENSOR_TYPE_FLAT_MOTION                (SS_SENSOR_TYPE_BASE + 21)
#define SS_SENSOR_TYPE_MOVEMENT_ALERT             (SS_SENSOR_TYPE_BASE + 22)
#define SS_SENSOR_TYPE_TEST_FLAT_MOTION           (SS_SENSOR_TYPE_BASE + 23)
#define SS_SENSOR_TYPE_TEMPERATURE_ALERT          (SS_SENSOR_TYPE_BASE + 24)
#define SS_SENSOR_TYPE_SPECIFIC_POSE_ALERT        (SS_SENSOR_TYPE_BASE + 25)
#define SS_SENSOR_TYPE_ACTIVITY_TRACKER           (SS_SENSOR_TYPE_BASE + 26)
#define SS_SENSOR_TYPE_STAYING_ALERT              (SS_SENSOR_TYPE_BASE + 27)
#define SS_SENSOR_TYPE_APDR                       (SS_SENSOR_TYPE_BASE + 28)
#define SS_SENSOR_TYPE_LIFE_LOG_COMPONENT         (SS_SENSOR_TYPE_BASE + 29)
#define SS_SENSOR_TYPE_CARE_GIVER                 (SS_SENSOR_TYPE_BASE + 30)
#define SS_SENSOR_TYPE_STEP_DETECTOR              (SS_SENSOR_TYPE_BASE + 31) //MR2 sensors are not defined in SContext. But this must be implemented as Library Type. [6-July-2016]
#define SS_SENSOR_TYPE_SIGNIFICANT_MOTION         (SS_SENSOR_TYPE_BASE + 32)
#define SS_SENSOR_TYPE_UNCALIBRATED_GYRO          (SS_SENSOR_TYPE_BASE + 33)
#define SS_SENSOR_TYPE_ROTATION_VECTOR            (SS_SENSOR_TYPE_BASE + 35)
#define SS_SENSOR_TYPE_STEP_COUNTER               (SS_SENSOR_TYPE_BASE + 36)
#define SS_SENSOR_TYPE_SLEEP_MONITOR              (SS_SENSOR_TYPE_BASE + 37)
#define SS_SENSOR_TYPE_ABNORMAL_SHOCK             (SS_SENSOR_TYPE_BASE + 38)
#define SS_SENSOR_TYPE_CAPTURE_MOTION             (SS_SENSOR_TYPE_BASE + 39)
#define SS_SENSOR_TYPE_CALL_MOTION                (SS_SENSOR_TYPE_BASE + 41)
#define SS_SENSOR_TYPE_STEP_LEVEL_MONITOR         (SS_SENSOR_TYPE_BASE + 44)
#define SS_SENSOR_TYPE_FLAT_MOTION_FOR_TABLE_MODE (SS_SENSOR_TYPE_BASE + 45)
#define SS_SENSOR_TYPE_EXERCISE                   (SS_SENSOR_TYPE_BASE + 46)
#define SS_SENSOR_TYPE_PHONE_STATE_MONITOR        (SS_SENSOR_TYPE_BASE + 47)
#define SS_SENSOR_TYPE_AUTO_BRIGHTNESS            (SS_SENSOR_TYPE_BASE + 48)
#define SS_SENSOR_TYPE_ABNORMAL_PRESSURE_MONITOR  (SS_SENSOR_TYPE_BASE + 49)
#define SS_SENSOR_TYPE_HALL_SENSOR                (SS_SENSOR_TYPE_BASE + 50)
#define SS_SENSOR_TYPE_EAD                        (SS_SENSOR_TYPE_BASE + 52)
#define SS_SENSOR_TYPE_DUAL_DISPLAY_ANGLE         (SS_SENSOR_TYPE_BASE + 53)
#define SS_SENSOR_TYPE_WIRELESS_CHARGING_MONITOR  (SS_SENSOR_TYPE_BASE + 54)
#define SS_SENSOR_TYPE_SLOCATION                  (SS_SENSOR_TYPE_BASE + 55)
#define SS_SENSOR_TYPE_DPCM                       (SS_SENSOR_TYPE_BASE + 56)
#define SS_SENSOR_TYPE_MAIN_SCREEN_DETECTION      (SS_SENSOR_TYPE_BASE + 57)
#define SS_SENSOR_TYPE_ANY_MOTION_DETECTOR        (SS_SENSOR_TYPE_BASE + 58)
#define SS_SENSOR_TYPE_SENSOR_STATUS_CHECK        (SS_SENSOR_TYPE_BASE + 59)
#define SS_SENSOR_TYPE_ACTIVITY_CALIBRATION       (SS_SENSOR_TYPE_BASE + 60)


void get_ss_sensor_name(struct ssp_data *data, int type, char *buf, int buf_size)
{
	memset(buf, 0, buf_size);
	switch(type) {
		case SS_SENSOR_TYPE_PEDOMETER :
			strncpy(buf, "pedoneter", buf_size);
			break;
		case SS_SENSOR_TYPE_STEP_COUNT_ALERT :
			strncpy(buf, "step count alert", buf_size);
			break;
		case SS_SENSOR_TYPE_AUTO_ROTATION :
			strncpy(buf, "auto rotation", buf_size);
			break;
		case SS_SENSOR_TYPE_SLOCATION :
			strncpy(buf, "slocation", buf_size);
			break;
		case SS_SENSOR_TYPE_MOVEMENT :
			strncpy(buf, "smart alert", buf_size);
			break;
		case SS_SENSOR_TYPE_ACTIVITY_TRACKER :
			strncpy(buf, "activity tracker", buf_size);
			break;
		case SS_SENSOR_TYPE_DPCM :
			strncpy(buf, "dpcm", buf_size);
			break;
		case SS_SENSOR_TYPE_SENSOR_STATUS_CHECK :
			strncpy(buf, "sensor status check", buf_size);
			break;
		case SS_SENSOR_TYPE_ACTIVITY_CALIBRATION :	
			strncpy(buf, "activity calibration", buf_size);
			break;

	}

	return;
}


static int ssp_scontext_send_instruction(struct ssp_data *data,
                                          const char *buf, int count)
{
	char command, type, sub_cmd = 0;
	char *buffer = (char *)(buf + 2);
	int length = count - 2;
	char name[SENSOR_NAME_MAX_LEN] = "";
	if (buf[0] == SCONTEXT_INST_LIBRARY_REMOVE) {
		command = CMD_REMOVE;
		type = buf[1] + SS_SENSOR_TYPE_BASE;
		get_ss_sensor_name(data, type, name, sizeof(name));
		ssp_infof("REMOVE LIB %s, type %d", name, type);

		return disable_sensor(data, type, buffer, length);
	} else if (buf[0] == SCONTEXT_INST_LIBRARY_ADD) {
		command = CMD_ADD;
		type = buf[1] + SS_SENSOR_TYPE_BASE;
		get_ss_sensor_name(data, type, name, sizeof(name));
		ssp_infof("ADD LIB, type %d", type);

		return enable_sensor(data, type, buffer, length);
	} else if (buf[0] == SCONTEXT_INST_LIB_SET_DATA) {
		command = CMD_SETVALUE;
		if (buf[1] != SCONTEXT_VALUE_LIBRARY_DATA) {
			type = TYPE_MCU;
			sub_cmd = convert_scontext_putvalue_subcmd(buf[1]);
		} else {
			type = buf[2] + SS_SENSOR_TYPE_BASE;
			sub_cmd = LIBRARY_DATA;
			length = count - 3;
			if (length > 0) {
				buffer = (char *)(buf + 3);
			} else {
				buffer = NULL;
			}
		}
	} else if (buf[0] == SCONTEXT_INST_LIB_GET_DATA) {
		command = CMD_GETVALUE;
		type = buf[1] + SS_SENSOR_TYPE_BASE;
		sub_cmd =  convert_scontext_getvalue_subcmd(buf[2]);
		length = count - 3;
		if (length > 0) {
			buffer = (char *)(buf + 3);
		} else {
			buffer = NULL;
		}
	} else {
		ssp_errf("0x%x is not supported", buf[0]);
		return ERROR;
	}

	return ssp_send_command(data, command, type, sub_cmd, 0,
	                        buffer, length, NULL, NULL);
}

static ssize_t ssp_scontext_write(struct file *file, const char __user *buf,
                                   size_t count, loff_t *pos)
{
	struct ssp_data *data = container_of(file->private_data, struct ssp_data, scontext_device);
	int ret = 0;
	char *buffer;

	if (!is_sensorhub_working(data)) {
		ssp_errf("stop sending library data(is not working)");
		return -EIO;
	}

	if (unlikely(count < 2)) {
		ssp_errf("library data length err(%d)", (int)count);
		return -EINVAL;
	}

	buffer = kzalloc(count * sizeof(char), GFP_KERNEL);

	ret = copy_from_user(buffer, buf, count);
	if (unlikely(ret)) {
		ssp_errf("memcpy for kernel buffer err");
		ret = -EFAULT;
		goto exit;
	}

	ssp_scontext_log(__func__, buffer, count);

	if (buffer[0] == SCONTEXT_INST_LIB_NOTI) {
		ret = ssp_scontext_send_cmd(data, buffer, count);
	} else {
		ret = ssp_scontext_send_instruction(data, buffer, count);
	}

	if (unlikely(ret < 0)) {
		ssp_errf("send library data err(%d)", ret);
		if (ret == ERROR) {
			ret = -EIO;
		}
		
		else if (ret == FAIL) {
			ret = -EAGAIN;
		}

		goto exit;
	}

	ret = count;

exit:
	kfree(buffer);
	return ret;
}

static struct file_operations ssp_scontext_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.write = ssp_scontext_write,
};

int ssp_scontext_initialize(struct ssp_data *data)
{
	int ret;
	ssp_dbgf("----------");
	
	/* register scontext misc device */
	data->scontext_device.minor = MISC_DYNAMIC_MINOR;
	data->scontext_device.name = "ssp_sensorhub";
	data->scontext_device.fops = &ssp_scontext_fops;

	ret = misc_register(&data->scontext_device);
	if (ret < 0) {
		ssp_errf("register scontext misc device err(%d)", ret);
	}

	return ret;
}

void ssp_scontext_remove(struct ssp_data *data)
{
	ssp_scontext_fops.write = NULL;
	misc_deregister(&data->scontext_device);
}

MODULE_DESCRIPTION("Seamless Sensor Platform(SSP) sensorhub driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
