/*!
 * @section LICENSE
 * (C) Copyright 2014 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bma2x2.c
 * @date    2014/04/04 16:13
 * @id       "564eaab"
 * @version  2.0.1
 *
 * @brief
 * This file contains all function implementations for the BMA2X2 in linux
 */

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <linux/regulator/consumer.h>

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/string.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#endif
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>

#include <linux/sensor/sensors_core.h>
#include "bma2x2_reg.h"

#define CHIP_VENDOR		"BOSCH"
#define CHIP_NAME		"BMA254"
#define SENSOR_NAME		"accelerometer_sensor"
#define CALIBRATION_FILE_PATH	"/efs/FactoryApp/calibration_data"
#define CALIBRATION_DATA_AMOUNT         20
#define MAX_ACCEL_1G			1024
#define MAX_ACCEL_1G_FOR4G		512
#define ACCEL_LOG_TIME                  10/* 10 sec */

struct bma2x2_v {
	union {
		s16 v[3];
		struct {
			s16 x;
			s16 y;
			s16 z;
		};
	};
};

enum {
	OFF = 0,
	ON = 1
};

/*!
 * we use a typedef to hide the detail,
 * because this type might be changed
*/
struct bma2x2_data {
	struct i2c_client *bma2x2_client;
	atomic_t delay;
	atomic_t enable;
	atomic_t selftest_result;
	unsigned int chip_id;
	unsigned int fifo_count;
	unsigned char fifo_datasel;
	unsigned char mode;
	signed char sensor_type;
	struct input_dev *input;

	struct bma2x2_v value;
	struct bma2x2_v caldata;
	struct mutex value_mutex;
	struct mutex enable_mutex;
	struct mutex mode_mutex;
	struct delayed_work work;
	struct work_struct irq_work;
	int IRQ;
	u64 old_timestamp;

	int bma_mode_enabled;
	atomic_t reactive_enable;
	atomic_t reactive_state;
	atomic_t factory_mode;
	int time_count;

	int place;
	int acc_int1;
	int range;
	unsigned char used_bw;
};

static int bma2x2_normal_to_suspend(struct bma2x2_data *bma2x2,
				unsigned char data1, unsigned char data2);

static int bma2x2_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;
	int retries = 0;

	do {
		dummy = i2c_smbus_read_byte_data(client, reg_addr);
		if (dummy >= 0)
			break;
	} while (retries++ < 3);

	if (dummy < 0) {
		SENSOR_ERR("i2c read error %d\n", dummy);
		return dummy;
	}

	*data = dummy & 0x000000ff;

	return 0;
}

static int bma2x2_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;
	int retries = 0;

	do {
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
		if (dummy >= 0)
			break;
	} while (retries++ < 3);

	if (dummy < 0) {
		SENSOR_ERR("i2c write error %d\n", dummy);
		return dummy;
	}

	udelay(2);
	return 0;
}

static int bma2x2_smbus_read_byte_block(struct i2c_client *client,
	unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	s32 dummy;
	int retries = 0;

	do {
		dummy = i2c_smbus_read_i2c_block_data(client, reg_addr, len,
			data);
		if (dummy >= 0)
			break;
	} while (retries++ < 3);

	if (dummy < 0) {
		SENSOR_ERR("i2c read error %d\n", dummy);
		return dummy;
	}

	return 0;
}

static int bma2x2_check_chip_id(struct i2c_client *client,
					struct bma2x2_data *data)
{
	int i = 0;
	int err = 0;
	unsigned char chip_id;
	unsigned char read_count = 0;
	unsigned char bma2x2_sensor_type_count = 0;

	bma2x2_sensor_type_count =
		sizeof(sensor_type_map) / sizeof(struct bma2x2_type_map_t);

	while (read_count++ < CHECK_CHIP_ID_TIME_MAX) {
		err = bma2x2_smbus_read_byte(client, BMA2X2_CHIP_ID_REG,
			&chip_id);
		if (err < 0) {
			dev_err(&client->dev,
				"Bosch Sensortec Device not found, id: %d\n",
				chip_id);
			continue;
		} else {
			for (i = 0; i < bma2x2_sensor_type_count; i++) {
				if (sensor_type_map[i].chip_id == chip_id) {
					data->sensor_type =
						sensor_type_map[i].sensor_type;
					data->chip_id = chip_id;
					dev_notice(&client->dev,
						"Bosch Sensortec Device detected,"
						"HW IC name: %s\n",
						sensor_type_map[i].sensor_name);
					return err;
				}
			}

			if (read_count == CHECK_CHIP_ID_TIME_MAX) {
				dev_err(&client->dev,
					"Failed!Bosch Sensortec Device"
					"not found, mismatch chip_id:%d\n",
					chip_id);
				err = -ENODEV;
				return err;
			}
			mdelay(1);
		}
	}
	return err;
}

#ifdef CONFIG_SENSORS_BMA2X2_ENABLE_INT1
static int bma2x2_set_int1_pad_sel(struct i2c_client *client,
						unsigned char int1sel)
{
	int comres = 0;
	unsigned char data;
	unsigned char state = 0x01;

	SENSOR_INFO("int1sel(%d)\n", int1sel);

	switch (int1sel) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_EN_INT1_PAD_LOWG__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_LOWG,
			state);
		comres += bma2x2_smbus_write_byte(client,
			BMA2X2_EN_INT1_PAD_LOWG__REG, &data);
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_EN_INT1_PAD_HIGHG__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_HIGHG,
			state);
		comres += bma2x2_smbus_write_byte(client,
			BMA2X2_EN_INT1_PAD_HIGHG__REG, &data);
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_EN_INT1_PAD_SLOPE__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_SLOPE,
			state);
		comres += bma2x2_smbus_write_byte(client,
			BMA2X2_EN_INT1_PAD_SLOPE__REG, &data);
		break;
	case 3:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_EN_INT1_PAD_DB_TAP__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_DB_TAP,
			state);
		comres += bma2x2_smbus_write_byte(client,
			BMA2X2_EN_INT1_PAD_DB_TAP__REG, &data);
		break;
	case 4:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_EN_INT1_PAD_SNG_TAP__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_SNG_TAP,
			state);
		comres += bma2x2_smbus_write_byte(client,
			BMA2X2_EN_INT1_PAD_SNG_TAP__REG, &data);
		break;
	case 5:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_EN_INT1_PAD_ORIENT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_ORIENT,
			state);
		comres += bma2x2_smbus_write_byte(client,
			BMA2X2_EN_INT1_PAD_ORIENT__REG, &data);
		break;
	case 6:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_EN_INT1_PAD_FLAT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_FLAT,
			state);
		comres += bma2x2_smbus_write_byte(client,
			BMA2X2_EN_INT1_PAD_FLAT__REG, &data);
		break;
	case 7:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_EN_INT1_PAD_SLO_NO_MOT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_SLO_NO_MOT,
			state);
		comres += bma2x2_smbus_write_byte(client,
			BMA2X2_EN_INT1_PAD_SLO_NO_MOT__REG, &data);
		break;
	default:
		break;
	}

	return comres;
}
#endif /* CONFIG_SENSORS_BMA2X2_ENABLE_INT1 */

