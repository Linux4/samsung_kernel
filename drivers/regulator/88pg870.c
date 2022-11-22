/*
 * Marvell 88PG870 Buck Converter Regulator Driver.
 *
 * Copyright (c) 2013 Marvell Technology Ltd.
 * Yipeng Yao <ypyao@marvell.com>
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
#include <linux/regulator/88pg870.h>

#define PG870_MIN_UV		600000
#define PG870_MID_UV		1600000
#define PG870_MAX_UV		3950000
#define SEL_STEPS		((PG870_MID_UV - PG870_MIN_UV) / 12500)

/* Status register */
#define PG870_STATUS_REG	0x01
/* Sysctrl register */
#define PG870_SYSCTRL_REG	0x02
/* Sleep Mode register */
#define PG870_SLEEP_MODE_REG	0x04
/* Sleep Mode Voltage register */
#define PG870_SLEEP_VOL_REG	0x0D
/* Control register */
#define PG870_CONTROL_REG	0x0E
/* IC Type */
#define PG870_CHIP_ID_REG	0x33

#define CTL_RAMP_MASK		0x7
#define SM_PD			(1 << 4)
#define PG870_DEF_SYSCTRL	0x1F
#define PG870_NVOLTAGES		128	/* Numbers of voltages */

static unsigned int vsel_reg_map[] = {0x05, 0x06, 0x07, 0x08};

struct pg870_device_info {
	struct regmap *regmap;
	struct device *dev;
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	struct regulator_init_data *regulator;
	/* IC Type and Rev */
	int chip_id;
	/* Voltage slew rate limiting */
	unsigned int ramp_rate;
	/* Voltage select table idx */
	unsigned int vsel_idx;
};

/* If the system has several 88PG870 we need a different id and name for each
 * of them...
 */
static DEFINE_IDR(regulator_id);
static DEFINE_MUTEX(regulator_mutex);

static inline int get_chip_id(struct pg870_device_info *di)
{
	unsigned int val;
	int ret;

	ret = regmap_read(di->regmap, PG870_CHIP_ID_REG, &val);
	if (ret < 0)
		return ret;
	return val;
}

static inline int check_range(struct pg870_device_info *di, int min_uV, int max_uV)
{
	if (min_uV < PG870_MIN_UV || max_uV > PG870_MAX_UV
		|| min_uV > PG870_MAX_UV) {
		return -EINVAL;
	}
	return 0;
}

/*
 * VOUT = 0.60V + NSELx * 12.5mV, from 0.6V to 1.6V.
 * VOUT = 1.60V + NSELx * 50mV, from 1.6V to 3.95V.
 */
static inline int choose_voltage(struct pg870_device_info *di,
				int min_uV, int max_uV)
{
	int data = 0;

	if (min_uV < PG870_MID_UV)
		data = (min_uV - PG870_MIN_UV) / 12500;
	else
		data = SEL_STEPS + (min_uV - PG870_MID_UV) / 50000;
	return data;
}

static int pg870_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	int vol = 0;

	if (selector < SEL_STEPS)
		vol = selector * 12500 + PG870_MIN_UV;
	else
		vol = selector * 50000 + PG870_MID_UV;
	return vol;
}

static int pg870_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned *selector)
{
	struct pg870_device_info *di = rdev_get_drvdata(rdev);
	unsigned int data;
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
	*selector = data;

	return regmap_update_bits(di->regmap, vsel_reg_map[di->vsel_idx], 0x7F, data);
}

static int pg870_get_voltage(struct regulator_dev *rdev)
{
	struct pg870_device_info *di = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;

	ret = regmap_read(di->regmap, vsel_reg_map[di->vsel_idx], &data);
	if (ret < 0)
		return ret;
	data = data & 0x7F;

	return pg870_list_voltage(rdev, data);
}

static struct regulator_ops pg870_regulator_ops = {
	.set_voltage = pg870_set_voltage,
	.get_voltage = pg870_get_voltage,
	.list_voltage = pg870_list_voltage,
};

static int pg870_set_ramp_rate(struct pg870_device_info *di,
				struct pg870_platform_data *pdata)
{
	unsigned int reg, data, mask;

