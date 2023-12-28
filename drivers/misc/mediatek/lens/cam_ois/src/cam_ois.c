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

#include "cam_ois.h"
#include "cam_ois_drv.h"
// #include "camera_ois.h"
#include "cam_ois_mcu_fw.h"
#include "cam_ois_define.h"
#include "cam_ois_i2c.h"
#include "cam_ois_sysfs.h"
#include "cam_ois_aois_if.h"
#include "cam_ois_power.h"
#include "cam_ois_common.h"

#include "kd_imgsensor_sysfs_adapter.h"

static struct i2c_client *i2c_client;
static struct ois_info ois_info;
static struct mcu_info mcu_info;

struct stAF_DrvList af_drv_list[MAX_NUM_OF_LENS] = {
	// {1, AFDRV_AK7377AF_OIS, AK7377AF_OIS_SetI2Cclient, AK7377AF_OIS_Ioctl,
	// AK7377AF_OIS_Release, AK7377AF_OIS_GetFileName, NULL},
	{1, AFDRV_DW9825AF_OIS_MCU, DW9825AF_OIS_MCU_SetI2Cclient,
	DW9825AF_OIS_MCU_Ioctl, DW9825AF_OIS_MCU_Release, DW9825AF_OIS_MCU_GetFileName, NULL},
};

int cam_ois_prove_init(void)
{
	int ret = 0;

	ois_info.af_pos_wide = 0;
	mcu_info.is_fw_updated = 0;
	mcu_info.power_count = 0;

	cam_ois_set_i2c_client();
	mcu_info.pdev = get_cam_ois_dev();
	if (mcu_info.pdev == NULL)
		LOG_INF("fail to get devices");
	else
		LOG_INF("MCU DEV name: %s", mcu_info.pdev->init_name);
	ret = cam_ois_pinctrl_init(&mcu_info);

	return ret;
}

int cam_ois_get_eeprom_from_sysfs(void)
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

	LOG_DBG("OIS cal start addr: %#06x, size: %#06x\n", ois_addr, ois_size);

	if (IMGSENSOR_GET_CAL_BUF_BY_SENSOR_IDX(sensor_dev_id, &rom_cal_buf)) {
		LOG_ERR("cal data is NULL, sensor_dev_id: %d", sensor_dev_id);
		return -1;
	}

	rom_cal_buf += ois_addr;

	memcpy(ois_info.wide_x_gg, rom_cal_buf + OIS_CAL_EEPROM_XGG_ADDR, OIS_CAL_EEPROM_GG_SIZE);
	memcpy(ois_info.wide_y_gg, rom_cal_buf + OIS_CAL_EEPROM_YGG_ADDR, OIS_CAL_EEPROM_GG_SIZE);

	memcpy(ois_info.wide_x_sr, rom_cal_buf + OIS_CAL_EEPROM_XGG_ADDR, OIS_CAL_EEPROM_SR_SIZE);
	memcpy(ois_info.wide_y_sr, rom_cal_buf + OIS_CAL_EEPROM_YGG_ADDR, OIS_CAL_EEPROM_SR_SIZE);

	ois_info.wide_x_gp = OIS_GYRO_WX_POLE;
	ois_info.wide_y_gp = OIS_GYRO_WY_POLE;
	ois_info.gyro_orientation = OIS_GYRO_ORIENTATION;

	return 0;
}

