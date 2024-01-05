/*
 * s2mpu02x.c
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
#include <linux/mfd/samsung/s2mpu02x.h>

struct s2mpu02x_info {
	struct device *dev;
	struct sec_pmic_dev *iodev;
	int num_regulators;
	struct regulator_dev **rdev;
	struct sec_opmode_data *opmode_data;
	struct mutex lock;

	int ramp_delay1;
	int ramp_delay2;
	int ramp_delay3;
	int ramp_delay4;

	bool buck1_ramp;
	bool buck2_ramp;
	bool buck3_ramp;
	bool buck4_ramp;
};

struct s2mpu02x_voltage_desc {
	int max;
	int min;
	int step;
};

static const struct s2mpu02x_voltage_desc buck_voltage_val1 = {
	.max = 1981250,
	.min =  600000,
	.step =   6250,
};

static const struct s2mpu02x_voltage_desc buck_voltage_val2 = {
	.max = 2200000,
	.min = 1200000,
	.step =  12500,
};

static const struct s2mpu02x_voltage_desc ldo_voltage_val1 = {
	.max = 1587500,
	.min =  800000,
	.step =  12500,
};

static const struct s2mpu02x_voltage_desc ldo_voltage_val2 = {
	.max = 2375000,
	.min =  800000,
	.step =  25000,
};

static const struct s2mpu02x_voltage_desc ldo_voltage_val3 = {
	.max = 3950000,
	.min =  800000,
	.step =  50000,
};


static const struct s2mpu02x_voltage_desc *reg_voltage_map[] = {
	[S2MPU02X_LDO1] = &ldo_voltage_val1,
	[S2MPU02X_LDO2] = &ldo_voltage_val2,
	[S2MPU02X_LDO3] = &ldo_voltage_val2,
	[S2MPU02X_LDO4] = &ldo_voltage_val3,
	[S2MPU02X_LDO5] = &ldo_voltage_val2,
	[S2MPU02X_LDO6] = &ldo_voltage_val1,
	[S2MPU02X_LDO7] = &ldo_voltage_val1,
	[S2MPU02X_LDO8] = &ldo_voltage_val2,
	[S2MPU02X_LDO9] = &ldo_voltage_val3,
	[S2MPU02X_LDO10] = &ldo_voltage_val1,
	[S2MPU02X_LDO11] = &ldo_voltage_val2,
	[S2MPU02X_LDO12] = &ldo_voltage_val3,
	[S2MPU02X_LDO13] = &ldo_voltage_val3,
	[S2MPU02X_LDO14] = &ldo_voltage_val3,
	[S2MPU02X_LDO15] = &ldo_voltage_val3,
	[S2MPU02X_LDO16] = &ldo_voltage_val3,
	[S2MPU02X_LDO17] = &ldo_voltage_val2,
	[S2MPU02X_LDO18] = &ldo_voltage_val3,
	[S2MPU02X_LDO19] = &ldo_voltage_val1,
	[S2MPU02X_LDO20] = &ldo_voltage_val2,
	[S2MPU02X_LDO21] = &ldo_voltage_val3,
	[S2MPU02X_LDO22] = &ldo_voltage_val3,
	[S2MPU02X_LDO23] = &ldo_voltage_val3,
	[S2MPU02X_LDO24] = &ldo_voltage_val2,
	[S2MPU02X_LDO25] = &ldo_voltage_val3,
	[S2MPU02X_LDO26] = &ldo_voltage_val2,
	[S2MPU02X_LDO27] = &ldo_voltage_val3,
	[S2MPU02X_LDO28] = &ldo_voltage_val3,
	[S2MPU02X_BUCK1] = &buck_voltage_val1,
	[S2MPU02X_BUCK2] = &buck_voltage_val1,
	[S2MPU02X_BUCK3] = &buck_voltage_val1,
	[S2MPU02X_BUCK4] = &buck_voltage_val1,
	[S2MPU02X_BUCK5] = &buck_voltage_val1,
	[S2MPU02X_BUCK6] = &buck_voltage_val2,
	[S2MPU02X_BUCK7] = &buck_voltage_val1,
};

static int s2mpu02x_list_voltage(struct regulator_dev *rdev,
				unsigned int selector)
{
	const struct s2mpu02x_voltage_desc *desc;
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

unsigned int s2mpu02x_opmode_reg[][3] = {
	{0x3, 0x2, 0x1}, /* LDO1 */
	{0x3, 0x2, 0x1}, /* LDO2 */
	{0x3, 0x2, 0x1}, /* LDO3 */
	{0x3, 0x2, 0x1}, /* LDO4 */
	{0x3, 0x2, 0x1}, /* LDO5 */
	{0x3, 0x2, 0x1}, /* LDO6 */
	{0x3, 0x2, 0x1}, /* LDO7 */
	{0x3, 0x2, 0x1}, /* LDO8 */
	{0x3, 0x2, 0x1}, /* LDO9 */
	{0x3, 0x2, 0x1}, /* LDO10 */
	{0x3, 0x2, 0x1}, /* LDO11 */
	{0x3, 0x2, 0x1}, /* LDO12 */
	{0x3, 0x3, 0x3}, /* LDO13 */
	{0x3, 0x3, 0x3}, /* LDO14 */
	{0x3, 0x3, 0x3}, /* LDO15 */
	{0x3, 0x3, 0x3}, /* LDO16 */
	{0x3, 0x3, 0x3}, /* LDO17 */
	{0x3, 0x2, 0x1}, /* LDO18 */
	{0x3, 0x2, 0x1}, /* LDO19 */
	{0x3, 0x2, 0x1}, /* LDO20 */
	{0x3, 0x2, 0x1}, /* LDO21 */
	{0x3, 0x2, 0x1}, /* LDO22 */
	{0x3, 0x2, 0x1}, /* LDO23 */
	{0x3, 0x2, 0x1}, /* LDO24 */
	{0x3, 0x2, 0x1}, /* LDO25 */
	{0x3, 0x2, 0x1}, /* LDO26 */
	{0x3, 0x2, 0x1}, /* LDO27 */
	{0x3, 0x2, 0x1}, /* LDO28 */
	{0x3, 0x2, 0x1}, /* BUCK1 */
	{0x3, 0x2, 0x1}, /* BUCK2 */
	{0x3, 0x2, 0x1}, /* BUCK3 */
	{0x3, 0x2, 0x1}, /* BUCK4 */
	{0x3, 0x2, 0x1}, /* BUCK5 */
	{0x3, 0x2, 0x1}, /* BUCK6 */
	{0x3, 0x3, 0x3}, /* BUCK7 */
};

