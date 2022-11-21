/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/version.h>

#include <linux/semaphore.h>
#include <linux/completion.h>

#include <linux/ioctl.h>
#include <linux/miscdevice.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

struct nqx_platform_data {
	unsigned int mode_sel;
	unsigned int tps_en;
	unsigned int config_reg;
	unsigned int vout_floor;
	unsigned int vout_roof;
	unsigned int ilimit_set;
};

static struct of_device_id tps61280a_power_table[] = {
	{.compatible = "ti,power-tps61280a"},
	{}
};

MODULE_DEVICE_TABLE(of, tps61280a_power_table);

#define TPS_CONFIG_REG			(0x01)
#define TPS_VFLOORSET_REG		(0x02)
#define TPS_VROOFSET_REG		(0x03)
#define TPS_ILIMITSET_REG		(0x04)
#define TPS_STATUS_REG			(0x05)
#define TPS_MAX_REG			TPS_STATUS_REG

struct nqx_dev {
	struct	i2c_client	*client;
	struct	miscdevice	nqx_device;
	struct	mutex		hwres_mutex;
	unsigned int		mode_sel;
	bool			tps_power_on;
	struct nqx_platform_data *pdata;
	u8 reg_data[4];
};

static int tps61280a_i2c_read(struct i2c_client *client, char *writebuf,
			   int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = 0,
				 .len = writelen,
				 .buf = writebuf,
			 },
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}

static int tps61280a_i2c_write(struct i2c_client *client, char *writebuf,
			    int writelen)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
		 },
	};
	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s: i2c write error.\n", __func__);

	return ret;
}

static int tps61280a_write_reg(struct i2c_client *client, u8 addr, const u8 val)
{
	struct nqx_dev *nqx_dev = i2c_get_clientdata(client);
	u8 buf[2] = {0};

	buf[0] = addr;
	buf[1] = val;
	nqx_dev->reg_data[addr - 1] = val;
	return tps61280a_i2c_write(client, buf, sizeof(buf));
}

static int tps61280a_read_reg(struct i2c_client *client, u8 addr, u8 *val)
{
	return tps61280a_i2c_read(client, &addr, 1, val, 1);
}

static void tps61280a_power_control(struct i2c_client *client, bool use_platform, bool enable)
{
	struct nqx_dev *nqx_dev = i2c_get_clientdata(client);
	struct nqx_platform_data *pdata = nqx_dev->pdata;
	int icnt = 0;

	if (enable == nqx_dev->tps_power_on) {
		pr_debug("%s: already in state(%d).\n", __func__, enable);
		return;
	}

	if (enable) {
		gpio_set_value(nqx_dev->mode_sel, 1);
		if (use_platform) {
			tps61280a_write_reg(client, TPS_CONFIG_REG, pdata->config_reg);
			tps61280a_write_reg(client, TPS_VFLOORSET_REG, pdata->vout_floor);
			tps61280a_write_reg(client, TPS_VROOFSET_REG, pdata->vout_roof);
			tps61280a_write_reg(client, TPS_ILIMITSET_REG, pdata->ilimit_set);
		} else {
			for (icnt = 1; icnt < TPS_MAX_REG; icnt++)
				tps61280a_write_reg(client, icnt, nqx_dev->reg_data[icnt - 1]);
		}
	} else {
		gpio_set_value(nqx_dev->mode_sel, 0);
	}

	nqx_dev->tps_power_on = enable;
	pr_info("%s:use_platform(%d), tps_power_on(%d).\n", __func__, use_platform, enable);
}

static int tps61280a_hw_check(struct i2c_client *client, unsigned int enable_gpio)
{
	int ret = 0;
	u8 value = 0;

	ret = tps61280a_read_reg(client, TPS_STATUS_REG, &value);
	if (ret < 0) {
		dev_err(&client->dev,
		"%s: tps61280a power supply isn't present, ret(%d)\n", __func__, ret);
	} else {
		dev_info(&client->dev,
		"%s: tps61280a TPS_STATUS_REG(%d) = 0x%x.\n", __func__, TPS_STATUS_REG, value);
	}

	return ret;
}

