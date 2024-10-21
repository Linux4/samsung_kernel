// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Inc.
 */

#include "imgsensor_eeprom_rear_hi1337.h"

#define REAR_HI1337_MAX_CAL_SIZE               (0x1C89 + 0x1)

#define REAR_HI1337_HEADER_CHECKSUM_LEN        (0x00FB - 0x0000 + 0x1)
#define REAR_HI1337_OEM_CHECKSUM_LEN           (0x0E1B - 0x0DD0 + 0x1)
#define REAR_HI1337_AWB_CHECKSUM_LEN           (0x164B - 0x1630 + 0x1)
#define REAR_HI1337_SHADING_CHECKSUM_LEN       (0x1A4B - 0x1650 + 0x1)

#define REAR_HI1337_DUAL_CHECKSUM_LEN          -1
#define REAR_HI1337_PDAF_CHECKSUM_LEN          (0x162B - 0x0E20 + 0x1)
#define REAR_HI1337_OIS_CHECKSUM_LEN           -1

#define REAR_HI1337_CONVERTED_MAX_CAL_SIZE     (0x1FD9 + 0x1)
#define REAR_HI1337_CONVERTED_AWB_CHECKSUM_LEN (0x163B - 0x1630 + 0x1)
#define REAR_HI1337_CONVERTED_LSC_CHECKSUM_LEN (0x1D9B - 0x1640 + 0x1)

const struct rom_converted_cal_addr rear_hi1337_converted_cal_addr = {
	.rom_awb_cal_data_start_addr            = 0x1630,
	.rom_awb_checksum_addr                  = 0x163C,
	.rom_awb_checksum_len                   = (REAR_HI1337_CONVERTED_AWB_CHECKSUM_LEN),
	.rom_shading_cal_data_start_addr        = 0x1640,
	.rom_shading_checksum_addr              = 0x1D9C,
	.rom_shading_checksum_len               = (REAR_HI1337_CONVERTED_LSC_CHECKSUM_LEN),
};

