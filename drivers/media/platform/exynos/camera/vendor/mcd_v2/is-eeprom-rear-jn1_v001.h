#ifndef IS_EEPROM_REAR_JN1_V001_H
#define IS_EEPROM_REAR_JN1_V001_H

/* Reference File
 * 20210621_A13 5G_CAM1(Wide_50M_JN1)_EEPROM_Rear_Cal map V008.001_QC_LSI_MTK_Map.xlsx
 */

#define IS_REAR_MAX_CAL_SIZE                 (0x5320 - 0x0000 + 0x1)

#define REAR_HEADER_CHECKSUM_LEN             (0x00FB - 0x0000 + 0x1)
#define REAR_AWB_CHECKSUM_LEN                (0x36FB - 0x36E0 + 0x1)
#define REAR_SHADING_CHECKSUM_LEN            (0x3AFB - 0x3700 + 0x1)
#define REAR_PDAF_CHECKSUM_LEN               (0x2E7B - 0x26C0 + 0x1)

#define REAR_AWB_SEC2LSI_CHECKSUM_LEN        (0x36EB - 0x36E0 + 0x1)
#define REAR_SHADING_SEC2LSI_CHECKSUM_LEN    (0x50DB - 0x36F0 + 0x1)
/* Crosstalk cal data for SW remosaic */
#define REAR_XTALK_CAL_DATA_SIZE     (8364)
#define REAR_REMOSAIC_TETRA_XTC_START_ADDR	0x14AC
#define REAR_REMOSAIC_TETRA_XTC_SIZE		2612
#define REAR_REMOSAIC_SENSOR_XTC_START_ADDR	0x1EE0
#define REAR_REMOSAIC_SENSOR_XTC_SIZE		768
#define REAR_REMOSAIC_PDXTC_START_ADDR		0x0140
#define REAR_REMOSAIC_PDXTC_SIZE		4000
#define REAR_REMOSAIC_SW_GGC_START_ADDR		0x10E0
#define REAR_REMOSAIC_SW_GGC_SIZE		626

struct rom_standard_cal_data rear_jn1_standard_cal_info = {
	.rom_standard_cal_start_addr                               = -1,
	.rom_standard_cal_end_addr                                 = -1,
	.rom_standard_cal_module_crc_addr                          = -1,
	.rom_standard_cal_module_checksum_len                      = -1,
	.rom_header_standard_cal_end_addr                          = -1,
	.rom_standard_cal_sec2lsi_end_addr                         = -1,

	// reference data only
	.rom_awb_start_addr                                        = 0x36E0,
	.rom_awb_end_addr                                          = 0x36E7,
	.rom_shading_start_addr                                    = 0x3700,
	.rom_shading_end_addr                                      = 0x3AF8,

	.rom_awb_sec2lsi_start_addr                                = 0x36E0,
	.rom_awb_sec2lsi_end_addr                                  = 0x36E7,
	.rom_awb_sec2lsi_checksum_addr                             = 0x36EC,
	.rom_awb_sec2lsi_checksum_len                              = REAR_AWB_SEC2LSI_CHECKSUM_LEN,

	.rom_shading_sec2lsi_start_addr                            = 0x36F0,
	.rom_shading_sec2lsi_end_addr                              = 0x50D7,
	.rom_shading_sec2lsi_checksum_addr                         = 0x50DC,
	.rom_shading_sec2lsi_checksum_len                          = REAR_SHADING_SEC2LSI_CHECKSUM_LEN,
};

const struct rom_extend_cal_addr rear_jn1_extend_cal_addr = {
	.name = EXTEND_STANDARD_CAL,
	.data = &rear_jn1_standard_cal_info,
	.next = NULL,
};

