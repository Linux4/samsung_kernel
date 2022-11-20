#ifndef IS_EEPROM_MACRO_GC02M1_V001_H
#define IS_EEPROM_MACRO_GC02M1_V001_H

/* Reference File
 * 20200831_A12_M12s_CAM3((Macro_2M_GC02M1)_EEPROM_Rear_Cal map V008.001.xlsx
 */

#define IS_REAR4_MAX_CAL_SIZE          (0x1B9F - 0x0000 + 0x1)//Including space for standard cal 

#define MACRO_HEADER_CHECKSUM_LEN          (0x00FB - 0x0000 + 0x1)
#define MACRO_OEM_CHECKSUM_LEN             (0x019B - 0x0100 + 0x1)
#define MACRO_AWB_CHECKSUM_LEN             (0x01AB - 0x01A0 + 0x1)
#define MACRO_SHADING_CHECKSUM_LEN         (0x05AB - 0x01B0 + 0x1)
#define MACRO_AWB_SEC2LSI_CHECKSUM_LEN     (0x01AB - 0x01A0 + 0x1)
#define MACRO_SHADING_SEC2LSI_CHECKSUM_LEN (0x1B9B - 0x01B0 + 0x1)

struct rom_standard_cal_data macro_gc02m1_standard_cal_info = {
	.rom_standard_cal_start_addr                               = -1,
	.rom_standard_cal_end_addr                                 = -1,
	.rom_standard_cal_module_crc_addr                          = -1,
	.rom_standard_cal_module_checksum_len                      = -1,
	.rom_header_standard_cal_end_addr                          = -1,
	.rom_standard_cal_sec2lsi_end_addr                         = -1,

	// reference data only
	.rom_awb_start_addr                                        = 0x01A0,
	.rom_awb_end_addr                                          = 0x01A7,
	.rom_shading_start_addr                                    = 0x01B0,
	.rom_shading_end_addr                                      = 0x05A8,

	.rom_awb_sec2lsi_start_addr                                = 0x01A0,
	.rom_awb_sec2lsi_end_addr                                  = 0x01A7,
	.rom_awb_sec2lsi_checksum_addr                             = 0x01AC,
	.rom_awb_sec2lsi_checksum_len                              = MACRO_AWB_SEC2LSI_CHECKSUM_LEN,

	.rom_shading_sec2lsi_start_addr                            = 0x01B0,
	.rom_shading_sec2lsi_end_addr                              = 0x1B97,
	.rom_shading_sec2lsi_checksum_addr                         = 0x1B9C,
	.rom_shading_sec2lsi_checksum_len                          = MACRO_SHADING_SEC2LSI_CHECKSUM_LEN,
};

const struct rom_extend_cal_addr macro_gc02m1_extend_cal_addr = {
	.name = EXTEND_STANDARD_CAL,
	.data = &macro_gc02m1_standard_cal_info,
	.next = NULL,
};

const struct is_vender_rom_addr macro_gc02m1_cal_addr = {
	/* Set '-1' if not used */

	.camera_module_es_version                  = 'A',
	.cal_map_es_version                        = '1',
	.rom_max_cal_size                          = IS_REAR4_MAX_CAL_SIZE,

	.rom_header_cal_data_start_addr            = 0x00,
	.rom_header_main_module_info_start_addr    = 0x5E,
	.rom_header_cal_map_ver_start_addr         = 0x90,
	.rom_header_project_name_start_addr        = 0x98,
	.rom_header_module_id_addr                 = 0xAE,
	.rom_header_main_sensor_id_addr            = 0xB8,

	.rom_header_sub_module_info_start_addr     = -1,
	.rom_header_sub_sensor_id_addr             = -1,

	.rom_header_main_header_start_addr         = 0x00,
	.rom_header_main_header_end_addr           = 0x04,
	.rom_header_main_oem_start_addr            = 0x08,  /* AF start address */
	.rom_header_main_oem_end_addr              = 0x0C,  /* AF end address */
	.rom_header_main_awb_start_addr            = 0x10,
	.rom_header_main_awb_end_addr              = 0x14,
	.rom_header_main_shading_start_addr        = 0x18,
	.rom_header_main_shading_end_addr          = 0x1C,
	.rom_header_main_sensor_cal_start_addr     = -1,
	.rom_header_main_sensor_cal_end_addr       = -1,
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

	.rom_header_main_mtf_data_addr             = 0x0134,
	.rom_header_sub_mtf_data_addr              = -1,

	.rom_header_checksum_addr                  = 0xFC,
	.rom_header_checksum_len                   = MACRO_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr              = -1,
	.rom_oem_af_macro_position_addr            = -1,
	.rom_oem_module_info_start_addr            = -1,
	.rom_oem_checksum_addr                     = 0x019C,
	.rom_oem_checksum_len                      = MACRO_OEM_CHECKSUM_LEN, /* AF checksum length */

	.rom_awb_module_info_start_addr            = -1,
	.rom_awb_checksum_addr                     = 0x01AC,
	.rom_awb_checksum_len                      = MACRO_AWB_CHECKSUM_LEN,

	.rom_shading_module_info_start_addr        = -1,
	.rom_shading_checksum_addr                 = 0x05AC,
	.rom_shading_checksum_len                  = MACRO_SHADING_CHECKSUM_LEN,

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

	.extend_cal_addr                           = &macro_gc02m1_extend_cal_addr,
};

#endif /* IS_EEPROM_MACRO_GC02M1_V001_H */
