#include <linux/delay.h>
#include <linux/i2c.h>

#include "cam_ois_define.h"
#include "cam_ois_common.h"
#include "cam_ois_i2c.h"
#include "cam_ois.h"
#include "camera_ois.h"
#include "cam_ois_sysfs.h"
#include "cam_ois_aois_if.h"
#include "kd_imgsensor_sysfs_adapter.h"

static int cam_ois_get_gc_status(struct i2c_client *client, int retries)
{
	int ret = 0;
	u8 recv = 0;

	/* Check Gyro Calibration Sequence End */
	do {
		/* GCCTRL Read */
		ret = cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x0014, &recv, 1);
		if (ret < 0)
			LOG_WARN("i2c read fail %d", ret);
		if (--retries < 0) {
			LOG_ERR("GCCTRL Read failed %d", recv);
			ret = -1;
			break;
		}
		usleep_range(20000, 21000);
	} while (recv != 0x00);

	return ret;
}

static int cam_ois_get_gyro_cal_result(long *out_data, struct i2c_client *client, u16 addr)
{
	int scale_factor = OIS_GYRO_SCALE_FACTOR;
	int GZERO;
	int ret = 0;
	u8 recv = 0, tmp = 0;

	ret = cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, addr, &recv, 1);
	tmp = recv;
	ret = cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, addr + 1, &recv, 1);
	GZERO = (recv << 8) | tmp;
	if (GZERO > 0x7FFF)
		GZERO = -((GZERO ^ 0xFFFF) + 1);

	*out_data = GZERO * 1000 / scale_factor;
	return ret;
}

int cam_ois_gyro_calibration(struct mcu_info *mcu_info, struct i2c_client *client,
	long *out_gyro_x, long *out_gyro_y, long *out_gyro_z)
{
	int ret = 0, result = 0;
	u8 recv = 0;
	int retries = 30;

	LOG_INF("Enter");
	if (!client) {
		LOG_ERR("i2c client is null");
		return result;
	}

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_on();
#endif

	ret = cam_ois_mcu_wait_idle(client, retries);
	if (!ret) {
		/* Gyro Calibration Start */
		/* GCCTRL GSCEN set */
		/* GCCTRL register(0x0014) 1Byte Send */
		ret = cam_ois_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0014, 0x01);
		if (ret < 0)
			LOG_WARN("i2c write fail %d", ret);

		/* Check Gyro Calibration Sequence End */
		ret = cam_ois_get_gc_status(client, 80);
		if (ret < 0)
			LOG_WARN("fail to get gyro calibration status %d", ret);

		/* Result check */
		/* OISERR Read */
		ret = cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x0004, &recv, 1);
		/* OISERR register GXZEROERR & GYZEROERR & GCOMERR Bit = 0(No Error)*/
		if ((ret >= 0) && ((recv & 0x23) == 0x00)) {
			LOG_INF("mcu gyro calibration ok %d", recv);
			result = 1;
		} else {
			LOG_ERR("mcu gyro calibration fail, ret %d, recv %d", ret, recv);
			result = 0;
		}

		cam_ois_get_gyro_cal_result(out_gyro_x, client, 0x0248);
		cam_ois_get_gyro_cal_result(out_gyro_y, client, 0x024A);
		cam_ois_get_gyro_cal_result(out_gyro_z, client, 0x024C);

		LOG_INF("result %d, raw_data_x %ld, raw_data_y %ld, raw_data_z %ld", result, *out_gyro_x, *out_gyro_y, *out_gyro_z);
	}

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_off();
#endif

	return result;
}

int cam_ois_get_offset_data(struct mcu_info *mcu_info, struct i2c_client *client,
	const char *buf, int size, long *out_gyro_x, long *out_gyro_y, long *out_gyro_z)
{
	int ret = 0;

	LOG_INF("E\n");

	ret = cam_ois_get_offset_from_buf(buf, size, out_gyro_x, out_gyro_y, out_gyro_z);
	if (ret)
		LOG_WARN("fail to get offset from efs buf");

	LOG_INF("X raw_x = %ld, raw_y = %ld, raw_z = %ld\n", *out_gyro_x, *out_gyro_y, *out_gyro_z);
	return ret;
}

