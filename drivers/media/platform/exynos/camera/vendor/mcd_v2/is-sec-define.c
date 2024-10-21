/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-sec-define.h"
#include "is-vender.h"
#include "is-vender-specific.h"
#include "is-interface-library.h"

#include <linux/i2c.h>
#include "is-device-eeprom.h"
#include "is-vender-caminfo.h"

#include "is-device-sensor-peri.h"

#define IS_DEFAULT_CAL_SIZE		(20 * 1024)
#define IS_DUMP_CAL_SIZE			(172 * 1024)
#define IS_LATEST_ROM_VERSION_M	'M'

#define IS_READ_MAX_EEP_CAL_SIZE	(32 * 1024)

#define NUM_OF_DUALIZATION_CHECK 6

bool force_caldata_dump = false;

#ifdef USES_STANDARD_CAL_RELOAD
bool sec2lsi_reload = false;
#endif

static struct is_rom_info sysfs_finfo[ROM_ID_MAX];
static struct is_rom_info sysfs_pinfo[ROM_ID_MAX];
static char rom_buf[ROM_ID_MAX][IS_MAX_CAL_SIZE];
char loaded_fw[IS_HEADER_VER_SIZE + 1] = {0, };
char loaded_companion_fw[30] = {0, };

static u32 is_check_dualized_sensor[SENSOR_POSITION_MAX] = {false};

#ifdef CONFIG_SEC_CAL_ENABLE
#ifdef USE_KERNEL_VFS_READ_WRITE
static char *eeprom_cal_dump_path[ROM_ID_MAX] = {
	"dump/eeprom_rear_cal.bin",
	"dump/eeprom_front_cal.bin",
	"dump/eeprom_rear2_cal.bin",
	"dump/eeprom_front2_cal.bin",
	"dump/eeprom_rear3_cal.bin",
	"dump/eeprom_front3_cal.bin",
	"dump/eeprom_rear4_cal.bin",
	"dump/eeprom_front4_cal.bin",
};

static char *otprom_cal_dump_path[ROM_ID_MAX] = {
	"dump/otprom_rear_cal.bin",
	"dump/otprom_front_cal.bin",
	"dump/otprom_rear2_cal.bin",
	"dump/otprom_front2_cal.bin",
	"dump/otprom_rear3_cal.bin",
	"dump/otprom_front3_cal.bin",
	"dump/otprom_rear4_cal.bin",
	"dump/otprom_front4_cal.bin",
};
#endif
#ifdef IS_REAR_MAX_CAL_SIZE
static char cal_buf_rom_data_rear[IS_REAR_MAX_CAL_SIZE];
#endif
#ifdef IS_FRONT_MAX_CAL_SIZE
static char cal_buf_rom_data_front[IS_FRONT_MAX_CAL_SIZE];
#endif
#ifdef IS_REAR2_MAX_CAL_SIZE
static char cal_buf_rom_data_rear2[IS_REAR2_MAX_CAL_SIZE];
#endif
#ifdef IS_FRONT2_MAX_CAL_SIZE
static char cal_buf_rom_data_front2[IS_FRONT2_MAX_CAL_SIZE];
#endif
#ifdef IS_REAR3_MAX_CAL_SIZE
static char cal_buf_rom_data_rear3[IS_REAR3_MAX_CAL_SIZE];
#endif
#ifdef IS_FRONT3_MAX_CAL_SIZE
static char cal_buf_rom_data_front3[IS_FRONT3_MAX_CAL_SIZE];
#endif
#ifdef IS_REAR4_MAX_CAL_SIZE
static char cal_buf_rom_data_rear4[IS_REAR4_MAX_CAL_SIZE];
#endif
#ifdef IS_FRONT4_MAX_CAL_SIZE
static char cal_buf_rom_data_front4[IS_FRONT4_MAX_CAL_SIZE];
#endif

/* cal_buf_rom_data is used for storing original rom data, before standard cal conversion */
static char *cal_buf_rom_data[ROM_ID_MAX] = {

#ifdef IS_REAR_MAX_CAL_SIZE
	cal_buf_rom_data_rear,
#else
	NULL,
#endif
#ifdef IS_FRONT_MAX_CAL_SIZE
	cal_buf_rom_data_front,
#else
	NULL,
#endif
#ifdef IS_REAR2_MAX_CAL_SIZE
	cal_buf_rom_data_rear2,
#else
	NULL,
#endif
#ifdef IS_FRONT2_MAX_CAL_SIZE
	cal_buf_rom_data_front2,
#else
	NULL,
#endif
#ifdef IS_REAR3_MAX_CAL_SIZE
	cal_buf_rom_data_rear3,
#else
	NULL,
#endif
#ifdef IS_FRONT3_MAX_CAL_SIZE
	cal_buf_rom_data_front3,
#else
	NULL,
#endif
#ifdef IS_REAR4_MAX_CAL_SIZE
	cal_buf_rom_data_rear4,
#else
	NULL,
#endif
#ifdef IS_FRONT4_MAX_CAL_SIZE
	cal_buf_rom_data_front4,
#else
	NULL,
#endif
};
#endif

enum {
	CAL_DUMP_STEP_INIT = 0,
	CAL_DUMP_STEP_CHECK,
	CAL_DUMP_STEP_DONE,
};

int check_need_cal_dump = CAL_DUMP_STEP_INIT;

bool is_sec_get_force_caldata_dump(void)
{
	return force_caldata_dump;
}

int is_sec_set_force_caldata_dump(bool fcd)
{
	force_caldata_dump = fcd;
	if (fcd)
		info("forced caldata dump enabled!!\n");
	return 0;
}

int is_sec_get_sysfs_finfo(struct is_rom_info **finfo, int rom_id)
{
	if (rom_id < ROM_ID_MAX)
		*finfo = &sysfs_finfo[rom_id];
	else
		*finfo = 0;
	return 0;
}
EXPORT_SYMBOL_GPL(is_sec_get_sysfs_finfo);

#ifdef CONFIG_SEC_CAL_ENABLE

#ifdef USES_STANDARD_CAL_RELOAD
bool is_sec_sec2lsi_check_cal_reload(void)
{
	info("%s is_sec_sec2lsi_check_cal_reload=%d\n", __func__, sec2lsi_reload);
	return sec2lsi_reload;
}

void is_sec_sec2lsi_set_cal_reload(bool val)
{
	sec2lsi_reload = val;
}
#endif

int is_sec_get_sysfs_finfo_by_position(int position, struct is_rom_info **finfo)
{
	*finfo = &sysfs_finfo[position];

	if (*finfo == NULL) {
		err("finfo addr is null. postion %d", position);
		/*WARN(true, "finfo is null\n");*/
		return -EINVAL;
	}

	return 0;
}

