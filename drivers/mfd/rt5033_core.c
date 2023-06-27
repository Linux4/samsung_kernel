/* drivers/mfd/rt5033_core.c
 * RT5033 Multifunction Device Driver
 * Charger / Buck / LDOs / FlashLED
 *
 * Copyright (C) 2013 Richtek Technology Corp.
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/mfd/core.h>
#include <linux/mfd/rt5033.h>
#include <linux/mfd/rt5033_irq.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/pm.h>

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#ifdef CONFIG_FLED_RT5033
#include <linux/leds/rt5033_fled.h>
#define ALIAS_NAME "rt5033-fled"
#endif

#define RT5033_DECLARE_IRQ(irq) { \
	irq, irq, \
	irq##_NAME, IORESOURCE_IRQ }


#ifdef CONFIG_CHARGER_RT5033
const static struct resource rt5033_charger_res[] = {
	RT5033_DECLARE_IRQ(RT5033_PPBATLV_IRQ),
	RT5033_DECLARE_IRQ(RT5033_IEOC_IRQ),
	RT5033_DECLARE_IRQ(RT5033_VINOVPI_IRQ),
	RT5033_DECLARE_IRQ(RT5033_TSDI_IRQ),
	RT5033_DECLARE_IRQ(RT5033_CHMIVRI_IRQ),
	RT5033_DECLARE_IRQ(RT5033_CHTREGI_IRQ),
	RT5033_DECLARE_IRQ(RT5033_CHTMRFI_IRQ),
	RT5033_DECLARE_IRQ(RT5033_CHRCHGI_IRQ),
	RT5033_DECLARE_IRQ(RT5033_CHTERMI_IRQ),
	RT5033_DECLARE_IRQ(RT5033_CHBATOVI_IRQ),
	RT5033_DECLARE_IRQ(RT5033_CHRVPI_IRQ),
	RT5033_DECLARE_IRQ(RT5033_BSTLOWVI_IRQ),
	RT5033_DECLARE_IRQ(RT5033_BSTOLI_IRQ),
	RT5033_DECLARE_IRQ(RT5033_BSTVMIDOVP_IRQ),
};

static struct mfd_cell rt5033_charger_devs[] = {
	{
		.name		= "rt5033-charger",
		.num_resources	= ARRAY_SIZE(rt5033_charger_res),
		.id		= -1,
		.resources = rt5033_charger_res,
	},
};
#endif /*CONFIG_CHARGER_RT5033*/

#ifdef CONFIG_FLED_RT5033
const static struct resource rt5033_fled_res[] = {
	RT5033_DECLARE_IRQ(RT5033_VF_L_IRQ),
	RT5033_DECLARE_IRQ(RT5033_LEDCS2_SHORT_IRQ),
	RT5033_DECLARE_IRQ(RT5033_LEDCS1_SHORT_IRQ),
};
static struct mfd_cell rt5033_fled_devs[] = {
	{
		.name		= "rt5033-fled",
		.num_resources	= ARRAY_SIZE(rt5033_fled_res),
		.id		= -1,
		.resources = rt5033_fled_res,
	},
};
#endif /*CONFIG_FLED_RT5033*/

#ifdef CONFIG_REGULATOR_RT5033
const static struct resource rt5033_regulator_res_LDO_SAFE[] = {
	RT5033_DECLARE_IRQ(RT5033_SAFE_LDO_LV_IRQ),
};

const static struct resource rt5033_regulator_res_LDO1[] = {
	RT5033_DECLARE_IRQ(RT5033_LDO_LV_IRQ),
};

const static struct resource rt5033_regulator_res_DCDC1[] = {
	RT5033_DECLARE_IRQ(RT5033_BUCK_OCP_IRQ),
	RT5033_DECLARE_IRQ(RT5033_BUCK_LV_IRQ),
	RT5033_DECLARE_IRQ(RT5033_OT_IRQ),
	RT5033_DECLARE_IRQ(RT5033_VDDA_UV_IRQ),
};

