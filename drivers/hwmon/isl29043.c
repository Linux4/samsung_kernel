/*
 * isl29043.c - ISL29043 light and proximity sensor driver
 *
 * Copyright 2008-2009 Intersil Inc.
 * Copyright (C) 2012 Marvell International Ltd.
 *
 * This driver is partly based on v1.0 isl29028 driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/version.h>
#include <linux/pm.h>
#include <video/mmp_disp.h>

#include "isl29043.h"

#define CHIPID_REG		0x0
#define CFG_REG			0x1
#define INT_REG			0x2
#define PROX_LT			0x3
#define PROX_HT			0x4
#define ALSIR_TH1		0x5
#define ALSIR_TH2		0x6
#define ALSIR_TH3		0x7
#define PROX_DATA		0x8
#define ALSIR_DT1		0x9
#define ALSIR_DT2		0xa

#define PROX_EN			(0x1 << 7)
#define ALS_EN			(0x1 << 2)
#define ALS_RANGE_H		(0x1 << 1)
#define PROX_SLP_100		(0x3 << 4)
#define PROX_SLP_200		(0x2 << 4)
#define PROX_SLP_400		(0x1 << 4)
#define PROX_SLP_800		(0x0 << 4)
#define PROX_FLAG		(0X1 << 7)

#define ALS_SAMPLE_INTERVAL	200
#define PS_SAMPLE_INTERVAL	200
#define FAR_TO_NEAR		2
#define NEAR_TO_FAR		10
#define DEVICE_ATTR2(_name, _mode, _show, _store) \
struct device_attribute dev_attr2_##_name = __ATTR(_name, _mode, _show, _store)

struct isl29043_dev {
	struct i2c_client *client;
	struct input_dev *input_dev_ps;
	struct input_dev *input_dev_als;
	struct mutex active_lock;
	int als_user_count;
	int ps_user_count;
	unsigned char ps_data;
	unsigned int als_data;
	unsigned int als_interval;
	unsigned int ps_interval;
	struct isl29043_platform_data *pdata;
	struct regulator *avdd;
	struct gpio_edge_desc *edge_ps_desc;
};

/* avoid android access isl29043 during suspend */
#define NUMTRY 5
static atomic_t suspend_flag = ATOMIC_INIT(0);

static struct delayed_work als_input_work;
static struct work_struct ps_int_work;
/* static struct work_struct ps_wakeup_work; */

static struct isl29043_dev isl29043_dev = {
	.client = NULL,
	.als_user_count = 0,
	.ps_user_count = 0,
	.als_interval = ALS_SAMPLE_INTERVAL,
	.ps_interval = PS_SAMPLE_INTERVAL,
	.edge_ps_desc = NULL,
};

static int active_ps_set(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	s32 val;
	int ret;
	int enable = strcmp(buf, "1\n") ? 0 : 1;

	mutex_lock(&isl29043_dev.active_lock);
	if (enable) {
		if ((isl29043_dev.ps_user_count == 0) &&
		    (!atomic_read(&suspend_flag))) {
			if (regulator_enable(isl29043_dev.avdd)) {
				dev_err(dev, "intersil sensor avdd power supply get failed\n");
				goto out;
			}
			/* set PROX_LT */
			ret = i2c_smbus_write_byte_data(isl29043_dev.client,
							PROX_LT, 10);
			if (ret < 0) {
				dev_err(dev, "set PROX_LT error\n");
				goto out;
			}
			/* set PROX_HT */
			ret = i2c_smbus_write_byte_data(isl29043_dev.client,
							PROX_HT, 50);
			if (ret < 0) {
				dev_err(dev, "set PROX_LT error\n");
				goto out;
			}
			enable_irq(isl29043_dev.client->irq);
			val = i2c_smbus_read_byte_data(isl29043_dev.client,
						       CFG_REG);
			if (val < 0) {
				dev_err(dev, "read data error when enable\n");
				goto out;
			}
			val |= PROX_EN | PROX_SLP_100;
			ret = i2c_smbus_write_byte_data(isl29043_dev.client,
							CFG_REG, val);
			if (ret < 0) {
				dev_err(dev, "write data error when enable\n");
				goto out;
			}
			dev_info(dev, "PS on\n");
		}
		isl29043_dev.ps_user_count++;
	} else {
		if ((isl29043_dev.ps_user_count == 1) &&
		    (!atomic_read(&suspend_flag))) {
			val = i2c_smbus_read_byte_data(isl29043_dev.client,
						       CFG_REG);
			if (val < 0) {
				dev_err(dev, "read data error when disable\n");
				goto out;
			}
			val &= ~PROX_EN;
			ret = i2c_smbus_write_byte_data(isl29043_dev.client,
							CFG_REG, val);
			if (ret < 0) {
				dev_err(dev, "write data error when disable\n");
				goto out;
			}
			disable_irq(isl29043_dev.client->irq);
			dev_info(dev, "PS off\n");
			regulator_disable(isl29043_dev.avdd);
		}
		isl29043_dev.ps_user_count--;
		if (isl29043_dev.ps_user_count < 0)
			isl29043_dev.ps_user_count = 0;
	}

out:
	mutex_unlock(&isl29043_dev.active_lock);
	return count;
}

