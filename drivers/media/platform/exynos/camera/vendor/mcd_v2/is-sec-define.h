/*
 * Samsung Exynos5 SoC series IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_SEC_DEFINE_H
#define IS_SEC_DEFINE_H

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/zlib.h>

#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"

#include "is-device-sensor.h"
#include "is-device-ischain.h"
#include "crc32.h"
#include "is-device-from.h"
#include "is-dt.h"
#include "is-device-ois.h"

#define FW_CORE_VER		0
#define FW_PIXEL_SIZE		1
#define FW_ISP_COMPANY		3
#define FW_SENSOR_MAKER		4
#define FW_PUB_YEAR		5
#define FW_PUB_MON		6
#define FW_PUB_NUM		7
#define FW_MODULE_COMPANY	9
#define FW_VERSION_INFO		10

#define FW_ISP_COMPANY_BROADCOMM	'B'
#define FW_ISP_COMPANY_TN		'C'
#define FW_ISP_COMPANY_FUJITSU		'F'
#define FW_ISP_COMPANY_INTEL		'I'
#define FW_ISP_COMPANY_LSI		'L'
#define FW_ISP_COMPANY_MARVELL		'M'
#define FW_ISP_COMPANY_QUALCOMM		'Q'
#define FW_ISP_COMPANY_RENESAS		'R'
#define FW_ISP_COMPANY_STE		'S'
#define FW_ISP_COMPANY_TI		'T'
#define FW_ISP_COMPANY_DMC		'D'
#ifdef CONFIG_SEC_CAL_ENABLE
#define FW_ISP_COMPANY_LSI_STANDARD_CAL		'E'
#endif

#define FW_SENSOR_MAKER_SF		'F'
#define FW_SENSOR_MAKER_SLSI		'L'			/* rear_front*/
#define FW_SENSOR_MAKER_SONY		'S'			/* rear_front*/
#define FW_SENSOR_MAKER_SLSI_SONY		'X'		/* rear_front*/
#define FW_SENSOR_MAKER_SONY_LSI		'Y'		/* rear_front*/

#define FW_SENSOR_MAKER_SLSI		'L'

#define FW_MODULE_COMPANY_SEMCO		'S'
#define FW_MODULE_COMPANY_GUMI		'O'
#define FW_MODULE_COMPANY_CAMSYS	'C'
#define FW_MODULE_COMPANY_PATRON	'P'
#define FW_MODULE_COMPANY_MCNEX		'M'
#define FW_MODULE_COMPANY_LITEON	'L'
#define FW_MODULE_COMPANY_VIETNAM	'V'
#define FW_MODULE_COMPANY_SHARP		'J'
#define FW_MODULE_COMPANY_NAMUGA	'N'
#define FW_MODULE_COMPANY_POWER_LOGIX	'A'
#define FW_MODULE_COMPANY_DI		'D'


#define FW_IMX576			"B24LS" /*IMX576 REAR*/
#define FW_IMX576_C			"C24LS" /*IMX576 FRONT*/
#define FW_IMX616_S			"K32LS"		/* RST8 */
#define FW_IMX582			"K48LS" /*IMX582 REAR*/
#define FW_IMX586			"K48LS" /*IMX586 REAR*/

#define FW_SR846			"B08LF" /*SR846 REAR*/
#define FW_SR846_P			"P08LF" /*SR846 FRONT*/

#define SDCARD_FW
/*#define IS_FW_SDCARD			"/data/media/0/is_fw.bin" */

#define IS_DDK				"is_lib.bin"
#define IS_RTA				"is_rta.bin"
#define IS_4HA_SETF			"setfile_4ha.bin"
#define IS_4HA_FRONT_SETF		"setfile_4ha_front.bin"
#define IS_GM2_SETF			"setfile_gm2.bin"
#define IS_GW1_SETF			"setfile_gw1.bin"

#define IS_IMX258_SETF			"setfile_imx258.bin"
#define IS_IMX576_SETF			"setfile_imx576.bin"
#define IS_IMX576_FRONT_SETF	"setfile_imx576_front.bin"
#define IS_IMX582_SETF			"setfile_imx582.bin"
#define IS_IMX586_SETF			"setfile_imx586.bin"
#define IS_IMX616_SETF			"setfile_imx616.bin"
#define IS_IMX686_SETF			"setfile_imx686.bin"

