/*
 *  drivers/power/rt9455.c
 *  rt9455.c
 *  Richtek RT9455 Switch Mode Charging IC Drivers
 *
 *  Copyright (C) 2013 Richtek Electronics
 *  cy_huang <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/err.h>

//for interrupt config and interrupt register
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>

#include <linux/gpio.h>
#include <linux/gpio-pxa.h>

#include <linux/power/rt9455.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/string.h>
#endif
#if defined(CONFIG_SPA)
#include <mach/spa.h>
static void (*spa_external_event)(int, int) = NULL;
#endif
#ifdef CONFIG_BATTERY_D2199
#include <linux/d2199/d2199_battery.h>
#endif
static int rt9455_is_ovp = 0;

//Internal rt9455_chip ptr for function call from other people
static struct rt9455_chip* internal_chip;

static inline int rt9455_read_device(struct i2c_client *i2c,
				      int reg, int bytes, void *dest)
{
	int ret;
	if (bytes > 1)
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, bytes, dest);
	else {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret < 0)
			return ret;
		*(unsigned char *)dest = (unsigned char)ret;
	}
	return ret;
}

static int rt9455_reg_read(struct i2c_client *i2c, int reg)
{
	struct rt9455_chip* chip = i2c_get_clientdata(i2c);
	int ret;
	RT_DBG("I2C Read (client : 0x%x) reg = 0x%x\n",
           (unsigned int)i2c,(unsigned int)reg);
	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&chip->io_lock);
	return ret;
}

static int rt9455_reg_write(struct i2c_client *i2c, int reg, unsigned char data)
{
	struct rt9455_chip* chip = i2c_get_clientdata(i2c);
	int ret;
	RT_DBG("I2C Write (client : 0x%x) reg = 0x%x, data = 0x%x\n",
           (unsigned int)i2c,(unsigned int)reg,(unsigned int)data);
	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, data);
	mutex_unlock(&chip->io_lock);
	return ret;
}

static int rt9455_assign_bits(struct i2c_client *i2c, int reg,
		unsigned char mask, unsigned char data)
{
	struct rt9455_chip *chip = i2c_get_clientdata(i2c);
	unsigned char value;
	int ret;
	mutex_lock(&chip->io_lock);
	ret = rt9455_read_device(i2c, reg, 1, &value);
	if (ret < 0)
		goto out;
	value &= ~mask;
	value |= (data&mask);
	ret = i2c_smbus_write_byte_data(i2c,reg,value);
out:
	mutex_unlock(&chip->io_lock);
	return ret;
}

static int rt9455_set_bits(struct i2c_client *i2c, int reg,
		unsigned char mask)
{
	return rt9455_assign_bits(i2c,reg,mask,mask);
}

static int rt9455_clr_bits(struct i2c_client *i2c, int reg,
		unsigned char mask)
{
	return rt9455_assign_bits(i2c,reg,mask,0);
}

#ifdef CONFIG_DEBUG_FS
static struct i2c_client *this_client;
static struct dentry *debugfs_rt_dent;
static struct dentry *debugfs_peek;
static struct dentry *debugfs_poke;

static unsigned char read_data;

static int rt9455_debug_open(struct inode *inode, struct file *file)
{
	RT_DBG("\n");
	file->private_data = inode->i_private;
	return 0;
}

static int get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	RT_DBG("\n");
	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (strict_strtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
			}
		else
			return -EINVAL;
	}
	return 0;
}

static ssize_t rt9455_debug_read(struct file *filp, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char lbuf[10];

	RT_DBG("\n");
	snprintf(lbuf, sizeof(lbuf), "0x%02x\n", read_data);
	return simple_read_from_buffer(ubuf, count, ppos, lbuf, strlen(lbuf));
}

static ssize_t rt9455_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char *access_str = filp->private_data;
	char lbuf[32];
	int rc;
	long int param[5];

	RT_DBG("\n");
	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';

	if (!strcmp(access_str, "poke")) {
		/* write */
		rc = get_parameters(lbuf, param, 2);
		if ((param[0] <= 0xFF) && (param[1] <= 0xFF) && (rc == 0))
		{
			rt9455_reg_write(this_client, param[0], (unsigned char)param[1]);
		}
		else
			rc = -EINVAL;
	} else if (!strcmp(access_str, "peek")) {
		/* read */
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] <= 0xFF) && (rc == 0))
		{
			read_data = rt9455_reg_read(this_client, param[0]);
		}
		else
			rc = -EINVAL;
	}

	if (rc == 0)
		rc = cnt;

	return rc;
}