static int bma2x2_set_Int_Enable(struct i2c_client *client,
					unsigned char interrupt_type,
					unsigned char value)
{
	int comres;
	unsigned char data1;

	SENSOR_INFO("InterruptType(%d)\n", interrupt_type);

	comres = bma2x2_smbus_read_byte(client, BMA2X2_INT_ENABLE1_REG,
		&data1);

	value = value & 1;
	switch (interrupt_type) {
	case 5:
		/* Slope X Interrupt */
		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_SLOPE_X_INT,
			value);
		break;
	case 6:
		/* Slope Y Interrupt */
		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_SLOPE_Y_INT,
			value);
		break;
	case 7:
		/* Slope Z Interrupt */
		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_SLOPE_Z_INT,
			value);
		break;
	default:
		break;
	}
	comres += bma2x2_smbus_write_byte(client, BMA2X2_INT_ENABLE1_REG,
		&data1);

	return comres;
}


static int bma2x2_get_interruptstatus1(struct i2c_client *client,
						unsigned char *intstatus)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_STATUS1_REG, &data);
	*intstatus = data;

	SENSOR_INFO("ACC_INT_STATUS %x\n", data);

	return comres;
}

static int bma2x2_get_slope_first(struct i2c_client *client, unsigned char
	param, unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data;

	switch (param) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_TAP_SLOPE_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_FIRST_X);
		*intstatus = data;
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_TAP_SLOPE_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_FIRST_Y);
		*intstatus = data;
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_TAP_SLOPE_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_FIRST_Z);
		*intstatus = data;
		break;
	default:
		break;
	}

	return comres;
}

static int bma2x2_get_slope_sign(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_STATUS_TAP_SLOPE_REG,
			&data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_SIGN_S);
	*intstatus = data;

	return comres;
}

static int bma2x2_set_Int_Mode(struct i2c_client *client, unsigned char mode)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
		BMA2X2_INT_MODE_SEL__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_INT_MODE_SEL, mode);
	comres += bma2x2_smbus_write_byte(client,
		BMA2X2_INT_MODE_SEL__REG, &data);

	return comres;
}

static int bma2x2_set_slope_threshold(struct i2c_client *client,
		unsigned char threshold)
{
	int comres;
	unsigned char data;

	data = threshold;
	SENSOR_INFO("data[%x]\n", data);

	comres = bma2x2_smbus_write_byte(client,
		BMA2X2_SLOPE_THRES__REG, &data);

	return comres;
}

static int bma2x2_set_slope_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_SLOPE_DUR__REG, &data);

	SENSOR_INFO("data[%x]\n", data);

	data = BMA2X2_SET_BITSLICE(data, BMA2X2_SLOPE_DUR, duration);
	comres += bma2x2_smbus_write_byte(client,
			BMA2X2_SLOPE_DUR__REG, &data);

	return comres;
}

/*!
 * brief: bma2x2 switch from normal to suspend mode
 * @param[i] bma2x2
 * @param[i] data1, write to PMU_LPW
 * @param[i] data2, write to PMU_LOW_NOSIE
 *
 * @return zero success, none-zero failed
 */
static int bma2x2_normal_to_suspend(struct bma2x2_data *bma2x2,
				unsigned char data1, unsigned char data2)
{
	unsigned char current_fifo_mode;
	unsigned char current_op_mode = 0;
	if (bma2x2 == NULL)
		return -1;
	/* get current op mode from mode register */
	if (bma2x2_get_mode(bma2x2->bma2x2_client, &current_op_mode) < 0)
		return -1;
	/* only aimed at operatiom mode chang from normal/lpw1 mode
	 * to suspend state.
	*/
	if (current_op_mode == BMA2X2_MODE_NORMAL ||
			current_op_mode == BMA2X2_MODE_LOWPOWER1) {
		/* get current fifo mode from fifo config register */
		if (bma2x2_get_fifo_mode(bma2x2->bma2x2_client,
							&current_fifo_mode) < 0)
			return -1;
		else {
			bma2x2_smbus_write_byte(bma2x2->bma2x2_client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
			bma2x2_smbus_write_byte(bma2x2->bma2x2_client,
					BMA2X2_MODE_CTRL_REG, &data1);
			bma2x2_smbus_write_byte(bma2x2->bma2x2_client,
				BMA2X2_FIFO_MODE__REG, &current_fifo_mode);
			mdelay(3);
			return 0;
		}
	} else {
		bma2x2_smbus_write_byte(bma2x2->bma2x2_client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
		bma2x2_smbus_write_byte(bma2x2->bma2x2_client,
					BMA2X2_MODE_CTRL_REG, &data1);
		mdelay(3);
		return 0;
	}

}

static int bma2x2_open_calibration(struct bma2x2_data *data)
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY, 0);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(cal_filp);

		data->caldata.x = 0;
		data->caldata.y = 0;
		data->caldata.z = 0;

		SENSOR_ERR("No Calibration\n");
		return ret;
	}

	ret = cal_filp->f_op->read(cal_filp, (char *)&data->caldata.v,
		3 * sizeof(s16), &cal_filp->f_pos);
	if (ret != 3 * sizeof(s16)) {
		SENSOR_ERR("Can't read the cal data\n");
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	SENSOR_INFO("open accel calibration %d, %d, %d\n",
		data->caldata.x, data->caldata.y, data->caldata.z);

	if ((data->caldata.x == 0) && (data->caldata.y == 0)
		&& (data->caldata.z == 0))
		return -EIO;

	return ret;
}

