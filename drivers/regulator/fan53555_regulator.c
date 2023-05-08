/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Fixes:
 *	0.2 unprotect dcdc/ldo before turn on and off
 *		remove dcdc/ldo calibration
 * To Fix:
 *
 *
 */
#include <linux/init.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/regulator/of_regulator.h>
#endif
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/adc.h>

#undef debug
#define debug(format, arg...) pr_info("regu: " "@@@%s: " format, __func__, ## arg)
#define debug0(format, arg...)	//pr_debug("regu: " "@@@%s: " format, __func__, ## arg)
#define debug2(format, arg...)	pr_debug("regu: " "@@@%s: " format, __func__, ## arg)
#define FAIRCHILD_DCDC_NAME			"fairchild_fan53555"
enum fan53555_regs {
	FAN53555_REG_VSEL_0 = 0x0,
	FAN53555_REG_VSEL_1 = 0x1,
	FAN53555_REG_CONTROL = 0x2,
	FAN53555_REG_ID1 = 0x3,
	FAN53555_REG_ID2 = 0x4,
	FAN53555_REG_MONITOR = 0x5,
	FAN53555_REG_MAX_NUM = FAN53555_REG_MONITOR
};

struct fan5xx_regs {
	int opt;
	u8 vsel;
	u8 vsel_msk;
	u32 min_uV, step_uV;
	u32 vol_def;
};

struct fan5xx_regulator_info {
	struct regulator_desc desc;
	struct regulator_init_data *init_data;
	struct fan5xx_regs regs;
	struct i2c_client *client;
};
static struct dentry *debugfs_root = NULL;
static atomic_t idx = ATOMIC_INIT(1);	/* 0: dummy */
static int fan5xx_write_reg(struct fan5xx_regulator_info *fan5xx_dcdc, u8 addr,
			    u8 para)
{
	struct i2c_msg msgs[1] = { 0 };
	u8 buf[2] = { 0 };
	int ret = -1, msg_len = 0;

	buf[0] = addr;
	buf[1] = para;

	msgs[0].addr = fan5xx_dcdc->client->addr;
	msgs[0].flags = 0;
	msgs[0].buf = buf;
	msgs[0].len = ARRAY_SIZE(buf);

	msg_len = ARRAY_SIZE(msgs);

	ret = i2c_transfer(fan5xx_dcdc->client->adapter, msgs, msg_len);
	if (ret != 1) {
		pr_err("%s i2c write error, ret %d\n", __func__, ret);
		return -EIO;
	}

	return 0;
}

static int fan5xx_read_reg(struct fan5xx_regulator_info *fan5xx_dcdc,
			   u8 reg_addr, u8 * pdata)
{
	int ret = 0, msg_len = 0;
	u8 *write_buf = &reg_addr;
	u8 *read_buf = pdata;

	struct i2c_msg msgs[] = {
		{
		 .addr = fan5xx_dcdc->client->addr,
		 .flags = 0,	//i2c write
		 .len = 1,
		 .buf = write_buf,
		 },
		{
		 .addr = fan5xx_dcdc->client->addr,
		 .flags = I2C_M_RD,	//i2c read
		 .len = 1,
		 .buf = read_buf,
		 },
	};
	msg_len = ARRAY_SIZE(msgs);

	ret = i2c_transfer(fan5xx_dcdc->client->adapter, msgs, msg_len);
	if (ret != msg_len) {
		pr_err("%s i2c read error, ret %d, msg_len %d\n", __func__, ret,
		       msg_len);
		ret = -EIO;
	}
	//*pdata = buf[1];

	return ret;
}

static int fan5xx_dcdc_turn_on(struct regulator_dev *rdev)
{
	struct fan5xx_regulator_info *fan5xx_dcdc = rdev_get_drvdata(rdev);
	struct fan5xx_regs *regs = &fan5xx_dcdc->regs;

	return 0;
}

static int fan5xx_dcdc_turn_off(struct regulator_dev *rdev)
{
	struct fan5xx_regulator_info *fan5xx_dcdc = rdev_get_drvdata(rdev);
	struct fan5xx_regs *regs = &fan5xx_dcdc->regs;

	return 0;
}

static int fan5xx_dcdc_is_on(struct regulator_dev *rdev)
{
	struct fan5xx_regulator_info *fan5xx_dcdc = rdev_get_drvdata(rdev);
	struct fan5xx_regs *regs = &fan5xx_dcdc->regs;

	return 1;
}

