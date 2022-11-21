/*
 *
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
//#include <linux/hardware_info.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>

//#include <mt-plat/mt_boot.h>

#define	LID_DEV_NAME	"hall_sensor"
//#define HALL_INPUT	"/dev/input/hall_dev"
#define EINT_PIN_COVER        (1)
#define EINT_PIN_LEAVE        (0)
static struct input_dev *mhall_input_dev = NULL;
//bug530234 yl.wt MODIFY,2020/05/05,remove hall  func
#if 0 
static struct work_struct mhall_eint_work;
#endif
int cur_mhall_eint_state = EINT_PIN_LEAVE;
static int mhall_is_enable = 1;
#define KEY_HALL_SENSOR 252

#define HALL_SENSOR_GET_STATUS	_IOW('c', 9, int *)

#define MHALL_DEBUG
#define MHALL_TAG	"[MHALL]"
#if defined(MHALL_DEBUG)
#define MHALL_FUN(f)			printk(KERN_INFO MHALL_TAG"%s\n", __FUNCTION__)
#define MHALL_ERR(fmt, args...)	printk(KERN_ERR  MHALL_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define MHALL_LOG(fmt, args...)	printk(KERN_INFO MHALL_TAG"%s(%d):" fmt, __FUNCTION__, __LINE__, ##args)
#define MHALL_DBG(fmt, args...)	printk(KERN_INFO fmt, ##args)
#else
#define MHALL_FUN(f)
#define MHALL_ERR(fmt, args...)
#define MHALL_LOG(fmt, args...)
#define MHALL_DBG(fmt, args...)
#endif
//+bug530234 yl.wt MODIFY,2020/05/05,remove hall  func
#if 0
int hall_irq;
unsigned int hall_eint_type;
static DEFINE_MUTEX(mhall_eint_mutex);
#endif
//-bug530234 yl.wt MODIFY,2020/05/05,remove hall  func
struct hall_data {
	int gpio;	/* device use gpio number */
	int irq;	/* device request irq number */
	int active_low;	/* gpio active high or low for valid value */
	struct input_dev *hall_dev;
};

static struct platform_driver hall_driver;
//+ bug530234 yl.wt delete,2020/05/05,remove hall func  
#if 0
static irqreturn_t hall_interrupt_handler(int irq, void *dev)
{
	struct hall_data *data = dev;
	int val = 0;

	MHALL_FUN();

	val = gpio_get_value(data->gpio);
	printk("[hall] data->gpio is %d\n", val);

	disable_irq_nosync(hall_irq);
		
	if(cur_mhall_eint_state == EINT_PIN_COVER){
		 if (hall_eint_type == IRQ_TYPE_LEVEL_HIGH)
			 irq_set_irq_type(hall_irq, IRQ_TYPE_LEVEL_HIGH);
		 else
			 irq_set_irq_type(hall_irq, IRQ_TYPE_LEVEL_LOW);
		 cur_mhall_eint_state = EINT_PIN_LEAVE;

	}else{
		if (hall_eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(hall_irq, IRQ_TYPE_LEVEL_LOW);
		else
			irq_set_irq_type(hall_irq, IRQ_TYPE_LEVEL_HIGH);

		cur_mhall_eint_state = EINT_PIN_COVER; 
	}

	enable_irq(hall_irq);

	if(mhall_is_enable)
	{
		schedule_work(&mhall_eint_work);
	}

	return IRQ_HANDLED;
}

static void mhall_eint_work_callback(struct work_struct *work)
{
	mutex_lock(&mhall_eint_mutex);
	input_report_key(mhall_input_dev, KEY_HALL_SENSOR, EINT_PIN_COVER);
	input_sync(mhall_input_dev);
	input_report_key(mhall_input_dev, KEY_HALL_SENSOR, EINT_PIN_LEAVE);
	input_sync(mhall_input_dev);
	mutex_unlock(&mhall_eint_mutex);
	
	MHALL_LOG("report mhall state = %s\n", cur_mhall_eint_state ? "cover" : "leave");

}
#endif
//- bug530234 yl.wt delete,2020/05/05,remove hall func
/*----------------------------------------------------------------------------*/
static ssize_t show_mhall_state(struct device_driver *ddri, char *buf)
{
	return sprintf(buf, "%d\n", cur_mhall_eint_state);		  
}
static ssize_t show_mhall_enable(struct device_driver *ddri, char *buf)
{
	return sprintf(buf, "%d\n", mhall_is_enable);		 
}
static ssize_t store_mhall_enable(struct device_driver *ddri, const char *buf, size_t count)
{
	int tmp;
	if(1 == sscanf(buf, "%d", &tmp))
	{
		mhall_is_enable = tmp;
	}
	return count; 
}
static DRIVER_ATTR(mhall_state, S_IWUSR | S_IRUGO, show_mhall_state, NULL);
static DRIVER_ATTR(mhall_enable, S_IWUSR | S_IRUGO, show_mhall_enable, store_mhall_enable);

static struct driver_attribute *mhall_attr_list[] = {
	&driver_attr_mhall_state,
	&driver_attr_mhall_enable,
};


/*----------------------------------------------------------------------------*/
static int mhall_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(mhall_attr_list)/sizeof(mhall_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		err = driver_create_file(driver, mhall_attr_list[idx]);
		if(err)
		{			 
			MHALL_LOG("driver_create_file (%s) = %d\n", mhall_attr_list[idx]->attr.name, err);
			break;
		}
	}	 
	return err;
}


