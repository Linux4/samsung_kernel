#include "fimc-is-sec-define.h"
#include <mach/pinctrl-samsung.h>

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
#include <linux/i2c.h>
#include "fimc-is-device-eeprom.h"
#endif

bool crc32_fw_check = true;
bool crc32_check = true;
bool crc32_check_factory = true;
bool crc32_header_check = true;
bool crc32_check_front = true;
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
#endif
u8 cal_map_version[FIMC_IS_CAL_MAP_VER_SIZE] = {0,};

#define FIMC_IS_DEFAULT_CAL_SIZE	(20 * 1024)
#define FIMC_IS_DUMP_CAL_SIZE	        (172 * 1024)
#define FIMC_IS_LATEST_FROM_VERSION_A	'A'
#define FIMC_IS_LATEST_FROM_VERSION_B	'B'
#define FIMC_IS_LATEST_FROM_VERSION_C	'C'
#define FIMC_IS_LATEST_FROM_VERSION_D	'D'
#define FIMC_IS_LATEST_FROM_VERSION_M	'M'

#ifndef CAMERA_MODULE_ES_VERSION
#define CAMERA_MODULE_ES_VERSION	FIMC_IS_LATEST_FROM_VERSION_B
#endif

#ifndef CAL_MAP_ES_VERSION
#define CAL_MAP_ES_VERSION		FROM_VERSION_V003
#endif


//static bool is_caldata_read = false;
//static bool is_c1_caldata_read = false;
static bool force_caldata_dump = false;
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
static struct fimc_is_from_info sysfs_pinfo_front;
static char cal_buf_front[FIMC_IS_MAX_CAL_SIZE_FRONT];
#endif
static struct fimc_is_from_info sysfs_finfo_front;
bool is_final_cam_module_front = false;
bool is_latest_cam_module_front = false;

static char cal_buf[FIMC_IS_MAX_CAL_SIZE];
char loaded_fw[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
char loaded_companion_fw[30] = {0, };

int fimc_is_sec_set_force_caldata_dump(bool fcd)
{
	force_caldata_dump = fcd;
	if (fcd)
		pr_info("forced caldata dump enabled!!\n");
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

int fimc_is_sec_get_sysfs_finfo_front(struct fimc_is_from_info **finfo)
{
	*finfo = &sysfs_finfo_front;
	return 0;
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
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

int fimc_is_sec_get_cal_buf(char **buf)
{
	*buf = &cal_buf[0];
	return 0;
}

int fimc_is_sec_get_loaded_fw(char **buf)
{
	*buf = &loaded_fw[0];
	return 0;
}

int fimc_is_sec_get_loaded_c1_fw(char **buf)
{
	*buf = &loaded_companion_fw[0];
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
	pr_err("%s: waring, you're calling the disabled func!\n\n", __func__);
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
		checksum_base = CHECKSUM_HEADER_ADDR_FRONT / 4;
		check_length = HEADER_CRC32_LEN_FRONT;
	} else
#endif
	{
		finfo = &sysfs_finfo;
		checksum_base = CHECKSUM_HEADER_ADDR / 4;
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

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
#if defined(EEP_HEADER_OEM_START_ADDR_FRONT)
		check_base = finfo->oem_start_addr / 4;
		checksum = 0;
		check_length = (finfo->oem_end_addr - finfo->oem_start_addr + 1);
		checksum_base = CHECKSUM_OEM_ADDR_FRONT / 4;

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
	} else
#endif
	{
		check_base = finfo->oem_start_addr / 4;
		checksum = 0;
		check_length = (finfo->oem_end_addr - finfo->oem_start_addr + 1);
		checksum_base = CHECKSUM_OEM_ADDR / 4;

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
	}

	/* AWB */
	check_base = finfo->awb_start_addr / 4;
	checksum = 0;
	check_length = (finfo->awb_end_addr - finfo->awb_start_addr + 1);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		checksum_base = CHECKSUM_AWB_ADDR_FRONT / 4;
	} else
#endif
	{
		checksum_base = CHECKSUM_AWB_ADDR / 4;
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
	check_length = (finfo->shading_end_addr - finfo->shading_start_addr + 1);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		checksum_base = CHECKSUM_SHADING_ADDR_FRONT / 4;
	} else
#endif
	{
		checksum_base = CHECKSUM_SHADING_ADDR / 4;
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

#ifdef CONFIG_COMPANION_USE
	/* pdaf cal */
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
#ifdef CONFIG_COMPANION_USE
		return crc32_check && crc32_c1_check;
#else
		return crc32_check;
#endif
	}
}

bool fimc_is_sec_check_fw_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;

	buf32 = (u32 *)buf;

	pr_info("Camera: Start checking CRC32 FW\n\n");

	crc32_fw_check = true;

	checksum = 0;

	checksum = getCRC((u16 *)&buf32[0],
						(sysfs_finfo.setfile_end_addr - sysfs_finfo.bin_start_addr + 1)/2, NULL, NULL);
	if (checksum != buf32[(0x3FFFFC - 0x80000)/4]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[(0x3FFFFC - 0x80000)/4]);
		crc32_fw_check = false;
	}

	pr_info("Camera: End checking CRC32 FW\n\n");

	return crc32_fw_check;
}

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
bool fimc_is_sec_check_front_otp_crc32(char *buf)
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

#ifdef CONFIG_COMPANION_USE
bool fimc_is_sec_check_companion_fw_crc32(char *buf)
{
	u32 *buf32 = NULL;
	u32 checksum;

	buf32 = (u32 *)buf;

	pr_info("Camera: Start checking CRC32 Companion FW\n\n");

	crc32_c1_fw_check = true;

	checksum = 0;

	checksum = getCRC((u16 *)&buf32[0], (sysfs_finfo.concord_mode_setfile_end_addr - sysfs_finfo.concord_bin_start_addr + 1)/2, NULL, NULL);
	if (checksum != buf32[(0x6AFFC - 0x2B000)/4]) {
		err("Camera: CRC32 error at the binary section (0x%08X != 0x%08X)",
					checksum, buf32[(0x6AFFC - 0x2B000)/4]);
		crc32_c1_fw_check = false;
	}

	pr_info("Camera: End checking CRC32 Companion FW\n\n");

	return crc32_c1_fw_check;
}
#endif

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos)
{
	struct file *fp;
	mm_segment_t old_fs;
	ssize_t tx;
	int fd, old_mask;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	old_mask = sys_umask(0);

	fd = sys_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0664);
	if (fd < 0) {
		err("open file error!!\n");
		sys_umask(old_mask);
		set_fs(old_fs);
		return -EINVAL;
	}

	fp = fget(fd);
	if (fp) {
		tx = vfs_write(fp, buf, count, pos);
		vfs_fsync(fp, 0);
		fput(fp);
	}

	sys_close(fd);
	sys_umask(old_mask);
	set_fs(old_fs);

	return count;
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
			pr_info("%s: file(%s) not exist!\n", __func__, name);
		else
			pr_info("%s: error %d, failed to open %s\n", __func__, fd, name);

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

	printk(KERN_ERR "%s\n\n\n\n", __func__);


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

	pr_info("Manufacturer ID(0x40): 0x%02x\n", cal_data[0x40]);
	pr_info("Pixel Number(0x41): 0x%02x\n", cal_data[0x41]);


	pr_info("Manufacturer ID(0x4FE7): 0x%02x\n", cal_data[0x4FE7]);
	pr_info("Pixel Number(0x4FE8): 0x%02x\n", cal_data[0x4FE8]);
	pr_info("Manufacturer ID(0x4FE9): 0x%02x\n", cal_data[0x4FE9]);
	pr_info("Pixel Number(0x4FEA): 0x%02x\n", cal_data[0x4FEA]);

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
	printk(KERN_INFO "%s\n\n\n\n", __func__);
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

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
int fimc_is_i2c_read(struct i2c_client *client, void *buf, u32 addr, size_t size)
{
	const u32 addr_size = 2, max_retry = 5;
	u8 addr_buf[addr_size];
	int retries = max_retry;
	int ret = 0;

	if (!client) {
		pr_info("%s: client is null\n", __func__);
		return -ENODEV;
	}

	/* pr_info("%s %s: fimc_is_i2c_read\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));*/

	/* Send addr */
	addr_buf[0] = ((u16)addr) >> 8;
	addr_buf[1] = (u8)addr;

	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_send(client, addr_buf, addr_size);
		if (likely(addr_size == ret))
			break;

		pr_info("%s: i2c_master_send failed(%d), try %d\n", __func__, ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		pr_err("%s: error %d, fail to write 0x%04X\n", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	/* Receive data */
	for (retries = max_retry; retries > 0; retries--) {
		ret = i2c_master_recv(client, buf, size);
		if (likely(ret == size))
			break;

		pr_info("%s: i2c_master_recv failed(%d), try %d\n", __func__,  ret, retries);
		usleep_range(1000, 1000);
	}

	if (unlikely(ret <= 0)) {
		pr_err("%s: error %d, fail to read 0x%04X\n", __func__, ret, addr);
		return ret ? ret : -ETIMEDOUT;
	}

	return 0;
}

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

int fimc_is_i2c_config(struct i2c_client *client, bool onoff)
{
	struct fimc_is_device_eeprom *eeprom_device;
	struct fimc_is_eeprom_gpio *gpio;

	if (!client) {
		pr_info("%s: client is null\n", __func__);
		return -ENODEV;
	}

	eeprom_device = i2c_get_clientdata(client);
	gpio = &eeprom_device->gpio;
	info("(%s):onoff(%d) use_i2c_pinctrl(%d)\n", __func__, onoff, gpio->use_i2c_pinctrl);

	if (gpio->use_i2c_pinctrl) {
		if (onoff) {
			pin_config_set(gpio->pinname, gpio->scl,
				PINCFG_PACK(PINCFG_TYPE_FUNC, gpio->pinfunc_on));
			pin_config_set(gpio->pinname, gpio->sda,
				PINCFG_PACK(PINCFG_TYPE_FUNC, gpio->pinfunc_on));

		} else {
			pin_config_set(gpio->pinname, gpio->scl,
				PINCFG_PACK(PINCFG_TYPE_FUNC, gpio->pinfunc_off));
			pin_config_set(gpio->pinname, gpio->sda,
				PINCFG_PACK(PINCFG_TYPE_FUNC, gpio->pinfunc_off));
		}
	}

	return 0;
}

int fimc_is_sec_read_eeprom_header(struct device *dev)
{
	int ret = 0;
	struct fimc_is_core *core = dev_get_drvdata(dev);
	u8 header_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
	struct i2c_client *client;
	client = core->eeprom_client0;

	fimc_is_i2c_config(client, true);
	ret = fimc_is_i2c_read(client, header_version, EEP_HEADER_VERSION_START_ADDR, FIMC_IS_HEADER_VER_SIZE);
	if (unlikely(ret)) {
		err("failed to fimc_is_i2c_read for header version (%d)\n", ret);
		ret = -EINVAL;
	}
	fimc_is_i2c_config(client, false);
	memcpy(sysfs_finfo.header_ver, header_version, FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';

	return ret;
}

int fimc_is_sec_readcal_eeprom(struct device *dev, int position)
{
	int ret = 0;
	char *buf = NULL;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	struct fimc_is_core *core = dev_get_drvdata(dev);
	struct fimc_is_from_info *finfo = NULL;
	int cal_size = 0;
	struct i2c_client *client = NULL;
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		finfo = &sysfs_finfo_front;
		fimc_is_sec_get_front_cal_buf(&buf);
		cal_size = FIMC_IS_MAX_CAL_SIZE_FRONT;
		client = core->eeprom_client1;
#endif
	} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		finfo = &sysfs_finfo;
		fimc_is_sec_get_cal_buf(&buf);
		cal_size = FIMC_IS_MAX_CAL_SIZE;
		client = core->eeprom_client0;
#endif
	}

	fimc_is_i2c_config(client, true);

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		ret = fimc_is_i2c_read(client, finfo->cal_map_ver,
					EEP_HEADER_CAL_MAP_VER_START_ADDR_FRONT,
					FIMC_IS_CAL_MAP_VER_SIZE);
		ret = fimc_is_i2c_read(client, finfo->header_ver,
				       EEP_HEADER_VERSION_START_ADDR_FRONT,
				       FIMC_IS_HEADER_VER_SIZE);
#endif
	} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		ret = fimc_is_i2c_read(client, finfo->cal_map_ver,
					EEP_HEADER_CAL_MAP_VER_START_ADDR,
					FIMC_IS_CAL_MAP_VER_SIZE);
		ret = fimc_is_i2c_read(client, finfo->header_ver,
				       EEP_HEADER_VERSION_START_ADDR,
				       FIMC_IS_HEADER_VER_SIZE);
#endif
	}

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

