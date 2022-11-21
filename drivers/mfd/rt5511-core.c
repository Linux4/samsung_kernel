/* drivers/mfd/rt5511-core.c
 * RT5511 Codec - Multifunction Device Driver
 *
 * Copyright (C) 2013 Richtek Technology Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/mfd/rt5511/core.h>
#include <linux/mfd/rt5511/pdata.h>
#include <linux/mfd/rt5511/registers.h>
#include <linux/err.h>
#include <linux/of_gpio.h>
#include <linux/version.h>


#define RT5511_USE_EXT_POWER_SUPPLY 1

static int rt5511_read(struct rt5511 *rt5511, u16 reg,
		       int bytes, void *dest)
{
	int ret, i;
	u8 *buf = dest;

	BUG_ON(bytes <= 0);

	ret = rt5511->read_dev(rt5511, reg, bytes, dest);
	if (ret < 0) {
		dev_err(rt5511->dev, "rt5511_read(0x%02x) failed: %d\n",
			reg, ret);
		BUG_ON(ret < 0);
	}

	for (i = 0; i < bytes; i++) {
		dev_vdbg(rt5511->dev, "Read %02x from Reg0x%x)\n",
			 buf[i], reg + i);
	}

	return ret;
}

/**
 * rt5511_reg_read: Read a single RT5511 register.
 *
 * @rt5511: Device to read from.
 * @reg: Register to read.
 */
int rt5511_reg_read(struct rt5511 *rt5511, u16 reg, u32 *pval, int size)
{
	int ret;
	u32 val = 0;
	mutex_lock(&rt5511->io_lock);

	ret = rt5511_read(rt5511, reg, size, &val);

	mutex_unlock(&rt5511->io_lock);

	BUG_ON(size != 1 && size != 4);
	if (ret < 0)
		return ret;
	if (size == 4)
		val = be32_to_cpu(val);
	*pval = val;
	return ret;

}
EXPORT_SYMBOL_GPL(rt5511_reg_read);

/**
 * rt5511_bulk_read: Read multiple RT5511 registers
 *
 * @rt5511: Device to read from
 * @reg: First register
 * @count: Number of registers
 * @buf: Buffer to fill.  The data will be returned big endian.
 */
