/*
 * File:         al3006_pls.c
 * Based on:
 * Author:       Yunlong Wang <Yunlong.Wang @spreadtrum.com>
 *
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
 */


#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/gpio.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/i2c/al3006_pls.h>
#include <linux/miscdevice.h>

static struct i2c_client *this_client;
static atomic_t p_flag;
static atomic_t l_flag;
static int al3006_pls_opened=0;

/*0: sleep out; 1: sleep in*/
static unsigned char suspend_flag=0;


static ssize_t al3006_pls_show_suspend(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t al3006_pls_store_suspend(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static ssize_t al3006_pls_show_version(struct device* cd,struct device_attribute *attr, char* buf);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void al3006_pls_early_suspend(struct early_suspend *handler);
static void al3006_pls_early_resume(struct early_suspend *handler);
#endif
static int al3006_pls_write_data(unsigned char addr, unsigned char data);
static int al3006_pls_read_data(unsigned char addr, unsigned char *data);
static int al3006_pls_enable(SENSOR_TYPE type);
static int al3006_pls_disable(SENSOR_TYPE type);
static void al3006_pls_report_init(void);

static int al3006_pls_adc2lux[64]={
	1, 1, 1, 2, 2, 2, 3, 4,
	4, 5, 6, 7, 9, 11, 13, 16,
	19, 22, 27, 32, 39, 46, 56, 67,
	80, 96, 116, 139, 167, 200, 240, 289,
	346, 416, 499, 599, 720, 864, 1037, 1245,
	1495, 1795, 2154, 2586, 3105, 3728, 4475, 5372,
	6449, 7743, 9295, 11159, 13396, 16082, 19307, 23178,
	27862, 33405, 40103, 48144, 57797, 69386, 83298, 100000
};


static DEVICE_ATTR(suspend, S_IRUGO | S_IWUSR, al3006_pls_show_suspend, al3006_pls_store_suspend);
static DEVICE_ATTR(version, S_IRUGO | S_IWUSR, al3006_pls_show_version, NULL);

static ssize_t al3006_pls_show_suspend(struct device* cd,
				     struct device_attribute *attr, char* buf)
{
	ssize_t ret = 0;

	if(suspend_flag==1)
		sprintf(buf, "AL3006 Resume\n");
	else
		sprintf(buf, "AL3006 Suspend\n");

	ret = strlen(buf) + 1;

	return ret;
}

static ssize_t al3006_pls_store_suspend(struct device* cd, struct device_attribute *attr,
		       const char* buf, size_t len)
{
	unsigned long on_off = simple_strtoul(buf, NULL, 10);
	suspend_flag = on_off;

	if(on_off==1)
	{
		printk(KERN_INFO "AL3006 Entry Resume\n");
		al3006_pls_enable(AL3006_PLS_BOTH);
	}
	else
	{
		printk(KERN_INFO "AL3006 Entry Suspend\n");
		al3006_pls_disable(AL3006_PLS_BOTH);
	}

	return len;
}

static ssize_t al3006_pls_show_version(struct device* cd,
				     struct device_attribute *attr, char* buf)
{
	ssize_t ret = 0;

	sprintf(buf, "AL3006");
	ret = strlen(buf) + 1;

	return ret;
}

static int al3006_pls_create_sysfs(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);

	PLS_DBG("%s", __func__);

	err = device_create_file(dev, &dev_attr_suspend);
	err = device_create_file(dev, &dev_attr_version);

	return err;
}

/*******************************************************************************
* Function    :  al3006_pls_enable
* Description :  type: proximity / light
*******************************************************************************/
static int al3006_pls_enable(SENSOR_TYPE type)
{
	unsigned char config;

	al3006_pls_read_data(AL3006_PLS_REG_CONFIG, &config);

	PLS_DBG("%s: type=%d; config=0x%x", __func__, type, config);

	switch(type) {
		case AL3006_PLS_ALPS:
			if(config == AL3006_PLS_PXY_ACTIVE)
				type = AL3006_PLS_BOTH;
			break;
		case AL3006_PLS_PXY:
			if(config == AL3006_PLS_ALPS_ACTIVE)
				type = AL3006_PLS_BOTH;
			break;
		case AL3006_PLS_BOTH:
			break;
	}

	PLS_DBG("%s: after type=%d\n",__func__, type);

	switch(type) {
		case AL3006_PLS_ALPS:
			al3006_pls_write_data(AL3006_PLS_REG_CONFIG, AL3006_PLS_ALPS_ACTIVE);
			break;
		case AL3006_PLS_PXY:
			al3006_pls_write_data(AL3006_PLS_REG_CONFIG, AL3006_PLS_PXY_ACTIVE);
			al3006_pls_report_init();
			break;
		case AL3006_PLS_BOTH:
			al3006_pls_write_data(AL3006_PLS_REG_CONFIG, AL3006_PLS_BOTH_ACTIVE);
			al3006_pls_report_init();
			break;
		default:
			break;
	}

	return 0;
}

/*******************************************************************************
* Function    :  al3006_pls_disable
* Description :  type: proximity / light
*******************************************************************************/
static int al3006_pls_disable(SENSOR_TYPE type)
{
	int pxy_flag, apls_flag;

	PLS_DBG("%s: type=%d", __func__, type);

	switch(type) {
		case AL3006_PLS_ALPS:
			pxy_flag = atomic_read(&p_flag);
			/*check proximiy sensor status*/
			if(pxy_flag == 0) {
				PLS_DBG("%s: Disable both sensor for pxy_flag=%d", __func__, pxy_flag);
				al3006_pls_write_data(AL3006_PLS_REG_CONFIG, AL3006_PLS_BOTH_DEACTIVE);
			} else {
				PLS_DBG("%s: Only Proximity Sensor alive", __func__);
				al3006_pls_write_data(AL3006_PLS_REG_CONFIG, AL3006_PLS_PXY_ACTIVE);
			}

			break;
		case AL3006_PLS_PXY:
			apls_flag = atomic_read(&l_flag);
			/*check light sensor status*/
			if(apls_flag == 0) {
				PLS_DBG("%s: Disable both sensor for apls_flag=%d", __func__, apls_flag);
				al3006_pls_write_data(AL3006_PLS_REG_CONFIG, AL3006_PLS_BOTH_DEACTIVE);
			} else {
				PLS_DBG("%s: Only Light Sensor alive", __func__);
				al3006_pls_write_data(AL3006_PLS_REG_CONFIG, AL3006_PLS_ALPS_ACTIVE);
			}
			break;
		case AL3006_PLS_BOTH:
			PLS_DBG("%s: Disable both sensors", __func__);
			al3006_pls_write_data(AL3006_PLS_REG_CONFIG, AL3006_PLS_BOTH_DEACTIVE);
			break;
		default:
			break;
	}

	return 0;
}

static int al3006_pls_open(struct inode *inode, struct file *file)
{
	PLS_DBG("%s\n", __func__);
	if (al3006_pls_opened)
		return -EBUSY;
	al3006_pls_opened = 1;
	return 0;
}

static int al3006_pls_release(struct inode *inode, struct file *file)
{
	PLS_DBG("%s", __func__);
	al3006_pls_opened = 0;
	return al3006_pls_disable(AL3006_PLS_BOTH);
}


/*******************************************************************************
* Function    :  al3006_pls_ioctl
* Description :  ioctl interface to control the sensor
*******************************************************************************/
static long al3006_pls_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int flag;
	unsigned char data;

	PLS_DBG("%s: cmd %d", __func__, _IOC_NR(cmd));

	/*get ioctl parameter*/
	switch (cmd) {
	case LTR_IOCTL_SET_PFLAG:
	case LTR_IOCTL_SET_LFLAG:
		if (copy_from_user(&flag, argp, sizeof(flag))) {
			return -EFAULT;
		}
		if (flag < 0 || flag > 1) {
			return -EINVAL;
		}
		PLS_DBG("%s: set flag=%d", __func__, flag);
		break;
	default:
		break;
	}

	/*handle ioctl*/
	switch (cmd) {
	case LTR_IOCTL_GET_PFLAG:
		flag = atomic_read(&p_flag);
		break;

	case LTR_IOCTL_GET_LFLAG:
		flag = atomic_read(&l_flag);
		break;

	case LTR_IOCTL_GET_DATA:
		break;

	case LTR_IOCTL_SET_PFLAG:
		atomic_set(&p_flag, flag);
		if(flag==1){
			al3006_pls_enable(AL3006_PLS_PXY);
		}
		else if(flag==0) {
			al3006_pls_disable(AL3006_PLS_PXY);
		}
		break;

	case LTR_IOCTL_SET_LFLAG:
		atomic_set(&l_flag, flag);
		if(flag==1){
			al3006_pls_enable(AL3006_PLS_ALPS);
		}
		else if(flag==0) {
			al3006_pls_disable(AL3006_PLS_ALPS);
		}
		break;

	default:
		pr_err("%s: invalid cmd %d\n", __func__, _IOC_NR(cmd));
		return -EINVAL;
	}

	/*report ioctl*/
	switch (cmd) {
	case LTR_IOCTL_GET_PFLAG:
	case LTR_IOCTL_GET_LFLAG:
		if (copy_to_user(argp, &flag, sizeof(flag))) {
			return -EFAULT;
		}
		PLS_DBG("%s: get flag=%d", __func__, flag);
		break;

	case LTR_IOCTL_GET_DATA:
		al3006_pls_read_data(AL3006_PLS_REG_DATA,&data);
		if (copy_to_user(argp, &data, sizeof(data))) {
			return -EFAULT;
		}
		PLS_DBG("%s: get data=%d", __func__, flag);
		break;

	default:
		break;
	}

	return 0;

}


