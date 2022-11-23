/*
 * (C) Copyright 2013 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html
 *
 * @date        Jun 12th, 2013
 * @version     v2.5.5
 * @brief       BMM050 Linux Driver
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "sensors_core.h"

#include "bst_sensor_common.h"

#include "bmm050.h"
//#include "bs_log.h"

/* sensor specific */
#define SENSOR_NAME "bmm050"

#define SENSOR_CHIP_ID_BMM (0x32)

#define BMM_REG_NAME(name) BMM050_##name
#define BMM_VAL_NAME(name) BMM050_##name
#define BMM_CALL_API(name) bmm050_##name

#define BMM_I2C_WRITE_DELAY_TIME 5

#define BMM_DEFAULT_REPETITION_XY BMM_VAL_NAME(REGULAR_REPXY)
#define BMM_DEFAULT_REPETITION_Z BMM_VAL_NAME(REGULAR_REPZ)
#define BMM_DEFAULT_ODR BMM_VAL_NAME(REGULAR_DR)
/* generic */
#define BMM_MAX_RETRY_I2C_XFER (100)
#define BMM_MAX_RETRY_WAKEUP (5)
#define BMM_MAX_RETRY_WAIT_DRDY (100)

#define BMM_DELAY_MIN (1)
#define BMM_DELAY_DEFAULT (200)

#define MAG_VALUE_MAX (32767)
#define MAG_VALUE_MIN (-32768)

#define BYTES_PER_LINE (16)

#define BMM_SELF_TEST 1
#define BMM_ADV_TEST 2

#define BMM_OP_MODE_UNKNOWN (-1)

struct op_mode_map {
	char *op_mode_name;
	long op_mode;
};

static const u8 odr_map[] = {10, 2, 6, 8, 15, 20, 25, 30};
static const struct op_mode_map op_mode_maps[] = {
	{"normal", BMM_VAL_NAME(NORMAL_MODE)},
	{"forced", BMM_VAL_NAME(FORCED_MODE)},
	{"suspend", BMM_VAL_NAME(SUSPEND_MODE)},
	{"sleep", BMM_VAL_NAME(SLEEP_MODE)},
};


struct bmm_client_data {
	struct bmm050 device;
	struct i2c_client *client;
	struct input_dev *input;
	struct delayed_work work;

	atomic_t delay;
	/* whether the system in suspend state */
	atomic_t in_suspend;

	struct bmm050_mdata_s32 value;
	u8 enable:1;
	s8 op_mode:4;
	u8 odr;
	u8 rept_xy;
	u8 rept_z;

	u8 selftest;
	s16 result_test;

	struct mutex mutex_power_mode;

	/* controls not only reg, but also workqueue */
	struct mutex mutex_op_mode;
	struct mutex mutex_enable;
	struct mutex mutex_odr;
	struct mutex mutex_rept_xy;
	struct mutex mutex_rept_z;

	struct mutex mutex_value;
#ifdef CONFIG_BMM_USE_PLATFORM_DATA
	struct bosch_sensor_specific *bst_pd;
#endif

	int place;
};

static struct device *magnetic_device;

static struct i2c_client *bmm_client;
/* i2c operation for API */
static void bmm_delay(u32 msec);
static char bmm_i2c_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len);
static char bmm_i2c_write(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len);

static void bmm_dump_reg(struct i2c_client *client);
static int bmm_wakeup(struct i2c_client *client);
static int bmm_check_chip_id(struct i2c_client *client);

static int bmm_pre_suspend(struct i2c_client *client);
static int bmm_post_resume(struct i2c_client *client);

static int bmm_restore_hw_cfg(struct i2c_client *client);

static void bmm_remap_sensor_data(struct bmm050_mdata_s32 *val,
		struct bmm_client_data *client_data)
{
#ifdef CONFIG_BMM_USE_PLATFORM_DATA
	struct bosch_sensor_data bsd;

	//if (NULL == client_data->bst_pd)
	//	return;

	bsd.x = val->datax;
	bsd.y = val->datay;
	bsd.z = val->dataz;

	bst_remap_sensor_data_dft_tab(&bsd,
			client_data->place/*client_data->bst_pd->place*/);

	val->datax = bsd.x;
	val->datay = bsd.y;
	val->dataz = bsd.z;
#else
	(void)val;
	(void)client_data;
#endif
}

static int bmm_check_chip_id(struct i2c_client *client)
{
	int err = 0;
	u8 chip_id = 0;

	bmm_i2c_read(client, BMM_REG_NAME(CHIP_ID), &chip_id, 1);
	pr_info("%s read chip id result: %#x\n", __func__, chip_id);

	if ((chip_id & 0xff) != SENSOR_CHIP_ID_BMM)
		err = -1;

	return err;
}

static void bmm_delay(u32 msec)
{
	mdelay(msec);
}

static inline int bmm_get_forced_drdy_time(int rept_xy, int rept_z)
{
	return  (145 * rept_xy + 500 * rept_z + 980 + (1000 - 1)) / 1000;
}


