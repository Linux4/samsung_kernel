/*
 * s2mpb06_regulator.c - Regulator driver for the Samsung s2mpb06
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
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/samsung/pmic/s2mpb06.h>
#include <linux/samsung/pmic/s2mpb06-regulator.h>
#include <linux/regulator/of_regulator.h>
#include <linux/samsung/pmic/pmic_class.h>

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
struct s2mpb06_sysfs_dev {
	uint8_t read_addr;
	uint8_t read_val;
	struct device *dev;
};
#endif

struct s2mpb06_data {
	struct s2mpb06_dev *iodev;
	int num_regulators;
	struct regulator_dev *rdev[S2MPB06_REGULATOR_MAX];
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct s2mpb06_sysfs_dev *sysfs;
#endif
};

static int s2m_enable(struct regulator_dev *rdev)
{
	struct s2mpb06_data *info = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = info->iodev->i2c;

	return s2mpb06_update_reg(i2c, rdev->desc->enable_reg,
					rdev->desc->enable_mask,
					rdev->desc->enable_mask);
}

static int s2m_disable_regmap(struct regulator_dev *rdev)
{
	struct s2mpb06_data *info = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = info->iodev->i2c;
	uint8_t val;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;
	else
		val = 0;

	return s2mpb06_update_reg(i2c, rdev->desc->enable_reg,
				   val, rdev->desc->enable_mask);
}

static int s2m_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2mpb06_data *info = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = info->iodev->i2c;
	int ret;
	uint8_t val;

	ret = s2mpb06_read_reg(i2c, rdev->desc->enable_reg, &val);
	if (ret < 0)
		return ret;

	val &= rdev->desc->enable_mask;

	return (val == rdev->desc->enable_mask);
}

static int s2m_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2mpb06_data *info = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = info->iodev->i2c;
	int ret;
	uint8_t val;

	ret = s2mpb06_read_reg(i2c, rdev->desc->vsel_reg, &val);
	if (ret < 0)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}

static int s2m_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2mpb06_data *info = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = info->iodev->i2c;
	int ret;

	ret = s2mpb06_update_reg(i2c, rdev->desc->vsel_reg,
				sel, rdev->desc->vsel_mask);
	if (ret < 0)
		goto out;

	if (rdev->desc->apply_bit)
		ret = s2mpb06_update_reg(i2c, rdev->desc->apply_reg,
					 rdev->desc->apply_bit,
					 rdev->desc->apply_bit);
	return ret;
out:
	pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
	return ret;
}

static int s2m_set_voltage_time_sel(struct regulator_dev *rdev,
				   unsigned int old_selector,
				   unsigned int new_selector)
{
	int old_volt, new_volt;

	/* sanity check */
	if (!rdev->desc->ops->list_voltage)
		return -EINVAL;

	old_volt = rdev->desc->ops->list_voltage(rdev, old_selector);
	new_volt = rdev->desc->ops->list_voltage(rdev, new_selector);

	if (old_selector < new_selector)
		return DIV_ROUND_UP(new_volt - old_volt, S2MPB06_RAMP_DELAY);

	return 0;
}

static struct regulator_ops s2mpb06_ldo_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2m_is_enabled_regmap,
	.enable			= s2m_enable,
	.disable		= s2m_disable_regmap,
	.get_voltage_sel	= s2m_get_voltage_sel_regmap,
	.set_voltage_sel	= s2m_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2m_set_voltage_time_sel,
};

#define _LDO(macro)	S2MPB06_LDO##macro
#define _REG(ctrl)	S2MPB06_REG##ctrl
#define _ldo_ops(num)	s2mpb06_ldo_ops##num
#define _TIME(macro)	S2MPB06_ENABLE_TIME##macro

#define LDO_DESC(_name, _id, _ops, m, s, v, e)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.ops		= _ops,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= m,					\
	.uV_step	= s,					\
	.n_voltages	= S2MPB06_LDO_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPB06_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPB06_LDO_ENABLE_MASK,		\
	.enable_time	= _TIME(_LDO),				\
}