static int s2mpu02x_get_register(struct regulator_dev *rdev,
	unsigned int *reg, int *pmic_en)
{
	int reg_id = rdev_get_id(rdev);
	unsigned int mode;
	struct s2mpu02x_info *s2mpu02x = rdev_get_drvdata(rdev);

	switch (reg_id) {
	case S2MPU02X_LDO1 ... S2MPU02X_LDO2:
		*reg = S2MPU02X_REG_L1CTRL + (reg_id - S2MPU02X_LDO1);
		break;
	case S2MPU02X_LDO3 ... S2MPU02X_LDO28:
		*reg = S2MPU02X_REG_L3CTRL + (reg_id - S2MPU02X_LDO3);
		break;
	case S2MPU02X_BUCK1 ... S2MPU02X_BUCK5:
		*reg = S2MPU02X_REG_B1CTRL1 + (reg_id - S2MPU02X_BUCK1) * 2;
		break;
	case S2MPU02X_BUCK6 ... S2MPU02X_BUCK7:
		*reg = S2MPU02X_REG_B6CTRL1 + (reg_id - S2MPU02X_BUCK6) * 2;
		break;
	case S2MPU02X_AP_EN32KHZ:
		*reg = S2MPU02X_REG_RTC_CTRL;
		*pmic_en = 0x01 << (reg_id - S2MPU02X_AP_EN32KHZ);
		return 0;
	default:
		return -EINVAL;
	}

	mode = s2mpu02x->opmode_data[reg_id].mode;
	*pmic_en = s2mpu02x_opmode_reg[reg_id][mode] << S2MPU02X_PMIC_EN_SHIFT;

	return 0;
}