static const struct file_operations rt9455_debug_ops = {
	.open = rt9455_debug_open,
	.write = rt9455_debug_write,
	.read = rt9455_debug_read
};
#endif /* CONFIG_DEBUG_FS */

/* Function name : rt9455_ext_force_otamode
 * Function parameter : (onoff  1: on  0: off)
 * Function return value : (integer < 0 fail)
 * Function description:
 *	Let the user force enable OTA mode
 *	whenever the external pin mode is used or not
 */
int rt9455_ext_force_otamode(int onoff)
{
	int ret = 0;
	struct rt9455_chip *chip = internal_chip;

	if (onoff)
		ret = rt9455_set_bits(chip->i2c, RT9455_REG_CTRL2, RT9455_OPAMODE_MASK);
	else
		ret = rt9455_clr_bits(chip->i2c, RT9455_REG_CTRL2, RT9455_OPAMODE_MASK);

	return ret;
}
EXPORT_SYMBOL(rt9455_ext_force_otamode);

static void rt9455_set_input_current_limit(struct i2c_client *i2c, int current_limit)
{
	u8 data;

	RT_DBG("current_limit = %d\n", current_limit);
	#if 0
	if (current_limit <= 100)
		data = 0;
	else if (current_limit > 100 && current_limit <= 500)
		data = 0x1;
	else if (current_limit > 500 && current_limit <= 1050)
		data = 0x2;
	else
		data = 0x3;
	#else
	//always set AICR to no limit (workaround)
	data = 0x3;
	#endif

	rt9455_assign_bits(i2c, RT9455_REG_CTRL2, RT9455_AICR_LIMIT_MASK,
		data << RT9455_AICR_LIMIT_SHIFT);
}

static void rt9455_set_fast_charging_current(struct i2c_client *i2c, int charging_current)
{
	u8 data;

	RT_DBG("charging_current = %d\n", charging_current);
	if (charging_current < 500)
		data = 0;
	else if (charging_current >= 500 && charging_current <= 1550)
	{
		data = (charging_current-500)/150;
	}
	else
		data = 0x7;

	rt9455_assign_bits(i2c, RT9455_REG_CTRL6, RT9455_ICHRG_MASK,
		data << RT9455_ICHRG_SHIFT);
}

/* Function name : rt9455_ext_charger_current
 * Function parameter : (curr_value: xxx mA:unit)
 * Function return value : (integer < 0 fail)
 * Function description:
 *	Let the user set the charging current
 *      This function will set AICR & ICHG, both.
 *	Use this function, just send the current_value you want
 */
int rt9455_ext_charger_current(int curr_value)
{
	int ret = 0;
	struct rt9455_chip *chip = internal_chip;
	chip->current_value = curr_value;

	//anyway always calcen
	cancel_delayed_work_sync(&chip->delayed_work);

	wake_lock_timeout(&chip->wake_lock, msecs_to_jiffies(300));
	queue_delayed_work(chip->wq, &chip->delayed_work, msecs_to_jiffies(250));

	return ret;
}
EXPORT_SYMBOL(rt9455_ext_charger_current);

static void rt9455_ieoc_mask_enable(struct i2c_client *client, int onoff)
{
	RT_DBG("onoff = %d\n", onoff);
	if (onoff)
	{
		rt9455_set_bits(client, RT9455_REG_MASK2, RT9455_EVENT_CHTERMI);
	}
	else
	{
		rt9455_clr_bits(client, RT9455_REG_MASK2, RT9455_EVENT_CHTERMI);
	}
}

static void rt9455_charger_switch(struct i2c_client *client, int onoff)
{
	RT_DBG("onoff = %d\n", onoff);
	if (onoff)
	{
			rt9455_set_bits(client, RT9455_REG_CTRL7, RT9455_CHGEN_MASK);
	}
		else
	{
			rt9455_clr_bits(client, RT9455_REG_CTRL7, RT9455_CHGEN_MASK);
	}
}

/* Function name : rt9455_ext_charger_switch
 * Function parameter : (onoff  1: on  0: off)
 * Function return value : (integer < 0 fail)
 * Function description:
 *	Let the user set the charge buck switch
 */
int rt9455_ext_charger_switch(int onoff)
{
	struct rt9455_chip *chip = internal_chip;
	rt9455_charger_switch(chip->i2c, onoff);
	rt9455_ieoc_mask_enable(chip->i2c, !onoff);
	return 0;
}
EXPORT_SYMBOL(rt9455_ext_charger_switch);