	if (pdata->ramp_rate & 0x7)
		di->ramp_rate = pdata->ramp_rate;
	else
		di->ramp_rate = PG870_RAMP_RATE_14000UV;
	reg = PG870_CONTROL_REG;
	data = di->ramp_rate;
	mask = CTL_RAMP_MASK;
	return regmap_update_bits(di->regmap, reg, mask, data);
}

static int pg870_device_setup(struct pg870_device_info *di,
				struct pg870_platform_data *pdata)
{
	unsigned int data, sm, sv;
	int ret = 0;
	/* Set ramp rate */
	ret = pg870_set_ramp_rate(di, pdata);
	if (ret < 0) {
		dev_err(di->dev, "Fialed to set ramp rate!\n");
		return ret;
	}

	/* Set SYSCTRL */
	if (pdata->sysctrl)
		data = pdata->sysctrl;
	else
		data = PG870_DEF_SYSCTRL;
	ret = regmap_write(di->regmap, PG870_SYSCTRL_REG, data);
	if (ret < 0) {
		dev_err(di->dev, "Failed to set SYSCTRL!\n");
		return ret;
	}

	/* Set sleep mode */
	if (pdata->sleep_mode)
		sm = pdata->sleep_mode;
	else
		sm = PG870_SM_DISABLE;
	switch (sm) {
	case PG870_SM_TURN_OFF:
	case PG870_SM_RUN_SLEEP:
	case PG870_SM_LPM_SLEEP:
	case PG870_SM_RUN_ACTIVE:
		data = 0x40 | SM_PD | sm;
		break;
	case PG870_SM_DISABLE:
		data = 0x40;
		break;
	}
	ret = regmap_write(di->regmap, PG870_SLEEP_MODE_REG, data);
	if (ret < 0) {
		dev_err(di->dev, "Failed to set Sleep Mode register!\n");
		return ret;
	}

	if (pdata->sleep_vol)
		sv = pdata->sleep_vol;
	else
		sv = PG870_MIN_UV;
	data = choose_voltage(di, sv, sv);
	ret = regmap_write(di->regmap, PG870_SLEEP_VOL_REG, data);
	if (ret < 0) {
		dev_err(di->dev, "Failed to set Sleep Voltage register!\n");
		return ret;
	}

	/* Voltage select table idx */
	if (pdata->vsel_idx)
		di->vsel_idx = pdata->vsel_idx;
	else
		di->vsel_idx = 0;

	return ret;
}

static int voltage_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pg870_device_info *di = i2c_get_clientdata(client);
	int ret;

	ret = pg870_get_voltage(di->rdev);
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
	struct pg870_device_info *di = i2c_get_clientdata(client);
	unsigned int vol, sel;
	int ret;

	vol = simple_strtoul(buf, NULL, 10);
	ret = pg870_set_voltage(di->rdev, vol, vol, &sel);
	if (ret < 0)
		dev_err(dev, "Can't set voltage!\n");
	return count;
}

static DEVICE_ATTR(voltage, S_IRUGO | S_IWUSR, voltage_show, voltage_store);

static int regs_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct pg870_device_info *di = i2c_get_clientdata(client);
	unsigned int val;
	int cnt = 0, ret, i;

	/* STATUS */
	ret = regmap_read(di->regmap, PG870_STATUS_REG, &val);
	if (ret < 0)
		return ret;
	cnt += sprintf(buf + cnt, "STATUS:  0x%02x\n", val);
	/* SYSCTRL */
	ret = regmap_read(di->regmap, PG870_SYSCTRL_REG, &val);
	if (ret < 0)
		return ret;
	cnt += sprintf(buf + cnt, "SYSCTRL: 0x%02x\n", val);
	/* SLEEP MODE */
	ret = regmap_read(di->regmap, PG870_SLEEP_MODE_REG, &val);
	if (ret < 0)
		return ret;
	cnt += sprintf(buf + cnt, "SLP_MOD: 0x%02x\n", val);
	/* VOL SETTING */
	for (i = 0; i < 4; i++) {
		ret = regmap_read(di->regmap, vsel_reg_map[i], &val);
		if (ret < 0)
			return ret;
		cnt += sprintf(buf + cnt, "VOL%d:    0x%02x\n", i, val);
	}
	/* SLEEP VOL SETTING */
	ret = regmap_read(di->regmap, PG870_SLEEP_VOL_REG, &val);
	if (ret < 0)
		return ret;
	cnt += sprintf(buf + cnt, "SLP_VOL: 0x%02x\n", val);
	/* BUCK CONTROL */
	ret = regmap_read(di->regmap, PG870_CONTROL_REG, &val);
	if (ret < 0)
		return ret;
	cnt += sprintf(buf + cnt, "CTRL:    0x%02x\n", val);
	/* ID */
	ret = regmap_read(di->regmap, PG870_CHIP_ID_REG, &val);
	if (ret < 0)
		return ret;
	cnt += sprintf(buf + cnt, "CHIP_ID: 0x%x\n", val);

	return cnt;
}

