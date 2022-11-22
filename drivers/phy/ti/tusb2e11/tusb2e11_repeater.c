// SPDX-License-Identifier: GPL-2.0
/*
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * Core file for Samsung TUSB2E11 driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

 #define pr_fmt(fmt)	"[USB_PHY] " fmt

#include "tusb2e11_repeater.h"

static int tusb2e11_write_reg(struct tusb2e11_ddata *ddata,
	u8 reg, u8 *data, int len)
{
	struct i2c_client *i2c = ddata->client;
	int ret, i;

	mutex_lock(&ddata->i2c_mutex);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, len, data);
		if (ret >= 0)
			break;
		pr_info("%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
			__func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}
	mutex_unlock(&ddata->i2c_mutex);
	if (ret < 0)
		return ret;

	return 0;
}

static int tusb2e11_read_reg(struct tusb2e11_ddata *ddata,
	u8 reg, u8 *data, int len)
{
	struct i2c_client *i2c = ddata->client;
	int ret, i;

	mutex_lock(&ddata->i2c_mutex);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, len, data);
		if (ret >= 0)
			break;
		pr_info("%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n",
			__func__, reg, ret, i + 1, I2C_RETRY_CNT);
	}
	mutex_unlock(&ddata->i2c_mutex);
	if (ret < 0)
		return ret;

	return 0;
}

static int tusb2e11_gpio_ctrl(struct tusb2e11_ddata *ddata, int value)
{
	struct tusb2e11_pdata *pdata = ddata->pdata;
	int ret = 0;

	if (gpio_is_valid(pdata->gpio_eusb_ctrl_sel)) {
		gpio_set_value_cansleep(pdata->gpio_eusb_ctrl_sel, value);
		ddata->ctrl_sel_status = value;
		if (!!value)
			mdelay(3);

		if (value != gpio_get_value(pdata->gpio_eusb_ctrl_sel))
			ret = -EAGAIN;
	}

	return ret;
}

static int tusb2e11_power(void *_data, bool en)
{
	struct tusb2e11_ddata *ddata = (struct tusb2e11_ddata *)_data;
	struct tusb2e11_pdata *pdata = NULL;
	u8 write_data = 0, rev_id = 0;
	int ret = 0, i = 0;

	if (!ddata)
		return -EEXIST;

	pdata = ddata->pdata;

	ret = tusb2e11_gpio_ctrl(ddata, en);
	if (ret < 0) {
		pr_err("%s failed to write error(%d)\n", __func__, ret);
		goto err;
	}

	if (en) {
		ret = tusb2e11_read_reg(ddata,
			TUSB2E11_REV_ID_REG, &rev_id, 1);
		if (ret < 0)
			pr_err("%s failed to read TUSB2E11_REV_ID_REG(%d)\n",
				__func__, ret);
		else
			pr_info("%s REV_ID=%02X\n", __func__, rev_id);

		write_data = TUSB2E11_UART_PORT1_INIT;
		ret = tusb2e11_write_reg(ddata,
				TUSB2E11_UART_PORT1_REG,
				&write_data, 1);
		if (ret < 0) {
			pr_err("%s failed to write error(%d)\n", __func__, ret);
			goto err;
		}

		for (i = 0; i < pdata->tune_cnt; i++) {
			write_data = pdata->tune[i].value;
			ret = tusb2e11_write_reg(ddata, pdata->tune[i].reg,
					 &write_data, 1);
			if (ret < 0) {
				pr_err("%s failed to write error(%d)\n", __func__, ret);
				goto err;
			}
			pr_info("%s : 0x%x\n", pdata->tune[i].name, write_data);
		}
	}

	return 0;
err:
	return ret;
}

static ssize_t tusb2e11_repeater_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tusb2e11_ddata *ddata = dev_get_drvdata(dev);
	struct tusb2e11_pdata *pdata = ddata->pdata;
	int len = 0, i = 0, ret = 0;
	u8 val = 0;

	len += snprintf(buf + len, PAGE_SIZE, "\t==== Print Repeater Tune Value ====\n");
	len += snprintf(buf + len, PAGE_SIZE, "Tune value count : %d\n", pdata->tune_cnt);

	for (i = 0; i < pdata->tune_cnt; i++) {
		const struct tusb2e11_tune *tune = &pdata->tune[i];

		ret = tusb2e11_read_reg(ddata, tune->reg, &val, 1);
		if (ret < 0)
			pr_err("%s failed to read i2c(0x%x)\n", __func__, ret);
		len += snprintf(buf + len, PAGE_SIZE, "%s\t\t\t: 0x%x\n",
				tune->name, val);
	}

	return len;
}

static ssize_t tusb2e11_repeater_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t n)
{
	struct tusb2e11_ddata *ddata = dev_get_drvdata(dev);
	struct tusb2e11_pdata *pdata = ddata->pdata;
	char name[30];
	int ret = 0, i = 0, val = 0;
	u8 read_data = 0, write_data = 0;

	if (sscanf(buf, "%29s %x", name, &val) != 2)
		return -EINVAL;

	if (ddata->ctrl_sel_status == false)
		tusb2e11_gpio_ctrl(ddata, true);

	for (i = 0; i < pdata->tune_cnt; i++) {
		struct tusb2e11_tune *tune = &pdata->tune[i];

		if (!strncmp(tune->name, name, strlen(tune->name))) {
			tune->value = (u8)val;
			write_data = tune->value;
			ret = tusb2e11_write_reg(ddata, tune->reg, &write_data, 1);
			if (ret < 0)
				pr_err("%s failed to write i2c(0x%x)\n", __func__, ret);

			ret = tusb2e11_read_reg(ddata, tune->reg, &read_data, 1);
			if (ret < 0)
				pr_err("%s failed to read i2c(0x%x)\n", __func__, ret);
			pr_info("%s 0x%x, 0x%x\n", tune->name,
				 tune->reg, read_data);
		}
	}

	return n;
}
static DEVICE_ATTR_RW(tusb2e11_repeater);

static struct attribute *tusb2e11_repeater_attrs[] = {
	&dev_attr_tusb2e11_repeater.attr,
	NULL
};
ATTRIBUTE_GROUPS(tusb2e11_repeater);

static struct tusb2e11_pdata *tusb2e11_parse_dt(struct device *dev)
{
	struct tusb2e11_pdata *pdata;
	struct device_node *node = NULL, *child = NULL;
	const char *name;
	int ret = 0, idx = 0;
	u8 res[2];

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->gpio_eusb_ctrl_sel = of_get_named_gpio(dev->of_node,
		"eusb,gpio_eusb_ctrl", 0);

	node = of_find_node_by_name(dev->of_node, "repeater_tune_param");
	if (node != NULL) {
		ret = of_property_read_u8(node, "repeater_tune_cnt",
				&pdata->tune_cnt);
		if (ret < 0) {
			pdata->tune_cnt = 0;
			pr_err("failed to get repeater_tune_cnt(%d), %d\n",
				ret, pdata->tune_cnt);
			return pdata;
		}

		pdata->tune = devm_kcalloc(dev, pdata->tune_cnt,
					sizeof(pdata->tune[0]), GFP_KERNEL);
		if (!pdata->tune)
			return ERR_PTR(-ENOMEM);

		for_each_child_of_node(node, child) {
			if (pdata->tune_cnt <= idx) {
				pr_err("tune count is wrong (%d)\n", pdata->tune_cnt);
				break;
			}
			ret = of_property_read_string(child, "tune_name", &name);
			if (!ret) {
				strlcpy(pdata->tune[idx].name, name, TUNE_NAME_SIZE);
			} else {
				pr_err("failed to read tune name from %s node\n", child->name);
				return ERR_PTR(-EINVAL);
			}
			ret = of_property_read_u8_array(child, "tune_value", res, 2);
			if (!ret) {
				pdata->tune[idx].reg = res[0];
				pdata->tune[idx].value = res[1];
			} else {
				pr_err("failed to read tune value from %s node\n", child->name);
				return ERR_PTR(-EINVAL);
			}
			pr_info("%s : 0x%x, 0x%x\n",
				 pdata->tune[idx].name,
				 pdata->tune[idx].reg,
				 pdata->tune[idx].value);

			idx++;
		}
	} else
		pr_err("don't need repeater tuning param\n");

	return pdata;
}

static int tusb2e11_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct tusb2e11_ddata *ddata;
	struct tusb2e11_pdata *pdata;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s EIO err!\n", __func__);
		ret = -EIO;
		goto err;
	}

	ddata = devm_kzalloc(&client->dev, sizeof(*ddata), GFP_KERNEL);
	if (ddata == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	if (client->dev.of_node) {
		pdata = tusb2e11_parse_dt(&client->dev);
		if (IS_ERR(pdata)) {
			ret = PTR_ERR(pdata);
			goto err;
		}
	} else
		pdata = client->dev.platform_data;

	ddata->client = client;
	ddata->pdata = pdata;
	i2c_set_clientdata(client, ddata);
	mutex_init(&ddata->i2c_mutex);
	if (gpio_is_valid(pdata->gpio_eusb_ctrl_sel)) {
		pr_info("gpio_eusb_ctrl_sel is valid\n");
		ret = gpio_request(pdata->gpio_eusb_ctrl_sel, "eusb,gpio_eusb_ctrl");
		if (ret) {
			pr_err("failed to gpio_request eusb,gpio_eusb_ctrl\n");
			goto err;
		}
	}

	/*
	 * eUSB repeater control should be done with ldo on.
	 * So control will be done according to utmi_init/exit.
	 * tusb2e11_power();
	 */

	ddata->rdata.enable = tusb2e11_power;
	ddata->rdata.private_data = ddata;
	repeater_core_register(&ddata->rdata);

	return 0;

