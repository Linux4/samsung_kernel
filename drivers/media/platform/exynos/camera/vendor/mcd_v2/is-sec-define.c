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

#define IS_DEFAULT_CAL_SIZE (20 * 1024)
#define IS_DUMP_CAL_SIZE (172 * 1024)
#define IS_LATEST_ROM_VERSION_M 'M'

#define IS_READ_MAX_EEP_CAL_SIZE (32 * 1024)

#define I2C_READ 4
#define I2C_WRITE 3
#define I2C_BYTE 2
#define I2C_DATA 1
#define I2C_ADDR 0

#define NUM_OF_DUALIZATION_CHECK 6

bool force_caldata_dump = false;

#ifdef USES_STANDARD_CAL_RELOAD
bool sec2lsi_reload = false;
#endif

static int cam_id = CAMERA_SINGLE_REAR;
bool is_dumped_fw_loading_needed = false;
static struct is_rom_info sysfs_finfo[ROM_ID_MAX];
static struct is_rom_info sysfs_pinfo[ROM_ID_MAX];
#if defined(CAMERA_UWIDE_DUALIZED)
static bool rear3_dualized_rom_probe = false;
static struct is_rom_info sysfs_finfo_rear3_otp;
#endif
static char rom_buf[ROM_ID_MAX][IS_MAX_CAL_SIZE];
char loaded_fw[IS_HEADER_VER_SIZE + 1] = {
	0,
};
char loaded_companion_fw[30] = {
	0,
};

static u32 is_check_dualized_sensor[SENSOR_POSITION_MAX] = { false };

#ifdef CONFIG_SEC_CAL_ENABLE
#ifdef USE_KERNEL_VFS_READ_WRITE
static char *eeprom_cal_dump_path[ROM_ID_MAX] = {
	"dump/eeprom_rear_cal.bin",  "dump/eeprom_front_cal.bin",
	"dump/eeprom_rear2_cal.bin", "dump/eeprom_front2_cal.bin",
	"dump/eeprom_rear3_cal.bin", "dump/eeprom_front3_cal.bin",
	"dump/eeprom_rear4_cal.bin", "dump/eeprom_front4_cal.bin",
};

static char *otprom_cal_dump_path[ROM_ID_MAX] = {
	"dump/otprom_rear_cal.bin",  "dump/otprom_front_cal.bin",
	"dump/otprom_rear2_cal.bin", "dump/otprom_front2_cal.bin",
	"dump/otprom_rear3_cal.bin", "dump/otprom_front3_cal.bin",
	"dump/otprom_rear4_cal.bin", "dump/otprom_front4_cal.bin",
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

int is_sec_set_registers(struct i2c_client *client, const u32 *regs,
			 const u32 size, int position)
{
	int ret = 0;
	int i = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	is_vendor_get_module_from_position(position, &module);

	BUG_ON(!regs);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	if (cis == NULL) {
		err("cis is NULL");
		return -1;
	}

	for (i = 0; i < size; i += I2C_WRITE) {
		if (regs[i + I2C_BYTE] == I2C_WRITE_FORMAT_ADDR8_DATA8) {
			ret = is_sensor_addr8_write8(client, regs[i + I2C_ADDR],
							 regs[i + I2C_DATA]);
			if (ret < 0) {
				err("is_sensor_addr8_write8 fail, ret(%d), addr(%#x), data(%#x)",
					ret, regs[i + I2C_ADDR],
					regs[i + I2C_DATA]);
				break;
			}
		} else if (regs[i + I2C_BYTE] ==
			   I2C_WRITE_FORMAT_ADDR16_DATA8) {
			ret = is_sensor_write8(client, regs[i + I2C_ADDR],
						   regs[i + I2C_DATA]);
			if (ret < 0) {
				err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)",
					ret, regs[i + I2C_ADDR],
					regs[i + I2C_DATA]);
				break;
			}
		} else if (regs[i + I2C_BYTE] ==
			   I2C_WRITE_FORMAT_ADDR16_DATA16) {
			ret = is_sensor_write16(client, regs[i + I2C_ADDR],
						regs[i + I2C_DATA]);
			if (ret < 0) {
				err("is_sensor_write16 fail, ret(%d), addr(%#x), data(%#x)",
					ret, regs[i + I2C_ADDR],
					regs[i + I2C_DATA]);
				break;
			}
		}
	}
	return ret;
}

#if defined(CAMERA_UWIDE_DUALIZED)
void is_sec_set_rear3_dualized_rom_probe(void)
{
	rear3_dualized_rom_probe = true;
}
EXPORT_SYMBOL_GPL(is_sec_set_rear3_dualized_rom_probe);
#endif

