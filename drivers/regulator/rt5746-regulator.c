/*
 *  drivers/regulator/rt5746-regulator.c
 *  Driver for Richtek RT5746 5A Buck
 *
 *  Copyright (C) 2014 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/regulator/of_regulator.h>
#endif /* #ifdef CONFIG_OF */
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <linux/regulator/rt5746-regulator.h>

struct rt5746_regulator_info {
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	struct i2c_client *i2c;
	struct device *dev;
	struct mutex io_lock;
	struct delayed_work irq_dwork;
	const int step_uV;
	const int vol_output_size;
	int min_uV;
	int max_uV;
	unsigned char nvol_reg;
	unsigned char nvol_shift;
	unsigned char nvol_mask;
	unsigned char svol_reg;
	unsigned char svol_shift;
	unsigned char svol_mask;
	unsigned char reg_addr;
	unsigned char reg_data;
	unsigned char suspend:1;
};

static unsigned char rt5746_init_regval[5] = {
	0x02, //reg 0x02
	0x00, //reg 0x12
	0x19, //reg 0x13
	0x01, //reg 0x14
	0x63, //reg 0x16
};

static inline int rt5746_read_device(struct i2c_client *i2c,
				      int reg, int bytes, void *dest)
{
	int ret;
	if (bytes>1)
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, bytes, dest);
	else {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret<0)
			return ret;
		*(unsigned char *)dest = (unsigned char)ret;
	}
	return ret;
}

static inline int rt5746_write_device(struct i2c_client *i2c,
				      int reg, int bytes, void *dest)
{
	int ret;
	if (bytes > 1)
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, bytes, dest);
	else {
		ret = i2c_smbus_write_byte_data(i2c, reg, *(u8 *)dest);
		if (ret < 0)
			return ret;
	}
	return ret;
}

static int rt5746_reg_block_write(struct i2c_client *i2c, \
			int reg, int bytes, void *dest)
{
	return rt5746_write_device(i2c, reg, bytes, dest);
}

static int rt5746_reg_read(struct i2c_client *i2c, int reg)
{
	struct rt5746_regulator_info *ri = i2c_get_clientdata(i2c);
	int ret;
	RTINFO("I2C Read (client : 0x%x) reg = 0x%x\n",
           (unsigned int)i2c,(unsigned int)reg);
	mutex_lock(&ri->io_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&ri->io_lock);
	return ret;
}

static int rt5746_reg_write(struct i2c_client *i2c, int reg, unsigned char data)
{
	struct rt5746_regulator_info *ri = i2c_get_clientdata(i2c);
	int ret;
	RTINFO("I2C Write (client : 0x%x) reg = 0x%x, data = 0x%x\n",
           (unsigned int)i2c,(unsigned int)reg,(unsigned int)data);
	mutex_lock(&ri->io_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, data);
	mutex_unlock(&ri->io_lock);
	return ret;
}

static int rt5746_assign_bits(struct i2c_client *i2c, int reg,
		unsigned char mask, unsigned char data)
{
	struct rt5746_regulator_info *ri = i2c_get_clientdata(i2c);
	unsigned char value;
	int ret;
	RTINFO("(client : 0x%x) reg = 0x%x, mask = 0x%x, data = 0x%x\n",
           (unsigned int)i2c,(unsigned int)reg,(unsigned int)mask,
	   (unsigned int)data);
	mutex_lock(&ri->io_lock);
	ret = rt5746_read_device(i2c, reg, 1, &value);

	if (ret < 0)
		goto out;
	value &= ~mask;
	value |= (data&mask);
	ret = i2c_smbus_write_byte_data(i2c,reg,value);
out:
	mutex_unlock(&ri->io_lock);
	return ret;
}

static inline int check_range(struct rt5746_regulator_info *info,
			      int min_uV, int max_uV)
{
	if (min_uV < info->min_uV || min_uV > info->max_uV)
		return -EINVAL;
	return 0;
}

static int rt5746_list_voltage(struct regulator_dev *rdev, unsigned index)
{
	struct rt5746_regulator_info *info = rdev_get_drvdata(rdev);

	return (index>=info->vol_output_size)? \
		 -EINVAL: \
		(info->min_uV+info->step_uV*index);
}

