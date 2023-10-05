/*
 * s2mpu16.c - mfd core driver for the s2mpu16
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
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/samsung/pmic/s2mpu16.h>
#include <linux/samsung/pmic/s2mpu16-regulator-8835.h>
#include <linux/regulator/machine.h>
#include <linux/rtc.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#endif
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
static struct device_node *acpm_mfd_node;
#endif
static struct mfd_cell s2mpu16_devs[] = {
	{ .name = "s2mpu16-regulator", },
	{ .name = "s2mpu16-gpio", },
};

int s2mpu16_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *dest)
{

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpu16_dev *s2mpu16 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpu16->i2c_lock);
	ret = exynos_acpm_read_reg(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, dest);
	mutex_unlock(&s2mpu16->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu16_read_reg);

int s2mpu16_bulk_read(struct i2c_client *i2c, uint8_t reg, int count, uint8_t *buf)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpu16_dev *s2mpu16 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpu16->i2c_lock);
	ret = exynos_acpm_bulk_read(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mpu16->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu16_bulk_read);

int s2mpu16_write_reg(struct i2c_client *i2c, uint8_t reg, uint8_t value)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	int ret = 0;
	struct s2mpu16_dev *s2mpu16 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mpu16->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, value);
	mutex_unlock(&s2mpu16->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu16_write_reg);

int s2mpu16_bulk_write(struct i2c_client *i2c, uint8_t reg, int count, uint8_t *buf)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpu16_dev *s2mpu16 = i2c_get_clientdata(i2c);
	int ret = 0;

	mutex_lock(&s2mpu16->i2c_lock);
	ret = exynos_acpm_bulk_write(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mpu16->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu16_bulk_write);

int s2mpu16_update_reg(struct i2c_client *i2c, uint8_t reg, uint8_t val, uint8_t mask)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpu16_dev *s2mpu16 = i2c_get_clientdata(i2c);
	int ret = 0;

	mutex_lock(&s2mpu16->i2c_lock);
	ret = exynos_acpm_update_reg(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, val, mask);
	mutex_unlock(&s2mpu16->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpu16_update_reg);

#if IS_ENABLED(CONFIG_OF)
static int of_s2mpu16_dt(struct device *dev,
			 struct s2mpu16_platform_data *pdata,
			 struct s2mpu16_dev *s2mpu16)
{
	struct device_node *np = dev->of_node;
	int strlen;
	const char *status;

	if (!np)
		return -EINVAL;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	acpm_mfd_node = np;
#endif
	status = of_get_property(np, "s2mpu16,wakeup", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wakeup = true;
		else
			pdata->wakeup = false;
	}

	return 0;
}
#else
static int of_s2mpu16_dt(struct device *dev,
			 struct s2mpu16_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static void s2mpu16_set_new_i2c_device(struct s2mpu16_dev *s2mpu16)
{
	s2mpu16->vgpio = devm_i2c_new_dummy_device(s2mpu16->dev, s2mpu16->i2c->adapter, TEMP_ADDR);
	s2mpu16->pmic1 = devm_i2c_new_dummy_device(s2mpu16->dev, s2mpu16->i2c->adapter, PMIC1_ADDR);
	s2mpu16->pmic2 = devm_i2c_new_dummy_device(s2mpu16->dev, s2mpu16->i2c->adapter, PMIC2_ADDR);
	s2mpu16->close1 = devm_i2c_new_dummy_device(s2mpu16->dev, s2mpu16->i2c->adapter, CLOSE1_ADDR);
	s2mpu16->close2 = devm_i2c_new_dummy_device(s2mpu16->dev, s2mpu16->i2c->adapter, CLOSE2_ADDR);
	s2mpu16->gpio = devm_i2c_new_dummy_device(s2mpu16->dev, s2mpu16->i2c->adapter, GPIO_ADDR);

	s2mpu16->i2c->addr = COMMON_ADDR;	/* forced COMMON address */
	s2mpu16->vgpio->addr = VGPIO_ADDR;

	i2c_set_clientdata(s2mpu16->i2c, s2mpu16);
	i2c_set_clientdata(s2mpu16->vgpio, s2mpu16);
	i2c_set_clientdata(s2mpu16->pmic1, s2mpu16);
	i2c_set_clientdata(s2mpu16->pmic2, s2mpu16);
	i2c_set_clientdata(s2mpu16->close1, s2mpu16);
	i2c_set_clientdata(s2mpu16->close2, s2mpu16);
	i2c_set_clientdata(s2mpu16->gpio, s2mpu16);
}

