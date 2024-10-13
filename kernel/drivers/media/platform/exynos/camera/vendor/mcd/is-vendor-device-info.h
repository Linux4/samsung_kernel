/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VENDOR_DEVICE_INFO_H
#define IS_VENDOR_DEVICE_INFO_H

#include "is-core.h"

#ifndef _LINUX_TYPES_H
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#endif

#define DEVICE_INFO_SUPPORTED_CAM_LIST_SIZE_MAX 20
#define DEVICE_INFO_TILT_CAL_TELE_EFS_SIZE_MAX 800
#define DEVICE_INFO_GYRO_EFS_SIZE_MAX 30
#define DEVICE_INFO_BPC_OTP_SIZE_MAX 0x9000 /* 36 KB */
#define DEVICE_INFO_ROM_SIZE_MAX 0x10000 /* 64 KB */
#define DEVICE_INFO_MIPI_PHY_MAX_SIZE 3000

struct device_info_data_ctrl {
	int32_t param1;
	union {
		int32_t int32;
		char *char_ptr;
		uint8_t *uint8_ptr;
		int32_t *int32_ptr;
		uint32_t *uint32_ptr;
		void *void_ptr;
	};
	uint32_t ptr_size;
};

struct device_info_romdata_sec2lsi {
	uint32_t camID;
	unsigned char *secBuf;
	unsigned char *lsiBuf;
	unsigned char *mdInfo; // Module information data in the calmap Header area
	uint32_t awb_size;
	uint32_t lsc_size;
};

struct device_info_efs_data {
	uint8_t *tilt_cal_tele_efs_buf;
	uint8_t *tilt_cal_tele2_efs_buf;
	uint8_t *gyro_efs_buf;
	uint32_t tilt_cal_tele_efs_size;
	uint32_t tilt_cal_tele2_efs_size;
	uint32_t gyro_efs_size;
};

#define MIPI_PHY_REG_MAX_SIZE 40
struct device_info_mipi_phy_reg {
	uint32_t reg;
	char *reg_name;
};

static struct device_info_mipi_phy_reg device_info_mipi_phy_reg_list[MIPI_PHY_REG_MAX_SIZE] = {
	{0x1000, "M_BIAS_CON0"},
	{0x1004, "M_BIAS_CON1"},
	{0x1008, "M_BIAS_CON2"},
	{0x1010, "M_BIAS_CON4"},
	{0x1114, "M_PLL_CON5"},
	{0x1118, "M_PLL_CON6"},
	{0x111C, "M_PLL_CON7"},
	{0x1120, "M_PLL_CON8"},
	{0x1124, "M_PLL_CON9"},
	{0x0004, "SD_GNR_CON1"},
	{0x0008, "SD_ANA_CON0"},
	{0x000C, "SD_ANA_CON1"},
	{0x0010, "SD_ANA_CON2"},
	{0x0014, "SD_ANA_CON3"},
	{0x0018, "SD_ANA_CON4"},
	{0x001C, "SD_ANA_CON5"},
	{0x0020, "SD_ANA_CON6"},
	{0x0024, "SD_ANA_CON7"},
	{0x0030, "SD_TIME_CON0"},
	{0x0034, "SD_TIME_CON1"},
	{0x005C, "SD_CRC_DLY_CON0"},
	{0x0064, "SD_CRC_CON1"},
	{0x0068, "SD_CRC_CON2"},
	{0x009c, "SD_CRC_DLY_CON3"},
	{0x0000, "SD_GNR_CON0"},
};

int is_vendor_device_info_get_factory_supported_id(void __user *user_data);
int is_vendor_device_info_get_rom_data_by_position(void __user *user_data);
int is_vendor_device_info_set_efs_data(void __user *user_data);
int is_vendor_device_info_get_sensorid_by_cameraid(void __user *user_data);
int is_vendor_device_info_get_awb_data_addr(void __user *user_data);
int is_vendor_device_info_perform_cal_reload(void __user *user_data);
int is_vendor_device_info_get_ois_hall_data(void __user *user_data);
#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
int is_vendor_device_info_get_crop_shift_data(void __user *user_data);
#endif
int is_vendor_device_info_get_bpc_otp_data(void __user *user_data);
int is_vendor_device_info_get_mipi_phy(void __user *user_data);
int is_vendor_device_info_set_mipi_phy(void __user *user_data);
int is_vendor_device_info_get_sec2lsi_module_info(void __user *user_data);
int is_vendor_device_info_get_sec2lsi_buff(void __user *user_data);
int is_vendor_device_info_set_sec2lsi_buff(void __user *user_data);

#endif /* IS_VENDOR_DEVICE_INFO_H */
