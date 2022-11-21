/*
 * driver/staging/sec_irled
 *
 * SEC IrLED driver using AVOV MC96 IC for Smart Remote
 *
 * Copyright (C) 2012 Samsung Electronics
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/sec_irled.h>
#include <linux/spinlock.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "mc96_fw.h"

#define MC96_READ_LENGTH	8

#define LENG_OFFSET	0
#define FREQ_OFFSET	2
#define SIG_OFFSET		5
#define CSUM_LENG		2
#define MAX_SIZE		(SIG_OFFSET + CSUM_LENG + 2048)

#define I2C_RETRY_COUNT	5

#define MC96_POR_DELAY_MS	100
#define MC96_OFF_DELAY_MS	20

struct sec_irled_data {
	struct i2c_client *client;
	struct sec_irled_platform_data *pdata;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	bool busy_flag;
	spinlock_t lock;
	char signal[MAX_SIZE];
	int length;
	int count;
	int dev_id;
	int ir_freq;
	int ir_sum;
	int on_off;
	bool checksum_state;
	bool send_state;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sec_irled_early_suspend(struct early_suspend *h);
static void sec_irled_late_resume(struct early_suspend *h);
#endif

static int mc96_read_device_info(struct sec_irled_data *data)
{
	struct i2c_client *client = data->client;
	u8 buf_ir_test[8];
	int ret;
	unsigned int retry_count = I2C_RETRY_COUNT - 1;

	do {
		ret = i2c_master_recv(client, buf_ir_test, MC96_READ_LENGTH);
		if (ret < 0) {
			pr_err("irled: read device info error (%d)\n", ret);
			if (!retry_count--) {
				pr_err("irled: read error is not recovered\n");
				goto dev_info_err;
			}
		}
	} while (!ret);
	ret = data->dev_id =
		((buf_ir_test[2] & 0xFF) << 8 | (buf_ir_test[3] & 0xFF));

dev_info_err:
	return ret;
}

static bool mc96_check_boot_checksum(struct sec_irled_data *data)
{
	struct i2c_client *client = data->client;
	int i, ret;
	u8 buf_ir_test[8];
	int csum = 0;
	ret = i2c_master_recv(client, buf_ir_test, MC96_READ_LENGTH);
	if (ret < 0) {
		pr_err("irled: bootmode checksum error (%d)\n", ret);
		return 0;
	}

	ret = (buf_ir_test[6] & 0xFF) << 8 | (buf_ir_test[7] & 0xFF);

	for (i = 0; i < 6; i++)
		csum += buf_ir_test[i];

	return ret == csum;
}

static int mc96_fw_update(struct sec_irled_data *data)
{
	struct i2c_client *client = data->client;
	int i, j, ret;

	while (1) {
		spin_lock(&data->lock);
		if (!data->busy_flag)
			break;
		spin_unlock(&data->lock);
		msleep(100);
	};
	data->busy_flag = true;
	spin_unlock(&data->lock);

	data->pdata->ir_vdd_onoff(1);
	data->pdata->ir_wake_en(1);
	msleep(MC96_POR_DELAY_MS);

	ret = mc96_read_device_info(data);
	if (ret != FW_VERSION) {
		pr_info("irled: chip(%04X), bin(%04X), need update\n",
							ret, FW_VERSION);
		for (i = 0;i < I2C_RETRY_COUNT;i++) {
			data->pdata->ir_wake_en(0);
			data->pdata->ir_vdd_onoff(0);
			msleep(MC96_OFF_DELAY_MS);
			data->pdata->ir_vdd_onoff(1);
			msleep(MC96_POR_DELAY_MS);

			for (j = 0; j < FW_SIZE; j += FW_PACKET_SIZE) {
				ret = i2c_master_send(client, &FW_binary[j],
					(j + FW_PACKET_SIZE < FW_SIZE) ?
					FW_PACKET_SIZE : FW_SIZE - j);
				if (ret < 0) {
					pr_err(
				"irled: update error, count(%d), ret(%d)\n", j, ret);
					break;
				}
				msleep(30);
			}
			if (ret < 0)
				continue;

			if (mc96_check_boot_checksum(data)) {
				pr_info("irled: FW download complete\n");
				break;
			}
		}
		if (i == I2C_RETRY_COUNT) {
			ret = -EINVAL;
			goto err_update;
		}

		data->pdata->ir_vdd_onoff(0);
		msleep(MC96_OFF_DELAY_MS);
		data->pdata->ir_vdd_onoff(1);
		data->pdata->ir_wake_en(1);
		msleep(MC96_POR_DELAY_MS);

		ret = mc96_read_device_info(data);
		pr_info("irled: user mode, chip(%04X)\n", ret);

		data->pdata->ir_wake_en(0);
		data->pdata->ir_vdd_onoff(0);
		msleep(MC96_OFF_DELAY_MS);
		data->on_off = 0;
	} else {
		pr_info("irled: chip(%04X), bin(%04X)\n", ret, FW_VERSION);
		data->pdata->ir_wake_en(0);
		data->pdata->ir_vdd_onoff(0);
		msleep(MC96_OFF_DELAY_MS);
		data->on_off = 0;
	}

	spin_lock(&data->lock);
	data->busy_flag = false;
	spin_unlock(&data->lock);

	return 0;
err_read_device_info:
err_update:
	data->pdata->ir_wake_en(0);
	data->pdata->ir_vdd_onoff(0);
	msleep(MC96_OFF_DELAY_MS);
	data->on_off = 0;

	spin_lock(&data->lock);
	data->busy_flag = false;
	spin_unlock(&data->lock);

	return ret;
}

static void mc96_add_checksum_length(struct sec_irled_data *data)
{
	int i = 0, csum = 0;

	data->signal[LENG_OFFSET] = data->count >> 8;
	data->signal[LENG_OFFSET+1] = data->count & 0xFF;

	while (i < data->count)
		csum += data->signal[i++] & 0xFF;

	pr_debug("irled: checksum: %04X\n", csum);

	data->signal[data->count] = csum >> 8;
	data->signal[data->count + 1] = csum & 0xFF;

	data->count += CSUM_LENG;

	data->signal[data->count + 2] = 0;
	data->signal[data->count + 3] = 0;

	pr_debug("irled: length: %04X\n", data->count);
}

static void sec_irled_send(struct sec_irled_data *data)
{
	struct i2c_client *client = data->client;

	int ret;
	int emission_time;
	unsigned int retry_count = I2C_RETRY_COUNT - 1;

	data->checksum_state = false;
	data->send_state = false;

	do {
		ret = i2c_master_send(client, data->signal, data->count);
		if (ret < 0) {
			pr_err("irled: irled send error %d\n", ret);
			if (!retry_count--) {
				pr_err("irled: send error is not recovered\n");
				return;
			}
			data->pdata->ir_wake_en(0);
			data->pdata->ir_vdd_onoff(0);
			msleep(MC96_OFF_DELAY_MS);
			data->on_off = 0;

			data->pdata->ir_vdd_onoff(1);
			data->pdata->ir_wake_en(1);
			msleep(MC96_POR_DELAY_MS);
			data->on_off = 1;
		}
	} while (!ret);

	msleep(20);

	data->checksum_state = !data->pdata->ir_irq_state();

	emission_time =
		(1000 * data->ir_sum / data->ir_freq) + 50;
	if (emission_time > 0)
		msleep(emission_time);
	pr_debug("irled: emission_time = %d\n", emission_time);

	data->send_state = data->pdata->ir_irq_state();
}

static ssize_t remocon_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct sec_irled_data *data = dev_get_drvdata(dev);
	unsigned int _data;

	if (sscanf(buf++, "%u", &_data) == 1) {
		if (unlikely(!_data)) {
			pr_err("irled: invalid input\n");
			return -EINVAL;
		}
	} else {
		pr_err("irled: invalid write operation\n");
		return -EINVAL;
	}

	while (1) {
		spin_lock(&data->lock);
		if (!data->busy_flag)
			break;
		spin_unlock(&data->lock);
		msleep(100);
	};
	data->busy_flag = true;
	spin_unlock(&data->lock);

	data->ir_sum = 0;
	data->pdata->ir_vdd_onoff(1);
	data->pdata->ir_wake_en(1);
	msleep(MC96_POR_DELAY_MS);
	data->on_off = 1;

	data->ir_freq = _data;
	data->signal[FREQ_OFFSET] = (_data >> 16) & 0xFF;
	data->signal[FREQ_OFFSET+1] = (_data >> 8) & 0xFF;
	data->signal[FREQ_OFFSET+2] = _data & 0xFF;

	while (_data > 0) {
		buf++;
		_data /= 10;
	}

	for (data->count = SIG_OFFSET; data->count < MAX_SIZE;) {
		if (sscanf(buf++, "%u", &_data) == 1) {
			if (!_data)
				break;

			data->ir_sum += _data;
			data->signal[data->count++] = (_data >> 8) & 0xFF;
			data->signal[data->count++] = _data & 0xFF;

			while (_data > 0) {
				buf++;
				_data /= 10;
			}
		} else
			break;
	}

	mc96_add_checksum_length(data);
	sec_irled_send(data);

	data->pdata->ir_wake_en(0);
	data->pdata->ir_vdd_onoff(0);
	msleep(MC96_OFF_DELAY_MS);
	data->on_off = 0;

	spin_lock(&data->lock);
		data->busy_flag = false;
	spin_unlock(&data->lock);

	return size;
}

static ssize_t remocon_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_irled_data *data = dev_get_drvdata(dev);
	int i;
	char *bufp = buf;

	while (1) {
		spin_lock(&data->lock);
		if (!data->busy_flag)
			break;
		spin_unlock(&data->lock);
		msleep(100);
	};
	data->busy_flag = true;
	spin_unlock(&data->lock);

	if (data->count) {
		bufp += sprintf(bufp, "%u,",
			(data->signal[FREQ_OFFSET] & 0xFF) << 16
			| (data->signal[FREQ_OFFSET+1] & 0xFF) << 8
			| (data->signal[FREQ_OFFSET+2] & 0xFF));
		pr_debug("%u,",
			(data->signal[FREQ_OFFSET] & 0xFF) << 16
			| (data->signal[FREQ_OFFSET+1] & 0xFF) << 8
			| (data->signal[FREQ_OFFSET+2] & 0xFF));
		for (i = SIG_OFFSET; i < data->count; i += 2) {
			bufp += sprintf(bufp, "%u,",
				(data->signal[i] & 0xFF) << 8
				| (data->signal[i+1] & 0xFF));
			pr_debug("%u,",
				(data->signal[i] & 0xFF) << 8
				| (data->signal[i+1] & 0xFF));
		}
		bufp += sprintf(bufp, "\n");
	}

	spin_lock(&data->lock);
	data->busy_flag = false;
	spin_unlock(&data->lock);

	return strlen(buf);
}

static DEVICE_ATTR(ir_send, S_IRUGO | S_IWUSR | S_IWGRP,
				remocon_show, remocon_store);

static ssize_t remocon_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_irled_data *data = dev_get_drvdata(dev);

	while (1) {
		spin_lock(&data->lock);
		if (!data->busy_flag)
			break;
		spin_unlock(&data->lock);
		msleep(100);
	};
	data->busy_flag = true;
	spin_unlock(&data->lock);

	sprintf(buf, "%d\n", data->checksum_state & data->send_state);

	spin_lock(&data->lock);
	data->busy_flag = false;
	spin_unlock(&data->lock);

	return strlen(buf);
}

static DEVICE_ATTR(ir_send_result, S_IRUGO,
				remocon_state_show, NULL);

static ssize_t check_ir_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_irled_data *data = dev_get_drvdata(dev);
	int ret;

	while (1) {
		spin_lock(&data->lock);
		if (!data->busy_flag)
			break;
		spin_unlock(&data->lock);
		msleep(100);
	};
	data->busy_flag = true;
	spin_unlock(&data->lock);

	data->pdata->ir_vdd_onoff(1);
	data->pdata->ir_wake_en(1);
	msleep(MC96_POR_DELAY_MS);
	data->on_off = 1;

	ret = mc96_read_device_info(data);
	if (ret >= 0)
		pr_info("irled: chip(%04X)\n", ret);

	data->pdata->ir_wake_en(0);
	data->pdata->ir_vdd_onoff(0);
	msleep(MC96_OFF_DELAY_MS);
	data->on_off = 0;

	spin_lock(&data->lock);
	data->busy_flag = false;
	spin_unlock(&data->lock);

	return snprintf(buf, 4, "%d\n", ret);
}

static DEVICE_ATTR(check_ir, S_IRUGO, check_ir_show, NULL);


static int __devinit sec_irled_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct sec_irled_data *data;
	struct device *sec_irled_dev;
	int ret;

	pr_info("irled: probe\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	data = kzalloc(sizeof(struct sec_irled_data), GFP_KERNEL);
	if (unlikely(!data)) {
		pr_err("irled: failed to allocate memory\n");
		return -ENOMEM;
	}

	data->client = client;
	data->pdata = client->dev.platform_data;

	if (unlikely(!data->pdata->ir_vdd_onoff
		|| !data->pdata->ir_wake_en || !data->pdata->ir_irq_state)) {
		pr_err("irled: invalid platform_data\n");
		ret = -EINVAL;
		goto err_free_mem;
	}

	i2c_set_clientdata(client, data);
	spin_lock_init(&data->lock);

	ret = mc96_fw_update(data);
	if (ret < 0) {
		pr_err("irled: failed to update firmware\n");
		goto err_free_mem;
	}

	if (!sec_class) {
		pr_err("irled: sec_class is invalid\n");
		ret = -EINVAL;
		goto err_free_mem;
	}

	sec_irled_dev = device_create(sec_class, NULL,
		client->dev.devt, data, "sec_ir");

	if (IS_ERR(sec_irled_dev)) {
		pr_err("irled: failed to create sec_irled_dev device\n");
		ret = -ENOMEM;
		goto err_create_dev;
	}

	if (device_create_file(sec_irled_dev, &dev_attr_ir_send) < 0) {
		pr_err("irled: failed to create device file(%s)!\n",
				dev_attr_ir_send.attr.name);
		ret = -ENOMEM;
		goto err_create_dev_file;
	}

	if (device_create_file(sec_irled_dev, &dev_attr_ir_send_result) < 0) {
		pr_err("irled: failed to create device file(%s)!\n",
				dev_attr_ir_send_result.attr.name);
		ret = -ENOMEM;
		goto err_create_dev_file;
	}

	if (device_create_file(sec_irled_dev, &dev_attr_check_ir) < 0) {
		pr_err("irled: failed to create device file(%s)!\n",
				dev_attr_check_ir.attr.name);
		ret = -ENOMEM;
		goto err_create_dev_file;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = sec_irled_early_suspend;
	data->early_suspend.resume = sec_irled_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	return 0;

err_create_dev_file:
	device_destroy(sec_class, client->dev.devt);
err_create_dev:
	i2c_set_clientdata(client, NULL);
err_free_mem:
	kfree(data);
	return ret;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
static int sec_irled_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sec_irled_data *data = i2c_get_clientdata(client);

	while (1) {
		spin_lock(&data->lock);
		if (!data->busy_flag)
			break;
		spin_unlock(&data->lock);
		msleep(100);
	};
	data->busy_flag = true;
	spin_unlock(&data->lock);

	if (data->on_off) {
		data->pdata->ir_wake_en(0);
		data->pdata->ir_vdd_onoff(0);
		msleep(MC96_OFF_DELAY_MS);
		data->on_off = 0;
	}

	return 0;
}

static int sec_irled_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sec_irled_data *data = i2c_get_clientdata(client);

	spin_lock(&data->lock);
	data->busy_flag = false;
	spin_unlock(&data->lock);

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sec_irled_early_suspend(struct early_suspend *h)
{
	struct sec_irled_data *data;
	data = container_of(h, struct sec_irled_data, early_suspend);
	sec_irled_suspend(&data->client->dev);
}

static void sec_irled_late_resume(struct early_suspend *h)
{
	struct sec_irled_data *data;
	data = container_of(h, struct sec_irled_data, early_suspend);
	sec_irled_resume(&data->client->dev);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static const struct dev_pm_ops sec_irled_pm_ops = {
	.suspend	= sec_irled_suspend,
	.resume	= sec_irled_resume,
};
#endif

static int __devexit sec_irled_remove(struct i2c_client *client)
{
	struct sec_irled_data *data = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);
	kfree(data);
	return 0;
}

static const struct i2c_device_id mc96_id[] = {
	{"mc96", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, mc96_id);

static struct i2c_driver sec_irled_i2c_driver = {
	.driver = {
		.name = "mc96",
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
		.pm	= &sec_irled_pm_ops,
#endif
	},
	.probe = sec_irled_probe,
	.remove = __devexit_p(sec_irled_remove),
	.id_table = mc96_id,
};

static int __init sec_irled_init(void)
{
	return i2c_add_driver(&sec_irled_i2c_driver);
}
module_init(sec_irled_init);

static void __exit sec_irled_exit(void)
{
	i2c_del_driver(&sec_irled_i2c_driver);
}
module_exit(sec_irled_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SEC IrLED driver");
