// SPDX-License-Identifier: GPL-2.0
/*
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * Core file for Samsung PTN3222 driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

 #define pr_fmt(fmt)	"[USB_PHY] " fmt

#include "ptn3222_repeater.h"

static int ptn3222_write_reg(struct ptn3222_ddata *ddata,
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

static int ptn3222_read_reg(struct ptn3222_ddata *ddata,
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

static int ptn3222_power(void *_data, bool en)
{
	struct ptn3222_ddata *ddata = (struct ptn3222_ddata *)_data;
	struct ptn3222_pdata *pdata = NULL;
	u8 write_data = 0, rev_id = 0;
	int ret = 0, i = 0;

	if (!ddata)
		return -EEXIST;

	pdata = ddata->pdata;

	if (en) {
		mdelay(3);

		ret = ptn3222_read_reg(ddata,
			PTN3222_REV_ID_REG, &rev_id, 1);
		if (ret < 0)
			pr_err("%s failed to read PTN3222_REV_ID_REG(%d)\n",
				__func__, ret);
		else
			pr_info("%s REV_ID=%02X\n", __func__, rev_id);

		for (i = 0; i < pdata->tune_cnt; i++) {
			write_data = pdata->tune[i].value;
			ret = ptn3222_write_reg(ddata, pdata->tune[i].reg,
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

int ptn3222_repeater_set_bypass(void *_data, bool enable)
{
	struct ptn3222_ddata *ddata = (struct ptn3222_ddata *)_data;
	struct ptn3222_pdata *pdata = NULL;
	u8 write_data = 0, read_data = 0;
	int ret = 0;

	if (!ddata)
		return -EEXIST;

	pdata = ddata->pdata;

	ret = ptn3222_read_reg(ddata, 0x02, &read_data, 1);
	if (ret < 0) {
		pr_info("%s: i2c read error\n", __func__);
	}

	if (enable)
		write_data = (read_data & ~(0x7 << 0)) | 4 << 0;
	else
		write_data = (read_data & ~(0x7 << 0)) | 0 << 0;


	ret = ptn3222_write_reg(ddata, 0x02, &write_data, 1);
	if (ret < 0) {
		pr_info("%s: i2c write error\n", __func__);
	}

	return 0;
}

static ssize_t ptn3222_repeater_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ptn3222_ddata *ddata = dev_get_drvdata(dev);
	struct ptn3222_pdata *pdata = ddata->pdata;
	int len = 0, i = 0, ret = 0;
	u8 val = 0;

	len += snprintf(buf + len, PAGE_SIZE, "\t==== Print Repeater Tune Value ====\n");
	len += snprintf(buf + len, PAGE_SIZE, "Tune value count : %d\n", pdata->tune_cnt);

	for (i = 0; i < pdata->tune_cnt; i++) {
		const struct ptn3222_tune *tune = &pdata->tune[i];

		ret = ptn3222_read_reg(ddata, tune->reg, &val, 1);
		if (ret < 0)
			pr_err("%s failed to read i2c(0x%x)\n", __func__, ret);
		len += snprintf(buf + len, PAGE_SIZE, "%s\t\t\t: 0x%x\n",
				tune->name, val);
	}

	return len;
}

static ssize_t ptn3222_repeater_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t n)
{
	struct ptn3222_ddata *ddata = dev_get_drvdata(dev);
	struct ptn3222_pdata *pdata = ddata->pdata;
	char name[30];
	int ret = 0, i = 0, val = 0;
	u8 read_data = 0, write_data = 0;

	if (sscanf(buf, "%29s %x", name, &val) != 2)
		return -EINVAL;

	for (i = 0; i < pdata->tune_cnt; i++) {
		struct ptn3222_tune *tune = &pdata->tune[i];

		if (!strncmp(tune->name, name, strlen(tune->name))) {
			tune->value = (u8)val;
			write_data = tune->value;
			ret = ptn3222_write_reg(ddata, tune->reg, &write_data, 1);
			if (ret < 0)
				pr_err("%s failed to write i2c(0x%x)\n", __func__, ret);

			ret = ptn3222_read_reg(ddata, tune->reg, &read_data, 1);
			if (ret < 0)
				pr_err("%s failed to read i2c(0x%x)\n", __func__, ret);
			pr_info("%s 0x%x, 0x%x\n", tune->name,
				 tune->reg, read_data);
		}
	}

	return n;
}
static DEVICE_ATTR_RW(ptn3222_repeater);

static struct attribute *ptn3222_repeater_attrs[] = {
	&dev_attr_ptn3222_repeater.attr,
	NULL
};
ATTRIBUTE_GROUPS(ptn3222_repeater);

static struct ptn3222_pdata *ptn3222_parse_dt(struct device *dev)
{
	struct ptn3222_pdata *pdata;
	struct device_node *node = NULL, *child = NULL;
	const char *name;
	int ret = 0, idx = 0;
	u8 res[2];

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	node = of_find_node_by_name(dev->of_node, "repeater_tune_param");
	if (node != NULL) {
		ret = of_get_child_count(node);
		if (ret <= 0) {
			pdata->tune_cnt = 0;
			pr_err("failed to get child count(%d)\n", ret);
			return pdata;
		}
		pdata->tune_cnt = ret;
		pdata->tune = devm_kcalloc(dev, pdata->tune_cnt,
					sizeof(pdata->tune[0]), GFP_KERNEL);
		if (!pdata->tune)
			return ERR_PTR(-ENOMEM);

		for_each_child_of_node(node, child) {
			ret = of_property_read_string(child, "tune_name", &name);
			if (!ret) {
				if (!strncmp("RSV", name, 4)) {
					pdata->tune_cnt = idx;
					break;
				}
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

static int ptn3222_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct ptn3222_ddata *ddata;
	struct ptn3222_pdata *pdata;
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
		pdata = ptn3222_parse_dt(&client->dev);
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

	/*
	 * eUSB repeater control should be done with ldo on.
	 * So control will be done according to utmi_init/exit.
	 * ptn3222_power();
	 */

	ddata->rdata.enable = ptn3222_power;
	ddata->rdata.private_data = ddata;
	ddata->rdata.set_bypass = ptn3222_repeater_set_bypass;
	repeater_core_register(&ddata->rdata);
	return 0;