static int active_als_set(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	s32 val;
	int ret;
	int enable = strcmp(buf, "1\n") ? 0 : 1;

	mutex_lock(&isl29043_dev.active_lock);
	if (enable) {
		if ((isl29043_dev.als_user_count == 0) &&
		    (!atomic_read(&suspend_flag))) {
			if (regulator_enable(isl29043_dev.avdd)) {
				dev_err(dev, "intersil sensor avdd power supply get failed\n");
				goto out;
			}
			/*
			 * set ALSIR low threshold to 0x0, set high threshold
			 * to 0xFFF, so ALS will not trigger interrupt.
			 */
			ret = i2c_smbus_write_byte_data(isl29043_dev.client,
							ALSIR_TH1, 0x0);
			if (ret < 0) {
				dev_err(dev, "set ALSIR_TH1 error\n");
				goto out;
			}
			ret = i2c_smbus_write_byte_data(isl29043_dev.client,
							ALSIR_TH2, 0xF0);
			if (ret < 0) {
				dev_err(dev, "set ALSIR_TH2 error\n");
				goto out;
			}
			ret = i2c_smbus_write_byte_data(isl29043_dev.client,
							ALSIR_TH3, 0xFF);
			if (ret < 0) {
				dev_err(dev, "set ALSIR_TH2 error\n");
				goto out;
			}

			val = i2c_smbus_read_byte_data(isl29043_dev.client,
						       CFG_REG);
			if (val < 0) {
				dev_err(dev, "read data error when enable\n");
				goto out;
			}
			val |= ALS_EN | ALS_RANGE_H;
			ret = i2c_smbus_write_byte_data(isl29043_dev.client,
							CFG_REG, val);
			if (ret < 0) {
				dev_err(dev, "write data error when enable\n");
				goto out;
			}
			schedule_delayed_work(&als_input_work,
					      msecs_to_jiffies
					      (isl29043_dev.als_interval));
			dev_info(dev, "ALS on\n");
		}
		isl29043_dev.als_user_count++;
	} else {
		if ((isl29043_dev.als_user_count == 1) &&
		    (!atomic_read(&suspend_flag))) {
			cancel_delayed_work_sync(&als_input_work);
			val = i2c_smbus_read_byte_data(isl29043_dev.client,
						       CFG_REG);
			if (val < 0) {
				dev_err(dev, "read data error when disable\n");
				goto out;
			}
			val &= ~ALS_EN;
			ret = i2c_smbus_write_byte_data(isl29043_dev.client,
							CFG_REG, val);
			if (ret < 0) {
				dev_err(dev, "write data error when disable\n");
				goto out;
			}
			dev_info(dev, "ALS off\n");
			regulator_disable(isl29043_dev.avdd);
		}
		isl29043_dev.als_user_count--;
		if (isl29043_dev.als_user_count < 0)
			isl29043_dev.als_user_count = 0;
	}
out:
	mutex_unlock(&isl29043_dev.active_lock);
	return count;
}

static int active_ps_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", isl29043_dev.ps_user_count > 0);
}

