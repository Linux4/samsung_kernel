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

#define IS_DDK				"is_lib.bin"
#define IS_RTA				"is_rta.bin"

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
void is_sec_sec2lsi_set_cal_reload(bool val);
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

	int32_t		rear_remosaic_tetra_xtc_start_addr;
	int32_t		rear_remosaic_tetra_xtc_size;
	int32_t		rear_remosaic_sensor_xtc_start_addr ;
	int32_t		rear_remosaic_sensor_xtc_size;
	int32_t		rear_remosaic_pdxtc_start_addr;
	int32_t		rear_remosaic_pdxtc_size;
	int32_t		rear_remosaic_sw_ggc_start_addr;
	int32_t		rear_remosaic_sw_ggc_size;

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

	char		header_ver[IS_HEADER_VER_SIZE + 1];
	char		header2_ver[IS_HEADER_VER_SIZE + 1];
	char		cal_map_ver[IS_CAL_MAP_VER_SIZE + 1];
	char		setfile_ver[IS_ISP_SETFILE_VER_SIZE + 1];

	char		project_name[IS_PROJECT_NAME_SIZE + 1];
	char		rom_sensor_id[IS_SENSOR_ID_SIZE + 1];
	char		rom_sensor2_id[IS_SENSOR_ID_SIZE + 1];
	u8		rom_module_id[IS_MODULE_ID_SIZE + 1];
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

int is_sec_sensorid_find(struct is_core *core);
int is_sec_sensorid_find_front(struct is_core *core);
int is_sec_run_fw_sel(int rom_id);

#ifdef RETRY_READING_CAL_CNT
int is_sec_read_rom(int rom_id);
#endif

int is_sec_readfw(struct is_core *core);
#ifdef CAMERA_MODULE_COMPRESSED_FW_DUMP
int is_sec_inflate_fw(u8 **buf, unsigned long *size);
#endif
//int is_sec_fw_sel_eeprom(int id);
//int is_sec_fw_sel_otprom(int id);
#if defined (CONFIG_CAMERA_EEPROM_DUALIZED)
int is_sec_readcal_eeprom_dualized(int rom_id);
#endif
int is_sec_write_fw(struct is_core *core);

int is_sec_compare_ver(int rom_id);
bool is_sec_check_rom_ver(struct is_core *core, int rom_id);

bool is_sec_check_fw_crc32(char *buf, u32 checksum_seed, unsigned long size);
bool is_sec_check_cal_crc32(char *buf, int id);
void is_sec_make_crc32_table(u32 *table, u32 id);
#ifdef CONFIG_SEC_CAL_ENABLE
bool is_sec_check_awb_lsc_crc32_post_sec2lsi(char *buf, int position, int awb_length, int lsc_length);
#endif

int is_sec_rom_power_on(struct is_core *core, int position);
int is_sec_rom_power_off(struct is_core *core, int position);
void remove_dump_fw_file(void);
int is_get_dual_cal_buf(int slave_position, char **buf, int *size);
int is_sec_update_dualized_sensor(struct is_core *core, int position);

int is_get_remosaic_cal_buf(int sensor_position, char **buf, int *size);
#ifdef MODIFY_CAL_MAP_FOR_SWREMOSAIC_LIB
int is_modify_remosaic_cal_buf(int sensor_position, char *cal_buf, char **buf, int *size);
#endif
int is_sec_get_sysfs_finfo_by_position(int position, struct is_rom_info **finfo);

#endif /* IS_SEC_DEFINE_H */
