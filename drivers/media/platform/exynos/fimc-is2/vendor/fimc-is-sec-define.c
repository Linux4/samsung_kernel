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

#include <linux/pinctrl/pinctrl-samsung.h>

#include "fimc-is-sec-define.h"

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_OIS) && defined(CONFIG_SEC_FACTORY)
#include "fimc-is-device-ois.h"
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
#include <linux/i2c.h>
#include "fimc-is-device-eeprom.h"
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
#include "fimc-is-device-sensor.h"
#endif
#endif
bool crc32_fw_check = true;
bool crc32_setfile_check = true;
bool crc32_check = true;
bool crc32_check_factory = true;
bool crc32_header_check = true;
bool crc32_header_check_front = true;
bool crc32_check_factory_front = true;
bool fw_version_crc_check = true;
bool is_latest_cam_module = false;
bool is_final_cam_module = false;
#if defined(CONFIG_SOC_EXYNOS5433)
bool is_right_prj_name = true;
#endif
#ifdef CONFIG_COMPANION_USE
bool crc32_c1_fw_check = true;
bool crc32_c1_check = true;
bool crc32_c1_check_factory = true;
bool companion_lsc_isvalid = false;
bool companion_coef_isvalid = false;
#ifdef CONFIG_COMPANION_C2_USE
bool companion_front_lsc_isvalid = false;
#endif
#endif
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_OIS
bool crc32_ois_check = true;
bool crc32_ois_fw_check = true;
bool crc32_ois_cal_check = true;
#endif

#define FIMC_IS_DEFAULT_CAL_SIZE	(20 * 1024)
#define FIMC_IS_DUMP_CAL_SIZE	(172 * 1024)
#define FIMC_IS_DUMP_EEPROM_CAL_SIZE	(16 * 1024)
#define FIMC_IS_DUMP_OTP_CAL_SIZE	(8 * 1024)
#define FIMC_IS_LATEST_FROM_VERSION_A	'A'
#define FIMC_IS_LATEST_FROM_VERSION_B	'B'
#define FIMC_IS_LATEST_FROM_VERSION_C	'C'
#define FIMC_IS_LATEST_FROM_VERSION_D	'D'
#define FIMC_IS_LATEST_FROM_VERSION_M	'M'

//static bool is_caldata_read = false;
//static bool is_c1_caldata_read = false;
bool force_caldata_dump = false;

static int cam_id = CAMERA_SINGLE_REAR;
bool is_dumped_fw_loading_needed = false;
bool is_dumped_c1_fw_loading_needed = false;
char fw_core_version;
//struct class *camera_class;
//struct device *camera_front_dev; /*sys/class/camera/front*/
//struct device *camera_rear_dev; /*sys/class/camera/rear*/
static struct fimc_is_from_info sysfs_finfo;
static struct fimc_is_from_info sysfs_pinfo;
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
bool crc32_check_front = true;
bool is_latest_cam_module_front = false;
bool is_final_cam_module_front = false;
static struct fimc_is_from_info sysfs_finfo_front;
static struct fimc_is_from_info sysfs_pinfo_front;
static char cal_buf_front[FIMC_IS_MAX_CAL_SIZE_FRONT];
#endif

#if defined(CONFIG_CAMERA_FROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
static char cal_buf[FIMC_IS_MAX_CAL_SIZE];
#ifdef CAMERA_MODULE_DUALIZE
static char fw_buf[FIMC_IS_MAX_FW_SIZE];
#endif
#endif

char loaded_fw[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
char loaded_companion_fw[30] = {0, };

bool fimc_is_sec_get_force_caldata_dump(void)
{
	return force_caldata_dump;
}

int fimc_is_sec_set_force_caldata_dump(bool fcd)
{
	force_caldata_dump = fcd;
	if (fcd)
		info("forced caldata dump enabled!!\n");
	return 0;
}

int fimc_is_sec_get_sysfs_finfo(struct fimc_is_from_info **finfo)
{
	*finfo = &sysfs_finfo;
	return 0;
}

int fimc_is_sec_get_sysfs_pinfo(struct fimc_is_from_info **pinfo)
{
	*pinfo = &sysfs_pinfo;
	return 0;
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
int fimc_is_sec_get_sysfs_finfo_front(struct fimc_is_from_info **finfo)
{
	*finfo = &sysfs_finfo_front;
	return 0;
}

int fimc_is_sec_get_sysfs_pinfo_front(struct fimc_is_from_info **pinfo)
{
	*pinfo = &sysfs_pinfo_front;
	return 0;
}

int fimc_is_sec_get_front_cal_buf(char **buf)
{
	*buf = &cal_buf_front[0];
	return 0;
}
#endif

#if defined(CONFIG_CAMERA_FROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
int fimc_is_sec_get_cal_buf(char **buf)
{
	*buf = &cal_buf[0];
	return 0;
}
#endif

int fimc_is_sec_get_loaded_fw(char **buf)
{
	*buf = &loaded_fw[0];
	return 0;
}

int fimc_is_sec_set_loaded_fw(char *buf)
{
	strncpy(loaded_fw, buf, FIMC_IS_HEADER_VER_SIZE);
	return 0;
}

int fimc_is_sec_get_loaded_c1_fw(char **buf)
{
	*buf = &loaded_companion_fw[0];
	return 0;
}

int fimc_is_sec_set_loaded_c1_fw(char *buf)
{
	strncpy(loaded_companion_fw, buf, FIMC_IS_HEADER_VER_SIZE);
	return 0;
}

int fimc_is_sec_set_camid(int id)
{
	cam_id = id;
	return 0;
}

int fimc_is_sec_get_camid(void)
{
	return cam_id;
}

int fimc_is_sec_get_camid_from_hal(char *fw_name, char *setf_name)
{
#if 0
	char buf[1];
	loff_t pos = 0;
	int pixelSize;

	read_data_from_file("/data/CameraID.txt", buf, 1, &pos);
	if (buf[0] == '0')
		cam_id = CAMERA_SINGLE_REAR;
	else if (buf[0] == '1')
		cam_id = CAMERA_SINGLE_FRONT;
	else if (buf[0] == '2')
		cam_id = CAMERA_DUAL_REAR;
	else if (buf[0] == '3')
		cam_id = CAMERA_DUAL_FRONT;

	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_3L2), "%s", FIMC_IS_FW_3L2);
		snprintf(setf_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX135)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
		snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX134)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
		snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
	} else {
		pixelSize = fimc_is_sec_get_pixel_size(sysfs_finfo.header_ver);
		if (pixelSize == 13) {
			snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
			snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
		} else if (pixelSize == 8) {
			snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
			snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
		} else {
			snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
			snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
		}
	}

	if (cam_id == CAMERA_SINGLE_FRONT ||
		cam_id == CAMERA_DUAL_FRONT) {
		snprintf(setf_name, sizeof(FIMC_IS_6B2_SETF), "%s", FIMC_IS_6B2_SETF);
	}
#else
	err("%s: waring, you're calling the disabled func!", __func__);
#endif
	return 0;
}

int fimc_is_sec_fw_revision(char *fw_ver)
{
	int revision = 0;
	revision = revision + ((int)fw_ver[FW_PUB_YEAR] - 58) * 10000;
	revision = revision + ((int)fw_ver[FW_PUB_MON] - 64) * 100;
	revision = revision + ((int)fw_ver[FW_PUB_NUM] - 48) * 10;
	revision = revision + (int)fw_ver[FW_PUB_NUM + 1] - 48;

	return revision;
}

bool fimc_is_sec_fw_module_compare(char *fw_ver1, char *fw_ver2)
{
	if (fw_ver1[FW_CORE_VER] != fw_ver2[FW_CORE_VER]
		|| fw_ver1[FW_PIXEL_SIZE] != fw_ver2[FW_PIXEL_SIZE]
		|| fw_ver1[FW_PIXEL_SIZE + 1] != fw_ver2[FW_PIXEL_SIZE + 1]
		|| fw_ver1[FW_ISP_COMPANY] != fw_ver2[FW_ISP_COMPANY]
		|| fw_ver1[FW_SENSOR_MAKER] != fw_ver2[FW_SENSOR_MAKER]) {
		return false;
	}

	return true;
}

u8 fimc_is_sec_compare_ver(int position)
{
	u32 from_ver = 0, def_ver = 0;
	u8 ret = 0;
	char ver[3] = {'V', '0', '0'};
	struct fimc_is_from_info *finfo = NULL;

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT)
		finfo = &sysfs_finfo_front;
	else
#endif
		finfo = &sysfs_finfo;

	def_ver = ver[0] << 16 | ver[1] << 8 | ver[2];
	from_ver = finfo->cal_map_ver[0] << 16 | finfo->cal_map_ver[1] << 8 | finfo->cal_map_ver[2];
	if (from_ver == def_ver) {
		return finfo->cal_map_ver[3];
	} else {
		err("FROM core version is invalid. version is %c%c%c%c",
			finfo->cal_map_ver[0], finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);
		return 0;
	}

	return ret;
}

bool fimc_is_sec_check_from_ver(struct fimc_is_core *core, int position)
{
	struct fimc_is_from_info *finfo = NULL;
	struct exynos_platform_fimc_is *core_pdata = NULL;
	char compare_version;
	u8 from_ver;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return false;
	}

	if (core_pdata->skip_cal_loading) {
		err("skip_cal_loading implemented");
		return false;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT)
		finfo = &sysfs_finfo_front;
	else
#endif
		finfo = &sysfs_finfo;

	compare_version = CAMERA_MODULE_ES_VERSION;
	from_ver = fimc_is_sec_compare_ver(position);

	if ((from_ver < CAL_MAP_ES_VERSION) ||
		(finfo->header_ver[10] < compare_version)) {
		err("invalid from version. from_ver %u, header_ver[10] %c", from_ver, finfo->header_ver[10]);
		return false;
	} else {
		return true;
	}
}

#if defined(CONFIG_CAMERA_FROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
bool fimc_is_sec_check_cal_crc32(char *buf, int id)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 check_base;
	u32 check_length;
	u32 checksum_base;
	u32 address_boundary;
	bool crc32_temp, crc32_header_temp;
	struct fimc_is_from_info *finfo = NULL;
	struct fimc_is_companion_retention *ret_data;
	struct fimc_is_core *core;
	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	ret_data = &core->companion.retention_data;
	buf32 = (u32 *)buf;

	printk(KERN_INFO "+++ %s\n", __func__);

	crc32_temp = true;
#ifdef CONFIG_COMPANION_USE
	if (id == SENSOR_POSITION_REAR)
		crc32_c1_check = true;
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		address_boundary = FIMC_IS_MAX_CAL_SIZE_FRONT;
	} else
#endif
	{
		address_boundary = FIMC_IS_MAX_CAL_SIZE;
	}

	/* Header data */
	check_base = 0;
	checksum = 0;
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		finfo = &sysfs_finfo_front;
		checksum_base = EEP_CHECKSUM_HEADER_ADDR_FRONT / 4;
		check_length = HEADER_CRC32_LEN_FRONT;
	} else
#endif
	{
		finfo = &sysfs_finfo;
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		checksum_base = EEP_CHECKSUM_HEADER_ADDR / 4;
#else
		checksum_base = FROM_CHECKSUM_HEADER_ADDR / 4;
#endif
		check_length = HEADER_CRC32_LEN;
	}

	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the header (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_temp = false;
		crc32_header_temp = false;
		goto out;
	} else {
		crc32_header_temp = true;
	}

#if defined(EEP_HEADER_OEM_START_ADDR_FRONT)
	/* OEM */
	check_base = finfo->oem_start_addr / 4;
	checksum = 0;
	check_length = (finfo->oem_end_addr - finfo->oem_start_addr + 1);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		checksum_base = EEP_CHECKSUM_OEM_ADDR_FRONT / 4;
	} else
#endif
	{
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		checksum_base = EEP_CHECKSUM_OEM_ADDR / 4;
#else
		checksum_base = FROM_CHECKSUM_OEM_ADDR / 4;
#endif
	}

	if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
		err("Camera: OEM address has error: start(0x%08X), end(0x%08X)",
			finfo->oem_start_addr, finfo->oem_end_addr);
		crc32_temp = false;
		goto out;
	}

	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the OEM (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_temp = false;
		goto out;
	}
#endif

	/* AWB */
	check_base = finfo->awb_start_addr / 4;
	checksum = 0;
	check_length = (finfo->awb_end_addr - finfo->awb_start_addr + 1) ;

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		checksum_base = EEP_CHECKSUM_AWB_ADDR_FRONT / 4;
	} else
#endif
	{
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		checksum_base = EEP_CHECKSUM_AWB_ADDR / 4;
#else
		checksum_base = FROM_CHECKSUM_AWB_ADDR / 4;
#endif
	}

	if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
		err("Camera: AWB address has error: start(0x%08X), end(0x%08X)",
			finfo->awb_start_addr, finfo->awb_end_addr);
		crc32_temp = false;
		goto out;
	}

	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the AWB (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_temp = false;
		goto out;
	}

	/* Shading */
	check_base = finfo->shading_start_addr / 4;
	checksum = 0;
	check_length = (finfo->shading_end_addr - finfo->shading_start_addr + 1) ;

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		checksum_base = EEP_CHECKSUM_AP_SHADING_ADDR_FRONT / 4;
	} else
#endif
	{
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		checksum_base = EEP_CHECKSUM_AP_SHADING_ADDR / 4;
#else
		checksum_base = FROM_CHECKSUM_SHADING_ADDR / 4;
#endif
	}

	if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
		err("Camera: Shading address has error: start(0x%08X), end(0x%08X)",
			finfo->shading_start_addr, finfo->shading_end_addr);
		crc32_temp = false;
		goto out;
	}

	checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the Shading (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
		crc32_temp = false;
		goto out;
	}

#ifdef CONFIG_COMPANION_C2_USE
	/* c2 Shading */
	if (id == SENSOR_POSITION_FRONT) {
		check_base = finfo->c2_shading_start_addr / 4;
		checksum = 0;
		check_length = (finfo->c2_shading_end_addr - finfo->c2_shading_start_addr + 1);

		checksum_base = EEP_CHECKSUM_C2_SHADING_ADDR_FRONT / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: C2 Shading address has error: start(0x%08X), end(0x%08X)",
				finfo->c2_shading_start_addr, finfo->c2_shading_end_addr);
			crc32_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the C2 Shading (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			goto out;
		}
	}
#endif

#ifdef CONFIG_COMPANION_USE
	/* pdaf cal */
	if (id == SENSOR_POSITION_REAR) {
		check_base = finfo->pdaf_cal_start_addr / 4;
		checksum = 0;
		check_length = (finfo->pdaf_cal_end_addr - finfo->pdaf_cal_start_addr + 1);
		checksum_base = FROM_CHECKSUM_PAF_CAL_ADDR / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: pdaf address has error: start(0x%08X), end(0x%08X)",
				finfo->pdaf_start_addr, finfo->pdaf_end_addr);
			crc32_temp = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the pdaf cal (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_temp = false;
			goto out;
		}

		/* concord cal */
		check_base = finfo->concord_cal_start_addr / 4;
		checksum = 0;
		check_length = (finfo->concord_cal_end_addr - finfo->concord_cal_start_addr + 1);
		checksum_base = FROM_CHECKSUM_CONCORD_CAL_ADDR / 4;

		if (check_base > address_boundary || checksum_base > address_boundary || check_length <= 0) {
			err("Camera: concord cal address has error: start(0x%08X), end(0x%08X)",
				finfo->concord_cal_start_addr, finfo->concord_cal_end_addr);
			crc32_c1_check = false;
			goto out;
		}

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the concord cal (0x%08X != 0x%08X)", checksum, buf32[checksum_base]);
			crc32_c1_check = false;
			goto out;
		}
	}
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_OIS)
	if (id == SENSOR_POSITION_REAR) {
		/* OIS Header  */
		check_base = EEP_HEADER_OIS_START_ADDR / 4;
		checksum = 0;
		check_length = OIS_HEADER_CRC32_LEN;
		checksum_base = EEP_CHECKSUM_OIS_HEADER_ADDR / 4;

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the OIS Header(0x%08X != 0x%08X)",
				checksum, buf32[checksum_base]);
			crc32_ois_check = false;
			goto out;
		}

		/* OIS Cal */
		check_base = finfo->ois_cal_start_addr / 4;
		checksum = 0;
		check_length = OIS_HEADER_CRC32_CAL_LEN;
		checksum_base = EEP_CHECKSUM_OIS_CAL_ADDR / 4;

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the OIS Cal(0x%08X != 0x%08X)",
				checksum, buf32[checksum_base]);
			crc32_ois_check = false;
			crc32_ois_cal_check = false;
			goto out;
		}

		/* OIS Shift Data */
		check_base = finfo->ois_shift_start_addr / 4;
		checksum = 0;
		check_length = OIS_HEADER_CRC32_SHIFT_LEN;
		checksum_base = EEP_CHECKSUM_OIS_SHIFT_ADDR / 4;

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the OIS Shift Data(0x%08X != 0x%08X)",
				checksum, buf32[checksum_base]);
			crc32_ois_check = false;
			crc32_ois_cal_check = false;
			goto out;
		}

		/* OIS FW Set */
		check_base = finfo->ois_fw_set_start_addr / 4;
		checksum = 0;
		check_length = OIS_HEADER_CRC32_FW_SET_LEN;
		checksum_base = EEP_CHECKSUM_OIS_FW_SET_ADDR / 4;

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the OIS FW Set(0x%08X != 0x%08X)",
				checksum, buf32[checksum_base]);
			crc32_ois_check = false;
			crc32_ois_fw_check = false;
			goto out;
		}

		/* OIS FW Factory */
		check_base = finfo->ois_fw_factory_start_addr / 4;
		checksum = 0;
		check_length = OIS_HEADER_CRC32_FW_FACTORY_LEN;
		checksum_base = EEP_CHECKSUM_OIS_FW_FACTORY_ADDR / 4;

		checksum = (u32)getCRC((u16 *)&buf32[check_base], check_length, NULL, NULL);
		if (checksum != buf32[checksum_base]) {
			err("Camera: CRC32 error at the OIS FW Factory(0x%08X != 0x%08X)",
				checksum, buf32[checksum_base]);
			crc32_ois_check = false;
			crc32_ois_fw_check = false;
			goto out;
		}
	}