#define IS_SR556_SETF			"setfile_sr556.bin"
#define IS_SR846_SETF			"setfile_sr846.bin"
#define IS_SR846_FRONT_SETF	"setfile_sr846_front.bin"

#define IS_GC5035_SETF			"setfile_gc5035.bin"
#define IS_GC5035_MACRO_SETF		"setfile_gc5035_macro.bin"
#define IS_HI1336_SETF			"setfile_hi1336.bin"
#define IS_GC02M1_SETF			"setfile_gc02m1.bin"
#define IS_GC02M1_MACRO_SETF		"setfile_gc02m1_macro.bin"

#define IS_CAL_SDCARD_FRONT		"/data/cal_data_front.bin"
#define IS_FW_FROM_SDCARD		"/data/media/0/CamFW_Main.bin"
#define IS_SETFILE_FROM_SDCARD		"/data/media/0/CamSetfile_Main.bin"
#define IS_KEY_FROM_SDCARD		"/data/media/0/1q2w3e4r.key"

#define IS_HEADER_VER_SIZE      11
#define IS_HEADER_VER_OFFSET    (IS_HEADER_VER_SIZE + IS_SIGNATURE_LEN)
#define IS_CAM_VER_SIZE         11
#define IS_OEM_VER_SIZE         11
#define IS_AWB_VER_SIZE         11
#define IS_SHADING_VER_SIZE     11
#define IS_CAL_MAP_VER_SIZE     4
#define IS_PROJECT_NAME_SIZE    8
#define IS_ISP_SETFILE_VER_SIZE 6
#define IS_SENSOR_ID_SIZE       16
#define IS_CAL_DLL_VERSION_SIZE 4
#define IS_MODULE_ID_SIZE       10
#define IS_RESOLUTION_DATA_SIZE 54
#define IS_AWB_MASTER_DATA_SIZE 8
#define IS_AWB_MODULE_DATA_SIZE 8
#define IS_OIS_GYRO_DATA_SIZE 4
#define IS_OIS_COEF_DATA_SIZE 2
#define IS_OIS_SUPPERSSION_RATIO_DATA_SIZE 2
#define IS_OIS_CAL_MARK_DATA_SIZE 1

#define IS_PAF_OFFSET_MID_OFFSET		(0x0730) /* REAR PAF OFFSET MID (30CM, WIDE) */
#define IS_PAF_OFFSET_MID_SIZE			936
#define IS_PAF_OFFSET_PAN_OFFSET		(0x0CD0) /* REAR PAF OFFSET FAR (1M, WIDE) */
#define IS_PAF_OFFSET_PAN_SIZE			234
#define IS_PAF_CAL_ERR_CHECK_OFFSET	0x14
#define IS_ROM_CRC_MAX_LIST 150
#define IS_ROM_DUAL_TILT_MAX_LIST 10
#define IS_DUAL_TILT_PROJECT_NAME_SIZE 20
#define IS_ROM_OIS_MAX_LIST 14
#define IS_ROM_OIS_SINGLE_MODULE_MAX_LIST 7

#define IS_MAX_TUNNING_BUFFER_SIZE (15 * 1024)

#define GC5035_OTP_PAGE_ADDR                         0xfe
#define GC5035_OTP_MODE_ADDR                         0xf3
#define GC5035_OTP_BUSY_ADDR                         0x6f
#define GC5035_OTP_PAGE                              0x02
#define GC5035_OTP_ACCESS_ADDR_HIGH                  0x69

#define GC5035_OTP_ACCESS_ADDR_LOW                   0x6a
#define GC5035_OTP_READ_ADDR                         0x6c
#define GC5035_BANK_SELECT_ADDR                      0x1000

#define GC5035_BOKEH_OTP_START_ADDR_BANK1            0x1080
#define GC5035_BOKEH_OTP_START_ADDR_BANK2            0x1480
#define GC5035_BOKEH_OTP_USED_CAL_SIZE               (0x6F + 0x1)

#define GC5035_UW_OTP_START_ADDR_BANK1               0x1020
#define GC5035_UW_OTP_START_ADDR_BANK2               0x17C0
#define GC5035_UW_OTP_USED_CAL_SIZE                  (0xF3 + 0x1)