static int bma2x2_set_range(struct i2c_client *client, unsigned char range)
{
	int comres;
	unsigned char data1;

	if ((range == 3) || (range == 5) || (range == 8) || (range == 12)) {
		comres = bma2x2_smbus_read_byte(client, BMA2X2_RANGE_SEL_REG,
				&data1);
		switch (range) {
		case BMA2X2_RANGE_2G:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_RANGE_SEL, 3);
			break;
		case BMA2X2_RANGE_4G:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_RANGE_SEL, 5);
			break;
		case BMA2X2_RANGE_8G:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_RANGE_SEL, 8);
			break;
		case BMA2X2_RANGE_16G:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_RANGE_SEL, 12);
			break;
		default:
			break;
		}
		comres += bma2x2_smbus_write_byte(client, BMA2X2_RANGE_SEL_REG,
				&data1);
	} else {
		comres = -1;
	}

	return comres;
}

static int bma2x2_set_mode(struct i2c_client *client, unsigned char mode,
						unsigned char enabled_mode)
{
	int comres;
	unsigned char data1 = 0, data2 = 0;
	int ret = 0;
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	SENSOR_INFO("mode[%d]\n", mode);

	mutex_lock(&bma2x2->mode_mutex);
	if (BMA2X2_MODE_SUSPEND == mode) {
		if (enabled_mode != BMA_ENABLED_ALL) {
			if ((bma2x2->bma_mode_enabled &
						(1<<enabled_mode)) == 0) {
				/* sensor is already closed in this mode */
				mutex_unlock(&bma2x2->mode_mutex);
				return 0;
			} else {
				bma2x2->bma_mode_enabled &= ~(1<<enabled_mode);
			}
		} else {
			/* shut down, close all and force do it*/
			bma2x2->bma_mode_enabled = 0;
		}
	} else {
		if ((bma2x2->bma_mode_enabled & (1<<enabled_mode)) == 1) {
			/* sensor is already enabled in this mode */
			mutex_unlock(&bma2x2->mode_mutex);
			return 0;
		} else {
			bma2x2->bma_mode_enabled |= (1<<enabled_mode);
		}
	}
	mutex_unlock(&bma2x2->mode_mutex);

	if (mode < 6) {
		comres = bma2x2_smbus_read_byte(client, BMA2X2_MODE_CTRL_REG,
				&data1);
		comres += bma2x2_smbus_read_byte(client,
				BMA2X2_LOW_NOISE_CTRL_REG,
				&data2);
		if (comres < 0)
			SENSOR_ERR("i2c read error\n");

		switch (mode) {
		case BMA2X2_MODE_NORMAL:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_MODE_CTRL, 0);
			data2  = BMA2X2_SET_BITSLICE(data2,
					BMA2X2_LOW_POWER_MODE, 0);
			comres = bma2x2_smbus_write_byte(client,
					BMA2X2_MODE_CTRL_REG, &data1);
			usleep_range(5000, 5100);
			comres += bma2x2_smbus_write_byte(client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
			if (comres < 0)
				SENSOR_ERR("i2c write error\n");

			ret = bma2x2_set_range(client, BMA2X2_RANGE_SET);
			if (ret < 0)
				SENSOR_ERR("Error range setting\n");
			break;
		case BMA2X2_MODE_LOWPOWER1:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_MODE_CTRL, 2);
			data2  = BMA2X2_SET_BITSLICE(data2,
					BMA2X2_LOW_POWER_MODE, 0);
			comres += bma2x2_smbus_write_byte(client,
					BMA2X2_MODE_CTRL_REG, &data1);
			usleep_range(5000, 5100);
			comres += bma2x2_smbus_write_byte(client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
				break;
		case BMA2X2_MODE_SUSPEND:
			data1  = BMA2X2_SET_BITSLICE(data1,
						BMA2X2_MODE_CTRL, 4);
			data2  = BMA2X2_SET_BITSLICE(data2,
						BMA2X2_LOW_POWER_MODE, 0);
			/*aimed at anomaly resolution when switch to suspend*/
			ret = bma2x2_normal_to_suspend(bma2x2, data1, data2);
			if (ret < 0)
				SENSOR_ERR("Error switching to suspend\n");
				break;
		}
	} else {
		comres = -1;
		SENSOR_ERR("Error mode control\n");

	}

	SENSOR_INFO("comres[%d]\n", comres);
	return comres;
}


static int bma2x2_get_mode(struct i2c_client *client, unsigned char *mode)
{
	int comres;
	unsigned char data1, data2;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_MODE_CTRL_REG, &data1);
	comres += bma2x2_smbus_read_byte(client, BMA2X2_LOW_NOISE_CTRL_REG,
			&data2);

	data1  = (data1 & 0xE0) >> 5;
	data2  = (data2 & 0x40) >> 6;

	if ((data1 == 0x00) && (data2 == 0x00))
		*mode  = BMA2X2_MODE_NORMAL;
	else {
		if ((data1 == 0x02) && (data2 == 0x00))
			*mode  = BMA2X2_MODE_LOWPOWER1;
		else {
			if ((data1 == 0x04 || data1 == 0x06) &&
						(data2 == 0x00))
				*mode  = BMA2X2_MODE_SUSPEND;
		}
	}

	return comres;
}