#endif
out:
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		crc32_check_front = crc32_temp;
		crc32_header_check_front = crc32_header_temp;
		return crc32_check_front;
	} else
#endif
	{
		crc32_check = crc32_temp;
		crc32_header_check = crc32_header_temp;
#if defined(CONFIG_COMPANION_USE)
		return crc32_check && crc32_c1_check;
#elif defined(CONFIG_CAMERA_EEPROM_SUPPORT_OIS)
		return crc32_check && crc32_ois_check;
#else
		return crc32_check;
#endif
	}
}
#endif

#if defined(CONFIG_CAMERA_FROM_SUPPORT_REAR)
bool fimc_is_sec_check_fw_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;

	buf32 = (u32 *)buf;

	info("Camera: Start checking CRC32 FW\n");

	checksum = (u32)getCRC((u16 *)&buf32[0], sysfs_finfo.fw_size, NULL, NULL);
	checksum_base = (0x2A8FFC & 0xffffffff) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_fw_check = false;
	} else {
		crc32_fw_check = true;
	}

	info("Camera: End checking CRC32 FW\n");

	return crc32_fw_check;
}
#endif

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
bool fimc_is_sec_check_front_otp_crc32(char *buf, int id)
{
	u32 *buf32 = NULL;
	u32 checksum;
	bool crc32_temp, crc32_header_temp;
	u32 checksumFromOTP;

	buf32 = (u32 *)buf;
	checksumFromOTP = buf[41] +( buf[42] << 8) +( buf[43] << 16) + (buf[44] << 24);

	/* Header data */
	checksum = (u32)getCRC((u16 *)&buf32[0], 41, NULL, NULL);

	if(checksum != checksumFromOTP) {
		crc32_temp = crc32_header_temp = false;
		err("Camera: CRC32 error at the header data section (0x%08X != 0x%08X)",
					checksum, checksumFromOTP);
	} else {
		crc32_temp = crc32_header_temp = true;
		pr_info("Camera: End checking CRC32 (0x%08X = 0x%08X)",
					checksum, checksumFromOTP);
	}

	crc32_check_front = crc32_temp;
	crc32_header_check_front = crc32_header_temp;

	return crc32_check_front;
}
#endif

bool fimc_is_sec_check_setfile_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;

	buf32 = (u32 *)buf;

	info("Camera: Start checking CRC32 Setfile\n");

	checksum = (u32)getCRC((u16 *)&buf32[0], sysfs_finfo.setfile_size, NULL, NULL);
	checksum_base = (0x1167FC & 0xffffffff) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_setfile_check = false;
	} else {
		crc32_setfile_check = true;
	}

	info("Camera: End checking CRC32 Setfile\n");

	return crc32_setfile_check;
}

#ifdef CONFIG_COMPANION_USE
bool fimc_is_sec_check_companion_fw_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;
	u32 checksum_base;

	buf32 = (u32 *)buf;

	info("Camera: Start checking CRC32 Companion FW\n");

	checksum = (u32)getCRC((u16 *)&buf32[0], sysfs_finfo.comp_fw_size, NULL, NULL);
	checksum_base = ((0x307FC & 0xffffffff)) / 4;
	if (checksum != buf32[checksum_base]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[checksum_base]);
		crc32_c1_fw_check = false;
	} else {
		crc32_c1_fw_check = true;
	}

	info("Camera: End checking CRC32 Companion FW\n");

	return crc32_c1_fw_check;
}
#endif

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos)
{
	struct file *fp;
	mm_segment_t old_fs;
	ssize_t tx = -ENOENT;
	int fd, old_mask;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	old_mask = sys_umask(0);

	if (force_caldata_dump) {
		sys_rmdir(name);
		fd = sys_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
	} else {
		fd = sys_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0664);
	}
	if (fd < 0) {
		err("open file error: %s", name);
		sys_umask(old_mask);
		set_fs(old_fs);
		return -EINVAL;
	}

	fp = fget(fd);
	if (fp) {
		tx = vfs_write(fp, buf, count, pos);
		if (tx != count) {
			err("fail to write %s. ret %zd", name, tx);
			tx = -ENOENT;
		}

		vfs_fsync(fp, 0);
		fput(fp);
	} else {
		err("fail to get file *: %s", name);
	}

	sys_close(fd);
	sys_umask(old_mask);
	set_fs(old_fs);

	return tx;
}

ssize_t read_data_from_file(char *name, char *buf, size_t count, loff_t *pos)
{
	struct file *fp;
	mm_segment_t old_fs;
	ssize_t tx;
	int fd;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = sys_open(name, O_RDONLY, 0664);
	if (fd < 0) {
		if (-ENOENT == fd)
			info("%s: file(%s) not exist!\n", __func__, name);
		else
			info("%s: error %d, failed to open %s\n", __func__, fd, name);

		set_fs(old_fs);
		return -EINVAL;
	}
	fp = fget(fd);
	if (fp) {
		tx = vfs_read(fp, buf, count, pos);
		fput(fp);
	}
	sys_close(fd);
	set_fs(old_fs);

	return count;
}

bool fimc_is_sec_file_exist(char *name)
{
	mm_segment_t old_fs;
	bool exist = true;
	int ret;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	ret = sys_access(name, 0);
	if (ret) {
		exist = false;
		if (-ENOENT == ret)
			info("%s: file(%s) not exist!\n", __func__, name);
		else
			info("%s: error %d, failed to access %s\n", __func__, ret, name);
	}

	set_fs(old_fs);
	return exist;
}

void fimc_is_sec_make_crc32_table(u32 *table, u32 id)
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

#if 0 /* unused */
static void fimc_is_read_sensor_version(void)
{
	int ret;
	char buf[0x50];

	memset(buf, 0x0, 0x50);

	printk(KERN_INFO "+++ %s\n", __func__);

	ret = fimc_is_spi_read(buf, 0x0, 0x50);

	printk(KERN_INFO "--- %s\n", __func__);

	if (ret) {
		err("fimc_is_spi_read - can't read sensor version\n");
	}

	err("Manufacturer ID(0x40): 0x%02x\n", buf[0x40]);
	err("Pixel Number(0x41): 0x%02x\n", buf[0x41]);
}

static void fimc_is_read_sensor_version2(void)
{
	char *buf;
	char *cal_data;
	u32 cur;
	u32 count = SETFILE_SIZE/READ_SIZE;
	u32 extra = SETFILE_SIZE%READ_SIZE;

	printk(KERN_ERR "%s\n", __func__);

	buf = (char *)kmalloc(READ_SIZE, GFP_KERNEL);
	cal_data = (char *)kmalloc(SETFILE_SIZE, GFP_KERNEL);

	memset(buf, 0x0, READ_SIZE);
	memset(cal_data, 0x0, SETFILE_SIZE);

	for (cur = 0; cur < SETFILE_SIZE; cur += READ_SIZE) {
		fimc_is_spi_read(buf, cur, READ_SIZE);
		memcpy(cal_data+cur, buf, READ_SIZE);
		memset(buf, 0x0, READ_SIZE);
	}

	if (extra != 0) {
		fimc_is_spi_read(buf, cur, extra);
		memcpy(cal_data+cur, buf, extra);
		memset(buf, 0x0, extra);
	}

	info("Manufacturer ID(0x40): 0x%02x\n", cal_data[0x40]);
	info("Pixel Number(0x41): 0x%02x\n", cal_data[0x41]);

	info("Manufacturer ID(0x4FE7): 0x%02x\n", cal_data[0x4FE7]);
	info("Pixel Number(0x4FE8): 0x%02x\n", cal_data[0x4FE8]);
	info("Manufacturer ID(0x4FE9): 0x%02x\n", cal_data[0x4FE9]);
	info("Pixel Number(0x4FEA): 0x%02x\n", cal_data[0x4FEA]);

	kfree(buf);
	kfree(cal_data);
}

static int fimc_is_get_cal_data(void)
{
	int err = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long ret = 0;
	u8 mem0 = 0, mem1 = 0;
	u32 CRC = 0;
	u32 DataCRC = 0;
	u32 IntOriginalCRC = 0;
	u32 crc_index = 0;
	int retryCnt = 2;
	u32 header_crc32 =	0x1000;
	u32 oem_crc32 =		0x2000;
	u32 awb_crc32 =		0x3000;
	u32 shading_crc32 = 0x6000;
	u32 shading_header = 0x22C0;

	char *cal_data;

	crc32_check = true;
	printk(KERN_INFO "%s\n", __func__);
	printk(KERN_INFO "+++ %s\n", __func__);

	fimc_is_spi_read(cal_map_version, 0x60, 0x4);
	printk(KERN_INFO "cal_map_version = %.4s\n", cal_map_version);

	if (cal_map_version[3] == '5') {
		shading_crc32 = 0x6000;
		shading_header = 0x22C0;
	} else if (cal_map_version[3] == '6') {
		shading_crc32 = 0x4000;
		shading_header = 0x920;
	} else {
		shading_crc32 = 0x5000;
		shading_header = 0x22C0;
	}

	/* Make CRC Table */
	fimc_is_sec_make_crc32_table((u32 *)&crc_table, 0xEDB88320);


	retry:
		cal_data = (char *)kmalloc(SETFILE_SIZE, GFP_KERNEL);

		memset(cal_data, 0x0, SETFILE_SIZE);

		mem0 = 0, mem1 = 0;
		CRC = 0;
		DataCRC = 0;
		IntOriginalCRC = 0;
		crc_index = 0;

		fimc_is_spi_read(cal_data, 0, SETFILE_SIZE);

		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0x80)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made HEADER CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[header_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[header_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[header_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[header_crc32-1]&0x00ff);
		printk(KERN_INFO "Original HEADER CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;

		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0xC0)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x1000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x1000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made OEM CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[oem_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[oem_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[oem_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[oem_crc32-1]&0x00ff);
		printk(KERN_INFO "Original OEM CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;


		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (0x20)/2; crc_index++) {
			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x2000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x2000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made AWB CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[awb_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[awb_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[awb_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[awb_crc32-1]&0x00ff);
		printk(KERN_INFO "Original AWB CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;


		CRC = 0;
		CRC = ~CRC;
		for (crc_index = 0; crc_index < (shading_header)/2; crc_index++) {

			/*low byte*/
			mem0 = (unsigned char)(cal_data[0x3000 + crc_index*2] & 0x00ff);
			/*high byte*/
			mem1 = (unsigned char)((cal_data[0x3000 + crc_index*2+1]) & 0x00ff);
			CRC = crc_table[(CRC ^ (mem0)) & 0xFF] ^ (CRC >> 8);
			CRC = crc_table[(CRC ^ (mem1)) & 0xFF] ^ (CRC >> 8);
		}
		CRC = ~CRC;


		DataCRC = (CRC&0x000000ff)<<24;
		DataCRC += (CRC&0x0000ff00)<<8;
		DataCRC += (CRC&0x00ff0000)>>8;
		DataCRC += (CRC&0xff000000)>>24;
		printk(KERN_INFO "made SHADING CSC value by S/W = 0x%x\n", DataCRC);

		IntOriginalCRC = (cal_data[shading_crc32-4]&0x00ff)<<24;
		IntOriginalCRC += (cal_data[shading_crc32-3]&0x00ff)<<16;
		IntOriginalCRC += (cal_data[shading_crc32-2]&0x00ff)<<8;
		IntOriginalCRC += (cal_data[shading_crc32-1]&0x00ff);
		printk(KERN_INFO "Original SHADING CRC Int = 0x%x\n", IntOriginalCRC);

		if (IntOriginalCRC != DataCRC)
			crc32_check = false;


		old_fs = get_fs();
		set_fs(KERNEL_DS);

		if (crc32_check == true) {
			printk(KERN_INFO "make cal_data.bin~~~~ \n");
			fp = filp_open(FIMC_IS_CAL_SDCARD, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (IS_ERR(fp) || fp == NULL) {
				printk(KERN_INFO "failed to open %s, err %ld\n",
					FIMC_IS_CAL_SDCARD, PTR_ERR(fp));
				err = -EINVAL;
				goto out;
			}

			ret = vfs_write(fp, (char __user *)cal_data,
				SETFILE_SIZE, &fp->f_pos);

		} else {
			if (retryCnt > 0) {
				set_fs(old_fs);
				retryCnt--;
				goto retry;
			}
		}

/*
		{
			fp = filp_open(FIMC_IS_CAL_SDCARD, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (IS_ERR(fp) || fp == NULL) {
				printk(KERN_INFO "failed to open %s, err %ld\n",
					FIMC_IS_CAL_SDCARD, PTR_ERR(fp));
				err = -EINVAL;
				goto out;
			}

			ret = vfs_write(fp, (char __user *)cal_data,
				SETFILE_SIZE, &fp->f_pos);

		}
*/

		if (fp != NULL)
			filp_close(fp, current->files);

	out:
		set_fs(old_fs);
		kfree(cal_data);
		return err;

}

#endif

int fimc_is_sec_rom_power_on(struct fimc_is_core *core, int position)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_module_enum *module = NULL;
	int sensor_id = 0;
	int i = 0;
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	struct fimc_is_device_sensor *device_sensor;
#endif

	info("%s: Sensor position = %d.", __func__, position);

	if(position == SENSOR_POSITION_REAR)
		sensor_id = core->pdata->rear_sensor_id;
	else
		sensor_id = core->pdata->front_sensor_id;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module(&core->sensor[i], sensor_id, &module);
		if (module) {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
			device_sensor = &core->sensor[i];
#endif
			break;
		}
	}

	if (!module) {
		err("%s: Could not find sensor id[%d].", __func__, sensor_id);
		ret = -EINVAL;
		goto p_err;
	}

	module_pdata = module->pdata;

	if (!module_pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	ret = fimc_is_sensor_mclk_on(device_sensor, GPIO_SCENARIO_ON, module_pdata->mclk_ch);
	if (ret) {
		err("mclk_on is fail(%d)", ret);
		goto p_err;
	}
#endif

	ret = module_pdata->gpio_cfg(module->pdev, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_ON);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_sec_rom_power_off(struct fimc_is_core *core, int position)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *module_pdata;
	struct fimc_is_module_enum *module = NULL;
	int sensor_id = 0;
	int i = 0;
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	struct fimc_is_device_sensor *device_sensor;
#endif

	info("%s: Sensor position = %d.", __func__, position);

	if(position == SENSOR_POSITION_REAR)
		sensor_id = core->pdata->rear_sensor_id;
	else
		sensor_id = core->pdata->front_sensor_id;

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		fimc_is_search_sensor_module(&core->sensor[i], sensor_id, &module);
		if (module) {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
			device_sensor = &core->sensor[i];
#endif
			break;
		};
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

	ret = module_pdata->gpio_cfg(module->pdev, SENSOR_SCENARIO_READ_ROM, GPIO_SCENARIO_OFF);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	ret = fimc_is_sensor_mclk_off(device_sensor, GPIO_SCENARIO_OFF, module_pdata->mclk_ch);
	if (ret) {
		err("mclk_on is fail(%d)", ret);
		goto p_err;
	}
#endif

p_err:
	return ret;
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
int fimc_is_i2c_read(struct i2c_client *client, void *buf, u32 addr, size_t size)
{
	const u32 addr_size = 2, max_retry = 5;
	u8 addr_buf[addr_size];
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
		ret = i2c_master_send(client, addr_buf, addr_size);
		if (likely(addr_size == ret))
			break;

		info("%s: i2c_master_send failed(%d), try %d\n", __func__, ret, retries);
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

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
int fimc_is_i2c_write(struct i2c_client *client, u16 addr, u8 data)
{
	const u32 write_buf_size = 3, max_retry = 5;
	u8 write_buf[write_buf_size];
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
		ret = i2c_master_send(client, write_buf, write_buf_size);
		if (likely(write_buf_size == ret))
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
#else
int fimc_is_i2c_write(struct i2c_client *client, void *buf, u32 addr, size_t size)
{
	info("%s: do nothing\n", __func__);
	return 0;
}
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
static int fimc_is_i2c_config(struct i2c_client *client, bool onoff)
{
	struct device *i2c_dev = client->dev.parent->parent;
	struct pinctrl *pinctrl_i2c = NULL;
	struct fimc_is_device_eeprom *eeprom_device;
	struct fimc_is_eeprom_gpio *gpio;

	if (!client) {
		pr_info("%s: client is null\n", __func__);
		return -ENODEV;
	}

	eeprom_device = i2c_get_clientdata(client);
	gpio = &eeprom_device->gpio;

	info("(%s):onoff(%d) use_i2c_pinctrl(%d)\n", __func__, onoff, gpio->use_i2c_pinctrl);
	if (onoff) {
		if(gpio->use_i2c_pinctrl) {
			pin_config_set(gpio->pinname, gpio->sda,
				PINCFG_PACK(PINCFG_TYPE_FUNC, gpio->pinfunc_on));
			pin_config_set(gpio->pinname, gpio->scl,
				PINCFG_PACK(PINCFG_TYPE_FUNC, gpio->pinfunc_on));
		}
		/* ON */
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "on_i2c");
		if (IS_ERR_OR_NULL(pinctrl_i2c)) {
			printk(KERN_ERR "%s: don't use on_i2c\n", __func__);
		} else {
			devm_pinctrl_put(pinctrl_i2c);
		}
	} else {
		/* OFF */
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "off_i2c");
		if (IS_ERR_OR_NULL(pinctrl_i2c)) {
			printk(KERN_ERR "%s: don't use off_i2c\n", __func__);
		} else {
			devm_pinctrl_put(pinctrl_i2c);
		}
		if(gpio->use_i2c_pinctrl) {
			pin_config_set(gpio->pinname, gpio->sda,
				PINCFG_PACK(PINCFG_TYPE_FUNC, gpio->pinfunc_off));
			pin_config_set(gpio->pinname, gpio->scl,
				PINCFG_PACK(PINCFG_TYPE_FUNC, gpio->pinfunc_off));
		}
	}

	return 0;
}
#endif

int fimc_is_sec_read_eeprom_header(struct device *dev)
{
	int ret = 0;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	struct exynos_platform_fimc_is *core_pdata = NULL;
	u8 header_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
	struct i2c_client *client;
	client = core->eeprom_client0;
	core_pdata = dev_get_platdata(fimc_is_dev);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_i2c_config(client, true);
	}
#endif

	ret = fimc_is_i2c_read(client, header_version,
			       EEP_HEADER_VERSION_START_ADDR_FRONT, FIMC_IS_HEADER_VER_SIZE);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_i2c_config(client, false);
	}
#endif

	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_read for header version (%d)\n", ret);
		ret = -EINVAL;
	}

	memcpy(sysfs_finfo.header_ver, header_version, FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';

	return ret;
}

int fimc_is_sec_readcal_eeprom(struct device *dev, int position)
{
	int ret = 0;
	char *buf = NULL;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	struct exynos_platform_fimc_is *core_pdata = NULL;
	struct fimc_is_from_info *finfo = NULL;
	int cal_size = 0;
	struct i2c_client *client = NULL;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;

	core_pdata = dev_get_platdata(fimc_is_dev);
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT) {
		finfo = &sysfs_finfo_front;
		fimc_is_sec_get_front_cal_buf(&buf);
		cal_size = FIMC_IS_MAX_CAL_SIZE_FRONT;
		client = core->eeprom_client1;
	} else
#endif
	{
		finfo = &sysfs_finfo;
		fimc_is_sec_get_cal_buf(&buf);
		cal_size = FIMC_IS_MAX_CAL_SIZE;
		client = core->eeprom_client0;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_i2c_config(client, true);
	}
#endif

	if (position == SENSOR_POSITION_FRONT) {
		ret = fimc_is_i2c_read(client, finfo->cal_map_ver,
						EEP_HEADER_CAL_MAP_VER_START_ADDR_FRONT,
						FIMC_IS_CAL_MAP_VER_SIZE);
	}
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
	else {
		ret = fimc_is_i2c_read(client, finfo->cal_map_ver,
						EEP_HEADER_CAL_MAP_VER_START_ADDR,
						FIMC_IS_CAL_MAP_VER_SIZE);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_OIS)
		ret = fimc_is_i2c_read(client, finfo->ois_cal_ver,
						EEP_HEADER_OIS_CAL_VER_START_ADDR,
						FIMC_IS_OIS_CAL_VER_SIZE);
#endif
	}
#endif
	if (position == SENSOR_POSITION_FRONT) {
		ret = fimc_is_i2c_read(client, finfo->header_ver,
						EEP_HEADER_VERSION_START_ADDR_FRONT,
						FIMC_IS_HEADER_VER_SIZE);
	}
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
	else {
		ret = fimc_is_i2c_read(client, finfo->header_ver,
						EEP_HEADER_VERSION_START_ADDR,
						FIMC_IS_HEADER_VER_SIZE);
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_OIS)
		ret = fimc_is_i2c_read(client, finfo->ois_fw_ver,
						EEP_HEADER_OIS_FW_VER_START_ADDR,
						FIMC_IS_OIS_FW_VER_SIZE);
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_OIS) && defined(CONFIG_SEC_FACTORY)
		fimc_is_ois_factory_read_IC_ROM_checksum(core);
#endif
	}
#endif

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_i2c_config(client, false);
	}
#endif

	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	printk(KERN_INFO "Camera: EEPROM Cal map_version = %c%c%c%c\n", finfo->cal_map_ver[0],
			finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);

	if (!fimc_is_sec_check_from_ver(core, position)) {
		info("Camera: Do not read eeprom cal data. EEPROM version is low.\n");
		return 0;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_OIS)
	if (position == SENSOR_POSITION_REAR) {
		printk(KERN_INFO "Camera: EEPROM OIS Cal version = %c%c%c%c\n", finfo->ois_cal_ver[0],
				finfo->ois_cal_ver[1], finfo->ois_cal_ver[2], finfo->ois_cal_ver[3]);
		printk(KERN_INFO "Camera: EEPROM OIS FW version =  %s\n", finfo->ois_fw_ver);
	}
#endif
crc_retry:

	/* read cal data */
	info("Camera: I2C read cal data\n");
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_i2c_config(client, true);
	}
#endif

	fimc_is_i2c_read(client, buf, 0x0, cal_size);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (core_pdata->use_ois_hsi2c) {
		fimc_is_i2c_config(client, false);
	}
#endif

	if (position == SENSOR_POSITION_FRONT) {
		info("FRONT EEPROM header version = %s\n", finfo->header_ver);
#if defined(EEP_HEADER_OEM_START_ADDR_FRONT)
		finfo->oem_start_addr = *((u32 *)&buf[EEP_HEADER_OEM_START_ADDR_FRONT]);
		finfo->oem_end_addr = *((u32 *)&buf[EEP_HEADER_OEM_END_ADDR_FRONT]);
		info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
#endif
		finfo->awb_start_addr = *((u32 *)&buf[EEP_HEADER_AWB_START_ADDR_FRONT]);
		finfo->awb_end_addr = *((u32 *)&buf[EEP_HEADER_AWB_END_ADDR_FRONT]);
		info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
		finfo->shading_start_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_START_ADDR_FRONT]);
		finfo->shading_end_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_END_ADDR_FRONT]);
		info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));
		if (finfo->shading_end_addr > 0x3AFF) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->shading_end_addr = 0x3AFF;
		}
