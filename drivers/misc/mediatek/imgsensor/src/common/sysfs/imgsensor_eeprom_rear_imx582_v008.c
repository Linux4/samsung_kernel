#include "imgsensor_eeprom_rear_imx582_v008.h"

#define REAR_IMX582_MAX_CAL_SIZE               (0x2BAF + 0x1)

#define REAR_IMX582_HEADER_CHECKSUM_LEN        (0x00FB + 0x1)
#define REAR_IMX582_OEM_CHECKSUM_LEN           (0x1D5B - 0x1CE0 + 0x1)
#define REAR_IMX582_AWB_CHECKSUM_LEN           (0x258B - 0x2570 + 0x1)
#define REAR_IMX582_SHADING_CHECKSUM_LEN       (0x298B - 0x2590 + 0x1)
#define REAR_IMX582_SENSOR_CAL_CHECKSUM_LEN    (0x103B - 0x01D0 + 0x1)
#define REAR_IMX582_OIS_CHECKSUM_LEN           (0x01CB - 0x0170 + 0x1)
#define REAR_IMX582_PDAF_CHECKSUM_LEN          (0x256B - 0x1D60 + 0x1)

#define REAR_IMX582_CONVERTED_MAX_CAL_SIZE     (0x2EFF + 0x1)
#define REAR_IMX582_CONVERTED_AWB_CHECKSUM_LEN (0x257B - 0x2570 + 0x1)
#define REAR_IMX582_CONVERTED_LSC_CHECKSUM_LEN (0x2CDB - 0x2580 + 0x1)

const struct rom_converted_cal_addr rear_imx582_converted_cal_addr = {
	.rom_awb_cal_data_start_addr            = 0x2570,
	.rom_awb_checksum_addr                  = 0x257C,
	.rom_awb_checksum_len                   = (REAR_IMX582_CONVERTED_AWB_CHECKSUM_LEN),
	.rom_shading_cal_data_start_addr        = 0x2580,
	.rom_shading_checksum_addr              = 0x2CDC,
	.rom_shading_checksum_len               = (REAR_IMX582_CONVERTED_LSC_CHECKSUM_LEN),
};

const struct rom_sac_cal_addr rear_imx582_sac_addr = {
	.rom_mode_addr                          = 0x1D14,
	.rom_time_addr                          = 0x1D15,
};

const struct rom_cal_addr rear_imx582_ois_addr = {
	.addr                                   = 0x0170,
	.size                                   = (0x01BF - 0x0170 + 0x1),
	.next                                   = NULL,
};

const struct rom_cal_addr rear_imx582_qsc_addr = {
	.name                                   = CROSSTALK_CAL_QSC,
	.addr                                   = 0x0350,
	.size                                   = 2304,
	.next                                   = NULL,
};

const struct rom_cal_addr rear_imx582_lrc_addr = {
	.name                                   = CROSSTALK_CAL_LRC,
	.addr                                   = 0x01D0,
	.size                                   = 384,
	.next                                   = &rear_imx582_qsc_addr,
};