static void bmm_dump_reg(struct i2c_client *client)
{
#ifdef DEBUG
	int i;
	u8 dbg_buf[64];
	u8 dbg_buf_str[64 * 3 + 1] = "";

	for (i = 0; i < BYTES_PER_LINE; i++) {
		dbg_buf[i] = i;
		sprintf(dbg_buf_str + i * 3, "%02x%c",
				dbg_buf[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	printk(KERN_DEBUG "%s\n", dbg_buf_str);

	bmm_i2c_read(client, BMM_REG_NAME(CHIP_ID), dbg_buf, 64);
	for (i = 0; i < 64; i++) {
		sprintf(dbg_buf_str + i * 3, "%02x%c",
				dbg_buf[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	printk(KERN_DEBUG "%s\n", dbg_buf_str);
#endif
}

static int bmm_wakeup(struct i2c_client *client)
{
	int err = 0;
	int try_times = BMM_MAX_RETRY_WAKEUP;
	const u8 value = 0x01;
	u8 dummy;

	pr_info("%s waking up the chip...\n", __func__);
	mdelay(BMM_I2C_WRITE_DELAY_TIME);
	while (try_times) {
		err = bmm_i2c_write(client,
				BMM_REG_NAME(POWER_CNTL), (u8 *)&value, 1);
		mdelay(BMM_I2C_WRITE_DELAY_TIME);
		dummy = 0;
		err = bmm_i2c_read(client, BMM_REG_NAME(POWER_CNTL), &dummy, 1);
		if (value == dummy)
			break;

		try_times--;
	}

	pr_info("wake up result: %s, tried times: %d\n",
			(try_times > 0) ? "succeed" : "fail",
			BMM_MAX_RETRY_WAKEUP - try_times + 1);

	err = (try_times > 0) ? 0 : -1;

	return err;
}

/*	i2c read routine for API*/
static char bmm_i2c_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined BMM_USE_BASIC_I2C_FUNC
	s32 dummy;
	if (NULL == client)
		return -1;

	while (0 != len--) {
#ifdef BMM_SMBUS
		dummy = i2c_smbus_read_byte_data(client, reg_addr);
		if (dummy < 0) {
			pr_err("%s i2c bus read error\n", __func__);
			return -1;
		}
		*data = (u8)(dummy & 0xff);
#else
		dummy = i2c_master_send(client, (char *)&reg_addr, 1);
		if (dummy < 0)
			return -1;

		dummy = i2c_master_recv(client, (char *)data, 1);
		if (dummy < 0)
			return -1;
#endif
		reg_addr++;
		data++;
	}
	return 0;
#else
	int retry;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		},

		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};

	for (retry = 0; retry < BMM_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			mdelay(BMM_I2C_WRITE_DELAY_TIME);
	}

	if (BMM_MAX_RETRY_I2C_XFER <= retry) {
		pr_err("%s I2C xfer error\n", __func__);
		return -EIO;
	}

	return 0;
#endif
}

/*	i2c write routine for */
static char bmm_i2c_write(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined BMM_USE_BASIC_I2C_FUNC
	s32 dummy;

#ifndef BMM_SMBUS
	u8 buffer[2];
#endif

	if (NULL == client)
		return -1;

	while (0 != len--) {
#ifdef BMM_SMBUS
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
#else
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(client, (char *)buffer, 2);
#endif
		reg_addr++;
		data++;
		if (dummy < 0) {
			pr_err("%s error writing i2c bus\n", __func__);
			return -1;
		}

	}
	return 0;
#else
	u8 buffer[2];
	int retry;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 2,
		 .buf = buffer,
		 },
	};

	while (0 != len--) {
		buffer[0] = reg_addr;
		buffer[1] = *data;
		for (retry = 0; retry < BMM_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0) {
				break;
			} else {
				mdelay(BMM_I2C_WRITE_DELAY_TIME);
			}
		}
		if (BMM_MAX_RETRY_I2C_XFER <= retry) {
			pr_err("%s I2C xfer error\n", __func__);
			return -EIO;
		}
		reg_addr++;
		data++;
	}

	return 0;
#endif
}

static char bmm_i2c_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	char err = 0;
	err = bmm_i2c_read(bmm_client, reg_addr, data, len);
	return err;
}

static char bmm_i2c_write_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	char err = 0;
	err = bmm_i2c_write(bmm_client, reg_addr, data, len);
	return err;
}

/* this function exists for optimization of speed,
 * because it is frequently called */
static inline int bmm_set_forced_mode(struct i2c_client *client)
{
	int err = 0;

	/* FORCED_MODE */
	const u8 value = 0x02;
	err = bmm_i2c_write(client, BMM_REG_NAME(CONTROL), (u8 *)&value, 1);

	return err;
}

static void bmm_work_func(struct work_struct *work)
{
	struct bmm_client_data *client_data =
		container_of((struct delayed_work *)work,
			struct bmm_client_data, work);
	struct i2c_client *client = client_data->client;
	unsigned long delay =
		msecs_to_jiffies(atomic_read(&client_data->delay));

	mutex_lock(&client_data->mutex_value);

	mutex_lock(&client_data->mutex_op_mode);
	if (BMM_VAL_NAME(NORMAL_MODE) != client_data->op_mode)
		bmm_set_forced_mode(client);

	mutex_unlock(&client_data->mutex_op_mode);

	BMM_CALL_API(read_mdataXYZ_s32)(&client_data->value);
	bmm_remap_sensor_data(&client_data->value, client_data);

	input_report_abs(client_data->input, ABS_X, client_data->value.datax);
	input_report_abs(client_data->input, ABS_Y, client_data->value.datay);
	input_report_abs(client_data->input, ABS_Z, client_data->value.dataz);
	mutex_unlock(&client_data->mutex_value);

	input_sync(client_data->input);

	schedule_delayed_work(&client_data->work, delay);
}