bool is_sec_readcal_dump_post_sec2lsi(struct is_core *core, char *buf, int position)
{
	int ret = false;
#ifdef USE_KERNEL_VFS_READ_WRITE
	int rom_type = ROM_TYPE_NONE;
	int cal_size = 0;
	bool rom_valid = false;

	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;

	mm_segment_t old_fs;
	loff_t pos = 0;
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo;
	struct is_module_enum *module;
	int rom_id = is_vendor_get_rom_id_from_position(position);

	is_sec_get_sysfs_finfo(&finfo, rom_id);

	if (!finfo) {
		err("%s: There is no cal map (rom_id : %d)\n", __func__, rom_id);
		ret = false;
		goto EXIT;
	}

	is_vendor_get_module_from_position(position, &module);

	if (!module) {
		err("%s: There is no module (position : %d)\n", __func__, position);
		ret = false;
		goto EXIT;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	key_fp = filp_open("/data/vendor/camera/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	}

	dump_fp = filp_open("/data/vendor/camera/dump", O_RDONLY, 0);
	if (IS_ERR(dump_fp)) {
		info("dump folder does not exist.\n");
		dump_fp = NULL;
		goto key_err;
	}

	rom_valid = specific->rom_valid[rom_id];
	rom_type = module->pdata->rom_type;
	cal_size = finfo->rom_size;

	if (rom_valid == true) {
		char path[100] = IS_SETFILE_SDCARD_PATH;

		if (rom_type == ROM_TYPE_EEPROM) {
			info("dump folder exist, Dump EEPROM cal data.\n");

			strcat(path, eeprom_cal_dump_path[rom_id]);
			strcat(path, ".post_sec2lsi.bin");
			if (write_data_to_file(path, buf, cal_size, &pos) < 0) {
				info("Failed to rear dump cal data.\n");
				goto dump_err;
			}

			ret = true;
		} else if (rom_type == ROM_TYPE_OTPROM) {
			info("dump folder exist, Dump OTPROM cal data.\n");

			strcat(path, otprom_cal_dump_path[rom_id]);
			strcat(path, ".post_sec2lsi.bin");
			if (write_data_to_file(path, buf, cal_size, &pos) < 0) {
				info("Failed to dump cal data.\n");
				goto dump_err;
			}
			ret = true;
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);

key_err:
	if (key_fp)
		filp_close(key_fp, current->files);


	set_fs(old_fs);

EXIT:
#endif
	return ret;
}

int is_sec_get_max_cal_size(struct is_core *core, int rom_id)
{
	int size = 0;
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;

	if (!specific->rom_valid[rom_id][0]) {
		err("Invalid rom_id[%d]. This rom_id don't have rom!\n", rom_id);
		return size;
	}

	is_sec_get_sysfs_finfo(&finfo, rom_id);

	if (finfo == NULL) {
		err("rom_%d: There is no cal map!\n", rom_id);
		return size;
	}

	size = finfo->rom_size;

	if (!size)
		err("Cal size is 0 (rom_id %d). Check cal size!", rom_id);

	return size;
}

bool is_sec_check_awb_lsc_crc32_post_sec2lsi(char *buf, int position, int awb_length, int lsc_length)
{
	u32 *buf32 = NULL;
	u32 checksum, check_base, checksum_base;
	u32 address_boundary;
	int rom_id;
	bool crc32_check_temp = true;

	struct is_core *core;
	struct is_vender_specific *specific;
	struct is_rom_info *finfo = NULL;
	struct is_rom_info *default_finfo = NULL;
	struct is_module_enum *module = NULL;
	int i = 0;
	int ret = 0;

	bool is_running_camera, is_cal_reload;

	struct rom_standard_cal_data *standard_cal_data;

	core = is_get_is_core();
	specific = core->vender.private_data;
	buf32 = (u32 *)buf;

	is_running_camera = is_vendor_check_camera_running(position);

	info("%s E\n", __func__);

	/***** START CHECK CRC *****/
	rom_id = is_vendor_get_rom_id_from_position(position);
	if (rom_id == ROM_ID_NOTHING) {
		err("%s Failed to get rom_id", __func__);
		crc32_check_temp = false;
		goto EXIT;
	}

	is_sec_get_sysfs_finfo(&finfo, rom_id);
	address_boundary = is_sec_get_max_cal_size(core, rom_id);

	standard_cal_data = &(finfo->standard_cal_data);

	/* AWB Cal Data CRC CHECK */

	if (awb_length > 0) {
		checksum = 0;
		check_base = standard_cal_data->rom_awb_start_addr / 4;
		checksum_base = standard_cal_data->rom_awb_section_crc_addr / 4;

#ifdef ROM_CRC32_DEBUG
		printk(KERN_INFO "[CRC32_DEBUG] AWB CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
			awb_length, standard_cal_data->rom_awb_section_crc_addr);
		printk(KERN_INFO "[CRC32_DEBUG] start = 0x%08X, end = 0x%08X\n",
			standard_cal_data->rom_awb_start_addr, standard_cal_data->rom_awb_end_addr);
#endif

		if (check_base > address_boundary || checksum_base > address_boundary) {
			err("Camera[%d]: AWB address has error: start(0x%08X), end(0x%08X)",
				position, standard_cal_data->rom_awb_start_addr, standard_cal_data->rom_awb_end_addr);
			crc32_check_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], awb_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera[%d]: AWB address has error: start(0x%08X), end(0x%08X) Crc address(0x%08X)",
				position, standard_cal_data->rom_awb_start_addr, standard_cal_data->rom_awb_end_addr,
				standard_cal_data->rom_awb_section_crc_addr);
			err("Camera[%d]: CRC32 error at the AWB (0x%08X != 0x%08X)", position, checksum, buf32[checksum_base]);
			crc32_check_temp = false;
			goto out;
		} else {
			info("%s  AWB CRC is pass! ", __func__);
		}
	} else {
		err("Camera[%d]: Skip to check awb crc32\n", position);
	}

	/* Shading Cal Data CRC CHECK*/
	if (lsc_length > 0) {
		checksum = 0;
		check_base = standard_cal_data->rom_shading_start_addr / 4;
		checksum_base = standard_cal_data->rom_shading_section_crc_addr / 4;

#ifdef ROM_CRC32_DEBUG
		printk(KERN_INFO "[CRC32_DEBUG] Shading CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
			lsc_length, standard_cal_data->rom_shading_section_crc_addr);
		printk(KERN_INFO "[CRC32_DEBUG] start = 0x%08X, end = 0x%08X\n",
			standard_cal_data->rom_shading_start_addr, standard_cal_data->rom_shading_end_addr);
#endif

		if (check_base > address_boundary || checksum_base > address_boundary) {
			err("Camera[%d]: Shading address has error: start(0x%08X), end(0x%08X)",
				position, standard_cal_data->rom_shading_start_addr,
				standard_cal_data->rom_shading_end_addr);
			crc32_check_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], lsc_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera[%d]: Shading address has error: start(0x%08X), end(0x%08X) Crc address(0x%08X)",
				position, standard_cal_data->rom_shading_start_addr, standard_cal_data->rom_shading_end_addr,
				standard_cal_data->rom_shading_section_crc_addr);
			err("Camera[%d]: CRC32 error at the Shading (0x%08X != 0x%08X)", position, checksum, buf32[checksum_base]);
			crc32_check_temp = false;
			goto out;

		} else {
			info("%s  LSC CRC is pass! ", __func__);
		}

	} else {
		err("Camera[%d]: Skip to check shading crc32\n", position);
	}

out:
	/* Sync DDK Cal with cal_buf during cal reload */
	is_sec_get_sysfs_finfo(&default_finfo, ROM_ID_REAR);
	is_cal_reload = test_bit(IS_ROM_STATE_CAL_RELOAD, &default_finfo->rom_state);
	// todo cal reaload part
	info("%s: Sensor running = %d\n", __func__, is_running_camera);
	if (crc32_check_temp && is_cal_reload == true && is_running_camera == true) {
		for (i = 0; i < IS_SENSOR_COUNT; i++) {
			is_search_sensor_module_with_position(&core->sensor[i], position, &module);
			if (module)
				break;
		}

		if (!module) {
			err("%s: Could not find sensor id.", __func__);
			crc32_check_temp = false;
			goto out;
		}

		ret =  is_vender_cal_load(&core->vender, module);
		if (ret < 0)
			err("(%s) Unable to sync cal, is_vender_cal_load failed\n", __func__);
	}
EXIT:
	info("%s X\n", __func__);
	return crc32_check_temp;
}
#endif

int is_sec_get_sysfs_pinfo(struct is_rom_info **pinfo, int rom_id)
{
	*pinfo = &sysfs_pinfo[rom_id];
	return 0;
}

int is_sec_get_cal_buf(char **buf, int rom_id)
{
	*buf = rom_buf[rom_id];
	return 0;
}
EXPORT_SYMBOL_GPL(is_sec_get_cal_buf);

#ifdef CONFIG_SEC_CAL_ENABLE
int is_sec_get_cal_buf_rom_data(char **buf, int rom_id)
{
	*buf = cal_buf_rom_data[rom_id];

	if (*buf == NULL) {
		err("cal buf rom data is null. rom_id %d", rom_id);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(is_sec_get_cal_buf_rom_data);
#endif

int is_sec_get_loaded_fw(char **buf)
{
	*buf = &loaded_fw[0];
	return 0;
}

int is_sec_set_loaded_fw(char *buf)
{
	strncpy(loaded_fw, buf, IS_HEADER_VER_SIZE);
	return 0;
}

int is_sec_compare_ver(int rom_id)
{
	struct is_rom_info *finfo = NULL;

	is_sec_get_sysfs_finfo(&finfo, rom_id);

	if (finfo->cal_map_ver[0] == 'V'
		&& finfo->cal_map_ver[1] >= '0' && finfo->cal_map_ver[1] <= '9'
		&& finfo->cal_map_ver[2] >= '0' && finfo->cal_map_ver[2] <= '9'
		&& finfo->cal_map_ver[3] >= '0' && finfo->cal_map_ver[3] <= '9') {
		return ((finfo->cal_map_ver[2] - '0') * 10) + (finfo->cal_map_ver[3] - '0');
	} else {
		err("ROM core version is invalid. version is %c%c%c%c",
			finfo->cal_map_ver[0], finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);
		return -1;
	}
}

bool is_sec_check_rom_ver(struct is_core *core, int rom_id)
{
	struct is_rom_info *finfo = NULL;
	char compare_version;
	int rom_ver;
	int latest_rom_ver;

	is_sec_get_sysfs_finfo(&finfo, rom_id);

	if (test_bit(IS_ROM_STATE_SKIP_CAL_LOADING, &finfo->rom_state)) {
		err("[rom_id:%d] skip_cal_loading implemented", rom_id);
		return false;
	}

	if (test_bit(IS_ROM_STATE_SKIP_HEADER_LOADING, &finfo->rom_state)) {
		err("[rom_id:%d] skip_header_loading implemented", rom_id);
		return true;
	}

	latest_rom_ver = finfo->cal_map_es_version;
	compare_version = finfo->camera_module_es_version;

	rom_ver = is_sec_compare_ver(rom_id);

	if ((rom_ver < latest_rom_ver && finfo->rom_header_cal_map_ver_start_addr > 0) ||
		(finfo->header_ver[10] < compare_version)) {
		err("[%d]invalid rom version. rom_ver %c, header_ver[10] %c\n",
			rom_id, rom_ver, finfo->header_ver[10]);
		return false;
	} else {
		return true;
	}
}

bool is_sec_check_cal_crc32(char *buf, int rom_id)
{
	u8 *buf8 = NULL;
	u32 checksum;
	u32 check_base;
	u32 check_length;
	u32 checksum_base;
	u32 address_boundary;
	bool crc32_temp, crc32_header_temp, crc32_dual_temp;
	struct is_rom_info *finfo = NULL;
	struct is_core *core;
	struct is_vender_specific *specific;
 	int i;

	core = is_get_is_core();
	specific = core->vender.private_data;
	buf8 = (u8 *)buf;

	info("+++ %s\n", __func__);

	is_sec_get_sysfs_finfo(&finfo, rom_id);
	finfo->crc_error = 0; /* clear all bits */

	if (test_bit(IS_ROM_STATE_SKIP_CRC_CHECK, &finfo->rom_state)) {
		info("%s : skip crc check. return\n", __func__);
		return true;
	}

	crc32_temp = true;
	crc32_header_temp = true;
	crc32_dual_temp = true;

	address_boundary = IS_MAX_CAL_SIZE;

	/* header crc check */
	for (i = 0; i < finfo->header_crc_check_list_len; i += 3) {
		checksum = 0;
		check_base = finfo->header_crc_check_list[i];
		check_length = (finfo->header_crc_check_list[i+1] - finfo->header_crc_check_list[i] + 1) ;
		checksum_base = finfo->header_crc_check_list[i+2];

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("[rom%d/header cal:%d] address has error: start(0x%08X), end(0x%08X)",
				rom_id, i, finfo->header_crc_check_list[i], finfo->header_crc_check_list[i+1]);
			crc32_header_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf8[check_base], check_length, NULL, NULL);
		if (checksum != *((u32 *)&buf8[checksum_base])) {
			err("[rom%d/header cal:%d] CRC32 error (0x%08X != 0x%08X), base[0x%X] len[0x%X] checksum[0x%X]",
				rom_id, i, checksum, *((u32 *)&buf8[checksum_base]), check_base, check_length, checksum_base);
			crc32_header_temp = false;
			goto out;
		}
	}

	/* main crc check */
	for (i = 0; i < finfo->crc_check_list_len; i += 3) {
		checksum = 0;
		check_base = finfo->crc_check_list[i];
		check_length = (finfo->crc_check_list[i+1] - finfo->crc_check_list[i] + 1) ;
		checksum_base = finfo->crc_check_list[i+2];

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("[rom%d/main cal:%d] address has error: start(0x%08X), end(0x%08X)",
				rom_id, i, finfo->crc_check_list[i], finfo->crc_check_list[i+1]);
			crc32_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf8[check_base], check_length, NULL, NULL);
		if (checksum != *((u32 *)&buf8[checksum_base])) {
			err("[rom%d/main cal:%d] CRC32 error (0x%08X != 0x%08X), base[0x%X] len[0x%X] checksum[0x%X]",
				rom_id, i, checksum, *((u32 *)&buf8[checksum_base]), check_base, check_length, checksum_base);
			crc32_temp = false;
			goto out;
		}
	}

	/* dual crc check */
	for (i = 0; i < finfo->dual_crc_check_list_len; i += 3) {
		checksum = 0;
		check_base = finfo->dual_crc_check_list[i];
		check_length = (finfo->dual_crc_check_list[i+1] - finfo->dual_crc_check_list[i] + 1) ;
		checksum_base = finfo->dual_crc_check_list[i+2];

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("[rom%d/dual cal:%d] data address has error: start(0x%08X), end(0x%08X)",
				rom_id, i, finfo->dual_crc_check_list[i], finfo->dual_crc_check_list[i+1]);
			crc32_temp = false;
			crc32_dual_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf8[check_base], check_length, NULL, NULL);
		if (checksum != *((u32 *)&buf8[checksum_base])) {
			err("[rom%d/main cal:%d] CRC32 error (0x%08X != 0x%08X), base[0x%X] len[0x%X] checksum[0x%X]",
				rom_id, i, checksum, *((u32 *)&buf8[checksum_base]), check_base, check_length, checksum_base);
			crc32_temp = false;
			crc32_dual_temp = false;
			goto out;
		}
	}