static struct file_operations al3006_pls_fops = {
	.owner				= THIS_MODULE,
	.open				= al3006_pls_open,
	.release			= al3006_pls_release,
	.unlocked_ioctl		= al3006_pls_ioctl,
};

static struct miscdevice al3006_pls_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = AL3006_PLS_DEVICE,
	.fops = &al3006_pls_fops,
};

/*******************************************************************************
* Function    :  al3006_pls_rx_data
* Description :  read data from i2c
* Parameters  :  buf: content, len: length
*******************************************************************************/
static int al3006_pls_rx_data(char *buf, int len)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
		}
	};

	ret = i2c_transfer(this_client->adapter, msgs, 2);
    if (ret < 0)
            printk(KERN_ERR "%s i2c read error: %d\n", __func__, ret);


	return ret;
}

/*******************************************************************************
* Function    :  al3006_pls_tx_data
* Description :  send data to i2c
* Parameters  :  buf: content, len: length
*******************************************************************************/
static int al3006_pls_tx_data(char *buf, int len)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= len,
			.buf	= buf,
		}
	};

	ret = i2c_transfer(this_client->adapter, msg, 1);
	if (ret < 0)
		printk(KERN_ERR "%s i2c write error: %d\n", __func__, ret);

	return ret;
}

/*******************************************************************************
* Function    :  al3006_pls_write_data
* Description :  write data to IC
* Parameters  :  addr: register address, data: register data
*******************************************************************************/
static int al3006_pls_write_data(unsigned char addr, unsigned char data)
{
	unsigned char buf[2];
	buf[0]=addr;
	buf[1]=data;
	return al3006_pls_tx_data(buf, 2);
}

