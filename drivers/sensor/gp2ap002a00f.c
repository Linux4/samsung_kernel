/*
 * Copyright (C) 2011 Sony Ericsson Mobile Communications Inc.
 *
 * Author: Courtney Cavin <courtney.cavin@sonyericsson.com>
 * Prepared for up-stream by: Oskar Andero <oskar.andero@sonyericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/gp2ap002a00f.h>
#include <linux/sensors_core.h>

static struct device *proximity_sensor_device = NULL;

#define OFFSET_FILE_PATH	"/efs/prox_cal"


#define PROX_NONDETECT	0x2F
#define PROX_DETECT		0x0C

#define PROX_NONDETECT_MODE1	0x43
#define PROX_DETECT_MODE1		0x28

#define PROX_NONDETECT_MODE2	0x48
#define PROX_DETECT_MODE2		0x42

struct gp2a_data {
	struct input_dev *input;
	const struct gp2a_platform_data *pdata;
	struct i2c_client *i2c_client;
	int proximity_enabled;
	int irq;
	int value;

        char cal_mode;

};

static int nondetect;
static int detect;


enum gp2a_addr {
	GP2A_ADDR_PROX	= 0x0,
	GP2A_ADDR_GAIN	= 0x1,
	GP2A_ADDR_HYS	= 0x2,
	GP2A_ADDR_CYCLE	= 0x3,
	GP2A_ADDR_OPMOD	= 0x4,
	GP2A_ADDR_CON	= 0x6
};

enum gp2a_controls {
	/* Software Shutdown control: 0 = shutdown, 1 = normal operation */
	GP2A_CTRL_SSD	= 0x01
};

static int gp2a_report(struct gp2a_data *dt)
{
	int vo = gpio_get_value(dt->pdata->vout_gpio);

	dt->value = vo;

	input_report_switch(dt->input, SW_FRONT_PROXIMITY, vo);
	input_sync(dt->input);

	return 0;
}

static irqreturn_t gp2a_irq(int irq, void *handle)
{
	struct gp2a_data *dt = handle;
	
	printk(KERN_EMERG"Proximity Interrupts ::%d\n",irq);
	
	gp2a_report(dt);

	return IRQ_HANDLED;
}

static int gp2a_enable(struct gp2a_data *dt)
{
	return i2c_smbus_write_byte_data(dt->i2c_client, GP2A_ADDR_OPMOD,
					 GP2A_CTRL_SSD);
}

static int gp2a_disable(struct gp2a_data *dt)
{
	return i2c_smbus_write_byte_data(dt->i2c_client, GP2A_ADDR_OPMOD,
					 0x00);
}


/*Only One Read Only register, so word address need not be specified (from Data Sheet)*/
static int gp2a_i2c_read(u8 reg, u8 *value,struct gp2a_data *gp2a_cal_data)
{
	int ret =0;
	int count=0;
	u8 buf[3];
	struct i2c_msg msg[1];

	buf[0] = reg;
	
	/*first byte read(buf[0]) is dummy read*/
	msg[0].addr = gp2a_cal_data->i2c_client->addr;
	msg[0].flags = I2C_M_RD;	
	msg[0].len = 2;
	msg[0].buf = buf;
	count = i2c_transfer(gp2a_cal_data->i2c_client->adapter, msg, 1);
	
	if(count < 0)
	{
		ret =-1;
	}
	else
	{
		*value = buf[0] << 8 | buf[1];
	}
	
	return ret;	
}

static int gp2a_i2c_write( u8 reg, u8 *value,struct gp2a_data *gp2a_cal_data)
{
    int ret =0;
    int count=0;
    struct i2c_msg msg[1];
    u8 data[2];

	printk(KERN_INFO "[GP2A] %s : start \n", __func__);

    if( (gp2a_cal_data->i2c_client == NULL) || (!gp2a_cal_data->i2c_client->adapter) ){
        return -ENODEV;
    }	

    data[0] = reg;
    data[1] = *value;

    msg[0].addr = gp2a_cal_data->i2c_client->addr;
    msg[0].flags = 0;
    msg[0].len = 2;
    msg[0].buf 	= data;
    count = i2c_transfer(gp2a_cal_data->i2c_client->adapter,msg,1);
	
    if(count < 0)
        ret =-1;
		
		
	printk(KERN_INFO "[GP2A] %s : stop \n", __func__);
	
    return ret;
}

static ssize_t proximity_enable_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct gp2a_data *dt = dev_get_drvdata(dev);
	
        pr_info("%s, %d \n", __func__, __LINE__);
	return snprintf(buf, PAGE_SIZE, "%d\n", dt->proximity_enabled);
}

