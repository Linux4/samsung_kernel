/*
 * s2mpa01.c
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
#include <linux/mfd/samsung/s2mpa01.h>



struct s2mpa01_info {
	struct device *dev;
#ifdef CONFIG_SEC_PM
	struct device *sec_power;
#endif
	struct sec_pmic_dev *iodev;
	int num_regulators;
	struct regulator_dev **rdev;
	struct sec_opmode_data *opmode_data;
	struct mutex lock;

	int ramp_delay24;
	int ramp_delay3;
	int ramp_delay5;
	int ramp_delay16;
	int ramp_delay7;
	int ramp_delay8910;

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

struct s2mpa01_voltage_desc {
	int max;
	int min;
	int step;
};

static const struct s2mpa01_voltage_desc buck_voltage_val1 = {
	.max = 1500000,
	.min =  600000,
	.step =   6250,
};

static const struct s2mpa01_voltage_desc buck_voltage_val2 = {
	.max = 3000000,
	.min =  800000,
	.step =  12500,
};
static const struct s2mpa01_voltage_desc ldo_voltage_val1 = {
	.max = 3950000,
	.min =  800000,
	.step =  50000,
};

static const struct s2mpa01_voltage_desc ldo_voltage_val2 = {
	.max = 2375000,
	.min =  800000,
	.step =  25000,
};

static const struct s2mpa01_voltage_desc *reg_voltage_map[] = {
	[S2MPA01_LDO1] = &ldo_voltage_val2,
	[S2MPA01_LDO2] = &ldo_voltage_val1,
	[S2MPA01_LDO3] = &ldo_voltage_val1,
	[S2MPA01_LDO4] = &ldo_voltage_val1,
	[S2MPA01_LDO5] = &ldo_voltage_val2,
	[S2MPA01_LDO6] = &ldo_voltage_val2,
	[S2MPA01_LDO7] = &ldo_voltage_val1,
	[S2MPA01_LDO8] = &ldo_voltage_val1,
	[S2MPA01_LDO9] = &ldo_voltage_val1,
	[S2MPA01_LDO10] = &ldo_voltage_val1,
	[S2MPA01_LDO11] = &ldo_voltage_val1,
	[S2MPA01_LDO12] = &ldo_voltage_val1,
	[S2MPA01_LDO13] = &ldo_voltage_val1,
	[S2MPA01_LDO14] = &ldo_voltage_val1,
	[S2MPA01_LDO15] = &ldo_voltage_val1,
	[S2MPA01_LDO16] = &ldo_voltage_val1,
	[S2MPA01_LDO17] = &ldo_voltage_val1,
	[S2MPA01_LDO18] = &ldo_voltage_val1,
	[S2MPA01_LDO19] = &ldo_voltage_val1,
	[S2MPA01_LDO20] = &ldo_voltage_val1,
	[S2MPA01_LDO21] = &ldo_voltage_val1,
	[S2MPA01_LDO22] = &ldo_voltage_val1,
	[S2MPA01_LDO23] = &ldo_voltage_val1,
	[S2MPA01_LDO24] = &ldo_voltage_val1,
	[S2MPA01_LDO25] = &ldo_voltage_val1,
	[S2MPA01_LDO26] = &ldo_voltage_val2,
	[S2MPA01_BUCK1] = &buck_voltage_val1,
	[S2MPA01_BUCK2] = &buck_voltage_val1,
	[S2MPA01_BUCK3] = &buck_voltage_val1,
	[S2MPA01_BUCK4] = &buck_voltage_val1,
	[S2MPA01_BUCK5] = &buck_voltage_val1,
	[S2MPA01_BUCK6] = &buck_voltage_val1,
	[S2MPA01_BUCK7] = &buck_voltage_val1,
	[S2MPA01_BUCK8] = &buck_voltage_val2,
	[S2MPA01_BUCK9] = &buck_voltage_val2,
	[S2MPA01_BUCK10] = &buck_voltage_val2,
};

static int s2mpa01_list_voltage(struct regulator_dev *rdev,
				unsigned int selector)
{
	const struct s2mpa01_voltage_desc *desc;
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

unsigned int s2mpa01_opmode_reg[][3] = {
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

static int s2mpa01_get_register(struct regulator_dev *rdev,
	unsigned int *reg, int *pmic_en)
{
	int reg_id = rdev_get_id(rdev);
	unsigned int mode;
	struct s2mpa01_info *s2mpa01 = rdev_get_drvdata(rdev);

	switch (reg_id) {
	case S2MPA01_LDO1 ... S2MPA01_LDO26:
		*reg = S2MPA01_REG_L1CTRL + (reg_id - S2MPA01_LDO1);
		break;
	case S2MPA01_BUCK1 ... S2MPA01_BUCK5:
		*reg = S2MPA01_REG_B1CTRL1 + (reg_id - S2MPA01_BUCK1) * 2;
		break;
	case S2MPA01_BUCK6 ... S2MPA01_BUCK10:
		*reg = S2MPA01_REG_B6CTRL1 + (reg_id - S2MPA01_BUCK6) * 2;
		break;
	case S2MPA01_AP_EN32KHZ ... S2MPA01_CP_EN32KHZ:
		*reg = S2MPA01_REG_RTC_BUF;
		*pmic_en = 0x01 << (reg_id - S2MPA01_AP_EN32KHZ);
		return 0;
	default:
		return -EINVAL;
	}

	mode = s2mpa01->opmode_data[reg_id].mode;
	*pmic_en = s2mpa01_opmode_reg[reg_id][mode] << S2MPA01_PMIC_EN_SHIFT;

	return 0;
}

static int s2mpa01_reg_is_enabled(struct regulator_dev *rdev)
{
	struct s2mpa01_info *s2mpa01 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg, val;
	int ret, mask = 0xc0, pmic_en;

	ret = s2mpa01_get_register(rdev, &reg, &pmic_en);
	if (ret == -EINVAL)
		return 1;
	else if (ret)
		return ret;

	ret = sec_reg_read(s2mpa01->iodev, reg, &val);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPA01_LDO1 ... S2MPA01_BUCK10:
		mask = 0xc0;
		break;
	case S2MPA01_AP_EN32KHZ:
		mask = 0x01;
		break;
	case S2MPA01_CP_EN32KHZ:
		mask = 0x02;
		break;
	default:
		return -EINVAL;
	}

	return (val & mask) == pmic_en;
}

static int s2mpa01_reg_enable(struct regulator_dev *rdev)
{
	struct s2mpa01_info *s2mpa01 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	int ret, mask, pmic_en;

	ret = s2mpa01_get_register(rdev, &reg, &pmic_en);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPA01_LDO1 ... S2MPA01_BUCK10:
		mask = 0xc0;
		break;
	case S2MPA01_AP_EN32KHZ:
		mask = 0x01;
		break;
	case S2MPA01_CP_EN32KHZ:
		mask = 0x02;
		break;
	default:
		return -EINVAL;
	}

	return sec_reg_update(s2mpa01->iodev, reg, pmic_en, mask);
}

static int s2mpa01_reg_disable(struct regulator_dev *rdev)
{
	struct s2mpa01_info *s2mpa01 = rdev_get_drvdata(rdev);
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	int ret, mask, pmic_en;

	ret = s2mpa01_get_register(rdev, &reg, &pmic_en);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPA01_LDO1 ... S2MPA01_BUCK10:
		mask = 0xc0;
		break;
	case S2MPA01_AP_EN32KHZ:
		mask = 0x01;
		break;
	case S2MPA01_CP_EN32KHZ:
		mask = 0x02;
		break;
	default:
		return -EINVAL;
	}

	return sec_reg_update(s2mpa01->iodev, reg, ~mask, mask);
}

static int s2mpa01_get_voltage_register(struct regulator_dev *rdev,
		unsigned int *_reg)
{
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;

	switch (reg_id) {
	case S2MPA01_LDO1 ... S2MPA01_LDO26:
		reg = S2MPA01_REG_L1CTRL + (reg_id - S2MPA01_LDO1);
		break;
	case S2MPA01_BUCK1 ... S2MPA01_BUCK4:
	case S2MPA01_BUCK6:
		reg = S2MPA01_REG_DVS_DATA;
		break;
	case S2MPA01_BUCK5:
		reg = S2MPA01_REG_B5CTRL3;
		break;
	case S2MPA01_BUCK7 ... S2MPA01_BUCK10:
		reg = S2MPA01_REG_B7CTRL2 + (reg_id - S2MPA01_BUCK7) * 2;
		break;
	default:
		return -EINVAL;
	}

	*_reg = reg;

	return 0;
}

static int s2mpa01_get_voltage_sel(struct regulator_dev *rdev)
{
	struct s2mpa01_info *s2mpa01 = rdev_get_drvdata(rdev);
	int mask, ret;
	int reg_id = rdev_get_id(rdev);
	unsigned int reg, val, dvs_ptr;

	ret = s2mpa01_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPA01_BUCK1 ... S2MPA01_BUCK10:
		mask = 0xff;
		break;
	case S2MPA01_LDO1 ... S2MPA01_LDO26:
		mask = 0x3f;
		break;
	default:
		return -EINVAL;
	}

	if (reg_id >= S2MPA01_BUCK1 && reg_id <= S2MPA01_BUCK4)
		dvs_ptr = 0x01 | ((reg_id - S2MPA01_BUCK1) << 3);
	else if (reg_id == S2MPA01_BUCK6)
		dvs_ptr = 0x01 | (0x04 << 3);
	else
		dvs_ptr = 0;

	mutex_lock(&s2mpa01->lock);
	if (dvs_ptr) {
		ret = sec_reg_write(s2mpa01->iodev, S2MPA01_REG_DVS_PTR, dvs_ptr);
		if (ret)
			goto err_exit;
	}
	ret = sec_reg_read(s2mpa01->iodev, reg, &val);
	if (ret)
		goto err_exit;

	ret = val & mask;
err_exit:
	mutex_unlock(&s2mpa01->lock);
	return ret;
}

static inline int s2mpa01_convert_voltage_to_sel(
		const struct s2mpa01_voltage_desc *desc,
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

static int s2mpa01_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned *selector)
{
	struct s2mpa01_info *s2mpa01 = rdev_get_drvdata(rdev);
	int min_vol = min_uV, max_vol = max_uV;
	const struct s2mpa01_voltage_desc *desc;
	int ret, reg_id = rdev_get_id(rdev);
	unsigned int reg, mask;
	int sel;

	mask = (reg_id < S2MPA01_BUCK1) ? 0x3f : 0xff;

	desc = reg_voltage_map[reg_id];

	sel = s2mpa01_convert_voltage_to_sel(desc, min_vol, max_vol);
	if (sel < 0)
		return sel;

	ret = s2mpa01_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	mutex_lock(&s2mpa01->lock);
	ret = sec_reg_update(s2mpa01->iodev, reg, sel, mask);
	mutex_unlock(&s2mpa01->lock);

	*selector = sel;

	return ret;
}

static int s2mpa01_set_voltage_time_sel(struct regulator_dev *rdev,
					     unsigned int old_sel,
					     unsigned int new_sel)
{
	struct s2mpa01_info *s2mpa01 = rdev_get_drvdata(rdev);
	const struct s2mpa01_voltage_desc *desc;
	int reg_id = rdev_get_id(rdev);
	int ramp_delay = 0;

	switch (reg_id) {
	case S2MPA01_BUCK1:
	case S2MPA01_BUCK6:
		ramp_delay = s2mpa01->ramp_delay16;
		break;
	case S2MPA01_BUCK2:
	case S2MPA01_BUCK4:
		ramp_delay = s2mpa01->ramp_delay24;
		break;
	case S2MPA01_BUCK3:
		ramp_delay = s2mpa01->ramp_delay3;
		break;
	case S2MPA01_BUCK5:
		ramp_delay = s2mpa01->ramp_delay5;
		break;
	case S2MPA01_BUCK7:
		ramp_delay = s2mpa01->ramp_delay7;
		break;
	case S2MPA01_BUCK8 ... S2MPA01_BUCK10:
		ramp_delay = s2mpa01->ramp_delay8910;
		break;
	default:
		return -EINVAL;
	}

	desc = reg_voltage_map[reg_id];

	if (((old_sel < new_sel) && (reg_id >= S2MPA01_BUCK1)) && ramp_delay) {
		return DIV_ROUND_UP(desc->step * (new_sel - old_sel),
			ramp_delay * 1000);
	}

	return 0;
}

static int s2mpa01_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	struct s2mpa01_info *s2mpa01 = rdev_get_drvdata(rdev);
	int mask, ret;
	int reg_id = rdev_get_id(rdev);
	unsigned int reg;
	u8 data[2];

	ret = s2mpa01_get_voltage_register(rdev, &reg);
	if (ret)
		return ret;

	switch (reg_id) {
	case S2MPA01_BUCK1 ... S2MPA01_BUCK10:
		mask = 0xff;
		break;
	case S2MPA01_LDO1 ... S2MPA01_LDO26:
		mask = 0x3f;
		break;
	default:
		return -EINVAL;
	}

	if ((reg_id >= S2MPA01_BUCK1 && reg_id <= S2MPA01_BUCK4)) {
		data[0] = 0x01 | ((reg_id - S2MPA01_BUCK1) << 3);
		data[1] = selector;
	} else if (reg_id == S2MPA01_BUCK6) {
		data[0] = 0x01 | (0x04 << 3);
		data[1] = selector;
	} else {
		data[0] = 0;
	}

	mutex_lock(&s2mpa01->lock);
	if (data[0])
		ret = sec_bulk_write(s2mpa01->iodev, S2MPA01_REG_DVS_PTR, 2, data);
	else
		ret = sec_reg_update(s2mpa01->iodev, reg, selector, mask);
	mutex_unlock(&s2mpa01->lock);

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

static struct regulator_ops s2mpa01_ldo_ops = {
	.list_voltage		= s2mpa01_list_voltage,
	.is_enabled		= s2mpa01_reg_is_enabled,
	.enable			= s2mpa01_reg_enable,
	.disable		= s2mpa01_reg_disable,
	.get_voltage_sel	= s2mpa01_get_voltage_sel,
	.set_voltage		= s2mpa01_set_voltage,
	.set_voltage_time_sel	= s2mpa01_set_voltage_time_sel,
};

static struct regulator_ops s2mpa01_buck_ops = {
	.list_voltage		= s2mpa01_list_voltage,
	.is_enabled		= s2mpa01_reg_is_enabled,
	.enable			= s2mpa01_reg_enable,
	.disable		= s2mpa01_reg_disable,
	.get_voltage_sel	= s2mpa01_get_voltage_sel,
	.set_voltage_sel	= s2mpa01_set_voltage_sel,
	.set_voltage_time_sel	= s2mpa01_set_voltage_time_sel,
};

static struct regulator_ops s2mpa01_others_ops = {
	.is_enabled		= s2mpa01_reg_is_enabled,
	.enable			= s2mpa01_reg_enable,
	.disable		= s2mpa01_reg_disable,
};

#define regulator_desc_ldo(num)		{	\
	.name		= "LDO"#num,		\
	.id		= S2MPA01_LDO##num,	\
	.ops		= &s2mpa01_ldo_ops,	\
	.type		= REGULATOR_VOLTAGE,	\
	.owner		= THIS_MODULE,		\
}
#define regulator_desc_buck(num)	{	\
	.name		= "BUCK"#num,		\
	.id		= S2MPA01_BUCK##num,	\
	.ops		= &s2mpa01_buck_ops,	\
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
	regulator_desc_buck(1),
	regulator_desc_buck(2),
	regulator_desc_buck(3),
	regulator_desc_buck(4),
	regulator_desc_buck(5),
	regulator_desc_buck(6),
	regulator_desc_buck(7),
	regulator_desc_buck(8),
	regulator_desc_buck(9),
	regulator_desc_buck(10),
	{
		.name	= "EN32KHz AP",
		.id	= S2MPA01_AP_EN32KHZ,
		.ops	= &s2mpa01_others_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
	}, {
		.name	= "EN32KHz CP",
		.id	= S2MPA01_CP_EN32KHZ,
		.ops	= &s2mpa01_others_ops,
		.type	= REGULATOR_VOLTAGE,
		.owner	= THIS_MODULE,
	},
};

#ifdef CONFIG_SEC_PM
extern struct class *sec_class;
#define MRSTB_EN_BIT BIT(3)

static ssize_t switch_show_manual_reset(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mpa01_info *s2mpa01 = dev_get_drvdata(dev);
	unsigned int val;

	sec_reg_read(s2mpa01->iodev, S2MPA01_REG_CTRL1, &val);
	val &= MRSTB_EN_BIT;

	return sprintf(buf, "%d", val ? 1 : 0);
}

static ssize_t switch_store_manual_reset(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct s2mpa01_info *s2mpa01 = dev_get_drvdata(dev);
	unsigned int val, mask;
	int ret;

	if (!strncmp(buf, "0", 1)) {
		val = 0;
	} else if (!strncmp(buf, "1", 1)) {
		val = MRSTB_EN_BIT;
	} else {
		pr_warn("%s: Wrong command:%s\n", __func__, buf);
		return -EINVAL;
	}

	mask = MRSTB_EN_BIT;
	ret = sec_reg_update(s2mpa01->iodev, S2MPA01_REG_CTRL1, val, mask);
	if (ret < 0)
		return ret;
	return count;
}

static DEVICE_ATTR(enable_hw_reset, 0664, switch_show_manual_reset,
		switch_store_manual_reset);
#endif /* CONFIG_SEC_PM */