static int s2mpu02x_reg_is_enabled(struct regulator_dev *rdev)
{
	struct s2mpu02x_info *s2mpu02x = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg, val;
	int ret, mask = 0xc0, pmic_en;

	ret = s2mpu02x_get_register(rdev, &reg, &pmic_en);
	if (ret == -EINVAL)
		return 1;
	else if (ret)
		return ret;

	ret = sec_reg_read(s2mpu02x->iodev, reg, &val);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPU02X_LDO1 ... S2MPU02X_BUCK7:
		mask = 0xc0;
		break;
	case S2MPU02X_AP_EN32KHZ:
		mask = 0x01;
		break;
	default:
		return -EINVAL;
	}

	return (val & mask) == pmic_en;
}

static int s2mpu02x_reg_enable(struct regulator_dev *rdev)
{
	struct s2mpu02x_info *s2mpu02x = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	int ret, mask, pmic_en;

	ret = s2mpu02x_get_register(rdev, &reg, &pmic_en);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPU02X_LDO1 ... S2MPU02X_BUCK7:
		mask = 0xc0;
		break;
	case S2MPU02X_AP_EN32KHZ:
		mask = 0x01;
		break;
	default:
		return -EINVAL;
	}

	return sec_reg_update(s2mpu02x->iodev, reg, pmic_en, mask);
}

static int s2mpu02x_reg_disable(struct regulator_dev *rdev)
{
	struct s2mpu02x_info *s2mpu02x = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	int ret, mask, pmic_en;

	ret = s2mpu02x_get_register(rdev, &reg, &pmic_en);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPU02X_LDO1 ... S2MPU02X_BUCK7:
		mask = 0xc0;
		break;
	case S2MPU02X_AP_EN32KHZ:
		mask = 0x01;
		break;
	default:
		return -EINVAL;
	}

	return sec_reg_update(s2mpu02x->iodev, reg, ~mask, mask);
}

static int s2mpu02x_get_voltage_register(struct regulator_dev *rdev,
		unsigned int *_reg)
{
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;

	switch (reg_id) {
	case S2MPU02X_LDO1 ... S2MPU02X_LDO2:
		reg = S2MPU02X_REG_L1CTRL + (reg_id - S2MPU02X_LDO1);
		break;
	case S2MPU02X_LDO3 ... S2MPU02X_LDO28:
		reg = S2MPU02X_REG_L3CTRL + (reg_id - S2MPU02X_LDO3);
		break;
	case S2MPU02X_BUCK1 ... S2MPU02X_BUCK4:
		reg = S2MPU02X_REG_DVS_DATA;
		break;
	case S2MPU02X_BUCK5:
		reg = S2MPU02X_REG_B5CTRL2;
		break;
	case S2MPU02X_BUCK6 ... S2MPU02X_BUCK7:
		reg = S2MPU02X_REG_B6CTRL2 + (reg_id - S2MPU02X_BUCK6) * 2;
		break;
	default:
		return -EINVAL;
	}

	*_reg = reg;

	return 0;
}

static int s2mpu02x_get_voltage_sel(struct regulator_dev *rdev)
{
	struct s2mpu02x_info *s2mpu02x = rdev_get_drvdata(rdev);
	int mask, ret;
	int reg_id = rdev_get_id(rdev);
	unsigned int reg, val, dvs_ptr;

	ret = s2mpu02x_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPU02X_BUCK1 ... S2MPU02X_BUCK7:
		mask = 0xff;
		break;
	case S2MPU02X_LDO1 ... S2MPU02X_LDO28:
		mask = 0x3f;
		break;
	default:
		return -EINVAL;
	}

	if (reg_id >= S2MPU02X_BUCK1 && reg_id <= S2MPU02X_BUCK4)
		dvs_ptr = 0x01 | ((reg_id - S2MPU02X_BUCK1) << 3);

	else
		dvs_ptr = 0;

	mutex_lock(&s2mpu02x->lock);
	if (dvs_ptr) {
		ret = sec_reg_write(s2mpu02x->iodev,
					S2MPU02X_REG_DVS_PTR, dvs_ptr);
		if (ret)
			goto err_exit;
	}
	ret = sec_reg_read(s2mpu02x->iodev, reg, &val);
	if (ret)
		goto err_exit;

	ret = val & mask;
