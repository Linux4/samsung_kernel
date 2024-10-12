#include <linux/delay.h>
#include <linux/i2c.h>

#include "mtk_ois_i2c.h"
#include "mtk_ois_define.h"
#include "mtk_ois_mcu_fw.h"
#include "mtk_ois_mcu.h"

#include "mtk_ois_mcu_sysfs.h"

static int ois_mcu_get_gc_status(struct i2c_client *client, int retries)
{
	int ret = 0;
	u8 recv = 0;

	/* Check Gyro Calibration Sequence End */
	do {
		/* GCCTRL Read */
		ret = ois_mcu_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x0014, &recv, 1);
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

static int ois_mcu_get_gyro_cal_result(long *out_data, struct i2c_client *client, u16 addr)
{
	int scale_factor = OIS_GYRO_SCALE_FACTOR_LSM6DSO;
	int GZERO;
	int ret = 0;
	u8 recv = 0, tmp = 0;

	ret = ois_mcu_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, addr, &recv, 1);
	tmp = recv;
	ret = ois_mcu_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, addr + 1, &recv, 1);
	GZERO = (recv << 8) | tmp;
	if (GZERO > 0x7FFF)
		GZERO = -((GZERO ^ 0xFFFF) + 1);

	*out_data = GZERO * 1000 / scale_factor;
	return ret;
}


int ois_mcu_gyro_calibration(struct mcu_info *mcu_info, struct i2c_client *client,
	long *out_gyro_x, long *out_gyro_y)
{
	int ret = 0, result = 0;
	u8 recv = 0;
	int retries = 30;


	LOG_INF("Enter");
	if (!client) {
		LOG_ERR("i2c client is null");
		return result;
	}

	ret = ois_mcu_wait_idle(client, retries);
	if (!ret) {
		/* Gyro Calibration Start */
		/* GCCTRL GSCEN set */
		/* GCCTRL register(0x0014) 1Byte Send */
		ret = ois_mcu_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0014, 0x01);
		if (ret < 0)
			LOG_WARN("i2c write fail %d", ret);

		/* Check Gyro Calibration Sequence End */
		ret = ois_mcu_get_gc_status(client, 80);
		if (ret < 0)
			LOG_WARN("fail to get gyro calibration status %d", ret);

		/* Result check */
		/* OISERR Read */
		ret = ois_mcu_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x0004, &recv, 1);
		/* OISERR register GXZEROERR & GYZEROERR & GCOMERR Bit = 0(No Error)*/
		if ((ret >= 0) && ((recv & 0x23) == 0x00)) {
			LOG_INF("mcu gyro calibration ok %d", recv);
			result = 1;
		} else {
			LOG_ERR("mcu gyro calibration fail, ret %d, recv %d", ret, recv);
			result = 0;
		}

		ois_mcu_get_gyro_cal_result(out_gyro_x, client, 0x0248);
		ois_mcu_get_gyro_cal_result(out_gyro_y, client, 0x024A);

		LOG_INF("result %d, raw_data_x %ld, raw_data_y %ld", result, *out_gyro_x, *out_gyro_y);
	}

	return result;
}

int ois_mcu_get_offset_data(struct mcu_info *mcu_info, struct i2c_client *client,
	long *out_gyro_x, long *out_gyro_y)
{
	int ret = 0;
	int retries = 20;

	LOG_INF("E\n");

	ret = ois_mcu_wait_idle(client, retries);
	if (ret)
		LOG_WARN("fail to wait idle");

	ret = ois_mcu_get_offset_from_efs(out_gyro_x, out_gyro_y);
	if (ret)
		LOG_WARN("fail to get offset from efs");

	LOG_INF("X raw_x = %ld, raw_y = %ld\n", *out_gyro_x, *out_gyro_y);
	return ret;
}

int ois_mcu_get_offset_testdata(struct mcu_info *mcu_info, struct i2c_client *client,
	long *out_gyro_x, long *out_gyro_y)
{
	int i;
	int ret = 0, result = 0;
	u8 recv = 0;
	long x_sum = 0, y_sum = 0, x = 0, y = 0;
	int retries = 20;

	LOG_INF("E\n");

	/* Gyro Calibration Start */
	/* GCCTRL GSCEN set */
	/* GCCTRL register(0x0014) 1Byte Send */
	ret = ois_mcu_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0014, 0x01);
	if (ret < 0)
		LOG_WARN("i2c write fail %d", ret);

	/* Check Gyro Calibration Sequence End */
	ret = ois_mcu_get_gc_status(client, 80);
	if (ret < 0) {
		LOG_ERR("fail to get gyro calibration status %d", ret);
		return result;
	}

	/* Result check */
	/* OISERR Read */
	ret = ois_mcu_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x0004, &recv, 1);
	/* OISERR register GXZEROERR & GYZEROERR & GCOMERR Bit = 0(No Error)*/
	if ((ret >= 0) && ((recv & 0x23) == 0x00)) {
		LOG_INF("mcu gyro calibration ok %d", recv);
		result = 1;
	} else {
		LOG_ERR("mcu gyro calibration fail, ret %d, recv %d", ret, recv);
		result = 0;
	}

	for (i = 0; i < retries; i++) {
		ois_mcu_get_gyro_cal_result(&x, client, 0x0248);
		ois_mcu_get_gyro_cal_result(&y, client, 0x024A);
		x_sum += x;
		y_sum += y;
	}

	*out_gyro_x = x_sum / retries;
	*out_gyro_y = y_sum / retries;

	LOG_INF("X raw_x = %ld, raw_y = %ld\n", *out_gyro_x, *out_gyro_y);
	return result;
}


