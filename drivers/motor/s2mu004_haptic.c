/*
 * haptic motor driver for s2mu004 - s2mu004_haptic.c
 *
 * Copyright (C) 2011 ByungChang Cha <bc.cha@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include "../staging/android/timed_output.h"
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/s2mu004_haptic.h>
#include <linux/kthread.h>
//#include <plat/devs.h>
#include <linux/delay.h>
#include <linux/sec_sysfs.h>
#include <linux/power_supply.h>

#define AUTO_CAL_EN 1
#define TEST_MODE_TIME 10000

#define MOTOR_EN	(1<<0)
#define AUTO_CAL	(1<<2)
#define MOTOR_LRA	(1<<4)
#define MOTOR_ERM	(0<<4)
#define MOTOR_MODE	0x10
#define EXT_PWM		(1<<5)

#define ERM_MODE	0

struct s2mu004_haptic_data {
	struct s2mu004_dev *s2mu004;
	struct i2c_client *i2c;
	struct s2mu004_haptic_platform_data *pdata;
	bool running;
};

struct s2mu004_haptic_data *s2mu004_g_hap_data;

static int s2mu004_write_reg(struct i2c_client *client, u8 reg, u8 data)
{
	int ret, i = 0;

	ret = i2c_smbus_write_byte_data(client, reg,  data);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = i2c_smbus_write_byte_data(client, reg,  data);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}

	return ret;
}

static int s2mu004_read_reg(struct i2c_client *client, u8 reg, u8 *data)
{
	int ret = 0;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		pr_info("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}
	ret &= 0xff;
	*data = ret;

	return 0;
}

static int s2mu004_update_reg(struct i2c_client *client, u8 reg, u8 data, u8 mask)
{
	int ret;
	u8 old_val, new_val;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret >= 0) {
		old_val = ret & 0xff;
		new_val = (data & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(client, reg, new_val);
	}
	return ret;
}

static void s2mu004_haptic_i2c(struct s2mu004_haptic_data *hap_data, bool en)
{
	int ret = 0;
	u8 temp = 0;

	pr_debug("[VIB] %s %d\n", __func__, en);

	if (en) {
		if (AUTO_CAL_EN)
			s2mu004_write_reg(hap_data->i2c, S2MU004_REG_HAPTIC_MODE, 0x53);
		else
			s2mu004_update_reg(hap_data->i2c, S2MU004_REG_HAPTIC_MODE, MOTOR_EN, MOTOR_EN);
	} else {
		if (AUTO_CAL_EN)
			s2mu004_write_reg(hap_data->i2c, S2MU004_REG_HAPTIC_MODE, 0x02);
		else
			s2mu004_update_reg(hap_data->i2c, S2MU004_REG_HAPTIC_MODE, 0, MOTOR_EN);
	}

	ret = s2mu004_read_reg(hap_data->i2c, S2MU004_REG_HAPTIC_MODE, &temp);

	if (ret)
		pr_err("[VIB] i2c MOTOR_EN_PWM update error %d\n", ret);

	pr_info("[VIB]%s : haptic reg value : %x\n", __func__, temp);
}
#if defined(CONFIG_SS_VIBRATOR)
void s2mu004_vibtonz_en(bool en)
{
	if (s2mu004_g_hap_data == NULL)
		return;

	if (en) {
		if (s2mu004_g_hap_data->running)
			return;
		s2mu004_haptic_i2c(s2mu004_g_hap_data, true);

		s2mu004_g_hap_data->running = true;
	} else {
		if (!s2mu004_g_hap_data->running)
			return;
		s2mu004_haptic_i2c(s2mu004_g_hap_data, false);

		s2mu004_g_hap_data->running = false;
	}
}
EXPORT_SYMBOL(s2mu004_vibtonz_en);

void s2mu004_set_intensity(int intensity)
{
	u8 temp = 0;
	u8 value = 0;

	if (s2mu004_g_hap_data == NULL)
		return;
	value = ((intensity * S2MU004_MAX_INTENSITY) / MAX_INTENSITY);
	s2mu004_update_reg(s2mu004_g_hap_data->i2c, S2MU004_REG_AMPCOEF1, value, 0x7F);
	s2mu004_read_reg(s2mu004_g_hap_data->i2c, S2MU004_REG_AMPCOEF1, &temp);
	pr_info("[VIB] %s, intensity = %d, setting intensity = 0x%2x\n", __func__, intensity, value);
}
EXPORT_SYMBOL(s2mu004_set_intensity);
#endif

#if defined(CONFIG_OF)
static int s2mu004_haptic_parse_dt(struct device *dev, struct s2mu004_haptic_data *haptic)
{
	struct device_node *np = dev->of_node;

	pr_info("%s : start dt parsing\n", __func__);

	if (of_property_read_u32(np, "haptic,mode", &haptic->pdata->mode))
		haptic->pdata->mode = 1;
	if (of_property_read_u32(np, "haptic,divisor", &haptic->pdata->divisor))
		haptic->pdata->divisor = 128;
	pr_info("[VIB] %s: mode: %d\n", __func__, haptic->pdata->mode);
	pr_info("[VIB] %s: divisor: %d\n", __func__, haptic->pdata->divisor);

	return 0;
}
#endif

static int s2mu004_haptic_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int error = 0, ret = 0;
	struct s2mu004_haptic_data *haptic;
	u8 temp = 0;

	haptic = kzalloc(sizeof(struct s2mu004_haptic_data), GFP_KERNEL);
	pr_err("[VIB] %d probe  %s\n", __LINE__,  __func__);

	if (!haptic) {
		pr_err("[VIB] %s: no hap_pdata\n", __func__);
		//kfree(pdata);
		return -ENOMEM;
	}

	haptic->i2c = client;

	if (client->dev.of_node) {
		haptic->pdata = devm_kzalloc(&client->dev,
			sizeof(*(haptic->pdata)), GFP_KERNEL);
		if (!haptic->pdata) {
			kfree(haptic);
			return -ENOMEM;
		}

		ret = s2mu004_haptic_parse_dt(&client->dev, haptic);
		if (ret < 0)
			return -EFAULT;
	} else {
		haptic->pdata = client->dev.platform_data;
	}

	i2c_set_clientdata(client, haptic);
        s2mu004_g_hap_data = haptic;

	s2mu004_update_reg(haptic->i2c, 0x9B, 0x03, 0x03);

	if (AUTO_CAL_EN == 1) {
		s2mu004_write_reg(haptic->i2c, S2MU004_REG_IMPCONF1, 0x07);
		s2mu004_write_reg(haptic->i2c, S2MU004_REG_IMPCONF2, 0x40);
		s2mu004_write_reg(haptic->i2c, S2MU004_REG_IMPCONF3, 0x03);

		s2mu004_write_reg(haptic->i2c, S2MU004_REG_SINECOEF1, 0x7F);
		s2mu004_write_reg(haptic->i2c, S2MU004_REG_SINECOEF2, 0xCA);
		s2mu004_write_reg(haptic->i2c, S2MU004_REG_SINECOEF3, 0x55);
	} else {
		error = s2mu004_write_reg(haptic->i2c, S2MU004_REG_HAPTIC_MODE, 0x20);
		if (error < 0) {
			pr_err("[VIB] %s Failed to write reg to MODE.\n",
				__func__);
		} else {
			ret = s2mu004_read_reg(haptic->i2c, S2MU004_REG_HAPTIC_MODE, &temp);
			pr_err("[VIB] %s : S2MU004_REG_HAPTIC_MODE[0x%x] = 0x%x \n",__func__,
				S2MU004_REG_HAPTIC_MODE, temp);
		}
		s2mu004_write_reg(haptic->i2c, S2MU004_REG_SINECOEF1, 0x7B);
		s2mu004_write_reg(haptic->i2c, S2MU004_REG_SINECOEF2, 0xB3);
		s2mu004_write_reg(haptic->i2c, S2MU004_REG_SINECOEF3, 0x35);

		s2mu004_write_reg(haptic->i2c, S2MU004_REG_PERI_TAR1, 0x00);
		s2mu004_write_reg(haptic->i2c, S2MU004_REG_PERI_TAR2, 0x00);
		s2mu004_write_reg(haptic->i2c, S2MU004_REG_DUTY_TAR1, 0x00);
		s2mu004_write_reg(haptic->i2c, S2MU004_REG_DUTY_TAR2, 0x01);
	}

	return error;
}

static int s2mu004_haptic_remove(struct i2c_client *client)
{
	struct s2mu004_haptic_data *haptic = i2c_get_clientdata(client);

	s2mu004_haptic_i2c(haptic, false);
	kfree(haptic);
	return 0;
}

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
int s2mu004_haptic_reset(void)
{
	u8 temp = 0;

	s2mu004_read_reg(s2mu004_g_hap_data->i2c, 0x9F, &temp);
	temp |= 0x3;
	s2mu004_write_reg(s2mu004_g_hap_data->i2c, 0x9F, temp);

	return 0;
}
#endif

static int s2mu004_haptic_suspend(struct device *dev)
{

	return 0;
}
static int s2mu004_haptic_resume(struct device *dev)
{
	return 0;
}

void s2mu004_haptic_shutdown(struct device *dev)
{
	struct s2mu004_haptic_data *haptic = dev_get_drvdata(dev);

	pr_info("[VIB] %s: Disable HAPTIC\n", __func__);
	s2mu004_haptic_i2c(haptic, false);
	kfree(haptic);
}

static SIMPLE_DEV_PM_OPS(s2mu004_haptic_pm_ops, s2mu004_haptic_suspend, s2mu004_haptic_resume);
static const struct i2c_device_id s2mu004_haptic_id[] = {
	{"s2mu004-haptic", 0},
	{}
};
static struct i2c_driver s2mu004_haptic_driver = {
	.driver = {
		.name	= "s2mu004-haptic",
		.owner	= THIS_MODULE,
		.pm	= &s2mu004_haptic_pm_ops,
		.shutdown = s2mu004_haptic_shutdown,
	},
	.probe		= s2mu004_haptic_probe,
	.remove		= s2mu004_haptic_remove,
	.id_table	= s2mu004_haptic_id,
};

static int __init s2mu004_haptic_init(void)
{
	pr_info("[VIB] %s\n", __func__);
	return i2c_add_driver(&s2mu004_haptic_driver);
}
late_initcall(s2mu004_haptic_init);

static void __exit s2mu004_haptic_exit(void)
{
	i2c_del_driver(&s2mu004_haptic_driver);
}
module_exit(s2mu004_haptic_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("s2mu004 haptic driver");

