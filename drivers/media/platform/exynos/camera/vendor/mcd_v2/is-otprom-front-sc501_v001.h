#ifndef IS_OTPROM_FRONT_SC501_V001_H
#define IS_OTPROM_FRONT_SC501_V001_H

/*********SC501 OTP*********/

/* It caused compile error in A04s. Because it is duplicated in is-otprom-front-hi556_v001.h */
//#define IS_FRONT_MAX_CAL_SIZE                                (0x1A7F - 0x0000 + 0x1)
#define SC501_FRONT_HEADER_CHECKSUM_LEN                        ((0x4B - 0x00 + 0x1))

struct rom_ap2ap_standard_cal_data front_sc501_ap2ap_standard_cal_info = {
	.rom_orig_start_addr                                       = 0x0,
	.rom_orig_end_addr                                         = 0x783,
	.rom_lsi_start_addr                                        = 0x0,
	.rom_lsi_end_addr                                          = 0x1A5F,

	.rom_num_of_segments                                       = 4,
	.rom_num_of_groups                                         = 2,

	.rom_group_start_addr = {
		{0x80EC, 0x8870},
		{0x80F5, 0x8879},
		{0x8103, 0x8887},
		{0x8115, 0x8899}
	},
	.rom_seg_checksum_len = {
		8, 13, 17, 1881
	},
	.rom_seg_len = {
		9, 14, 18, 1882
	},
	.rom_total_checksum_addr = {
		0x886F, 0x8FE7
	},
};

const struct rom_extend_cal_addr front_sc501_extend_cal_addr = {
	.name = EXTEND_AP2AP_STANDARD_CAL,
	.data = &front_sc501_ap2ap_standard_cal_info,
	.next = NULL,
};

struct is_vender_rom_addr front_sc501_otp_cal_addr = {
	/* Set '-1' if not used */

	.camera_module_es_version                                  = 'A',
	.cal_map_es_version                                        = '1',
	.rom_max_cal_size                                          = IS_FRONT_MAX_CAL_SIZE,

	.rom_header_cal_data_start_addr                            = 0x00,
	.rom_header_main_module_info_start_addr                    = 0x0A,
	.rom_header_cal_map_ver_start_addr                         = 0x18,
	.rom_header_project_name_start_addr                        = 0x20,
	.rom_header_module_id_addr                                 = 0x30,
	.rom_header_main_sensor_id_addr                            = 0x3A,

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

	.rom_header_checksum_addr                                  = 0x4C,
	.rom_header_checksum_len                                   = SC501_FRONT_HEADER_CHECKSUM_LEN,

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

	.extend_cal_addr                                           = &front_sc501_extend_cal_addr,
};

#endif /* IS_OTPROM_FRONT_SC501_V001_H */
