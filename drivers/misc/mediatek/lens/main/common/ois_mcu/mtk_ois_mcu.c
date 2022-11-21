#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/i2c.h>

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/stat.h>

#include <linux/of_gpio.h>

#include <linux/regulator/consumer.h>

#include <linux/syscalls.h>
#include <linux/file.h>

#include "mtk_ois_mcu.h"
#include "camera_ois_mcu.h"
#include "mtk_ois_define.h"
#include "mtk_ois_mcu_fw.h"
#include "mtk_ois_i2c.h"
#include "mtk_ois_mcu_sysfs.h"
#include "kd_imgsensor_sysfs_adapter.h"

static struct i2c_client *i2c_client;
static struct ois_info ois_info;
static struct mcu_info mcu_info;

struct stAF_DrvList af_drv_list[MAX_NUM_OF_LENS] = {
	{1, AFDRV_DW9825AF_OIS_MCU, DW9825AF_OIS_MCU_SetI2Cclient,
	DW9825AF_OIS_MCU_Ioctl, DW9825AF_OIS_MCU_Release, DW9825AF_OIS_MCU_GetFileName, NULL},
};

int ois_mcu_probe(struct device *pdev, struct pinctrl *pinctrl, const struct file_operations *af_op)
{
	ois_info.af_pos_wide = 0;
	ois_info.is_fw_updated = 0;
	mcu_info.pdev = pdev;
	mcu_info.power_on = false;
	ois_info.af_op = af_op;
	ois_mcu_pinctrl_init(&mcu_info, pinctrl);

	return 0;
}

static int ois_mcu_get_eeprom_data(void)
{
	const struct rom_cal_addr *ois_cal_addr;
	char *rom_cal_buf = NULL;
	int sensor_dev_id = IMGSENSOR_SENSOR_IDX_MAIN;
	int ois_addr, ois_size;

	ois_cal_addr = IMGSENSOR_GET_CAL_ADDR_BY_SENSOR_IDX(sensor_dev_id, GET_CAL_OIS);
	if (ois_cal_addr == NULL) {
		LOG_ERR("ois_cal_addr is NULL");
		return -1;
	}

	ois_addr = ois_cal_addr->addr;
	ois_size = ois_cal_addr->size;

	LOG_INF("OIS cal start addr: %#06x, size: %#06x\n", ois_addr, ois_size);

	if (IMGSENSOR_GET_CAL_BUF_BY_SENSOR_IDX(sensor_dev_id, &rom_cal_buf)) {
		LOG_ERR("cal data is NULL, sensor_dev_id: %d", sensor_dev_id);
		return -1;
	}

	rom_cal_buf += ois_addr;

	memcpy(ois_info.wide_x_gg, rom_cal_buf + OIS_CAL_EEPROM_XGG_ADDR, OIS_CAL_EEPROM_GG_SIZE);
	memcpy(ois_info.wide_y_gg, rom_cal_buf + OIS_CAL_EEPROM_YGG_ADDR, OIS_CAL_EEPROM_GG_SIZE);

	memcpy(ois_info.wide_x_sr, rom_cal_buf + OIS_CAL_EEPROM_XGG_ADDR, OIS_CAL_EEPROM_SR_SIZE);
	memcpy(ois_info.wide_y_sr, rom_cal_buf + OIS_CAL_EEPROM_YGG_ADDR, OIS_CAL_EEPROM_SR_SIZE);

	return 0;
}

int ois_mcu_update(void)
{
	LOG_INF("E");
	if (!ois_info.is_fw_updated && mcu_info.pinctrl != NULL && i2c_client != NULL) {
		if (!ois_mcu_fw_update(&mcu_info, i2c_client))
			ois_info.is_fw_updated = 1;
	} else {
		LOG_ERR("fw_updated %d, pinctrl %x, i2c %x", ois_info.is_fw_updated, mcu_info.pinctrl, i2c_client);
	}
	return 0;
}