static void rt9455_work_func(struct work_struct *work)
{
	struct rt9455_chip *chip = (struct rt9455_chip *)container_of(work, struct rt9455_chip, work);
	unsigned char irq_stat[3] = {0};
	RT_DBG("\n");
	// Continuously read three IRQ status register from the chip
	if (rt9455_read_device(chip->i2c, RT9455_REG_IRQ1, 3, irq_stat) >= 0)
	{
		RT_DBG("irq1->%02x, irq2->%02x, irq3->%02x\n", irq_stat[0], irq_stat[1], irq_stat[2]);
		RT_DBG("stat value = %02x\n", rt9455_reg_read(chip->i2c, RT9455_REG_CTRL1));
		if (irq_stat[0])
		{
			if (irq_stat[0] & RT9455_EVENT_TSDI || irq_stat[0] & RT9455_EVENT_VINOVPI)
			{
				rt9455_charger_switch(chip->i2c, 0);
			}

			if (chip->cb->general_callback)
				chip->cb->general_callback(irq_stat[0]);
		}

		if (irq_stat[1])
		{
			if (irq_stat[1] & RT9455_EVENT_CHBATOVI)
			{
				rt9455_charger_switch(chip->i2c, 0);
			}

			if (irq_stat[1] & RT9455_EVENT_CHTERMI)
			{
				//when charger termation occurs, mask
				//CHTERMI interrupt
				rt9455_ieoc_mask_enable(chip->i2c, 1);
			}

			if (irq_stat[1] & RT9455_EVENT_CHRCHGI)
			{
				//when recharger request occurs, unmask
				//CHTERMI interrupt
				rt9455_ieoc_mask_enable(chip->i2c, 0);
			}

			if (chip->cb->charger_callback)
				chip->cb->charger_callback(irq_stat[1]);
		}

		if (irq_stat[2])
		{
			if (chip->cb->boost_callback)
			{
				chip->cb->boost_callback(irq_stat[2]);
			}
		}
	}
	else
		dev_err(chip->dev, "read irq stat io fail\n");

}

static irqreturn_t rt9455_interrupt(int irqno, void *param)
{
	struct rt9455_chip *chip = (struct rt9455_chip *)param;
	queue_work(chip->wq, &chip->work);
	return IRQ_HANDLED;
}

static void rt9455_cc_work_func(struct work_struct *work)
{
	struct delayed_work *dw = (struct delayed_work *)container_of(work, struct delayed_work, work);
	struct rt9455_chip *chip = (struct rt9455_chip *)container_of(dw, struct rt9455_chip, delayed_work);

	rt9455_set_input_current_limit(chip->i2c, chip->current_value);
	rt9455_set_fast_charging_current(chip->i2c, chip->current_value);
}

static int rt9455_chip_reset(struct i2c_client *client)
{
	struct rt9455_chip *chip = i2c_get_clientdata(client);
	int ret;

	RT_DBG("\n");
	//Before the IC reset, inform the customer
	if (chip->cb)
		chip->cb->reset_callback(RT9455_EVENT_BEFORE_RST);

	ret = rt9455_set_bits(client, RT9455_REG_CTRL4, RT9455_RST_MASK);
	if (ret < 0)
		return -EIO;

	if (chip->cb)
		chip->cb->reset_callback(RT9455_EVENT_AFTER_RST);


	return 0;
}

static int rt9455_chip_reg_init(struct i2c_client *client)
{
	struct rt9455_platform_data *pdata = client->dev.platform_data;

	RT_DBG("\n");
	//ctrl value
	RT_DBG("write_init_ctrlval = %d\n", pdata->write_init_ctrlval);
	if (pdata->write_init_ctrlval)
	{
		rt9455_reg_write(client, RT9455_REG_CTRL2, pdata->init_ctrlval.ctrl2.val);
		rt9455_reg_write(client, RT9455_REG_CTRL3, pdata->init_ctrlval.ctrl3.val);
		rt9455_reg_write(client, RT9455_REG_CTRL5, pdata->init_ctrlval.ctrl5.val);
		rt9455_reg_write(client, RT9455_REG_CTRL6, pdata->init_ctrlval.ctrl6.val);
		rt9455_reg_write(client, RT9455_REG_CTRL7, pdata->init_ctrlval.ctrl7.val);
	}
	// irq mask value
	RT_DBG("write_irq_mask = %d\n", pdata->write_irq_mask);
	if (pdata->write_irq_mask)
	{
		rt9455_reg_write(client, RT9455_REG_MASK1, pdata->irq_mask.mask1.val);
		rt9455_reg_write(client, RT9455_REG_MASK2, pdata->irq_mask.mask2.val);
		rt9455_reg_write(client, RT9455_REG_MASK3, pdata->irq_mask.mask3.val);
	}
	return 0;
}