int rt5511_bulk_read(struct rt5511 *rt5511, u16 reg,
		     int count, u8 *buf)
{
	int ret;

	mutex_lock(&rt5511->io_lock);

	ret = rt5511_read(rt5511, reg, count, buf);

	mutex_unlock(&rt5511->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rt5511_bulk_read);

static int rt5511_write(struct rt5511 *rt5511, u16 reg,
			int bytes, const void *src)
{
	const u8 *buf = src;
	int i, ret;

	BUG_ON(bytes <= 0);

	for (i = 0; i < bytes; i++) {
		dev_vdbg(rt5511->dev, "Write %04x to Reg0x%x\n",
			 buf[i], reg + i);
	}

	ret = rt5511->write_dev(rt5511, reg, bytes, src);
	if (ret < 0) {
		dev_err(rt5511->dev, "rt5511_write(0x%02x) failed: %d\n",
			reg, ret);
		BUG_ON(ret < 0);
	}

	return ret;
}

/**
 * rt5511_reg_write: Write a single RT5511 register.
 *
 * @rt5511: Device to write to.
 * @reg: Register to write to.
 * @val: Value to write.
 */
int rt5511_reg_write(struct rt5511 *rt5511, u16 reg, u32 val, int size)
{
	int ret;
	BUG_ON(size != 1 && size != 4);
	if (size == 4)
		val = be32_to_cpu(val);

	mutex_lock(&rt5511->io_lock);
	ret = rt5511_write(rt5511, reg, size, &val);
	mutex_unlock(&rt5511->io_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(rt5511_reg_write);

/**
 * rt5511_bulk_write: Write multiple RT5511 registers
 *
 * @rt5511: Device to write to
 * @reg: First register
 * @count: Number of registers
 * @buf: Buffer to write from.  Data must be big-endian formatted.
 */
int rt5511_bulk_write(struct rt5511 *rt5511, u16 reg,
		      int count, const u8 *buf)
{
	int ret;

	mutex_lock(&rt5511->io_lock);

	ret = rt5511_write(rt5511, reg, count, buf);

	mutex_unlock(&rt5511->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rt5511_bulk_write);

/**
 * rt5511_set_bits: Set the value of a bitfield in a RT5511 register
 *
 * @rt5511: Device to write to.
 * @reg: Register to write to.
 * @mask: Mask of bits to set.
 * @val: Value to set (unshifted)
 */
int rt5511_set_bits(struct rt5511 *rt5511, u16 reg,
		    u8 mask, u8 val)
{
	int ret;
	u8 r;

	mutex_lock(&rt5511->io_lock);

	ret = rt5511_read(rt5511, reg, 1, &r);
	if (ret < 0)
		goto out;

	r &= ~mask;
	r |= (val & mask);

	ret = rt5511_write(rt5511, reg, 1, &r);

out:
	mutex_unlock(&rt5511->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rt5511_set_bits);

int rt5511_bits_on(struct rt5511 *rt5511, u16 reg, u8 mask)
{
	return rt5511_set_bits(rt5511, reg, mask, mask);
}
EXPORT_SYMBOL_GPL(rt5511_bits_on);

int rt5511_bits_off(struct rt5511 *rt5511, u16 reg, u8 mask)
{
	return rt5511_set_bits(rt5511, reg, mask, 0);
}
EXPORT_SYMBOL_GPL(rt5511_bits_off);

struct i2c_client *rt5511_get_i2c_client(struct rt5511 *rt5511)
{
	return rt5511->control_data;
}
EXPORT_SYMBOL_GPL(rt5511_get_i2c_client);

void rt5511_set_private_power_on_event(struct rt5511 *rt5511,
				       rt5511_poweron_cb cb,
				       void *priv_data)
{
	BUG_ON(rt5511 == NULL);
	rt5511->poweron_cb_priv_data = priv_data;
	rt5511->poweron_cb = cb;
}
EXPORT_SYMBOL_GPL(rt5511_set_private_power_on_event);

int rt5511_software_reset(struct rt5511 *rt5511)
{
	int ret;
	ret = rt5511_reg_write(rt5511, RT5511_SWRESET_POWERDOWN, 0x80, 1);
	msleep(1);
	return ret;
}
EXPORT_SYMBOL_GPL(rt5511_software_reset);

static struct mfd_cell rt5511_devs[] = {
	{
		.name = "rt5511-codec",
		.num_resources = 0,
		.resources = NULL,
#ifdef CONFIG_OF
		.of_compatible = "richtek,rt5511-codec",
#endif
	},
};

/*
 * Supplies for the main bulk of CODEC; the LDO supplies are ignored
 * and should be handled via the standard regulator API supply
 * management.
 */
static const char * const rt5511_main_supplies[] = {
	"DBVDD1",
	"DBVDD2",
	"DBVDD3",
	"AVDD",
	"CPVDD",
	"SPKVDD",
	"MICVDD",
};

#ifdef CONFIG_PM

#define RT5511_VMID_MASK (1 << 1)

int rt5511_mfd_do_suspend(struct rt5511 *rt5511)
{
	int ret;
	u32 regval = 0;

	/* Don't actually go through with the suspend if the CODEC is
	 * still active (eg, for audio passthrough from CP. */
	dev_info(rt5511->dev, "%s : rt5511 MFD suspend function is called\n",
		 __func__);

	if (rt5511->suspended)
		return 0;

	ret = rt5511_reg_read(rt5511, RT5511_BIASE_MIC_CTRL, &regval, 1);
	if (ret < 0) {
		dev_err(rt5511->dev, "Failed to read power status: %d\n", ret);
	} else if (regval & RT5511_VMID_MASK) {
		dev_dbg(rt5511->dev, "CODEC still active, ignoring suspend\n");
		return 0;
	}

	ret = rt5511_reg_read(rt5511, RT5511_SOURCE_ENABLE, &regval, 1);
	if (ret < 0) {
		dev_err(rt5511->dev, "Failed to read power status: %d\n", ret);
	} else if (regval & (0x0f)) {
		dev_dbg(rt5511->dev, "CODEC still active, ignoring suspend\n");
		return 0;
	}

	rt5511_software_reset(rt5511);

	rt5511->suspended = true;
	dev_info(rt5511->dev, "%s : suspend => turn off RT5511's power\n",
		 __func__);
#if RT5511_USE_EXT_POWER_SUPPLY
	dev_info(rt5511->dev, "%s : USE_EXT_POWER = 1, just send POWERDN cmd\n",
		 __func__);
	rt5511_bits_on(rt5511, RT5511_SWRESET_POWERDOWN, 1 << 6);
#else
	ret = regulator_bulk_disable(rt5511->num_supplies,
				     rt5511->supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to disable supplies: %d\n", ret);
		return ret;
	}
#endif /* RT5511_USE_EXT_POWER_SUPPLY */
	return 0;
}

static int rt5511_disable_vmid_and_path(struct rt5511 *rt5511)
{

	/* Change RT5511's default setting
	 * VMID and path should be turn off initially
	 * and then turned on by request
	 */
	/* Set PATH1/2 DAC and ADC off */
	rt5511_bits_off(rt5511, RT5511_SOURCE_ENABLE, 0x0f);
	rt5511_bits_off(rt5511, RT5511_BIASE_MIC_CTRL, 0x02);
	return 0;
}

int rt5511_mfd_do_resume(struct rt5511 *rt5511)
{
#if !RT5511_USE_EXT_POWER_SUPPLY
	int ret;
#endif
	dev_info(rt5511->dev, "%s : resume() is called\n",
		 __func__);

	/* We may have lied to the PM core about suspending */
	if (!rt5511->suspended) {
		dev_info(rt5511->dev, "%s : previous status is not real suspended\n",
			 __func__);
		return 0;
	}
	dev_info(rt5511->dev, "%s : resume => turn on RT5511's power\n",
		 __func__);
#if RT5511_USE_EXT_POWER_SUPPLY
	dev_info(rt5511->dev, "%s : USE_EXT_POWER = 1, just send POWERUP cmd\n",
		 __func__);
	rt5511_bits_off(rt5511, RT5511_SWRESET_POWERDOWN, 1 << 6);
#else
	ret = regulator_bulk_enable(rt5511->num_supplies,
				    rt5511->supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to enable supplies: %d\n", ret);
		return ret;
	}
#endif
	rt5511_disable_vmid_and_path(rt5511);
	rt5511->suspended = false;
	if (rt5511->poweron_cb)
		rt5511->poweron_cb(rt5511->poweron_cb_priv_data);

	return 0;
}

static int rt5511_suspend(struct device *dev)
{
	return 0;
}

static int rt5511_resume(struct device *dev)
{
	return 0;
}
#endif


/*
 * Instantiate the generic non-control parts of the device.
 */
static int rt5511_device_init(struct rt5511 *rt5511, int irq)
{
	const char *devname;
	int ret, i;
	u32 reg_val;


	mutex_init(&rt5511->io_lock);
	dev_set_drvdata(rt5511->dev, rt5511);
	rt5511_software_reset(rt5511);
	rt5511_disable_vmid_and_path(rt5511);

	switch (rt5511->type) {
	case RT5511:
		rt5511->num_supplies = ARRAY_SIZE(rt5511_main_supplies);
		devname = "RT5511";
		break;
	default:
		BUG();
		goto err;
	}

	rt5511->supplies = kzalloc(sizeof(struct regulator_bulk_data) *
				   rt5511->num_supplies,
				   GFP_KERNEL);
	if (!rt5511->supplies) {
		ret = -ENOMEM;
		goto err;
	}

	switch (rt5511->type) {
	case RT5511:
		for (i = 0; i < ARRAY_SIZE(rt5511_main_supplies); i++)
			rt5511->supplies[i].supply = rt5511_main_supplies[i];
		break;
	default:
		BUG();
		goto err;
	}
#if RT5511_USE_EXT_POWER_SUPPLY
	dev_info(rt5511->dev, "%s: %s Use Ext Power\n",
		 __func__, devname);
#else
	ret = regulator_bulk_get(rt5511->dev, rt5511->num_supplies,
				 rt5511->supplies);

	if (ret != 0) {
		dev_err(rt5511->dev, "Failed to get supplies: %d\n", ret);
		goto err_supplies;
	}

	ret = regulator_bulk_enable(rt5511->num_supplies,
				    rt5511->supplies);
	if (ret != 0) {
		dev_err(rt5511->dev, "Failed to enable supplies: %d\n", ret);
		goto err_get;
	}
#endif

	ret = rt5511_reg_read(rt5511, RT5511_VID, &reg_val, 1);
	if (ret < 0) {
		dev_err(rt5511->dev, "Failed to read revision register: %d\n",
			ret);
		goto err_enable;
	}

	rt5511->revision = reg_val;


	dev_info(rt5511->dev, "%s Revision 0x%02x\n", devname,
		 rt5511->revision);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
	ret = mfd_add_devices(rt5511->dev, -1,
			      rt5511_devs, ARRAY_SIZE(rt5511_devs),
			      NULL, 0, NULL);
#else
	ret = mfd_add_devices(rt5511->dev, -1,
			      rt5511_devs, ARRAY_SIZE(rt5511_devs),
			      NULL, 0);
#endif

	if (ret != 0) {
		dev_err(rt5511->dev, "Failed to add children: %d\n", ret);
		goto err_mfd_add;
	}

#if 0 /* To do */
	pm_runtime_enable(rt5511->dev);
	pm_runtime_resume(rt5511->dev);
#endif

	return 0;
err_mfd_add:
err_enable:
#if !RT5511_USE_EXT_POWER_SUPPLY
	regulator_bulk_disable(rt5511->num_supplies,
			       rt5511->supplies);
err_get:
	regulator_bulk_free(rt5511->num_supplies, rt5511->supplies);
err_supplies:
	kfree(rt5511->supplies);
#endif
err:
	mfd_remove_devices(rt5511->dev);
	return ret;
}

static void rt5511_device_exit(struct rt5511 *rt5511)
{
	pm_runtime_disable(rt5511->dev);
	mfd_remove_devices(rt5511->dev);
#if !RT5511_USE_EXT_POWER_SUPPLY
	regulator_bulk_disable(rt5511->num_supplies,
			       rt5511->supplies);
	regulator_bulk_free(rt5511->num_supplies, rt5511->supplies);
#endif
	kfree(rt5511->supplies);
	kfree(rt5511);
}

static int rt5511_i2c_read_device(struct rt5511 *rt5511, u8 reg,
				  int bytes, void *dest)
{
	struct i2c_client *i2c = rt5511->control_data;
	return i2c_smbus_read_i2c_block_data(i2c, reg, bytes, dest);
}


static int rt5511_i2c_write_device(struct rt5511 *rt5511, u8 reg,
				   int bytes, const void *src)
{
	struct i2c_client *i2c = rt5511->control_data;
	return i2c_smbus_write_i2c_block_data(i2c, reg, bytes, src);
}

#ifdef CONFIG_OF

#define RT5511_MAX_PROPERTY_LENGTH 100

static void rt5511_parse_dt_eq_cfg(struct device *dev,
	struct rt5511_pdata *pdata,
	struct device_node *np, int index)
{
	int i, j, k;
	char name[RT5511_MAX_PROPERTY_LENGTH];
	struct device_node *child;
	u32 buffer[RT5511_BQ_REG_SIZE];

	i = index;
	sprintf(name, "rt5511,eq_cfg%d", i);
	child = of_find_node_by_name(of_node_get(np), name);

	if (of_property_read_u32_array(child, "rt5511,eq_coeff_en",
		buffer, RT5511_BQ_REG_NUM) == 0) {
		for (j = 0; j < RT5511_BQ_REG_NUM; j++)
			pdata->eq_cfgs[i].en[j] = buffer[j];
	} else {
		dev_warn(dev,
			"%s : cannot read eq_coeff_en[%d]\n",
			__func__, i);
	}

	for (j = 0 ; j < RT5511_BQ_REG_NUM ; j++) {
		sprintf(name, "rt5511,eq_coeff%d", j);
		if (of_property_read_u32_array(child, name,
			buffer, RT5511_BQ_REG_SIZE) == 0) {
			for (k = 0; k < RT5511_BQ_REG_SIZE; k++)
				pdata->eq_cfgs[i].coeff[j][k] = buffer[k];
		} else {
			dev_warn(dev,
				"%s : cannot read eq_coeff%d[%d]\n",
				__func__, j, i);
		}
	}
	of_node_put(child);
}

static void rt5511_parse_dt_drc_cfg(struct device *dev,
	struct rt5511_pdata *pdata,
	struct device_node *np, int index)
{
	int i, j;
	char name[RT5511_MAX_PROPERTY_LENGTH];
	struct device_node *child;
	u32 buffer[RT5511_DRC_REG2_SIZE];

	i = index;
	sprintf(name, "rt5511,drc_cfg%d", i);
	child = of_find_node_by_name(of_node_get(np), name);

	if (of_property_read_u32_array(child, "rt5511,drc_coeff1",
					buffer, RT5511_DRC_REG1_SIZE) == 0) {
		for (j = 0; j < RT5511_DRC_REG1_SIZE; j++)
			pdata->drc_cfgs[i].coeff1[j] = buffer[j];

	} else {
		dev_warn(dev,
			"%s : cannot read drc_coeff1[%d]\n", __func__, i);
	}
	if (of_property_read_u32_array(child, "rt5511,drc_coeff2",
					buffer, RT5511_DRC_REG2_SIZE) == 0) {
		for (j = 0; j < RT5511_DRC_REG2_SIZE; j++)
			pdata->drc_cfgs[i].coeff2[j] = buffer[j];

	} else {
		dev_warn(dev,
			"%s : cannot read drc_coeff2[%d]\n", __func__, i);
	}
	of_node_put(child);
};

static int rt5511_parse_dt(struct device *dev,
			   struct rt5511_pdata *pdata)
{
	struct device_node *np = dev->of_node;
	//struct device_node *child;
	u32 buffer[RT5511_BQ_REG_NUM];
	u32 val32;
	int i, j;
	//char name[RT5511_MAX_PROPERTY_LENGTH];
	pdata->micbias[0] = 0;
	pdata->micbias[1] = 0;
	pdata->mic1_differential = 0;
	pdata->mic2_differential = 0;
	pdata->mic3_differential = 0;

	if (of_property_read_u32_array(np, "rt5511,mic_diff", buffer, 3) == 0) {
		pdata->mic1_differential = buffer[0];
		pdata->mic2_differential = buffer[1];
		pdata->mic3_differential = buffer[2];
	}

	if (of_property_read_u32_array(np, "rt5511,micbias", buffer, 2) == 0) {
		pdata->micbias[0] = buffer[0];
		pdata->micbias[1] = buffer[1];
	}

	pdata->gpio_scl = of_get_named_gpio_flags(np, "rt5511,scl-gpio",
				0, &pdata->scl_gpio_flags);
	pdata->gpio_sda = of_get_named_gpio_flags(np, "rt5511,sda-gpio",
				0, &pdata->sda_gpio_flags);

	/* parse EQ configurations */
	pdata->eq_en = devm_kzalloc(dev,
		sizeof(bool) * RT5511_BQ_PATH_NUM, GFP_KERNEL);

	if (of_property_read_u32_array(np, "rt5511,eq_en",
				       buffer, RT5511_BQ_PATH_NUM) == 0) {
		for (i = 0; i < RT5511_BQ_PATH_NUM; i++)
			pdata->eq_en[i] = buffer[i];
	} else {
		dev_warn(dev, "%s : cannot read eq_en\n", __func__);
	}

	pdata->eq_cfgs = devm_kzalloc(dev,
		sizeof(struct rt5511_eq_cfg) * RT5511_BQ_SET_NUM, GFP_KERNEL);

	for (i = 0; i < RT5511_BQ_SET_NUM ; i++)
		rt5511_parse_dt_eq_cfg(dev, pdata, np, i);

	/* parse DRC configurations */
	pdata->drc_en = devm_kzalloc(dev,
		sizeof(bool) * RT5511_DRC_REG_NUM, GFP_KERNEL);

	if (of_property_read_u32_array(np, "rt5511,drc_en",
				       buffer, RT5511_DRC_REG_NUM) == 0) {
		for (i = 0; i < RT5511_DRC_REG_NUM; i++)
			pdata->drc_en[i] = buffer[i];
	} else {
		dev_warn(dev, "%s : cannot read drc_en\n", __func__);
	}

	pdata->drc_cfgs = devm_kzalloc(dev,
		sizeof(struct rt5511_drc_cfg) * RT5511_DRC_REG_NUM,
		GFP_KERNEL);

	for (i = 0 ; i < RT5511_DRC_REG_NUM ; i++)
		rt5511_parse_dt_drc_cfg(dev, pdata, np, i);

	/* parse DRC-NG configurations */
	pdata->drc_ng_cfg = devm_kzalloc(dev,
		sizeof(u8) * RT5511_DRC_NG_REG_SIZE, GFP_KERNEL);

	if (of_property_read_u32(np, "rt5511,drc_ng_en", &val32) >= 0)
		pdata->drc_ng_en = val32;
	else
		pdata->drc_ng_en = 0;

	if (of_property_read_u32_array(np, "rt5511,drc_ng_cfg",
				       buffer, RT5511_DRC_NG_REG_SIZE) == 0) {

		for (j = 0; j < RT5511_DRC_NG_REG_SIZE; j++)
			pdata->drc_ng_cfg[j] = buffer[j];
	}

	/* parse DRE configurations */

	pdata->dre_cfg = devm_kzalloc(dev,
		sizeof(u8) * RT5511A_DRE_REG_SIZE, GFP_KERNEL);

	if (of_property_read_u32_array(np, "rt5511,dre_cfg",
				       buffer, RT5511A_DRE_REG_SIZE) == 0) {

		for (j = 0; j < RT5511A_DRE_REG_SIZE; j++)
			pdata->dre_cfg[j] = buffer[j];
	}

	return 0;
}
#else
static int rt5511_parse_dt(struct device *dev,
			   struct rt5511_pdata *pdata)
{
	/* dummuy parse DT */
	return 0;
}

#endif

static int rt5511_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	int ret;
	struct rt5511 *rt5511;
	struct rt5511_pdata *pdata;
	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_nodevm;
		}
		ret = rt5511_parse_dt(&i2c->dev, pdata);
		if (ret < 0)
			goto err_parse_dt;
		i2c->dev.platform_data = pdata;
	}

	rt5511 = kzalloc(sizeof(struct rt5511), GFP_KERNEL);
	if (rt5511 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, rt5511);
	rt5511->dev = &i2c->dev;
	rt5511->control_data = i2c;
	rt5511->read_dev = rt5511_i2c_read_device;
	rt5511->write_dev = rt5511_i2c_write_device;
	rt5511->type = id->driver_data;
	dev_info(&i2c->dev, "%s : device type = 0x%x\n",
		 __func__, rt5511->type);
	if (rt5511_device_init(rt5511, i2c->irq) < 0) {
		kfree(rt5511);
		return -ENODEV;
	}
	return 0;
err_parse_dt:
err_nodevm:
	return ret;
}

static int rt5511_i2c_remove(struct i2c_client *i2c)
{
	struct rt5511 *rt5511 = i2c_get_clientdata(i2c);

	rt5511_device_exit(rt5511);

	return 0;
}

static const struct i2c_device_id rt5511_i2c_id[] = {

	{ "rt5511", RT5511 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rt5511_i2c_id);

static UNIVERSAL_DEV_PM_OPS(rt5511_pm_ops, rt5511_suspend, rt5511_resume,
			    NULL);


#ifdef CONFIG_OF
static struct of_device_id rt5511_match_table[] = {
	{ .compatible = "richtek,rt5511",},
	{},
};
#else
#define rt5511_match_table NULL
#endif


static struct i2c_driver rt5511_i2c_driver = {
	.driver = {
		.name = "rt5511",
		.owner = THIS_MODULE,
		.pm = &rt5511_pm_ops,
		.of_match_table = rt5511_match_table,
	},
	.probe = rt5511_i2c_probe,
	.remove = rt5511_i2c_remove,
	.id_table = rt5511_i2c_id,
};

static int __init rt5511_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&rt5511_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register rt5511 I2C driver: %d\n", ret);

	return ret;
}
module_init(rt5511_i2c_init);

static void __exit rt5511_i2c_exit(void)
{
	i2c_del_driver(&rt5511_i2c_driver);
}
module_exit(rt5511_i2c_exit);

MODULE_DESCRIPTION("Core support for the RT5511 audio CODEC");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Patrick Chang<patrick_chang@richtek.com>");