static int bma2x2_set_bandwidth(struct i2c_client *client, unsigned char bw)
{
	int comres = 0;
	int bandwidth = 0;
	unsigned char data;
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (atomic_read(&bma2x2->factory_mode)) {
		SENSOR_INFO("pass... factory_mode!\n");
		return comres;
	}

	if (bw > 7 && bw < 16) {
		switch (bw) {
		case BMA2X2_BW_7_81HZ:
			bandwidth = BMA2X2_BW_7_81HZ;
			/*  7.81 Hz      64000 uS   */
			break;
		case BMA2X2_BW_15_63HZ:
			bandwidth = BMA2X2_BW_15_63HZ;
			/*  15.63 Hz     32000 uS   */
			break;
		case BMA2X2_BW_31_25HZ:
			bandwidth = BMA2X2_BW_31_25HZ;
			/*  31.25 Hz     16000 uS   */
			break;
		case BMA2X2_BW_62_50HZ:
			bandwidth = BMA2X2_BW_62_50HZ;
			/*  62.50 Hz     8000 uS   */
			break;
		case BMA2X2_BW_125HZ:
			bandwidth = BMA2X2_BW_125HZ;
			/*  125 Hz       4000 uS   */
			break;
		case BMA2X2_BW_250HZ:
			bandwidth = BMA2X2_BW_250HZ;
			/*  250 Hz       2000 uS   */
			break;
		case BMA2X2_BW_500HZ:
			bandwidth = BMA2X2_BW_500HZ;
			/*  500 Hz       1000 uS   */
			break;
		case BMA2X2_BW_1000HZ:
			bandwidth = BMA2X2_BW_1000HZ;
			/*  1000 Hz      500 uS   */
			break;
		default:
			break;
		}
		comres = bma2x2_smbus_read_byte(client, BMA2X2_BANDWIDTH__REG,
							&data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_BANDWIDTH, bandwidth);
		comres += bma2x2_smbus_write_byte(client, BMA2X2_BANDWIDTH__REG,
							&data);
		if (comres < 0)
			SENSOR_ERR("i2c bandwidth error\n");
	} else {
		comres = -1;
	}

	SENSOR_INFO("[%d]\n", bandwidth);

	return comres;
}

static int bma2x2_get_bandwidth(struct i2c_client *client, unsigned char *bw)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_BANDWIDTH__REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_BANDWIDTH);
	*bw = data;

	return comres;
}

static int bma2x2_get_fifo_mode(struct i2c_client *client, unsigned char
		*fifo_mode)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_FIFO_MODE__REG, &data);
	*fifo_mode = BMA2X2_GET_BITSLICE(data, BMA2X2_FIFO_MODE);

	return comres;
}

static int bma2x2_soft_reset(struct i2c_client *client)
{
	int comres;
	unsigned char data = BMA2X2_EN_SOFT_RESET_VALUE;

	comres = bma2x2_smbus_write_byte(client, BMA2X2_EN_SOFT_RESET__REG,
					&data);

	return comres;
}

const int bma2x2_sensor_bitwidth[] = {
	12,  10,  8, 14
};

static int bma2x2_read_accel_xyz(struct i2c_client *client,
		signed char sensor_type, struct bma2x2_v *acc)
{
	int comres;
	unsigned char data[6];
	struct bma2x2_data *client_data = i2c_get_clientdata(client);
	int bitwidth;

	comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_X12_LSB__REG, data, 6);
	if (comres < 0)
		SENSOR_ERR("i2c read error\n");

	if (sensor_type >= 4)
		return -EINVAL;

	acc->x = (data[1]<<8)|data[0];
	acc->y = (data[3]<<8)|data[2];
	acc->z = (data[5]<<8)|data[4];

	bitwidth = bma2x2_sensor_bitwidth[sensor_type];

	acc->x = (acc->x >> (16 - bitwidth));
	acc->y = (acc->y >> (16 - bitwidth));
	acc->z = (acc->z >> (16 - bitwidth));

	remap_sensor_data(acc->v, client_data->place);
	return comres;
}

static void bma2x2_work_func(struct work_struct *work)
{
	struct bma2x2_data *bma2x2 = container_of((struct delayed_work *)work,
						struct bma2x2_data, work);
	static struct bma2x2_v acc;
	unsigned long delay = msecs_to_jiffies(atomic_read(&bma2x2->delay));
	struct timespec ts = ktime_to_timespec(ktime_get_boottime());
	u64 timestamp_new = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
	u64 timestamp;
	int time_hi, time_lo;
	int ret;

	ret = bma2x2_read_accel_xyz(bma2x2->bma2x2_client, bma2x2->sensor_type,
		&acc);
	if (ret < 0) {
		SENSOR_ERR("i2c read error\n");
		goto exit;
	}
	bma2x2->value.x = acc.x - bma2x2->caldata.x;
	bma2x2->value.y = acc.y - bma2x2->caldata.y;
	bma2x2->value.z = acc.z - bma2x2->caldata.z;

	if (((timestamp_new - bma2x2->old_timestamp)
			> atomic_read(&bma2x2->delay) * 1800000LL)
			&& (bma2x2->old_timestamp != 0)) {
		timestamp = (timestamp_new + bma2x2->old_timestamp) >> 1;
		time_hi = (int)((timestamp & TIME_HI_MASK) >> TIME_HI_SHIFT);
		time_lo = (int)(timestamp & TIME_LO_MASK);

		input_report_rel(bma2x2->input, REL_X, acc.x);
		input_report_rel(bma2x2->input, REL_Y, acc.y);
		input_report_rel(bma2x2->input, REL_Z, acc.z);
		input_report_rel(bma2x2->input, REL_DIAL, time_hi);
		input_report_rel(bma2x2->input, REL_MISC, time_lo);
		input_sync(bma2x2->input);
	}
	time_hi = (int)((timestamp_new & TIME_HI_MASK) >> TIME_HI_SHIFT);
	time_lo = (int)(timestamp_new & TIME_LO_MASK);

	input_report_rel(bma2x2->input, REL_X, acc.x);
	input_report_rel(bma2x2->input, REL_Y, acc.y);
	input_report_rel(bma2x2->input, REL_Z, acc.z);
	input_report_rel(bma2x2->input, REL_DIAL, time_hi);
	input_report_rel(bma2x2->input, REL_MISC, time_lo);
	input_sync(bma2x2->input);

	bma2x2->old_timestamp = timestamp_new;

exit:
	if ((atomic_read(&bma2x2->delay) * bma2x2->time_count)
			>= (ACCEL_LOG_TIME * MSEC_PER_SEC)) {
		SENSOR_INFO("x = %d, y = %d, z = %d\n",
			bma2x2->value.x, bma2x2->value.y, bma2x2->value.z);
		bma2x2->time_count = 0;
	} else {
		bma2x2->time_count++;
	}

	schedule_delayed_work(&bma2x2->work, delay);
}