static void ois_mcu_pinctrl_print(struct mcu_info *mcu_info)
{
	LOG_DBG("pinctrl: %x, mcu boot: %x, %x, mcu nRST: %x, %x, mcu VDD: %x, %x, ois VDD: %x, %x",
		mcu_info->pinctrl, mcu_info->mcu_boot_high, mcu_info->mcu_boot_low,
		mcu_info->mcu_nrst_high, mcu_info->mcu_nrst_low, mcu_info->mcu_vdd_high,
		mcu_info->mcu_vdd_low, mcu_info->ois_vdd_high, mcu_info->ois_vdd_low);
}

static void ois_mcu_print_ois_info(void)
{
	LOG_DBG("xgg: 0x%x, 0x%x, 0x%x, 0x%x", ois_info.wide_x_gg[0],
		ois_info.wide_x_gg[1], ois_info.wide_x_gg[2], ois_info.wide_x_gg[3]);
	LOG_DBG("ygg: 0x%x, 0x%x, 0x%x, 0x%x", ois_info.wide_y_gg[0],
		ois_info.wide_y_gg[1], ois_info.wide_y_gg[2], ois_info.wide_y_gg[3]);
	LOG_DBG("x_coef: 0x%x, 0x%x", ois_info.wide_x_coef[0], ois_info.wide_x_coef[1]);
	LOG_DBG("y_coef: 0x%x, 0x%x", ois_info.wide_y_coef[0], ois_info.wide_y_coef[1]);
}

static void ois_mcu_print_info(void)
{
	LOG_DBG("mcu info dev: 0x%x\n", mcu_info.pdev);
	LOG_DBG("mcu info dev name: %s\n", mcu_info.pdev->init_name);
	ois_mcu_print_ois_info();
	ois_mcu_pinctrl_print(&mcu_info);
	LOG_DBG("mcu is_updated: %d\n", mcu_info.is_updated);
	ois_mcu_print_fw_version(&mcu_info, NULL);
}

static int ois_mcu_write_camera_mode_to_mcu(int mode)
{
	int ret = 0;

	LOG_DBG("mode value(%d)\n", mode);
	switch (mode) {
	case OIS_MODE_STILL:
		ret = ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0002, 0x00);
		break;
	case OIS_MODE_MOVIE:
		ret = ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0002, 0x01);
		break;
	case OIS_MODE_CENTERING:
		ret = ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0002, 0x05);
		break;
	case OIS_MODE_ZOOM:
		ret = ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0002, 0x13);
		break;
	case OIS_MODE_VDIS:
		ret = ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0002, 0x14);
		break;
	default:
		LOG_INF("undefined mode %d\n", mode);
		ret = -1;
		break;
	}
	return ret;
}

static int ois_mcu_control_servo(u8 data)
{
	return ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0000, data);
}