int cam_ois_get_offset_testdata(struct mcu_info *mcu_info, struct i2c_client *client,
	long *out_gyro_x, long *out_gyro_y, long *out_gyro_z)
{
	int i;
	int ret = 0, result = 0;
	u8 recv = 0;
	long x_sum = 0, y_sum = 0, z_sum = 0, x = 0, y = 0, z = 0;
	int retries = 20;

	LOG_INF("E\n");

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_on();
#endif

	ret = cam_ois_mcu_wait_idle(client, retries);
	if (ret)
		LOG_ERR("OIS is NOT IDLE");

	/* Gyro Calibration Start */
	/* GCCTRL GSCEN set */
	/* GCCTRL register(0x0014) 1Byte Send */
	ret = cam_ois_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0014, 0x01);
	if (ret < 0)
		LOG_WARN("i2c write fail %d", ret);

#if ENABLE_AOIS == 1
	msleep(200);
#endif

	/* Check Gyro Calibration Sequence End */
	ret = cam_ois_get_gc_status(client, 80);
	if (ret < 0) {
		LOG_ERR("fail to get gyro calibration status %d", ret);
#if ENABLE_AOIS == 1
		cam_ois_set_aois_fac_mode_off();
#endif
		return result;
	}

	/* Result check */
	/* OISERR Read */
	ret = cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x0004, &recv, 1);
	/* OISERR register GXZEROERR & GYZEROERR & GCOMERR Bit = 0(No Error)*/
	if ((ret >= 0) && ((recv & 0x23) == 0x00)) {
		LOG_INF("mcu gyro calibration ok, recv: %d", recv);
		result = 1;
	} else {
		LOG_ERR("mcu gyro calibration fail, ret %d, recv %d", ret, recv);
		result = 0;
	}

	for (i = 0; i < retries; i++) {
		cam_ois_get_gyro_cal_result(&x, client, 0x0248);
		cam_ois_get_gyro_cal_result(&y, client, 0x024A);
		cam_ois_get_gyro_cal_result(&z, client, 0x024C);
		x_sum += x;
		y_sum += y;
		z_sum += z;
	}

	*out_gyro_x = x_sum / retries;
	*out_gyro_y = y_sum / retries;
	*out_gyro_z = z_sum / retries;

	LOG_INF("X raw_x = %ld, raw_y = %ld, raw_z = %ld\n", *out_gyro_x, *out_gyro_y, *out_gyro_z);

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_off();
#endif
	return result;
}

int cam_ois_selftest(struct mcu_info *mcu_info, struct i2c_client *client)
{
	int ret = 0;
	u8 recv = 0;
	int retries = 30;

	LOG_INF("E\n");

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_on();
#endif

	ret = cam_ois_mcu_wait_idle(client, retries);
	if (ret) {
		LOG_ERR("failt to wait idle");
		return -1;
	}

	ret = cam_ois_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0014, 0x08);
	if (ret)
		LOG_WARN("i2c write fail %d", ret);

	do {
		ret = cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x0014, &recv, 1);
		if (ret != 0) {
			recv = -EIO;
			break;
		}
		usleep_range(20000, 21000);
		if (--retries < 0) {
			LOG_ERR("Read register failed!!!!, data = 0x%04x\n", recv);
			break;
		}
	} while (recv != 0x00);

	ret = cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x0004, &recv, 1);
	if (ret != 0) {
		LOG_ERR("i2c error");
		recv = -EIO;
	}

	if ((recv & 0x80) != 0x0) {
		/* Gyro Sensor Self Test Error Process */
		LOG_ERR("GyroSensorSelfTest failed %d\n", recv);
#if ENABLE_AOIS == 1
		cam_ois_set_aois_fac_mode_off();
#endif
		return -1;
	}

	LOG_INF("- X, recv: %d, ret: %d\n", recv, ret);