static __devinit int s2mpa01_pmic_probe(struct platform_device *pdev)
{
	struct sec_pmic_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct sec_pmic_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct regulator_dev **rdev;
	struct s2mpa01_info *s2mpa01;
	int i, ret, size;
	unsigned int ramp_enable, ramp_reg = 0;
	unsigned int buck_init;
	if (!pdata) {
		dev_err(pdev->dev.parent, "Platform data not supplied\n");
		return -ENODEV;
	}

	s2mpa01 = devm_kzalloc(&pdev->dev, sizeof(struct s2mpa01_info),
				GFP_KERNEL);
	if (!s2mpa01)
		return -ENOMEM;

	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	s2mpa01->rdev = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!s2mpa01->rdev)
		return -ENOMEM;

	ret = sec_reg_read(iodev, S2MPA01_REG_ID, &iodev->rev_num);
	if (ret < 0)
		return ret;

	mutex_init(&s2mpa01->lock);

	rdev = s2mpa01->rdev;
	s2mpa01->dev = &pdev->dev;
	s2mpa01->iodev = iodev;
	s2mpa01->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, s2mpa01);

#ifdef CONFIG_SEC_PM
		s2mpa01->sec_power= device_create(sec_class, NULL, 0, NULL, "sec_power");
		if (IS_ERR(s2mpa01->sec_power)) {
			ret = PTR_ERR(s2mpa01->sec_power);
			dev_err(&pdev->dev, "Failed to create sec_power:%d\n", ret);
			return ret;
		}
		dev_set_drvdata(s2mpa01->sec_power, s2mpa01);
	
		ret = device_create_file(s2mpa01->sec_power, &dev_attr_enable_hw_reset);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to create enable_hw_reset:%d\n", ret);
			return ret;
		}