static inline u8 ois_mcu_check_mcu_status(struct i2c_client *i2c_client_in)
{
	int ret = 0;
	u8 val = 0;
	int retries = 10;

	do {
		ret = ois_mcu_i2c_read_multi(i2c_client_in, MCU_I2C_SLAVE_W_ADDR, 0x01, &val, 1);
		if (ret != 0) {
			val = -EIO;
			break;
		}
		usleep_range(3000, 3100);
		if (--retries < 0) {
			LOG_ERR("read ois status failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val != 0x01);
	return val;
}

static noinline_for_stack long __get_file_size(struct file *file)
{
	struct kstat st;
	u32 request_mask = (STATX_MODE | STATX_SIZE);

	if (vfs_getattr(&file->f_path, &st, request_mask, KSTAT_QUERY_FLAGS))
		return -1;
	if (!S_ISREG(st.mode))
		return -1;
	if (st.size != (long)st.size)
		return -1;

	return st.size;
}

// imgsensor_sysfs.c read_file
long ois_mcu_read_efs(char *efs_path, u8 *buf, int buflen)
{
	struct file *fp = NULL;
	mm_segment_t old_fs;
	char *filename;
	long ret = 0, fsize = 0, nread = 0;
	loff_t file_offset = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filename = __getname();
	if (unlikely(!filename)) {
		set_fs(old_fs);
		return 0;
	}

	snprintf(filename, PATH_MAX, "%s", efs_path);

	fp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		__putname(filename);
		set_fs(old_fs);
		return 0;
	}

	fsize = __get_file_size(fp);
	if (fsize <= 0 || fsize > buflen) {
		LOG_ERR("__get_file_size fail(%ld)", fsize);
		ret = 0;
		goto p_err;
	}

	nread = kernel_read(fp, buf, fsize, &file_offset);
	if (nread != fsize) {
		LOG_ERR("kernel_read was failed(%ld != %ld)",
			nread, fsize);
		ret = 0;
		goto p_err;
	}

	ret = fsize;

p_err:
	__putname(filename);
	filp_close(fp, current->files);
	set_fs(old_fs);

	return ret;
}

int ois_mcu_get_offset_from_efs(long *raw_data_x, long *raw_data_y)
{
	int ret = 0, i = 0, j = 0, comma_offset = 0;
	bool detect_comma = false;
	char efs_data[MAX_EFS_DATA_LENGTH] = { 0 };
	unsigned char *buffer = NULL;
	long efs_size = 0;

	buffer = vmalloc(MAX_EFS_DATA_LENGTH);
	if (!buffer) {
		LOG_ERR("vmalloc failed");
		return -1;
	}

	efs_size = ois_mcu_read_efs(OIS_GYRO_CAL_VALUE_FROM_EFS, buffer, MAX_EFS_DATA_LENGTH);
	if (efs_size == 0) {
		LOG_ERR("efs read failed");
		ret = 0;
		goto ERROR;
	}

	i = 0;
	detect_comma = false;
	for (i = 0; i < efs_size; i++) {
		if (*(buffer + i) == ',') {
			comma_offset = i;
			detect_comma = true;
			break;
		}
	}

	if (detect_comma) {
		memset(efs_data, 0x00, sizeof(efs_data));
		j = 0;
		for (i = 0; i < comma_offset; i++) {
			if (buffer[i] != '.') {
				efs_data[j] = buffer[i];
				j++;
			}
		}
		kstrtol(efs_data, 10, raw_data_x);

		memset(efs_data, 0x00, sizeof(efs_data));
		j = 0;
		for (i = comma_offset + 1; i < efs_size; i++) {
			if (buffer[i] != '.') {
				efs_data[j] = buffer[i];
				j++;
			}
		}
		kstrtol(efs_data, 10, raw_data_y);
	} else {
		LOG_INF("cannot find delimeter");
		ret = -1;
	}
	LOG_INF("X raw_x = %ld, raw_y = %ld", *raw_data_x, *raw_data_y);

ERROR:
	if (buffer) {
		vfree(buffer);
		buffer = NULL;
	}

	return ret;
}

int ois_mcu_get_gyro_offset(void)
{
	int ret = 0;
	long raw_data_x = 0;
	long raw_data_y = 0;
	int xg_zero = 0, yg_zero = 0;

	ret = ois_mcu_get_offset_from_efs(&raw_data_x, &raw_data_y);
	if (ret < 0) {
		LOG_ERR("Failed to get gyro calibration data");
		return -1;
	}

	xg_zero = raw_data_x * OIS_GYRO_SCALE_FACTOR_LSM6DSO / 1000;
	yg_zero = raw_data_y * OIS_GYRO_SCALE_FACTOR_LSM6DSO / 1000;
	if (xg_zero > 0x7FFF)
		xg_zero = -((xg_zero ^ 0xFFFF) + 1);
	if (yg_zero > 0x7FFF)
		yg_zero = -((yg_zero ^ 0xFFFF) + 1);

	LOG_INF("xgzero: 0x%x, ygzero: 0x%x\n", xg_zero, yg_zero);

	ois_info.wide_x_goffset[0] = (xg_zero & 0xFF);
	ois_info.wide_x_goffset[1] = ((xg_zero >> 8) & 0xFF);

	ois_info.wide_y_goffset[0] = (yg_zero & 0xFF);
	ois_info.wide_y_goffset[1] = ((yg_zero >> 8) & 0xFF);

	return ret;
}

static int ois_mcu_write_gyro_info_to_mcu(void)
{
	int ret = 0;

	LOG_INF("E");
	ret = ois_mcu_i2c_write_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0254, ois_info.wide_x_gg, 6);
	ret |= ois_mcu_i2c_write_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0258, ois_info.wide_y_gg, 6);
	if (ret < 0)
		LOG_ERR("fail to write ois gyro gains\n");

	ret = ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0240, ois_info.wide_x_gp);
	ret |= ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0241, ois_info.wide_y_gp);
	ret |= ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0242, ois_info.gyro_orientation);
	if (ret < 0)
		LOG_ERR("fail to write gyro orientions\n");

	ret = ois_mcu_get_gyro_offset();
	if (ret < 0) {
		LOG_ERR("Failed to get gyro calibration data");
		return ret;
	}

	ret = ois_mcu_i2c_write_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0248, ois_info.wide_x_goffset, 4);
	ret |= ois_mcu_i2c_write_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x024A, ois_info.wide_y_goffset, 4);
	if (ret < 0)
		LOG_ERR("fail to write gyro offsets\n");

	return ret;
}