static ssize_t tps61280a_store_reg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int data[2] = {0};
	int icnt = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct nqx_dev *nqx_dev = i2c_get_clientdata(client);

	if (2 != sscanf(buf, "0x%x,0x%x.", &data[0], &data[1]))
		pr_err("Not able to read split values, buf[%s].\n", buf);

	for (icnt = 0; icnt < 2; icnt++)
		pr_info("%s:data[%d] = 0x%x.\n", __func__, icnt, data[icnt]);

	if ((data[0] <= 0) || (data[1] < 0) || (data[0] > TPS_ILIMITSET_REG) ||
		(data[1] > 0xFF)) {
		pr_err("%s: input data err, buf[%s].\n", __func__, buf);
	} else {
		mutex_lock(&nqx_dev->hwres_mutex);
		tps61280a_write_reg(client, data[0], data[1]);
		mutex_unlock(&nqx_dev->hwres_mutex);
	}

	return len;
}

static ssize_t tps61280a_show_reg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int icnt = 1;
	u8 value = 0;
	int char_cnt = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct nqx_dev *nqx_dev = i2c_get_clientdata(client);

	/* get the write cnt, and caculate the buf_size */
	char_cnt += snprintf(buf + char_cnt, PAGE_SIZE -char_cnt,
		"tps power reg: ");

	for (icnt = 1; icnt <= TPS_MAX_REG; icnt++) {
		mutex_lock(&nqx_dev->hwres_mutex);
		ret = tps61280a_read_reg(client, icnt, &value);
		mutex_unlock(&nqx_dev->hwres_mutex);
		if (ret < 0) {
			dev_err(&client->dev, "%s: read reg error, ret(%zd)\n", __func__, ret);

			char_cnt += snprintf(buf + char_cnt, PAGE_SIZE -char_cnt,
				"read reg err(%zd): ", ret);
			ret = char_cnt;
			return ret;
		}

		char_cnt += snprintf(buf + char_cnt, PAGE_SIZE -char_cnt,
			"r0x%02x=0x%02x,", icnt, value);
	}
	char_cnt += snprintf(buf + char_cnt, PAGE_SIZE -char_cnt, " end.");

	/* the ret value will determine the out buf size we can see */
	ret = char_cnt;
	return ret;
}

static ssize_t tps61280a_show_power_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nqx_dev *nqx_dev = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&nqx_dev->hwres_mutex);
	ret = scnprintf(buf, PAGE_SIZE, "tps_power_mode=%s\n",
		nqx_dev->tps_power_on ? "on" : "off");
	mutex_unlock(&nqx_dev->hwres_mutex);

	return ret;
}

static ssize_t tps61280a_store_power_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nqx_dev *nqx_dev = i2c_get_clientdata(client);
	u32 power_on = 0; 

	if (kstrtouint(buf, 0, &power_on))
		pr_err("kstrtouint buf error!\n");
	else {
		mutex_lock(&nqx_dev->hwres_mutex);
		tps61280a_power_control(client, false, power_on);
		mutex_unlock(&nqx_dev->hwres_mutex);
	}

	return len;
}

static DEVICE_ATTR(reg_ctrl, S_IRUGO | S_IWUSR,
	tps61280a_show_reg, tps61280a_store_reg);
static DEVICE_ATTR(pwr_ctrl, S_IRUGO | S_IWUSR,
	tps61280a_show_power_mode, tps61280a_store_power_mode);

static struct attribute *tps61280a_power_attrs[] = {
	&dev_attr_reg_ctrl.attr,
	&dev_attr_pwr_ctrl.attr,
	NULL,
};

static struct attribute_group tps61280a_power_attr_group = {
	.attrs = tps61280a_power_attrs,
};