static int rt9455_init_interrupt(struct i2c_client *client)
{
	struct rt9455_chip *chip = i2c_get_clientdata(client);
	int ret = 0;
	RT_DBG("\n");
	chip->wq = create_workqueue("rt9455_wq");
	INIT_WORK(&chip->work, rt9455_work_func);
	// init gpio pin
	if (gpio_is_valid(chip->intr_gpio))
	{
		ret = gpio_request(chip->intr_gpio, "rt9455_intr");
		if (ret)
			return ret;

		ret = gpio_direction_input(chip->intr_gpio);
		if (ret)
			return ret;

		if (request_irq(chip->irq, rt9455_interrupt, IRQ_TYPE_EDGE_FALLING|IRQF_DISABLED, "RT9455_IRQ", chip))
		{
			dev_err(chip->dev, "could not allocate IRQ_NO(%d) !\n", chip->irq);
			return -EBUSY;
		}
		enable_irq_wake(chip->irq);

		if (!gpio_get_value(chip->intr_gpio))
		{
			disable_irq_nosync(chip->irq);
			queue_work(chip->wq, &chip->work);
		}
	}
	else
		return -EINVAL;
	return ret;
}

#if defined(CONFIG_SPA)
static void rt9455_enable_charge(enum power_supply_type type)
{
	int ret = 0;
	int chg_current;

	printk("%s\n", __func__);

	if(type == POWER_SUPPLY_TYPE_MAINS)
	{
		chg_current = 650;
#ifdef CONFIG_BATTERY_D2199
		d2199_battery_write_status(D2199_BATTERY_CHG_CURRENT, 1050);
#endif
	}
	else
	{
		chg_current = 500;
#ifdef CONFIG_BATTERY_D2199
		 d2199_battery_write_status(D2199_BATTERY_CHG_CURRENT, 550);
#endif
	}

	rt9455_ext_charger_switch(1);
	ret = rt9455_ext_charger_current(chg_current);
	if(ret < 0)
	{
		printk("  %s bq24157_set_charge_current failed %d\n", __func__, ret);
	}

	return;
}

static void rt9455_disable_charge(unsigned char end_of_charge)
{
	printk("%s\n", __func__);

	switch(end_of_charge)
	{
		case SPA_END_OF_CHARGE_NONE:
			rt9455_is_ovp = 0;
			rt9455_ext_charger_switch(0);
#ifdef CONFIG_BATTERY_D2199
			d2199_battery_write_status(D2199_BATTERY_CHG_CURRENT, 0);
#endif
			break;
		case SPA_END_OF_CHARGE_BY_FULL:
		case SPA_END_OF_CHARGE_BY_TEMPERATURE:
		case SPA_END_OF_CHARGE_BY_TIMER:
		case SPA_END_OF_CHARGE_BY_VF_OPEN:
		case SPA_END_OF_CHARGE_BY_QUICKSTART:
			rt9455_ext_charger_switch(0);
#ifdef CONFIG_BATTERY_D2199
			d2199_battery_write_status(D2199_BATTERY_CHG_CURRENT, 0);
#endif
			break;
		case SPA_END_OF_CHARGE_BY_OVP:
			rt9455_is_ovp = 1;
			break;
	}

	return;
}
#endif

static int __devinit rt9455_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct rt9455_chip *chip;
	struct rt9455_platform_data *pdata = client->dev.platform_data;
	int ret = 0;

	RT_DBG("\n");
	if (!pdata)
		return -EINVAL;

	chip =kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->i2c = client;
	chip->dev = &client->dev;
#if 0
	chip->intr_gpio = pdata->intr_gpio;
	chip->irq = gpio_to_irq(pdata->intr_gpio);
#else
	chip->intr_gpio = pxa_irq_to_gpio(client->irq);
	chip->irq = client->irq;
