#include "imgsensor_eeprom_rear_gm2_v007.h"

#define REAR_GM2_MAX_CAL_SIZE            (0x3E3F - 0x0000 + 0x1)
#define REAR_GM2_HEADER_CHECKSUM_LEN     (0x00FB - 0x0000 + 0x1)
#define REAR_GM2_OEM_CHECKSUM_LEN        (0x01BB - 0x0100 + 0x1)
#define REAR_GM2_AWB_CHECKSUM_LEN        (0x025B - 0x01C0 + 0x1)
#define REAR_GM2_SHADING_CHECKSUM_LEN    (0x09CB - 0x0260 + 0x1)
#define REAR_GM2_SENSOR_CAL_CHECKSUM_LEN (0x2B4B - 0x09D0 + 0x1)
#define REAR_GM2_DUAL_CHECKSUM_LEN       (0x33EB - 0x2BA0 + 0x1)
#define REAR_GM2_PDAF_CHECKSUM_LEN       (0x3C1B - 0x33F0 + 0x1)
#define DUAL_CAL_MODEL_ID_LEN            (0x2D5F - 0x2D4C + 0x1)

const struct imgsensor_vendor_rom_addr rear_gm2_cal_addr = {
	/* Set '-1' if not used */
	.camera_module_es_version               = 'A',
	.cal_map_es_version                     = '1',
	.rom_max_cal_size                       = REAR_GM2_MAX_CAL_SIZE,
	.rom_header_cal_data_start_addr         = 0x00,
	.rom_header_main_module_info_start_addr = 0x5E,
	.rom_header_cal_map_ver_start_addr      = 0x90,
	.rom_header_project_name_start_addr     = 0x98,
	.rom_header_module_id_addr              = 0xAE,
	.rom_header_main_sensor_id_addr         = 0xB8,

	.rom_header_sub_module_info_start_addr  = -1,
	.rom_header_sub_sensor_id_addr          = -1,

	.rom_header_main_header_start_addr      = 0x00,
	.rom_header_main_header_end_addr        = 0x04,
	.rom_header_main_oem_start_addr         = 0x08,
	.rom_header_main_oem_end_addr           = 0x0C,
	.rom_header_main_awb_start_addr         = 0x10,
	.rom_header_main_awb_end_addr           = 0x14,
	.rom_header_main_shading_start_addr     = 0x18,
	.rom_header_main_shading_end_addr       = 0x1C,
	.rom_header_main_sensor_cal_start_addr  = 0x20,
	.rom_header_main_sensor_cal_end_addr    = 0x24,
	.rom_header_dual_cal_start_addr         = 0x30,
	.rom_header_dual_cal_end_addr           = 0x34,
	.rom_header_pdaf_cal_start_addr         = 0x38,
	.rom_header_pdaf_cal_end_addr           = 0x3C,

	.rom_header_sub_oem_start_addr          = -1,
	.rom_header_sub_oem_end_addr            = -1,
	.rom_header_sub_awb_start_addr          = -1,
	.rom_header_sub_awb_end_addr            = -1,
	.rom_header_sub_shading_start_addr      = -1,
	.rom_header_sub_shading_end_addr        = -1,

	.rom_header_main_mtf_data_addr          = 0x015C,
	.rom_header_sub_mtf_data_addr           = -1,

	.rom_header_checksum_addr               = 0xFC,
	.rom_header_checksum_len                = REAR_GM2_HEADER_CHECKSUM_LEN,

	.rom_module_cal_data_start_addr         = -1,
	.rom_module_module_info_start_addr      = -1,
	.rom_module_checksum_addr               = -1,
	.rom_module_checksum_len                = -1,

	.rom_oem_af_inf_position_addr           = 0x0128,
	.rom_oem_af_macro_position_addr         = 0x0124,
	.rom_oem_module_info_start_addr         = 0x01A7,
	.rom_oem_checksum_addr                  = 0x01BC,
	.rom_oem_checksum_len                   = REAR_GM2_OEM_CHECKSUM_LEN,

	.rom_awb_cal_data_start_addr			= 0x01C0,
	.rom_awb_module_info_start_addr         = 0x024A,
	.rom_awb_checksum_addr                  = 0x025C,
	.rom_awb_checksum_len                   = REAR_GM2_AWB_CHECKSUM_LEN,

	.rom_shading_cal_data_start_addr		= 0x0260,
	.rom_shading_module_info_start_addr     = 0x09B2,
	.rom_shading_checksum_addr              = 0x09CC,
	.rom_shading_checksum_len               = REAR_GM2_SHADING_CHECKSUM_LEN,

	.rom_sensor_cal_module_info_start_addr  = 0x09D0,
	.rom_sensor_cal_checksum_addr           = 0x2B4C,
	.rom_sensor_cal_checksum_len            = REAR_GM2_SENSOR_CAL_CHECKSUM_LEN,

	.rom_dual_module_info_start_addr        = 0x2BA0,
	.rom_dual_checksum_addr                 = 0x33EC,
	.rom_dual_checksum_len                  = REAR_GM2_DUAL_CHECKSUM_LEN,

	.rom_pdaf_module_info_start_addr        = 0x33F0,
	.rom_pdaf_checksum_addr                 = 0x3C1C,
	.rom_pdaf_checksum_len                  = REAR_GM2_PDAF_CHECKSUM_LEN,

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

// Fix me: dual cal data
	.rom_dual_cal_data2_start_addr          = 0x2BA0,
	.rom_dual_cal_data2_size                = (2060),
	.rom_dual_tilt_x_addr                   = 0x2E99,
	.rom_dual_tilt_y_addr                   = 0x2E9B,
	.rom_dual_tilt_z_addr                   = 0x2E9D,
	.rom_dual_tilt_sx_addr                  = 0x2E9E,
	.rom_dual_tilt_sy_addr                  = 0x2EA0,
	.rom_dual_tilt_range_addr               = 0x2EA4,
	.rom_dual_tilt_max_err_addr             = 0x2EA5,
	.rom_dual_tilt_avg_err_addr             = 0x2EA6,
	.rom_dual_tilt_dll_version_addr         = 0x2FB1,
	.rom_dual_tilt_dll_modelID_addr         = 0x2EA2,
	.rom_dual_tilt_dll_modelID_size         = 1,
	.rom_dual_shift_x_addr                  = 0x3157,
	.rom_dual_shift_y_addr                  = 0x322A,

	.extend_cal_addr                        = NULL,

	.converted_cal_addr 					= NULL,
	.rom_converted_max_cal_size 			= REAR_GM2_MAX_CAL_SIZE,

	.sensor_maker                           = "SLSI",
	.sensor_name                            = "GM2",
	.sub_sensor_maker                       = NULL,
	.sub_sensor_name                        = NULL,

	.bayerformat							= -1,
};
