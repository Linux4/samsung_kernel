/*
 * s2mps28_core.c - mfd core driver for the s2mps28
 *
 * Copyright (C) 2023 Samsung Electronics
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
#include <linux/samsung/pmic/s2mps28.h>
#include <linux/samsung/pmic/s2mps28-regulator.h>
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

static struct mfd_cell s2mps28_1_devs[] = {
	{ .name = "s2mps28-1-regulator", },
	{ .name = "s2mps28-1-gpio", },
};

static struct mfd_cell s2mps28_2_devs[] = {
	{ .name = "s2mps28-2-regulator", },
	{ .name = "s2mps28-2-gpio", },
};

static struct mfd_cell s2mps28_3_devs[] = {
	{ .name = "s2mps28-3-regulator", },
	{ .name = "s2mps28-3-gpio", },
};

static struct mfd_cell s2mps28_4_devs[] = {
	{ .name = "s2mps28-4-regulator", },
	{ .name = "s2mps28-4-gpio", },
};

static struct mfd_cell s2mps28_5_devs[] = {
	{ .name = "s2mps28-5-regulator", },
	{ .name = "s2mps28-5-gpio", },
};

int s2mps28_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *dest)
{

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps28_dev *s2mps28 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps28->i2c_lock);
	ret = exynos_acpm_read_reg(acpm_mfd_node, s2mps28->sid, i2c->addr, reg, dest);
	mutex_unlock(&s2mps28->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mps28_read_reg);

int s2mps28_bulk_read(struct i2c_client *i2c, uint8_t reg, int count, uint8_t *buf)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps28_dev *s2mps28 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps28->i2c_lock);
	ret = exynos_acpm_bulk_read(acpm_mfd_node, s2mps28->sid, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mps28->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mps28_bulk_read);

int s2mps28_write_reg(struct i2c_client *i2c, uint8_t reg, uint8_t value)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps28_dev *s2mps28 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mps28->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, s2mps28->sid, i2c->addr, reg, value);
	mutex_unlock(&s2mps28->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mps28_write_reg);

int s2mps28_bulk_write(struct i2c_client *i2c, uint8_t reg, int count, uint8_t *buf)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps28_dev *s2mps28 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mps28->i2c_lock);
	ret = exynos_acpm_bulk_write(acpm_mfd_node, s2mps28->sid, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mps28->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mps28_bulk_write);

int s2mps28_update_reg(struct i2c_client *i2c, uint8_t reg, uint8_t val, uint8_t mask)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps28_dev *s2mps28 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mps28->i2c_lock);
	ret = exynos_acpm_update_reg(acpm_mfd_node, s2mps28->sid, i2c->addr, reg, val, mask);
	mutex_unlock(&s2mps28->i2c_lock);
	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mps28_update_reg);

#if IS_ENABLED(CONFIG_OF)
static int of_s2mps28_dt(struct device *dev,
			 struct s2mps28_platform_data *pdata,
			 struct s2mps28_dev *s2mps28)
{
	struct device_node *np = dev->of_node;
	int strlen;
	const char *status;

	if (!np)
		return -EINVAL;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	acpm_mfd_node = np;
#endif
	status = of_get_property(np, "s2mps28,wakeup", &strlen);
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
static int of_s2mps28_dt(struct device *dev,
			 struct s2mps28_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */
static void s2mps28_set_new_i2c_device(struct s2mps28_dev *s2mps28)
{
	s2mps28->vgpio = devm_i2c_new_dummy_device(s2mps28->dev, s2mps28->com->adapter, TEMP_ADDR);
	s2mps28->pm1 = devm_i2c_new_dummy_device(s2mps28->dev, s2mps28->com->adapter, PM1_ADDR);
	s2mps28->pm2 = devm_i2c_new_dummy_device(s2mps28->dev, s2mps28->com->adapter, PM2_ADDR);
	s2mps28->pm3 = devm_i2c_new_dummy_device(s2mps28->dev, s2mps28->com->adapter, PM3_ADDR);
	s2mps28->ext = devm_i2c_new_dummy_device(s2mps28->dev, s2mps28->com->adapter, EXT_ADDR);
	s2mps28->gpio = devm_i2c_new_dummy_device(s2mps28->dev, s2mps28->com->adapter, GPIO_ADDR);

	s2mps28->com->addr = COM_ADDR;	/* forced COMMON address */
	s2mps28->vgpio->addr = VGPIO_ADDR;

	i2c_set_clientdata(s2mps28->com, s2mps28);
	i2c_set_clientdata(s2mps28->vgpio, s2mps28);
	i2c_set_clientdata(s2mps28->pm1, s2mps28);
	i2c_set_clientdata(s2mps28->pm2, s2mps28);
	i2c_set_clientdata(s2mps28->pm3, s2mps28);
	i2c_set_clientdata(s2mps28->ext, s2mps28);
	i2c_set_clientdata(s2mps28->gpio, s2mps28);
}

