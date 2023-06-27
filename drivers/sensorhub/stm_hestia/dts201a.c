/*
* drivers/sensors/dts201a.c
*
* Partron DTS201A Thermopile Sensor module driver
*
* Copyright (C) 2013 Partron Co., Ltd. - Sensor Lab.
* partron (partron@partron.co.kr)
*
* Both authors are willing to be considered the contact and update points for
* the driver.
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
*
*/
/******************************************************************************
 Revision 1.0.0 2013/Aug/08:
	first release

******************************************************************************/

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include "dts201a.h"
#include <linux/sensor/sensors_core.h>

//#undef DTS201A_DEBUG
#define DELAY_FIX_200MS

static u8 o_avg_cnt = 0;
static u8 o_avg = 8;

struct dts201a_data {
	struct i2c_client *client;
	struct mutex lock;
	struct workqueue_struct *thermopile_wq;
	struct work_struct work_thermopile;
	struct input_dev *input_dev;
	struct device *dev;
	struct outputdata out;
	struct hrtimer timer;
	ktime_t poll_delay;
	int hw_initialized;
	atomic_t enabled;
	atomic_t suspended;
	int bufferdata[42];
//	u32 scl;
//	u32 sda;
	u32 power;
};

static int dts201a_i2c_read(struct dts201a_data *ther,
				  u8 *buf, u8 len)
{
	struct i2c_msg msg[1];
	int err = 0;
	int retry = 3;

	if ((ther->client == NULL) || (!ther->client->adapter))
		return -ENODEV;

	msg->addr = ther->client->addr;
//	msg->addr = DTS201A_SADDR;
	msg->flags = DTS_I2C_READ;
	msg->len = len;
	msg->buf = buf;

	while (retry--) {
		err = i2c_transfer(ther->client->adapter, msg, 1);
		if (err >= 0)
			return err;
	}
	pr_err("%s: i2c read failed at addr 0x%x: %d\n", __func__, msg->addr, err);

	return err;
}

static int dts201a_i2c_write(struct dts201a_data *ther,
				u8 cmd, u16 bm_config)
{
	struct i2c_msg msg[1];
	int err = 0;
	int retry = 3;
	u8 wdata[3]={0, };

	if ((ther->client == NULL) || (!ther->client->adapter))
		return -ENODEV;

	wdata[0] = cmd;
	wdata[1] = (u8)(bm_config>>8);
	wdata[2] = (u8)(bm_config);

	msg->addr = ther->client->addr;
//	msg->addr = DTS201A_SADDR;
	msg->flags = DTS_I2C_WRITE;
	msg->len = 3;
	msg->buf = wdata;

	while (retry--) {
		err = i2c_transfer(ther->client->adapter, msg, 1);
		if (err >= 0)
			return err;
	}
	pr_err("%s, i2c transfer error(%d)\n", __func__, err);

	return err;
}

static int dts201a_hw_init(struct dts201a_data *ther)
{
	int err = 0;
	u8 rbuf[5]={0, };
	u16 custom_id[2]={0, };

	pr_info("[BODYTEMP] %s = %s\n", __func__, DTS_DEV_NAME);

	err = dts201a_i2c_write(ther, DTS_CUST_ID0, 0x0);
	if (err < 0)
		goto error_firstread;

	msleep(20);

	err = dts201a_i2c_read(ther, rbuf, 5);
	if (err < 0)
		goto error_firstread;
	custom_id[0] = ((rbuf[1] << 8) | rbuf[2]);

	err = dts201a_i2c_write(ther, DTS_CUST_ID1, 0x0);
	if (err < 0)
		goto error_firstread;

	msleep(20);

	/* hw chip id check */
	err = dts201a_i2c_read(ther, rbuf, 5);
	if (err < 0)
		goto error_firstread;

	custom_id[1] = ((rbuf[1] << 8) | rbuf[2]);
	if (custom_id[1] != DTS201A_ID1 && custom_id[0] != DTS201A_ID0) {
		err = -ENODEV;
		goto error_unknown_device;
	}

	ther->hw_initialized = 1;

	pr_info("[BODYTEMP] Hw init is done!\n");

	return 0;

error_firstread:
	pr_err("[BODYTEMP] Error reading CUST_ID_DTS201A : is device "
		"available/working?\n");
	goto err_resume_state;
error_unknown_device:
	pr_err("[BODYTEMP] device unknown. Expected: 0x%04x_%04x,"
		" custom_id: 0x%04x_%04x\n", DTS201A_ID1, 
		DTS201A_ID0, custom_id[1], custom_id[0]);
err_resume_state:
	ther->hw_initialized = 0;
	pr_err("[BODYTEMP] hw init error : status = 0x%02x: err = %d\n",
		rbuf[0], err);

	return err;
}

