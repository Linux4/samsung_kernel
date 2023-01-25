/*
 * Copyright (C) 2011 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/input/bmp180.h>
#include <linux/sensors_core.h>
#define BMP180_DRV_NAME		"bmp180"
#define DRIVER_VERSION		"1.0"

#define BMP_VENDOR	"BOCH"
#define BMP_PART_ID	"BMP180"

#define CALIBRATION_FILE_PATH	"/efs/FactoryApp/baro_delta"

struct bmp180_eeprom_data {
	s16 AC1, AC2, AC3;
	u16 AC4, AC5, AC6;
	s16 B1, B2;
	s16 MB, MC, MD;
};

struct bmp180_data {
	struct i2c_client *client;
	struct mutex lock;
	struct workqueue_struct *wq;
	struct work_struct work_pressure;
	struct input_dev *input_dev;
	struct hrtimer timer;
#ifdef FACTORY_TEST
	struct class *class;
	struct device *dev;
	s32 cal_data;
#endif
	ktime_t poll_delay;
	u8 oversampling_rate;
	struct bmp180_eeprom_data bmp180_eeprom_vals;
	bool enabled;
	bool on_before_suspend;
	void (*power_on) (int);
};

static int bmp180_i2c_read(struct i2c_client *client, u8 cmd,
	u8 *buf, int len)
{
	int err;
	int tries = 0;

	do {
		err = i2c_smbus_read_i2c_block_data(client, cmd, len, buf);
		if (err == len)
			return 0;
	} while (++tries < I2C_TRIES);

	return err;
}

static int bmp180_i2c_write(struct i2c_client *client, u8 cmd, u8 data)
{
	int err;
	int tries = 0;

	do {
		err = i2c_smbus_write_byte_data(client, cmd, data);
		if (!err)
			return 0;
	} while (++tries < I2C_TRIES);

	return err;
}

static void bmp180_enable(struct bmp180_data *barom)
{
	pr_debug("%s: bmp180_enable\n", __func__);
	mutex_lock(&barom->lock);
	if (!barom->enabled) {
		barom->enabled = true;
		pr_debug("%s: start timer\n", __func__);

		hrtimer_start(&barom->timer, barom->poll_delay,
			HRTIMER_MODE_REL);
	}
	mutex_unlock(&barom->lock);
}

static void bmp180_disable(struct bmp180_data *barom)
{
	mutex_lock(&barom->lock);
	if (barom->enabled) {
		barom->enabled = false;
		pr_debug("%s: stop timer\n", __func__);
		hrtimer_cancel(&barom->timer);
		cancel_work_sync(&barom->work_pressure);
	}
	mutex_unlock(&barom->lock);
}

static int bmp180_get_raw_temperature(struct bmp180_data *barom,
					u16 *raw_temperature)
{
	int err;
	u16 buf;

	pr_debug("%s: read uncompensated temperature value\n", __func__);
	err = bmp180_i2c_write(barom->client, BMP180_TAKE_MEAS_REG,
		BMP180_MEAS_TEMP);
	if (err) {
		pr_err("%s: can't write BMP180_TAKE_MEAS_REG\n", __func__);

		return err;
	}

	usleep(5000);

	err = bmp180_i2c_read(barom->client, BMP180_READ_MEAS_REG_U,
				(u8 *)&buf, 2);
	if (err) {
		pr_err("%s: Fail to read uncompensated temperature\n",
			__func__);

		return err;
	}
	*raw_temperature = be16_to_cpu(buf);
	pr_debug("%s: uncompensated temperature:  %d\n",
		__func__, *raw_temperature);

	return err;
}

static int bmp180_get_raw_pressure(struct bmp180_data *barom,
					u32 *raw_pressure)
{
	int err;
	u32 buf = 0;

	pr_debug("%s: read uncompensated pressure value\n", __func__);

	err = bmp180_i2c_write(barom->client, BMP180_TAKE_MEAS_REG,
		BMP180_MEAS_PRESS_OVERSAMP_0 |
		(barom->oversampling_rate << 6));
	if (err) {
		pr_err("%s: can't write BMP180_TAKE_MEAS_REG\n", __func__);

		return err;
	}

	msleep(2+(3 << barom->oversampling_rate));

	err = bmp180_i2c_read(barom->client, BMP180_READ_MEAS_REG_U,
				((u8 *)&buf)+1, 3);
	if (err) {
		pr_err("%s: Fail to read uncompensated pressure\n", __func__);

		return err;
	}

	*raw_pressure = be32_to_cpu(buf);
	*raw_pressure >>= (8 - barom->oversampling_rate);
	pr_debug("%s: uncompensated pressure:  %d\n",
		__func__, *raw_pressure);

	return err;
}

static int bmp180_get_pressure_data_body(struct bmp180_data *barom)
{
	u16 raw_temperature;
	u32 raw_pressure;
	long x1, x2, x3, b3, b5, b6;
	unsigned long b4, b7;
	long p;
	int pressure;
	int temperature = 0;

	if (bmp180_get_raw_temperature(barom, &raw_temperature)) {
		pr_err("%s: can't read uncompensated temperature\n", __func__);
		return -1;
	}

#ifdef FACTORY_TEST
	x1 = ((raw_temperature - barom->bmp180_eeprom_vals.AC6)
			* barom->bmp180_eeprom_vals.AC5) >> 15;
	x2 = (barom->bmp180_eeprom_vals.MC << 11)
			/ (x1 + barom->bmp180_eeprom_vals.MD);
	temperature = (x1 + x2 + 8) >> 4;

	if (temperature == 0) {
		pr_info("%s, temperature = 0\n", __func__);
		temperature = -1;
	}
	input_report_rel(barom->input_dev, REL_Z, temperature);
	input_sync(barom->input_dev);
#endif

	if (bmp180_get_raw_pressure(barom, &raw_pressure)) {
		pr_err("%s: Fail to read uncompensated pressure\n",
			__func__);

		return -1;
	}

	x1 = ((raw_temperature - barom->bmp180_eeprom_vals.AC6) *
	      barom->bmp180_eeprom_vals.AC5) >> 15;
	x2 = (barom->bmp180_eeprom_vals.MC << 11) /
	    (x1 + barom->bmp180_eeprom_vals.MD);
	b5 = x1 + x2;

	b6 = (b5 - 4000);
	x1 = (barom->bmp180_eeprom_vals.B2 * ((b6 * b6) >> 12)) >> 11;
	x2 = (barom->bmp180_eeprom_vals.AC2 * b6) >> 11;
	x3 = x1 + x2;
	b3 = (((((long)barom->bmp180_eeprom_vals.AC1) * 4 +
		x3) << barom->oversampling_rate) + 2) >> 2;
	x1 = (barom->bmp180_eeprom_vals.AC3 * b6) >> 13;
	x2 = (barom->bmp180_eeprom_vals.B1 * (b6 * b6 >> 12)) >> 16;
	x3 = ((x1 + x2) + 2) >> 2;
	b4 = (barom->bmp180_eeprom_vals.AC4 *
	      (unsigned long)(x3 + 32768)) >> 15;
	b7 = ((unsigned long)raw_pressure - b3) *
		(50000 >> barom->oversampling_rate);
	if (b7 < 0x80000000)
		p = (b7 * 2) / b4;
	else
		p = (b7 / b4) * 2;

	x1 = (p >> 8) * (p >> 8);
	x1 = (x1 * 3038) >> 16;
	x2 = (-7357 * p) >> 16;
	pressure = p + ((x1 + x2 + 3791) >> 4);
	pr_debug("%s: calibrated pressure: %d\n",
		__func__, pressure);

	return pressure;
}

static void bmp180_get_pressure_data(struct work_struct *work)
{
	struct bmp180_data *barom =
		container_of(work, struct bmp180_data, work_pressure);

	int pressure = bmp180_get_pressure_data_body(barom);

	pressure -= barom->cal_data;
	if (pressure == 0) {
		pr_info("%s, temperature = 0\n", __func__);
		pressure = -1;
	}
	input_report_rel(barom->input_dev, REL_X, pressure);
	input_sync(barom->input_dev);

	return;
}


#ifdef FACTORY_TEST
static int bmp180_open_calibration(struct bmp180_data *barom)
{
	struct file *cal_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;
	char read_data[10] = {0,};

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}

	err = cal_filp->f_op->read(cal_filp,
		read_data, sizeof(char)*10, &cal_filp->f_pos);
	if (err <= 0) {
		pr_err("%s: Can't read the cal data from file\n", __func__);
		err = -EIO;
	}

	err = kstrtoint(read_data, 10, &barom->cal_data);
	if (err < 0) {
		pr_err("error strtoint %s\n", __func__);
		err = -EIO;
	}
	pr_debug("%s: offset (%d)\n", __func__,
		barom->cal_data);
	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int bmp180_do_calibrate(struct device *dev, bool do_calib,
				int pressure_from_outside)
{
	struct bmp180_data *barom = dev_get_drvdata(dev);
	int err = 0;

	if (do_calib)
		barom->cal_data = pressure_from_outside;
	else
		barom->cal_data = 0;

	printk(KERN_INFO "%s: cal data (%d)\n", __func__, barom->cal_data);

	return err;
}
#endif

static int bmp180_input_init(struct bmp180_data *barom)
{
	int err;

	pr_debug("%s: enter\n", __func__);

	barom->input_dev = input_allocate_device();
	if (!barom->input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);

		return -ENOMEM;
	}
	input_set_drvdata(barom->input_dev, barom);
	barom->input_dev->name = "barometer_sensor";

	/* pressure */
	input_set_capability(barom->input_dev, EV_REL, REL_X);