out:
	if (!crc32_temp)
		set_bit(IS_CRC_ERROR_ALL_SECTION, &finfo->crc_error);
	if (!crc32_header_temp)
		set_bit(IS_CRC_ERROR_HEADER, &finfo->crc_error);
	if (!crc32_dual_temp)
		set_bit(IS_CRC_ERROR_DUAL_CAMERA, &finfo->crc_error);

	info("[rom_id:%d] crc32_check %d crc32_header %d crc32_dual %d\n",
		rom_id, crc32_temp, crc32_header_temp, crc32_dual_temp);

	return crc32_temp && crc32_header_temp;
}

void remove_dump_fw_file(void)
{
#ifdef USE_KERNEL_VFS_READ_WRITE
	mm_segment_t old_fs;
	long ret;
	char fw_path[100];
	struct is_rom_info *finfo = NULL;
	struct file *fp;
	struct inode *parent_inode;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	is_sec_get_sysfs_finfo(&finfo, ROM_ID_REAR);

	/* RTA binary */
	snprintf(fw_path, sizeof(fw_path), "%s%s", IS_FW_DUMP_PATH, IS_RTA);

	fp = filp_open(fw_path, O_RDONLY, 0);

	if (!IS_ERR(fp)) {
		parent_inode= fp->f_path.dentry->d_parent->d_inode;
		inode_lock(parent_inode);
		ret = vfs_unlink(parent_inode, fp->f_path.dentry, NULL);
		inode_unlock(parent_inode);
	} else {
		ret = -ENOENT;
	}

	info("sys_unlink (%s) %ld", fw_path, ret);

	/* DDK binary */
	snprintf(fw_path, sizeof(fw_path), "%s%s", IS_FW_DUMP_PATH, IS_DDK);

	fp = filp_open(fw_path, O_RDONLY, 0);

	if (!IS_ERR(fp)) {
		parent_inode= fp->f_path.dentry->d_parent->d_inode;
		inode_lock(parent_inode);
		ret = vfs_unlink(parent_inode, fp->f_path.dentry, NULL);
		inode_unlock(parent_inode);
	} else {
		ret = -ENOENT;
	}

	info("sys_unlink (%s) %ld", fw_path, ret);

	set_fs(old_fs);

#endif
}

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos)
{
	ssize_t tx = -ENOENT;
#ifdef USE_KERNEL_VFS_READ_WRITE
	struct file *fp;

	if (force_caldata_dump) {
		fp = filp_open(name, O_RDONLY, 0);

		if (IS_ERR(fp)) {
			fp = filp_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
		} else {
			struct inode *parent_inode= fp->f_path.dentry->d_parent->d_inode;
			inode_lock(parent_inode);
			vfs_rmdir(parent_inode, fp->f_path.dentry);
			inode_unlock(parent_inode);
			fp = filp_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
		}
	} else {
		fp = filp_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
	}

	if (IS_ERR(fp)) {
		err("open file error: %s", name);
		return -EINVAL;
	}

	tx = kernel_write(fp, buf, count, pos);
	if (tx != count) {
		err("fail to write %s. ret %zd", name, tx);
		tx = -ENOENT;
	}
	vfs_fsync(fp, 0);
	fput(fp);

	filp_close(fp, NULL);
#endif
	return tx;
}

ssize_t read_data_rom_file(char *name, char *buf, size_t count, loff_t *pos)
{
#ifdef USE_KERNEL_VFS_READ_WRITE
	struct file *fp;
	ssize_t tx;

	fp = filp_open(name, O_RDONLY, 0);

	if (IS_ERR(fp)) {
		info("%s: error %d, failed to open %s\n", __func__, PTR_ERR(fp), name);
		return -EINVAL;
	}

	tx = kernel_read(fp, buf, count, pos);

	if (tx != count) {
		err("fail to read %s. ret %zd", name, tx);
		tx = -ENOENT;
	}

	fput(fp);
	filp_close(fp, NULL);
#endif
	return count;
}

bool is_sec_file_exist(char *name)
{
#ifdef USE_KERNEL_VFS_READ_WRITE
	struct file *fp;
	mm_segment_t old_fs;
	bool exist = true;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(name, O_RDONLY, 0);

	if (IS_ERR(fp)) {
		exist = false;
		info("%s: error %d, failed to open %s\n", __func__, PTR_ERR(fp), name);
	} else {
		filp_close(fp, NULL);
	}

	set_fs(old_fs);
	return exist;
#endif
	return false;
}

void is_sec_make_crc32_table(u32 *table, u32 id)
{
	u32 i, j, k;

	for (i = 0; i < 256; ++i) {
		k = i;
		for (j = 0; j < 8; ++j) {
			if (k & 1)
				k = (k >> 1) ^ id;
			else
				k >>= 1;
		}
		table[i] = k;
	}
}

