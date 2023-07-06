#ifndef IS_EEPROM_REAR_GM2_GC02M1_V001_H
#define IS_EEPROM_REAR_GM2_GC02M1_V001_H

/* Reference File
 * Universal EEPROM map data V001_20201014_for M12_2020_Nacho_16K_(PDAF_GM2_48M).xlsx
 */

#define IS_REAR_MAX_CAL_SIZE    (16 * 1024)

#define REAR_HEADER_CHECKSUM_LEN     (0x00FB - 0x0000 + 0x1)
#define REAR_OEM_CHECKSUM_LEN        (0x0189 - 0x0100 + 0x1)
#define REAR_AWB_CHECKSUM_LEN        (0x023B - 0x01C0 + 0x1)
#define REAR_SHADING_CHECKSUM_LEN    (0x08AF - 0x0260 + 0x1)
#define REAR_SENSOR_CAL_CHECKSUM_LEN (0x2A91 - 0x0970 + 0x1)
#define REAR_AE_CHECKSUM_LEN         (0x2EB0 - 0x2E80 + 0x1)
#define REAR_DUAL_CHECKSUM_LEN       (0x370B - 0x2ED0 + 0x1)
#define REAR_PDAF_CHECKSUM_LEN       (0x3CDF - 0x3720 + 0x1)
/* Crosstalk cal data for SW remosaic */
#define REAR_XTALK_CAL_DATA_SIZE     (8482)

struct rom_ae_cal_data rear_gm2_ae_cal_info = {
	.rom_header_main_ae_start_addr  = 0x28,
	.rom_header_main_ae_end_addr    = 0x2C,
	.rom_ae_module_info_start_addr  = 0x2EB1,
	.rom_ae_checksum_addr           = 0x2ECC,
	.rom_ae_checksum_len            = REAR_AE_CHECKSUM_LEN,
};

const struct rom_extend_cal_addr rear_gm2_extend_cal_addr = {
	.name = EXTEND_AE_CAL,
	.data = &rear_gm2_ae_cal_info,
	.next = NULL,
};

const struct is_vender_rom_addr rear_gm2_cal_addr = {
	/* Set '-1' if not used */

	.camera_module_es_version                  = 'A',
	.cal_map_es_version                        = '1',
	.rom_max_cal_size                          = IS_REAR_MAX_CAL_SIZE,

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
	.rom_header_main_oem_start_addr            = 0x08,
	.rom_header_main_oem_end_addr              = 0x0C,
	.rom_header_main_awb_start_addr            = 0x10,
	.rom_header_main_awb_end_addr              = 0x14,
	.rom_header_main_shading_start_addr        = 0x18,
	.rom_header_main_shading_end_addr          = 0x1C,
	.rom_header_main_sensor_cal_start_addr     = 0x20,
	.rom_header_main_sensor_cal_end_addr       = 0x24,
	.rom_header_dual_cal_start_addr            = 0x30,
	.rom_header_dual_cal_end_addr              = 0x34,
	.rom_header_pdaf_cal_start_addr            = 0x38,
	.rom_header_pdaf_cal_end_addr              = 0x3C,

	.rom_header_sub_oem_start_addr             = -1,
	.rom_header_sub_oem_end_addr               = -1,
	.rom_header_sub_awb_start_addr             = -1,
	.rom_header_sub_awb_end_addr               = -1,
	.rom_header_sub_shading_start_addr         = -1,
	.rom_header_sub_shading_end_addr           = -1,

	.rom_header_main_mtf_data_addr             = 0x0156,
	.rom_header_sub_mtf_data_addr              = -1,

	.rom_header_checksum_addr                  = 0xFC,
	.rom_header_checksum_len                   = REAR_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr              = 0x0100,
	.rom_oem_af_macro_position_addr            = 0x0108,
	.rom_oem_module_info_start_addr            = 0x018A,
	.rom_oem_checksum_addr                     = 0x01BC,
	.rom_oem_checksum_len                      = REAR_OEM_CHECKSUM_LEN,

	.rom_awb_module_info_start_addr            = 0x023C,
	.rom_awb_checksum_addr                     = 0x025C,
	.rom_awb_checksum_len                      = REAR_AWB_CHECKSUM_LEN,

	.rom_shading_module_info_start_addr        = 0x08B0,
	.rom_shading_checksum_addr                 = 0x096C,
	.rom_shading_checksum_len                  = REAR_SHADING_CHECKSUM_LEN,

	.rom_sensor_cal_module_info_start_addr     = 0x2E50,
	.rom_sensor_cal_checksum_addr              = 0x2E7C,
	.rom_sensor_cal_checksum_len               = REAR_SENSOR_CAL_CHECKSUM_LEN,

	.rom_dual_module_info_start_addr           = 0x370C,
	.rom_dual_checksum_addr                    = 0x371C,
	.rom_dual_checksum_len                     = REAR_DUAL_CHECKSUM_LEN,

	.rom_pdaf_module_info_start_addr           = 0x3CE0,
	.rom_pdaf_checksum_addr                    = 0x3D0C,
	.rom_pdaf_checksum_len                     = REAR_PDAF_CHECKSUM_LEN,

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

	.rom_dual_cal_data2_start_addr             = 0x2ED0,
	.rom_dual_cal_data2_size                   = (2108),
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

	.extend_cal_addr                           = &rear_gm2_extend_cal_addr,
};

#endif /* IS_EEPROM_REAR_GM2_GC02M1_V001_H */