#if ENABLE_AOIS == 1
	cam_ois_set_aois_fac_mode_off();
#endif
	return (int)recv;
}

int cam_ois_sine_wavecheck(int *sin_x, int *sin_y, int *result, struct i2c_client *client, int threshold)
{
	u8 val[3] = {0, };
	int ret = 0, retries = 10;
	int sinx_count = 0, siny_count = 0;

	if (!client) {
		LOG_ERR("i2c client is null");
		return -1;
	}

	/* auto test setting */
	/* select module */
	ret = cam_ois_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x00BE, 0x01);
	/* error threshold level. */
	ret |= cam_ois_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0052, (u8)threshold);
	ret |= cam_ois_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0053, 0x00);
	ret |= cam_ois_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0054, 0x05);
	ret |= cam_ois_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0055, 0x2A);
	ret |= cam_ois_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0056, 0x03);
	ret |= cam_ois_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0057, 0x02);

	/* auto test start */
	ret |= cam_ois_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0050, 0x01);
	if (ret) {
		LOG_ERR("i2c write fail");
		*result = 0xFF;
		return -1;
	}
	LOG_INF("i2c write success");

	retries = 60;
	do {
		ret = cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x0050, val, 1);
		if (ret < 0) {
			LOG_ERR("i2c read fail");
			break;
		}

		usleep_range(20000, 21000);

		if (--retries < 0) {
			LOG_ERR("sine wave operation fail.");
			*result = 0xFF;
			return -1;
		}
	} while (val[0]);

	ret = cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x004C, val, 1);
	if (ret < 0)
		LOG_ERR("i2c read fail");

	*result = (int)val[0];
	if (val[0] != 0x00)
		LOG_ERR("MCERR_W(0x004C)=%d", val[0]);
	else
		LOG_INF("MCERR_W(0x004C)=%d", val[0]);

	/* read test results */
	ret = cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0xC0, val, 2);
	sinx_count = (val[1] << 8) | val[0];
	if (sinx_count > 0x7FFF)
		sinx_count = -((sinx_count ^ 0xFFFF) + 1);

	ret |= cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0xC2, val, 2);
	siny_count = (val[1] << 8) | val[0];
	if (siny_count > 0x7FFF)
		siny_count = -((siny_count ^ 0xFFFF) + 1);

	ret |= cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0xC4, val, 2);
	*sin_x = (val[1] << 8) | val[0];
	if (*sin_x > 0x7FFF)
		*sin_x = -((*sin_x ^ 0xFFFF) + 1);

	ret |= cam_ois_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0xC6, val, 2);
	*sin_y = (val[1] << 8) | val[0];
	if (*sin_y > 0x7FFF)
		*sin_y = -((*sin_y ^ 0xFFFF) + 1);

	if (ret < 0) {
		LOG_WARN("i2c read fail");
		return -1;
	}

	LOG_INF("threshold = %d, sinx = %d, siny = %d, sinx_count = %d, siny_count = %d",
	threshold, *sin_x, *sin_y, sinx_count, siny_count);

	if (*result == 0x00)
		return 0;

	return -2;
}

#ifdef CONFIG_IMGSENSOR_SYSFS
#define OIS_SYSFS_FW_VER_SIZE       40
struct device  *camera_ois_dev;
long raw_init_x;
long raw_init_y;
long raw_init_z;
uint32_t ois_autotest_threshold = 150;
struct class *ois_camera_class;