#ifdef FACTORY_TEST

	/* reference altitude */
	input_set_capability(barom->input_dev, EV_REL, REL_Y);

	/* temperature */
	input_set_capability(barom->input_dev, EV_REL, REL_Z);

#endif

	pr_debug("%s: registering barometer input device\n", __func__);

	err = input_register_device(barom->input_dev);
	if (err) {
		pr_err("%s: unable to register input polled device %s\n",
			__func__, barom->input_dev->name);

		goto err_register_device;
	}

	goto done;

err_register_device:
	input_free_device(barom->input_dev);
done:
	return err;
}

static void bmp180_input_cleanup(struct bmp180_data *barom)
{
	input_unregister_device(barom->input_dev);
	input_free_device(barom->input_dev);
}

static int bmp180_read_store_eeprom_val(struct bmp180_data *barom)
{
	int err;
	u16 buf[11];

	err = bmp180_i2c_read(barom->client, BMP180_EEPROM_AC1_U,
				(u8 *)buf, 22);
	if (err) {
		pr_err("%s: Cannot read EEPROM values\n", __func__);
		return err;
	}
	barom->bmp180_eeprom_vals.AC1 = be16_to_cpu(buf[0]);
	barom->bmp180_eeprom_vals.AC2 = be16_to_cpu(buf[1]);
	barom->bmp180_eeprom_vals.AC3 = be16_to_cpu(buf[2]);
	barom->bmp180_eeprom_vals.AC4 = be16_to_cpu(buf[3]);
	barom->bmp180_eeprom_vals.AC5 = be16_to_cpu(buf[4]);
	barom->bmp180_eeprom_vals.AC6 = be16_to_cpu(buf[5]);
	barom->bmp180_eeprom_vals.B1 = be16_to_cpu(buf[6]);
	barom->bmp180_eeprom_vals.B2 = be16_to_cpu(buf[7]);
	barom->bmp180_eeprom_vals.MB = be16_to_cpu(buf[8]);
	barom->bmp180_eeprom_vals.MC = be16_to_cpu(buf[9]);
	barom->bmp180_eeprom_vals.MD = be16_to_cpu(buf[10]);

	return 0;
}