const struct imgsensor_vendor_rom_addr rear_imx582_cal_addr = {
	/* Set '-1' if not used */
	.camera_module_es_version               = 'A',
	.cal_map_es_version                     = '1',
	.rom_max_cal_size                       = REAR_IMX582_MAX_CAL_SIZE,
	.rom_header_cal_data_start_addr         = 0x00,
	.rom_header_main_module_info_start_addr = 0x6E,
	.rom_header_cal_map_ver_start_addr      = 0x90,
	.rom_header_project_name_start_addr     = 0x98,
	.rom_header_module_id_addr              = 0xAE,
	.rom_header_main_sensor_id_addr         = 0xB8,

	.rom_header_sub_module_info_start_addr  = -1,
	.rom_header_sub_sensor_id_addr          = -1,

	.rom_header_main_header_start_addr      = 0x00,
	.rom_header_main_header_end_addr        = 0x04,
	.rom_header_main_oem_start_addr         = 0x40,
	.rom_header_main_oem_end_addr           = 0x44,
	.rom_header_main_awb_start_addr         = 0x50,
	.rom_header_main_awb_end_addr           = 0x54,
	.rom_header_main_shading_start_addr     = 0x58,
	.rom_header_main_shading_end_addr       = 0x5C,
	.rom_header_main_sensor_cal_start_addr  = 0x18,
	.rom_header_main_sensor_cal_end_addr    = 0x1C,
	.rom_header_dual_cal_start_addr         = -1,
	.rom_header_dual_cal_end_addr           = -1,
	.rom_header_pdaf_cal_start_addr         = 0x48,
	.rom_header_pdaf_cal_end_addr           = 0x4C,
	.rom_header_ois_cal_start_addr          = 0x10,
	.rom_header_ois_cal_end_addr            = 0x14,

	.rom_header_sub_oem_start_addr          = -1,
	.rom_header_sub_oem_end_addr            = -1,
	.rom_header_sub_awb_start_addr          = -1,
	.rom_header_sub_awb_end_addr            = -1,
	.rom_header_sub_shading_start_addr      = -1,
	.rom_header_sub_shading_end_addr        = -1,

	.rom_header_main_mtf_data_addr          = 0x1D18,
	.rom_header_sub_mtf_data_addr           = -1,

	.rom_header_checksum_addr               = 0xFC,
	.rom_header_checksum_len                = REAR_IMX582_HEADER_CHECKSUM_LEN,

	.rom_oem_af_inf_position_addr           = 0x1CEC,
	.rom_oem_af_macro_position_addr         = 0x1CF8,
	.rom_oem_module_info_start_addr         = -1,
	.rom_oem_checksum_addr                  = 0x1D5C,
	.rom_oem_checksum_len                   = REAR_IMX582_OEM_CHECKSUM_LEN,

	.rom_module_cal_data_start_addr         = -1,
	.rom_module_module_info_start_addr      = -1,
	.rom_module_checksum_addr               = -1,
	.rom_module_checksum_len                = -1,

	.rom_awb_cal_data_start_addr            = 0x2570,
	.rom_awb_module_info_start_addr         = -1,
	.rom_awb_checksum_addr                  = 0x258C,
	.rom_awb_checksum_len                   = REAR_IMX582_AWB_CHECKSUM_LEN,

	.rom_shading_cal_data_start_addr        = 0x2590,
	.rom_shading_module_info_start_addr     = -1,
	.rom_shading_checksum_addr              = 0x298C,
	.rom_shading_checksum_len               = REAR_IMX582_SHADING_CHECKSUM_LEN,

	.rom_sensor_cal_module_info_start_addr  = -1,
	.rom_sensor_cal_checksum_addr           = 0x103C,
	.rom_sensor_cal_checksum_len            = REAR_IMX582_SENSOR_CAL_CHECKSUM_LEN,

	.rom_dual_module_info_start_addr        = -1,
	.rom_dual_checksum_addr                 = -1,
	.rom_dual_checksum_len                  = -1,

	.rom_pdaf_module_info_start_addr        = -1,
	.rom_pdaf_checksum_addr                 = 0x256C,
	.rom_pdaf_checksum_len                  = REAR_IMX582_PDAF_CHECKSUM_LEN,

	.rom_ois_checksum_addr                  = 0x01CC,
	.rom_ois_checksum_len                   = REAR_IMX582_OIS_CHECKSUM_LEN,

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

	.ois_cal_addr                           = &rear_imx582_ois_addr,
	.sac_cal_addr                           = &rear_imx582_sac_addr,
	.crosstalk_cal_addr                     = &rear_imx582_lrc_addr,
	.extend_cal_addr                        = NULL,

	.converted_cal_addr                     = &rear_imx582_converted_cal_addr,
	.rom_converted_max_cal_size             = REAR_IMX582_CONVERTED_MAX_CAL_SIZE,

	.sensor_maker                           = "SONY",
	.sensor_name                            = "IMX582",
	.sub_sensor_maker                       = NULL,
	.sub_sensor_name                        = NULL,

	.bayerformat                            = -1,
};
