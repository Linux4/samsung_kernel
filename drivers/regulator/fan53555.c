/*
 * FAN53555 Fairchild Digitally Programmable TinyBuck Regulator Driver.
 *
 * Copyright (c) 2012 Marvell Technology Ltd.
 * Yunfan Zhang <yfzhang@marvell.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/param.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/idr.h>
#include <linux/regmap.h>
#include <linux/regulator/fan53555.h>

/* Voltage setting */
#define FAN53555_VSEL0		0x00
#define FAN53555_VSEL1		0x01
/* Control register */
#define FAN53555_CONTROL	0x02
/* IC Type */
#define FAN53555_ID1		0x03
/* IC mask version */
#define FAN53555_ID2		0x04
/* Monitor register */
#define FAN53555_MONITOR	0x05

/* VSEL bit definitions */
#define VSEL_BUCK_EN	(1 << 7)
#define VSEL_MODE		(1 << 6)
#define VSEL_NSEL_MASK	0x3F
/* Chip ID and Verison */
#define DIE_ID		0x0F	/* ID1 */
#define DIE_REV		0x0F	/* ID2 */
/* Control bit definitions */
#define CTL_OUTPUT_DISCHG	(1 << 7)
#define CTL_SLEW_MASK		(0x7 << 4)
#define CTL_SLEW_SHIFT		4
#define CTL_RESET			(1 << 2)

#define FAN53555_SLEEP_VOL	1050000	/* Default sleep output voltage: uV */

#define FAN53555_NVOLTAGES	64	/* Numbers of voltages */

struct fan53555_device_info {
	struct regmap *regmap;
	struct device *dev;
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	struct regulator_init_data *regulator;
	/* Voltage setting register */
	unsigned int vol_reg;
	unsigned int sleep_reg;
	/* IC Type and Rev */
	int chip_id;
	int chip_rev;
	/* Voltage range */
	unsigned int vsel_min;
	unsigned int vsel_max;
	/* Voltage slew rate limiting */
	unsigned int slew_rate;
	/* Sleep voltage at VSEL is low */
	unsigned int sleep_vol;
};

/* If the system has several fan53555 we need a different id and name for each
 * of them...
 */
static DEFINE_IDR(regulator_id);
static DEFINE_MUTEX(regulator_mutex);

/* IC Type */
static int fan53555_get_chip_id(struct fan53555_device_info *di)
{
	unsigned int val;
	int ret;

	ret = regmap_read(di->regmap, FAN53555_ID1, &val);
	if (ret < 0)
		return ret;
	return val & DIE_ID;
}

/* IC mask revision */
static int fan53555_get_chip_rev(struct fan53555_device_info *di)
{
	unsigned int val;
	int ret;

	ret = regmap_read(di->regmap, FAN53555_ID2, &val);
	if (ret < 0)
		return ret;
	return val & DIE_REV;
}

static int check_range(struct fan53555_device_info *di, int min_uV, int max_uV)
{
	if (min_uV < di->vsel_min || max_uV > di->vsel_max
		|| min_uV > max_uV) {
		return -EINVAL;
	}
	return 0;
}

/* For 00,01,03,05 options:
 * VOUT = 0.60V + NSELx * 10mV, from 0.60 to 1.23V.
 * For 04 option:
 * VOUT = 0.603V + NSELx * 12.826mV, from 0.603 to 1.411V.
 * */
static int choose_voltage(struct fan53555_device_info *di,
				int min_uV, int max_uV)
{
	int data = 0;

	switch (di->chip_id) {
	case FAN53555_CHIP_ID_00:
	case FAN53555_CHIP_ID_01:
	case FAN53555_CHIP_ID_03:
	case FAN53555_CHIP_ID_05:
		data = (min_uV - di->vsel_min) / 10000;
		break;
	case FAN53555_CHIP_ID_04:
		data = (min_uV - di->vsel_min) / 12826;
		break;
	default:
		dev_err(di->dev,
			"Chip ID[%d]\n not supported!\n", di->chip_id);
		return -EINVAL;
	}
	return data;
}

static int fan53555_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	struct fan53555_device_info *di = rdev_get_drvdata(rdev);
	int vol = 0;

	switch (di->chip_id) {
	case FAN53555_CHIP_ID_00:
	case FAN53555_CHIP_ID_01:
	case FAN53555_CHIP_ID_03:
	case FAN53555_CHIP_ID_05:
		vol = (di->vsel_min + selector * 10000);
		break;
	case FAN53555_CHIP_ID_04:
		vol = (di->vsel_min + selector * 12826);
		break;
	default:
		dev_err(di->dev,
			"Chip ID[%d]\n not supported!\n", di->chip_id);
		return -EINVAL;
	}
	return vol;
}