#define RT5033_VR_DEVS(_id)             \
{                                       \
	.name		= "rt5033-regulator",	\
	.num_resources = ARRAY_SIZE(rt5033_regulator_res_##_id), \
	.id		= RT5033_ID_##_id,          \
	.resources = rt5033_regulator_res_##_id, \
}

static struct mfd_cell rt5033_regulator_devs[] = {
	RT5033_VR_DEVS(LDO_SAFE),
	RT5033_VR_DEVS(LDO1),
	RT5033_VR_DEVS(DCDC1),
};
#endif

inline static int rt5033_read_device(struct i2c_client *i2c,
		int reg, int bytes, void *dest)
{
	int ret;
	if (bytes > 1) {
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, bytes, dest);
	} else {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret < 0)
			return ret;
		*(unsigned char *)dest = (unsigned char)ret;
	}
	return ret;
}

inline static int rt5033_write_device(struct i2c_client *i2c,
		int reg, int bytes, void *src)
{
	int ret;
	uint8_t *data;
	if (bytes > 1)
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, bytes, src);
	else {
		data = src;
		ret = i2c_smbus_write_byte_data(i2c, reg, *data);
	}
	return ret;
}

int rt5033_block_read_device(struct i2c_client *i2c,
		int reg, int bytes, void *dest)
{
	struct rt5033_mfd_chip *chip = i2c_get_clientdata(i2c);
	int ret;
	mutex_lock(&chip->io_lock);
	ret = rt5033_read_device(i2c, reg, bytes, dest);
	mutex_unlock(&chip->io_lock);
	return ret;
}
EXPORT_SYMBOL(rt5033_block_read_device);

int rt5033_block_write_device(struct i2c_client *i2c,
		int reg, int bytes, void *src)
{
	struct rt5033_mfd_chip *chip = i2c_get_clientdata(i2c);
	int ret;
	mutex_lock(&chip->io_lock);
	ret = rt5033_write_device(i2c, reg, bytes, src);
	mutex_unlock(&chip->io_lock);
	return ret;
}
EXPORT_SYMBOL(rt5033_block_write_device);

int rt5033_reg_read(struct i2c_client *i2c, int reg)
{
	struct rt5033_mfd_chip *chip = i2c_get_clientdata(i2c);
	unsigned char data = 0;
	int ret;

	mutex_lock(&chip->io_lock);
	ret = rt5033_read_device(i2c, reg, 1, &data);
	mutex_unlock(&chip->io_lock);

	if (ret < 0)
		return ret;
	else
		return (int)data;
}
EXPORT_SYMBOL(rt5033_reg_read);

int rt5033_reg_write(struct i2c_client *i2c, int reg,
		unsigned char data)
{
	struct rt5033_mfd_chip *chip = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}
EXPORT_SYMBOL(rt5033_reg_write);

int rt5033_assign_bits(struct i2c_client *i2c, int reg,
		unsigned char mask, unsigned char data)
{
	struct rt5033_mfd_chip *chip = i2c_get_clientdata(i2c);
	unsigned char value;
	int ret;

	mutex_lock(&chip->io_lock);
	ret = rt5033_read_device(i2c, reg, 1, &value);

	if (ret < 0)
		goto out;

	value &= ~mask;
	value |= data;
	ret = i2c_smbus_write_byte_data(i2c, reg, value);

out:
	mutex_unlock(&chip->io_lock);
	return ret;
}
EXPORT_SYMBOL(rt5033_assign_bits);

int rt5033_set_bits(struct i2c_client *i2c, int reg,
		unsigned char mask)
{
	return rt5033_assign_bits(i2c,reg,mask,mask);
}
EXPORT_SYMBOL(rt5033_set_bits);

int rt5033_clr_bits(struct i2c_client *i2c, int reg,
		unsigned char mask)
{
	return rt5033_assign_bits(i2c,reg,mask,0);
}
EXPORT_SYMBOL(rt5033_clr_bits);

extern int rt5033_init_irq(rt5033_mfd_chip_t *chip);
extern int rt5033_exit_irq(rt5033_mfd_chip_t *chip);

