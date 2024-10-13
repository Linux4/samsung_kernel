#ifndef IS_OTPROM_FRONT_HI1336B_V001_H
#define IS_OTPROM_FRONT_HI1336B_V001_H

/*
 * Reference File
 * 20220711_A14_5G_CAM1(Front_13M_Hi1336)_OTP_Front_Cal map V008.002_QC_LSI_MTK_공용_Map.xlsx
 */

/*********HI1336B OTP*********/
#define HI1336B_OTP_ACCESS_ADDR_HIGH                             0x030A
#define HI1336B_OTP_ACCESS_ADDR_LOW                              0x030B
#define HI1336B_OTP_MODE_ADDR                                    0x0302
#define HI1336B_OTP_READ_ADDR                                    0x0308

#define HI1336B_BANK_SELECT_ADDR                                 0x0400
#define HI1336B_OTP_START_ADDR_BANK1                             0x0404
#define HI1336B_OTP_START_ADDR_BANK2                             0x07A4
#define HI1336B_OTP_START_ADDR_BANK3                             0x0B44
#define HI1336B_OTP_START_ADDR_BANK4                             0x0EE4
#define HI1336B_OTP_START_ADDR_BANK5                             0x1284

#define HI1336B_OTP_USED_CAL_SIZE                                (0x079F - 0x0404 + 0x1)
#define IS_FRONT_MAX_CAL_SIZE                                    (8*1024)
#define HI1336B_FRONT_HEADER_CHECKSUM_LEN                        (0x0455 - 0x0404 + 0x1)

#define FRONT_STANDARD_CAL_CHECKSUM_LEN                          (0x057B - 0x045A + 0x1)
#define FRONT_AWB_SEC2LSI_CHECKSUM_LEN                           (0x006F - 0x0064 + 0x1)
#define FRONT_SHADING_SEC2LSI_CHECKSUM_LEN                       (0x1A5F - 0x0074 + 0x1)

struct rom_standard_cal_data front_hi1336b_standard_cal_info = {
	.rom_standard_cal_start_addr               = 0x56,
	.rom_standard_cal_end_addr                 = 0x17B,
	.rom_standard_cal_module_crc_addr          = 0x178,
	.rom_standard_cal_module_checksum_len      = FRONT_STANDARD_CAL_CHECKSUM_LEN,
	.rom_header_standard_cal_end_addr          = 0x04,
	.rom_standard_cal_sec2lsi_end_addr         = 0x1A63,

	// reference data only
	.rom_awb_start_addr                        = 0x64,
	.rom_awb_end_addr                          = 0x6B,
	.rom_shading_start_addr                    = 0x6E,
	.rom_shading_end_addr                      = 0x166,

	.rom_awb_sec2lsi_start_addr                = 0x64,
	.rom_awb_sec2lsi_end_addr                  = 0x6B,
	.rom_awb_sec2lsi_checksum_addr             = 0x70,
	.rom_awb_sec2lsi_checksum_len              = FRONT_AWB_SEC2LSI_CHECKSUM_LEN,

	.rom_shading_sec2lsi_start_addr            = 0x74,
	.rom_shading_sec2lsi_end_addr              = 0x1A5B,
	.rom_shading_sec2lsi_checksum_addr         = 0x1A60,
	.rom_shading_sec2lsi_checksum_len          = FRONT_SHADING_SEC2LSI_CHECKSUM_LEN,
};

const struct rom_extend_cal_addr front_hi1336b_extend_cal_addr = {
	.name = EXTEND_STANDARD_CAL,
	.data = &front_hi1336b_standard_cal_info,
	.next = NULL,
};