#define GC5035_MACRO_OTP_START_ADDR_BANK1            0x1020
#define GC5035_MACRO_OTP_START_ADDR_BANK2            0x17C0
#define GC5035_MACRO_OTP_USED_CAL_SIZE               (0xF3 + 0x1)

#define GC02M1_OTP_PAGE_ADDR                         0xfe
#define GC02M1_OTP_MODE_ADDR                         0xf3
#define GC02M1_OTP_PAGE                              0x02
#define GC02M1_OTP_ACCESS_ADDR                       0x17
#define GC02M1_OTP_READ_ADDR                         0x19

#define GC02M1_OTP_START_ADDR                        0x0078
#define GC02M1_OTP_USED_CAL_SIZE                     0x11 	//((0x00FF - 0x0078 + 0x1) / 8)

/*********S5K4HA OTP*********/
#define S5K4HA_STANDBY_ADDR                         0x0136
#define S5K4HA_OTP_R_W_MODE_ADDR                    0x0A00
#define S5K4HA_OTP_CHECK_ADDR                       0x0A01
#define S5K4HA_OTP_PAGE_SELECT_ADDR                 0x0A02
#define S5K4HA_OTP_USED_CAL_SIZE                    (0x0A43 + 0x0440 - 0x0A08 + 0x1)
#define S5K4HA_OTP_PAGE_ADDR_L                      0x0A04
#define S5K4HA_OTP_PAGE_ADDR_H                      0x0A43
#define S5K4HA_OTP_BANK_SELECT                      0x0A04
#define S5K4HA_OTP_START_PAGE_BANK1                 0x11
#define S5K4HA_OTP_START_PAGE_BANK2                 0x23

#define S5K4HA_OTP_START_ADDR                       0x0A08

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
/*******************************/

#define FROM_VERSION_V002 '2'
#define FROM_VERSION_V003 '3'
#define FROM_VERSION_V004 '4'
#define FROM_VERSION_V005 '5'

/* #define DEBUG_FORCE_DUMP_ENABLE */

/* #define CONFIG_RELOAD_CAL_DATA 1 */

enum {
        CC_BIN1 = 0,
        CC_BIN2,
        CC_BIN3,
        CC_BIN4,
        CC_BIN5,
        CC_BIN6,
        CC_BIN7,
        CC_BIN_MAX,
};

enum fnumber_index {
	FNUMBER_1ST = 0,
	FNUMBER_2ND,
	FNUMBER_3RD,
	FNUMBER_MAX
};

enum is_rom_state {
	IS_ROM_STATE_FINAL_MODULE,
	IS_ROM_STATE_LATEST_MODULE,
	IS_ROM_STATE_INVALID_ROM_VERSION,
	IS_ROM_STATE_OTHER_VENDOR,	/* Q module */
	IS_ROM_STATE_CAL_RELOAD,
	IS_ROM_STATE_CAL_READ_DONE,
	IS_ROM_STATE_FW_FIND_DONE,
	IS_ROM_STATE_CHECK_BIN_DONE,
	IS_ROM_STATE_SKIP_CAL_LOADING,
	IS_ROM_STATE_SKIP_CRC_CHECK,
	IS_ROM_STATE_SKIP_HEADER_LOADING,
	IS_ROM_STATE_MAX
};

enum is_crc_error {
	IS_CRC_ERROR_HEADER,
	IS_CRC_ERROR_ALL_SECTION,
	IS_CRC_ERROR_DUAL_CAMERA,
	IS_CRC_ERROR_FIRMWARE,
	IS_CRC_ERROR_SETFILE_1,
	IS_CRC_ERROR_SETFILE_2,
	IS_CRC_ERROR_HIFI_TUNNING,
	IS_ROM_ERROR_MAX
};

#ifdef CONFIG_SEC_CAL_ENABLE
struct rom_standard_cal_data {
	int32_t		rom_standard_cal_start_addr;
	int32_t		rom_standard_cal_end_addr;
	int32_t		rom_standard_cal_module_crc_addr;
	int32_t		rom_standard_cal_module_checksum_len;
	int32_t		rom_standard_cal_sec2lsi_end_addr;

	int32_t		rom_header_standard_cal_end_addr;
	int32_t		rom_header_main_shading_end_addr;

	int32_t		rom_awb_start_addr;
	int32_t		rom_awb_end_addr;
	int32_t		rom_awb_section_crc_addr;

	int32_t		rom_shading_start_addr;
	int32_t		rom_shading_end_addr;
	int32_t		rom_shading_section_crc_addr;