const struct is_vender_rom_addr rear_jn1_cal_addr = {
	/* Set '-1' if not used */

	.camera_module_es_version                  = 'A',
	.cal_map_es_version                        = '1',
	.rom_max_cal_size                          = IS_REAR_MAX_CAL_SIZE,

	.rom_header_cal_data_start_addr            = 0x00,
	.rom_header_main_module_info_start_addr    = 0x6E,
	.rom_header_cal_map_ver_start_addr         = 0x90,
	.rom_header_project_name_start_addr        = 0x98,
	.rom_header_module_id_addr                 = 0xAE,
	.rom_header_main_sensor_id_addr            = 0xB8,

	.rom_header_sub_module_info_start_addr     = -1,
	.rom_header_sub_sensor_id_addr             = -1,

	.rom_header_main_header_start_addr         = 0x00,
	.rom_header_main_header_end_addr           = 0x04,
	.rom_header_main_oem_start_addr            = -1,
	.rom_header_main_oem_end_addr              = -1,
	.rom_header_main_awb_start_addr            = 0x48,
	.rom_header_main_awb_end_addr              = 0x4C,
	.rom_header_main_shading_start_addr        = 0x50,
	.rom_header_main_shading_end_addr          = 0x54,
	.rom_header_main_sensor_cal_start_addr     = 0x10,
	.rom_header_main_sensor_cal_end_addr       = 0x14,
	.rom_header_dual_cal_start_addr            = -1,
	.rom_header_dual_cal_end_addr              = -1,
	.rom_header_pdaf_cal_start_addr            = -1,
	.rom_header_pdaf_cal_end_addr              = -1,

	.rom_header_sub_oem_start_addr             = -1,
	.rom_header_sub_oem_end_addr               = -1,
	.rom_header_sub_awb_start_addr             = -1,
	.rom_header_sub_awb_end_addr               = -1,
	.rom_header_sub_shading_start_addr         = -1,
	.rom_header_sub_shading_end_addr           = -1,

	.rom_header_main_mtf_data_addr             = -1,
	.rom_header_sub_mtf_data_addr              = -1,

	.rom_header_checksum_addr                  = 0xFC,
	.rom_header_checksum_len                   = REAR_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr              = 0x2670,
	.rom_oem_af_macro_position_addr            = 0x267C,
	.rom_oem_module_info_start_addr            = -1,
	.rom_oem_checksum_addr                     = -1,
	.rom_oem_checksum_len                      = -1,

	.rom_awb_module_info_start_addr            = -1,
	.rom_awb_checksum_addr                     = 0x36FC,
	.rom_awb_checksum_len                      = REAR_AWB_CHECKSUM_LEN,

	.rom_shading_module_info_start_addr        = -1,
	.rom_shading_checksum_addr                 = 0x3AFC,
	.rom_shading_checksum_len                  = REAR_SHADING_CHECKSUM_LEN,

	.rom_sensor_cal_module_info_start_addr     = -1,
	.rom_sensor_cal_checksum_addr              = -1,
	.rom_sensor_cal_checksum_len               = -1,

	.rom_dual_module_info_start_addr           = -1,
	.rom_dual_checksum_addr                    = -1,
	.rom_dual_checksum_len                     = -1,

	.rom_pdaf_module_info_start_addr           = -1,
	.rom_pdaf_checksum_addr                    = -1,
	.rom_pdaf_checksum_len                     = -1,

	.rom_sub_oem_af_inf_position_addr          = -1,
	.rom_sub_oem_af_macro_position_addr        = -1,
	.rom_sub_oem_module_info_start_addr        = -1,
	.rom_sub_oem_checksum_addr                 = -1,
	.rom_sub_oem_checksum_len                  = -1,

	.rom_sub_awb_module_info_start_addr        = -1,
	.rom_sub_awb_checksum_addr                 = -1,
	.rom_sub_awb_checksum_len                  = -1,

	.rom_sub_shading_module_info_start_addr    = -1,
	.rom_sub_shading_checksum_addr             = -1,
	.rom_sub_shading_checksum_len              = -1,

	.rom_dual_cal_data2_start_addr             = -1,
	.rom_dual_cal_data2_size                   = -1,
	.rom_dual_tilt_x_addr                      = -1,
	.rom_dual_tilt_y_addr                      = -1,
	.rom_dual_tilt_z_addr                      = -1,
	.rom_dual_tilt_sx_addr                     = -1,
	.rom_dual_tilt_sy_addr                     = -1,
	.rom_dual_tilt_range_addr                  = -1,
	.rom_dual_tilt_max_err_addr                = -1,
	.rom_dual_tilt_avg_err_addr                = -1,
	.rom_dual_tilt_dll_version_addr            = -1,
	.rom_dual_tilt_dll_model_id_addr           = -1,
	.rom_dual_tilt_dll_model_id_size           = -1,
	.rom_dual_shift_x_addr                     = -1,
	.rom_dual_shift_y_addr                     = -1,

	.extend_cal_addr                           = &rear_jn1_extend_cal_addr,
};

#endif /* IS_EEPROM_REAR_JN1_V001_H */
