/*
 * Samsung Exynos5 SoC series FIMC-IS ROM driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_ROM_H
#define IS_DEVICE_ROM_H

#define IS_HEADER_VER_SIZE      11
#define IS_CAL_MAP_VER_SIZE     4
#define IS_PROJECT_NAME_SIZE    8
#define IS_SENSOR_NAME_SIZE     8
#define IS_SENSOR_ID_SIZE       16
#define IS_MODULE_ID_SIZE       10
#define IS_RESOLUTION_DATA_SIZE 54
#define IS_AWB_MASTER_DATA_SIZE 8
#define IS_AWB_MODULE_DATA_SIZE 8
#define IS_OIS_GYRO_DATA_SIZE 4
#define IS_OIS_COEF_DATA_SIZE 2
#define IS_OIS_SUPPERSSION_RATIO_DATA_SIZE 2
#define IS_OIS_CAL_MARK_DATA_SIZE 1

#define IS_PAF_OFFSET_MID_OFFSET		(0x0730) /* REAR PAF OFFSET MID (30CM, WIDE) */
#define IS_PAF_OFFSET_MID_SIZE 936
#define IS_PAF_OFFSET_PAN_OFFSET		(0x0CD0) /* REAR PAF OFFSET FAR (1M, WIDE) */
#define IS_PAF_OFFSET_PAN_SIZE 234
#define IS_PAF_CAL_ERR_CHECK_OFFSET	0x14

#define IS_ROM_CRC_MAX_LIST 150
#define IS_ROM_DUAL_TILT_MAX_LIST 10
#define IS_DUAL_TILT_PROJECT_NAME_SIZE 20
#define IS_ROM_OIS_MAX_LIST 14
#define IS_ROM_OIS_SINGLE_MODULE_MAX_LIST 7

#define SEC2LSI_AWB_DATA_SIZE 8
#define SEC2LSI_LSC_DATA_SIZE 6632
#define SEC2LSI_MODULE_INFO_SIZE		11
#define SEC2LSI_CHECKSUM_SIZE 4

#define IS_TOF_CAL_SIZE_ONCE	4095
#define IS_TOF_CAL_CAL_RESULT_OK	0x11

#define AF_CAL_MAX 9
#define CROSSTALK_CAL_MAX (3 * 13)
#define TOF_CAL_SIZE_MAX 10
#define TOF_CAL_VALID_MAX 10

struct is_vendor_standard_cal {
	int32_t rom_standard_cal_start_addr;
	int32_t rom_standard_cal_end_addr;
	int32_t rom_standard_cal_module_crc_addr;
	int32_t rom_standard_cal_module_checksum_len;
	int32_t rom_standard_cal_sec2lsi_end_addr;

	int32_t rom_header_standard_cal_end_addr;
	int32_t rom_header_main_shading_end_addr;

	int32_t rom_awb_start_addr;
	int32_t rom_awb_end_addr;
	int32_t rom_awb_section_crc_addr;

	int32_t rom_shading_start_addr;
	int32_t rom_shading_end_addr;
	int32_t rom_shading_section_crc_addr;

	int32_t rom_factory_start_addr;
	int32_t rom_factory_end_addr;

	int32_t rom_awb_sec2lsi_start_addr;
	int32_t rom_awb_sec2lsi_end_addr;
	int32_t rom_awb_sec2lsi_checksum_addr;
	int32_t rom_awb_sec2lsi_checksum_len;

	int32_t rom_shading_sec2lsi_start_addr;
	int32_t rom_shading_sec2lsi_end_addr;
	int32_t rom_shading_sec2lsi_checksum_addr;
	int32_t rom_shading_sec2lsi_checksum_len;

	int32_t rom_factory_sec2lsi_start_addr;
};

struct is_vendor_rom {
	struct i2c_client *client;
	bool valid;

	char *buf;
	char *sec_buf; /* sec_buf is used for storing original rom data, before standard cal sec2lsi conversion */

	bool read_done;
	bool read_error;
	bool crc_error;
	bool final_module;
	bool latest_module;
	bool other_vendor_module;
	bool skip_cal_loading;
	bool skip_crc_check;
	bool skip_header_loading;
	bool ignore_cal_crc_error;
	bool check_dualize;

	u32 crc_retry_cnt;
	u32 rom_type;

	/* set by dts parsing */
	u32 rom_power_position;
	u32 rom_size;
	bool need_i2c_config;	/* Set if the rom i2c_port is separated from the sensor i2c_port. */

	char camera_module_es_version;
	u32 cal_map_es_version;