static ssize_t bma2x2_raw_data_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma2x2_data *bma2x2 = input_get_drvdata(input);
	struct bma2x2_v acc_value;

	if (atomic_read(&bma2x2->enable) == OFF) {
		bma2x2_set_mode(bma2x2->bma2x2_client,
			BMA2X2_MODE_NORMAL, BMA_ENABLED_INPUT);
		msleep(20);
		bma2x2_read_accel_xyz(bma2x2->bma2x2_client,
				bma2x2->sensor_type, &acc_value);

		bma2x2_set_mode(bma2x2->bma2x2_client,
			BMA2X2_MODE_SUSPEND, BMA_ENABLED_INPUT);

		acc_value.x = acc_value.x - bma2x2->caldata.x;
		acc_value.y = acc_value.y - bma2x2->caldata.y;
		acc_value.z = acc_value.z - bma2x2->caldata.z;
	} else {
		acc_value = bma2x2->value;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		acc_value.x, acc_value.y, acc_value.z);
}

static ssize_t bma2x2_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&bma2x2->delay));

}

static ssize_t bma2x2_delay_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long data;
	int error;
	int bw = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	data = data / 1000000L;

	SENSOR_INFO("[%d]\n", (int)data);

	if (data > BMA2X2_MAX_DELAY)
		data = BMA2X2_MAX_DELAY;
	else if (data < BMA2X2_MIN_DELAY)
		data = BMA2X2_MIN_DELAY;
	atomic_set(&bma2x2->delay, (unsigned int)data);

	/*set bandwidth */
	switch (data) {
	case 0:
	case 1:
	case 5:
	case 10:
		bw = 0x0b; /*100Hz*/
		break;
	case 20:
		bw = 0x0a; /*50hz;*/
		break;
	case 50:
	case 66:
		bw = 0x09; /*20Hz;*/
		break;
	case 200:
		bw = 0x08; /*5Hz;*/
		break;
	default:
		break;
	}

	if (bw >= 0x08) {
		if (bma2x2_set_bandwidth(bma2x2->bma2x2_client,
				(unsigned char) bw) < 0) {
			SENSOR_ERR("failed to set bandwidth\n");
			return -EINVAL;
		}
	}

	return count;
}


static ssize_t bma2x2_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&bma2x2->enable));

}

static void bma2x2_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	int pre_enable = atomic_read(&bma2x2->enable);

	SENSOR_INFO("enable[%d], pre_enable[%d]\n", enable, pre_enable);

	mutex_lock(&bma2x2->enable_mutex);
	if (enable) {
		if (pre_enable == 0) {
			bma2x2->old_timestamp = 0LL;
			bma2x2_set_mode(bma2x2->bma2x2_client,
				BMA2X2_MODE_NORMAL, BMA_ENABLED_INPUT);
			/* Access to calibration data in filesystem
			 * only when there are no caldata */
			if (bma2x2->caldata.x == 0 && bma2x2->caldata.y == 0
					&& bma2x2->caldata.z == 0)
				bma2x2_open_calibration(bma2x2);
			schedule_delayed_work(&bma2x2->work,
				msecs_to_jiffies(atomic_read(&bma2x2->delay)));
			atomic_set(&bma2x2->enable, 1);
			SENSOR_INFO("enable[%d]\n",
				atomic_read(&bma2x2->enable));
		}
	} else {
		if (pre_enable == 1) {
			if (atomic_read(&bma2x2->reactive_enable) == 0)
				bma2x2_set_mode(bma2x2->bma2x2_client,
					BMA2X2_MODE_SUSPEND, BMA_ENABLED_INPUT);
			cancel_delayed_work_sync(&bma2x2->work);
			atomic_set(&bma2x2->enable, 0);
		}
	}
	mutex_unlock(&bma2x2->enable_mutex);

}

static ssize_t bma2x2_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long data = 0;
	int error;

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	SENSOR_INFO("[%lu]\n", data);

	if ((data == 0) || (data == 1))
		bma2x2_set_enable(dev, data);

	return count;
}

static ssize_t bma2x2_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	int ret;

	ret = bma2x2_open_calibration(bma2x2);
	if (ret < 0)
		SENSOR_ERR("calibration open failed(%d)\n", ret);

	SENSOR_INFO("cal data %d %d %d, ret(%d)\n",
		bma2x2->caldata.x, bma2x2->caldata.y, bma2x2->caldata.z,
		ret);

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n", ret, bma2x2->caldata.x,
			bma2x2->caldata.y, bma2x2->caldata.z);
}

static int bma2x2_do_calibrate(struct bma2x2_data *data, int enable)
{
	int sum[3] = {0, };
	int ret = 0, cnt;
	struct file *cal_filp = NULL;
	struct bma2x2_v acc;
	mm_segment_t old_fs;

	if (enable) {
		data->caldata.x = 0;
		data->caldata.y = 0;
		data->caldata.z = 0;

		for (cnt = 0; cnt < CALIBRATION_DATA_AMOUNT; cnt++) {
			bma2x2_read_accel_xyz(data->bma2x2_client,
						data->sensor_type, &acc);
			sum[0] += acc.x;
			sum[1] += acc.y;
			sum[2] += acc.z;
			mdelay(10);
		}

		data->caldata.x = (sum[0] / CALIBRATION_DATA_AMOUNT);
		data->caldata.y = (sum[1] / CALIBRATION_DATA_AMOUNT);
		data->caldata.z = (sum[2] / CALIBRATION_DATA_AMOUNT);

		if (data->range == BMA2X2_RANGE_4G) {
			if (data->caldata.z > 0)
				data->caldata.z -= MAX_ACCEL_1G_FOR4G;
			else if (data->caldata.z < 0)
				data->caldata.z += MAX_ACCEL_1G_FOR4G;
		} else {
			if (data->caldata.z > 0)
				data->caldata.z -= MAX_ACCEL_1G;
			else if (data->caldata.z < 0)
				data->caldata.z += MAX_ACCEL_1G;
		}
	} else {
		data->caldata.x = 0;
		data->caldata.y = 0;
		data->caldata.z = 0;
	}

	SENSOR_INFO("do accel calibrate %d, %d, %d\n",
		data->caldata.x, data->caldata.y, data->caldata.z);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY, 0660);
	if (IS_ERR(cal_filp)) {
		SENSOR_ERR("Can't open calibration file\n");
			set_fs(old_fs);
		ret = PTR_ERR(cal_filp);
		return ret;
	}

	ret = cal_filp->f_op->write(cal_filp, (char *)&data->caldata.v,
		3 * sizeof(s16), &cal_filp->f_pos);
	if (ret != 3 * sizeof(s16)) {
		SENSOR_ERR("Can't write the caldata to file\n");
		ret = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

static ssize_t bma2x2_calibration_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	int64_t d_enable;
	int err;

	err = kstrtoll(buf, 10, &d_enable);
	if (err)
		return err;

	err = bma2x2_do_calibrate(bma2x2, (int)d_enable);
	if (err < 0)
		SENSOR_ERR("accel calibrate failed\n");

	return count;
}