int cam_ois_mcu_update(void)
{
	LOG_INF("E");
	// if (!mcu_info.is_fw_updated && mcu_info.pinctrl != NULL && i2c_client != NULL) {
	if (mcu_info.pinctrl != NULL && i2c_client != NULL) {
		// if (!cam_ois_mcu_fw_update(&mcu_info, i2c_client))
		cam_ois_mcu_fw_update(&mcu_info, i2c_client);
			// mcu_info.is_fw_updated = 1;
	} else {
		// LOG_ERR("fw_updated %d, pinctrl %x, i2c %x", mcu_info.is_fw_updated, mcu_info.pinctrl, i2c_client);
		LOG_ERR("fw_updated %d, pinctrl %x, i2c %x", mcu_info.is_updated, mcu_info.pinctrl, i2c_client);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(cam_ois_mcu_update);

static void cam_ois_print_pinctrl(struct mcu_info *mcu_info)
{
	LOG_DBG("pinctrl: %x, mcu boot: %x, %x, mcu nRST: %x, %x, mcu VDD: %x, %x, ois VDD: %x, %x",
		mcu_info->pinctrl, mcu_info->mcu_boot_high, mcu_info->mcu_boot_low,
		mcu_info->mcu_nrst_high, mcu_info->mcu_nrst_low, mcu_info->mcu_vdd_high,
		mcu_info->mcu_vdd_low, mcu_info->ois_vdd_high, mcu_info->ois_vdd_low);
}

static void cam_ois_print_ois_info(void)
{
	LOG_DBG("xgg: 0x%x, 0x%x, 0x%x, 0x%x", ois_info.wide_x_gg[0],
		ois_info.wide_x_gg[1], ois_info.wide_x_gg[2], ois_info.wide_x_gg[3]);
	LOG_DBG("ygg: 0x%x, 0x%x, 0x%x, 0x%x", ois_info.wide_y_gg[0],
		ois_info.wide_y_gg[1], ois_info.wide_y_gg[2], ois_info.wide_y_gg[3]);
	LOG_DBG("x_coef: 0x%x, 0x%x", ois_info.wide_x_coef[0], ois_info.wide_x_coef[1]);
	LOG_DBG("y_coef: 0x%x, 0x%x", ois_info.wide_y_coef[0], ois_info.wide_y_coef[1]);
	LOG_DBG("wx_pole: 0x%x, wy_pole: 0x%x, orientation 0x%x", ois_info.wide_x_gp,
		ois_info.wide_y_gp, ois_info.gyro_orientation);
}

static void cam_ois_print_info(void)
{
	LOG_DBG("mcu info dev: 0x%x\n", mcu_info.pdev);
	if (mcu_info.pdev != NULL)
		LOG_DBG("mcu info dev name: %s\n", mcu_info.pdev->init_name);

	if (i2c_client) {
		LOG_DBG("mcu i2c client name: %s\n", i2c_client->name);
		LOG_DBG("mcu i2c addr: %x\n", i2c_client->addr);
	} else
		LOG_DBG("i2c client name: NULL\n");

	cam_ois_print_ois_info();
	cam_ois_print_pinctrl(&mcu_info);
	LOG_INF("mcu is_updated: %d\n", mcu_info.is_updated);
	cam_ois_mcu_print_fw_version(&mcu_info, NULL);
}

static int cam_ois_send_mode_to_mcu(int mode)
{
	int ret = 0;

	LOG_DBG("mode value(%d)\n", mode);
	switch (mode) {
	case OIS_MODE_STILL:
		ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0002, 0x00);
		break;
	case OIS_MODE_MOVIE:
		ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0002, 0x01);
		break;
	case OIS_MODE_CENTERING:
		ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0002, 0x05);
		break;
	case OIS_MODE_ZOOM:
		ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0002, 0x13);
		break;
	case OIS_MODE_VDIS:
		ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0002, 0x14);
		break;
	default:
		LOG_INF("undefined mode %d\n", mode);
		ret = -1;
		break;
	}
	return ret;
}

static int cam_ois_control_servo(u8 data)
{
	return cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0000, data);
}