err_exit:
	mutex_unlock(&s2mpu02x->lock);
	return ret;
}

static inline int s2mpu02x_convert_voltage_to_sel(
		const struct s2mpu02x_voltage_desc *desc,
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

static int s2mpu02x_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned *selector)
{
	struct s2mpu02x_info *s2mpu02x = rdev_get_drvdata(rdev);
	int min_vol = min_uV, max_vol = max_uV;
	const struct s2mpu02x_voltage_desc *desc;
	int ret, reg_id = rdev_get_id(rdev);
	unsigned int reg, mask;
	int sel;

	mask = (reg_id < S2MPU02X_BUCK1) ? 0x3f : 0xff;

	desc = reg_voltage_map[reg_id];

	sel = s2mpu02x_convert_voltage_to_sel(desc, min_vol, max_vol);
	if (sel < 0)
		return sel;

	ret = s2mpu02x_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	mutex_lock(&s2mpu02x->lock);
	ret = sec_reg_update(s2mpu02x->iodev, reg, sel, mask);
	mutex_unlock(&s2mpu02x->lock);
	*selector = sel;

	return ret;
}

static int s2mpu02x_set_voltage_time_sel(struct regulator_dev *rdev,
					     unsigned int old_sel,
					     unsigned int new_sel)
{
	struct s2mpu02x_info *s2mpu02x = rdev_get_drvdata(rdev);
	const struct s2mpu02x_voltage_desc *desc;
	int reg_id = rdev_get_id(rdev);
	int ramp_delay = 0;

	switch (reg_id) {
	case S2MPU02X_BUCK1:
		ramp_delay = s2mpu02x->ramp_delay1;
		break;
	case S2MPU02X_BUCK2:
		ramp_delay = s2mpu02x->ramp_delay2;
		break;
	case S2MPU02X_BUCK3:
		ramp_delay = s2mpu02x->ramp_delay3;
		break;
	case S2MPU02X_BUCK4:
		ramp_delay = s2mpu02x->ramp_delay4;
		break;
	default:
		return -EINVAL;
	}

	desc = reg_voltage_map[reg_id];

	if (((old_sel < new_sel) && (reg_id >= S2MPU02X_BUCK1)) && ramp_delay) {
		return DIV_ROUND_UP(desc->step * (new_sel - old_sel),
			ramp_delay * 1000);
	}

	return 0;
}

static int s2mpu02x_set_voltage_sel(struct regulator_dev *rdev,
						unsigned selector)
{
	struct s2mpu02x_info *s2mpu02x = rdev_get_drvdata(rdev);
	int mask, ret;
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	u8 data[2];

	ret = s2mpu02x_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPU02X_BUCK1 ... S2MPU02X_BUCK7:
		mask = 0xff;
		break;
	case S2MPU02X_LDO1 ... S2MPU02X_LDO28:
		mask = 0x3f;
		break;
	default:
		return -EINVAL;
	}

	if (reg_id >= S2MPU02X_BUCK1 && reg_id <= S2MPU02X_BUCK4) {
		data[0] = 0x01 | ((reg_id - S2MPU02X_BUCK1) << 3);
		data[1] = selector;
	} else {
		data[0] = 0;
	}

	mutex_lock(&s2mpu02x->lock);
	if (data[0])
		ret = sec_bulk_write(s2mpu02x->iodev,
					S2MPU02X_REG_DVS_PTR, 2, data);
	else
		ret = sec_reg_update(s2mpu02x->iodev, reg, selector, mask);
	mutex_unlock(&s2mpu02x->lock);

	return ret;
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