static int bmm_set_odr(struct i2c_client *client, u8 odr)
{
	int err = 0;

	err = BMM_CALL_API(set_datarate)(odr);
	mdelay(BMM_I2C_WRITE_DELAY_TIME);

	return err;
}

static int bmm_get_odr(struct i2c_client *client, u8 *podr)
{
	int err = 0;
	u8 value;

	err = BMM_CALL_API(get_datarate)(&value);
	if (!err)
		*podr = value;

	return err;
}

static ssize_t bmm_show_chip_id(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", SENSOR_CHIP_ID_BMM);
}

static ssize_t bmm_show_op_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	u8 op_mode = 0xff;
	u8 power_mode;

	mutex_lock(&client_data->mutex_power_mode);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode) {
		mutex_lock(&client_data->mutex_op_mode);
		BMM_CALL_API(get_functional_state)(&op_mode);
		mutex_unlock(&client_data->mutex_op_mode);
	} else {
		op_mode = BMM_VAL_NAME(SUSPEND_MODE);
	}

	mutex_unlock(&client_data->mutex_power_mode);

	pr_info("%s op_mode: %d\n", __func__, op_mode);
	ret = sprintf(buf, "%d\n", op_mode);

	return ret;
}


static inline int bmm_get_op_mode_idx(u8 op_mode)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(op_mode_maps); i++) {
		if (op_mode_maps[i].op_mode == op_mode)
			break;
	}

	if (i < ARRAY_SIZE(op_mode_maps))
		return i;
	else
		return -1;
}


static int bmm_set_op_mode(struct bmm_client_data *client_data, int op_mode)
{
	int err = 0;

	err = BMM_CALL_API(set_functional_state)(
			op_mode);

	if (BMM_VAL_NAME(SUSPEND_MODE) == op_mode)
		atomic_set(&client_data->in_suspend, 1);
	else
		atomic_set(&client_data->in_suspend, 0);

	return err;
}

static ssize_t bmm_store_op_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err = 0;
	int i;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	struct i2c_client *client = client_data->client;
	long op_mode;

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	mutex_lock(&client_data->mutex_power_mode);

	i = bmm_get_op_mode_idx(op_mode);

	if (i != -1) {
		mutex_lock(&client_data->mutex_op_mode);
		if (op_mode != client_data->op_mode) {
			if (BMM_VAL_NAME(FORCED_MODE) == op_mode) {
				/* special treat of forced mode
				 * for optimization */
				err = bmm_set_forced_mode(client);
			} else {
				err = bmm_set_op_mode(client_data, op_mode);
			}

			if (!err) {
				if (BMM_VAL_NAME(FORCED_MODE) == op_mode)
					client_data->op_mode =
						BMM_OP_MODE_UNKNOWN;
				else
					client_data->op_mode = op_mode;
			}
		}
		mutex_unlock(&client_data->mutex_op_mode);
	} else {
		err = -EINVAL;
	}

	mutex_unlock(&client_data->mutex_power_mode);

	if (err)
		return err;
	else
		return count;
}

static ssize_t bmm_show_odr(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	struct i2c_client *client = client_data->client;
	int err;
	u8 power_mode;

	mutex_lock(&client_data->mutex_power_mode);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode) {
		mutex_lock(&client_data->mutex_odr);
		err = bmm_get_odr(client, &data);
		mutex_unlock(&client_data->mutex_odr);
	} else {
		err = -EIO;
	}
	mutex_unlock(&client_data->mutex_power_mode);

	if (!err) {
		if (data < ARRAY_SIZE(odr_map))
			err = sprintf(buf, "%d\n", odr_map[data]);
		else
			err = -EINVAL;
	}

	return err;
}

static ssize_t bmm_store_odr(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long tmp;
	unsigned char data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	struct i2c_client *client = client_data->client;
	u8 power_mode;
	int i;

	err = kstrtoul(buf, 10, &tmp);
	if (err)
		return err;

	if (tmp > 255)
		return -EINVAL;

	data = (unsigned char)tmp;

	mutex_lock(&client_data->mutex_power_mode);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode) {
		for (i = 0; i < ARRAY_SIZE(odr_map); i++) {
			if (odr_map[i] == data)
				break;
		}

		if (i < ARRAY_SIZE(odr_map)) {
			mutex_lock(&client_data->mutex_odr);
			err = bmm_set_odr(client, i);
			if (!err)
				client_data->odr = i;

			mutex_unlock(&client_data->mutex_odr);
		} else {
			err = -EINVAL;
		}
	} else {
		err = -EIO;
	}

	mutex_unlock(&client_data->mutex_power_mode);
	if (err)
		return err;

	return count;
}

static ssize_t bmm_show_rept_xy(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	int err;
	u8 power_mode;

	mutex_lock(&client_data->mutex_power_mode);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode) {
		mutex_lock(&client_data->mutex_rept_xy);
		err = BMM_CALL_API(get_repetitions_XY)(&data);
		mutex_unlock(&client_data->mutex_rept_xy);
	} else {
		err = -EIO;
	}

	mutex_unlock(&client_data->mutex_power_mode);

	if (err)
		return err;

	return sprintf(buf, "%d\n", data);
}

