/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SENSOR_RELOADINFO_THREAD_H_
#define _SENSOR_RELOADINFO_THREAD_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <video/sensor_drv_k.h>
#include <linux/delay.h>
#include "sensor_drv_sprd.h"
#include "otp/sensor_otp.h"
#include "identify/sensor_identify.h"

int sensor_reloadinfo_thread(void *data);
void sensor_get_fw_version_otp(void *read_fw_version);
void sensor_get_front_sensor_pid(uint32_t *read_sensor_pid);
#endif
