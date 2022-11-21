/*
 * I2C driver for Marvell 88PM80x
 *
 * Copyright (C) 2012 Marvell International Ltd.
 * Haojian Zhuang <haojian.zhuang@marvell.com>
 * Joseph(Yossi) Hanin <yhanin@marvell.com>
 * Qiao Zhou <zhouqiao@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mfd/88pm80x.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/err.h>

/*
 * workaround: some registers needed by pm805 are defined in pm800, so
 * need to use this global variable to maintain the relation between
 * pm800 and pm805. would remove it after HW chip fixes the issue.
 */
static struct pm80x_chip *g_pm80x_chip;

const struct regmap_config pm80x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};
EXPORT_SYMBOL_GPL(pm80x_regmap_config);

int pm80x_init(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct pm80x_chip *chip;
	struct regmap *map;
	int ret = 0;

	chip =
	    devm_kzalloc(&client->dev, sizeof(struct pm80x_chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	map = devm_regmap_init_i2c(client, &pm80x_regmap_config);
	if (IS_ERR(map)) {
		ret = PTR_ERR(map);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	chip->id = id->driver_data;
	if (chip->id < CHIP_PM800 || chip->id > CHIP_PM805)
		return -EINVAL;

	chip->client = client;
	chip->regmap = map;

	chip->irq = client->irq;

	chip->dev = &client->dev;
	dev_set_drvdata(chip->dev, chip);
	i2c_set_clientdata(chip->client, chip);

	device_init_wakeup(&client->dev, 1);

	/*
	 * workaround: set g_pm80x_chip to the first probed chip. if the
	 * second chip is probed, just point to the companion to each
	 * other so that pm805 can access those specific register. would
	 * remove it after HW chip fixes the issue.
	 */
	if (!g_pm80x_chip)
		g_pm80x_chip = chip;
	else {
		chip->companion = g_pm80x_chip->client;
		g_pm80x_chip->companion = chip->client;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pm80x_init);

int pm80x_deinit(void)
{
	/*
	 * workaround: clear the dependency between pm800 and pm805.
	 * would remove it after HW chip fixes the issue.
	 */
	if (g_pm80x_chip->companion)
		g_pm80x_chip->companion = NULL;
	else
		g_pm80x_chip = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(pm80x_deinit);

static inline struct pm80x_chip *get_pm800_chip(void)
{
	if (!g_pm80x_chip) {
		pr_err("%s: g_pm80x_chip is NOT available!\n", __FILE__);
		return NULL;
	}
	if (g_pm80x_chip->id == CHIP_PM800)
		return g_pm80x_chip;
	else if (g_pm80x_chip->companion != NULL)
		return i2c_get_clientdata(g_pm80x_chip->companion);
	else
		return NULL;
}

int pm800_extern_read(int page, int reg)
{
	int ret;
	unsigned int val;
	struct pm80x_chip *chip = get_pm800_chip();
	if (!chip) {
		pr_err("%s: chip is NULL\n", __func__);
		return -EINVAL;
	}
	switch (page) {
	case PM80X_BASE_PAGE:
		ret = regmap_read(chip->regmap, reg, &val);
		break;
	case PM80X_POWER_PAGE:
		ret = regmap_read(chip->subchip->regmap_power,
				  reg, &val);
		break;
	case PM80X_GPADC_PAGE:
		ret = regmap_read(chip->subchip->regmap_gpadc,
				  reg, &val);
		break;
	default:
		ret = -1;
		break;
	}

	if (ret < 0) {
		pr_err("fail to read reg 0x%x\n", reg);
		return ret;
	}

	return val;
}
EXPORT_SYMBOL(pm800_extern_read);

int pm800_extern_write(int page, int reg, unsigned char val)
{
	int ret;
	struct pm80x_chip *chip = get_pm800_chip();
	if (!chip) {
		pr_err("%s: chip is NULL\n", __func__);
		return -EINVAL;
	}
	switch (page) {
	case PM80X_BASE_PAGE:
		ret = regmap_write(chip->regmap, reg, val);
		break;
	case PM80X_POWER_PAGE:
		ret = regmap_write(chip->subchip->regmap_power,
				  reg, val);
		break;
	case PM80X_GPADC_PAGE:
		ret = regmap_write(chip->subchip->regmap_gpadc,
				  reg, val);
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}
EXPORT_SYMBOL(pm800_extern_write);

int pm800_extern_setbits(int page, int reg,
			 unsigned char mask, unsigned char val)
{
	int ret;
	struct pm80x_chip *chip = get_pm800_chip();
	if (!chip) {
		pr_err("%s: chip is NULL\n", __func__);
		return -EINVAL;
	}
	switch (page) {
	case PM80X_BASE_PAGE:
		ret = regmap_update_bits(chip->regmap, reg, mask, val);
		break;
	case PM80X_POWER_PAGE:
		ret = regmap_update_bits(chip->subchip->regmap_power,
					 reg, mask, val);
		break;
	case PM80X_GPADC_PAGE:
		ret = regmap_update_bits(chip->subchip->regmap_gpadc,
					 reg, mask, val);
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}
EXPORT_SYMBOL(pm800_extern_setbits);


#if defined(CONFIG_SPA) ||			\
	defined(CONFIG_MACH_LT02)
extern int pm80x_read_temperature(int *tbat)
{
      unsigned char buf[2];
	int sum = 0, ret = -1;
	*tbat=0;
	struct pm80x_chip *chip = get_pm800_chip();
	ret =regmap_bulk_read(chip->subchip->regmap_gpadc, PM800_GPADC1_MEAS1, buf,2);
	if (ret >= 0)
	{
	sum = ((buf[0] & 0xFF) << 4) | (buf[1] & 0x0F);
	sum = ((sum & 0xFFF) * 1400) >> 12;
	 *tbat = sum;
	 }
	 return ret;
}
EXPORT_SYMBOL(pm80x_read_temperature);

extern int pm80x_rf_read_temperature(int *tbat)
{
	unsigned char buf[2];
	int sum = 0, ret = -1;
	*tbat=0;
	struct pm80x_chip *chip = get_pm800_chip();
	ret =regmap_bulk_read(chip->subchip->regmap_gpadc, PM800_GPADC0_MEAS1, buf,2);
	if (ret >= 0)
	{
		sum = ((buf[0] & 0xFF) << 4) | (buf[1] & 0x0F);
		sum = ((sum & 0xFFF) * 1400) >> 12;
		*tbat = sum;
	}
	return ret;
}
EXPORT_SYMBOL(pm80x_rf_read_temperature);
extern int pm80x_read_vf(int *vfbat)
{
	unsigned char buf[2];
	int sum = 0, ret = -1;
	u8 reg;

	*vfbat=0;
	struct pm80x_chip *chip = get_pm800_chip();

	ret =regmap_bulk_read(chip->subchip->regmap_gpadc, PM800_GPADC3_MEAS1, buf,2);
	if (ret >= 0)
	{
		sum = ((buf[0] & 0xFF) << 4) | (buf[1] & 0x0F);
		sum = ((sum & 0xFFF) * 1400) >> 12;
		*vfbat = sum;
	}

	return ret;
}
EXPORT_SYMBOL(pm80x_read_vf);
#endif


#ifdef CONFIG_PM_SLEEP
static int pm80x_suspend(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct pm80x_chip *chip = i2c_get_clientdata(client);
	int i, tmp;

	if (chip) {
		tmp = chip->wu_flag;
		if (tmp && device_may_wakeup(chip->dev)) {
			enable_irq_wake(chip->irq);

			for (i = 0; i < 32; i++) {
				if (tmp & (1 << i))
					enable_irq_wake(chip->irq_base + i);
			}
		}
	}

	return 0;
}

static int pm80x_resume(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct pm80x_chip *chip = i2c_get_clientdata(client);
	int i, tmp;

	if (chip) {
		tmp = chip->wu_flag;
		if (tmp && device_may_wakeup(chip->dev)) {
			disable_irq_wake(chip->irq);

			for (i = 0; i < 32; i++) {
				if (tmp & (1 << i))
					disable_irq_wake(chip->irq_base + i);
			}
		}
	}
	return 0;
}
#endif

SIMPLE_DEV_PM_OPS(pm80x_pm_ops, pm80x_suspend, pm80x_resume);
EXPORT_SYMBOL_GPL(pm80x_pm_ops);

MODULE_DESCRIPTION("I2C Driver for Marvell 88PM80x");
MODULE_AUTHOR("Qiao Zhou <zhouqiao@marvell.com>");
MODULE_LICENSE("GPL");