u32 dts201a_moving_o_avg(struct dts201a_data *ther, u32 o_adc)
{
	u8 i;
	u32 ret;
	u32 therm_tot = 0;

	if((o_avg_cnt&0x7f) >= o_avg) {
		o_avg_cnt = 0;
		o_avg_cnt |= 0x80;
	}

	if( o_avg_cnt&0x80 ) {
		ther->out.therm_avg[(o_avg_cnt & 0x7f)] = o_adc;
		o_avg_cnt = (o_avg_cnt & 0x7f) + 1;
		o_avg_cnt |= 0x80;

		for(i=0; i < o_avg; i++)
			therm_tot += ther->out.therm_avg[i];

		ret = (therm_tot/o_avg);
	} else {
		ther->out.therm_avg[(o_avg_cnt & 0x7f)] = o_adc;
		o_avg_cnt++;

		for(i=0; i < o_avg_cnt; i++)
			therm_tot += ther->out.therm_avg[i];

		ret = (therm_tot / o_avg_cnt);
	}

	return ret;
}

static int dts201a_get_thermtemp_data(struct dts201a_data *ther)
{
	int err = 0;
	u8 rbuf[7] = {0,};
	u32 thermopile = 0;
	u32 temperature = 0;
	u8 status = 0;

#if 0
	err = dts201a_i2c_write(ther, DTS_SM_AZSM, 0x0);
	if (err < 0) goto i2c_error;
	msleep(DTS_CONVERSION_TIME);
	err = dts201a_i2c_read(ther, rbuf, 4);
	if (err < 0) goto i2c_error;
	thermopile = (s32) ((((s8) rbuf[1]) << 16) | (rbuf[2] << 8) | rbuf[3]);

	err = dts201a_i2c_write(ther, DTS_TM_AZTM, 0x0);
	if (err < 0) goto i2c_error;
	msleep(DTS_CONVERSION_TIME);
	err = dts201a_i2c_read(ther, rbuf, 4);
	if (err < 0) goto i2c_error;
	temperature = (s32) ((((s8) rbuf[1]) << 16) | (rbuf[2] << 8) | rbuf[3]);

	status = rbuf[0];
#else
	err = dts201a_i2c_write(ther, DTS_MEASURE4, 0x0);
	if (err < 0)
		goto i2c_error;
	
	msleep(DTS_CONVERSION_TIME);
	
	err = dts201a_i2c_read(ther, rbuf, 7);
	if (err < 0)
		goto i2c_error;

	status = rbuf[0];
//	thermopile = ((rbuf[1]) << 16) | (rbuf[2] << 8) | rbuf[3];
	thermopile = dts201a_moving_o_avg(ther, ((rbuf[1]) << 16) | (rbuf[2] << 8) | rbuf[3]);
	temperature = ((rbuf[4]) << 16) | (rbuf[5] << 8) | rbuf[6];

	//average
//	for(i=0; i<3; i++) {
//		err = dts201a_i2c_write(ther, DTS_MEASURE4, 0x0);
//		if (err < 0) goto i2c_error;
//		msleep(DTS_CONVERSION_TIME);
//		err = dts201a_i2c_read(ther, rbuf, 7);
//		if (err < 0) goto i2c_error;
//
//		status = rbuf[0];
//		thermopile = (thermopile + ((rbuf[1] << 16) | (rbuf[2] << 8) | rbuf[3]))/2;
//		temperature = (temperature + ((rbuf[4] << 16) | (rbuf[5] << 8) | rbuf[6]))/2;
//	}
#endif

#ifdef DTS201A_DEBUG
//	dev_dbg(&ther->client->dev, "thermopile = 0x%06x"
//			"Temperature = 0x%06x, status = 0x%02x\n",
//					thermopile, temperature, status);
	pr_info("[BODYTEMP] %s : rbuf[1][2][3] = 0x%02x_%02x_%02x = 0x%06x\n", __func__,
		rbuf[1], rbuf[2], rbuf[3], thermopile);
	pr_info("[BODYTEMP] %s : rbuf[4][5][6] = 0x%02x_%02x_%02x = 0x%06x\n", __func__,
		rbuf[4], rbuf[5], rbuf[6], temperature);
#endif

//	thermopile = thermopile & 0x00ffffff;
//	temperature = temperature & 0x00ffffff;

	ther->out.therm = thermopile;
	ther->out.temp = temperature;
	ther->out.status = status;

	return err;

i2c_error :
	ther->out.therm = 0;
	ther->out.temp = 0;
	ther->out.status = 0;

	return err;
}

