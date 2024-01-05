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

#define IRDA_TEST_CODE_SIZE	141
#define IRDA_TEST_CODE_ADDR	0x00
#define MAX_SIZE		2048
#define READ_LENGTH		8

struct ice4_fpga_data {
	struct i2c_client	*client;
	struct mutex		mutex;
	struct {
		unsigned char addr;
		unsigned char data[MAX_SIZE];
	} i2c_block_transfer;
	int count;
	int dev_id;
	int ir_freq;
	int ir_sum;

	int gpio_irda_irq;
	int gpio_creset;
	int gpio_fpga_rst_n;
	int gpio_cdone;
};

static int ack_number;
static int count_number;

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

/* sysfs node ir_send */
static void ir_remocon_work(struct ice4_fpga_data *data, int count)
{
	struct i2c_client *client = data->client;

	int buf_size = count + 2;
	int ret;
	int emission_time;
	int ack_pin_onoff;
	int retry_count = 0;

	data->i2c_block_transfer.addr = 0x00;

	data->i2c_block_transfer.data[0] = count >> 8;
	data->i2c_block_transfer.data[1] = count & 0xff;

	if (count_number >= 100)
		count_number = 0;

	count_number++;

	pr_info("%s: total buf_size: %d\n", __func__, buf_size);

	mutex_lock(&data->mutex);

	buf_size++;
	ret = i2c_master_send(client,
		(unsigned char *) &(data->i2c_block_transfer), buf_size);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err1 %d\n", __func__, ret);
		ret = i2c_master_send(client,
		(unsigned char *) &(data->i2c_block_transfer), buf_size);
		if (ret < 0) {
			dev_err(&client->dev, "%s: err2 %d\n", __func__, ret);
			print_fpga_gpio_status(data);
		}
	}

	mdelay(10);

	ack_pin_onoff = 0;

	if (gpio_get_value(data->gpio_irda_irq)) {
		pr_info("%s : %d Checksum NG!\n", __func__, count_number);
		ack_pin_onoff = 1;
	} else {
		pr_info("%s : %d Checksum OK!\n", __func__, count_number);
		ack_pin_onoff = 2;
	}
	ack_number = ack_pin_onoff;

	mutex_unlock(&data->mutex);

	data->count = 2;

	emission_time = \
		(1000 * (data->ir_sum) / (data->ir_freq));
	if (emission_time > 0)
		msleep(emission_time);

	pr_info("%s: emission_time = %d\n", __func__, emission_time);

	while (!gpio_get_value(data->gpio_irda_irq)) {
		mdelay(10);
		pr_info("%s : %d try to check IRDA_IRQ\n",
					__func__, retry_count);
		retry_count++;

		if (retry_count > 5)
			break;
	}

	if (gpio_get_value(data->gpio_irda_irq)) {
		pr_info("%s : %d Sending IR OK!\n", __func__, count_number);
		ack_pin_onoff = 4;
	} else {
		pr_info("%s : %d Sending IR NG!\n", __func__, count_number);
		ack_pin_onoff = 2;
	}

	ack_number += ack_pin_onoff;

	data->ir_freq = 0;
	data->ir_sum = 0;
}

static ssize_t remocon_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	unsigned int value;
	int count, i;

	pr_info("%s : ir_send called\n", __func__);

	for (i = 0; i < MAX_SIZE; i++) {
		if (sscanf(buf++, "%u", &value) == 1) {
			if (value == 0 || buf == '\0')
				break;

			if (data->count == 2) {
				data->ir_freq = value;
				data->i2c_block_transfer.data[2] = value >> 16;
				data->i2c_block_transfer.data[3]
							= (value >> 8) & 0xFF;
				data->i2c_block_transfer.data[4] = value & 0xFF;

				data->count += 3;
			} else {
				data->ir_sum += value;
				count = data->count;
				data->i2c_block_transfer.data[count]
								= value >> 8;
				data->i2c_block_transfer.data[count+1]
								= value & 0xFF;
				data->count += 2;
			}

			while (value > 0) {
				buf++;
				value /= 10;
			}
		} else {
			break;
		}
	}

	ir_remocon_work(data, data->count);

	return size;
}