static int ois_mcu_read_hall_center(short *hall_x_center, short *hall_y_center)
{
	int ret = 0;
	u8 x_data_array[2] = {0, };
	u8 y_data_array[2] = {0, };

	ret = ois_mcu_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x021A, x_data_array, 2);
	ret |= ois_mcu_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x021C, y_data_array, 2);
	if (ret < 0)
		LOG_ERR("fail to read hole center\n");

	*hall_x_center = (x_data_array[1] << 8) | x_data_array[0];
	*hall_y_center = (y_data_array[1] << 8) | y_data_array[0];
	return ret;
}

static int ois_mcu_get_error_register(short *val)
{
	int ret = 0;
	u8 data[2] = {0, };

	ret = ois_mcu_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0004, data, 2);
	if (ret != 0) {
		*val = -EIO;
		LOG_ERR("i2c read fail\n");
	} else {
		*val = (data[1] << 8) | data[0];
	}
	return ret;
}

static int ois_mcu_read_error_register(void)
{
	int ret = 0;
	short val = 0;

	ret = ois_mcu_get_error_register(&val);

	if ((val & 0x0023) != 0x0) {
		LOG_INF("OIS GYRO Error Register %x\n", val);
		if ((val & 0x0001) != 0x0)
			LOG_INF("OIS GYRO Error Register in x sensor\n");
		if ((val & 0x0002) != 0x0)
			LOG_INF("OIS GYRO Error Register in y sensor\n");
		if ((val & 0x0020) != 0x0)
			LOG_INF("OIS GYRO Error Register in SPI connection\n");
	}

	return ret;
}

static int ois_mcu_set_init_data(void)
{
	int ret = 0;
	int ggfadeup_val = 1000;
	int ggfadedown_val = 1000;
	u8 data[2];

	LOG_INF("E");
	ret = ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x003A, 0x80);
	if (ret < 0) {
		LOG_ERR("fail to write ois center shift data of wide to OIS MCU\n");
		return ret;
	}

	ret = ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0039, 0x01);
	if (ret < 0) {
		LOG_ERR("fail to write VDIS setting data to OIS MCU\n");
		return ret;
	}

	data[0] = ggfadeup_val & 0xFF;
	data[1] = (ggfadeup_val >> 8) & 0xFF;
	ret = ois_mcu_i2c_write_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0238, data, 4);
	if (ret < 0)
		LOG_ERR("fail to set fadeup\n");

	data[0] = ggfadedown_val & 0xFF;
	data[1] = (ggfadedown_val >> 8) & 0xFF;
	ret = ois_mcu_i2c_write_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x023A, data, 4);
	if (ret < 0)
		LOG_ERR("fail to set fadedown\n");

	ret = ois_mcu_read_hall_center(&(ois_info.x_hall_center), &(ois_info.y_hall_center));
	LOG_INF("hall center x: %d, y: %d\n", ois_info.x_hall_center, ois_info.y_hall_center);

	ret = ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x00BE, 0x01);
	if (ret < 0)
		LOG_ERR("fail to set ois single mode\n");

	ret = ois_mcu_read_error_register();
	ret = ois_mcu_write_camera_mode_to_mcu(OIS_MODE_STILL);
	if (ret)
		LOG_ERR("fail to set OIS_MODE_STILL mode\n");

	return ret;
}