static ssize_t bmm_store_rept_xy(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long tmp = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	int err;
	u8 data;
	u8 power_mode;

	err = kstrtoul(buf, 10, &tmp);
	if (err)
		return err;

	if (tmp > 255)
		return -EINVAL;

	data = (unsigned char)tmp;

	mutex_lock(&client_data->mutex_power_mode);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode) {
		mutex_lock(&client_data->mutex_rept_xy);
		err = bmm050_set_repetitions_XY(data);
		if (!err) {
			mdelay(BMM_I2C_WRITE_DELAY_TIME);
			client_data->rept_xy = data;
		}
		mutex_unlock(&client_data->mutex_rept_xy);
	} else {
		err = -EIO;
	}
	mutex_unlock(&client_data->mutex_power_mode);

	if (err)
		return err;

	return count;
}

static ssize_t bmm_show_rept_z(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	int err;
	u8 power_mode;

	mutex_lock(&client_data->mutex_power_mode);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode) {
		mutex_lock(&client_data->mutex_rept_z);
		err = BMM_CALL_API(get_repetitions_Z)(&data);
		mutex_unlock(&client_data->mutex_rept_z);
	} else {
		err = -EIO;
	}

	mutex_unlock(&client_data->mutex_power_mode);

	if (err)
		return err;

	return sprintf(buf, "%d\n", data);
}

static ssize_t bmm_store_rept_z(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long tmp = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	int err;
	u8 data;
	u8 power_mode;

	err = kstrtoul(buf, 10, &tmp);
	if (err)
		return err;

	if (tmp > 255)
		return -EINVAL;

	data = (unsigned char)tmp;

	mutex_lock(&client_data->mutex_power_mode);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode) {
		mutex_lock(&client_data->mutex_rept_z);
		err = BMM_CALL_API(set_repetitions_Z)(data);
		if (!err) {
			mdelay(BMM_I2C_WRITE_DELAY_TIME);
			client_data->rept_z = data;
		}
		mutex_unlock(&client_data->mutex_rept_z);
	} else {
		err = -EIO;
	}
	mutex_unlock(&client_data->mutex_power_mode);

	if (err)
		return err;

	return count;
}


static ssize_t bmm_show_value(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	int count;
	struct bmm050_mdata_s32 value = {0, 0, 0, 0, 0};

	if (client_data->selftest == 1)
		return 0;

	BMM_CALL_API(read_mdataXYZ_s32)(&value);
	if (value.drdy) {
		bmm_remap_sensor_data(&value, client_data);
		client_data->value = value;
	} else
		pr_err("%s data not ready\n", __func__);

	count = sprintf(buf, "%d %d %d\n",
			client_data->value.datax,
			client_data->value.datay,
			client_data->value.dataz);
	pr_debug("%s %d %d %d\n", __func__,
			client_data->value.datax,
			client_data->value.datay,
			client_data->value.dataz);

	return count;
}

static ssize_t bmm_show_value_raw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	struct bmm050_mdata_s32 value = {0, 0, 0, 0, 0};
	static struct bmm050_mdata_s32 prev_value = {0, 0, 0};
	int count;

	if (client_data->selftest == 1)
		return 0;

	BMM_CALL_API(read_mdataXYZ_s32)(&value);

	if ((value.datax == 0) && (value.datay == 0)) {
		value.datax = prev_value.datax;
		value.datay = prev_value.datay;
		value.dataz = prev_value.dataz;
	} else {
		prev_value.datax = value.datax;
		prev_value.datay = value.datay;
		prev_value.dataz = value.dataz;
	}

	if (value.datax == BMM050_OVERFLOW_OUTPUT_S32)
		value.datax = BMM050_OVERFLOW_OUTPUT_S32_XY;
	if (value.datay == BMM050_OVERFLOW_OUTPUT_S32)
		value.datay = BMM050_OVERFLOW_OUTPUT_S32_XY;
	if (value.dataz == BMM050_OVERFLOW_OUTPUT_S32)
		value.dataz = BMM050_OVERFLOW_OUTPUT_S32_Z;


	count = sprintf(buf, "%d,%d,%d\n",
			value.datax,
			value.datay,
			value.dataz);

	return count;
}

static ssize_t bmm_show_raw_data(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	struct bmm050_mdata_s32 value = {0, 0, 0, 0, 0};
	static struct bmm050_mdata_s32 prev_value = {0, 0, 0};
	int count;

	if (client_data->selftest == 1)
		return 0;

	BMM_CALL_API(read_mdataXYZ_s32)(&value);

	if ((value.datax == 0) && (value.datay == 0)) {
		value.datax = prev_value.datax;
		value.datay = prev_value.datay;
		value.dataz = prev_value.dataz;
	} else {
		prev_value.datax = value.datax;
		prev_value.datay = value.datay;
		prev_value.dataz = value.dataz;
	}

	if (value.datax > BMM050_OVERFLOW_OUTPUT_XY_12BIT)
		value.datax = BMM050_OVERFLOW_OUTPUT_XY_12BIT;
	else if (value.datax < BMM050_OVERFLOW_OUTPUT_XY_12BIT*(-1))
		value.datax = BMM050_OVERFLOW_OUTPUT_XY_12BIT*(-1);
	if (value.datay > BMM050_OVERFLOW_OUTPUT_XY_12BIT)
		value.datay = BMM050_OVERFLOW_OUTPUT_XY_12BIT;
	else if (value.datay < BMM050_OVERFLOW_OUTPUT_XY_12BIT*(-1))
		value.datay = BMM050_OVERFLOW_OUTPUT_XY_12BIT*(-1);

	if (value.dataz > BMM050_OVERFLOW_OUTPUT_Z_13BIT)
		value.dataz = BMM050_OVERFLOW_OUTPUT_Z_13BIT;
	else if (value.dataz < BMM050_OVERFLOW_OUTPUT_Z_13BIT*(-1))
		value.dataz = BMM050_OVERFLOW_OUTPUT_Z_13BIT*(-1);

	count = sprintf(buf, "%d,%d,%d\n",
			value.datax,
			value.datay,
			value.dataz);

	return count;
}

