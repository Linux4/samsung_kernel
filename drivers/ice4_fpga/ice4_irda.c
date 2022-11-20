/*
 * driver/ice4_fpga IRDA fpga driver
 *
 * Copyright (C) 2013 Samsung Electronics
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/platform_data/ice4_irda.h>

#define MAX_SIZE		2048
#define US_TO_PATTERN		1000000

enum {
	IRLED_OFF = 0,
	IRLED_ON = 1,
};

struct ice4_fpga_data {
	struct i2c_client	*client;
	struct mutex		signal_mutex;
	union {
		struct {
			unsigned char addr;
			/*
			 * signal_length includes the length(bytes) of
			 * carrier_freq and frequency data
			 */
			unsigned char signal_length[2];
			unsigned char carrier_freq[3];
			unsigned char data[MAX_SIZE - 6];
		} signal;
		unsigned char serialized_data[MAX_SIZE];
	} i2c_buffer;
	int signal_length;
	int dev_id;
	int carrier_freq;
	int ir_sum;
	bool last_send_ok;

	int gpio_irda_irq;
	int gpio_creset;
	int gpio_fpga_rst_n;
	int gpio_cdone;

	int (*irled_on)(int);
};

static int ice4_irda_check_cdone(struct ice4_fpga_data *data)
{
	if (!data->gpio_cdone)
		return 1;

	/* Device in Operation when CDONE='1'; Device Failed when CDONE='0'. */
	if (gpio_get_value(data->gpio_cdone) != 1) {
		pr_debug("CDONE_FAIL %d\n", gpio_get_value(data->gpio_cdone));

		return 0;
	}

	return 1;
}

/* When IR test does not work, we need to check some gpios' status */
static void print_fpga_gpio_status(struct ice4_fpga_data *data)
{
	if (data->gpio_cdone)
		pr_info("%s : CDONE    : %d\n", __func__,
				gpio_get_value(data->gpio_cdone));
	pr_info("%s : RST_N    : %d\n", __func__,
			gpio_get_value(data->gpio_fpga_rst_n));
	pr_info("%s : CRESET_B : %d\n", __func__,
			gpio_get_value(data->gpio_creset));
}

static int ice4_send_signal(struct ice4_fpga_data *data)
{
	struct i2c_client *client = data->client;
	/* 3 = 1 (for addr) + 2 (for signal_length) */
	int i2c_entire_size = data->signal_length + 3;
	int retry;
	int ret;
	bool send_checksum_ok = false;
	int emission_time;

	data->i2c_buffer.signal.addr = 0x00;

	data->i2c_buffer.signal.signal_length[0] = (data->signal_length >> 8)
							& 0xFF;
	data->i2c_buffer.signal.signal_length[1] = data->signal_length & 0xFF;
#if defined(DEBUG)
	print_hex_dump(KERN_INFO, "signal.data: ", DUMP_PREFIX_NONE, 16, 1,
			data->i2c_buffer.serialized_data, i2c_entire_size,
			false);
#endif
	if (data->irled_on)
		data->irled_on(IRLED_ON);

	for (retry = 2; retry; retry--) {
		ret = i2c_master_send(client,
				data->i2c_buffer.serialized_data,
				i2c_entire_size);

		if (ret == i2c_entire_size)
			break;

		if (ret < 0) {
			dev_err(&client->dev, "%s: i2c send failed. Retry %d\n",
					__func__, retry);
			print_fpga_gpio_status(data);
		}
	}

	if (ret < 0) {
		data->last_send_ok = false;

		return ret;
	}

	msleep(10);

	for (retry = 10; retry; msleep(1), retry--) {
		if (!gpio_get_value(data->gpio_irda_irq)) {
			send_checksum_ok = true;
			break;
		}
		dev_dbg(&client->dev, "%s: Checking checksum. Retry %d\n",
				__func__, retry);
	}

	if (!data->carrier_freq)
		return -EINVAL;

	emission_time = (1000 * data->ir_sum / data->carrier_freq);
	if (emission_time > 0)
		msleep(emission_time);

	for (retry = 5; retry; retry--) {
		if (gpio_get_value(data->gpio_irda_irq))
			break;

		mdelay(10);
	}

	if (gpio_get_value(data->gpio_irda_irq)) {
		if (send_checksum_ok) {
			data->last_send_ok = true;
			ret = 0;
		}
	} else {
		pr_err("%s: Sending IR NG!\n", __func__);
		data->last_send_ok = false;

		ret = -EIO;
	}

	if (data->irled_on)
		data->irled_on(IRLED_OFF);

	data->carrier_freq = 0;
	data->ir_sum = 0;

	return 0;
}