int is_sec_get_sysfs_finfo(struct is_rom_info **finfo, int rom_id)
{
#if defined(CAMERA_UWIDE_DUALIZED)
	if (rom_id == ROM_ID_REAR3 && rear3_dualized_rom_probe) {
		*finfo = &sysfs_finfo_rear3_otp;
		rear3_dualized_rom_probe = false;
	} else {
		*finfo = &sysfs_finfo[rom_id];
	}
#else
	*finfo = &sysfs_finfo[rom_id];
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(is_sec_get_sysfs_finfo);

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

int is_sec_get_max_cal_size(struct is_core *core, int rom_id)
{
	int size = 0;
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;

	if (!specific->rom_valid[rom_id]) {
		err("Invalid rom_id[%d]. This rom_id don't have rom!\n",
			rom_id);
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

#ifdef USES_STANDARD_CAL_RELOAD
bool is_sec_sec2lsi_check_cal_reload(void)
{
	info("%s is_sec_sec2lsi_check_cal_reload=%d\n", __func__,
		 sec2lsi_reload);
	return sec2lsi_reload;
}

void is_sec_sec2lsi_set_cal_reload(bool val)
{
	sec2lsi_reload = val;
}
#endif

#ifdef CONFIG_SEC_CAL_ENABLE

bool is_sec_readcal_dump_post_sec2lsi(struct is_core *core, char *buf,
					  int position)
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
		err("%s: There is no cal map (rom_id : %d)\n", __func__,
			rom_id);
		ret = false;
		goto EXIT;
	}

	is_vendor_get_module_from_position(position, &module);

	if (!module) {
		err("%s: There is no module (position : %d)\n", __func__,
			position);
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

bool is_sec_check_awb_lsc_crc32_post_sec2lsi(char *buf, int position,
						 int awb_length, int lsc_length)
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
		printk(KERN_INFO
			   "[CRC32_DEBUG] AWB CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
			   awb_length, standard_cal_data->rom_awb_section_crc_addr);
		printk(KERN_INFO "[CRC32_DEBUG] start = 0x%08X, end = 0x%08X\n",
			   standard_cal_data->rom_awb_start_addr,
			   standard_cal_data->rom_awb_end_addr);
#endif

		if (check_base > address_boundary ||
			checksum_base > address_boundary) {
			err("Camera[%d]: AWB address has error: start(0x%08X), end(0x%08X)",
				position, standard_cal_data->rom_awb_start_addr,
				standard_cal_data->rom_awb_end_addr);
			crc32_check_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], awb_length,
					   NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera[%d]: AWB address has error: start(0x%08X), end(0x%08X) Crc address(0x%08X)",
				position, standard_cal_data->rom_awb_start_addr,
				standard_cal_data->rom_awb_end_addr,
				standard_cal_data->rom_awb_section_crc_addr);
			err("Camera[%d]: CRC32 error at the AWB (0x%08X != 0x%08X)",
				position, checksum, buf32[checksum_base]);
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
		checksum_base =
			standard_cal_data->rom_shading_section_crc_addr / 4;

#ifdef ROM_CRC32_DEBUG
		printk(KERN_INFO
			   "[CRC32_DEBUG] Shading CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
			   lsc_length,
			   standard_cal_data->rom_shading_section_crc_addr);
		printk(KERN_INFO "[CRC32_DEBUG] start = 0x%08X, end = 0x%08X\n",
			   standard_cal_data->rom_shading_start_addr,
			   standard_cal_data->rom_shading_end_addr);
#endif

		if (check_base > address_boundary ||
			checksum_base > address_boundary) {
			err("Camera[%d]: Shading address has error: start(0x%08X), end(0x%08X)",
				position, standard_cal_data->rom_shading_start_addr,
				standard_cal_data->rom_shading_end_addr);
			crc32_check_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], lsc_length,
					   NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera[%d]: Shading address has error: start(0x%08X), end(0x%08X) Crc address(0x%08X)",
				position, standard_cal_data->rom_shading_start_addr,
				standard_cal_data->rom_shading_end_addr,
				standard_cal_data->rom_shading_section_crc_addr);
			err("Camera[%d]: CRC32 error at the Shading (0x%08X != 0x%08X)",
				position, checksum, buf32[checksum_base]);
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
	is_cal_reload =
		test_bit(IS_ROM_STATE_CAL_RELOAD, &default_finfo->rom_state);
	// todo cal reaload part
	info("%s: Sensor running = %d\n", __func__, is_running_camera);
	if (crc32_check_temp && is_cal_reload == true &&
		is_running_camera == true) {
		for (i = 0; i < SENSOR_POSITION_MAX; i++) {
			is_search_sensor_module_with_position(
				&core->sensor[i], position, &module);
			if (module)
				break;
		}

		if (!module) {
			err("%s: Could not find sensor id.", __func__);
			crc32_check_temp = false;
			goto out;
		}

		ret = is_vender_cal_load(&core->vender, module);
		if (ret < 0)
			err("(%s) Unable to sync cal, is_vender_cal_load failed\n",
				__func__);
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

int is_sec_set_camid(int id)
{
	cam_id = id;
	return 0;
}

int is_sec_get_camid(void)
{
	return cam_id;
}

int is_sec_fw_revision(char *fw_ver)
{
	int revision = 0;
	revision = revision + ((int)fw_ver[FW_PUB_YEAR] - 58) * 10000;
	revision = revision + ((int)fw_ver[FW_PUB_MON] - 64) * 100;
	revision = revision + ((int)fw_ver[FW_PUB_NUM] - 48) * 10;
	revision = revision + (int)fw_ver[FW_PUB_NUM + 1] - 48;

	return revision;
}

bool is_sec_fw_module_compare(char *fw_ver1, char *fw_ver2)
{
	if (fw_ver1[FW_CORE_VER] != fw_ver2[FW_CORE_VER] ||
		fw_ver1[FW_PIXEL_SIZE] != fw_ver2[FW_PIXEL_SIZE] ||
		fw_ver1[FW_PIXEL_SIZE + 1] != fw_ver2[FW_PIXEL_SIZE + 1] ||
		fw_ver1[FW_ISP_COMPANY] != fw_ver2[FW_ISP_COMPANY] ||
		fw_ver1[FW_SENSOR_MAKER] != fw_ver2[FW_SENSOR_MAKER]) {
		return false;
	}

	return true;
}

bool is_sec_fw_module_compare_for_dump(char *fw_ver1, char *fw_ver2)
{
	if (fw_ver1[FW_CORE_VER] != fw_ver2[FW_CORE_VER] ||
		fw_ver1[FW_PIXEL_SIZE] != fw_ver2[FW_PIXEL_SIZE] ||
		fw_ver1[FW_PIXEL_SIZE + 1] != fw_ver2[FW_PIXEL_SIZE + 1] ||
		fw_ver1[FW_ISP_COMPANY] != fw_ver2[FW_ISP_COMPANY]) {
		return false;
	}

	return true;
}

int is_sec_compare_ver(int rom_id)
{
	struct is_rom_info *finfo = NULL;

	is_sec_get_sysfs_finfo(&finfo, rom_id);

	if (finfo->cal_map_ver[0] == 'V' && finfo->cal_map_ver[1] >= '0' &&
		finfo->cal_map_ver[1] <= '9' && finfo->cal_map_ver[2] >= '0' &&
		finfo->cal_map_ver[2] <= '9' && finfo->cal_map_ver[3] >= '0' &&
		finfo->cal_map_ver[3] <= '9') {
		return ((finfo->cal_map_ver[2] - '0') * 10) +
			   (finfo->cal_map_ver[3] - '0');
	} else {
		err("ROM core version is invalid. version is %c%c%c%c",
			finfo->cal_map_ver[0], finfo->cal_map_ver[1],
			finfo->cal_map_ver[2], finfo->cal_map_ver[3]);
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

	if ((rom_ver < latest_rom_ver &&
		 finfo->rom_header_cal_map_ver_start_addr > 0) ||
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
		check_length = (finfo->header_crc_check_list[i + 1] -
				finfo->header_crc_check_list[i] + 1);
		checksum_base = finfo->header_crc_check_list[i + 2];

		if (check_base > address_boundary ||
			checksum_base > address_boundary || check_length <= 0) {
			err("[rom%d/header cal:%d] address has error: start(0x%08X), end(0x%08X)",
				rom_id, i, finfo->header_crc_check_list[i],
				finfo->header_crc_check_list[i + 1]);
			crc32_header_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf8[check_base], check_length,
					   NULL, NULL);
		if (checksum != *((u32 *)&buf8[checksum_base])) {
			err("[rom%d/header cal:%d] CRC32 error (0x%08X != 0x%08X), base[0x%X] len[0x%X] checksum[0x%X]",
				rom_id, i, checksum, *((u32 *)&buf8[checksum_base]),
				check_base, check_length, checksum_base);
			crc32_header_temp = false;
			goto out;
		}
	}

	/* main crc check */
	for (i = 0; i < finfo->crc_check_list_len; i += 3) {
		checksum = 0;
		check_base = finfo->crc_check_list[i];
		check_length = (finfo->crc_check_list[i + 1] -
				finfo->crc_check_list[i] + 1);
		checksum_base = finfo->crc_check_list[i + 2];

		if (check_base > address_boundary ||
			checksum_base > address_boundary || check_length <= 0) {
			err("[rom%d/main cal:%d] address has error: start(0x%08X), end(0x%08X)",
				rom_id, i, finfo->crc_check_list[i],
				finfo->crc_check_list[i + 1]);
			crc32_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf8[check_base], check_length,
					   NULL, NULL);
		if (checksum != *((u32 *)&buf8[checksum_base])) {
			err("[rom%d/main cal:%d] CRC32 error (0x%08X != 0x%08X), base[0x%X] len[0x%X] checksum[0x%X]",
				rom_id, i, checksum, *((u32 *)&buf8[checksum_base]),
				check_base, check_length, checksum_base);
			crc32_temp = false;
			goto out;
		}
	}

	/* dual crc check */
	for (i = 0; i < finfo->dual_crc_check_list_len; i += 3) {
		checksum = 0;
		check_base = finfo->dual_crc_check_list[i];
		check_length = (finfo->dual_crc_check_list[i + 1] -
				finfo->dual_crc_check_list[i] + 1);
		checksum_base = finfo->dual_crc_check_list[i + 2];

		if (check_base > address_boundary ||
			checksum_base > address_boundary || check_length <= 0) {
			err("[rom%d/dual cal:%d] data address has error: start(0x%08X), end(0x%08X)",
				rom_id, i, finfo->dual_crc_check_list[i],
				finfo->dual_crc_check_list[i + 1]);
			crc32_temp = false;
			crc32_dual_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf8[check_base], check_length,
					   NULL, NULL);
		if (checksum != *((u32 *)&buf8[checksum_base])) {
			err("[rom%d/main cal:%d] CRC32 error (0x%08X != 0x%08X), base[0x%X] len[0x%X] checksum[0x%X]",
				rom_id, i, checksum, *((u32 *)&buf8[checksum_base]),
				check_base, check_length, checksum_base);
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
		parent_inode = fp->f_path.dentry->d_parent->d_inode;
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
		parent_inode = fp->f_path.dentry->d_parent->d_inode;
		inode_lock(parent_inode);
		ret = vfs_unlink(parent_inode, fp->f_path.dentry, NULL);
		inode_unlock(parent_inode);
	} else {
		ret = -ENOENT;
	}

	info("sys_unlink (%s) %ld", fw_path, ret);

	set_fs(old_fs);

	is_dumped_fw_loading_needed = false;
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
			fp = filp_open(name,
					   O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
					   0666);
		} else {
			struct inode *parent_inode =
				fp->f_path.dentry->d_parent->d_inode;
			inode_lock(parent_inode);
			vfs_rmdir(parent_inode, fp->f_path.dentry);
			inode_unlock(parent_inode);
			fp = filp_open(name,
					   O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
					   0666);
		}
	} else {
		fp = filp_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC,
				   0666);
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
		info("%s: error %d, failed to open %s\n", __func__, PTR_ERR(fp),
			 name);
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
		info("%s: error %d, failed to open %s\n", __func__, PTR_ERR(fp),
			 name);
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

/**
 * is_sec_ldo_enabled: check whether the ldo has already been enabled.
 *
 * @ return: true, false or error value
 */
int is_sec_ldo_enabled(struct device *dev, char *name)
{
	struct regulator *regulator = NULL;
	int enabled = 0;

	regulator = regulator_get(dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get(%s) fail", __func__, name);
		return -EINVAL;
	}

	enabled = regulator_is_enabled(regulator);

	regulator_put(regulator);

	return enabled;
}

int is_sec_ldo_enable(struct device *dev, char *name, bool on)
{
	struct regulator *regulator = NULL;
	int ret = 0;

	regulator = regulator_get(dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get(%s) fail", __func__, name);
		return -EINVAL;
	}

	if (on) {
		if (regulator_is_enabled(regulator)) {
			pr_warn("%s: regulator is already enabled\n", name);
			goto exit;
		}

		ret = regulator_enable(regulator);
		if (ret) {
			err("%s : regulator_enable(%s) fail", __func__, name);
			goto exit;
		}
	} else {
		if (!regulator_is_enabled(regulator)) {
			pr_warn("%s: regulator is already disabled\n", name);
			goto exit;
		}

		ret = regulator_disable(regulator);
		if (ret) {
			err("%s : regulator_disable(%s) fail", __func__, name);
			goto exit;
		}
	}

exit:
	regulator_put(regulator);

	return ret;
}

int is_sec_rom_power_on(struct is_core *core, int position)
{
	int ret = 0;
	struct exynos_platform_is_module *module_pdata;
	struct is_module_enum *module = NULL;
	int i = 0;

	info("%s: Sensor position = %d.", __func__, position);

	for (i = 0; i < SENSOR_POSITION_MAX; i++) {
		is_search_sensor_module_with_position(&core->sensor[i],
							  position, &module);
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

	ret = module_pdata->gpio_cfg(module, SENSOR_SCENARIO_READ_ROM,
					 GPIO_SCENARIO_ON);
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

	for (i = 0; i < SENSOR_POSITION_MAX; i++) {
		is_search_sensor_module_with_position(&core->sensor[i],
							  position, &module);
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

	ret = module_pdata->gpio_cfg(module, SENSOR_SCENARIO_READ_ROM,
					 GPIO_SCENARIO_OFF);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int is_sec_check_is_sensor(struct is_core *core, int position, int nextSensorId,
			   bool *bVerified)
{
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
		is_search_sensor_module_with_position(&core->sensor[i],
							  position, &module);
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
		sensor_peri =
			(struct is_device_sensor_peri *)module->private_data;
		if (sensor_peri->subdev_cis) {
			i2c_channel = module_pdata->sensor_i2c_ch;
			if (i2c_channel < SENSOR_CONTROL_I2C_MAX) {
				sensor_peri->cis.i2c_lock =
					&core->i2c_lock[i2c_channel];
			} else {
				warn("%s: wrong cis i2c_channel(%d)", __func__,
					 i2c_channel);
				ret = -EINVAL;
				goto p_err;
			}
		} else {
			err("%s: subdev cis is NULL. dual_sensor check failed",
				__func__);
			ret = -EINVAL;
			goto p_err;
		}

		subdev_cis = sensor_peri->subdev_cis;
		ret = CALL_CISOPS(&sensor_peri->cis, cis_check_rev_on_init,
				  subdev_cis);
		if (ret < 0) {
			*bVerified = false;
			specific->sensor_id[position] = nextSensorId;
			warn("CIS test failed, check the nextSensor");
		} else {
			*bVerified = true;
			core->sensor[module->device].sensor_name =
				module->sensor_name;
			info("%s CIS test passed (sensorId[%d] = %d)", __func__,
				 module->device, specific->sensor_id[position]);
		}

		ret = module_pdata->gpio_cfg(module, scenario,
						 GPIO_SCENARIO_OFF);
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

	if (sensorid_2nd != SENSOR_NAME_NOTHING &&
		!is_check_dualized_sensor[position]) {
		int count_check = NUM_OF_DUALIZATION_CHECK;
		bool bVerified = false;

		while (count_check-- > 0) {
			ret = is_sec_check_is_sensor(core, position,
							 sensorid_2nd, &bVerified);
			if (ret) {
				err("%s: Failed to check corresponding sensor equipped",
					__func__);
				break;
			}

			if (bVerified) {
				is_check_dualized_sensor[position] = true;
				break;
			}

			sensorid_2nd = sensorid_1st;
			sensorid_1st = specific->sensor_id[position];

#if 0
			/* Update specific for dualized for OTPROM */
			if (specific->rom_data[position].rom_type == ROM_TYPE_OTPROM) {
				specific->rom_client[position] = specific->dualized_rom_client[position];
				specific->rom_cal_map_addr[position] = specific->dualized_rom_cal_map_addr[position];
			}
#endif

			if (count_check < NUM_OF_DUALIZATION_CHECK - 2) {
				err("Failed to find out both sensors on this device !!! (count : %d)",
					count_check);
				if (count_check <= 0)
					err("None of camera was equipped (sensorid : %d, %d)",
						sensorid_1st, sensorid_2nd);
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
			clear_bit(IS_ROM_STATE_LATEST_MODULE,
				  &finfo->rom_state);
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

#define ADDR_SIZE 2
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

		info("%s: i2c_master_send failed(%d), try %d, addr(%x)\n",
			 __func__, ret, retries, client->addr);
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

		info("%s: i2c_master_recv failed(%d), try %d\n", __func__, ret,
			 retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		err("%s: error %d, fail to read 0x%04X", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	return 0;
}

#define WRITE_BUF_SIZE 3
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

		pr_info("%s: i2c_master_send failed(%d), try %d\n", __func__,
			ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		pr_err("%s: error %d, fail to write 0x%04X\n", __func__, ret,
			   addr);
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
	snprintf(file_path, sizeof(file_path), "%sreload/r1e2l3o4a5d.key",
		 IS_FW_DUMP_PATH);
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
	snprintf(file_path, sizeof(file_path), "%si1s2p3s4r.key",
		 IS_FW_DUMP_PATH);
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

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
void is_sec_cal_dump(struct is_core *core)
{
#ifdef USE_KERNEL_VFS_READ_WRITE
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	char file_path[100];
	char *cal_buf;
	struct is_rom_info *finfo = NULL;
	struct is_vender_specific *specific = core->vender.private_data;
	int i;

	loff_t pos = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	memset(file_path, 0x00, sizeof(file_path));
	snprintf(file_path, sizeof(file_path), "%s1q2w3e4r.key",
		 IS_FW_DUMP_PATH);
	key_fp = filp_open(file_path, O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		memset(file_path, 0x00, sizeof(file_path));
		snprintf(file_path, sizeof(file_path), "%sdump",
			 IS_FW_DUMP_PATH);
		dump_fp = filp_open(file_path, O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			info("dump folder exist.\n");
			for (i = 0; i < ROM_ID_MAX; i++) {
				if (specific->rom_valid[i] == true) {
#ifdef CONFIG_SEC_CAL_ENABLE
					if (is_need_use_standard_cal(i) == true)
						is_sec_get_cal_buf_rom_data(
							&cal_buf, i);
					else
#endif
						is_sec_get_cal_buf(&cal_buf, i);
					is_sec_get_sysfs_finfo(&finfo, i);
					pos = 0;
					info("Dump ROM_ID(%d) cal data.\n", i);
					memset(file_path, 0x00,
						   sizeof(file_path));
					snprintf(file_path, sizeof(file_path),
						 "%sdump/rom_%d_cal.bin",
						 IS_FW_DUMP_PATH, i);
					if (write_data_to_file(file_path,
								   cal_buf,
								   finfo->rom_size,
								   &pos) < 0) {
						info("Failed to dump rom_id(%d) cal data.\n",
							 i);
						goto dump_err;
					}
				}
			}
		}
	}

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
key_err:
	if (key_fp)
		filp_close(key_fp, current->files);
	set_fs(old_fs);
#endif
}
#endif

int is_sec_parse_rom_info(struct is_rom_info *finfo, char *buf, int rom_id)
{
#if defined(CONFIG_CAMERA_USE_MCU) || defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
	struct is_ois_info *ois_pinfo = NULL;
#endif

	if (finfo->rom_header_version_start_addr != -1) {
		memcpy(finfo->header_ver,
			   &buf[finfo->rom_header_version_start_addr],
			   IS_HEADER_VER_SIZE);
		finfo->header_ver[IS_HEADER_VER_SIZE] = '\0';
	}

	if (finfo->rom_header_sensor2_version_start_addr != -1) {
		memcpy(finfo->header2_ver,
			   &buf[finfo->rom_header_sensor2_version_start_addr],
			   IS_HEADER_VER_SIZE);
		finfo->header2_ver[IS_HEADER_VER_SIZE] = '\0';
	}

	if (finfo->rom_header_project_name_start_addr != -1) {
		memcpy(finfo->project_name,
			   &buf[finfo->rom_header_project_name_start_addr],
			   IS_PROJECT_NAME_SIZE);
		finfo->project_name[IS_PROJECT_NAME_SIZE] = '\0';
	}

	if (finfo->rom_header_module_id_addr != -1) {
		memcpy(finfo->rom_module_id,
			   &buf[finfo->rom_header_module_id_addr],
			   IS_MODULE_ID_SIZE);
		finfo->rom_module_id[IS_MODULE_ID_SIZE] = '\0';
	}

	if (finfo->rom_header_sensor_id_addr != -1) {
		memcpy(finfo->rom_sensor_id,
			   &buf[finfo->rom_header_sensor_id_addr],
			   IS_SENSOR_ID_SIZE);
		finfo->rom_sensor_id[IS_SENSOR_ID_SIZE] = '\0';
	}

	if (finfo->rom_header_sensor2_id_addr != -1) {
		memcpy(finfo->rom_sensor2_id,
			   &buf[finfo->rom_header_sensor2_id_addr],
			   IS_SENSOR_ID_SIZE);
		finfo->rom_sensor2_id[IS_SENSOR_ID_SIZE] = '\0';
	}

	if (finfo->rom_header_cal_map_ver_start_addr != -1) {
		memcpy(finfo->cal_map_ver,
			   &buf[finfo->rom_header_cal_map_ver_start_addr],
			   IS_CAL_MAP_VER_SIZE);
		finfo->cal_map_ver[IS_CAL_MAP_VER_SIZE] = '\0';
	}

#if defined(CONFIG_CAMERA_USE_MCU) || defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
	is_sec_get_ois_pinfo(&ois_pinfo);
	if (finfo->rom_ois_list_len == IS_ROM_OIS_MAX_LIST) {
		if (rom_id == WIDE_OIS_ROM_ID) {
			memcpy(ois_pinfo->wide_romdata.xgg,
				   &buf[finfo->rom_ois_list[0]],
				   IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.ygg,
				   &buf[finfo->rom_ois_list[1]],
				   IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.xcoef,
				   &buf[finfo->rom_ois_list[2]],
				   IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.ycoef,
				   &buf[finfo->rom_ois_list[3]],
				   IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.supperssion_xratio,
				   &buf[finfo->rom_ois_list[4]],
				   IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.supperssion_yratio,
				   &buf[finfo->rom_ois_list[5]],
				   IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.cal_mark,
				   &buf[finfo->rom_ois_list[6]],
				   IS_OIS_CAL_MARK_DATA_SIZE);
		}
	} else if (finfo->rom_ois_list_len ==
		   IS_ROM_OIS_SINGLE_MODULE_MAX_LIST) {
		if (rom_id == WIDE_OIS_ROM_ID) {
			memcpy(ois_pinfo->wide_romdata.xgg,
				   &buf[finfo->rom_ois_list[0]],
				   IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.ygg,
				   &buf[finfo->rom_ois_list[1]],
				   IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.xcoef,
				   &buf[finfo->rom_ois_list[2]],
				   IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.ycoef,
				   &buf[finfo->rom_ois_list[3]],
				   IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.supperssion_xratio,
				   &buf[finfo->rom_ois_list[4]],
				   IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.supperssion_yratio,
				   &buf[finfo->rom_ois_list[5]],
				   IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.cal_mark,
				   &buf[finfo->rom_ois_list[6]],
				   IS_OIS_CAL_MARK_DATA_SIZE);
		}
	}
#endif

	/* debug info dump */
	info("++++ ROM data info - rom_id:%d\n", rom_id);
	info("1. Header info\n");
	info("Module info : %s\n", finfo->header_ver);
	info(" ID : %c\n", finfo->header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", finfo->header_ver[FW_PIXEL_SIZE],
		 finfo->header_ver[FW_PIXEL_SIZE + 1]);
	info(" ISP ID : %c\n", finfo->header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", finfo->header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", finfo->header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", finfo->header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", finfo->header_ver[FW_PUB_NUM],
		 finfo->header_ver[FW_PUB_NUM + 1]);
	info(" Manufacturer ID : %c\n", finfo->header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
	info(" Project name : %s\n", finfo->project_name);
	info(" Cal data map ver : %s\n", finfo->cal_map_ver);
	info(" MODULE ID : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
		 finfo->rom_module_id[0], finfo->rom_module_id[1],
		 finfo->rom_module_id[2], finfo->rom_module_id[3],
		 finfo->rom_module_id[4], finfo->rom_module_id[5],
		 finfo->rom_module_id[6], finfo->rom_module_id[7],
		 finfo->rom_module_id[8], finfo->rom_module_id[9]);
	info("---- ROM data info\n");

	return 0;
}

int is_sec_read_eeprom_header(int rom_id)
{
	int ret = 0;
	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific;
	u8 header_version[IS_HEADER_VER_SIZE + 1] = {
		0,
	};
	u8 header2_version[IS_HEADER_VER_SIZE + 1] = {
		0,
	};
	struct i2c_client *client;
	struct is_rom_info *finfo = NULL;
	struct is_device_eeprom *eeprom;

	specific = core->vender.private_data;
	client = specific->rom_client[rom_id];

	eeprom = i2c_get_clientdata(client);

	is_sec_get_sysfs_finfo(&finfo, rom_id);

	if (finfo->rom_header_version_start_addr != -1) {
		ret = is_i2c_read(client, header_version,
				  finfo->rom_header_version_start_addr,
				  IS_HEADER_VER_SIZE);

		if (unlikely(ret)) {
			err("failed to is_i2c_read for header version (%d)\n",
				ret);
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
			err("failed to is_i2c_read for header version (%d)\n",
				ret);
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
	u8 header_version[IS_HEADER_VER_SIZE + 1] = {
		0,
	};
	u8 header2_version[IS_HEADER_VER_SIZE + 1] = {
		0,
	};
	struct i2c_client *client;
	struct is_rom_info *finfo = NULL;
	struct is_device_otprom *otprom;

	specific = core->vender.private_data;
	client = specific->rom_client[rom_id];

	otprom = i2c_get_clientdata(client);

	is_sec_get_sysfs_finfo(&finfo, rom_id);

	if (finfo->rom_header_version_start_addr != -1) {
		ret = is_i2c_read(client, header_version,
				  finfo->rom_header_version_start_addr,
				  IS_HEADER_VER_SIZE);

		if (unlikely(ret)) {
			err("failed to is_i2c_read for header version (%d)\n",
				ret);
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
			err("failed to is_i2c_read for header version (%d)\n",
				ret);
			ret = -EINVAL;
		}

		memcpy(finfo->header2_ver, header2_version, IS_HEADER_VER_SIZE);
		finfo->header2_ver[IS_HEADER_VER_SIZE] = '\0';
	}

	return ret;
}

#if defined(CAMERA_UWIDE_DUALIZED)
u16 is_i2c_select_otp_bank_4ha(struct i2c_client *client)
{
	int ret = 0;
	u8 otp_bank = 0;
	u16 curr_page = 0;

	//The Bank details itself is present in bank-1 page-0
	is_sensor_write8(client, S5K4HA_OTP_PAGE_SELECT_ADDR,
			 S5K4HA_OTP_START_PAGE_BANK1);
	ret = is_sensor_read8(client, S5K4HA_OTP_BANK_SELECT, &otp_bank);

	if (unlikely(ret)) {
		err("failed to is_sensor_read8 (%d). OTP Bank selection failed\n",
			ret);
		goto exit;
	}
	info("%s otp_bank = %d\n", __func__, otp_bank);

	switch (otp_bank) {
	case 0x01:
		curr_page = S5K4HA_OTP_START_PAGE_BANK1;
		break;
	case 0x03:
		curr_page = S5K4HA_OTP_START_PAGE_BANK2;
		break;
	default:
		curr_page = S5K4HA_OTP_START_PAGE_BANK1;
		break;
	}
	is_sensor_write8(client, S5K4HA_OTP_PAGE_SELECT_ADDR, curr_page);
exit:
	return curr_page;
}

int is_sec_i2c_read_otp_4ha(struct i2c_client *client, char *buf,
				u16 start_addr, size_t size)
{
	int ret = 0;
	int index = 0;
	u16 curr_addr = start_addr;
	u16 curr_page;

	curr_page = is_i2c_select_otp_bank_4ha(client);
	for (index = 0; index < size; index++) {
		ret = is_sensor_read8(client, curr_addr,
					  &buf[index]); /* OTP read */
		if (unlikely(ret)) {
			err("failed to is_sensor_read8 (%d)\n", ret);
			goto exit;
		}
		curr_addr++;
		if (curr_addr > S5K4HA_OTP_PAGE_ADDR_H) {
			curr_addr = S5K4HA_OTP_PAGE_ADDR_L;
			curr_page++;
			is_sensor_write8(client, S5K4HA_OTP_PAGE_SELECT_ADDR,
					 curr_page);
		}
	}
exit:
	return ret;
}

int is_sec_readcal_otprom_4ha(int rom_id)
{
	int ret = 0;
	int retry = IS_CAL_RETRY_CNT;
	char *buf = NULL;
	u8 read_command_complete_check = 0;
	bool camera_running;

	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;
	struct i2c_client *client = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module = NULL;
	int position = is_vendor_get_position_from_rom_id(rom_id);

	is_vendor_get_module_from_position(position, &module);
	info("Camera: read cal data from OTPROM (rom_id:%d)\n", rom_id);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	subdev_cis = sensor_peri->subdev_cis;
	is_sec_get_sysfs_finfo(&finfo, rom_id);
	is_sec_get_cal_buf(&buf, rom_id);
	client = specific->rom_client[rom_id];

	camera_running =
		is_vendor_check_camera_running(finfo->rom_power_position);

	/* Sensor Initial Settings (Global) */
i2c_write_retry_global:
	if (camera_running == false) {
		ret = is_sec_set_registers(client, sensor_otp_4ha_global,
					   sensor_otp_4ha_global_size,
					   position);
		if (unlikely(ret)) {
			err("failed to apply otprom global settings (%d)\n",
				ret);
			if (retry >= 0) {
				retry--;
				msleep(50);
				goto i2c_write_retry_global;
			}
			ret = -EINVAL;
			goto exit;
		}
	}

	is_sensor_write8(client, S5K4HA_STANDBY_ADDR, 0x00); /* standby on */
	msleep(10); /* sleep 10msec */

	is_sensor_write8(client, S5K4HA_OTP_R_W_MODE_ADDR,
			 0x01); /* write "read" command */

	/* confirm write complete */
write_complete_retry:
	msleep(1);

	retry = IS_CAL_RETRY_CNT;
	is_sensor_read8(client, S5K4HA_OTP_CHECK_ADDR,
			&read_command_complete_check);
	if (read_command_complete_check != 1) {
		if (retry >= 0) {
			retry--;
			goto write_complete_retry;
		}
		goto exit;
	}

crc_retry:
	/* Read OTP Cal Data */
	info("I2C read cal data\n");
	ret = is_sec_i2c_read_otp_4ha(client, buf, S5K4HA_OTP_START_ADDR,
					  S5K4HA_OTP_USED_CAL_SIZE);
	if (unlikely(ret)) {
		err("failed to read otp 4ha (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	/* Parse Cal */
	retry = IS_CAL_RETRY_CNT;
	ret = is_sec_read_otprom_header(rom_id);
	if (unlikely(ret)) {
		if (retry >= 0) {
			retry--;
			goto crc_retry;
		}
		err("OTPROM CRC failed, retry %d", retry);
	}
	info("OTPROM CRC passed\n");
	is_sec_parse_rom_info(finfo, buf, rom_id);

exit:
	/* streaming mode change */
	is_sensor_write8(client, S5K4HA_OTP_R_W_MODE_ADDR,
			 0x04); /* clear error bit */
	msleep(1); /* sleep 1msec */
	is_sensor_write8(client, S5K4HA_OTP_R_W_MODE_ADDR,
			 0x00); /* initial command */
	info("%s X\n", __func__);
	return ret;
}
#endif /* CAMERA_UWIDE_DUALIZED */

int is_i2c_read_otp_hi556(int rom_id, struct i2c_client *client, char *buf,
			  u16 start_addr, size_t size)
{
	u16 curr_addr = start_addr;
	u8 start_addr_h = 0;
	u8 start_addr_l = 0;
	int ret = 0;
	int index;

	for (index = 0; index < size; index++) {
		start_addr_h = ((curr_addr >> 8) & 0xFF);
		start_addr_l = (curr_addr & 0xFF);
		is_sensor_write8(client, HI556_OTP_ACCESS_ADDR_HIGH,
				 start_addr_h);
		is_sensor_write8(client, HI556_OTP_ACCESS_ADDR_LOW,
				 start_addr_l);
		is_sensor_write8(client, HI556_OTP_MODE_ADDR, 0x01);

		ret = is_sensor_read8(client, HI556_OTP_READ_ADDR, &buf[index]);
		if (unlikely(ret)) {
			err("failed to is_sensor_read8 (%d)\n", ret);
			goto exit;
		}
		curr_addr++;
	}

exit:
	return ret;
}

int is_sec_readcal_otprom_hi556(int rom_id, struct is_rom_info *finfo,
				char *buf, int position)
{
	int ret = 0;
	int retry = IS_CAL_RETRY_CNT;
	u8 otp_bank = 0;
	u16 start_addr = 0;
	int cal_size = 0;
	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific = core->vender.private_data;
	struct i2c_client *client = NULL;
	bool camera_running;
	specific->rom_cal_map_addr[position];
	struct is_cis *cis = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module = NULL;
	u32 i2c_channel;

	is_vendor_get_module_from_position(position, &module);
	info("Camera: read cal data from OTPROM (rom_id:%d)\n", rom_id);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	subdev_cis = sensor_peri->subdev_cis;
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	if (cis == NULL) {
		err("cis is NULL");
		return -1;
	}

	client = specific->rom_client[position];

	cal_size = finfo->rom_size;
	info("%s: rom_id : %d, cal_size :%d\n", __func__, rom_id, cal_size);

	camera_running = is_vendor_check_camera_running(position);

	info("%s E\n", __func__);

	/* OTP read only setting */
	is_sec_set_registers(client, otp_read_initial_setting_hi556,
				 otp_read_initial_setting_hi556_size, position);

	/* OTP mode on */
	msleep(10);
	is_sensor_write8(client, 0x0A02, 0x01);
	is_sensor_write8(client, 0x0114, 0x00);
	msleep(10);
	is_sensor_write8(client, 0x0A00, 0x00);
	msleep(10);
	is_sec_set_registers(client, otp_mode_on_setting_hi556,
				 otp_mode_on_setting_hi556_size, position);
	msleep(10);
	is_sensor_write8(client, 0x0A00, 0x01);
	msleep(10);

	/* Select the bank */

	is_sensor_write8(client, HI556_OTP_ACCESS_ADDR_HIGH,
			 ((HI556_BANK_SELECT_ADDR >> 8) & 0xFF));
	is_sensor_write8(client, HI556_OTP_ACCESS_ADDR_LOW,
			 (HI556_BANK_SELECT_ADDR & 0xFF));
	is_sensor_write8(client, HI556_OTP_MODE_ADDR, 0x01);
	ret = is_sensor_read8(client, HI556_OTP_READ_ADDR, &otp_bank);
	if (unlikely(ret)) {
		err("failed to read otp_bank data from bank select address (%d)\n",
			ret);
		ret = -EINVAL;
	}

	info("%s: otp_bank = %d\n", __func__, otp_bank);

	/* select start address */
	switch (otp_bank) {
	case 0x01:
		start_addr = HI556_OTP_START_ADDR_BANK1;
		break;
	case 0x03:
		start_addr = HI556_OTP_START_ADDR_BANK2;
		break;
	case 0x07:
		start_addr = HI556_OTP_START_ADDR_BANK3;
		break;
	case 0x0F:
		start_addr = HI556_OTP_START_ADDR_BANK4;
		break;
	case 0x01F:
		start_addr = HI556_OTP_START_ADDR_BANK5;
		break;
	default:
		start_addr = HI556_OTP_START_ADDR_BANK1;
		break;
	}

	info("%s: otp_start_addr = %x\n", __func__, start_addr);

crc_retry:
	info("%s I2C read cal data", __func__);
	is_i2c_read_otp_hi556(rom_id, client, buf, start_addr, HI556_OTP_USED_CAL_SIZE);

	is_sec_parse_rom_info(finfo, buf, rom_id);

	/* CRC check */
	if (!is_sec_check_cal_crc32(buf, rom_id) && (retry > 0)) {
		err("OTP CRC failed (retry:%d)\n", retry);
		retry--;
		goto crc_retry;
	} else
		pr_info("OTP CRC passed for HI556 \n");

	/* OTP mode off*/
	is_sensor_write8(client, 0x0114, 0x00);
	msleep(10);
	is_sensor_write8(client, 0x0A00, 0x00);
	msleep(10);
	is_sec_set_registers(client, otp_mode_off_setting_hi556,
				 otp_mode_off_setting_hi556_size, position);
	msleep(10);
	is_sensor_write8(client, 0x0A00, 0x01);
	msleep(10);

exit:
	info("%s X\n", __func__);
	return ret;
}

int is_sec_readcal_otprom_hi1336(int rom_id, struct is_rom_info *finfo,
				 char *buf, int position)
{
	int ret = 0;
	int retry = IS_CAL_RETRY_CNT;
	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific = core->vender.private_data;
	int cal_size = 0;
	struct i2c_client *client = NULL;
	bool camera_running;
	u16 read_addr;
	struct is_cis *cis = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module = NULL;
	u32 i2c_channel;
	u8 bank;
	u16 start_addr = 0;
#ifdef CONFIG_SEC_CAL_ENABLE
	char *buf_rom_data = NULL;
#endif

	is_vendor_get_module_from_position(position, &module);
	info("Camera: read cal data from OTPROM (rom_id:%d)\n", rom_id);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	subdev_cis = sensor_peri->subdev_cis;
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	if (cis == NULL) {
		err("cis is NULL");
		return -1;
	}

	client = specific->rom_client[rom_id];

	cal_size = finfo->rom_size;
	info("%s: rom_id : %d, cal_size :%d\n", __func__, rom_id, cal_size);

	camera_running = is_vendor_check_camera_running(position);
	/* sensor initial settings */
i2c_write_retry_global:
	if (camera_running == false) {
		i2c_channel = module->pdata->sensor_i2c_ch;
		if (i2c_channel < SENSOR_CONTROL_I2C_MAX) {
			sensor_peri->cis.i2c_lock =
				&core->i2c_lock[i2c_channel];
		} else {
			warn("%s: wrong cis i2c_channel(%d)", __func__,
				 i2c_channel);
			ret = -EINVAL;
			goto exit;
		}
		ret = CALL_CISOPS(&sensor_peri->cis, cis_set_global_setting,
				  subdev_cis);

		if (unlikely(ret)) {
			err("failed to apply global settings (%d)\n", ret);
			if (retry >= 0) {
				retry--;
				msleep(50);
				goto i2c_write_retry_global;
			}
			ret = -EINVAL;
			goto exit;
		}
	}

	ret = CALL_CISOPS(&sensor_peri->cis, cis_mode_change, subdev_cis, 1);
	if (unlikely(ret)) {
		err("failed to apply cis_mode_change (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	read_addr = 0x0308;
crc_retry:
	ret = is_sensor_write8(client, 0x0808, 0x01); /* stream on */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0808, 0x0001);
		return ret;
	}
	ret = is_sensor_write8(client, 0x0B02, 0x01); /* fast standby on */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0B02, 0x01);
		return ret;
	}
	ret = is_sensor_write8(client, 0x0809, 0x00); /* stream off */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0809, 0x00);
		return ret;
	}
	ret = is_sensor_write8(client, 0x0B00, 0x00); /* stream off */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0B00, 0x00);
		return ret;
	}
	usleep_range(10000, 10000); /* sleep 10msec */
	ret = is_sensor_write8(client, 0x0260, 0x10); /* OTP test mode enable */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0260, 0x10);
		return ret;
	}
	ret = is_sensor_write8(client, 0x0809, 0x01); /* stream on */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0809, 0x01);
		return ret;
	}
	ret = is_sensor_write8(client, 0x0b00, 0x01); /* stream on */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0b00, 0x01);
		return ret;
	}

	usleep_range(1000, 1000); /* sleep 1msec */

	/* read otp bank */
	ret = is_sensor_write8(client, 0x030A,
				   ((0x400) >> 8) & 0xFF); /* upper 8bit */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x030A, ((0x400) >> 8) & 0xFF);
		return ret;
	}
	ret = is_sensor_write8(client, 0x030B, 0x400 & 0xFF); /* lower 8bit */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x030B, 0x400 & 0xFF);
		return ret;
	}
	ret = is_sensor_write8(client, 0x0302, 0x01); /* read mode */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0302, 0x01);
		return ret;
	}

	ret = is_sensor_read8(client, read_addr, &bank);
	if (unlikely(ret)) {
		err("failed to read OTP bank address (%d)\n", ret);
		goto exit;
	}

	/* select start address */
	switch (bank) {
	case 0x01:
		start_addr = HI1336_OTP_START_ADDR_BANK1;
		break;
	case 0x03:
		start_addr = HI1336_OTP_START_ADDR_BANK2;
		break;
	case 0x07:
		start_addr = HI1336_OTP_START_ADDR_BANK3;
		break;
	case 0x0F:
		start_addr = HI1336_OTP_START_ADDR_BANK4;
		break;
	case 0x1F:
		start_addr = HI1336_OTP_START_ADDR_BANK5;
		break;
	default:
		start_addr = HI1336_OTP_START_ADDR_BANK1;
		break;
	}
	info("%s: otp_bank = %d start_addr = %x\n", __func__, bank, start_addr);

	/* OTP burst read */
	ret = is_sensor_write8(client, 0x030A,
				   ((start_addr) >> 8) & 0xFF); /* upper 8bit */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x030A, ((start_addr) >> 8) & 0xFF);
		return ret;
	}
	ret = is_sensor_write8(client, 0x030B,
				   start_addr & 0xFF); /* lower 8bit */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x030B, start_addr & 0xFF);
		return ret;
	}
	ret = is_sensor_write8(client, 0x0302, 0x01); /* read mode */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0302, 0x01);
		return ret;
	}
	ret = is_sensor_write8(client, 0x0712,
				   0x01); /* burst read register on */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0712, 0x01);
		return ret;
	}

	info("Camera: I2C read cal data for rom_id:%d\n", rom_id);
	ret = is_sensor_read8_size(client, &buf[0], read_addr,
				   IS_READ_MAX_HI1336_OTP_CAL_SIZE);
	if (ret != IS_READ_MAX_HI1336_OTP_CAL_SIZE) {
		err("failed to is_i2c_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = is_sensor_write8(client, 0x0712,
				   0x00); /* burst read register off */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0712, 0x00);
		return ret;
	}

	if (finfo->rom_header_cal_map_ver_start_addr != -1)
		memcpy(finfo->cal_map_ver,
			   &buf[finfo->rom_header_cal_map_ver_start_addr],
			   IS_CAL_MAP_VER_SIZE);

	if (finfo->rom_header_version_start_addr != -1)
		memcpy(finfo->header_ver,
			   &buf[finfo->rom_header_version_start_addr],
			   IS_HEADER_VER_SIZE);

	info("Camera : OTPROM Cal map_version = %s(%x%x%x%x)\n",
		 finfo->cal_map_ver, finfo->cal_map_ver[0], finfo->cal_map_ver[1],
		 finfo->cal_map_ver[2], finfo->cal_map_ver[3]);
	info("OTPROM header version = %s(%x%x%x%x)\n", finfo->header_ver,
		 finfo->header_ver[0], finfo->header_ver[1], finfo->header_ver[2],
		 finfo->header_ver[3]);

	if (!is_sec_check_rom_ver(core, rom_id)) {
		info("Camera: Do not read eeprom cal data. EEPROM version is low.\n");
		return -EINVAL;
	}

	is_sec_parse_rom_info(finfo, buf, rom_id);