#endif
	chip->otg_ext_enable = (pdata->init_ctrlval.ctrl3.bitfield.OTG_EN ? 1 : 0);
	chip->cb = &pdata->callbacks;
	mutex_init(&chip->io_lock);
	wake_lock_init(&chip->wake_lock, WAKE_LOCK_SUSPEND, "rt9455_cc_wakeock");
	INIT_DELAYED_WORK(&chip->delayed_work, rt9455_cc_work_func);

	i2c_set_clientdata(client, chip);
	//Internal chip ptr for function call
	internal_chip = chip;

	rt9455_chip_reset(client);
	rt9455_chip_reg_init(client);

	//Config intr gpio and register interrupt
	ret = rt9455_init_interrupt(client);
	if (ret<0) {
		dev_err(chip->dev, "register interrupt fail\n");
		goto irq_fail;
	}

#if defined(CONFIG_SPA)
	spa_external_event = spa_get_external_event_handler();

	ret = spa_chg_register_enable_charge(rt9455_enable_charge);
	if(ret)
	{
	    printk("%s fail to register enable_charge function\n", __func__);
	}
	ret = spa_chg_register_disable_charge(rt9455_disable_charge);
	if(ret)
	{
	    printk("%s fail to register disable_charge function\n", __func__);
	}
	spa_external_event = spa_get_external_event_handler();
#endif

	#ifdef CONFIG_DEBUG_FS
	RT_DBG("%s: add debugfs for RT9455 debug\n", __func__);
	this_client = client;
	debugfs_rt_dent = debugfs_create_dir("rt9455_dbg", 0);
	if (!IS_ERR(debugfs_rt_dent)) {
		debugfs_peek = debugfs_create_file("peek",
		S_IFREG | S_IRUGO, debugfs_rt_dent,
		(void *) "peek", &rt9455_debug_ops);

		debugfs_poke = debugfs_create_file("poke",
		S_IFREG | S_IRUGO, debugfs_rt_dent,
		(void *) "poke", &rt9455_debug_ops);
	}
	#endif

	pr_info("RT9455 Driver successfully loaded\n");
	return ret;
irq_fail:
	kfree(chip);
	return -EINVAL;
}

static int __devexit rt9455_i2c_remove(struct i2c_client *client)
{
	struct rt9455_chip *chip = i2c_get_clientdata(client);
	if (chip->irq)
		free_irq(chip->irq, chip);
	if (chip->wq)
	{
		cancel_delayed_work_sync(&chip->delayed_work);
		cancel_work_sync(&chip->work);
		flush_workqueue(chip->wq);
		destroy_workqueue(chip->wq);
	}

	wake_lock_destroy(&chip->wake_lock);

	if (!IS_ERR(debugfs_rt_dent))
		debugfs_remove_recursive(debugfs_rt_dent);

	kfree(chip);
	return 0;
}

static void rt9455_i2c_shutdown(struct i2c_client *client)
{
	RT_DBG("\n");
	rt9455_chip_reset(client);
}

#ifdef CONFIG_PM
static int rt9455_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct rt9455_chip *chip = i2c_get_clientdata(client);
	RT_DBG("\n");
	chip->suspend = 1;
	return 0;
}

static int rt9455_i2c_resume(struct i2c_client *client)
{
	struct rt9455_chip *chip = i2c_get_clientdata(client);
	RT_DBG("\n");
	chip->suspend = 0;
	return 0;
}
#else
#define rt9455_i2c_suspend NULL
#define rt9455_i2c_resume NULL
#endif  /* CONFIG_PM */


static const struct i2c_device_id rt9455_i2c_id[] =
{
	{RT9455_DEVICE_NAME, 0},
	{}
};

static struct i2c_driver rt9455_i2c_driver =
{
	.driver = {
		.name = RT9455_DEVICE_NAME,
		.owner = THIS_MODULE,
	},
	.probe = rt9455_i2c_probe,
	.remove = __devexit_p(rt9455_i2c_remove),
	.shutdown = rt9455_i2c_shutdown,
	.suspend = rt9455_i2c_suspend,
	.resume = rt9455_i2c_resume,
	.id_table = rt9455_i2c_id,
};

static int __init rt9455_init(void)
{
	return i2c_add_driver(&rt9455_i2c_driver);
}

static void __exit rt9455_exit(void)
{
	i2c_del_driver(&rt9455_i2c_driver);
}

module_init(rt9455_init);
module_exit(rt9455_exit);

MODULE_AUTHOR("cy_huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("RT9455 Switch Mode Driver");
MODULE_LICENSE("GPL");