static int fan5xx_dcdc_get_voltage(struct regulator_dev *rdev)
{
	struct fan5xx_regulator_info *fan5xx_dcdc = rdev_get_drvdata(rdev);
	struct fan5xx_regs *regs = &fan5xx_dcdc->regs;
	int vol_uV = 0, ret = -EINVAL;
	u8 reg_addr = (u8) (regs->vsel);
	u8 reg_val = 0, shft = __ffs(regs->vsel_msk);

	ret = fan5xx_read_reg(fan5xx_dcdc, reg_addr, &reg_val);
	if (ret <= 0) {
		debug("read reg(%#x) data error! ret = %d\n", reg_addr, ret);
		return -1;
	}

	reg_val = (reg_val & regs->vsel_msk) >> shft;
	vol_uV = regs->min_uV + reg_val * regs->step_uV;

	debug("vddbigarm get voltage %d\n", vol_uV);

	return vol_uV;		/* uV */
}

static int fan5xx_dcdc_set_voltage(struct regulator_dev *rdev, int min_uV,
				   int max_uV, unsigned *selector)
{
	struct fan5xx_regulator_info *fan5xx_dcdc = rdev_get_drvdata(rdev);
	struct fan5xx_regs *regs = &fan5xx_dcdc->regs;
	int uV = min_uV, ret = -EINVAL;
	u8 reg_addr = (u8) (regs->vsel);
	u8 reg_val = 0, shft = __ffs(regs->vsel_msk);
	int old_vol = rdev->desc->ops->get_voltage(rdev);

	debug("vddbigarm set voltage, %d(uV) - %d(uV)\n", min_uV, max_uV);

	if (uV < regs->min_uV)
		return -1;

	ret = fan5xx_read_reg(fan5xx_dcdc, reg_addr, &reg_val);
	if (ret <= 0) {
		debug("read reg(%#x) data error! ret = %d\n", reg_addr, ret);
		return -2;
	}
	debug0("read reg(%#x) data(%#x)\n", reg_addr, reg_val);

	uV = (int)(uV - (int)regs->min_uV) / regs->step_uV;
	if (uV > (regs->vsel_msk >> shft))
		uV = regs->vsel_msk >> shft;

	reg_val &= ~regs->vsel_msk;
	reg_val |= (uV << shft);
	ret = fan5xx_write_reg(fan5xx_dcdc, reg_addr, reg_val);
	if (ret) {
		debug("write reg(%#x) data(%#x) error! ret = %d\n", reg_addr,
		      reg_val, ret);
		return -3;
	}
	debug("write reg(%#x) value(%#x)\n", reg_addr, reg_val);

	/* dcdc boost delay */
	if (min_uV > old_vol) {
		int dly = 0;
/* FIXME: for dcdc fan53555, each step slew_rate[i] uV takes 1us */
		//int slew_rate_list[8] = {64000, 32000, 16000, 8000, 4000, 2000, 1000, 500} /* unit: uV/us */;
		int slew_rate = 0;

		ret =
		    fan5xx_read_reg(fan5xx_dcdc, FAN53555_REG_CONTROL,
				    &reg_val);
		if (ret <= 0) {
			debug("read reg(0x2) data error! ret = %d\n", ret);
			return -4;
		}
		debug0("read reg(0x2) data(%#x)\n", reg_val);
		reg_val = (reg_val & 0x70) >> 4;
		slew_rate = ((64 * 1000) << reg_val);
		dly = (min_uV - old_vol) / slew_rate;
		//dly = (min_uV - old_vol) / slew_rate_list[reg_val];
		debug("slew_rate %d(uV/us), delay_time %d(us)\n", slew_rate,
		      dly);
		WARN_ON(dly > 1000);
		udelay(dly);
	}

	return 0;
}

static struct regulator_ops fan5xx_dcdc_ops = {
	.enable = fan5xx_dcdc_turn_on,
	.disable = fan5xx_dcdc_turn_off,
	.is_enabled = fan5xx_dcdc_is_on,
	.set_voltage = fan5xx_dcdc_set_voltage,
	.get_voltage = fan5xx_dcdc_get_voltage,
};

static int debugfs_voltage_get(void *data, u64 * val)
{
	struct regulator_dev *rdev = data;
	if (rdev)
		if (0 == strcmp(rdev->desc->name, "vddbigarm"))
			if (rdev->desc->ops->get_voltage)
				*val = rdev->desc->ops->get_voltage(rdev);
			else
				*val = -1;
	return 0;
}

static int debugfs_enable_get(void *data, u64 * val)
{
	struct regulator_dev *rdev = data;
	if (rdev && rdev->desc->ops->is_enabled)
		*val = rdev->desc->ops->is_enabled(rdev);
	else
		*val = -1;
	return 0;
}

static int debugfs_enable_set(void *data, u64 val)
{
	struct regulator_dev *rdev = data;
	if (rdev && rdev->desc->ops->enable)
		(val) ? rdev->desc->ops->enable(rdev)
		    : rdev->desc->ops->disable(rdev);
	return 0;
}