err:
	pr_err("%s err(%d)\n", __func__, ret);

	return ret;
}

static void tusb2e11_shutdown(struct i2c_client *client)
{
	pr_info("%s\n", __func__);

	repeater_core_unregister();
}

static int tusb2e11_remove(struct i2c_client *client)
{
	struct tusb2e11_ddata *ddata = i2c_get_clientdata(client);

	repeater_core_unregister();

	mutex_destroy(&ddata->i2c_mutex);
	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int tusb2e11_suspend(struct device *dev)
{
	return 0;
}

static int tusb2e11_resume(struct device *dev)
{
	return 0;
}
#else
#define tusb2e11_suspend NULL
#define tusb2e11_resume NULL
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops tusb2e11_pm_ops = {
	.suspend = tusb2e11_suspend,
	.resume = tusb2e11_resume,
};
#endif

static const struct of_device_id tusb2e11_match_table[] = {
	{ .compatible = "tusb2e11-repeater",},
	{},
};

static const struct i2c_device_id tusb2e11_id[] = {
	{"tusb2e11-repeater", 0},
	{}
};

static struct i2c_driver tusb2e11_driver = {
	.probe  = tusb2e11_probe,
	.remove = tusb2e11_remove,
	.shutdown   = tusb2e11_shutdown,
	.id_table   = tusb2e11_id,
	.driver = {
		.name = "tusb2e11-repeater",
		.owner = THIS_MODULE,
		.dev_groups = tusb2e11_repeater_groups,
		.pm = &tusb2e11_pm_ops,
		.of_match_table = tusb2e11_match_table,
	},
};

static int __init tusb2e11_init(void)
{
	return i2c_add_driver(&tusb2e11_driver);
}

static void __exit tusb2e11_exit(void)
{
	i2c_del_driver(&tusb2e11_driver);
}
module_init(tusb2e11_init);
module_exit(tusb2e11_exit);

MODULE_DESCRIPTION("Samsung eUSB Repeater Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");

