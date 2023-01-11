/*
 * camera_mmp.h - marvell camera driver header file
 *
 * Based on PXA camera.h file:
 * Copyright (C) 2014 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_CAMERA_H__
#define __ASM_ARCH_CAMERA_H__

#include <media/mrvl-camera.h>
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
#include <media/mc_debug.h>
#endif

struct sensor_power_data {
	unsigned char *sensor_name; /*row sensor name  */
	int rst_gpio; /* sensor reset GPIO */
	int pwdn_gpio;  /* sensor power enable GPIO*/
	int rst_en; /* sensor reset value: 0 or 1 */
	int pwdn_en; /* sensor power value: 0 or 1*/
	const char *afvcc;
	const char *avdd;
};

struct sensor_type {
	unsigned char chip_id;
	unsigned char *sensor_name;
	unsigned int reg_num;
	long reg_pid[3];/* REG_PIDH REG_PIDM REG_PIDL */
	/* REG_PIDH_VALUE REG_PIDM_VALUE REG_PIDL_VALUE */
	unsigned char reg_pid_val[3];
};

struct sensor_platform_data {
	int *chip_ident_id;
	struct sensor_power_data *sensor_pwd;
	struct sensor_type *sensor_cid;
	int sensor_num;
	int flash_enable;	/* Flash enable data; -1 means invalid */
	int torch_enable;	/* Torch enable data; -1 means invalid */
	int (*power_on)(int);
};

#endif