#ifdef CONFIG_COMPANION_C2_USE
		finfo->c2_shading_start_addr = *((u32 *)&buf[EEP_HEADER_C2_SHADING_START_ADDR_FRONT]);
		finfo->c2_shading_end_addr = *((u32 *)&buf[EEP_HEADER_C2_SHADING_END_ADDR_FRONT]);
		info("c2_shading start = 0x%08x, end = 0x%08x\n",
				(finfo->c2_shading_start_addr), (finfo->c2_shading_end_addr));
		if (finfo->c2_shading_end_addr > 0x1CFF) {
			err("C2 Shading end_addr has error!! 0x%08x", finfo->c2_shading_end_addr);
			finfo->c2_shading_end_addr = 0x1CFF;
		}

		/* C2 SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->c2_shading_ver, &buf[EEP_C2_SHADING_VER_START_ADDR_FRONT], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';

		finfo->lsc_i0_gain_addr = EEP_C2_SHADING_LSC_I0_GAIN_ADDR_FRONT;
		info("Shading lsc_i0 start = 0x%08x\n", finfo->lsc_i0_gain_addr);
		finfo->lsc_j0_gain_addr = EEP_C2_SHADING_LSC_J0_GAIN_ADDR_FRONT;
		info("Shading lsc_j0 start = 0x%08x\n", finfo->lsc_j0_gain_addr);
		finfo->lsc_a_gain_addr = EEP_C2_SHADING_LSC_A_GAIN_ADDR_FRONT;
		info("Shading lsc_a start = 0x%08x\n", finfo->lsc_a_gain_addr);
		finfo->lsc_k4_gain_addr = EEP_C2_SHADING_LSC_K4_GAIN_ADDR_FRONT;
		info("Shading lsc_k4 start = 0x%08x\n", finfo->lsc_k4_gain_addr);
		finfo->lsc_scale_gain_addr = EEP_C2_SHADING_LSC_SCALE_GAIN_ADDR_FRONT;
		info("Shading lsc_scale start = 0x%08x\n", finfo->lsc_scale_gain_addr);
		finfo->grasTuning_AwbAshCord_N_addr = EEP_C2_SHADING_GRASTUNING_AWB_ASH_CORD_ADDR_FRONT;
		info("Shading grasTuning_AwbAshCord_N start = 0x%08x\n", finfo->grasTuning_AwbAshCord_N_addr);
		finfo->grasTuning_awbAshCordIndexes_N_addr = EEP_C2_SHADING_GRASTUNING_AWB_ASH_CORD_INDEX_ADDR_FRONT;
		info("Shading grasTuning_awbAshCordIndexes_N start = 0x%08x\n",
				finfo->grasTuning_awbAshCordIndexes_N_addr);
		finfo->grasTuning_GASAlpha_M__N_addr = EEP_C2_SHADING_GRASTUNING_GAS_ALPHA_ADDR_FRONT;
		info("Shading grasTuning_GASAlpha_M__N_addr start = 0x%08x\n", finfo->grasTuning_GASAlpha_M__N_addr);
		finfo->grasTuning_GASBeta_M__N_addr = EEP_C2_SHADING_GRASTUNING_GAS_BETA_ADDR_FRONT;
		info("Shading grasTuning_GASBeta_M__N start = 0x%08x\n", finfo->grasTuning_GASBeta_M__N_addr);
		finfo->grasTuning_GASOutdoorAlpha_N_addr = EEP_C2_SHADING_GRASTUNING_GAS_OUTDOOR_ALPHA_ADDR_FRONT;
		info("Shading grasTuning_GASOutdoorAlpha_N start = 0x%08x\n",
				finfo->grasTuning_GASOutdoorAlpha_N_addr);
		finfo->grasTuning_GASOutdoorBeta_N_addr = EEP_C2_SHADING_GRASTUNING_GAS_OUTDOOR_BETA_ADDR_FRONT;
		info("Shading grasTuning_GASOutdoorBeta_N start = 0x%08x\n", finfo->grasTuning_GASOutdoorBeta_N_addr);
		finfo->grasTuning_GASIndoorAlpha_N_addr = EEP_C2_SHADING_GRASTUNING_GAS_INDOOR_ALPHA_ADDR_FRONT;
		info("Shading grasTuning_GASIndoorAlpha_N start = 0x%08x\n", finfo->grasTuning_GASIndoorAlpha_N_addr);
		finfo->grasTuning_GASIndoorBeta_N_addr = EEP_C2_SHADING_GRASTUNING_GAS_INDOOR_BETA_ADDR_FRONT;
		info("Shading grasTuning_GASIndoorBeta_N start = 0x%08x\n", finfo->grasTuning_GASIndoorBeta_N_addr);

		finfo->lsc_gain_start_addr = EEP_C2_SHADING_LSC_GAIN_START_ADDR_FRONT;
		finfo->lsc_gain_end_addr = EEP_C2_SHADING_LSC_GAIN_END_ADDR_FRONT;
		info("LSC start = 0x%04x, end = 0x%04x\n", finfo->lsc_gain_start_addr, finfo->lsc_gain_end_addr);
		finfo->lsc_gain_crc_addr = EEP_C2_SHADING_LSC_GAIN_CRC_ADDR_FRONT;
		info("lsc_gain_crc_addr = 0x%04x,\n", finfo->lsc_gain_crc_addr);
		finfo->lsc_parameter_crc_addr = EEP_C2_SHADING_LSC_PARAMETER_CRC_ARRD_FRONT;
		info("lsc_parameter_crc_addr = 0x%04x,\n", finfo->lsc_parameter_crc_addr);
		info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
#endif
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[EEP_HEADER_VERSION_START_ADDR_FRONT], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver,
		       &buf[EEP_HEADER_CAL_MAP_VER_START_ADDR_FRONT], FIMC_IS_CAL_MAP_VER_SIZE);

		memcpy(finfo->project_name,
		       &buf[EEP_HEADER_PROJECT_NAME_START_ADDR_FRONT], FIMC_IS_PROJECT_NAME_SIZE);
		finfo->project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';
#if defined(EEP_HEADER_OEM_START_ADDR_FRONT)
		/* OEM Data : Module/Manufacturer Information */
		memcpy(finfo->oem_ver, &buf[EEP_OEM_VER_START_ADDR_FRONT], FIMC_IS_OEM_VER_SIZE);
		finfo->oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
#endif
		/* AWB Data : Module/Manufacturer Information */
		memcpy(finfo->awb_ver, &buf[EEP_AWB_VER_START_ADDR_FRONT], FIMC_IS_AWB_VER_SIZE);
		finfo->awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';

		/* SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->shading_ver, &buf[EEP_AP_SHADING_VER_START_ADDR_FRONT], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
#endif
	}
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
    else {
		finfo->oem_start_addr = *((u32 *)&buf[EEP_HEADER_OEM_START_ADDR]);
		finfo->oem_end_addr = *((u32 *)&buf[EEP_HEADER_OEM_END_ADDR]);
		info("OEM start = 0x%08x, end = 0x%08x\n",
			(finfo->oem_start_addr), (finfo->oem_end_addr));
		finfo->awb_start_addr = *((u32 *)&buf[EEP_HEADER_AWB_START_ADDR]);
		finfo->awb_end_addr = *((u32 *)&buf[EEP_HEADER_AWB_END_ADDR]);
		info("AWB start = 0x%08x, end = 0x%08x\n",
			(finfo->awb_start_addr), (finfo->awb_end_addr));
		finfo->shading_start_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_START_ADDR]);
		finfo->shading_end_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_END_ADDR]);
		if (finfo->shading_end_addr > 0x1fff) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->setfile_end_addr = 0x1fff;
		}
		info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));

		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[EEP_HEADER_VERSION_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver, &buf[EEP_HEADER_CAL_MAP_VER_START_ADDR], FIMC_IS_CAL_MAP_VER_SIZE);

		memcpy(finfo->project_name, &buf[EEP_HEADER_PROJECT_NAME_START_ADDR], FIMC_IS_PROJECT_NAME_SIZE);
		finfo->project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';

		/* OEM Data : Module/Manufacturer Information */
		memcpy(finfo->oem_ver, &buf[EEP_OEM_VER_START_ADDR], FIMC_IS_OEM_VER_SIZE);
		finfo->oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';

		/* AWB Data : Module/Manufacturer Information */
		memcpy(finfo->awb_ver, &buf[EEP_AWB_VER_START_ADDR], FIMC_IS_AWB_VER_SIZE);
		finfo->awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';

		/* SHADING Data : Module/Manufacturer Information */
		memcpy(finfo->shading_ver, &buf[EEP_AP_SHADING_VER_START_ADDR], FIMC_IS_SHADING_VER_SIZE);
		finfo->shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';

		sysfs_finfo.af_cal_pan = *((u32 *)&buf[FROM_AF_CAL_PAN_ADDR]);
		sysfs_finfo.af_cal_macro = *((u32 *)&buf[FROM_AF_CAL_MACRO_ARRD]);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_OIS)
		finfo->ois_cal_start_addr = *((u32 *)&buf[EEP_HEADER_OIS_CAL_START_ADDR]);
		finfo->ois_cal_end_addr = *((u32 *)&buf[EEP_HEADER_OIS_CAL_END_ADDR]);
		info("OIS Cal start = 0x%08x, end = 0x%08x\n", (finfo->ois_cal_start_addr), (finfo->ois_cal_end_addr));

		finfo->ois_shift_start_addr = *((u32 *)&buf[EEP_HEADER_OIS_SHIFT_START_ADDR]);
		finfo->ois_shift_end_addr = *((u32 *)&buf[EEP_HEADER_OIS_SHIFT_END_ADDR]);
		info("OIS Shift start = 0x%08x, end = 0x%08x\n", (finfo->ois_shift_start_addr), (finfo->ois_shift_end_addr));

		finfo->ois_fw_set_start_addr = *((u32 *)&buf[EEP_HEADER_OIS_FW_SET_START_ADDR]);
		finfo->ois_fw_set_end_addr = *((u32 *)&buf[EEP_HEADER_OIS_FW_SET_END_ADDR]);
		info("OIS Set FW start = 0x%08x, end = 0x%08x\n", (finfo->ois_fw_set_start_addr), (finfo->ois_fw_set_end_addr));

		finfo->ois_fw_factory_start_addr = *((u32 *)&buf[EEP_HEADER_OIS_FW_FACTORY_START_ADDR]);
		finfo->ois_fw_factory_end_addr = *((u32 *)&buf[EEP_HEADER_OIS_FW_FACTORY_END_ADDR]);
		info("OIS Factory FW start = 0x%08x, end = 0x%08x\n", (finfo->ois_fw_factory_start_addr), (finfo->ois_fw_factory_end_addr));

		/* HEARDER Data : OIS F/W Version[SEC Info] */
		memcpy(finfo->ois_fw_ver, &buf[EEP_HEADER_OIS_FW_VER_START_ADDR], FIMC_IS_OIS_FW_VER_SIZE);
		finfo->ois_fw_ver[FIMC_IS_OIS_FW_VER_SIZE] = '\0';
		/* HEARDER Data : OIS Cal Version */
		memcpy(finfo->ois_cal_ver, &buf[EEP_HEADER_OIS_CAL_VER_START_ADDR], FIMC_IS_OIS_CAL_VER_SIZE);
		finfo->ois_cal_ver[FIMC_IS_OIS_CAL_VER_SIZE] = '\0';
		/* HEARDER Data : OIS Chip Info */
		memcpy(finfo->ois_chip_info, &buf[EEP_HEADER_OIS_CHIP_INFO_START_ADDR], FIMC_IS_OIS_CHIP_INFO_SIZE);
		finfo->ois_chip_info[FIMC_IS_OIS_CHIP_INFO_SIZE] = '\0';
		/* HEARDER Data : OIS Adjust Factor */
		memcpy(finfo->ois_adjust_factor, &buf[EEP_HEADER_OIS_ADJ_FACTOR_START_ADDR], FIMC_IS_OIS_ADJ_FACTOR_SIZE);
		finfo->ois_adjust_factor[FIMC_IS_OIS_ADJ_FACTOR_SIZE] = '\0';