#if (LINUX_VERSION_CODE>=KERNEL_VERSION(2,6,38))
static int rt5746_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	struct rt5746_regulator_info *info = rdev_get_drvdata(rdev);
	unsigned char data;
	const int count = info->vol_output_size;
	if (selector>count)
		return -EINVAL;
	data = (unsigned char)selector;
	data <<= info->nvol_shift;
	return rt5746_assign_bits(info->i2c, info->nvol_reg,
			info->nvol_mask, data);
}

static int rt5746_get_voltage_sel(struct regulator_dev *rdev)
{
	struct rt5746_regulator_info *info = rdev_get_drvdata(rdev);
	int ret;
	ret = rt5746_reg_read(info->i2c, info->nvol_reg);
	if (ret<0)
		return ret;
	return (ret&info->nvol_mask)>>info->nvol_shift;
}
#else
static int rt5746_set_voltage(struct regulator_dev *rdev,
			       int min_uV, int max_uV, unsigned *selector)
{
	struct rt5746_regulator_info *info = rdev_get_drvdata(rdev);
	unsigned char data;
	if (check_range(info, min_uV, max_uV)) {
		dev_err(&rdev->dev, "invalid voltage range (%d, %d) uV\n",
			min_uV, max_uV);
		return -EINVAL;
	}
	data = DIV_ROUND_UP(min_uV-info->min_uV, info->step_uV);
	data <<= info->nvol_shift;
	*selector = data;
	return rt5746_assign_bits(info->i2c, info->nvol_reg,
			info->nvol_mask, data);
}


static int rt5746_get_voltage(struct regulator_dev *rdev)
{
	struct rt5746_regulator_info *info = rdev_get_drvdata(rdev);
	int ret;
	ret = rt5746_reg_read(info->i2c, info->nvol_reg);
	if (ret<0)
		return ret;
	ret =  (ret&info->nvol_mask)>>info->nvol_shift;
	return rt5746_list_voltage(rdev, ret);
}
#endif /* LINUX_VERSION_CODE>=KERNEL_VERSION(2,6,38) */

static int rt5746_set_suspend_voltage(struct regulator_dev *rdev, int uV)
{
	struct rt5746_regulator_info *info = rdev_get_drvdata(rdev);
	unsigned char data;
	if (check_range(info, uV, uV)) {
		dev_err(&rdev->dev, "invalid voltage range (%d, %d) uV\n",
                         uV, uV);
		return -EINVAL;
	}
	data = DIV_ROUND_UP(uV-info->min_uV, info->step_uV);
	data <<= info->svol_shift;
	return rt5746_assign_bits(info->i2c, info->svol_reg, info->svol_mask, data);
}

static struct regulator_ops rt5746_regulator_ops = {
	.list_voltage		= rt5746_list_voltage,
#if (LINUX_VERSION_CODE>=KERNEL_VERSION(2,6,38))
	.get_voltage_sel	= rt5746_get_voltage_sel,
	.set_voltage_sel	= rt5746_set_voltage_sel,
#else
	.set_voltage		= rt5746_set_voltage,
	.get_voltage		= rt5746_get_voltage,
#endif /* LINUX_VERSION_CODE>=KERNEL_VERSION(2,6,38) */
	.set_suspend_voltage	= rt5746_set_suspend_voltage,
};

#define RT5746_STEP_UV		6250
#define RT5746_VOUT_SIZE	128
static struct rt5746_regulator_info rt5746_regulator_info = {
	.desc = {
		.name = "rt5746-dcdc1",
		.n_voltages = RT5746_VOUT_SIZE,
		.ops = &rt5746_regulator_ops,
		.type = REGULATOR_VOLTAGE,
		.id = -1,
		.owner = THIS_MODULE,
	},
	.step_uV = RT5746_STEP_UV,
	.vol_output_size = RT5746_VOUT_SIZE,
	.min_uV = 600000,
	.max_uV = 1393750,
	.nvol_reg = RT5746_REG_BUCKVN,
	.nvol_shift = RT5746_BUCKVOUT_SHIFT,
	.nvol_mask = RT5746_BUCKVOUT_MASK,
	.svol_reg = RT5746_REG_BUCKVS,
	.svol_shift = RT5746_BUCKVOUT_SHIFT,
	.svol_mask = RT5746_BUCKVOUT_MASK,
};