static int s2mpu16_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *dev_id)
{
	struct s2mpu16_dev *s2mpu16;
	struct s2mpu16_platform_data *pdata = i2c->dev.platform_data;
	uint8_t reg_data;
	int ret = 0;

	dev_info(&i2c->dev, "[PMIC] %s: start\n", __func__);

	s2mpu16 = devm_kzalloc(&i2c->dev, sizeof(struct s2mpu16_dev), GFP_KERNEL);
	if (!s2mpu16)
		return -ENOMEM;

	pdata = devm_kzalloc(&i2c->dev, sizeof(struct s2mpu16_platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = of_s2mpu16_dt(&i2c->dev, pdata, s2mpu16);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to get device of_node\n");
		return ret;
	}
	i2c->dev.platform_data = pdata;

	s2mpu16->dev = &i2c->dev;
	s2mpu16->i2c = i2c;
	s2mpu16->device_type = s2mpu16X;
	s2mpu16->pdata = pdata;
	s2mpu16->wakeup = pdata->wakeup;

	s2mpu16_set_new_i2c_device(s2mpu16);
	mutex_init(&s2mpu16->i2c_lock);

	ret = s2mpu16_read_reg(i2c, S2MPU16_COMMON_CHIP_ID, &reg_data);
	if (ret < 0) {
		dev_err(s2mpu16->dev,
			"device not found on this channel (this is not an error)\n");
		goto err_w_lock;
	}
	s2mpu16->pmic_rev = reg_data; /* print rev */

	dev_info(&i2c->dev, "%s: device found: rev.0x%02hhx\n", __func__, s2mpu16->pmic_rev);

	ret = s2mpu16_notifier_init(s2mpu16);
	if (ret < 0)
		pr_err("%s: s2mpu16_notifier_init fail\n", __func__);

	ret = devm_mfd_add_devices(s2mpu16->dev, -1, s2mpu16_devs, ARRAY_SIZE(s2mpu16_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_w_lock;

	ret = device_init_wakeup(s2mpu16->dev, pdata->wakeup);
	if (ret < 0) {
		pr_err("%s: device_init_wakeup fail(%d)\n", __func__, ret);
		goto err_w_lock;
	}

	dev_info(&i2c->dev, "[PMIC] %s: end\n", __func__);

	return ret;

err_w_lock:
	mutex_destroy(&s2mpu16->i2c_lock);
	return ret;
}

static int s2mpu16_i2c_remove(struct i2c_client *i2c)
{
	struct s2mpu16_dev *s2mpu16 = i2c_get_clientdata(i2c);

	if(s2mpu16->pdata->wakeup)
		device_init_wakeup(s2mpu16->dev, false);

	s2mpu16_notifier_deinit(s2mpu16);
	mutex_destroy(&s2mpu16->i2c_lock);

	return 0;
}

static const struct i2c_device_id s2mpu16_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_S2MPU16 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mpu16_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2mpu16_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mpu16mfd" },
	{ },
};
#endif /* CONFIG_OF */

static struct i2c_driver s2mpu16_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mpu16_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe		= s2mpu16_i2c_probe,
	.remove		= s2mpu16_i2c_remove,
	.id_table	= s2mpu16_i2c_id,
};

static int __init s2mpu16_i2c_init(void)
{
	pr_info("[PMIC] %s: %s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2mpu16_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2mpu16_i2c_init);

static void __exit s2mpu16_i2c_exit(void)
{
	i2c_del_driver(&s2mpu16_i2c_driver);
}
module_exit(s2mpu16_i2c_exit);

MODULE_DESCRIPTION("s2mpu16 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
