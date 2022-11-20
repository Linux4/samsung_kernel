/*
 * s2mpu01a.c
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
#include <linux/mfd/samsung/s2mpu01a.h>

#define DEBUG_SLEEP_WITHOUT_CP

struct s2mpu01a_info {
	struct device *dev;
	struct sec_pmic_dev *iodev;
	int num_regulators;
	struct regulator_dev **rdev;
	struct sec_opmode_data *opmode_data;

	int ramp_delay2;
	int ramp_delay34;
	int ramp_delay5;
	int ramp_delay16;
	int ramp_delay7;
	int ramp_delay8;

	bool buck1_ramp;
	bool buck2_ramp;
	bool buck3_ramp;
	bool buck4_ramp;
};

struct s2mpu01a_voltage_desc {
	int max;
	int min;
	int step;
};

static const struct s2mpu01a_voltage_desc buck_voltage_val1 = {
	.max = 1400000,
	.min =  600000,
	.step =   6250,
};

static const struct s2mpu01a_voltage_desc buck_voltage_val2 = {
	.max = 1500000,
	.min =  800000,
	.step =   6250,
};

static const struct s2mpu01a_voltage_desc buck_voltage_val3 = {
	.max = 2300000,
	.min =  1500000,
	.step =   12500,
};

static const struct s2mpu01a_voltage_desc buck_voltage_val4 = {
	.max = 1500000,
	.min =  800000,
	.step =   12500,
};

static const struct s2mpu01a_voltage_desc ldo_voltage_val1 = {
	.max = 3950000,
	.min =  800000,
	.step =  50000,
};

static const struct s2mpu01a_voltage_desc ldo_voltage_val2 = {
	.max = 2375000,
	.min =  800000,
	.step =  25000,
};

static const struct s2mpu01a_voltage_desc *reg_voltage_map[] = {
	[S2MPU01A_LDO1] = &ldo_voltage_val2,
	[S2MPU01A_LDO2] = &ldo_voltage_val2,
	[S2MPU01A_LDO3] = &ldo_voltage_val1,
	[S2MPU01A_LDO4] = &ldo_voltage_val1,
	[S2MPU01A_LDO5] = &ldo_voltage_val1,
	[S2MPU01A_LDO6] = &ldo_voltage_val2,
	[S2MPU01A_LDO7] = &ldo_voltage_val2,
	[S2MPU01A_LDO8] = &ldo_voltage_val1,
	[S2MPU01A_LDO9] = &ldo_voltage_val1,
	[S2MPU01A_LDO23] = &ldo_voltage_val1,
	[S2MPU01A_LDO24] = &ldo_voltage_val1,
	[S2MPU01A_LDO25] = &ldo_voltage_val1,
	[S2MPU01A_LDO26] = &ldo_voltage_val1,
	[S2MPU01A_LDO27] = &ldo_voltage_val1,
	[S2MPU01A_LDO28] = &ldo_voltage_val1,
	[S2MPU01A_LDO29] = &ldo_voltage_val1,
	[S2MPU01A_LDO30] = &ldo_voltage_val1,
	[S2MPU01A_LDO31] = &ldo_voltage_val1,
	[S2MPU01A_LDO32] = &ldo_voltage_val1,
	[S2MPU01A_LDO33] = &ldo_voltage_val1,
	[S2MPU01A_LDO34] = &ldo_voltage_val1,
	[S2MPU01A_LDO35] = &ldo_voltage_val1,
	[S2MPU01A_LDO36] = &ldo_voltage_val2,
	[S2MPU01A_LDO37] = &ldo_voltage_val1,
	[S2MPU01A_LDO38] = &ldo_voltage_val1,
	[S2MPU01A_LDO39] = &ldo_voltage_val1,
	[S2MPU01A_LDO40] = &ldo_voltage_val1,
	[S2MPU01A_LDO41] = &ldo_voltage_val1,
	[S2MPU01A_LDO42] = &ldo_voltage_val1,
	[S2MPU01A_BUCK1] = &buck_voltage_val1,
	[S2MPU01A_BUCK2] = &buck_voltage_val1,
	[S2MPU01A_BUCK3] = &buck_voltage_val1,
	[S2MPU01A_BUCK4] = &buck_voltage_val1,
	[S2MPU01A_BUCK5] = &buck_voltage_val2,
	[S2MPU01A_BUCK6] = &buck_voltage_val2,
	[S2MPU01A_BUCK7] = &buck_voltage_val3,
	[S2MPU01A_BUCK8] = &buck_voltage_val4,
};

static int s2mpu01a_list_voltage(struct regulator_dev *rdev,
				unsigned int selector)
{
	const struct s2mpu01a_voltage_desc *desc;
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

unsigned int s2mpu01a_opmode_reg[][3] = {
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
};

static int s2mpu01a_get_register(struct regulator_dev *rdev,
	unsigned int *reg, int *pmic_en)
{
	int reg_id = rdev_get_id(rdev);
	unsigned int mode;
	struct s2mpu01a_info *s2mpu01a = rdev_get_drvdata(rdev);

	switch (reg_id) {
	case S2MPU01A_LDO1 ... S2MPU01A_LDO42:
		*reg = S2MPU01A_REG_L1CTRL + (reg_id - S2MPU01A_LDO1);
		break;
	case S2MPU01A_BUCK1 ... S2MPU01A_BUCK8:
		*reg = S2MPU01A_REG_B1CTRL1 + (reg_id - S2MPU01A_BUCK1) * 2;
		break;
	case S2MPU01A_AP_EN32KHZ ... S2MPU01A_CP_EN32KHZ:
		*reg = S2MPU01A_REG_RTC_CTRL;
		*pmic_en = 0x01 << (reg_id - S2MPU01A_AP_EN32KHZ);
		return 0;
	default:
		return -EINVAL;
	}

	mode = s2mpu01a->opmode_data[reg_id].mode;
	*pmic_en = s2mpu01a_opmode_reg[reg_id][mode] << S2MPU01A_PMIC_EN_SHIFT;

	return 0;
}

static int s2mpu01a_reg_is_enabled(struct regulator_dev *rdev)
{
	struct s2mpu01a_info *s2mpu01a = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg, val;
	int ret, mask = 0xc0, pmic_en;

	ret = s2mpu01a_get_register(rdev, &reg, &pmic_en);
	if (ret == -EINVAL)
		return 1;
	else if (ret)
		return ret;

	ret = sec_reg_read(s2mpu01a->iodev, reg, &val);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPU01A_LDO1 ... S2MPU01A_BUCK8:
		mask = 0xc0;
		break;
	case S2MPU01A_AP_EN32KHZ:
		mask = 0x01;
		break;
	case S2MPU01A_CP_EN32KHZ:
		mask = 0x02;
		break;
	default:
		return -EINVAL;
	}

	return (val & mask) == pmic_en;
}

static int s2mpu01a_reg_enable(struct regulator_dev *rdev)
{
	struct s2mpu01a_info *s2mpu01a = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	int ret, mask, pmic_en;

	ret = s2mpu01a_get_register(rdev, &reg, &pmic_en);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPU01A_LDO1 ... S2MPU01A_BUCK8:
		mask = 0xc0;
		break;
	case S2MPU01A_AP_EN32KHZ:
		mask = 0x01;
		break;
	case S2MPU01A_CP_EN32KHZ:
		mask = 0x02;
		break;
	default:
		return -EINVAL;
	}

	return sec_reg_update(s2mpu01a->iodev, reg, pmic_en, mask);
}

static int s2mpu01a_reg_disable(struct regulator_dev *rdev)
{
	struct s2mpu01a_info *s2mpu01a = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	int ret, mask, pmic_en;

	ret = s2mpu01a_get_register(rdev, &reg, &pmic_en);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPU01A_LDO1 ... S2MPU01A_BUCK8:
		mask = 0xc0;
		break;
	case S2MPU01A_AP_EN32KHZ:
		mask = 0x01;
		break;
	case S2MPU01A_CP_EN32KHZ:
		mask = 0x02;
		break;
	default:
		return -EINVAL;
	}

	return sec_reg_update(s2mpu01a->iodev, reg, ~mask, mask);
}

static int s2mpu01a_get_voltage_register(struct regulator_dev *rdev,
		unsigned int *_reg)
{
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;

	switch (reg_id) {
	case S2MPU01A_LDO1 ... S2MPU01A_LDO42:
		reg = S2MPU01A_REG_L1CTRL + (reg_id - S2MPU01A_LDO1);
		break;
	case S2MPU01A_BUCK1 ... S2MPU01A_BUCK4:
		reg = S2MPU01A_REG_DVS_DATA;
		break;
	case S2MPU01A_BUCK5 ... S2MPU01A_BUCK8:
		reg = S2MPU01A_REG_B5CTRL2 + (reg_id - S2MPU01A_BUCK5) * 2;
		break;
	default:
		return -EINVAL;
	}

	*_reg = reg;

	return 0;
}

static int s2mpu01a_get_voltage_sel(struct regulator_dev *rdev)
{
	struct s2mpu01a_info *s2mpu01a = rdev_get_drvdata(rdev);
	int mask, ret;
	int reg_id = rdev_get_id(rdev);
	unsigned int reg, val;

	ret = s2mpu01a_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPU01A_BUCK1 ... S2MPU01A_BUCK8:
		mask = 0xff;
		break;
	case S2MPU01A_LDO1 ... S2MPU01A_LDO42:
		mask = 0x3f;
		break;
	default:
		return -EINVAL;
	}

	ret = sec_reg_read(s2mpu01a->iodev, reg, &val);
	if (ret)
		return ret;

	val &= mask;

	return val;
}

static inline int s2mpu01a_convert_voltage_to_sel(
		const struct s2mpu01a_voltage_desc *desc,
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

static int s2mpu01a_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned *selector)
{
	struct s2mpu01a_info *s2mpu01a = rdev_get_drvdata(rdev);
	int min_vol = min_uV, max_vol = max_uV;
	const struct s2mpu01a_voltage_desc *desc;
	int ret, reg_id = rdev_get_id(rdev);
	unsigned int reg, mask;
	int sel;

	mask = (reg_id < S2MPU01A_BUCK1) ? 0x3f : 0xff;

	desc = reg_voltage_map[reg_id];

	sel = s2mpu01a_convert_voltage_to_sel(desc, min_vol, max_vol);
	if (sel < 0)
		return sel;

	ret = s2mpu01a_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	ret = sec_reg_update(s2mpu01a->iodev, reg, sel, mask);
	*selector = sel;

	return ret;
}

static int s2mpu01a_set_voltage_time_sel(struct regulator_dev *rdev,
					     unsigned int old_sel,
					     unsigned int new_sel)
{
	struct s2mpu01a_info *s2mpu01a = rdev_get_drvdata(rdev);
	const struct s2mpu01a_voltage_desc *desc;
	int reg_id = rdev_get_id(rdev);
	int ramp_delay = 0;

	switch (reg_id) {
	case S2MPU01A_BUCK1:
	case S2MPU01A_BUCK6:
		ramp_delay = s2mpu01a->ramp_delay16;
		break;
	case S2MPU01A_BUCK2:
		ramp_delay = s2mpu01a->ramp_delay2;
		break;
	case S2MPU01A_BUCK3 ... S2MPU01A_BUCK4:
		ramp_delay = s2mpu01a->ramp_delay34;
		break;
	case S2MPU01A_BUCK5:
		ramp_delay = s2mpu01a->ramp_delay5;
		break;
	case S2MPU01A_BUCK7:
		ramp_delay = s2mpu01a->ramp_delay7;
		break;
	case S2MPU01A_BUCK8:
		ramp_delay = s2mpu01a->ramp_delay8;
		break;
	default:
		return -EINVAL;
	}

	desc = reg_voltage_map[reg_id];

	if (((old_sel < new_sel) && (reg_id >= S2MPU01A_BUCK1)) && ramp_delay) {
		return DIV_ROUND_UP(desc->step * (new_sel - old_sel),
			ramp_delay * 1000);
	}

	return 0;
}

static int s2mpu01a_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	struct s2mpu01a_info *s2mpu01a = rdev_get_drvdata(rdev);
	int mask, ret;
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	u8 data[2];

	ret = s2mpu01a_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPU01A_BUCK1 ... S2MPU01A_BUCK8:
		mask = 0xff;
		break;
	case S2MPU01A_LDO1 ... S2MPU01A_LDO42:
		mask = 0x3f;
		break;
	default:
		return -EINVAL;
	}

	if(reg_id >= S2MPU01A_BUCK1 && reg_id <= S2MPU01A_BUCK4) {
		data[0] = 0x01 | ((reg_id - S2MPU01A_BUCK1) << 3);
		data[1] = selector;
		return sec_bulk_write(s2mpu01a->iodev, S2MPU01A_REG_DVS_PTR, 2, data);
	}

	return sec_reg_update(s2mpu01a->iodev, reg, selector, mask);
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

static struct regulator_ops s2mpu01a_ldo_ops = {
	.list_voltage		= s2mpu01a_list_voltage,
	.is_enabled		= s2mpu01a_reg_is_enabled,
	.enable			= s2mpu01a_reg_enable,
	.disable		= s2mpu01a_reg_disable,
	.get_voltage_sel	= s2mpu01a_get_voltage_sel,
	.set_voltage		= s2mpu01a_set_voltage,
	.set_voltage_time_sel	= s2mpu01a_set_voltage_time_sel,
};

static struct regulator_ops s2mpu01a_buck_ops = {
	.list_voltage		= s2mpu01a_list_voltage,
	.is_enabled		= s2mpu01a_reg_is_enabled,
	.enable			= s2mpu01a_reg_enable,
	.disable		= s2mpu01a_reg_disable,
	.get_voltage_sel	= s2mpu01a_get_voltage_sel,
	.set_voltage_sel	= s2mpu01a_set_voltage_sel,
	.set_voltage_time_sel	= s2mpu01a_set_voltage_time_sel,
};

static struct regulator_ops s2mpu01a_others_ops = {
	.is_enabled		= s2mpu01a_reg_is_enabled,
	.enable			= s2mpu01a_reg_enable,
	.disable		= s2mpu01a_reg_disable,
};

#define regulator_desc_ldo(num)		{	\
	.name		= "LDO"#num,		\
	.id		= S2MPU01A_LDO##num,	\
	.ops		= &s2mpu01a_ldo_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
}
#define regulator_desc_buck(num)	{	\
	.name		= "BUCK"#num,		\
	.id		= S2MPU01A_BUCK##num,	\
	.ops		= &s2mpu01a_buck_ops,	\
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
	regulator_desc_ldo(23),
	regulator_desc_ldo(24),
	regulator_desc_ldo(25),
	regulator_desc_ldo(26),
	regulator_desc_ldo(27),
	regulator_desc_ldo(28),
	regulator_desc_ldo(29),
	regulator_desc_ldo(30),
	regulator_desc_ldo(31),
	regulator_desc_ldo(32),
	regulator_desc_ldo(33),
	regulator_desc_ldo(34),
	regulator_desc_ldo(35),
	regulator_desc_ldo(36),
	regulator_desc_ldo(37),
	regulator_desc_ldo(38),
	regulator_desc_ldo(39),
	regulator_desc_ldo(40),
	regulator_desc_ldo(41),
	regulator_desc_ldo(42),
	regulator_desc_buck(1),
	regulator_desc_buck(2),
	regulator_desc_buck(3),
	regulator_desc_buck(4),
	regulator_desc_buck(5),
	regulator_desc_buck(6),
	regulator_desc_buck(7),
	regulator_desc_buck(8),
	{
		.name	= "EN32KHz AP",
		.id	= S2MPU01A_AP_EN32KHZ,
		.ops	= &s2mpu01a_others_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
	}, {
		.name	= "EN32KHz CP",
		.id	= S2MPU01A_CP_EN32KHZ,
		.ops	= &s2mpu01a_others_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
	},
};

static __devinit int s2mpu01a_pmic_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_pmic_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct regulator_dev **rdev;
	struct s2mpu01a_info *s2mpu01a;
	int i, ret, size;
	unsigned int ramp_enable, ramp_reg = 0;
#ifdef DEBUG_SLEEP_WITHOUT_CP
	unsigned int val, buck_init;
#endif
	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mpu01a = devm_kzalloc(&pdev->dev, sizeof(struct s2mpu01a_info),
				GFP_KERNEL);
	if (!s2mpu01a)
		return -ENOMEM;

	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	s2mpu01a->rdev = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!s2mpu01a->rdev)
		return -ENOMEM;

	ret = sec_reg_read(iodev, S2MPU01A_REG_ID, &iodev->rev_num);
	if (ret < 0)
		return ret;

	rdev = s2mpu01a->rdev;
	s2mpu01a->dev = &pdev->dev;
	s2mpu01a->iodev = iodev;
	s2mpu01a->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, s2mpu01a);

	s2mpu01a->ramp_delay2 = pdata->buck2_ramp_delay;
	s2mpu01a->ramp_delay34 = pdata->buck34_ramp_delay;
	s2mpu01a->ramp_delay5 = pdata->buck5_ramp_delay;
	s2mpu01a->ramp_delay16 = pdata->buck16_ramp_delay;
	s2mpu01a->ramp_delay7 = pdata->buck7_ramp_delay;
	s2mpu01a->ramp_delay8 = pdata->buck8_ramp_delay;

	s2mpu01a->buck1_ramp = pdata->buck1_ramp_enable;
	s2mpu01a->buck2_ramp = pdata->buck2_ramp_enable;
	s2mpu01a->buck3_ramp = pdata->buck3_ramp_enable;
	s2mpu01a->buck4_ramp = pdata->buck4_ramp_enable;
	s2mpu01a->opmode_data = pdata->opmode_data;

	sec_reg_update(s2mpu01a->iodev, S2MPU01A_REG_LEE_NO, 0x20, 0x20);

	if (pdata->buck1_init) {
		buck_init = s2mpu01a_convert_voltage_to_sel(&buck_voltage_val1,
						pdata->buck1_init,
						pdata->buck1_init +
						buck_voltage_val1.step);

		sec_reg_write(s2mpu01a->iodev, S2MPU01A_REG_DVS_PTR, 0x01);
		sec_reg_write(s2mpu01a->iodev, S2MPU01A_REG_DVS_DATA, buck_init);
	}

	if (pdata->buck2_init) {
		buck_init = s2mpu01a_convert_voltage_to_sel(&buck_voltage_val2,
						pdata->buck2_init,
						pdata->buck2_init +
						buck_voltage_val2.step);

		sec_reg_write(s2mpu01a->iodev, S2MPU01A_REG_DVS_PTR, 0x09);
		sec_reg_write(s2mpu01a->iodev, S2MPU01A_REG_DVS_DATA, buck_init);
	}

	if (pdata->buck3_init) {
		buck_init = s2mpu01a_convert_voltage_to_sel(&buck_voltage_val2,
						pdata->buck3_init,
						pdata->buck3_init +
						buck_voltage_val2.step);

		sec_reg_write(s2mpu01a->iodev, S2MPU01A_REG_DVS_PTR, 0x11);
		sec_reg_write(s2mpu01a->iodev, S2MPU01A_REG_DVS_DATA, buck_init);
	}

	if (pdata->buck4_init) {
		buck_init = s2mpu01a_convert_voltage_to_sel(&buck_voltage_val2,
						pdata->buck4_init,
						pdata->buck4_init +
						buck_voltage_val2.step);

		sec_reg_write(s2mpu01a->iodev, S2MPU01A_REG_DVS_PTR, 0x19);
		sec_reg_write(s2mpu01a->iodev, S2MPU01A_REG_DVS_DATA, buck_init);
	}

	sec_reg_write(s2mpu01a->iodev, S2MPU01A_REG_DVS_SEL, 0x01);

	ramp_enable = (s2mpu01a->buck1_ramp << 3) | (s2mpu01a->buck2_ramp << 2) |
		(s2mpu01a->buck3_ramp << 1) | s2mpu01a->buck4_ramp ;

	if (ramp_enable) {
		if (s2mpu01a->buck2_ramp)
			ramp_reg |= get_ramp_delay(s2mpu01a->ramp_delay2) << 6;
		if (s2mpu01a->buck3_ramp || s2mpu01a->buck4_ramp)
			ramp_reg |= get_ramp_delay(s2mpu01a->ramp_delay34) << 4;
		sec_reg_update(s2mpu01a->iodev, S2MPU01A_REG_RAMP,
			ramp_reg | ramp_enable, 0xff);
	}

	ramp_reg &= 0x00;
	ramp_reg |= get_ramp_delay(s2mpu01a->ramp_delay5) << 6;
	ramp_reg |= get_ramp_delay(s2mpu01a->ramp_delay16) << 4;
	ramp_reg |= get_ramp_delay(s2mpu01a->ramp_delay7) << 2;
	ramp_reg |= get_ramp_delay(s2mpu01a->ramp_delay8);
	sec_reg_update(s2mpu01a->iodev, S2MPU01A_REG_RAMP_BUCK, ramp_reg, 0xff);

#ifdef DEBUG_SLEEP_WITHOUT_CP
	/* temporarily code, will be impremented bootloader */
	sec_reg_read(s2mpu01a->iodev, 0x59, &val);
	printk(KERN_ERR "%s--pmu_mif[0x%02x]\n", __func__, val);
	sec_reg_write(s2mpu01a->iodev, 0x59, 0xff);