static ssize_t ois_calibration_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int result = 0;
	long raw_data_x = 0, raw_data_y = 0, raw_data_z = 0;

	LOG_INF(" - E");

	cam_ois_sysfs_power_onoff(1);

	msleep(100);

	result = cam_ois_sysfs_gyro_cal(&raw_data_x, &raw_data_y, &raw_data_z);

	cam_ois_sysfs_power_onoff(0);

	LOG_INF(" - X");

	if (raw_data_x < 0 && raw_data_y < 0) {
		if (raw_data_z < 0)
			return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n", result,
				abs(raw_data_x / 1000), abs(raw_data_x % 1000),
				abs(raw_data_y / 1000), abs(raw_data_y % 1000),
				abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		else
			return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,-%ld.%03ld,%ld.%03ld\n", result,
				abs(raw_data_x / 1000), abs(raw_data_x % 1000),
				abs(raw_data_y / 1000), abs(raw_data_y % 1000),
				abs(raw_data_z / 1000), abs(raw_data_z % 1000));
	} else if (raw_data_x < 0) {
		if (raw_data_z < 0)
			return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,%ld.%03ld,-%ld.%03ld\n", result,
				abs(raw_data_x / 1000), abs(raw_data_x % 1000),
				abs(raw_data_y / 1000), abs(raw_data_y % 1000),
				abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		else
			return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,%ld.%03ld,%ld.%03ld\n", result,
				abs(raw_data_x / 1000), abs(raw_data_x % 1000),
				abs(raw_data_y / 1000), abs(raw_data_y % 1000),
				abs(raw_data_z / 1000), abs(raw_data_z % 1000));
	} else if (raw_data_y < 0) {
		if (raw_data_z < 0)
			return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n", result,
				abs(raw_data_x / 1000), abs(raw_data_x % 1000),
				abs(raw_data_y / 1000), abs(raw_data_y % 1000),
				abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		else
			return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,-%ld.%03ld,%ld.%03ld\n", result,
				abs(raw_data_x / 1000), abs(raw_data_x % 1000),
				abs(raw_data_y / 1000), abs(raw_data_y % 1000),
				abs(raw_data_z / 1000), abs(raw_data_z % 1000));
	} else {
		if (raw_data_z < 0)
			return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,%ld.%03ld,-%ld.%03ld\n", result,
				abs(raw_data_x / 1000), abs(raw_data_x % 1000),
				abs(raw_data_y / 1000), abs(raw_data_y % 1000),
				abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		else
			return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,%ld.%03ld,%ld.%03ld\n", result,
				abs(raw_data_x / 1000), abs(raw_data_x % 1000),
				abs(raw_data_y / 1000), abs(raw_data_y % 1000),
				abs(raw_data_z / 1000), abs(raw_data_z % 1000));
	}
}

static ssize_t ois_gain_rear_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	u32 xgg = 0, ygg = 0;
	int result = 2; //0:normal, 1: No cal, 2: rear cal fail

	result = cam_ois_sysfs_get_gyro_gain_from_eeprom(&xgg, &ygg);
	LOG_INF("result : %d\n", result);

	if (result == 0)
		ret = scnprintf(buf, PAGE_SIZE, "%d,0x%x,0x%x", result, xgg, ygg);
	else
		ret = scnprintf(buf, PAGE_SIZE, "%d", result);

	if (ret)
		return ret;
	return 0;
}

static ssize_t ois_supperssion_ratio_rear_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	u32 xsr = 0, ysr = 0;
	int result = 2; //0:normal, 1: No cal, 2: rear cal fail

	result = cam_ois_sysfs_get_supperssion_ratio_from_eeprom(&xsr, &ysr);
	LOG_INF("result : %d\n", result);

	if (result == 0) {
		ret = scnprintf(buf, PAGE_SIZE, "%d,%u.%02u,%u.%02u",
			result, (xsr / 100), (xsr % 100), (ysr / 100), (ysr % 100));
	} else {
		ret = scnprintf(buf, PAGE_SIZE, "%d", result);
	}

	if (ret)
		return ret;
	return ret;
}

static ssize_t ois_rawdata_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	long raw_data_x = 0, raw_data_y = 0, raw_data_z = 0;

	cam_ois_sysfs_read_gyro_offset(buf, size, &raw_data_x, &raw_data_y, &raw_data_z);

	raw_init_x = raw_data_x;
	raw_init_y = raw_data_y;
	raw_init_z = raw_data_z;
	LOG_INF("efs data = %s, size = %ld, raw x = %ld, raw y = %ld, raw z = %ld",
			buf, size, raw_data_x, raw_data_y, raw_data_z);

	return size;
}