static ssize_t rt_dbg_show_attrs(struct device *, struct device_attribute *, char *);
static ssize_t rt_dbg_store_attrs(struct device *, struct device_attribute *, const char *, \
		size_t count);

#define RT_DBG_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},		\
	.show = rt_dbg_show_attrs,			\
	.store = rt_dbg_store_attrs,			\
}

static struct device_attribute rt_dbg_attrs[] = {
	RT_DBG_ATTR(reg),
	RT_DBG_ATTR(data),
	RT_DBG_ATTR(regs),
};

enum {
	RT_REG = 0,
	RT_DATA,
	RT_REGS,
};

static ssize_t rt_dbg_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct rt5746_regulator_info *ri = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - rt_dbg_attrs;
	int i = 0;
	int j = 0;

	switch (offset) {
	case RT_REG:
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%02x\n",
				ri->reg_addr);
		break;
	case RT_DATA:
		ri->reg_data = rt5746_reg_read(ri->i2c, ri->reg_addr);
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%02x\n",
				ri->reg_data);
		dev_dbg(dev, "%s: (read) addr = 0x%x, data = 0x%x\n",
				__func__, ri->reg_addr, ri->reg_data);
		break;
	case RT_REGS:
		for (j = RT5746_REG_RANGE1START; j <= RT5746_REG_RANGE1END; j++)
			i += scnprintf(buf + i, PAGE_SIZE - i, "reg%02x 0x%02x\n",
					j, rt5746_reg_read(ri->i2c, j));
		for (j = RT5746_REG_RANGE2START; j <= RT5746_REG_RANGE2END; j++)
			i += scnprintf(buf + i, PAGE_SIZE - i, "reg%02x 0x%02x\n",
					j, rt5746_reg_read(ri->i2c, j));
		for (j = RT5746_REG_RANGE3START; j <= RT5746_REG_RANGE3END; j++)
			i += scnprintf(buf + i, PAGE_SIZE - i, "reg%02x 0x%02x\n",
					j, rt5746_reg_read(ri->i2c, j));

		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

static ssize_t rt_dbg_store_attrs(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct rt5746_regulator_info *ri = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - rt_dbg_attrs;
	int ret = 0;
	int x = 0;

	switch (offset) {
	case RT_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			if (x<RT5746_REG_MAX && \
			((x>=RT5746_REG_RANGE1START && x<=RT5746_REG_RANGE1END)|| \
			 (x>=RT5746_REG_RANGE2START && x<=RT5746_REG_RANGE2END)|| \
			 (x>=RT5746_REG_RANGE3START && x<=RT5746_REG_RANGE3END)))
			{
				ri->reg_addr = x;
				ret = count;
			}
			else
				ret = -EINVAL;
		}
		else
			ret = -EINVAL;
		break;
	case RT_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			rt5746_reg_write(ri->i2c, ri->reg_addr, x);
			ri->reg_data = x;
			dev_dbg(dev, "%s: (write) addr = 0x%x, data = 0x%x\n",
					__func__, ri->reg_addr, ri->reg_data);
			ret = count;
		}
		else
			ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}


static int rt_dbg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(rt_dbg_attrs); i++) {
		rc = device_create_file(dev, &rt_dbg_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &rt_dbg_attrs[i]);
create_attrs_succeed:
	return rc;
}

