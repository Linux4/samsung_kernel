#ifndef IS_EEPROM_REAR_PDAF_2P6_V001_H
#define IS_EEPROM_REAR_PDAF_2P6_V001_H

/**
 * Reference File: 
 * 20200922_Xcover5_CAM1(Wide_16M_2P6)_EEPROM_Rear_Cal map V008.001_공용.xlsx
 */

#define IS_REAR_MAX_CAL_SIZE (0x3ABF - 0x0000 + 0x1) //Include Standard Cal Size

#define REAR_HEADER_CHECKSUM_LEN (0x00FB - 0x0000 + 0x1)
#define REAR_OEM_CHECKSUM_LEN (0x122B - 0x11B0 + 0x1)
#define REAR_AWB_CHECKSUM_LEN (0x1EAB - 0x1EA0 + 0x1)
#define REAR_SHADING_CHECKSUM_LEN (0x22AB - 0x1EB0 + 0x1)
#define REAR_PDAF_CHECKSUM_LEN (0x19EB - 0x1230 + 0x1)
#define REAR_DUAL_CHECKSUM_LEN (0x099B - 0x0180 + 0x1)
#define REAR_AWB_SEC2LSI_CHECKSUM_LEN (0x1EAB - 0x1EA0 + 0x1)
#define REAR_SHADING_SEC2LSI_CHECKSUM_LEN (0x389B - 0x1EB0 + 0x1)

struct rom_standard_cal_data rear_2p6_standard_cal_info = {
	.rom_standard_cal_start_addr                               = -1,
	.rom_standard_cal_end_addr                                 = -1,
	.rom_standard_cal_module_crc_addr                          = -1,
	.rom_standard_cal_module_checksum_len                      = -1,
	.rom_header_standard_cal_end_addr                          = -1,
	.rom_standard_cal_sec2lsi_end_addr                         = -1,

	// reference data only
	.rom_awb_start_addr                                        = 0x1EA0,
	.rom_awb_end_addr                                          = 0x1EA7,
	.rom_shading_start_addr                                    = 0x1EB0,
	.rom_shading_end_addr                                      = 0x22A8,

	.rom_awb_sec2lsi_start_addr                                = 0x1EA0,
	.rom_awb_sec2lsi_end_addr                                  = 0x1EA7,
	.rom_awb_sec2lsi_checksum_addr                             = 0x1EAC,
	.rom_awb_sec2lsi_checksum_len                              = REAR_AWB_SEC2LSI_CHECKSUM_LEN,

	.rom_shading_sec2lsi_start_addr                            = 0x1EB0,
	.rom_shading_sec2lsi_end_addr                              = 0x3897,
	.rom_shading_sec2lsi_checksum_addr                         = 0x389C,
	.rom_shading_sec2lsi_checksum_len                          = REAR_SHADING_SEC2LSI_CHECKSUM_LEN,
};

const struct rom_extend_cal_addr rear_2p6_extend_cal_addr = {
	.name = EXTEND_STANDARD_CAL,
	.data = &rear_2p6_standard_cal_info,
	.next = NULL,
};