static int dts201a_enable(struct dts201a_data *ther)
{
	u8 i;
	int err = 0;


	pr_info("[BODYTEMP] %s = %d\n", __func__, 0);


	if (!atomic_cmpxchg(&ther->enabled, 0, 1)) {
		hrtimer_start(&ther->timer, ther->poll_delay, HRTIMER_MODE_REL);
		o_avg_cnt = 0;
		for(i = 0; i < o_avg; i++)
			ther->out.therm_avg[i]=0;
	}

	return err;
}

static int dts201a_disable(struct dts201a_data *ther)
{
	int err = 0;


	pr_info("[BODYTEMP] %s = %d\n", __func__, 0);


	if (atomic_cmpxchg(&ther->enabled, 1, 0)) {
		hrtimer_cancel(&ther->timer);
		cancel_work_sync(&ther->work_thermopile);
	}

	return err;
}

static ssize_t dts201a_poll_delay_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct dts201a_data *ther = dev_get_drvdata(dev);


	pr_info("[BODYTEMP] %s poll_delay = %lld\n",
		__func__, ktime_to_ns(ther->poll_delay));


	return sprintf(buf, "%lld\n", ktime_to_ns(ther->poll_delay));
}

static ssize_t dts201a_poll_delay_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct dts201a_data *ther = dev_get_drvdata(dev);
	unsigned long new_delay = 0;
	int err;

	err = strict_strtoul(buf, 10, &new_delay);
	if (err < 0)
		return err;

	pr_info("[BODYTEMP] %s new_delay = %ld\n", __func__, new_delay);

	if(new_delay < DTS_DELAY_MINIMUM)
		new_delay = DTS_DELAY_MINIMUM;

	mutex_lock(&ther->lock);
#ifdef DELAY_FIX_200MS
	ther->poll_delay = ns_to_ktime(DTS_DELAY_DEFAULT * NSEC_PER_MSEC);
#else
	ther->poll_delay = ns_to_ktime(new_delay);
#endif
	mutex_unlock(&ther->lock);

	return size;
}

static ssize_t dts201a_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dts201a_data *ther = dev_get_drvdata(dev);

	pr_info("[BODYTEMP] %s ther->enabled = %d\n", __func__,
		atomic_read(&ther->enabled));

	return sprintf(buf, "%d\n", atomic_read(&ther->enabled));
}

static ssize_t dts201a_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct dts201a_data *ther = dev_get_drvdata(dev);
	bool new_value;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("[BODYTEMP] %s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("[BODYTEMP] %s new_value = 0x%d\n", __func__, new_value);

	if (new_value)
		dts201a_enable(ther);
	else
		dts201a_disable(ther);

	return size;
}

static DEVICE_ATTR(poll_delay, 0664,
		dts201a_poll_delay_show, dts201a_poll_delay_store);
static DEVICE_ATTR(enable, 0664,
		dts201a_enable_show, dts201a_enable_store);

static struct attribute *thermopile_sysfs_attrs[] = {
	&dev_attr_poll_delay.attr,
	&dev_attr_enable.attr,
	NULL
};

static struct attribute_group thermopile_attribute_group = {
	.attrs = thermopile_sysfs_attrs,
};

static ssize_t dts201a_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[BODYTEMP] %s VENDOR = %s\n", __func__, DTS_VENDOR);
	return sprintf(buf, "%s\n", DTS_VENDOR);
}

static ssize_t dts201a_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[BODYTEMP] %s CHIP_ID = %s\n", __func__, DTS_CHIP_ID);
	return sprintf(buf, "%s\n", DTS_CHIP_ID);
}