static int active_als_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", isl29043_dev.als_user_count > 0);
}

static int interval_ps_set(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	unsigned long val = 0;
	u8 tmp;
	int ret, i = 0;
	for (i = 0; i < NUMTRY; i++) {
		if (!atomic_read(&suspend_flag))
			break;
		msleep(100);
	}
	if (i == NUMTRY) {
		dev_warn(dev, "%s: can't set interval during suspend\n",
			 __func__);
		return 0;
	}
	if (isl29043_dev.ps_user_count == 0) {
		dev_warn(dev, "%s: can't set interval when inactive\n",
			 __func__);
		return 0;
	}
	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;
	tmp = i2c_smbus_read_byte_data(isl29043_dev.client, CFG_REG);
	if (val < 150) {
		val = 100;
		tmp |= PROX_SLP_100;
	} else if (val < 300) {
		val = 200;
		tmp |= PROX_SLP_200;
	} else if (val < 600) {
		val = 400;
		tmp |= PROX_SLP_400;
	} else {
		val = 800;
		tmp |= PROX_SLP_800;
	}
	i2c_smbus_write_byte_data(isl29043_dev.client, CFG_REG, tmp);
	isl29043_dev.ps_interval = val;
	return count;
}

static int interval_als_set(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned long val = 0;
	int ret, i = 0;
	for (i = 0; i < NUMTRY; i++) {
		if (!atomic_read(&suspend_flag))
			break;
		msleep(100);
	}
	if (i == NUMTRY) {
		dev_warn(dev, "%s: can't set interval during suspend\n",
			 __func__);
		return 0;
	}
	if (isl29043_dev.als_user_count == 0) {
		dev_warn(dev, "%s: can't set interval when inactive\n",
			 __func__);
		return 0;
	}
	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;
	if (val < 100)
		val = 100;
	else if (val > 800)
		val = 800;
	isl29043_dev.als_interval = val;
	return count;
}

static int interval_ps_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", isl29043_dev.ps_interval);
}

static int interval_als_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", isl29043_dev.als_interval);
}

static ssize_t wake_ps_set(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	static int i = 1;

	if (i == 10)
		i = 1;
	input_report_abs(isl29043_dev.input_dev_ps, ABS_MISC, i++);
	return count;
}

static ssize_t wake_als_set(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	static int i = 1;

	if (i == 10)
		i = 1;
	input_report_abs(isl29043_dev.input_dev_als, ABS_MISC, i++);
	return count;

}

static int data_ps_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int i = 0;

	for (i = 0; i < NUMTRY; i++) {
		if (!atomic_read(&suspend_flag))
			break;
		msleep(100);
	}
	if (i == NUMTRY) {
		dev_warn(dev, "%s: can't data show during suspend\n", __func__);
		return 0;
	}

	return sprintf(buf, "%d\n", isl29043_dev.ps_data);
}

static int data_als_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	int i = 0;

	for (i = 0; i < NUMTRY; i++) {
		if (!atomic_read(&suspend_flag))
			break;
		msleep(100);
	}
	if (i == NUMTRY) {
		dev_warn(dev, "%s: can't data show during suspend\n", __func__);
		return 0;
	}

	return sprintf(buf, "%d\n", isl29043_dev.als_data);
}

static int status_show(struct device *dev,
		       struct device_attribute *attr, char *buf)
{
	return 0;
}

static DEVICE_ATTR(active, S_IRUGO | S_IWUSR | S_IWGRP,
		   active_als_show, active_als_set);
static DEVICE_ATTR(interval, S_IRUGO | S_IWUSR | S_IWGRP,
		   interval_als_show, interval_als_set);
static DEVICE_ATTR(data, S_IRUGO, data_als_show, NULL);
static DEVICE_ATTR(wake, S_IWUSR | S_IWGRP, NULL, wake_als_set);
static DEVICE_ATTR(status, S_IRUGO, status_show, NULL);

static DEVICE_ATTR2(active, S_IRUGO | S_IWUSR | S_IWGRP,
		    active_ps_show, active_ps_set);
static DEVICE_ATTR2(interval, S_IRUGO | S_IWUSR | S_IWGRP,
		    interval_ps_show, interval_ps_set);
