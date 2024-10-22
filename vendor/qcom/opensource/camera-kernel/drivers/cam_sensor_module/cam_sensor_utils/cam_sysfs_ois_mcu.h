/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SYSFS_OIS_MCU_H_
#define _CAM_SYSFS_OIS_MCU_H_

#include <linux/sysfs.h>
#include "cam_sensor_cmn_header.h"
#include "cam_eeprom_dev.h"
#include "cam_actuator_dev.h"

#define OIS_POWER_ON '1'
#define OIS_POWER_OFF '0'

#define ACTUATOR_POWER_ON '1'
#define ACTUATOR_POWER_OFF '0'

extern const struct device_attribute *ois_attrs[];
extern struct cam_ois_ctrl_t *g_o_ctrl;
extern struct cam_actuator_ctrl_t *g_a_ctrls[SEC_SENSOR_ID_MAX];

extern uint8_t ois_xygg[INDEX_MAX][OIS_XYGG_SIZE];
extern uint8_t ois_cal_mark[INDEX_MAX];
extern uint8_t ois_xysr[INDEX_MAX][OIS_XYSR_SIZE];
extern uint8_t ois_cross_talk[INDEX_MAX][OIS_CROSSTALK_SIZE];

#endif /* _CAM_SYSFS_OIS_MCU_H_ */