#endif
	}
#endif

	/* debug info dump */
#if defined(EEPROM_DEBUG)
	info("++++ EEPROM data info\n");
	info("1. Header info\n");
	info("Module info : %s\n", finfo->header_ver);
	info(" ID : %c\n", finfo->header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", finfo->header_ver[FW_PIXEL_SIZE],
		finfo->header_ver[FW_PIXEL_SIZE+1]);
	info(" ISP ID : %c\n", finfo->header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", finfo->header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", finfo->header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", finfo->header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", finfo->header_ver[FW_PUB_NUM],
		finfo->header_ver[FW_PUB_NUM+1]);
	info(" Manufacturer ID : %c\n", finfo->header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
	info("project_name : %s\n", finfo->project_name);
	info("Cal data map ver : %s\n", finfo->cal_map_ver);
	info("2. OEM info\n");
	info("Module info : %s\n", finfo->oem_ver);
	info("3. AWB info\n");
	info("Module info : %s\n", finfo->awb_ver);
	info("4. Shading info\n");
	info("Module info : %s\n", finfo->shading_ver);
	info("---- EEPROM data info\n");
#endif

	/* CRC check */
	if (!fimc_is_sec_check_cal_crc32(buf, position) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT) {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory_front = crc32_check_front;
		} else {
			crc32_check_factory_front = false;
		}
	} else
#endif
	{
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory = crc32_check;
		} else {
			crc32_check_factory = false;
		}
	}

#ifdef CONFIG_COMPANION_C2_USE
	if (fimc_is_sec_check_from_ver(core, position)) {
		/* If FROM LSC value is not valid, loading default lsc data */
		if (*((u32 *)&cal_buf_front[sysfs_finfo_front.lsc_gain_start_addr]) == 0x00000000) {
			companion_front_lsc_isvalid = false;
		} else {
			companion_front_lsc_isvalid = true;
		}
	}
#endif

	// check CAMERA_MODULE_ES_VERSION in EEPROM
	if (position == SENSOR_POSITION_FRONT) {
		if (core->use_module_check) {
			if (finfo->header_ver[10] >= CAMERA_MODULE_ES_VERSION) {
				is_latest_cam_module_front = true;
			} else {
				is_latest_cam_module_front = false;
			}
		} else {
			is_latest_cam_module_front = true;
		}
	} else { // SENSOR_POSITION_REAR
		if (core->use_module_check) {
			if (finfo->header_ver[10] >= CAMERA_MODULE_ES_VERSION) {
				is_latest_cam_module = true;
			} else {
				is_latest_cam_module = false;
			}
		} else {
			is_latest_cam_module = true;
		}
	}

	// check FIMC_IS_LATEST_FROM_VERSION_M in EEPROM
	if (position == SENSOR_POSITION_FRONT) {
		if (core_pdata->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module_front = true;
			} else {
				is_final_cam_module_front = false;
			}
		} else {
			is_final_cam_module_front = true;
		}
	} else { // SENSOR_POSITION_REAR
		if (core_pdata->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module = true;
			} else {
				is_final_cam_module = false;
			}
		} else {
			is_final_cam_module = true;
		}
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		pr_info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			pr_info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			pr_info("dump folder exist, Dump EEPROM cal data.\n");
			if (write_data_to_file("/data/media/0/dump/eeprom_cal.bin", cal_buf,
									cal_size, &pos) < 0) {
				pr_info("Failed to dump cal data.\n");
				goto dump_err;
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

exit:
	return ret;
}

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
int fimc_is_sec_readcal_otprom(struct device *dev, int position)
{
	int ret = 0;
	char *buf = NULL;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct fimc_is_core *core = dev_get_drvdata(dev);
	struct exynos_platform_fimc_is *core_pdata = NULL;
	struct fimc_is_from_info *finfo = NULL;
	int cal_size = 0;
	struct i2c_client *client = NULL;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	char selected_page[2] = {0,};

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return -EINVAL;
	}

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
		finfo = &sysfs_finfo_front;
		fimc_is_sec_get_front_cal_buf(&buf);
		cal_size = FIMC_IS_MAX_CAL_SIZE_FRONT;
		client = core->eeprom_client1;
#endif
	} else {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
		finfo = &sysfs_finfo;
		fimc_is_sec_get_cal_buf(&buf);
		cal_size = FIMC_IS_MAX_CAL_SIZE;
		client = core->eeprom_client0;
#endif
	}


	fimc_is_i2c_config(client, true);
	msleep(10);

	ret = fimc_is_i2c_write(client, 0xA00, 0x04);
	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_write (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	fimc_is_i2c_write(client, 0xA02, 0x02);
	fimc_is_i2c_write(client, 0xA00, 0x01);

	ret = fimc_is_i2c_read(client, selected_page, 0xA12, 0x1);
	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	printk(KERN_INFO "Camera: otp_bank = %d\n", selected_page[0]);
	if (selected_page[0] == 0x3) {
		printk(KERN_INFO "Camera: OTP 3 page selected\n");
		fimc_is_i2c_write(client, 0xA00, 0x04);
		fimc_is_i2c_write(client, 0xA00, 0x00);

		msleep(1);

		fimc_is_i2c_write(client, 0xA00, 0x04);
		fimc_is_i2c_write(client, 0xA02, 0x03);
		fimc_is_i2c_write(client, 0xA00, 0x01);
	}
	fimc_is_i2c_read(client, finfo->cal_map_ver, 0xA22, 0x4);

	fimc_is_i2c_write(client, 0xA00, 0x04);
	fimc_is_i2c_write(client, 0xA00, 0x00);

	if(finfo->cal_map_ver[0] != 'V') {
		printk(KERN_INFO "Camera: Cal Map version read fail or there's no available data.\n");
		crc32_check_factory_front = false;
		goto exit;
	}

	printk(KERN_INFO "Camera: OTPROM Cal map_version = %c%c%c%c\n", finfo->cal_map_ver[0],
			finfo->cal_map_ver[1], finfo->cal_map_ver[2], finfo->cal_map_ver[3]);

crc_retry:
	cal_size = 50;
	fimc_is_i2c_write(client, 0xA00, 0x04);
	if(selected_page[0] == 1)
		fimc_is_i2c_write(client, 0xA02, 0x02);
	else
		fimc_is_i2c_write(client, 0xA02, 0x03);
	fimc_is_i2c_write(client, 0xA00, 0x01);

	/* read cal data */
	pr_info("Camera: I2C read cal data\n\n");
	fimc_is_i2c_read(client, buf, 0xA15, cal_size);

	fimc_is_i2c_write(client, 0xA00, 0x04);
	fimc_is_i2c_write(client, 0xA00, 0x00);

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[OPT_HEADER_VERSION_START_ADDR_FRONT], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver, &buf[OPT_HEADER_CAL_MAP_VER_START_ADDR_FRONT], FIMC_IS_CAL_MAP_VER_SIZE);
#endif
	} else {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[OPT_HEADER_VERSION_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver, &buf[OPT_HEADER_CAL_MAP_VER_START_ADDR], FIMC_IS_CAL_MAP_VER_SIZE);
#endif
	}

	/* debug info dump */
	pr_info("++++ OTPROM data info\n");
	pr_info("1. Header info\n");
	pr_info("Module info : %s\n", finfo->header_ver);
	pr_info(" ID : %c\n", finfo->header_ver[FW_CORE_VER]);
	pr_info(" Pixel num : %c%c\n", finfo->header_ver[FW_PIXEL_SIZE],
		finfo->header_ver[FW_PIXEL_SIZE+1]);
	pr_info(" ISP ID : %c\n", finfo->header_ver[FW_ISP_COMPANY]);
	pr_info(" Sensor Maker : %c\n", finfo->header_ver[FW_SENSOR_MAKER]);
	pr_info(" Year : %c\n", finfo->header_ver[FW_PUB_YEAR]);
	pr_info(" Month : %c\n", finfo->header_ver[FW_PUB_MON]);
	pr_info(" Release num : %c%c\n", finfo->header_ver[FW_PUB_NUM],
		finfo->header_ver[FW_PUB_NUM+1]);
	pr_info(" Manufacturer ID : %c\n", finfo->header_ver[FW_MODULE_COMPANY]);
	pr_info(" Module ver : %c\n", finfo->header_ver[FW_VERSION_INFO]);
	pr_info("---- OTPROM data info\n");

	/* CRC check */
	if (!fimc_is_sec_check_front_otp_crc32(buf, position) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

	if (position == SENSOR_POSITION_FRONT) {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory_front = crc32_check_front;
		} else {
			crc32_check_factory_front = false;
		}
		if (core_pdata->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module_front = true;
			} else {
				is_final_cam_module_front = false;
			}
		} else {
			is_final_cam_module_front = true;
		}
	} else {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory = crc32_check;
		} else {
			crc32_check_factory = false;
		}


		if (!core_pdata->use_module_check) {
			is_latest_cam_module = true;
		} else {
			if (sysfs_finfo.header_ver[10] >= CAMERA_MODULE_ES_VERSION) {
				is_latest_cam_module = true;
			} else {
				is_latest_cam_module = false;
			}
		}

		if (!core_pdata->use_module_check) {
			is_final_cam_module = true;
		} else {
			if (sysfs_finfo.header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module = true;
			} else {
				is_final_cam_module = false;
			}
		}
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		pr_info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			pr_info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			pr_info("dump folder exist, Dump OTPROM cal data.\n");
			if (write_data_to_file("/data/media/0/dump/otprom_cal.bin", cal_buf,
									FIMC_IS_DUMP_OTP_CAL_SIZE, &pos) < 0) {
				pr_info("Failed to dump cal data.\n");
				goto dump_err;
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
exit:
	fimc_is_i2c_config(client, false);

	return ret;
}
#endif
#endif


#if defined(CONFIG_CAMERA_FROM_SUPPORT_REAR)
int fimc_is_sec_read_from_header(struct device *dev)
{
	int ret = 0;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);
	u8 header_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };

	ret = fimc_is_spi_read(&core->spi0, header_version, FROM_HEADER_VERSION_START_ADDR, FIMC_IS_HEADER_VER_SIZE);
	if (ret < 0) {
		printk(KERN_ERR "failed to fimc_is_spi_read for header version (%d)\n", ret);
		ret = -EINVAL;
	}

	memcpy(sysfs_finfo.header_ver, header_version, FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';

	return ret;
}

int fimc_is_sec_check_status(struct fimc_is_core *core)
{
	int retry_read = 50;
	u8 temp[5] = {0x0, };
	int ret = 0;

	do {
		memset(temp, 0x0, sizeof(temp));
		fimc_is_spi_read_status_bit(&core->spi0, &temp[0]);
		if (retry_read < 0) {
			ret = -EINVAL;
			err("check status failed.");
			break;
		}
		retry_read--;
		msleep(3);
	} while (temp[0]);

	return ret;
}

#ifdef CAMERA_MODULE_DUALIZE
int fimc_is_sec_read_fw_from_sdcard(char *name, unsigned long *size)
{
	struct file *fw_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	char data_path[100];
	int ret = 0;
	unsigned long fsize;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	snprintf(data_path, sizeof(data_path), "%s", name);
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);

	fw_fp = filp_open(data_path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fw_fp)) {
		info("%s does not exist.\n", data_path);
		fw_fp = NULL;
		ret = -EIO;
		goto fw_err;
	} else {
		info("%s exist, Dump from sdcard.\n", name);
		fsize = fw_fp->f_path.dentry->d_inode->i_size;
		read_data_from_file(name, fw_buf, fsize, &pos);
		*size = fsize;
	}

fw_err:
	if (fw_fp)
		filp_close(fw_fp, current->files);
	set_fs(old_fs);

	return ret;
}

u32 fimc_is_sec_get_fw_crc32(char *buf, size_t size)
{
	u32 *buf32 = NULL;
	u32 checksum;

	buf32 = (u32 *)buf;
	checksum = (u32)getCRC((u16 *)&buf32[0], size, NULL, NULL);

	return checksum;
}

