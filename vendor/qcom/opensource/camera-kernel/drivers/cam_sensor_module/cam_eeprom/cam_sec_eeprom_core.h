/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef _CAM_SEC_EEPROM_CORE_H_
#define _CAM_SEC_EEPROM_CORE_H_

#include "cam_eeprom_dev.h"

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
extern uint8_t ois_m1_xygg[OIS_XYGG_SIZE];
extern uint8_t ois_m1_cal_mark;
extern int ois_gain_rear_result;
extern int ois_sr_rear_result;
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
extern uint8_t ois_m1_center_shift[OIS_CENTER_SHIFT_SIZE];
extern uint8_t ois_m2_xygg[OIS_XYGG_SIZE];
extern uint8_t ois_m2_center_shift[OIS_CENTER_SHIFT_SIZE];
extern uint8_t ois_m2_cal_mark;
extern int ois_m2_cross_talk_result;
extern int ois_gain_rear3_result;
extern int ois_sr_rear3_result;
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
extern uint8_t ois_m3_xygg[OIS_XYGG_SIZE];
extern uint8_t ois_m3_center_shift[OIS_CENTER_SHIFT_SIZE];
extern uint8_t ois_m3_cal_mark;
extern int ois_m3_cross_talk_result;
extern int ois_gain_rear4_result;
extern int ois_sr_rear4_result;
#endif
#endif

int cam_sec_eeprom_dump(uint32_t subdev_id, uint8_t *mapdata, uint32_t addr, uint32_t size);
int cam_sec_eeprom_update_module_info(struct cam_eeprom_ctrl_t *e_ctrl);
void cam_sec_eeprom_link_module_info(struct cam_eeprom_ctrl_t *e_ctrl, ModuleInfo_t *mInfo, eeprom_camera_id_type camera_id);
int32_t cam_sec_eeprom_check_firmware_cal(uint32_t camera_cal_crc, uint32_t camera_normal_cal_crc, ModuleInfo_t *mInfo);
uint32_t cam_sec_eeprom_match_crc(struct cam_eeprom_memory_block_t *data, uint32_t subdev_id);
int32_t cam_sec_eeprom_calc_calmap_size(struct cam_eeprom_ctrl_t *e_ctrl);
int32_t cam_sec_eeprom_fill_configInfo(char *configString, uint32_t value, ConfigInfo_t *ConfigInfo);
int32_t cam_sec_eeprom_get_customInfo(struct cam_eeprom_ctrl_t *e_ctrl,	struct cam_packet *csl_packet);
int32_t cam_sec_eeprom_get_phone_ver(struct cam_eeprom_ctrl_t *e_ctrl, struct cam_packet *csl_packet);
#if defined(CONFIG_HI847_OTP)
int cam_otp_hi847_read_memory(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_eeprom_memory_block_t *block);
#endif
#if defined(CONFIG_HI1337_OTP)
int cam_otp_hi1337_read_memory( struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_eeprom_memory_block_t *block);
#endif

#endif
/* _CAM_SEC_EEPROM_CORE_H_ */