int ois_mcu_set_i2c_client(struct i2c_client *i2c_client_in, spinlock_t *af_spinLock, int *af_opened)
{
	if (!i2c_client_in) {
		LOG_ERR("i2c_client_in is NULL\n", __func__);
		return -1;
	}
	i2c_client = i2c_client_in;
	ois_info.af_spinLock = af_spinLock;
	ois_info.af_opened = af_opened;
	LOG_INF("i2c client name: %s\n", i2c_client->name);
	LOG_INF("i2c adapter name: %s\n", i2c_client->adapter->name);
	return 0;
}

int ois_mcu_power_on(void)
{
	int ret = 0;

	ret = ois_mcu_power_up(&mcu_info);

	return ret;
}

int ois_mcu_power_off(void)
{
	int ret = 0;

	ret = ois_mcu_power_down(&mcu_info);

	return ret;
}

int ois_mcu_power_onoff(bool onoff)
{
	int ret = 0;

	if (onoff == true)
		ret = ois_mcu_power_on();
	else
		ret = ois_mcu_power_off();

	return ret;
}

int ois_mcu_init(void)
{
	int ret = 0;
	unsigned char val = 0;

	LOG_INF("ois_init E\n");

	mcu_info.prev_vdis_coef = 255;

	ois_mcu_get_eeprom_data();

	val = ois_mcu_check_mcu_status(i2c_client);
	if (val != 0x01)
		ret = -1;
	else
		ret = ois_mcu_write_gyro_info_to_mcu();

	val = ois_mcu_check_mcu_status(i2c_client);
	if (val != 0x01)
		ret = -1;
	else
		ret = ois_mcu_set_init_data();

	ois_mcu_print_info();

	return ret;
}

int ois_mcu_deinit(void)
{
	int ret = 0;

	LOG_INF("ois_deinit E\n");

	ret = ois_mcu_control_servo(0x00);

	usleep_range(10000, 11000);
	ois_info.af_pos_wide = 0;
	return ret;
}


int ois_mcu_center_shift(unsigned long position)
{
	int ret = 0;
	int position_changed;

	position_changed = ((position & 0x3FF) >> 2); // convert to 8 bit
	if (position_changed > 255) {
		LOG_ERR("position_changed over 8 bits %d\n", position_changed);
		return -1;
	}
	if (ois_info.af_pos_wide != position_changed) {
		ret = ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x003A, (u8)position_changed);
		if (ret < 0) {
			LOG_ERR("fails to write ois wide center shift\n");
			return ret;
		}
		LOG_DBG("af position changed: %d\n", position_changed);
		ois_info.af_pos_wide = position_changed;
	}
	return ret;
}

int ois_mcu_set_camera_mode(int mode)
{
	int ret = 0;

	LOG_INF("mode value(%d)\n", mode);
	ret = ois_mcu_write_camera_mode_to_mcu(mode);
	ret |= ois_mcu_control_servo(0x01);
	if (ret)
		LOG_ERR("fail to set ois mode %d", mode);
	return ret;
}