int fimc_is_sec_change_from_header(struct fimc_is_core *core)
{
	int ret = 0;
	u8 crc_value[4];
	u32 crc_result = 0;

	/* read header data */
	info("Camera: Start SPI read header data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);

	ret = fimc_is_spi_read(&core->spi0, fw_buf, 0x0, HEADER_CRC32_LEN);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	fw_buf[0x7] = (sysfs_finfo.bin_end_addr & 0xFF000000) >> 24;
	fw_buf[0x6] = (sysfs_finfo.bin_end_addr & 0xFF0000) >> 16;
	fw_buf[0x5] = (sysfs_finfo.bin_end_addr & 0xFF00) >> 8;
	fw_buf[0x4] = (sysfs_finfo.bin_end_addr & 0xFF);
	fw_buf[0x27] = (sysfs_finfo.setfile_end_addr & 0xFF000000) >> 24;
	fw_buf[0x26] = (sysfs_finfo.setfile_end_addr & 0xFF0000) >> 16;
	fw_buf[0x25] = (sysfs_finfo.setfile_end_addr & 0xFF00) >> 8;
	fw_buf[0x24] = (sysfs_finfo.setfile_end_addr & 0xFF);
	fw_buf[0x37] = (sysfs_finfo.concord_bin_end_addr & 0xFF000000) >> 24;
	fw_buf[0x36] = (sysfs_finfo.concord_bin_end_addr & 0xFF0000) >> 16;
	fw_buf[0x35] = (sysfs_finfo.concord_bin_end_addr & 0xFF00) >> 8;
	fw_buf[0x34] = (sysfs_finfo.concord_bin_end_addr & 0xFF);

	strncpy(&fw_buf[0x40], sysfs_finfo.header_ver, 9);
	strncpy(&fw_buf[0x50], sysfs_finfo.concord_header_ver, FIMC_IS_HEADER_VER_SIZE);
	strncpy(&fw_buf[0x64], sysfs_finfo.setfile_ver, FIMC_IS_ISP_SETFILE_VER_SIZE);

	fimc_is_spi_write_enable(&core->spi0);
	ret = fimc_is_spi_erase_sector(&core->spi0, 0x0);
	if (ret) {
		err("failed to fimc_is_spi_erase_sector (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	ret = fimc_is_sec_check_status(core);
	if (ret) {
		err("failed to fimc_is_sec_check_status (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_write_enable(&core->spi0);
	ret = fimc_is_spi_write(&core->spi0, 0x0, fw_buf, HEADER_CRC32_LEN);
	if (ret) {
		err("failed to fimc_is_spi_write (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	ret = fimc_is_sec_check_status(core);
	if (ret) {
		err("failed to fimc_is_sec_check_status (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	crc_result = fimc_is_sec_get_fw_crc32(fw_buf, HEADER_CRC32_LEN);
	crc_value[3] = (crc_result & 0xFF000000) >> 24;
	crc_value[2] = (crc_result & 0xFF0000) >> 16;
	crc_value[1] = (crc_result & 0xFF00) >> 8;
	crc_value[0] = (crc_result & 0xFF);

	ret = fimc_is_spi_write_enable(&core->spi0);
	ret = fimc_is_spi_write(&core->spi0, 0x0FFC, crc_value, 0x4);
	if (ret) {
		err("failed to fimc_is_spi_write (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	ret = fimc_is_sec_check_status(core);
	if (ret) {
		err("failed to fimc_is_sec_check_status (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	info("Camera: End SPI read header data\n");

exit:
	return ret;
}

int fimc_is_sec_write_fw_to_from(struct fimc_is_core *core, char *name, bool first_section)
{
	int ret = 0;
	unsigned long i = 0;
	unsigned long size = 0;
	u32 start_addr = 0, erase_addr = 0, end_addr = 0;
	u32 checksum_addr = 0, crc_result = 0, erase_end_addr = 0;
	u8 crc_value[4];

	if (!strcmp(name, FIMC_IS_FW_FROM_SDCARD)) {
		ret = fimc_is_sec_read_fw_from_sdcard(FIMC_IS_FW_FROM_SDCARD, &size);
		start_addr = sysfs_finfo.bin_start_addr;
		end_addr = size + start_addr - 1;
		sysfs_finfo.bin_end_addr = end_addr;
		checksum_addr = 0x3FFFFF;
		sysfs_finfo.fw_size = size;
		strncpy(sysfs_finfo.header_ver, &fw_buf[size - 11], 9);
	} else if (!strcmp(name, FIMC_IS_SETFILE_FROM_SDCARD)) {
		ret = fimc_is_sec_read_fw_from_sdcard(FIMC_IS_SETFILE_FROM_SDCARD, &size);
		start_addr = sysfs_finfo.setfile_start_addr;
		end_addr = size + start_addr - 1;
		sysfs_finfo.setfile_end_addr = end_addr;
		checksum_addr = 0x156FFF;
		sysfs_finfo.setfile_size = size;
		strncpy(sysfs_finfo.setfile_ver, &fw_buf[size - 64], 6);
	} else if (!strcmp(name, FIMC_IS_COMPANION_FROM_SDCARD)) {
		ret = fimc_is_sec_read_fw_from_sdcard(FIMC_IS_COMPANION_FROM_SDCARD, &size);
		start_addr = sysfs_finfo.concord_bin_start_addr;
		end_addr = size + start_addr - 1;
		sysfs_finfo.concord_bin_end_addr = end_addr;
		checksum_addr = 0x407FF;
		erase_end_addr = 0x3FFFFF;
		sysfs_finfo.comp_fw_size = size;
		strncpy(sysfs_finfo.concord_header_ver, &fw_buf[size - 16], FIMC_IS_HEADER_VER_SIZE);
	} else {
		err("Not supported binary type.");
		return -EIO;
	}

	if (ret < 0) {
		err("FW is not exist in sdcard.");
		return -EIO;
	}

	info("Start %s write to FROM.\n", name);

	if (first_section) {
		for (erase_addr = start_addr; erase_addr < erase_end_addr; erase_addr += FIMC_IS_FROM_ERASE_SIZE) {
			ret = fimc_is_spi_write_enable(&core->spi0);
			ret |= fimc_is_spi_erase_block(&core->spi0, erase_addr);
			if (ret) {
				err("failed to fimc_is_spi_erase_block (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
			ret = fimc_is_sec_check_status(core);
			if (ret) {
				err("failed to fimc_is_sec_check_status (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
		}
	}

	for (i = 0; i < size; i += 256) {
		ret = fimc_is_spi_write_enable(&core->spi0);
		if (size - i >= 256) {
			ret = fimc_is_spi_write(&core->spi0, start_addr + i, fw_buf + i, 256);
			if (ret) {
				err("failed to fimc_is_spi_write (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
		} else {
			ret = fimc_is_spi_write(&core->spi0, start_addr + i, fw_buf + i, size - i);
			if (ret) {
				err("failed to fimc_is_spi_write (%d)\n", ret);
				ret = -EINVAL;
				goto exit;
			}
		}
		ret = fimc_is_sec_check_status(core);
		if (ret) {
			err("failed to fimc_is_sec_check_status (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}
	}

	crc_result = fimc_is_sec_get_fw_crc32(fw_buf, size);
	crc_value[3] = (crc_result & 0xFF000000) >> 24;
	crc_value[2] = (crc_result & 0xFF0000) >> 16;
	crc_value[1] = (crc_result & 0xFF00) >> 8;
	crc_value[0] = (crc_result & 0xFF);

	ret = fimc_is_spi_write_enable(&core->spi0);
	if (ret) {
		err("failed to fimc_is_spi_write_enable (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_write(&core->spi0, checksum_addr -4 + 1, crc_value, 0x4);
	if (ret) {
		err("failed to fimc_is_spi_write (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	ret = fimc_is_sec_check_status(core);
	if (ret) {
		err("failed to fimc_is_sec_check_status (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	info("End %s write to FROM.\n", name);

exit:
	return ret;
}

int fimc_is_sec_write_fw(struct fimc_is_core *core, struct device *dev)
{
	int ret = 0;
#ifdef CONFIG_COMPANION_USE
	struct fimc_is_spi_gpio *spi_gpio = &core->spi0.gpio;
#endif
	struct file *key_fp = NULL;
	struct file *comp_fw_fp = NULL;
	struct file *setfile_fp = NULL;
	struct file *isp_fw_fp = NULL;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open(FIMC_IS_KEY_FROM_SDCARD, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		ret = -EIO;
		goto key_err;
	} else {
		comp_fw_fp = filp_open(FIMC_IS_COMPANION_FROM_SDCARD, O_RDONLY, 0);
		if (IS_ERR_OR_NULL(comp_fw_fp)) {
			info("Companion FW does not exist.\n");
			comp_fw_fp = NULL;
			ret = -EIO;
			goto comp_fw_err;
		}

		setfile_fp = filp_open(FIMC_IS_SETFILE_FROM_SDCARD, O_RDONLY, 0);
		if (IS_ERR_OR_NULL(setfile_fp)) {
			info("setfile does not exist.\n");
			setfile_fp = NULL;
			ret = -EIO;
			goto setfile_err;
		}

		isp_fw_fp = filp_open(FIMC_IS_FW_FROM_SDCARD, O_RDONLY, 0);
		if (IS_ERR_OR_NULL(isp_fw_fp)) {
			info("ISP FW does not exist.\n");
			isp_fw_fp = NULL;
			ret = -EIO;
			goto isp_fw_err;
		}
	}

	info("FW file exist, Write Firmware to FROM .\n");

	if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM"))
		fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);

#ifdef CONFIG_COMPANION_USE
	fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_FUNC, false);
#endif

#if defined(CONFIG_CAMERA_FROM_SUPPORT_REAR)
	ret = fimc_is_sec_write_fw_to_from(core, FIMC_IS_COMPANION_FROM_SDCARD, true);
	if (ret) {
		err("fimc_is_sec_write_fw_to_from failed.");
		ret = -EIO;
		goto isp_fw_err;
	}

	ret = fimc_is_sec_write_fw_to_from(core, FIMC_IS_SETFILE_FROM_SDCARD, false);
	if (ret) {
		err("fimc_is_sec_write_fw_to_from failed.");
		ret = -EIO;
		goto isp_fw_err;
	}

	ret = fimc_is_sec_write_fw_to_from(core, FIMC_IS_FW_FROM_SDCARD, false);
	if (ret) {
		err("fimc_is_sec_write_fw_to_from failed.");
		ret = -EIO;
		goto isp_fw_err;
	}
#endif
#ifdef CONFIG_COMPANION_USE
	fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_OUTPUT, true);
#endif

	/* Off to reset FROM operation. Without this routine, spi read does not work. */
	if (!core->running_rear_camera)
		fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);

	if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM"))
		fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);

#ifdef CONFIG_COMPANION_USE
	fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_FUNC, false);
#endif

#if defined(CONFIG_CAMERA_FROM_SUPPORT_REAR)
	ret = fimc_is_sec_change_from_header(core);
	if (ret) {
		err("fimc_is_sec_change_from_header failed.");
		ret = -EIO;
		goto isp_fw_err;
	}
#endif
#ifdef CONFIG_COMPANION_USE
	fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_OUTPUT, true);
#endif

	if (!core->running_rear_camera)
		fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);

isp_fw_err:
	if (isp_fw_fp)
		filp_close(isp_fw_fp, current->files);

setfile_err:
	if (setfile_fp)
		filp_close(setfile_fp, current->files);

comp_fw_err:
	if (comp_fw_fp)
		filp_close(comp_fw_fp, current->files);

key_err:
	if (key_fp)
		filp_close(key_fp, current->files);
	set_fs(old_fs);

	return ret;
}
#endif

int fimc_is_sec_readcal(struct fimc_is_core *core)
{
	int ret = 0;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	u16 id;

	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
	}

	/* reset spi */
	if (!core->spi0.device) {
		err("spi0 device is not available");
		goto exit;
	}

	ret = fimc_is_spi_reset(&core->spi0);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_read_module_id(&core->spi0, &id, FROM_HEADER_MODULE_ID_START_ADDR, FROM_HEADER_MODULE_ID_SIZE);
	if (ret) {
		printk(KERN_ERR "fimc_is_spi_read_module_id (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: FROM Module ID = 0x%04x\n", id);

	ret = fimc_is_spi_read(&core->spi0, sysfs_finfo.cal_map_ver,
			       FROM_HEADER_CAL_MAP_VER_START_ADDR, FIMC_IS_CAL_MAP_VER_SIZE);
	if (ret) {
		printk(KERN_ERR "failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: FROM Cal map_version = %c%c%c%c\n", sysfs_finfo.cal_map_ver[0],
			sysfs_finfo.cal_map_ver[1], sysfs_finfo.cal_map_ver[2], sysfs_finfo.cal_map_ver[3]);

crc_retry:
	/* read cal data */
	info("Camera: SPI read cal data\n");
	ret = fimc_is_spi_read(&core->spi0, cal_buf, 0x0, FIMC_IS_MAX_CAL_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	sysfs_finfo.bin_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_START_ADDR]);
	sysfs_finfo.bin_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_END_ADDR]);
	info("Binary start = 0x%08x, end = 0x%08x\n",
			(sysfs_finfo.bin_start_addr), (sysfs_finfo.bin_end_addr));
	sysfs_finfo.oem_start_addr = *((u32 *)&cal_buf[FROM_HEADER_OEM_START_ADDR]);
	sysfs_finfo.oem_end_addr = *((u32 *)&cal_buf[FROM_HEADER_OEM_END_ADDR]);
	info("OEM start = 0x%08x, end = 0x%08x\n",
			(sysfs_finfo.oem_start_addr), (sysfs_finfo.oem_end_addr));
	sysfs_finfo.awb_start_addr = *((u32 *)&cal_buf[FROM_HEADER_AWB_START_ADDR]);
	sysfs_finfo.awb_end_addr = *((u32 *)&cal_buf[FROM_HEADER_AWB_END_ADDR]);
	info("AWB start = 0x%08x, end = 0x%08x\n",
			(sysfs_finfo.awb_start_addr), (sysfs_finfo.awb_end_addr));
	sysfs_finfo.shading_start_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_START_ADDR]);
	sysfs_finfo.shading_end_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_END_ADDR]);
	info("Shading start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.shading_start_addr), (sysfs_finfo.shading_end_addr));
	sysfs_finfo.setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_START_ADDR]);
	sysfs_finfo.setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_END_ADDR]);
	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));
#ifdef CONFIG_COMPANION_USE
	sysfs_finfo.concord_cal_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_CAL_START_ADDR]);
	sysfs_finfo.concord_cal_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_CAL_END_ADDR]);
	info("concord cal start = 0x%08x, end = 0x%08x\n",
		sysfs_finfo.concord_cal_start_addr, sysfs_finfo.concord_cal_end_addr);
	sysfs_finfo.concord_bin_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_BINARY_START_ADDR]);
	sysfs_finfo.concord_bin_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_BINARY_END_ADDR]);
	info("concord bin start = 0x%08x, end = 0x%08x\n",
		sysfs_finfo.concord_bin_start_addr, sysfs_finfo.concord_bin_end_addr);
	sysfs_finfo.concord_master_setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MASTER_SETFILE_START_ADDR]);
	sysfs_finfo.concord_master_setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MASTER_SETFILE_START_ADDR]);
	sysfs_finfo.concord_mode_setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MODE_SETFILE_START_ADDR]);
	sysfs_finfo.concord_mode_setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MODE_SETFILE_END_ADDR]);
	sysfs_finfo.pdaf_cal_start_addr = *((u32 *)&cal_buf[FROM_HEADER_PDAF_CAL_START_ADDR]);
	sysfs_finfo.pdaf_cal_end_addr = *((u32 *)&cal_buf[FROM_HEADER_PDAF_CAL_END_ADDR]);
	info("pdaf start = 0x%08x, end = 0x%08x\n", sysfs_finfo.pdaf_cal_start_addr, sysfs_finfo.pdaf_cal_end_addr);
	sysfs_finfo.lsc_i0_gain_addr = FROM_SHADING_LSC_I0_GAIN_ADDR;
	info("Shading lsc_i0 start = 0x%08x\n", sysfs_finfo.lsc_i0_gain_addr);
	sysfs_finfo.lsc_j0_gain_addr = FROM_SHADING_LSC_J0_GAIN_ADDR;
	info("Shading lsc_j0 start = 0x%08x\n", sysfs_finfo.lsc_j0_gain_addr);
	sysfs_finfo.lsc_a_gain_addr = FROM_SHADING_LSC_A_GAIN_ADDR;
	info("Shading lsc_a start = 0x%08x\n", sysfs_finfo.lsc_a_gain_addr);
	sysfs_finfo.lsc_k4_gain_addr = FROM_SHADING_LSC_K4_GAIN_ADDR;
	info("Shading lsc_k4 start = 0x%08x\n", sysfs_finfo.lsc_k4_gain_addr);
	sysfs_finfo.lsc_scale_gain_addr = FROM_SHADING_LSC_SCALE_GAIN_ADDR;