int is_sec_rom_power_on(struct is_core *core, int position)
{
	int ret = 0;
	struct exynos_platform_is_module *module_pdata;
	struct is_module_enum *module = NULL;
	int i = 0;

	info("%s: Sensor position = %d.", __func__, position);

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		is_search_sensor_module_with_position(&core->sensor[i], position, &module);
		if (module)
			break;
	}

	if (!module) {
		err("%s: Could not find sensor id.", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	module_pdata = module->pdata;

	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = module_pdata->gpio_cfg(module, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int is_sec_rom_power_off(struct is_core *core, int position)
{
	int ret = 0;
	struct exynos_platform_is_module *module_pdata;
	struct is_module_enum *module = NULL;
	int i = 0;

	info("%s: Sensor position = %d.", __func__, position);

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		is_search_sensor_module_with_position(&core->sensor[i], position, &module);
		if (module)
			break;
	}

	if (!module) {
		err("%s: Could not find sensor id.", __func__);
		ret = -EINVAL;
		goto p_err;
	}

	module_pdata = module->pdata;

	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = module_pdata->gpio_cfg(module, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int is_sec_check_is_sensor(struct is_core *core, int position, int nextSensorId, bool *bVerified) {
	int ret;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct is_vender_specific *specific = core->vender.private_data;
	int i;
	u32 i2c_channel;
	struct exynos_platform_is_module *module_pdata;
	struct is_module_enum *module = NULL;
	const u32 scenario = SENSOR_SCENARIO_NORMAL;

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		is_search_sensor_module_with_position(&core->sensor[i], position, &module);
		if (module)
			break;
	}

	if (!module) {
		err("%s: Could not find sensor id.", __func__);
		ret = -EINVAL;
		goto p_err;
	}
	module_pdata = module->pdata;
	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = module_pdata->gpio_cfg(module, scenario, GPIO_SCENARIO_ON);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
	} else {
		sensor_peri = (struct is_device_sensor_peri *)module->private_data;
		if (sensor_peri->subdev_cis) {
			i2c_channel = module_pdata->sensor_i2c_ch;
			if (i2c_channel < SENSOR_CONTROL_I2C_MAX) {
				sensor_peri->cis.ixc_lock = &core->ixc_lock[i2c_channel];
			} else {
				warn("%s: wrong cis i2c_channel(%d)", __func__, i2c_channel);
				ret = -EINVAL;
				goto p_err;
			}
		} else {
			err("%s: subdev cis is NULL. dual_sensor check failed", __func__);
			ret = -EINVAL;
			goto p_err;
		}

		subdev_cis = sensor_peri->subdev_cis;
		ret = CALL_CISOPS(&sensor_peri->cis, cis_check_rev_on_init, subdev_cis);
		if (ret < 0) {
			*bVerified = false;
			specific->sensor_id[position] = nextSensorId;
			warn("CIS test failed, check the nextSensor");
		} else {
			*bVerified = true;
			core->sensor[module->device].sensor_name = module->sensor_name;
			info("%s CIS test passed (sensorId[%d] = %d)", __func__, module->device, specific->sensor_id[position]);
		}

		ret = module_pdata->gpio_cfg(module, scenario, GPIO_SCENARIO_OFF);
		if (ret) {
			err("%s gpio_cfg is fail(%d)", __func__, ret);
		}

		/* Requested by HQE to meet the power guidance, add 20ms delay */
#ifdef PUT_30MS_BETWEEN_EACH_CAL_LOADING
		msleep(30);
#else
		msleep(20);
#endif
	}
p_err:
	return ret;
}

int is_sec_update_dualized_sensor(struct is_core *core, int position)
{
	int ret = 0;
	int sensorid_1st, sensorid_2nd;
	struct is_vender_specific *specific = core->vender.private_data;

	sensorid_1st = specific->sensor_id[position];
	sensorid_2nd = is_vender_get_dualized_sensorid(position);

	if (sensorid_2nd != SENSOR_NAME_NOTHING && !is_check_dualized_sensor[position]) {
		int count_check = NUM_OF_DUALIZATION_CHECK;
		bool bVerified = false;

		while (count_check-- > 0) {
			ret = is_sec_check_is_sensor(core, position, sensorid_2nd, &bVerified);
			if (ret) {
				err("%s: Failed to check corresponding sensor equipped", __func__);
				break;
			}

			if (bVerified) {
				is_check_dualized_sensor[position] = true;
				break;
			}

			sensorid_2nd = sensorid_1st;
			sensorid_1st = specific->sensor_id[position];

			if (count_check < NUM_OF_DUALIZATION_CHECK - 2) {
				err("Failed to find out both sensors on this device !!! (count : %d)", count_check);
				if (count_check <= 0)
					err("None of camera was equipped (sensorid : %d, %d)", sensorid_1st, sensorid_2nd);
			}
		}
	}

	return ret;
}

void is_sec_check_module_state(struct is_rom_info *finfo)
{
	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific = core->vender.private_data;

	if (test_bit(IS_ROM_STATE_SKIP_HEADER_LOADING, &finfo->rom_state)) {
		clear_bit(IS_ROM_STATE_OTHER_VENDOR, &finfo->rom_state);
		info("%s : skip header loading. return ", __func__);
		return;
	}

	if (finfo->header_ver[3] == 'L' || finfo->header_ver[3] == 'X'
#ifdef CAMERA_CAL_VERSION_GC5035B
		|| finfo->header_ver[3] == 'Q'
#endif
#ifdef CAMERA_STANDARD_CAL_ISP_VERSION
		|| finfo->header_ver[3] == CAMERA_STANDARD_CAL_ISP_VERSION
#endif
	) {
		clear_bit(IS_ROM_STATE_OTHER_VENDOR, &finfo->rom_state);
	} else {
		set_bit(IS_ROM_STATE_OTHER_VENDOR, &finfo->rom_state);
	}

	if (specific->use_module_check) {
		if (finfo->header_ver[10] >= finfo->camera_module_es_version) {
			set_bit(IS_ROM_STATE_LATEST_MODULE, &finfo->rom_state);
		} else {
			clear_bit(IS_ROM_STATE_LATEST_MODULE, &finfo->rom_state);
		}

		if (finfo->header_ver[10] == IS_LATEST_ROM_VERSION_M) {
			set_bit(IS_ROM_STATE_FINAL_MODULE, &finfo->rom_state);
		} else {
			clear_bit(IS_ROM_STATE_FINAL_MODULE, &finfo->rom_state);
		}
	} else {
		set_bit(IS_ROM_STATE_LATEST_MODULE, &finfo->rom_state);
		set_bit(IS_ROM_STATE_FINAL_MODULE, &finfo->rom_state);
	}
}

#define ADDR_SIZE	2
int is_i2c_read(struct i2c_client *client, void *buf, u32 addr, size_t size)
{
	const u32 max_retry = 2;
	u8 addr_buf[ADDR_SIZE];
	int retries = max_retry;
	int ret = 0;

	if (!client) {
		info("%s: client is null\n", __func__);
		return -ENODEV;
	}

	/* Send addr */
	addr_buf[0] = ((u16)addr) >> 8;
	addr_buf[1] = (u8)addr;

	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_send(client, addr_buf, ADDR_SIZE);
		if (likely(ADDR_SIZE == ret))
			break;

		info("%s: i2c_master_send failed(%d), try %d, addr(%x)\n", __func__, ret, retries, client->addr);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		err("%s: error %d, fail to write 0x%04X", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	/* Receive data */
	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_recv(client, buf, size);
		if (likely(ret == size))
			break;

		info("%s: i2c_master_recv failed(%d), try %d\n", __func__,  ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		err("%s: error %d, fail to read 0x%04X", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	return 0;
}

#define WRITE_BUF_SIZE	3
int is_i2c_write(struct i2c_client *client, u16 addr, u8 data)
{
	const u32 max_retry = 2;
	u8 write_buf[WRITE_BUF_SIZE];
	int retries = max_retry;
	int ret = 0;

	if (!client) {
		pr_info("%s: client is null\n", __func__);
		return -ENODEV;
	}

	/* Send addr+data */
	write_buf[0] = ((u16)addr) >> 8;
	write_buf[1] = (u8)addr;
	write_buf[2] = data;


	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_send(client, write_buf, WRITE_BUF_SIZE);
		if (likely(WRITE_BUF_SIZE == ret))
			break;

		pr_info("%s: i2c_master_send failed(%d), try %d\n", __func__, ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		pr_err("%s: error %d, fail to write 0x%04X\n", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	return 0;
}

int is_sec_check_reload(struct is_core *core)
{
#ifdef USE_KERNEL_VFS_READ_WRITE
	struct file *reload_key_fp = NULL;
	struct file *supend_resume_key_fp = NULL;
	mm_segment_t old_fs;
	struct is_vender_specific *specific = core->vender.private_data;
	char file_path[100];

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	memset(file_path, 0x00, sizeof(file_path));
	snprintf(file_path, sizeof(file_path), "%sreload/r1e2l3o4a5d.key", IS_FW_DUMP_PATH);
	reload_key_fp = filp_open(file_path, O_RDONLY, 0);
	if (IS_ERR(reload_key_fp)) {
		reload_key_fp = NULL;
	} else {
		info("Reload KEY exist, reload cal data.\n");
		force_caldata_dump = true;
#ifdef USES_STANDARD_CAL_RELOAD
		sec2lsi_reload = true;
#endif
		specific->suspend_resume_disable = true;
	}

	if (reload_key_fp)
		filp_close(reload_key_fp, current->files);

	memset(file_path, 0x00, sizeof(file_path));
	snprintf(file_path, sizeof(file_path), "%si1s2p3s4r.key", IS_FW_DUMP_PATH);
	supend_resume_key_fp = filp_open(file_path, O_RDONLY, 0);
	if (IS_ERR(supend_resume_key_fp)) {
		supend_resume_key_fp = NULL;
	} else {
		info("Supend_resume KEY exist, disable runtime supend/resume. \n");
		specific->suspend_resume_disable = true;
	}

	if (supend_resume_key_fp)
		filp_close(supend_resume_key_fp, current->files);

	set_fs(old_fs);
#endif
	return 0;
}

int is_sec_parse_rom_info(struct is_rom_info *finfo, char *buf, int rom_id)
{
#if defined(CONFIG_CAMERA_USE_MCU) || defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
	struct is_ois_info *ois_pinfo = NULL;
#endif

	if (finfo->rom_header_cal_map_ver_start_addr != -1) {
		memcpy(finfo->cal_map_ver, &buf[finfo->rom_header_cal_map_ver_start_addr], IS_CAL_MAP_VER_SIZE);
		finfo->cal_map_ver[IS_CAL_MAP_VER_SIZE] = '\0';
	}

	if (finfo->rom_header_version_start_addr != -1) {
		memcpy(finfo->header_ver, &buf[finfo->rom_header_version_start_addr], IS_HEADER_VER_SIZE);
		finfo->header_ver[IS_HEADER_VER_SIZE] = '\0';
	}

	if (finfo->rom_header_sensor2_version_start_addr != -1) {
		memcpy(finfo->header2_ver, &buf[finfo->rom_header_sensor2_version_start_addr], IS_HEADER_VER_SIZE);
		finfo->header2_ver[IS_HEADER_VER_SIZE] = '\0';
	}

	if (finfo->rom_header_project_name_start_addr != -1) {
		memcpy(finfo->project_name, &buf[finfo->rom_header_project_name_start_addr], IS_PROJECT_NAME_SIZE);
		finfo->project_name[IS_PROJECT_NAME_SIZE] = '\0';
	}

	if (finfo->rom_header_module_id_addr != -1) {
		memcpy(finfo->rom_module_id, &buf[finfo->rom_header_module_id_addr], IS_MODULE_ID_SIZE);
		finfo->rom_module_id[IS_MODULE_ID_SIZE] = '\0';
	}

	if (finfo->rom_header_sensor_id_addr != -1) {
		memcpy(finfo->rom_sensor_id, &buf[finfo->rom_header_sensor_id_addr], IS_SENSOR_ID_SIZE);
		finfo->rom_sensor_id[IS_SENSOR_ID_SIZE] = '\0';
	}

	if (finfo->rom_header_sensor2_id_addr != -1) {
		memcpy(finfo->rom_sensor2_id, &buf[finfo->rom_header_sensor2_id_addr], IS_SENSOR_ID_SIZE);
		finfo->rom_sensor2_id[IS_SENSOR_ID_SIZE] = '\0';
	}

#if defined(CONFIG_CAMERA_USE_MCU) || defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
	is_sec_get_ois_pinfo(&ois_pinfo);
	if (finfo->rom_ois_list_len == IS_ROM_OIS_MAX_LIST) {
		if (rom_id == WIDE_OIS_ROM_ID) {
			memcpy(ois_pinfo->wide_romdata.xgg, &buf[finfo->rom_ois_list[0]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.ygg, &buf[finfo->rom_ois_list[1]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.xcoef, &buf[finfo->rom_ois_list[2]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.ycoef, &buf[finfo->rom_ois_list[3]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.supperssion_xratio, &buf[finfo->rom_ois_list[4]], IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.supperssion_yratio, &buf[finfo->rom_ois_list[5]], IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.cal_mark, &buf[finfo->rom_ois_list[6]], IS_OIS_CAL_MARK_DATA_SIZE);
		}
	} else if (finfo->rom_ois_list_len == IS_ROM_OIS_SINGLE_MODULE_MAX_LIST) {
		if (rom_id == WIDE_OIS_ROM_ID) {
			memcpy(ois_pinfo->wide_romdata.xgg, &buf[finfo->rom_ois_list[0]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.ygg, &buf[finfo->rom_ois_list[1]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.xcoef, &buf[finfo->rom_ois_list[2]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.ycoef, &buf[finfo->rom_ois_list[3]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.supperssion_xratio, &buf[finfo->rom_ois_list[4]], IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.supperssion_yratio, &buf[finfo->rom_ois_list[5]], IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.cal_mark, &buf[finfo->rom_ois_list[6]], IS_OIS_CAL_MARK_DATA_SIZE);
		}
	}
#endif

	/* debug info dump */
	info("++++ ROM data info - rom_id:%d\n", rom_id);
	info("Cal Map Version = %s\n", finfo->cal_map_ver);
	info("Module Header Version = %s : Ver(%c) ISP ID(%c) Maker(%c)\n",
		finfo->header_ver, finfo->header_ver[FW_VERSION_INFO],
		finfo->header_ver[FW_ISP_COMPANY], finfo->header_ver[FW_SENSOR_MAKER]);
	info(" Module info : %s\n", finfo->header_ver);
	info(" Project name : %s\n", finfo->project_name);
	info(" MODULE ID : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
		finfo->rom_module_id[0], finfo->rom_module_id[1], finfo->rom_module_id[2],
		finfo->rom_module_id[3], finfo->rom_module_id[4], finfo->rom_module_id[5],
		finfo->rom_module_id[6], finfo->rom_module_id[7], finfo->rom_module_id[8],
		finfo->rom_module_id[9]);
	info("---- ROM data info\n");

	return 0;
}

int is_sec_read_eeprom_header(int rom_id)
{
	int ret = 0;
	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific;
	u8 header_version[IS_HEADER_VER_SIZE + 1] = {0, };
	u8 header2_version[IS_HEADER_VER_SIZE + 1] = {0, };
	struct i2c_client *client;
	struct is_rom_info *finfo = NULL;
	struct is_device_eeprom *eeprom;

	specific = core->vender.private_data;
	client = specific->rom_client[rom_id][0];

	eeprom = i2c_get_clientdata(client);

	is_sec_get_sysfs_finfo(&finfo, rom_id);

	if (finfo->rom_header_version_start_addr != -1) {
		ret = is_i2c_read(client, header_version,
					finfo->rom_header_version_start_addr,
					IS_HEADER_VER_SIZE);

		if (unlikely(ret)) {
			err("failed to is_i2c_read for header version (%d)\n", ret);
			ret = -EINVAL;
		}

		memcpy(finfo->header_ver, header_version, IS_HEADER_VER_SIZE);
		finfo->header_ver[IS_HEADER_VER_SIZE] = '\0';
	}

	if (finfo->rom_header_sensor2_version_start_addr != -1) {
		ret = is_i2c_read(client, header2_version,
					finfo->rom_header_sensor2_version_start_addr,
					IS_HEADER_VER_SIZE);

		if (unlikely(ret)) {
			err("failed to is_i2c_read for header version (%d)\n", ret);
			ret = -EINVAL;
		}

		memcpy(finfo->header2_ver, header2_version, IS_HEADER_VER_SIZE);
		finfo->header2_ver[IS_HEADER_VER_SIZE] = '\0';
	}

	return ret;
}

int is_sec_read_otprom_header(int rom_id)
{
	int ret = 0;
	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific;
	u8 header_version[IS_HEADER_VER_SIZE + 1] = {0, };
	u8 header2_version[IS_HEADER_VER_SIZE + 1] = {0, };
	struct i2c_client *client;
	struct is_rom_info *finfo = NULL;
	struct is_device_otprom *otprom;
	int position = is_vendor_get_position_from_rom_id(rom_id);
	int sensor_id = is_vendor_get_sensor_id_from_position(position);
	int dualized_sensor_id = is_vender_get_dualized_sensorid(position);

	specific = core->vender.private_data;
	if (specific->rom_valid[rom_id][1] &&
				sensor_id == dualized_sensor_id)
		client = specific->rom_client[rom_id][1];
	else 
		client = specific->rom_client[rom_id][0];

	otprom = i2c_get_clientdata(client);

	is_sec_get_sysfs_finfo(&finfo, rom_id);

	if (finfo->rom_header_version_start_addr != -1) {
		ret = is_i2c_read(client, header_version,
					finfo->rom_header_version_start_addr,
					IS_HEADER_VER_SIZE);

		if (unlikely(ret)) {
			err("failed to is_i2c_read for header version (%d)\n", ret);
			ret = -EINVAL;
		}

		memcpy(finfo->header_ver, header_version, IS_HEADER_VER_SIZE);
		finfo->header_ver[IS_HEADER_VER_SIZE] = '\0';
	}

	if (finfo->rom_header_sensor2_version_start_addr != -1) {
		ret = is_i2c_read(client, header2_version,
					finfo->rom_header_sensor2_version_start_addr,
					IS_HEADER_VER_SIZE);

		if (unlikely(ret)) {
			err("failed to is_i2c_read for header version (%d)\n", ret);
			ret = -EINVAL;
		}

		memcpy(finfo->header2_ver, header2_version, IS_HEADER_VER_SIZE);
		finfo->header2_ver[IS_HEADER_VER_SIZE] = '\0';
	}

	return ret;
}

int is_sec_readcal_otprom(int rom_id)
{
	int ret = 0;
	char *buf = NULL;
	struct is_rom_info *finfo = NULL;
	int retry = IS_CAL_RETRY_CNT;
	int crc_check = false;
	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific = core->vender.private_data;
	int cal_size = 0;
	struct i2c_client *client = NULL;
	bool camera_running;
	struct is_cis *cis = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module = NULL;
	u32 i2c_channel;
	int position = is_vendor_get_position_from_rom_id(rom_id);
	int sensor_id = is_vendor_get_sensor_id_from_position(position);
	int dualized_sensor_id = is_vender_get_dualized_sensorid(position);

#ifdef CONFIG_SEC_CAL_ENABLE
	char *buf_rom_data = NULL;
#endif

	is_sec_get_sysfs_finfo(&finfo, rom_id);
	is_sec_get_cal_buf(&buf, rom_id);
	is_vendor_get_module_from_position(position, &module);

	if (!module) {
		err("There is no module (position : %d)", position);
		ret = false;
		goto exit;
	}

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	subdev_cis = sensor_peri->subdev_cis;
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	if (cis == NULL) {
		err("cis is NULL");
		return -1;
	}
	if (specific->rom_valid[rom_id][1] &&
				sensor_id == dualized_sensor_id)
		client = specific->rom_client[rom_id][1];
	else 
		client = specific->rom_client[rom_id][0];

	cal_size = finfo->rom_size;
	info("Camera: read cal data from OTPROM (rom_id:%d, cal_size:%d, sensor_id:%d)\n", rom_id, cal_size, sensor_id);

crc_retry:
	camera_running = is_vendor_check_camera_running(position);

	if (camera_running == false) {
		i2c_channel = module->pdata->sensor_i2c_ch;
		if (i2c_channel < SENSOR_CONTROL_I2C_MAX) {
			sensor_peri->cis.ixc_lock = &core->ixc_lock[i2c_channel];
		} else {
			warn("%s: wrong cis i2c_channel(%d)", __func__, i2c_channel);
			ret = -EINVAL;
			goto exit;
		}
	}

	/* 1. Get cal data */
	ret = CALL_CISOPS(&sensor_peri->cis, cis_get_otprom_data, subdev_cis, buf, camera_running, rom_id);
	if (ret < 0) {
		err("failed to get otprom");
		ret = -EINVAL;
		goto exit;
	}

	/* 2. CRC check */
	crc_check = is_sec_check_cal_crc32(buf, rom_id);
	if (!crc_check && (retry > 0)) {
		retry--;
		goto crc_retry;
	}
	if (!crc_check) {
		err("crc error : check the cis_get_otprom_data func");
		ret = -EINVAL;
	}

	/* 3. parse rom info */
	is_sec_parse_rom_info(finfo, buf, rom_id);

	/* 4. check rom version & update module state */
	if (!is_sec_check_rom_ver(core, rom_id)) {
		info("Camera: Do not read otprom cal data. OTPROM version is low.\n");
		ret = -EINVAL;
		goto exit;
	}

	is_sec_check_module_state(finfo);

#ifdef CONFIG_SEC_CAL_ENABLE
	/* 5. Store original rom data before conversion for intrinsic cal */
	if (crc_check == true && is_need_use_standard_cal(rom_id)) {
		is_sec_get_cal_buf_rom_data(&buf_rom_data, rom_id);
		if (buf != NULL && buf_rom_data != NULL)
			memcpy(buf_rom_data, buf, is_sec_get_max_cal_size(core, rom_id));
	}
#endif

exit:
	return ret;
}

#if defined (CONFIG_CAMERA_EEPROM_DUALIZED)
int is_sec_readcal_eeprom_dualized(int rom_id)
{
	int ret = 0;
	int i;
	struct is_rom_info *finfo = NULL;
#ifdef CONFIG_SEC_CAL_ENABLE
	struct rom_standard_cal_data *standard_cal_data;
	struct rom_standard_cal_data *backup_standard_cal_data;
#endif

	is_sec_get_sysfs_finfo(&finfo, rom_id);

#ifdef CONFIG_SEC_CAL_ENABLE
	standard_cal_data = &(finfo->standard_cal_data);
	backup_standard_cal_data = &(finfo->backup_standard_cal_data);
#endif

	if (finfo->is_read_dualized_values){
		pr_info("Dualized values already read\n");
		goto EXIT;
	}

	for (i = 0 ; i < IS_ROM_CRC_MAX_LIST; i++){
		finfo->header_crc_check_list[i] = finfo->header_dualized_crc_check_list[i];
		finfo->crc_check_list[i] = finfo->crc_dualized_check_list[i];
		finfo->dual_crc_check_list[i] = finfo->dual_crc_dualized_check_list[i];
	}
	finfo->header_crc_check_list_len = finfo->header_dualized_crc_check_list_len;
	finfo->crc_check_list_len = finfo->crc_dualized_check_list_len;
	finfo->dual_crc_check_list_len = finfo->dual_crc_dualized_check_list_len;

	for (i = 0 ; i < IS_ROM_DUAL_TILT_MAX_LIST; i++){
		finfo->rom_dualcal_slave0_tilt_list[i] = finfo->rom_dualized_dualcal_slave0_tilt_list[i];
		finfo->rom_dualcal_slave1_tilt_list[i] = finfo->rom_dualized_dualcal_slave1_tilt_list[i];
		finfo->rom_dualcal_slave2_tilt_list[i] = finfo->rom_dualized_dualcal_slave2_tilt_list[i];
		finfo->rom_dualcal_slave3_tilt_list[i] = finfo->rom_dualized_dualcal_slave3_tilt_list[i];
	}
	finfo->rom_dualcal_slave0_tilt_list_len = finfo->rom_dualized_dualcal_slave0_tilt_list_len;
	finfo->rom_dualcal_slave1_tilt_list_len = finfo->rom_dualized_dualcal_slave1_tilt_list_len;
	finfo->rom_dualcal_slave2_tilt_list_len = finfo->rom_dualized_dualcal_slave2_tilt_list_len;
	finfo->rom_dualcal_slave3_tilt_list_len = finfo->rom_dualized_dualcal_slave3_tilt_list_len;

	for (i = 0 ; i < IS_ROM_OIS_MAX_LIST; i++)
		finfo->rom_ois_list[i] = finfo->rom_dualized_ois_list[i];
	finfo->rom_ois_list_len = finfo->rom_dualized_ois_list_len;

	/* set -1 if not support */
	finfo->rom_header_cal_data_start_addr = finfo->rom_dualized_header_cal_data_start_addr;
	finfo->rom_header_version_start_addr = finfo->rom_dualized_header_version_start_addr;
	finfo->rom_header_cal_map_ver_start_addr = finfo->rom_dualized_header_cal_map_ver_start_addr;
	finfo->rom_header_isp_setfile_ver_start_addr = finfo->rom_dualized_header_isp_setfile_ver_start_addr;
	finfo->rom_header_project_name_start_addr = finfo->rom_dualized_header_project_name_start_addr;
	finfo->rom_header_module_id_addr = finfo->rom_dualized_header_module_id_addr;
	finfo->rom_header_sensor_id_addr =	finfo->rom_dualized_header_sensor_id_addr;
	finfo->rom_header_mtf_data_addr = finfo->rom_dualized_header_mtf_data_addr;
	finfo->rom_awb_master_addr = finfo->rom_dualized_awb_master_addr;
	finfo->rom_awb_module_addr = finfo->rom_dualized_awb_module_addr;

	for (i = 0 ; i < AF_CAL_MAX; i++)
		finfo->rom_af_cal_addr[i] = finfo->rom_dualized_af_cal_addr[i];
	finfo->rom_af_cal_addr_len = finfo->rom_dualized_af_cal_addr_len;
	finfo->rom_paf_cal_start_addr = finfo->rom_dualized_paf_cal_start_addr;

#ifdef CONFIG_SEC_CAL_ENABLE
	/* standard cal */
	finfo->use_standard_cal = finfo->use_dualized_standard_cal;
	memcpy(&finfo->standard_cal_data, &finfo->standard_dualized_cal_data, sizeof(struct rom_standard_cal_data));
	memcpy(&finfo->backup_standard_cal_data, &finfo->backup_standard_dualized_cal_data, sizeof(struct rom_standard_cal_data));
#endif

	finfo->rom_header_sensor2_id_addr = finfo->rom_dualized_header_sensor2_id_addr;
	finfo->rom_header_sensor2_version_start_addr = finfo->rom_dualized_header_sensor2_version_start_addr;
	finfo->rom_header_sensor2_mtf_data_addr = finfo->rom_dualized_header_sensor2_mtf_data_addr;
	finfo->rom_sensor2_awb_master_addr = finfo->rom_dualized_sensor2_awb_master_addr;
	finfo->rom_sensor2_awb_module_addr = finfo->rom_dualized_sensor2_awb_module_addr;

	for (i = 0 ; i < AF_CAL_MAX; i++)
		finfo->rom_sensor2_af_cal_addr[i] = finfo->rom_dualized_sensor2_af_cal_addr[i];

	finfo->rom_sensor2_af_cal_addr_len = finfo->rom_dualized_sensor2_af_cal_addr_len;
	finfo->rom_sensor2_paf_cal_start_addr = finfo->rom_dualized_sensor2_paf_cal_start_addr;

	finfo->rom_dualcal_slave0_start_addr = finfo->rom_dualized_dualcal_slave0_start_addr;
	finfo->rom_dualcal_slave0_size = finfo->rom_dualized_dualcal_slave0_size;
	finfo->rom_dualcal_slave1_start_addr = finfo->rom_dualized_dualcal_slave1_start_addr;
	finfo->rom_dualcal_slave1_size = finfo->rom_dualized_dualcal_slave1_size;
	finfo->rom_dualcal_slave2_start_addr = finfo->rom_dualized_dualcal_slave2_start_addr;
	finfo->rom_dualcal_slave2_size = finfo->rom_dualized_dualcal_slave2_size;

	finfo->rom_pdxtc_cal_data_start_addr = finfo->rom_dualized_pdxtc_cal_data_start_addr;
	finfo->rom_pdxtc_cal_data_0_size = finfo->rom_dualized_pdxtc_cal_data_0_size;
	finfo->rom_pdxtc_cal_data_1_size = finfo->rom_dualized_pdxtc_cal_data_1_size;

	finfo->rom_spdc_cal_data_start_addr = finfo->rom_dualized_spdc_cal_data_start_addr;
	finfo->rom_spdc_cal_data_size = finfo->rom_dualized_spdc_cal_data_size;
	finfo->rom_xtc_cal_data_start_addr = finfo->rom_dualized_xtc_cal_data_start_addr;
	finfo->rom_xtc_cal_data_size = finfo->rom_dualized_xtc_cal_data_size;
	finfo->rom_pdxtc_cal_endian_check = finfo->rom_dualized_pdxtc_cal_endian_check;

	for (i = 0 ; i < CROSSTALK_CAL_MAX; i++)
		finfo->rom_pdxtc_cal_data_addr_list[i] = finfo->rom_dualized_pdxtc_cal_data_addr_list[i];

	finfo->rom_pdxtc_cal_data_addr_list_len = finfo->rom_dualized_pdxtc_cal_data_addr_list_len;
	finfo->rom_gcc_cal_endian_check = finfo->rom_dualized_gcc_cal_endian_check;

	for (i = 0 ; i < CROSSTALK_CAL_MAX; i++)
		finfo->rom_gcc_cal_data_addr_list[i] = finfo->rom_dualized_gcc_cal_data_addr_list[i];

	finfo->rom_gcc_cal_data_addr_list_len = finfo->rom_dualized_gcc_cal_data_addr_list_len;
	finfo->rom_xtc_cal_endian_check = finfo->rom_dualized_xtc_cal_endian_check;

	for (i = 0 ; i < CROSSTALK_CAL_MAX; i++)
		finfo->rom_xtc_cal_data_addr_list[i] = finfo->rom_dualized_xtc_cal_data_addr_list[i];

	finfo->rom_xtc_cal_data_addr_list_len = finfo->rom_dualized_xtc_cal_data_addr_list_len;

	finfo->rom_dualcal_slave1_cropshift_x_addr = finfo->rom_dualized_dualcal_slave1_cropshift_x_addr;
	finfo->rom_dualcal_slave1_cropshift_y_addr = finfo->rom_dualized_dualcal_slave1_cropshift_y_addr;
	finfo->rom_dualcal_slave1_oisshift_x_addr = finfo->rom_dualized_dualcal_slave1_oisshift_x_addr;
	finfo->rom_dualcal_slave1_oisshift_y_addr = finfo->rom_dualized_dualcal_slave1_oisshift_y_addr;
	finfo->rom_dualcal_slave1_dummy_flag_addr = finfo->rom_dualized_dualcal_slave1_dummy_flag_addr;

#ifdef CONFIG_SEC_CAL_ENABLE
	standard_cal_data->rom_standard_cal_start_addr = standard_cal_data->rom_dualized_standard_cal_start_addr;
	standard_cal_data->rom_standard_cal_end_addr = standard_cal_data->rom_dualized_standard_cal_end_addr;
	standard_cal_data->rom_standard_cal_module_crc_addr = standard_cal_data->rom_dualized_standard_cal_module_crc_addr;
	standard_cal_data->rom_standard_cal_module_checksum_len = standard_cal_data->rom_dualized_standard_cal_module_checksum_len;
	standard_cal_data->rom_standard_cal_sec2lsi_end_addr = standard_cal_data->rom_dualized_standard_cal_sec2lsi_end_addr;
	standard_cal_data->rom_header_standard_cal_end_addr = standard_cal_data->rom_dualized_header_standard_cal_end_addr;
	standard_cal_data->rom_header_main_shading_end_addr = standard_cal_data->rom_dualized_header_main_shading_end_addr;
	standard_cal_data->rom_awb_start_addr = standard_cal_data->rom_dualized_awb_start_addr;
	standard_cal_data->rom_awb_end_addr = standard_cal_data->rom_dualized_awb_end_addr;
	standard_cal_data->rom_awb_section_crc_addr = standard_cal_data->rom_dualized_awb_section_crc_addr;
	standard_cal_data->rom_shading_start_addr = standard_cal_data->rom_dualized_shading_start_addr;
	standard_cal_data->rom_shading_end_addr = standard_cal_data->rom_dualized_shading_end_addr;
	standard_cal_data->rom_shading_section_crc_addr = standard_cal_data->rom_dualized_shading_section_crc_addr;
	standard_cal_data->rom_factory_start_addr = standard_cal_data->rom_dualized_factory_start_addr;
	standard_cal_data->rom_factory_end_addr = standard_cal_data->rom_dualized_factory_end_addr;
	standard_cal_data->rom_awb_sec2lsi_start_addr = standard_cal_data->rom_dualized_awb_sec2lsi_start_addr;
	standard_cal_data->rom_awb_sec2lsi_end_addr = standard_cal_data->rom_dualized_awb_sec2lsi_end_addr;
	standard_cal_data->rom_awb_sec2lsi_checksum_addr = standard_cal_data->rom_dualized_awb_sec2lsi_checksum_addr;
	standard_cal_data->rom_awb_sec2lsi_checksum_len = standard_cal_data->rom_dualized_awb_sec2lsi_checksum_len;
	standard_cal_data->rom_shading_sec2lsi_start_addr = standard_cal_data->rom_dualized_shading_sec2lsi_start_addr;
	standard_cal_data->rom_shading_sec2lsi_end_addr = standard_cal_data->rom_dualized_shading_sec2lsi_end_addr;
	standard_cal_data->rom_shading_sec2lsi_checksum_addr = standard_cal_data->rom_dualized_shading_sec2lsi_checksum_addr;
	standard_cal_data->rom_shading_sec2lsi_checksum_len = standard_cal_data->rom_dualized_shading_sec2lsi_checksum_len;
	standard_cal_data->rom_factory_sec2lsi_start_addr = standard_cal_data->rom_dualized_factory_sec2lsi_start_addr;

	backup_standard_cal_data->rom_standard_cal_start_addr = backup_standard_cal_data->rom_dualized_standard_cal_start_addr;
	backup_standard_cal_data->rom_standard_cal_end_addr = backup_standard_cal_data->rom_dualized_standard_cal_end_addr;
	backup_standard_cal_data->rom_standard_cal_module_crc_addr = backup_standard_cal_data->rom_dualized_standard_cal_module_crc_addr;
	backup_standard_cal_data->rom_standard_cal_module_checksum_len = backup_standard_cal_data->rom_dualized_standard_cal_module_checksum_len;
	backup_standard_cal_data->rom_standard_cal_sec2lsi_end_addr = backup_standard_cal_data->rom_dualized_standard_cal_sec2lsi_end_addr;
	backup_standard_cal_data->rom_header_standard_cal_end_addr = backup_standard_cal_data->rom_dualized_header_standard_cal_end_addr;
	backup_standard_cal_data->rom_header_main_shading_end_addr = backup_standard_cal_data->rom_dualized_header_main_shading_end_addr;
	backup_standard_cal_data->rom_awb_start_addr = backup_standard_cal_data->rom_dualized_awb_start_addr;
	backup_standard_cal_data->rom_awb_end_addr = backup_standard_cal_data->rom_dualized_awb_end_addr;
	backup_standard_cal_data->rom_awb_section_crc_addr = backup_standard_cal_data->rom_dualized_awb_section_crc_addr;
	backup_standard_cal_data->rom_shading_start_addr = backup_standard_cal_data->rom_dualized_shading_start_addr;
	backup_standard_cal_data->rom_shading_end_addr = backup_standard_cal_data->rom_dualized_shading_end_addr;
	backup_standard_cal_data->rom_shading_section_crc_addr = backup_standard_cal_data->rom_dualized_shading_section_crc_addr;
	backup_standard_cal_data->rom_factory_start_addr = backup_standard_cal_data->rom_dualized_factory_start_addr;
	backup_standard_cal_data->rom_factory_end_addr = backup_standard_cal_data->rom_dualized_factory_end_addr;
	backup_standard_cal_data->rom_awb_sec2lsi_start_addr = backup_standard_cal_data->rom_dualized_awb_sec2lsi_start_addr;
	backup_standard_cal_data->rom_awb_sec2lsi_end_addr = backup_standard_cal_data->rom_dualized_awb_sec2lsi_end_addr;
	backup_standard_cal_data->rom_awb_sec2lsi_checksum_addr = backup_standard_cal_data->rom_dualized_awb_sec2lsi_checksum_addr;
	backup_standard_cal_data->rom_awb_sec2lsi_checksum_len = backup_standard_cal_data->rom_dualized_awb_sec2lsi_checksum_len;
	backup_standard_cal_data->rom_shading_sec2lsi_start_addr = backup_standard_cal_data->rom_dualized_shading_sec2lsi_start_addr;
	backup_standard_cal_data->rom_shading_sec2lsi_end_addr = backup_standard_cal_data->rom_dualized_shading_sec2lsi_end_addr;
	backup_standard_cal_data->rom_shading_sec2lsi_checksum_addr = backup_standard_cal_data->rom_dualized_shading_sec2lsi_checksum_addr;
	backup_standard_cal_data->rom_shading_sec2lsi_checksum_len = backup_standard_cal_data->rom_dualized_shading_sec2lsi_checksum_len;
	backup_standard_cal_data->rom_factory_sec2lsi_start_addr = backup_standard_cal_data->rom_dualized_factory_sec2lsi_start_addr;
#endif

	finfo->is_read_dualized_values = true;
	pr_info("%s EEPROM dualized values read\n",__func__);
EXIT:
	return ret;
}
#endif

int is_sec_readcal_eeprom(int rom_id)
{
	int ret = 0;
	int count = 0;
	char *buf = NULL;
	int retry = IS_CAL_RETRY_CNT;
	int crc_check = false;
	struct is_core *core = is_get_is_core();
	struct is_rom_info *finfo = NULL;
	struct is_vender_specific *specific = core->vender.private_data;
	int cal_size = 0;
	u32 read_addr = 0x0;
	struct i2c_client *client = NULL;
	struct is_device_eeprom *eeprom;
#if defined(CONFIG_CAMERA_EEPROM_DUALIZED)
	int rom_type;
	struct is_module_enum *module;
	int position = is_vendor_get_position_from_rom_id(rom_id);
	int sensor_id = is_vendor_get_sensor_id_from_position(position);
#endif

#ifdef CONFIG_SEC_CAL_ENABLE
	char *buf_rom_data = NULL;
#endif

	is_sec_get_sysfs_finfo(&finfo, rom_id);
	is_sec_get_cal_buf(&buf, rom_id);

	client = specific->rom_client[rom_id][0];
	eeprom = i2c_get_clientdata(client);
	cal_size = finfo->rom_size;

	info("Camera: read cal data from EEPROM (rom_id:%d, cal_size:%d)\n", rom_id, cal_size);

#if defined (CONFIG_CAMERA_EEPROM_DUALIZED)
	if (position == SENSOR_POSITION_MAX) {
		err("position is invalid (rom_id : %d)", rom_id);
		ret = false;
		goto exit;
	}

	is_vendor_get_module_from_position(position, &module);

	if (!module) {
		err("There is no module (position : %d)", position);
		ret = false;
		goto exit;
	}
	rom_type = module->pdata->rom_type;

/* to do check : Do not use a specific sensor id in common code. */
	if (!(sensor_id == SENSOR_NAME_IMX616 || sensor_id == SENSOR_NAME_IMX882))	//put all dualized sensor names here
		finfo->is_read_dualized_values = true;
	if (!(finfo->is_read_dualized_values)){
		is_sec_readcal_eeprom_dualized(rom_id); // call this function only once for EEPROM dualized sensors
	}
#endif

	if (finfo->rom_header_cal_map_ver_start_addr != -1) {
		ret = is_i2c_read(client, finfo->cal_map_ver,
					finfo->rom_header_cal_map_ver_start_addr,
					IS_CAL_MAP_VER_SIZE);

		if (unlikely(ret)) {
			err("failed to is_i2c_read (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}
	}

	if (finfo->rom_header_version_start_addr != -1) {
		ret = is_i2c_read(client, finfo->header_ver,
					finfo->rom_header_version_start_addr,
					IS_HEADER_VER_SIZE);

		if (unlikely(ret)) {
			err("failed to is_i2c_read (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}
	}

	info("Camera : EEPROM Cal map_version = %s(%x%x%x%x)\n", finfo->cal_map_ver, finfo->cal_map_ver[0],
			finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);
	info("EEPROM header version = %s(%x%x%x%x)\n", finfo->header_ver,
		finfo->header_ver[0], finfo->header_ver[1], finfo->header_ver[2], finfo->header_ver[3]);

	if (!is_sec_check_rom_ver(core, rom_id)) {
		info("Camera: Do not read eeprom cal data. EEPROM version is low.\n");
		return 0;
	}

crc_retry:
	/* read cal data */
	read_addr = 0x0;
	if (cal_size > IS_READ_MAX_EEP_CAL_SIZE) {
		for (count = 0; count < cal_size/IS_READ_MAX_EEP_CAL_SIZE; count++) {
			info("Camera: I2C read cal data : offset = 0x%x, size = 0x%x\n", read_addr, IS_READ_MAX_EEP_CAL_SIZE);
			ret = is_i2c_read(client, &buf[read_addr], read_addr, IS_READ_MAX_EEP_CAL_SIZE);
			if (ret) {
				err("failed to is_i2c_read (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
			read_addr += IS_READ_MAX_EEP_CAL_SIZE;
		}

		if (read_addr < cal_size) {
			ret = is_i2c_read(client, &buf[read_addr], read_addr, cal_size - read_addr);
		}
	} else {
		info("Camera: I2C read cal data\n");
		ret = is_i2c_read(client, buf, read_addr, cal_size);
		if (ret) {
			err("failed to is_i2c_read (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}
	}

	/* CRC check */
	crc_check = is_sec_check_cal_crc32(buf, rom_id);
	if (!crc_check && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

	is_sec_parse_rom_info(finfo, buf, rom_id);

	is_sec_check_module_state(finfo);

#ifdef USE_CAMERA_NOTIFY_WACOM
	if (!test_bit(IS_CRC_ERROR_HEADER, &finfo->crc_error))
		is_eeprom_info_update(rom_id, finfo->header_ver);
#endif

#ifdef CONFIG_SEC_CAL_ENABLE
	/* Store original rom data before conversion for intrinsic cal */
	if (crc_check == true && is_need_use_standard_cal(rom_id)) {
		is_sec_get_cal_buf_rom_data(&buf_rom_data, rom_id);
		if (buf != NULL && buf_rom_data != NULL)
			memcpy(buf_rom_data, buf, is_sec_get_max_cal_size(core, rom_id));
	}
#endif
exit:
	return ret;
}

int is_sec_sensorid_find(struct is_core *core)
{
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;

	is_sec_get_sysfs_finfo(&finfo, ROM_ID_REAR);

	info("%s sensor id %d\n", __func__, specific->sensor_id[SENSOR_POSITION_REAR]);

	return 0;
}

int is_sec_sensorid_find_front(struct is_core *core)
{
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;

	is_sec_get_sysfs_finfo(&finfo, ROM_ID_FRONT);

	info("%s sensor id %d\n", __func__, specific->sensor_id[SENSOR_POSITION_FRONT]);

	return 0;
}

int is_get_dual_cal_buf(int slave_position, char **buf, int *size)
{
	char *cal_buf;
	int rom_dual_cal_start_addr;
	int rom_dual_cal_size;
	u32 rom_dual_flag_dummy_addr = 0;
	int rom_type;
	int rom_dualcal_id;
	int rom_dualcal_index;
	struct is_core *core = is_get_is_core();
	struct is_rom_info *finfo = NULL;

	*buf = NULL;
	*size = 0;

	is_vendor_get_rom_dualcal_info_from_position(slave_position, &rom_type, &rom_dualcal_id, &rom_dualcal_index);
	if (rom_type == ROM_TYPE_NONE) {
		err("[rom_dualcal_id:%d pos:%d] not support, no rom for camera", rom_dualcal_id, slave_position);
		return -EINVAL;
	} else if (rom_dualcal_id == ROM_ID_NOTHING) {
		err("[rom_dualcal_id:%d pos:%d] invalid ROM ID", rom_dualcal_id, slave_position);
		return -EINVAL;
	}

	is_sec_get_cal_buf(&cal_buf, rom_dualcal_id);
	if (is_sec_check_rom_ver(core, rom_dualcal_id) == false) {
		err("[rom_dualcal_id:%d pos:%d] ROM version is low. Cannot load dual cal.",
			rom_dualcal_id, slave_position);
		return -EINVAL;
	}

	is_sec_get_sysfs_finfo(&finfo, rom_dualcal_id);
	if (test_bit(IS_CRC_ERROR_ALL_SECTION, &finfo->crc_error) ||
		test_bit(IS_CRC_ERROR_DUAL_CAMERA, &finfo->crc_error)) {
		err("[rom_dualcal_id:%d pos:%d] ROM Cal CRC is wrong. Cannot load dual cal.",
			rom_dualcal_id, slave_position);
		return -EINVAL;
	}

	if (rom_dualcal_index == ROM_DUALCAL_SLAVE0) {
		rom_dual_cal_start_addr = finfo->rom_dualcal_slave0_start_addr;
		rom_dual_cal_size = finfo->rom_dualcal_slave0_size;
	} else if (rom_dualcal_index == ROM_DUALCAL_SLAVE1) {
		rom_dual_cal_start_addr = finfo->rom_dualcal_slave1_start_addr;
		rom_dual_cal_size = finfo->rom_dualcal_slave1_size;
		rom_dual_flag_dummy_addr = finfo->rom_dualcal_slave1_dummy_flag_addr;
	} else {
		err("[index:%d] not supported index.", rom_dualcal_index);
		return -EINVAL;
	}

	if (rom_dual_flag_dummy_addr != 0 && cal_buf[rom_dual_flag_dummy_addr] != 7) {
		err("[rom_dualcal_id:%d pos:%d addr:%x] invalid dummy_flag [%d]. Cannot load dual cal.",
			rom_dualcal_id, slave_position, rom_dual_flag_dummy_addr, cal_buf[rom_dual_flag_dummy_addr]);
		return -EINVAL;
	}

	if (rom_dual_cal_start_addr <= 0) {
		info("[%s] not available dual_cal\n", __func__);
		return -EINVAL;
	}

	*buf = &cal_buf[rom_dual_cal_start_addr];
	*size = rom_dual_cal_size;

	return 0;
}

int is_sec_read_rom(int rom_id)
{
	int ret = 0;
	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;
	struct is_rom_info *pinfo = NULL;
	bool camera_running;
	bool rom_power_on;
	struct is_module_enum *module = NULL;
	int rom_type;
	int position = is_vendor_get_position_from_rom_id(rom_id);
	int sensor_id = is_vendor_get_sensor_id_from_position(position);
	int dualized_sensor_id = is_vender_get_dualized_sensorid(position);

	if (position == SENSOR_POSITION_MAX) {
		err("position is invalid");
		ret = -EINVAL;
		return ret;
	}

	is_vendor_get_module_from_position(position, &module);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		return ret;
	}

	rom_type = module->pdata->rom_type;
	if (rom_type != ROM_TYPE_EEPROM && rom_type != ROM_TYPE_OTPROM) {
		err("unknown rom_type(%d)", rom_type);
		ret = -EINVAL;
		return ret;
	}

	is_sec_get_sysfs_pinfo(&pinfo, rom_id);
	is_sec_get_sysfs_finfo(&finfo, rom_id);

	rom_power_on = false;

	/* Use mutex for i2c read */
	mutex_lock(&specific->rom_lock);

	camera_running = is_vendor_check_camera_running(finfo->rom_power_position);
	if (!camera_running) {
		is_sec_rom_power_on(core, finfo->rom_power_position);
		rom_power_on = true;
	}

	if (rom_type == ROM_TYPE_EEPROM) {
		ret = is_sec_readcal_eeprom(rom_id);
	} else if (rom_type == ROM_TYPE_OTPROM) {
		struct is_device_sensor_peri *sensor_peri;
		sensor_peri = (struct is_device_sensor_peri *)module->private_data;

		if (sensor_peri->cis.client) {
			if (specific->rom_valid[rom_id][1] &&
				sensor_id == dualized_sensor_id)
				specific->rom_client[rom_id][1] = sensor_peri->cis.client;
			else
				specific->rom_client[rom_id][0] = sensor_peri->cis.client;
			info("%s sensor client will be used for otprom", __func__);
		}

		ret = is_sec_readcal_otprom(rom_id);
	}

	if (!ret)
		set_bit(IS_ROM_STATE_CAL_READ_DONE, &finfo->rom_state);
	else
		err("CAL_READ_DONE ERR rom_id(%d) rom_type(%d)", rom_id, rom_type);

	if (rom_id == ROM_ID_REAR)
		is_sec_sensorid_find(core);

	camera_running = is_vendor_check_camera_running(finfo->rom_power_position);
	if (rom_power_on && !camera_running) {
		is_sec_rom_power_off(core, finfo->rom_power_position);
		msleep(20);
	}

	mutex_unlock(&specific->rom_lock);

	return ret;
}

int is_sec_run_fw_sel(int rom_id)
{
	struct is_core *core = is_get_is_core();
	int ret = 0;
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;

	is_sec_get_sysfs_finfo(&finfo, rom_id);

	if (test_bit(IS_ROM_STATE_SKIP_CAL_LOADING, &finfo->rom_state)) {
		info("%s: skipping cal read as skip_cal_loading is enabled for rom_id %d", __func__, rom_id);
		return 0;
	}

	if (!test_bit(IS_ROM_STATE_CAL_READ_DONE, &finfo->rom_state) || force_caldata_dump) {
		ret = is_sec_read_rom(rom_id);
	}

	if (specific->check_sensor_vendor) {
		if (is_sec_check_rom_ver(core, rom_id)) {
			is_sec_check_module_state(finfo);
			if (test_bit(IS_ROM_STATE_OTHER_VENDOR, &finfo->rom_state)) {
				err("Not supported module. Rom ID = %d, Module ver = %s", rom_id, finfo->header_ver);
				return  -EIO;
			}
		} else {
			info("Camera: Do not read eeprom cal data. EEPROM version is low.\n");
			return 0;
		}
	}

	return ret;
}

int is_get_remosaic_cal_buf(int sensor_position, char **buf, int *size)
{
	int ret = -1;
	char *cal_buf;
	u32 start_addr;
	struct is_rom_info *finfo;
#ifdef MODIFY_CAL_MAP_FOR_SWREMOSAIC_LIB
/* to do check */
	int sensor_id = is_vendor_get_sensor_id_from_position(sensor_position);
#endif

	ret = is_sec_get_cal_buf(&cal_buf, sensor_position);
	if (ret < 0) {
		err("[%s]: get_cal_buf fail", __func__);
		return ret;
	}

	ret = is_sec_get_sysfs_finfo_by_position(sensor_position, &finfo);
	if (ret < 0) {
		err("[%s]: get_sysfs_finfo fail", __func__);
		return -EINVAL;
	}

	start_addr = finfo->rom_xtc_cal_data_start_addr;

#ifdef MODIFY_CAL_MAP_FOR_SWREMOSAIC_LIB
// Modify cal_buf for some sensors (if required)
	switch(sensor_id) {
		case SENSOR_NAME_S5KJN1:
			ret = is_modify_remosaic_cal_buf(sensor_position, cal_buf, buf, size);
			break;
		default:
			*buf  = &cal_buf[start_addr];
			*size = finfo->rom_xtc_cal_data_size;
			break;
	}
#else
	*buf  = &cal_buf[start_addr];
	*size = finfo->rom_xtc_cal_data_size;
#endif
	return ret;
}

#ifdef MODIFY_CAL_MAP_FOR_SWREMOSAIC_LIB
int is_modify_remosaic_cal_buf(int sensor_position, char *cal_buf, char **buf, int *size)
{
	int ret = -1;
	char *modified_cal_buf;
	u32 start_addr;
	u32 cal_size = 0;
	u32 curr_pos = 0;

	struct is_rom_info *finfo;

	ret = is_sec_get_sysfs_finfo_by_position(sensor_position, &finfo);
	if (ret < 0) {
		err("[%s]: get_sysfs_finfo fail", __func__);
		return -EINVAL;
	}

	cal_size = finfo->rear_remosaic_tetra_xtc_size + finfo->rear_remosaic_sensor_xtc_size + finfo->rear_remosaic_pdxtc_size
		+ finfo->rear_remosaic_sw_ggc_size;

	start_addr = finfo->rom_xtc_cal_data_start_addr;

	if (cal_size <= 0 ) {
		err("[%s]: invalid cal_size(%d)",
			__func__, cal_size);
		return -EINVAL;
	}

	modified_cal_buf = (char *)kmalloc(cal_size * sizeof(char), GFP_KERNEL);

	memcpy(&modified_cal_buf[curr_pos], &cal_buf[finfo->rear_remosaic_tetra_xtc_start_addr], finfo->rear_remosaic_tetra_xtc_size);
	curr_pos += finfo->rear_remosaic_tetra_xtc_size;

	memcpy(&modified_cal_buf[curr_pos], &cal_buf[finfo->rear_remosaic_sensor_xtc_start_addr], finfo->rear_remosaic_sensor_xtc_size);
	curr_pos += finfo->rear_remosaic_sensor_xtc_size;

	memcpy(&modified_cal_buf[curr_pos], &cal_buf[finfo->rear_remosaic_pdxtc_start_addr], finfo->rear_remosaic_pdxtc_size);
	curr_pos += finfo->rear_remosaic_pdxtc_size;

	memcpy(&modified_cal_buf[curr_pos], &cal_buf[finfo->rear_remosaic_sw_ggc_start_addr], finfo->rear_remosaic_sw_ggc_size);

	*buf  = &modified_cal_buf[0];
	*size = cal_size;

	return 0;
}
#endif
