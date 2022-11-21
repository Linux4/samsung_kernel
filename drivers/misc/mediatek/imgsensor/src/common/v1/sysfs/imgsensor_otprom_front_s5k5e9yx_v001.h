#ifndef IMGSENSOR_OTPROM_FRONT_S5K5E9YX_V001_H
#define IMGSENSOR_OTPROM_FRONT_S5K5E9YX_V001_H

#define FRONT_S5K5E9YX_OTP_USED_CAL_SIZE        (0x100)
#define FRONT_S5K5E9YX_HEADER_CHECKSUM_LEN      (0x00AF - 0x0000 + 0x1)
#define FRONT_S5K5E9YX_AWB_CHECKSUM_LEN         (0x00EF - 0x00B4 + 0x1)

const struct imgsensor_vendor_rom_addr front_s5k5e9yx_cal_addr = {
	/* Set '-1' if not used */
	.camera_module_es_version                   = 'A',
	.cal_map_es_version                         = '1',
	.rom_max_cal_size                           = FRONT_S5K5E9YX_OTP_USED_CAL_SIZE,

	.rom_header_cal_data_start_addr             = 0x00,
	.rom_header_main_module_info_start_addr     = 0x10,
	.rom_header_cal_map_ver_start_addr          = 0x20,
	.rom_header_project_name_start_addr         = 0x28,
	.rom_header_module_id_addr                  = 0x7A,
	.rom_header_main_sensor_id_addr             = 0x84,

	.rom_header_sub_module_info_start_addr      = -1,
	.rom_header_sub_sensor_id_addr              = -1,

	.rom_header_main_header_start_addr          = -1,
	.rom_header_main_header_end_addr            = -1,
	.rom_header_main_oem_start_addr             = -1,
	.rom_header_main_oem_end_addr               = -1,
	.rom_header_main_awb_start_addr             = 0x00,
	.rom_header_main_awb_end_addr               = 0x04,
	.rom_header_main_shading_start_addr         = -1,
	.rom_header_main_shading_end_addr           = -1,
	.rom_header_main_sensor_cal_start_addr      = -1,
	.rom_header_main_sensor_cal_end_addr        = -1,
	.rom_header_dual_cal_start_addr             = -1,
	.rom_header_dual_cal_end_addr               = -1,
	.rom_header_pdaf_cal_start_addr             = -1,
	.rom_header_pdaf_cal_end_addr               = -1,

	.rom_header_sub_oem_start_addr              = -1,
	.rom_header_sub_oem_end_addr                = -1,
	.rom_header_sub_awb_start_addr              = -1,
	.rom_header_sub_awb_end_addr                = -1,
	.rom_header_sub_shading_start_addr          = -1,
	.rom_header_sub_shading_end_addr            = -1,

	.rom_header_main_mtf_data_addr              = -1,
	.rom_header_sub_mtf_data_addr               = -1,

	.rom_header_checksum_addr                   = 0xB0,
	.rom_header_checksum_len                    = FRONT_S5K5E9YX_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr               = -1,
	.rom_oem_af_macro_position_addr             = -1,
	
	.rom_oem_module_info_start_addr             = -1,
	.rom_oem_checksum_addr                      = -1,
	.rom_oem_checksum_len                       = -1,

	.rom_awb_cal_data_start_addr                = 0xB4,
	.rom_awb_module_info_start_addr             = 0xD4,
	.rom_awb_checksum_addr                      = 0xF0,
	.rom_awb_checksum_len                       = FRONT_S5K5E9YX_AWB_CHECKSUM_LEN,

	.rom_shading_module_info_start_addr         = -1,
	.rom_shading_checksum_addr                  = -1,
	.rom_shading_checksum_len                   = -1,

	.rom_sensor_cal_module_info_start_addr      = -1,
	.rom_sensor_cal_checksum_addr               = -1,
	.rom_sensor_cal_checksum_len                = -1,

	.rom_dual_module_info_start_addr            = -1,
	.rom_dual_checksum_addr                     = -1,
	.rom_dual_checksum_len                      = -1,

	.rom_pdaf_module_info_start_addr            = -1,
	.rom_pdaf_checksum_addr                     = -1,
	.rom_pdaf_checksum_len                      = -1,

	.rom_sub_oem_af_inf_position_addr           = -1,
	.rom_sub_oem_af_macro_position_addr         = -1,
	.rom_sub_oem_module_info_start_addr         = -1,
	.rom_sub_oem_checksum_addr                  = -1,
	.rom_sub_oem_checksum_len                   = -1,

	.rom_sub_awb_module_info_start_addr         = -1,
	.rom_sub_awb_checksum_addr                  = -1,
	.rom_sub_awb_checksum_len                   = -1,

	.rom_sub_shading_module_info_start_addr     = -1,
	.rom_sub_shading_checksum_addr              = -1,
	.rom_sub_shading_checksum_len               = -1,

	.rom_dual_cal_data2_start_addr              = -1,
	.rom_dual_cal_data2_size                    = -1,
	.rom_dual_tilt_x_addr                       = -1,
	.rom_dual_tilt_y_addr                       = -1,
	.rom_dual_tilt_z_addr                       = -1,
	.rom_dual_tilt_sx_addr                      = -1,
	.rom_dual_tilt_sy_addr                      = -1,
	.rom_dual_tilt_range_addr                   = -1,
	.rom_dual_tilt_max_err_addr                 = -1,
	.rom_dual_tilt_avg_err_addr                 = -1,
	.rom_dual_tilt_dll_version_addr             = -1,
	.rom_dual_shift_x_addr                      = -1,
	.rom_dual_shift_y_addr                      = -1,

	.extend_cal_addr                            = NULL,

	.sensor_maker                               = "SLSI",
	.sensor_name                                = "S5K5E9YX",
	.sub_sensor_maker                           = "",
	.sub_sensor_name                            = "",
};

#endif /* IMGSENSOR_OTPROM_FRONT_S5K5E9YX_V001_H */