static ssize_t dts201a_raw_data_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct dts201a_data *ther = dev_get_drvdata(dev);
	int raw_object = 0, raw_ambient = 0;

	mutex_lock(&ther->lock);
	dts201a_get_thermtemp_data(ther);
	mutex_unlock(&ther->lock);

	raw_object = ther->out.therm;
	raw_ambient = ther->out.temp;

	return sprintf(buf, "%d.%d\n", raw_ambient, raw_object);
}

static ssize_t coefficient_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dts201a_data *ther = dev_get_drvdata(dev);
	int err = 0;
	u8 rbuf[3]={0, };
	u8 rdata[42] = {0,};
	int i;
	pr_info("[BODYTEMP] %s\n", __func__);

	for(i = 0x1a; i <= 0x2e; i++)
	{
		err = dts201a_i2c_write(ther, i, 0x0);
		if (err < 0) {
			pr_err("%s: i2c write error\n", __func__);
			return err;
		}

		usleep_range(2500, 2600);

		err = dts201a_i2c_read(ther, rbuf, 3);
		/* add busy bit check */
	        if ((err < 0) || (rbuf[0] & 0x20)) {
	            pr_err("%s: i2c read error=0x%02x, 0x%02x\n", __func__,
			err, rbuf[0]);
	            return err;
	        }

	        rdata[(i - 0x1a) + (i - 0x1a)] = rbuf[1];
	        rdata[((i - 0x1a) + (i - 0x1a)) + 1] = rbuf[2];

	        msleep(1);
	}

	for(i=0; i < 42; i++)
		pr_info("[BODYTEMP] %s rdata[%d] = 0x%02x\n",
			__func__, i, rdata[i]);

	return sprintf(buf, "%02x%02x%02x%02x%02x%02x%02x%02x"\
		"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"\
		"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"\
		"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"\
		"%02x%02x%02x%02x\n",
		rdata[0], rdata[1], rdata[2], rdata[3], rdata[4],
		rdata[5], rdata[6], rdata[7], rdata[8], rdata[9],
		rdata[10], rdata[11], rdata[12], rdata[13], rdata[14],
		rdata[15], rdata[16], rdata[17], rdata[18], rdata[19],
		rdata[20], rdata[21], rdata[22], rdata[23], rdata[24],
		rdata[25], rdata[26], rdata[27], rdata[28], rdata[29],
		rdata[30], rdata[31], rdata[32], rdata[33], rdata[34],
		rdata[35], rdata[36], rdata[37], rdata[38], rdata[39],
		rdata[40], rdata[41]);
}

static DEVICE_ATTR(vendor, 0644, dts201a_vendor_show, NULL);
static DEVICE_ATTR(name, 0644, dts201a_name_show, NULL);
static DEVICE_ATTR(raw_data, 0644, dts201a_raw_data_show, NULL);
static DEVICE_ATTR(coefficient_reg, 0664, coefficient_reg_show, NULL);

static struct device_attribute *bodytemp_sensor_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_enable,
	&dev_attr_raw_data,
	&dev_attr_coefficient_reg,
	NULL,
};

static enum hrtimer_restart dts201a_timer_func(struct hrtimer *timer)
{
	struct dts201a_data *ther
		= container_of(timer, struct dts201a_data, timer);
	queue_work(ther->thermopile_wq, &ther->work_thermopile);
	hrtimer_forward_now(&ther->timer, ther->poll_delay);

	return HRTIMER_RESTART;
}

static void dts201a_work_func(struct work_struct *work)
{
	int err = 0;
	struct dts201a_data *ther = container_of(work, struct dts201a_data,
						work_thermopile);

	mutex_lock(&ther->lock);
	err = dts201a_get_thermtemp_data(ther);
	mutex_unlock(&ther->lock);
	if (err < 0)
		dev_err(&ther->client->dev, "get_thermopile_data failed\n");

	input_report_rel(ther->input_dev, EVENT_TYPE_AMBIENT_TEMP,
			ther->out.temp);
	input_report_rel(ther->input_dev, EVENT_TYPE_OBJECT_TEMP,
			ther->out.therm);
	input_sync(ther->input_dev);
//	input_report_rel(ther->input_dev, REL_Z, ther->out.status);
	pr_info("[BODYTEMP] %s therm = %d, temp = %d\n",
		__func__, ther->out.therm, ther->out.temp);

	return;
}

