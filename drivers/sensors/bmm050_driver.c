/*
 * Last modified: Nov 12th, 2012
 * Revision: V2.5
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
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
#include "sensors_head.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/bst_sensor_common.h>

#include "bmm050.h"
#include "bs_log.h"

/* sensor specific */
#define SENSOR_NAME "bmm050"

#define	VENDOR		"BOSCH"
#define	CHIP_ID		"BMM050"

#define SENSOR_CHIP_ID_BMM (0x32)

#define BMM_I2C_WRITE_DELAY_TIME 2

#define BMM_DEFAULT_REPETITION_XY	BMM050_REGULAR_REPXY
#define BMM_DEFAULT_REPETITION_Z	BMM050_REGULAR_REPZ
#define BMM_DEFAULT_ODR			BMM050_REGULAR_DR
/* generic */
#define BMM_MAX_RETRY_I2C_XFER (10)
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
	{"normal", BMM050_NORMAL_MODE},
	{"forced", BMM050_FORCED_MODE},
	{"suspend", BMM050_SUSPEND_MODE},
	{"sleep", BMM050_SLEEP_MODE},
};

#define BMM_USE_BASIC_I2C_FUNC

struct bmm_client_data {
	struct bmm050 device;
	struct i2c_client *client;
	struct input_dev *input;
	struct delayed_work work;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend_handler;
#endif

	atomic_t delay;

	struct bmm050_mdata value;
	u8 enable:1;
	s8 op_mode:4;
	u8 odr;
	u8 rept_xy;
	u8 rept_z;

	s16 result_test;
	int result_x;
	int result_y;
	int result_z;

	struct mutex mutex_power_mode;

	/* controls not only reg, but also workqueue */
	struct mutex mutex_op_mode;
	struct mutex mutex_enable;
	struct mutex mutex_odr;
	struct mutex mutex_rept_xy;
	struct mutex mutex_rept_z;

	struct mutex mutex_value;
#ifdef BMM_USE_PLATFORM_DATA
	struct bosch_sensor_specific *bst_pd;
#endif
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

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bmm_early_suspend(struct early_suspend *handler);
static void bmm_late_resume(struct early_suspend *handler);
#endif

static int bmm_restore_hw_cfg(struct i2c_client *client);

static void bmm_remap_sensor_data(struct bmm050_mdata *val,
		struct bmm_client_data *client_data)
{
#ifdef BMM_USE_PLATFORM_DATA
	struct bosch_sensor_data bsd;

	if (NULL == client_data->bst_pd)
		return;

	if (BOSCH_SENSOR_PLACE_UNKNOWN == client_data->bst_pd->place) {
		pr_info("%s : BOSCH_SENSOR_PLACE_UNKNOWN\n", __func__);
		return;
	}

	bsd.x = val->datax;
	bsd.y = val->datay;
	bsd.z = val->dataz;

	bst_remap_sensor_data_dft_tab(&bsd,
			client_data->bst_pd->place);

	val->datax = bsd.x;
	val->datay = bsd.y;
	val->dataz = bsd.z;
#else
	(void)val;
	(void)client_data;
#endif
}

static void bmm_remap_sensor_data_s32(struct bmm050_mdata_s32 *val,
		struct bmm_client_data *client_data)
{
#ifdef BMM_USE_PLATFORM_DATA
	struct bosch_sensor_data bsd;

	if (NULL == client_data->bst_pd)
		return;

	if (BOSCH_SENSOR_PLACE_UNKNOWN == client_data->bst_pd->place) {
		pr_info("%s : BOSCH_SENSOR_PLACE_UNKNOWN\n", __func__);
		return;
	}

	bsd.x = val->datax;
	bsd.y = val->datay;
	bsd.z = val->dataz;

	bst_remap_sensor_data_dft_tab(&bsd,
			client_data->bst_pd->place);

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

	bmm_i2c_read(client, BMM050_CHIP_ID, &chip_id, 1);
	pr_info("%s : read chip id result: %#x", __func__, chip_id);