static ssize_t remocon_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	int i;
	char *bufp = buf;

	for (i = 5; i < MAX_SIZE - 1; i++) {
		if (data->i2c_block_transfer.data[i] == 0
			&& data->i2c_block_transfer.data[i+1] == 0)
			break;
		else
			bufp += sprintf(bufp, "%u,",
					data->i2c_block_transfer.data[i]);
	}
	return strlen(buf);
}

static DEVICE_ATTR(ir_send, 0664, remocon_show, remocon_store);
/* sysfs node ir_send_result */
static ssize_t remocon_ack(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	pr_info("%s : ack_number = %d\n", __func__, ack_number);

	if (ack_number == 6)
		return sprintf(buf, "1\n");

	return sprintf(buf, "0\n");
}

static DEVICE_ATTR(ir_send_result, 0664, remocon_ack, NULL);

static int irda_read_device_info(struct ice4_fpga_data *data)
{
	struct i2c_client *client = data->client;
	u8 buf_ir_test[8];
	int ret;

	pr_info("%s: called\n", __func__);

	ret = i2c_master_recv(client, buf_ir_test, READ_LENGTH);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	pr_info("%s: buf_ir dev_id: 0x%02x, 0x%02x\n", __func__,
			buf_ir_test[2], buf_ir_test[3]);
	ret = data->dev_id = (buf_ir_test[2] << 8 | buf_ir_test[3]);

	return ret;
}

/* sysfs node check_ir */
static ssize_t check_ir_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	int ret;

	ret = irda_read_device_info(data);

	return snprintf(buf, 4, "%d\n", ret);
}

static DEVICE_ATTR(check_ir, 0664, check_ir_show, NULL);

static ssize_t toggle_rst_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	static int high;

	pr_info("%s: GPIO_FPGA_RST_N(%d) will be %d\n", __func__,
			data->gpio_fpga_rst_n, high);
	gpio_set_value(data->gpio_fpga_rst_n, high);

	high = !high;

	return size;
}

static DEVICE_ATTR(toggle_rst, 0664, NULL, toggle_rst_store);

/* sysfs node irda_test */
static ssize_t irda_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, i;
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	struct {
			unsigned char addr;
			unsigned char data[IRDA_TEST_CODE_SIZE];
	} i2c_block_transfer;
	unsigned char BSR_data[IRDA_TEST_CODE_SIZE] = {
		0x00, 0x8D, 0x00, 0x96, 0x00, 0x01, 0x50, 0x00,
		0xA8, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F
	};

	if (data->gpio_cdone) {
		if (gpio_get_value(data->gpio_cdone) != 1) {
			pr_err("%s: cdone fail !!\n", __func__);
			return 1;
		}
	}

	pr_debug("IRDA test code start\n");

	/* make data for sending */
	for (i = 0; i < IRDA_TEST_CODE_SIZE; i++)
		i2c_block_transfer.data[i] = BSR_data[i];

	/* sending data by I2C */
	i2c_block_transfer.addr = IRDA_TEST_CODE_ADDR;
	ret = i2c_master_send(client, (unsigned char *) &i2c_block_transfer,
			IRDA_TEST_CODE_SIZE);
	if (ret < 0) {
		pr_err("%s: err1 %d\n", __func__, ret);
		ret = i2c_master_send(client,
		(unsigned char *) &i2c_block_transfer, IRDA_TEST_CODE_SIZE);
		if (ret < 0)
			pr_err("%s: err2 %d\n", __func__, ret);
	}

	return size;
}

static ssize_t irda_test_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return strlen(buf);
}

static DEVICE_ATTR(irda_test, 0664, irda_test_show, irda_test_store);

static struct attribute *sec_ir_attributes[] = {
	&dev_attr_ir_send.attr,
	&dev_attr_ir_send_result.attr,
	&dev_attr_check_ir.attr,
	&dev_attr_irda_test.attr,
	&dev_attr_toggle_rst.attr,
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

	if (!data->gpio_irda_irq || !data->gpio_fpga_rst_n ||
			!data->gpio_creset) {
		pr_err("%s: platform data was not filled\n", __func__);
		goto err_platdata;
	}

	if (ice4_irda_check_cdone(data))
		pr_debug("FPGA FW is loaded!\n");
	else
		pr_debug("FPGA FW is NOT loaded!\n");

	gpio_set_value(data->gpio_fpga_rst_n, GPIO_LEVEL_HIGH);

	data->client = client;
	mutex_init(&data->mutex);
	data->count = 2;

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
