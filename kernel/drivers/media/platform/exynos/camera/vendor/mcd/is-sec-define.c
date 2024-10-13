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
#include "is-vendor-private.h"
#include "is-interface-library.h"
#include "is-device-sensor-peri.h"

#include <linux/i2c.h>
#include "is-device-rom.h"
#include "is-notifier.h"
#include "is-sysfs.h"
#if defined(CONFIG_OIS_USE)
#include "is-sysfs-ois.h"
#endif

#define IS_LATEST_ROM_VERSION_M	'M'
#define IS_READ_MAX_EEP_CAL_SIZE (0x8000) /* 32KB */

int is_sec_get_rom_info(struct is_vendor_rom **rom_info, int rom_id)
{
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;

	if (rom_id < ROM_ID_MAX)
		*rom_info = &vendor_priv->rom[rom_id];
	else
		*rom_info = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(is_sec_get_rom_info);

bool is_sec_check_awb_lsc_crc32_post_sec2lsi(char *buf, int position)
{
	u8 *buf8 = NULL;
	u32 checksum, check_base, checksum_base;
	u32 address_boundary;
	int rom_id;
	bool crc32_check_temp = true;
	int awb_length = 0;
	int lsc_length = 0;
	int awb_sec2lsi_end_addr = 0;
	int lsc_sec2lsi_end_addr = 0;

	struct is_vendor_rom *rom_info = NULL;
	struct is_vendor_standard_cal *standard_cal_data;

	buf8 = (u8 *)buf;

	/***** START CHECK CRC *****/
	rom_id = is_vendor_get_rom_id_from_position(position);
	if (rom_id == ROM_ID_NOTHING) {
		err("Failed to get rom_id");
		crc32_check_temp = false;
		goto exit;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	address_boundary = rom_info->rom_size;

	standard_cal_data = &(rom_info->standard_cal_data);

	awb_length = standard_cal_data->rom_awb_sec2lsi_checksum_len;
	lsc_length = standard_cal_data->rom_shading_sec2lsi_checksum_len;

	/* AWB Cal Data CRC CHECK */
	if (awb_length > 0) {
		checksum = 0;
		check_base = standard_cal_data->rom_awb_sec2lsi_start_addr;
		checksum_base = standard_cal_data->rom_awb_sec2lsi_checksum_addr;
		awb_sec2lsi_end_addr = standard_cal_data->rom_awb_sec2lsi_checksum_addr
				+ (SEC2LSI_CHECKSUM_SIZE - 1);

#ifdef ROM_CRC32_DEBUG
		pr_info("[CRC32_DEBUG] AWB CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
			awb_length, standard_cal_data->rom_awb_sec2lsi_checksum_addr);
		pr_info("[CRC32_DEBUG] start = 0x%08X, end = 0x%08X\n",
			standard_cal_data->rom_awb_sec2lsi_start_addr, awb_sec2lsi_end_addr);
#endif

		if (check_base > address_boundary || checksum_base > address_boundary) {
			err("[%d]: AWB address has error: start(0x%08X), end(0x%08X)",
				position, standard_cal_data->rom_awb_sec2lsi_start_addr, awb_sec2lsi_end_addr);
			crc32_check_temp = false;
			goto exit;
		}

		checksum = (u32)getCRC((u16 *)&buf8[check_base], awb_length, NULL, NULL);
		if (checksum != *((u32 *)&buf8[checksum_base])) {
			err("[%d]: AWB address has error: start(0x%08X), end(0x%08X) Crc address(0x%08X)",
				position, standard_cal_data->rom_awb_sec2lsi_start_addr, awb_sec2lsi_end_addr,
				standard_cal_data->rom_awb_sec2lsi_checksum_addr);
			err("[%d]: CRC32 error at the AWB (0x%08X != 0x%08X)",
				position, checksum, *((u32 *)&buf8[checksum_base]));
			crc32_check_temp = false;
			goto exit;
		} else {
			info("%s : AWB CRC is pass!\n", __func__);
		}
	} else {
		err("[%d]: Skip to check awb crc32\n", position);
	}

	/* Shading Cal Data CRC CHECK*/
	if (lsc_length > 0) {
		checksum = 0;
		check_base = standard_cal_data->rom_shading_sec2lsi_start_addr;
		checksum_base = standard_cal_data->rom_shading_sec2lsi_checksum_addr;
		lsc_sec2lsi_end_addr = standard_cal_data->rom_shading_sec2lsi_checksum_addr
				+ (SEC2LSI_CHECKSUM_SIZE - 1);

#ifdef ROM_CRC32_DEBUG
		pr_info("[CRC32_DEBUG] Shading CRC32 Check. check_length = %d, crc addr = 0x%08X\n",
			lsc_length, standard_cal_data->rom_shading_sec2lsi_checksum_addr);
		pr_info("[CRC32_DEBUG] start = 0x%08X, end = 0x%08X\n",
			standard_cal_data->rom_shading_sec2lsi_start_addr, lsc_sec2lsi_end_addr);
#endif

		if (check_base > address_boundary || checksum_base > address_boundary) {
			err("[%d]: Shading address has error: start(0x%08X), end(0x%08X)",
				position, standard_cal_data->rom_shading_sec2lsi_start_addr,
				lsc_sec2lsi_end_addr);
			crc32_check_temp = false;
			goto exit;
		}

		checksum = (u32)getCRC((u16 *)&buf8[check_base], lsc_length, NULL, NULL);
		if (checksum != *((u32 *)&buf8[checksum_base])) {
			err("[%d]: Shading address has error: start(0x%08X), end(0x%08X) Crc address(0x%08X)",
				position, standard_cal_data->rom_shading_sec2lsi_start_addr,
				lsc_length,
				standard_cal_data->rom_shading_sec2lsi_checksum_addr);
			err("[%d]: CRC32 error at the Shading (0x%08X != 0x%08X)",
				position, checksum, *((u32 *)&buf8[checksum_base]));
			crc32_check_temp = false;
			goto exit;
		} else {
			info("%s : LSC CRC is pass!\n", __func__);
		}
	} else {
		err("[%d]: Skip to check shading crc32\n", position);
	}

exit:
	return crc32_check_temp;
}

int is_sec_compare_ver(int rom_id)
{
	struct is_vendor_rom *rom_info = NULL;

	is_sec_get_rom_info(&rom_info, rom_id);

	if (rom_info->cal_map_ver[0] == 'V'
		&& rom_info->cal_map_ver[1] >= '0' && rom_info->cal_map_ver[1] <= '9'
		&& rom_info->cal_map_ver[2] >= '0' && rom_info->cal_map_ver[2] <= '9'
		&& rom_info->cal_map_ver[3] >= '0' && rom_info->cal_map_ver[3] <= '9') {
		return ((rom_info->cal_map_ver[2] - '0') * 10) + (rom_info->cal_map_ver[3] - '0');
	} else {
		err("ROM core version is invalid. version is %c%c%c%c",
			rom_info->cal_map_ver[0], rom_info->cal_map_ver[1],
			rom_info->cal_map_ver[2], rom_info->cal_map_ver[3]);
		return -1;
	}
}

bool is_sec_check_rom_ver(struct is_core *core, int rom_id)
{
	struct is_vendor_rom *rom_info = NULL;
	char compare_version;
	int rom_ver;
	int latest_rom_ver;

	is_sec_get_rom_info(&rom_info, rom_id);

	if (rom_info->skip_cal_loading) {
		err("[rom_id:%d] skip_cal_loading implemented", rom_id);
		return false;
	}

	if (rom_info->skip_header_loading) {
		err("[rom_id:%d] skip_header_loading implemented", rom_id);
		return true;
	}

	latest_rom_ver = rom_info->cal_map_es_version;
	compare_version = rom_info->camera_module_es_version;

	rom_ver = is_sec_compare_ver(rom_id);

	if ((rom_ver < latest_rom_ver) ||
		(rom_info->header_ver[10] < compare_version)) {
		err("[%d]invalid rom version. rom_ver %d, header_ver[10] %c\n",
			rom_id, rom_ver, rom_info->header_ver[10]);
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
	bool crc32_all_section, crc32_header, crc32_dual;
	struct is_core *core;
 	int i;
	struct is_vendor_rom *rom_info;

	core = is_get_is_core();
	buf8 = (u8 *)buf;

	info("+++ %s\n", __func__);

	is_sec_get_rom_info(&rom_info, rom_id);
	rom_info->crc_error = false;

	if (rom_info->skip_crc_check) {
		info("%s : skip crc check. return\n", __func__);
		return true;
	}

	crc32_all_section = true;
	crc32_header = true;
	crc32_dual = true;

	address_boundary = IS_MAX_CAL_SIZE;

	/* header crc check */
	for (i = 0; i < rom_info->header_crc_check_list_len; i += 3) {
		checksum = 0;
		check_base = rom_info->header_crc_check_list[i];
		check_length = (rom_info->header_crc_check_list[i+1] - rom_info->header_crc_check_list[i] + 1);
		checksum_base = rom_info->header_crc_check_list[i+2];

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("[rom%d/header cal:%d] address has error: start(0x%08X), end(0x%08X)",
				rom_id, i, rom_info->header_crc_check_list[i], rom_info->header_crc_check_list[i+1]);
			crc32_header = false;
			crc32_all_section = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf8[check_base], check_length, NULL, NULL);
		if (checksum != *((u32 *)&buf8[checksum_base])) {
			err("[rom%d/header cal:%d] CRC32 error (0x%08X != 0x%08X), base[0x%X] len[0x%X] checksum[0x%X]",
				rom_id, i, checksum, *((u32 *)&buf8[checksum_base]), check_base, check_length, checksum_base);
			crc32_header = false;
			crc32_all_section = false;
			goto out;
		}
	}

	/* main crc check */
	for (i = 0; i < rom_info->crc_check_list_len; i += 3) {
		checksum = 0;
		check_base = rom_info->crc_check_list[i];
		check_length = (rom_info->crc_check_list[i+1] - rom_info->crc_check_list[i] + 1);
		checksum_base = rom_info->crc_check_list[i+2];

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("[rom%d/main cal:%d] address has error: start(0x%08X), end(0x%08X)",
				rom_id, i, rom_info->crc_check_list[i], rom_info->crc_check_list[i+1]);
			crc32_all_section = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf8[check_base], check_length, NULL, NULL);
		if (checksum != *((u32 *)&buf8[checksum_base])) {
			err("[rom%d/main cal:%d] CRC32 error (0x%08X != 0x%08X), base[0x%X] len[0x%X] checksum[0x%X]",
				rom_id, i, checksum, *((u32 *)&buf8[checksum_base]), check_base, check_length, checksum_base);
			crc32_all_section = false;
			goto out;
		}
	}

	/* dual crc check */
	for (i = 0; i < rom_info->dual_crc_check_list_len; i += 3) {
		checksum = 0;
		check_base = rom_info->dual_crc_check_list[i];
		check_length = (rom_info->dual_crc_check_list[i+1] - rom_info->dual_crc_check_list[i] + 1);
		checksum_base = rom_info->dual_crc_check_list[i+2];

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("[rom%d/dual cal:%d] data address has error: start(0x%08X), end(0x%08X)",
				rom_id, i, rom_info->dual_crc_check_list[i], rom_info->dual_crc_check_list[i+1]);
			crc32_all_section = false;
			crc32_dual = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf8[check_base], check_length, NULL, NULL);
		if (checksum != *((u32 *)&buf8[checksum_base])) {
			err("[rom%d/dual cal:%d] CRC32 error (0x%08X != 0x%08X), base[0x%X] len[0x%X] checksum[0x%X]",
				rom_id, i, checksum, *((u32 *)&buf8[checksum_base]), check_base, check_length, checksum_base);
			crc32_all_section = false;
			crc32_dual = false;
			goto out;
		}
	}

out:
	if (!crc32_all_section)
		rom_info->crc_error = true;
	else
		rom_info->crc_error = false;

	info("[rom_id:%d] crc32_all_section %d crc32_header %d crc32_dual %d\n",
		rom_id, crc32_all_section, crc32_header, crc32_dual);

	return crc32_all_section;
}

