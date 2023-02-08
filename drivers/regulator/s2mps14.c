/*
 * s2mps14.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/samsung/core.h>
#include <linux/mfd/samsung/s2mps14.h>



struct s2mps14_info {
	struct device *dev;
	struct sec_pmic_dev *iodev;
	int num_regulators;
	struct regulator_dev **rdev;
	struct sec_opmode_data *opmode_data;

	int ramp_delay24;
	int ramp_delay3;
	int ramp_delay5;
	int ramp_delay16;
	int ramp_delay7;
	int ramp_delay8910;
	int ramp_delay12345;

	bool buck1_ramp;
	bool buck2_ramp;
	bool buck3_ramp;
	bool buck4_ramp;
	bool dvs1_en;
	bool dvs2_en;
	bool dvs3_en;
	bool dvs4_en;
	bool dvs6_en;
};

struct s2mps14_voltage_desc {
	int max;
	int min;
	int step;
};

static const struct s2mps14_voltage_desc buck_voltage_val1 = {
	.max = 1600000,
	.min =  400000,
	.step =   6250,
};

static const struct s2mps14_voltage_desc buck_voltage_val2 = {
	.max = 2387500,
	.min =  600000,
	.step =  12500,
};
static const struct s2mps14_voltage_desc ldo_voltage_val1 = {
	.max = 2375000,
	.min =  800000,
	.step =  25000,
};

static const struct s2mps14_voltage_desc ldo_voltage_val2 = {
	.max = 3375000,
	.min = 1800000,
	.step =  25000,
};

static const struct s2mps14_voltage_desc ldo_voltage_val3 = {
	.max = 1587500,
	.min =  800000,
	.step =  12500,
};


static const struct s2mps14_voltage_desc *reg_voltage_map[] = {
	[S2MPS14_LDO1] = &ldo_voltage_val3,
	[S2MPS14_LDO2] = &ldo_voltage_val3,
	[S2MPS14_LDO3] = &ldo_voltage_val1,
	[S2MPS14_LDO4] = &ldo_voltage_val1,
	[S2MPS14_LDO5] = &ldo_voltage_val3,
	[S2MPS14_LDO6] = &ldo_voltage_val3,
	[S2MPS14_LDO7] = &ldo_voltage_val1,
	[S2MPS14_LDO8] = &ldo_voltage_val2,
	[S2MPS14_LDO9] = &ldo_voltage_val3,
	[S2MPS14_LDO10] = &ldo_voltage_val3,
	[S2MPS14_LDO11] = &ldo_voltage_val1,
	[S2MPS14_LDO12] = &ldo_voltage_val2,
	[S2MPS14_LDO13] = &ldo_voltage_val2,
	[S2MPS14_LDO14] = &ldo_voltage_val2,
	[S2MPS14_LDO15] = &ldo_voltage_val2,
	[S2MPS14_LDO16] = &ldo_voltage_val2,
	[S2MPS14_LDO17] = &ldo_voltage_val2,
	[S2MPS14_LDO18] = &ldo_voltage_val2,
	[S2MPS14_LDO19] = &ldo_voltage_val1,
	[S2MPS14_LDO20] = &ldo_voltage_val1,
	[S2MPS14_LDO21] = &ldo_voltage_val1,
	[S2MPS14_LDO22] = &ldo_voltage_val3,
	[S2MPS14_LDO23] = &ldo_voltage_val1,
	[S2MPS14_LDO24] = &ldo_voltage_val2,
	[S2MPS14_LDO25] = &ldo_voltage_val2,
	[S2MPS14_BUCK1] = &buck_voltage_val1,
	[S2MPS14_BUCK2] = &buck_voltage_val1,
	[S2MPS14_BUCK3] = &buck_voltage_val1,
	[S2MPS14_BUCK4] = &buck_voltage_val2,
	[S2MPS14_BUCK5] = &buck_voltage_val1,
};

static int s2mps14_list_voltage(struct regulator_dev *rdev,
				unsigned int selector)
{
	const struct s2mps14_voltage_desc *desc;
	int reg_id = rdev_get_id(rdev);
	int val;

	if (reg_id >= ARRAY_SIZE(reg_voltage_map) || reg_id < 0)
		return -EINVAL;

	desc = reg_voltage_map[reg_id];
	if (desc == NULL)
		return -EINVAL;

	val = desc->min + desc->step * selector;
	if (val > desc->max)
		return -EINVAL;

	return val;
}

unsigned int s2mps14_opmode_reg[][3] = {
	{0x3, 0x2, 0x1}, /* LDO1 */
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1}, /* LDO25 */
	{0x3, 0x2, 0x1}, /* BUCK1 */
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1}, /* BUCK5 */
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
	{0x3, 0x2, 0x1},
};