crc_retry:
	/* read cal data */
	pr_info("Camera: I2C read cal data\n\n");
	fimc_is_i2c_read(client, buf, 0x0, cal_size);

	if (position == SENSOR_POSITION_FRONT) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
#if defined(EEP_HEADER_OEM_START_ADDR_FRONT)
		finfo->oem_start_addr = *((u32 *)&buf[EEP_HEADER_OEM_START_ADDR_FRONT]);
		finfo->oem_end_addr = *((u32 *)&buf[EEP_HEADER_OEM_END_ADDR_FRONT]);
		pr_info("OEM start = 0x%08x, end = 0x%08x\n",
				(finfo->oem_start_addr), (finfo->oem_end_addr));
#endif
		finfo->awb_start_addr = *((u32 *)&buf[EEP_HEADER_AWB_START_ADDR_FRONT]);
		finfo->awb_end_addr = *((u32 *)&buf[EEP_HEADER_AWB_END_ADDR_FRONT]);
		pr_info("AWB start = 0x%08x, end = 0x%08x\n",
				(finfo->awb_start_addr), (finfo->awb_end_addr));
		finfo->shading_start_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_START_ADDR_FRONT]);
		finfo->shading_end_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_END_ADDR_FRONT]);
		if (finfo->shading_end_addr > 0x1fff) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->setfile_end_addr = 0x1fff;
		}
		pr_info("Shading start = 0x%08x, end = 0x%08x\n",
			(finfo->shading_start_addr), (finfo->shading_end_addr));

		/* HEARDER Data : Module/Manufacturer Information */
		memcpy(finfo->header_ver, &buf[EEP_HEADER_VERSION_START_ADDR_FRONT], FIMC_IS_HEADER_VER_SIZE);
		finfo->header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
		/* HEARDER Data : Cal Map Version */
		memcpy(finfo->cal_map_ver, &buf[EEP_HEADER_CAL_MAP_VER_START_ADDR_FRONT], FIMC_IS_CAL_MAP_VER_SIZE);

		memcpy(finfo->project_name, &buf[EEP_HEADER_PROJECT_NAME_START_ADDR_FRONT], FIMC_IS_PROJECT_NAME_SIZE);
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
	} else {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		finfo->oem_start_addr = *((u32 *)&buf[EEP_HEADER_OEM_START_ADDR]);
		finfo->oem_end_addr = *((u32 *)&buf[EEP_HEADER_OEM_END_ADDR]);
		pr_info("OEM start = 0x%08x, end = 0x%08x\n",
				(finfo->oem_start_addr), (finfo->oem_end_addr));
		finfo->awb_start_addr = *((u32 *)&buf[EEP_HEADER_AWB_START_ADDR]);
		finfo->awb_end_addr = *((u32 *)&buf[EEP_HEADER_AWB_END_ADDR]);
		pr_info("AWB start = 0x%08x, end = 0x%08x\n",
				(finfo->awb_start_addr), (finfo->awb_end_addr));
		finfo->shading_start_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_START_ADDR]);
		finfo->shading_end_addr = *((u32 *)&buf[EEP_HEADER_AP_SHADING_END_ADDR]);
		if (finfo->shading_end_addr > 0x1fff) {
			err("Shading end_addr has error!! 0x%08x", finfo->shading_end_addr);
			finfo->setfile_end_addr = 0x1fff;
		}
		pr_info("Shading start = 0x%08x, end = 0x%08x\n",
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
#endif
	}

	/* debug info dump */
#if defined(EEPROM_DEBUG)
	pr_info("++++ EEPROM data info\n");
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
	pr_info("project_name : %s\n", finfo->project_name);
	pr_info("Cal data map ver : %s\n", finfo->cal_map_ver);
	pr_info("2. OEM info\n");
	pr_info("Module info : %s\n", finfo->oem_ver);
	pr_info("3. AWB info\n");
	pr_info("Module info : %s\n", finfo->awb_ver);
	pr_info("4. Shading info\n");
	pr_info("Module info : %s\n", finfo->shading_ver);
	pr_info("---- EEPROM data info\n");
#endif

	/* CRC check */
	if (!fimc_is_sec_check_cal_crc32(buf, position) && (retry > 0)) {
		retry--;
		goto crc_retry;
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (position == SENSOR_POSITION_FRONT) {
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory_front = crc32_check_front;
		} else {
			crc32_check_factory_front = false;
		}

		if (!core->use_module_check) {
			is_latest_cam_module_front = true;
		} else {
			if (finfo->header_ver[10] >= CAMERA_MODULE_ES_VERSION) {
				is_latest_cam_module_front = true;
			} else {
				is_latest_cam_module_front = false;
			}
		}

		if (core->use_module_check) {
			if (finfo->header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
				is_final_cam_module_front = true;
			} else {
				is_final_cam_module_front = false;
			}
		} else {
			is_final_cam_module_front = true;
		}
	} else
