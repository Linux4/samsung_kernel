#ifndef IS_OTPROM_UW_GC5035_V001_H
#define IS_OTPROM_UW_GC5035_V001_H

/*
 * Reference File
 * 20200831_A12_M12s_CAM2(UW_5M_GC5035B)_OTP_Rear_Cal map V008.001.xlsx
 */

#define GC5035_OTP_PAGE_ADDR                         0xfe
#define GC5035_OTP_MODE_ADDR                         0xf3
#define GC5035_OTP_BUSY_ADDR                         0x6f
#define GC5035_OTP_PAGE                              0x02
#define GC5035_OTP_ACCESS_ADDR_HIGH                  0x69
#define GC5035_OTP_ACCESS_ADDR_LOW                   0x6a
#define GC5035_OTP_READ_ADDR                         0x6c

#define GC5035_BANK_SELECT_ADDR                      0x1000
#define GC5035_OTP_START_ADDR_BANK1                  0x1020
#define GC5035_OTP_START_ADDR_BANK2                  0x17C0
#define GC5035_OTP_USED_CAL_SIZE                     ((0x179F - 0x1020 + 0x1) / 8)

#define IS_REAR3_MAX_CAL_SIZE                        ((0xE2DF - 0x1020 + 0x1) / 8)
#define UW_HEADER_CHECKSUM_LEN                       ((0x126F - 0x1020 + 0x1) / 8)

#define UW_STANDARD_CAL_CHECKSUM_LEN                 ((0x177F - 0x1290 + 0x1) / 8)
#define UW_AWB_SEC2LSI_CHECKSUM_LEN                  ((0x133F - 0x12E0 + 0x1) / 8)
#define UW_SHADING_SEC2LSI_CHECKSUM_LEN              ((0xE2BF - 0x1360 + 0x1) / 8)

struct rom_standard_cal_data uw_gc5035_standard_cal_info = {
	.rom_standard_cal_start_addr               = 0x4E,
	.rom_standard_cal_end_addr                 = 0xEF,
	.rom_standard_cal_module_crc_addr          = 0xEC,
	.rom_standard_cal_module_checksum_len      = UW_STANDARD_CAL_CHECKSUM_LEN,
	.rom_header_standard_cal_end_addr          = 0x04,
	.rom_standard_cal_sec2lsi_end_addr         = 0xE2DF,
	
	// reference data only
	.rom_awb_start_addr                        = 0x58,
	.rom_awb_end_addr                          = 0x5F,
	.rom_shading_start_addr                    = 0x62,
	.rom_shading_end_addr                      = 0xEA,
	
	.rom_awb_sec2lsi_start_addr                = 0x58,
	.rom_awb_sec2lsi_end_addr                  = 0x5F,
	.rom_awb_sec2lsi_checksum_addr             = 0x64,
	.rom_awb_sec2lsi_checksum_len              = UW_AWB_SEC2LSI_CHECKSUM_LEN,
	
	.rom_shading_sec2lsi_start_addr            = 0x68,
	.rom_shading_sec2lsi_end_addr              = 0x1A4F,
	.rom_shading_sec2lsi_checksum_addr         = 0x1A54,
	.rom_shading_sec2lsi_checksum_len          = UW_SHADING_SEC2LSI_CHECKSUM_LEN,
};

const struct rom_extend_cal_addr uw_gc5035_extend_cal_addr = {
	.name = EXTEND_STANDARD_CAL,
	.data = &uw_gc5035_standard_cal_info,
	.next = NULL,
};

const struct is_vender_rom_addr uw_gc5035_otp_cal_addr = {
	/* Set '-1' if not used */

	.camera_module_es_version                  = 'A',
	.cal_map_es_version                        = '1',
	.rom_max_cal_size                          = IS_REAR3_MAX_CAL_SIZE,

	.rom_header_cal_data_start_addr            = 0x00,
	.rom_header_main_module_info_start_addr    = 0x0A,
	.rom_header_cal_map_ver_start_addr         = 0x16,
	.rom_header_project_name_start_addr        = 0x1E,
	.rom_header_module_id_addr                 = 0x2E,
	.rom_header_main_sensor_id_addr            = 0x38,

	.rom_header_sub_module_info_start_addr     = -1,
	.rom_header_sub_sensor_id_addr             = -1,

	.rom_header_main_header_start_addr         = -1,
	.rom_header_main_header_end_addr           = -1,
	.rom_header_main_oem_start_addr            = -1,
	.rom_header_main_oem_end_addr              = -1,
	.rom_header_main_awb_start_addr            = -1,
	.rom_header_main_awb_end_addr              = -1,
	.rom_header_main_shading_start_addr        = -1,
	.rom_header_main_shading_end_addr          = -1,
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

	.rom_header_main_mtf_data_addr             = -1,
	.rom_header_sub_mtf_data_addr              = -1,

	.rom_header_checksum_addr                  = 0x4A,
	.rom_header_checksum_len                   = UW_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr              = -1,
	.rom_oem_af_macro_position_addr            = -1,
	.rom_oem_module_info_start_addr            = -1,
	.rom_oem_checksum_addr                     = -1,
	.rom_oem_checksum_len                      = -1,

	.rom_awb_module_info_start_addr            = -1,
	.rom_awb_checksum_addr                     = -1,
	.rom_awb_checksum_len                      = -1,

	.rom_shading_module_info_start_addr        = -1,
	.rom_shading_checksum_addr                 = -1,
	.rom_shading_checksum_len                  = -1,

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
	.rom_dual_shift_x_addr                     = -1,
	.rom_dual_shift_y_addr                     = -1,

	.extend_cal_addr                           = &uw_gc5035_extend_cal_addr,
};