static ssize_t ois_rawdata_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	long raw_data_x = 0, raw_data_y = 0, raw_data_z = 0;

	raw_data_x = raw_init_x;
	raw_data_y = raw_init_y;
	raw_data_z = raw_init_z;

	LOG_INF("raw data x = %ld.%03ld, raw data y = %ld.%03ld, raw data z = %ld.%03ld\n",
		raw_data_x / 1000, raw_data_x % 1000,
		raw_data_y / 1000, raw_data_y % 1000,
		raw_data_z / 1000, raw_data_z % 1000);

	if (raw_data_x < 0 && raw_data_y < 0) {
		if (raw_data_z < 0)
			rc = scnprintf(buf, PAGE_SIZE, "-%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n",
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
		else
			rc = scnprintf(buf, PAGE_SIZE, "-%ld.%03ld,-%ld.%03ld,%ld.%03ld\n",
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
	} else if (raw_data_x < 0) {
		if (raw_data_z < 0)
			rc = scnprintf(buf, PAGE_SIZE, "-%ld.%03ld,%ld.%03ld,-%ld.%03ld\n",
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
		else
			rc = scnprintf(buf, PAGE_SIZE, "-%ld.%03ld,%ld.%03ld,%ld.%03ld\n",
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
	} else if (raw_data_y < 0) {
		if (raw_data_z < 0)
			rc = scnprintf(buf, PAGE_SIZE, "%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n",
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
		else
			rc = scnprintf(buf, PAGE_SIZE, "%ld.%03ld,-%ld.%03ld,%ld.%03ld\n",
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
	} else {
		if (raw_data_z < 0)
			rc = scnprintf(buf, PAGE_SIZE, "%ld.%03ld,%ld.%03ld,-%ld.%03ld\n",
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
		else
			rc = scnprintf(buf, PAGE_SIZE, "%ld.%03ld,%ld.%03ld,%ld.%03ld\n",
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
	}
	if (rc)
		return rc;
	return 0;
}

static ssize_t ois_selftest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int result_total = 0;
	bool result_offset = 0, result_selftest = 0;
	uint32_t selftest_ret = 0, offsettest_ret = 0;
	long raw_data_x = 0, raw_data_y = 0, raw_data_z = 0;

	offsettest_ret = cam_ois_sysfs_read_gyro_offset_test(&raw_data_x, &raw_data_y, &raw_data_z);
	msleep(50);
	selftest_ret = cam_ois_sysfs_selftest();

	if (selftest_ret == 0x0)
		result_selftest = true;
	else
		result_selftest = false;

	if ((offsettest_ret < 0) ||
		abs(raw_data_x) > OIS_GYRO_OFFSET_SPEC ||
		abs(raw_data_y) > OIS_GYRO_OFFSET_SPEC ||
		abs(raw_data_z) > OIS_GYRO_OFFSET_SPEC ||
		abs(raw_init_x - raw_data_x) > OIS_GYRO_OFFSET_SPEC ||
		abs(raw_init_y - raw_data_y) > OIS_GYRO_OFFSET_SPEC ||
		abs(raw_init_z - raw_data_z) > OIS_GYRO_OFFSET_SPEC)
		result_offset = false;
	else
		result_offset = true;

	if (result_offset && result_selftest)
		result_total = 0;
	else if (!result_offset && !result_selftest)
		result_total = 3;
	else if (!result_offset)
		result_total = 1;
	else if (!result_selftest)
		result_total = 2;

	sprintf(buf, "Result : %d, result x = %ld.%03ld, result y = %ld.%03ld, result z = %ld.%03ld\n",
		result_total, raw_data_x / 1000, (long)abs(raw_data_x % 1000),
		raw_data_y / 1000, (long)abs(raw_data_y % 1000), raw_data_z / 1000, (long)abs(raw_data_z % 1000));
	LOG_INF("Result : 0 (success), 1 (offset fail), 2 (selftest fail) , 3 (both fail)\n");
	LOG_INF("%s", buf);

	if (raw_data_x < 0 && raw_data_y < 0) {
		if (raw_data_z < 0)
			return sprintf(buf, "%d,-%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n", result_total,
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
		else
			return sprintf(buf, "%d,-%ld.%03ld,-%ld.%03ld,%ld.%03ld\n", result_total,
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
	} else if (raw_data_x < 0) {
		if (raw_data_z < 0)
			return sprintf(buf, "%d,-%ld.%03ld,%ld.%03ld,-%ld.%03ld\n", result_total,
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
		else
			return sprintf(buf, "%d,-%ld.%03ld,%ld.%03ld,%ld.%03ld\n", result_total,
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
	} else if (raw_data_y < 0) {
		if (raw_data_z < 0)
			return sprintf(buf, "%d,%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n", result_total,
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
		else
			return sprintf(buf, "%d,%ld.%03ld,-%ld.%03ld,%ld.%03ld\n", result_total,
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
	} else {
		if (raw_data_z < 0)
			return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld,-%ld.%03ld\n", result_total,
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
		else
			return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld,%ld.%03ld\n", result_total,
				(long)abs(raw_data_x / 1000), (long)abs(raw_data_x % 1000),
				(long)abs(raw_data_y / 1000), (long)abs(raw_data_y % 1000),
				(long)abs(raw_data_z / 1000), (long)abs(raw_data_z % 1000));
	}
	return 0;
}

static ssize_t ois_power_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	LOG_INF(" - E");
	switch (buf[0]) {
	case '0':
		cam_ois_sysfs_power_onoff(0);
		LOG_INF("power down");
		break;
	case '1':
		cam_ois_sysfs_power_onoff(1);
		msleep(200);
		LOG_INF("power up");
		break;

	default:
		LOG_ERR("out of value %d", buf[0]);
		break;
	}
	return size;
}

static ssize_t ois_autotest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int sin_x = 0, sin_y = 0, result_x = 0, result_y = 0;

	cam_ois_af_power_on();

	msleep(50);

	ret = cam_ois_sysfs_autotest(&sin_x, &sin_y, &result_x, &result_y, ois_autotest_threshold);
	if (ret)
		LOG_ERR("cam_ois_sysfs_autotest fail\n");

	ret = scnprintf(buf, PAGE_SIZE, "%s, %d, %s, %d",
			(result_x ? "pass" : "fail"), sin_x,
			(result_y ? "pass" : "fail"), sin_y);

	LOG_INF("result : %s\n", buf);

	// imgsensor_hw_power(imgsensor_hw, psensor, IMGSENSOR_HW_POWER_STATUS_OFF);
	cam_ois_af_power_off();
	return ret;
}

static ssize_t ois_autotest_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value = 0;

	LOG_INF("- E\n");
	if (buf == NULL || kstrtouint(buf, 10, &value))
		return 0;
	ois_autotest_threshold = value;
	return size;
}

static ssize_t oisfw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char ois_fw_full[OIS_SYSFS_FW_VER_SIZE] = "NULL NULL\n";

	rc = cam_ois_sysfs_get_oisfw_version(ois_fw_full);
	if (rc) {
		LOG_ERR("fail to get ois fw version\n");
		return rc;
	}

	LOG_INF("fw ver : %s\n", ois_fw_full);

	rc = scnprintf(buf, PAGE_SIZE, "%s", ois_fw_full);
	if (rc)
		return rc;

	return 0;
}

static ssize_t ois_set_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value = 0;
	int ret = 0;

	if (kstrtoint(buf, 10, &value))
		LOG_ERR("convert fail");

	ret = cam_ois_sysfs_set_mode(value);
	if (ret)
		LOG_ERR("cam_ois_sysfs_set_mode fail");

	return size;
}

static ssize_t ois_check_valid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int wide_val = 0;

	cam_ois_sysfs_get_mcu_error(&wide_val);

	LOG_INF(" 0x%02x\n", wide_val);
	return sprintf(buf, "0x%02x\n", wide_val);
}

static ssize_t gyro_noise_stdev_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int result = 0;
	long stdev_data_x = 0, stdev_data_y = 0;

	result = cam_ois_gyro_sensor_noise_check(&stdev_data_x, &stdev_data_y);

	if (stdev_data_x < 0 && stdev_data_y < 0) {
		return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,-%ld.%03ld\n", result, abs(stdev_data_x / 1000),
			abs(stdev_data_x % 1000), abs(stdev_data_y / 1000), abs(stdev_data_y % 1000));
	} else if (stdev_data_x < 0) {
		return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,%ld.%03ld\n", result, abs(stdev_data_x / 1000),
			abs(stdev_data_x % 1000), stdev_data_y / 1000, stdev_data_y % 1000);
	} else if (stdev_data_y < 0) {
		return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,-%ld.%03ld\n", result, stdev_data_x / 1000,
			stdev_data_x % 1000, abs(stdev_data_y / 1000), abs(stdev_data_y % 1000));
	} else {
		return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,%ld.%03ld\n", result, stdev_data_x / 1000,
			stdev_data_x % 1000, stdev_data_y / 1000, stdev_data_y % 1000);
	}
}