static int __devinit rt5033_mfd_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	int ret = 0;
	u8 data = 0;

	rt5033_mfd_chip_t *chip;
	rt5033_mfd_platform_data_t *pdata = i2c->dev.platform_data;

	pr_info("%s : RT5033 MFD Driver start probe\n", __func__);

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (chip ==  NULL) {
		dev_err(chip->dev, "Memory is not enough.\n");
		ret = -ENOMEM;
		goto err_mfd_nomem;
	}

	ret = i2c_check_functionality(i2c->adapter, I2C_FUNC_SMBUS_BYTE_DATA |
			I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);
	if (!ret) {
		ret = i2c_get_functionality(i2c->adapter);
		dev_err(chip->dev, "I2C functionality is not supported.\n");
		ret = -ENOSYS;
		goto err_i2cfunc_not_support;
	}

	chip->dev = &i2c->dev;
	chip->i2c_client = i2c;
	chip->pdata = pdata;

	pdata->irq_base = irq_alloc_descs(-1, 0, RT5033_IRQS_NR, -1);
	if (pdata->irq_base < 0) {
		pr_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
				"rt5033-mfd", __func__, pdata->irq_base);
		ret = -EINVAL;
		goto irq_base_err;
	} else {
		chip->irq_base = pdata->irq_base;
	}

	i2c_set_clientdata(i2c, chip);
	mutex_init(&chip->io_lock);

	wake_lock_init(&(chip->irq_wake_lock), WAKE_LOCK_SUSPEND,
			"rt5033mfd_wakelock");

	ret = rt5033_clr_bits(i2c, 0x47, 1<<3);
	pr_info("Diable MANUAL RESET (%d)\n", ret);

	ret = rt5033_init_irq(chip);

	if (ret < 0) {
		dev_err(chip->dev,
				"Error : can't initialize RT5033 MFD irq\n");
		goto err_init_irq;
	}

#ifdef CONFIG_REGULATOR_RT5033
#if (LINUX_VERSION_CODE>=KERNEL_VERSION(3,6,0))
	ret = mfd_add_devices(chip->dev, 0, &rt5033_regulator_devs[0],
			ARRAY_SIZE(rt5033_regulator_devs),
			NULL, chip->irq_base, NULL);
#else
	ret = mfd_add_devices(chip->dev, 0, &rt5033_regulator_devs[0],
			ARRAY_SIZE(rt5033_regulator_devs),
			NULL, chip->irq_base);
#endif
	if (ret < 0) {
		dev_err(chip->dev,
				"Error : can't add regulator\n");
		goto err_add_regulator_devs;
	}
#endif /*CONFIG_REGULATOR_RT5033*/

#ifdef CONFIG_FLED_RT5033
#if (LINUX_VERSION_CODE>=KERNEL_VERSION(3,6,0))
	ret = mfd_add_devices(chip->dev, 0, &rt5033_fled_devs[0],
			ARRAY_SIZE(rt5033_fled_devs),
			NULL, chip->irq_base, NULL);
#else
	ret = mfd_add_devices(chip->dev, 0, &rt5033_fled_devs[0],
			ARRAY_SIZE(rt5033_fled_devs),
			NULL, chip->irq_base);
#endif
	if (ret < 0)
	{
		dev_err(chip->dev,"Failed : add FlashLED devices");
		goto err_add_fled_devs;
	}
#endif /*CONFIG_FLED_RT5033*/


#ifdef CONFIG_CHARGER_RT5033
#if (LINUX_VERSION_CODE>=KERNEL_VERSION(3,6,0))
	ret = mfd_add_devices(chip->dev, 0, &rt5033_charger_devs[0],
			ARRAY_SIZE(rt5033_charger_devs),
			NULL, chip->irq_base, NULL);
#else
	ret = mfd_add_devices(chip->dev, 0, &rt5033_charger_devs[0],
			ARRAY_SIZE(rt5033_charger_devs),
			NULL, chip->irq_base);
#endif
	if (ret<0) {
		dev_err(chip->dev, "Failed : add charger devices\n");
		goto err_add_chg_devs;
	}
#endif /*CONFIG_CHARGER_RT5033*/

	pr_info("%s : RT5033 MFD Driver Fin probe\n", __func__);
	return ret;

#ifdef CONFIG_CHARGER_RT5033
err_add_chg_devs:
#endif /*CONFIG_CHARGER_RT5033*/