/*******************************************************************************
* Function    :  al3006_pls_read_data
* Description :  read data from IC
* Parameters  :  addr: register address, data: read data
*******************************************************************************/
static int al3006_pls_read_data(unsigned char addr, unsigned char *data)
{
	int ret;
	unsigned char buf;

	buf=addr;
	ret = al3006_pls_rx_data(&buf, 1);
	*data = buf;

	return ret;;
}

#if PLS_DEBUG
static void al3006_pls_reg_dump(void)
{
	unsigned char config,time_ctrl,dls_ctrl,int_status,dps_ctrl,data,dls_win;
	al3006_pls_read_data(AL3006_PLS_REG_CONFIG,&config);
	al3006_pls_read_data(AL3006_PLS_REG_TIME_CTRL,&time_ctrl);
	al3006_pls_read_data(AL3006_PLS_REG_DLS_CTRL,&dls_ctrl);
	al3006_pls_read_data(AL3006_PLS_REG_INT_STATUS,&int_status);
	al3006_pls_read_data(AL3006_PLS_REG_DPS_CTRL,&dps_ctrl);
	al3006_pls_read_data(AL3006_PLS_REG_DATA,&data);
	al3006_pls_read_data(AL3006_PLS_REG_DLS_WIN,&dls_win);

	PLS_DBG("%s: config=0x%x,time_ctrl=0x%x,dls_ctrl=0x%x,int_status=0x%x,dps_ctrl=0x%x,data=0x%x,dls_win=0x%x", \
		__func__, config,time_ctrl,dls_ctrl,int_status,dps_ctrl,data,dls_win);
}
#endif