	int32_t		rom_factory_start_addr;
	int32_t		rom_factory_end_addr;

	int32_t		rom_awb_sec2lsi_start_addr;
	int32_t		rom_awb_sec2lsi_end_addr;
	int32_t		rom_awb_sec2lsi_checksum_addr;
	int32_t		rom_awb_sec2lsi_checksum_len;

	int32_t		rom_shading_sec2lsi_start_addr;
	int32_t		rom_shading_sec2lsi_end_addr;
	int32_t		rom_shading_sec2lsi_checksum_addr;
	int32_t		rom_shading_sec2lsi_checksum_len;

	int32_t		rom_factory_sec2lsi_start_addr;

#if defined (CONFIG_CAMERA_EEPROM_DUALIZED)
	int32_t		rom_dualized_standard_cal_start_addr;
	int32_t		rom_dualized_standard_cal_end_addr;
	int32_t		rom_dualized_standard_cal_module_crc_addr;
	int32_t		rom_dualized_standard_cal_module_checksum_len;
	int32_t		rom_dualized_standard_cal_sec2lsi_end_addr;

	int32_t		rom_dualized_header_standard_cal_end_addr;
	int32_t		rom_dualized_header_main_shading_end_addr;

	int32_t		rom_dualized_awb_start_addr;
	int32_t		rom_dualized_awb_end_addr;
	int32_t		rom_dualized_awb_section_crc_addr;

	int32_t		rom_dualized_shading_start_addr;
	int32_t		rom_dualized_shading_end_addr;
	int32_t		rom_dualized_shading_section_crc_addr;

	int32_t		rom_dualized_factory_start_addr;
	int32_t		rom_dualized_factory_end_addr;

	int32_t		rom_dualized_awb_sec2lsi_start_addr;
	int32_t		rom_dualized_awb_sec2lsi_end_addr;
	int32_t		rom_dualized_awb_sec2lsi_checksum_addr;
	int32_t		rom_dualized_awb_sec2lsi_checksum_len;

	int32_t		rom_dualized_shading_sec2lsi_start_addr;
	int32_t		rom_dualized_shading_sec2lsi_end_addr;
	int32_t		rom_dualized_shading_sec2lsi_checksum_addr;
	int32_t		rom_dualized_shading_sec2lsi_checksum_len;

	int32_t		rom_dualized_factory_sec2lsi_start_addr;
#endif
};

bool is_sec_readcal_dump_post_sec2lsi(struct is_core *core, char *buf, int position);
#ifdef USES_STANDARD_CAL_RELOAD
bool is_sec_sec2lsi_check_cal_reload(void);
#endif
#endif

struct is_rom_info {
	unsigned long	rom_state;
	unsigned long	crc_error;

	/* set by dts parsing */
	u32		rom_power_position;
	u32		rom_size;

	char	camera_module_es_version;
	u32		cal_map_es_version;

	u32		header_crc_check_list[IS_ROM_CRC_MAX_LIST];
	u32		header_crc_check_list_len;
	u32		crc_check_list[IS_ROM_CRC_MAX_LIST];
	u32		crc_check_list_len;
	u32		dual_crc_check_list[IS_ROM_CRC_MAX_LIST];
	u32		dual_crc_check_list_len;
	u32		rom_dualcal_slave0_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualcal_slave0_tilt_list_len;
	u32		rom_dualcal_slave1_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualcal_slave1_tilt_list_len;
	u32		rom_dualcal_slave2_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualcal_slave2_tilt_list_len;
	u32		rom_dualcal_slave3_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualcal_slave3_tilt_list_len;
	u32		rom_ois_list[IS_ROM_OIS_MAX_LIST];
	u32		rom_ois_list_len;

	/* set -1 if not support */
	int32_t		rom_header_cal_data_start_addr;
	int32_t		rom_header_version_start_addr;
	int32_t		rom_header_cal_map_ver_start_addr;
	int32_t		rom_header_isp_setfile_ver_start_addr;
	int32_t		rom_header_project_name_start_addr;
	int32_t		rom_header_module_id_addr;
	int32_t		rom_header_sensor_id_addr;
	int32_t		rom_header_mtf_data_addr;
	int32_t		rom_header_f2_mtf_data_addr;
	int32_t		rom_header_f3_mtf_data_addr;
	int32_t		rom_awb_master_addr;
	int32_t		rom_awb_module_addr;
	int32_t		rom_af_cal_addr[AF_CAL_MAX];
	int32_t		rom_af_cal_addr_len;
	int32_t		rom_paf_cal_start_addr;
#ifdef CONFIG_SEC_CAL_ENABLE
	/* standard cal */
	bool		use_standard_cal;
	struct rom_standard_cal_data standard_cal_data;
	struct rom_standard_cal_data backup_standard_cal_data;
#endif