static ssize_t ois_hall_position_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	uint32_t const UNSUPPORTED_POSITION = 2048;
	uint32_t targetPosition[4] = { 0, 0, UNSUPPORTED_POSITION, UNSUPPORTED_POSITION};
	uint32_t hallPosition[4] = {0, 0, UNSUPPORTED_POSITION, UNSUPPORTED_POSITION};

	cam_ois_get_hall_position(targetPosition, hallPosition);

	return scnprintf(buf, PAGE_SIZE, "%u,%u,%u,%u,%u,%u,%u,%u\n",
			targetPosition[0], targetPosition[1], targetPosition[2], targetPosition[3],
			hallPosition[0], hallPosition[1], hallPosition[2], hallPosition[3]);
}

static DEVICE_ATTR(autotest, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH, ois_autotest_show, ois_autotest_store);
static DEVICE_ATTR(ois_power, S_IWUSR, NULL, ois_power_store);
static DEVICE_ATTR(calibrationtest, S_IRUGO, ois_calibration_show, NULL);
static DEVICE_ATTR(ois_rawdata, S_IRUGO|S_IWUSR|S_IWGRP, ois_rawdata_show, ois_rawdata_store);
static DEVICE_ATTR(selftest, S_IRUGO, ois_selftest_show, NULL);
static DEVICE_ATTR(oisfw, S_IRUGO, oisfw_show, NULL);