#endif

	for (i = 0; i < pdata->num_regulators; i++) {
		const struct s2mpu01a_voltage_desc *desc;
		int id = pdata->regulators[i].id;

		desc = reg_voltage_map[id];
		if (desc)
			regulators[id].n_voltages =
				(desc->max - desc->min) / desc->step + 1;

		rdev[i] = regulator_register(&regulators[id], s2mpu01a->dev,
				pdata->regulators[i].initdata, s2mpu01a, NULL);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(s2mpu01a->dev, "regulator init failed for %d\n",
					id);
			rdev[i] = NULL;
			goto err;
		}
	}

	return 0;
err:
	for (i = 0; i < s2mpu01a->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	return ret;
}

static int __devexit s2mpu01a_pmic_remove(struct platform_device *pdev)
{
	struct s2mpu01a_info *s2mpu01a = platform_get_drvdata(pdev);
	struct regulator_dev **rdev = s2mpu01a->rdev;
	int i;

	for (i = 0; i < s2mpu01a->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	return 0;
}

static const struct platform_device_id s2mpu01a_pmic_id[] = {
	{ "s2mpu01a-pmic", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpu01a_pmic_id);

static struct platform_driver s2mpu01a_pmic_driver = {
	.driver = {
		.name = "s2mpu01a-pmic",
		.owner = THIS_MODULE,
	},
	.probe = s2mpu01a_pmic_probe,
	.remove = __devexit_p(s2mpu01a_pmic_remove),
	.id_table = s2mpu01a_pmic_id,
};

static int __init s2mpu01a_pmic_init(void)
{
	return platform_driver_register(&s2mpu01a_pmic_driver);
}
subsys_initcall(s2mpu01a_pmic_init);

static void __exit s2mpu01a_pmic_exit(void)
{
	platform_driver_unregister(&s2mpu01a_pmic_driver);
}
module_exit(s2mpu01a_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_AUTHOR("Dongsu Ha <dsfine.ha@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPU01A Regulator Driver");
MODULE_LICENSE("GPL");
