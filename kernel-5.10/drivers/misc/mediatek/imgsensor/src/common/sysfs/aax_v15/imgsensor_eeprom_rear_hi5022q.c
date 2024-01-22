// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Samsung Electronics Inc.
 */

#include "imgsensor_eeprom_rear_hi5022q.h"

#define REAR_HI5022Q_MAX_CAL_SIZE               (0x3119 + 0x1)

#define REAR_HI5022Q_HEADER_CHECKSUM_LEN        (0x00FB - 0x0000 + 0x1)
#define REAR_HI5022Q_OEM_CHECKSUM_LEN           (0x22AB - 0x2260 + 0x1)
#define REAR_HI5022Q_AWB_CHECKSUM_LEN           (0x2ADB - 0x2AC0 + 0x1)
#define REAR_HI5022Q_SHADING_CHECKSUM_LEN       (0x2EDB - 0x2AE0 + 0x1)
#define REAR_HI5022Q_SENSOR_CAL_CHECKSUM_LEN    (0x15CB - 0x0140 + 0x1)
#define REAR_HI5022Q_DUAL_CHECKSUM_LEN          -1
#define REAR_HI5022Q_PDAF_CHECKSUM_LEN          (0x2ABB - 0x22B0 + 0x1)
#define REAR_HI5022Q_OIS_CHECKSUM_LEN           -1

#define REAR_HI5022Q_CONVERTED_MAX_CAL_SIZE     (0x3469 + 0x1)
#define REAR_HI5022Q_CONVERTED_AWB_CHECKSUM_LEN (0x2ACB - 0x2AC0 + 0x1)
#define REAR_HI5022Q_CONVERTED_LSC_CHECKSUM_LEN (0x322B - 0x2AD0 + 0x1)

const struct rom_converted_cal_addr rear_hi5022q_converted_cal_addr = {
	.rom_awb_cal_data_start_addr            = 0x2AC0,
	.rom_awb_checksum_addr                  = 0x2ACC,
	.rom_awb_checksum_len                   = (REAR_HI5022Q_CONVERTED_AWB_CHECKSUM_LEN),
	.rom_shading_cal_data_start_addr        = 0x2AD0,
	.rom_shading_checksum_addr              = 0x322C,
	.rom_shading_checksum_len               = (REAR_HI5022Q_CONVERTED_LSC_CHECKSUM_LEN),
};

const struct rom_cal_addr rear_hi5022q_opc_addr = {
	.name                            = CROSSTALK_CAL_OPC,
	.addr                            = 0x0DC0,
	.size                            = (0x15BF - 0x0DC0 + 1),
	.next                            = NULL,
};

const struct rom_cal_addr rear_hi5022q_qbgc_addr = {
	.name                            = CROSSTALK_CAL_QBGC,
	.addr                            = 0x08C0,
	.size                            = (0x0DBF - 0x08C0 + 1),
	.next                            = &rear_hi5022q_opc_addr,
};

const struct rom_cal_addr rear_hi5022q_xgc_addr = {
	.name                            = CROSSTALK_CAL_XGC,
	.addr                            = 0x0140,
	.size                            = (0x08BF - 0x0140 + 1),
	.next                            = &rear_hi5022q_qbgc_addr,
};

const struct rom_sac_cal_addr rear_hi5022q_sac_addr = {
	.rom_mode_addr = 0x2294,
	.rom_time_addr = 0x2295,
};