static int dts201a_parse_dt(struct device *dev, struct dts201a_data *data)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int errorno = 0;

	pr_info("[BODYTEMP] %s : dts201a,power\n", __func__);
	data->power = of_get_named_gpio_flags(np, "dts201a,power",
		0, &flags);
	if (data->power < 0) {
		errorno = data->power;
		pr_err("[BODYTEMP] %s: dts201a,power\n", __func__);
		goto dt_exit;
	}

	errorno = gpio_request(data->power, "DTS201A_POWER");
	if (errorno) {
		pr_err("[BODYTEMP] %s: request ENABLE PIN for dts201a failed\n",
			__func__);
		goto dt_exit;
	}

	errorno = gpio_direction_output(data->power, 0);
	if (errorno) {
		pr_err("[BODYTEMP] %s: failed to set ENABLE PIN as input\n",
			__func__);
		goto dt_exit;
	}
dt_exit:
	return 0;
#if 0
dt_exit:
	return errorno;
#endif
}

static int dts201a_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct dts201a_data *ther;
	struct dts_platform_data *pdata;
	int err = 0;

	pr_info("[BODYTEMP] %s DEV_NAME = %s\n", __func__, DTS_DEV_NAME);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto err_exit_check_functionality_failed;
	}

	ther = kzalloc(sizeof(struct dts201a_data), GFP_KERNEL);
	if (ther == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
			"failed to allocate memory for module data: %d\n", err);
		goto err_exit_alloc_data_failed;
	}

	if (client->dev.of_node)
		pr_info("[BODYTEMP] %s : of node\n", __func__);
	else {
		pr_err("[BODYTEMP] %s : no of node\n", __func__);
		goto err_setup_reg;
	}

	err = dts201a_parse_dt(&client->dev, ther);

	if (err) {
		pr_err("[BODYTEMP] %s : parse dt error\n", __func__);
		//goto err_parse_dt;
	}

	gpio_set_value(ther->power, 1);
	udelay(100);

	pdata = client->dev.platform_data;

	ther->client = client;
	i2c_set_clientdata(client, ther);

	mutex_init(&ther->lock);
	mutex_lock(&ther->lock);

	err = dts201a_hw_init(ther);
	if (err < 0)
		goto err_mutexunlockfreedata;

	hrtimer_init(&ther->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ther->poll_delay = ns_to_ktime(DTS_DELAY_DEFAULT * NSEC_PER_MSEC);
	ther->timer.function = dts201a_timer_func;

	ther->thermopile_wq =
		create_singlethread_workqueue("dts201a_thermopile_wq");
	if (!ther->thermopile_wq) {
		err = -ENOMEM;
		pr_err("%s: could not create workqueue\n", __func__);
		goto err_create_workqueue;
	}

	ther->input_dev = input_allocate_device();
	if (!ther->input_dev) {
		err = -ENOMEM;
		dev_err(&ther->client->dev, "input device allocate failed\n");
		goto err_input_allocate_device;
	}

	ther->input_dev->name = "bodytemp_sensor";
	input_set_capability(ther->input_dev, EV_REL, EVENT_TYPE_AMBIENT_TEMP);
	input_set_capability(ther->input_dev, EV_REL, EVENT_TYPE_OBJECT_TEMP);
	input_set_drvdata(ther->input_dev, ther);
//	input_set_capability(ther->input_dev, EV_REL, REL_Z);

	err = input_register_device(ther->input_dev);
	if (err < 0) {
		dev_err(&ther->client->dev,
			"unable to register input polled device %s\n",
			ther->input_dev->name);
		goto err_input_register_device;
	}

	err = sensors_create_symlink(&ther->input_dev->dev.kobj,
					ther->input_dev->name);
	if (err < 0) {
		input_unregister_device(ther->input_dev);
		goto err_sysfs_create_symlink;
	}

	err = sysfs_create_group(&ther->input_dev->dev.kobj,
				&thermopile_attribute_group);
	if (err) {
		pr_err("[BODYTEMP] %s: could not create sysfs group\n",
			__func__);
		goto err_sysfs_create_group;
	}

	err = sensors_register(ther->dev,
			ther, bodytemp_sensor_attrs, "bodytemp_sensor");
	if (err) {
		pr_err("[BODYTEMP] %s: register bodytemp device failed(%d).\n",
			__func__, err);
		goto err_sysfs_create_symlink;
	}

	INIT_WORK(&ther->work_thermopile, dts201a_work_func);

	mutex_unlock(&ther->lock);

	return 0;

err_sysfs_create_group:
	sysfs_remove_group(&ther->input_dev->dev.kobj,
					&thermopile_attribute_group);
	input_unregister_device(ther->input_dev);
err_sysfs_create_symlink:
	sensors_remove_symlink(&ther->input_dev->dev.kobj,
			ther->input_dev->name);
err_input_register_device:
	input_free_device(ther->input_dev);
err_input_allocate_device:
err_create_workqueue:
	destroy_workqueue(ther->thermopile_wq);
err_mutexunlockfreedata:
	mutex_destroy(&ther->lock);
err_setup_reg:
	kfree(ther);
err_exit_alloc_data_failed:
err_exit_check_functionality_failed:
	pr_err("[BODYTEMP] %s: Driver Init failed\n", DTS_DEV_NAME);
	return err;
}

