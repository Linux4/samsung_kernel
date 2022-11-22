/**
 *  Copyright (C) 2012 Maxim Integrated Products
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.

 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "max86900.h"
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#define MAX86900_I2C_RETRY_DELAY	10
#define MAX86900_I2C_MAX_RETRIES	5
#define MAX86900_COUNT_MAX		65535
#define SENSOR_MIN_ABS			0
#define SENSOR_MAX_ABS			65535
#define MAX86900_WHOAMI_REG		0xFF
#define MAX86900_WHOAMI			0x0B
#define MODULE_NAME_HRM			"hrm_sensor"
#define CHIP_NAME			"MAX86900"
#define VENDOR				"MAXIM"

/* I2C function */
static int max86900_write_reg(struct max86900_device_data *device,
	u8 reg_addr, u8 data)
{
	int err;
	int tries = 0;
	u8 buffer[2] = { reg_addr, data };
	struct i2c_msg msgs[] = {
		{
			.addr = device->client->addr,
			.flags = device->client->flags & I2C_M_TEN,
			.len = 2,
			.buf = buffer,
		},
	};

	do {
		mutex_lock(&device->i2clock);
		err = i2c_transfer(device->client->adapter, msgs, 1);
		mutex_unlock(&device->i2clock);
		if (err != 1)
			msleep_interruptible(MAX86900_I2C_RETRY_DELAY);

	} while ((err != 1) && (++tries < MAX86900_I2C_MAX_RETRIES));

	if (err != 1) {
		pr_err("%s -write transfer error\n", __func__);
		err = -EIO;
		return err;
	}

	return 0;
}