#ifdef CONFIG_COMPANION_C1_USE
	sysfs_finfo.lsc_gain_start_addr = FROM_SHADING_LSC_GAIN_START_ADDR;
	sysfs_finfo.lsc_gain_end_addr = FROM_SHADING_LSC_GAIN_END_ADDR;
	info("LSC start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.lsc_gain_start_addr, sysfs_finfo.lsc_gain_end_addr);
#endif
#ifdef CONFIG_COMPANION_C2_USE
	info("Shading lsc_scale start = 0x%08x\n", sysfs_finfo.lsc_scale_gain_addr);
	sysfs_finfo.grasTuning_AwbAshCord_N_addr = FROM_SHADING_GRASTUNING_AWB_ASH_CORD_ADDR;
	info("Shading grasTuning_AwbAshCord_N start = 0x%08x\n", sysfs_finfo.grasTuning_AwbAshCord_N_addr);
	sysfs_finfo.grasTuning_awbAshCordIndexes_N_addr = FROM_SHADING_GRASTUNING_AWB_ASH_CORD_INDEX_ADDR;
	info("Shading grasTuning_awbAshCordIndexes_N start = 0x%08x\n", sysfs_finfo.grasTuning_awbAshCordIndexes_N_addr);
	sysfs_finfo.grasTuning_GASAlpha_M__N_addr = FROM_SHADING_GRASTUNING_GAS_ALPHA_ADDR;
	info("Shading lsc_scale start = 0x%08x\n", sysfs_finfo.grasTuning_GASAlpha_M__N_addr);
	sysfs_finfo.grasTuning_GASBeta_M__N_addr = FROM_SHADING_GRASTUNING_GAS_BETA_ADDR;
	info("Shading grasTuning_GASBeta_M__N start = 0x%08x\n", sysfs_finfo.grasTuning_GASBeta_M__N_addr);
	sysfs_finfo.grasTuning_GASOutdoorAlpha_N_addr = FROM_SHADING_GRASTUNING_GAS_OUTDOOR_ALPHA_ADDR;
	info("Shading grasTuning_GASOutdoorAlpha_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASOutdoorAlpha_N_addr);
	sysfs_finfo.grasTuning_GASOutdoorBeta_N_addr = FROM_SHADING_GRASTUNING_GAS_OUTDOOR_BETA_ADDR;
	info("Shading grasTuning_GASOutdoorBeta_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASOutdoorBeta_N_addr);
	sysfs_finfo.grasTuning_GASIndoorAlpha_N_addr = FROM_SHADING_GRASTUNING_GAS_INDOOR_ALPHA_ADDR;
	info("Shading grasTuning_GASIndoorAlpha_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASIndoorAlpha_N_addr);
	sysfs_finfo.grasTuning_GASIndoorBeta_N_addr = FROM_SHADING_GRASTUNING_GAS_INDOOR_BETA_ADDR;
	info("Shading grasTuning_GASIndoorBeta_N start = 0x%08x\n", sysfs_finfo.grasTuning_GASIndoorBeta_N_addr);
	sysfs_finfo.lsc_gain_start_addr = FROM_SHADING_LSC_GAIN_START_ADDR;
	sysfs_finfo.lsc_gain_end_addr = FROM_SHADING_LSC_GAIN_END_ADDR;
	info("LSC start = 0x%04x, end = 0x%04x\n", sysfs_finfo.lsc_gain_start_addr, sysfs_finfo.lsc_gain_end_addr);
#endif
	sysfs_finfo.pdaf_start_addr = FROM_CONCORD_CAL_PDAF_START_ADDR;
	sysfs_finfo.pdaf_end_addr = FROM_CONCORD_CAL_PDAF_END_ADDR;
	info("pdaf start = 0x%04x, end = 0x%04x\n", sysfs_finfo.pdaf_start_addr, sysfs_finfo.pdaf_end_addr);
#ifdef CONFIG_COMPANION_C2_USE
	sysfs_finfo.pdaf_shad_start_addr = FROM_CONCORD_CAL_PDAF_SHADING_START_ADDR;
	sysfs_finfo.pdaf_shad_end_addr = FROM_CONCORD_CAL_PDAF_SHADING_END_ADDR;
	info("pdaf_shad start = 0x%04x, end = 0x%04x\n", sysfs_finfo.pdaf_shad_start_addr,
		sysfs_finfo.pdaf_shad_end_addr);
#endif
#ifdef CONFIG_COMPANION_C1_USE
	/*sysfs_finfo.coefficient_cal_start_addr = sysfs_finfo.pdaf_start_addr + 512 + 16;
	sysfs_finfo.coefficient_cal_end_addr = sysfs_finfo.coefficient_cal_start_addr + 24576 -1;
	info("coefficient_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coefficient_cal_start_addr, sysfs_finfo.coefficient_cal_end_addr);*/
	sysfs_finfo.coef1_start = FROM_CONCORD_XTALK_10_START_ADDR;
	sysfs_finfo.coef1_end = FROM_CONCORD_XTALK_10_END_ADDR;
	info("coefficient1_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef1_start, sysfs_finfo.coef1_end);
	sysfs_finfo.coef2_start = FROM_CONCORD_XTALK_20_START_ADDR;
	sysfs_finfo.coef2_end = FROM_CONCORD_XTALK_20_END_ADDR;
	info("coefficient2_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef2_start, sysfs_finfo.coef2_end);
	sysfs_finfo.coef3_start = FROM_CONCORD_XTALK_30_START_ADDR;
	sysfs_finfo.coef3_end = FROM_CONCORD_XTALK_30_END_ADDR;
	info("coefficient3_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef3_start, sysfs_finfo.coef3_end);
	sysfs_finfo.coef4_start = FROM_CONCORD_XTALK_40_START_ADDR;
	sysfs_finfo.coef4_end = FROM_CONCORD_XTALK_40_END_ADDR;
	info("coefficient4_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef4_start, sysfs_finfo.coef4_end);
	sysfs_finfo.coef5_start = FROM_CONCORD_XTALK_50_START_ADDR;
	sysfs_finfo.coef5_end = FROM_CONCORD_XTALK_50_END_ADDR;
	info("coefficient5_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef5_start, sysfs_finfo.coef5_end);
	sysfs_finfo.coef6_start = FROM_CONCORD_XTALK_60_START_ADDR;
	sysfs_finfo.coef6_end = FROM_CONCORD_XTALK_60_END_ADDR;
	info("coefficient6_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef6_start, sysfs_finfo.coef6_end);
	sysfs_finfo.wcoefficient1_addr = FROM_CONCORD_WCOEF_ADDR;
	info("Shading wcoefficient1 start = 0x%04x\n", sysfs_finfo.wcoefficient1_addr);
#endif
#ifdef CONFIG_COMPANION_C2_USE
	sysfs_finfo.xtalk_coef_start = FROM_CONCORD_XTALK_COEF_ADDR;
	sysfs_finfo.coef_offset_R = FROM_CONCORD_COEF_OFFSET_R_ADDR;
	sysfs_finfo.coef_offset_G = FROM_CONCORD_COEF_OFFSET_G_ADDR;
	sysfs_finfo.coef_offset_B = FROM_CONCORD_COEF_OFFSET_B_ADDR;
#endif
	memcpy(sysfs_finfo.concord_header_ver,
	       &cal_buf[FROM_HEADER_CONCORD_HEADER_VER_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.concord_header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
#ifdef CONFIG_COMPANION_C1_USE
	sysfs_finfo.af_inf_addr = FROM_OEM_AF_INF_ADDR;
	sysfs_finfo.af_macro_addr = FROM_OEM_AF_INF_ADDR;
	sysfs_finfo.coef1_crc_addr= FROM_CONCORD_XTALK_10_CHECKSUM_ADDR;
	sysfs_finfo.coef2_crc_addr = FROM_CONCORD_XTALK_20_CHECKSUM_ADDR;
	sysfs_finfo.coef3_crc_addr = FROM_CONCORD_XTALK_30_CHECKSUM_ADDR;
	sysfs_finfo.coef4_crc_addr = FROM_CONCORD_XTALK_40_CHECKSUM_ADDR;
	sysfs_finfo.coef5_crc_addr = FROM_CONCORD_XTALK_50_CHECKSUM_ADDR;
	sysfs_finfo.coef6_crc_addr = FROM_CONCORD_XTALK_60_CHECKSUM_ADDR;
#endif
	sysfs_finfo.lsc_gain_crc_addr = FROM_SHADING_LSC_GAIN_CRC_ADDR;
	sysfs_finfo.pdaf_crc_addr = FROM_CONCORD_PDAF_CRC_ADDR;
#ifdef CONFIG_COMPANION_C2_USE
	sysfs_finfo.pdaf_shad_crc_addr = FROM_CONCORD_PDAF_SHAD_CRC_ADDR;
	sysfs_finfo.xtalk_coef_crc_addr = FROM_CONCORD_XTALK_COEF_CRC_ADDR;
	sysfs_finfo.lsc_parameter_crc_addr = FROM_SHADING_LSC_PARAMETER_CRC_ARRD;

	info("xtalk_coef_start = 0x%04x\n", sysfs_finfo.xtalk_coef_start);
	info("lsc_gain_crc_addr = 0x%04x\n", sysfs_finfo.lsc_gain_crc_addr);
	info("pdaf_crc_addr = 0x%04x\n", sysfs_finfo.pdaf_crc_addr);
	info("pdaf_shad_crc_addr = 0x%04x\n", sysfs_finfo.pdaf_shad_crc_addr);
	info("xtalk_coef_crc_addr= 0x%04x\n", sysfs_finfo.xtalk_coef_crc_addr);
	info("lsc_parameter_crc_addr = 0x%04x\n", sysfs_finfo.lsc_parameter_crc_addr);
#endif
#endif

	if (sysfs_finfo.setfile_end_addr < FROM_ISP_BINARY_SETFILE_START_ADDR
	 || sysfs_finfo.setfile_end_addr > FROM_ISP_BINARY_SETFILE_END_ADDR) {
		info("setfile end_addr has error!!  0x%08x\n", sysfs_finfo.setfile_end_addr);
		sysfs_finfo.setfile_end_addr = 0x1fffff;
	}

	info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));

	memcpy(sysfs_finfo.header_ver, &cal_buf[FROM_HEADER_VERSION_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.cal_map_ver, &cal_buf[FROM_HEADER_CAL_MAP_VER_START_ADDR], FIMC_IS_CAL_MAP_VER_SIZE);
	memcpy(sysfs_finfo.setfile_ver,
	       &cal_buf[FROM_HEADER_ISP_SETFILE_VER_START_ADDR], FIMC_IS_ISP_SETFILE_VER_SIZE);
	sysfs_finfo.setfile_ver[FIMC_IS_ISP_SETFILE_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.oem_ver, &cal_buf[FROM_OEM_VER_START_ADDR], FIMC_IS_OEM_VER_SIZE);
	sysfs_finfo.oem_ver[FIMC_IS_OEM_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.awb_ver, &cal_buf[FROM_AWB_VER_START_ADDR], FIMC_IS_AWB_VER_SIZE);
	sysfs_finfo.awb_ver[FIMC_IS_AWB_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.shading_ver, &cal_buf[FROM_SHADING_VER_START_ADDR], FIMC_IS_SHADING_VER_SIZE);
	sysfs_finfo.shading_ver[FIMC_IS_SHADING_VER_SIZE] = '\0';
	memcpy(sysfs_finfo.project_name, &cal_buf[FROM_HEADER_PROJECT_NAME_START_ADDR], FIMC_IS_PROJECT_NAME_SIZE);
	sysfs_finfo.project_name[FIMC_IS_PROJECT_NAME_SIZE] = '\0';

	fw_core_version = sysfs_finfo.header_ver[0];
	sysfs_finfo.fw_size = sysfs_finfo.bin_end_addr - sysfs_finfo.bin_start_addr + 1;
	sysfs_finfo.setfile_size = sysfs_finfo.setfile_end_addr - sysfs_finfo.setfile_start_addr + 1;
	sysfs_finfo.comp_fw_size = sysfs_finfo.concord_bin_end_addr - sysfs_finfo.concord_bin_start_addr + 1;
	info("fw_size = %ld\n", sysfs_finfo.fw_size);
	info("setfile_size = %ld\n", sysfs_finfo.setfile_size);
	info("comp_fw_size = %ld\n", sysfs_finfo.comp_fw_size);

	/* debug info dump */
	info("++++ FROM data info\n");
	info("1. Header info\n");
	info("Module info : %s\n", sysfs_finfo.header_ver);
#ifdef CONFIG_COMPANION_USE
	info("Companion version info : %s\n", sysfs_finfo.concord_header_ver);
#endif
	info(" ID : %c\n", sysfs_finfo.header_ver[FW_CORE_VER]);
	info(" Pixel num : %c%c\n", sysfs_finfo.header_ver[FW_PIXEL_SIZE],
							sysfs_finfo.header_ver[FW_PIXEL_SIZE + 1]);
	info(" ISP ID : %c\n", sysfs_finfo.header_ver[FW_ISP_COMPANY]);
	info(" Sensor Maker : %c\n", sysfs_finfo.header_ver[FW_SENSOR_MAKER]);
	info(" Year : %c\n", sysfs_finfo.header_ver[FW_PUB_YEAR]);
	info(" Month : %c\n", sysfs_finfo.header_ver[FW_PUB_MON]);
	info(" Release num : %c%c\n", sysfs_finfo.header_ver[FW_PUB_NUM],
							sysfs_finfo.header_ver[FW_PUB_NUM + 1]);
	info(" Manufacturer ID : %c\n", sysfs_finfo.header_ver[FW_MODULE_COMPANY]);
	info(" Module ver : %c\n", sysfs_finfo.header_ver[FW_VERSION_INFO]);
	info("Cal data map ver : %s\n", sysfs_finfo.cal_map_ver);
	info("Setfile ver : %s\n", sysfs_finfo.setfile_ver);
	info("Project name : %s\n", sysfs_finfo.project_name);
	info("2. OEM info\n");
	info("Module info : %s\n", sysfs_finfo.oem_ver);
	info("3. AWB info\n");
	info("Module info : %s\n", sysfs_finfo.awb_ver);
	info("4. Shading info\n");
	info("Module info : %s\n", sysfs_finfo.shading_ver);
	info("---- FROM data info\n");

	/* CRC check */
#ifdef CONFIG_COMPANION_USE
	if (fimc_is_sec_check_from_ver(core, SENSOR_POSITION_REAR)) {
		if (!fimc_is_sec_check_cal_crc32(cal_buf, SENSOR_POSITION_REAR) && (retry > 0)) {
			retry--;
			goto crc_retry;
		}
	} else {
		fw_version_crc_check = false;
		crc32_check = false;
		crc32_c1_check = false;
	}
#else
	if (!fimc_is_sec_check_cal_crc32(cal_buf, SENSOR_POSITION_REAR) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}
#endif

	if (sysfs_finfo.header_ver[3] == 'L') {
		crc32_check_factory = crc32_check;
#ifdef CONFIG_COMPANION_USE
		crc32_c1_check_factory = crc32_c1_check;
#endif
	} else {
		crc32_check_factory = false;
#ifdef CONFIG_COMPANION_USE
		crc32_c1_check_factory = false;
#endif
	}

#ifdef CONFIG_COMPANION_USE
	if (fimc_is_sec_check_from_ver(core, SENSOR_POSITION_REAR)) {
		if (crc32_check && crc32_c1_check) {
			/* If FROM LSC value is not valid, loading default lsc data */
			if (*((u32 *)&cal_buf[sysfs_finfo.lsc_gain_start_addr]) == 0x00000000) {
				companion_lsc_isvalid = false;
			} else {
				companion_lsc_isvalid = true;
			}
#ifdef CONFIG_COMPANION_C2_USE
			if (*((u32 *)&cal_buf[sysfs_finfo.xtalk_coef_start]) == 0x00000000)
#endif
#ifdef CONFIG_COMPANION_C1_USE
			if (*((u32 *)&cal_buf[sysfs_finfo.coef1_start]) == 0x00000000)
#endif
			{
				companion_coef_isvalid = false;
			} else {
				companion_coef_isvalid = true;
			}
		} else {
			companion_lsc_isvalid = false;
			companion_coef_isvalid = false;
		}
	} else {
		companion_lsc_isvalid = true;
		companion_coef_isvalid = true;
	}
#endif

	if (!core_pdata->use_module_check) {
		is_latest_cam_module = true;
	} else {
		if (sysfs_finfo.header_ver[10] >= CAMERA_MODULE_ES_VERSION) {
			is_latest_cam_module = true;
		} else {
			is_latest_cam_module = false;
		}
	}

	if (core_pdata->use_module_check) {
		if (sysfs_finfo.header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
			is_final_cam_module = true;
		} else {
			is_final_cam_module = false;
		}
	} else {
		is_final_cam_module = true;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	key_fp = filp_open("/data/media/0/1q2w3e4r.key", O_RDONLY, 0);
	if (IS_ERR(key_fp)) {
		info("KEY does not exist.\n");
		key_fp = NULL;
		goto key_err;
	} else {
		dump_fp = filp_open("/data/media/0/dump", O_RDONLY, 0);
		if (IS_ERR(dump_fp)) {
			info("dump folder does not exist.\n");
			dump_fp = NULL;
			goto key_err;
		} else {
			info("dump folder exist, Dump FROM cal data.\n");
			if (write_data_to_file("/data/media/0/dump/from_cal.bin", cal_buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
				info("Failedl to dump cal data.\n");
				goto dump_err;
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
exit:
	return ret;
}

#ifdef CAMERA_MODULE_DUALIZE
int fimc_is_sec_readfw(struct fimc_is_core *core)
{
	int ret = 0;
	loff_t pos = 0;
	char fw_path[100];
	int retry = FIMC_IS_FW_RETRY_CNT;

	info("Camera: FW need to be dumped\n");

crc_retry:
	/* read fw data */
	info("Camera: Start SPI read fw data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
	ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.bin_start_addr, FIMC_IS_MAX_FW_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: End SPI read fw data\n");

	/* CRC check */
	if (!fimc_is_sec_check_fw_crc32(fw_buf) && (retry > 0)) {
		retry--;
		goto crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_fw_name);

	if (write_data_to_file(fw_path, fw_buf, sysfs_finfo.fw_size, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: FW Data has dumped successfully\n");

exit:
	return ret;
}

int fimc_is_sec_read_setfile(struct fimc_is_core *core)
{
	int ret = 0;
	loff_t pos = 0;
	char setfile_path[100];
	int retry = FIMC_IS_FW_RETRY_CNT;

	info("Camera: Setfile need to be dumped\n");

setfile_crc_retry:
	/* read setfile data */
	info("Camera: Start SPI read setfile data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
	ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.setfile_start_addr, FIMC_IS_MAX_SETFILE_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: End SPI read setfile data\n");

	/* CRC check */
	if (!fimc_is_sec_check_setfile_crc32(fw_buf) && (retry > 0)) {
		retry--;
		goto setfile_crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	snprintf(setfile_path, sizeof(setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_setfile_name);
	pos = 0;

	if (write_data_to_file(setfile_path, fw_buf, sysfs_finfo.setfile_size, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: Setfile has dumped successfully\n");

exit:
	return ret;
}
#endif
#endif

int fimc_is_sec_check_reload(void)
{
	struct file *reload_key_fp = NULL;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	reload_key_fp = filp_open("/data/media/0/reload/r1e2l3o4a5d.key", O_RDONLY, 0);
	if (IS_ERR(reload_key_fp)) {
		info("Reload KEY does not exist.\n");
		reload_key_fp = NULL;
	} else {
		info("Reload KEY exist, reload cal data.\n");
		force_caldata_dump = true;
	}

	if (reload_key_fp)
		filp_close(reload_key_fp, current->files);
	set_fs(old_fs);

	return 0;
}

#ifdef CAMERA_MODULE_DUALIZE
#ifdef CONFIG_COMPANION_USE
int fimc_is_sec_read_companion_fw(struct fimc_is_core *core)
{
	int ret = 0;
	loff_t pos = 0;
	char fw_path[100];
	//char master_setfile_path[100];
	//char mode_setfile_path[100];
	int retry = FIMC_IS_FW_RETRY_CNT;

	info("Camera: Companion FW, Setfile need to be dumped\n");

crc_retry:
	/* read companion fw data */
	info("Camera: Start SPI read companion fw data\n");
	memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
	ret = fimc_is_spi_read(&core->spi0, fw_buf, sysfs_finfo.concord_bin_start_addr, FIMC_IS_MAX_COMPANION_FW_SIZE);
	if (ret) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	info("Camera: End SPI read companion fw data\n");

	/* CRC check */
	if (!fimc_is_sec_check_companion_fw_crc32(fw_buf) && (retry > 0)) {
		retry--;
		goto crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	snprintf(fw_path, sizeof(fw_path), "%s%s",
		FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_c1_fw_name);

	if (write_data_to_file(fw_path, fw_buf, sysfs_finfo.comp_fw_size, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: Companion FW Data has dumped successfully\n");
#if 0
	snprintf(master_setfile_path, sizeof(master_setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_c1_mastersetf_name);
	snprintf(mode_setfile_path, sizeof(mode_setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_c1_modesetf_name);
	pos = 0;

	if (write_data_to_file(master_setfile_path,
			comp_fw_buf + sysfs_finfo.concord_master_setfile_start_addr,
			sysfs_finfo.concord_master_setfile_end_addr - sysfs_finfo.concord_master_setfile_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}
	pos = 0;
	if (write_data_to_file(mode_setfile_path,
			comp_fw_buf + sysfs_finfo.concord_mode_setfile_start_addr,
			sysfs_finfo.concord_mode_setfile_end_addr - sysfs_finfo.concord_mode_setfile_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	info("Camera: Companion Setfile has dumped successfully\n");
#endif

exit:
	return ret;
}
#endif
#endif

#if 0
int fimc_is_sec_gpio_enable(struct exynos_platform_fimc_is *pdata, char *name, bool on)
{
	struct gpio_set *gpio;
	int ret = 0;
	int i = 0;

	for (i = 0; i < FIMC_IS_MAX_GPIO_NUM; i++) {
			gpio = &pdata->gpio_info->cfg[i];
			if (strcmp(gpio->name, name) == 0)
				break;
			else
				continue;
	}

	if (i == FIMC_IS_MAX_GPIO_NUM) {
		err("GPIO %s is not found!!", name);
		ret = -EINVAL;
		goto exit;
	}

	ret = gpio_request(gpio->pin, gpio->name);
	if (ret) {
		err("Request GPIO error(%s)", gpio->name);
		goto exit;
	}

	if (on) {
		switch (gpio->act) {
		case GPIO_PULL_NONE:
			s3c_gpio_cfgpin(gpio->pin, gpio->value);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			break;
		case GPIO_OUTPUT:
			s3c_gpio_cfgpin(gpio->pin, S3C_GPIO_OUTPUT);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio->pin, gpio->value);
			break;
		case GPIO_INPUT:
			s3c_gpio_cfgpin(gpio->pin, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_set_value(gpio->pin, gpio->value);
			break;
		case GPIO_RESET:
			s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_NONE);
			gpio_direction_output(gpio->pin, 0);
			gpio_direction_output(gpio->pin, 1);
			break;
		default:
			err("unknown act for gpio");
			break;
		}
	} else {
		s3c_gpio_cfgpin(gpio->pin, S3C_GPIO_INPUT);
		s3c_gpio_setpull(gpio->pin, S3C_GPIO_PULL_DOWN);
	}

	gpio_free(gpio->pin);

exit:
	return ret;
}
#endif

int fimc_is_sec_get_pixel_size(char *header_ver)
{
	int pixelsize = 0;

	pixelsize += (int) (header_ver[FW_PIXEL_SIZE] - 0x30) * 10;
	pixelsize += (int) (header_ver[FW_PIXEL_SIZE + 1] - 0x30);

	return pixelsize;
}

int fimc_is_sec_core_voltage_select(struct device *dev, char *header_ver)
{
	struct regulator *regulator = NULL;
	int ret = 0;
	int minV, maxV;
	int pixelSize = 0;

	regulator = regulator_get(dev, "cam_sensor_core_1.2v");
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get fail",
			__func__);
		return -EINVAL;
	}
	pixelSize = fimc_is_sec_get_pixel_size(header_ver);

	if (header_ver[FW_SENSOR_MAKER] == FW_SENSOR_MAKER_SONY) {
		if (pixelSize == 13) {
			minV = 1050000;
			maxV = 1050000;
		} else if (pixelSize == 8) {
			minV = 1100000;
			maxV = 1100000;
		} else {
			minV = 1050000;
			maxV = 1050000;
		}
	} else if (header_ver[FW_SENSOR_MAKER] == FW_SENSOR_MAKER_SLSI) {
		minV = 1200000;
		maxV = 1200000;
	} else {
		minV = 1050000;
		maxV = 1050000;
	}

	ret = regulator_set_voltage(regulator, minV, maxV);

	if (ret >= 0)
		info("%s : set_core_voltage %d, %d successfully\n",
				__func__, minV, maxV);
	regulator_put(regulator);

	return ret;
}

/**
 * fimc_is_sec_ldo_enabled: check whether the ldo has already been enabled.
 *
 * @ return: true, false or error value
 */
int fimc_is_sec_ldo_enabled(struct device *dev, char *name) {
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

int fimc_is_sec_ldo_set_voltage(struct device *dev, char *name, int uV) {
	struct regulator *regulator = NULL;
	int ret = 0;

	regulator = regulator_get(dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		err("%s : regulator_get(%s) fail", __func__, name);
		regulator_put(regulator);
		return -EINVAL;
	}

	ret = regulator_set_voltage(regulator, uV, uV);
	if (ret) {
		err("%s: regulator set voltage failed \n", name);
	}

	regulator_put(regulator);

	return ret;
}

int fimc_is_sec_ldo_enable(struct device *dev, char *name, bool on)
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
			pr_warning("%s: regulator is already enabled\n", name);
			goto exit;
		}

		ret = regulator_enable(regulator);
		if (ret) {
			err("%s : regulator_enable(%s) fail", __func__, name);
			goto exit;
		}
	} else {
		if (!regulator_is_enabled(regulator)) {
			pr_warning("%s: regulator is already disabled\n", name);
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

int fimc_is_sec_fw_find(struct fimc_is_core *core)
{
	int pixelSize = 0;
	int sensor_id = 0;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return -EINVAL;
	}

	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_F) ||
		fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_I) ||
		fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_A)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2), "%s", FIMC_IS_FW_2P2);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_SETF), "%s", FIMC_IS_2P2_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_S5K2P2;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_12M)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2_12M), "%s", FIMC_IS_FW_2P2_12M);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_12M_SETF), "%s", FIMC_IS_2P2_12M_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_S5K2P2_12M;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P3)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P3), "%s", FIMC_IS_FW_2P3);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P3_SETF), "%s", FIMC_IS_2P3_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_S5K2P3;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_4H5)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_4H5), "%s", FIMC_IS_FW_4H5);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_4H5_SETF), "%s", FIMC_IS_4H5_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_S5K4H5;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX240)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX240), "%s", FIMC_IS_FW_IMX240);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX240_SETF), "%s", FIMC_IS_IMX240_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_IMX240;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX228)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX228), "%s", FIMC_IS_FW_IMX228);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX228_SETF), "%s", FIMC_IS_IMX228_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_IMX228;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2T2)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2T2_EVT1), "%s", FIMC_IS_FW_2T2_EVT1);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2T2_SETF), "%s", FIMC_IS_2T2_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_S5K2T2;
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2) ||
			fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2_E) ||
			fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2_T) ||
			fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2_U)) {
		snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_3L2), "%s", FIMC_IS_FW_3L2);
		snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
		core_pdata->rear_sensor_id = SENSOR_NAME_S5K3L2;
	} else {
		/* Use the PixelSize information */
		pixelSize = fimc_is_sec_get_pixel_size(sysfs_finfo.header_ver);
#if defined(FRONT_CAMERA_ONLY_SUPPORT)
		sensor_id = core_pdata->front_sensor_id;
#else
		sensor_id = core_pdata->rear_sensor_id;
#endif
		if (pixelSize == 16) {
			if (sensor_id == SENSOR_NAME_S5K2P3) {
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P3), "%s", FIMC_IS_FW_2P3);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P3_SETF), "%s", FIMC_IS_2P3_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K3P3) {
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_3P3), "%s", FIMC_IS_FW_3P3);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_3P3_SETF), "%s", FIMC_IS_3P3_SETF);
			} else {
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2), "%s", FIMC_IS_FW_2P2);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_SETF), "%s", FIMC_IS_2P2_SETF);
			}
		} else if (pixelSize == 13) {
			if (sensor_id == SENSOR_NAME_S5K3L2) {
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_3L2), "%s", FIMC_IS_FW_3L2);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
			} else {
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
			}
		} else if (pixelSize == 12) {
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2_12M), "%s", FIMC_IS_FW_2P2_12M);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_12M_SETF), "%s", FIMC_IS_2P2_12M_SETF);
		} else if (pixelSize == 8) {
			snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
			snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
		} else {
			/* default firmware and setfile */
			if (sensor_id == SENSOR_NAME_IMX240) {
				/* IMX240 */
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX240), "%s", FIMC_IS_FW_IMX240);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX240_SETF), "%s", FIMC_IS_IMX240_SETF);
			} else if (sensor_id == SENSOR_NAME_IMX134) {
				/* IMX134 */
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K2P2_12M) {
				/* 2P2_12M */
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2_12M), "%s", FIMC_IS_FW_2P2_12M);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_12M_SETF), "%s", FIMC_IS_2P2_12M_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K2P2) {
				/* 2P2 */
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P2), "%s", FIMC_IS_FW_2P2);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P2_SETF), "%s", FIMC_IS_2P2_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K3P3) {
				/* 3P3 */
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_3P3), "%s", FIMC_IS_FW_3P3);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_3P3_SETF), "%s", FIMC_IS_3P3_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K4H5) {
				/* 4H5 */
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_4H5), "%s", FIMC_IS_FW_4H5);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_4H5_SETF), "%s", FIMC_IS_4H5_SETF);
			} else if ( sensor_id == SENSOR_NAME_S5K2P3 ) {
				/* 2P3 */
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2P3), "%s", FIMC_IS_FW_2P3);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2P3_SETF), "%s", FIMC_IS_2P3_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K2T2) {
				/* 2T2 */
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_2T2), "%s", FIMC_IS_FW_2T2);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_2T2_SETF), "%s", FIMC_IS_2T2_SETF);
			} else if (sensor_id == SENSOR_NAME_SR544) {
				/* SR544 */
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_SR544), "%s", FIMC_IS_FW_SR544);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_SR544_SETF), "%s", FIMC_IS_SR544_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K3L2) {
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_3L2), "%s", FIMC_IS_FW_3L2);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
			} else if (sensor_id == SENSOR_NAME_SR261) {
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW_SR261), "%s", FIMC_IS_FW_SR261);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_SR261_SETF), "%s", FIMC_IS_SR261_SETF);
			} else {
				snprintf(sysfs_finfo.load_fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
				snprintf(sysfs_finfo.load_setfile_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
			}
		}
	}

	return 0;
}