static int fan53555_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned *selector)
{
	struct fan53555_device_info *di = rdev_get_drvdata(rdev);
	unsigned int data, mask;
	int ret = 0;

	if (check_range(di, min_uV, max_uV)) {
		dev_err(di->dev,
			"invalid voltage range (%d, %d) uV\n", min_uV, max_uV);
		return -EINVAL;
	}
	ret = choose_voltage(di, min_uV, max_uV);
	if (ret < 0)
		return ret;
	data = ret;
	mask = VSEL_NSEL_MASK;
	*selector = data;

	return regmap_update_bits(di->regmap, di->vol_reg, mask, data);
}

static int fan53555_get_voltage(struct regulator_dev *rdev)
{
	struct fan53555_device_info *di = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;

	ret = regmap_read(di->regmap, di->vol_reg, &data);
	if (ret < 0)
		return ret;
	data = data & VSEL_NSEL_MASK;

	return fan53555_list_voltage(rdev, data);
}

static int fan53555_enable(struct regulator_dev *rdev)
{
	struct fan53555_device_info *di = rdev_get_drvdata(rdev);

	return regmap_update_bits(di->regmap, di->vol_reg,
				VSEL_BUCK_EN, VSEL_BUCK_EN);
}

static int fan53555_disable(struct regulator_dev *rdev)
{
	struct fan53555_device_info *di = rdev_get_drvdata(rdev);

	return regmap_update_bits(di->regmap, di->vol_reg, VSEL_BUCK_EN, 0);
}

static int fan53555_is_enabled(struct regulator_dev *rdev)
{
	struct fan53555_device_info *di = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	ret  = regmap_read(di->regmap, di->vol_reg, &val);
	if (ret < 0)
		return ret;
	return val & VSEL_BUCK_EN;
}

static int fan53555_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct fan53555_device_info *di = rdev_get_drvdata(rdev);

	switch (mode) {
	case REGULATOR_MODE_FAST:
		regmap_update_bits(di->regmap, di->vol_reg,
				VSEL_MODE, VSEL_MODE);
		break;
	case REGULATOR_MODE_NORMAL:
		regmap_update_bits(di->regmap, di->vol_reg, VSEL_MODE, 0);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static unsigned int fan53555_get_mode(struct regulator_dev *rdev)
{
	struct fan53555_device_info *di = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret = 0;

	ret = regmap_read(di->regmap, di->vol_reg, &val);
	if (ret < 0)
		return ret;
	if (val & VSEL_MODE)
		return REGULATOR_MODE_FAST;
	else
		return REGULATOR_MODE_NORMAL;
}

static struct regulator_ops fan53555_regulator_ops = {
	.set_voltage = fan53555_set_voltage,
	.get_voltage = fan53555_get_voltage,
	.list_voltage = fan53555_list_voltage,
	.enable = fan53555_enable,
	.disable = fan53555_disable,
	.is_enabled = fan53555_is_enabled,
	.set_mode = fan53555_set_mode,
	.get_mode = fan53555_get_mode,
};

static int fan53555_vol_init(struct fan53555_device_info *di,
				struct fan53555_platform_data *pdata)
{
	unsigned int data, mask;
	int ret = 0;
	/* Setup voltage control register */
	if (pdata->sleep_vsel_id == FAN53555_VSEL_ID_0) {
		/* Default setting */
		di->sleep_reg = FAN53555_VSEL0;
		di->vol_reg = FAN53555_VSEL1;
	} else {
		di->sleep_reg = FAN53555_VSEL1;
		di->vol_reg = FAN53555_VSEL0;
	}
	/* Init voltage range */
	switch (di->chip_id) {
	case FAN53555_CHIP_ID_00:
	case FAN53555_CHIP_ID_01:
	case FAN53555_CHIP_ID_03:
	case FAN53555_CHIP_ID_05:
		di->vsel_min = 600000;
		di->vsel_max = 1230000;
		break;
	case FAN53555_CHIP_ID_04:
		di->vsel_min = 603000;
		di->vsel_max = 1411000;
		break;
	default:
		dev_err(di->dev,
			"Chip ID[%d]\n not supported!\n", di->chip_id);
		return -EINVAL;
	}
	/* Init sleep voltage */
	if (pdata->sleep_vol < di->vsel_min || pdata->sleep_vol > di->vsel_max)
		di->sleep_vol = FAN53555_SLEEP_VOL;
	else
		di->sleep_vol = pdata->sleep_vol;
	ret = choose_voltage(di, di->sleep_vol, di->sleep_vol);
	if (ret < 0)
		return ret;
	data = ret;
	mask = VSEL_NSEL_MASK;
	ret = regmap_update_bits(di->regmap, di->sleep_reg, mask, data);

	return ret;
}

static int fan53555_set_slew_rate(struct fan53555_device_info *di,
				struct fan53555_platform_data *pdata)
{
	unsigned int reg, data, mask;

	if (pdata->slew_rate & 0x7)
		di->slew_rate = pdata->slew_rate;
	else
		di->slew_rate = FAN53555_SLEW_RATE_64MV;
	reg = FAN53555_CONTROL;
	data = di->slew_rate << CTL_SLEW_SHIFT;
	mask = CTL_SLEW_MASK;
	return regmap_update_bits(di->regmap, reg, mask, data);
}

static int fan53555_device_setup(struct fan53555_device_info *di,
				struct fan53555_platform_data *pdata)
{
	int ret = 0;
	/* Set slew rate */
	ret = fan53555_set_slew_rate(di, pdata);
	if (ret < 0)
		return ret;
	/* Init voltage settings */
	ret = fan53555_vol_init(di, pdata);
	if (ret < 0)
		dev_err(di->dev, "Failed to init voltage!\n");
	return ret;
}

static int voltage_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fan53555_device_info *di = i2c_get_clientdata(client);
	int ret;

	ret = fan53555_get_voltage(di->rdev);
	if (ret < 0) {
		dev_err(dev, "Can't get voltage!\n");
		return 0;
	}
	return sprintf(buf, "%d\n", ret);
}