#ifdef CONFIG_FLED_RT5033
err_add_fled_devs:
#endif /*CONFIG_FLED_RT5033*/
	mfd_remove_devices(chip->dev);
#ifdef CONFIG_REGULATOR_RT5033
err_add_regulator_devs:
#endif /*CONFIG_REGULATOR_RT5033*/
err_init_irq:
	wake_lock_destroy(&(chip->irq_wake_lock));
	mutex_destroy(&chip->io_lock);
	kfree(chip);
irq_base_err:
err_mfd_nomem:
err_i2cfunc_not_support:
	return ret;
}

static int __devexit rt5033_mfd_remove(struct i2c_client *i2c)
{
	rt5033_mfd_chip_t *chip = i2c_get_clientdata(i2c);

	pr_info("%s : RT5033 MFD Driver remove\n", __func__);

	mfd_remove_devices(chip->dev);
	wake_lock_destroy(&(chip->irq_wake_lock));
	mutex_destroy(&chip->io_lock);
	kfree(chip);

	return 0;
}

#ifdef CONFIG_PM
/*check here cause error
  type define must de changed....
  use struct device *dev for function attribute
 */
int rt5033_mfd_suspend(struct device *dev)
{
	struct i2c_client *i2c =
		container_of(dev, struct i2c_client, dev);

	rt5033_mfd_chip_t *chip = i2c_get_clientdata(i2c);
	chip->suspend_flag = 1;

	return 0;
}

int rt5033_mfd_resume(struct device *dev)
{
	struct i2c_client *i2c =
		container_of(dev, struct i2c_client, dev);
	rt5033_mfd_chip_t *chip = i2c_get_clientdata(i2c);
	chip->suspend_flag = 0;

	return 0;
}
#else
int rt5033_mfd_suspend(struct i2c_client *i2c, pm_message_t mesg)
{
	rt5033_mfd_chip_t *chip = i2c_get_clientdata(i2c);
	chip->suspend_flag = 1;

	return 0;
}

int rt5033_mfd_resume(struct i2c_client *i2c)
{
	rt5033_mfd_chip_t *chip = i2c_get_clientdata(i2c);
	chip->suspend_flag = 0;

	return 0;
}

#endif /* CONFIG_PM */

static const struct i2c_device_id rt5033_mfd_id_table[] = {
	{ "rt5033-mfd", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, rt5033_id_table);

#define RT5033_REGULATOR_REG_OUTPUT_EN (0x41)
#define RT5033_REGULATOR_EN_MASK_LDO_SAFE (1<<6)

static void rt5033_shutdown(struct i2c_client *iic)
{
    /* Force to turn on SafeLDO */
    rt5033_set_bits(iic, RT5033_REGULATOR_REG_OUTPUT_EN,
                    RT5033_REGULATOR_EN_MASK_LDO_SAFE);
}

const struct dev_pm_ops rt5033_pm = {
	.suspend = rt5033_mfd_suspend,
	.resume = rt5033_mfd_resume,
};

static struct i2c_driver rt5033_mfd_driver = {
	.driver	= {
		.name	= "rt5033-mfd",
		.owner	= THIS_MODULE,
		.pm		= &rt5033_pm,
	},
	.shutdown   = rt5033_shutdown,
	.probe		= rt5033_mfd_probe,
	.remove		= __devexit_p(rt5033_mfd_remove),
	.id_table	= rt5033_mfd_id_table,
};

static int __init rt5033_mfd_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&rt5033_mfd_driver);
	if (ret != 0)
		pr_info("%s : Failed to register RT5033 MFD I2C driver\n",
				__func__);

	return ret;
}
subsys_initcall(rt5033_mfd_i2c_init);

static void __exit rt5033_mfd_i2c_exit(void)
{
	i2c_del_driver(&rt5033_mfd_driver);
}
module_exit(rt5033_mfd_i2c_exit);

MODULE_DESCRIPTION("Richtek RT5033 MFD I2C Driver");
MODULE_AUTHOR("Patrick Chang <patrick_chang@richtek.com>");
MODULE_VERSION(RT5033_DRV_VER);
MODULE_LICENSE("GPL");
