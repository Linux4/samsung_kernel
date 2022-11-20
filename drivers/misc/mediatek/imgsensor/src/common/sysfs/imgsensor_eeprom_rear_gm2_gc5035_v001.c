#include "imgsensor_eeprom_rear_gm2_gc5035_v001.h"

#define IMGSENSOR_REAR_MAX_CAL_SIZE    (16 * 1024)
#define IMGSENSOR_REAR2_MAX_CAL_SIZE   (16 * 1024)

#define REAR_HEADER_CHECKSUM_LEN     (0x00FB - 0x0000 + 0x1)
#define REAR_OEM_CHECKSUM_LEN        (0x01BB - 0x0100 + 0x1)
#define REAR_AWB_CHECKSUM_LEN        (0x025B - 0x01C0 + 0x1)
#define REAR_SHADING_CHECKSUM_LEN    (0x09CB - 0x0260 + 0x1)
#define REAR_SENSOR_CAL_CHECKSUM_LEN (0x2B4B - 0x09D0 + 0x1)
#define REAR_DUAL_CHECKSUM_LEN       (0x33EB - 0x2BA0 + 0x1)
#define REAR_PDAF_CHECKSUM_LEN       (0x3C1B - 0x33F0 + 0x1)
#define DUAL_CAL_MODEL_ID_LEN        (0x2D5F - 0x2D4C + 0x1)
/* Crosstalk cal data for SW remosaic */
#define REAR_XTALK_CAL_DATA_SIZE     (8482)

const struct rom_cal_addr rear_gm2_ggc_addr = {
	.name       = CROSSTALK_CAL_GGC,
	.addr       = 0x2A96,
	.size       = (0x2AF1 - 0x2A96 + 1),
	.next       = NULL,
};

const struct rom_cal_addr rear_gm2_xtc_addr = {
	.name       = CROSSTALK_CAL_XTC,
	.addr       = 0x09D0,
	.size       = (0x2A95 - 0x09D0 + 1),
	.next       = &rear_gm2_ggc_addr,
};

const struct rom_sac_cal_addr rear_gm2_sac_addr = {
	.rom_mode_addr      = 0x0158, //Control, Mode
	.rom_time_addr      = 0x0159, //Resonance
};

const struct imgsensor_vendor_rom_addr rear_gm2_gc5035_cal_addr = {
	/* Set '-1' if not used */
	.camera_module_es_version               = 'A',
	.cal_map_es_version                     = '1',
	.rom_max_cal_size                       = IMGSENSOR_REAR_MAX_CAL_SIZE,
	.rom_header_cal_data_start_addr         = 0x00,
	.rom_header_main_module_info_start_addr = 0x5E,
	.rom_header_cal_map_ver_start_addr      = 0x90,
	.rom_header_project_name_start_addr     = 0x98,
	.rom_header_module_id_addr              = 0xAE,
	.rom_header_main_sensor_id_addr         = 0xB8,

	.rom_header_sub_module_info_start_addr  = 0x77,
	.rom_header_sub_sensor_id_addr          = 0xC8,

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
	.rom_header_checksum_len                = REAR_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr           = 0x0128,
	.rom_oem_af_macro_position_addr         = 0x0124,
	.rom_oem_module_info_start_addr         = 0x01A7,
	.rom_oem_checksum_addr                  = 0x01BC,
	.rom_oem_checksum_len                   = REAR_OEM_CHECKSUM_LEN,

	.rom_awb_module_info_start_addr         = 0x024A,
	.rom_awb_checksum_addr                  = 0x025C,
	.rom_awb_checksum_len                   = REAR_AWB_CHECKSUM_LEN,

	.rom_shading_module_info_start_addr     = 0x09B2,
	.rom_shading_checksum_addr              = 0x09CC,
	.rom_shading_checksum_len               = REAR_SHADING_CHECKSUM_LEN,

	.rom_sensor_cal_module_info_start_addr  = 0x2B30,
	.rom_sensor_cal_checksum_addr           = 0x2B4C,
	.rom_sensor_cal_checksum_len            = REAR_SENSOR_CAL_CHECKSUM_LEN,

	.rom_dual_module_info_start_addr        = 0x33DC,
	.rom_dual_checksum_addr                 = 0x33EC,
	.rom_dual_checksum_len                  = REAR_DUAL_CHECKSUM_LEN,

	.rom_pdaf_module_info_start_addr        = 0x3C00,
	.rom_pdaf_checksum_addr                 = 0x3C1C,
	.rom_pdaf_checksum_len                  = REAR_PDAF_CHECKSUM_LEN,

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

	.rom_dual_cal_data2_start_addr          = 0x2BA4,
	.rom_dual_cal_data2_size                = (512),
	.rom_dual_tilt_x_addr                   = 0x2D38,
	.rom_dual_tilt_y_addr                   = 0x2D3C,
	.rom_dual_tilt_z_addr                   = 0x2D40,
	.rom_dual_tilt_sx_addr                  = 0x2D44,
	.rom_dual_tilt_sy_addr                  = 0x2D48,
	.rom_dual_tilt_range_addr               = 0x2F84,
	.rom_dual_tilt_max_err_addr             = 0x2F88,
	.rom_dual_tilt_avg_err_addr             = 0x2F8C,
	.rom_dual_tilt_dll_version_addr         = 0x2BA0,
	.rom_dual_tilt_dll_modelID_addr         = 0x2D4C,
	.rom_dual_tilt_dll_modelID_size         = DUAL_CAL_MODEL_ID_LEN,
	.rom_dual_shift_x_addr                  = -1,
	.rom_dual_shift_y_addr                  = -1,

	.crosstalk_cal_addr                     = &rear_gm2_xtc_addr,
	.sac_cal_addr                           = &rear_gm2_sac_addr,
	.extend_cal_addr                        = NULL,
	.converted_cal_addr                     = NULL,
	.rom_converted_max_cal_size             = IMGSENSOR_REAR_MAX_CAL_SIZE,

	.sensor_maker                           = "SLSI",
	.sensor_name                            = "GM2",
	.sub_sensor_maker                       = "GALAXYCORE",
	.sub_sensor_name                        = "GC5035",
};