static DEVICE_ATTR(ois_gain_rear, S_IRUGO, ois_gain_rear_show, NULL);
static DEVICE_ATTR(ois_supperssion_ratio_rear, S_IRUGO, ois_supperssion_ratio_rear_show, NULL);
static DEVICE_ATTR(ois_set_mode, S_IWUSR, NULL, ois_set_mode_store);
static DEVICE_ATTR(check_ois_valid, S_IRUGO, ois_check_valid_show, NULL);

static DEVICE_ATTR(ois_noise_stdev, S_IRUGO, gyro_noise_stdev_show, NULL);
static DEVICE_ATTR(ois_hall_position, S_IRUGO, ois_hall_position_show, NULL);

static int create_ois_sysfs(struct kobject *svc)
{
	int ret = 0;

	if (camera_ois_dev == NULL)
		camera_ois_dev = device_create(ois_camera_class, NULL, 1, NULL, "ois");

	if (IS_ERR(camera_ois_dev)) {
		LOG_ERR("imgsensor_sysfs_init: failed to create device(ois)\n");
		return -ENODEV;
	}

	if (device_create_file(camera_ois_dev, &dev_attr_calibrationtest) < 0)
		LOG_ERR("failed to create ois device file, %s\n",
			dev_attr_calibrationtest.attr.name);

	if (device_create_file(camera_ois_dev, &dev_attr_selftest) < 0)
		LOG_ERR("failed to create ois device file, %s\n",
			dev_attr_selftest.attr.name);

	if (device_create_file(camera_ois_dev, &dev_attr_oisfw) < 0)
		LOG_ERR("failed to create ois device file, %s\n",
			dev_attr_oisfw.attr.name);

	if (device_create_file(camera_ois_dev, &dev_attr_ois_power) < 0)
		LOG_ERR("failed to create ois device file, %s\n",
			dev_attr_ois_power.attr.name);

	if (device_create_file(camera_ois_dev, &dev_attr_autotest) < 0)
		LOG_ERR("failed to create ois device file, %s\n",
			dev_attr_autotest.attr.name);

	if (device_create_file(camera_ois_dev, &dev_attr_ois_rawdata) < 0)
		LOG_ERR("failed to create ois device file, %s\n",
			dev_attr_ois_rawdata.attr.name);

	if (device_create_file(camera_ois_dev, &dev_attr_ois_gain_rear) < 0)
		LOG_ERR("failed to create ois device file, %s\n",
			dev_attr_ois_gain_rear.attr.name);

	if (device_create_file(camera_ois_dev, &dev_attr_ois_supperssion_ratio_rear) < 0)
		LOG_ERR("failed to create ois device file, %s\n",
			dev_attr_ois_supperssion_ratio_rear.attr.name);

	if (device_create_file(camera_ois_dev, &dev_attr_ois_set_mode) < 0)
		LOG_ERR("failed to create ois device file, %s\n",
			dev_attr_ois_set_mode.attr.name);

	if (device_create_file(camera_ois_dev, &dev_attr_check_ois_valid) < 0)
		LOG_ERR("failed to create ois device file, %s\n",
			dev_attr_check_ois_valid.attr.name);

	if (device_create_file(camera_ois_dev, &dev_attr_ois_noise_stdev) < 0)
		LOG_ERR("failed to create ois device file, %s\n",
			dev_attr_ois_noise_stdev.attr.name);

	if (device_create_file(camera_ois_dev, &dev_attr_ois_hall_position) < 0)
		LOG_ERR("failed to create ois device file, %s\n",
			dev_attr_ois_hall_position.attr.name);

	return ret;
}