static DEVICE_ATTR(regs, S_IRUGO | S_IWUSR, regs_show, NULL);

static struct attribute *pg870_attributes[] = {
	&dev_attr_voltage.attr,
	&dev_attr_regs.attr,
	NULL,
};

static struct attribute_group pg870_attr_grp = {
	.attrs = pg870_attributes,
};

static int pg870_regulator_register(struct pg870_device_info *di,
				char *name, int num)
{
	struct regulator_desc *rdesc = &di->desc;

	rdesc->name = name;
	rdesc->id = num;
	rdesc->ops = &pg870_regulator_ops;
	rdesc->type = REGULATOR_VOLTAGE;
	rdesc->n_voltages = PG870_NVOLTAGES;
	rdesc->owner = THIS_MODULE;

	di->rdev = regulator_register(&di->desc, di->dev,
					di->regulator, di, NULL);
	if (IS_ERR(di->rdev))
		return PTR_ERR(di->rdev);
	return 0;

}

static struct regmap_config pg870_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int __devinit pg870_regulator_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct pg870_device_info *di;
	struct pg870_platform_data *pdata;

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
	di->regmap = regmap_init_i2c(client, &pg870_regmap_config);
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
	di->chip_id = get_chip_id(di);
	if (di->chip_id < 0) {
		dev_err(&client->dev, "Failed to get chip ID!\n");
		ret = -ENODEV;
		goto err_get_chip_id;
	}
	dev_info(&client->dev, "88PG870 Rev[0X%X] Detected!\n",
				di->chip_id);
	/* Device init */
	ret = pg870_device_setup(di, pdata);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to setup device!\n");
		goto err_device_setup;
	}
	/* Register regulator */
	ret = pg870_regulator_register(di, name, num);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to register regulator!\n");
		goto err_rdev_register;
	}

	/* Create sysfs interface */
	ret = sysfs_create_group(&client->dev.kobj, &pg870_attr_grp);
	if (ret) {
		dev_err(&client->dev, "Failed to create sysfs group!\n");
		goto err_create_sysfs;
	}

	return 0;

err_create_sysfs:
	regulator_unregister(di->rdev);
err_rdev_register:
err_device_setup:
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

static int __devexit pg870_regulator_remove(struct i2c_client *client)
{
	struct pg870_device_info *di = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &pg870_attr_grp);
	regulator_unregister(di->rdev);

	kfree(di->desc.name);
	mutex_lock(&regulator_mutex);
	idr_remove(&regulator_id, di->desc.id);
	mutex_unlock(&regulator_mutex);

	regmap_exit(di->regmap);
	devm_kfree(di->dev, di);
	return 0;
}

static const struct i2c_device_id pg870_id[] = {
	{"pg870", -1},
	{},
};

static struct i2c_driver pg870_regulator_driver = {
	.driver = {
		.name = "pg870-regulator",
	},
	.probe = pg870_regulator_probe,
	.remove = __devexit_p(pg870_regulator_remove),
	.id_table = pg870_id,
};

static int __init pg870_init(void)
{
	int ret;
	ret = i2c_add_driver(&pg870_regulator_driver);
	if (ret)
		pr_err("Unable to register 88PG870 regulator driver");
	return ret;
}
module_init(pg870_init);

static void __exit pg870_exit(void)
{
	i2c_del_driver(&pg870_regulator_driver);
}
module_exit(pg870_exit);

MODULE_AUTHOR("Yipeng Yao <ypyao@marvell.com>");
MODULE_DESCRIPTION("88PG870 regulator driver");
MODULE_LICENSE("GPL");