int fimc_is_sec_run_fw_sel(struct device *dev, int position)
{
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	int ret = 0;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return -EINVAL;
	}

	/* Check reload cal data enabled */
	if (!sysfs_finfo.is_check_cal_reload) {
		fimc_is_sec_check_reload();
	}
	sysfs_finfo.is_check_cal_reload = true;

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT) {
		if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
			ret = fimc_is_sec_fw_sel_eeprom(dev, SENSOR_POSITION_REAR, true);
#else
/* When using C2 retention, Cal loading for both front and rear cam will be done at a time */
#if !defined(CONFIG_COMPANION_C2_USE)
			ret = fimc_is_sec_fw_sel(core, dev, true);
#endif
#endif
		}

		if (!sysfs_finfo_front.is_caldata_read || force_caldata_dump) {
			ret = fimc_is_sec_fw_sel_eeprom(dev, SENSOR_POSITION_FRONT, false);
			if (ret < 0) {
				err("failed to select firmware (%d)", ret);
				goto p_err;
			}
		}
	} else
#endif
	{
		if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
			ret = fimc_is_sec_fw_sel_eeprom(dev, SENSOR_POSITION_REAR, false);
#elif defined(CONFIG_CAMERA_FROM_SUPPORT_REAR)
			ret = fimc_is_sec_fw_sel(core, dev, false);
#else
			ret = fimc_is_sec_fw_find(core);
#endif
			if (ret < 0) {
				err("failed to select firmware (%d)", ret);
				goto p_err;
			}
		}
	}

p_err:
	if (position == SENSOR_POSITION_REAR) {
		if (core_pdata->check_sensor_vendor) {
			if (fimc_is_sec_check_from_ver(core, position)) {
				if (sysfs_finfo.header_ver[3] != 'L') {
					err("Not supported module. Module ver = %s", sysfs_finfo.header_ver);
					return  -EIO;
				}
			}
		}
	}
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	else {
		if (core_pdata->check_sensor_vendor) {
			if (fimc_is_sec_check_from_ver(core, position)) {
				if (sysfs_finfo_front.header_ver[3] != 'L') {
					err("Not supported front module. Module ver = %s", sysfs_finfo_front.header_ver);
					return  -EIO;
				}
			}
		}
	}
#endif

	return ret;
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
int fimc_is_sec_fw_sel_eeprom(struct device *dev, int id, bool headerOnly)
{
	int ret = 0;
	char fw_path[100];
	char phone_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
	bool is_ldo_enabled[2];
	struct fimc_is_core *core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);

	is_ldo_enabled[0] = false;
	is_ldo_enabled[1] = false;

	/* Use mutex for i2c read */
	mutex_lock(&core->spi_lock);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		if (!sysfs_finfo_front.is_caldata_read || force_caldata_dump) {
			if (force_caldata_dump)
				info("Front forced caldata dump!!\n");

			if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_VT")) {
				fimc_is_sec_rom_power_on(core, SENSOR_POSITION_FRONT);
				is_ldo_enabled[0] = true;
			}

			if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM")) {
				fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);
				is_ldo_enabled[1] = true;
			}

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
			info("Camera: read cal data from Front OTPROM\n");
			if (!fimc_is_sec_readcal_otprom(dev, SENSOR_POSITION_FRONT)) {
				sysfs_finfo_front.is_caldata_read = true;
			}
#else
			info("Camera: read cal data from Front EEPROM\n");
			if (!fimc_is_sec_readcal_eeprom(dev, SENSOR_POSITION_FRONT)) {
				sysfs_finfo_front.is_caldata_read = true;
			}
#endif
		}
		goto exit;
	} else
#endif
	{
		if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
			is_dumped_fw_loading_needed = false;
			if (force_caldata_dump)
				info("Rear forced caldata dump!!\n");

			if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM")) {
				fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);
				is_ldo_enabled[0] = true;
			}

			info("Camera: read cal data from Rear EEPROM, headerOnly(%d)\n", headerOnly);
			if (headerOnly) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
				fimc_is_sec_read_eeprom_header(dev);
#endif
			} else {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
				if (!fimc_is_sec_readcal_otprom(dev, SENSOR_POSITION_REAR)) {
					sysfs_finfo.is_caldata_read = true;
				}
#else
				if (!fimc_is_sec_readcal_eeprom(dev, SENSOR_POSITION_REAR)) {
					sysfs_finfo.is_caldata_read = true;
				}
#endif
			}
		}
	}

	fimc_is_sec_fw_find(core);
	if (headerOnly) {
		goto exit;
	}

	snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_PATH, sysfs_finfo.load_fw_name);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		err("Camera: Failed open phone firmware");
		ret = -EIO;
		fp = NULL;
		goto read_phone_fw_exit;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	info("start, file path %s, size %ld Bytes\n",
		fw_path, fsize);

#ifdef CAMERA_MODULE_DUALIZE
	if (FIMC_IS_MAX_FW_SIZE >= fsize) {
		memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
		temp_buf = fw_buf;
	} else
#endif
	{
		info("Phone FW size is larger than FW buffer. Use vmalloc.\n");
		read_buf = vmalloc(fsize);
		if (!read_buf) {
			err("failed to allocate memory");
			ret = -ENOMEM;
			goto read_phone_fw_exit;
		}
		temp_buf = read_buf;
	}
	nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		err("failed to read firmware file, %ld Bytes", nread);
		ret = -EIO;
		goto read_phone_fw_exit;
	}

	strncpy(phone_fw_version, temp_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
	strncpy(sysfs_pinfo.header_ver, temp_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
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

	set_fs(old_fs);

exit:
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		if (is_ldo_enabled[0] && !core->running_front_camera)
			fimc_is_sec_rom_power_off(core, SENSOR_POSITION_FRONT);

		if (is_ldo_enabled[1] && !core->running_rear_camera)
			fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);
	} else