/*******************************************************************************
* Function    :  al3006_pls_reg_init
* Description :  set al3006 registers
* Parameters  :  none
* Return      :  void
*******************************************************************************/
static int al3006_pls_reg_init(void)
{
	int ret=0;

	/*set window loss*/
	if(al3006_pls_write_data(AL3006_PLS_REG_DLS_WIN,0x0D)<0) {
		printk(KERN_ERR "%s: I2C Write Reg %x Failed\n", __func__, AL3006_PLS_REG_DLS_WIN);
		ret = -1;
	}

	/*set time control*/
	if(al3006_pls_write_data(AL3006_PLS_REG_TIME_CTRL,0x1)<0) {
		printk(KERN_ERR "%s: I2C Write Reg %x Failed\n", __func__, AL3006_PLS_REG_TIME_CTRL);
		ret = -1;
	}

	/*set alps level*/
#if defined(AL3006_PLS_ADC_LEVEL64)
	al3006_pls_write_data(AL3006_PLS_REG_DLS_CTRL,0xA0);
#elif defined(AL3006_PLS_ADC_LEVEL9)
	al3006_pls_write_data(AL3006_PLS_REG_DLS_CTRL,0x40);
#elif defined(AL3006_PLS_ADC_LEVEL5)
	al3006_pls_write_data(AL3006_PLS_REG_DLS_CTRL,0x20);
#endif

	/*set dps distance*/
	if(al3006_pls_write_data(AL3006_PLS_REG_DPS_CTRL,0x10)<0) {
		printk(KERN_ERR "%s: I2C Write Reg %x Failed\n", __func__, AL3006_PLS_REG_DPS_CTRL);
		ret = -1;
	}

	/*enter power down mode*/
	if(al3006_pls_write_data(AL3006_PLS_REG_CONFIG,AL3006_PLS_BOTH_DEACTIVE)<0) {
		printk(KERN_ERR "%s: I2C Write Reg %x Failed\n", __func__, AL3006_PLS_BOTH_DEACTIVE);
		ret = -1;
	}

#if PLS_DEBUG
	al3006_pls_reg_dump();
#endif

	return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
/*******************************************************************************
* Function    :  al3006_pls_early_suspend
* Description :  cancel the delayed work and put ts to shutdown mode
* Parameters  :  handler
* Return      :    none
*******************************************************************************/
static void al3006_pls_early_suspend(struct early_suspend *handler)
{
	PLS_DBG("%s\n", __func__);
#if 0
	al3006_pls_disable(AL3006_PLS_BOTH);
#endif
}

/*******************************************************************************
* Function    :  al3006_pls_early_resume
* Description :  ts re-entry the normal mode and schedule the work, there need to be  a litte time
                      for ts ready
* Parameters  :  handler
* Return      :    none
*******************************************************************************/
static void al3006_pls_early_resume(struct early_suspend *handler)
{
	PLS_DBG("%s\n", __func__);
#if 0
	al3006_pls_enable(AL3006_PLS_BOTH);
#endif
}
#endif

static void al3006_pls_report_init(void)
{
	al3006_pls_struct *al3006_pls = (al3006_pls_struct *)i2c_get_clientdata(this_client);
	PLS_DBG("%s\n",__func__);
	/*report far*/
	input_report_abs(al3006_pls->input, ABS_DISTANCE, 1);
	input_sync(al3006_pls->input);
}

/*******************************************************************************
* Function    :  al3006_pls_report_dps
* Description :  reg data,input dev
* Parameters  :  report proximity sensor data to input system
* Return      :  none
*******************************************************************************/
static void al3006_pls_report_dps(unsigned char data, struct input_dev *input)
{
	unsigned char dps_data = (data&0x80)>>7;

	dps_data = (dps_data==1) ? 0: 1;
	PLS_DBG("%s: proximity=%d", __func__, dps_data);

	input_report_abs(input, ABS_DISTANCE, dps_data);
	input_sync(input);
}

/*******************************************************************************
* Function    :  al3006_pls_report_dls
* Description :  reg data,input dev
* Parameters  :  report light sensor data to input system
* Return      :  none
*******************************************************************************/
static void al3006_pls_report_dls(unsigned char data, struct input_dev *input)
{
	int lux;
	unsigned char adc = data&0x3f;

#if defined(AL3006_PLS_ADC_LEVEL9)
	adc = adc<<3;
#elif defined(AL3006_PLS_ADC_LEVEL5)
	adc = adc<<4;
#endif

	lux = al3006_pls_adc2lux[adc];
	PLS_DBG("%s: adc=%d, lux=%d", __func__, adc, lux);
	input_report_abs(input, ABS_MISC, lux);
	input_sync(input);
}

/*******************************************************************************
* Function    :  al3006_pls_work
* Description :  handler current data and report coordinate
* Parameters  :  work
* Return      :    none
*******************************************************************************/

static void al3006_pls_work(struct work_struct *work)
{
	unsigned char int_status, data, enable;
	al3006_pls_struct *pls = container_of(work, al3006_pls_struct, work);

	al3006_pls_read_data(AL3006_PLS_REG_CONFIG,&enable);
	al3006_pls_read_data(AL3006_PLS_REG_INT_STATUS,&int_status);
	al3006_pls_read_data(AL3006_PLS_REG_DATA,&data);

	PLS_DBG("\033[33;1m %s: enable=0x%x; int_status=0x%x; data=0x%x \033[m",\
		__func__, enable, int_status, data);

	switch(int_status & AL3006_PLS_INT_MASK) {
		case AL3006_PLS_DPS_INT:
			al3006_pls_report_dps(data, pls->input);
			break;

		case AL3006_PLS_DLS_INT:
			al3006_pls_report_dls(data, pls->input);
			break;

		case AL3006_PLS_INT_MASK:
			al3006_pls_report_dps(data, pls->input);
			al3006_pls_report_dls(data, pls->input);
			break;

		default:
			break;
	}

	enable_irq(pls->client->irq);

}


/*******************************************************************************
* Function    :  al3006_pls_irq_handler
* Description :  handle ts irq
* Parameters  :  handler
* Return      :    none
*******************************************************************************/
static irqreturn_t al3006_pls_irq_handler(int irq, void *dev_id)
{
	al3006_pls_struct *pls = (al3006_pls_struct *)dev_id;

	disable_irq_nosync(pls->client->irq);
	queue_work(pls->ltr_work_queue,&pls->work);
	return IRQ_HANDLED;
}

static void al3006_pls_pininit(int irq_pin)
{
	printk(KERN_INFO "%s [irq=%d]\n",__func__,irq_pin);
	gpio_request(irq_pin, AL3006_PLS_IRQ_PIN);
}


static int al3006_pls_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	int err = 0;
	struct al3006_pls_platform_data *pdata = client->dev.platform_data;
	struct input_dev  *input_dev;
	al3006_pls_struct *al3006_pls;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk("%s: functionality check failed\n", __func__);
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	al3006_pls = kzalloc(sizeof(al3006_pls_struct), GFP_KERNEL);
	if (!al3006_pls)
	{
		printk("%s: request memory failed\n", __func__);
		err= -ENOMEM;
		goto exit_request_memory_failed;
	}

	this_client = client;
	al3006_pls->client = client;
	i2c_set_clientdata(client, al3006_pls);

	/*pin init*/
	al3006_pls_pininit(pdata->irq_gpio_number);

	/*get irq*/
	client->irq = gpio_to_irq(pdata->irq_gpio_number);
	al3006_pls->irq = client->irq;

	PLS_DBG("I2C name=%s, addr=0x%x, gpio=%d, irq=%d",client->name,client->addr, \
		pdata->irq_gpio_number, client->irq);

	/*init AL3006_PLS*/
	if(al3006_pls_reg_init()<0)
	{
		printk(KERN_ERR "%s: device init failed\n", __func__);
		err=-1;
		goto exit_device_init_failed;
	}

	/*register device*/
	err = misc_register(&al3006_pls_device);
	if (err) {
		printk(KERN_ERR "%s: al3006_pls_device register failed\n", __func__);
		goto exit_device_register_failed;
	}


	/* register input device*/
	input_dev = input_allocate_device();
	if (!input_dev)
	{
		printk(KERN_ERR "%s: input allocate device failed\n", __func__);
		err = -ENOMEM;
		goto exit_input_dev_allocate_failed;
	}

	al3006_pls->input = input_dev;

	input_dev->name  = AL3006_PLS_INPUT_DEV;
	input_dev->phys  = AL3006_PLS_INPUT_DEV;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0010;

	__set_bit(EV_ABS, input_dev->evbit);
	/*for proximity*/
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);
	/*for lightsensor*/
	input_set_abs_params(input_dev, ABS_MISC, 0, 100001, 0, 0);

	err = input_register_device(input_dev);
	if (err < 0)
	{
	    printk(KERN_ERR "%s: input device regist failed\n", __func__);
	    goto exit_input_register_failed;
	}

	/*create work queue*/
	INIT_WORK(&al3006_pls->work, al3006_pls_work);
	al3006_pls->ltr_work_queue= create_singlethread_workqueue(AL3006_PLS_DEVICE);