static int __devexit dts201a_remove(struct i2c_client *client)
{
	struct dts201a_data *ther = i2c_get_clientdata(client);

	pr_info("[BODYTEMP] %s = %d\n", __func__, 0);

	sysfs_remove_group(&ther->input_dev->dev.kobj,
				&thermopile_attribute_group);
	input_unregister_device(ther->input_dev);
	dts201a_disable(ther);
	flush_workqueue(ther->thermopile_wq);
	destroy_workqueue(ther->thermopile_wq);
	input_free_device(ther->input_dev);
	mutex_destroy(&ther->lock);
	kfree(ther);

	return 0;
}

static int dts201a_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct dts201a_data *ther = i2c_get_clientdata(client);
	int ret = 0;

	pr_info("[BODYTEMP] %s = %d\n", __func__, 0);

	if (atomic_read(&ther->suspended) == 1) {
		ret = dts201a_enable(ther);
		if (ret < 0)
			pr_err("[BODYTEMP] %s: could not enable\n", __func__);
		atomic_set(&ther->suspended, 0);
	}

	return ret;
}

static int dts201a_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct dts201a_data *ther = i2c_get_clientdata(client);
	int ret = 0;

	pr_info("[BODYTEMP] %s = %d\n", __func__, 0);

	if (atomic_read(&ther->enabled) == 1) {
		ret = dts201a_disable(ther);
		if (ret < 0)
			pr_err("[BODYTEMP] %s: could not disable\n", __func__);
		atomic_set(&ther->suspended, 1);
	}

	return ret;
}

static struct of_device_id dts201a_table[] = {
	{ .compatible = "dts201a",},
	{},
};

static const struct i2c_device_id dts201a_id[] = {
	{ DTS_DEV_NAME, 0},
	{ },
};

//MODULE_DEVICE_TABLE(i2c, dts201a_id);

static const struct dev_pm_ops dts201a_pm_ops = {
	.suspend = dts201a_suspend,
	.resume = dts201a_resume
};

static struct i2c_driver dts201a_driver = {
	.driver = {
		.name = DTS_DEV_NAME,
		.owner = THIS_MODULE,
		.pm = &dts201a_pm_ops,
		.of_match_table = dts201a_table,
	},
	.probe = dts201a_probe,
	.remove = __devexit_p(dts201a_remove),
	.id_table = dts201a_id,
};

static int __init dts201a_init(void)
{
	pr_info("[BODYTEMP] %s = %s\n", __func__, DTS_DEV_NAME);

	return i2c_add_driver(&dts201a_driver);
}

static void __exit dts201a_exit(void)
{
	pr_info("[BODYTEMP] %s = %s\n", __func__, DTS_DEV_NAME);

	i2c_del_driver(&dts201a_driver);
	return;
}

module_init(dts201a_init);
module_exit(dts201a_exit);

MODULE_DESCRIPTION("PARTRON dts201a thermopile sensor sysfs driver");
MODULE_AUTHOR("PARTRON");
MODULE_LICENSE("GPL");