static int s2mps14_get_register(struct regulator_dev *rdev,
	unsigned int *reg, int *pmic_en)
{
	int reg_id = rdev_get_id(rdev);
	unsigned int mode;
	struct s2mps14_info *s2mps14 = rdev_get_drvdata(rdev);

	switch (reg_id) {
	case S2MPS14_LDO1 ... S2MPS14_LDO25:
		*reg = S2MPS14_REG_L1CTRL + (reg_id - S2MPS14_LDO1);
		break;
	case S2MPS14_BUCK1 ... S2MPS14_BUCK5:
		*reg = S2MPS14_REG_B1CTRL1 + (reg_id - S2MPS14_BUCK1) * 2;
		break;
	case S2MPS14_AP_EN32KHZ ... S2MPS14_BT_EN32KHZ:
		*reg = S2MPS14_REG_RTC_BUF;
		*pmic_en = 0x01 << (reg_id - S2MPS14_AP_EN32KHZ);
		return 0;
	default:
		return -EINVAL;
	}

	mode = s2mps14->opmode_data[reg_id].mode;
	*pmic_en = s2mps14_opmode_reg[reg_id][mode] << S2MPS14_PMIC_EN_SHIFT;

	return 0;
}

static int s2mps14_reg_is_enabled(struct regulator_dev *rdev)
{
	struct s2mps14_info *s2mps14 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg, val;
	int ret, mask = 0xc0, pmic_en;

	ret = s2mps14_get_register(rdev, &reg, &pmic_en);
	if (ret == -EINVAL)
		return 1;
	else if (ret)
		return ret;

	ret = sec_reg_read(s2mps14->iodev, reg, &val);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPS14_LDO1 ... S2MPS14_BUCK5:
		mask = 0xc0;
		break;
	case S2MPS14_AP_EN32KHZ:
		mask = 0x01;
		break;
	case S2MPS14_CP_EN32KHZ:
		mask = 0x02;
		break;
	case S2MPS14_BT_EN32KHZ:
		mask = 0x04;
		break;
	default:
		return -EINVAL;
	}

	return (val & mask) == pmic_en;
}

static int s2mps14_reg_enable(struct regulator_dev *rdev)
{
	struct s2mps14_info *s2mps14 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	int ret, mask, pmic_en;

	ret = s2mps14_get_register(rdev, &reg, &pmic_en);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPS14_LDO1 ... S2MPS14_BUCK5:
		mask = 0xc0;
		break;
	case S2MPS14_AP_EN32KHZ:
		mask = 0x01;
		break;
	case S2MPS14_CP_EN32KHZ:
		mask = 0x02;
		break;
	case S2MPS14_BT_EN32KHZ:
		mask = 0x04;
		break;
	default:
		return -EINVAL;
	}

	return sec_reg_update(s2mps14->iodev, reg, pmic_en, mask);
}

static int s2mps14_reg_disable(struct regulator_dev *rdev)
{
	struct s2mps14_info *s2mps14 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	int ret, mask, pmic_en;

	ret = s2mps14_get_register(rdev, &reg, &pmic_en);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPS14_LDO1 ... S2MPS14_BUCK5:
		mask = 0xc0;
		break;
	case S2MPS14_AP_EN32KHZ:
		mask = 0x01;
		break;
	case S2MPS14_CP_EN32KHZ:
		mask = 0x02;
		break;
	case S2MPS14_BT_EN32KHZ:
		mask = 0x04;
		break;
	default:
		return -EINVAL;
	}

	return sec_reg_update(s2mps14->iodev, reg, ~mask, mask);
}

static int s2mps14_get_voltage_register(struct regulator_dev *rdev,
		unsigned int *_reg)
{
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;

	switch (reg_id) {
	case S2MPS14_LDO1 ... S2MPS14_LDO25:
		reg = S2MPS14_REG_L1CTRL + (reg_id - S2MPS14_LDO1);
		break;
	case S2MPS14_BUCK1 ... S2MPS14_BUCK5:
		reg = S2MPS14_REG_B1CTRL2 + (reg_id - S2MPS14_BUCK1) * 2;
		break;
	default:
		return -EINVAL;
	}

	*_reg = reg;

	return 0;
}

static int s2mps14_get_voltage_sel(struct regulator_dev *rdev)
{
	struct s2mps14_info *s2mps14 = rdev_get_drvdata(rdev);
	int mask, ret;
	int reg_id = rdev_get_id(rdev);
	unsigned int reg, val;

	ret = s2mps14_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPS14_BUCK1 ... S2MPS14_BUCK5:
		mask = 0xff;
		break;
	case S2MPS14_LDO1 ... S2MPS14_LDO25:
		mask = 0x3f;
		break;
	default:
		return -EINVAL;
	}

	ret = sec_reg_read(s2mps14->iodev, reg, &val);
	if (ret)
		return ret;

	val &= mask;

	return val;
}