static ssize_t bma2x2_reactive_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		atomic_read(&bma2x2->reactive_state));
}

static ssize_t bma2x2_reactive_enable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	bool onoff = false;
	unsigned long value = 0;
	unsigned char data;

	if (kstrtoul(buf, 10, &value))
		return -EINVAL;

	SENSOR_INFO("value[%lu]\n", value);

	switch (value) {
	case 0:
		bma2x2_set_Int_Enable(bma2x2->bma2x2_client, 7, 0);
		if (atomic_read(&bma2x2->factory_mode))
			atomic_set(&bma2x2->factory_mode, false);
		if (!atomic_read(&bma2x2->enable))
			bma2x2_set_mode(bma2x2->bma2x2_client,
				BMA2X2_MODE_SUSPEND, BMA_ENABLED_INPUT);
		break;
	case 1:
		onoff = true;
		data = 0x01;
		bma2x2_set_slope_duration(bma2x2->bma2x2_client, data);
		usleep_range(3000, 3100);
		data = 0x05;
		bma2x2_set_slope_threshold(bma2x2->bma2x2_client, data);
		usleep_range(3000, 3100);
		bma2x2_set_mode(bma2x2->bma2x2_client,
			BMA2X2_MODE_LOWPOWER1, BMA_ENABLED_INPUT);
		usleep_range(3000, 3100);
		bma2x2_set_Int_Enable(bma2x2->bma2x2_client, 7, 1);
		break;
	case 2:
		onoff = true;
		bma2x2_set_mode(bma2x2->bma2x2_client,
			BMA2X2_MODE_NORMAL, BMA_ENABLED_INPUT);
		usleep_range(3000, 3100);
		bma2x2_set_bandwidth(bma2x2->bma2x2_client, BMA2X2_BW_1000HZ);
		data = 0x00;
		bma2x2_set_slope_duration(bma2x2->bma2x2_client, data);
		usleep_range(3000, 3100);
		data = 0x00;
		bma2x2_set_slope_threshold(bma2x2->bma2x2_client, data);
		usleep_range(3000, 3100);
		bma2x2_set_Int_Enable(bma2x2->bma2x2_client, 7, 1);
		atomic_set(&bma2x2->factory_mode, true);
		break;
	default:
		SENSOR_ERR("invalid value %d\n", *buf);
		return -EINVAL;
	}

	if (bma2x2->IRQ) {
		if (!value) {
			disable_irq_wake(bma2x2->IRQ);
			disable_irq(bma2x2->IRQ);
		} else {
			enable_irq(bma2x2->IRQ);
			enable_irq_wake(bma2x2->IRQ);
		}
	}

	atomic_set(&bma2x2->reactive_enable, onoff);
	atomic_set(&bma2x2->reactive_state, false);
	usleep_range(1000, 1100);

	SENSOR_INFO("onoff[%d]\n", atomic_read(&bma2x2->reactive_enable));

	return count;
}

static ssize_t bma2x2_lowpassfilter_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned char data;
	struct bma2x2_data *bma2x2 = dev_get_drvdata(dev);

	ret = bma2x2_get_bandwidth(bma2x2->bma2x2_client, &data);
	if (ret < 0)
		SENSOR_ERR("Read error [%d]\n", ret);

	if (data == BMA2X2_BW_1000HZ)
		ret = 0;
	else
		ret = 1;

	SENSOR_INFO("%d\n", data);

	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t bma2x2_lowpassfilter_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned char data;
	int64_t d_enable;
	struct bma2x2_data *bma2x2 = dev_get_drvdata(dev);

	ret = kstrtoll(buf, 10, &d_enable);
	if (ret < 0)
		SENSOR_ERR("kstrtoll failed\n");

	ret = bma2x2_get_bandwidth(bma2x2->bma2x2_client, &data);
	if (ret < 0)
		SENSOR_ERR("Read error [%d]\n", ret);

	SENSOR_INFO("data[%d] used_bw[%d]\n", data, bma2x2->used_bw);

	if (d_enable) {
		if (bma2x2->used_bw)
			data = bma2x2->used_bw;
		bma2x2->used_bw = 0;
	} else {
		if (!bma2x2->used_bw)
			bma2x2->used_bw = data;
		data = BMA2X2_BW_1000HZ;
	}
	SENSOR_INFO("data[%d] used_bw[%d]\n", data, bma2x2->used_bw);

	ret = bma2x2_set_bandwidth(bma2x2->bma2x2_client, data);
	if (ret < 0)
		SENSOR_ERR("bma2x2_set_bandwidth failed\n");

	return count;
}

static ssize_t bma2x2_read_name(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_NAME);
}

static ssize_t bma2x2_read_vendor(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_VENDOR);
}

static DEVICE_ATTR(raw_data, S_IRUGO,
		bma2x2_raw_data_read, NULL);
static DEVICE_ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_delay_show, bma2x2_delay_store);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_delay_show, bma2x2_delay_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_enable_show, bma2x2_enable_store);
static DEVICE_ATTR(calibration, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_calibration_show,
		bma2x2_calibration_store);
static DEVICE_ATTR(reactive_alert, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_reactive_enable_show, bma2x2_reactive_enable_store);
static DEVICE_ATTR(lowpassfilter, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_lowpassfilter_show, bma2x2_lowpassfilter_store);
static DEVICE_ATTR(name, S_IRUGO, bma2x2_read_name, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, bma2x2_read_vendor, NULL);