static int tps61280a_power_create_sysfs(struct device *dev)
{
	int rc;

	rc = sysfs_create_group(&dev->kobj, &tps61280a_power_attr_group);
	if (rc)
		pr_err("sysfs group creation failed, rc=%d\n", rc);
	return rc;
}

static void tps61280a_power_remove_sysfs(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &tps61280a_power_attr_group);
}

static int tps_power_parse_dt(struct device *dev, struct nqx_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int rc = 0;
	uint32_t tmp;

	pdata->mode_sel = of_get_named_gpio(np, "ti,mode-sel", 0);
	if ((!gpio_is_valid(pdata->mode_sel))) {
		pr_err("%s: mode_sel(%d) gpio is invalid.\n", __func__, pdata->mode_sel);
		return -EINVAL;
	}

	pdata->tps_en = of_get_named_gpio(np, "ti,tps-en", 0);
	if ((!gpio_is_valid(pdata->tps_en))) {
		pr_err("%s: tps_en(%d) gpio is invalid.\n", __func__, pdata->tps_en);
		return -EINVAL;
	}


	rc = of_property_read_u32(np, "ti,config-reg", &tmp);
	pdata->config_reg = (!rc ? tmp : 0x0D);

	rc = of_property_read_u32(np, "ti,vout-floor-set", &tmp);
	pdata->vout_floor = (!rc ? tmp : 0x0D);

	rc = of_property_read_u32(np, "ti,vout-roof-set", &tmp);
	pdata->vout_roof = (!rc ? tmp : 0x19);

	rc = of_property_read_u32(np, "ti,ilimit-set", &tmp);
	pdata->ilimit_set = (!rc ? tmp : 0x0F);

	pr_info("%s:config_reg(0x%02x), vout_floor(0x%02x), vout_roof(0x%02x), ilimit_set(0x%02x).\n",
			__func__, pdata->config_reg, pdata->vout_floor, pdata->vout_roof, pdata->ilimit_set);
	return 0;
}

static int tps_power_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct nqx_platform_data *platform_data;
	struct nqx_dev *nqx_dev;

	pr_info("%s: enter\n", __func__);
	if (client->dev.of_node) {
		platform_data = devm_kzalloc(&client->dev,
			sizeof(struct nqx_platform_data), GFP_KERNEL);
		if (!platform_data) {
			ret = -ENOMEM;
			goto err_platform_data;
		}
		ret = tps_power_parse_dt(&client->dev, platform_data);
		if (ret)
			goto err_free_data;
	} else
		platform_data = client->dev.platform_data;

	pr_info("%s, inside tps-power flags = %x\n",
		__func__, client->flags);

	if (platform_data == NULL) {
		pr_err("%s: failed\n", __func__);
		ret = -ENODEV;
		goto err_platform_data;
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto err_free_data;
	}
	nqx_dev = kzalloc(sizeof(*nqx_dev), GFP_KERNEL);
	if (nqx_dev == NULL) {
		ret = -ENOMEM;
		goto err_free_data;
	}

	if (gpio_is_valid(platform_data->tps_en)) {
		ret = gpio_request(platform_data->tps_en, "tps_enable");
		if (ret) {
			pr_err("%s: unable to request tps_enable gpio [%d]\n",__func__, platform_data->tps_en);
		}
		ret = gpio_direction_output(platform_data->tps_en, 1);
		if (ret) {
			pr_err("%s: unable to set direction for tps_enable gpio [%d]\n",__func__, platform_data->tps_en);
			goto err_en_gpio;
		}
	} else {
		pr_info("%s: tps-power mode_sel(%d) not provided. \n",
				__func__, platform_data->mode_sel);
		goto err_en_gpio;
	}

	msleep(10);

	if (gpio_is_valid(platform_data->mode_sel)) {
		ret = gpio_request(platform_data->mode_sel, "tps_power_mode");
		if (ret) {
			pr_err("%s: unable to request tps-power mode gpio [%d]\n",__func__, platform_data->mode_sel);
		}
		ret = gpio_direction_output(platform_data->mode_sel, 1);
		if (ret) {
			pr_err("%s: unable to set direction for tps-power mode gpio [%d]\n",__func__, platform_data->mode_sel);
			goto err_sel_gpio;
		}
	} else {
		pr_info("%s: tps-power mode_sel(%d) not provided. \n",
				__func__, platform_data->mode_sel);
		goto err_sel_gpio;
	}

	nqx_dev->client = client;
	nqx_dev->mode_sel = platform_data->mode_sel;
	nqx_dev->pdata = platform_data;
	i2c_set_clientdata(client, nqx_dev);
	nqx_dev->reg_data[0] = platform_data->config_reg;
	nqx_dev->reg_data[1] = platform_data->vout_floor;
	nqx_dev->reg_data[2] = platform_data->vout_roof;
	nqx_dev->reg_data[3] = platform_data->ilimit_set;
	tps61280a_power_control(client, true, true);
	ret = tps61280a_hw_check(client , platform_data->mode_sel);
	if (ret < 0) {
		pr_err("%s: tps61280a power supply isn't present, ret(%d), power off the device.\n",
			__func__, ret);
		tps61280a_power_control(client, true, false);
		goto err_en_gpio;
	}

	ret = gpio_direction_output(platform_data->mode_sel, 0);
	if (ret) {
		pr_err("%s: unable to set vsel gpio out high level \n",__func__);
	}

	/* init mutex and queues */
	mutex_init(&nqx_dev->hwres_mutex);

	tps61280a_power_create_sysfs(&client->dev);
	nqx_dev->nqx_device.minor = MISC_DYNAMIC_MINOR;
	nqx_dev->nqx_device.name = "tps-power";

	ret = misc_register(&nqx_dev->nqx_device);
	if (ret) {
		pr_err("%s: misc_register failed\n", __func__);
		goto err_misc_register;
	}

	pr_info("%s: probing tps61280a-power supply successfully.\n",
		 __func__);
	return 0;