static enum hrtimer_restart bmp180_timer_func(struct hrtimer *timer)
{
	struct bmp180_data *barom = container_of(timer,
		struct bmp180_data, timer);

	pr_debug("%s: start\n", __func__);

	queue_work(barom->wq, &barom->work_pressure);
	hrtimer_forward_now(&barom->timer, barom->poll_delay);
	return HRTIMER_RESTART;
}

static ssize_t bmp180_poll_delay_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct bmp180_data *barom = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		ktime_to_ns(barom->poll_delay));
}

static ssize_t bmp180_poll_delay_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	int err;
	int64_t new_delay;
	struct bmp180_data *barom = dev_get_drvdata(dev);

	err = kstrtoll(buf, 10, &new_delay);
	if (err < 0)
		return err;

	pr_debug("%s: new delay = %lldns, old delay = %lldns\n",
		__func__, new_delay, ktime_to_ns(barom->poll_delay));

	if (new_delay < DELAY_LOWBOUND || new_delay > DELAY_UPBOUND)
		return -EINVAL;

	mutex_lock(&barom->lock);
	if (new_delay != ktime_to_ns(barom->poll_delay))
		barom->poll_delay = ns_to_ktime(new_delay);

	mutex_unlock(&barom->lock);

	return size;
}

static ssize_t bmp180_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct bmp180_data *barom = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", barom->enabled);
}

static ssize_t bmp180_enable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	bool new_value;
	struct bmp180_data *barom = dev_get_drvdata(dev);


	if (sysfs_streq(buf, "1")) {
		new_value = true;
	} else if (sysfs_streq(buf, "0")) {
		new_value = false;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_debug("%s: new_value = %d, old state = %d\n",
		__func__, new_value, barom->enabled);

	if (new_value) {
		bmp180_open_calibration(barom);
		bmp180_enable(barom);
	} else {
		bmp180_disable(barom);
	}

	return size;
}