	int32_t		rom_header_sensor2_id_addr;
	int32_t		rom_header_sensor2_version_start_addr;
	int32_t		rom_header_sensor2_mtf_data_addr;
	int32_t		rom_sensor2_awb_master_addr;
	int32_t		rom_sensor2_awb_module_addr;
	int32_t		rom_sensor2_af_cal_addr[AF_CAL_MAX];
	int32_t		rom_sensor2_af_cal_addr_len;
	int32_t		rom_sensor2_paf_cal_start_addr;

	int32_t		rom_dualcal_slave0_start_addr;
	int32_t		rom_dualcal_slave0_size;
	int32_t		rom_dualcal_slave1_start_addr;
	int32_t		rom_dualcal_slave1_size;
	int32_t		rom_dualcal_slave2_start_addr;
	int32_t		rom_dualcal_slave2_size;

	int32_t		rom_pdxtc_cal_data_start_addr;
	int32_t		rom_pdxtc_cal_data_0_size;
	int32_t		rom_pdxtc_cal_data_1_size;

	int32_t		rom_spdc_cal_data_start_addr;
	int32_t		rom_spdc_cal_data_size;

	int32_t		rom_xtc_cal_data_start_addr;
	int32_t		rom_xtc_cal_data_size;

	bool	rom_pdxtc_cal_endian_check;
	u32		rom_pdxtc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32		rom_pdxtc_cal_data_addr_list_len;
	bool	rom_gcc_cal_endian_check;
	u32		rom_gcc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32		rom_gcc_cal_data_addr_list_len;
	bool	rom_xtc_cal_endian_check;
	u32		rom_xtc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32		rom_xtc_cal_data_addr_list_len;

	u16		rom_dualcal_slave1_cropshift_x_addr;
	u16		rom_dualcal_slave1_cropshift_y_addr;

	u16		rom_dualcal_slave1_oisshift_x_addr;
	u16		rom_dualcal_slave1_oisshift_y_addr;

	u16		rom_dualcal_slave1_dummy_flag_addr;

#if defined (CONFIG_CAMERA_EEPROM_DUALIZED)
	bool	is_read_dualized_values;
	u32		header_dualized_crc_check_list[IS_ROM_CRC_MAX_LIST];
	u32		header_dualized_crc_check_list_len;
	u32		crc_dualized_check_list[IS_ROM_CRC_MAX_LIST];
	u32		crc_dualized_check_list_len;
	u32		dual_crc_dualized_check_list[IS_ROM_CRC_MAX_LIST];
	u32		dual_crc_dualized_check_list_len;
	u32		rom_dualized_dualcal_slave0_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualized_dualcal_slave0_tilt_list_len;
	u32		rom_dualized_dualcal_slave1_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualized_dualcal_slave1_tilt_list_len;
	u32		rom_dualized_dualcal_slave2_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualized_dualcal_slave2_tilt_list_len;
	u32		rom_dualized_dualcal_slave3_tilt_list[IS_ROM_DUAL_TILT_MAX_LIST];
	u32		rom_dualized_dualcal_slave3_tilt_list_len;
	u32		rom_dualized_ois_list[IS_ROM_OIS_MAX_LIST];
	u32		rom_dualized_ois_list_len;

