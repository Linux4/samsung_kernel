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

#ifndef IS_VENDER_CAMINFO_H
#define IS_VENDER_CAMINFO_H

#include "is-core.h"

#ifndef _LINUX_TYPES_H
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#endif

#define IS_CAMINFO_IOCTL_MAGIC 0xFB

#define IS_CAMINFO_IOCTL_COMMAND	_IOWR(IS_CAMINFO_IOCTL_MAGIC, 0x01, caminfo_ioctl_cmd *)

typedef struct
{
	uint32_t cmd;
	void *data;
} caminfo_ioctl_cmd;

enum caminfo_cmd_id
{
	CAMINFO_CMD_ID_GET_FACTORY_SUPPORTED_ID = 0,
	CAMINFO_CMD_ID_GET_ROM_DATA_BY_POSITION = 1,
	CAMINFO_CMD_ID_SET_EFS_DATA = 2,
	CAMINFO_CMD_ID_GET_SENSOR_ID = 3,
	CAMINFO_CMD_ID_GET_AWB_DATA_ADDR = 4,
	CAMINFO_CMD_ID_PERFORM_CAL_RELOAD = 5,
	CAMINFO_CMD_ID_GET_OIS_HALL_DATA = 6,
	CAMINFO_CMD_ID_SET_MIPI_PHY = 8,
	CAMINFO_CMD_ID_GET_MIPI_PHY = 9,
	CAMINFO_CMD_ID_SET_SENSOR_GLOBAL_TEST = 101,
	CAMINFO_CMD_ID_GET_SENSOR_GLOBAL_TEST = 102,
	CAMINFO_CMD_ID_SET_SENSOR_SETFILE_TEST = 103,
	CAMINFO_CMD_ID_GET_SENSOR_SETFILE_TEST = 104,
	CAMINFO_CMD_ID_SET_SENSOR_PLLINFO_TEST = 105,
	CAMINFO_CMD_ID_GET_SENSOR_PLLINFO_TEST = 106,
	CAMINFO_CMD_ID_ENABLE_SENSOR_TEST = 107,
};

#define CAM_MAX_SUPPORTED_LIST	20

typedef struct
{
	uint32_t cam_position;
	uint32_t buf_size;
	uint8_t *buf;
	uint32_t rom_size;
} caminfo_romdata;

typedef struct
{
	uint32_t size;
	uint32_t data[CAM_MAX_SUPPORTED_LIST];
} caminfo_supported_list;

typedef struct
{
	int tilt_cal_tele_efs_size;
	int tilt_cal_tele2_efs_size;
	int gyro_efs_size;
	uint8_t *tilt_cal_tele_efs_buf;
	uint8_t *tilt_cal_tele2_efs_buf;
	uint8_t *gyro_efs_buf;
} caminfo_efs_data;

typedef struct
{
	uint32_t cameraId;
	uint32_t sensorId;
} caminfo_sensor_id;

typedef struct
{
	int cameraId;
	int32_t awb_master_addr;
	int32_t awb_module_addr;
	int32_t awb_master_data_size;
	int32_t awb_module_data_size;
} caminfo_awb_data_addr;

#define CAM_MAX_SENSOR_POSITION	8

typedef struct
{
	uint32_t size;
	uint8_t *data;
} caminfo_bin_data;

typedef struct
{
	caminfo_bin_data ddk;
	caminfo_bin_data rta;
	caminfo_bin_data setfile[CAM_MAX_SENSOR_POSITION];
} caminfo_bin_data_isp;

#ifdef USE_SENSOR_TEST_SETTING
#define GLOBAL_TEST_MAX_SIZE 40000
#define SETFILE_TEST_MAX_SIZE 5000
#define SENSOR_MODE_MAX 100

struct sensor_pll_info_compact_test {
	u32 ext_clk;
	u64 mipi_datarate;
	u64 pclk;
	u32 frame_length_lines;
	u32 line_length_pck;
};

enum sensor_test_type {
	TEST_TYPE_NORMAL = 0,
	TEST_TYPE_RETENTION = 1,
	TEST_TYPE_LOAD_SRAM = 2,
};

struct caminfo_global_test_data {
	uint32_t sensor_id;
	uint32_t type;
	uint32_t *global;
	uint32_t size;
};

struct caminfo_setfile_test_data {
	uint32_t sensor_id;
	uint32_t type;
	uint32_t idx;
	uint32_t *setfile;
	uint32_t size;
};

struct caminfo_pllinfo_test_data {
	uint32_t sensor_id;
	uint32_t idx;
	uint32_t ext_clk;
	uint64_t mipi_datarate;
	uint64_t pclk;
	uint32_t frame_length_lines;
	uint32_t line_length_pck;
};

struct caminfo_enable_sensor_test {
	uint32_t sensor_id;
	bool enable;
};
#endif

#ifdef USE_MIPI_PHY_TUNING
#define MIPI_PHY_DATA_MAX_SIZE 3000
struct caminfo_mipi_phy_data {
	uint32_t position;
	uint8_t *mipi_phy_buf;
	uint32_t mipi_phy_size;
};

#define MIPI_PHY_REG_MAX_SIZE 40
struct caminfo_mipi_phy_reg {
	uint32_t reg;
	char *reg_name;
};

static struct caminfo_mipi_phy_reg caminfo_mipi_phy_reg_list[MIPI_PHY_REG_MAX_SIZE] = {
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
#endif

typedef struct
{
	struct mutex	mlock;
} is_vender_caminfo;

#endif /* IS_VENDER_CAMINFO_H */