void remove_dump_fw_file(void)
{
#ifdef USE_KERNEL_VFS_READ_WRITE
	mm_segment_t old_fs;
	long ret;
	char fw_path[100];
	struct is_vendor_rom *rom_info = NULL;
	struct file *fp;
	struct inode *parent_inode;

	old_fs = force_uaccess_begin();

	is_sec_get_rom_info(&rom_info, ROM_ID_REAR);

	/* RTA binary */
	snprintf(fw_path, sizeof(fw_path), "%s%s", IS_FW_DUMP_PATH, rom_info->load_rta_fw_name);

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
	snprintf(fw_path, sizeof(fw_path), "%s%s", IS_FW_DUMP_PATH, rom_info->load_fw_name);

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

	force_uaccess_end(old_fs);

#endif
}

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos)
{
	ssize_t tx = -ENOENT;
#ifdef USE_KERNEL_VFS_READ_WRITE
	struct file *fp;

	fp = filp_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
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

	old_fs = force_uaccess_begin();

	fp = filp_open(name, O_RDONLY, 0);

	if (IS_ERR(fp)) {
		exist = false;
		info("%s: error %d, failed to open %s\n", __func__, PTR_ERR(fp), name);
	} else {
		filp_close(fp, NULL);
	}

	force_uaccess_end(old_fs);
	return exist;
#endif
	return false;
}