#ifdef CAMERA_REAR_TOF
	if (rom_id == REAR_TOF_ROM_ID) {
#ifdef REAR_TOF_CHECK_SENSOR_ID
		is_sec_sensorid_find_rear_tof(core);
		if (specific->rear_tof_sensor_id == SENSOR_NAME_IMX316) {
			finfo->rom_tof_cal_size_addr_len = 1;
			if (finfo->cal_map_ver[3] == '1') {
				finfo->crc_check_list[1] =
					REAR_TOF_IMX316_CRC_ADDR1_MAP001;
			} else {
				finfo->crc_check_list[1] =
					REAR_TOF_IMX316_CRC_ADDR1_MAP002;
			}
		}
#endif
		is_sec_sensor_find_rear_tof_mode_id(core, buf);
	}
#endif

#ifdef CAMERA_FRONT_TOF
	if (rom_id == FRONT_TOF_ROM_ID) {
		is_sec_sensor_find_front_tof_mode_id(core, buf);
	}
#endif

#ifdef DEBUG_FORCE_DUMP_ENABLE
	{
		char file_path[100];

		loff_t pos = 0;

		memset(file_path, 0x00, sizeof(file_path));
		snprintf(file_path, sizeof(file_path), "%srom%d_dump.bin",
			 IS_FW_DUMP_PATH, rom_id);

		if (write_data_to_file(file_path, buf, cal_size, &pos) < 0) {
			info("Failed to dump cal data. rom_id:%d\n", rom_id);
		}
	}