err:
	pr_err("%s err(%d)\n", __func__, ret);

	return ret;
}

static void ptn3222_shutdown(struct i2c_client *client)
{
	pr_info("%s\n", __func__);

	repeater_core_unregister();
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
static int ptn3222_remove(struct i2c_client *client)
#else
static void ptn3222_remove(struct i2c_client *client)
#endif
{
	struct ptn3222_ddata *ddata = i2c_get_clientdata(client);

	repeater_core_unregister();

	mutex_destroy(&ddata->i2c_mutex);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
	return 0;
#endif
}

#if IS_ENABLED(CONFIG_PM)
static int ptn3222_suspend(struct device *dev)
{
	return 0;
}

static int ptn3222_resume(struct device *dev)
{
	return 0;
}
#else
#define ptn3222_suspend NULL
#define ptn3222_resume NULL
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops ptn3222_pm_ops = {
	.suspend = ptn3222_suspend,
	.resume = ptn3222_resume,
};
#endif

static const struct of_device_id ptn3222_match_table[] = {
	{ .compatible = "ptn3222-repeater",},
	{},
};

static const struct i2c_device_id ptn3222_id[] = {
	{"ptn3222-repeater", 0},
	{}
};

static struct i2c_driver ptn3222_driver = {
	.probe  = ptn3222_probe,
	.remove = ptn3222_remove,
	.shutdown   = ptn3222_shutdown,
	.id_table   = ptn3222_id,
	.driver = {
		.name = "ptn3222-repeater",
		.owner = THIS_MODULE,
		.dev_groups = ptn3222_repeater_groups,
		.pm = &ptn3222_pm_ops,
		.of_match_table = ptn3222_match_table,
	},
};

static int __init ptn3222_init(void)
{
	return i2c_add_driver(&ptn3222_driver);
}

static void __exit ptn3222_exit(void)
{
	i2c_del_driver(&ptn3222_driver);
}
module_init(ptn3222_init);
module_exit(ptn3222_exit);

MODULE_DESCRIPTION("Samsung eUSB Repeater Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");