static const u32 sensor_mode_read_initial_setting_gc5035[] = {
	0xfa, 0x10, 0x0,
	0xfe, 0x02, 0x0,
	0x67, 0xc0, 0x0,
	0x59, 0x3f, 0x0,
	0x55, 0x80, 0x0,
	0x65, 0x80, 0x0,
	0x66, 0x03, 0x0,
};

static const u32 sensor_global_gc5035[] = {
	0xfc, 0x01, 0x0, //CM Mode
	0xf4, 0x40, 0x0, //PLL Mode3
	0xf5, 0xe9, 0x0, //CP_CLK_MODE
	0xf6, 0x14, 0x0, //[7:3]WPLLCLK_DIV, [2:0]REFMP_DIV
	0xf8, 0x45, 0x0, //PLL Mode2
	0xf9, 0x82, 0x0,
	0xfa, 0x00, 0x0, //CLK_DIV_MODE
	0xfc, 0x81, 0x0, //CM_MODE
	0xfe, 0x00, 0x0,
	0x36, 0x01, 0x0,
	0xd3, 0x87, 0x0,
	0x36, 0x00, 0x0,
	0x33, 0x00, 0x0, //[5]Col Scaler en, [1:0]Binning Mode
	0xfe, 0x03, 0x0,
	0x01, 0xe7, 0x0,
	0xf7, 0x01, 0x0, //PLL_Mode1
	0xfc, 0x8f, 0x0,
	0xfc, 0x8f, 0x0,
	0xfc, 0x8e, 0x0, //CM_MODE
	0xfe, 0x00, 0x0,
	0xee, 0x30, 0x0,
	0x87, 0x18, 0x0,
	0xfe, 0x01, 0x0,
	0x8c, 0x90, 0x0,
	0xfe, 0x00, 0x0,
	0x11, 0x02, 0x0,
	0x17, 0x80, 0x0, //[1]Updown & [0]Mirror
	0x19, 0x05, 0x0,
	0xfe, 0x02, 0x0,
	0x30, 0x03, 0x0,
	0x31, 0x03, 0x0,
	0xfe, 0x00, 0x0,
	0xd9, 0xc0, 0x0,
	0x1b, 0x20, 0x0,
	0x21, 0x48, 0x0,
	0x28, 0x22, 0x0,
	0x29, 0x58, 0x0,
	0x44, 0x20, 0x0,
	0x4b, 0x10, 0x0,
	0x4e, 0x1a, 0x0,
	0x50, 0x11, 0x0,
	0x52, 0x33, 0x0,
	0x53, 0x44, 0x0,
	0x55, 0x10, 0x0,
	0x5b, 0x11, 0x0,
	0xc5, 0x02, 0x0,
	0x8c, 0x1a, 0x0,
	0xfe, 0x02, 0x0,
	0x33, 0x05, 0x0,
	0x32, 0x38, 0x0,
	0xfe, 0x00, 0x0,
	0x16, 0x0c, 0x0,
	0x1a, 0x1a, 0x0,
	0x20, 0x10, 0x0,
	0x46, 0x83, 0x0,
	0x4a, 0x04, 0x0,
	0x54, 0x02, 0x0,
	0x62, 0x00, 0x0,
	0x72, 0x8f, 0x0,
	0x73, 0x89, 0x0,
	0x7a, 0x05, 0x0,
	0x7c, 0xa0, 0x0,
	0x7d, 0xcc, 0x0,
	0x90, 0x00, 0x0,
	0xce, 0x18, 0x0,
	0xd2, 0x40, 0x0,
	0xe6, 0xe0, 0x0,
	0xfe, 0x02, 0x0,
	0x12, 0x01, 0x0,
	0x13, 0x01, 0x0,
	0x14, 0x01, 0x0,
	0x15, 0x02, 0x0,
	0x22, 0x7c, 0x0,
	0xfe, 0x00, 0x0,
	0xfc, 0x88, 0x0,
	0xfe, 0x10, 0x0,
	0xfe, 0x00, 0x0,
	0xfc, 0x8e, 0x0,
	0xfe, 0x00, 0x0,
	0xfe, 0x00, 0x0,
	0xfe, 0x00, 0x0,
	0xfc, 0x88, 0x0,
	0xfe, 0x10, 0x0,
	0xfe, 0x00, 0x0,
	0xfc, 0x8e, 0x0,
	0xfe, 0x00, 0x0,
	0xb0, 0x6e, 0x0, //Global Gain
	0xb1, 0x01, 0x0, //Digital Gain
	0xb2, 0x00, 0x0,
	0xb3, 0x00, 0x0, //[7:0]Analog PGA Gain
	0xb4, 0x00, 0x0, //[9:8]Analog PGA Gain
	0xb6, 0x00, 0x0, //Analog Gain Code
	0xfe, 0x01, 0x0,
	0x53, 0x00, 0x0,
	0x89, 0x03, 0x0,
	0x60, 0x40, 0x0, //WB Offset
	0xfe, 0x00, 0x0,
	0x3e, 0x00, 0x0, //On: 0x91, Off: 0x00
};

static const u32 sensor_mode_read_initial_setting_gc5035_size =
    sizeof(sensor_mode_read_initial_setting_gc5035) / sizeof( sensor_mode_read_initial_setting_gc5035[0]);

static const u32 sensor_global_gc5035_size =
    sizeof(sensor_global_gc5035) / sizeof( sensor_global_gc5035[0]);

#endif /* IS_OTPROM_UW_GC5035_V001_H */