static int voltage_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fan53555_device_info *di = i2c_get_clientdata(client);
	unsigned int vol, sel;
	int ret;

	vol = simple_strtoul(buf, NULL, 10);
	ret = fan53555_set_voltage(di->rdev, vol, vol, &sel);
	if (ret < 0)
		dev_err(dev, "Can't set voltage!\n");
	return count;
}

static DEVICE_ATTR(voltage, S_IRUGO | S_IWUSR, voltage_show, voltage_store);

static int regs_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fan53555_device_info *di = i2c_get_clientdata(client);
	unsigned int val;
	int cnt = 0, ret;

	/* VSEL0 */
	ret = regmap_read(di->regmap, FAN53555_VSEL0, &val);
	if (ret < 0)
		return ret;
	cnt += sprintf(buf + cnt, "VSEL0:   0x%02x\n", val);
	/* VSEL1 */
	ret = regmap_read(di->regmap, FAN53555_VSEL1, &val);
	if (ret < 0)
		return ret;
	cnt += sprintf(buf + cnt, "VSEL1:   0x%02x\n", val);
	/* CONTROL */
	ret = regmap_read(di->regmap, FAN53555_CONTROL, &val);
	if (ret < 0)
		return ret;
	cnt += sprintf(buf + cnt, "CONTROL: 0x%02x\n", val);
	/* ID1 */
	ret = regmap_read(di->regmap, FAN53555_ID1, &val);
	if (ret < 0)
		return ret;
	cnt += sprintf(buf + cnt, "ID1:     0x%02x\n", val);
	/* ID2 */
	ret = regmap_read(di->regmap, FAN53555_ID2, &val);
	if (ret < 0)
		return ret;
	cnt += sprintf(buf + cnt, "ID2:     0x%02x\n", val);
	/* MONITOR */
	ret = regmap_read(di->regmap, FAN53555_MONITOR, &val);
	if (ret < 0)
		return ret;
	cnt += sprintf(buf + cnt, "MONITOR: 0x%x\n", val);

	return cnt;
}

static DEVICE_ATTR(regs, S_IRUGO | S_IWUSR, regs_show, NULL);

static struct attribute *fan53555_attributes[] = {
	&dev_attr_voltage.attr,
	&dev_attr_regs.attr,
	NULL,
};

static struct attribute_group fan53555_attr_grp = {
	.attrs = fan53555_attributes,
};

static int fan53555_regulator_register(struct fan53555_device_info *di,
				char *name, int num)
{
	struct regulator_desc *rdesc = &di->desc;

	rdesc->name = name;
	rdesc->id = num;
	rdesc->ops = &fan53555_regulator_ops;
	rdesc->type = REGULATOR_VOLTAGE;
	rdesc->n_voltages = FAN53555_NVOLTAGES;
	rdesc->owner = THIS_MODULE;

	di->rdev = regulator_register(&di->desc, di->dev,
					di->regulator, di, NULL);
	if (IS_ERR(di->rdev))
		return PTR_ERR(di->rdev);
	return 0;

}