const struct imgsensor_vendor_rom_addr rear_hi1337_cal_addr = {
	/* Set '-1' if not used */
	.camera_module_es_version               = 'A',
	.cal_map_es_version                     = '1',
	.rom_max_cal_size                       = REAR_HI1337_MAX_CAL_SIZE,
	.rom_header_cal_data_start_addr         = 0x00,
	.rom_header_main_module_info_start_addr = 0x6E,
	.rom_header_cal_map_ver_start_addr      = 0x90,
	.rom_header_project_name_start_addr     = 0x98,
	.rom_header_module_id_addr              = 0xB6, 
	.rom_header_main_sensor_id_addr         = 0xC0,

	.rom_header_sub_module_info_start_addr  = -1,
	.rom_header_sub_sensor_id_addr          = -1,

	.rom_header_main_header_start_addr      = 0x00,
	.rom_header_main_header_end_addr        = 0x04,
	.rom_header_main_oem_start_addr         = 0x30,
	.rom_header_main_oem_end_addr           = 0x34,
	.rom_header_main_awb_start_addr         = 0x40,
	.rom_header_main_awb_end_addr           = 0x44,
	.rom_header_main_shading_start_addr     = 0x48,
	.rom_header_main_shading_end_addr       = 0x4C,
	.rom_header_main_sensor_cal_start_addr  = -1,  
	.rom_header_main_sensor_cal_end_addr    = -1,
	.rom_header_dual_cal_start_addr         = -1,
	.rom_header_dual_cal_end_addr           = -1,
	.rom_header_pdaf_cal_start_addr         = 0x38,
	.rom_header_pdaf_cal_end_addr           = 0x3C,
	.rom_header_ois_cal_start_addr          = -1,
	.rom_header_ois_cal_end_addr            = -1,

	.rom_header_sub_oem_start_addr          = -1,
	.rom_header_sub_oem_end_addr            = -1,
	.rom_header_sub_awb_start_addr          = -1,
	.rom_header_sub_awb_end_addr            = -1,
	.rom_header_sub_shading_start_addr      = -1,
	.rom_header_sub_shading_end_addr        = -1,

	.rom_header_main_mtf_data_addr          = -1, // It must be in af area, not in factory area in cal data
	.rom_header_sub_mtf_data_addr           = -1, // It must be in af area, not in factory area in cal data

	.rom_header_checksum_addr               = 0x00FC,
	.rom_header_checksum_len                = REAR_HI1337_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr           = 0x0DDC,
	.rom_oem_af_macro_position_addr         = 0x0DE8,
	.rom_oem_module_info_start_addr         = -1,
	.rom_oem_checksum_addr                  = 0x0E1C,
	.rom_oem_checksum_len                   = REAR_HI1337_OEM_CHECKSUM_LEN,

	.rom_module_cal_data_start_addr         = -1,
	.rom_module_module_info_start_addr      = -1,
	.rom_module_checksum_addr               = -1,
	.rom_module_checksum_len                = -1,

	.rom_awb_cal_data_start_addr            = 0x1630,
	.rom_awb_module_info_start_addr         = -1,
	.rom_awb_checksum_addr                  = 0x164C,
	.rom_awb_checksum_len                   = REAR_HI1337_AWB_CHECKSUM_LEN,

	.rom_shading_cal_data_start_addr        = 0x1650,
	.rom_shading_module_info_start_addr     = -1,
	.rom_shading_checksum_addr              = 0x1A4C,
	.rom_shading_checksum_len               = REAR_HI1337_SHADING_CHECKSUM_LEN,

	.rom_sensor_cal_module_info_start_addr  = -1,
	.rom_sensor_cal_checksum_addr           = -1,
	.rom_sensor_cal_checksum_len            = -1,

	.rom_dual_module_info_start_addr        = -1,
	.rom_dual_checksum_addr                 = -1,
	.rom_dual_checksum_len                  = -1,

	.rom_pdaf_module_info_start_addr        = 0x0E20,
	.rom_pdaf_checksum_addr                 = 0x162C,
	.rom_pdaf_checksum_len                  = REAR_HI1337_PDAF_CHECKSUM_LEN,
	.rom_ois_checksum_addr                  = -1,
	.rom_ois_checksum_len                   = REAR_HI1337_OIS_CHECKSUM_LEN,

	.rom_sub_oem_af_inf_position_addr       = -1,
	.rom_sub_oem_af_macro_position_addr     = -1,
	.rom_sub_oem_module_info_start_addr     = -1,
	.rom_sub_oem_checksum_addr              = -1,
	.rom_sub_oem_checksum_len               = -1,

	.rom_sub_awb_module_info_start_addr     = -1,
	.rom_sub_awb_checksum_addr              = -1,
	.rom_sub_awb_checksum_len               = -1,

	.rom_sub_shading_module_info_start_addr = -1,
	.rom_sub_shading_checksum_addr          = -1,
	.rom_sub_shading_checksum_len           = -1,

	.rom_dual_cal_data2_start_addr          = -1,
	.rom_dual_cal_data2_size                = -1,
	.rom_dual_tilt_x_addr                   = -1,
	.rom_dual_tilt_y_addr                   = -1,
	.rom_dual_tilt_z_addr                   = -1,
	.rom_dual_tilt_sx_addr                  = -1,
	.rom_dual_tilt_sy_addr                  = -1,
	.rom_dual_tilt_range_addr               = -1,
	.rom_dual_tilt_max_err_addr             = -1,
	.rom_dual_tilt_avg_err_addr             = -1,
	.rom_dual_tilt_dll_version_addr         = -1,
	.rom_dual_tilt_dll_modelID_addr         = -1,
	.rom_dual_tilt_dll_modelID_size         = -1,
	.rom_dual_shift_x_addr                  = -1,
	.rom_dual_shift_y_addr                  = -1,

	.extend_cal_addr                        = NULL,

	.converted_cal_addr                     = &rear_hi1337_converted_cal_addr,
	.rom_converted_max_cal_size             = REAR_HI1337_CONVERTED_MAX_CAL_SIZE,

	.sensor_maker                           = "SKHYNIX",  // SENSOR_MAKER NAME 
	.sensor_name                            = "HI1337",
	.sub_sensor_maker                       = NULL,
	.sub_sensor_name                        = NULL,

	.bayerformat                            = -1,
};
