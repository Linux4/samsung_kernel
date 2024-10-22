/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     hi847mipiraw_Sensor.h
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _HI847MIPI_SENSOR_H
#define _HI847MIPI_SENSOR_H

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "hi847_ana_gain_table.h"
#include "hi847_Sensor_setting.h"

#include "adaptor-subdrv-ctrl.h"
#include "adaptor-i2c.h"
#include "adaptor.h"
#include "adaptor-ctrls.h"

#define HI847_PLL_ENABLE_ADDR    0x0732

#define HI847_OTP_CHECK_BANK    0x0700
#define HI847_OTP_BANK1_MARK    0x01
#define HI847_OTP_BANK2_MARK    0x03
#define HI847_OTP_BANK3_MARK    0x07
#define HI847_OTP_BANK4_MARK    0x0F
#define HI847_OTP_BANK1_START_ADDR  0x0704
#define HI847_OTP_BANK2_START_ADDR  0x0D04
#define HI847_OTP_BANK3_START_ADDR  0x1304
#define HI847_OTP_BANK4_START_ADDR  0x1904

#endif