int ois_mcu_set_vdis_coef(int inCoef)
{
	int ret = 0;
	unsigned char coef = (unsigned char)inCoef;
	if (mcu_info.prev_vdis_coef == coef)
		return ret;

	LOG_INF("vdis coef(%u)\n", (unsigned char)coef);
	ret = ois_mcu_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x005E, coef);
	if (ret)
		LOG_ERR("fail to set vdis coefficient %u", (unsigned char)coef);
	else
		mcu_info.prev_vdis_coef = coef;
	return ret;
}

int ois_mcu_sysfs_gyro_cal(long *out_gyro_x, long *out_gyro_y)
{
	int result = 2;

	result = ois_mcu_gyro_calibration(&mcu_info, i2c_client, out_gyro_x, out_gyro_y);
	if (result < 1)
		LOG_ERR("gyro calibration fail");

	return result;
}


int ois_mcu_sysfs_get_gyro_gain_from_eeprom(u32 *xgg, u32 *ygg)
{
	int ret = 0;

	ret = ois_mcu_get_eeprom_data();
	if (ret < 0)
		return 1;

	memcpy(xgg, ois_info.wide_x_gg, 4);
	memcpy(ygg, ois_info.wide_y_gg, 4);

	return 0;
}

int ois_mcu_sysfs_get_supperssion_ratio_from_eeprom(u32 *xsr, u32 *ysr)
{
	int ret = 0;

	ret = ois_mcu_get_eeprom_data();
	if (ret < 0)
		return 1;

	memcpy(xsr, ois_info.wide_x_sr, 2);
	memcpy(ysr, ois_info.wide_y_sr, 2);

	return 0;
}

int ois_mcu_sysfs_read_gyro_offset(long *out_gyro_x, long *out_gyro_y)
{
	return ois_mcu_get_offset_data(&mcu_info, i2c_client, out_gyro_x, out_gyro_y);
}

int ois_mcu_sysfs_read_gyro_offset_test(long *out_gyro_x, long *out_gyro_y)
{
	return ois_mcu_get_offset_testdata(&mcu_info, i2c_client, out_gyro_x, out_gyro_y);
}

int ois_mcu_sysfs_selftest(void)
{
	return  ois_mcu_selftest(&mcu_info, i2c_client);
}


int ois_mcu_sysfs_get_oisfw_version(char *ois_mcu_fw_ver)
{
	int ret = 0;
	int result = 0;

	result = snprintf(ois_mcu_fw_ver, 40, "%s %s\n", mcu_info.module_ver,
		(mcu_info.no_module_ver) ? ("NULL") : (mcu_info.phone_ver));
	if (result <= 0) {
		LOG_ERR("fail to get ois fw ver");
		ret = -1;
	}
	LOG_INF("ois fw ver: %s", ois_mcu_fw_ver);

	return ret;
}

static int ois_mcu_af_drv_init(char *af_name)
{
	int ret;
	int i;

	LOG_INF("E: AF(%s)", af_name);

	ois_info.cur_af_drv = NULL;
	for (i = 0; i < MAX_NUM_OF_LENS; i++) {
		if (af_drv_list[i].uEnable != 1)
			break;

		if (strcmp(af_name, af_drv_list[i].uDrvName) == 0) {
			LOG_INF("AF Name : %s", af_name);
			ois_info.cur_af_drv = &af_drv_list[0];
			break;
		}
	}

	if (!ois_info.cur_af_drv) {
		LOG_ERR("af drv is null");
		return -1;
	}

	if (!ois_info.af_op) {
		LOG_ERR("af_op is null");
		return -1;
	}

	ret = ois_info.af_op->open(NULL, NULL);
	if (ret) {
		LOG_ERR("fail to open AF");
		return ret;
	}

	ret = ois_info.cur_af_drv->pAF_SetI2Cclient(i2c_client, ois_info.af_spinLock, ois_info.af_opened);
	if (ret != 1) {
		LOG_ERR("fail to set AF I2C client");
		return ret;
	}

	return ret;
}