static ssize_t ir_send_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	unsigned int value,_tdata;
	char *string, *pstring;
	int signal_length = 0;
	int carrier_freq = 0;
	int ir_sum = 0;
	int ret = 0;
	int converting_factor = 1;

	if (!mutex_trylock(&data->signal_mutex))
		return -EBUSY;

	string = kmalloc(size, GFP_KERNEL);
	strlcpy(string, buf, size);
	pstring = strim(string);

	while (pstring) {
		char *curr_string;

		if (pstring - string > size)
			goto out;

		curr_string = strsep(&pstring, " ,");

		if (*curr_string == '\0')
			continue;

		ret = kstrtouint(curr_string, 0, &value);

		if (value == 0)	/* for backward compatiblity */
			break;

		if (ret < 0) {
			dev_err(dev, "Error: Invalid argument. Could not convert.\n");
			goto out;
		}

		if (carrier_freq == 0) {
			data->carrier_freq = carrier_freq = value;
			converting_factor = US_TO_PATTERN / data->carrier_freq;
			data->i2c_buffer.signal.carrier_freq[0]
				= (value >> 16) & 0xFF;
			data->i2c_buffer.signal.carrier_freq[1]
				= (value >> 8) & 0xFF;
			data->i2c_buffer.signal.carrier_freq[2]
				= value & 0xFF;
		} else {
			_tdata = value / converting_factor;
			ir_sum += _tdata;
			data->i2c_buffer.signal.data[signal_length++]
				= (_tdata >> 8) & 0xFF;
			data->i2c_buffer.signal.data[signal_length++]
				= _tdata & 0xFF;
		}
	}
	data->signal_length = signal_length + 3; /* sizeof(carrier_freq) */
	data->ir_sum = ir_sum;

	ret = ice4_send_signal(data);
out:
	kfree(string);
	mutex_unlock(&data->signal_mutex);

	if (ret < 0)
		return ret;

	return size;
}

static ssize_t ir_send_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	int i2c_entire_size = data->signal_length + 3;
	int i;
	int len = 0;
	unsigned char *p = data->i2c_buffer.serialized_data;

	if (!mutex_trylock(&data->signal_mutex))
		return -EBUSY;

	for (i = 1; i <= i2c_entire_size; i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%02X", *p++);

		if (i & 0xF)
			len += scnprintf(buf + len, PAGE_SIZE - len, " ");
		else
			len += scnprintf(buf + len, PAGE_SIZE - len, "\n");

	}
	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
	mutex_unlock(&data->signal_mutex);

	return len;
}

static DEVICE_ATTR(ir_send, 0664, ir_send_show, ir_send_store);
/* sysfs node ir_send_result */
static ssize_t ir_send_result_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);

	if (data->last_send_ok)
		return sprintf(buf, "1\n");

	return sprintf(buf, "0\n");
}

static DEVICE_ATTR(ir_send_result, 0664, ir_send_result_show, NULL);

static struct attribute *sec_ir_attributes[] = {
	&dev_attr_ir_send.attr,
	&dev_attr_ir_send_result.attr,
	NULL,
};

static struct attribute_group sec_ir_attr_group = {
	.attrs = sec_ir_attributes,
};

