/*
 * s2mps27_core.c - mfd core driver for the s2mps27
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
#include <linux/samsung/pmic/s2mps27.h>
#include <linux/samsung/pmic/s2mps27-regulator.h>
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
static struct mfd_cell s2mps27_devs[] = {
	{ .name = "s2mps27-regulator", },
	{ .name = "s2mps27-rtc", },
	{ .name = "s2mps27-power-keys", },
	{ .name = "s2mps27-gpadc", },
	{ .name = "s2mps27-gpio", },
};

int s2mps27_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *dest)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps27_dev *s2mps27 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps27->i2c_lock);
	ret = exynos_acpm_read_reg(acpm_mfd_node, MAIN_ID, i2c->addr, reg, dest);
	mutex_unlock(&s2mps27->i2c_lock);

	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mps27_read_reg);

int s2mps27_bulk_read(struct i2c_client *i2c, uint8_t reg, int count, uint8_t *buf)
{
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps27_dev *s2mps27 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mps27->i2c_lock);
	ret = exynos_acpm_bulk_read(acpm_mfd_node, MAIN_ID, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mps27->i2c_lock);

	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(s2mps27_bulk_read);

int s2mps27_write_reg(struct i2c_client *i2c, uint8_t reg, uint8_t value)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps27_dev *s2mps27 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mps27->i2c_lock);
	ret = exynos_acpm_write_reg(acpm_mfd_node, MAIN_ID, i2c->addr, reg, value);
	mutex_unlock(&s2mps27->i2c_lock);

	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mps27_write_reg);

int s2mps27_bulk_write(struct i2c_client *i2c, uint8_t reg, int count, uint8_t *buf)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps27_dev *s2mps27 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mps27->i2c_lock);
	ret = exynos_acpm_bulk_write(acpm_mfd_node, MAIN_ID, i2c->addr, reg, count, buf);
	mutex_unlock(&s2mps27->i2c_lock);

	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mps27_bulk_write);

int s2mps27_update_reg(struct i2c_client *i2c, uint8_t reg, uint8_t val, uint8_t mask)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2mps27_dev *s2mps27 = i2c_get_clientdata(i2c);

	mutex_lock(&s2mps27->i2c_lock);
	ret = exynos_acpm_update_reg(acpm_mfd_node, MAIN_ID, i2c->addr, reg, val, mask);
	mutex_unlock(&s2mps27->i2c_lock);

	if (ret) {
		pr_err("[%s] acpm ipc fail!\n", __func__);
		return ret;
	}
#endif
	return ret;
}
EXPORT_SYMBOL_GPL(s2mps27_update_reg);

#if IS_ENABLED(CONFIG_OF)
static int of_s2mps27_dt(struct device *dev,
			 struct s2mps27_platform_data *pdata,
			 struct s2mps27_dev *s2mps27)
{
	struct device_node *np = dev->of_node;
	int ret, strlen;
	const char *status;
	uint32_t val;

	if (!np)
		return -EINVAL;

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	acpm_mfd_node = np;
#endif

	status = of_get_property(np, "s2mps27,wakeup", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wakeup = true;
		else
			pdata->wakeup = false;
	}

	/* WTSR, SMPL */
	pdata->wtsr_smpl = devm_kzalloc(dev, sizeof(*pdata->wtsr_smpl),
			GFP_KERNEL);
	if (!pdata->wtsr_smpl)
		return -ENOMEM;

	status = of_get_property(np, "wtsr_en", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wtsr_smpl->wtsr_en = true;
		else
			pdata->wtsr_smpl->wtsr_en = false;
	}

	status = of_get_property(np, "smpl_en", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wtsr_smpl->smpl_en = true;
		else
			pdata->wtsr_smpl->smpl_en = false;
	}

	ret = of_property_read_u32(np, "wtsr_timer_val",
			&pdata->wtsr_smpl->wtsr_timer_val);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "smpl_timer_val",
			&pdata->wtsr_smpl->smpl_timer_val);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "check_jigon", &val);
	if (ret)
		return -EINVAL;
	pdata->wtsr_smpl->check_jigon = !!val;

	ret = of_property_read_u32(np, "inst_acok_en", &val);
	if (ret)
		return -EINVAL;
	pdata->inst_acok_en = !!val;

	ret = of_property_read_u32(np, "jig_reboot_en", &val);
	if (ret)
		return -EINVAL;
	pdata->jig_reboot_en = !!val;

	/* init time */
	pdata->init_time = devm_kzalloc(dev, sizeof(*pdata->init_time), GFP_KERNEL);
	if (!pdata->init_time)
		return -ENOMEM;

	ret = of_property_read_u32(np, "init_time,sec",
			&pdata->init_time->tm_sec);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,min",
			&pdata->init_time->tm_min);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,hour",
			&pdata->init_time->tm_hour);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,mday",
			&pdata->init_time->tm_mday);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,mon",
			&pdata->init_time->tm_mon);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,year",
			&pdata->init_time->tm_year);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,wday",
			&pdata->init_time->tm_wday);
	if (ret)
		return -EINVAL;

	/* rtc optimize */
	ret = of_property_read_u32(np, "osc-bias-up", &val);
	if (!ret)
		pdata->osc_bias_up = val;
	else
		pdata->osc_bias_up = -1;

	ret = of_property_read_u32(np, "rtc_cap_sel", &val);
	if (!ret)
		pdata->cap_sel = val;
	else
		pdata->cap_sel = -1;

	ret = of_property_read_u32(np, "rtc_osc_xin", &val);
	if (!ret)
		pdata->osc_xin = val;
	else
		pdata->osc_xin = -1;

	ret = of_property_read_u32(np, "rtc_osc_xout", &val);
	if (!ret)
		pdata->osc_xout = val;
	else
		pdata->osc_xout = -1;

	return 0;
}
#else
static int of_s2mps27_dt(struct device *dev,
			 struct s2mps27_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static void s2mps27_set_new_i2c_device(struct s2mps27_dev *s2mps27)
{
	struct i2c_client *com = s2mps27->com;

	s2mps27->vgpio = devm_i2c_new_dummy_device(s2mps27->dev, com->adapter, TEMP_ADDR);
	s2mps27->pm1 = devm_i2c_new_dummy_device(s2mps27->dev, com->adapter, PM1_ADDR);
	s2mps27->pm2 = devm_i2c_new_dummy_device(s2mps27->dev, com->adapter, PM2_ADDR);
	s2mps27->pm3 = devm_i2c_new_dummy_device(s2mps27->dev, com->adapter, PM3_ADDR);
	s2mps27->rtc = devm_i2c_new_dummy_device(s2mps27->dev, com->adapter, RTC_ADDR);
	s2mps27->adc = devm_i2c_new_dummy_device(s2mps27->dev, com->adapter, ADC_ADDR);
	s2mps27->gpio = devm_i2c_new_dummy_device(s2mps27->dev, com->adapter, GPIO_ADDR);
	s2mps27->ext = devm_i2c_new_dummy_device(s2mps27->dev, com->adapter, EXT_ADDR);

	com->addr = COM_ADDR;	/* forced COMMON address */
	s2mps27->vgpio->addr = VGPIO_ADDR;

	i2c_set_clientdata(s2mps27->com, s2mps27);
	i2c_set_clientdata(s2mps27->vgpio, s2mps27);
	i2c_set_clientdata(s2mps27->pm1, s2mps27);
	i2c_set_clientdata(s2mps27->pm2, s2mps27);
	i2c_set_clientdata(s2mps27->pm3, s2mps27);
	i2c_set_clientdata(s2mps27->rtc, s2mps27);
	i2c_set_clientdata(s2mps27->adc, s2mps27);
	i2c_set_clientdata(s2mps27->gpio, s2mps27);
	i2c_set_clientdata(s2mps27->ext, s2mps27);
}

static int s2mps27_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *dev_id)
{
	struct s2mps27_dev *s2mps27;
	struct s2mps27_platform_data *pdata = i2c->dev.platform_data;
	uint8_t reg_data;
	int ret = 0;

	dev_info(&i2c->dev, "[PMIC] %s: start\n", __func__);

	s2mps27 = devm_kzalloc(&i2c->dev, sizeof(struct s2mps27_dev), GFP_KERNEL);
	if (!s2mps27)
		return -ENOMEM;

	pdata = devm_kzalloc(&i2c->dev, sizeof(struct s2mps27_platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = of_s2mps27_dt(&i2c->dev, pdata, s2mps27);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to get device of_node\n");
		return ret;
	}

	i2c->dev.platform_data = pdata;

	pdata->irq_base = devm_irq_alloc_descs(&i2c->dev, -1, 0, S2MPS27_IRQ_NR, -1);
	if (pdata->irq_base < 0){
		dev_err(&i2c->dev, "%s: devm_irq_alloc_descs Fail! (%d)\n", __func__, pdata->irq_base);
		return -EINVAL;
	}

	s2mps27->dev = &i2c->dev;
	s2mps27->com = i2c;
	s2mps27->device_type = S2MPS27X;
	s2mps27->irq = i2c->irq;
	s2mps27->pdata = pdata;
	s2mps27->wakeup = pdata->wakeup;
	s2mps27->inst_acok_en = pdata->inst_acok_en;
	s2mps27->jig_reboot_en = pdata->jig_reboot_en;
	s2mps27->irq_base = pdata->irq_base;

	s2mps27_set_new_i2c_device(s2mps27);
	mutex_init(&s2mps27->i2c_lock);

	ret = s2mps27_read_reg(i2c, S2MPS27_COM_CHIP_ID, &reg_data);
	if (ret < 0) {
		dev_err(s2mps27->dev,
			"device not found on this channel (this is not an error)\n");
		goto err_w_lock;
	}
	s2mps27->pmic_rev = reg_data; /* print rev */

	dev_info(&i2c->dev, "%s: device found: rev.0x%02hhx\n", __func__, s2mps27->pmic_rev);

	ret = s2mps27_irq_init(s2mps27);
	if (ret < 0)
		goto err_w_lock;

	ret = devm_mfd_add_devices(s2mps27->dev, -1, s2mps27_devs, ARRAY_SIZE(s2mps27_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_w_lock;

	ret = device_init_wakeup(s2mps27->dev, pdata->wakeup);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: device_init_wakeup fail(%d)\n", __func__, ret);
		goto err_w_lock;
	}

	dev_info(&i2c->dev, "[PMIC] %s: end\n", __func__);

	return ret;

err_w_lock:
	mutex_destroy(&s2mps27->i2c_lock);
	return ret;
}

static void s2mps27_i2c_remove(struct i2c_client *i2c)
{
	struct s2mps27_dev *s2mps27 = i2c_get_clientdata(i2c);

	if (s2mps27->pdata->wakeup)
		device_init_wakeup(s2mps27->dev, false);

	s2mps27_irq_exit(s2mps27);
	mutex_destroy(&s2mps27->i2c_lock);

}

static const struct i2c_device_id s2mps27_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_S2MPS27 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mps27_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2mps27_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mps27_mfd" },
	{ },
};
MODULE_DEVICE_TABLE(of, s2mps27_i2c_dt_ids);
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_PM)
static int s2mps27_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct s2mps27_dev *s2mps27 = i2c_get_clientdata(i2c);

	dev->power.must_resume = true;

	if (device_may_wakeup(dev))
		enable_irq_wake(s2mps27->irq);

	disable_irq(s2mps27->irq);

	return 0;
}

static int s2mps27_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct s2mps27_dev *s2mps27 = i2c_get_clientdata(i2c);

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	pr_info("%s: %s\n", MFD_DEV_NAME, __func__);
#endif /* CONFIG_SAMSUNG_PRODUCT_SHIP */

	if (device_may_wakeup(dev))
		disable_irq_wake(s2mps27->irq);

	enable_irq(s2mps27->irq);

	return 0;
}
#else
#define s2mps27_suspend	NULL
#define s2mps27_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops s2mps27_pm = {
	.suspend_late = s2mps27_suspend,
	.resume_early = s2mps27_resume,
};

static struct i2c_driver s2mps27_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm	= &s2mps27_pm,
#endif /* CONFIG_PM */
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mps27_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe		= s2mps27_i2c_probe,
	.remove		= s2mps27_i2c_remove,
	.id_table	= s2mps27_i2c_id,
};

static int __init s2mps27_i2c_init(void)
{
	pr_info("[PMIC] %s: %s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2mps27_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2mps27_i2c_init);

static void __exit s2mps27_i2c_exit(void)
{
	i2c_del_driver(&s2mps27_i2c_driver);
}
module_exit(s2mps27_i2c_exit);

MODULE_DESCRIPTION("s2mps27 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