int ois_mcu_selftest(struct mcu_info *mcu_info, struct i2c_client *client)
{
	int ret = 0;
	u8 recv = 0;
	int retries = 30;

	LOG_INF("E\n");

	ret = ois_mcu_wait_idle(client, retries);
	if (ret) {
		LOG_ERR("failt to wait idle");
		return -1;
	}

	ret = ois_mcu_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0014, 0x08);
	if (ret)
		LOG_WARN("i2c write fail %d", ret);

	do {
		ret = ois_mcu_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x0004, &recv, 1);
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

	ret = ois_mcu_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x0004, &recv, 1);
	if (ret != 0)
		recv = -EIO;

	if ((recv & 0x80) != 0x0) {
		/* Gyro Sensor Self Test Error Process */
		LOG_ERR("GyroSensorSelfTest failed %d\n", recv);
		return -1;
	}

	LOG_INF("%d: X\n", recv);
	return (int)recv;
}


bool ois_mcu_sine_wavecheck(int *sin_x, int *sin_y, int *result, struct i2c_client *client, int threshold)
{
	u8 val[3] = {0, };
	int ret = 0, retries = 10;
	int sinx_count = 0, siny_count = 0;

	if (!client) {
		LOG_ERR("i2c client is null");
		return false;
	}

	/* auto test setting */
	/* select module */
	ret = ois_mcu_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x00BE, 0x01);
	/* error threshold level. */
	ret |= ois_mcu_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0052, (u8)threshold);
	ret |= ois_mcu_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0053, 0x00);
	ret |= ois_mcu_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0054, 0x05);
	ret |= ois_mcu_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0055, 0x2A);
	ret |= ois_mcu_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0056, 0x03);
	ret |= ois_mcu_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0057, 0x02);

	/* auto test start */
	ret |= ois_mcu_i2c_write_one(client, MCU_I2C_SLAVE_W_ADDR, 0x0050, 0x01);
	if (ret) {
		LOG_ERR("i2c write fail");
		*result = 0xFF;
		return false;
	}
	LOG_INF("i2c write success");

	retries = 60;
	do {
		ret = ois_mcu_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x0050, val, 1);
		if (ret < 0) {
			LOG_ERR("i2c read fail");
			break;
		}

		usleep_range(20000, 21000);

		if (--retries < 0) {
			LOG_ERR("sine wave operation fail.");
			*result = 0xFF;
			return false;
		}
	} while (val[0]);


	ret = ois_mcu_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0x004C, val, 1);
	if (ret < 0)
		LOG_ERR("i2c read fail");

	*result = (int)val[0];
	if (val[0] != 0x00)
		LOG_ERR("MCERR_W(0x004C)=%d", val[0]);
	else
		LOG_INF("MCERR_W(0x004C)=%d", val[0]);

	/* read test results */
	ret = ois_mcu_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0xC0, val, 2);
	sinx_count = (val[1] << 8) | val[0];
	if (sinx_count > 0x7FFF)
		sinx_count = -((sinx_count ^ 0xFFFF) + 1);


	ret |= ois_mcu_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0xC2, val, 2);
	siny_count = (val[1] << 8) | val[0];
	if (siny_count > 0x7FFF)
		siny_count = -((siny_count ^ 0xFFFF) + 1);

	ret |= ois_mcu_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0xC4, val, 2);
	*sin_x = (val[1] << 8) | val[0];
	if (*sin_x > 0x7FFF)
		*sin_x = -((*sin_x ^ 0xFFFF) + 1);

	ret |= ois_mcu_i2c_read_multi(client, MCU_I2C_SLAVE_W_ADDR, 0xC6, val, 2);
	*sin_y = (val[1] << 8) | val[0];
	if (*sin_y > 0x7FFF)
		*sin_y = -((*sin_y ^ 0xFFFF) + 1);

	if (ret < 0)
		LOG_WARN("i2c read fail");

	LOG_INF("threshold = %d, sinx = %d, siny = %d, sinx_count = %d, siny_count = %d",
	threshold, *sin_x, *sin_y, sinx_count, siny_count);


	if (val != 0x00)
		return true;

	*sin_x = -1;
	*sin_y = -1;

	return false;
}