static int debugfs_voltage_set(void *data, u64 val)
{
	struct regulator_dev *rdev = data;
	u32 min_uV;
	if (rdev && rdev->desc->ops->set_voltage) {
		if (0 != strcmp(rdev->desc->name, "vddbigarm"))
			min_uV = (u32) val *1000;
		else
			min_uV = (u32) val;
		min_uV += rdev->constraints->uV_offset;
		rdev->desc->ops->set_voltage(rdev, min_uV, min_uV, 0);
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_enable,
			debugfs_enable_get, debugfs_enable_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_ldo,
			debugfs_voltage_get, debugfs_voltage_set, "%llu\n");
static void ext_dcdc_init_debugfs(struct regulator_dev *rdev)
{
	struct fan5xx_regulator_info *fan5xx_dcdc = rdev_get_drvdata(rdev);
	struct dentry *regu_debugfs = NULL;

	regu_debugfs = debugfs_create_dir(rdev->desc->name, debugfs_root);
	if (IS_ERR_OR_NULL(regu_debugfs)) {
		pr_warn("Failed to create (%s) debugfs directory\n",
			rdev->desc->name);
		rdev->debugfs = NULL;
		return;
	}

	debugfs_create_file("enable", S_IRUGO | S_IWUSR,
			    regu_debugfs, rdev, &fops_enable);

	debugfs_create_file("voltage", S_IRUGO | S_IWUSR,
			    regu_debugfs, rdev, &fops_ldo);
}

static const unsigned short fan5xx_addr_list[] = {
	0x60,			/* 0xc0 >> 1 */
	I2C_CLIENT_END
};

static const struct i2c_device_id fan53555_i2c_id[] = {
	{FAIRCHILD_DCDC_NAME, 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id fan53555_of_match[] = {
	{.compatible = "fairchild,fairchild_fan53555",},
	{}
};
#endif
static inline int __strcmp(const char *cs, const char *ct)
{
	if (!cs || !ct)
		return -1;

	return strcmp(cs, ct);
}

static struct regulator_consumer_supply *set_supply_map(struct device *dev,
							const char *supply_name,
							int *num)
{
	char **map = (char **)dev_get_platdata(dev);
	int i, n;
	struct regulator_consumer_supply *consumer_supplies = NULL;

	if (!supply_name || !(map && map[0]))
		return NULL;

	for (i = 0; map[i] || map[i + 1]; i++) {
		if (map[i] && 0 == strcmp(map[i], supply_name))
			break;
	}

	/* i++; *//* Do not skip supply name */

	for (n = 0; map[i + n]; n++) ;

	if (n) {
		debug0("supply %s consumers %d - %d\n", supply_name, i, n);
		consumer_supplies =
		    kzalloc(n * sizeof(*consumer_supplies), GFP_KERNEL);
		BUG_ON(!consumer_supplies);
		for (n = 0; map[i]; i++, n++) {
			consumer_supplies[n].supply = map[i];
		}
		if (num)
			*num = n;
	}
	return consumer_supplies;
}

static int fan53555_regulator_parse_dt(struct device *dev,
				       struct device_node *np,
				       struct fan5xx_regulator_info *desc,
				       struct regulator_consumer_supply *supply,
				       int sz)
{
	/*struct sci_regulator_regs *regs = &desc->regs; */
	struct fan5xx_regs *regs = &desc->regs;
	const __be32 *tmp;
	u32 data[8] = { 0 };
	u32 tmp_val_u32;
	int type = 0, cnt = 0, ret = 0;

	if (!dev || !np || !desc || !supply) {
		return -EINVAL;
	}

	desc->desc.name = np->name;
	desc->desc.id = (atomic_inc_return(&idx) - 1);
	desc->desc.type = REGULATOR_VOLTAGE;
	desc->desc.owner = THIS_MODULE;

	supply[0].dev_name = NULL;
	supply[0].supply = np->name;
	desc->init_data = of_get_regulator_init_data(dev, np);
	if (!desc->init_data
	    || 0 != __strcmp(desc->init_data->constraints.name, np->name)) {
		dev_err(dev,
			"failed to parse regulator(%s) init data! \n",
			np->name);
		return -EINVAL;
	}

	desc->init_data->supply_regulator = 0;
	desc->init_data->constraints.valid_modes_mask =
	    REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY;
	desc->init_data->constraints.valid_ops_mask =
	    REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS |
	    REGULATOR_CHANGE_VOLTAGE;
	desc->init_data->num_consumer_supplies = sz;

	desc->init_data->consumer_supplies =
	    set_supply_map(dev, desc->desc.name,
			   &desc->init_data->num_consumer_supplies);

	if (!desc->init_data->consumer_supplies)
		desc->init_data->consumer_supplies = supply;

	/* Fill struct sci_regulator_regs variable desc->regs */

	regs->min_uV = desc->init_data->constraints.min_uV;

	ret =
	    of_property_read_u32(np, "regulator-step-microvolt", &tmp_val_u32);
	if (!ret)
		regs->step_uV = tmp_val_u32;

	ret =
	    of_property_read_u32(np, "regulator-default-microvolt",
				 &tmp_val_u32);
	if (!ret)
		regs->vol_def = tmp_val_u32;

	debug("[%d] %s type %d, range %d(uV) - %d(uV), step %d(uV) \n",
	      (idx.counter - 1), np->name, type,
	      desc->init_data->constraints.min_uV,
	      desc->init_data->constraints.max_uV, regs->step_uV);

	return 0;
}

static int fan53555_parse_dt(struct device *dev)
{
	struct device_node *dev_np = dev->of_node;
	struct device_node *child_np;
	for_each_child_of_node(dev_np, child_np) {
	}
	return 0;
}

static int fan53335_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int ret = 0;
	u8 chipid = 0;

	struct fan5xx_regulator_info *fan5xx_dcdc = NULL;
	struct regulator_dev *rdev = NULL;
	struct regulator_consumer_supply consumer_supplies_default[1] = { };
	struct regulator_config config = { };
	//add start
	struct device *dev = &client->dev;
	struct device_node *dev_np = dev->of_node;
	struct device_node *child_np;
	struct regulator_init_data init_data = {
		.supply_regulator = 0,
		.regulator_init = 0,
		.driver_data = 0,
	};

	pr_info("%s -- enter probe -->\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		return -ENODEV;
	}

	fan5xx_dcdc = kzalloc(sizeof(*fan5xx_dcdc), GFP_KERNEL);
	if (!fan5xx_dcdc) {
		pr_err("failed allocate memory for fan5xx_regulator_info!\n");
		return -ENOMEM;
	}
	for_each_child_of_node(dev_np, child_np) {
		ret =
		    fan53555_regulator_parse_dt(dev, child_np, fan5xx_dcdc,
						consumer_supplies_default,
						ARRAY_SIZE
						(consumer_supplies_default));
	}

	fan5xx_dcdc->client = client;
	i2c_set_clientdata(client, fan5xx_dcdc);
	fan5xx_dcdc->regs.opt = 18;	/* FAN53555 BUC 18x */
	fan5xx_dcdc->regs.vsel = FAN53555_REG_VSEL_0;
	fan5xx_dcdc->regs.vsel_msk = 0x3F;

	/* read fan53555 chipid */
	ret = fan5xx_read_reg(fan5xx_dcdc, FAN53555_REG_ID1, &chipid);	/* chipid = 0x88 */
	if (ret > 0)
		pr_info("%s fan53555 chipid %#x\n", __func__, chipid);
	else
		pr_err("%s fan53555 read chipid error!\n", __func__);

	//fan5xx_write_reg(fan5xx_dcdc, FAN53555_REG_CONTROL, 0x0); /* set slew rate to 64mV/us */

	fan5xx_dcdc->desc.type = REGULATOR_VOLTAGE;
	fan5xx_dcdc->desc.ops = &fan5xx_dcdc_ops;

	config.dev = &client->dev;
	config.init_data = fan5xx_dcdc->init_data;	//&init_data;
	config.driver_data = fan5xx_dcdc;
	config.of_node = NULL;
	rdev = regulator_register(&fan5xx_dcdc->desc, &config);
	if (!IS_ERR_OR_NULL(rdev)) {
		pr_info("%s regulator vddbigarm ok!\n", __func__);
		rdev->reg_data = fan5xx_dcdc;
		ext_dcdc_init_debugfs(rdev);
		fan5xx_dcdc->desc.ops->set_voltage(rdev,
						   fan5xx_dcdc->regs.vol_def,
						   fan5xx_dcdc->regs.vol_def,
						   0);
	}

	pr_info("%s -- exit probe -->\n", __func__);

	return 0;
}

static int fan53335_remove(struct i2c_client *client)
{
	struct fan5xx_regulator_info *fan5xx_dcdc = i2c_get_clientdata(client);

	if (fan5xx_dcdc) {
		kfree(fan5xx_dcdc);
		fan5xx_dcdc = NULL;
	}

	return 0;
}

static struct i2c_driver fan5xx_i2c_driver = {
	.probe = fan53335_probe,
	.remove = fan53335_remove,
	.id_table = fan53555_i2c_id,
	.driver = {
		   .name = FAIRCHILD_DCDC_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = fan53555_of_match,
		   },
};

static int __init fan53555_regulator_init(void)
{
	return i2c_add_driver(&fan5xx_i2c_driver);
}

static void __exit fan53555_regulator_exit(void)
{
	i2c_del_driver(&fan5xx_i2c_driver);
}

subsys_initcall(fan53555_regulator_init);
module_exit(fan53555_regulator_exit);
MODULE_DESCRIPTION("fairchild fan53555 regulator driver");
MODULE_LICENSE("GPL");