static ssize_t proximity_value_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct gp2a_data *dt = dev_get_drvdata(dev);
	
    pr_info("%s, %d \n", __func__, __LINE__);
	
	return snprintf(buf, PAGE_SIZE, "%d\n", dt->value);
}

static ssize_t proximity_enable_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
	struct gp2a_data *dt = dev_get_drvdata(dev);
	 
	unsigned long value = 0;
        int err = 0;

        err = strict_strtoul(buf, 10, &value);
        if (err)
                pr_err("%s, kstrtoint failed.", __func__);

        if (value != 0 && value != 1)
                return count;
		
		printk(KERN_ALERT"%s :: Request for State(%d->%d)\n",__func__,dt->proximity_enabled,value);
		
	//check for multiple times disable and enable sensor
        if (dt->proximity_enabled && !value) {        /* Prox power off */

                disable_irq(dt->irq);

                gp2a_disable(dt);

				if (dt->pdata->hw_pwr)
						dt->pdata->hw_pwr(false);
					
				dt->proximity_enabled = 0;
        }
        if (!dt->proximity_enabled && value) {  		/* prox power on */
		
				if (dt->pdata->hw_pwr)
						dt->pdata->hw_pwr(true);

                msleep(20);

                gp2a_enable(dt);
		
				enable_irq(dt->irq);
				dt->proximity_enabled = 1;

				gp2a_report(dt);
		}

	return count;
}

static int __init gp2a_initialize(struct gp2a_data *dt)
{
	int error;

	error = i2c_smbus_write_byte_data(dt->i2c_client, GP2A_ADDR_GAIN,
					  0x08);
	if (error < 0)
		return error;

	error = i2c_smbus_write_byte_data(dt->i2c_client, GP2A_ADDR_HYS,
					  0xc2);
	if (error < 0)
		return error;

	error = i2c_smbus_write_byte_data(dt->i2c_client, GP2A_ADDR_CYCLE,
					  0x04);
	if (error < 0)
		return error;

	error = gp2a_disable(dt);

	return error;
}

static int gp2a_prox_offset(unsigned char vout,struct gp2a_data *gp2a_cal_data)
{	
    	u8 reg_value;
	int ret=0;

	printk(KERN_INFO "[GP2A] %s called\n", __func__);

	/* Write HYS Register */
	if(!vout)
		reg_value = nondetect;
	else
		reg_value = detect;
    
	if((ret=gp2a_i2c_write(GP2A_ADDR_HYS/*0x02*/,&reg_value,gp2a_cal_data))<0)
			printk(KERN_INFO "[GP2A]gp2a_i2c_write 2 failed\n", __func__);  
	
	return ret;
}

static int gp2a_prox_cal_mode(char mode,struct gp2a_data *gp2a_cal_data)
{
	int ret=0;
	u8 reg_value;

	printk(KERN_INFO "[GP2A] %s : start \n", __func__);

	if (mode == 1) 
        {
		nondetect = PROX_NONDETECT_MODE1;
		detect = PROX_DETECT_MODE1;
	} 
        else if (mode == 2) 
        {
		nondetect = PROX_NONDETECT_MODE2;
		detect = PROX_DETECT_MODE2;
	} 
        else
        {
		nondetect = PROX_NONDETECT;
		detect = PROX_DETECT;
	} 
    
	
	reg_value = 0x08;
	if((ret=gp2a_i2c_write(GP2A_ADDR_GAIN/*0x01*/,&reg_value,gp2a_cal_data))<0)
			printk(KERN_INFO "[GP2A]gp2a_i2c_write 1 failed\n", __func__);  

        gp2a_prox_offset(0,gp2a_cal_data);
        
	reg_value = 0x04;
	if((ret=gp2a_i2c_write(GP2A_ADDR_CYCLE/*0x03*/,&reg_value,gp2a_cal_data))<0)
				printk(KERN_INFO "[GP2A]gp2a_i2c_write 2 failed\n", __func__);  

	reg_value = 0x03;
	if((ret=gp2a_i2c_write(GP2A_ADDR_OPMOD,&reg_value,gp2a_cal_data))<0)
			printk(KERN_INFO "[GP2A]gp2a_i2c_write 3 failed\n", __func__);  

	reg_value = 0x00;
	if((ret=gp2a_i2c_write(GP2A_ADDR_CON,&reg_value,gp2a_cal_data))<0)
			printk(KERN_INFO "[GP2A]gp2a_i2c_write 4 failed\n", __func__);  

	printk(KERN_INFO "[GP2A] %s : stop \n", __func__);
	
        return ret;

}