	if ((chip_id & 0xff) != SENSOR_CHIP_ID_BMM)
		err = -1;

	return err;
}

static void bmm_delay(u32 msec)
{
	mdelay(msec);
}

static void bmm_dump_reg(struct i2c_client *client)
{
	int i;
	u8 dbg_buf[64];
	u8 dbg_buf_str[64 * 3 + 1] = "";

	for (i = 0; i < BYTES_PER_LINE; i++) {
		dbg_buf[i] = i;
		sprintf(dbg_buf_str + i * 3, "%02x%c",
				dbg_buf[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	pr_debug("%s : %s\n", __func__, dbg_buf_str);

	bmm_i2c_read(client, BMM050_CHIP_ID, dbg_buf, 64);
	for (i = 0; i < 64; i++) {
		sprintf(dbg_buf_str + i * 3, "%02x%c",
				dbg_buf[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	pr_debug("%s : %s\n", __func__, dbg_buf_str);
}

static int bmm_wakeup(struct i2c_client *client)
{
	int err = 0;
	int try_times = BMM_MAX_RETRY_WAKEUP;
	const u8 value = 0x01;
	u8 dummy;

	pr_info("%s : waking up the chip..\n", __func__);

	while (try_times) {
		err = bmm_i2c_write(client,
				BMM050_POWER_CNTL, (u8 *)&value, 1);
		mdelay(BMM_I2C_WRITE_DELAY_TIME);
		dummy = 0;
		err = bmm_i2c_read(client, BMM050_POWER_CNTL, &dummy, 1);
		if (value == dummy)
			break;

		try_times--;
	}

	pr_info("%s : wake up result: %s, tried times: %d", __func__,
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
	if (NULL == client) {
		pr_info("%s : client is null\n", __func__);
		return -1;
	}

	while (0 != len--) {
#ifdef BMM_SMBUS
		dummy = i2c_smbus_read_byte_data(client, reg_addr);
		if (dummy < 0) {
			pr_err("%s : i2c bus read error", __func__);
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
		pr_err("%s : I2C xfer error", __func__);
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
			pr_err("%s : error writing i2c bus", __func__);
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
			pr_err("%s : I2C xfer error", __func__);
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
	err = bmm_i2c_write(client, BMM050_CONTROL, (u8 *)&value, 1);

	return err;
}

static int bmm_set_normal_mode(struct i2c_client *client)
{
	int err = 0;

	/* FORCED_MODE */
	const u8 value = 0x00;
	err = bmm_i2c_write(client, BMM050_CONTROL, (u8 *)&value, 1);

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
	if (BMM050_NORMAL_MODE != client_data->op_mode)
		bmm_set_normal_mode(client);

	mutex_unlock(&client_data->mutex_op_mode);

	bmm050_read_mdataXYZ(&client_data->value);
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
	int i;

	for (i = 0; i < ARRAY_SIZE(odr_map); i++) {
		if (odr_map[i] == odr)
			break;
	}

	if (ARRAY_SIZE(odr_map) == i) {
		err = -1;
		return err;
	}

	err = bmm050_set_datarate(i);
	mdelay(BMM_I2C_WRITE_DELAY_TIME);

	return err;
}

static int bmm_get_odr(struct i2c_client *client, u8 *podr)
{
	int err = 0;
	u8 value;

	err = bmm050_get_datarate(&value);
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
	bmm050_get_powermode(&power_mode);
	if (power_mode) {
		mutex_lock(&client_data->mutex_op_mode);
		bmm050_get_functional_state(&op_mode);
		mutex_unlock(&client_data->mutex_op_mode);
	} else {
		op_mode = BMM050_SUSPEND_MODE;
	}

	mutex_unlock(&client_data->mutex_power_mode);

	PDEBUG("op_mode: %d", op_mode);

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


static ssize_t bmm_store_op_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err = 0;
	int i;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	struct i2c_client *client = client_data->client;
	int op_mode;

	err = kstrtoint(buf, 10, &op_mode);
	if (err)
		return err;

	mutex_lock(&client_data->mutex_power_mode);

	i = bmm_get_op_mode_idx(op_mode);

	if (i != -1) {
		mutex_lock(&client_data->mutex_op_mode);
		if (op_mode != client_data->op_mode) {
			if (BMM050_FORCED_MODE == op_mode) {
				/* special treat of forced mode
				 * for optimization */
				err = bmm_set_forced_mode(client);
			} else {
				err = bmm050_set_functional_state(
						op_mode);
			}

			if (!err) {
				if (BMM050_FORCED_MODE == op_mode)
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
	bmm050_get_powermode(&power_mode);
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
	int tmp;
	unsigned char data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	struct i2c_client *client = client_data->client;
	u8 power_mode;
	int i;

	err = kstrtoint(buf, 10, &tmp);
	if (err)
		return err;

	if (tmp > 255)
		return -EINVAL;

	data = (unsigned char)tmp;

	mutex_lock(&client_data->mutex_power_mode);
	bmm050_get_powermode(&power_mode);
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
	bmm050_get_powermode(&power_mode);
	if (power_mode) {
		mutex_lock(&client_data->mutex_rept_xy);
		err = bmm050_get_repetitions_XY(&data);
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
	int tmp = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	int err;
	u8 data;
	u8 power_mode;

	err = kstrtoint(buf, 10, &tmp);
	if (err)
		return err;

	if (tmp > 255)
		return -EINVAL;

	data = (unsigned char)tmp;

	mutex_lock(&client_data->mutex_power_mode);
	bmm050_get_powermode(&power_mode);
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
	bmm050_get_powermode(&power_mode);
	if (power_mode) {
		mutex_lock(&client_data->mutex_rept_z);
		err = bmm050_get_repetitions_Z(&data);
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
	int tmp = 0;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	int err;
	u8 data;
	u8 power_mode;

	err = kstrtoint(buf, 10, &tmp);
	if (err)
		return err;

	if (tmp > 255)
		return -EINVAL;

	data = (unsigned char)tmp;

	mutex_lock(&client_data->mutex_power_mode);
	bmm050_get_powermode(&power_mode);
	if (power_mode) {
		mutex_lock(&client_data->mutex_rept_z);
		err = bmm050_set_repetitions_Z(data);
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

	bmm050_read_mdataXYZ(&client_data->value);
	bmm_remap_sensor_data(&client_data->value, client_data);

	count = sprintf(buf, "%hd %hd %hd\n",
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
	struct bmm050_mdata_s32 value;
	static struct bmm050_mdata_s32 prev_value = {0,};
/*
	static struct bmm050_mdata_s32 bvalue[3] = {0,};
	static int b = 0;
	static int ix = 0, iy = 0, iz = 0;
	int x_comp, y_comp, z_comp;
*/
	int count;

	mutex_lock(&client_data->mutex_op_mode);

	/* set bmm normal mode */
	if (BMM050_FORCED_MODE != client_data->op_mode) {
		bmm050_set_functional_state(BMM050_FORCED_MODE);
		msleep(30);
	}

	bmm050_read_mdataXYZ_s32(&value);

	/* restore bmm operation mode */
	if (BMM050_FORCED_MODE != client_data->op_mode)
		bmm050_set_functional_state(client_data->op_mode);

	mutex_unlock(&client_data->mutex_op_mode);

#if 0
	if (!value.drdy) {
		pr_info("%s : sensor data not ready\n", __func__);
		goto done;
	}
#endif
/*
	x_comp = value.datax - prev_value.datax;
	x_comp = (x_comp < 0)? x_comp * -1 : x_comp;
	y_comp = value.datay - prev_value.datay;
	y_comp = (y_comp < 0)? y_comp * -1 : y_comp;
	z_comp = value.dataz - prev_value.dataz;
	z_comp = (z_comp < 0)? z_comp * -1 : z_comp;

	if (x_comp > 500 && ix == 0) {
		ix = 1;
		value.datax = prev_value.datax;
	} else
		ix = 0;

	if (y_comp > 500 && iy == 0) {
		iy = 1;
		value.datay = prev_value.datay;
	} else
		iy = 0;

	if (z_comp > 500 && iz == 0) {
		iz = 1;
		value.dataz = prev_value.dataz;
	} else
		iz = 0;
*/
	if ((value.datax == 0) && (value.datay == 0)) {
		value.datax = prev_value.datax;
		value.datay = prev_value.datay;
		value.dataz = prev_value.dataz;
	} else {
		prev_value.datax = value.datax;
		prev_value.datay = value.datay;
		prev_value.dataz = value.dataz;
	}

/*
	bmm_remap_sensor_data_s32(&value, client_data);

	b%=3;
	bvalue[b].datax = value.datax;
	bvalue[b].datay = value.datay;
	bvalue[b].dataz = value.dataz;
	b++;
	value.datax = (bvalue[0].datax + bvalue[1].datax
					+ bvalue[2].datax) / 3;
	value.datay = (bvalue[0].datay + bvalue[1].datay
					+ bvalue[2].datay) / 3;
	value.dataz = (bvalue[0].dataz + bvalue[1].dataz
					+ bvalue[2].dataz) / 3;
*/
	if (value.datax == BMM050_OVERFLOW_OUTPUT_S32)
		value.datax = -20800;
	if (value.datay == BMM050_OVERFLOW_OUTPUT_S32)
		value.datay = -20800;
	if (value.dataz == BMM050_OVERFLOW_OUTPUT_S32)
		value.dataz = -40000;

	count = sprintf(buf, "%d,%d,%d\n",
			value.datax,
			value.datay,
			value.dataz);
	return count;
}


static int bmm_show_status(struct device *dev,
	/* set bmm normal mode */
	/* restore bmm operation mode */
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	struct i2c_client *client = client_data->client;
	int err = 0;
	int result = 0;
	u8 pcntlReg;

	err = bmm_i2c_read(client, BMM050_POWER_CNTL, &pcntlReg, 1);
	result = (err == 0) ? 1 : 0;

	err = sprintf(buf, "%d\n", result);
	pr_info("%s : power on is = %d\n", __func__, result);
	return err;
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
	pr_info("%s : %s\n", __func__, buf);
	return err;
}

static ssize_t bmm_store_enable(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	pr_info("%s : %s %d", __func__, buf, client_data->enable);
	err = kstrtoint(buf, 10, &data);
	if (err)
		return err;

	data = data ? 1 : 0;
	mutex_lock(&client_data->mutex_enable);
	if (data != client_data->enable) {
		if (data) {
			pr_info("go work %s %d %d",
				__func__, data, client_data->enable);
			schedule_delayed_work(
					&client_data->work,
					msecs_to_jiffies(atomic_read(
							&client_data->delay)));
		} else {
			pr_info("stop work %s %d %d",
				__func__, data, client_data->enable);
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
	pr_info("%s\n", __func__);
	return sprintf(buf, "%d\n", atomic_read(&client_data->delay));
}

static ssize_t bmm_store_delay(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int err;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	pr_info("%s\n", __func__);
	err = kstrtoint(buf, 10, &data);
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
	pr_info("%s+\n", __func__);
	err = sprintf(buf, "%d\n", client_data->result_test);
	pr_info("%s-\n", __func__);
	return err;
}

static ssize_t bmm_store_test(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int data;
	int err;
	int x, y, z;
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	struct i2c_client *client = client_data->client;
	u8 result;
	err = kstrtoint(buf, 10, &data);
	if (err)
		return err;

	pr_info("%s+ %d\n", __func__, data);

	/* the following code assumes the work thread is not running */
	if (BMM_SELF_TEST == data) {
		/* self test */
		err = bmm050_set_functional_state(BMM050_SLEEP_MODE);
		mdelay(3);
		err = bmm050_set_selftest(1);
		mdelay(3);
		err = bmm050_get_self_test_XYZ(&result, &x, &y, &z);
		client_data->result_test = result;
	} else if (BMM_ADV_TEST == data) {
		/* advanced self test */
		err = bmm050_perform_advanced_selftest(
					&client_data->result_test);
	} else {
		err = -EINVAL;
	}

	if (!err) {
		bmm050_soft_reset();
		bmm_restore_hw_cfg(client);
	}

	err = sprintf(buf, "%d, %d, %d, %d\n",
			client_data->result_test, x, y, z);
	pr_info("%s-\n", __func__);
	return err;
}

static ssize_t bmm_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int x = 0, y = 0, z = 0;
	u8 result;
	u8 op_mode;

	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
	struct i2c_client *client = client_data->client;

	client_data->result_test = 0;

	bmm050_get_functional_state(&op_mode);
	pr_info("%s : op_mode = %s\n", __func__,
		op_mode_maps[op_mode].op_mode_name);

	if (bmm050_set_functional_state(BMM050_SLEEP_MODE)) {
		pr_info("%s : BMM050_SLEEP_MODE error\n", __func__);
		goto exit_err;
	}
	mdelay(3);
	if (bmm050_set_functional_state(BMM050_NORMAL_MODE)) {
		pr_info("%s : BMM050_NORMAL_MODE error\n", __func__);
		goto exit_err;
	}
	mdelay(3);
	if (bmm050_set_selftest(1)) {
		pr_info("%s : bmm050_set_selftest error\n", __func__);
		goto exit_err;
	}
	msleep(30);
	if (bmm050_get_self_test_XYZ(&result, &x, &y, &z)) {
		pr_info("%s : bmm050_set_selftest error\n", __func__);
		goto exit_err;
	}

	bmm050_soft_reset();
	mdelay(BMM_I2C_WRITE_DELAY_TIME);
	bmm_restore_hw_cfg(client);

	if (result != 0x07) {
		pr_info("%s : fail, result = 0x%02x\n", __func__, result);
		client_data->result_test = 0;
		goto exit_err;
	}

	client_data->result_x = x;
	client_data->result_y = y;
	client_data->result_z = z;

	if ((x > -20800 && x < 20800) &&
	    (y > -20800 && y < 20800) &&
	    (z > -40000 && z < 40000))
		client_data->result_test = 1;
	else
		client_data->result_test = 0;

	pr_info("%s : %s x = %d, y = %d, z = %d\n", __func__,
		(client_data->result_test) ? "success" : "fail",
		x, y, z);
exit_err:
	bmm050_set_functional_state(op_mode);

	return sprintf(buf, "%d, %d, %d, %d\n",
			client_data->result_test, x, y, z);
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


	bmm_i2c_read(client, BMM050_CHIP_ID, dbg_buf, 64);
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
#ifdef BMM_USE_PLATFORM_DATA
	struct input_dev *input = to_input_dev(dev);
	struct bmm_client_data *client_data = input_get_drvdata(input);
#endif
	int place = BOSCH_SENSOR_PLACE_UNKNOWN;

#ifdef BMM_USE_PLATFORM_DATA
	if (NULL != client_data->bst_pd)
		place = client_data->bst_pd->place;
#endif
	return sprintf(buf, "%d\n", place);
}

static ssize_t bmm_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t bmm_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
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
static DEVICE_ATTR(raw_data, S_IRUGO,
		bmm_show_value_raw, NULL);
static DEVICE_ATTR(status, S_IRUGO,
		bmm_show_status, NULL);
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
static DEVICE_ATTR(vendor, S_IRUGO,
		bmm_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO,
		bmm_name_show, NULL);
static struct attribute *bmm_attributes[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_op_mode.attr,
	&dev_attr_odr.attr,
	&dev_attr_rept_xy.attr,
	&dev_attr_rept_z.attr,
	&dev_attr_value.attr,
	&dev_attr_raw_data.attr,
	&dev_attr_status.attr,
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

static struct device_attribute *bmm_sysfs_attributes[] = {
	&dev_attr_chip_id,
	&dev_attr_op_mode,
	&dev_attr_raw_data,
	&dev_attr_status,
	&dev_attr_enable,
	&dev_attr_delay,
	&dev_attr_selftest,
	&dev_attr_vendor,
	&dev_attr_name,
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
	err = bmm050_set_functional_state(BMM050_SLEEP_MODE);

	if (bmm_get_op_mode_idx(client_data->op_mode) != -1)
		err = bmm050_set_functional_state(client_data->op_mode);

	op_mode = client_data->op_mode;
	mutex_unlock(&client_data->mutex_op_mode);

	if (BMM050_SUSPEND_MODE == op_mode)
		return err;

	pr_info("%s : app did not close this sensor before suspend",
			__func__);

	mutex_lock(&client_data->mutex_odr);
	bmm050_set_datarate(client_data->odr);
	mdelay(BMM_I2C_WRITE_DELAY_TIME);
	mutex_unlock(&client_data->mutex_odr);

	mutex_lock(&client_data->mutex_rept_xy);
	err = bmm_i2c_write(client, BMM050_NO_REPETITIONS_XY,
			&client_data->rept_xy, 1);
	mdelay(BMM_I2C_WRITE_DELAY_TIME);
	err = bmm_i2c_read(client, BMM050_NO_REPETITIONS_XY, &value, 1);
	pr_info("%s : BMM_NO_REPETITIONS_XY: %02x", __func__, value);
	mutex_unlock(&client_data->mutex_rept_xy);

	mutex_lock(&client_data->mutex_rept_z);
	err = bmm_i2c_write(client, BMM050_NO_REPETITIONS_Z,
			&client_data->rept_z, 1);
	mdelay(BMM_I2C_WRITE_DELAY_TIME);
	err = bmm_i2c_read(client, BMM050_NO_REPETITIONS_Z, &value, 1);
	pr_info("%s : BMM_NO_REPETITIONS_Z: %02x", __func__, value);
	mutex_unlock(&client_data->mutex_rept_z);

	pr_info("%s : register dump after init", __func__);
	bmm_dump_reg(client);

	return err;
}

static int bmm_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	struct bmm_client_data *client_data = NULL;
	int dummy;

	pr_info("%s : function entrance", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : i2c_check_functionality error!", __func__);
		err = -EIO;
		goto exit_err_clean;
	}

	if (NULL == bmm_client) {
		pr_info("%s : bmm_client set", __func__);
		bmm_client = client;
	} else {
		pr_err("%s : this driver does not support multiple clients"
			, __func__);
		err = -EINVAL;
		goto exit_err_clean;
	}

	/* wake up the chip */
	dummy = bmm_wakeup(client);
	if (dummy < 0) {
		pr_err("%s : Cannot wake up %s, I2C xfer error",
				__func__, SENSOR_NAME);
		err = -EIO;
		goto exit_err_clean;
	}

	pr_info("%s : register dump after waking up", __func__);
	bmm_dump_reg(client);
	/* check chip id */
	err = bmm_check_chip_id(client);
	if (!err) {
		PNOTICE("Bosch Sensortec Device %s detected", SENSOR_NAME);
	} else {
		pr_err("Bosch Sensortec Device not found, chip id mismatch");
		err = -1;
		goto exit_err_clean;
	}

	client_data = kzalloc(sizeof(struct bmm_client_data), GFP_KERNEL);
	if (NULL == client_data) {
		pr_err("%s : no memory available", __func__);
		err = -ENOMEM;
		goto exit_err_clean;
	}

	i2c_set_clientdata(client, client_data);
	client_data->client = client;

	mutex_init(&client_data->mutex_power_mode);
	mutex_init(&client_data->mutex_op_mode);
	mutex_init(&client_data->mutex_enable);
	mutex_init(&client_data->mutex_odr);
	mutex_init(&client_data->mutex_rept_xy);
	mutex_init(&client_data->mutex_rept_z);
	mutex_init(&client_data->mutex_value);

	/* input device init */
	err = bmm_input_init(client_data);
	if (err < 0)
		goto exit_err_clean;

	/* sysfs node creation */
	err = sysfs_create_group(&client_data->input->dev.kobj,
			&bmm_attribute_group);

	if (err < 0)
		goto exit_err_sysfs;

#ifdef BMM_USE_PLATFORM_DATA
	if (NULL != client->dev.platform_data) {
		client_data->bst_pd = kzalloc(sizeof(*client_data->bst_pd),
				GFP_KERNEL);

		if (NULL != client_data->bst_pd) {
			memcpy(client_data->bst_pd, client->dev.platform_data,
					sizeof(*client_data->bst_pd));

			pr_info("%s : platform data of bmm %s: place: %d",
				__func__,
				client_data->bst_pd->name,
				client_data->bst_pd->place);
		}
	}
#endif

	err = sensors_register(&magnetic_device, client_data,
		bmm_sysfs_attributes, "magnetic_sensor");
	if (err < 0) {
		pr_info("%s: could not sensors_register\n", __func__);
		goto exit_bmm_sensors_register;
	}

	/* workqueue init */
	INIT_DELAYED_WORK(&client_data->work, bmm_work_func);
	atomic_set(&client_data->delay, BMM_DELAY_DEFAULT);

	/* h/w init */
	client_data->device.bus_read = bmm_i2c_read_wrapper;
	client_data->device.bus_write = bmm_i2c_write_wrapper;
	client_data->device.delay_msec = bmm_delay;
	bmm050_init(&client_data->device);

	bmm_dump_reg(client);

	PDEBUG("trimming_reg x1: %d y1: %d x2: %d y2: %d xy1: %d xy2: %d",
			client_data->device.dig_x1,
			client_data->device.dig_y1,
			client_data->device.dig_x2,
			client_data->device.dig_y2,
			client_data->device.dig_xy1,
			client_data->device.dig_xy2);

	PDEBUG("trimming_reg z1: %d z2: %d z3: %d z4: %d xyz1: %d",
			client_data->device.dig_z1,
			client_data->device.dig_z2,
			client_data->device.dig_z3,
			client_data->device.dig_z4,
			client_data->device.dig_xyz1);

	client_data->enable = 0;
	/* now it's power on which is considered as resuming from suspend */
	client_data->op_mode = BMM050_NORMAL_MODE;
	client_data->odr = BMM_DEFAULT_ODR;
	client_data->rept_xy = BMM_DEFAULT_REPETITION_XY;
	client_data->rept_z = BMM_DEFAULT_REPETITION_Z;
	bmm_set_normal_mode(client);

#if 0
	err = bmm_restore_hw_cfg(client);
#else
	err = bmm050_set_functional_state(
			BMM050_NORMAL_MODE);
	if (err) {
		pr_err("%s : fail to init h/w of %s\n", __func__,
					SENSOR_NAME);
		err = -EIO;
		goto exit_err_sysfs;
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	client_data->early_suspend_handler.level =
		EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	client_data->early_suspend_handler.suspend = bmm_early_suspend;
	client_data->early_suspend_handler.resume = bmm_late_resume;
	register_early_suspend(&client_data->early_suspend_handler);
#endif

	PNOTICE("sensor %s probed successfully", SENSOR_NAME);

	PDEBUG("i2c_client: %p client_data: %p i2c_device: %p input: %p",
			client, client_data, &client->dev, client_data->input);

	return 0;

exit_bmm_sensors_register:
	sysfs_remove_group(&client_data->input->dev.kobj,
		&bmm_attribute_group);

exit_err_sysfs:
	if (err)
		bmm_input_destroy(client_data);

exit_err_clean:
	if (err) {
		if (client_data != NULL) {
#ifdef BMM_USE_PLATFORM_DATA
			if (NULL != client_data->bst_pd) {
				kfree(client_data->bst_pd);
				client_data->bst_pd = NULL;
			}
#endif
			kfree(client_data);
			client_data = NULL;
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
	PDEBUG("function entrance");

	mutex_lock(&client_data->mutex_enable);
	if (client_data->enable) {
		cancel_delayed_work_sync(&client_data->work);
		PDEBUG("cancel work");
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

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bmm_early_suspend(struct early_suspend *handler)
{
	int err = 0;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)container_of(handler,
			struct bmm_client_data, early_suspend_handler);
	struct i2c_client *client = client_data->client;
	u8 power_mode;
	PDEBUG("function entrance");

	mutex_lock(&client_data->mutex_power_mode);
	bmm050_get_powermode(&power_mode);
	if (power_mode) {
		err = bmm_pre_suspend(client);
		err = bmm050_set_functional_state(
				BMM050_SUSPEND_MODE);
	}
	mutex_unlock(&client_data->mutex_power_mode);
}

static void bmm_late_resume(struct early_suspend *handler)
{
	int err = 0;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)container_of(handler,
			struct bmm_client_data, early_suspend_handler);
	struct i2c_client *client = client_data->client;
	PDEBUG("function entrance");

	mutex_lock(&client_data->mutex_power_mode);

	err = bmm_restore_hw_cfg(client);
	/* post resume operation */
	bmm_post_resume(client);

	mutex_unlock(&client_data->mutex_power_mode);
}
#else
static int bmm_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int err = 0;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);
	u8 power_mode;

	PDEBUG("function entrance");

	mutex_lock(&client_data->mutex_power_mode);
	bmm050_get_powermode(&power_mode);
	if (power_mode) {
		err = bmm_pre_suspend(client);
		err = bmm050_set_functional_state(
				BMM050_SUSPEND_MODE);
	}
	mutex_unlock(&client_data->mutex_power_mode);

	return err;
}

static int bmm_resume(struct i2c_client *client)
{
	int err = 0;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);

	PDEBUG("function entrance");

	mutex_lock(&client_data->mutex_power_mode);
	err = bmm_restore_hw_cfg(client);
	/* post resume operation */
	bmm_post_resume(client);

	mutex_unlock(&client_data->mutex_power_mode);

	return err;
}
#endif

static int bmm_remove(struct i2c_client *client)
{
	int err = 0;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);

	if (NULL != client_data) {
#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&client_data->early_suspend_handler);
#endif

		mutex_lock(&client_data->mutex_op_mode);
		if (BMM050_NORMAL_MODE == client_data->op_mode) {
			cancel_delayed_work_sync(&client_data->work);
			PDEBUG("cancel work");
		}
		mutex_unlock(&client_data->mutex_op_mode);

		err = bmm050_set_functional_state(
				BMM050_SUSPEND_MODE);
		mdelay(BMM_I2C_WRITE_DELAY_TIME);

		sysfs_remove_group(&client_data->input->dev.kobj,
				&bmm_attribute_group);
		bmm_input_destroy(client_data);

#ifdef BMM_USE_PLATFORM_DATA
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

static const struct i2c_device_id bmm_id[] = {
	{SENSOR_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, bmm_id);

static struct i2c_driver bmm_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
	},
	.class = I2C_CLASS_HWMON,
	.id_table = bmm_id,
	.probe = bmm_probe,
	.remove = bmm_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = bmm_suspend,
	.resume = bmm_resume,
#endif
};

static int __init BMM_init(void)
{
	return i2c_add_driver(&bmm_driver);
}

static void __exit BMM_exit(void)
{
	i2c_del_driver(&bmm_driver);
}

MODULE_AUTHOR("Zhengguang.Guo <Zhengguang.Guo@bosch-sensortec.com>");
MODULE_DESCRIPTION("driver for " SENSOR_NAME);
MODULE_LICENSE("GPL");

module_init(BMM_init);
module_exit(BMM_exit);