static struct regulator_desc regulators[S2MPB06_REGULATOR_MAX] = {
	/* name, id, ops, min_uv, uV_step, vsel_reg, enable_reg */
	LDO_DESC("s2mpb06-ldo1", _LDO(1), &_ldo_ops(), _LDO(_MIN1),
		 _LDO(_STEP1), _REG(_LDO1_OUTPUT), _REG(_LDO1_CTRL)),
	LDO_DESC("s2mpb06-ldo2", _LDO(2), &_ldo_ops(), _LDO(_MIN1),
		 _LDO(_STEP1), _REG(_LDO2_OUTPUT), _REG(_LDO2_CTRL)),
	LDO_DESC("s2mpb06-ldo3", _LDO(3), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_LDO3_OUTPUT), _REG(_LDO3_CTRL)),
	LDO_DESC("s2mpb06-ldo4", _LDO(4), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_LDO4_OUTPUT), _REG(_LDO4_CTRL)),
	LDO_DESC("s2mpb06-ldo5", _LDO(5), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_LDO5_OUTPUT), _REG(_LDO5_CTRL)),
	LDO_DESC("s2mpb06-ldo6", _LDO(6), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_LDO6_OUTPUT), _REG(_LDO6_CTRL)),
	LDO_DESC("s2mpb06-ldo7", _LDO(7), &_ldo_ops(), _LDO(_MIN2),
		 _LDO(_STEP2), _REG(_LDO7_OUTPUT), _REG(_LDO7_CTRL)),
};

#if IS_ENABLED(CONFIG_OF)
static int s2mpb06_pmic_dt_parse_pdata(struct s2mpb06_dev *iodev,
					struct s2mpb06_platform_data *pdata)
{
	struct device_node *pmic_np, *regulators_np, *reg_np;
	struct s2mpb06_regulator_data *rdata;
	size_t i;

	pmic_np = iodev->dev->of_node;
	if (!pmic_np) {
		dev_err(iodev->dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}

	regulators_np = of_find_node_by_name(pmic_np, "regulators");
	if (!regulators_np) {
		dev_err(iodev->dev, "could not find regulators sub-node\n");
		return -EINVAL;
	}

	/* count the number of regulators to be supported in pmic */
	pdata->num_regulators = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		pdata->num_regulators++;
	}

	rdata = devm_kzalloc(iodev->dev, sizeof(*rdata) *
				pdata->num_regulators, GFP_KERNEL);
	if (!rdata)
		return -ENOMEM;

	pdata->regulators = rdata;
	pdata->num_rdata = 0;
	for_each_child_of_node(regulators_np, reg_np) {
		for (i = 0; i < ARRAY_SIZE(regulators); i++)
			if (!of_node_cmp(reg_np->name, regulators[i].name))
				break;

		if (i == ARRAY_SIZE(regulators)) {
			dev_warn(iodev->dev,
			"don't know how to configure regulator %s\n",
			reg_np->name);
			continue;
		}

		rdata->id = i;
		rdata->initdata = of_get_regulator_init_data(iodev->dev, reg_np,
							     &regulators[i]);
		rdata->reg_node = reg_np;
		rdata++;
		pdata->num_rdata++;
	}
	of_node_put(regulators_np);

	return 0;
}
#else
static int s2mpb06_pmic_dt_parse_pdata(struct s2mpb06_dev *iodev,
					struct s2mpb06_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t s2mpb06_read_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct s2mpb06_data *s2mpb06 = dev_get_drvdata(dev);
	int ret;
	uint8_t val, reg_addr;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return -1;
	}

	ret = kstrtou8(buf, 0, &reg_addr);
	if (ret < 0)
		pr_info("%s: fail to transform i2c address\n", __func__);

	ret = s2mpb06_read_reg(s2mpb06->iodev->i2c, reg_addr, &val);
	if (ret < 0)
		pr_info("%s: fail to read i2c address\n", __func__);

	pr_info("%s: reg(0x%02hhx) data(0x%02hhx)\n", __func__, reg_addr, val);
	s2mpb06->sysfs->read_addr = reg_addr;
	s2mpb06->sysfs->read_val = val;

	return size;
}

static ssize_t s2mpb06_read_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct s2mpb06_data *s2mpb06 = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02hhx: 0x%02hhx\n", s2mpb06->sysfs->read_addr,
				s2mpb06->sysfs->read_val);
}

static ssize_t s2mpb06_write_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct s2mpb06_data *s2mpb06 = dev_get_drvdata(dev);
	int ret;
	uint8_t reg = 0, data = 0;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return size;
	}

	ret = sscanf(buf, "0x%02hhx 0x%02hhx", &reg, &data);
	if (ret != 2) {
		pr_info("%s: input error\n", __func__);
		return size;
	}

	pr_info("%s: reg(0x%02hhx) data(0x%02hhx)\n", __func__, reg, data);

	ret = s2mpb06_write_reg(s2mpb06->iodev->i2c, reg, data);
	if (ret < 0)
		pr_info("%s: fail to write i2c addr/data\n", __func__);

	return size;
}

static ssize_t s2mpb06_write_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "echo (register addr.) (data) > s2mpb06_write\n");
}

