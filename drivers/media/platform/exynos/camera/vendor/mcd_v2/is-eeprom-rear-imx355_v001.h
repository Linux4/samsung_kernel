#ifndef IS_EEPROM_REAR_IMX355_V001_H
#define IS_EEPROM_REAR_IMX355_V001_H

/* Reference File
 * 20210224_A02Core_CAM1(Wide_8M_IMX355)_EEPROM_Rear_Cal map V008.001_QC_LSI_MTK_ê³µìš©_Map.xlsx
 */
 
#define IS_REAR_MAX_CAL_SIZE (0x1E6F - 0x0000 + 0x1)

#define REAR_HEADER_CHECKSUM_LEN     (0x00FB - 0x0000 + 0x1)
#define REAR_OEM_CHECKSUM_LEN        (0x01EB - 0x0170 + 0x1)
#define REAR_STANDARD_CAL_CHECKSUM_LEN (0x068B - 0x0270 + 1)
#define REAR_AWB_SEC2LSI_CHECKSUM_LEN        (0x027B - 0x0270 + 0x1)
#define REAR_SHADING_SEC2LSI_CHECKSUM_LEN    (0x1C6B - 0x0280 + 0x1)
#define REAR_SENSOR_CAL_CHECKSUM_LEN (0x1E6F - 0x1C70 + 0x1)

struct rom_standard_cal_data wide_imx355_standard_cal_info = {
	.rom_standard_cal_start_addr                               = -1,
	.rom_standard_cal_end_addr                                 = -1,
	.rom_standard_cal_module_crc_addr                          = -1,
	.rom_standard_cal_module_checksum_len                      = -1,
	.rom_header_standard_cal_end_addr                          = -1,
	.rom_standard_cal_sec2lsi_end_addr                         = -1,

        // reference data only
	.rom_awb_start_addr                                        = 0x0270,
	.rom_awb_end_addr                                          = 0x0277,
	.rom_shading_start_addr                                    = 0x0284,
	.rom_shading_end_addr                                      = 0x067C,

	.rom_awb_sec2lsi_start_addr                                = 0x0270,
	.rom_awb_sec2lsi_end_addr                                  = 0x0277,
	.rom_awb_sec2lsi_checksum_addr                             = 0x027C,
	.rom_awb_sec2lsi_checksum_len                              = REAR_AWB_SEC2LSI_CHECKSUM_LEN,

	.rom_shading_sec2lsi_start_addr                            = 0x0280,
	.rom_shading_sec2lsi_end_addr                              = 0x1C67,
	.rom_shading_sec2lsi_checksum_addr                         = 0x1C6C,
	.rom_shading_sec2lsi_checksum_len                          = REAR_SHADING_SEC2LSI_CHECKSUM_LEN,
};

const struct rom_extend_cal_addr wide_imx355_extend_cal_addr = {
	.name = EXTEND_STANDARD_CAL,
	.data = &wide_imx355_standard_cal_info,
	.next = NULL,
};

const struct is_vender_rom_addr rear_imx355_cal_addr = {
    /* Set '-1' if not used */

	.camera_module_es_version                  = 'A',
	.cal_map_es_version                        = '1',
	.rom_max_cal_size                          = IS_REAR_MAX_CAL_SIZE,

	.rom_header_cal_data_start_addr            = 0x00,
	.rom_header_main_module_info_start_addr    = 0x48,
	.rom_header_cal_map_ver_start_addr         = 0x73,
	.rom_header_project_name_start_addr        = 0x7B,
	.rom_header_module_id_addr                 = 0xAE,
	.rom_header_main_sensor_id_addr            = 0xB8,

	.rom_header_sub_module_info_start_addr     = -1,
	.rom_header_sub_sensor_id_addr             = -1,

	.rom_header_main_header_start_addr         = 0x00,
	.rom_header_main_header_end_addr           = 0x04,
	.rom_header_main_oem_start_addr            = 0x10,  /* AF start address */
	.rom_header_main_oem_end_addr              = 0x14,  /* AF end address */
	.rom_header_main_awb_start_addr            = -1, // ??
	.rom_header_main_awb_end_addr              = -1, // ??
	.rom_header_main_shading_start_addr        = -1, // ??
	.rom_header_main_shading_end_addr          = -1, // ??
	.rom_header_main_sensor_cal_start_addr     = 0x20,
	.rom_header_main_sensor_cal_end_addr       = 0x24,
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

	.rom_oem_af_inf_position_addr              = 0x0170,
	.rom_oem_af_macro_position_addr            = 0x017C, // ??
	.rom_oem_module_info_start_addr            = 0x48,
	.rom_oem_checksum_addr                     = 0x01EC,
	.rom_oem_checksum_len                      = REAR_OEM_CHECKSUM_LEN, /* AF checksum length */

	.rom_awb_module_info_start_addr            = -1, // ??
	.rom_awb_checksum_addr                     = -1, // ??
	.rom_awb_checksum_len                      = -1,

	.rom_shading_module_info_start_addr        = -1, // ??
	.rom_shading_checksum_addr                 = -1, // ??
	.rom_shading_checksum_len                  = -1,

	.rom_sensor_cal_module_info_start_addr     = -1, // ??
	.rom_sensor_cal_checksum_addr              = 0x068C, // ??
	.rom_sensor_cal_checksum_len               = REAR_STANDARD_CAL_CHECKSUM_LEN,
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

	.extend_cal_addr                           = &wide_imx355_extend_cal_addr,
};

#endif /* IS_EEPROM_REAR_IMX355_V001_H */