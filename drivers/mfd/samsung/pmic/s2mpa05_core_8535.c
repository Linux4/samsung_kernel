/*
 * s2mpa05.c - mfd core driver for the s2mpa05
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
#include <linux/samsung/pmic/s2mpa05.h>
#include <linux/samsung/pmic/s2mpa05-regulator-8535.h>
#include <linux/regulator/machine.h>
#include <linux/rtc.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#define SUB_CHANNEL	1 // TODO: check channel
#endif
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_VGPIO		0x00
#define I2C_ADDR_COM		0x03
#define I2C_ADDR_PMIC		0x05
#define I2C_ADDR_CLOSE		0x0E
#define I2C_ADDR_GPIO		0x0B
#define I2C_ADDR_TEMP		0x0C

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
static struct device_node *acpm_mfd_node;
#endif
static struct mfd_cell s2mpa05_devs[] = {
	{ .name = "s2mpa05-regulator", },
	{ .name = "s2mpa05-gpio", },
};

int s2mpa05_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *dest)
{

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpa05_dev *s2mpa05 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpa05->i2c_lock);
	ret = exynos_acpm_read_reg(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, dest);
	mutex_unlock(&s2mpa05->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpa05_read_reg);

int s2mpa05_bulk_read(struct i2c_client *i2c, uint8_t reg, int count, uint8_t *buf)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpa05_dev *s2mpa05 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpa05->i2c_lock);
	ret = exynos_acpm_bulk_read(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mpa05->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpa05_bulk_read);

int s2mpa05_write_reg(struct i2c_client *i2c, uint8_t reg, uint8_t value)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	int ret = 0;
	struct s2mpa05_dev *s2mpa05 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mpa05->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, value);
	mutex_unlock(&s2mpa05->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpa05_write_reg);

int s2mpa05_bulk_write(struct i2c_client *i2c, uint8_t reg, int count, uint8_t *buf)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpa05_dev *s2mpa05 = i2c_get_clientdata(i2c);
	int ret = 0;

	mutex_lock(&s2mpa05->i2c_lock);
	ret = exynos_acpm_bulk_write(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mpa05->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpa05_bulk_write);

int s2mpa05_update_reg(struct i2c_client *i2c, uint8_t reg, uint8_t val, uint8_t mask)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mpa05_dev *s2mpa05 = i2c_get_clientdata(i2c);
	int ret = 0;

	mutex_lock(&s2mpa05->i2c_lock);
	ret = exynos_acpm_update_reg(acpm_mfd_node, SUB_CHANNEL, i2c->addr, reg, val, mask);
	mutex_unlock(&s2mpa05->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpa05_update_reg);

#if IS_ENABLED(CONFIG_OF)
static int of_s2mpa05_dt(struct device *dev,
			 struct s2mpa05_platform_data *pdata,
			 struct s2mpa05_dev *s2mpa05)
{
	struct device_node *np = dev->of_node;
	int strlen;
	const char *status;

	if (!np)
		return -EINVAL;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	acpm_mfd_node = np;
#endif
	status = of_get_property(np, "s2mpa05,wakeup", &strlen);
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
static int of_s2mpa05_dt(struct device *dev,
			 struct s2mpa05_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static void s2mpa05_set_new_i2c_device(struct s2mpa05_dev *s2mpa05)
{
	s2mpa05->vgpio = devm_i2c_new_dummy_device(s2mpa05->dev, s2mpa05->i2c->adapter, I2C_ADDR_TEMP);
	s2mpa05->pmic = devm_i2c_new_dummy_device(s2mpa05->dev, s2mpa05->i2c->adapter, I2C_ADDR_PMIC);
	s2mpa05->close = devm_i2c_new_dummy_device(s2mpa05->dev, s2mpa05->i2c->adapter, I2C_ADDR_CLOSE);
	s2mpa05->gpio = devm_i2c_new_dummy_device(s2mpa05->dev, s2mpa05->i2c->adapter, I2C_ADDR_GPIO);

	s2mpa05->i2c->addr = I2C_ADDR_COM;	/* forced COMMON address */
	s2mpa05->vgpio->addr = I2C_ADDR_VGPIO;

	i2c_set_clientdata(s2mpa05->i2c, s2mpa05);
	i2c_set_clientdata(s2mpa05->vgpio, s2mpa05);
	i2c_set_clientdata(s2mpa05->pmic, s2mpa05);
	i2c_set_clientdata(s2mpa05->close, s2mpa05);
	i2c_set_clientdata(s2mpa05->gpio, s2mpa05);
}