static inline u8 cam_ois_check_mcu_status(struct i2c_client *i2c_client_in)
{
	u8 val = 0;
#if ENABLE_AOIS == 1
	val = 0x01;
#else
	int retries = 5;
	int ret = 0;

	do {
		ret = cam_ois_i2c_read_multi(i2c_client_in, MCU_I2C_SLAVE_W_ADDR, 0x01, &val, 1);
		if (ret != 0) {
			val = -EIO;
#ifdef IMGSENSOR_HW_PARAM
			imgsensor_increase_hw_param_ois_err_cnt(SENSOR_POSITION_REAR);
#endif
		}

		usleep_range(2000, 2500);
		if (--retries < 0) {
			LOG_ERR("read ois status failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val != 0x01);
#endif
	return val;
}

int cam_ois_get_offset_from_buf(const char *buffer, int efs_size,
	long *raw_data_x, long *raw_data_y, long *raw_data_z)
{
	int ret = 0, i = 0, j = 0, comma_offset = 0;
	bool detect_comma = false;
	int comma_offset_z = 0;
	bool detect_comma_z = false;
	char efs_data[MAX_EFS_DATA_LENGTH] = { 0 };
	int max_buf_size = 0;

	if (!buffer) {
		LOG_ERR("buffer is NULL");
		return -1;
	}

	if (efs_size == 0) {
		LOG_ERR("efs size zero");
		return -1;
	}

	max_buf_size = efs_size;
	i = 0;
	detect_comma = false;
	for (i = 0; i < efs_size; i++) {
		if (*(buffer + i) == ',') {
			comma_offset = i;
			detect_comma = true;
			break;
		}
	}

	for (i = comma_offset + 1; i < efs_size; i++) {
		if (*(buffer + i) == ',') {
			comma_offset_z = i;
			detect_comma_z = true;
			max_buf_size = comma_offset_z;
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
		for (i = comma_offset + 1; i < max_buf_size; i++) {
			if (buffer[i] != '.') {
				efs_data[j] = buffer[i];
				j++;
			}
		}
		kstrtol(efs_data, 10, raw_data_y);

		if (detect_comma_z) {
			memset(efs_data, 0x00, sizeof(efs_data));
			j = 0;
			for (i = comma_offset_z + 1; i < efs_size; i++) {
				if (buffer[i] != '.') {
					efs_data[j] = buffer[i];
					j++;
				}
			}
			kstrtol(efs_data, 10, raw_data_z);
		}
	} else {
		LOG_INF("cannot find delimeter");
		ret = -1;
	}
	LOG_INF("X raw_x = %ld, raw_y = %ld, raw_z = %ld", *raw_data_x, *raw_data_y, *raw_data_z);

	return ret;
}

int cam_ois_set_efs_gyro_cal(char *efs_gyro_cal)
{
	int ret = 0;
	long raw_data_x = 0;
	long raw_data_y = 0;
	long raw_data_z = 0;
	int xg_zero = 0, yg_zero = 0, zg_zero = 0;

	ois_info.wide_x_goffset[0] = 0;
	ois_info.wide_x_goffset[1] = 0;
	ois_info.wide_y_goffset[0] = 0;
	ois_info.wide_y_goffset[1] = 0;
	ois_info.wide_z_goffset[0] = 0;
	ois_info.wide_z_goffset[1] = 0;

	if (efs_gyro_cal == NULL) {
		LOG_ERR("efs_gyro_cal is null");
		return -1;
	}
	memcpy(ois_info.efs_gyro_cal, efs_gyro_cal, MAX_EFS_DATA_LENGTH);
	LOG_INF("efs_gyro_cal is %s", ois_info.efs_gyro_cal);
	ret = cam_ois_get_offset_from_buf(ois_info.efs_gyro_cal, MAX_EFS_DATA_LENGTH,
		&raw_data_x, &raw_data_y, &raw_data_z);
	if (ret < 0) {
		LOG_ERR("Failed to get gyro calibration data");
		return -1;
	}

	xg_zero = raw_data_x * OIS_GYRO_SCALE_FACTOR / 1000;
	yg_zero = raw_data_y * OIS_GYRO_SCALE_FACTOR / 1000;
	zg_zero = raw_data_z * OIS_GYRO_SCALE_FACTOR / 1000;
	if (xg_zero > 0x7FFF)
		xg_zero = -((xg_zero ^ 0xFFFF) + 1);
	if (yg_zero > 0x7FFF)
		yg_zero = -((yg_zero ^ 0xFFFF) + 1);
	if (zg_zero > 0x7FFF)
		zg_zero = -((zg_zero ^ 0xFFFF) + 1);

	LOG_INF("xgzero: 0x%x, ygzero: 0x%x, zgzero: 0x%x\n", xg_zero, yg_zero, zg_zero);

	ois_info.wide_x_goffset[0] = (xg_zero & 0xFF);
	ois_info.wide_x_goffset[1] = ((xg_zero >> 8) & 0xFF);

	ois_info.wide_y_goffset[0] = (yg_zero & 0xFF);
	ois_info.wide_y_goffset[1] = ((yg_zero >> 8) & 0xFF);

	ois_info.wide_z_goffset[0] = (zg_zero & 0xFF);
	ois_info.wide_z_goffset[1] = ((zg_zero >> 8) & 0xFF);

	return ret;
}
EXPORT_SYMBOL_GPL(cam_ois_set_efs_gyro_cal);

static int cam_ois_write_gyro_info_to_mcu(void)
{
	int ret = 0;

	LOG_INF("E");
	ret = cam_ois_i2c_write_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0254, ois_info.wide_x_gg, 4);
	ret |= cam_ois_i2c_write_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0258, ois_info.wide_y_gg, 4);
	if (ret < 0)
		LOG_ERR("fail to write ois gyro gains\n");

	ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0240, ois_info.wide_x_gp);
	ret |= cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0241, ois_info.wide_y_gp);
	ret |= cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0242, ois_info.gyro_orientation);
	if (ret < 0)
		LOG_ERR("fail to write gyro orientions\n");

	ret = cam_ois_i2c_write_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0248, ois_info.wide_x_goffset, 2);
	ret |= cam_ois_i2c_write_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x024A, ois_info.wide_y_goffset, 2);
	ret |= cam_ois_i2c_write_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x024C, ois_info.wide_z_goffset, 2);
	if (ret < 0)
		LOG_ERR("fail to write gyro offsets\n");

	return ret;
}