/*----------------------------------------------------------------------------*/
static int mhall_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(mhall_attr_list)/sizeof(mhall_attr_list[0]));

	if (!driver)
	return -EINVAL;

	for(idx = 0; idx < num; idx++) 
	{
		driver_remove_file(driver, mhall_attr_list[idx]);
	}
	
	return err;
}

/*----------------------------------------------------------------------------*/

#ifdef CONFIG_OF
static int hall_parse_dt(struct device *dev, struct hall_data *data)
{
	//unsigned int tmp;
	//+ bug530234 yl.wt delete,2020/05/05,remove hall func  
	#if 0
	int rc;
	struct device_node *np = dev->of_node;
	int gpio_config[2];

	if (np) {
		of_property_read_u32_array(np, "interrupts", gpio_config, ARRAY_SIZE(gpio_config));
		MHALL_LOG("[hall]gpio_config [%d] [%d]\n", gpio_config[0], gpio_config[1]);
		data->gpio = gpio_config[0];
		hall_eint_type = gpio_config[1];
		data->active_low = gpio_config[1] & OF_GPIO_ACTIVE_LOW ? 0 : 1;
		hall_irq = irq_of_parse_and_map(np, 0);
		rc = request_irq(hall_irq, hall_interrupt_handler, IRQF_TRIGGER_NONE, "hall_sensor_irq", data);
		if (rc != 0) {
			MHALL_ERR("[hall]EINT IRQ LINE NOT AVAILABLE\n");
			return -EINVAL;
		} else {
			MHALL_LOG("[hall]hall set EINT finished, hall_irq=%d\n",hall_irq);
		}
	} else {
		MHALL_ERR("[hall]%s can't find compatible node\n", __func__);
		return -EINVAL;
	}
    #endif
    //+ bug530234 yl.wt delete,2020/05/05,remove hall func  
    
    return 0;
}
#else
static int hall_parse_dt(struct device *dev, struct hall_data *data)
{
	return -EINVAL;
}
#endif

static int hall_driver_probe(struct platform_device *dev)
{
	struct hall_data *data;
	int err = 0;

	MHALL_LOG("hall_driver probe\n");

	data = devm_kzalloc(&dev->dev, sizeof(*data), GFP_KERNEL);//bug 518717,gjx.wt,20191205,modify,change the sizeof data
	if (data == NULL) {
		err = -ENOMEM;
		MHALL_LOG("failed to allocate memory %d\n", err);
		goto exit;
	}

	dev_set_drvdata(&dev->dev, data);

	if (dev->dev.of_node) {
		err = hall_parse_dt(&dev->dev, data);
		if (err < 0) {
			dev_err(&dev->dev, "Failed to parse device tree\n");
			goto exit;
		}
	} else if (dev->dev.platform_data != NULL) {
		memcpy(data, dev->dev.platform_data, sizeof(*data));
	} else {
		MHALL_ERR("No valid platform data.\n");
		err = -ENODEV;
		goto exit;
	}
	
	/*** creat mhall_input_dev ***/
	mhall_input_dev = input_allocate_device();
	if (!mhall_input_dev)
    {
	    MHALL_ERR(" failed to allocate input device\n");
    }

	mhall_input_dev->name = "hall";

	__set_bit(EV_KEY,  mhall_input_dev->evbit);		//bug 15842 modify for input mode
	input_set_capability(mhall_input_dev, EV_KEY, KEY_HALL_SENSOR);
	
	err = input_register_device(mhall_input_dev);
	if(err){
		MHALL_ERR("failed to register mhall input device\n");
        input_free_device(mhall_input_dev);
	}
	//bug530234 yl.wt delete,2020/05/05,remove hall func  
    #if 0
	INIT_WORK(&mhall_eint_work, mhall_eint_work_callback);
	#endif
	/* Create sysfs files. */
	err = mhall_create_attr(&hall_driver.driver);
	if(err)
	{
		MHALL_ERR("create attr file fail\n");
	}

	MHALL_LOG("%s: OK\n", __func__);
	return 0;
exit:
	return err;
}


static int hall_driver_remove(struct platform_device *dev)	
{	
	int err = 0;

	input_unregister_device(mhall_input_dev);
	//cancel_work_sync(&mhall_eint_work);//bug530234 yl.wt MODIFY,2020/05/05,remove hall  func
	/* Destroy sysfs files. */
	err = mhall_delete_attr(&hall_driver.driver);
	if(err)
	{
		MHALL_LOG("delete attr file fail\n");
	}
	return 0;
}


static struct platform_device_id hall_id[] = {
	{LID_DEV_NAME, 0 },
	{ },
};


static struct of_device_id hall_match_table[] = {
	{.compatible = "mediatek,hall", },
	{ },
};

static struct platform_driver hall_driver = {
	.driver = {
		.name = LID_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = hall_match_table,
	},
	.probe = hall_driver_probe,
	.remove = hall_driver_remove,
	.id_table = hall_id,
};
static int __init hall_init(void)
{
	MHALL_LOG("hall_init\n");
	platform_driver_register(&hall_driver);
	return 0;
}

static void __exit hall_exit(void)
{
	platform_driver_unregister(&hall_driver);
}

module_init(hall_init);
module_exit(hall_exit);
MODULE_DESCRIPTION("Hall sensor driver");
MODULE_LICENSE("GPL v2");