	/* set -1 if not support */
	int32_t		rom_dualized_header_cal_data_start_addr;
	int32_t		rom_dualized_header_version_start_addr;
	int32_t		rom_dualized_header_cal_map_ver_start_addr;
	int32_t		rom_dualized_header_isp_setfile_ver_start_addr;
	int32_t		rom_dualized_header_project_name_start_addr;
	int32_t		rom_dualized_header_module_id_addr;
	int32_t		rom_dualized_header_sensor_id_addr;
	int32_t		rom_dualized_header_mtf_data_addr;
	int32_t		rom_dualized_header_f2_mtf_data_addr;
	int32_t		rom_dualized_header_f3_mtf_data_addr;
	int32_t		rom_dualized_awb_master_addr;
	int32_t		rom_dualized_awb_module_addr;
	int32_t		rom_dualized_af_cal_addr[AF_CAL_MAX];
	int32_t		rom_dualized_af_cal_addr_len;
	int32_t		rom_dualized_paf_cal_start_addr;
#ifdef CONFIG_SEC_CAL_ENABLE
	/* standard cal */
	bool		use_dualized_standard_cal;
	struct rom_standard_cal_data standard_dualized_cal_data;
	struct rom_standard_cal_data backup_standard_dualized_cal_data;
#endif

	int32_t		rom_dualized_header_sensor2_id_addr;
	int32_t		rom_dualized_header_sensor2_version_start_addr;
	int32_t		rom_dualized_header_sensor2_mtf_data_addr;
	int32_t		rom_dualized_sensor2_awb_master_addr;
	int32_t		rom_dualized_sensor2_awb_module_addr;
	int32_t		rom_dualized_sensor2_af_cal_addr[AF_CAL_MAX];
	int32_t		rom_dualized_sensor2_af_cal_addr_len;
	int32_t		rom_dualized_sensor2_paf_cal_start_addr;

	int32_t		rom_dualized_dualcal_slave0_start_addr;
	int32_t		rom_dualized_dualcal_slave0_size;
	int32_t		rom_dualized_dualcal_slave1_start_addr;
	int32_t		rom_dualized_dualcal_slave1_size;
	int32_t		rom_dualized_dualcal_slave2_start_addr;
	int32_t		rom_dualized_dualcal_slave2_size;

	int32_t		rom_dualized_pdxtc_cal_data_start_addr;
	int32_t		rom_dualized_pdxtc_cal_data_0_size;
	int32_t		rom_dualized_pdxtc_cal_data_1_size;

	int32_t		rom_dualized_spdc_cal_data_start_addr;
	int32_t		rom_dualized_spdc_cal_data_size;

	int32_t		rom_dualized_xtc_cal_data_start_addr;
	int32_t		rom_dualized_xtc_cal_data_size;

	bool	rom_dualized_pdxtc_cal_endian_check;
	u32		rom_dualized_pdxtc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32		rom_dualized_pdxtc_cal_data_addr_list_len;
	bool	rom_dualized_gcc_cal_endian_check;
	u32		rom_dualized_gcc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32		rom_dualized_gcc_cal_data_addr_list_len;
	bool	rom_dualized_xtc_cal_endian_check;
	u32		rom_dualized_xtc_cal_data_addr_list[CROSSTALK_CAL_MAX];
	u32		rom_dualized_xtc_cal_data_addr_list_len;

	u16		rom_dualized_dualcal_slave1_cropshift_x_addr;
	u16		rom_dualized_dualcal_slave1_cropshift_y_addr;

	u16		rom_dualized_dualcal_slave1_oisshift_x_addr;
	u16		rom_dualized_dualcal_slave1_oisshift_y_addr;

	u16		rom_dualized_dualcal_slave1_dummy_flag_addr;
#endif

	u32		bin_start_addr;		/* DDK */
	u32		bin_end_addr;
#ifdef USE_RTA_BINARY
	u32		rta_bin_start_addr;	/* RTA */
	u32		rta_bin_end_addr;
#endif
	u32		setfile_start_addr;
	u32		setfile_end_addr;
#ifdef CAMERA_MODULE_REAR2_SETF_DUMP
	u32		setfile2_start_addr;
	u32		setfile2_end_addr;
#endif
	u32		front_setfile_start_addr;
	u32		front_setfile_end_addr;

	char		header_ver[IS_HEADER_VER_SIZE + 1];
	char		header2_ver[IS_HEADER_VER_SIZE + 1];
	char		cal_map_ver[IS_CAL_MAP_VER_SIZE + 1];
	char		setfile_ver[IS_ISP_SETFILE_VER_SIZE + 1];

	char		load_fw_name[50];		/* DDK */
	char		load_rta_fw_name[50];	/* RTA */