static inline int s2mps14_convert_voltage_to_sel(
		const struct s2mps14_voltage_desc *desc,
		int min_vol, int max_vol)
{
	int selector = 0;

	if (desc == NULL)
		return -EINVAL;

	if (max_vol < desc->min || min_vol > desc->max)
		return -EINVAL;

	selector = (min_vol - desc->min) / desc->step;

	if (desc->min + desc->step * selector > max_vol)
		return -EINVAL;

	return selector;
}

static int s2mps14_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned *selector)
{
	struct s2mps14_info *s2mps14 = rdev_get_drvdata(rdev);
	int min_vol = min_uV, max_vol = max_uV;
	const struct s2mps14_voltage_desc *desc;
	int ret, reg_id = rdev_get_id(rdev);
	unsigned int reg, mask;
	int sel;

	mask = (reg_id < S2MPS14_BUCK1) ? 0x3f : 0xff;

	desc = reg_voltage_map[reg_id];

	sel = s2mps14_convert_voltage_to_sel(desc, min_vol, max_vol);
	if (sel < 0)
		return sel;

	ret = s2mps14_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;
	ret = sec_reg_update(s2mps14->iodev, reg, sel, mask);
	*selector = sel;

	return ret;
}

static int s2mps14_set_voltage_time_sel(struct regulator_dev *rdev,
					     unsigned int old_sel,
					     unsigned int new_sel)
{
	struct s2mps14_info *s2mps14 = rdev_get_drvdata(rdev);
	const struct s2mps14_voltage_desc *desc;
	int reg_id = rdev_get_id(rdev);
	int ramp_delay = 0;

	switch (reg_id) {
	case S2MPS14_BUCK1 ... S2MPS14_BUCK5:
		ramp_delay = s2mps14->ramp_delay12345;
		break;
	default:
		return -EINVAL;
	}

	desc = reg_voltage_map[reg_id];

	if (((old_sel < new_sel) && (reg_id >= S2MPS14_BUCK1)) && ramp_delay) {
		return DIV_ROUND_UP(desc->step * (new_sel - old_sel),
			ramp_delay * 1000);
	}

	return 0;
}

static int s2mps14_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	struct s2mps14_info *s2mps14 = rdev_get_drvdata(rdev);
	int mask, ret;
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;

	ret = s2mps14_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPS14_BUCK1 ... S2MPS14_BUCK5:
		mask = 0xff;
		break;
	case S2MPS14_LDO1 ... S2MPS14_LDO25:
		mask = 0x3f;
		break;
	default:
		return -EINVAL;
	}

	return sec_reg_update(s2mps14->iodev, reg, selector, mask);
}

static int get_ramp_delay(int ramp_delay)
{
	unsigned char cnt = 0;

	ramp_delay /= 6;

	while (true) {
		ramp_delay = ramp_delay >> 1;
		if (ramp_delay == 0)
			break;
		cnt++;
	}
	return cnt;
}

static struct regulator_ops s2mps14_ldo_ops = {
	.list_voltage		= s2mps14_list_voltage,
	.is_enabled		= s2mps14_reg_is_enabled,
	.enable			= s2mps14_reg_enable,
	.disable		= s2mps14_reg_disable,
	.get_voltage_sel	= s2mps14_get_voltage_sel,
	.set_voltage		= s2mps14_set_voltage,
	.set_voltage_time_sel	= s2mps14_set_voltage_time_sel,
};

static struct regulator_ops s2mps14_buck_ops = {
	.list_voltage		= s2mps14_list_voltage,
	.is_enabled		= s2mps14_reg_is_enabled,
	.enable			= s2mps14_reg_enable,
	.disable		= s2mps14_reg_disable,
	.get_voltage_sel	= s2mps14_get_voltage_sel,
	.set_voltage_sel	= s2mps14_set_voltage_sel,
	.set_voltage_time_sel	= s2mps14_set_voltage_time_sel,
};

static struct regulator_ops s2mps14_others_ops = {
	.is_enabled		= s2mps14_reg_is_enabled,
	.enable			= s2mps14_reg_enable,
	.disable		= s2mps14_reg_disable,
};

#define regulator_desc_ldo(num)		{	\
	.name		= "LDO"#num,		\
	.id		= S2MPS14_LDO##num,	\
	.ops		= &s2mps14_ldo_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
}
#define regulator_desc_buck(num)	{	\
	.name		= "BUCK"#num,		\
	.id		= S2MPS14_BUCK##num,	\
	.ops		= &s2mps14_buck_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
}

