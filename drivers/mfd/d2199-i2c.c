/*
 * d2199-i2c.c: I2C (Serial Communication) driver for D2199
 *   
 * Copyright(c) 2013 Dialog Semiconductor Ltd.
 *  
 * Author: Dialog Semiconductor Ltd. D. Chen, D. Patel
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/d2199/pmic.h> 
#include <linux/d2199/d2199_reg.h> 
#include <linux/d2199/hwmon.h>
#include <linux/d2199/rtc.h>
#include <linux/d2199/core.h>

static int d2199_i2c_read_device(struct d2199 *d2199, char reg,
					int bytes, void *dest)
{
	int ret;
	struct i2c_msg msgs[2];
	struct i2c_adapter *adap = d2199->pmic_i2c_client->adapter;

	msgs[0].addr = d2199->pmic_i2c_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;

	msgs[1].addr = d2199->pmic_i2c_client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = bytes;
	msgs[1].buf = (char *)dest;

	mutex_lock(&d2199->i2c_mutex);
#if 0	// TEST only
	ret = adap->algo->master_xfer(adap, msgs, ARRAY_SIZE(msgs));
#else
	ret = i2c_transfer(adap,msgs,ARRAY_SIZE(msgs));
#endif

	mutex_unlock(&d2199->i2c_mutex);

	if (ret < 0 )
		return ret;
	else if (ret == ARRAY_SIZE(msgs))
		return 0;
	else
		return -EFAULT;
}

static int d2199_i2c_write_device(struct d2199 *d2199, char reg,
				   int bytes, u8 *src )
{
	int ret;
	struct i2c_msg msgs[1];
	u8 data[12];
	u8 *buf = data;
	
	struct i2c_adapter *adap = d2199->pmic_i2c_client->adapter;

	if (bytes == 0)
		return -EINVAL;

	BUG_ON(bytes >= ARRAY_SIZE(data));

	msgs[0].addr = d2199->pmic_i2c_client->addr;
	msgs[0].flags = d2199->pmic_i2c_client->flags & I2C_M_TEN;
	msgs[0].len = 1+bytes;
	msgs[0].buf = data;

	*buf++ = reg;
	while(bytes--) *buf++ = *src++;

	mutex_lock(&d2199->i2c_mutex);
#if 0	// TEST only
	ret = adap->algo->master_xfer(adap, msgs, ARRAY_SIZE(msgs));
#else
	ret = i2c_transfer(adap,msgs,ARRAY_SIZE(msgs));
#endif
	mutex_unlock(&d2199->i2c_mutex);

	if (ret < 0 )
		return ret;
	else if (ret == ARRAY_SIZE(msgs))
		return 0;
	else
		return -EFAULT;
}


static int d2199_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct d2199 *d2199;
	int ret = 0;

	dlg_info("%s() Starting I2C\n", __FUNCTION__);

	d2199 = kzalloc(sizeof(struct d2199), GFP_KERNEL);
	if (d2199 == NULL) {
		kfree(i2c);
		return -ENOMEM;
	}

	i2c_set_clientdata(i2c, d2199);
	d2199->dev = &i2c->dev;
	d2199->pmic_i2c_client = i2c;
	
	mutex_init(&d2199->i2c_mutex);

	d2199->read_dev = d2199_i2c_read_device;
	d2199->write_dev = d2199_i2c_write_device;

	ret = d2199_device_init(d2199, i2c->irq, i2c->dev.platform_data);
	dev_info(d2199->dev, "I2C initialized err=%d\n",ret);
	if (ret < 0)
		goto err;


	dlg_info("%s() Finished I2C setup\n",__FUNCTION__);
	return ret;

err:
	kfree(d2199);
	return ret;
}

static int d2199_i2c_remove(struct i2c_client *i2c)
{
	struct d2199 *d2199 = i2c_get_clientdata(i2c);

	d2199_device_exit(d2199);
	kfree(d2199);
	return 0;
}


static const struct i2c_device_id d2199_i2c_id[] = {
	{ D2199_I2C, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, d2199_i2c_id);


static struct i2c_driver d2199_i2c_driver = {
	.driver = {
		   .name = D2199_I2C,
		   .owner = THIS_MODULE,
	},
	.probe = d2199_i2c_probe,
	.remove = d2199_i2c_remove,
	.id_table = d2199_i2c_id,
};

static int __init d2199_i2c_init(void)
{
	return i2c_add_driver(&d2199_i2c_driver);
}

/* Initialised very early during bootup (in parallel with Subsystem init) */
subsys_initcall(d2199_i2c_init);
//module_init(d2199_i2c_init);

static void __exit d2199_i2c_exit(void)
{
	i2c_del_driver(&d2199_i2c_driver);
}
module_exit(d2199_i2c_exit);

MODULE_AUTHOR("Dialog Semiconductor Ltd < william.seo@diasemi.com >");
MODULE_DESCRIPTION("I2C MFD driver for Dialog D2199 PMIC plus Audio");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" D2199_I2C);