static int s2mpa05_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *dev_id)
{
	struct s2mpa05_dev *s2mpa05;
	struct s2mpa05_platform_data *pdata = i2c->dev.platform_data;
	uint8_t reg_data;
	int ret = 0;

	dev_info(&i2c->dev, "[PMIC] %s: start\n", __func__);

	s2mpa05 = devm_kzalloc(&i2c->dev, sizeof(struct s2mpa05_dev), GFP_KERNEL);
	if (!s2mpa05)
		return -ENOMEM;

	pdata = devm_kzalloc(&i2c->dev, sizeof(struct s2mpa05_platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = of_s2mpa05_dt(&i2c->dev, pdata, s2mpa05);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to get device of_node\n");
		return ret;
	}
	i2c->dev.platform_data = pdata;

	s2mpa05->dev = &i2c->dev;
	s2mpa05->i2c = i2c;
	s2mpa05->device_type = s2mpa05X;
	s2mpa05->pdata = pdata;
	s2mpa05->wakeup = pdata->wakeup;

	s2mpa05_set_new_i2c_device(s2mpa05);
	mutex_init(&s2mpa05->i2c_lock);

	ret = s2mpa05_read_reg(i2c, S2MPA05_COMMON_CHIP_ID, &reg_data);
	if (ret < 0) {
		dev_err(s2mpa05->dev,
			"device not found on this channel (this is not an error)\n");
		goto err_w_lock;
	}
	s2mpa05->pmic_rev = reg_data; /* print rev */

	dev_info(&i2c->dev, "%s: device found: rev.0x%02hhx\n", __func__, s2mpa05->pmic_rev);

	ret = s2mpa05_notifier_init(s2mpa05);
	if (ret < 0)
		pr_err("%s: s2mpa05_notifier_init fail\n", __func__);

	ret = devm_mfd_add_devices(s2mpa05->dev, -1, s2mpa05_devs, ARRAY_SIZE(s2mpa05_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_w_lock;

	ret = device_init_wakeup(s2mpa05->dev, pdata->wakeup);
	if (ret < 0) {
		pr_err("%s: device_init_wakeup fail(%d)\n", __func__, ret);
		goto err_w_lock;
	}

	dev_info(&i2c->dev, "[PMIC] %s: end\n", __func__);

	return ret;

err_w_lock:
	mutex_destroy(&s2mpa05->i2c_lock);
	return ret;
}

static int s2mpa05_i2c_remove(struct i2c_client *i2c)
{
	struct s2mpa05_dev *s2mpa05 = i2c_get_clientdata(i2c);

	if(s2mpa05->pdata->wakeup)
		device_init_wakeup(s2mpa05->dev, false);

	s2mpa05_notifier_deinit(s2mpa05);
	mutex_destroy(&s2mpa05->i2c_lock);

	return 0;
}

static const struct i2c_device_id s2mpa05_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_s2mpa05 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mpa05_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2mpa05_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mpa05mfd" },
	{ },
};
#endif /* CONFIG_OF */

static struct i2c_driver s2mpa05_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mpa05_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe		= s2mpa05_i2c_probe,
	.remove		= s2mpa05_i2c_remove,
	.id_table	= s2mpa05_i2c_id,
};

static int __init s2mpa05_i2c_init(void)
{
	pr_info("[PMIC] %s: %s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2mpa05_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2mpa05_i2c_init);

static void __exit s2mpa05_i2c_exit(void)
{
	i2c_del_driver(&s2mpa05_i2c_driver);
}
module_exit(s2mpa05_i2c_exit);

MODULE_DESCRIPTION("s2mpa05 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