int is_sec_rom_power_on(struct is_core *core, int position)
{
	int ret = 0;
	struct exynos_platform_is_module *module_pdata;
	struct is_module_enum *module = NULL;
	struct is_vendor_private *vendor_priv = core->vendor.private_data;
	int i = 0;

	info("%s: Sensor position = %d.\n", __func__, position);

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

	/* 20 ms margin is required between every cal loading */
	if (vendor_priv->eeprom_on_delay)
		msleep(vendor_priv->eeprom_on_delay);

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

	info("%s: Sensor position = %d.\n", __func__, position);

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

void is_sec_check_module_state(struct is_vendor_rom *rom_info)
{
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;

	if (rom_info->skip_header_loading) {
		rom_info->other_vendor_module = false;
		info("%s : skip header loading. return ", __func__);
		return;
	}

	if (rom_info->header_ver[3] == 'L' || rom_info->header_ver[3] == 'X'
#ifdef CAMERA_STANDARD_CAL_ISP_VERSION
		|| rom_info->header_ver[3] == CAMERA_STANDARD_CAL_ISP_VERSION
#endif
		)
		rom_info->other_vendor_module = false;
	else
		rom_info->other_vendor_module = true;

	if (vendor_priv->use_module_check) {
		if (rom_info->header_ver[10] >= rom_info->camera_module_es_version)
			rom_info->latest_module = true;
		else
			rom_info->latest_module = false;

		if (rom_info->header_ver[10] == IS_LATEST_ROM_VERSION_M)
			rom_info->final_module = true;
		else
			rom_info->final_module = false;
	} else {
		rom_info->latest_module = true;
		rom_info->final_module = true;
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
		info("%s: client is null\n", __func__);
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

		info("%s: i2c_master_send failed(%d), try %d\n", __func__, ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		pr_err("%s: error %d, fail to write 0x%04X\n", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	return 0;
}

int is_sec_parse_rom_info(struct is_vendor_rom *rom_info, char *buf, int rom_id)
{
#if defined(CONFIG_OIS_USE)
	struct is_ois_info *ois_pinfo = NULL;
#endif

	if (rom_info->rom_header_cal_map_ver_start_addr != -1) {
		memcpy(rom_info->cal_map_ver, &buf[rom_info->rom_header_cal_map_ver_start_addr],
			IS_CAL_MAP_VER_SIZE);
		rom_info->cal_map_ver[IS_CAL_MAP_VER_SIZE] = '\0';
	}

	if (rom_info->rom_header_version_start_addr != -1) {
		memcpy(rom_info->header_ver, &buf[rom_info->rom_header_version_start_addr],
			IS_HEADER_VER_SIZE);
		rom_info->header_ver[IS_HEADER_VER_SIZE] = '\0';
	}

	if (rom_info->rom_header_sensor2_version_start_addr != -1) {
		memcpy(rom_info->header2_ver, &buf[rom_info->rom_header_sensor2_version_start_addr],
			IS_HEADER_VER_SIZE);
		rom_info->header2_ver[IS_HEADER_VER_SIZE] = '\0';
	}

	if (rom_info->rom_header_project_name_start_addr != -1) {
		memcpy(rom_info->project_name, &buf[rom_info->rom_header_project_name_start_addr],
			IS_PROJECT_NAME_SIZE);
		rom_info->project_name[IS_PROJECT_NAME_SIZE] = '\0';
	}

	if (rom_info->rom_header_module_id_addr != -1) {
		memcpy(rom_info->rom_module_id, &buf[rom_info->rom_header_module_id_addr],
			IS_MODULE_ID_SIZE);
		rom_info->rom_module_id[IS_MODULE_ID_SIZE] = '\0';
	}

	if (rom_info->rom_header_sensor_id_addr != -1) {
		memcpy(rom_info->rom_sensor_id, &buf[rom_info->rom_header_sensor_id_addr],
			IS_SENSOR_ID_SIZE);
		rom_info->rom_sensor_id[IS_SENSOR_ID_SIZE] = '\0';
	}

	if (rom_info->rom_header_sensor2_id_addr != -1) {
		memcpy(rom_info->rom_sensor2_id, &buf[rom_info->rom_header_sensor2_id_addr],
			IS_SENSOR_ID_SIZE);
		rom_info->rom_sensor2_id[IS_SENSOR_ID_SIZE] = '\0';
	}

#if defined(CONFIG_OIS_USE)
	if (rom_info->rom_ois_list_len == IS_ROM_OIS_MAX_LIST) {
		is_ois_get_phone_version(&ois_pinfo);
		if (rom_id == WIDE_OIS_ROM_ID) {
			memcpy(ois_pinfo->wide_romdata.xgg, &buf[rom_info->rom_ois_list[0]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.ygg, &buf[rom_info->rom_ois_list[1]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.xcoef, &buf[rom_info->rom_ois_list[2]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.ycoef, &buf[rom_info->rom_ois_list[3]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.supperssion_xratio, &buf[rom_info->rom_ois_list[4]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.supperssion_yratio, &buf[rom_info->rom_ois_list[5]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.cal_mark, &buf[rom_info->rom_ois_list[6]], IS_OIS_CAL_MARK_DATA_SIZE);
		}
#ifdef CAMERA_2ND_OIS
		if (rom_id == TELE_OIS_ROM_ID) {
			memcpy(ois_pinfo->tele_romdata.xgg, &buf[rom_info->rom_ois_list[7]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->tele_romdata.ygg, &buf[rom_info->rom_ois_list[8]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->tele_romdata.xcoef, &buf[rom_info->rom_ois_list[9]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->tele_romdata.ycoef, &buf[rom_info->rom_ois_list[10]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->tele_romdata.supperssion_xratio, &buf[rom_info->rom_ois_list[11]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->tele_romdata.supperssion_yratio, &buf[rom_info->rom_ois_list[12]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->tele_romdata.cal_mark, &buf[rom_info->rom_ois_list[13]], IS_OIS_CAL_MARK_DATA_SIZE);
		}
#endif
#ifdef CAMERA_3RD_OIS
		if (rom_id == TELE2_OIS_ROM_ID) {
			memcpy(ois_pinfo->tele2_romdata.xgg, &buf[rom_info->rom_ois_list[7]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->tele2_romdata.ygg, &buf[rom_info->rom_ois_list[8]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->tele2_romdata.xcoef, &buf[rom_info->rom_ois_list[9]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->tele2_romdata.ycoef, &buf[rom_info->rom_ois_list[10]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->tele2_romdata.supperssion_xratio, &buf[rom_info->rom_ois_list[11]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->tele2_romdata.supperssion_yratio, &buf[rom_info->rom_ois_list[12]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->tele2_romdata.cal_mark, &buf[rom_info->rom_ois_list[13]], IS_OIS_CAL_MARK_DATA_SIZE);
		}
#endif
#ifdef CAMERA_2ND_OIS
		if (rom_id == TELE_OIS_TILT_ROM_ID) {
			memcpy(ois_pinfo->tele_tilt_romdata.xgg, &buf[rom_info->rom_ois_list[7]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->tele_tilt_romdata.ygg, &buf[rom_info->rom_ois_list[8]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->tele_tilt_romdata.xcoef, &buf[rom_info->rom_ois_list[9]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->tele_tilt_romdata.ycoef, &buf[rom_info->rom_ois_list[10]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->tele_tilt_romdata.supperssion_xratio, &buf[rom_info->rom_ois_list[11]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->tele_tilt_romdata.supperssion_yratio, &buf[rom_info->rom_ois_list[12]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->tele_tilt_romdata.cal_mark, &buf[rom_info->rom_ois_list[13]], IS_OIS_CAL_MARK_DATA_SIZE);
		}
#endif
	} else if (rom_info->rom_ois_list_len == IS_ROM_OIS_SINGLE_MODULE_MAX_LIST) {
		is_ois_get_phone_version(&ois_pinfo);

		if (rom_id == WIDE_OIS_ROM_ID) {
			memcpy(ois_pinfo->wide_romdata.xgg, &buf[rom_info->rom_ois_list[0]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.ygg, &buf[rom_info->rom_ois_list[1]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.xcoef, &buf[rom_info->rom_ois_list[2]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.ycoef, &buf[rom_info->rom_ois_list[3]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.supperssion_xratio, &buf[rom_info->rom_ois_list[4]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.supperssion_yratio, &buf[rom_info->rom_ois_list[5]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->wide_romdata.cal_mark, &buf[rom_info->rom_ois_list[6]], IS_OIS_CAL_MARK_DATA_SIZE);
		}
#ifdef CAMERA_2ND_OIS
		if (rom_id == TELE_OIS_ROM_ID) {
			memcpy(ois_pinfo->tele_romdata.xgg, &buf[rom_info->rom_ois_list[0]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->tele_romdata.ygg, &buf[rom_info->rom_ois_list[1]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->tele_romdata.xcoef, &buf[rom_info->rom_ois_list[2]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->tele_romdata.ycoef, &buf[rom_info->rom_ois_list[3]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->tele_romdata.supperssion_xratio, &buf[rom_info->rom_ois_list[4]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->tele_romdata.supperssion_yratio, &buf[rom_info->rom_ois_list[5]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->tele_romdata.cal_mark, &buf[rom_info->rom_ois_list[6]], IS_OIS_CAL_MARK_DATA_SIZE);
		}
#endif
#ifdef CAMERA_3RD_OIS
		if (rom_id == TELE2_OIS_ROM_ID) {
			memcpy(ois_pinfo->tele2_romdata.xgg, &buf[rom_info->rom_ois_list[0]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->tele2_romdata.ygg, &buf[rom_info->rom_ois_list[1]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->tele2_romdata.xcoef, &buf[rom_info->rom_ois_list[2]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->tele2_romdata.ycoef, &buf[rom_info->rom_ois_list[3]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->tele2_romdata.supperssion_xratio, &buf[rom_info->rom_ois_list[4]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->tele2_romdata.supperssion_yratio, &buf[rom_info->rom_ois_list[5]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->tele2_romdata.cal_mark, &buf[rom_info->rom_ois_list[6]], IS_OIS_CAL_MARK_DATA_SIZE);
		}
#endif
#ifdef CAMERA_2ND_OIS
		if (rom_id == TELE_OIS_TILT_ROM_ID) {
			memcpy(ois_pinfo->tele_tilt_romdata.xgg, &buf[rom_info->rom_ois_list[0]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->tele_tilt_romdata.ygg, &buf[rom_info->rom_ois_list[1]], IS_OIS_GYRO_DATA_SIZE);
			memcpy(ois_pinfo->tele_tilt_romdata.xcoef, &buf[rom_info->rom_ois_list[2]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->tele_tilt_romdata.ycoef, &buf[rom_info->rom_ois_list[3]], IS_OIS_COEF_DATA_SIZE);
			memcpy(ois_pinfo->tele_tilt_romdata.supperssion_xratio, &buf[rom_info->rom_ois_list[4]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->tele_tilt_romdata.supperssion_yratio, &buf[rom_info->rom_ois_list[5]],
				IS_OIS_SUPPERSSION_RATIO_DATA_SIZE);
			memcpy(ois_pinfo->tele_tilt_romdata.cal_mark, &buf[rom_info->rom_ois_list[6]], IS_OIS_CAL_MARK_DATA_SIZE);
		}
#endif
	}
#endif /* CONFIG_OIS_USE */

	if (rom_info->use_standard_cal)
		memcpy(rom_info->sec_buf, buf, rom_info->rom_size);
	/* debug info dump */
	info("++++ ROM data info - rom_id:%d\n", rom_id);
	info("ROM Cal Map Version = %s(%x%x%x%x)\n", rom_info->cal_map_ver, rom_info->cal_map_ver[0],
			rom_info->cal_map_ver[1], rom_info->cal_map_ver[2], rom_info->cal_map_ver[3]);
	info("Module Header Version = %s(%x%x%x%x)\n", rom_info->header_ver,
		rom_info->header_ver[0], rom_info->header_ver[1], rom_info->header_ver[2], rom_info->header_ver[3]);
	info("Module info : %s\n", rom_info->header_ver);
	info(" Project name : %s\n", rom_info->project_name);
	info(" Cal data map ver : %s\n", rom_info->cal_map_ver);
	info(" MODULE ID : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
		rom_info->rom_module_id[0], rom_info->rom_module_id[1], rom_info->rom_module_id[2],
		rom_info->rom_module_id[3], rom_info->rom_module_id[4], rom_info->rom_module_id[5],
		rom_info->rom_module_id[6], rom_info->rom_module_id[7], rom_info->rom_module_id[8],
		rom_info->rom_module_id[9]);

	return 0;
}

int is_sec_read_eeprom(struct is_vendor_rom *rom_info, char *buf, int rom_id)
{
	struct i2c_client *client;
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;
	int i;
	int cal_size;
	int ret = 0;
	u32 read_addr;

	client = vendor_priv->rom[rom_id].client;
	cal_size = rom_info->rom_size;

	rom_info->read_error = false;

	/* read eeprom data */
	read_addr = 0x0;
	if (cal_size > IS_READ_MAX_EEP_CAL_SIZE) {
		for (i = 0; i < cal_size/IS_READ_MAX_EEP_CAL_SIZE; i++) {
			info("Camera: I2C read cal data : offset = 0x%x, size = 0x%x\n", read_addr, IS_READ_MAX_EEP_CAL_SIZE);
			ret = is_i2c_read(client, &buf[read_addr], read_addr, IS_READ_MAX_EEP_CAL_SIZE);
			if (ret) {
				err("failed to is_i2c_read (%d) rom_id (%d)", ret, rom_id);
				rom_info->read_error = true;
				ret = -EINVAL;
				goto exit;
			}
			read_addr += IS_READ_MAX_EEP_CAL_SIZE;
		}

		if (read_addr < cal_size) {
			ret = is_i2c_read(client, &buf[read_addr], read_addr, cal_size - read_addr);
			if (ret) {
				err("failed to is_i2c_read (%d) rom_id (%d)", ret, rom_id);
				rom_info->read_error = true;
				ret = -EINVAL;
				goto exit;
			}
		}
	} else {
		info("Camera: I2C read cal data\n");
		ret = is_i2c_read(client, buf, read_addr, cal_size);
		if (ret) {
			err("failed to is_i2c_read (%d) rom_id (%d)", ret, rom_id);
			rom_info->read_error = true;
			ret = -EINVAL;
			goto exit;
		}
	}

exit:
	return ret;
}

int is_sec_read_otprom(struct is_vendor_rom *rom_info, char *buf, int rom_id)
{
	int ret = 0;
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;
	int cal_size = 0;
	bool camera_running;
	struct is_cis *cis = NULL;
	struct v4l2_subdev *subdev_cis = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module = NULL;
	u32 i2c_channel;
	int position;

	is_vendor_get_position_from_rom_id(rom_id, &position);
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
		return -EINVAL;
	}

	if (sensor_peri->cis.client) {
		vendor_priv->rom[rom_id].client = sensor_peri->cis.client;
		info("%s sensor client will be used for otprom", __func__);
	}

	cal_size = rom_info->rom_size;
	rom_info->read_error = false;
	info("Camera: read cal data from OTPROM (rom_id:%d, cal_size:%d)\n",
		rom_id, cal_size);

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

	ret = CALL_CISOPS(&sensor_peri->cis, cis_get_otprom_data, subdev_cis, buf, camera_running, rom_id);
	if (ret < 0) {
		rom_info->read_error = true;
		err("failed to get otprom");
		ret = -EINVAL;
	}

exit:
	return ret;
}

int is_sec_read_module_id(struct is_vendor_rom *rom_info, char *module_id, int rom_id)
{
	struct i2c_client *client;
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;
	char *temp_buf = NULL;
	int ret = 0;

	client = vendor_priv->rom[rom_id].client;

	if (rom_info->rom_type == ROM_TYPE_EEPROM)
		ret = is_i2c_read(client, module_id, rom_info->rom_header_module_id_addr, IS_MODULE_ID_SIZE);
	else if (rom_info->rom_type == ROM_TYPE_OTPROM) {
		temp_buf = vzalloc(rom_info->rom_size);
		if (!temp_buf) {
			err("Cannot alloc temp buf for OTPROM read");
			return ret;
		}
		ret = is_sec_read_otprom(rom_info, temp_buf, rom_id);
		if (ret) {
			err("failed to OTPROM read (%d)\n", ret);
			vfree(temp_buf);
			return ret;
		}
		memcpy(module_id, &temp_buf[rom_info->rom_header_module_id_addr], IS_MODULE_ID_SIZE);
		vfree(temp_buf);
	}
	else {
		err("ROM TYPE is invalid [%d]", rom_info->rom_type);
		return ret;
	}

	if (ret) {
		err("failed to is_i2c_read (%d)\n", ret);
		return ret;
	}

	info("Camera: I2C read module id data, rom_id[%d], MODULE ID : %c%c%c%c%c%02X%02X%02X%02X%02X\n", rom_id,
		module_id[0], module_id[1], module_id[2], module_id[3], module_id[4], module_id[5],
		module_id[6], module_id[7], module_id[8], module_id[9]);

	return ret;
}

int is_sec_read_header_verion(struct is_vendor_rom *rom_info, char *header_ver, int rom_id)
{
	struct i2c_client *client;
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;
	int ret = 0;
	char *temp_buf;

	client = vendor_priv->rom[rom_id].client;

	if (rom_info->rom_type == ROM_TYPE_EEPROM)
		ret = is_i2c_read(client, header_ver, rom_info->rom_header_version_start_addr, IS_HEADER_VER_SIZE);
	else if (rom_info->rom_type == ROM_TYPE_OTPROM) {
		temp_buf = vzalloc(rom_info->rom_size);
		if (!temp_buf) {
			err("%s Cannot alloc temp buf for OTPROM read", __func__);
			return ret;
		}
		ret = is_sec_read_otprom(rom_info, temp_buf, rom_id);
		if (ret) {
			err("%s failed to OTPROM read (%d)\n", __func__, ret);
			vfree(temp_buf);
			return ret;
		}
		memcpy(header_ver, &temp_buf[rom_info->rom_header_version_start_addr], IS_HEADER_VER_SIZE);
		vfree(temp_buf);
	}
	else {
		err("%s ROM TYPE is invalid [%d]", __func__, rom_info->rom_type);
		return ret;
	}

	if (ret) {
		err("failed to is_i2c_read (%d)\n", ret);
		return ret;
	}
	info("Camera: I2C read header version, rom_id[%d], header_ver : %c%c%c%c%c\n", rom_id,
		header_ver[0], header_ver[1], header_ver[2], header_ver[3], header_ver[4]);

	return ret;
}

int is_sec_probe_dualized_rom(struct is_vendor_rom *rom_info, char *buf, int rom_id)
{
	struct i2c_client *client;
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;

	client = vendor_priv->rom[rom_id].client;
	memcpy(rom_info->sensor_name, &buf[rom_info->sensor_name_addr], IS_SENSOR_NAME_SIZE);

	rom_info->sensor_name[IS_SENSOR_NAME_SIZE] = '\0';

	return is_vendor_dualized_rom_parse_dt(client->dev.of_node, rom_id);
}

int is_sec_read_rom(int rom_id)
{
	int ret = 0;
	char *buf = NULL;
	int retry;
	struct is_core *core = is_get_is_core();
	struct is_vendor_rom *rom_info = NULL;
	int rom_type;
#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
	struct cam_hw_param *hw_param;
	int position;
#endif

	is_sec_get_rom_info(&rom_info, rom_id);

	rom_type = rom_info->rom_type;
	if (rom_type != ROM_TYPE_EEPROM && rom_type != ROM_TYPE_OTPROM) {
		err("unknown rom_type(%d)", rom_type);
		ret = -EINVAL;
		return ret;
	}

	buf = rom_info->buf;

	info("Read Camera ROM, rom_id[%d]\n", rom_id);

	for (retry = 0; retry < IS_CAL_RETRY_CNT; retry++) {
		if (rom_type == ROM_TYPE_EEPROM)
			ret = is_sec_read_eeprom(rom_info, buf, rom_id);
		else if (rom_type == ROM_TYPE_OTPROM)
			ret = is_sec_read_otprom(rom_info, buf, rom_id);

		if (is_sec_check_cal_crc32(buf, rom_id))
			break;
	}

#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
	if (is_vendor_get_position_from_rom_id(rom_id, &position) == 0) {
		is_sec_get_hw_param(&hw_param, position);
		if (hw_param) {
			if (rom_info->read_error) {
				hw_param->eeprom_i2c_err_cnt++;
				is_sec_update_hw_param_mipi_info_on_err(hw_param);
			} else if (rom_info->crc_error) {
				hw_param->eeprom_crc_err_cnt++;
				is_sec_update_hw_param_mipi_info_on_err(hw_param);
			}
		}
	} else {
		info("Cannot find position from rom_id(%d)\n", rom_id);
	}
#endif

	rom_info->crc_retry_cnt = retry;

	if (rom_info->check_dualize)
		is_sec_probe_dualized_rom(rom_info, buf, rom_id);

	is_sec_parse_rom_info(rom_info, buf, rom_id);

	if (!is_sec_check_rom_ver(core, rom_id)) {
		info("Camera: Do not read cal data. ROM version is low.\n");
		ret = 0;
		goto exit;
	}

	is_sec_check_module_state(rom_info);

#ifdef USE_CAMERA_NOTIFY_WACOM
	if (!rom_info->crc_error)
		is_eeprom_info_update(rom_id, rom_info->header_ver);
#endif

	rom_info->read_done = true;

exit:
	return ret;
}

/* TODO : move to dt */
struct dualize_match_entry dualize_table[] = {
	{ "M50EL", SENSOR_NAME_S5KGN5, "S5KGN5" },
};

void is_sec_select_dualized_sensor(int rom_id)
{
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;
	struct is_vendor_rom *rom_info = NULL;
	struct is_module_enum *module;
	int pos, i, ret;
	int table_size, module_header_len;
	char header_ver[IS_HEADER_VER_SIZE];

	is_sec_get_rom_info(&rom_info, rom_id);

	if (!rom_info->check_dualize)
		return;

	if (is_sec_read_header_verion(rom_info, header_ver, rom_id)) {
		err("[rom_id:%d] fail to get header version", rom_id);
		return;
	}

	for (pos = SENSOR_POSITION_REAR; pos < SENSOR_POSITION_MAX; pos++) {
		ret = is_vendor_get_module_from_position(pos, &module);
		if (ret || !module || module->pdata->rom_id != rom_id)
			continue;

		table_size = ARRAY_SIZE(dualize_table);
		for (i = 0; i < table_size; i++) {
			module_header_len = strlen(dualize_table[i].module_header);
			if (strncmp(header_ver, dualize_table[i].module_header, module_header_len) == 0) {
				info("%s: found! %s => %d\n",
					__func__, dualize_table[i].module_header, dualize_table[i].sensor_id);
				vendor_priv->sensor_id[pos] = dualize_table[i].sensor_id;
				vendor_priv->sensor_name[pos] = dualize_table[i].sensor_name;
				break;
			}
		}

		core->sensor[module->device].sensor_name = (char *)vendor_priv->sensor_name[pos];
		info("%s: select sensor id: %d, name(%s)\n",
			__func__, vendor_priv->sensor_id[pos], vendor_priv->sensor_name[pos]);
	}
}

int is_sec_run_fw_sel(int rom_id)
{
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;
	struct is_vendor_rom *rom_info = NULL;
	int ret = 0;
	bool camera_running;
	bool rom_power_on = false;

	is_sec_get_rom_info(&rom_info, rom_id);

	if (rom_info->skip_cal_loading) {
		err("[rom_id:%d] skip_cal_loading implemented", rom_id);
		return false;
	}

	if (!rom_info->read_done) {
		mutex_lock(&vendor_priv->rom_lock);

		if (rom_info->need_i2c_config) {
			camera_running = is_vendor_check_camera_running(rom_info->rom_power_position);
			info("need_i2c_config, camera_running(%d)\n", camera_running);
			if (camera_running) {
				is_sec_rom_power_on(core, rom_info->rom_power_position);
				rom_power_on = true;
			}
		}

		is_sec_select_dualized_sensor(rom_id);

		ret = is_sec_read_rom(rom_id);
		if (ret)
			err("is_sec_read_rom for ROM_ID(%d) is fail(%d)", rom_id, ret);

		if (rom_info->need_i2c_config && rom_power_on)
			is_sec_rom_power_off(core, rom_info->rom_power_position);

		mutex_unlock(&vendor_priv->rom_lock);
	}

	if (vendor_priv->check_sensor_vendor) {
		if (is_sec_check_rom_ver(core, rom_id)) {
			is_sec_check_module_state(rom_info);

			if (rom_info->other_vendor_module) {
				err("Not supported module. Rom ID = %d, Module ver = %s", rom_id, rom_info->header_ver);
				return -EIO;
			}
		}
	}

	return ret;
}

void is_sec_retention_inactive(struct is_vendor *vendor)
{
	int i;
	struct is_vendor_private *vendor_priv;
	struct sensor_open_extended *ext_info;
	struct is_module_enum *module;

	vendor_priv = vendor->private_data;

	for (i = 0; i < SENSOR_POSITION_MAX; i++) {
		if (is_vendor_get_module_from_position(i, &module))
			continue;

		ext_info = &(((struct is_module_enum *)module)->ext);

		if (ext_info->use_retention_mode == SENSOR_RETENTION_ACTIVATED) {
			ext_info->use_retention_mode = SENSOR_RETENTION_INACTIVE;
			info("Sensor[%d] reload. retention inactive!\n", vendor_priv->sensor_id[i]);
		}
	}
}

int is_sec_cal_reload(void)
{
	int ret = 0;
	int curr_rom_id;
	char module_id[IS_MODULE_ID_SIZE];
	bool need_retention_inactive = false;
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = NULL;
	struct is_vendor_rom *rom_info;

	vendor_priv = core->vendor.private_data;
	for (curr_rom_id = 0; curr_rom_id < ROM_ID_MAX; curr_rom_id++) {
		if (!vendor_priv->rom[curr_rom_id].valid)
			continue;

		is_sec_get_rom_info(&rom_info, curr_rom_id);

		is_sec_rom_power_on(core, rom_info->rom_power_position);

		is_sec_read_module_id(rom_info, module_id, curr_rom_id);

		if (strncmp(module_id, rom_info->rom_module_id, IS_MODULE_ID_SIZE)) {
			rom_info->read_done = false;
			rom_info->sec2lsi_conv_done = false;
			is_sec_run_fw_sel(curr_rom_id);
			need_retention_inactive = true;
		}

		is_sec_rom_power_off(core, rom_info->rom_power_position);
	}

#if IS_ENABLED(CONFIG_OIS_USE)
	vendor_priv->ois_ver_read = false;
	read_ois_version();
#endif

	if (need_retention_inactive)
		is_sec_retention_inactive(&core->vendor);

	return ret;
}
