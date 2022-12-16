#include "imgsensor_eeprom_rear2_imx355_v008.h"

#define REAR2_IMX355_MAX_CAL_SIZE               (0x6C9 + 0x1)

#define REAR2_IMX355_HEADER_CHECKSUM_LEN        (0x004B - 0x0000 + 0x1)
#define REAR2_IMX355_MODULE_CAL_CHECKSUM_LEN    (0x0485 - 0x0050 + 0x1)

#define REAR2_IMX355_CONVERTED_MAX_CAL_SIZE     (0x0A17 + 0x1)
#define REAR2_IMX355_CONVERTED_AWB_CHECKSUM_LEN (0x0073 - 0x0068 + 0x1)
#define REAR2_IMX355_CONVERTED_LSC_CHECKSUM_LEN (0x07D3 - 0x0078 + 0x1)

const struct rom_converted_cal_addr rear2_imx355_converted_cal_addr = {
	.rom_awb_cal_data_start_addr            = 0x0068,
	.rom_awb_checksum_addr                  = 0x0074,
	.rom_awb_checksum_len                   = (REAR2_IMX355_CONVERTED_AWB_CHECKSUM_LEN),
	.rom_shading_cal_data_start_addr        = 0x0078,
	.rom_shading_checksum_addr              = 0x07D4,
	.rom_shading_checksum_len               = (REAR2_IMX355_CONVERTED_LSC_CHECKSUM_LEN),
};

const struct imgsensor_vendor_rom_addr rear2_imx355_cal_addr = {
	/* Set '-1' if not used */
	.camera_module_es_version               = 'A',
	.cal_map_es_version                     = '1',
	.rom_max_cal_size                       = REAR2_IMX355_MAX_CAL_SIZE,
	.rom_header_cal_data_start_addr         = 0x00,
	.rom_header_main_module_info_start_addr = 0x0A,
	.rom_header_cal_map_ver_start_addr      = 0x16,
	.rom_header_project_name_start_addr     = 0x1E,
	.rom_header_module_id_addr              = 0x2E,
	.rom_header_main_sensor_id_addr         = 0x38,

	.rom_header_sub_module_info_start_addr  = -1,
	.rom_header_sub_sensor_id_addr          = -1,

	.rom_header_main_header_start_addr      = -1,
	.rom_header_main_header_end_addr        = -1,
	.rom_header_main_oem_start_addr         = -1,
	.rom_header_main_oem_end_addr           = -1,
	.rom_header_main_awb_start_addr         = 0x00,
	.rom_header_main_awb_end_addr           = 0x04,
	.rom_header_main_shading_start_addr     = -1,
	.rom_header_main_shading_end_addr       = -1,
	.rom_header_main_sensor_cal_start_addr  = -1,
	.rom_header_main_sensor_cal_end_addr    = -1,
	.rom_header_dual_cal_start_addr         = -1,
	.rom_header_dual_cal_end_addr           = -1,
	.rom_header_pdaf_cal_start_addr         = -1,
	.rom_header_pdaf_cal_end_addr           = -1,
	.rom_header_ois_cal_start_addr          = -1,
	.rom_header_ois_cal_end_addr            = -1,

	.rom_header_sub_oem_start_addr          = -1,
	.rom_header_sub_oem_end_addr            = -1,
	.rom_header_sub_awb_start_addr          = -1,
	.rom_header_sub_awb_end_addr            = -1,
	.rom_header_sub_shading_start_addr      = -1,
	.rom_header_sub_shading_end_addr        = -1,

	.rom_header_main_mtf_data_addr          = 0x048C,
	.rom_header_sub_mtf_data_addr           = -1,

	.rom_header_checksum_addr               = 0x004C,
	.rom_header_checksum_len                = REAR2_IMX355_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr           = -1,
	.rom_oem_af_macro_position_addr         = -1,
	.rom_oem_module_info_start_addr         = -1,
	.rom_oem_checksum_addr                  = -1,
	.rom_oem_checksum_len                   = -1,

	.rom_module_cal_data_start_addr         = 0x0050,
	.rom_module_module_info_start_addr      = -1,
	.rom_module_checksum_addr               = 0x0486,
	.rom_module_checksum_len                = REAR2_IMX355_MODULE_CAL_CHECKSUM_LEN,

	.rom_awb_cal_data_start_addr            = 0x0068,
	.rom_awb_module_info_start_addr         = -1,
	.rom_awb_checksum_addr                  = -1,
	.rom_awb_checksum_len                   = -1,

	.rom_shading_cal_data_start_addr        = 0x007C,
	.rom_shading_module_info_start_addr     = -1,
	.rom_shading_checksum_addr              = -1,
	.rom_shading_checksum_len               = -1,

	.rom_sensor_cal_module_info_start_addr  = -1,
	.rom_sensor_cal_checksum_addr           = -1,
	.rom_sensor_cal_checksum_len            = -1,

	.rom_dual_module_info_start_addr        = -1,
	.rom_dual_checksum_addr                 = -1,
	.rom_dual_checksum_len                  = -1,

	.rom_pdaf_module_info_start_addr        = -1,
	.rom_pdaf_checksum_addr                 = -1,
	.rom_pdaf_checksum_len                  = -1,
	.rom_ois_checksum_addr                  = -1,
	.rom_ois_checksum_len                   = -1,

	.rom_sub_oem_af_inf_position_addr       = -1,
	.rom_sub_oem_af_macro_position_addr     = -1,
	.rom_sub_oem_module_info_start_addr     = -1,
	.rom_sub_oem_checksum_addr              = -1,
	.rom_sub_oem_checksum_len               = -1,

	.rom_sub_awb_module_info_start_addr     = -1,
	.rom_sub_awb_checksum_addr              = -1,
	.rom_sub_awb_checksum_len               = -1,

	.rom_sub_shading_module_info_start_addr = -1,
	.rom_sub_shading_checksum_addr          = -1,
	.rom_sub_shading_checksum_len           = -1,

	.rom_dual_cal_data2_start_addr          = -1,
	.rom_dual_cal_data2_size                = -1,
	.rom_dual_tilt_x_addr                   = -1,
	.rom_dual_tilt_y_addr                   = -1,
	.rom_dual_tilt_z_addr                   = -1,
	.rom_dual_tilt_sx_addr                  = -1,
	.rom_dual_tilt_sy_addr                  = -1,
	.rom_dual_tilt_range_addr               = -1,
	.rom_dual_tilt_max_err_addr             = -1,
	.rom_dual_tilt_avg_err_addr             = -1,
	.rom_dual_tilt_dll_version_addr         = -1,
	.rom_dual_tilt_dll_modelID_addr         = -1,
	.rom_dual_tilt_dll_modelID_size         = -1,
	.rom_dual_shift_x_addr                  = -1,
	.rom_dual_shift_y_addr                  = -1,


	.crosstalk_cal_addr                     = NULL,
	.sac_cal_addr                           = NULL,
	.extend_cal_addr                        = NULL,

	.converted_cal_addr                     = &rear2_imx355_converted_cal_addr,
	.rom_converted_max_cal_size             = REAR2_IMX355_CONVERTED_MAX_CAL_SIZE,

	.sensor_maker                           = "SONY",
	.sensor_name                            = "IMX355",
	.sub_sensor_maker                       = NULL,
	.sub_sensor_name                        = NULL,

	.bayerformat                            = -1,
};