#ifdef CONFIG_HAS_EARLYSUSPEND
	/*register early suspend*/
	al3006_pls->ltr_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	al3006_pls->ltr_early_suspend.suspend = al3006_pls_early_suspend;
	al3006_pls->ltr_early_suspend.resume = al3006_pls_early_resume;
	register_early_suspend(&al3006_pls->ltr_early_suspend);
#endif

	if(client->irq > 0)
	{
		err =  request_irq(client->irq, al3006_pls_irq_handler, IRQ_TYPE_LEVEL_LOW, client->name,al3006_pls);
		if (err <0)
		{
			printk(KERN_ERR "%s: IRQ setup failed %d\n", __func__, err);
			goto irq_request_err;
		}
	}

	/*create attribute files*/
	al3006_pls_create_sysfs(client);


	printk(KERN_INFO "%s: Probe Success!\n",__func__);

	return 0;

irq_request_err:
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&al3006_pls->ltr_early_suspend);
#endif
	destroy_workqueue(al3006_pls->ltr_work_queue);
exit_input_register_failed:
	input_free_device(input_dev);
	misc_deregister(&al3006_pls_device);
exit_device_register_failed:
exit_input_dev_allocate_failed:
exit_device_init_failed:
	kfree(al3006_pls);
exit_request_memory_failed:
exit_check_functionality_failed:
	printk(KERN_ERR "%s: Probe Fail!\n",__func__);
	return err;

}