static void rt5746_parse_initconfig(
		struct rt5746_platform_data *pd)
{
	//rampup_sel
	rt5746_init_regval[2] &= (~RT5746_RAMPUP_MASK);
	rt5746_init_regval[2] |= (pd->rampup_sel<<RT5746_RAMPUP_SHIFT);
	//rampdn_sel
	rt5746_init_regval[4] &= (~RT5746_RAMPDN_MASK);
	rt5746_init_regval[4] |= (pd->rampdn_sel<<RT5746_RAMPDN_SHIFT);
	//ipeak_sel
	rt5746_init_regval[4] &= (~RT5746_IPEAK_MASK);
	rt5746_init_regval[4] |= (pd->ipeak_sel<<RT5746_IPEAK_SHIFT);
	//tpwth_sel
	rt5746_init_regval[4] &= (~RT5746_TPWTH_MASK);
	rt5746_init_regval[4] |= (pd->tpwth_sel<<RT5746_TPWTH_SHIFT);
	//rearm_en
	if (pd->rearm_en)
		rt5746_init_regval[4] |= RT5746_REARM_MASK;
	else
		rt5746_init_regval[4] &= (~RT5746_REARM_MASK);
	if (pd->discharge_en)
		rt5746_init_regval[1] |= RT5746_DISCHG_MASK;
	else
		rt5746_init_regval[1] &= (~RT5746_DISCHG_MASK);
}

static void  rt5746_register_init(struct i2c_client *i2c)
{
	int val;
	rt5746_reg_write(i2c, RT5746_REG_INTMSK, rt5746_init_regval[0]);
	rt5746_reg_block_write(i2c, RT5746_REG_PGOOD, 3, &rt5746_init_regval[1]);
	rt5746_reg_write(i2c, RT5746_REG_LIMCONF, rt5746_init_regval[4]);
	//clear interrupt ack
	val = rt5746_reg_read(i2c, RT5746_REG_INTACK);
	dev_info(&i2c->dev, "last interrupt status = 0x%02x\n", val);
}

static inline struct regulator_dev* rt5746_regulator_register(
	struct regulator_desc *regulator_desc, struct device *dev,
	struct regulator_init_data *init_data, void *driver_data)
{
#if (LINUX_VERSION_CODE>=KERNEL_VERSION(3,5,0))
    struct regulator_config config = {
        .dev = dev,
        .init_data = init_data,
        .driver_data = driver_data,
	.of_node = dev->of_node,
    };
    return regulator_register(&regulator_desc, &config);
#elif (LINUX_VERSION_CODE>=KERNEL_VERSION(3,0,0))
    return regulator_register(regulator_desc,dev,init_data,
		driver_data,dev->of_node);
#else
    return regulator_register(regulator_desc,dev,init_data,driver_data);
#endif /* LINUX_VERSION_CODE>=KERNEL_VERSION(3,5,0)) */
}

static irqreturn_t rt5746_irq_handler(int irqno, void *param)
{
	struct rt5746_regulator_info *ri = param;
	int regval;
	int i;

	if (ri->suspend)
	{
		schedule_delayed_work(&ri->irq_dwork, msecs_to_jiffies(50));
		goto fin_intr;
	}

	regval = rt5746_reg_read(ri->i2c, RT5746_REG_INTACK);
	if (regval<0)
		dev_err(ri->dev, "read irq event io fail\n");
	else {
		if (regval == 0)
			goto fin_intr;
		regval &= (~rt5746_init_regval[0]);
		for (i=0; i<RT5746_IRQ_MAX; i++) {
			if (regval&(1<<i)) {
				switch (i) {
				case RT5746_IRQ_PGOOD:
					dev_err(ri->dev, "pgood event occur\n");
					break;
				case RT5746_IRQ_IDCDC:
					dev_err(ri->dev, "dcdc over current\n");
					break;
				case RT5746_IRQ_UVLO:
					dev_err(ri->dev, "uvlo event occur\n");
					break;
				case RT5746_IRQ_TPREW:
					dev_info(ri->dev, "thermal pre-warning\n");
					break;
				case RT5746_IRQ_TWARN:
					dev_warn(ri->dev, "thermal warning\n");
					break;
				case RT5746_IRQ_TSD:
					dev_err(ri->dev, "thermal shutdown\n");
					break;
				default:
					dev_err(ri->dev, "unrecognized event\n");
					break;
				}
			}
		}
	}
fin_intr:
	return IRQ_HANDLED;
}