const struct is_vender_rom_addr rear_2p6_cal_addr = {
	/* Set '-1' if not used */

	.camera_module_es_version                 = 'A',
	.cal_map_es_version                       = '1',
	.rom_max_cal_size                         = IS_REAR_MAX_CAL_SIZE,
  
	.rom_header_cal_data_start_addr           = 0x00,
	.rom_header_main_module_info_start_addr   = 0x6E,
	.rom_header_cal_map_ver_start_addr        = 0x90,
	.rom_header_project_name_start_addr       = 0x98,
	.rom_header_module_id_addr                = 0xAE,
	.rom_header_main_sensor_id_addr           = 0xB8,
  
	.rom_header_sub_module_info_start_addr    = -1,
	.rom_header_sub_sensor_id_addr            = -1,
  
	.rom_header_main_header_start_addr        = 0x00,
	.rom_header_main_header_end_addr          = 0x04,
	.rom_header_main_oem_start_addr           = 0x30, //LSI AF Cal.data in Cal Map Excel sheet
	.rom_header_main_oem_end_addr             = 0x34,
	.rom_header_main_awb_start_addr           = 0x50,
	.rom_header_main_awb_end_addr             = 0x54,
	.rom_header_main_shading_start_addr       = 0x58, //LSC Cal.Data in the Excel sheet
	.rom_header_main_shading_end_addr         = 0x5C,
	.rom_header_main_sensor_cal_start_addr    = -1,
	.rom_header_main_sensor_cal_end_addr      = -1,
	.rom_header_dual_cal_start_addr           = 0x20,
	.rom_header_dual_cal_end_addr             = 0x24,
	.rom_header_pdaf_cal_start_addr           = 0x38, //LSI PDAF Cal.Data
	.rom_header_pdaf_cal_end_addr             = 0x3C,
  
	.rom_header_sub_oem_start_addr            = -1,	
	.rom_header_sub_oem_end_addr              = -1,	
	.rom_header_sub_awb_start_addr            = -1,
	.rom_header_sub_awb_end_addr              = -1,
	.rom_header_sub_shading_start_addr        = -1,
	.rom_header_sub_shading_end_addr          = -1,	
  
	.rom_header_main_mtf_data_addr            = -1,
	.rom_header_sub_mtf_data_addr             = -1,
  
	.rom_header_checksum_addr                 = 0xFC,
	.rom_header_checksum_len                  = REAR_HEADER_CHECKSUM_LEN,
  
	.rom_oem_af_inf_position_addr             = 0x11B0,
	.rom_oem_af_macro_position_addr           = 0x11B8,
	.rom_oem_module_info_start_addr           = 0x11B0,
	.rom_oem_checksum_addr                    = 0x122C,
	.rom_oem_checksum_len                     = REAR_OEM_CHECKSUM_LEN,

	.rom_awb_module_info_start_addr           = -1,
	.rom_awb_checksum_addr                    = 0x1EAC,
	.rom_awb_checksum_len                     = REAR_AWB_CHECKSUM_LEN,
  
	.rom_shading_module_info_start_addr       = -1,
	.rom_shading_checksum_addr                = 0x22AC,
	.rom_shading_checksum_len                 = REAR_SHADING_CHECKSUM_LEN,		
  
	.rom_sensor_cal_module_info_start_addr    = -1,
	.rom_sensor_cal_checksum_addr             = -1,
	.rom_sensor_cal_checksum_len              = -1,
  
	.rom_dual_module_info_start_addr          = 0x0180,
	.rom_dual_checksum_addr                   = 0x099C,
	.rom_dual_checksum_len                    = REAR_DUAL_CHECKSUM_LEN,
  
	.rom_pdaf_module_info_start_addr          = 0x1230,
	.rom_pdaf_checksum_addr                   = 0x19EC,
	.rom_pdaf_checksum_len                    = REAR_PDAF_CHECKSUM_LEN,

	.rom_sub_oem_af_inf_position_addr         = -1,
	.rom_sub_oem_af_macro_position_addr       = -1,
	.rom_sub_oem_module_info_start_addr       = -1,
	.rom_sub_oem_checksum_addr                = -1,
	.rom_sub_oem_checksum_len                 = -1,

	.rom_sub_awb_module_info_start_addr       = -1,
	.rom_sub_awb_checksum_addr                = -1,
	.rom_sub_awb_checksum_len                 = -1,

	.rom_sub_shading_module_info_start_addr   = -1,
	.rom_sub_shading_checksum_addr            = -1,
	.rom_sub_shading_checksum_len             = -1,

	.rom_dual_cal_data2_start_addr            = -1,
	.rom_dual_cal_data2_size                  = -1,
	.rom_dual_tilt_x_addr                     = -1,
	.rom_dual_tilt_y_addr                     = -1,
	.rom_dual_tilt_z_addr                     = -1,
	.rom_dual_tilt_sx_addr                    = -1,
	.rom_dual_tilt_sy_addr                    = -1,
	.rom_dual_tilt_range_addr                 = -1,
	.rom_dual_tilt_max_err_addr               = -1,
	.rom_dual_tilt_avg_err_addr               = -1,
	.rom_dual_tilt_dll_version_addr           = -1,
	.rom_dual_shift_x_addr                    = -1,
	.rom_dual_shift_y_addr                    = -1,

	.extend_cal_addr                        = &rear_2p6_extend_cal_addr,
};

#endif /* IS_EEPROM_REAR_PDAF_2P6_V001_H */
