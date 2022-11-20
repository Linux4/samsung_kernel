#ifndef IS_OTPROM_FRONT_4HA_V001_H
#define IS_OTPROM_FRONT_4HA_V001_H

/*
 * Reference File
 * 20210511_A12_CAM1(Front_8M_4HA)_OTP_Front_Cal map V008.001.xlsx
 */

#define S5K4HA_FRONT_HEADER_CHECKSUM_LEN                              (0x0A11 + 0x0040 - 0x0A08 + 0x1)
#define S5K4HA_FRONT_STANDARD_CAL_CHECKSUM_LEN                        (0x0A3F + 0x400 - 0x0A16 + 0x1)
#define S5K4HA_FRONT_AWB_SEC2LSI_CHECKSUM_LEN                         (0x0067 - 0x005C + 0x1)
#define S5K4HA_FRONT_SHADING_SEC2LSI_CHECKSUM_LEN                     (0x1A57 - 0x006C + 0x1)

#define S5K4HA_OTP_USED_CAL_SIZE                                      (0x0A43 + 0x0440 - 0x0A08 + 0x1)

#ifndef DUALIZE_SR846_S5K4HA
#define IS_FRONT_MAX_CAL_SIZE                                         (0x1A5B - 0x0000 + 0x1)//Map for LSI AP
#endif

/*********S5K4HA OTP*********/
#define S5K4HA_STANDBY_ADDR                                     0x0136
#define S5K4HA_OTP_R_W_MODE_ADDR                                0x0A00
#define S5K4HA_OTP_CHECK_ADDR                                   0x0A01
#define S5K4HA_OTP_PAGE_SELECT_ADDR                             0x0A02

#define S5K4HA_OTP_PAGE_ADDR_L                                  0x0A04
#define S5K4HA_OTP_PAGE_ADDR_H                                  0x0A43
#define S5K4HA_OTP_BANK_SELECT                                  0x0A04
#define S5K4HA_OTP_START_PAGE_BANK1                             0x11
#define S5K4HA_OTP_START_PAGE_BANK2                             0x23

#define S5K4HA_OTP_START_ADDR                                   0x0A08

struct rom_standard_cal_data front_4ha_standard_cal_info = {
	.rom_standard_cal_start_addr                               = 0x4E,
	.rom_standard_cal_end_addr                                 = 0x047B,
	.rom_standard_cal_module_crc_addr                          = 0x0478,
	.rom_standard_cal_module_checksum_len                      = S5K4HA_FRONT_STANDARD_CAL_CHECKSUM_LEN,
	.rom_header_standard_cal_end_addr                          = 0x04,
	.rom_standard_cal_sec2lsi_end_addr                         = 0x1A5B,

	// reference data only
	.rom_awb_start_addr                                        = 0x64,
	.rom_awb_end_addr                                          = 0x6B,
	.rom_shading_start_addr                                    = 0x6E,
	.rom_shading_end_addr                                      = 0x0466,

	.rom_awb_sec2lsi_start_addr                                = 0x5C,
	.rom_awb_sec2lsi_end_addr                                  = 0x63,
	.rom_awb_sec2lsi_checksum_addr                             = 0x68,
	.rom_awb_sec2lsi_checksum_len                              = S5K4HA_FRONT_AWB_SEC2LSI_CHECKSUM_LEN,

	.rom_shading_sec2lsi_start_addr                            = 0x6C,
	.rom_shading_sec2lsi_end_addr                              = 0x1A53,
	.rom_shading_sec2lsi_checksum_addr                         = 0x1A58,
	.rom_shading_sec2lsi_checksum_len                          = S5K4HA_FRONT_SHADING_SEC2LSI_CHECKSUM_LEN,
};

const struct rom_extend_cal_addr front_4ha_extend_cal_addr = {
	.name = EXTEND_STANDARD_CAL,
	.data = &front_4ha_standard_cal_info,
	.next = NULL,
};

const struct is_vender_rom_addr front_4ha_otp_cal_addr = {
	/* Set '-1' if not used */

	.camera_module_es_version                                  = 'A',
	.cal_map_es_version                                        = '1',
	.rom_max_cal_size                                          = IS_FRONT_MAX_CAL_SIZE,

	.rom_header_cal_data_start_addr                            = 0x00,
	.rom_header_main_module_info_start_addr                    = 0x0A,
	.rom_header_cal_map_ver_start_addr                         = 0x16,
	.rom_header_project_name_start_addr                        = 0x1E,
	.rom_header_module_id_addr                                 = 0x2E,
	.rom_header_main_sensor_id_addr                            = 0x38,

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

	.rom_header_checksum_addr                                  = 0x4A,
	.rom_header_checksum_len                                   = S5K4HA_FRONT_HEADER_CHECKSUM_LEN,

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

	.extend_cal_addr                                           = &front_4ha_extend_cal_addr,
};

static const u32 sensor_otp_4ha_global[] = {
	0x0100, 0x00, 0x01,
	0x0A02, 0x7F, 0x01,
	0x3B45, 0x01, 0x01,
	0x3264, 0x01, 0x01,
	0x3290, 0x10, 0x01,
	0x0B05, 0x01, 0x01,
	0x3069, 0xC7, 0x01,
	0x3074, 0x06, 0x01,
	0x3075, 0x32, 0x01,
	0x3068, 0xF7, 0x01,
	0x30C6, 0x01, 0x01,
	0x301F, 0x20, 0x01,
	0x306B, 0x9A, 0x01,
	0x3091, 0x1F, 0x01,
	0x306E, 0x71, 0x01,
	0x306F, 0x28, 0x01,
	0x306D, 0x08, 0x01,
	0x3084, 0x16, 0x01,
	0x3070, 0x0F, 0x01,
	0x306A, 0x79, 0x01,
	0x30B0, 0xFF, 0x01,
	0x30C2, 0x05, 0x01,
	0x30C4, 0x06, 0x01,
	0x3081, 0x07, 0x01,
	0x307B, 0x85, 0x01,
	0x307A, 0x0A, 0x01,
	0x3079, 0x0A, 0x01,
	0x308A, 0x20, 0x01,
	0x308B, 0x08, 0x01,
	0x308C, 0x0B, 0x01,
	0x392F, 0x01, 0x01,
	0x3930, 0x00, 0x01,
	0x3924, 0x7F, 0x01,
	0x3925, 0xFD, 0x01,
	0x3C08, 0xFF, 0x01,
	0x3C09, 0xFF, 0x01,
	0x3C31, 0xFF, 0x01,
	0x3C32, 0xFF, 0x01,
};

static const u32 sensor_otp_4ha_global_size = ARRAY_SIZE(sensor_otp_4ha_global);

#endif /* IS_OTPROM_FRONT_4HA_V001_H */