#endif

	s2mpa01->ramp_delay24 = pdata->buck24_ramp_delay;
	s2mpa01->ramp_delay3 = pdata->buck3_ramp_delay;
	s2mpa01->ramp_delay5 = pdata->buck5_ramp_delay;
	s2mpa01->ramp_delay16 = pdata->buck16_ramp_delay;
	s2mpa01->ramp_delay7 = pdata->buck7_ramp_delay;
	s2mpa01->ramp_delay8910 = pdata->buck8910_ramp_delay;

	s2mpa01->buck1_ramp = pdata->buck1_ramp_enable;
	s2mpa01->buck2_ramp = pdata->buck2_ramp_enable;
	s2mpa01->buck3_ramp = pdata->buck3_ramp_enable;
	s2mpa01->buck4_ramp = pdata->buck4_ramp_enable;
	s2mpa01->opmode_data = pdata->opmode_data;

	sec_reg_update(s2mpa01->iodev, S2MPA01_REG_LEE_NO, 0x20, 0x20);

	if (pdata->buck1_init) {
		buck_init = s2mpa01_convert_voltage_to_sel(&buck_voltage_val1,
						pdata->buck1_init,
						pdata->buck1_init +
						buck_voltage_val1.step);
	} else {
		sec_reg_read(s2mpa01->iodev, S2MPA01_REG_B1CTRL2, &buck_init);
	}
	sec_reg_write(s2mpa01->iodev, S2MPA01_REG_DVS_PTR, 0x01);
	sec_reg_write(s2mpa01->iodev, S2MPA01_REG_DVS_DATA, buck_init);

	if (pdata->buck2_init) {
		buck_init = s2mpa01_convert_voltage_to_sel(&buck_voltage_val1,
						pdata->buck2_init,
						pdata->buck2_init +
						buck_voltage_val1.step);
	} else {
		sec_reg_read(s2mpa01->iodev, S2MPA01_REG_B2CTRL2, &buck_init);
	}
	sec_reg_write(s2mpa01->iodev, S2MPA01_REG_DVS_PTR, 0x09);
	sec_reg_write(s2mpa01->iodev, S2MPA01_REG_DVS_DATA, buck_init);

	if (pdata->buck3_init) {
		buck_init = s2mpa01_convert_voltage_to_sel(&buck_voltage_val1,
						pdata->buck3_init,
						pdata->buck3_init +
						buck_voltage_val1.step);
	} else {
		sec_reg_read(s2mpa01->iodev, S2MPA01_REG_B3CTRL2, &buck_init);
	}
	sec_reg_write(s2mpa01->iodev, S2MPA01_REG_DVS_PTR, 0x11);
	sec_reg_write(s2mpa01->iodev, S2MPA01_REG_DVS_DATA, buck_init);

	if (pdata->buck4_init) {
		buck_init = s2mpa01_convert_voltage_to_sel(&buck_voltage_val1,
						pdata->buck4_init,
						pdata->buck4_init +
						buck_voltage_val1.step);
	} else {
		sec_reg_read(s2mpa01->iodev, S2MPA01_REG_B4CTRL2, &buck_init);
	}
	sec_reg_write(s2mpa01->iodev, S2MPA01_REG_DVS_PTR, 0x19);
	sec_reg_write(s2mpa01->iodev, S2MPA01_REG_DVS_DATA, buck_init);

	if (pdata->buck6_init) {
		buck_init = s2mpa01_convert_voltage_to_sel(&buck_voltage_val1,
						pdata->buck6_init,
						pdata->buck6_init +
						buck_voltage_val1.step);
	} else {
		sec_reg_read(s2mpa01->iodev, S2MPA01_REG_B6CTRL2, &buck_init);
	}
	sec_reg_write(s2mpa01->iodev, S2MPA01_REG_DVS_PTR, 0x21);
	sec_reg_write(s2mpa01->iodev, S2MPA01_REG_DVS_DATA, buck_init);

	sec_reg_write(s2mpa01->iodev, S2MPA01_REG_DVS_SEL, 0x01);

	ramp_enable = (s2mpa01->buck1_ramp << 3) | (s2mpa01->buck2_ramp << 2) |
		(s2mpa01->buck3_ramp << 1) | s2mpa01->buck4_ramp ;

	if (ramp_enable) {
		if (s2mpa01->buck2_ramp || s2mpa01->buck4_ramp)
			ramp_reg |= get_ramp_delay(s2mpa01->ramp_delay24) << 6;
		if (s2mpa01->buck3_ramp)
			ramp_reg |= get_ramp_delay(s2mpa01->ramp_delay3) << 4;
		sec_reg_update(s2mpa01->iodev, S2MPA01_REG_BUCK_RAMP1,
			ramp_reg | ramp_enable, 0xff);
	}

	ramp_reg &= 0x00;
	ramp_reg |= get_ramp_delay(s2mpa01->ramp_delay5) << 6;
	ramp_reg |= get_ramp_delay(s2mpa01->ramp_delay16) << 4;
	ramp_reg |= get_ramp_delay(s2mpa01->ramp_delay7) << 2;
	ramp_reg |= get_ramp_delay(s2mpa01->ramp_delay8910);
	sec_reg_update(s2mpa01->iodev, S2MPA01_REG_BUCK_RAMP2, ramp_reg, 0xff);

	for (i = 0; i < pdata->num_regulators; i++) {
		const struct s2mpa01_voltage_desc *desc;
		int id = pdata->regulators[i].id;

		desc = reg_voltage_map[id];
		if (desc)
			regulators[id].n_voltages =
				(desc->max - desc->min) / desc->step + 1;

		rdev[i] = regulator_register(&regulators[id], s2mpa01->dev,
				pdata->regulators[i].initdata, s2mpa01, NULL);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(s2mpa01->dev, "regulator init failed for %d\n",
					id);
			rdev[i] = NULL;
			goto err;
		}
	}

	return 0;