static ssize_t bmp180_oversampling_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct bmp180_data *barom = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", barom->oversampling_rate);
}

static ssize_t bmp180_oversampling_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct bmp180_data *barom = dev_get_drvdata(dev);

	unsigned long oversampling;
	int success = kstrtoul(buf, 10, &oversampling);
	if (success == 0) {
		if (oversampling > 3)
			oversampling = 3;
		barom->oversampling_rate = oversampling;
		return count;
	}
	return success;
}

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		bmp180_poll_delay_show, bmp180_poll_delay_store);

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		bmp180_enable_show, bmp180_enable_store);

static DEVICE_ATTR(oversampling, S_IWUSR | S_IRUGO,
		bmp180_oversampling_show, bmp180_oversampling_store);

static struct attribute *bmp180_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	&dev_attr_oversampling.attr,
	NULL
};

static struct attribute_group bmp180_attribute_group = {
	.attrs = bmp180_sysfs_attrs,
};

#ifdef FACTORY_TEST
static ssize_t eeprom_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct bmp180_data *barom = dev_get_drvdata(dev);
	int val = 1;

	if (barom->bmp180_eeprom_vals.AC1 != 0 &&
		barom->bmp180_eeprom_vals.AC1 != 0xFFFF)
		goto done;
	if (barom->bmp180_eeprom_vals.AC2 != 0 &&
		barom->bmp180_eeprom_vals.AC2 != 0xFFFF)
		goto done;
	if (barom->bmp180_eeprom_vals.AC3 != 0 &&
		barom->bmp180_eeprom_vals.AC3 != 0xFFFF)
		goto done;
	if (barom->bmp180_eeprom_vals.AC4 != 0 &&
		barom->bmp180_eeprom_vals.AC4 != 0xFFFF)
		goto done;
	if (barom->bmp180_eeprom_vals.AC5 != 0 &&
		barom->bmp180_eeprom_vals.AC5 != 0xFFFF)
		goto done;
	if (barom->bmp180_eeprom_vals.AC6 != 0 &&
		barom->bmp180_eeprom_vals.AC6 != 0xFFFF)
		goto done;

	val = -1;

done:
	return snprintf(buf, PAGE_SIZE, "%d", val);
}

static ssize_t sea_level_pressure_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct bmp180_data *barom = dev_get_drvdata(dev);
	unsigned int new_sea_level_pressure;
	int err;

	err = kstrtouint(buf, 10, &new_sea_level_pressure);
	if (err < 0)
		return err;

	if (new_sea_level_pressure == 0) {
		pr_info("%s, barom->temperature = 0\n", __func__);
		new_sea_level_pressure = -1;
	}
	input_report_rel(barom->input_dev, REL_Y, new_sea_level_pressure);
	input_sync(barom->input_dev);

	return size;
}

static ssize_t bmp180_vendor_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", BMP_VENDOR);
}

static ssize_t bmp180_name_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", BMP_PART_ID);
}

static ssize_t factory_calibration_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct bmp180_data *barom = dev_get_drvdata(dev);
	return sprintf(buf, "%d", barom->cal_data);
}

static ssize_t factory_calibration(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int pressure_from_outside;

	sscanf(buf, "%d", &pressure_from_outside);

	bmp180_do_calibrate(dev, true, pressure_from_outside);

	return size;
}

static DEVICE_ATTR(eeprom_check, S_IRUSR | S_IRGRP,
		eeprom_check_show, NULL);

static DEVICE_ATTR(sea_level_pressure, S_IRUGO | S_IWUSR,
		NULL, sea_level_pressure_store);

static DEVICE_ATTR(vendor, S_IRUGO,
		bmp180_vendor_show, NULL);

static DEVICE_ATTR(name, S_IRUGO,
		bmp180_name_show, NULL);

static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
		factory_calibration_show, factory_calibration);

static struct device_attribute *barom_sensor_attrs[] = {
	&dev_attr_eeprom_check,
	&dev_attr_sea_level_pressure,
	&dev_attr_calibration,
	&dev_attr_vendor,
	&dev_attr_name,
	NULL,
};

#endif