static struct regulator_desc regulators[] = {
	regulator_desc_ldo(1),
	regulator_desc_ldo(2),
	regulator_desc_ldo(3),
	regulator_desc_ldo(4),
	regulator_desc_ldo(5),
	regulator_desc_ldo(6),
	regulator_desc_ldo(7),
	regulator_desc_ldo(8),
	regulator_desc_ldo(9),
	regulator_desc_ldo(10),
	regulator_desc_ldo(11),
	regulator_desc_ldo(12),
	regulator_desc_ldo(13),
	regulator_desc_ldo(14),
	regulator_desc_ldo(15),
	regulator_desc_ldo(16),
	regulator_desc_ldo(17),
	regulator_desc_ldo(18),
	regulator_desc_ldo(19),
	regulator_desc_ldo(20),
	regulator_desc_ldo(21),
	regulator_desc_ldo(22),
	regulator_desc_ldo(23),
	regulator_desc_ldo(24),
	regulator_desc_ldo(25),
	regulator_desc_buck(1),
	regulator_desc_buck(2),
	regulator_desc_buck(3),
	regulator_desc_buck(4),
	regulator_desc_buck(5),
	{
		.name	= "EN32KHz AP",
		.id	= S2MPS14_AP_EN32KHZ,
		.ops	= &s2mps14_others_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
	}, {
		.name	= "EN32KHz CP",
		.id	= S2MPS14_CP_EN32KHZ,
		.ops	= &s2mps14_others_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
	}, {
		.name	= "EN32KHz BT",
		.id	= S2MPS14_BT_EN32KHZ,
		.ops	= &s2mps14_others_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
	},
};

static __devinit int s2mps14_pmic_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_pmic_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct regulator_dev **rdev;
	struct s2mps14_info *s2mps14;
	int i, ret, size;
	unsigned int ramp_reg = 0;

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mps14 = devm_kzalloc(&pdev->dev, sizeof(struct s2mps14_info),
				GFP_KERNEL);
	if (!s2mps14)
		return -ENOMEM;

	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	s2mps14->rdev = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!s2mps14->rdev)
		return -ENOMEM;

	ret = sec_reg_read(iodev, S2MPS14_REG_ID, &iodev->rev_num);
	if (ret < 0)
		return ret;

	rdev = s2mps14->rdev;
	s2mps14->dev = &pdev->dev;
	s2mps14->iodev = iodev;
	s2mps14->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, s2mps14);

	s2mps14->ramp_delay12345 = pdata->buck12345_ramp_delay;
	s2mps14->opmode_data = pdata->opmode_data;


	ramp_reg |= get_ramp_delay(s2mps14->ramp_delay12345);

	sec_reg_update(s2mps14->iodev, S2MPS14_REG_ETC_OTP, ramp_reg, 0x03);

	for (i = 0; i < pdata->num_regulators; i++) {
		const struct s2mps14_voltage_desc *desc;
		int id = pdata->regulators[i].id;

		desc = reg_voltage_map[id];
		if (desc)
			regulators[id].n_voltages =
				(desc->max - desc->min) / desc->step + 1;

		rdev[i] = regulator_register(&regulators[id], s2mps14->dev,
				pdata->regulators[i].initdata, s2mps14, NULL);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(s2mps14->dev, "regulator init failed for %d\n",
					id);
			rdev[i] = NULL;
			goto err;
		}
	}

	return 0;
err:
	for (i = 0; i < s2mps14->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	return ret;
}

static int __devexit s2mps14_pmic_remove(struct platform_device *pdev)
{
	struct s2mps14_info *s2mps14 = platform_get_drvdata(pdev);
	struct regulator_dev **rdev = s2mps14->rdev;
	int i;

	for (i = 0; i < s2mps14->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	return 0;
}

static const struct platform_device_id s2mps14_pmic_id[] = {
	{ "s2mps14-pmic", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mps14_pmic_id);

static struct platform_driver s2mps14_pmic_driver = {
	.driver = {
		.name = "s2mps14-pmic",
		.owner = THIS_MODULE,
	},
	.probe = s2mps14_pmic_probe,
	.remove = __devexit_p(s2mps14_pmic_remove),
	.id_table = s2mps14_pmic_id,
};

static int __init s2mps14_pmic_init(void)
{
	return platform_driver_register(&s2mps14_pmic_driver);
}
subsys_initcall(s2mps14_pmic_init);

static void __exit s2mps14_pmic_exit(void)
{
	platform_driver_unregister(&s2mps14_pmic_driver);
}
module_exit(s2mps14_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_AUTHOR("Dongsu Ha <dsfine.ha@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPS14 Regulator Driver");
MODULE_LICENSE("GPL");