static int al3006_pls_remove(struct i2c_client *client)
{
	al3006_pls_struct *al3006_pls = i2c_get_clientdata(client);

	PLS_DBG("%s", __func__);

	/*remove queue*/
	flush_workqueue(al3006_pls->ltr_work_queue);
	destroy_workqueue(al3006_pls->ltr_work_queue);

#ifdef CONFIG_HAS_EARLYSUSPEND
	/*free suspend*/
	unregister_early_suspend(&al3006_pls->ltr_early_suspend);
#endif
	misc_deregister(&al3006_pls_device);
	/*free input*/
	input_unregister_device(al3006_pls->input);
	input_free_device(al3006_pls->input);
	/*free irq*/
	free_irq(al3006_pls->client->irq, al3006_pls);
	/*free malloc*/
	kfree(al3006_pls);

	return 0;
}

static const struct i2c_device_id al3006_pls_id[] = {
	{ AL3006_PLS_DEVICE, 0 },
	{ }
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver al3006_pls_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name  = AL3006_PLS_DEVICE,
    },
	.probe      = al3006_pls_probe,
	.remove     = al3006_pls_remove,
	.id_table = al3006_pls_id,
};
/*----------------------------------------------------------------------------*/

static int __init al3006_pls_init(void)
{
	PLS_DBG("%s", __func__);
	return i2c_add_driver(&al3006_pls_driver);
}

static void __exit al3006_pls_exit(void)
{
	PLS_DBG("%s", __func__);
	i2c_del_driver(&al3006_pls_driver);
}

module_init(al3006_pls_init);
module_exit(al3006_pls_exit);


MODULE_AUTHOR("Yunlong Wang <Yunlong.Wang@spreadtrum.com>");
MODULE_DESCRIPTION("Proximity&Light Sensor AL3006 DRIVER");
MODULE_LICENSE("GPL");