static DEVICE_ATTR2(data, S_IRUGO, data_ps_show, NULL);
static DEVICE_ATTR2(wake, S_IWUSR | S_IWGRP, NULL, wake_ps_set);
static DEVICE_ATTR2(status, S_IRUGO, status_show, NULL);

static struct attribute *sysfs_als_attributes[] = {
	&dev_attr_status.attr,
	&dev_attr_interval.attr,
	&dev_attr_data.attr,
	&dev_attr_active.attr,
	&dev_attr_wake.attr,
	NULL
};

static struct attribute *sysfs_ps_attributes[] = {
	&dev_attr2_active.attr,
	&dev_attr2_interval.attr,
	&dev_attr2_wake.attr,
	&dev_attr2_data.attr,
	&dev_attr2_status.attr,
	NULL
};

static struct attribute_group sysfs_als_attribute_group = {
	.attrs = sysfs_als_attributes
};

static struct attribute_group sysfs_ps_attribute_group = {
	.attrs = sysfs_ps_attributes
};

static void ps_int_worker(struct work_struct *work)
{
	u8 val;

	val = i2c_smbus_read_byte_data(isl29043_dev.client, INT_REG);
	if (val & PROX_FLAG) {
		isl29043_dev.ps_data = FAR_TO_NEAR;
		input_report_abs(isl29043_dev.input_dev_ps,
				 ABS_DISTANCE, isl29043_dev.ps_data);
		input_sync(isl29043_dev.input_dev_ps);

		pr_debug("%s: far to near detect, ps_data = %d\n",
			 __func__, isl29043_dev.ps_data);
	} else {
		isl29043_dev.ps_data = NEAR_TO_FAR;
		input_report_abs(isl29043_dev.input_dev_ps,
				 ABS_DISTANCE, isl29043_dev.ps_data);
		input_sync(isl29043_dev.input_dev_ps);

		pr_debug("%s: near to far detect, ps_data = %d\n",
			 __func__, isl29043_dev.ps_data);
	}
}

static irqreturn_t isl29043_ps_irq_handler(int irq, void *dev_id)
{
	schedule_work(&ps_int_work);

	return IRQ_HANDLED;
}

static void isl29043_report_als_value(struct work_struct *work)
{
	u8 m_data, l_data;
	u16 data;

	/* read als data */
	l_data = i2c_smbus_read_byte_data(isl29043_dev.client, ALSIR_DT1);
	m_data = i2c_smbus_read_byte_data(isl29043_dev.client, ALSIR_DT2);
	data = l_data | ((m_data & 0xf) << 8);
	isl29043_dev.als_data = data;
	input_report_abs(isl29043_dev.input_dev_als, ABS_PRESSURE, data);
	input_sync(isl29043_dev.input_dev_als);

	mmp_notifier(0, (u32 *)&data);

	schedule_delayed_work(&als_input_work,
			      msecs_to_jiffies(isl29043_dev.als_interval));
}
/*
 * TODO: will add gpio edge wakeup later
static void ps_wakeup_worker(struct work_struct *work)
{
	// hold 3s to prevent suspend
	pm_wakeup_event(&isl29043_dev.input_dev_ps->dev, 3000);
}

static void ps_edge_wakeup(int mfp, void *data)
{
	queue_work(system_wq, &ps_wakeup_work);
}
*/