err_misc_register:
	mutex_destroy(&nqx_dev->hwres_mutex);
err_sel_gpio:
	gpio_free(platform_data->mode_sel);
err_en_gpio:
	gpio_free(platform_data->tps_en);
	kfree(nqx_dev);
err_free_data:
	if (client->dev.of_node)
		devm_kfree(&client->dev, platform_data);
err_platform_data:
	pr_err("%s: probing tps-power failed.\n", __func__);
	return ret;
}

static int tps_power_remove(struct i2c_client *client)
{
	int ret = 0;
	struct nqx_dev *nqx_dev;

	tps61280a_power_remove_sysfs(&client->dev);
	nqx_dev = i2c_get_clientdata(client);
	if (!nqx_dev) {
		dev_err(&client->dev,
		"%s: device doesn't exist anymore\n", __func__);
		ret = -ENODEV;
		goto err;
	}

	misc_deregister(&nqx_dev->nqx_device);
	mutex_destroy(&nqx_dev->hwres_mutex);
	gpio_free(nqx_dev->mode_sel);
	if (client->dev.of_node)
		devm_kfree(&client->dev, nqx_dev->pdata);

	kfree(nqx_dev);
err:
	return ret;
}

static const struct i2c_device_id tps_power_id[] = {
	{"tps61280a-power-i2c", 0},
	{}
};

static struct i2c_driver tps61280a_power = {
	.id_table = tps_power_id,
	.probe = tps_power_probe,
	.remove = tps_power_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "tps-power",
		.of_match_table = tps61280a_power_table,
	},
};

/*
 * module load/unload record keeping
 */
static int __init tps_power_dev_init(void)
{
	return i2c_add_driver(&tps61280a_power);
}
module_init(tps_power_dev_init);

static void __exit tps_power_dev_exit(void)
{
	i2c_del_driver(&tps61280a_power);
}
module_exit(tps_power_dev_exit);

MODULE_AUTHOR("Yang Jie <yangjie@wingtech.com>");
MODULE_DESCRIPTION("TI TPS61280a power");
MODULE_LICENSE("GPL v2");