err:
	for (i = 0; i < s2mpa01->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	return ret;
}

static int __devexit s2mpa01_pmic_remove(struct platform_device *pdev)
{
	struct s2mpa01_info *s2mpa01 = platform_get_drvdata(pdev);
	struct regulator_dev **rdev = s2mpa01->rdev;
	int i;

	for (i = 0; i < s2mpa01->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	return 0;
}

static const struct platform_device_id s2mpa01_pmic_id[] = {
	{ "s2mpa01-pmic", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpa01_pmic_id);

static struct platform_driver s2mpa01_pmic_driver = {
	.driver = {
		.name = "s2mpa01-pmic",
		.owner = THIS_MODULE,
	},
	.probe = s2mpa01_pmic_probe,
	.remove = __devexit_p(s2mpa01_pmic_remove),
	.id_table = s2mpa01_pmic_id,
};

static int __init s2mpa01_pmic_init(void)
{
	return platform_driver_register(&s2mpa01_pmic_driver);
}
subsys_initcall(s2mpa01_pmic_init);

static void __exit s2mpa01_pmic_exit(void)
{
	platform_driver_unregister(&s2mpa01_pmic_driver);
}
module_exit(s2mpa01_pmic_exit);

/* Module information */
MODULE_AUTHOR("Sangbeom Kim <sbkim73@samsung.com>");
MODULE_AUTHOR("Dongsu Ha <dsfine.ha@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG S2MPA01 Regulator Driver");
MODULE_LICENSE("GPL");