static ssize_t bmm_show_enable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	int err;

	mutex_lock(&client_data->mutex_enable);
	err = sprintf(buf, "%d\n", client_data->enable);
	mutex_unlock(&client_data->mutex_enable);
	return err;
}

static ssize_t bmm_store_enable(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	pr_info("%s :%d\n", __func__, (int)data);

	data = data ? 1 : 0;
	mutex_lock(&client_data->mutex_enable);
	if (data != client_data->enable) {
		if (data) {
			schedule_delayed_work(
					&client_data->work,
					msecs_to_jiffies(atomic_read(
							&client_data->delay)));
		} else {
			cancel_delayed_work_sync(&client_data->work);
		}

		client_data->enable = data;
	}
	mutex_unlock(&client_data->mutex_enable);

	return count;
}

static ssize_t bmm_show_delay(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", atomic_read(&client_data->delay));

}

static ssize_t bmm_store_delay(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	if (data <= 0) {
		err = -EINVAL;
		return err;
	}

	if (data < BMM_DELAY_MIN)
		data = BMM_DELAY_MIN;

	atomic_set(&client_data->delay, data);

	return count;
}

static ssize_t bmm_show_test(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	int err;

	err = sprintf(buf, "%d\n", client_data->result_test);
	return err;
}

static ssize_t bmm_store_test(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	struct i2c_client *client = client_data->client;
	u8 dummy;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	pr_info("%s [%d]\n", __func__, (int)data);

	client_data->selftest = 1;
	mdelay(30);

	/* the following code assumes the work thread is not running */
	if (BMM_SELF_TEST == data) {
		/* self test */
		err = bmm_set_op_mode(client_data, BMM_VAL_NAME(SLEEP_MODE));
		mdelay(3);
		err = BMM_CALL_API(set_selftest)(1);
		mdelay(3);
		err = BMM_CALL_API(get_self_test_XYZ)(&dummy);
		client_data->result_test = dummy;
	} else if (BMM_ADV_TEST == data) {
		/* advanced self test */
		err = BMM_CALL_API(perform_advanced_selftest)(
				&client_data->result_test);
	} else {
		err = -EINVAL;
	}

	if (!err) {
		BMM_CALL_API(soft_reset)();
		mdelay(BMM_I2C_WRITE_DELAY_TIME);
		bmm_restore_hw_cfg(client);
	}

	if (err)
		count = -1;

	client_data->selftest = 0;
	return count;
}