static int __devinit ice4_irda_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct ice4_fpga_data *data;
	struct device *ice4_irda_dev;
	struct ice4_irda_platform_data *pdata;

	pr_debug("%s: probe!\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	data = kzalloc(sizeof(struct ice4_fpga_data), GFP_KERNEL);
	if (NULL == data) {
		pr_err("Failed to data allocate %s\n", __func__);
		goto alloc_fail;
	}

	if (!client->dev.platform_data) {
		pr_err("%s: platform data not found\n", __func__);
		goto err_platdata;
	}

	pdata = client->dev.platform_data;
	data->gpio_irda_irq = pdata->gpio_irda_irq;
	data->gpio_fpga_rst_n = pdata->gpio_fpga_rst_n;
	data->gpio_creset = pdata->gpio_creset;
	data->gpio_cdone = pdata->gpio_cdone;
	data->irled_on = pdata->irled_on;

	if (!data->gpio_irda_irq || !data->gpio_fpga_rst_n ||
			!data->gpio_creset) {
		pr_err("%s: platform data was not filled\n", __func__);
		goto err_platdata;
	}

	if (data->irled_on)
		data->irled_on(IRLED_OFF);

	if (ice4_irda_check_cdone(data))
		pr_debug("FPGA FW is loaded!\n");
	else
		pr_debug("FPGA FW is NOT loaded!\n");

	pr_info("Irda driver using iCE40, Lattice semiconductor\n");
	gpio_set_value(data->gpio_fpga_rst_n, GPIO_LEVEL_HIGH);

	data->client = client;
	mutex_init(&data->signal_mutex);

	i2c_set_clientdata(client, data);

	ice4_irda_dev = device_create(sec_class, NULL, 0, data, "sec_ir");
	if (IS_ERR(ice4_irda_dev))
		pr_err("Failed to create ice4_irda_dev device in sec_ir\n");

	if (sysfs_create_group(&ice4_irda_dev->kobj, &sec_ir_attr_group) < 0)
		pr_err("Failed to create sysfs group for samsung ir!\n");

	pr_debug("%s: probe complete\n", __func__);

alloc_fail:
	return 0;

err_platdata:
	kfree(data);

	return -EINVAL;
}

static int __devexit ice4_irda_remove(struct i2c_client *client)
{
	struct ice4_fpga_data *data = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);
	kfree(data);

	return 0;
}

#ifdef CONFIG_PM
static int ice4_irda_suspend(struct device *dev)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);

	if (data->irled_on)
		data->irled_on(IRLED_OFF);

	gpio_set_value(data->gpio_fpga_rst_n, GPIO_LEVEL_LOW);

	return 0;
}

static int ice4_irda_resume(struct device *dev)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);

	gpio_set_value(data->gpio_fpga_rst_n, GPIO_LEVEL_HIGH);

	return 0;
}

static const struct dev_pm_ops ice4_fpga_pm_ops = {
	.suspend	= ice4_irda_suspend,
	.resume		= ice4_irda_resume,
};
#endif

static const struct i2c_device_id ice4_irda_id[] = {
	{ "ice4_irda", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ice4_irda_id);

static struct i2c_driver ice4_irda_i2c_driver = {
	.driver = {
		.name	= "ice4_irda",
#ifdef CONFIG_PM
		.pm	= &ice4_fpga_pm_ops,
#endif
	},
	.probe = ice4_irda_probe,
	.remove = __devexit_p(ice4_irda_remove),
	.id_table = ice4_irda_id,
};

static int __init ice4_irda_init(void)
{
	i2c_add_driver(&ice4_irda_i2c_driver);

	return 0;
}

static void __exit ice4_irda_exit(void)
{
	i2c_del_driver(&ice4_irda_i2c_driver);
}

module_init(ice4_irda_init);
module_exit(ice4_irda_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SEC IRDA driver using ice4 fpga");