#endif
	/* CRC check */
	retry = IS_CAL_RETRY_CNT;
	if (!is_sec_check_cal_crc32(buf, rom_id) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

	is_sec_check_module_state(finfo);

#ifdef USE_CAMERA_NOTIFY_WACOM
	if (!test_bit(IS_CRC_ERROR_HEADER, &finfo->crc_error))
		is_eeprom_info_update(rom_id, finfo->header_ver);
#endif

#ifdef CONFIG_SEC_CAL_ENABLE
	/* Store original rom data before conversion for intrinsic cal */
	if (is_sec_check_cal_crc32(buf, rom_id) == true &&
		is_need_use_standard_cal(rom_id)) {
		is_sec_get_cal_buf_rom_data(&buf_rom_data, rom_id);
		if (buf != NULL && buf_rom_data != NULL)
			memcpy(buf_rom_data, buf,
				   is_sec_get_max_cal_size(core, rom_id));
	}
#endif
exit:
	/* streaming mode change */
	ret = is_sensor_write8(client, 0x0809, 0x00); /* stream off */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0809, 0x00);
		return ret;
	}
	ret = is_sensor_write8(client, 0x0b00, 0x00); /* stream off */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0b00, 0x00);
		return ret;
	}
	usleep_range(10000, 10000); /* sleep 10msec */
	ret = is_sensor_write8(client, 0x0260, 0x00); /* OTP mode display */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0260, 0x00);
		return ret;
	}
	ret = is_sensor_write8(client, 0x0809, 0x01); /* stream on */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0809, 0x01);
		return ret;
	}
	ret = is_sensor_write8(client, 0x0b00, 0x01); /* stream on */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0b00, 0x01);
		return ret;
	}
	usleep_range(1000, 1000); /* sleep 1msec */
	return ret;
}

int is_sec_readcal_otprom_3l6(int rom_id, struct is_rom_info *finfo, char *buf,
				  int position)
{
	int ret = 0;
	int page_byte_index = 0;
	int num_otp_byte_read = 0;
	int page_number = 0;
	int retry = IS_CAL_RETRY_CNT;
	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific = core->vender.private_data;
	int cal_size = 0;
	struct i2c_client *client = NULL;
	bool camera_running;
	u16 read_addr;
	struct is_cis *cis = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module = NULL;
	u32 i2c_channel;
	u8 bank;
	u16 start_addr = 0;
#ifdef CONFIG_SEC_CAL_ENABLE
	char *buf_rom_data = NULL;
#endif

	is_vendor_get_module_from_position(position, &module);
	info("Camera: read cal data from OTPROM (rom_id:%d)\n", rom_id);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	subdev_cis = sensor_peri->subdev_cis;
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	if (cis == NULL) {
		err("cis is NULL");
		return -1;
	}

	client = specific->rom_client[rom_id];

	cal_size = finfo->rom_size;
	info("%s: rom_id : %d, cal_size :%d\n", __func__, rom_id, cal_size);

	camera_running = is_vendor_check_camera_running(position);
	/* sensor initial settings */
i2c_write_retry_global:
	if (camera_running == false) {
		i2c_channel = module->pdata->sensor_i2c_ch;
		if (i2c_channel < SENSOR_CONTROL_I2C_MAX) {
			sensor_peri->cis.i2c_lock =
				&core->i2c_lock[i2c_channel];
		} else {
			warn("%s: wrong cis i2c_channel(%d)", __func__,
				 i2c_channel);
			ret = -EINVAL;
			goto exit;
		}
		ret = CALL_CISOPS(&sensor_peri->cis, cis_set_global_setting,
				  subdev_cis);

		if (unlikely(ret)) {
			err("failed to apply global settings (%d)\n", ret);
			if (retry >= 0) {
				retry--;
				msleep(50);
				goto i2c_write_retry_global;
			}
			ret = -EINVAL;
			goto exit;
		}
	}

	ret = CALL_CISOPS(&sensor_peri->cis, cis_mode_change, subdev_cis, 1);
	if (unlikely(ret)) {
		err("failed to apply cis_mode_change (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

crc_retry:
	ret = is_sensor_write8(client, 0x0100, 0x00); /* stream on */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0100, 0x00);
		return ret;
	}

	ret = is_sensor_write8(client, 0x0101, 0x01); /* stream on */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0101, 0x01);
		return ret;
	}

	/* set page for otp bank */
	ret = is_sensor_write8(client, 0x0A00, 0x04); /* make initial state */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0A00, 0x04);
		goto exit;
	}
	ret = is_sensor_write8(client, 0x0A02,
				   S5K3L6_OTP_BANK_READ_PAGE); /* select page */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0A02, S5K3L6_OTP_BANK_READ_PAGE);
		goto exit;
	}

	ret = is_sensor_write8(client, 0x0A00, 0x01); /* read mode */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0A00, 0x01);
		goto exit;
	}

	usleep_range(47, 47); /* sleep 47usec */

	ret = is_sensor_read8(client, S5K3L6_OTP_PAGE_START_ADDR, &bank);
	if (unlikely(ret)) {
		err("failed to read OTP bank address(%d) ret(%d)\n",
			S5K3L6_OTP_PAGE_START_ADDR, ret);
		goto exit;
	}

	start_addr = S5K3L6_OTP_READ_START_ADDR;
	/* select start address */
	switch (bank) {
	case 0x01:
		page_number = S5K3L6_OTP_START_PAGE_BANK1;
		break;
	case 0x03:
		page_number = S5K3L6_OTP_START_PAGE_BANK2;
		break;
	default:
		page_number = S5K3L6_OTP_START_PAGE_BANK1;
		break;
	}
	info("%s: otp_bank = %d start_addr = %x\n", __func__, bank, start_addr);

	/* Disable read mode */

	ret = is_sensor_write8(client, 0x0A00, 0x04); /* make initial state */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0A00, 0x04);
		goto exit;
	}
	ret = is_sensor_write8(client, 0x0A00,
				   0x00); /* disable NVM controller */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0A00, 0x00);
		goto exit;
	}

	/* OTP cal read */
	num_otp_byte_read = 0;
	page_byte_index = 5;
	read_addr = start_addr; /* bank read address */

	while (num_otp_byte_read < IS_READ_MAX_S5K3L6_OTP_CAL_SIZE) {
		/* set page for cal read */
		ret = is_sensor_write8(client, 0x0A00,
					   0x04); /* make initial state */
		if (ret < 0) {
			err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
				ret, 0x0A00, 0x04);
			goto exit;
		}
		ret = is_sensor_write8(client, 0x0A02,
					   page_number); /* select page */
		if (ret < 0) {
			err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
				ret, 0x0A02, page_number);
			goto exit;
		}

		ret = is_sensor_write8(client, 0x0A00, 0x01); /* read mode */
		if (ret < 0) {
			err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
				ret, 0x0A00, 0x01);
			goto exit;
		}

		usleep_range(47, 47); /* sleep 47usec */

		while (page_byte_index <= S5K3L6_OTP_PAGE_SIZE) {
			ret = is_sensor_read8(client, read_addr,
						  &buf[num_otp_byte_read]);
			if (unlikely(ret)) {
				err("failed to read OTP bank address(%d) ret(%d)\n",
					read_addr, ret);
				goto exit;
			}
			read_addr++;
			page_byte_index++;
			num_otp_byte_read++;
			if (num_otp_byte_read ==
				IS_READ_MAX_S5K3L6_OTP_CAL_SIZE)
				break;
		}
		page_number++;
		read_addr = S5K3L6_OTP_PAGE_START_ADDR;
		page_byte_index = 1;

		/* Disable read mode */
		ret = is_sensor_write8(client, 0x0A00,
					   0x04); /* make initial state */
		if (ret < 0) {
			err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
				ret, 0x0A00, 0x04);
			goto exit;
		}
		ret = is_sensor_write8(client, 0x0A00,
					   0x00); /* disable NVM controller */
		if (ret < 0) {
			err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
				ret, 0x0A00, 0x00);
			goto exit;
		}
	}

	if (finfo->rom_header_cal_map_ver_start_addr != -1)
		memcpy(finfo->cal_map_ver,
			   &buf[finfo->rom_header_cal_map_ver_start_addr],
			   IS_CAL_MAP_VER_SIZE);

	if (finfo->rom_header_version_start_addr != -1)
		memcpy(finfo->header_ver,
			   &buf[finfo->rom_header_version_start_addr],
			   IS_HEADER_VER_SIZE);

	info("Camera : OTPROM Cal map_version = %s(%x%x%x%x)\n",
		 finfo->cal_map_ver, finfo->cal_map_ver[0], finfo->cal_map_ver[1],
		 finfo->cal_map_ver[2], finfo->cal_map_ver[3]);
	info("OTPROM header version = %s(%x%x%x%x)\n", finfo->header_ver,
		 finfo->header_ver[0], finfo->header_ver[1], finfo->header_ver[2],
		 finfo->header_ver[3]);

	if (!is_sec_check_rom_ver(core, rom_id)) {
		info("Camera: Do not read eeprom cal data. OTPROM version is low.\n");
		ret = -EINVAL;
		goto exit;
	}

	is_sec_parse_rom_info(finfo, buf, rom_id);