	char		load_setfile_name[50];
#ifdef CAMERA_MODULE_REAR2_SETF_DUMP
	char		load_setfile2_name[50];  /*rear 2*/
#endif
	char		load_front_setfile_name[50];
	char		load_tunning_hifills_name[50];
	char		project_name[IS_PROJECT_NAME_SIZE + 1];
	char		rom_sensor_id[IS_SENSOR_ID_SIZE + 1];
	char		rom_sensor2_id[IS_SENSOR_ID_SIZE + 1];
	u8		rom_module_id[IS_MODULE_ID_SIZE + 1];
	unsigned long		fw_size;
#ifdef USE_RTA_BINARY
	unsigned long		rta_fw_size;
#endif
	unsigned long		setfile_size;
#ifdef CAMERA_MODULE_REAR2_SETF_DUMP
	unsigned long		setfile2_size;
#endif
	unsigned long		front_setfile_size;
	unsigned long		hifi_tunning_size;
};

bool is_sec_get_force_caldata_dump(void);
int is_sec_set_force_caldata_dump(bool fcd);

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos);
ssize_t read_data_rom_file(char *name, char *buf, size_t count, loff_t *pos);
bool is_sec_file_exist(char *name);

#ifdef CONFIG_SEC_CAL_ENABLE
int is_sec_get_max_cal_size(struct is_core *core, int position);
int is_sec_get_cal_buf_rom_data(char **buf, int rom_id);
#endif
int is_sec_get_sysfs_finfo(struct is_rom_info **finfo, int rom_id);
int is_sec_get_sysfs_pinfo(struct is_rom_info **pinfo, int rom_id);
int is_sec_get_front_cal_buf(char **buf, int rom_id);

int is_sec_get_cal_buf(char **buf, int rom_id);
int is_sec_get_loaded_fw(char **buf);
int is_sec_set_loaded_fw(char *buf);
int is_sec_get_loaded_c1_fw(char **buf);
int is_sec_set_loaded_c1_fw(char *buf);

int is_sec_get_camid(void);
int is_sec_set_camid(int id);
int is_sec_get_pixel_size(char *header_ver);
int is_sec_sensorid_find(struct is_core *core);
int is_sec_sensorid_find_front(struct is_core *core);
int is_sec_fw_find(struct is_core *core);
int is_sec_run_fw_sel(int rom_id);

int is_sec_readfw(struct is_core *core);
int is_sec_read_setfile(struct is_core *core);
#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
int is_sec_inflate_fw(u8 **buf, unsigned long *size);
#endif
int is_sec_fw_sel_eeprom(int id, bool headerOnly);
int is_sec_fw_sel_otprom(int id, bool headerOnly);
#if defined (CONFIG_CAMERA_EEPROM_DUALIZED)
int is_sec_readcal_eeprom_dualized(int rom_id);
#endif
int is_sec_write_fw(struct is_core *core);
#if defined(CONFIG_CAMERA_FROM)
int is_sec_readcal(struct is_core *core);
int is_sec_fw_sel(struct is_core *core, bool headerOnly);
int is_sec_check_bin_files(struct is_core *core);
#endif
int is_sec_fw_revision(char *fw_ver);
int is_sec_fw_revision(char *fw_ver);
bool is_sec_fw_module_compare(char *fw_ver1, char *fw_ver2);
int is_sec_compare_ver(int rom_id);
bool is_sec_check_rom_ver(struct is_core *core, int rom_id);

bool is_sec_check_fw_crc32(char *buf, u32 checksum_seed, unsigned long size);
bool is_sec_check_cal_crc32(char *buf, int id);
void is_sec_make_crc32_table(u32 *table, u32 id);
#ifdef CONFIG_SEC_CAL_ENABLE
bool is_sec_check_awb_lsc_crc32_post_sec2lsi(char *buf, int position, int awb_length, int lsc_length);
#endif

int is_sec_gpio_enable(struct exynos_platform_is *pdata, char *name, bool on);
int is_sec_core_voltage_select(struct device *dev, char *header_ver);
int is_sec_ldo_enable(struct device *dev, char *name, bool on);
int is_sec_rom_power_on(struct is_core *core, int position);
int is_sec_rom_power_off(struct is_core *core, int position);
void remove_dump_fw_file(void);
int is_get_dual_cal_buf(int slave_position, char **buf, int *size);
#if defined(CAMERA_UWIDE_DUALIZED)
void is_sec_set_rear3_dualized_rom_probe(void);
#endif
#endif /* IS_SEC_DEFINE_H */
