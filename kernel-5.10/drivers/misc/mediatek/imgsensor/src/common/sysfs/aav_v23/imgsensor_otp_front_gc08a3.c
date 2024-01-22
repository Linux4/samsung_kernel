#include "imgsensor_otp_front_gc08a3.h"

#define FRONT_GC08A3_MAX_CAL_SIZE               (0x045F + 0x1)

#define FRONT_GC08A3_HEADER_CHECKSUM_LEN        (0x0049 - 0x0000 + 0x1)
#define FRONT_GC08A3_OEM_CHECKSUM_LEN           -1
#define FRONT_GC08A3_AWB_CHECKSUM_LEN           -1
#define FRONT_GC08A3_SHADING_CHECKSUM_LEN       -1
#define FRONT_GC08A3_MODULE_CHECKSUM_LEN        (0x0045B - 0x4E + 0x1)
#define FRONT_GC08A3_SENSOR_CAL_CHECKSUM_LEN    -1
#define FRONT_GC08A3_DUAL_CHECKSUM_LEN          -1
#define FRONT_GC08A3_PDAF_CHECKSUM_LEN          -1
#define FRONT_GC08A3_OIS_CHECKSUM_LEN           -1

#define FRONT_GC08A3_CONVERTED_MAX_CAL_SIZE     (0x07C7 + 0x1)
#define FRONT_GC08A3_CONVERTED_AWB_CHECKSUM_LEN (0x0063 - 0x0058 + 0x1)
#define FRONT_GC08A3_CONVERTED_LSC_CHECKSUM_LEN (0x07C3 - 0x0068 + 0x1)

const struct rom_converted_cal_addr front_gc08a3_converted_cal_addr = {
	.rom_awb_cal_data_start_addr            = 0x0058,
	.rom_awb_checksum_addr                  = 0x0064,
	.rom_awb_checksum_len                   = (FRONT_GC08A3_CONVERTED_AWB_CHECKSUM_LEN),
	.rom_shading_cal_data_start_addr        = 0x0068,
	.rom_shading_checksum_addr              = 0x07C4,
	.rom_shading_checksum_len               = (FRONT_GC08A3_CONVERTED_LSC_CHECKSUM_LEN),
};

const struct imgsensor_vendor_rom_addr front_gc08a3_cal_addr = {
	/* Set '-1' if not used */
	.camera_module_es_version               = 'A',
	.cal_map_es_version                     = '1',
	.rom_max_cal_size                       = FRONT_GC08A3_MAX_CAL_SIZE,
	.rom_header_cal_data_start_addr         = 0x00,
	.rom_header_main_module_info_start_addr = 0x0A,
	.rom_header_cal_map_ver_start_addr      = 0x16,
	.rom_header_project_name_start_addr     = 0x1E,
	.rom_header_module_id_addr              = 0x2E,
	.rom_header_main_sensor_id_addr         = 0x38,

	.rom_header_sub_module_info_start_addr  = -1,
	.rom_header_sub_sensor_id_addr          = -1,

	.rom_header_main_header_start_addr      = -1,
	.rom_header_main_header_end_addr        = -1,
	.rom_header_main_oem_start_addr         = -1,
	.rom_header_main_oem_end_addr           = -1,
	.rom_header_main_awb_start_addr         = -1,//0x00,
	.rom_header_main_awb_end_addr           = -1,//0x04,
	.rom_header_main_shading_start_addr     = -1,
	.rom_header_main_shading_end_addr       = -1,
	.rom_header_main_sensor_cal_start_addr  = -1,
	.rom_header_main_sensor_cal_end_addr    = -1,
	.rom_header_dual_cal_start_addr         = -1,
	.rom_header_dual_cal_end_addr           = -1,
	.rom_header_pdaf_cal_start_addr         = -1,
	.rom_header_pdaf_cal_end_addr           = -1,
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

	.rom_header_checksum_addr               = 0x4A,
	.rom_header_checksum_len                = FRONT_GC08A3_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr           = -1,
	.rom_oem_af_macro_position_addr         = -1,
	.rom_oem_module_info_start_addr         = -1,
	.rom_oem_checksum_addr                  = -1,
	.rom_oem_checksum_len                   = FRONT_GC08A3_OEM_CHECKSUM_LEN,

	.rom_module_cal_data_start_addr         = 0x4E,
	.rom_module_module_info_start_addr      = -1,
	.rom_module_checksum_addr               = 0x45C,
	.rom_module_checksum_len                = FRONT_GC08A3_MODULE_CHECKSUM_LEN,

	.rom_awb_cal_data_start_addr            = 0x58,
	.rom_awb_module_info_start_addr         = -1,
	.rom_awb_checksum_addr                  = -1,
	.rom_awb_checksum_len                   = FRONT_GC08A3_AWB_CHECKSUM_LEN,

	.rom_shading_cal_data_start_addr        = 0x62,
	.rom_shading_module_info_start_addr     = -1,
	.rom_shading_checksum_addr              = -1,
	.rom_shading_checksum_len               = FRONT_GC08A3_SHADING_CHECKSUM_LEN,

	.rom_sensor_cal_module_info_start_addr  = -1,
	.rom_sensor_cal_checksum_addr           = -1,
	.rom_sensor_cal_checksum_len            = FRONT_GC08A3_SENSOR_CAL_CHECKSUM_LEN,

	.rom_dual_module_info_start_addr        = -1,
	.rom_dual_checksum_addr                 = -1,
	.rom_dual_checksum_len                  = FRONT_GC08A3_DUAL_CHECKSUM_LEN,

	.rom_pdaf_module_info_start_addr        = -1,
	.rom_pdaf_checksum_addr                 = -1,
	.rom_pdaf_checksum_len                  = FRONT_GC08A3_PDAF_CHECKSUM_LEN,
	.rom_ois_checksum_addr                  = -1,
	.rom_ois_checksum_len                   = -1,

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


	.crosstalk_cal_addr                     = NULL,
	.sac_cal_addr                           = NULL,
	.extend_cal_addr                        = NULL,

	.converted_cal_addr                     = &front_gc08a3_converted_cal_addr,
	.rom_converted_max_cal_size             = FRONT_GC08A3_CONVERTED_MAX_CAL_SIZE,

	.sensor_maker                           = "GALAXYCORE",
	.sensor_name                            = "GC08A3",
	.sub_sensor_maker                       = NULL,
	.sub_sensor_name                        = NULL,

	.bayerformat                            = -1,
};