static struct pmic_device_attribute regulator_attr[] = {
	PMIC_ATTR(write, 0644, s2mpb06_write_show, s2mpb06_write_store),
	PMIC_ATTR(read, 0644, s2mpb06_read_show, s2mpb06_read_store),
};

static int s2mpb06_create_sysfs(struct s2mpb06_data *s2mpb06)
{
	struct device *dev = s2mpb06->iodev->dev;
	struct device *sysfs_dev = NULL;
	char device_name[32] = {0, };
	int ret = 0, i = 0;

	pr_info("%s()\n", __func__);

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mpb06->sysfs = devm_kzalloc(dev, sizeof(struct s2mpb06_sysfs_dev),
					GFP_KERNEL);
	s2mpb06->sysfs->dev = pmic_device_create(s2mpb06, device_name);
	sysfs_dev = s2mpb06->sysfs->dev;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++) {
		ret = device_create_file(sysfs_dev, &regulator_attr[i].dev_attr);
		if (ret)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(sysfs_dev, &regulator_attr[i].dev_attr);
	pmic_device_destroy(sysfs_dev->devt);

	return -EINVAL;
}
#endif

static int s2mpb06_pmic_probe(struct platform_device *pdev)
{
	struct s2mpb06_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2mpb06_platform_data *pdata = iodev->pdata;
	struct regulator_config config = { };
	struct s2mpb06_data *s2mpb06;
	struct i2c_client *i2c;
	int i = 0, ret = 0;

	dev_info(&pdev->dev, "%s start\n", __func__);

	ret = s2mpb06_pmic_dt_parse_pdata(iodev, pdata);
	if (ret)
		goto err;

	s2mpb06 = devm_kzalloc(&pdev->dev, sizeof(struct s2mpb06_data),
				GFP_KERNEL);
	if (!s2mpb06)
		return -ENOMEM;

	s2mpb06->iodev = iodev;
	s2mpb06->num_regulators = pdata->num_rdata;
	platform_set_drvdata(pdev, s2mpb06);
	i2c = s2mpb06->iodev->i2c;

	config.dev = &pdev->dev;
	config.driver_data = s2mpb06;
	for (i = 0; i < pdata->num_rdata; i++) {
		int id = pdata->regulators[i].id;

		config.init_data = pdata->regulators[i].initdata;
		config.of_node = pdata->regulators[i].reg_node;
		s2mpb06->rdev[i] = devm_regulator_register(&pdev->dev,
							   &regulators[id], &config);
		if (IS_ERR(s2mpb06->rdev[i])) {
			ret = PTR_ERR(s2mpb06->rdev[i]);
			dev_err(&pdev->dev, "regulator init failed for %d\n", i);
			s2mpb06->rdev[i] = NULL;
			goto err;
		}
	}
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	ret = s2mpb06_create_sysfs(s2mpb06);
	if (ret < 0) {
		pr_err("%s: s2mpb06_create_sysfs fail\n", __func__);
		return -ENODEV;
	}
#endif
	dev_info(&pdev->dev, "%s end\n", __func__);

	return 0;

err:
	return ret;
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static void s2mpb06_remove_sysfs_entries(struct device *sysfs_dev)
{
	uint32_t i = 0;

	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++)
		device_remove_file(sysfs_dev, &regulator_attr[i].dev_attr);
	pmic_device_destroy(sysfs_dev->devt);
}
#endif

static int s2mpb06_pmic_remove(struct platform_device *pdev)
{
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct s2mpb06_data *s2mpb06 = platform_get_drvdata(pdev);

	s2mpb06_remove_sysfs_entries(s2mpb06->sysfs->dev);
#endif
	dev_info(&pdev->dev, "%s\n", __func__);
	return 0;
}

static const struct platform_device_id s2mpb06_pmic_id[] = {
	{"s2mpb06-regulator", 0},
	{},
};
MODULE_DEVICE_TABLE(platform, s2mpb06_pmic_id);

static struct platform_driver s2mpb06_pmic_driver = {
	.driver = {
		   .name = "s2mpb06-regulator",
		   .owner = THIS_MODULE,
		   .suppress_bind_attrs = true,
		   },
	.probe = s2mpb06_pmic_probe,
	.remove = s2mpb06_pmic_remove,
	.id_table = s2mpb06_pmic_id,
};

static int __init s2mpb06_pmic_init(void)
{
	return platform_driver_register(&s2mpb06_pmic_driver);
}
subsys_initcall(s2mpb06_pmic_init);

static void __exit s2mpb06_pmic_exit(void)
{
	platform_driver_unregister(&s2mpb06_pmic_driver);
}
module_exit(s2mpb06_pmic_exit);

MODULE_DESCRIPTION("SAMSUNG S2MPB06 Regulator Driver");
MODULE_LICENSE("GPL");
