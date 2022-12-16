#include "imgsensor_eeprom_front_imx616_v007.h"

#define FRONT_IMX616_MAX_CAL_SIZE               (0x0E6F + 0x1)

#define FRONT_IMX616_HEADER_CHECKSUM_LEN        (0x007B - 0x0000 + 0x1)
#define FRONT_IMX616_MODULE_CAL_CHECKSUM_LEN    (0x0C2B - 0x0820 + 0x1)
#define FRONT_IMX616_SENSOR_CAL_CHECKSUM_LEN    (0x081B - 0x0080 + 0x1)

#define FRONT_IMX616_CONVERTED_MAX_CAL_SIZE     (0x11DF + 0x1)
#define FRONT_IMX616_CONVERTED_AWB_CHECKSUM_LEN (0x083B - 0x830 + 0x1)
#define FRONT_IMX616_CONVERTED_LSC_CHECKSUM_LEN (0x0F9B - 0x840 + 0x1)

const struct rom_converted_cal_addr front_imx616_converted_cal_addr = {
	.rom_awb_cal_data_start_addr            = 0x0820,
	.rom_awb_checksum_addr                  = 0x083C,
	.rom_awb_checksum_len                   = (FRONT_IMX616_CONVERTED_AWB_CHECKSUM_LEN),
	.rom_shading_cal_data_start_addr        = 0x0840,
	.rom_shading_checksum_addr              = 0x0F9C,
	.rom_shading_checksum_len               = (FRONT_IMX616_CONVERTED_LSC_CHECKSUM_LEN),
};

const struct rom_cal_addr front_imx616_lrc_addr = {
	.name                            = CROSSTALK_CAL_LRC,
	.addr                            = 0x0698,
	.size                            = (0x079B - 0x0698 + 1),
	.next                            = NULL,
};

const struct rom_cal_addr front_imx616_qsc_addr = {
	.name                            = CROSSTALK_CAL_QSC,
	.addr                            = 0x0080,
	.size                            = (0x0697 - 0x0080 + 1),
	.next                            = &front_imx616_lrc_addr,
};

const struct imgsensor_vendor_rom_addr front_imx616_cal_addr = {
	/* Set '-1' if not used */
	.camera_module_es_version               = 'A',
	.cal_map_es_version                     = '1',
	.rom_max_cal_size                       = FRONT_IMX616_MAX_CAL_SIZE,
	.rom_header_cal_data_start_addr         = 0x00,
	.rom_header_main_module_info_start_addr = 0x1E,
	.rom_header_cal_map_ver_start_addr      = 0x30,
	.rom_header_project_name_start_addr     = 0x38,
	.rom_header_module_id_addr              = 0x56,
	.rom_header_main_sensor_id_addr         = 0x60,

	.rom_header_sub_module_info_start_addr  = -1,
	.rom_header_sub_sensor_id_addr          = -1,

	.rom_header_main_header_start_addr      = 0x00,
	.rom_header_main_header_end_addr        = 0x04,
	.rom_header_main_oem_start_addr         = -1,
	.rom_header_main_oem_end_addr           = -1,
	.rom_header_main_awb_start_addr         = 0x10,
	.rom_header_main_awb_end_addr           = 0x14,
	.rom_header_main_shading_start_addr     = -1,
	.rom_header_main_shading_end_addr       = -1,
	.rom_header_main_sensor_cal_start_addr  = 0x08,
	.rom_header_main_sensor_cal_end_addr    = 0x0C,
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

	.rom_header_main_mtf_data_addr          = 0x0C32,
	.rom_header_sub_mtf_data_addr           = -1,

	.rom_header_checksum_addr               = 0x007C,
	.rom_header_checksum_len                = FRONT_IMX616_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr           = -1,
	.rom_oem_af_macro_position_addr         = -1,
	.rom_oem_module_info_start_addr         = -1,
	.rom_oem_checksum_addr                  = -1,
	.rom_oem_checksum_len                   = -1,

	.rom_module_cal_data_start_addr         = 0x0820,
	.rom_module_module_info_start_addr      = -1,
	.rom_module_checksum_addr               = 0x0C2C,
	.rom_module_checksum_len                = FRONT_IMX616_MODULE_CAL_CHECKSUM_LEN,

	.rom_awb_cal_data_start_addr            = 0x0820,
	.rom_awb_module_info_start_addr         = -1,
	.rom_awb_checksum_addr                  = -1,
	.rom_awb_checksum_len                   = -1,

	.rom_shading_cal_data_start_addr        = 0x0830,
	.rom_shading_module_info_start_addr     = -1,
	.rom_shading_checksum_addr              = -1,
	.rom_shading_checksum_len               = -1,

	.rom_sensor_cal_module_info_start_addr  = -1,
	.rom_sensor_cal_checksum_addr           = 0x081C,
	.rom_sensor_cal_checksum_len            = FRONT_IMX616_SENSOR_CAL_CHECKSUM_LEN,

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


	.crosstalk_cal_addr                     = &front_imx616_qsc_addr,
	.sac_cal_addr                           = NULL,
	.extend_cal_addr                        = NULL,

	.converted_cal_addr                     = &front_imx616_converted_cal_addr,
	.rom_converted_max_cal_size             = FRONT_IMX616_CONVERTED_MAX_CAL_SIZE,

	.sensor_maker                           = "SONY",
	.sensor_name                            = "IMX616",
	.sub_sensor_maker                       = NULL,
	.sub_sensor_name                        = NULL,

	.bayerformat                            = -1,
};
