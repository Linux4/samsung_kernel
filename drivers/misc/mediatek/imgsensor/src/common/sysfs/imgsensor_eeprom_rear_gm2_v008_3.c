#include "imgsensor_eeprom_rear_gm2_v008_3.h"

#define IMGSENSOR_REAR_MAX_CAL_SIZE    (16 * 1024)

#define REAR_HEADER_CHECKSUM_LEN     (0x00FB - 0x0000 + 0x1)
#define REAR_OEM_CHECKSUM_LEN        (0x2FCB - 0x2F50 + 0x1)
#define REAR_AWB_CHECKSUM_LEN        (0x37FB - 0x37E0 + 0x1)
#define REAR_SHADING_CHECKSUM_LEN    (0x3BFB - 0x3800 + 0x1)
#define REAR_SENSOR_CAL_CHECKSUM_LEN (0x22AB - 0x0170 + 0x1)
#define REAR_PDAF_CHECKSUM_LEN       (0x37DB - 0x2FD0 + 0x1)
/* Crosstalk cal data for SW remosaic */
#define REAR_XTALK_CAL_DATA_SIZE     (8482)

#define REAR_CONVERTED_MAX_CAL_SIZE (0x416F + 0x1)
#define REAR_CONVERTED_AWB_CHECKSUM_LEN (0x37EB - 0x37E0 + 0x1)
#define REAR_CONVERTED_LSC_CHECKSUM_LEN (0x3F4B - 0x37F0 + 0x1)

const struct rom_converted_cal_addr rear_gm2_converted_cal_addr = {
	.rom_awb_cal_data_start_addr     = 0x37E0,
	.rom_awb_checksum_addr           = 0x37EC,
	.rom_awb_checksum_len            = (REAR_CONVERTED_AWB_CHECKSUM_LEN),
	.rom_shading_cal_data_start_addr = 0x37F0,
	.rom_shading_checksum_addr       = 0x3F4C,
	.rom_shading_checksum_len        = (REAR_CONVERTED_LSC_CHECKSUM_LEN),
};

const struct rom_cal_addr rear_gm2_ggc_addr = {
	.name                            = CROSSTALK_CAL_GGC,
	.addr                            = 0x2236,
	.size                            = (0x2291 - 0x2236 + 1),
	.next                            = NULL,
};

const struct rom_cal_addr rear_gm2_xtc_addr = {
	.name                            = CROSSTALK_CAL_XTC,
	.addr                            = 0x0170,
	.size                            = (0x2235 - 0x0170 + 1),
	.next                            = &rear_gm2_ggc_addr,
};


const struct rom_sac_cal_addr rear_gm2_sac_addr = {
	.rom_mode_addr                   = 0x2F84, //Control, Mode
	.rom_time_addr                   = 0x2F85, //Resonance
};

const struct imgsensor_vendor_rom_addr rear_gm2_cal_addr = {
	/* Set '-1' if not used */
	.camera_module_es_version               = 'A',
	.cal_map_es_version                     = '1',
	.rom_max_cal_size                       = IMGSENSOR_REAR_MAX_CAL_SIZE,
	.rom_header_cal_data_start_addr         = 0x00,
	.rom_header_main_module_info_start_addr = 0x6E,
	.rom_header_cal_map_ver_start_addr      = 0x90,
	.rom_header_project_name_start_addr     = 0x98,
	.rom_header_module_id_addr              = 0xAE,
	.rom_header_main_sensor_id_addr         = 0xB8,

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

	.rom_header_sub_oem_start_addr          = -1,
	.rom_header_sub_oem_end_addr            = -1,
	.rom_header_sub_awb_start_addr          = -1,
	.rom_header_sub_awb_end_addr            = -1,
	.rom_header_sub_shading_start_addr      = -1,
	.rom_header_sub_shading_end_addr        = -1,

	.rom_header_main_mtf_data_addr          = 0x2F88,
	.rom_header_sub_mtf_data_addr           = -1,

	.rom_header_checksum_addr               = 0x00FC,
	.rom_header_checksum_len                = REAR_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr           = 0x2F5C,
	.rom_oem_af_macro_position_addr         = 0x2F68,
	.rom_oem_module_info_start_addr         = -1,
	.rom_oem_checksum_addr                  = 0x2FCC,
	.rom_oem_checksum_len                   = REAR_OEM_CHECKSUM_LEN,

	.rom_module_cal_data_start_addr         = -1,
	.rom_module_module_info_start_addr      = -1,
	.rom_module_checksum_addr               = -1,
	.rom_module_checksum_len                = -1,

	.rom_awb_cal_data_start_addr            = 0x37E0,
	.rom_awb_module_info_start_addr         = -1,
	.rom_awb_checksum_addr                  = 0x37FC,
	.rom_awb_checksum_len                   = REAR_AWB_CHECKSUM_LEN,

	.rom_shading_cal_data_start_addr        = 0x3800,
	.rom_shading_module_info_start_addr     = -1,
	.rom_shading_checksum_addr              = 0x3BFC,
	.rom_shading_checksum_len               = REAR_SHADING_CHECKSUM_LEN,

	.rom_sensor_cal_module_info_start_addr  = -1,
	.rom_sensor_cal_checksum_addr           = 0x22AC,
	.rom_sensor_cal_checksum_len            = REAR_SENSOR_CAL_CHECKSUM_LEN,

	.rom_dual_module_info_start_addr        = -1,
	.rom_dual_checksum_addr                 = -1,
	.rom_dual_checksum_len                  = -1,

	.rom_pdaf_module_info_start_addr        = -1,
	.rom_pdaf_checksum_addr                 = 0x37DC,
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

	.crosstalk_cal_addr                     = &rear_gm2_xtc_addr,
	.sac_cal_addr                           = &rear_gm2_sac_addr,
	.converted_cal_addr                     = &rear_gm2_converted_cal_addr,
	.rom_converted_max_cal_size             = REAR_CONVERTED_MAX_CAL_SIZE,

	.sensor_maker                           = "SLSI",
	.sensor_name                            = "GM2",
	.sub_sensor_maker                       = NULL,
	.sub_sensor_name                        = NULL,

	.bayerformat                            = -1,
};