const struct imgsensor_vendor_rom_addr rear_hi5022q_cal_addr = {
	/* Set '-1' if not used */
	.camera_module_es_version               = 'A',
	.cal_map_es_version                     = '1',
	.rom_max_cal_size                       = REAR_HI5022Q_MAX_CAL_SIZE,
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
	.rom_header_main_oem_start_addr         = 0x38,
	.rom_header_main_oem_end_addr           = 0x3C,
	.rom_header_main_awb_start_addr         = 0x48,
	.rom_header_main_awb_end_addr           = 0x4C,
	.rom_header_main_shading_start_addr     = 0x50,
	.rom_header_main_shading_end_addr       = 0x54,
	.rom_header_main_sensor_cal_start_addr  = 0x10,
	.rom_header_main_sensor_cal_end_addr    = 0x14,
	.rom_header_dual_cal_start_addr         = -1,
	.rom_header_dual_cal_end_addr           = -1,
	.rom_header_pdaf_cal_start_addr         = 0x40,
	.rom_header_pdaf_cal_end_addr           = 0x44,
	.rom_header_ois_cal_start_addr          = -1,
	.rom_header_ois_cal_end_addr            = -1,

	.rom_header_sub_oem_start_addr          = -1,
	.rom_header_sub_oem_end_addr            = -1,
	.rom_header_sub_awb_start_addr          = -1,
	.rom_header_sub_awb_end_addr            = -1,
	.rom_header_sub_shading_start_addr      = -1,
	.rom_header_sub_shading_end_addr        = -1,

	.rom_header_main_mtf_data_addr          = -1, // Need to check // It must be in af area, not in factory area in cal data
	.rom_header_sub_mtf_data_addr           = -1, // It must be in af area, not in factory area in cal data

	.rom_header_checksum_addr               = 0x00FC,
	.rom_header_checksum_len                = REAR_HI5022Q_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr           = 0x226C,
	.rom_oem_af_macro_position_addr         = 0x2278,
	.rom_oem_module_info_start_addr         = -1,
	.rom_oem_checksum_addr                  = 0x22AC,
	.rom_oem_checksum_len                   = REAR_HI5022Q_OEM_CHECKSUM_LEN,

	.rom_module_cal_data_start_addr         = -1,
	.rom_module_module_info_start_addr      = -1,
	.rom_module_checksum_addr               = -1,
	.rom_module_checksum_len                = -1,

	.rom_awb_cal_data_start_addr            = 0x2AC0,
	.rom_awb_module_info_start_addr         = -1,
	.rom_awb_checksum_addr                  = 0x2ADC,
	.rom_awb_checksum_len                   = REAR_HI5022Q_AWB_CHECKSUM_LEN,

	.rom_shading_cal_data_start_addr        = 0x2AE0,
	.rom_shading_module_info_start_addr     = -1,
	.rom_shading_checksum_addr              = 0x2EDC,
	.rom_shading_checksum_len               = REAR_HI5022Q_SHADING_CHECKSUM_LEN,

	.rom_sensor_cal_module_info_start_addr  = -1,
	.rom_sensor_cal_checksum_addr           = 0x15CC,
	.rom_sensor_cal_checksum_len            = REAR_HI5022Q_SENSOR_CAL_CHECKSUM_LEN,

	.rom_dual_module_info_start_addr        = -1,
	.rom_dual_checksum_addr                 = -1,
	.rom_dual_checksum_len                  = REAR_HI5022Q_DUAL_CHECKSUM_LEN,

	.rom_pdaf_module_info_start_addr        = 0x22B0,
	.rom_pdaf_checksum_addr                 = 0x2ABC,
	.rom_pdaf_checksum_len                  = REAR_HI5022Q_PDAF_CHECKSUM_LEN,
	.rom_ois_checksum_addr                  = -1,
	.rom_ois_checksum_len                   = REAR_HI5022Q_OIS_CHECKSUM_LEN,

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


	.crosstalk_cal_addr                     = &rear_hi5022q_xgc_addr,
	.sac_cal_addr                           = &rear_hi5022q_sac_addr,
	.ois_cal_addr                           = NULL,
	.extend_cal_addr                        = NULL,

	.converted_cal_addr                     = &rear_hi5022q_converted_cal_addr,
	.rom_converted_max_cal_size             = REAR_HI5022Q_CONVERTED_MAX_CAL_SIZE,

	.sensor_maker                           = "SKHYNIX",
	.sensor_name                            = "HI5022Q",
	.sub_sensor_maker                       = NULL,
	.sub_sensor_name                        = NULL,

	.bayerformat                            = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr,
};
