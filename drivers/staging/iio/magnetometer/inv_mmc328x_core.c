/*
* Copyright (C) 2012 Invensense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

/**
 *  @addtogroup  DRIVERS
 *  @brief       Hardware drivers.
 *
 *  @{
 *      @file    inv_mmc328x_core.c
 *      @brief   Invensense implementation for MMC328x
 *      @details This driver currently works for the MMC328x
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>

#include "inv_mmc328x_iio.h"
#include "sysfs.h"
#include "inv_test/inv_counters.h"

#define MMC328X_RESET_INTV		100
#define MMC328X_WAIT_TM		10	/* ms */
#define MMC328X_DELAY_RRM		1	/* ms */
#define MMC328X_DELAY_TM		50	/* ms */
#define MMC328X_DELAY_RM		100	/* ms */


static u32 read_idx;

s32 i2c_write(const struct i2c_client *client,
		u8 command, u8 length, const u8 values)
{
	INV_I2C_INC_COMPASSWRITE(3);
	return i2c_smbus_write_i2c_block_data(client, command, length, &values);
}

s32 i2c_read(const struct i2c_client *client,
		u8 command, u8 length, u8 *values)
{
	INV_I2C_INC_COMPASSWRITE(3);
	INV_I2C_INC_COMPASSREAD(length);
	return i2c_smbus_read_i2c_block_data(client, command, length, values);
}

void set_mmc328x_enable(struct iio_dev *indio_dev, bool enable)
{
	struct inv_mmc328x_state_s *st = iio_priv(indio_dev);
	int pre_enable = st->enable;

	if (enable) {
		if (pre_enable == 0) {
			schedule_delayed_work(&st->work,
				msecs_to_jiffies(st->delay));
			st->enable = 1;
		}

	} else {
		if (pre_enable == 1) {
			cancel_delayed_work_sync(&st->work);
			st->enable = 0;
		}
	}
}

int mmc328x_read_raw_data(struct inv_mmc328x_state_s *st, short dat[3])
{
	unsigned char buf[6];

	if (!(read_idx % MMC328X_RESET_INTV)) {
		/* Reset sensor Periodly */
		/* RRM */
		i2c_write(st->i2c, MMC328X_REG_CTRL, 1, MMC328X_CTRL_RRM);
		/* wait external capacitor charging done for next RRM */
		msleep(MMC328X_DELAY_RRM);
		/* TM */
		i2c_write(st->i2c, MMC328X_REG_CTRL, 1, MMC328X_CTRL_TM);
		msleep(MMC328X_DELAY_TM);
		/* RM */
		i2c_write(st->i2c, MMC328X_REG_CTRL, 1, MMC328X_CTRL_RM);
		/* wait external capacitor charging done for next RM */
		msleep(MMC328X_DELAY_RM);
	}
	/* Take meaurement */
	i2c_write(st->i2c, MMC328X_REG_CTRL, 1, MMC328X_CTRL_TM);
	msleep(MMC328X_WAIT_TM);
	i2c_read(st->i2c, MMC328X_REG_DATA, sizeof(buf), buf);
	dat[0] = buf[1] << 8 | buf[0];
	dat[1] = buf[3] << 8 | buf[2];
	dat[2] = buf[5] << 8 | buf[4];

	read_idx++;

	return 0;
}

/**
 *  mmc328x_read_raw() - read raw method.
 */
static int mmc328x_read_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int *val,
			      int *val2,
			      long mask)
{
	struct inv_mmc328x_state_s  *st = iio_priv(indio_dev);
	switch (mask) {
	case 0:
		if (chan->type == IIO_MAGN) {
			*val = st->compass_data[chan->channel2 - IIO_MOD_X];
			return IIO_VAL_INT;
		}

		return -EINVAL;
	case IIO_CHAN_INFO_SCALE:
		if (chan->type == IIO_MAGN) {
			*val = MEMSIC_SCALE;
			return IIO_VAL_INT;
		}
		return -EINVAL;
	default:
		return -EINVAL;
	}
}