#ifdef CONFIG_OF
static int isl29043_probe_dt(struct i2c_client *client)
{
	struct isl29043_platform_data *platform_data;
	struct device_node *np = client->dev.of_node;

	platform_data = kzalloc(sizeof(*platform_data), GFP_KERNEL);
	if (platform_data == NULL) {
		dev_err(&client->dev, "Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}
	client->dev.platform_data = platform_data;

	platform_data->irq =
		of_get_named_gpio(np, "irq-gpios", 0);
	if (platform_data->irq < 0) {
		dev_err(&client->dev, "of_get_named_gpio irq faild\n");
		return -EINVAL;
	}
	platform_data->wakup_gpio =
		of_get_named_gpio(np, "intersil,wakup-gpios", 0);
	if (platform_data->wakup_gpio < 0) {
		dev_err(&client->dev, "of_get_named_gpio wakup gpio faild\n");
		return -EINVAL;
	}

	return 0;
}
static struct of_device_id isl29043_dt_ids[] = {
	{ .compatible = "intersil,isl29043", },
	{}
};

#endif

static int isl29043_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int ret = 0;
	int retval = 0;

#ifdef CONFIG_OF
	retval = isl29043_probe_dt(client);
	if (retval == -ENOMEM) {
		dev_err(&client->dev,
		"%s: Failed to alloc mem for isl29043_platform_data\n", __func__);
		return retval;
	} else if (retval == -EINVAL) {
		kfree(client->dev.platform_data);
		dev_err(&client->dev,
			"%s: Probe device tree data failed\n", __func__);
		return retval;
	}

	isl29043_dev.pdata = client->dev.platform_data;
#else
	isl29043_dev.pdata = client->dev.platform_data;
	if (!isl29043_dev.pdata) {
		dev_err(&client->dev, "%s: No platform data found\n", __func__);
		return -EINVAL;
	}
#endif
	isl29043_dev.pdata = (struct isl29043_platform_data *)
		(client->dev.platform_data);
	isl29043_dev.client = client;

#ifdef CONFIG_OF
	isl29043_dev.avdd = regulator_get(&client->dev, "avdd");
#else
	isl29043_dev.avdd = regulator_get(NULL, isl29043_dev.pdata.avdd_name);
#endif

	if (IS_ERR(isl29043_dev.avdd)) {
		dev_err(&client->dev, "intersil sensor avdd power supply get failed\n");
		goto err_regulator;
	}
	regulator_set_voltage(isl29043_dev.avdd, 2800000, 2800000);
	if (regulator_enable(isl29043_dev.avdd)) {
		dev_err(&client->dev, "intersil sensor regulator enable failed\n");
		goto out;
	}

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE |
				     I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_WORD_DATA |
				     I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&client->dev, "client not i2c capable!\n");
		ret = -EIO;
		goto out;
	}

	if (i2c_smbus_read_byte(client) < 0) {
		dev_err(&client->dev, "i2c_smbus_read_byte error!\n");
		ret = -EIO;
		goto out;
	}

	INIT_WORK(&ps_int_work, ps_int_worker);
	INIT_DELAYED_WORK(&als_input_work, isl29043_report_als_value);
	mutex_init(&isl29043_dev.active_lock);

	isl29043_dev.input_dev_ps = input_allocate_device();
	if (!isl29043_dev.input_dev_ps) {
		ret = -ENOMEM;
		dev_err(&client->dev, "input device allocate for ps failed\n");
		goto out;
	}

	isl29043_dev.input_dev_als = input_allocate_device();
	if (!isl29043_dev.input_dev_als) {
		ret = -ENOMEM;
		dev_err(&client->dev, "input device allocate for als failed\n");
		goto err_alloc_als;
	}

	isl29043_dev.input_dev_ps->name = "ISL_proximity_sensor";
	isl29043_dev.input_dev_ps->phys = "Proximity-sensor/input0";
	isl29043_dev.input_dev_ps->id.bustype = BUS_I2C;
	isl29043_dev.input_dev_ps->id.vendor = 0;

	__set_bit(EV_ABS, isl29043_dev.input_dev_ps->evbit);
	__set_bit(ABS_DISTANCE, isl29043_dev.input_dev_ps->absbit);
	__set_bit(ABS_MISC, isl29043_dev.input_dev_ps->absbit);
	__set_bit(EV_SYN, isl29043_dev.input_dev_ps->evbit);
	input_set_abs_params(isl29043_dev.input_dev_ps,
			     ABS_DISTANCE, 0, 255, 0, 0);
	input_set_abs_params(isl29043_dev.input_dev_ps,
			     ABS_MISC, -100, 100, 0, 0);

	isl29043_dev.input_dev_als->name = "ISL_light_sensor";
	isl29043_dev.input_dev_als->phys = "Light-sensor/input0";
	isl29043_dev.input_dev_als->id.bustype = BUS_I2C;
	isl29043_dev.input_dev_als->id.vendor = 0;

	__set_bit(EV_ABS, isl29043_dev.input_dev_als->evbit);
	__set_bit(ABS_PRESSURE, isl29043_dev.input_dev_als->absbit);
	__set_bit(ABS_MISC, isl29043_dev.input_dev_als->absbit);
	__set_bit(EV_SYN, isl29043_dev.input_dev_als->evbit);
	input_set_abs_params(isl29043_dev.input_dev_als,
			     ABS_PRESSURE, 0, 65535, 0, 0);
	input_set_abs_params(isl29043_dev.input_dev_als,
			     ABS_MISC, -100, 100, 0, 0);

	ret = input_register_device(isl29043_dev.input_dev_ps);
	if (ret) {
		dev_err(&client->dev,
			"register ps input device failed, ret = %d\n", ret);
		goto err_register_ps;
	}

	ret = input_register_device(isl29043_dev.input_dev_als);
	if (ret) {
		dev_err(&client->dev,
			"register als input device failed, ret = %d\n", ret);
		goto err_register_als;
	}
	/* FIXME: add wakup func after gpio edge wakup framework is ready */
	/*
	if (isl29043_dev.pdata && isl29043_dev.pdata->wakup_gpio) {
		device_set_wakeup_capable(&isl29043_dev.input_dev_ps->dev, 1);
		INIT_WORK(&ps_wakeup_work, ps_wakeup_worker);
		edge = kzalloc(sizeof(struct gpio_edge_desc), GFP_KERNEL);
		edge->mfp = isl29043_dev.pdata->wakup_gpio;
		edge->handler = ps_edge_wakeup;
		isl29043_dev.edge_ps_desc = edge;
	}
	*/
	ret = request_irq(client->irq, isl29043_ps_irq_handler,
			  IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			  client->name, &client->dev);
	if (ret) {
		dev_err(&client->dev, "request irq failed\n");
		goto err_request_irq;
	}

	ret = sysfs_create_group(&isl29043_dev.input_dev_ps->dev.kobj,
				 &sysfs_ps_attribute_group);
	if (ret) {
		dev_err(&client->dev,
			"sysfs_create_group for ps failed, ret = %d\n", ret);
		goto err_attribute_group;
	}

	ret = sysfs_create_group(&isl29043_dev.input_dev_als->dev.kobj,
				 &sysfs_als_attribute_group);
	if (ret) {
		dev_err(&client->dev,
			"sysfs_create_group for als failed, ret = %d\n", ret);
		goto err_attribute_group;
	}
	disable_irq(client->irq);
	regulator_disable(isl29043_dev.avdd);
	goto out;