static void rt5746_irq_dwork_func(struct work_struct *work)
{
	struct rt5746_regulator_info *ri = container_of(work,
		struct rt5746_regulator_info, irq_dwork.work);
	rt5746_irq_handler(0, ri);
}

static void rt5746_interrupt_init(struct rt5746_regulator_info *ri)
{
	struct rt5746_platform_data *pdata = ri->dev->platform_data;
	int rc = (int)0, irq_no = (int)0;
	if (gpio_is_valid(pdata->irq_gpio)) {
		rc = gpio_request(pdata->irq_gpio, "rt5746_irq_gpio");
		if (rc<0) {
			dev_err(ri->dev, "gpio request failed\n");
			return;
		}
		gpio_direction_input(pdata->irq_gpio);
		irq_no = gpio_to_irq(pdata->irq_gpio);
		if (irq_no<0) {
			dev_err(ri->dev, "gpio to irq fail\n");
			gpio_free(pdata->irq_gpio);
			return;
		}
		if (devm_request_threaded_irq(ri->dev, irq_no, NULL,
			rt5746_irq_handler,
			IRQF_TRIGGER_FALLING|IRQF_NO_SUSPEND|IRQF_DISABLED,
			"rt5746_irq", ri)) {
			dev_err(ri->dev, "request irq fail\n");
			gpio_free(pdata->irq_gpio);
			return;
		}
		enable_irq_wake(irq_no);
		schedule_delayed_work(&ri->irq_dwork, msecs_to_jiffies(100));
	}
	else
		dev_info(ri->dev, "gpio is not valid\n");
}

static void rt_parse_dt(struct device *dev)
{
	#ifdef CONFIG_OF
	struct regulator_init_data *init_data = NULL;
	struct rt5746_platform_data *pdata = dev->platform_data;
	struct device_node *np = dev->of_node;
	int rc;
	u32 tmp;
	#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
	init_data = of_get_regulator_init_data(dev, dev->of_node);
	#else
	init_data = of_get_regulator_init_data(dev);
	#endif /* #if (LINUX_KERNEL_VERSION >= KERNEL_VERSION(3,3,0)) */

	pdata->regulator = init_data;
	rc = of_property_read_u32(np, "rt,standby_uV", &tmp);
	if (rc<0)
		dev_info(dev, "no standby voltage setting, use ic default\n");
	else
		pdata->standby_uV = tmp;

	rc = of_property_read_u32(np, "rt,rampup_sel", &tmp);
	if (rc)
		dev_info(dev, "no rampup_sel property, use ic default value\n");
	else
	{
		if (tmp>RT5746_RAMPUP_MAX)
			tmp = RT5746_RAMPUP_MAX;
		pdata->rampup_sel = tmp;
	}

	rc = of_property_read_u32(np, "rt,rampdn_sel", &tmp);
	if (rc)
		dev_info(dev, "no rampdn_sel property, use ic default value\n");
	else
	{
		if (tmp>RT5746_RAMPDN_MAX)
			tmp = RT5746_RAMPDN_MAX;
		pdata->rampdn_sel = tmp;
	}
	rc = of_property_read_u32(np, "rt,ipeak_sel", &tmp);
	if (rc)
		dev_info(dev, "no ipeak_sel property, use ic default value\n");
	else {
		if (tmp>RT5746_IPEAK_MAX)
			tmp = RT5746_IPEAK_MAX;
		pdata->ipeak_sel = tmp;
	}
	rc = of_property_read_u32(np, "rt,tpwth_sel", &tmp);
	if (rc)
		dev_info(dev, "no tpwth_sel property, use ic default value\n");
	else {
		if (tmp>RT5746_TPWTH_MAX)
			tmp = RT5746_TPWTH_MAX;
		pdata->tpwth_sel = tmp;
	}
	if (of_property_read_bool(np, "rt,rearm_en"))
		pdata->rearm_en = 1;
	if (of_property_read_bool(np, "rt,discharge_en"))
		pdata->discharge_en = 1;
	pdata->irq_gpio = of_get_named_gpio(np, "rt,irq_gpio", 0);
	#endif /* #ifdef CONFIG_OF */
}