/**
 *  mmc328x_write_raw() - write raw method.
 */
static int mmc328x_write_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val,
			       int val2,
			       long mask)
{
	int result;
	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		result = -EINVAL;
		return result;
	default:
		return -EINVAL;
	}
	return 0;
}

/**
 * inv_compass_matrix_show() - show orientation matrix
 */
static ssize_t inv_compass_matrix_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	signed char *m;
	struct inv_mmc328x_state_s *st = iio_priv(indio_dev);
	m = st->plat_data.orientation;
	return sprintf(buf,
	"%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		m[0],  m[1],  m[2],  m[3], m[4], m[5], m[6], m[7], m[8]);
}

static ssize_t mmc328x_rate_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mmc328x_state_s *st = iio_priv(indio_dev);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (0 == data)
		return -EINVAL;
	/* transform rate to delay in ms */
	data = 1000 / data;
	if (data > MMC328X_MAX_DELAY)
		data = MMC328X_MAX_DELAY;
	if (data < MMC328X_MIN_DELAY)
		data = MMC328X_MIN_DELAY;
	st->delay = data;
	return count;
}

static ssize_t mmc328x_rate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct inv_mmc328x_state_s *st = iio_priv(indio_dev);
	/* transform delay in ms to rate */
	return sprintf(buf, "%d\n", 1000 / st->delay);
}

static void mmc328x_work_func(struct work_struct *work)
{
	struct inv_mmc328x_state_s *st =
		container_of((struct delayed_work *)work,
			struct inv_mmc328x_state_s, work);
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	unsigned long delay = msecs_to_jiffies(st->delay);

	inv_read_mmc328x_fifo(indio_dev);
	schedule_delayed_work(&st->work, delay);
	INV_I2C_INC_COMPASSIRQ();

}

static const struct iio_chan_spec compass_channels[] = {
	{
		.type = IIO_MAGN,
		.modified = 1,
		.channel2 = IIO_MOD_X,
		.info_mask = IIO_CHAN_INFO_SCALE_SHARED_BIT,
		.scan_index = INV_MMC328X_SCAN_MAGN_X,
		.scan_type = IIO_ST('s', 16, 16, 0)
	}, {
		.type = IIO_MAGN,
		.modified = 1,
		.channel2 = IIO_MOD_Y,
		.info_mask = IIO_CHAN_INFO_SCALE_SHARED_BIT,
		.scan_index = INV_MMC328X_SCAN_MAGN_Y,
		.scan_type = IIO_ST('s', 16, 16, 0)
	}, {
		.type = IIO_MAGN,
		.modified = 1,
		.channel2 = IIO_MOD_Z,
		.info_mask = IIO_CHAN_INFO_SCALE_SHARED_BIT,
		.scan_index = INV_MMC328X_SCAN_MAGN_Z,
		.scan_type = IIO_ST('s', 16, 16, 0)
	},
	IIO_CHAN_SOFT_TIMESTAMP(INV_MMC328X_SCAN_TIMESTAMP)
};

static DEVICE_ATTR(compass_matrix, S_IRUGO, inv_compass_matrix_show, NULL);
static DEVICE_ATTR(sampling_frequency, S_IRUGO | S_IWUSR, mmc328x_rate_show,
		mmc328x_rate_store);

static struct attribute *inv_mmc328x_attributes[] = {
	&dev_attr_compass_matrix.attr,
	&dev_attr_sampling_frequency.attr,
	NULL,
};
static const struct attribute_group inv_attribute_group = {
	.name = "mmc328x",
	.attrs = inv_mmc328x_attributes,
};

static const struct iio_info mmc328x_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &mmc328x_read_raw,
	.write_raw = &mmc328x_write_raw,
	.attrs = &inv_attribute_group,
};

/*constant IIO attribute */
/**
 *  inv_mmc328x_probe() - probe function.
 */
