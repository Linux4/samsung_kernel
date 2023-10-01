#ifndef IS_OTPROM_UW_GC02M1_V001_H
#define IS_OTPROM_UW_GC02M1_V001_H

/*
 * Reference File
 * 20200731_A12_CAM4(Bokeh_2M_GC02M1B)_OTP_Rear_Cal map V008.001_for MTK.xlsx
 */

#define GC02M1_OTP_PAGE_ADDR                         0xfe
#define GC02M1_OTP_MODE_ADDR                         0xf3
#define GC02M1_OTP_PAGE                              0x02
#define GC02M1_OTP_ACCESS_ADDR                       0x17
#define GC02M1_OTP_READ_ADDR                         0x19

#define GC02M1_OTP_START_ADDR                        0x0078
#define GC02M1_OTP_USED_CAL_SIZE                     ((0x00FF - 0x0078 + 0x1) / 8)

#define IS_REAR2_MAX_CAL_SIZE                        (GC02M1_OTP_USED_CAL_SIZE)
#define REAR2_HEADER_CHECKSUM_LEN                    ((0x00DF - 0x0078 + 0x1) / 8)

const struct is_vender_rom_addr rear2_gc02m1_otp_cal_addr = {
	/* Set '-1' if not used */

	.camera_module_es_version                  = 'M',
	.cal_map_es_version                        = '\0',
	.rom_max_cal_size                          = IS_REAR2_MAX_CAL_SIZE,

	.rom_header_cal_data_start_addr            = 0x00,
	.rom_header_main_module_info_start_addr    = 0x00,
	.rom_header_cal_map_ver_start_addr         = -1,
	.rom_header_project_name_start_addr        = -1,
	.rom_header_module_id_addr                 = -1,
	.rom_header_main_sensor_id_addr            = -1,

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

	.rom_header_checksum_addr                  = 0x0D,
	.rom_header_checksum_len                   = REAR2_HEADER_CHECKSUM_LEN,

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

	.extend_cal_addr                           = NULL,
};

static const u32 sensor_mode_read_initial_setting_gc02m1[] = {
	0xfc, 0x01, 0x00,
	0xf4, 0x41, 0x00,
	0xf5, 0xc0, 0x00,
	0xf6, 0x44, 0x00,
	0xf8, 0x38, 0x00,
	0xf9, 0x82, 0x00,
	0xfa, 0x00, 0x00,
	0xfd, 0x80, 0x00,
	0xfc, 0x81, 0x00,
	0xfe, 0x03, 0x00,
	0x01, 0x0b, 0x00,
	0xf7, 0x01, 0x00,
	0xfc, 0x80, 0x00,
	0xfc, 0x80, 0x00,
	0xfc, 0x80, 0x00,
	0xfc, 0x8e, 0x00,
	0xfe, 0x00, 0x00,
	0x87, 0x09, 0x00,
	0xee, 0x72, 0x00,
	0xfe, 0x01, 0x00,
	0x8c, 0x90, 0x00,
	0xf3, 0x30, 0x00,
	0xfe, 0x02, 0x00,
};

static const u32 sensor_global_gc02m1[] = {
	0xfc, 0x01, 0x00,
	0xf4, 0x41, 0x00,
	0xf5, 0xc0, 0x00,
	0xf6, 0x44, 0x00,
	0xf8, 0x34, 0x00,
	0xf9, 0x82, 0x00,
	0xfa, 0x00, 0x00,
	0xfd, 0x80, 0x00,
	0xfc, 0x81, 0x00,
	0xfe, 0x03, 0x00,
	0x01, 0x0b, 0x00,
	0xf7, 0x01, 0x00,
	0xfc, 0x80, 0x00,
	0xfc, 0x80, 0x00,
	0xfc, 0x80, 0x00,
	0xfc, 0x8e, 0x00,
	0xfe, 0x00, 0x00,
	0x87, 0x09, 0x00,
	0xee, 0x72, 0x00,
	0xfe, 0x01, 0x00,
	0x8c, 0x90, 0x00,
	0xfe, 0x00, 0x00,
	0x17, 0x80, 0x00,
	0x19, 0x04, 0x00,
	0x56, 0x20, 0x00,
	0x5b, 0x00, 0x00,
	0x5e, 0x01, 0x00,
	0x21, 0x3c, 0x00,
	0x29, 0x40, 0x00,
	0x44, 0x20, 0x00,
	0x4b, 0x10, 0x00,
	0x55, 0x1a, 0x00,
	0xcc, 0x01, 0x00,
	0x27, 0x30, 0x00,
	0x2b, 0x00, 0x00,
	0x33, 0x00, 0x00,
	0x53, 0x90, 0x00,
	0xe6, 0x50, 0x00,
	0x39, 0x07, 0x00,
	0x43, 0x04, 0x00,
	0x46, 0x2a, 0x00,
	0x7c, 0xa0, 0x00,
	0xd0, 0xbe, 0x00,
	0xd1, 0x60, 0x00,
	0xd2, 0x40, 0x00,
	0xd3, 0xf3, 0x00,
	0xde, 0x1d, 0x00,
	0xcd, 0x05, 0x00,
	0xce, 0x6f, 0x00,
	0xfc, 0x88, 0x00,
	0xfe, 0x10, 0x00,
	0xfe, 0x00, 0x00,
	0xfc, 0x8e, 0x00,
	0xfe, 0x00, 0x00,
	0xfe, 0x00, 0x00,
	0xfe, 0x00, 0x00,
	0xfe, 0x00, 0x00,
	0xfc, 0x88, 0x00,
	0xfe, 0x10, 0x00,
	0xfe, 0x00, 0x00,
	0xfc, 0x8e, 0x00,
	0xfe, 0x04, 0x00,
	0xe0, 0x01, 0x00,
	0xfe, 0x00, 0x00,
	0xfe, 0x00, 0x00,
	0x3e, 0x00, 0x00,
};

static const u32 sensor_mode_read_initial_setting_gc02m1_size =
	sizeof(sensor_mode_read_initial_setting_gc02m1) / sizeof( sensor_mode_read_initial_setting_gc02m1[0]);

static const u32 sensor_global_gc02m1_size =
	sizeof(sensor_global_gc02m1) / sizeof( sensor_global_gc02m1[0]);

#endif /* IS_OTPROM_UW_GC02M1_V001_H */