static int __devinit bmp180_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	struct bmp_i2c_platform_data *platform_data;
	struct bmp180_data *barom;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: client not i2c capable\n", __func__);

		return -EIO;
	}

	barom = kzalloc(sizeof(*barom), GFP_KERNEL);
	if (!barom) {
		pr_err("%s: failed to allocate memory for module\n", __func__);
		return -ENOMEM;
	}

	if (client->dev.platform_data != NULL) {
		platform_data = client->dev.platform_data;
		barom->power_on = platform_data->power_on;
	}
	if (barom->power_on)
		barom->power_on(1);

	mutex_init(&barom->lock);
	barom->client = client;

	i2c_set_clientdata(client, barom);

	err = bmp180_read_store_eeprom_val(barom);
	if (err) {
		pr_err("%s: Reading the EEPROM failed\n", __func__);
		err = -ENODEV;
		goto err_read_eeprom;
	}

	hrtimer_init(&barom->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	barom->poll_delay = ns_to_ktime(DELAY_DEFAULT);
	barom->timer.function = bmp180_timer_func;

	barom->wq = create_singlethread_workqueue("bmp180_wq");
	if (!barom->wq) {
		err = -ENOMEM;
		pr_err("%s: could not create workqueue\n", __func__);

		goto err_create_workqueue;
	}

	INIT_WORK(&barom->work_pressure, bmp180_get_pressure_data);

	err = bmp180_input_init(barom);
	if (err) {
		pr_err("%s: could not create input device\n", __func__);

		goto err_input_init;
	}
	err = sysfs_create_group(&barom->input_dev->dev.kobj,
		&bmp180_attribute_group);
	if (err) {
		pr_err("%s: could not create sysfs group\n", __func__);
		goto err_sysfs_create_group;
	}

#ifdef FACTORY_TEST
	/* sysfs for factory test */

	err = sensors_register(barom->dev, barom, barom_sensor_attrs,
		"barometer_sensor");
	if (err) {
		pr_err("%s: cound not register barometer_sensor(%d).\n",
			__func__, err);
		goto err_sysfs_register;
	}

#endif
	goto done;

err_sysfs_register:
err_sysfs_create_group:
	sysfs_remove_group(&barom->input_dev->dev.kobj,
		&bmp180_attribute_group);
	bmp180_input_cleanup(barom);
err_input_init:
	destroy_workqueue(barom->wq);
err_create_workqueue:
err_read_eeprom:
	mutex_destroy(&barom->lock);
	kfree(barom);
done:
	return err;
}

static int __devexit bmp180_remove(struct i2c_client *client)
{
	/* TO DO: revisit ordering here once _probe order is finalized */
	struct bmp180_data *barom = i2c_get_clientdata(client);

	pr_debug("%s: bmp180_remove +\n", __func__);

	sysfs_remove_group(&barom->input_dev->dev.kobj,
				&bmp180_attribute_group);

	bmp180_disable(barom);

	bmp180_input_cleanup(barom);

	destroy_workqueue(barom->wq);

	mutex_destroy(&barom->lock);
	kfree(barom);

	pr_debug("%s: bmp180_remove -\n", __func__);
	return 0;
}

static int bmp180_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bmp180_data *barom = i2c_get_clientdata(client);
	pr_debug("%s: on_before_suspend %d\n",
		__func__, barom->on_before_suspend);
	if (barom->power_on)
		barom->power_on(1);

	if (barom->on_before_suspend)
		bmp180_enable(barom);
	return 0;
}

static int bmp180_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bmp180_data *barom = i2c_get_clientdata(client);

	barom->on_before_suspend = barom->enabled;
	pr_debug("%s: on_before_suspend %d\n",
		__func__, barom->on_before_suspend);
	bmp180_disable(barom);
	if (barom->power_on)
		barom->power_on(0);

	return 0;
}

static const struct i2c_device_id bmp180_id[] = {
	{BMP180_DRV_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, bmp180_id);
static const struct dev_pm_ops bmp180_pm_ops = {
	.suspend	= bmp180_suspend,
	.resume		= bmp180_resume,
};

static struct i2c_driver bmp180_driver = {
	.driver = {
		.name	= BMP180_DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &bmp180_pm_ops,
	},
	.probe		= bmp180_probe,
	.remove		= __devexit_p(bmp180_remove),
	.id_table	= bmp180_id,
};

static int __init bmp180_init(void)
{
	return i2c_add_driver(&bmp180_driver);
}

static void __exit bmp180_exit(void)
{
	i2c_del_driver(&bmp180_driver);

	return;
}

MODULE_AUTHOR("Hyoung Wook Ham <hwham@sta.samsung.com>");
MODULE_DESCRIPTION("BMP180 Pressure sensor driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRIVER_VERSION);

module_init(bmp180_init);
module_exit(bmp180_exit);