static int cam_ois_read_hall_center(short *hall_x_center, short *hall_y_center)
{
	int ret = 0;
	u8 x_data_array[2] = {0, };
	u8 y_data_array[2] = {0, };

	ret = cam_ois_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x021A, x_data_array, 2);
	ret |= cam_ois_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x021C, y_data_array, 2);
	if (ret < 0)
		LOG_ERR("fail to read hole center\n");

	*hall_x_center = (x_data_array[1] << 8) | x_data_array[0];
	*hall_y_center = (y_data_array[1] << 8) | y_data_array[0];
	return ret;
}

static int cam_ois_get_error_register(short *val)
{
	int ret = 0;
	u8 data[2] = {0, };

	ret = cam_ois_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0004, data, 2);
	if (ret != 0) {
		*val = -EIO;
		LOG_ERR("i2c read fail\n");
	} else {
		*val = (data[1] << 8) | data[0];
	}
	return ret;
}

static int cam_ois_read_error_register(void)
{
	int ret = 0;
	short val = 0;

	ret = cam_ois_get_error_register(&val);

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

static int cam_ois_set_init_data(void)
{
	int ret = 0;
	int ggfadeup_val = 1000;
	int ggfadedown_val = 1000;
	u8 data[2];

	LOG_INF("E");
	ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x003A, 0x80);
	if (ret < 0) {
		LOG_ERR("fail to write ois center shift data of wide to OIS MCU\n");
		return ret;
	}

	ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0039, 0x01);
	if (ret < 0) {
		LOG_ERR("fail to write VDIS setting data to OIS MCU\n");
		return ret;
	}

	data[0] = ggfadeup_val & 0xFF;
	data[1] = (ggfadeup_val >> 8) & 0xFF;
	ret = cam_ois_i2c_write_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0238, data, 2);
	if (ret < 0)
		LOG_ERR("fail to set fadeup\n");

	data[0] = ggfadedown_val & 0xFF;
	data[1] = (ggfadedown_val >> 8) & 0xFF;
	ret = cam_ois_i2c_write_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x023A, data, 2);
	if (ret < 0)
		LOG_ERR("fail to set fadedown\n");

	ret = cam_ois_read_hall_center(&(ois_info.x_hall_center), &(ois_info.y_hall_center));
	LOG_DBG("hall center x: %d, y: %d\n", ois_info.x_hall_center, ois_info.y_hall_center);

	ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x00BE, 0x01);
	if (ret < 0)
		LOG_ERR("fail to set ois single mode\n");

	ret = cam_ois_read_error_register();
	ret = cam_ois_send_mode_to_mcu(OIS_MODE_STILL);
	if (ret)
		LOG_ERR("fail to set OIS_MODE_STILL mode\n");

	return ret;
}

int cam_ois_set_af_client(struct i2c_client *i2c_client_in, spinlock_t *af_spinLock, int *af_opened,
							const struct file_operations *af_op)
{
	if (!i2c_client_in) {
		LOG_ERR("i2c_client_in is NULL\n", __func__);
		return -1;
	}
	ois_info.af_i2c_client = i2c_client_in;
	ois_info.af_spinLock = af_spinLock;
	ois_info.af_opened = af_opened;
	ois_info.af_op = af_op;
	LOG_INF("i2c client name: %s\n", ois_info.af_i2c_client->name);
	if (ois_info.af_i2c_client->adapter)
		LOG_INF("i2c adapter name: %s\n", ois_info.af_i2c_client->adapter->name);
	return 0;
}