#ifdef DEBUG_FORCE_DUMP_ENABLE
	{
		char file_path[100];

		loff_t pos = 0;

		memset(file_path, 0x00, sizeof(file_path));
		snprintf(file_path, sizeof(file_path), "%srom%d_dump.bin",
			 IS_FW_DUMP_PATH, rom_id);

		if (write_data_to_file(file_path, buf, cal_size, &pos) < 0) {
			info("Failed to dump cal data. rom_id:%d\n", rom_id);
		}
	}
#endif
	/* CRC check */
	retry = IS_CAL_RETRY_CNT;
	if (!is_sec_check_cal_crc32(buf, rom_id) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

	is_sec_check_module_state(finfo);

#ifdef CONFIG_SEC_CAL_ENABLE
	/* Store original rom data before conversion for intrinsic cal */
	if (is_sec_check_cal_crc32(buf, rom_id) == true &&
		is_need_use_standard_cal(rom_id)) {
		is_sec_get_cal_buf_rom_data(&buf_rom_data, rom_id);
		if (buf != NULL && buf_rom_data != NULL)
			memcpy(buf_rom_data, buf,
				   is_sec_get_max_cal_size(core, rom_id));
	}
#endif
	ret = is_sensor_write16(client, 0x0100, 0x0000); /* stream off */
	if (ret < 0) {
		err("is_sensor_write16 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0100, 0x0000);
		return ret;
	}

	usleep_range(1000, 1000); /* sleep 1msec */
	return ret;
exit:
	/* Disable read mode */
	ret = is_sensor_write8(client, 0x0A00, 0x04); /* make initial state */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0A00, 0x04);
	}
	ret = is_sensor_write8(client, 0x0A00,
				   0x00); /* disable NVM controller */
	if (ret < 0) {
		err("is_sensor_write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0A00, 0x00);
	}
	ret = is_sensor_write16(client, 0x0100, 0x0000); /* stream off */
	if (ret < 0) {
		err("is_sensor_write16 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0100, 0x0000);
		return ret;
	}
	usleep_range(1000, 1000); /* sleep 1msec */
	return ret;
}

int is_sec_i2c_read_otp_gc5035(struct i2c_client *client, char *buf,
				   u16 start_addr, size_t size)
{
	return 0;
	/*	u16 curr_addr = start_addr;
	u8 start_addr_h = 0;
	u8 start_addr_l = 0;
	u8 busy_flag = 0;
	int retry = 8;
	int ret = 0;
	int index;

	for (index = 0; index < size; index++)
	{
		start_addr_h = ((curr_addr>>8) & 0xFF);
		start_addr_l = (curr_addr & 0xFF);
		ret = is_sensor_addr8_write8(client, GC5035_OTP_PAGE_ADDR, GC5035_OTP_PAGE);
		ret |= is_sensor_addr8_write8(client, GC5035_OTP_ACCESS_ADDR_HIGH, start_addr_h);
		ret |= is_sensor_addr8_write8(client, GC5035_OTP_ACCESS_ADDR_LOW, start_addr_l);
		ret |= is_sensor_addr8_write8(client, GC5035_OTP_MODE_ADDR, 0x20);
		if (unlikely(ret))
		{
			err("failed to is_sensor_addr8_write8 (%d)\n", ret);
			goto exit;
		}

		ret = is_sensor_addr8_read8(client, GC5035_OTP_BUSY_ADDR, &busy_flag);
		if (unlikely(ret))
		{
			err("failed to is_sensor_addr8_read8 (%d)\n", ret);
			goto exit;
		}

		while ((busy_flag & 0x2) > 0 && retry > 0) {
			ret = is_sensor_addr8_read8(client, GC5035_OTP_BUSY_ADDR, &busy_flag);
			if (unlikely(ret))
			{
				err("failed to is_sensor_addr8_read8 (%d)\n", ret);
				goto exit;
			}
			retry--;
			msleep(1);
		}

		if ((busy_flag & 0x1))
		{
			err("Sensor OTP_check_flag failed\n");
			goto exit;
		}

		ret = is_sensor_addr8_read8(client, GC5035_OTP_READ_ADDR, &buf[index]);
		if (unlikely(ret))
		{
			err("failed to is_sensor_addr8_read8 (%d)\n", ret);
			goto exit;
		}
		curr_addr += 8;
	}

exit:
	return ret;
*/
}

int is_sec_readcal_otprom_gc5035(int rom_id)
{
	return 0;
#if 0
	int ret = 0;
	int retry = IS_CAL_RETRY_CNT;
	char *buf = NULL;
	u8 otp_bank = 0;
	u16 start_addr = 0;
	u8 busy_flag = 0;
	bool camera_running;
	u16 bank1_addr = 0;
	u16 bank2_addr = 0;
	u16 cal_size = 0;

	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;
	struct i2c_client *client = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module = NULL;
	u32 i2c_channel;
	int position = is_vendor_get_position_from_rom_id(rom_id);

	if(rom_id == 2) {
		bank1_addr = GC5035_BOKEH_OTP_START_ADDR_BANK1;
		bank2_addr = GC5035_BOKEH_OTP_START_ADDR_BANK2;
		cal_size = GC5035_BOKEH_OTP_USED_CAL_SIZE;
	}
	else if(rom_id == 4) {
		bank1_addr = GC5035_UW_OTP_START_ADDR_BANK1;
		bank2_addr = GC5035_UW_OTP_START_ADDR_BANK2;
		cal_size = GC5035_UW_OTP_USED_CAL_SIZE;
	}
	else if(rom_id == 6) {
		bank1_addr = GC5035_MACRO_OTP_START_ADDR_BANK1;
		bank2_addr = GC5035_MACRO_OTP_START_ADDR_BANK2;
		cal_size = GC5035_MACRO_OTP_USED_CAL_SIZE;
	}

	is_vendor_get_module_from_position(position,&module);
	info("Camera: read cal data from OTPROM (rom_id:%d)\n", rom_id);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	subdev_cis = sensor_peri->subdev_cis;
	is_sec_get_sysfs_finfo(&finfo, rom_id);
	is_sec_get_cal_buf(&buf, rom_id);
	client = specific->rom_client[rom_id];

	camera_running = is_vendor_check_camera_running(finfo->rom_power_position);
i2c_write_retry_global:
	if (camera_running == false) {
		i2c_channel = module->pdata->sensor_i2c_ch;
		if (i2c_channel < SENSOR_CONTROL_I2C_MAX) {
			sensor_peri->cis.i2c_lock = &core->i2c_lock[i2c_channel];
		} else {
			warn("%s: wrong cis i2c_channel(%d)", __func__, i2c_channel);
			ret = -EINVAL;
			goto exit;
		}
		ret = is_sec_set_registers(client, sensor_gc5035_setfile_otp_global, ARRAY_SIZE(sensor_gc5035_setfile_otp_global), position);
		if (unlikely(ret)) {
			err("failed to apply otprom global settings (%d)\n", ret);
			if (retry >= 0) {
				retry--;
				msleep(50);
				goto i2c_write_retry_global;
			}
			ret = -EINVAL;
			goto exit;
		}
	}

	ret = is_sec_set_registers(client, sensor_gc5035_setfile_otp_init, ARRAY_SIZE(sensor_gc5035_setfile_otp_init), position);
	if (unlikely(ret)) {
		err("failed to apply otprom init setting (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	/* Read OTP page */
	ret = is_sensor_addr8_write8(client, GC5035_OTP_PAGE_ADDR, GC5035_OTP_PAGE);
	ret |= is_sensor_addr8_write8(client, GC5035_OTP_ACCESS_ADDR_HIGH, (GC5035_BANK_SELECT_ADDR >> 8) & 0x1F);
	ret |= is_sensor_addr8_write8(client, GC5035_OTP_ACCESS_ADDR_LOW, GC5035_BANK_SELECT_ADDR & 0xFF);
	ret |= is_sensor_addr8_write8(client, GC5035_OTP_MODE_ADDR, 0x20);
	if (unlikely(ret))
	{
		err("failed to is_sensor_addr8_write8 (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	ret = is_sensor_addr8_read8(client, GC5035_OTP_BUSY_ADDR, &busy_flag);
	if (unlikely(ret))
	{
		err("failed to is_sensor_addr8_read8 (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	retry = IS_CAL_RETRY_CNT;

	while ((busy_flag & 0x2) > 0 && retry > 0) {
		ret = is_sensor_addr8_read8(client, GC5035_OTP_BUSY_ADDR, &busy_flag);
		if (unlikely(ret))
		{
			err("failed to is_sensor_addr8_read8 (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}
		retry--;
		msleep(1);
	}

	if ((busy_flag & 0x1))
	{
		err("Sensor OTP_check_flag failed\n");
		goto exit;
	}

	is_sensor_addr8_read8(client, GC5035_OTP_READ_ADDR, &otp_bank);
	if (unlikely(ret))
	{
		err("failed to is_sensor_addr8_read8 (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	/* select start address */
	switch(otp_bank) {
	case 0x01 :
		start_addr = bank1_addr;
		break;
	case 0x03 :
		start_addr = bank2_addr;
		break;
	default :
		start_addr = bank1_addr;
		break;
	}

	info("%s: otp_bank = %d otp_start_addr = %x\n", __func__, otp_bank, start_addr);

crc_retry:
	/* read cal data */
	info("I2C read cal data\n");
	ret = is_sec_i2c_read_otp_gc5035(client, buf, start_addr, cal_size);
	if (unlikely(ret)) {
		err("failed to read otp gc5035 (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = is_sec_read_otprom_header(rom_id);
	if (unlikely(ret)) {
		if (retry >= 0) {
			retry--;
			goto crc_retry;
		}
		err("OTPROM CRC failed, retry %d", retry);
	}
	info("OTPROM CRC passed\n");

#ifdef DEBUG_FORCE_DUMP_ENABLE
	{
		char file_path[100];

		loff_t pos = 0;

		memset(file_path, 0x00, sizeof(file_path));
		snprintf(file_path, sizeof(file_path), "%srom%d_dump.bin", FIMC_IS_FW_DUMP_PATH, rom_id);

		if (write_data_to_file(file_path, buf, finfo->rom_size, &pos) < 0) {
			info("Failed to dump cal data. rom_id:%d\n", rom_id);
		}
	}
#endif
	is_sec_parse_rom_info(finfo, buf, rom_id);

	/* CRC check */
	if (!is_sec_check_cal_crc32(buf, rom_id) && (retry > 0)) {
		err("OTP CRC failed (retry:%d)\n", retry);
		retry--;
		goto crc_retry;
	}

exit:
	return ret;
#endif
}

int is_sec_i2c_read_otp_gc02m1(struct i2c_client *client, char *buf,
				   u16 start_addr, size_t size)
{
	return 0;
	/*	u16 curr_addr = start_addr;
	int ret = 0;
	int index;

	for(index = 0; index < size; index++) {
		ret = is_sensor_addr8_write8(client, GC02M1_OTP_PAGE_ADDR, GC02M1_OTP_PAGE);
		ret |= is_sensor_addr8_write8(client, GC02M1_OTP_ACCESS_ADDR, curr_addr);
		ret |= is_sensor_addr8_write8(client, GC02M1_OTP_MODE_ADDR, 0x34); // Read Pulse
		if (unlikely(ret))
		{
			err("failed to is_sensor_addr8_write8 (%d)\n", ret);
			goto exit;
		}

		ret = is_sensor_addr8_read8(client, GC02M1_OTP_READ_ADDR, &buf[index]);
		if (unlikely(ret)) {
			err("failed to is_sensor_addr8_read8 (%d)\n", ret);
			goto exit;
		}
		curr_addr += 8;
	}
exit:
	return ret;
*/
}

int is_sec_readcal_otprom_gc02m1(int rom_id)
{
	int ret = 0;
	int retry = IS_CAL_RETRY_CNT;
	char *buf = NULL;
	bool camera_running;

	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;
	struct i2c_client *client = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module = NULL;
	u32 i2c_channel;
	int position = is_vendor_get_position_from_rom_id(rom_id);

	is_vendor_get_module_from_position(position, &module);
	info("Camera: read cal data from OTPROM (rom_id:%d)\n", rom_id);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	subdev_cis = sensor_peri->subdev_cis;
	is_sec_get_sysfs_finfo(&finfo, rom_id);
	is_sec_get_cal_buf(&buf, rom_id);
	client = specific->rom_client[rom_id];

	camera_running =
		is_vendor_check_camera_running(finfo->rom_power_position);
i2c_write_retry_global:
	if (camera_running == false) {
		i2c_channel = module->pdata->sensor_i2c_ch;
		if (i2c_channel < SENSOR_CONTROL_I2C_MAX) {
			sensor_peri->cis.i2c_lock =
				&core->i2c_lock[i2c_channel];
		} else {
			warn("%s: wrong cis i2c_channel(%d)", __func__,
				 i2c_channel);
			ret = -EINVAL;
			goto exit;
		}
		ret = is_sec_set_registers(
			client, sensor_gc02m1_setfile_otp_global,
			ARRAY_SIZE(sensor_gc02m1_setfile_otp_global), position);
		if (unlikely(ret)) {
			err("failed to apply otprom global settings (%d)\n",
				ret);
			if (retry >= 0) {
				retry--;
				msleep(50);
				goto i2c_write_retry_global;
			}
			ret = -EINVAL;
			goto exit;
		}
	}

	ret = is_sec_set_registers(client, sensor_gc02m1_setfile_otp_mode,
				   ARRAY_SIZE(sensor_gc02m1_setfile_otp_mode),
				   position);
	if (unlikely(ret)) {
		err("failed to apply otprom mode (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = is_sec_set_registers(client, sensor_gc02m1_setfile_otp_init,
				   ARRAY_SIZE(sensor_gc02m1_setfile_otp_init),
				   position);
	if (unlikely(ret)) {
		err("failed to apply otprom init setting (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

crc_retry:
	/* read cal data */
	info("I2C read cal data\n");
	ret = is_sec_i2c_read_otp_gc02m1(client, buf, GC02M1_OTP_START_ADDR,
					 GC02M1_OTP_USED_CAL_SIZE);
	if (unlikely(ret)) {
		err("failed to read otp gc02m1 (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	retry = IS_CAL_RETRY_CNT;

	ret = is_sec_read_otprom_header(rom_id);
	if (unlikely(ret)) {
		if (retry >= 0) {
			retry--;
			goto crc_retry;
		}
		err("OTPROM CRC failed, retry %d", retry);
	}
	info("OTPROM CRC passed\n");

#ifdef DEBUG_FORCE_DUMP_ENABLE
	{
		char file_path[100];

		loff_t pos = 0;

		memset(file_path, 0x00, sizeof(file_path));
		snprintf(file_path, sizeof(file_path), "%srom%d_dump.bin",
			 FIMC_IS_FW_DUMP_PATH, rom_id);

		if (write_data_to_file(file_path, buf, finfo->rom_size, &pos) <
			0) {
			info("Failed to dump cal data. rom_id:%d\n", rom_id);
		}
	}
#endif

	is_sec_parse_rom_info(finfo, buf, rom_id);

	/* CRC check */
	if (!is_sec_check_cal_crc32(buf, rom_id) && (retry > 0)) {
		err("OTP CRC failed (retry:%d)\n", retry);
		retry--;
		goto crc_retry;
	} else
		pr_info("OTP CRC passed for GC02M1 \n");

exit:
	info("%s X\n", __func__);
	return ret;
}

int is_sec_readcal_otprom(int rom_id)
{
	int ret = 0;
	char *buf = NULL;
	struct is_rom_info *finfo = NULL;
	int position = is_vendor_get_position_from_rom_id(rom_id);
	int sensor_id = is_vendor_get_sensor_id_from_position(position);

	is_sec_get_sysfs_finfo(&finfo, rom_id);
	is_sec_get_cal_buf(&buf, rom_id);

	switch (sensor_id) {
#if 0
		case SENSOR_NAME_GC5035:
			ret = is_sec_readcal_otprom_gc5035(rom_id);
			break;
		case SENSOR_NAME_GC02M1:
			ret = is_sec_readcal_otprom_gc02m1(rom_id);
			break;
#endif
	case SENSOR_NAME_HI1336:
		ret = is_sec_readcal_otprom_hi1336(rom_id, finfo, buf,
						   position);
		break;
	case SENSOR_NAME_HI556:
		ret = is_sec_readcal_otprom_hi556(rom_id, finfo, buf, position);
		break;
	case SENSOR_NAME_S5K3L6:
		ret = is_sec_readcal_otprom_3l6(rom_id, finfo, buf, position);
		break;
#if 0
		case SENSOR_NAME_S5K4HA:
			ret = is_sec_readcal_otprom_4ha(rom_id);
			break;
#endif
	default:
		err("[%s] No supported sensor_id:%d for rom_id:%d", __func__,
			sensor_id, rom_id);
		ret = -EINVAL;
		break;
	}
	return ret;
}

#if defined(CONFIG_CAMERA_EEPROM_DUALIZED)
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

	if (finfo->is_read_dualized_values) {
		pr_info("Dualized values already read\n");
		goto EXIT;
	}

	for (i = 0; i < IS_ROM_CRC_MAX_LIST; i++) {
		finfo->header_crc_check_list[i] =
			finfo->header_dualized_crc_check_list[i];
		finfo->crc_check_list[i] = finfo->crc_dualized_check_list[i];
		finfo->dual_crc_check_list[i] =
			finfo->dual_crc_dualized_check_list[i];
	}
	finfo->header_crc_check_list_len =
		finfo->header_dualized_crc_check_list_len;
	finfo->crc_check_list_len = finfo->crc_dualized_check_list_len;
	finfo->dual_crc_check_list_len =
		finfo->dual_crc_dualized_check_list_len;

	for (i = 0; i < IS_ROM_DUAL_TILT_MAX_LIST; i++) {
		finfo->rom_dualcal_slave0_tilt_list[i] =
			finfo->rom_dualized_dualcal_slave0_tilt_list[i];
		finfo->rom_dualcal_slave1_tilt_list[i] =
			finfo->rom_dualized_dualcal_slave1_tilt_list[i];
		finfo->rom_dualcal_slave2_tilt_list[i] =
			finfo->rom_dualized_dualcal_slave2_tilt_list[i];
		finfo->rom_dualcal_slave3_tilt_list[i] =
			finfo->rom_dualized_dualcal_slave3_tilt_list[i];
	}
	finfo->rom_dualcal_slave0_tilt_list_len =
		finfo->rom_dualized_dualcal_slave0_tilt_list_len;
	finfo->rom_dualcal_slave1_tilt_list_len =
		finfo->rom_dualized_dualcal_slave1_tilt_list_len;
	finfo->rom_dualcal_slave2_tilt_list_len =
		finfo->rom_dualized_dualcal_slave2_tilt_list_len;
	finfo->rom_dualcal_slave3_tilt_list_len =
		finfo->rom_dualized_dualcal_slave3_tilt_list_len;

	for (i = 0; i < IS_ROM_OIS_MAX_LIST; i++)
		finfo->rom_ois_list[i] = finfo->rom_dualized_ois_list[i];
	finfo->rom_ois_list_len = finfo->rom_dualized_ois_list_len;

	/* set -1 if not support */
	finfo->rom_header_cal_data_start_addr =
		finfo->rom_dualized_header_cal_data_start_addr;
	finfo->rom_header_version_start_addr =
		finfo->rom_dualized_header_version_start_addr;
	finfo->rom_header_cal_map_ver_start_addr =
		finfo->rom_dualized_header_cal_map_ver_start_addr;
	finfo->rom_header_isp_setfile_ver_start_addr =
		finfo->rom_dualized_header_isp_setfile_ver_start_addr;
	finfo->rom_header_project_name_start_addr =
		finfo->rom_dualized_header_project_name_start_addr;
	finfo->rom_header_module_id_addr =
		finfo->rom_dualized_header_module_id_addr;
	finfo->rom_header_sensor_id_addr =
		finfo->rom_dualized_header_sensor_id_addr;
	finfo->rom_header_mtf_data_addr =
		finfo->rom_dualized_header_mtf_data_addr;
	finfo->rom_awb_master_addr = finfo->rom_dualized_awb_master_addr;
	finfo->rom_awb_module_addr = finfo->rom_dualized_awb_module_addr;

	for (i = 0; i < AF_CAL_MAX; i++)
		finfo->rom_af_cal_addr[i] = finfo->rom_dualized_af_cal_addr[i];
	finfo->rom_af_cal_addr_len = finfo->rom_dualized_af_cal_addr_len;
	finfo->rom_paf_cal_start_addr = finfo->rom_dualized_paf_cal_start_addr;
	finfo->rom_af_cal_sac_addr = finfo->rom_dualized_af_cal_sac_addr;

#ifdef CONFIG_SEC_CAL_ENABLE
	/* standard cal */
	finfo->use_standard_cal = finfo->use_dualized_standard_cal;
	memcpy(&finfo->standard_cal_data, &finfo->standard_dualized_cal_data,
		   sizeof(struct rom_standard_cal_data));
	memcpy(&finfo->backup_standard_cal_data,
		   &finfo->backup_standard_dualized_cal_data,
		   sizeof(struct rom_standard_cal_data));
#endif

	finfo->rom_header_sensor2_id_addr =
		finfo->rom_dualized_header_sensor2_id_addr;
	finfo->rom_header_sensor2_version_start_addr =
		finfo->rom_dualized_header_sensor2_version_start_addr;
	finfo->rom_header_sensor2_mtf_data_addr =
		finfo->rom_dualized_header_sensor2_mtf_data_addr;
	finfo->rom_sensor2_awb_master_addr =
		finfo->rom_dualized_sensor2_awb_master_addr;
	finfo->rom_sensor2_awb_module_addr =
		finfo->rom_dualized_sensor2_awb_module_addr;

	for (i = 0; i < AF_CAL_MAX; i++)
		finfo->rom_sensor2_af_cal_addr[i] =
			finfo->rom_dualized_sensor2_af_cal_addr[i];

	finfo->rom_sensor2_af_cal_addr_len =
		finfo->rom_dualized_sensor2_af_cal_addr_len;
	finfo->rom_sensor2_paf_cal_start_addr =
		finfo->rom_dualized_sensor2_paf_cal_start_addr;

	finfo->rom_dualcal_slave0_start_addr =
		finfo->rom_dualized_dualcal_slave0_start_addr;
	finfo->rom_dualcal_slave0_size =
		finfo->rom_dualized_dualcal_slave0_size;
	finfo->rom_dualcal_slave1_start_addr =
		finfo->rom_dualized_dualcal_slave1_start_addr;
	finfo->rom_dualcal_slave1_size =
		finfo->rom_dualized_dualcal_slave1_size;
	finfo->rom_dualcal_slave2_start_addr =
		finfo->rom_dualized_dualcal_slave2_start_addr;
	finfo->rom_dualcal_slave2_size =
		finfo->rom_dualized_dualcal_slave2_size;

	finfo->rom_pdxtc_cal_data_start_addr =
		finfo->rom_dualized_pdxtc_cal_data_start_addr;
	finfo->rom_pdxtc_cal_data_0_size =
		finfo->rom_dualized_pdxtc_cal_data_0_size;
	finfo->rom_pdxtc_cal_data_1_size =
		finfo->rom_dualized_pdxtc_cal_data_1_size;

	finfo->rom_spdc_cal_data_start_addr =
		finfo->rom_dualized_spdc_cal_data_start_addr;
	finfo->rom_spdc_cal_data_size = finfo->rom_dualized_spdc_cal_data_size;
	finfo->rom_xtc_cal_data_start_addr =
		finfo->rom_dualized_xtc_cal_data_start_addr;
	finfo->rom_xtc_cal_data_size = finfo->rom_dualized_xtc_cal_data_size;
	finfo->rom_xgc_cal_data_start_addr =
		finfo->rom_dualized_xgc_cal_data_start_addr;
	finfo->rom_xgc_cal_data_0_size = finfo->rom_dualized_xgc_cal_data_0_size;
	finfo->rom_xgc_cal_data_1_size = finfo->rom_dualized_xgc_cal_data_1_size;
	finfo->rom_qbgc_cal_data_start_addr =
		finfo->rom_dualized_qbgc_cal_data_start_addr;
	finfo->rom_qbgc_cal_data_size = finfo->rom_dualized_qbgc_cal_data_size;
	finfo->rom_pdxtc_cal_endian_check =
		finfo->rom_dualized_pdxtc_cal_endian_check;

	for (i = 0; i < CROSSTALK_CAL_MAX; i++)
		finfo->rom_pdxtc_cal_data_addr_list[i] =
			finfo->rom_dualized_pdxtc_cal_data_addr_list[i];

	finfo->rom_pdxtc_cal_data_addr_list_len =
		finfo->rom_dualized_pdxtc_cal_data_addr_list_len;
	finfo->rom_gcc_cal_endian_check =
		finfo->rom_dualized_gcc_cal_endian_check;

	for (i = 0; i < CROSSTALK_CAL_MAX; i++)
		finfo->rom_gcc_cal_data_addr_list[i] =
			finfo->rom_dualized_gcc_cal_data_addr_list[i];

	finfo->rom_gcc_cal_data_addr_list_len =
		finfo->rom_dualized_gcc_cal_data_addr_list_len;
	finfo->rom_xtc_cal_endian_check =
		finfo->rom_dualized_xtc_cal_endian_check;

	for (i = 0; i < CROSSTALK_CAL_MAX; i++)
		finfo->rom_xtc_cal_data_addr_list[i] =
			finfo->rom_dualized_xtc_cal_data_addr_list[i];

	finfo->rom_xtc_cal_data_addr_list_len =
		finfo->rom_dualized_xtc_cal_data_addr_list_len;

	finfo->rom_dualcal_slave1_cropshift_x_addr =
		finfo->rom_dualized_dualcal_slave1_cropshift_x_addr;
	finfo->rom_dualcal_slave1_cropshift_y_addr =
		finfo->rom_dualized_dualcal_slave1_cropshift_y_addr;
	finfo->rom_dualcal_slave1_oisshift_x_addr =
		finfo->rom_dualized_dualcal_slave1_oisshift_x_addr;
	finfo->rom_dualcal_slave1_oisshift_y_addr =
		finfo->rom_dualized_dualcal_slave1_oisshift_y_addr;
	finfo->rom_dualcal_slave1_dummy_flag_addr =
		finfo->rom_dualized_dualcal_slave1_dummy_flag_addr;

#ifdef CONFIG_SEC_CAL_ENABLE
	standard_cal_data->rom_standard_cal_start_addr =
		standard_cal_data->rom_dualized_standard_cal_start_addr;
	standard_cal_data->rom_standard_cal_end_addr =
		standard_cal_data->rom_dualized_standard_cal_end_addr;
	standard_cal_data->rom_standard_cal_module_crc_addr =
		standard_cal_data->rom_dualized_standard_cal_module_crc_addr;
	standard_cal_data->rom_standard_cal_module_checksum_len =
		standard_cal_data->rom_dualized_standard_cal_module_checksum_len;
	standard_cal_data->rom_standard_cal_sec2lsi_end_addr =
		standard_cal_data->rom_dualized_standard_cal_sec2lsi_end_addr;
	standard_cal_data->rom_header_standard_cal_end_addr =
		standard_cal_data->rom_dualized_header_standard_cal_end_addr;
	standard_cal_data->rom_header_main_shading_end_addr =
		standard_cal_data->rom_dualized_header_main_shading_end_addr;
	standard_cal_data->rom_awb_start_addr =
		standard_cal_data->rom_dualized_awb_start_addr;
	standard_cal_data->rom_awb_end_addr =
		standard_cal_data->rom_dualized_awb_end_addr;
	standard_cal_data->rom_awb_section_crc_addr =
		standard_cal_data->rom_dualized_awb_section_crc_addr;
	standard_cal_data->rom_shading_start_addr =
		standard_cal_data->rom_dualized_shading_start_addr;
	standard_cal_data->rom_shading_end_addr =
		standard_cal_data->rom_dualized_shading_end_addr;
	standard_cal_data->rom_shading_section_crc_addr =
		standard_cal_data->rom_dualized_shading_section_crc_addr;
	standard_cal_data->rom_factory_start_addr =
		standard_cal_data->rom_dualized_factory_start_addr;
	standard_cal_data->rom_factory_end_addr =
		standard_cal_data->rom_dualized_factory_end_addr;
	standard_cal_data->rom_awb_sec2lsi_start_addr =
		standard_cal_data->rom_dualized_awb_sec2lsi_start_addr;
	standard_cal_data->rom_awb_sec2lsi_end_addr =
		standard_cal_data->rom_dualized_awb_sec2lsi_end_addr;
	standard_cal_data->rom_awb_sec2lsi_checksum_addr =
		standard_cal_data->rom_dualized_awb_sec2lsi_checksum_addr;
	standard_cal_data->rom_awb_sec2lsi_checksum_len =
		standard_cal_data->rom_dualized_awb_sec2lsi_checksum_len;
	standard_cal_data->rom_shading_sec2lsi_start_addr =
		standard_cal_data->rom_dualized_shading_sec2lsi_start_addr;
	standard_cal_data->rom_shading_sec2lsi_end_addr =
		standard_cal_data->rom_dualized_shading_sec2lsi_end_addr;
	standard_cal_data->rom_shading_sec2lsi_checksum_addr =
		standard_cal_data->rom_dualized_shading_sec2lsi_checksum_addr;
	standard_cal_data->rom_shading_sec2lsi_checksum_len =
		standard_cal_data->rom_dualized_shading_sec2lsi_checksum_len;
	standard_cal_data->rom_factory_sec2lsi_start_addr =
		standard_cal_data->rom_dualized_factory_sec2lsi_start_addr;

	backup_standard_cal_data->rom_standard_cal_start_addr =
		backup_standard_cal_data->rom_dualized_standard_cal_start_addr;
	backup_standard_cal_data->rom_standard_cal_end_addr =
		backup_standard_cal_data->rom_dualized_standard_cal_end_addr;
	backup_standard_cal_data->rom_standard_cal_module_crc_addr =
		backup_standard_cal_data
			->rom_dualized_standard_cal_module_crc_addr;
	backup_standard_cal_data->rom_standard_cal_module_checksum_len =
		backup_standard_cal_data
			->rom_dualized_standard_cal_module_checksum_len;
	backup_standard_cal_data->rom_standard_cal_sec2lsi_end_addr =
		backup_standard_cal_data
			->rom_dualized_standard_cal_sec2lsi_end_addr;
	backup_standard_cal_data->rom_header_standard_cal_end_addr =
		backup_standard_cal_data
			->rom_dualized_header_standard_cal_end_addr;
	backup_standard_cal_data->rom_header_main_shading_end_addr =
		backup_standard_cal_data
			->rom_dualized_header_main_shading_end_addr;
	backup_standard_cal_data->rom_awb_start_addr =
		backup_standard_cal_data->rom_dualized_awb_start_addr;
	backup_standard_cal_data->rom_awb_end_addr =
		backup_standard_cal_data->rom_dualized_awb_end_addr;
	backup_standard_cal_data->rom_awb_section_crc_addr =
		backup_standard_cal_data->rom_dualized_awb_section_crc_addr;
	backup_standard_cal_data->rom_shading_start_addr =
		backup_standard_cal_data->rom_dualized_shading_start_addr;
	backup_standard_cal_data->rom_shading_end_addr =
		backup_standard_cal_data->rom_dualized_shading_end_addr;
	backup_standard_cal_data->rom_shading_section_crc_addr =
		backup_standard_cal_data->rom_dualized_shading_section_crc_addr;
	backup_standard_cal_data->rom_factory_start_addr =
		backup_standard_cal_data->rom_dualized_factory_start_addr;
	backup_standard_cal_data->rom_factory_end_addr =
		backup_standard_cal_data->rom_dualized_factory_end_addr;
	backup_standard_cal_data->rom_awb_sec2lsi_start_addr =
		backup_standard_cal_data->rom_dualized_awb_sec2lsi_start_addr;
	backup_standard_cal_data->rom_awb_sec2lsi_end_addr =
		backup_standard_cal_data->rom_dualized_awb_sec2lsi_end_addr;
	backup_standard_cal_data->rom_awb_sec2lsi_checksum_addr =
		backup_standard_cal_data->rom_dualized_awb_sec2lsi_checksum_addr;
	backup_standard_cal_data->rom_awb_sec2lsi_checksum_len =
		backup_standard_cal_data->rom_dualized_awb_sec2lsi_checksum_len;
	backup_standard_cal_data->rom_shading_sec2lsi_start_addr =
		backup_standard_cal_data
			->rom_dualized_shading_sec2lsi_start_addr;
	backup_standard_cal_data->rom_shading_sec2lsi_end_addr =
		backup_standard_cal_data->rom_dualized_shading_sec2lsi_end_addr;
	backup_standard_cal_data->rom_shading_sec2lsi_checksum_addr =
		backup_standard_cal_data
			->rom_dualized_shading_sec2lsi_checksum_addr;
	backup_standard_cal_data->rom_shading_sec2lsi_checksum_len =
		backup_standard_cal_data
			->rom_dualized_shading_sec2lsi_checksum_len;
	backup_standard_cal_data->rom_factory_sec2lsi_start_addr =
		backup_standard_cal_data
			->rom_dualized_factory_sec2lsi_start_addr;
#endif

	finfo->is_read_dualized_values = true;
	pr_info("%s EEPROM dualized values read\n", __func__);
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

	info("Camera: read cal data from EEPROM (rom_id:%d)\n", rom_id);

	is_sec_get_sysfs_finfo(&finfo, rom_id);
	is_sec_get_cal_buf(&buf, rom_id);
	client = specific->rom_client[rom_id];

	eeprom = i2c_get_clientdata(client);

	cal_size = finfo->rom_size;
	info("%s: rom_id : %d, cal_size :%d\n", __func__, rom_id, cal_size);

#if defined(CONFIG_CAMERA_EEPROM_DUALIZED)
	is_vendor_get_module_from_position(position, &module);

	if (!module) {
		err("%s: There is no module (position : %d)\n", __func__,
			position);
		ret = false;
		goto exit;
	}
	rom_type = module->pdata->rom_type;

	if (!(sensor_id ==
		  SENSOR_NAME_HI5022)) //put all dualized sensor names here
		finfo->is_read_dualized_values = true;
	if (!(finfo->is_read_dualized_values)) {
		is_sec_readcal_eeprom_dualized(
			rom_id); // call this function only once for EEPROM dualized sensors
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

	info("Camera : EEPROM Cal map_version = %s(%x%x%x%x)\n",
		 finfo->cal_map_ver, finfo->cal_map_ver[0], finfo->cal_map_ver[1],
		 finfo->cal_map_ver[2], finfo->cal_map_ver[3]);
	info("EEPROM header version = %s(%x%x%x%x)\n", finfo->header_ver,
		 finfo->header_ver[0], finfo->header_ver[1], finfo->header_ver[2],
		 finfo->header_ver[3]);

	if (!is_sec_check_rom_ver(core, rom_id)) {
		info("Camera: Do not read eeprom cal data. EEPROM version is low.\n");
		return 0;
	}

crc_retry:

	/* read cal data */
	read_addr = 0x0;
	if (cal_size > IS_READ_MAX_EEP_CAL_SIZE) {
		for (count = 0; count < cal_size / IS_READ_MAX_EEP_CAL_SIZE;
			 count++) {
			info("Camera: I2C read cal data : offset = 0x%x, size = 0x%x\n",
				 read_addr, IS_READ_MAX_EEP_CAL_SIZE);
			ret = is_i2c_read(client, &buf[read_addr], read_addr,
					  IS_READ_MAX_EEP_CAL_SIZE);
			if (ret) {
				err("failed to is_i2c_read (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
			read_addr += IS_READ_MAX_EEP_CAL_SIZE;
		}

		if (read_addr < cal_size) {
			ret = is_i2c_read(client, &buf[read_addr], read_addr,
					  cal_size - read_addr);
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

	is_sec_parse_rom_info(finfo, buf, rom_id);

#ifdef DEBUG_FORCE_DUMP_ENABLE
	{
		char file_path[100];

		loff_t pos = 0;

		memset(file_path, 0x00, sizeof(file_path));
		snprintf(file_path, sizeof(file_path), "%srom%d_dump.bin",
			 IS_FW_DUMP_PATH, rom_id);

		if (write_data_to_file(file_path, buf, cal_size, &pos) < 0) {
			info("Failed to dump cal data. rom_id:%d\n", rom_id);
		}
	}
#endif

	/* CRC check */
	if (!is_sec_check_cal_crc32(buf, rom_id) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

	is_sec_check_module_state(finfo);

#ifdef USE_CAMERA_NOTIFY_WACOM
	if (!test_bit(IS_CRC_ERROR_HEADER, &finfo->crc_error))
		is_eeprom_info_update(rom_id, finfo->header_ver);
#endif

#ifdef CONFIG_SEC_CAL_ENABLE
	/* Store original rom data before conversion for intrinsic cal */
	if (is_sec_check_cal_crc32(buf, rom_id) == true &&
		is_need_use_standard_cal(rom_id)) {
		is_sec_get_cal_buf_rom_data(&buf_rom_data, rom_id);
		if (buf != NULL && buf_rom_data != NULL)
			memcpy(buf_rom_data, buf,
				   is_sec_get_max_cal_size(core, rom_id));
	}
#endif
exit:
	return ret;
}

int is_sec_get_pixel_size(char *header_ver)
{
	int pixelsize = 0;

	pixelsize += (int)(header_ver[FW_PIXEL_SIZE] - 0x30) * 10;
	pixelsize += (int)(header_ver[FW_PIXEL_SIZE + 1] - 0x30);

	return pixelsize;
}

int is_sec_sensorid_find(struct is_core *core)
{
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;

	is_sec_get_sysfs_finfo(&finfo, ROM_ID_REAR);

	info("%s sensor id %d\n", __func__,
		 specific->sensor_id[SENSOR_POSITION_REAR]);

	return 0;
}

int is_sec_sensorid_find_front(struct is_core *core)
{
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;

	is_sec_get_sysfs_finfo(&finfo, ROM_ID_FRONT);

	info("%s sensor id %d\n", __func__,
		 specific->sensor_id[SENSOR_POSITION_FRONT]);

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

	is_vendor_get_rom_dualcal_info_from_position(
		slave_position, &rom_type, &rom_dualcal_id, &rom_dualcal_index);
	if (rom_type == ROM_TYPE_NONE) {
		err("[rom_dualcal_id:%d pos:%d] not support, no rom for camera",
			rom_dualcal_id, slave_position);
		return -EINVAL;
	} else if (rom_dualcal_id == ROM_ID_NOTHING) {
		err("[rom_dualcal_id:%d pos:%d] invalid ROM ID", rom_dualcal_id,
			slave_position);
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
		rom_dual_flag_dummy_addr =
			finfo->rom_dualcal_slave1_dummy_flag_addr;
	} else {
		err("[index:%d] not supported index.", rom_dualcal_index);
		return -EINVAL;
	}

	if (rom_dual_flag_dummy_addr != 0 &&
		cal_buf[rom_dual_flag_dummy_addr] != 7) {
		err("[rom_dualcal_id:%d pos:%d addr:%x] invalid dummy_flag [%d]. Cannot load dual cal.",
			rom_dualcal_id, slave_position, rom_dual_flag_dummy_addr,
			cal_buf[rom_dual_flag_dummy_addr]);
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

int is_sec_fw_sel_rom(int rom_id, bool headerOnly)
{
	int ret;
	struct is_rom_info *finfo = NULL;
	struct is_module_enum *module;
	int rom_type;
	int position = is_vendor_get_position_from_rom_id(rom_id);

	if (position == SENSOR_POSITION_MAX) {
		ret = -EINVAL;
		goto EXIT;
	}

	is_sec_get_sysfs_finfo(&finfo, rom_id);

	if (!finfo) {
		err("%s: There is no cal map (rom_id : %d)\n", __func__,
			rom_id);
		ret = false;
		goto EXIT;
	}

	is_vendor_get_module_from_position(position, &module);

	if (!module) {
		err("%s: There is no module (rom_id : %d)\n", __func__, rom_id);
		ret = false;
		goto EXIT;
	}
	rom_type = module->pdata->rom_type;

	if (rom_type == ROM_TYPE_EEPROM)
		ret = is_sec_fw_sel_eeprom(rom_id, headerOnly);
	else if (rom_type == ROM_TYPE_OTPROM)
		ret = is_sec_fw_sel_otprom(rom_id, headerOnly);
	else {
		err("[%s] unknown rom_type:%d ", __func__, rom_type);
		ret = -EINVAL;
	}
EXIT:
	return ret;
}

#ifdef USE_PERSISTENT_DEVICE_PROPERTIES_FOR_CAL
int is_sec_run_fw_sel(int rom_id)
{
	struct is_core *core = is_get_is_core();
	int ret = 0;
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;

	is_sec_get_sysfs_finfo(&finfo, rom_id);

	if (!test_bit(IS_ROM_STATE_CAL_READ_DONE, &finfo->rom_state)) {
		ret = is_sec_fw_sel_rom(rom_id, false);
	}

	if (specific->check_sensor_vendor) {
		if (is_sec_check_rom_ver(core, rom_id)) {
			is_sec_check_module_state(finfo);

			if (test_bit(IS_ROM_STATE_OTHER_VENDOR,
					 &finfo->rom_state)) {
				err("Not supported module. Rom ID = %d, Module ver = %s",
					rom_id, finfo->header_ver);
				return -EIO;
			}
		}
	}

	return ret;
}

#else

int is_sec_run_fw_sel(int rom_id)
{
	struct is_core *core = is_get_is_core();
	int ret = 0;
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;
	struct is_rom_info *finfo_rear = NULL;

	is_sec_get_sysfs_finfo(&finfo, rom_id);

	/* IS_FW_DUMP_PATH folder cannot access until User unlock handset */
	if (!test_bit(IS_ROM_STATE_CAL_RELOAD, &finfo->rom_state)) {
		if (is_sec_file_exist(IS_FW_DUMP_PATH)) {
			/* Check reload cal data enabled */
			is_sec_check_reload(core);
			set_bit(IS_ROM_STATE_CAL_RELOAD, &finfo->rom_state);
			check_need_cal_dump = CAL_DUMP_STEP_CHECK;
			info("CAL_DUMP_STEP_CHECK");
		}
	}

	/* Check need to dump cal data */
	if (check_need_cal_dump == CAL_DUMP_STEP_CHECK) {
		if (test_bit(IS_ROM_STATE_CAL_READ_DONE, &finfo->rom_state)) {
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
			is_sec_cal_dump(core);
#endif
			check_need_cal_dump = CAL_DUMP_STEP_DONE;
			info("CAL_DUMP_STEP_DONE");
		}
	}

	info("%s rom id[%d] %d\n", __func__, rom_id,
		 test_bit(IS_ROM_STATE_CAL_READ_DONE, &finfo->rom_state));

	if (rom_id != ROM_ID_REAR) {
		is_sec_get_sysfs_finfo(&finfo_rear, ROM_ID_REAR);
		if (!test_bit(IS_ROM_STATE_CAL_READ_DONE,
				  &finfo_rear->rom_state) ||
			force_caldata_dump) {
			ret = is_sec_fw_sel_rom(ROM_ID_REAR, true);
		}
	}

	if (!test_bit(IS_ROM_STATE_CAL_READ_DONE, &finfo->rom_state) ||
		force_caldata_dump) {
#ifdef CONFIG_SEC_CAL_ENABLE
#ifdef USES_STANDARD_CAL_RELOAD
		if (sec2lsi_reload) {
			if (rom_id == 0) {
				for (rom_id = ROM_ID_REAR; rom_id < ROM_ID_MAX;
					 rom_id++) {
					if (specific->rom_valid[rom_id] ==
						true) {
						ret = is_sec_fw_sel_rom(rom_id,
									false);
						sec2lsi_conversion_done[rom_id] =
							false;
						info("sec2lsi reload for rom %d",
							 rom_id);
						if (ret) {
							err("is_sec_run_fw_sel for [%d] is fail(%d)",
								rom_id, ret);
							return ret;
						}
					}
				}
			}
		} else
#endif
#endif
			ret = is_sec_fw_sel_rom(rom_id, false);
	}

	if (specific->check_sensor_vendor) {
		if (is_sec_check_rom_ver(core, rom_id)) {
			is_sec_check_module_state(finfo);

			if (test_bit(IS_ROM_STATE_OTHER_VENDOR,
					 &finfo->rom_state)) {
				err("Not supported module. Rom ID = %d, Module ver = %s",
					rom_id, finfo->header_ver);
				return -EIO;
			}
		}
	}

	return ret;
}
#endif

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
	switch (sensor_id) {
	case SENSOR_NAME_S5KJN1:
		ret = is_modify_remosaic_cal_buf(sensor_position, cal_buf, buf,
						 size);
		break;
	default:
		*buf = &cal_buf[start_addr];
		*size = finfo->rom_xtc_cal_data_size;
		break;
	}
#else
	*buf = &cal_buf[start_addr];
	*size = finfo->rom_xtc_cal_data_size;
#endif
	return ret;
}

#ifdef MODIFY_CAL_MAP_FOR_SWREMOSAIC_LIB
int is_modify_remosaic_cal_buf(int sensor_position, char *cal_buf, char **buf,
				   int *size)
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

	cal_size = finfo->rear_remosaic_tetra_xtc_size +
		   finfo->rear_remosaic_sensor_xtc_size +
		   finfo->rear_remosaic_pdxtc_size +
		   finfo->rear_remosaic_sw_ggc_size;

	start_addr = finfo->rom_xtc_cal_data_start_addr;

	if (cal_size <= 0) {
		err("[%s]: invalid cal_size(%d)", __func__, cal_size);
		return -EINVAL;
	}

	modified_cal_buf = (char *)kmalloc(cal_size * sizeof(char), GFP_KERNEL);

	memcpy(&modified_cal_buf[curr_pos],
		   &cal_buf[finfo->rear_remosaic_tetra_xtc_start_addr],
		   finfo->rear_remosaic_tetra_xtc_size);
	curr_pos += finfo->rear_remosaic_tetra_xtc_size;

	memcpy(&modified_cal_buf[curr_pos],
		   &cal_buf[finfo->rear_remosaic_sensor_xtc_start_addr],
		   finfo->rear_remosaic_sensor_xtc_size);
	curr_pos += finfo->rear_remosaic_sensor_xtc_size;

	memcpy(&modified_cal_buf[curr_pos],
		   &cal_buf[finfo->rear_remosaic_pdxtc_start_addr],
		   finfo->rear_remosaic_pdxtc_size);
	curr_pos += finfo->rear_remosaic_pdxtc_size;

	memcpy(&modified_cal_buf[curr_pos],
		   &cal_buf[finfo->rear_remosaic_sw_ggc_start_addr],
		   finfo->rear_remosaic_sw_ggc_size);

	*buf = &modified_cal_buf[0];
	*size = cal_size;

	return 0;
}
#endif

int is_sec_fw_sel_eeprom(int rom_id, bool headerOnly)
{
	int ret = 0;
#ifdef USE_KERNEL_VFS_READ_WRITE
	char fw_path[100];
	char phone_fw_version[IS_HEADER_VER_SIZE + 1] = {
		0,
	};
	struct file *fp = NULL;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
#endif
	bool is_ldo_enabled;
	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;
	struct is_rom_info *pinfo = NULL;
	bool camera_running;
	struct is_module_enum *module = NULL;
	int position = is_vendor_get_position_from_rom_id(rom_id);

	is_vendor_get_module_from_position(position, &module);
	if (!module) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	if (module->pdata->rom_type == ROM_TYPE_OTPROM) {
		ret = -ENODEV;
		return ret;
	}

	is_sec_get_sysfs_pinfo(&pinfo, rom_id);
	is_sec_get_sysfs_finfo(&finfo, rom_id);

	is_ldo_enabled = false;

	if (test_bit(IS_ROM_STATE_SKIP_CAL_LOADING, &finfo->rom_state)) {
		info("%s: skipping cal read as skip_cal_loading is enabled for rom_id %d",
			 __func__, rom_id);
		return 0;
	}
	/* Use mutex for i2c read */
	mutex_lock(&specific->rom_lock);

	if (!test_bit(IS_ROM_STATE_CAL_READ_DONE, &finfo->rom_state) ||
		force_caldata_dump) {
		if (rom_id == ROM_ID_REAR)
			is_dumped_fw_loading_needed = false;

		if (force_caldata_dump)
			info("forced caldata dump!!\n");

		is_sec_rom_power_on(core, finfo->rom_power_position);
		is_ldo_enabled = true;

		info("Camera: read cal data from EEPROM (ROM ID:%d)\n", rom_id);
		if (rom_id == ROM_ID_REAR && headerOnly) {
			is_sec_read_eeprom_header(rom_id);
		} else {
			if (!is_sec_readcal_eeprom(rom_id)) {
				set_bit(IS_ROM_STATE_CAL_READ_DONE,
					&finfo->rom_state);
			}
		}

		if (rom_id != ROM_ID_REAR) {
			goto exit;
		}
	}

	is_sec_sensorid_find(core);
	if (headerOnly) {
		goto exit;
	}

#ifdef USE_KERNEL_VFS_READ_WRITE
	snprintf(fw_path, sizeof(fw_path), "%s%s", IS_FW_PATH, IS_DDK);

	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		err("Camera: Failed open phone firmware");
		ret = -EIO;
		fp = NULL;
		goto read_phone_fw_exit;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	info("start, file path %s, size %ld Bytes\n", fw_path, fsize);

	info("Phone FW size is larger than FW buffer. Use vmalloc.\n");
	read_buf = vmalloc(fsize);
	if (!read_buf) {
		err("failed to allocate memory");
		ret = -ENOMEM;
		goto read_phone_fw_exit;
	}
	temp_buf = read_buf;

	nread = kernel_read(fp, temp_buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		err("failed to read firmware file, %ld Bytes", nread);
		ret = -EIO;
		goto read_phone_fw_exit;
	}

	strncpy(phone_fw_version, temp_buf + nread - IS_HEADER_VER_OFFSET,
		IS_HEADER_VER_SIZE);
	strncpy(pinfo->header_ver, temp_buf + nread - IS_HEADER_VER_OFFSET,
		IS_HEADER_VER_SIZE);
	info("Camera: phone fw version: %s\n", phone_fw_version);

read_phone_fw_exit:
	if (read_buf) {
		vfree(read_buf);
		read_buf = NULL;
		temp_buf = NULL;
	}

	if (fp) {
		filp_close(fp, current->files);
		fp = NULL;
	}
#endif

exit:
	camera_running =
		is_vendor_check_camera_running(finfo->rom_power_position);
	if (is_ldo_enabled && !camera_running) {
		is_sec_rom_power_off(core, finfo->rom_power_position);
		msleep(20);
	}

	mutex_unlock(&specific->rom_lock);

	return ret;
}

int is_sec_fw_sel_otprom(int rom_id, bool headerOnly)
{
	int ret = 0;
#ifdef USE_KERNEL_VFS_READ_WRITE
	char fw_path[100];
	char phone_fw_version[IS_HEADER_VER_SIZE + 1] = {
		0,
	};
	struct file *fp = NULL;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
#endif
	bool is_ldo_enabled;
	struct is_core *core = is_get_is_core();
	struct is_vender_specific *specific = core->vender.private_data;
	struct is_rom_info *finfo = NULL;
	struct is_rom_info *pinfo = NULL;
	bool camera_running;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri;
	int position = is_vendor_get_position_from_rom_id(rom_id);

	is_vendor_get_module_from_position(position, &module);
	if (!module) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
		return ret;
	}

	if (module->pdata->rom_type == ROM_TYPE_EEPROM) {
		ret = -ENODEV;
		return ret;
	}

	is_sec_get_sysfs_pinfo(&pinfo, rom_id);
	is_sec_get_sysfs_finfo(&finfo, rom_id);

	is_ldo_enabled = false;

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	if (sensor_peri->cis.client) {
		specific->rom_client[rom_id] = sensor_peri->cis.client;
		info("%s sensor client will be used for otprom", __func__);
	}

	if (test_bit(IS_ROM_STATE_SKIP_CAL_LOADING, &finfo->rom_state)) {
		info("%s: skipping cal read as skip_cal_loading is enabled for rom_id %d",
			 __func__, rom_id);
		return 0;
	}
	/* Temp code for sensor bring up - Start - Remove this after HI1336 Cal Bring-up*/
#if defined(A33X_TEMP_DISABLE_UW_OTP_CAL_LOADING)
	if (rom_id == 4) {
		info("%s Skip cal for UW dualization check");
		return 0;
	}
#endif
	/* Use mutex for i2c read */
	mutex_lock(&specific->rom_lock);

	if (!test_bit(IS_ROM_STATE_CAL_READ_DONE, &finfo->rom_state) ||
		force_caldata_dump) {
		if (rom_id == ROM_ID_REAR)
			is_dumped_fw_loading_needed = false;

		if (force_caldata_dump)
			info("forced caldata dump!!\n");

		is_sec_rom_power_on(core, finfo->rom_power_position);
		is_ldo_enabled = true;

		info("Camera: read cal data from OTPROM (ROM ID:%d)\n", rom_id);
		if (rom_id == ROM_ID_REAR && headerOnly) {
			is_sec_read_otprom_header(rom_id);
		} else {
			if (!is_sec_readcal_otprom(rom_id)) {
				set_bit(IS_ROM_STATE_CAL_READ_DONE,
					&finfo->rom_state);
			}
		}

		if (rom_id != ROM_ID_REAR) {
			goto exit;
		}
	}

	is_sec_sensorid_find(core);
	if (headerOnly) {
		goto exit;
	}

#ifdef USE_KERNEL_VFS_READ_WRITE
	snprintf(fw_path, sizeof(fw_path), "%s%s", IS_FW_PATH, IS_DDK);
	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		err("Camera: Failed open phone firmware");
		ret = -EIO;
		fp = NULL;
		goto read_phone_fw_exit;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	info("start, file path %s, size %ld Bytes\n", fw_path, fsize);

	info("Phone FW size is larger than FW buffer. Use vmalloc.\n");
	read_buf = vmalloc(fsize);
	if (!read_buf) {
		err("failed to allocate memory");
		ret = -ENOMEM;
		goto read_phone_fw_exit;
	}
	temp_buf = read_buf;

	nread = kernel_read(fp, temp_buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		err("failed to read firmware file, %ld Bytes", nread);
		ret = -EIO;
		goto read_phone_fw_exit;
	}

	strncpy(phone_fw_version, temp_buf + nread - IS_HEADER_VER_OFFSET,
		IS_HEADER_VER_SIZE);
	strncpy(pinfo->header_ver, temp_buf + nread - IS_HEADER_VER_OFFSET,
		IS_HEADER_VER_SIZE);
	info("Camera: phone fw version: %s\n", phone_fw_version);

read_phone_fw_exit:
	if (read_buf) {
		vfree(read_buf);
		read_buf = NULL;
		temp_buf = NULL;
	}

	if (fp) {
		filp_close(fp, current->files);
		fp = NULL;
	}
#endif

exit:
	camera_running =
		is_vendor_check_camera_running(finfo->rom_power_position);
	if (is_ldo_enabled && !camera_running) {
		is_sec_rom_power_off(core, finfo->rom_power_position);
		msleep(20);
	}

	mutex_unlock(&specific->rom_lock);

	return ret;
}