static struct regulator_ops s2mpu02x_ldo_ops = {
	.list_voltage		= s2mpu02x_list_voltage,
	.is_enabled		= s2mpu02x_reg_is_enabled,
	.enable			= s2mpu02x_reg_enable,
	.disable		= s2mpu02x_reg_disable,
	.get_voltage_sel	= s2mpu02x_get_voltage_sel,
	.set_voltage		= s2mpu02x_set_voltage,
	.set_voltage_time_sel	= s2mpu02x_set_voltage_time_sel,
};

static struct regulator_ops s2mpu02x_buck_ops = {
	.list_voltage		= s2mpu02x_list_voltage,
	.is_enabled		= s2mpu02x_reg_is_enabled,
	.enable			= s2mpu02x_reg_enable,
	.disable		= s2mpu02x_reg_disable,
	.get_voltage_sel	= s2mpu02x_get_voltage_sel,
	.set_voltage_sel	= s2mpu02x_set_voltage_sel,
	.set_voltage_time_sel	= s2mpu02x_set_voltage_time_sel,
};

static struct regulator_ops s2mpu02x_others_ops = {
	.is_enabled		= s2mpu02x_reg_is_enabled,
	.enable			= s2mpu02x_reg_enable,
	.disable		= s2mpu02x_reg_disable,
};

#define regulator_desc_ldo(num)		{	\
	.name		= "LDO"#num,		\
	.id		= S2MPU02X_LDO##num,	\
	.ops		= &s2mpu02x_ldo_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
}
#define regulator_desc_buck(num)	{	\
	.name		= "BUCK"#num,		\
	.id		= S2MPU02X_BUCK##num,	\
	.ops		= &s2mpu02x_buck_ops,	\
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
	regulator_desc_ldo(26),
	regulator_desc_ldo(27),
	regulator_desc_ldo(28),
	regulator_desc_buck(1),
	regulator_desc_buck(2),
	regulator_desc_buck(3),
	regulator_desc_buck(4),
	regulator_desc_buck(5),
	regulator_desc_buck(6),
	regulator_desc_buck(7),
	{
		.name	= "EN32KHz AP",
		.id	= S2MPU02X_AP_EN32KHZ,
		.ops	= &s2mpu02x_others_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
	},
};

static __devinit int s2mpu02x_pmic_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_pmic_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct regulator_dev **rdev;
	struct s2mpu02x_info *s2mpu02x;
	int i, ret, size;
	unsigned int ramp_reg = 0;

	unsigned int val;

	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mpu02x = devm_kzalloc(&pdev->dev, sizeof(struct s2mpu02x_info),
				GFP_KERNEL);
	if (!s2mpu02x)
		return -ENOMEM;

	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	s2mpu02x->rdev = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!s2mpu02x->rdev)
		return -ENOMEM;

	ret = sec_reg_read(iodev, S2MPU02X_REG_ID, &iodev->rev_num);
	if (ret < 0)
		return ret;
	mutex_init(&s2mpu02x->lock);
	rdev = s2mpu02x->rdev;
	s2mpu02x->dev = &pdev->dev;
	s2mpu02x->iodev = iodev;
	s2mpu02x->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, s2mpu02x);

	s2mpu02x->ramp_delay1 = pdata->buck1_ramp_delay;
	s2mpu02x->ramp_delay2 = pdata->buck2_ramp_delay;
	s2mpu02x->ramp_delay3 = pdata->buck3_ramp_delay;
	s2mpu02x->ramp_delay4 = pdata->buck4_ramp_delay;

	s2mpu02x->opmode_data = pdata->opmode_data;

	/* DVS function enable */
	sec_reg_update(s2mpu02x->iodev, S2MPU02X_REG_RSVD11, 0x20, 0x20);

	/* BUCK1 DVS1 initialization */
	sec_reg_read(s2mpu02x->iodev, S2MPU02X_REG_B1CTRL2, &val);
	sec_reg_write(s2mpu02x->iodev, S2MPU02X_REG_DVS_PTR, 0x01);
	sec_reg_write(s2mpu02x->iodev, S2MPU02X_REG_DVS_DATA, val);

	/* BUCK2 DVS1 initialization */
	sec_reg_read(s2mpu02x->iodev, S2MPU02X_REG_B2CTRL2, &val);
	sec_reg_write(s2mpu02x->iodev, S2MPU02X_REG_DVS_PTR, 0x09);
	sec_reg_write(s2mpu02x->iodev, S2MPU02X_REG_DVS_DATA, val);

	/* BUCK3 DVS1 initialization */
	sec_reg_read(s2mpu02x->iodev, S2MPU02X_REG_B3CTRL2, &val);
	sec_reg_write(s2mpu02x->iodev, S2MPU02X_REG_DVS_PTR, 0x11);
	sec_reg_write(s2mpu02x->iodev, S2MPU02X_REG_DVS_DATA, val);

	/* BUCK4 DVS1 initialization */
	sec_reg_read(s2mpu02x->iodev, S2MPU02X_REG_B4CTRL2, &val);
	sec_reg_write(s2mpu02x->iodev, S2MPU02X_REG_DVS_PTR, 0x19);
	sec_reg_write(s2mpu02x->iodev, S2MPU02X_REG_DVS_DATA, val);

	/* DVS0 --> DVS1 */
	sec_reg_write(s2mpu02x->iodev, S2MPU02X_REG_DVS_SEL, 0x01);

	ramp_reg &= 0x00;
	ramp_reg = (get_ramp_delay(s2mpu02x->ramp_delay1) << 6) |
				(get_ramp_delay(s2mpu02x->ramp_delay2) << 4) |
				(get_ramp_delay(s2mpu02x->ramp_delay3) << 2) |
				(get_ramp_delay(s2mpu02x->ramp_delay4));
		sec_reg_update(s2mpu02x->iodev, S2MPU02X_REG_BUCK_RAMP1,
							ramp_reg, 0xff);
