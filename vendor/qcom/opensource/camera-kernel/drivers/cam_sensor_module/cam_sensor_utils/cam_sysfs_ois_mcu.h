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

extern uint8_t ois_m1_xygg[OIS_XYGG_SIZE];
extern uint8_t ois_m1_cal_mark;
extern uint8_t ois_m2_xygg[OIS_XYGG_SIZE];
extern uint8_t ois_m2_cal_mark;
extern uint8_t ois_m3_xygg[OIS_XYGG_SIZE];
extern uint8_t ois_m3_cal_mark;
extern uint8_t ois_m1_xysr[OIS_XYSR_SIZE];
extern uint8_t ois_m2_xysr[OIS_XYSR_SIZE];
extern uint8_t ois_m2_cross_talk[OIS_CROSSTALK_SIZE];
extern uint8_t ois_m3_xysr[OIS_XYSR_SIZE];
extern uint8_t ois_m3_cross_talk[OIS_CROSSTALK_SIZE];

#endif /* _CAM_SYSFS_OIS_MCU_H_ */