int cam_ois_set_i2c_client(void)
{
	i2c_client = get_ois_i2c_client(OIS_I2C_DEV_IDX_1);
	if (i2c_client == NULL) {
		LOG_ERR("fail to get i2c client from OIS drv");
		return -1;
	}
	LOG_INF("i2c client name: %s\n", i2c_client->name);
	LOG_INF("i2c adapter name: %s\n", i2c_client->adapter->name);
	return 0;
}

int cam_ois_power_on(void)
{
	int ret = 0;

	ret = cam_ois_mcu_power_up(&mcu_info);

	return ret;
}

int cam_ois_power_off(void)
{
	int ret = 0;

	ret = cam_ois_mcu_power_down(&mcu_info);

	return ret;
}


int cam_ois_af_power_on(void)
{
	int ret = 0;

	ret = cam_ois_af_power_up(&mcu_info);

	return ret;
}

int cam_ois_af_power_off(void)
{
	int ret = 0;

	ret = cam_ois_af_power_down(&mcu_info);

	return ret;
}

int cam_ois_sysfs_power_onoff(bool onoff)
{
	int ret = 0;

	if (onoff == true)
		ret = cam_ois_sysfs_mcu_power_up(&mcu_info);
	else
		ret = cam_ois_sysfs_mcu_power_down(&mcu_info);

	return ret;
}

int cam_ois_init(void)
{
	int ret = 0;
	unsigned char val = 0;

	LOG_INF(" - E\n");

	mcu_info.prev_vdis_coef = 255;
	mcu_info.enabled = true;
	cam_ois_get_eeprom_from_sysfs();

	val = cam_ois_check_mcu_status(i2c_client);
	if (val != 0x01) {
		ret = -1;
		mcu_info.enabled = false;
		LOG_ERR("Fail to check mcu status -> cam_ois_write_gyro_info_to_mcu");

	} else {
		ret = cam_ois_write_gyro_info_to_mcu();
		val = cam_ois_check_mcu_status(i2c_client);
		if (val != 0x01) {
			ret = -1;
			mcu_info.enabled = false;
			LOG_ERR("Fail to check mcu status -> cam_ois_set_init_data");
		} else
			ret = cam_ois_set_init_data();
	}

	cam_ois_print_info();

	LOG_INF(" - X\n");
	return ret;
}

int cam_ois_deinit(void)
{
	int ret = 0;

	LOG_INF("ois_deinit E\n");

	if (mcu_info.enabled) {
		ret = cam_ois_control_servo(0x00);
		usleep_range(10000, 11000);
	}

	ois_info.af_pos_wide = 0;
	mcu_info.enabled = false;

	return ret;
}


int cam_ois_center_shift(unsigned long position)
{
	int ret = 0;
	int position_changed;

	if (!mcu_info.enabled) {
		LOG_INF("OIS MCU is not enabled");
		return -1;
	}

	position_changed = ((position & 0x3FF) >> 2); // convert to 8 bit
	if (position_changed > 255) {
		LOG_ERR("position_changed over 8 bits %d\n", position_changed);
		return -1;
	}
	if (ois_info.af_pos_wide != position_changed) {
		ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x003A, (u8)position_changed);
		if (ret < 0) {
			LOG_ERR("fails to write ois wide center shift\n");
			return ret;
		}
		LOG_DBG("af position changed: %d\n", position_changed);
		ois_info.af_pos_wide = position_changed;
	}
	return ret;
}

int cam_ois_set_mode(int mode)
{
	int ret = 0;

	if (!mcu_info.enabled) {
		LOG_INF("OIS MCU is not enabled");
		return -1;
	}

	LOG_INF("mode value(%d)\n", mode);
	ret = cam_ois_send_mode_to_mcu(mode);
	ret |= cam_ois_control_servo(0x01);
	if (ret)
		LOG_ERR("fail to set ois mode %d", mode);
	return ret;
}

int cam_ois_set_vdis_coef(int inCoef)
{
	int ret = 0;
	unsigned char coef = (unsigned char)inCoef;

	if (!mcu_info.enabled) {
		LOG_INF("OIS MCU is not enabled");
		return -1;
	}

	if (mcu_info.prev_vdis_coef == coef)
		return ret;

	LOG_INF("vdis coef(%u)\n", (unsigned char)coef);
	ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x005E, coef);
	if (ret)
		LOG_ERR("fail to set vdis coefficient %u", (unsigned char)coef);
	else
		mcu_info.prev_vdis_coef = coef;
	return ret;
}