err_attribute_group:
err_request_irq:
	input_unregister_device(isl29043_dev.input_dev_als);
err_register_als:
	input_unregister_device(isl29043_dev.input_dev_ps);
err_register_ps:
	input_free_device(isl29043_dev.input_dev_als);
	isl29043_dev.input_dev_als = NULL;
err_alloc_als:
	input_free_device(isl29043_dev.input_dev_ps);
	isl29043_dev.input_dev_ps = NULL;
err_regulator:
	regulator_put(isl29043_dev.avdd);
out:
	return ret;
}

static int isl29043_remove(struct i2c_client *client)
{
	cancel_delayed_work_sync(&als_input_work);
	sysfs_remove_group(&isl29043_dev.input_dev_ps->dev.kobj,
			   &sysfs_ps_attribute_group);
	sysfs_remove_group(&isl29043_dev.input_dev_als->dev.kobj,
			   &sysfs_als_attribute_group);
	free_irq(client->irq, &client->dev);

	if (isl29043_dev.pdata->wakup_gpio) {
		kfree(isl29043_dev.edge_ps_desc);
		isl29043_dev.edge_ps_desc = NULL;
	}
	input_unregister_device(isl29043_dev.input_dev_als);
	input_unregister_device(isl29043_dev.input_dev_ps);
	input_free_device(isl29043_dev.input_dev_als);
	isl29043_dev.input_dev_als = NULL;
	input_free_device(isl29043_dev.input_dev_ps);
	isl29043_dev.input_dev_ps = NULL;
	isl29043_dev.client = NULL;

	return 0;
}