static int ois_mcu_af_setup(void)
{
	int ret = 0;
	unsigned int command = 0;
	unsigned long param = 0;

	LOG_INF("E");
	if (!ois_info.cur_af_drv) {
		LOG_ERR("cur_af_drv is null");
		return -1;
	}
	if (!ois_info.cur_af_drv->pAF_Ioctl) {
		LOG_ERR("pAF_Ioctl is null");
		return -1;
	}

	command = AFIOC_S_SETOISAUTO;

	ret = ois_info.cur_af_drv->pAF_Ioctl(NULL, command, param);
	if (ret) {
		LOG_ERR("fail to set AF I2C client");
		return ret;
	}
	return ret;
}

static int ois_mcu_af_drv_deinit(void)
{
	int ret;

	LOG_INF("E");
	if (!ois_info.af_op) {
		LOG_ERR("af_op is null");
		return -1;
	}

	ret = ois_info.af_op->release(NULL, NULL);
	if (ret) {
		LOG_ERR("fail to release AF");
		return ret;
	}

	return ret;
}

int ois_mcu_sysfs_autotest(int *sin_x, int *sin_y, int *result_x, int *result_y, int threshold)
{
	int ret = 0;
	int result = 0;

	LOG_INF("E");
	ret = ois_mcu_init();
	if (ret) {
		LOG_ERR("fail to init");
		return ret;
	}

	ois_mcu_af_drv_init(OIS_AF_MAIN_NAME);
	ois_mcu_af_setup();

	msleep(100);

	ret = ois_mcu_sine_wavecheck(sin_x, sin_y, &result, i2c_client, threshold);
	if (*sin_x == -1 && *sin_y == -1) {
		LOG_ERR("OIS device is not prepared.");
		*result_x = false;
		*result_y = false;
		ret = -1;
	}

	if (ret == true) {
		*result_x = true;
		*result_y = true;
		ret = 0;
	} else {
		LOG_ERR("OIS autotest is failed value = 0x%x\n", result);
		if ((result & 0x03) == 0x01) {
			*result_x = false;
			*result_y = true;
		} else if ((result & 0x03) == 0x02) {
			*result_x = true;
			*result_y = false;
		} else {
			*result_x = false;
			*result_y = false;
		}
		ret = -1;
	}

	AF_PowerDown();
	ois_mcu_af_drv_deinit();

	return ret;
}

int ois_mcu_sysfs_set_mode(int mode)
{
	int ret = 0;
	int internal_mode = 0;

	LOG_INF("E");

	if (!mcu_info.power_on) {
		LOG_ERR("MCU power condition: off");
		return -1;
	}

	ret = ois_mcu_init();
	if (ret) {
		LOG_ERR("fail to init");
		return ret;
	}

	switch (mode) {
	case 0x0:
		internal_mode = OIS_MODE_STILL;
		break;
	case 0x1:
		internal_mode = OIS_MODE_MOVIE;
		break;
	case 0x5:
		internal_mode = OIS_MODE_CENTERING;
		break;
	case 0x13:
		internal_mode = OIS_MODE_ZOOM;
		break;
	case 0x14:
		internal_mode = OIS_MODE_VDIS;
		break;
	default:
		LOG_INF("ois_mode value(%d)\n", mode);
		break;
	}

	ret = ois_mcu_set_camera_mode(internal_mode);
	if (ret) {
		LOG_ERR("fail to set mode");
		return ret;
	}
	return ret;
}

int ois_mcu_sysfs_get_mcu_error(int *w_error)
{
	int ret = 0;
	short err_reg_val = 0;

	LOG_INF("E");

	*w_error = 1;
	if (!mcu_info.power_on) {
		LOG_ERR("MCU power condition: off");
		return -1;
	}

	ret = ois_mcu_init();
	if (ret) {
		LOG_ERR("fail to init");
		return ret;
	}

	ret = ois_mcu_get_error_register(&err_reg_val);
	if (ret) {
		LOG_ERR("fail to get error register mode");
		return ret;
	}

	/* i2c communication error */
	/* b9(0x0200): wide cam x, b10(0x0400): widecam y */
	*w_error = ((err_reg_val & 0x0600) >> 8);

	return ret;
}