static struct attribute *bma2x2_attributes[] = {
	&dev_attr_raw_data.attr,
	&dev_attr_poll_delay.attr,
	&dev_attr_enable.attr,
	NULL
};
static struct device_attribute *bma2x2_attributes_sensors[] = {
	&dev_attr_raw_data,
	&dev_attr_delay,
	&dev_attr_enable,
	&dev_attr_calibration,
	&dev_attr_reactive_alert,
	&dev_attr_lowpassfilter,
	&dev_attr_name,
	&dev_attr_vendor,
	NULL
};

static struct attribute_group bma2x2_attribute_group = {
	.attrs = bma2x2_attributes
};

static void bma2x2_slope_interrupt_handle(struct bma2x2_data *bma2x2)
{
	unsigned char first_value = 0;
	unsigned char sign_value = 0;
	int i;

	for (i = 0; i < 3; i++) {
		bma2x2_get_slope_first(bma2x2->bma2x2_client, i, &first_value);
		if (first_value == 1) {
			bma2x2_get_slope_sign(bma2x2->bma2x2_client,
				&sign_value);

			SENSOR_INFO("exis is %d, first is %d, sign is %d\n",
				i, first_value, sign_value);
		}
	}
}

static void bma2x2_irq_work_func(struct work_struct *work)
{
	struct bma2x2_data *bma2x2 = container_of((struct work_struct *)work,
					struct bma2x2_data, irq_work);
	unsigned char status = 0;

	bma2x2_get_interruptstatus1(bma2x2->bma2x2_client, &status);
	SENSOR_INFO("status [0x%x]\n", status);

	switch (status) {
	case 0x04:
		bma2x2_slope_interrupt_handle(bma2x2);
		break;
	default:
		break;
	}

	atomic_set(&bma2x2->reactive_state, true);
}

static irqreturn_t bma2x2_irq_handler(int irq, void *handle)
{
	struct bma2x2_data *data = handle;

	if (data == NULL)
		return IRQ_HANDLED;
	if (data->bma2x2_client == NULL)
		return IRQ_HANDLED;

	schedule_work(&data->irq_work);

	return IRQ_HANDLED;
}

#if defined(CONFIG_SENSORS_BMA2X2_REGULATOR)
static int bma2x2_acc_power_onoff(struct bma2x2_data *data, bool onoff)
{
	int ret = 0;
#ifdef CONFIG_SENSORS_BMC150_VDD
	struct regulator *reg_vdd;
#endif
	struct regulator *reg_vio;

	SENSOR_INFO("%d\n", onoff);
#ifdef CONFIG_SENSORS_BMC150_VDD
	reg_vdd = devm_regulator_get(&data->bma2x2_client->dev,
		"bma2x2,vdd");
	if (IS_ERR(reg_vdd)) {
		SENSOR_ERR("could not get vdd[%ld]\n", PTR_ERR(reg_vdd));
		ret = -ENOMEM;
		goto err_vdd;
	} else if (!regulator_get_voltage(reg_vdd)) {
		ret = regulator_set_voltage(reg_vdd, 2850000, 2850000);
	}
#endif
	reg_vio = devm_regulator_get(&data->bma2x2_client->dev, "vdd18");
	if (IS_ERR(reg_vio)) {
		SENSOR_ERR("could not get vio[%ld]\n", PTR_ERR(reg_vio));
		ret = -ENOMEM;
		goto err_vio;
	} else if (!regulator_get_voltage(reg_vio)) {
		ret = regulator_set_voltage(reg_vio, 1800000, 1800000);
	}

	if (onoff) {
#ifdef CONFIG_SENSORS_BMC150_VDD
		ret = regulator_enable(reg_vdd);
		if (ret)
			SENSOR_ERR("Failed to enable vdd\n");
#endif
		ret = regulator_enable(reg_vio);
		if (ret)
			SENSOR_ERR("Failed to enable vio\n");
		msleep(30);
	} else {
#ifdef CONFIG_SENSORS_BMC150_VDD
		ret = regulator_disable(reg_vdd);
		if (ret)
			SENSOR_ERR("Failed to disable vdd\n");
#endif
		ret = regulator_disable(reg_vio);
		if (ret)
			SENSOR_ERR("Failed to disable vio\n");
	}
	SENSOR_INFO("success [%d]\n", onoff);

	devm_regulator_put(reg_vio);
err_vio:
#ifdef CONFIG_SENSORS_BMC150_VDD
	devm_regulator_put(reg_vdd);
err_vdd:
#endif
	return ret;
}
#endif

static int bma2x2_parse_dt(struct bma2x2_data *data, struct device *dev)
{

	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int ret = 0;

	if (np == NULL) {
		SENSOR_ERR("no dev_node\n");
		return -ENODEV;
	}

	data->acc_int1 = of_get_named_gpio_flags(np, "bma2x2,gpio_int1", 0,
		&flags);
	if (data->acc_int1 < 0) {
		SENSOR_ERR("get gpio_int error\n");
		return -ENODEV;
	}
	ret = of_property_read_u32(np, "bma2x2,accel_place", &data->place);
	if (unlikely(ret)) {
		dev_err(dev, "error reading property acc_place from device node %d\n",
			data->place);
		goto error;
	}
	SENSOR_INFO("place[%d] acc_int[%d]\n", data->place, data->acc_int1);

	return 0;
error:
	return ret;
}