static struct regmap_config fan53555_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int __devinit fan53555_regulator_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct fan53555_device_info *di;
	struct fan53555_platform_data *pdata;
	char *name;
	int num, ret = 0;

	pdata = client->dev.platform_data;
	if (!pdata || !pdata->regulator) {
		dev_err(&client->dev, "Missing platform data\n");
		return -EINVAL;
	}

	di = devm_kzalloc(&client->dev, sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev, "Failed to allocate device info data\n");
		return -ENOMEM;
	}
	di->regmap = regmap_init_i2c(client, &fan53555_regmap_config);
	if (IS_ERR(di->regmap)) {
		ret = PTR_ERR(di->regmap);
		dev_err(&client->dev, "Failed to allocate regmap\n");
		goto err_regmap;
	}
	di->dev = &client->dev;
	di->regulator = pdata->regulator;
	i2c_set_clientdata(client, di);
	/* Get a new ID for the new device */
	ret = idr_pre_get(&regulator_id, GFP_KERNEL);
	if (ret == 0) {
		ret = -ENOMEM;
		dev_err(&client->dev, "Can't get new id!\n");
		goto err_idr_pre_get;
	}
	mutex_lock(&regulator_mutex);
	ret = idr_get_new(&regulator_id, di, &num);
	mutex_unlock(&regulator_mutex);
	if (ret < 0) {
		dev_err(&client->dev, "Can't get new id!\n");
		goto err_idr_get_new;
	}
	/* Generate a name with new id */
	name = kasprintf(GFP_KERNEL, "%s-%d", id->name, num);
	/* Get chip ID */
	di->chip_id = fan53555_get_chip_id(di);
	if (di->chip_id < 0) {
		dev_err(&client->dev, "Failed to get chip ID!\n");
		ret = -ENODEV;
		goto err_get_chip_id;
	}
	/* Get chip revision */
	di->chip_rev = fan53555_get_chip_rev(di);
	if (di->chip_rev < 0) {
		dev_err(&client->dev, "Failed to get chip Rev!\n");
		ret = -ENODEV;
		goto err_get_chip_rev;
	}
	dev_info(&client->dev, "FAN53555 Option[%d] Rev[%d] Detected!\n",
				di->chip_id, di->chip_rev);
	/* Device init */
	ret = fan53555_device_setup(di, pdata);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to setup device!\n");
		goto err_device_setup;
	}
	/* Register regulator */
	ret = fan53555_regulator_register(di, name, num);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to register regulator!\n");
		goto err_rdev_register;
	}
	/* Create sysfs interface */
	ret = sysfs_create_group(&client->dev.kobj, &fan53555_attr_grp);
	if (ret) {
		dev_err(&client->dev, "Failed to create sysfs group!\n");
		goto err_create_sysfs;
	}
	return 0;

err_create_sysfs:
	regulator_unregister(di->rdev);
err_rdev_register:
err_device_setup:
err_get_chip_rev:
err_get_chip_id:
	kfree(name);
	mutex_lock(&regulator_mutex);
	idr_remove(&regulator_id, num);
	mutex_unlock(&regulator_mutex);
err_idr_get_new:
err_idr_pre_get:
	regmap_exit(di->regmap);
err_regmap:
	devm_kfree(di->dev, di);
	return ret;
}

static int __devexit fan53555_regulator_remove(struct i2c_client *client)
{
	struct fan53555_device_info *di = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &fan53555_attr_grp);
	regulator_unregister(di->rdev);

	kfree(di->desc.name);
	mutex_lock(&regulator_mutex);
	idr_remove(&regulator_id, di->desc.id);
	mutex_unlock(&regulator_mutex);

	regmap_exit(di->regmap);
	devm_kfree(di->dev, di);
	return 0;
}

static const struct i2c_device_id fan53555_id[] = {
	{"fan53555", -1},
	{},
};

static struct i2c_driver fan53555_regulator_driver = {
	.driver = {
		.name = "fan53555-regulator",
	},
	.probe = fan53555_regulator_probe,
	.remove = __devexit_p(fan53555_regulator_remove),
	.id_table = fan53555_id,
};

static int __init fan53555_init(void)
{
	int ret;
	ret = i2c_add_driver(&fan53555_regulator_driver);
	if (ret)
		pr_err("Unable to register FAN53555 regulator driver");
	return ret;
}

module_init(fan53555_init);

static void __exit fan53555_exit(void)
{
	i2c_del_driver(&fan53555_regulator_driver);
}

module_exit(fan53555_exit);

MODULE_AUTHOR("Yunfan Zhang <yfzhang@marvell.com>");
MODULE_DESCRIPTION("FAN53555 regulator driver");
MODULE_LICENSE("GPL");