static ssize_t bmm_show_reg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	int i;
	u8 dbg_buf[64];
	u8 dbg_buf_str[64 * 3 + 1] = "";
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	struct i2c_client *client = client_data->client;

	for (i = 0; i < BYTES_PER_LINE; i++) {
		dbg_buf[i] = i;
		sprintf(dbg_buf_str + i * 3, "%02x%c",
				dbg_buf[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	memcpy(buf, dbg_buf_str, BYTES_PER_LINE * 3);

	for (i = 0; i < BYTES_PER_LINE * 3 - 1; i++)
		dbg_buf_str[i] = '-';

	dbg_buf_str[i] = '\n';
	memcpy(buf + BYTES_PER_LINE * 3, dbg_buf_str, BYTES_PER_LINE * 3);


	bmm_i2c_read(client, BMM_REG_NAME(CHIP_ID), dbg_buf, 64);
	for (i = 0; i < 64; i++) {
		sprintf(dbg_buf_str + i * 3, "%02x%c",
				dbg_buf[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	memcpy(buf + BYTES_PER_LINE * 3 + BYTES_PER_LINE * 3,
			dbg_buf_str, 64 * 3);

	err = BYTES_PER_LINE * 3 + BYTES_PER_LINE * 3 + 64 * 3;
	return err;
}


static ssize_t bmm_show_place(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_BMM_USE_PLATFORM_DATA
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
#endif
	int place = BOSCH_SENSOR_PLACE_UNKNOWN;

#ifdef CONFIG_BMM_USE_PLATFORM_DATA
	if (NULL != client_data->bst_pd)
		place = client_data->bst_pd->place;
	else
		place = client_data->place;
#endif

	pr_info("%s magnetic sensor place=%d\n", __func__, place);
	return sprintf(buf, "%d\n", place);
}

static ssize_t bmm_read_name(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	printk(KERN_INFO "Bosch Sensortec Device!%s registered\n", CHIP_NAME);
	return sprintf(buf, "%s\n", CHIP_NAME);
}

static ssize_t bmm_read_vendor(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	printk(KERN_INFO "Magnetic] %s vendor\n", CHIP_VENDOR);
	return sprintf(buf, "%s\n", CHIP_VENDOR);
}

static ssize_t bmm_read_status(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int status=1;
#if 0
	int err = bmm_check_chip_id(client);
	if (!err) {
		pr_notice("%s Bosch Sensortec Device %s detected: %#x",
				__func__, SENSOR_NAME, client->addr);
	} else {
		pr_err("%s Bosch Sensortec Device not found, chip id mismatch", __func__);
		err = -1;
		goto exit_err_clean;
	}
#endif
	printk(KERN_INFO "Magnetic] %s [%d]\n", __func__, status);

	return sprintf(buf, "%d\n", status);
}

static DEVICE_ATTR(chip_id, S_IRUGO,
		bmm_show_chip_id, NULL);
static DEVICE_ATTR(op_mode, S_IRUGO|S_IWUSR,
		bmm_show_op_mode, bmm_store_op_mode);
static DEVICE_ATTR(odr, S_IRUGO|S_IWUSR,
		bmm_show_odr, bmm_store_odr);
static DEVICE_ATTR(rept_xy, S_IRUGO|S_IWUSR,
		bmm_show_rept_xy, bmm_store_rept_xy);
static DEVICE_ATTR(rept_z, S_IRUGO|S_IWUSR,
		bmm_show_rept_z, bmm_store_rept_z);
static DEVICE_ATTR(value, S_IRUGO,
		bmm_show_value, NULL);
static DEVICE_ATTR(value_raw, S_IRUGO,
		bmm_show_value_raw, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO,
		bmm_show_raw_data, NULL);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR,
		bmm_show_enable, bmm_store_enable);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR,
		bmm_show_delay, bmm_store_delay);
static DEVICE_ATTR(selftest, S_IRUGO|S_IWUSR,
		bmm_show_test, bmm_store_test);
static DEVICE_ATTR(reg, S_IRUGO,
		bmm_show_reg, NULL);
static DEVICE_ATTR(place, S_IRUGO,
		bmm_show_place, NULL);
static DEVICE_ATTR(status, S_IRUGO,
		bmm_read_status, NULL);
static DEVICE_ATTR(name, S_IRUGO,
		bmm_read_name, NULL);
static DEVICE_ATTR(vendor, S_IRUGO,
		bmm_read_vendor, NULL);

static struct attribute *bmm_attributes[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_op_mode.attr,
	&dev_attr_odr.attr,
	&dev_attr_rept_xy.attr,
	&dev_attr_rept_z.attr,
	&dev_attr_value.attr,
	&dev_attr_value_raw.attr,
	&dev_attr_raw_data.attr,
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_selftest.attr,
	&dev_attr_reg.attr,
	&dev_attr_place.attr,
	NULL
};


static struct attribute_group bmm_attribute_group = {
	.attrs = bmm_attributes
};

static struct device_attribute *bmm_attributes_sensors[] = {
	&dev_attr_chip_id,
	&dev_attr_op_mode,
	&dev_attr_odr,
	&dev_attr_rept_xy,
	&dev_attr_rept_z,
	&dev_attr_value,
	&dev_attr_value_raw,
	&dev_attr_raw_data,
	&dev_attr_enable,
	&dev_attr_delay,
	&dev_attr_selftest,
	&dev_attr_reg,
	&dev_attr_place,
	&dev_attr_status,
	&dev_attr_name,
	&dev_attr_vendor,
	NULL
};

static int bmm_input_init(struct bmm_client_data *client_data)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = SENSOR_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_X, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_drvdata(dev, client_data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	client_data->input = dev;

	return 0;
}

static void bmm_input_destroy(struct bmm_client_data *client_data)
{
	struct input_dev *dev = client_data->input;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int bmm_restore_hw_cfg(struct i2c_client *client)
{
	int err = 0;
	u8 value;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);
	int op_mode;

	mutex_lock(&client_data->mutex_op_mode);
	err = bmm_set_op_mode(client_data, BMM_VAL_NAME(SLEEP_MODE));

	if (bmm_get_op_mode_idx(client_data->op_mode) != -1)
		err = bmm_set_op_mode(client_data, client_data->op_mode);

	op_mode = client_data->op_mode;
	mutex_unlock(&client_data->mutex_op_mode);

	if (BMM_VAL_NAME(SUSPEND_MODE) == op_mode)
		return err;

	pr_info("%s app did not close this sensor before suspend\n", __func__);

	mutex_lock(&client_data->mutex_odr);
	BMM_CALL_API(set_datarate)(client_data->odr);
	mdelay(BMM_I2C_WRITE_DELAY_TIME);
	mutex_unlock(&client_data->mutex_odr);

	mutex_lock(&client_data->mutex_rept_xy);
	err = bmm_i2c_write(client, BMM_REG_NAME(NO_REPETITIONS_XY),
			&client_data->rept_xy, 1);
	mdelay(BMM_I2C_WRITE_DELAY_TIME);
	err = bmm_i2c_read(client, BMM_REG_NAME(NO_REPETITIONS_XY), &value, 1);
	pr_info("%s BMM_NO_REPETITIONS_XY: %02x\n", __func__, value);
	mutex_unlock(&client_data->mutex_rept_xy);

	mutex_lock(&client_data->mutex_rept_z);
	err = bmm_i2c_write(client, BMM_REG_NAME(NO_REPETITIONS_Z),
			&client_data->rept_z, 1);
	mdelay(BMM_I2C_WRITE_DELAY_TIME);
	err = bmm_i2c_read(client, BMM_REG_NAME(NO_REPETITIONS_Z), &value, 1);
	pr_info("%s BMM_NO_REPETITIONS_Z: %02x\n", __func__, value);
	mutex_unlock(&client_data->mutex_rept_z);

	mutex_lock(&client_data->mutex_op_mode);
	if (BMM_OP_MODE_UNKNOWN == (int)client_data->op_mode) {
		bmm_set_forced_mode(client);
		pr_info("%s set forced mode after hw_restore\n", __func__);
		mdelay(bmm_get_forced_drdy_time(client_data->rept_xy,
					client_data->rept_z));
	}
	mutex_unlock(&client_data->mutex_op_mode);


	pr_info("%s register dump after init\n", __func__);
	bmm_dump_reg(client);

	return err;
}

static int bmm_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	struct bmm_client_data *data = NULL;
	struct device_node *np = client->dev.of_node;
	int dummy;

	pr_info("%s unction entrance\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s i2c_check_functionality error!\n", __func__);
		err = -EIO;
		goto exit_err_clean;
	}

	if (NULL == bmm_client) {
		bmm_client = client;
	} else {
		pr_err("%s this driver does not support multiple clients\n", __func__);
		err = -EBUSY;
		return err;
	}

	/* wake up the chip */
	dummy = bmm_wakeup(client);
	if (dummy < 0) {
		pr_err("%s Cannot wake up %s, I2C xfer error\n", __func__, SENSOR_NAME);
		err = -EIO;
		goto exit_err_clean;
	}

	pr_info("%s register dump after waking up\n", __func__);
	bmm_dump_reg(client);
	/* check chip id */
	err = bmm_check_chip_id(client);
	if (!err) {
		pr_notice("%s Bosch Sensortec Device %s detected: %#x\n",
				__func__, SENSOR_NAME, client->addr);
	} else {
		pr_err("%s Bosch Sensortec Device not found, chip id mismatch\n", __func__);
		err = -1;
		goto exit_err_clean;
	}

	data = kzalloc(sizeof(struct bmm_client_data), GFP_KERNEL);
	if (NULL == data) {
		pr_err("%s no memory available\n", __func__);
		err = -ENOMEM;
		goto exit_err_clean;
	}

	i2c_set_clientdata(client, data);
	data->client = client;

	mutex_init(&data->mutex_power_mode);
	mutex_init(&data->mutex_op_mode);
	mutex_init(&data->mutex_enable);
	mutex_init(&data->mutex_odr);
	mutex_init(&data->mutex_rept_xy);
	mutex_init(&data->mutex_rept_z);
	mutex_init(&data->mutex_value);

	/* input device init */
	err = bmm_input_init(data);
	if (err < 0)
		goto exit_err_clean;
/*
	err = sensors_create_symlink(&data->input->dev.kobj,
					data->input->name);
	if (err < 0) {
		pr_err("%s failed sensors_create_symlink\n", __func__);
		goto err_sensors_create_symlink;
	}
*/
	/* sysfs node creation */
	err = sysfs_create_group(&data->input->dev.kobj,
			&bmm_attribute_group);

	if (err < 0)
		goto err_sysfs_create_group;

#ifdef CONFIG_BMM_USE_PLATFORM_DATA
	if (NULL != client->dev.platform_data) {
		data->bst_pd = kzalloc(sizeof(*data->bst_pd), GFP_KERNEL);
		if (NULL != data->bst_pd) {
			memcpy(data->bst_pd, client->dev.platform_data,
					sizeof(*data->bst_pd));

			pr_info("%s platform data of bmm %s: place: %d\n", __func__,
						data->bst_pd->name,	data->bst_pd->place);
		}
	}
	else
#ifdef CONFIG_OF
	//if (NULL != data->bst_pd) 
	{
		err = of_property_read_u32(np, "bmm050,magnetic_place", &data->place/*&data->bst_pd->place*/);
		if (unlikely(err)) {
			pr_err("error reading property magnetic_place from device node %d\n", data->place/*data->bst_pd->place*/);
			return err;
		}
	}
#endif
#endif

	err = sensors_register(magnetic_device, data,
		bmm_attributes_sensors, "magnetic_sensor");
	if (err < 0) {
		pr_info("%s: could not sensors_register\n", __func__);
		goto exit_bmm_sensors_register;
	}

	/* workqueue init */
	INIT_DELAYED_WORK(&data->work, bmm_work_func);
	atomic_set(&data->delay, BMM_DELAY_DEFAULT);

	/* h/w init */
	data->device.bus_read = bmm_i2c_read_wrapper;
	data->device.bus_write = bmm_i2c_write_wrapper;
	data->device.delay_msec = bmm_delay;
	BMM_CALL_API(init)(&data->device);

	bmm_dump_reg(client);

	pr_info("%s trimming_reg x1: %d y1: %d x2: %d y2: %d xy1: %d xy2: %d\n",
			__func__, data->device.dig_x1,
			data->device.dig_y1,
			data->device.dig_x2,
			data->device.dig_y2,
			data->device.dig_xy1,
			data->device.dig_xy2);

	pr_info("%s trimming_reg z1: %d z2: %d z3: %d z4: %d xyz1: %d\n",
			__func__, data->device.dig_z1,
			data->device.dig_z2,
			data->device.dig_z3,
			data->device.dig_z4,
			data->device.dig_xyz1);

	data->enable = 0;
	/* now it's power on which is considered as resuming from suspend */
	data->op_mode = BMM_VAL_NAME(SUSPEND_MODE);
	data->odr = BMM_DEFAULT_ODR;
	data->rept_xy = BMM_DEFAULT_REPETITION_XY;
	data->rept_z = BMM_DEFAULT_REPETITION_Z;

	err = bmm_set_op_mode(data, BMM_VAL_NAME(SUSPEND_MODE));
	if (err) {
		pr_err("%s fail to init h/w of %s\n", __func__, SENSOR_NAME);
		err = -EIO;
		goto exit_bmm_sensors_register;
	}

	data->selftest = 0;

	pr_notice("%s sensor %s probed successfully\n", __func__, SENSOR_NAME);

	pr_info("%s i2c_client: %p client_data: %p i2c_device: %p input: %p\n",
			__func__, client, data, &client->dev, data->input);

	return 0;

exit_bmm_sensors_register:
	sysfs_remove_group(&data->input->dev.kobj,
		&bmm_attribute_group);
err_sysfs_create_group:
//	sensors_remove_symlink(&data->input->dev.kobj, data->input->name);
//err_sensors_create_symlink:
	input_unregister_device(data->input);
	input_free_device(data->input);
	//bmm_input_destroy(data);

exit_err_clean:
	if (err) {
		if (data != NULL) {
#ifdef CONFIG_BMM_USE_PLATFORM_DATA
			if (NULL != data->bst_pd) {
				kfree(data->bst_pd);
				data->bst_pd = NULL;
			}
#endif
			kfree(data);
			data = NULL;
		}

		bmm_client = NULL;
	}

	return err;
}

static int bmm_pre_suspend(struct i2c_client *client)
{
	int err = 0;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);
	pr_debug("%s function entrance\n", __func__);

	mutex_lock(&client_data->mutex_enable);
	if (client_data->enable) {
		cancel_delayed_work_sync(&client_data->work);
		pr_debug("%s cancel work\n", __func__);
	}
	mutex_unlock(&client_data->mutex_enable);

	return err;
}

