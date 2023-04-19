/*
 * s2dps01.c - Regulator driver for the Samsung s2dps01
 *
 * Copyright (C) 2022 Samsung Electronics
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/samsung/pmic/s2dps01.h>
#include <linux/regulator/of_regulator.h>
#include <linux/samsung/pmic/pmic_class.h>

struct s2dps01_data {
	bool initialized;
	struct s2dps01_dev *iodev;
	struct regulator_dev *rdev;
};
struct s2dps01_data *s2dps01_private;

struct i2c_seq_type {
	u8 reg;
	u8 val;
};

struct i2c_seq_type s2dps01_init_seq[] = {
	{0x1C, 0x13},
	{0x1D, 0xCF},
	{0x1E, 0x70},
	{0x1F, 0x0A},
	{0x21, 0x0F},
	{0x22, 0x60},
	{0x23, 0x00},
	{0x24, 0x02},
	{0x25, 0x01},
	{0x26, 0x02},
	{0x11, 0xDF},
};

struct i2c_seq_type s2dps01_on_seq[] = {
	{0x20, 0x01},
	{0x22, 0x07},
	{0x23, 0xff},
};

struct i2c_seq_type s2dps01_off_seq[] = {
	{0x24, 0x01},
	{0x20, 0x00},
};

int s2dps01_init_reg(struct i2c_client *i2c)
{
	struct s2dps01_data *info = i2c_get_clientdata(i2c);
	struct s2dps01_dev *s2dps01 = info->iodev;
	int ret, i;
	u8 reg, val;

	mutex_lock(&s2dps01->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, S2DPS01_REGS_VPOS);
	if (ret >= 0) {
		if ((ret & 0xff) == S2DPS01_DEF_VPOS_DATA) {
			pr_info("%s:already initialzed\n", __func__);
			ret = 0;
			goto exit;
		}
	}

	for (i = 0; i < ARRAY_SIZE(s2dps01_init_seq); ++i) {
		reg = s2dps01_init_seq[i].reg;
		val = s2dps01_init_seq[i].val;

		ret = i2c_smbus_write_byte_data(i2c, reg, val);
		if (ret < 0) {
			dev_err(&i2c->dev, "%s:failed:0x%02x, 0x%02x\n",
					__func__, reg, val);
		}
	}

exit:
	mutex_unlock(&s2dps01->i2c_lock);
	return 0;
}

static int s2dps01_regulator_setmode(struct regulator_dev *rdev, unsigned int mode)
{
	struct s2dps01_data *info = rdev_get_drvdata(rdev);
	struct s2dps01_dev *s2dps01 = info->iodev;
	struct i2c_client *i2c = s2dps01->i2c;
	int ret, i;
	u8 reg, val;

	mutex_lock(&s2dps01->i2c_lock);
	if (mode == REGULATOR_MODE_STANDBY) {
		for (i = 0; i < ARRAY_SIZE(s2dps01_off_seq); ++i) {
			reg = s2dps01_on_seq[i].reg;
			val = s2dps01_on_seq[i].val;

			ret = i2c_smbus_write_byte_data(i2c, reg, val);
			if (ret < 0) {
				dev_err(&i2c->dev, "%s:failed:0x%02x, 0x%02x\n",
						__func__, reg, val);
			}
		}
	} else if (mode == REGULATOR_MODE_NORMAL) {
		for (i = 0; i < ARRAY_SIZE(s2dps01_on_seq); ++i) {
			reg = s2dps01_on_seq[i].reg;
			val = s2dps01_on_seq[i].val;

			ret = i2c_smbus_write_byte_data(i2c, reg, val);
			if (ret < 0) {
				dev_err(&i2c->dev, "%s:failed:0x%02x, 0x%02x\n",
						__func__, reg, val);
			}
		}

	} else {
		dev_info(&i2c->dev, "%s:not supported mode\n", __func__);
		ret = -EINVAL;
	}
	mutex_unlock(&s2dps01->i2c_lock);

	return ret;
};

static struct regulator_ops s2dps01_ops = {
	.set_mode	= s2dps01_regulator_setmode,
};

static struct regulator_desc regulators = {
	.name = "blrdev",
	.id = 0,
	.owner = THIS_MODULE,
	.ops = &s2dps01_ops,
};

static struct regulator_init_data regulator_data = {
	.constraints = {
		.valid_modes_mask = REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_MODE,
	},
};

static int s2dps01_register_regulator(struct s2dps01_data *sd)
{
	struct regulator_config config = { };
	struct s2dps01_dev *iodev = sd->iodev;
	struct i2c_client *i2c = iodev->i2c;
	struct device *dev = &i2c->dev;
	struct device_node *regulators_np, *reg_np;

	if (!dev->of_node)
		return -EINVAL;

	regulators_np = of_find_node_by_name(dev->of_node, "regulators");
	if (!regulators_np) {
		dev_err(dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	for_each_child_of_node(regulators_np, reg_np) {
		config.dev = &i2c->dev;
		config.driver_data = sd;
		config.init_data = &regulator_data;
		config.of_node = reg_np;
		sd->rdev = devm_regulator_register(&i2c->dev, &regulators, &config);
		if (IS_ERR(sd->rdev))
			dev_err(&i2c->dev, "regulator failed\n");
	}
	of_node_put(regulators_np);

	return 0;
}

static int s2dps01_pmic_probe(struct i2c_client *i2c,
				const struct i2c_device_id *dev_id)
{
	struct s2dps01_dev *iodev;
	struct s2dps01_data *s2dps01;
	int ret;

	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);

	iodev = devm_kzalloc(&i2c->dev, sizeof(struct s2dps01_dev), GFP_KERNEL);
	if (!iodev) {
		dev_err(&i2c->dev, "%s: Failed to alloc mem for s2dps01\n",
							__func__);
		return -ENOMEM;
	}

	iodev->dev = &i2c->dev;
	iodev->i2c = i2c;
	mutex_init(&iodev->i2c_lock);

	s2dps01 = devm_kzalloc(&i2c->dev, sizeof(struct s2dps01_data),
				GFP_KERNEL);
	if (!s2dps01) {
		dev_err(&i2c->dev, "[%s:%d] if (!s2dps01)\n", __FILE__, __LINE__);
		ret = -ENOMEM;
		goto err_s2dps01_data;
	}

	i2c_set_clientdata(i2c, s2dps01);
	s2dps01->iodev = iodev;

	s2dps01_register_regulator(s2dps01);

	ret = s2dps01_init_reg(iodev->i2c);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to init s2dps01 regulator\n");
		return ret;
	}

	s2dps01->initialized = true;
	s2dps01_private = s2dps01;

	pr_info("%s:%s finished\n", MFD_DEV_NAME, __func__);

	return ret;

err_s2dps01_data:
	mutex_destroy(&iodev->i2c_lock);
	return ret;
}

static int s2dps01_pmic_remove(struct i2c_client *i2c)
{
	dev_info(&i2c->dev, "%s\n", __func__);
	return 0;
}

static const struct i2c_device_id s2dps01_pmic_id[] = {
	{"s2dps01-regulator", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, s2dps01_pmic_id);

static struct of_device_id s2dps01_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2dps01pmic" },
	{ },
};
MODULE_DEVICE_TABLE(of, s2dps01_i2c_dt_ids);

static struct i2c_driver s2dps01_i2c_driver = {
	.driver = {
		.name = "s2dps01-regulator",
		.owner = THIS_MODULE,
		.of_match_table	= of_match_ptr(s2dps01_i2c_dt_ids),
	},
	.id_table = s2dps01_pmic_id,
	.probe = s2dps01_pmic_probe,
	.remove = s2dps01_pmic_remove,
};

static int __init s2dps01_i2c_init(void)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2dps01_i2c_driver);
}
module_init(s2dps01_i2c_init);

static void __exit s2dps01_i2c_exit(void)
{
	i2c_del_driver(&s2dps01_i2c_driver);
}
module_exit(s2dps01_i2c_exit);

MODULE_DESCRIPTION("SAMSUNG s2dos01 Regulator Driver");
MODULE_LICENSE("GPL");