int cam_ois_sysfs_gyro_cal(long *out_gyro_x, long *out_gyro_y, long *out_gyro_z)
{
	int result = 2;

	result = cam_ois_gyro_calibration(&mcu_info, i2c_client, out_gyro_x, out_gyro_y, out_gyro_z);
	if (result < 1)
		LOG_ERR("gyro calibration fail");

	return result;
}

int cam_ois_sysfs_get_gyro_gain_from_eeprom(u32 *xgg, u32 *ygg)
{
	int ret = 0;

	ret = cam_ois_get_eeprom_from_sysfs();
	if (ret < 0)
		return 1;

	memcpy(xgg, ois_info.wide_x_gg, 4);
	memcpy(ygg, ois_info.wide_y_gg, 4);

	return 0;
}

int cam_ois_sysfs_get_supperssion_ratio_from_eeprom(u32 *xsr, u32 *ysr)
{
	int ret = 0;

	ret = cam_ois_get_eeprom_from_sysfs();
	if (ret < 0)
		return 1;

	memcpy(xsr, ois_info.wide_x_sr, 2);
	memcpy(ysr, ois_info.wide_y_sr, 2);

	return 0;
}

int cam_ois_sysfs_read_gyro_offset(const char *buf, int size,
	long *out_gyro_x, long *out_gyro_y, long *out_gyro_z)
{
	return cam_ois_get_offset_data(&mcu_info, i2c_client, buf, size, out_gyro_x, out_gyro_y, out_gyro_z);
}

int cam_ois_sysfs_read_gyro_offset_test(long *out_gyro_x, long *out_gyro_y, long *out_gyro_z)
{
	return cam_ois_get_offset_testdata(&mcu_info, i2c_client, out_gyro_x, out_gyro_y, out_gyro_z);
}

int cam_ois_sysfs_selftest(void)
{
	return  cam_ois_selftest(&mcu_info, i2c_client);
}

int cam_ois_sysfs_get_oisfw_version(char *ois_mcu_fw_ver)
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

static int cam_ois_af_drv_init(char *af_name)
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

	ret = ois_info.cur_af_drv->pAF_SetI2Cclient(ois_info.af_i2c_client, ois_info.af_spinLock, ois_info.af_opened);
	if (ret != 1) {
		LOG_ERR("fail to set AF I2C client");
		return ret;
	}

	return ret;
}

static int cam_ois_af_setup(void)
{
	int ret = 0;
	unsigned int command = 0;
	unsigned long param = 0;
	int retry = 3;
	int i = 0;

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

	for (i = 0; i < retry; i++) {
		ret = ois_info.cur_af_drv->pAF_Ioctl(NULL, command, param);
		if (!ret) {
			LOG_ERR("AFIOC_S_SETOISAUTO success");
			break;
		}
	}
	if (i == retry)
		LOG_ERR("AFIOC_S_SETOISAUTO fail");

	return ret;
}

static int cam_ois_af_drv_deinit(void)
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