static int s2mps28_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *dev_id)
{
	struct s2mps28_dev *s2mps28;
	struct s2mps28_platform_data *pdata = i2c->dev.platform_data;
	uint8_t reg_data;
	int ret = 0, dev_type = (enum s2mps28_types)of_device_get_match_data(&i2c->dev);

	pr_info("[SUB%d_PMIC] %s: start\n", dev_type + 1, __func__);

	s2mps28 = devm_kzalloc(&i2c->dev, sizeof(struct s2mps28_dev), GFP_KERNEL);
	if (!s2mps28)
		return -ENOMEM;

	pdata = devm_kzalloc(&i2c->dev, sizeof(struct s2mps28_platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = of_s2mps28_dt(&i2c->dev, pdata, s2mps28);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to get device of_node\n");
		return ret;
	}
	i2c->dev.platform_data = pdata;

	s2mps28->dev = &i2c->dev;
	s2mps28->com = i2c;
	s2mps28->device_type = dev_type;
	s2mps28->pdata = pdata;
	s2mps28->wakeup = pdata->wakeup;

	s2mps28_set_new_i2c_device(s2mps28);
	mutex_init(&s2mps28->i2c_lock);

	ret = s2mps28_read_reg(i2c, S2MPS28_COM_CHIP_ID, &reg_data);
	if (ret < 0) {
		dev_err(s2mps28->dev,
			"device not found on this channel (this is not an error)\n");
		ret = -ENODEV;
		goto err_w_lock;
	} else
		s2mps28->pmic_rev = reg_data; /* print rev */

	pr_info("%s: SUB%d device found: rev.0x%02hhx\n", __func__, dev_type + 1, s2mps28->pmic_rev);

	switch (s2mps28->device_type) {
	case TYPE_S2MPS28_1:
		s2mps28->sid = SUB1_ID;
		ret = devm_mfd_add_devices(s2mps28->dev, -1, s2mps28_1_devs,
				      ARRAY_SIZE(s2mps28_1_devs), NULL, 0, NULL);
		break;
	case TYPE_S2MPS28_2:
		s2mps28->sid = SUB2_ID;
		ret = devm_mfd_add_devices(s2mps28->dev, -1, s2mps28_2_devs,
				      ARRAY_SIZE(s2mps28_2_devs), NULL, 0, NULL);
		break;
	case TYPE_S2MPS28_3:
		s2mps28->sid = SUB3_ID;
		ret = devm_mfd_add_devices(s2mps28->dev, -1, s2mps28_3_devs,
				      ARRAY_SIZE(s2mps28_3_devs), NULL, 0, NULL);
		break;
	case TYPE_S2MPS28_4:
		s2mps28->sid = SUB4_ID;
		ret = devm_mfd_add_devices(s2mps28->dev, -1, s2mps28_4_devs,
				      ARRAY_SIZE(s2mps28_4_devs), NULL, 0, NULL);
		break;
	case TYPE_S2MPS28_5:
		s2mps28->sid = SUB5_ID;
		ret = devm_mfd_add_devices(s2mps28->dev, -1, s2mps28_5_devs,
				      ARRAY_SIZE(s2mps28_5_devs), NULL, 0, NULL);
		break;
	default:
		pr_err("%s: device_type(%d) error\n", __func__, s2mps28->device_type);
		return -ENOMEM;
	}

	if (ret < 0)
		goto err_w_lock;

	/* init slave PMIC notifier */
	ret = s2mps28_notifier_init(s2mps28);
	if (ret < 0)
		pr_err("%s: s2mps28_notifier_init fail\n", __func__);

	ret = device_init_wakeup(s2mps28->dev, pdata->wakeup);
	if (ret < 0) {
		pr_err("%s: device_init_wakeup fail(%d)\n", __func__, ret);
		goto err_w_lock;
	}

	pr_info("[SUB%d_PMIC] %s: end\n", dev_type + 1, __func__);

	return ret;

err_w_lock:
	mutex_destroy(&s2mps28->i2c_lock);

	return ret;
}

static void s2mps28_i2c_remove(struct i2c_client *i2c)
{
	struct s2mps28_dev *s2mps28 = i2c_get_clientdata(i2c);

	if(s2mps28->pdata->wakeup)
		device_init_wakeup(s2mps28->dev, false);

	s2mps28_notifier_deinit(s2mps28);
	mutex_destroy(&s2mps28->i2c_lock);

}

static const struct i2c_device_id s2mps28_i2c_id[] = {
	{ "s2mps28-1", TYPE_S2MPS28_1 },
	{ "s2mps28-2", TYPE_S2MPS28_2 },
	{ "s2mps28-3", TYPE_S2MPS28_3 },
	{ "s2mps28-4", TYPE_S2MPS28_4 },
	{ "s2mps28-5", TYPE_S2MPS28_5 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mps28_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2mps28_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mps28_mfd", .data = (void *)TYPE_S2MPS28_1  },
	{ .compatible = "samsung,s2mps28_2_mfd", .data = (void *)TYPE_S2MPS28_2 },
	{ .compatible = "samsung,s2mps28_3_mfd", .data = (void *)TYPE_S2MPS28_3 },
	{ .compatible = "samsung,s2mps28_4_mfd", .data = (void *)TYPE_S2MPS28_4 },
	{ .compatible = "samsung,s2mps28_5_mfd", .data = (void *)TYPE_S2MPS28_5 },
	{ },
};
MODULE_DEVICE_TABLE(of, s2mps28_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver s2mps28_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mps28_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe		= s2mps28_i2c_probe,
	.remove		= s2mps28_i2c_remove,
	.id_table	= s2mps28_i2c_id,
};

static int __init s2mps28_i2c_init(void)
{
	pr_info("[PMIC] %s: %s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2mps28_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2mps28_i2c_init);

static void __exit s2mps28_i2c_exit(void)
{
	i2c_del_driver(&s2mps28_i2c_driver);
}
module_exit(s2mps28_i2c_exit);

MODULE_DESCRIPTION("s2mps28_1~5 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