static int gp2a_cal_mode_read_file(char *mode,struct gp2a_data *gp2a_cal_data)
{
	int err = 0;
	mm_segment_t old_fs;
	struct file *cal_mode_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_mode_filp = filp_open(OFFSET_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_mode_filp)) {
		printk(KERN_INFO "[GP2A] %s: no cal_mode file\n", __func__);        
		err = PTR_ERR(cal_mode_filp);
		if (err != -ENOENT)
			pr_err("[GP2A] %s: Can't open cal_mode file\n", __func__);
		set_fs(old_fs);
		return err;
	}
	err = cal_mode_filp->f_op->read(cal_mode_filp, mode, sizeof(u8), &cal_mode_filp->f_pos);

	if (err != sizeof(u8)) {
		pr_err("%s: Can't read the cal_mode from file\n",	__func__);
		filp_close(cal_mode_filp, current->files);
		set_fs(old_fs);
		return -EIO;
	}
               
	filp_close(cal_mode_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int gp2a_cal_mode_save_file(char mode,struct gp2a_data *gp2a_cal_data)
{
	struct file *cal_mode_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;
	
	printk(KERN_INFO "[GP2A] %s : start \n", __func__);

        gp2a_prox_cal_mode(mode,gp2a_cal_data);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_mode_filp = filp_open(OFFSET_FILE_PATH, O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0666);
	if (IS_ERR(cal_mode_filp)) 
        {
		pr_err("%s: Can't open cal_mode file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_mode_filp);
		pr_err("%s: err = %d\n", __func__, err);
		return err;
	}

	err = cal_mode_filp->f_op->write(cal_mode_filp, (char *)&mode, sizeof(u8), &cal_mode_filp->f_pos);
	if (err != sizeof(u8)) {
		pr_err("%s: Can't read the cal_mode from file\n", __func__);
		err = -EIO;
	}
	printk(KERN_INFO "[GP2A] %s : stop \n", __func__);
               
	filp_close(cal_mode_filp, current->files);
	set_fs(old_fs);

	return err;
}
static ssize_t proximity_cal_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        int result = 0;  
		
	struct gp2a_data *gp2a_cal_data=dev_get_drvdata(dev);
	
	
	result = gp2a_cal_data->cal_mode;
	printk(KERN_INFO "[GP2A] prox_cal_read = %d\n", result);        

	return sprintf(buf, "%d\n", result);
}

static ssize_t proximity_cal_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct gp2a_data *gp2a_cal_data=dev_get_drvdata(dev);
       
	if (sysfs_streq(buf, "1")) 
		gp2a_cal_data->cal_mode = 1;
        else if (sysfs_streq(buf, "2")) 
		gp2a_cal_data->cal_mode = 2;
        else if (sysfs_streq(buf, "0")) 
		gp2a_cal_data->cal_mode = 0;
        else  {
		pr_err("[GP2A] %s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}      

        printk(KERN_INFO "[GP2A] prox_cal_write =%d\n", gp2a_cal_data->cal_mode); 
        
        gp2a_cal_mode_save_file(gp2a_cal_data->cal_mode,gp2a_cal_data);
        
	return size;
}


static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR,
        proximity_enable_show, proximity_enable_store);

static DEVICE_ATTR(value, S_IRUGO | S_IWUSR | S_IWGRP,
		proximity_value_show, NULL);
		
static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR |S_IWGRP|S_IWOTH, proximity_cal_show, proximity_cal_store);
		
static struct attribute *gp2a_attrs[] = {
	&dev_attr_enable.attr,
	NULL,
};

static struct attribute_group gp2a_attribute_group = {
	.attrs = gp2a_attrs,
};

static struct device_attribute *proximity_sensor_attr[] = {
		&dev_attr_value,
		&dev_attr_enable,

		&dev_attr_prox_cal,

		NULL,
 };
 
static int __init gp2a_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	const struct gp2a_platform_data *pdata = client->dev.platform_data;
	struct gp2a_data *dt;
	int error;

	if (!pdata)
		return -EINVAL;

	if (pdata->hw_setup) {
		error = pdata->hw_setup(client);
		if (error < 0)
			return error;
	}

	error = gpio_request_one(pdata->vout_gpio, GPIOF_IN, GP2A_I2C_NAME);
	if (error)
		goto err_hw_shutdown;

	dt = kzalloc(sizeof(struct gp2a_data), GFP_KERNEL);
	if (!dt) {
		error = -ENOMEM;
		goto err_free_gpio;
	}

	dt->pdata = pdata;
	dt->i2c_client = client;

	error = gp2a_initialize(dt);
	if (error < 0)
		goto err_free_mem;

	dt->input = input_allocate_device();
	if (!dt->input) {
		error = -ENOMEM;
		goto err_free_mem;
	}

	input_set_drvdata(dt->input, dt);

