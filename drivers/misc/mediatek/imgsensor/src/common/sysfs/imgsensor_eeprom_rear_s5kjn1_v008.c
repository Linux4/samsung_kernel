#include "imgsensor_eeprom_rear_s5kjn1_v008.h"

#define REAR_S5KJN1_MAX_CAL_SIZE               (0x3D39 + 0x1)

#define REAR_S5KJN1_HEADER_CHECKSUM_LEN        (0x00FB - 0x0000 + 0x1)
#define REAR_S5KJN1_OEM_CHECKSUM_LEN           (0x2ECB - 0x2E80 + 0x1)
#define REAR_S5KJN1_AWB_CHECKSUM_LEN           (0x36FB - 0x36E0 + 0x1)
#define REAR_S5KJN1_SHADING_CHECKSUM_LEN       (0x3AFB - 0x3700 + 0x1)
#define REAR_S5KJN1_SENSOR_CAL_CHECKSUM_LEN    (0x21EB - 0x0140 + 0x1)
#define REAR_S5KJN1_DUAL_CHECKSUM_LEN          -1
#define REAR_S5KJN1_PDAF_CHECKSUM_LEN          (0x36DB - 0x2ED0 + 0x1)

#define REAR_S5KJN1_CONVERTED_MAX_CAL_SIZE     (0x4089 + 0x1)
#define REAR_S5KJN1_CONVERTED_AWB_CHECKSUM_LEN (0x36EB - 0x36E0 + 0x1)
#define REAR_S5KJN1_CONVERTED_LSC_CHECKSUM_LEN (0x3E4B - 0x36F0 + 0x1)

const struct rom_converted_cal_addr rear_s5kjn1_converted_cal_addr = {
	.rom_awb_cal_data_start_addr            = 0x36E0,
	.rom_awb_checksum_addr                  = 0x36EC,
	.rom_awb_checksum_len                   = (REAR_S5KJN1_CONVERTED_AWB_CHECKSUM_LEN),
	.rom_shading_cal_data_start_addr        = 0x36F0,
	.rom_shading_checksum_addr              = 0x3E4C,
	.rom_shading_checksum_len               = (REAR_S5KJN1_CONVERTED_LSC_CHECKSUM_LEN),
};

const struct rom_cal_addr rear_s5kjn1_hwggc_addr = {
	.name                            = CROSSTALK_CAL_HW_GGC,
	.addr                            = 0x1352,
	.size                            = (0x14AB - 0x1352 + 1),
	.next                            = NULL,
};

const struct rom_cal_addr rear_s5kjn1_swggc_addr = {
	.name                            = CROSSTALK_CAL_SW_GGC,
	.addr                            = 0x10E0,
	.size                            = (0x1351 - 0x10E0 + 1),
	.next                            = &rear_s5kjn1_hwggc_addr,
};

const struct rom_cal_addr rear_s5kjn1_pdxtc_addr = {
	.name                            = CROSSTALK_CAL_PD_XTC,
	.addr                            = 0x0140,
	.size                            = (0x10DF - 0x0140 + 1),
	.next                            = &rear_s5kjn1_swggc_addr,
};

const struct rom_cal_addr rear_s5kjn1_sensorxtc_addr = {
	.name                            = CROSSTALK_CAL_SENSOR_XTC,
	.addr                            = 0x1EE0,
	.size                            = (0x21DF - 0x1EE0 + 1),
	.next                            = &rear_s5kjn1_pdxtc_addr,
};

const struct rom_cal_addr rear_s5kjn1_tetraxtc_addr = {
	.name                            = CROSSTALK_CAL_TETRA_XTC,
	.addr                            = 0x14AC,
	.size                            = (0x1EDF - 0x14AC + 1),
	.next                            = &rear_s5kjn1_sensorxtc_addr,
};

const struct rom_sac_cal_addr rear_s5kjn1_sac_addr = {
	.rom_mode_addr = 0x2EB4,
	.rom_time_addr = 0x2EB5,
};

const struct imgsensor_vendor_rom_addr rear_s5kjn1_cal_addr = {
	/* Set '-1' if not used */
	.camera_module_es_version               = 'A',
	.cal_map_es_version                     = '1',
	.rom_max_cal_size                       = REAR_S5KJN1_MAX_CAL_SIZE,
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
	.rom_header_ois_cal_start_addr          = -1,
	.rom_header_ois_cal_end_addr            = -1,

	.rom_header_sub_oem_start_addr          = -1,
	.rom_header_sub_oem_end_addr            = -1,
	.rom_header_sub_awb_start_addr          = -1,
	.rom_header_sub_awb_end_addr            = -1,
	.rom_header_sub_shading_start_addr      = -1,
	.rom_header_sub_shading_end_addr        = -1,

	.rom_header_main_mtf_data_addr          = 0x3B02,
	.rom_header_sub_mtf_data_addr           = -1,

	.rom_header_checksum_addr               = 0x00FC,
	.rom_header_checksum_len                = REAR_S5KJN1_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr           = 0x2E8C,
	.rom_oem_af_macro_position_addr         = 0x2E98,
	.rom_oem_module_info_start_addr         = -1,
	.rom_oem_checksum_addr                  = 0x2ECC,
	.rom_oem_checksum_len                   = REAR_S5KJN1_OEM_CHECKSUM_LEN,

	.rom_module_cal_data_start_addr         = -1,
	.rom_module_module_info_start_addr      = -1,
	.rom_module_checksum_addr               = -1,
	.rom_module_checksum_len                = -1,

	.rom_awb_cal_data_start_addr            = 0x36E0,
	.rom_awb_module_info_start_addr         = -1,
	.rom_awb_checksum_addr                  = 0x36FC,
	.rom_awb_checksum_len                   = REAR_S5KJN1_AWB_CHECKSUM_LEN,

	.rom_shading_cal_data_start_addr        = 0x3700,
	.rom_shading_module_info_start_addr     = -1,
	.rom_shading_checksum_addr              = 0x3AFC,
	.rom_shading_checksum_len               = REAR_S5KJN1_SHADING_CHECKSUM_LEN,

	.rom_sensor_cal_module_info_start_addr  = -1,
	.rom_sensor_cal_checksum_addr           = 0x21EC,
	.rom_sensor_cal_checksum_len            = REAR_S5KJN1_SENSOR_CAL_CHECKSUM_LEN,

	.rom_dual_module_info_start_addr        = -1,
	.rom_dual_checksum_addr                 = -1,
	.rom_dual_checksum_len                  = REAR_S5KJN1_DUAL_CHECKSUM_LEN,

	.rom_pdaf_module_info_start_addr        = -1,
	.rom_pdaf_checksum_addr                 = 0x36DC,
	.rom_pdaf_checksum_len                  = REAR_S5KJN1_PDAF_CHECKSUM_LEN,
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


	.crosstalk_cal_addr                     = &rear_s5kjn1_tetraxtc_addr,
	.sac_cal_addr                           = &rear_s5kjn1_sac_addr,
	.extend_cal_addr                        = NULL,

	.converted_cal_addr                     = &rear_s5kjn1_converted_cal_addr,
	.rom_converted_max_cal_size             = REAR_S5KJN1_CONVERTED_MAX_CAL_SIZE,

	.sensor_maker                           = "SLSI",
	.sensor_name                            = "S5KJN1",
	.sub_sensor_maker                       = NULL,
	.sub_sensor_name                        = NULL,

	.bayerformat                            = -1,
};