	u32 header_crc_check_list[IS_ROM_CRC_MAX_LIST];
	u32 header_crc_check_list_len;
	u32 crc_check_list[IS_ROM_CRC_MAX_LIST];
	u32 crc_check_list_len;
	u32 dual_crc_check_list[IS_ROM_CRC_MAX_LIST];
	u32 dual_crc_check_list_len;
	u32 rom_dualcal_slave0_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32 rom_dualcal_slave0_tilt_list_len;
	u32 rom_dualcal_slave1_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32 rom_dualcal_slave1_tilt_list_len;
	u32 rom_dualcal_slave2_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32 rom_dualcal_slave2_tilt_list_len;
	u32 rom_dualcal_slave3_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32 rom_dualcal_slave3_tilt_list_len;
	u32 rom_ois_list[IS_ROM_OIS_MAX_LIST];
	u32 rom_ois_list_len;

	/* set -1 if not support */
	int32_t rom_header_cal_data_start_addr;
	int32_t rom_header_version_start_addr;
	int32_t rom_header_cal_map_ver_start_addr;
	int32_t rom_header_isp_setfile_ver_start_addr;
	int32_t rom_header_project_name_start_addr;
	int32_t rom_header_module_id_addr;
	int32_t rom_header_sensor_id_addr;
	int32_t rom_header_mtf_data_addr;
	int32_t rom_awb_master_addr;
	int32_t rom_awb_module_addr;
	int32_t rom_af_cal_addr[AF_CAL_MAX];
	int32_t rom_af_cal_addr_len;
	int32_t rom_paf_cal_start_addr;
	int32_t sensor_name_addr;

	/* standard cal */
	bool use_standard_cal;
	struct is_vendor_standard_cal standard_cal_data;
	bool sec2lsi_conv_done;

	int32_t rom_header_sensor2_id_addr;
	int32_t rom_header_sensor2_version_start_addr;
	int32_t rom_header_sensor2_mtf_data_addr;
	int32_t rom_sensor2_awb_master_addr;
	int32_t rom_sensor2_awb_module_addr;
	int32_t rom_sensor2_af_cal_addr[AF_CAL_MAX];
	int32_t rom_sensor2_af_cal_addr_len;
	int32_t rom_sensor2_paf_cal_start_addr;

	int32_t rom_dualcal_slave0_start_addr;
	int32_t rom_dualcal_slave0_size;
	int32_t rom_dualcal_slave1_start_addr;
	int32_t rom_dualcal_slave1_size;
	int32_t rom_dualcal_slave2_start_addr;
	int32_t rom_dualcal_slave2_size;

	int32_t rom_pdxtc_cal_data_start_addr;
	int32_t rom_pdxtc_cal_data_0_size;
	int32_t rom_pdxtc_cal_data_1_size;

	int32_t rom_spdc_cal_data_start_addr;
	int32_t rom_spdc_cal_data_size;

	int32_t rom_xtc_cal_data_start_addr;
	int32_t rom_xtc_cal_data_size;

	bool rom_pdxtc_cal_endian_check;
	u32 rom_pdxtc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32 rom_pdxtc_cal_data_addr_list_len;
	bool rom_gcc_cal_endian_check;
	u32 rom_gcc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32 rom_gcc_cal_data_addr_list_len;
	bool rom_xtc_cal_endian_check;
	u32 rom_xtc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32 rom_xtc_cal_data_addr_list_len;

	int32_t rom_tof_cal_size_addr[TOF_CAL_SIZE_MAX];
	int32_t rom_tof_cal_size_addr_len;
	int32_t rom_tof_cal_start_addr;
	int32_t rom_tof_cal_mode_id_addr;
	int32_t rom_tof_cal_result_addr;
	int32_t rom_tof_cal_validation_addr[TOF_CAL_VALID_MAX];
	int32_t rom_tof_cal_validation_addr_len;

	u16 rom_dualcal_slave1_cropshift_x_addr;
	u16 rom_dualcal_slave1_cropshift_y_addr;

	u16 rom_dualcal_slave1_oisshift_x_addr;
	u16 rom_dualcal_slave1_oisshift_y_addr;

	u16 rom_dualcal_slave1_dummy_flag_addr;

	char header_ver[IS_HEADER_VER_SIZE + 1];
	char header2_ver[IS_HEADER_VER_SIZE + 1];
	char cal_map_ver[IS_CAL_MAP_VER_SIZE + 1];

	char project_name[IS_PROJECT_NAME_SIZE + 1];
	char sensor_name[IS_SENSOR_NAME_SIZE + 1];
	char rom_sensor_id[IS_SENSOR_ID_SIZE + 1];
	char rom_sensor2_id[IS_SENSOR_ID_SIZE + 1];
	u8 rom_module_id[IS_MODULE_ID_SIZE + 1];
};

struct is_device_rom {
	struct v4l2_device v4l2_dev;
	struct platform_device *pdev;
	unsigned long state;
	struct exynos_platform_is_sensor *pdata;
	struct i2c_client *client;
	struct is_core *core;
	int rom_id;
};

struct i2c_driver *is_get_rom_driver(void);
#endif /* IS_DEVICE_ROM_H */