void imgsensor_destroy_ois_sysfs(void)
{
	device_remove_file(camera_ois_dev, &dev_attr_ois_rawdata);
	device_remove_file(camera_ois_dev, &dev_attr_calibrationtest);
	device_remove_file(camera_ois_dev, &dev_attr_selftest);
	device_remove_file(camera_ois_dev, &dev_attr_oisfw);
	device_remove_file(camera_ois_dev, &dev_attr_ois_power);
	device_remove_file(camera_ois_dev, &dev_attr_autotest);
	device_remove_file(camera_ois_dev, &dev_attr_ois_supperssion_ratio_rear);
	device_remove_file(camera_ois_dev, &dev_attr_ois_set_mode);
	device_remove_file(camera_ois_dev, &dev_attr_ois_gain_rear);
	device_remove_file(camera_ois_dev, &dev_attr_check_ois_valid);
	device_remove_file(camera_ois_dev, &dev_attr_ois_noise_stdev);
	device_remove_file(camera_ois_dev, &dev_attr_ois_hall_position);
}

void cam_ois_destroy_sysfs(void)
{
	if (camera_ois_dev) {
		imgsensor_destroy_ois_sysfs();
		if (ois_camera_class)
			device_destroy(ois_camera_class, camera_ois_dev->devt);
	}
}

int cam_ois_sysfs_init(void)
{
	int ret = 0;
	struct kobject *svc = 0;

	LOG_INF("start");
	IMGSENSOR_SYSFS_GET_CAMERA_CLASS(&ois_camera_class);
	if (IS_ERR_OR_NULL(ois_camera_class)) {
		LOG_ERR("error, camera class not exist");
		return -ENODEV;
	}

	create_ois_sysfs(svc);
	return ret;
}

#endif