#endif
	{
		if (is_ldo_enabled[0] && !core->running_rear_camera)
			fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);
	}

	mutex_unlock(&core->spi_lock);

	return ret;
}
#endif

#if defined(CONFIG_CAMERA_FROM_SUPPORT_REAR)
int fimc_is_sec_fw_sel(struct fimc_is_core *core, struct device *dev, bool headerOnly)
{
	int ret = 0;
	char fw_path[100];
	char dump_fw_path[100];
	char dump_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
	char phone_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
#ifdef CAMERA_MODULE_DUALIZE
	int from_fw_revision = 0;
	int dump_fw_revision = 0;
	int phone_fw_revision = 0;
	bool dump_flag = false;
#endif

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
	bool is_dump_existed = false;
	bool is_dump_needed = false;
#ifdef CONFIG_COMPANION_USE
	struct fimc_is_spi_gpio *spi_gpio = &core->spi0.gpio;
#endif
	bool is_ldo_enabled = false;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null\n");
		return -EINVAL;
	}

	/* Use mutex for spi read */
	mutex_lock(&core->spi_lock);
	if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
		is_dumped_fw_loading_needed = false;
		if (force_caldata_dump)
			info("forced caldata dump!!\n");

		if (!fimc_is_sec_ldo_enabled(dev, "VDDIO_1.8V_CAM")) {
			fimc_is_sec_rom_power_on(core, SENSOR_POSITION_REAR);
			is_ldo_enabled = true;
		}
		info("read cal data from FROM\n");
#ifdef CONFIG_COMPANION_USE
		fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_FUNC, false);
#endif

		if (headerOnly) {
			fimc_is_sec_read_from_header(dev);
		} else {
			if (!fimc_is_sec_readcal(core)) {
				sysfs_finfo.is_caldata_read = true;
			}
		}
#ifdef CONFIG_COMPANION_USE
		fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_OUTPUT, true);
#endif
		/*select AF actuator*/
		if (!crc32_header_check) {
			info("Camera : CRC32 error for all section.\n");
		}

		fimc_is_sec_fw_find(core);
		if (headerOnly) {
			goto exit;
		}

		snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_PATH, sysfs_finfo.load_fw_name);

		snprintf(dump_fw_path, sizeof(dump_fw_path), "%s%s",
			FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_fw_name);
		info("Camera: f-rom fw version: %s\n", sysfs_finfo.header_ver);

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		fp = filp_open(dump_fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			info("Camera: There is no dumped firmware\n");
			is_dump_existed = false;
			goto read_phone_fw;
		} else {
			is_dump_existed = true;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n",
			dump_fw_path, fsize);

#ifdef CAMERA_MODULE_DUALIZE
		if (FIMC_IS_MAX_FW_SIZE >= fsize) {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
			temp_buf = fw_buf;
		} else
#endif
		{
			info("Dumped FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw;
			}
			temp_buf = read_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw;
		}

		strncpy(dump_fw_version, temp_buf + nread-11, FIMC_IS_HEADER_VER_SIZE);
		info("Camera: dumped fw version: %s\n", dump_fw_version);

read_phone_fw:
		if (read_buf) {
			vfree(read_buf);
			read_buf = NULL;
			temp_buf = NULL;
		}

		if (fp && is_dump_existed) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			err("Camera: Failed open phone firmware");
			fp = NULL;
			goto read_phone_fw_exit;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n", fw_path, fsize);

#ifdef CAMERA_MODULE_DUALIZE
		if (FIMC_IS_MAX_FW_SIZE >= fsize) {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
			temp_buf = fw_buf;
		} else
#endif
		{
			info("Phone FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw_exit;
			}
			temp_buf = read_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw_exit;
		}

		strncpy(phone_fw_version, temp_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
		strncpy(sysfs_pinfo.header_ver, temp_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
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

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

#ifdef CAMERA_MODULE_DUALIZE
		from_fw_revision = fimc_is_sec_fw_revision(sysfs_finfo.header_ver);
		phone_fw_revision = fimc_is_sec_fw_revision(phone_fw_version);
		if (is_dump_existed) {
			dump_fw_revision = fimc_is_sec_fw_revision(dump_fw_version);
		}

		info("from_fw_revision = %d, phone_fw_revision = %d, dump_fw_revision = %d\n",
			from_fw_revision, phone_fw_revision, dump_fw_revision);

		if ((!fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, phone_fw_version)) ||
			(from_fw_revision > phone_fw_revision)) {
			is_dumped_fw_loading_needed = true;
			if (is_dump_existed) {
				if (!fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver,
							dump_fw_version)) {
					is_dump_needed = true;
				} else if (from_fw_revision > dump_fw_revision) {
					is_dump_needed = true;
				} else {
					is_dump_needed = false;
				}
			} else {
				is_dump_needed = true;
			}
		} else {
			is_dump_needed = false;
			if (is_dump_existed) {
				if (!fimc_is_sec_fw_module_compare(phone_fw_version,
					dump_fw_version)) {
					is_dumped_fw_loading_needed = false;
				} else if (phone_fw_revision > dump_fw_revision) {
					is_dumped_fw_loading_needed = false;
				} else {
					is_dumped_fw_loading_needed = true;
				}
			} else {
				is_dumped_fw_loading_needed = false;
			}
		}

		if (force_caldata_dump) {
			if ((!fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, phone_fw_version))
				|| (from_fw_revision > phone_fw_revision))
				dump_flag = true;
		} else {
			if (is_dump_needed)
				dump_flag = true;
		}

		if (dump_flag) {
			info("Dump ISP Firmware.\n");
#ifdef CONFIG_COMPANION_USE
			fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_FUNC, false);
#endif
			ret = fimc_is_sec_readfw(core);
			ret |= fimc_is_sec_read_setfile(core);
#ifdef CONFIG_COMPANION_USE
			fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_OUTPUT, true);
#endif
			if (ret < 0) {
				if (!crc32_fw_check || !crc32_setfile_check) {
					is_dumped_fw_loading_needed = false;
					err("Firmware CRC is not valid. Does not use dumped firmware.\n");
				}
			}
		}

		if (phone_fw_version[0] == 0) {
			strcpy(sysfs_pinfo.header_ver, "NULL");
		}
#endif

		if (is_dump_needed && is_dumped_fw_loading_needed) {
			strncpy(loaded_fw, sysfs_finfo.header_ver, FIMC_IS_HEADER_VER_SIZE);
		} else if (!is_dump_needed && is_dumped_fw_loading_needed) {
			strncpy(loaded_fw, dump_fw_version, FIMC_IS_HEADER_VER_SIZE);
		} else {
			strncpy(loaded_fw, phone_fw_version, FIMC_IS_HEADER_VER_SIZE);
		}

	} else {
		info("already loaded the firmware, Phone version=%s, F-ROM version=%s\n",
			sysfs_pinfo.header_ver, sysfs_finfo.header_ver);
	}

exit:
#ifdef CONFIG_COMPANION_USE
	fimc_is_spi_s_pin(spi_gpio, PINCFG_TYPE_FUNC, 0);
#endif
	if (is_ldo_enabled && !core->running_rear_camera)
		fimc_is_sec_rom_power_off(core, SENSOR_POSITION_REAR);

	mutex_unlock(&core->spi_lock);

	return ret;
}
#endif

#ifdef CONFIG_COMPANION_USE
int fimc_is_sec_concord_fw_sel(struct fimc_is_core *core, struct device *dev)
{
	int ret = 0;
	char c1_fw_path[100];
	char dump_c1_fw_path[100];
	char dump_c1_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
	char phone_c1_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
#ifdef CAMERA_MODULE_DUALIZE
	int from_c1_fw_revision = 0;
	int dump_c1_fw_revision = 0;
	int phone_c1_fw_revision = 0;
	struct fimc_is_spi_gpio *spi_gpio = &core->spi0.gpio;
#endif

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *read_buf = NULL;
	u8 *temp_buf = NULL;
	bool is_dump_existed = false;
	bool is_dump_needed = false;
	int sensor_id = 0;

	if ((!sysfs_finfo.is_c1_caldata_read &&
	    (cam_id == CAMERA_SINGLE_REAR /* || cam_id == CAMERA_DUAL_FRONT*/)) ||
	    force_caldata_dump) {
		is_dumped_c1_fw_loading_needed = false;
		if (force_caldata_dump)
			info("forced caldata dump!!\n");

		sysfs_finfo.is_c1_caldata_read = true;

		if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_F) ||
			fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_I) ||
			fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_A)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_EVT1);
			sysfs_finfo.sensor_id = COMPANION_SENSOR_2P2;
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2P2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2P2_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_IMX228)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX228_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX228_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX228_EVT1);
			sysfs_finfo.sensor_id = COMPANION_SENSOR_IMX240;
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX228_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX228_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX228_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX228_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_IMX240)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX240_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX240_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX240_EVT1);
			sysfs_finfo.sensor_id = COMPANION_SENSOR_IMX240;
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX240_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX240_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_12M)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_12M_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
			sysfs_finfo.sensor_id = COMPANION_SENSOR_2P2;
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2T2)) {
			snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
				FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2T2_EVT1);
			snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2T2_EVT1), "%s", FIMC_IS_FW_COMPANION_2T2_EVT1);
			sysfs_finfo.sensor_id = COMPANION_SENSOR_2T2;
			snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2T2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2T2_MASTER_SETF);
			snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2T2_MODE_SETF), "%s", FIMC_IS_COMPANION_2T2_MODE_SETF);
		} else {
			err("Companion FW selection failed! Default FW will be used");
			sensor_id = core->pdata->rear_sensor_id;
			if (sensor_id == SENSOR_NAME_IMX240) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX240_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX240_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX240_EVT1);
				sysfs_finfo.sensor_id = COMPANION_SENSOR_IMX240;
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX240_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX240_MODE_SETF);
			} else if (sensor_id == SENSOR_NAME_IMX228) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX228_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX228_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX228_EVT1);
				sysfs_finfo.sensor_id = COMPANION_SENSOR_IMX240; /* for bringup */
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX228_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX228_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX228_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX228_MODE_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K2P2_12M) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_12M_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
				sysfs_finfo.sensor_id = COMPANION_SENSOR_2P2;
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MODE_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K2P2) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_EVT1);
				sysfs_finfo.sensor_id = COMPANION_SENSOR_2P2;
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2P2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2P2_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_MODE_SETF);
			} else if (sensor_id == SENSOR_NAME_S5K2T2) {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2T2_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_2T2_EVT1), "%s", FIMC_IS_FW_COMPANION_2T2_EVT1);
				sysfs_finfo.sensor_id = COMPANION_SENSOR_2T2;
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_2T2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2T2_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_2T2_MODE_SETF), "%s", FIMC_IS_COMPANION_2T2_MODE_SETF);
			} else {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX240_EVT1);
				snprintf(sysfs_finfo.load_c1_fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX240_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX240_EVT1);
				sysfs_finfo.sensor_id = COMPANION_SENSOR_IMX240;
				snprintf(sysfs_finfo.load_c1_mastersetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX240_MASTER_SETF);
				snprintf(sysfs_finfo.load_c1_modesetf_name, sizeof(FIMC_IS_COMPANION_IMX240_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX240_MODE_SETF);
			}
		}

		snprintf(dump_c1_fw_path, sizeof(dump_c1_fw_path), "%s%s",
			FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_c1_fw_name);
		info("Camera: f-rom fw version: %s\n", sysfs_finfo.concord_header_ver);

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		ret = 0;
		fp = filp_open(dump_c1_fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			info("Camera: There is no dumped Companion firmware\n");
			is_dump_existed = false;
			goto read_phone_fw;
		} else {
			is_dump_existed = true;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n",
			dump_c1_fw_path, fsize);

#ifdef CAMERA_MODULE_DUALIZE
		if (FIMC_IS_MAX_FW_SIZE >= fsize) {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
			temp_buf = fw_buf;
		} else
#endif
		{
			info("Dumped Companion FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw;
			}
			temp_buf = read_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw;
		}

		strncpy(dump_c1_fw_version, temp_buf + nread - 16, FIMC_IS_HEADER_VER_SIZE);
		info("Camera: dumped companion fw version: %s\n", dump_c1_fw_version);

read_phone_fw:
		if (read_buf) {
			vfree(read_buf);
			read_buf = NULL;
			temp_buf = NULL;
		}

		if (fp && is_dump_existed) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);
		if (ret < 0)
			goto exit;

		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(c1_fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			err("Camera: Failed open phone companion firmware");
			fp = NULL;
			goto read_phone_fw_exit;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info("start, file path %s, size %ld Bytes\n",
			c1_fw_path, fsize);

#ifdef CAMERA_MODULE_DUALIZE
		if (FIMC_IS_MAX_FW_SIZE >= fsize) {
			memset(fw_buf, 0x0, FIMC_IS_MAX_FW_SIZE);
			temp_buf = fw_buf;
		} else
#endif
		{
			info("Phone Companion FW size is larger than FW buffer. Use vmalloc.\n");
			read_buf = vmalloc(fsize);
			if (!read_buf) {
				err("failed to allocate memory");
				ret = -ENOMEM;
				goto read_phone_fw_exit;
			}
			temp_buf = read_buf;
		}
		nread = vfs_read(fp, (char __user *)temp_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			err("failed to read companion firmware file, %ld Bytes", nread);
			ret = -EIO;
			goto read_phone_fw_exit;
		}

		strncpy(phone_c1_fw_version, temp_buf + nread - 16, FIMC_IS_HEADER_VER_SIZE);
		strncpy(sysfs_pinfo.concord_header_ver, temp_buf + nread - 16, FIMC_IS_HEADER_VER_SIZE);
		info("Camera: phone companion fw version: %s\n", phone_c1_fw_version);

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

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

#ifdef CAMERA_MODULE_DUALIZE
		from_c1_fw_revision = fimc_is_sec_fw_revision(sysfs_finfo.concord_header_ver);
		phone_c1_fw_revision = fimc_is_sec_fw_revision(phone_c1_fw_version);
		if (is_dump_existed) {
			dump_c1_fw_revision = fimc_is_sec_fw_revision(dump_c1_fw_version);
		}

		info("from_c1_fw_revision = %d, phone_c1_fw_revision = %d, dump_c1_fw_revision = %d\n",
			from_c1_fw_revision, phone_c1_fw_revision, dump_c1_fw_revision);

		if ((!fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, phone_c1_fw_version)) ||
				(from_c1_fw_revision > phone_c1_fw_revision)) {
			is_dumped_c1_fw_loading_needed = true;
			if (is_dump_existed) {
				if (!fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver,
							dump_c1_fw_version)) {
					is_dump_needed = true;
				} else if (from_c1_fw_revision > dump_c1_fw_revision) {
					is_dump_needed = true;
				} else {
					is_dump_needed = false;
				}
			} else {
				is_dump_needed = true;
			}
		} else {
			is_dump_needed = false;
			if (is_dump_existed) {
				if (!fimc_is_sec_fw_module_compare(phone_c1_fw_version,
							dump_c1_fw_version)) {
					is_dumped_c1_fw_loading_needed = false;
				} else if (phone_c1_fw_revision > dump_c1_fw_revision) {
					is_dumped_c1_fw_loading_needed = false;
				} else {
					is_dumped_c1_fw_loading_needed = true;
				}
			} else {
				is_dumped_c1_fw_loading_needed = false;
			}
		}

		if (is_dump_needed) {
			info("Dump companion Firmware.\n");
			fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_FUNC, false);
			ret = fimc_is_sec_read_companion_fw(core);
			fimc_is_spi_s_port(spi_gpio, FIMC_IS_SPI_OUTPUT, true);
			if (ret < 0) {
				if (!crc32_c1_fw_check) {
					is_dumped_c1_fw_loading_needed = false;
					err("Companion Firmware CRC is not valid. Does not use dumped firmware.\n");
				}
			}
		}

		if (phone_c1_fw_version[0] == 0) {
			strcpy(sysfs_pinfo.concord_header_ver, "NULL");
		}
#endif

		if (is_dump_needed && is_dumped_c1_fw_loading_needed) {
			strncpy(loaded_companion_fw, sysfs_finfo.concord_header_ver, FIMC_IS_HEADER_VER_SIZE);
		} else if (!is_dump_needed && is_dumped_c1_fw_loading_needed) {
			strncpy(loaded_companion_fw, dump_c1_fw_version, FIMC_IS_HEADER_VER_SIZE);
		} else {
			strncpy(loaded_companion_fw, phone_c1_fw_version, FIMC_IS_HEADER_VER_SIZE);
		}
	} else {
		info("already loaded the firmware, Phone_Comp version=%s, F-ROM_Comp version=%s\n",
			sysfs_pinfo.concord_header_ver, sysfs_finfo.concord_header_ver);
	}

exit:
	return ret;
}
#endif