const struct is_vender_rom_addr front_hi1336b_otp_cal_addr = {
	/* Set '-1' if not used */

	.camera_module_es_version                                  = 'A',
	.cal_map_es_version                                        = '1',
	.rom_max_cal_size                                          = IS_FRONT_MAX_CAL_SIZE,

	.rom_header_cal_data_start_addr                            = 0x00,
	.rom_header_main_module_info_start_addr                    = 0x0A,
	.rom_header_cal_map_ver_start_addr                         = 0x16,
	.rom_header_project_name_start_addr                        = 0x1E,
	.rom_header_module_id_addr                                 = 0x36,
	.rom_header_main_sensor_id_addr                            = 0x40,

	.rom_header_sub_module_info_start_addr                     = -1,
	.rom_header_sub_sensor_id_addr                             = -1,

	.rom_header_main_header_start_addr                         = -1,
	.rom_header_main_header_end_addr                           = -1,
	.rom_header_main_oem_start_addr                            = -1,
	.rom_header_main_oem_end_addr                              = -1,
	.rom_header_main_awb_start_addr                            = -1,
	.rom_header_main_awb_end_addr                              = -1,
	.rom_header_main_shading_start_addr                        = -1,
	.rom_header_main_shading_end_addr                          = -1,
	.rom_header_main_sensor_cal_start_addr                     = -1,
	.rom_header_main_sensor_cal_end_addr                       = -1,
	.rom_header_dual_cal_start_addr                            = -1,
	.rom_header_dual_cal_end_addr                              = -1,
	.rom_header_pdaf_cal_start_addr                            = -1,
	.rom_header_pdaf_cal_end_addr                              = -1,

	.rom_header_sub_oem_start_addr                             = -1,
	.rom_header_sub_oem_end_addr                               = -1,
	.rom_header_sub_awb_start_addr                             = -1,
	.rom_header_sub_awb_end_addr                               = -1,
	.rom_header_sub_shading_start_addr                         = -1,
	.rom_header_sub_shading_end_addr                           = -1,

	.rom_header_main_mtf_data_addr                             = -1,
	.rom_header_sub_mtf_data_addr                              = -1,

	.rom_header_checksum_addr                                  = 0x52,
	.rom_header_checksum_len                                   = HI1336B_FRONT_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr                              = -1,
	.rom_oem_af_macro_position_addr                            = -1,
	.rom_oem_module_info_start_addr                            = -1,
	.rom_oem_checksum_addr                                     = -1,
	.rom_oem_checksum_len                                      = -1,

	.rom_awb_module_info_start_addr                            = -1,
	.rom_awb_checksum_addr                                     = -1,
	.rom_awb_checksum_len                                      = -1,

	.rom_shading_module_info_start_addr                        = -1,
	.rom_shading_checksum_addr                                 = -1,
	.rom_shading_checksum_len                                  = -1,

	.rom_sensor_cal_module_info_start_addr                     = -1,
	.rom_sensor_cal_checksum_addr                              = -1,
	.rom_sensor_cal_checksum_len                               = -1,

	.rom_dual_module_info_start_addr                           = -1,
	.rom_dual_checksum_addr                                    = -1,
	.rom_dual_checksum_len                                     = -1,

	.rom_pdaf_module_info_start_addr                           = -1,
	.rom_pdaf_checksum_addr                                    = -1,
	.rom_pdaf_checksum_len                                     = -1,

	.rom_sub_oem_af_inf_position_addr                          = -1,
	.rom_sub_oem_af_macro_position_addr                        = -1,
	.rom_sub_oem_module_info_start_addr                        = -1,
	.rom_sub_oem_checksum_addr                                 = -1,
	.rom_sub_oem_checksum_len                                  = -1,

	.rom_sub_awb_module_info_start_addr                        = -1,
	.rom_sub_awb_checksum_addr                                 = -1,
	.rom_sub_awb_checksum_len                                  = -1,

	.rom_sub_shading_module_info_start_addr                    = -1,
	.rom_sub_shading_checksum_addr                             = -1,
	.rom_sub_shading_checksum_len                              = -1,

	.rom_dual_cal_data2_start_addr                             = -1,
	.rom_dual_cal_data2_size                                   = -1,
	.rom_dual_tilt_x_addr                                      = -1,
	.rom_dual_tilt_y_addr                                      = -1,
	.rom_dual_tilt_z_addr                                      = -1,
	.rom_dual_tilt_sx_addr                                     = -1,
	.rom_dual_tilt_sy_addr                                     = -1,
	.rom_dual_tilt_range_addr                                  = -1,
	.rom_dual_tilt_max_err_addr                                = -1,
	.rom_dual_tilt_avg_err_addr                                = -1,
	.rom_dual_tilt_dll_version_addr                            = -1,
	.rom_dual_tilt_dll_model_id_addr                           = -1,
	.rom_dual_tilt_dll_model_id_size                           = -1,
	.rom_dual_shift_x_addr                                     = -1,
	.rom_dual_shift_y_addr                                     = -1,

	.extend_cal_addr                                           = &front_hi1336b_extend_cal_addr,
};

#endif /* IS_OTPROM_FRONT_HI1336B_V001_H */