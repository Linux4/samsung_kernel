/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#ifndef _CAM_EEPROM_CORE_H_
#define _CAM_EEPROM_CORE_H_

#include "cam_eeprom_dev.h"

int32_t cam_eeprom_driver_cmd(struct cam_eeprom_ctrl_t *e_ctrl, void *arg);
int32_t cam_eeprom_parse_read_memory_map(struct device_node *of_node,
	struct cam_eeprom_ctrl_t *e_ctrl);
/**
 * @e_ctrl: EEPROM ctrl structure
 *
 * This API handles the shutdown ioctl/close
 */
void cam_eeprom_shutdown(struct cam_eeprom_ctrl_t *e_ctrl);
int32_t cam_eeprom_check_firmware_cal(uint32_t camera_cal_crc, uint8_t cal_map_version, cam_eeprom_idx_type idx);

#if defined(CONFIG_SEC_A90Q_PROJECT)
#define TOF_UID_FRONT_PARTRON_1     0xCA16     // Partron 80Mhz~20Mhz 2.1A
#define TOF_UID_FRONT_PARTRON_2     0xCA26     // Partron 80Mhz~20Mhz 1.5A
#define TOF_UID_FRONT_SEC_1         0xAA16     // Samsung 80Mhz~20Mhz 2.1A
#define TOF_UID_FRONT_SEC_2         0xAA26     // Samsung 80Mhz~20Mhz 1.5A
#define TOF_UID_REAR_PARTRON        0xCA06     // Partron 80Mhz~20Mhz 2.5A
#define TOF_UID_REAR_SEC            0xAA06     // Samsung 80Mhz~20Mhz 2.5A
#define TOF_UID_DEFAULT             0xCA05     // Default 100Mhz~20Mhz 2.8A
#endif

#endif
/* _CAM_EEPROM_CORE_H_ */