static int max86900_read_reg(struct max86900_device_data *device,
	u8 *buffer, int length)
{
	int err = -1;
	int tries = 0;		// # of attempts to read the device

	struct i2c_msg msgs[] = {
		{
			.addr = device->client->addr,
			.flags = device->client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buffer,
		},
		{
			.addr = device->client->addr,
			.flags = (device->client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = length,
			.buf = buffer,
		},
	};

	do {
		mutex_lock(&device->i2clock);
		err = i2c_transfer(device->client->adapter, msgs, 2);
		mutex_unlock(&device->i2clock);
		if (err != 2)
			msleep_interruptible(MAX86900_I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < MAX86900_I2C_MAX_RETRIES));

	if (err != 2) {
		pr_err("%s -read transfer error\n", __func__);
		err = -EIO;
	} else
		err = 0;

	return err;
}

/* Device Control */
static int max86900_init_device(struct max86900_device_data *device)
{
	int err;
	u8 recvData;

	err = max86900_write_reg(device, MAX86900_MODE_CONFIGURATION, 0x40);
	if (err != 0) {
		pr_err("%s - error sw shutdown device!\n", __func__);
		return -EIO;
	}

	//Interrupt Clear
	recvData = MAX86900_INTERRUPT_STATUS;
	if ((err = max86900_read_reg(device, &recvData, 1)) != 0) {
		pr_err("%s - max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}

	err = max86900_write_reg(device, MAX86900_MODE_CONFIGURATION, 0x83);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(device, MAX86900_INTERRUPT_ENABLE, 0x10);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_INTERRUPT_ENABLE!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(device, MAX86900_SPO2_CONFIGURATION, 0x47);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_SPO2_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(device, MAX86900_LED_CONFIGURATION, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_LED_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	pr_info("%s done\n", __func__);

	return 0;
}

static int max86900_enable(struct max86900_device_data *device)
{
	int err;
	device->led = 0;
	device->sample_cnt = 0;

	mutex_lock(&device->activelock);
	err = max86900_write_reg(device, MAX86900_LED_CONFIGURATION, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_LED_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(device, MAX86900_FIFO_WRITE_POINTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_FIFO_WRITE_POINTER!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(device, MAX86900_OVF_COUNTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_OVF_COUNTER!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(device, MAX86900_FIFO_READ_POINTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_FIFO_READ_POINTER!\n",
			__func__);
		return -EIO;
	}

	err = max86900_write_reg(device, MAX86900_MODE_CONFIGURATION, 0x03);
	if (err != 0) {
		printk("%s - error initializing MAX86900_MODE_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}
	enable_irq(device->irq);
	mutex_unlock(&device->activelock);
	return 0;
}

static int max86900_disable(struct max86900_device_data *device)
{
	u8 err;

	mutex_lock(&device->activelock);
	disable_irq(device->irq);

	err = max86900_write_reg(device, MAX86900_MODE_CONFIGURATION, 0x83);
	if (err != 0) {
		pr_err("%s - error initializing MAX86900_MODE_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}
	mutex_unlock(&device->activelock);

	return 0;
}

static int max86900_read_data(struct max86900_device_data *device, u16 *data)
{
	u8 err;
	u8 recvData[4] = { 0x00, };
	int i;

	if (device->sample_cnt == MAX86900_COUNT_MAX)
		device->sample_cnt = 0;

	recvData[0] = MAX86900_FIFO_DATA;
	if ((err = max86900_read_reg(device, recvData, 4)) != 0) {
		pr_err("%s max86900_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData[0]);
		return -EIO;
	}

	for (i = 0; i < 2; i++)	{
		data[i] = ((((u16)recvData[i * 2]) << 8 ) & 0xff00)
			| ((((u16)recvData[i * 2 + 1])) & 0x00ff);
	}

	data[2] = device->led;
	data[3] = device->sample_cnt;

	if ((device->sample_cnt % 1000) == 1)
		pr_info("%s - %u, %u, %u, %u\n", __func__,
			data[0], data[1], data[2], data[3]);

	if (device->sample_cnt++ > 100 && device->led == 0) {
		device->led = 1;
		err = max86900_write_reg(device, MAX86900_LED_CONFIGURATION,
			0x55);
		if (err != 0) {
			pr_err("%s - error initializing\
				MAX86900_LED_CONFIGURATION!\n",__func__);
			return -EIO;
		}
	}

	return 0;
}

void max86900_mode_enable(struct max86900_device_data *device, int onoff)
{
	int err;
	if (onoff) {
		if (device->sub_ldo4 != NULL) {
			regulator_set_voltage(device->vdd_1p8, 1800000, 1800000);
			regulator_enable(device->vdd_1p8);
			usleep_range(1000, 1100);
			err = max86900_init_device(device);
			if (err)
				pr_err("%s max86900_init device fail err = %d",
					__func__, err);
		}
		err = max86900_enable(device);
		if (err > 0)
			pr_err("max86900_enable err : %d\n", err);
		device->is_enable = 1;
	} else {
		err = max86900_disable(device);
		if (err > 0)
			pr_err("max86900_disable err : %d\n", err);
		if (device->sub_ldo4 != NULL)
			regulator_disable(device->vdd_1p8);
		device->is_enable = 0;
	}
	pr_info("%s - onoff = %d\n", __func__, onoff);
}

/* sysfs */
static ssize_t max86900_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct max86900_device_data *data = dev_get_drvdata(dev);

    return sprintf(buf, "%d\n", data->is_enable);
}

static ssize_t max86900_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct max86900_device_data *data = dev_get_drvdata(dev);
	int new_value;

	if (sysfs_streq(buf, "1"))
		new_value = 1;
	else if (sysfs_streq(buf, "0"))
		new_value = 0;
	else {
		pr_err("%s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	max86900_mode_enable(data, new_value);
	return count;
}

static ssize_t max86900_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lld\n", 10000000LL);
}

static ssize_t max86900_poll_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	pr_info("%s - max86900 hrm sensor delay was fixed as 10ms\n", __func__);
	return size;
}

static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
        max86900_enable_show, max86900_enable_store);
static DEVICE_ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP,
        max86900_poll_delay_show, max86900_poll_delay_store);

static struct attribute *hrm_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group hrm_attribute_group = {
	.attrs = hrm_sysfs_attrs,
};

/* test sysfs */
static ssize_t max86900_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_NAME);
}

static ssize_t max86900_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static DEVICE_ATTR(name, S_IRUGO, max86900_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, max86900_vendor_show, NULL);

static struct device_attribute *hrm_sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	NULL,
};

irqreturn_t max86900_irq_handler(int irq, void *device)
{
	int err;
	struct max86900_device_data *data = device;
	u16 raw_data[4] = {0x00, };

	err = max86900_read_data(device, raw_data);
	if (err > 0)
		pr_err("max86900_read_data err : %d\n", err);

	input_report_abs(data->hrm_input_dev, ABS_X, raw_data[0]);
	input_report_abs(data->hrm_input_dev, ABS_Y, raw_data[1]);
	input_report_abs(data->hrm_input_dev, ABS_Z, raw_data[2]);
	input_report_abs(data->hrm_input_dev, ABS_RX, raw_data[3]);
	input_sync(data->hrm_input_dev);

	return IRQ_HANDLED;
}

static int max86900_parse_dt(struct max86900_device_data *data,
	struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;

	if (dNode == NULL)
		return -ENODEV;

	data->hrm_int = of_get_named_gpio_flags(dNode,
		"max86900,hrm_int-gpio", 0, &flags);
	if (data->hrm_int < 0) {
		pr_err("%s - get hrm_int error\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_string(dNode, "max86900,sub_ldo4", &data->sub_ldo4) < 0)
		pr_err("%s - get sub_ldo4 error\n", __func__);

	return 0;
}

static int max86900_gpio_setup(struct max86900_device_data *data)
{
	int errorno = -EIO;

	errorno = gpio_request(data->hrm_int, "hrm_int");
	if (errorno) {
		pr_err("%s - failed to request hrm_int\n", __func__);
		return errorno;
	}

	errorno = gpio_direction_input(data->hrm_int);
	if (errorno) {
		pr_err("%s - failed to set hrm_int as input\n", __func__);
		goto err_gpio_direction_input;
	}

	data->irq = gpio_to_irq(data->hrm_int);

	errorno = request_threaded_irq(data->irq, NULL,
		max86900_irq_handler, IRQF_TRIGGER_FALLING,
		"hrm_sensor_irq", data);

	if (errorno < 0) {
		pr_err("%s - request_irq(%d) failed for gpio %d (%d)\n",
		       __func__, data->irq, data->hrm_int, errorno);
		errorno = -ENODEV;
		goto err_request_irq;
	}

	disable_irq(data->irq);
	goto done;
err_request_irq:
err_gpio_direction_input:
	gpio_free(data->hrm_int);
done:
	return errorno;
}

int max86900_probe(struct i2c_client *client, const struct i2c_device_id *id )
{
	int err = -ENODEV;
	u8 buffer;

	struct max86900_device_data *data;

	// check to make sure that the adapter supports I2C
	if (!i2c_check_functionality(client->adapter,I2C_FUNC_I2C)) {
		pr_err("%s - I2C_FUNC_I2C not supported\n", __func__);
		return -ENODEV;
	}

	// allocate some memory for the device
	data = kzalloc(sizeof(struct max86900_device_data), GFP_KERNEL);
	if (data == NULL) {
		pr_err("%s - couldn't allocate memory\n", __func__);
		return -ENOMEM;
	}

	data->client = client;
	i2c_set_clientdata(client, data);

	mutex_init(&data->i2clock);
	mutex_init(&data->activelock);

	data->dev = &client->dev;

	pr_info("%s - start \n", __func__);

	err = max86900_parse_dt(data, &client->dev);
	if (err < 0) {
		pr_err("[SENSOR] %s - of_node error\n", __func__);
		err = -ENODEV;
		goto err_of_node;
	}

	if (data->sub_ldo4 != NULL) {
		data->vdd_1p8 = regulator_get(&client->dev, data->sub_ldo4);
		if (IS_ERR(data->vdd_1p8)) {
			pr_err("%s - regulator_get fail\n", __func__);
			return -ENODEV;
		}
		regulator_set_voltage(data->vdd_1p8, 1800000, 1800000);
		regulator_enable(data->vdd_1p8);
	}


	buffer = MAX86900_WHOAMI_REG;
	err = max86900_read_reg(data, &buffer, 1);
	if (err) {
		pr_err("%s WHOAMI read fail\n", __func__);
		//err = -ENODEV;
		//goto err_of_node;
	}
	if (buffer != MAX86900_WHOAMI) {
		pr_err("%s WHOAMI value (0x%x)\n", __func__, buffer);
		//err = -ENODEV;
		//goto err_of_node;
	}

	err = max86900_gpio_setup(data);
	if (err) {
		pr_err("[SENSOR] %s - could not setup gpio\n", __func__);
		goto err_setup_gpio;
	}

	/* allocate input device */
	data->hrm_input_dev = input_allocate_device();
	if (!data->hrm_input_dev) {
		pr_err("%s - could not allocate input device\n",
			__func__);
		goto err_input_allocate_device;
	}

	input_set_drvdata(data->hrm_input_dev, data);
	data->hrm_input_dev->name = MODULE_NAME_HRM;
	input_set_capability(data->hrm_input_dev, EV_ABS, ABS_MISC);
	input_set_abs_params(data->hrm_input_dev, ABS_X,
		SENSOR_MIN_ABS, SENSOR_MAX_ABS, 0, 0);
	input_set_abs_params(data->hrm_input_dev, ABS_Y,
		SENSOR_MIN_ABS, SENSOR_MAX_ABS, 0, 0);
	input_set_abs_params(data->hrm_input_dev, ABS_Z,
		SENSOR_MIN_ABS, SENSOR_MAX_ABS, 0, 0);
	input_set_abs_params(data->hrm_input_dev, ABS_RX,
		SENSOR_MIN_ABS, SENSOR_MAX_ABS, 0, 0);

	err = input_register_device(data->hrm_input_dev);
	if (err < 0) {
		input_free_device(data->hrm_input_dev);
		pr_err("%s - could not register input device\n", __func__);
		goto err_input_register_device;
	}

	err = sensors_create_symlink(&data->hrm_input_dev->dev.kobj,
					data->hrm_input_dev->name);
	if (err < 0) {
		pr_err("%s - create_symlink error\n", __func__);
		goto err_sensors_create_symlink;
	}

	err = sysfs_create_group(&data->hrm_input_dev->dev.kobj,
				 &hrm_attribute_group);
	if (err) {
		pr_err("[SENSOR] %s - could not create sysfs group\n",
			__func__);
		goto err_sysfs_create_group;
	}

	/* set sysfs for hrm sensor */
	err = sensors_register(data->dev, data, hrm_sensor_attrs,
			"hrm_sensor");
	if (err) {
		pr_err("[SENSOR] %s - cound not register hrm_sensor(%d).\n",
			__func__, err);
		goto hrm_sensor_register_failed;
	}

	err = max86900_init_device(data);
	if (err) {
		pr_err("%s max86900_init device fail err = %d", __func__, err);
		goto max86900_init_device_failed;
	}

	err = dev_set_drvdata(data->dev, data);
	if (err) {
		pr_err("%s dev_set_drvdata fail err = %d", __func__, err);
		goto dev_set_drvdata_failed;
	}

	if (data->sub_ldo4 != NULL)
		regulator_disable(data->vdd_1p8);
	pr_info("%s success\n", __func__);
	goto done;

dev_set_drvdata_failed:
max86900_init_device_failed:
	sensors_unregister(data->dev, hrm_sensor_attrs);
hrm_sensor_register_failed:
err_sysfs_create_group:
	sensors_remove_symlink(&data->hrm_input_dev->dev.kobj,
			data->hrm_input_dev->name);
err_sensors_create_symlink:
	input_unregister_device(data->hrm_input_dev);
err_input_register_device:
err_input_allocate_device:
	gpio_free(data->hrm_int);
err_setup_gpio:
err_of_node:
	mutex_destroy(&data->i2clock);
	mutex_destroy(&data->activelock);
	kfree(data);
	pr_err("%s failed\n", __func__);
done:
	return err;
}

/**
 *  Remove function for this I2C driver.
 *
 *  @param struct i2c_client *client
 *		the associated I2C client for the device
 *
 *	@return
 *    0
 */
int max86900_remove(struct i2c_client *client)
{
	pr_info("%s\n", __func__);
	free_irq(client->irq, NULL);
	return 0;
}

static void max86900_shutdown(struct i2c_client *client)
{
	pr_info("%s\n", __func__);
}

static struct of_device_id max86900_match_table[] = {
	{ .compatible = "max86900",},
	{},
};

static const struct i2c_device_id max86900_device_id[] = {
	{ "max86900_match_table", 0 },
	{ }
};
// descriptor of the max86900 I2C driver
static struct i2c_driver max86900_i2c_driver =
{
	.driver = {
	    .name = CHIP_NAME,
	    .owner = THIS_MODULE,
	    .of_match_table = max86900_match_table,
	},
	.probe = max86900_probe,
	.shutdown = max86900_shutdown,
	.remove = max86900_remove,
	.id_table = max86900_device_id,
};

// initialization and exit functions
static int __init max86900_init(void)
{
	return i2c_add_driver(&max86900_i2c_driver);
}

static void __exit max86900_exit(void)
{
	i2c_del_driver(&max86900_i2c_driver);
}

module_init(max86900_init);
module_exit(max86900_exit);

MODULE_DESCRIPTION("max86900 Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("LGPL");