#endif
	{
		if (finfo->header_ver[3] == 'L') {
			crc32_check_factory = crc32_check;
		} else {
			crc32_check_factory = false;
		}


		if (!core->use_module_check) {
			is_latest_cam_module = true;
		} else {
			if (sysfs_finfo.header_ver[10] >= CAMERA_MODULE_ES_VERSION) {
				is_latest_cam_module = true;
			} else {
				is_latest_cam_module = false;
			}
		}

		if (!core->use_module_check) {
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
			pr_info("dump folder exist, Dump EEPROM cal data.\n");
			if (position == SENSOR_POSITION_FRONT) {
				if (write_data_to_file(FIMC_IS_CAL_DUMP_FRONT, buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
					pr_info("Failed to dump cal data.\n");
					goto dump_err;
				}
			} else {
				if (write_data_to_file(FIMC_IS_CAL_DUMP, buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
					pr_info("Failed to dump cal data.\n");
					goto dump_err;
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
exit:
	fimc_is_i2c_config(client, false);

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
	fimc_is_i2c_read(client, finfo->cal_map_ver, 0xA22, FIMC_IS_CAL_MAP_VER_SIZE);
	fimc_is_i2c_read(client, finfo->header_ver, 0xA15, FIMC_IS_HEADER_VER_SIZE);

	fimc_is_i2c_write(client, 0xA00, 0x04);
	fimc_is_i2c_write(client, 0xA00, 0x00);

	if (!fimc_is_sec_check_from_ver(core, position)) {
		info("Camera: Do not read otp cal data. EEPROM version is low.\n");
		return 0;
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
	if (!fimc_is_sec_check_front_otp_crc32(buf) && (retry > 0)) {
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
			pr_info("dump folder exist, Dump EEPROM cal data.\n");
			if (position == SENSOR_POSITION_FRONT) {
				if (write_data_to_file(FIMC_IS_CAL_DUMP_FRONT, buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
					pr_info("Failed to dump cal data.\n");
					goto dump_err;
				}
			} else {
				if (write_data_to_file(FIMC_IS_CAL_DUMP, buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
					pr_info("Failed to dump cal data.\n");
					goto dump_err;
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
exit:
	fimc_is_i2c_config(client, false);

	return ret;
}
#endif
#endif

#if !defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
extern int fimc_is_spi_reset_by_core(struct spi_device *spi, void *buf, u32 rx_addr, size_t size);
extern int fimc_is_spi_read_by_core(struct spi_device *spi, void *buf, u32 rx_addr, size_t size);
extern int fimc_is_spi_read_module_id(struct spi_device *spi, void *buf, u16 rx_addr, size_t size);

int fimc_is_sec_read_from_header(struct device *dev)
{
	int ret = 0;
	struct fimc_is_core *core = dev_get_drvdata(dev);
	u8 header_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };

	ret = fimc_is_spi_read_by_core(core->spi0, header_version, FROM_HEADER_VERSION_START_ADDR, FIMC_IS_HEADER_VER_SIZE);
	if (ret < 0) {
		printk(KERN_ERR "failed to fimc_is_spi_read for header version (%d)\n", ret);
		ret = -EINVAL;
	}

	memcpy(sysfs_finfo.header_ver, header_version, FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';

	return ret;
}

int fimc_is_sec_readcal(struct fimc_is_core *core)
{
	int ret = 0;
	int retry = FIMC_IS_CAL_RETRY_CNT;
	char spi_buf[0x50] = {0, };
	struct file *key_fp = NULL;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;
	u16 id;

	/* reset spi */
	if (!core->spi0) {
		pr_err("spi0 device is not available");
		goto exit;
	}

	ret = fimc_is_spi_reset_by_core(core->spi0, spi_buf, 0x0, FIMC_IS_MAX_CAL_SIZE);
	if (ret < 0) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	ret = fimc_is_spi_read_module_id(core->spi0, &id, FROM_HEADER_MODULE_ID_START_ADDR, FROM_HEADER_MODULE_ID_SIZE);
	pr_info("Camera: FROM Module ID = 0x%04x\n", id);

	ret = fimc_is_spi_read_by_core(core->spi0, cal_map_version,
		FROM_HEADER_CAL_MAP_VER_START_ADDR, FIMC_IS_CAL_MAP_VER_SIZE);
	if (ret < 0) {
		printk(KERN_ERR "failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	pr_info("Camera: Cal map_version = %c%c%c%c\n", cal_map_version[0],
			cal_map_version[1], cal_map_version[2], cal_map_version[3]);
crc_retry:
	/* read cal data */
	pr_info("Camera: SPI read cal data\n\n");
	ret = fimc_is_spi_read_by_core(core->spi0, cal_buf, 0x0, FIMC_IS_MAX_CAL_SIZE);
	if (ret < 0) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

	sysfs_finfo.bin_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_START_ADDR]);
	sysfs_finfo.bin_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_BINARY_END_ADDR]);
	pr_info("Binary start = 0x%08x, end = 0x%08x\n",
			(sysfs_finfo.bin_start_addr), (sysfs_finfo.bin_end_addr));
	sysfs_finfo.oem_start_addr = *((u32 *)&cal_buf[FROM_HEADER_OEM_START_ADDR]);
#if defined(CONFIG_CAMERA_A7)
	sysfs_finfo.oem_end_addr = *((u32 *)&cal_buf[FROM_HEADER_OEM_END_ADDR]);
#else
	sysfs_finfo.oem_end_addr = FROM_HEADER_OEM_END;
#endif
	pr_info("OEM start = 0x%08x, end = 0x%08x\n",
			(sysfs_finfo.oem_start_addr), (sysfs_finfo.oem_end_addr));
	sysfs_finfo.awb_start_addr = *((u32 *)&cal_buf[FROM_HEADER_AWB_START_ADDR]);
	sysfs_finfo.awb_end_addr = *((u32 *)&cal_buf[FROM_HEADER_AWB_END_ADDR]);
	pr_info("AWB start = 0x%08x, end = 0x%08x\n",
			(sysfs_finfo.awb_start_addr), (sysfs_finfo.awb_end_addr));
	sysfs_finfo.shading_start_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_START_ADDR]);
	sysfs_finfo.shading_end_addr = *((u32 *)&cal_buf[FROM_HEADER_SHADING_END_ADDR]);
	pr_info("Shading start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.shading_start_addr), (sysfs_finfo.shading_end_addr));
#if !defined(CONFIG_CAMERA_A7)
	sysfs_finfo.setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_START_ADDR]);
	sysfs_finfo.setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_ISP_SETFILE_END_ADDR]);
	/*if (sysfs_finfo.setfile_start_addr == 0xffffffff) {
		sysfs_finfo.setfile_start_addr = *((u32 *)&cal_buf[40]);
		sysfs_finfo.setfile_end_addr = *((u32 *)&cal_buf[44]);
	}*/
#endif
#ifdef CONFIG_COMPANION_USE
	sysfs_finfo.concord_cal_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_CAL_START_ADDR]);
	sysfs_finfo.concord_cal_end_addr = FROM_HEADER_CONCORD_CAL_END;
	pr_info("concord cal start = 0x%08x, end = 0x%08x\n",
		sysfs_finfo.concord_cal_start_addr, sysfs_finfo.concord_cal_end_addr);
	sysfs_finfo.concord_bin_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_BINARY_START_ADDR]);
	sysfs_finfo.concord_bin_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_BINARY_END_ADDR]);
	pr_info("concord bin start = 0x%08x, end = 0x%08x\n",
		sysfs_finfo.concord_bin_start_addr, sysfs_finfo.concord_bin_end_addr);
	sysfs_finfo.concord_master_setfile_start_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MASTER_SETFILE_START_ADDR]);
	sysfs_finfo.concord_master_setfile_end_addr = sysfs_finfo.concord_master_setfile_start_addr + 16064;
	sysfs_finfo.concord_mode_setfile_start_addr = sysfs_finfo.concord_master_setfile_end_addr + 1;
	sysfs_finfo.concord_mode_setfile_end_addr = *((u32 *)&cal_buf[FROM_HEADER_CONCORD_MODE_SETFILE_END_ADDR]);
	sysfs_finfo.pdaf_cal_start_addr = FROM_HEADER_PDAF_CAL_START;

	if (fimc_is_comp_is_compare_ver(core) >= FROM_VERSION_V005)
		sysfs_finfo.pdaf_cal_end_addr = FROM_HEADER_PDAF_CAL_END_V005;
	else
		sysfs_finfo.pdaf_cal_end_addr = FROM_HEADER_PDAF_CAL_END_V000;
	pr_info("pdaf start = 0x%08x, end = 0x%08x\n",
		sysfs_finfo.pdaf_cal_start_addr, sysfs_finfo.pdaf_cal_end_addr);

	sysfs_finfo.lsc_i0_gain_addr = FROM_SHADING_LSC_I0_GAIN_ADDR;
	pr_info("Shading lsc_i0 start = 0x%08x\n", sysfs_finfo.lsc_i0_gain_addr);
	sysfs_finfo.lsc_j0_gain_addr = FROM_SHADING_LSC_J0_GAIN_ADDR;
	pr_info("Shading lsc_j0 start = 0x%08x\n", sysfs_finfo.lsc_j0_gain_addr);
	sysfs_finfo.lsc_a_gain_addr = FROM_SHADING_LSC_A_GAIN_ADDR;
	pr_info("Shading lsc_a start = 0x%08x\n", sysfs_finfo.lsc_a_gain_addr);
	sysfs_finfo.lsc_k4_gain_addr = FROM_SHADING_LSC_K4_GAIN_ADDR;
	pr_info("Shading lsc_k4 start = 0x%08x\n", sysfs_finfo.lsc_k4_gain_addr);
	sysfs_finfo.lsc_scale_gain_addr = FROM_SHADING_LSC_SCALE_GAIN_ADDR;
	pr_info("Shading lsc_scale start = 0x%08x\n", sysfs_finfo.lsc_scale_gain_addr);

	sysfs_finfo.lsc_gain_start_addr = FROM_SHADING_LSC_GAIN_START_ADDR;
	sysfs_finfo.lsc_gain_end_addr = FROM_SHADING_LSC_GAIN_END_ADDR;
	pr_info("LSC start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.lsc_gain_start_addr, sysfs_finfo.lsc_gain_end_addr);
	sysfs_finfo.pdaf_start_addr = FROM_CONCORD_CAL_PDAF_START_ADDR;
	sysfs_finfo.pdaf_end_addr = FROM_CONCORD_CAL_PDAF_END_ADDR;
	pr_info("pdaf_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.pdaf_start_addr, sysfs_finfo.pdaf_end_addr);
	/*sysfs_finfo.coefficient_cal_start_addr = sysfs_finfo.pdaf_start_addr + 512 + 16;
	sysfs_finfo.coefficient_cal_end_addr = sysfs_finfo.coefficient_cal_start_addr + 24576 -1;
	pr_info("coefficient_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coefficient_cal_start_addr, sysfs_finfo.coefficient_cal_end_addr);*/
	sysfs_finfo.coef1_start = FROM_CONCORD_XTALK_10_START_ADDR;
	sysfs_finfo.coef1_end = FROM_CONCORD_XTALK_10_END_ADDR;
	pr_info("coefficient1_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef1_start, sysfs_finfo.coef1_end);
	sysfs_finfo.coef2_start = FROM_CONCORD_XTALK_20_START_ADDR;
	sysfs_finfo.coef2_end = FROM_CONCORD_XTALK_20_END_ADDR;
	pr_info("coefficient2_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef2_start, sysfs_finfo.coef2_end);
	sysfs_finfo.coef3_start = FROM_CONCORD_XTALK_30_START_ADDR;
	sysfs_finfo.coef3_end = FROM_CONCORD_XTALK_30_END_ADDR;
	pr_info("coefficient3_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef3_start, sysfs_finfo.coef3_end);
	sysfs_finfo.coef4_start = FROM_CONCORD_XTALK_40_START_ADDR;
	sysfs_finfo.coef4_end = FROM_CONCORD_XTALK_40_END_ADDR;
	pr_info("coefficient4_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef4_start, sysfs_finfo.coef4_end);
	sysfs_finfo.coef5_start = FROM_CONCORD_XTALK_50_START_ADDR;
	sysfs_finfo.coef5_end = FROM_CONCORD_XTALK_50_END_ADDR;
	pr_info("coefficient5_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef5_start, sysfs_finfo.coef5_end);
	sysfs_finfo.coef6_start = FROM_CONCORD_XTALK_60_START_ADDR;
	sysfs_finfo.coef6_end = FROM_CONCORD_XTALK_60_END_ADDR;
	pr_info("coefficient6_cal_addr start = 0x%04x, end = 0x%04x\n",
		sysfs_finfo.coef6_start, sysfs_finfo.coef6_end);
	sysfs_finfo.wcoefficient1_addr = FROM_CONCORD_WCOEF_ADDR;
	pr_info("Shading wcoefficient1 start = 0x%04x\n", sysfs_finfo.wcoefficient1_addr);
	memcpy(sysfs_finfo.concord_header_ver, &cal_buf[FROM_HEADER_CONCORD_HEADER_VER_START_ADDR], FIMC_IS_HEADER_VER_SIZE);
	sysfs_finfo.concord_header_ver[FIMC_IS_HEADER_VER_SIZE] = '\0';
	sysfs_finfo.af_inf_addr = FROM_OEM_AF_INF_ADDR;
	sysfs_finfo.af_macro_addr = FROM_OEM_AF_MACRO_ADDR;
	sysfs_finfo.lsc_gain_crc_addr = FROM_SHADING_LSC_GAIN_CRC_ADDR;
	sysfs_finfo.pdaf_crc_addr= FROM_CONCORD_PDAF_CRC_ADDR;
	sysfs_finfo.coef1_crc_addr= FROM_CONCORD_XTALK_10_CHECKSUM_ADDR;
	sysfs_finfo.coef2_crc_addr = FROM_CONCORD_XTALK_20_CHECKSUM_ADDR;
	sysfs_finfo.coef3_crc_addr = FROM_CONCORD_XTALK_30_CHECKSUM_ADDR;
	sysfs_finfo.coef4_crc_addr = FROM_CONCORD_XTALK_40_CHECKSUM_ADDR;
	sysfs_finfo.coef5_crc_addr = FROM_CONCORD_XTALK_50_CHECKSUM_ADDR;
	sysfs_finfo.coef6_crc_addr = FROM_CONCORD_XTALK_60_CHECKSUM_ADDR;
#endif
#if !defined(CONFIG_CAMERA_A7)
	if (sysfs_finfo.setfile_end_addr < FROM_ISP_BINARY_SETFILE_START_ADDR
	 || sysfs_finfo.setfile_end_addr > FROM_ISP_BINARY_SETFILE_END_ADDR) {
		err("setfile end_addr has error!!  0x%08x", sysfs_finfo.setfile_end_addr);
		sysfs_finfo.setfile_end_addr = 0x1fffff;
	}

	pr_info("Setfile start = 0x%08x, end = 0x%08x\n",
		(sysfs_finfo.setfile_start_addr), (sysfs_finfo.setfile_end_addr));
#endif
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

	fw_core_version = sysfs_finfo.header_ver[FW_CORE_VER];
	/* debug info dump */
//#if defined(FROM_DEBUG)
#if 1
	pr_info("++++ FROM data info\n");
	pr_info("1. Header info\n");
	pr_info("Module info : %s\n", sysfs_finfo.header_ver);
#ifdef CONFIG_COMPANION_USE
	pr_info("Companion version info : %s\n", sysfs_finfo.concord_header_ver);
#endif
	pr_info(" ID : %c\n", sysfs_finfo.header_ver[FW_CORE_VER]);
	pr_info(" Pixel num : %c%c\n", sysfs_finfo.header_ver[FW_PIXEL_SIZE],
							sysfs_finfo.header_ver[FW_PIXEL_SIZE + 1]);
	pr_info(" ISP ID : %c\n", sysfs_finfo.header_ver[FW_ISP_COMPANY]);
	pr_info(" Sensor Maker : %c\n", sysfs_finfo.header_ver[FW_SENSOR_MAKER]);
	pr_info(" Year : %c\n", sysfs_finfo.header_ver[FW_PUB_YEAR]);
	pr_info(" Month : %c\n", sysfs_finfo.header_ver[FW_PUB_MON]);
	pr_info(" Release num : %c%c\n", sysfs_finfo.header_ver[FW_PUB_NUM],
							sysfs_finfo.header_ver[FW_PUB_NUM + 1]);
	pr_info(" Manufacturer ID : %c\n", sysfs_finfo.header_ver[FW_MODULE_COMPANY]);
	pr_info(" Module ver : %c\n", sysfs_finfo.header_ver[FW_VERSION_INFO]);
	pr_info("Cal data map ver : %s\n", sysfs_finfo.cal_map_ver);
	pr_info("Setfile ver : %s\n", sysfs_finfo.setfile_ver);
	pr_info("Project name : %s\n", sysfs_finfo.project_name);
	pr_info("2. OEM info\n");
	pr_info("Module info : %s\n", sysfs_finfo.oem_ver);
	pr_info("3. AWB info\n");
	pr_info("Module info : %s\n", sysfs_finfo.awb_ver);
	pr_info("4. Shading info\n");
	pr_info("Module info : %s\n", sysfs_finfo.shading_ver);
	pr_info("---- FROM data info\n");
#endif

	/* CRC check */
#ifdef CONFIG_COMPANION_USE
	if (fimc_is_comp_is_compare_ver(core) >= FROM_VERSION_V004) {
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
	if (fimc_is_comp_is_compare_ver(core) >= FROM_VERSION_V004) {
		if (crc32_check && crc32_c1_check) {
			/* If FROM LSC value is not valid, loading default lsc data */
			if (*((u32 *)&cal_buf[sysfs_finfo.lsc_gain_start_addr]) == 0x00000000) {
				companion_lsc_isvalid = false;
			} else {
				companion_lsc_isvalid = true;
			}
			if (*((u32 *)&cal_buf[sysfs_finfo.coef1_start]) == 0x00000000) {
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

	if (!core->use_module_check) {
		is_latest_cam_module = true;
	} else {
		if (sysfs_finfo.header_ver[10] >= CAMERA_MODULE_ES_VERSION) {
			is_latest_cam_module = true;
		} else {
			is_latest_cam_module = false;
		}
	}

	if (core->use_module_check) {
		if (sysfs_finfo.header_ver[10] == FIMC_IS_LATEST_FROM_VERSION_M) {
			is_final_cam_module = true;
		} else {
			is_final_cam_module = false;
		}
	} else {
		is_final_cam_module = true;
	}

#if defined(CONFIG_SOC_EXYNOS5433)
	if (sysfs_finfo.project_name[6] != 'T' && sysfs_finfo.header_ver[0] == 'H' && sysfs_finfo.header_ver[1] == '1' &&
		sysfs_finfo.header_ver[2] == '6') {
		pr_info("FROM has abnormal project name : %c-%c\n", sysfs_finfo.project_name[6], sysfs_finfo.project_name[7]);
		is_right_prj_name = false;
	} else {
		is_right_prj_name = true;
	}
#endif

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
			pr_info("dump folder exist, Dump FROM cal data.\n");
			if (write_data_to_file("/data/media/0/dump/from_cal.bin", cal_buf, FIMC_IS_DUMP_CAL_SIZE, &pos) < 0) {
				pr_info("Failedl to dump cal data.\n");
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

int fimc_is_sec_readfw(struct fimc_is_core *core)
{
	int ret = 0;
	char *buf = NULL;
	loff_t pos = 0;
	char fw_path[100];
	char setfile_path[100];
	char setf_name[50];
	int retry = FIMC_IS_FW_RETRY_CNT;
	int pixelSize;
#ifdef USE_ION_ALLOC
	struct ion_handle *handle = NULL;
#endif

	pr_info("Camera: FW, Setfile need to be dumped\n");
#ifdef USE_ION_ALLOC
	handle = ion_alloc(core->fimc_ion_client, (size_t)FIMC_IS_MAX_FW_SIZE, 0,
				EXYNOS_ION_HEAP_SYSTEM_MASK, 0);
	if (IS_ERR_OR_NULL(handle)) {
		err("(%s):failed to ioc_alloc\n",__func__);
		ret = -ENOMEM;
		goto exit;
	}

	buf = (char *)ion_map_kernel(core->fimc_ion_client, handle);
	if (IS_ERR_OR_NULL(buf)) {
		err("(%s)fail to ion_map_kernle\n",__func__);
		ret = -ENOMEM;
		goto exit;
	}
#else
	buf = vmalloc(FIMC_IS_MAX_FW_SIZE);
	if (!buf) {
		err("vmalloc fail");
		ret = -ENOMEM;
		goto exit;
	}
#endif
	crc_retry:

	/* read fw data */
	pr_info("Camera: Start SPI read fw data\n\n");
	ret = fimc_is_spi_read_by_core(core->spi0, buf, 0x80000, FIMC_IS_MAX_FW_SIZE);
	if (ret < 0) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	pr_info("Camera: End SPI read fw data\n\n");

	/* CRC check */
	if (!fimc_is_sec_check_fw_crc32(buf) && (retry > 0)) {
		retry--;
		goto crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_DUMP_PATH, FIMC_IS_FW);
	if (write_data_to_file(fw_path, buf,
					sysfs_finfo.bin_end_addr - sysfs_finfo.bin_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	pr_info("Camera: FW Data has dumped successfully\n");

	if (sysfs_finfo.header_ver[FW_SENSOR_MAKER] == FW_SENSOR_MAKER_SLSI) {
		if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2) ||
			fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2_G)) {
			snprintf(setf_name, sizeof(setf_name), "%s", FIMC_IS_3L2_SETF);
		} else {
			snprintf(setf_name, sizeof(setf_name), "%s", FIMC_IS_2P2_SETF);
		}
	} else if (sysfs_finfo.header_ver[FW_SENSOR_MAKER] == FW_SENSOR_MAKER_SONY) {
		pixelSize = fimc_is_sec_get_pixel_size(sysfs_finfo.header_ver);
		if (pixelSize == 13) {
			snprintf(setf_name, sizeof(setf_name), "%s", FIMC_IS_IMX135_SETF);
		} else if (pixelSize == 8) {
			snprintf(setf_name, sizeof(setf_name), "%s", FIMC_IS_IMX134_SETF);
		} else {
			snprintf(setf_name, sizeof(setf_name), "%s", FIMC_IS_IMX135_SETF);
		}
	} else {
		snprintf(setf_name, sizeof(setf_name), "%s", FIMC_IS_2P2_SETF);
	}

	snprintf(setfile_path, sizeof(setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, setf_name);
	pos = 0;

	if (write_data_to_file(setfile_path,
			buf+(sysfs_finfo.setfile_start_addr - sysfs_finfo.bin_start_addr),
			sysfs_finfo.setfile_end_addr - sysfs_finfo.setfile_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	pr_info("Camera: Setfile has dumped successfully\n");
	pr_info("Camera: FW, Setfile were dumped successfully\n");

exit:
#ifdef USE_ION_ALLOC
	if (!IS_ERR_OR_NULL(buf)) {
		ion_unmap_kernel(core->fimc_ion_client, handle);
	}

	if (!IS_ERR_OR_NULL(handle)) {
		ion_free(core->fimc_ion_client, handle);
	}
#else
	if (buf) {
		vfree(buf);
	}
#endif
	return ret;
}
#endif

#ifdef CONFIG_COMPANION_USE
int fimc_is_sec_read_companion_fw(struct fimc_is_core *core)
{
	int ret = 0;
	char *buf = NULL;
	loff_t pos = 0;
	char fw_path[100];
	char master_setfile_path[100];
	char mode_setfile_path[100];
	char master_setf_name[50];
	char mode_setf_name[50];
	int retry = FIMC_IS_FW_RETRY_CNT;
#ifdef USE_ION_ALLOC
	struct ion_handle *handle = NULL;
#endif

	pr_info("Camera: Companion FW, Setfile need to be dumped\n");
#ifdef USE_ION_ALLOC
	handle = ion_alloc(core->fimc_ion_client, (size_t)FIMC_IS_MAX_COMPANION_FW_SIZE, 0,
				EXYNOS_ION_HEAP_SYSTEM_MASK, 0);
	if (IS_ERR_OR_NULL(handle)) {
		err("(%s)failed to ioc_alloc\n",__func__);
		ret = -ENOMEM;
		goto exit;
	}

	buf = (char *)ion_map_kernel(core->fimc_ion_client, handle);
	if (IS_ERR_OR_NULL(buf)) {
		err("(%s)fail to ion_map_kernle\n",__func__);
		ret = -ENOMEM;
		goto exit;
	}
#else
	buf = vmalloc(FIMC_IS_MAX_COMPANION_FW_SIZE);
	if (!buf) {
		err("vmalloc fail");
		ret = -ENOMEM;
		goto exit;
	}
#endif
	crc_retry:

	/* read companion fw data */
	pr_info("Camera: Start SPI read companion fw data\n\n");
	ret = fimc_is_spi_read_by_core(core->spi0, buf, 0x2B000, FIMC_IS_MAX_COMPANION_FW_SIZE);
	if (ret < 0) {
		err("failed to fimc_is_spi_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}
	pr_info("Camera: End SPI read companion fw data\n\n");

	/* CRC check */
	if (!fimc_is_sec_check_companion_fw_crc32(buf) && (retry > 0)) {
		retry--;
		goto crc_retry;
	} else if (!retry) {
		ret = -EINVAL;
		goto exit;
	}

	if (sysfs_finfo.concord_header_ver[9] == 0) {
		snprintf(fw_path, sizeof(fw_path), "%s%s",
			FIMC_IS_FW_DUMP_PATH, FIMC_IS_FW_COMPANION_EVT0);
	} else if (sysfs_finfo.concord_header_ver[9] == 10) {
		snprintf(fw_path, sizeof(fw_path), "%s%s",
			FIMC_IS_FW_DUMP_PATH, FIMC_IS_FW_COMPANION_EVT1);
	}

	if (write_data_to_file(fw_path, buf,
					sysfs_finfo.concord_bin_end_addr - sysfs_finfo.concord_bin_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	pr_info("Camera: Companion FW Data has dumped successfully\n");

	if (sysfs_finfo.concord_header_ver[FW_SENSOR_MAKER] == FW_SENSOR_MAKER_SLSI) {
		snprintf(master_setf_name, sizeof(master_setf_name), "%s", FIMC_IS_COMPANION_MASTER_SETF);
		snprintf(mode_setf_name, sizeof(mode_setf_name), "%s", FIMC_IS_COMPANION_MODE_SETF);
	} else {
		snprintf(master_setf_name, sizeof(master_setf_name), "%s", FIMC_IS_COMPANION_MASTER_SETF);
		snprintf(mode_setf_name, sizeof(mode_setf_name), "%s", FIMC_IS_COMPANION_MODE_SETF);
	}

	snprintf(master_setfile_path, sizeof(master_setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, master_setf_name);
	snprintf(mode_setfile_path, sizeof(mode_setfile_path), "%s%s", FIMC_IS_FW_DUMP_PATH, mode_setf_name);
	pos = 0;

	if (write_data_to_file(master_setfile_path,
			buf + sysfs_finfo.concord_master_setfile_start_addr,
			sysfs_finfo.concord_master_setfile_end_addr - sysfs_finfo.concord_master_setfile_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}
	pos = 0;
	if (write_data_to_file(mode_setfile_path,
			buf + sysfs_finfo.concord_mode_setfile_start_addr,
			sysfs_finfo.concord_mode_setfile_end_addr - sysfs_finfo.concord_mode_setfile_start_addr + 1, &pos) < 0) {
		ret = -EIO;
		goto exit;
	}

	pr_info("Camera: Companion Setfile has dumped successfully\n");
	pr_info("Camera: Companion FW, Setfile were dumped successfully\n");

exit:
#ifdef USE_ION_ALLOC
	if (!IS_ERR_OR_NULL(buf)) {
		ion_unmap_kernel(core->fimc_ion_client, handle);
	}

	if (!IS_ERR_OR_NULL(handle)) {
		ion_free(core->fimc_ion_client, handle);
	}
#else
	if (buf) {
		vfree(buf);
	}
#endif

	return ret;
}
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
		pr_err("GPIO %s is not found!!\n", name);
		ret = -EINVAL;
		goto exit;
	}

	ret = gpio_request(gpio->pin, gpio->name);
	if (ret) {
		pr_err("Request GPIO error(%s)\n", gpio->name);
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
			pr_err("unknown act for gpio\n");
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
		pr_err("%s : regulator_get fail\n",
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
		pr_info("%s : set_core_voltage %d, %d successfully\n",
				__func__, minV, maxV);
	regulator_put(regulator);

	return ret;
}

/**
 * fimc_is_sec_ldo_enabled: check whether the ldo has already been enabled.
 *
 * @ return: true, false or error value
 */
static int fimc_is_sec_ldo_enabled(struct device *dev, char *name) {
	struct regulator *regulator = NULL;
	int enabled = 0;

	regulator = regulator_get(dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		pr_err("%s : regulator_get(%s) fail\n", __func__, name);
		return -EINVAL;
	}

	enabled = regulator_is_enabled(regulator);

	regulator_put(regulator);

	return enabled;
}

int fimc_is_sec_ldo_enable(struct device *dev, char *name, bool on)
{
	struct regulator *regulator = NULL;
	int ret = 0;

	regulator = regulator_get(dev, name);
	if (IS_ERR_OR_NULL(regulator)) {
		pr_err("%s : regulator_get(%s) fail\n", __func__, name);
		return -EINVAL;
	}

	if (on) {
		if (regulator_is_enabled(regulator)) {
			pr_warning("%s: regulator is already enabled\n", name);
			goto exit;
		}

		ret = regulator_enable(regulator);
		if (ret) {
			pr_err("%s : regulator_enable(%s) fail\n", __func__, name);
			goto exit;
		}
	} else {
		if (!regulator_is_enabled(regulator)) {
			pr_warning("%s: regulator is already disabled\n", name);
			goto exit;
		}

		ret = regulator_disable(regulator);
		if (ret) {
			pr_err("%s : regulator_disable(%s) fail\n", __func__, name);
			goto exit;
		}
	}

exit:
	if (regulator)
		regulator_put(regulator);

	return ret;
}

int fimc_is_sec_fw_find(struct fimc_is_core *core, char *fw_name, char *setf_name)
{
	int pixelSize = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct fimc_is_device_sensor *sensor_device = &core->sensor[0];

	BUG_ON(!sensor_device);
	BUG_ON(!sensor_device->pdata);
	BUG_ON(!sensor_device->pdata->sensor_id);

	pdata = sensor_device->pdata;

	if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_F) ||
		fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_I)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_2P2), "%s", FIMC_IS_FW_2P2);
		snprintf(setf_name, sizeof(FIMC_IS_2P2_SETF), "%s", FIMC_IS_2P2_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P2_12M)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_2P2_12M), "%s", FIMC_IS_FW_2P2_12M);
		snprintf(setf_name, sizeof(FIMC_IS_2P2_12M_SETF), "%s", FIMC_IS_2P2_12M_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_2P3)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_2P3), "%s", FIMC_IS_FW_2P3);
		snprintf(setf_name, sizeof(FIMC_IS_2P3_SETF), "%s", FIMC_IS_2P3_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_4H5_F) ||
		fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_4H5_H) ||
		fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_4H5_J) ||
		fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_4H5_K)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_4H5), "%s", FIMC_IS_FW_4H5);
		snprintf(setf_name, sizeof(FIMC_IS_4H5_SETF), "%s", FIMC_IS_4H5_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX240) ||
		fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_IMX240_Q)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_IMX240), "%s", FIMC_IS_FW_IMX240);
		snprintf(setf_name, sizeof(FIMC_IS_IMX240_SETF), "%s", FIMC_IS_IMX240_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2) ||
		fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3L2_G)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_3L2), "%s", FIMC_IS_FW_3L2);
		snprintf(setf_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
	} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.header_ver, FW_3P3)) {
		snprintf(fw_name, sizeof(FIMC_IS_FW_3P3), "%s", FIMC_IS_FW_3P3);
		snprintf(setf_name, sizeof(FIMC_IS_3P3_SETF), "%s", FIMC_IS_3P3_SETF);
	} else {
		/* Use the PixelSize information */
		pixelSize = fimc_is_sec_get_pixel_size(sysfs_finfo.header_ver);
		if (pixelSize == 16) {
			if (pdata->sensor_id == SENSOR_NAME_S5K2P3) {
				snprintf(fw_name, sizeof(FIMC_IS_FW_2P3), "%s", FIMC_IS_FW_2P3);
				snprintf(setf_name, sizeof(FIMC_IS_2P3_SETF), "%s", FIMC_IS_2P3_SETF);
			} else {
				snprintf(fw_name, sizeof(FIMC_IS_FW_2P2), "%s", FIMC_IS_FW_2P2);
				snprintf(setf_name, sizeof(FIMC_IS_2P2_SETF), "%s", FIMC_IS_2P2_SETF);
			}
		} else if (pixelSize == 13) {
			snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
			snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
		} else if (pixelSize == 12) {
			snprintf(fw_name, sizeof(FIMC_IS_FW_2P2_12M), "%s", FIMC_IS_FW_2P2_12M);
			snprintf(setf_name, sizeof(FIMC_IS_2P2_12M_SETF), "%s", FIMC_IS_2P2_12M_SETF);
		} else if (pixelSize == 8) {
			snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
			snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
		} else {
			/* default firmware and setfile */
			if ( pdata->sensor_id == SENSOR_NAME_IMX240 ) {
				/* IMX240 */
				snprintf(fw_name, sizeof(FIMC_IS_FW_IMX240), "%s", FIMC_IS_FW_IMX240);
				snprintf(setf_name, sizeof(FIMC_IS_IMX240_SETF), "%s", FIMC_IS_IMX240_SETF);
			} else if ( pdata->sensor_id == SENSOR_NAME_IMX134 ) {
				/* IMX134 */
				snprintf(fw_name, sizeof(FIMC_IS_FW_IMX134), "%s", FIMC_IS_FW_IMX134);
				snprintf(setf_name, sizeof(FIMC_IS_IMX134_SETF), "%s", FIMC_IS_IMX134_SETF);
			} else if ( pdata->sensor_id == SENSOR_NAME_S5K2P2_12M ) {
				/* 2P2_12M */
				snprintf(fw_name, sizeof(FIMC_IS_FW_2P2_12M), "%s", FIMC_IS_FW_2P2_12M);
				snprintf(setf_name, sizeof(FIMC_IS_2P2_12M_SETF), "%s", FIMC_IS_2P2_12M_SETF);
			} else if ( pdata->sensor_id == SENSOR_NAME_S5K2P2 ) {
				/* 2P2 */
				snprintf(fw_name, sizeof(FIMC_IS_FW_2P2), "%s", FIMC_IS_FW_2P2);
				snprintf(setf_name, sizeof(FIMC_IS_2P2_SETF), "%s", FIMC_IS_2P2_SETF);
			} else if ( pdata->sensor_id == SENSOR_NAME_S5K2P3 ) {
				/* 2P3 */
				snprintf(fw_name, sizeof(FIMC_IS_FW_2P3), "%s", FIMC_IS_FW_2P3);
				snprintf(setf_name, sizeof(FIMC_IS_2P3_SETF), "%s", FIMC_IS_2P3_SETF);
			} else if ( pdata->sensor_id == SENSOR_NAME_S5K4H5 ) {
				/* 4H5 */
				snprintf(fw_name, sizeof(FIMC_IS_FW_4H5), "%s", FIMC_IS_FW_4H5);
				snprintf(setf_name, sizeof(FIMC_IS_4H5_SETF), "%s", FIMC_IS_4H5_SETF);
			} else if ( pdata->sensor_id == SENSOR_NAME_S5K3L2) {
				/* 3L2 */
				snprintf(fw_name, sizeof(FIMC_IS_FW_3L2), "%s", FIMC_IS_FW_3L2);
				snprintf(setf_name, sizeof(FIMC_IS_3L2_SETF), "%s", FIMC_IS_3L2_SETF);
			} else if ( pdata->sensor_id == SENSOR_NAME_S5K3P3) {
				/* 3P3 */
				snprintf(fw_name, sizeof(FIMC_IS_FW_3P3), "%s", FIMC_IS_FW_3P3);
				snprintf(setf_name, sizeof(FIMC_IS_3P3_SETF), "%s", FIMC_IS_3P3_SETF);
			} else {
				snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
				snprintf(setf_name, sizeof(FIMC_IS_IMX135_SETF), "%s", FIMC_IS_IMX135_SETF);
			}
		}
	}

	strcpy(sysfs_finfo.load_fw_name, fw_name);
	strcpy(sysfs_finfo.load_setfile_name, setf_name);

	return 0;
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
int fimc_is_sec_fw_sel_eeprom(struct device *dev,
                char *fw_name, char *setf_name, int id, bool headerOnly)
{
	int ret = 0;
	char fw_path[100];
	char phone_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *fw_buf = NULL;
	bool is_ldo_enabled = false;
	struct platform_device *pdev = to_platform_device(dev);
	struct fimc_is_core *core = (struct fimc_is_core *)platform_get_drvdata(pdev);
	struct exynos_platform_fimc_is_sensor *pdata;
	struct fimc_is_device_sensor *sensor_device = &core->sensor[0];

	BUG_ON(!sensor_device);
	BUG_ON(!sensor_device->pdata);
	BUG_ON(!sensor_device->pdata->sensor_id);

	pdata = sensor_device->pdata;

	/* Use mutex for i2c read */
	mutex_lock(&core->spi_lock);

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		if (!sysfs_finfo_front.is_caldata_read || force_caldata_dump) {
			if (force_caldata_dump)
				pr_info("forced caldata dump!!\n");

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
			ret = pdata->gpio_cfg((&core->sensor[1])->pdev, pdata->scenario, GPIO_SCENARIO_ON);
			if (ret) {
				pr_err("%s : error, failed to pdata->gpio_cfg(on)", __func__);
				goto exit;
			}
			is_ldo_enabled = true;
#else
			if (!fimc_is_sec_ldo_enabled(dev, "VT_CAM_1.8V")) {
				ret = fimc_is_sec_ldo_enable(dev, "VT_CAM_1.8V", true);
				if (ret) {
					pr_err("%s : error, failed to cam_io(on)", __func__);
					goto exit;
				}
			}

#if defined(SENSOR_5E3_EEPROM_I2C_NEED_CAMIO_1_8V)
			if (!fimc_is_sec_ldo_enabled(dev, "CAM_IO_1.8V_AP")) {
				ret = fimc_is_sec_ldo_enable(dev, "CAM_IO_1.8V_AP", true);
				if (ret) {
					pr_err("%s : error, failed to cam_io(on)", __func__);
					goto exit;
				}
			}
#endif
			is_ldo_enabled = true;
#endif
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
			pr_info("Camera: read cal data from Front OTPROM\n");
			if (!fimc_is_sec_readcal_otprom(dev, SENSOR_POSITION_FRONT))
#else
			pr_info("Camera: read cal data from Front EEPROM\n");
			if (!fimc_is_sec_readcal_eeprom(dev, SENSOR_POSITION_FRONT))
#endif
			{
				sysfs_finfo_front.is_caldata_read = true;
			}
		}
		goto exit;
	} else