#if 0
	/* BUCK1, LDO2,5,6,10,11,12 controlled by PWREN_MIF */
	sec_reg_write(s2mpu02x->iodev, S2MPU02X_REG_SELMIF, 0x7f);
#else
	/* BUCK1, LDO2,5,6,10,11,12 controlled by PWREN */
	sec_reg_write(s2mpu02x->iodev, S2MPU02X_REG_SELMIF, 0x00);
#endif
	for (i = 0; i < pdata->num_regulators; i++) {
		const struct s2mpu02x_voltage_desc *desc;
		int id = pdata->regulators[i].id;

		desc = reg_voltage_map[id];
		if (desc)
			regulators[id].n_voltages =
				(desc->max - desc->min) / desc->step + 1;

		rdev[i] = regulator_register(&regulators[id], s2mpu02x->dev,
				pdata->regulators[i].initdata, s2mpu02x, NULL);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(s2mpu02x->dev, "regulator init failed for %d\n",
					id);
			rdev[i] = NULL;
			goto err;
		}
	}

	return 0;
err:
	for (i = 0; i < s2mpu02x->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	return ret;
}

static int __devexit s2mpu02x_pmic_remove(struct platform_device *pdev)
{
	struct s2mpu02x_info *s2mpu02x = platform_get_drvdata(pdev);
	struct regulator_dev **rdev = s2mpu02x->rdev;
	int i;

	for (i = 0; i < s2mpu02x->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	return 0;
}

static const struct platform_device_id s2mpu02x_pmic_id[] = {
	{ "s2mpu02x-pmic", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpu02x_pmic_id);

static struct platform_driver s2mpu02x_pmic_driver = {
	.driver = {
		.name = "s2mpu02x-pmic",
		.owner = THIS_MODULE,
	},
	.probe = s2mpu02x_pmic_probe,
	.remove = __devexit_p(s2mpu02x_pmic_remove),
	.id_table = s2mpu02x_pmic_id,
};

static int __init s2mpu02x_pmic_init(void)
{
	return platform_driver_register(&s2mpu02x_pmic_driver);
}
subsys_initcall(s2mpu02x_pmic_init);

static void __exit s2mpu02x_pmic_exit(void)
{
	platform_driver_unregister(&s2mpu02x_pmic_driver);
}
module_exit(s2mpu02x_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_AUTHOR("Dongsu Ha <dsfine.ha@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPU02X Regulator Driver");
MODULE_LICENSE("GPL");