static int bma2x2_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;
	struct bma2x2_data *data;
	struct input_dev *dev;

	SENSOR_INFO("start\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		SENSOR_ERR("i2c_check_functionality error\n");
		err = -EIO;
		goto exit;
	}
	data = kzalloc(sizeof(struct bma2x2_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, data);
	data->bma2x2_client = client;
	err = bma2x2_parse_dt(data, &client->dev);
	if (err < 0) {
		SENSOR_ERR("Error getting platform data from device node\n");
		goto kfree_exit;
	}

	/* do soft reset */
	usleep_range(2900, 3000);
	if (bma2x2_soft_reset(client) < 0) {
		dev_err(&client->dev,
			"i2c bus write error, pls check HW connection\n");
		err = -EINVAL;
		goto kfree_exit;
	}
	usleep_range(4900, 5000);

	/* read and check chip id */
	if (bma2x2_check_chip_id(client, data) < 0) {
		err = -EINVAL;
		goto kfree_exit;
	}

	mutex_init(&data->value_mutex);
	mutex_init(&data->mode_mutex);
	mutex_init(&data->enable_mutex);
	bma2x2_set_bandwidth(client, BMA2X2_BW_SET);
	bma2x2_set_range(client, BMA2X2_RANGE_SET);
	data->range = BMA2X2_RANGE_SET;

#ifdef CONFIG_SENSORS_BMA2X2_ENABLE_INT1
	/* maps interrupt to INT1 pin */
	bma2x2_set_int1_pad_sel(client, PAD_SLOP);
#endif

	bma2x2_set_Int_Mode(client, 1);/*latch interrupt 250ms*/

	/* do not open any interrupt here  */
	/*10,orient  11,flat*/
	/*bma2x2_set_Int_Enable(client, 10, 1); */
	/*bma2x2_set_Int_Enable(client, 11, 1); */

	err = gpio_request(data->acc_int1, "bma2x2");
	if (err < 0) {
		SENSOR_ERR("request gpio [%d] fail\n", data->acc_int1);
		goto kfree_exit;
	}

	err = gpio_direction_input(data->acc_int1);
	if (err < 0) {
		SENSOR_ERR("failed to set gpio %d as input (%d)\n",
			data->acc_int1, err);
		goto err_gpio_direction_input;
	}
	data->IRQ = gpio_to_irq(data->acc_int1);
	err = request_irq(data->IRQ, bma2x2_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
			"bma2x2", data);
	if (err)
		SENSOR_ERR("could not request irq\n");

	disable_irq(data->IRQ);

	INIT_WORK(&data->irq_work, bma2x2_irq_work_func);

	INIT_DELAYED_WORK(&data->work, bma2x2_work_func);
	atomic_set(&data->delay, BMA2X2_MAX_DELAY);
	atomic_set(&data->enable, 0);

	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	/* only value events reported */
	dev->name = SENSOR_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, REL_X);
	input_set_capability(dev, EV_REL, REL_Y);
	input_set_capability(dev, EV_REL, REL_Z);
	input_set_capability(dev, EV_REL, REL_DIAL);
	input_set_capability(dev, EV_REL, REL_MISC);

	input_set_drvdata(dev, data);
	err = input_register_device(dev);
	if (err < 0)
		goto err_register_input_device;
	data->input = dev;

	err = sensors_create_symlink(&data->input->dev.kobj, data->input->name);
	if (err < 0) {
		SENSOR_ERR("failed sensors_create_symlink\n");
		goto error_sysfs;
	}

	err = sysfs_create_group(&data->input->dev.kobj,
			&bma2x2_attribute_group);
	if (err < 0)
		goto error_sysfs;

	data->bma_mode_enabled = 0;
	data->time_count = 0;

	err = sensors_register(bma2x2_device, data,
		bma2x2_attributes_sensors, "accelerometer_sensor");

	if (unlikely(err < 0)) {
		SENSOR_ERR("could not sensors_register\n");
		goto error_sensors_register;
	}

	SENSOR_INFO("success\n");

	return 0;

error_sensors_register:
	sysfs_remove_group(&data->input->dev.kobj,
		&bma2x2_attribute_group);

error_sysfs:
	input_unregister_device(data->input);
	sensors_remove_symlink(&data->input->dev.kobj, data->input->name);
	input_unregister_device(data->input);

err_register_input_device:
	input_free_device(dev);
err_gpio_direction_input:
	gpio_free(data->acc_int1);
kfree_exit:
	kfree(data);
exit:
	return err;
}


static int bma2x2_remove(struct i2c_client *client)
{
	struct bma2x2_data *data = i2c_get_clientdata(client);

	bma2x2_set_enable(&client->dev, 0);
	sysfs_remove_group(&data->input->dev.kobj, &bma2x2_attribute_group);
	input_unregister_device(data->input);

	kfree(data);

	return 0;
}

static int bma2x2_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *data = i2c_get_clientdata(client);

	SENSOR_INFO("reactive_enable[%d] reactive_state[%d]\n",
		atomic_read(&data->reactive_enable),
		atomic_read(&data->reactive_state));

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		if (!atomic_read(&data->reactive_enable))
			bma2x2_set_mode(data->bma2x2_client,
				BMA2X2_MODE_SUSPEND, BMA_ENABLED_INPUT);
		cancel_delayed_work_sync(&data->work);
	}
	mutex_unlock(&data->enable_mutex);

	return 0;
}

static int bma2x2_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		bma2x2_set_mode(data->bma2x2_client,
			BMA2X2_MODE_NORMAL, BMA_ENABLED_INPUT);
		schedule_delayed_work(&data->work,
			msecs_to_jiffies(atomic_read(&data->delay)));
	}
	mutex_unlock(&data->enable_mutex);
	SENSOR_INFO("\n");
	return 0;
}

static const struct of_device_id bma2x2_dt_ids[] = {
	{ .compatible = "bma2x2", },
	{},
};

static const struct i2c_device_id bma2x2_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, bma2x2_id);

static const struct dev_pm_ops bma_pm_ops = {
	.suspend = bma2x2_suspend,
	.resume = bma2x2_resume
};

static struct i2c_driver bma2x2_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= SENSOR_NAME,
		.pm	= &bma_pm_ops,
		.of_match_table = bma2x2_dt_ids
	},
	.id_table	= bma2x2_id,
	.probe		= bma2x2_probe,
	.remove		= bma2x2_remove,
};

static int __init BMA2X2_init(void)
{
	return i2c_add_driver(&bma2x2_driver);
}

static void __exit BMA2X2_exit(void)
{
	i2c_del_driver(&bma2x2_driver);
}

MODULE_AUTHOR("contact@bosch-sensortec.com");
MODULE_DESCRIPTION("BMA2X2 ACCELEROMETER SENSOR DRIVER");
MODULE_LICENSE("GPL v2");

module_init(BMA2X2_init);
module_exit(BMA2X2_exit);