static int bmm_post_resume(struct i2c_client *client)
{
	int err = 0;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);

	mutex_lock(&client_data->mutex_enable);
	if (client_data->enable) {
		schedule_delayed_work(&client_data->work,
				msecs_to_jiffies(
					atomic_read(&client_data->delay)));
	}
	mutex_unlock(&client_data->mutex_enable);

	return err;
}

static int bmm_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int err = 0;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);
	u8 power_mode;

	pr_debug("%s function entrance\n", __func__);

	mutex_lock(&client_data->mutex_power_mode);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode) {
		err = bmm_pre_suspend(client);
		err = bmm_set_op_mode(client_data, BMM_VAL_NAME(SUSPEND_MODE));
	}
	mutex_unlock(&client_data->mutex_power_mode);

	return err;
}

static int bmm_resume(struct i2c_client *client)
{
	int err = 0;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);

	pr_debug("%s function entrance\n", __func__);

	mutex_lock(&client_data->mutex_power_mode);
	err = bmm_restore_hw_cfg(client);
	/* post resume operation */
	bmm_post_resume(client);

	mutex_unlock(&client_data->mutex_power_mode);

	return err;
}

static int bmm_remove(struct i2c_client *client)
{
	int err = 0;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);

	if (NULL != client_data) {

		mutex_lock(&client_data->mutex_op_mode);
		if (BMM_VAL_NAME(NORMAL_MODE) == client_data->op_mode) {
			cancel_delayed_work_sync(&client_data->work);
			pr_debug("%s cancel work\n", __func__);
		}
		mutex_unlock(&client_data->mutex_op_mode);

		err = bmm_set_op_mode(client_data, BMM_VAL_NAME(SUSPEND_MODE));
		mdelay(BMM_I2C_WRITE_DELAY_TIME);

		sysfs_remove_group(&client_data->input->dev.kobj,
				&bmm_attribute_group);
		bmm_input_destroy(client_data);

#ifdef CONFIG_BMM_USE_PLATFORM_DATA
			if (NULL != client_data->bst_pd) {
				kfree(client_data->bst_pd);
				client_data->bst_pd = NULL;
			}
#endif
		kfree(client_data);

		bmm_client = NULL;
	}

	return err;
}

#ifdef CONFIG_OF
static const struct of_device_id bmm_dt_ids[] = {
	{ .compatible = "bmm050", },
	{},
};
MODULE_DEVICE_TABLE(of, bmm_dt_ids);
#endif

static const struct i2c_device_id bmm_id[] = {
	{SENSOR_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, bmm_id);

static struct i2c_driver bmm_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(bmm_dt_ids),
#endif
	},
	.class = I2C_CLASS_HWMON,
	.id_table = bmm_id,
	.probe = bmm_probe,
	.remove = bmm_remove,
	.suspend = bmm_suspend,
	.resume = bmm_resume,
};

static int __init BMM_init(void)
{
	return i2c_add_driver(&bmm_driver);
}

static void __exit BMM_exit(void)
{
	i2c_del_driver(&bmm_driver);
}

MODULE_AUTHOR("Bosch Sensortec <contact@bosch-sensortec.com>");
MODULE_DESCRIPTION("driver for " SENSOR_NAME);
MODULE_LICENSE("GPL");

module_init(BMM_init);
module_exit(BMM_exit);