int cam_ois_sysfs_autotest(int *sin_x, int *sin_y, int *result_x, int *result_y, int threshold)
{
	int ret = 0;
	int sw_ret = 0;
	int result = 0;

	LOG_INF("E");

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_on();
#endif

	ret = cam_ois_init();
	if (ret) {
		LOG_ERR("fail to init");
#if ENABLE_AOIS == 1
		cam_ois_set_aois_fac_mode_off();
#endif
		return ret;
	}

	ret = cam_ois_af_drv_init(OIS_AF_MAIN_NAME);
	ret = cam_ois_af_setup();
	if (ret == 0) {

		msleep(100);
		sw_ret = cam_ois_sine_wavecheck(sin_x, sin_y, &result, i2c_client, threshold);
	} else {
		LOG_ERR("AF device is not ready");
		sw_ret = -1;
	}

	if (sw_ret == 0) {
		*result_x = true;
		*result_y = true;
		ret = 0;
	} else if (sw_ret == -1) {
		LOG_ERR("OIS device is not prepared.");
		*result_x = false;
		*result_y = false;
		*sin_x = 0;
		*sin_y = 0;
		ret = -1;
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

	cam_ois_af_drv_deinit();

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_off();
#endif
	return ret;
}

int cam_ois_sysfs_set_mode(int mode)
{
	int ret = 0;
	int internal_mode = 0;

	LOG_INF("E");

	if (mcu_info.power_count == 0) {
		LOG_ERR("MCU power condition: off");
		return -1;
	}
#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_on();
#endif

	ret = cam_ois_init();
	if (ret) {
		LOG_ERR("fail to init");

#if ENABLE_AOIS == 1
		cam_ois_set_aois_fac_mode_off();
#endif
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

	ret = cam_ois_set_mode(internal_mode);
	if (ret)
		LOG_ERR("fail to set mode");

#if ENABLE_AOIS == 1
		cam_ois_set_aois_fac_mode_off();
#endif
	return ret;
}

int cam_ois_sysfs_get_mcu_error(int *w_error)
{
	int ret = 0;
	short err_reg_val = 0;

	LOG_INF("E");

	*w_error = 1;
	if (mcu_info.power_count == 0) {
		LOG_ERR("MCU power condition: off");
		return -1;
	}

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_on();
#else
	ret = cam_ois_init();
	if (ret) {
		LOG_ERR("fail to init");
		return ret;
	}
#endif

	ret = cam_ois_get_error_register(&err_reg_val);
	if (ret)
		LOG_ERR("fail to get error register mode");
	else
		*w_error = ((err_reg_val & 0x0600) >> 8);
	/* i2c communication error */
	/* b9(0x0200): wide cam x, b10(0x0400): widecam y */

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_off();
#endif
	return ret;
}

int cam_ois_gyro_sensor_noise_check(long *stdev_data_x, long *stdev_data_y)
{
	int ret = 0, result = 1;
	u8 rcv_data_array[2] = {0, };
	int xgnoise_val = 0, ygnoise_val = 0;
	int retries = 100;
	int scale_factor = OIS_GYRO_SCALE_FACTOR;

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_on();
#endif

	/* OIS Servo Off */
	ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0000, 0x00);
	if (ret < 0) {
		LOG_ERR("OIS Servo Off: i2c write fail %d", ret);
#if ENABLE_AOIS == 1
		cam_ois_set_aois_fac_mode_off();
#endif
		return 0;
	}

	/* Waiting for Idle */
	ret = cam_ois_mcu_wait_idle(i2c_client, 1);
	if (ret < 0) {
		LOG_ERR("wait ois idle status failed");
#if ENABLE_AOIS == 1
		cam_ois_set_aois_fac_mode_off();
#endif
		return 0;
	}

	/* Gyro Noise Measure Start */
	ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0029, 0x01);
	if (ret < 0) {
		LOG_ERR("Gyro Noise Measure Start: i2c write fail %d", ret);
#if ENABLE_AOIS == 1
		cam_ois_set_aois_fac_mode_off();
#endif
		return 0;
	}

	/* Check Noise Measure End */
	do {
		ret = cam_ois_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0029, rcv_data_array, 1);
		if (ret < 0) {
			LOG_ERR("Check Noise Measure End: i2c read fail %d", ret);
			result = 0;
		}

		if (--retries < 0) {
			LOG_ERR("Check Noise Measure End: read failed %d, retry %d", rcv_data_array[0], retries);
			ret = -1;
			break;
		}
		usleep_range(10000, 11000);
	} while (rcv_data_array[0] != 0);

	/* XGN_STDEV */
	ret = cam_ois_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x024E, rcv_data_array, 2);
	if (ret < 0) {
		LOG_ERR("XGN_STDEV: i2c read fail %d", ret);
		result = 0;
	}

	xgnoise_val = (rcv_data_array[1] << 8) | rcv_data_array[0];
	if (xgnoise_val > 0x7FFF)
		xgnoise_val = -((xgnoise_val ^ 0xFFFF) + 1);

	/* YGN_STDEV */
	ret = cam_ois_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0250, rcv_data_array, 2);
	if (ret < 0) {
		LOG_ERR("YGN_STDEV: i2c read fail %d", ret);
		result = 0;
	}

	ygnoise_val = (rcv_data_array[1] << 8) | rcv_data_array[0];
	if (ygnoise_val > 0x7FFF)
		ygnoise_val = -((ygnoise_val ^ 0xFFFF) + 1);

	*stdev_data_x = xgnoise_val * 1000 / scale_factor;
	*stdev_data_y = ygnoise_val * 1000 / scale_factor;

	LOG_INF("result: %d, stdev_x: %ld (0x%x), stdev_y: %ld (0x%x)", result, *stdev_data_x, xgnoise_val, *stdev_data_y, ygnoise_val);

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_off();
#endif
	return result;
}