//	dt->input->open = gp2a_device_open;
//	dt->input->close = gp2a_device_close;
	dt->input->name = GP2A_I2C_NAME;
	dt->input->id.bustype = BUS_I2C;
	dt->input->dev.parent = &client->dev;
	dt->proximity_enabled = 0;
	dt->irq = client->irq;
	dt->value = 0;

	input_set_capability(dt->input, EV_SW, SW_FRONT_PROXIMITY);

	error = request_threaded_irq(client->irq, NULL, gp2a_irq,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT | IRQF_NO_SUSPEND ,
			GP2A_I2C_NAME, dt);
	if (error) {
		dev_err(&client->dev, "irq request failed\n");
		goto err_free_input_dev;
	}

	error = input_register_device(dt->input);
	if (error) {
		dev_err(&client->dev, "device registration failed\n");
		goto err_free_irq;
	}

	error = sysfs_create_group(&dt->input->dev.kobj, &gp2a_attribute_group);
	if (error) {
		dev_err(&client->dev, "sysfs create failed\n");
		goto err_unregister_input;
	}

	device_init_wakeup(&client->dev, pdata->wakeup);
	i2c_set_clientdata(client, dt);
	dev_set_drvdata(&client->dev, dt);
	
	error = sensors_register(&proximity_sensor_device, dt,proximity_sensor_attr, "proximity_sensor");
    
	if (error < 0)
	{
		pr_err("%s: could not register""proximity sensor device(%d).\n",__func__, error);
		goto err_unregister_input;
	}
	disable_irq(client->irq);	
	
	if (dt->pdata->hw_pwr)
	{
			dt->pdata->hw_pwr(false);
			printk(KERN_ALERT"%s :: chip power down\n",__func__);
	}	
	return 0;

err_unregister_input:
	input_unregister_device(dt->input);
err_free_irq:
	free_irq(client->irq, dt);
err_free_input_dev:
	input_free_device(dt->input);
err_free_mem:
	kfree(dt);
err_free_gpio:
	gpio_free(pdata->vout_gpio);
err_hw_shutdown:
	if (pdata->hw_shutdown)
		pdata->hw_shutdown(client);
	if (pdata->hw_pwr)
		pdata->hw_pwr(false);
	return error;
}

static int __exit gp2a_remove(struct i2c_client *client)
{
	struct gp2a_data *dt = i2c_get_clientdata(client);
	const struct gp2a_platform_data *pdata = dt->pdata;

	device_init_wakeup(&client->dev, false);

	free_irq(client->irq, dt);
	
	sensors_unregister(proximity_sensor_device, proximity_sensor_attr);
	input_unregister_device(dt->input);
	kfree(dt);

	gpio_free(pdata->vout_gpio);

	if (pdata->hw_shutdown)
		pdata->hw_shutdown(client);

	if (pdata->hw_pwr)
		pdata->hw_pwr(false);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int gp2a_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2a_data *dt = i2c_get_clientdata(client);
	int retval = 0;
	printk(KERN_ALERT"%s\n",__func__);
	if (device_may_wakeup(&client->dev)) {
		enable_irq_wake(client->irq);
	} else {
		mutex_lock(&dt->input->mutex);
		if (dt->input->users)
			retval = gp2a_disable(dt);
		mutex_unlock(&dt->input->mutex);
	}

	return retval;
}

static int gp2a_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct gp2a_data *dt = i2c_get_clientdata(client);
	int retval = 0;
	printk(KERN_ALERT"%s\n",__func__);
	if (device_may_wakeup(&client->dev)) {
		disable_irq_wake(client->irq);
	} else {
		mutex_lock(&dt->input->mutex);
		if (dt->input->users)
			retval = gp2a_enable(dt);
		mutex_unlock(&dt->input->mutex);
	}

	return retval;
}
#endif

static SIMPLE_DEV_PM_OPS(gp2a_pm, gp2a_suspend, gp2a_resume);

static const struct i2c_device_id gp2a_i2c_id[] = {
	{ GP2A_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver gp2a_i2c_driver = {
	.driver = {
		.name	= GP2A_I2C_NAME,
		.owner	= THIS_MODULE,
		.pm	= &gp2a_pm,
	},
	.probe		= gp2a_probe,
	.remove		= gp2a_remove,
	.id_table	= gp2a_i2c_id,
};

module_i2c_driver(gp2a_i2c_driver);

MODULE_AUTHOR("Courtney Cavin <courtney.cavin@sonyericsson.com>");
MODULE_DESCRIPTION("Sharp GP2AP002A00F I2C Proximity/Opto sensor driver");
MODULE_LICENSE("GPL v2");