#endif
	{
		if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
			is_dumped_fw_loading_needed = false;
			if (force_caldata_dump)
				pr_info("forced caldata dump!!\n");

			if (!fimc_is_sec_ldo_enabled(dev, "CAM_IO_1.8V_AP")) {
				ret = fimc_is_sec_ldo_enable(dev, "CAM_IO_1.8V_AP", true);
				if (ret) {
					pr_err("%s : error, failed to cam_io(on)", __func__);
					goto exit;
				}
#ifdef CONFIG_CAMERA_ACTUATOR_DW9807
				if (fimc_is_sec_ldo_enable(dev, "CAM_AF_2.8V_AP", true))
					pr_err("%s : error, failed to cam_af(on)", __func__);
#endif

				is_ldo_enabled = true;
			}

			pr_info("Camera: read cal data from Rear EEPROM\n");

			if (headerOnly) {
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
				fimc_is_sec_read_eeprom_header(dev);
#endif
			} else {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR)
				if (!fimc_is_sec_readcal_otprom(dev, SENSOR_POSITION_REAR))
#else
				if (!fimc_is_sec_readcal_eeprom(dev, SENSOR_POSITION_REAR))
#endif
				{
					sysfs_finfo.is_caldata_read = true;
				}
			}
		}
	}

	fimc_is_sec_fw_find(core, fw_name, setf_name);
	if (headerOnly) {
		goto exit;
	}

	snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_PATH, fw_name);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("Camera: Failed open phone firmware\n");
		ret = -EIO;
		fp = NULL;
		goto read_phone_fw_exit;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	pr_info("start, file path %s, size %ld Bytes\n",
		fw_path, fsize);
	fw_buf = vmalloc(fsize);
	if (!fw_buf) {
		pr_err("failed to allocate memory\n");
		ret = -ENOMEM;
		goto read_phone_fw_exit;
	}
	nread = vfs_read(fp, (char __user *)fw_buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		pr_err("failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto read_phone_fw_exit;
	}

	strncpy(phone_fw_version, fw_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
	strncpy(sysfs_pinfo.header_ver, fw_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
	pr_info("Camera: phone fw version: %s\n", phone_fw_version);
read_phone_fw_exit:
	if (fw_buf) {
		vfree(fw_buf);
		fw_buf = NULL;
	}

	if (fp) {
		filp_close(fp, current->files);
		fp = NULL;
	}

	set_fs(old_fs);

exit:
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	if (id == SENSOR_POSITION_FRONT) {
		if (is_ldo_enabled && !core->running_front_camera) {
#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_REAR) || defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
			if (pdata->gpio_cfg((&core->sensor[1])->pdev, pdata->scenario, GPIO_SCENARIO_OFF))
				pr_err("%s : error, failed to pdata->gpio_cfg(off)", __func__);
#else
			if (fimc_is_sec_ldo_enable(dev, "VT_CAM_1.8V", false))
				pr_err("%s : error, failed to cam_io(off)", __func__);

#if defined(SENSOR_5E3_EEPROM_I2C_NEED_CAMIO_1_8V)
			if (fimc_is_sec_ldo_enable(dev, "CAM_IO_1.8V_AP", false))
				pr_err("%s : error, failed to cam_io(off)", __func__);
#endif
#endif
		}
	} else
#endif
	{
		if (is_ldo_enabled && !core->running_rear_camera) {
			if (fimc_is_sec_ldo_enable(dev, "CAM_IO_1.8V_AP", false))
				pr_err("%s : error, failed to cam_io(off)", __func__);
#ifdef CONFIG_CAMERA_ACTUATOR_DW9807
			if (fimc_is_sec_ldo_enable(dev, "CAM_AF_2.8V_AP", false))
				pr_err("%s : error, failed to cam_af(on)", __func__);
#endif
		}
	}

	mutex_unlock(&core->spi_lock);

	return ret;
}
#endif

#if !defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
int fimc_is_sec_fw_sel(struct fimc_is_core *core, struct device *dev,
	char *fw_name, char *setf_name, bool headerOnly)
{
	int ret = 0;
#if 1
	char fw_path[100];
	char dump_fw_path[100];
	char dump_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
	char phone_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
	int from_fw_revision = 0;
	int dump_fw_revision = 0;
	int phone_fw_revision = 0;

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *fw_buf = NULL;
	bool is_dump_existed = false;
	bool is_dump_needed = true;
#endif
	struct fimc_is_spi_gpio *spi_gpio = &core->spi_gpio;
	bool is_ldo_enabled = false;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct fimc_is_device_sensor *sensor_device = &core->sensor[0];

#ifdef CONFIG_CAMERA_ONLY_SUPPORT_FRONT
	/* This codes SHOULD BE REMOVED after bringing up rear camera */
	info("%s: not supported\n", __func__);
	snprintf(fw_name, sizeof(FIMC_IS_FW), "%s", FIMC_IS_FW);
	return 0;
#endif

	BUG_ON(!sensor_device);
	BUG_ON(!sensor_device->pdata);
	BUG_ON(!sensor_device->pdata->sensor_id);

	pdata = sensor_device->pdata;

	/* Use mutex for spi read */
	mutex_lock(&core->spi_lock);
	if (!sysfs_finfo.is_caldata_read || force_caldata_dump) {
		is_dumped_fw_loading_needed = false;
		if (force_caldata_dump)
			pr_info("forced caldata dump!!\n");
		if (!fimc_is_sec_ldo_enabled(dev, "CAM_IO_1.8V_AP")) {
			pr_info("enable %s in the %s\n", "CAM_IO_1.8V_AP", __func__);
			ret = fimc_is_sec_ldo_enable(dev, "CAM_IO_1.8V_AP", true);
			if (ret) {
				pr_err("fimc_is_sec_fw_sel: error, failed to cam_io(on)");
				goto exit;
			}

			is_ldo_enabled = true;
		}
		pr_info("read cal data from FROM\n");
#ifdef CONFIG_COMPANION_USE
		fimc_is_set_spi_config(spi_gpio, FIMC_IS_SPI_FUNC, false);
#else
		pin_config_set(spi_gpio->pinname, spi_gpio->spi_sclk,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
		pin_config_set(spi_gpio->pinname, spi_gpio->spi_miso,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
		pin_config_set(spi_gpio->pinname, spi_gpio->spi_mois,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 2));
		pin_config_set(spi_gpio->pinname, spi_gpio->spi_ssn,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 1));
		pin_config_set(spi_gpio->pinname, spi_gpio->spi_ssn,
				PINCFG_PACK(PINCFG_TYPE_DAT, 1));
#endif
		if (headerOnly) {
			fimc_is_sec_read_from_header(dev);
		} else {
			if (!fimc_is_sec_readcal(core)) {
				sysfs_finfo.is_caldata_read = true;
			}
		}

#ifdef CONFIG_COMPANION_USE
		fimc_is_set_spi_config(spi_gpio, FIMC_IS_SPI_OUTPUT, false);
#else
		pin_config_set(spi_gpio->pinname, spi_gpio->spi_sclk,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 1));
		pin_config_set(spi_gpio->pinname, spi_gpio->spi_ssn,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 0));
		pin_config_set(spi_gpio->pinname, spi_gpio->spi_ssn,
				PINCFG_PACK(PINCFG_TYPE_PUD, 1));
		pin_config_set(spi_gpio->pinname, spi_gpio->spi_ssn,
				PINCFG_PACK(PINCFG_TYPE_DRV, 0));
		pin_config_set(spi_gpio->pinname, spi_gpio->spi_miso,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 1));
		pin_config_set(spi_gpio->pinname, spi_gpio->spi_mois,
				PINCFG_PACK(PINCFG_TYPE_FUNC, 1));
#endif

		/*select AF actuator*/
		if (!crc32_header_check) {
			pr_info("Camera : CRC32 error for all section.\n");
			//ret = -EIO;
			//goto exit;
		}

		/*ret = fimc_is_sec_core_voltage_select(dev, sysfs_finfo.header_ver);
		if (ret < 0) {
			err("failed to fimc_is_sec_core_voltage_select (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}*/

		fimc_is_sec_fw_find(core, fw_name, setf_name);
		if (headerOnly) {
			goto exit;
		}

		snprintf(fw_path, sizeof(fw_path), "%s%s", FIMC_IS_FW_PATH, fw_name);

#if 1
		snprintf(dump_fw_path, sizeof(dump_fw_path), "%s%s",
			FIMC_IS_FW_DUMP_PATH, fw_name);
		pr_info("Camera: f-rom fw version: %s\n", sysfs_finfo.header_ver);

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		fp = filp_open(dump_fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			pr_info("Camera: There is no dumped firmware\n");
			is_dump_existed = false;
			goto read_phone_fw;
		} else {
			is_dump_existed = true;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		pr_info("start, file path %s, size %ld Bytes\n",
			dump_fw_path, fsize);
		fw_buf = vmalloc(fsize);
		if (!fw_buf) {
			pr_err("failed to allocate memory\n");
			ret = -ENOMEM;
			goto read_phone_fw;
		}
		nread = vfs_read(fp, (char __user *)fw_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			pr_err("failed to read firmware file, %ld Bytes\n", nread);
			ret = -EIO;
			goto read_phone_fw;
		}

		strncpy(dump_fw_version, fw_buf+nread-11, FIMC_IS_HEADER_VER_SIZE);
		pr_info("Camera: dumped fw version: %s\n", dump_fw_version);

read_phone_fw:
		if (fw_buf) {
			vfree(fw_buf);
			fw_buf = NULL;
		}

		if (fp && is_dump_existed) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;
#endif
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			pr_err("Camera: Failed open phone firmware\n");
			ret = -EIO;
			fp = NULL;
			goto read_phone_fw_exit;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		pr_info("start, file path %s, size %ld Bytes\n",
			fw_path, fsize);
		fw_buf = vmalloc(fsize);
		if (!fw_buf) {
			pr_err("failed to allocate memory\n");
			ret = -ENOMEM;
			goto read_phone_fw_exit;
		}
		nread = vfs_read(fp, (char __user *)fw_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			pr_err("failed to read firmware file, %ld Bytes\n", nread);
			ret = -EIO;
			goto read_phone_fw_exit;
		}

		strncpy(phone_fw_version, fw_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
		strncpy(sysfs_pinfo.header_ver, fw_buf + nread - 11, FIMC_IS_HEADER_VER_SIZE);
		pr_info("Camera: phone fw version: %s\n", phone_fw_version);
read_phone_fw_exit:
		if (fw_buf) {
			vfree(fw_buf);
			fw_buf = NULL;
		}

		if (fp) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

		from_fw_revision = fimc_is_sec_fw_revision(sysfs_finfo.header_ver);
		phone_fw_revision = fimc_is_sec_fw_revision(phone_fw_version);
		if (is_dump_existed)
			dump_fw_revision = fimc_is_sec_fw_revision(dump_fw_version);

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
#if 0
		if (sysfs_finfo.header_ver[0] == 'O') {
			/* hack: gumi module using phone fw */
			is_dumped_fw_loading_needed = false;
			is_dump_needed = false;
		} else if (sysfs_finfo.header_ver[FW_ISP_COMPANY] != FW_ISP_COMPANY_LSI) {
			ret = -EINVAL;
			goto exit;
		}

		if (is_dump_needed) {
			ret = fimc_is_sec_readfw(core);
			if (ret < 0) {
				if (!crc32_fw_check) {
					is_dumped_fw_loading_needed = false;
					ret = 0;
				} else
					goto exit;
			}
		}
#endif
		if (is_dump_needed && is_dumped_fw_loading_needed) {
			strncpy(loaded_fw, sysfs_finfo.header_ver, FIMC_IS_HEADER_VER_SIZE);
		} else if (!is_dump_needed && is_dumped_fw_loading_needed) {
			strncpy(loaded_fw, dump_fw_version, FIMC_IS_HEADER_VER_SIZE);
		} else
			strncpy(loaded_fw, phone_fw_version, FIMC_IS_HEADER_VER_SIZE);

	} else {
		pr_info("Did not read cal data from FROM, FW version = %s\n", sysfs_finfo.header_ver);
		strcpy(fw_name, sysfs_finfo.load_fw_name);
		strcpy(setf_name, sysfs_finfo.load_setfile_name);
	}

exit:
	if (is_ldo_enabled && !core->running_rear_camera) {
		pr_info("disable %s in the %s\n", "CAM_IO_1.8V_AP", __func__);
		ret = fimc_is_sec_ldo_enable(dev, "CAM_IO_1.8V_AP", false);
		if (ret)
			pr_err("fimc_is_sec_fw_sel: error, failed to cam_io(off)");
	}

	mutex_unlock(&core->spi_lock);

	if (sysfs_finfo.header_ver[3] != 'L') {
		pr_err("Not supported module. Module ver = %s", sysfs_finfo.header_ver);
		return  -EIO;
	}

	return ret;
}
#endif

#ifdef CONFIG_COMPANION_USE
void fimc_is_set_spi_config(struct fimc_is_spi_gpio *spi_gpio, int func, bool ssn) {
	pin_config_set(FIMC_IS_SPI_PINNAME, spi_gpio->spi_sclk,
			PINCFG_PACK(PINCFG_TYPE_FUNC, func));
	pin_config_set(FIMC_IS_SPI_PINNAME, spi_gpio->spi_miso,
			PINCFG_PACK(PINCFG_TYPE_FUNC, func));
	pin_config_set(FIMC_IS_SPI_PINNAME, spi_gpio->spi_mois,
			PINCFG_PACK(PINCFG_TYPE_FUNC, func));
	if(ssn == true) {
		pin_config_set(FIMC_IS_SPI_PINNAME, spi_gpio->spi_ssn,
				PINCFG_PACK(PINCFG_TYPE_FUNC, func));
	}
}

int fimc_is_sec_concord_fw_sel(struct fimc_is_core *core, struct device *dev,
	char *fw_name, char *master_setf_name, char *mode_setf_name)
{
	int ret = 0;
	char c1_fw_path[100];
	char dump_c1_fw_path[100];
	char dump_c1_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
	char phone_c1_fw_version[FIMC_IS_HEADER_VER_SIZE + 1] = {0, };
	int from_c1_fw_revision = 0;
	int dump_c1_fw_revision = 0;
	int phone_c1_fw_revision = 0;

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *c1_fw_buf = NULL;
	bool is_dump_existed = false;
	bool is_dump_needed = true;
	int pixelSize = 0;

	if ((!sysfs_finfo.is_c1_caldata_read &&
	    (cam_id == CAMERA_SINGLE_REAR /* || cam_id == CAMERA_DUAL_FRONT*/)) ||
	    force_caldata_dump) {
		is_dumped_c1_fw_loading_needed = false;
		if (force_caldata_dump)
			pr_info("forced caldata dump!!\n");

		pr_info("Load companion fw from FROM\n");
		sysfs_finfo.is_c1_caldata_read = true;

		/*ret = fimc_is_sec_core_voltage_select(dev, sysfs_finfo.header_ver);
		if (ret < 0) {
			err("failed to fimc_is_sec_core_voltage_select (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}*/

		if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_F) ||
			fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_I)) {
			if (sysfs_finfo.concord_header_ver[9] == '0') {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_EVT0);
				snprintf(fw_name, sizeof(FIMC_IS_FW_COMPANION_EVT0), "%s", FIMC_IS_FW_COMPANION_EVT0);
			} else if (sysfs_finfo.concord_header_ver[9] == '1') {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_EVT1);
				snprintf(fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_EVT1);
			} else {
				pr_info("Camera : Wrong companion module version.\n");
			}
			sysfs_finfo.sensor_id = COMPANION_SENSOR_2P2;
			snprintf(master_setf_name, sizeof(FIMC_IS_COMPANION_2P2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_MASTER_SETF);
			snprintf(mode_setf_name, sizeof(FIMC_IS_COMPANION_2P2_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_IMX240) ||
			fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_IMX240_Q_C1) ||
			fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_IMX240_Q)) {
			if (sysfs_finfo.concord_header_ver[9] == '0') {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_EVT0);
				snprintf(fw_name, sizeof(FIMC_IS_FW_COMPANION_EVT0), "%s", FIMC_IS_FW_COMPANION_EVT0);
			} else if (sysfs_finfo.concord_header_ver[9] == '1') {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_IMX240_EVT1);
				snprintf(fw_name, sizeof(FIMC_IS_FW_COMPANION_IMX240_EVT1), "%s", FIMC_IS_FW_COMPANION_IMX240_EVT1);
			} else {
				pr_info("Camera : Wrong companion module version.\n");
			}
			sysfs_finfo.sensor_id = COMPANION_SENSOR_IMX240;
			snprintf(master_setf_name, sizeof(FIMC_IS_COMPANION_IMX240_MASTER_SETF), "%s", FIMC_IS_COMPANION_IMX240_MASTER_SETF);
			snprintf(mode_setf_name, sizeof(FIMC_IS_COMPANION_IMX240_MODE_SETF), "%s", FIMC_IS_COMPANION_IMX240_MODE_SETF);
		} else if (fimc_is_sec_fw_module_compare(sysfs_finfo.concord_header_ver, FW_2P2_12M)) {
			if (sysfs_finfo.concord_header_ver[9] == '0') {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_EVT0);
				snprintf(fw_name, sizeof(FIMC_IS_FW_COMPANION_EVT0), "%s", FIMC_IS_FW_COMPANION_EVT0);
			} else if (sysfs_finfo.concord_header_ver[9] == '1') {
				snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
					FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
				snprintf(fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_12M_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
			} else {
				pr_info("Camera : Wrong companion module version.\n");
			}
			sysfs_finfo.sensor_id = COMPANION_SENSOR_2P2;
			snprintf(master_setf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MASTER_SETF);
			snprintf(mode_setf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MODE_SETF);
		} else {
			pixelSize = fimc_is_sec_get_pixel_size(sysfs_finfo.concord_header_ver);
			if (pixelSize == 16) {
				if (sysfs_finfo.concord_header_ver[9] == '0') {
					snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
						FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_EVT0);
					snprintf(fw_name, sizeof(FIMC_IS_FW_COMPANION_EVT0), "%s", FIMC_IS_FW_COMPANION_EVT0);
				} else if (sysfs_finfo.concord_header_ver[9] == '1') {
					snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
						FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_EVT1);
					snprintf(fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_EVT1);
				} else {
					pr_info("Camera : Wrong companion module version.\n");
				}
				sysfs_finfo.sensor_id = COMPANION_SENSOR_2P2;
				snprintf(master_setf_name, sizeof(FIMC_IS_COMPANION_2P2_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_MASTER_SETF);
				snprintf(mode_setf_name, sizeof(FIMC_IS_COMPANION_2P2_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_MODE_SETF);
			} else if (pixelSize == 12) {
				if (sysfs_finfo.concord_header_ver[9] == '0') {
					snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
						FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_EVT0);
					snprintf(fw_name, sizeof(FIMC_IS_FW_COMPANION_EVT0), "%s", FIMC_IS_FW_COMPANION_EVT0);
				} else if (sysfs_finfo.concord_header_ver[9] == '1') {
					snprintf(c1_fw_path, sizeof(c1_fw_path), "%s%s",
						FIMC_IS_FW_PATH, FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
					snprintf(fw_name, sizeof(FIMC_IS_FW_COMPANION_2P2_12M_EVT1), "%s", FIMC_IS_FW_COMPANION_2P2_12M_EVT1);
				} else {
					pr_info("Camera : Wrong companion module version.\n");
				}
				sysfs_finfo.sensor_id = COMPANION_SENSOR_2P2;
				snprintf(master_setf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MASTER_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MASTER_SETF);
				snprintf(mode_setf_name, sizeof(FIMC_IS_COMPANION_2P2_12M_MODE_SETF), "%s", FIMC_IS_COMPANION_2P2_12M_MODE_SETF);
			}
		}

		strcpy(sysfs_finfo.load_c1_fw_name, fw_name);
		strcpy(sysfs_finfo.load_c1_mastersetf_name, master_setf_name);
		strcpy(sysfs_finfo.load_c1_modesetf_name, mode_setf_name);

		if (sysfs_finfo.concord_header_ver[9] == '0') {
			snprintf(dump_c1_fw_path, sizeof(dump_c1_fw_path), "%s%s",
				FIMC_IS_FW_DUMP_PATH, FIMC_IS_FW_COMPANION_EVT0);
		} else if (sysfs_finfo.concord_header_ver[9] == '1') {
			snprintf(dump_c1_fw_path, sizeof(dump_c1_fw_path), "%s%s",
				FIMC_IS_FW_DUMP_PATH, sysfs_finfo.load_c1_fw_name);
		}
		pr_info("Camera: f-rom fw version: %s\n", sysfs_finfo.concord_header_ver);

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		ret = 0;
		fp = filp_open(dump_c1_fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			pr_info("Camera: There is no dumped Companion firmware\n");
			is_dump_existed = false;
			goto read_phone_fw;
		} else {
			is_dump_existed = true;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		pr_info("start, file path %s, size %ld Bytes\n",
			dump_c1_fw_path, fsize);
		c1_fw_buf = vmalloc(fsize);
		if (!c1_fw_buf) {
			pr_err("failed to allocate memory\n");
			ret = -ENOMEM;
			goto read_phone_fw;
		}
		nread = vfs_read(fp, (char __user *)c1_fw_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			pr_err("failed to read firmware file, %ld Bytes\n", nread);
			ret = -EIO;
			goto read_phone_fw;
		}

		strncpy(dump_c1_fw_version, c1_fw_buf+nread - 16, 11);
		pr_info("Camera: dumped companion fw version: %s\n", dump_c1_fw_version);
read_phone_fw:
		if (c1_fw_buf) {
			vfree(c1_fw_buf);
			c1_fw_buf = NULL;
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
			pr_err("Camera: Failed open phone companion firmware\n");
			ret = -EIO;
			fp = NULL;
			goto read_phone_fw_exit;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		pr_info("start, file path %s, size %ld Bytes\n",
			c1_fw_path, fsize);
		c1_fw_buf = vmalloc(fsize);
		if (!c1_fw_buf) {
			pr_err("failed to allocate memory\n");
			ret = -ENOMEM;
			goto read_phone_fw_exit;
		}
		nread = vfs_read(fp, (char __user *)c1_fw_buf, fsize, &fp->f_pos);
		if (nread != fsize) {
			pr_err("failed to read companion firmware file, %ld Bytes\n", nread);
			ret = -EIO;
			goto read_phone_fw_exit;
		}

		strncpy(phone_c1_fw_version, c1_fw_buf + nread - 16, 11);
		strncpy(sysfs_pinfo.concord_header_ver, c1_fw_buf + nread - 16, 11);
		pr_info("Camera: phone companion fw version: %s\n", phone_c1_fw_version);
read_phone_fw_exit:
		if (c1_fw_buf) {
			vfree(c1_fw_buf);
			c1_fw_buf = NULL;
		}

		if (fp) {
			filp_close(fp, current->files);
			fp = NULL;
		}

		set_fs(old_fs);

		if (ret < 0)
			goto exit;

		from_c1_fw_revision = fimc_is_sec_fw_revision(sysfs_finfo.concord_header_ver);
		phone_c1_fw_revision = fimc_is_sec_fw_revision(phone_c1_fw_version);
		if (is_dump_existed)
			dump_c1_fw_revision = fimc_is_sec_fw_revision(dump_c1_fw_version);

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
#if 0
		if (is_dump_needed) {
			ret = fimc_is_sec_read_companion_fw(core);
			if (ret < 0) {
				if (!crc32_c1_fw_check) {
					is_dumped_c1_fw_loading_needed = false;
					ret = 0;
				} else
					goto exit;
			}
		}
#endif
		if (is_dump_needed && is_dumped_c1_fw_loading_needed) {
			strncpy(loaded_companion_fw, sysfs_finfo.concord_header_ver, 11);
		} else if (!is_dump_needed && is_dumped_c1_fw_loading_needed) {
			strncpy(loaded_companion_fw, dump_c1_fw_version, 11);
		} else
			strncpy(loaded_companion_fw, phone_c1_fw_version, 11);
	} else {
		pr_info("Did not Load companion fw from FROM, Companion version = %s\n", sysfs_finfo.concord_header_ver);
		strcpy(fw_name, sysfs_finfo.load_c1_fw_name);
		strcpy(master_setf_name, sysfs_finfo.load_c1_mastersetf_name);
		strcpy(mode_setf_name, sysfs_finfo.load_c1_modesetf_name);
	}

exit:
	return ret;
}
#endif