int cam_ois_wait_ois_condition(struct i2c_client *client, unsigned short addr,
	unsigned short exp_cond, int retries, unsigned char msec)
{
	u8 status = 0;
	int ret = 0;

	do {
		if (msec < 10)
			usleep_range(msec * 1000, msec * 1100);
		else
			msleep(msec);

		ret = cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, addr, &status, 1);
		if (status == exp_cond)
			break;
		if (--retries < 0) {
			if (ret < 0) {
				LOG_ERR("failed due to i2c fail");
				return -EIO;
			}
			LOG_ERR("expected status: 0x%04x, current status: 0x%04x", exp_cond, status);
			return -EBUSY;
		}

	} while (status != exp_cond);
	return 0;
}

void cam_ois_get_hall_position(unsigned int *targetPosition, unsigned int *hallPosition)
{
	int ret = 0, i = 0, retries = 5;
	unsigned char servoCondition = 0;
	unsigned char recv[4];

	LOG_INF(" - E");

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_on();
#endif

	LOG_INF(" - Centering mode ");
	cam_ois_set_mode(OIS_MODE_CENTERING);


	LOG_INF("1: set hall read control bit");
	/* Set hall read ctrl bit */
	ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0080, 0x01);
	if (ret < 0)
		LOG_ERR("fail set ON hall read control bit: %d", ret);

	LOG_INF("2: check OIS condition");
	ret = cam_ois_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0000, &servoCondition, 1);
	if (ret < 0)
		LOG_ERR("fail read servo_on %d", ret);

	LOG_INF("3: run OIS depending on OIS Condition %d", servoCondition);
	if (servoCondition != 0x01) {
		LOG_INF("3-1: enable OIS");
		ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0000, 0x01);
		if (ret < 0)
			LOG_ERR("fail to set servo_on bit: %d", ret);
	}

	msleep(200);

	LOG_INF("4: GET OIS HALL POSITION");
	for (i = 0; i < retries; i++) {
		usleep_range(5000, 5100);
		ret = cam_ois_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0086, recv, 2);
		if (ret < 0)
			LOG_ERR("fail read target hall pos %d", ret);

		targetPosition[0] += (recv[1] << 8) | recv[0];

		ret = cam_ois_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x008E, recv, 2);
		if (ret < 0)
			LOG_ERR("fail read out hall pos %d", ret);

		hallPosition[0] += (recv[1] << 8) | recv[0];

		ret = cam_ois_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0088, recv, 2);
		if (ret < 0)
			LOG_ERR("fail read target hall pos %d", ret);

		targetPosition[1] += (recv[1] << 8) | recv[0];

		ret = cam_ois_i2c_read_multi(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0090, recv, 2);
		if (ret < 0)
			LOG_ERR("fail read out hall pos %d", ret);

		hallPosition[1] += (recv[1] << 8) | recv[0];

		LOG_INF("retries %d target (%u %u), hall (%u, %u)", i,
			targetPosition[0], targetPosition[1], hallPosition[0], hallPosition[1]);

	}

	for (i = 0; i < 2; i++) {
		targetPosition[i] /= retries;
		hallPosition[i] /= retries;
	}

	LOG_INF("5: disable FWINFO_CTRL");
	ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0080, 0x0);
	if (ret < 0)
		LOG_ERR("fail to set OFF hall read control bit: %d", ret);

	LOG_INF("6: REVERT SERVO %d", servoCondition);
	if (servoCondition != 0x1) {
		LOG_INF("6-1: SERVO OFF - %d", servoCondition);
		ret = cam_ois_i2c_write_one(i2c_client, MCU_I2C_SLAVE_W_ADDR, 0x0000, servoCondition);
		if (ret < 0)
			LOG_ERR("fail to set servo bit(%d): %d", servoCondition, ret);
	}

	LOG_INF("out: %u, %u, %u, %u\n", targetPosition[0], targetPosition[1],
		hallPosition[0], hallPosition[1]);

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_off();
#endif
	LOG_INF(" - X");
}