static int rt5746_regulator_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct rt5746_regulator_info *ri;
	struct rt5746_platform_data *pdata = i2c->dev.platform_data;
	struct regulator_dev *rdev;
	struct regulator_init_data *init_data;
	bool use_dt = i2c->dev.of_node;
	int val = 0;
	int ret = 0;

	ri = &rt5746_regulator_info;
	if (use_dt) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto err_init;
		}
		i2c->dev.platform_data = pdata;
		rt_parse_dt(&i2c->dev);
	}
	else {
		if (!pdata) {
			ret = -EINVAL;
			goto err_init;
		}
	}

	init_data = pdata->regulator;
	if (!init_data) {
		dev_err(&i2c->dev, "no initializing data\n");
		ret = -EINVAL;
		goto err_init;
	}

	mutex_init(&ri->io_lock);
	INIT_DELAYED_WORK(&ri->irq_dwork, rt5746_irq_dwork_func);
	ri->i2c = i2c;
	ri->dev = &i2c->dev;
	i2c_set_clientdata(i2c, ri);
	//check ic communication & ic vendor id
	val = rt5746_reg_read(i2c, RT5746_REG_VID);
	if (val<0) {
		ret = -EIO;
		goto err_init;
	}
	else {
		dev_info(&i2c->dev, "vendor id = %02x\n", val);
		if (val!=RT5746_VEN_ID) {
			ret = -EINVAL;
			goto err_init;
		}
	}
	rt5746_parse_initconfig(pdata);
	rt5746_register_init(i2c);

	//interrupt gpio init
	if (pdata->irq_gpio<0)
		dev_info(&i2c->dev, "no interrupt irq specified\n");
	else
		rt5746_interrupt_init(ri);

	rdev = rt5746_regulator_register(&ri->desc, &i2c->dev,
				  init_data, ri);
	if (IS_ERR(rdev)) {
		dev_err(&i2c->dev, "failed to register regulator %s\n",
				ri->desc.name);
		return PTR_ERR(rdev);
	}
	ri->rdev = rdev;
	//set suspend voltage
	if (pdata->standby_uV>0)
		rt5746_set_suspend_voltage(rdev, pdata->standby_uV);
	rt_dbg_create_attrs(&i2c->dev);
	dev_info(&i2c->dev, "regulator successfully registered\n");
err_init:
	return ret;
}

static int rt5746_regulator_remove(struct i2c_client *i2c)
{
	struct rt5746_regulator_info *ri = i2c_get_clientdata(i2c);
	regulator_unregister(ri->rdev);
	dev_info(&i2c->dev, "regulator driver removed\n");
	return 0;
}

static int rt5746_regulator_suspend(struct i2c_client *i2c, pm_message_t mesg)
{
	struct rt5746_regulator_info *ri = i2c_get_clientdata(i2c);
	ri->suspend = 1;
	return 0;
}

static int rt5746_regulator_resume(struct i2c_client *i2c)
{
	struct rt5746_regulator_info *ri = i2c_get_clientdata(i2c);
	ri->suspend = 0;
	return 0;
}

static const struct of_device_id rt_match_table[] = {
	{ .compatible = "richtek,rt5746",},
	{},
};

static const struct i2c_device_id rt5746_id_table[] = {
	{ RT5746_DEV_NAME, 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, rt5746_id_table);

static struct i2c_driver rt5746_regulator_driver =
{
	.driver = {
		.name = RT5746_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = rt_match_table,
	},
	.probe = rt5746_regulator_probe,
	.remove = rt5746_regulator_remove,
	.suspend = rt5746_regulator_suspend,
	.resume = rt5746_regulator_resume,
	.id_table = rt5746_id_table,
};

static int __init rt5746_regulator_init(void)
{
	return i2c_add_driver(&rt5746_regulator_driver);
}
subsys_initcall(rt5746_regulator_init);

static void __exit rt5746_regulator_exit(void)
{
	i2c_del_driver(&rt5746_regulator_driver);
}
module_exit(rt5746_regulator_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CY Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("Regulator driver for RT5746");
MODULE_VERSION(RT5746_DRV_VER);