static int inv_mmc328x_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct inv_mmc328x_state_s *st;
	struct iio_dev *indio_dev;
	int result;
	char data;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		result = -ENODEV;
		goto out_no_free;
	}
	indio_dev = iio_allocate_device(sizeof(*st));
	if (indio_dev == NULL) {
		result =  -ENOMEM;
		goto out_no_free;
	}

	st = iio_priv(indio_dev);
	st->i2c = client;
	st->sl_handle = client->adapter;
	st->plat_data =
		*(struct mpu_platform_data *)dev_get_platdata(&client->dev);
	st->delay = 10;

	i2c_set_clientdata(client, indio_dev);

	indio_dev->dev.parent = &client->dev;
	indio_dev->name = id->name;
	indio_dev->channels = compass_channels;
	indio_dev->num_channels = ARRAY_SIZE(compass_channels);
	indio_dev->info = &mmc328x_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->currentmode = INDIO_DIRECT_MODE;
	/* check whether mmc328x chip exist*/
	result = i2c_read(st->i2c, 0x10, 1, &data);

	if (result < 1) {
		pr_err("----mmc328x chip chip failed return-----\n");
		return -EIO;
	}
	result = inv_mmc328x_configure_ring(indio_dev);
	if (result)
		goto out_free;
	result = iio_buffer_register(indio_dev, indio_dev->channels,
				indio_dev->num_channels);
	if (result)
		goto out_unreg_ring;
	result = inv_mmc328x_probe_trigger(indio_dev);
	if (result)
		goto out_remove_ring;

	result = iio_device_register(indio_dev);
	if (result)
		goto out_remove_trigger;
	INIT_DELAYED_WORK(&st->work, mmc328x_work_func);
	pr_info("%s: Probe name %s\n", __func__, id->name);
	return 0;

out_remove_trigger:
	if (indio_dev->modes & INDIO_BUFFER_TRIGGERED)
		inv_mmc328x_remove_trigger(indio_dev);
out_remove_ring:
	iio_buffer_unregister(indio_dev);
out_unreg_ring:
	inv_mmc328x_unconfigure_ring(indio_dev);
out_free:
	iio_free_device(indio_dev);
out_no_free:
	dev_err(&client->adapter->dev, "%s failed %d\n", __func__, result);
	return -EIO;

}
/**
 *  inv_mmc328x_remove() - remove function.
 */
static int inv_mmc328x_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct inv_mmc328x_state_s *st = iio_priv(indio_dev);
	cancel_delayed_work_sync(&st->work);
	iio_device_unregister(indio_dev);
	inv_mmc328x_remove_trigger(indio_dev);
	iio_buffer_unregister(indio_dev);
	inv_mmc328x_unconfigure_ring(indio_dev);
	iio_free_device(indio_dev);

	dev_info(&client->adapter->dev, "inv-mmc328x-iio module removed.\n");
	return 0;
}
static const unsigned short normal_i2c[] = { I2C_CLIENT_END };
/* device id table is used to identify what device can be
 * supported by this driver
 */
static const struct i2c_device_id inv_mmc328x_id[] = {
	{"mmc328x", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, inv_mmc328x_id);

static struct i2c_driver inv_mmc328x_driver = {
	.class = I2C_CLASS_HWMON,
	.probe		=	inv_mmc328x_probe,
	.remove		=	inv_mmc328x_remove,
	.id_table	=	inv_mmc328x_id,
	.driver = {
		.owner	=	THIS_MODULE,
		.name	=	"inv-mmc328x-iio",
	},
	.address_list = normal_i2c,
};
static int __init inv_mmc328x_init(void)
{
	int result = i2c_add_driver(&inv_mmc328x_driver);
	if (result) {
		pr_err("%s failed\n", __func__);
		return result;
	}
	return 0;
}

static void __exit inv_mmc328x_exit(void)
{
	i2c_del_driver(&inv_mmc328x_driver);
}
module_init(inv_mmc328x_init);
module_exit(inv_mmc328x_exit);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("inv_mmc328x_iio");