#ifdef CONFIG_PM
static int isl29043_suspend(struct device *dev)
{
	u8 val;

	mutex_lock(&isl29043_dev.active_lock);
	atomic_set(&suspend_flag, 1);
	if (isl29043_dev.ps_user_count > 0) {
		if (device_may_wakeup(&isl29043_dev.input_dev_ps->dev) &&
		    isl29043_dev.edge_ps_desc)
			/* FIXME: wait for gpio edge support later */
			; /* mmp_gpio_edge_add(isl29043_dev.edge_ps_desc); */
		else {
			val = i2c_smbus_read_byte_data(isl29043_dev.client,
						       CFG_REG);
			val &= ~PROX_EN;
			i2c_smbus_write_byte_data(isl29043_dev.client,
						  CFG_REG, val);
			regulator_disable(isl29043_dev.avdd);
			disable_irq(isl29043_dev.client->irq);
		}
	}

	if (isl29043_dev.als_user_count > 0) {
		mutex_unlock(&isl29043_dev.active_lock);
		cancel_delayed_work_sync(&als_input_work);
		mutex_lock(&isl29043_dev.active_lock);
		val = i2c_smbus_read_byte_data(isl29043_dev.client, CFG_REG);
		val &= ~ALS_EN;
		i2c_smbus_write_byte_data(isl29043_dev.client, CFG_REG, val);
		regulator_disable(isl29043_dev.avdd);
	}

	mutex_unlock(&isl29043_dev.active_lock);
	return 0;
}

static int isl29043_resume(struct device *dev)
{
	u8 val;

	struct i2c_client *client = to_i2c_client(dev);
	mutex_lock(&isl29043_dev.active_lock);
	if (isl29043_dev.ps_user_count > 0) {
		if (device_may_wakeup(&isl29043_dev.input_dev_ps->dev) &&
		    isl29043_dev.edge_ps_desc)
			/* FIXME: wait for gpio edge support later */
			; /* mmp_gpio_edge_del(isl29043_dev.edge_ps_desc); */
		else {
			if (regulator_enable(isl29043_dev.avdd)) {
				dev_err(&client->dev, "intersil sensor avdd power supply get failed\n");
				goto disable;
			}
			enable_irq(isl29043_dev.client->irq);
			val = i2c_smbus_read_byte_data(isl29043_dev.client,
						       CFG_REG);
			val |= PROX_EN;
			i2c_smbus_write_byte_data(isl29043_dev.client,
						  CFG_REG, val);
		}
	}

	if (isl29043_dev.als_user_count > 0) {
		if (regulator_enable(isl29043_dev.avdd)) {
			dev_err(&client->dev, "intersil sensor avdd power supply get failed\n");
			goto disable;
		}
		val = i2c_smbus_read_byte_data(isl29043_dev.client, CFG_REG);
		val |= ALS_EN;
		i2c_smbus_write_byte_data(isl29043_dev.client, CFG_REG, val);
		schedule_delayed_work(&als_input_work,
				      msecs_to_jiffies
				      (isl29043_dev.als_interval));
	}

	atomic_set(&suspend_flag, 0);
	mutex_unlock(&isl29043_dev.active_lock);
	return 0;
disable:
	return 0;

}
#endif

static const struct i2c_device_id isl29043_id[] = {
	{"isl29043", 0},
	{}
};

static SIMPLE_DEV_PM_OPS(isl29043_pm_ops, isl29043_suspend, isl29043_resume);
static struct i2c_driver isl29043_driver = {
	.driver = {
		.name = "intersil,isl29043",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(isl29043_dt_ids),
	#ifdef CONFIG_PM
		.pm = &isl29043_pm_ops,
	#endif
	},
	.probe = isl29043_probe,
	.remove = isl29043_remove,
	.id_table = isl29043_id
};

module_i2c_driver(isl29043_driver);

MODULE_DESCRIPTION("Intersil Low Power Ambient Light and Proximity Sensor");
MODULE_AUTHOR("Yuhua Guo <guoyh@marvell.com>");
MODULE_LICENSE("GPL");
