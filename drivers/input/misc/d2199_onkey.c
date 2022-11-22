/*
 * d2199_onkey.c: ON Key support for Dialog D2199
 *   
 * Copyright(c) 2013 Dialog Semiconductor Ltd.
 *  
 * Author: Dialog Semiconductor Ltd. D. Chen
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/d2199/pmic.h> 
#include <linux/d2199/d2199_reg.h> 
#include <linux/d2199/hwmon.h>
#include <linux/d2199/core.h>

#define DRIVER_NAME "d2199-onkey"

static int powerkey_pressed;
struct device *sec_powerkey;
extern struct class *sec_class;


int d2199_onkey_check(void)
{
	return powerkey_pressed;
}
EXPORT_SYMBOL(d2199_onkey_check);


static irqreturn_t d2199_onkey_event_lo_handler(int irq, void *data)
{
	struct d2199 *d2199 = data;
	struct d2199_onkey *dlg_onkey = &d2199->onkey;

	dev_info(d2199->dev, "Onkey LO Interrupt Event generated\n");

	input_report_key(dlg_onkey->input, KEY_POWER, 1);
	input_sync(dlg_onkey->input);

	powerkey_pressed = 1;	

	return IRQ_HANDLED;
} 

static irqreturn_t d2199_onkey_event_hi_handler(int irq, void *data)
{
	struct d2199 *d2199 = data;
	struct d2199_onkey *dlg_onkey = &d2199->onkey;

	dev_info(d2199->dev, "Onkey HI Interrupt Event generated\n");

	input_report_key(dlg_onkey->input, KEY_POWER, 0);
	input_sync(dlg_onkey->input);

	powerkey_pressed = 0;
	
	return IRQ_HANDLED;
} 

// AT + KEYSHORT cmd
static ssize_t keys_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;

//	printk("is_key_pressed = %d . is_power_key_pressed = %d\n",is_key_pressed,is_power_key_pressed);
	if(powerkey_pressed !=0 ) 
	{	
		count = sprintf(buf,"%s\n","PRESS");
	}
	else
	{
		count = sprintf(buf,"%s\n","RELEASE");
	}

       return count;
}
static DEVICE_ATTR(sec_power_key_pressed, S_IRUGO, keys_read, NULL);

static int __devinit d2199_onkey_probe(struct platform_device *pdev)
{
	struct d2199 *d2199 = platform_get_drvdata(pdev);
	struct d2199_onkey *dlg_onkey = &d2199->onkey;
	int ret = 0;
	
	dev_info(d2199->dev, "%s() Starting Onkey Driver\n",  __FUNCTION__);

	dlg_onkey->input = input_allocate_device();
    if (!dlg_onkey->input) {
		dev_err(&pdev->dev, "failed to allocate data device\n");
		return -ENOMEM;
	}
        
	dlg_onkey->input->evbit[0] = BIT_MASK(EV_KEY);
	dlg_onkey->input->keybit[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER);
	dlg_onkey->input->name = DRIVER_NAME;
	dlg_onkey->input->phys = "d2199-onkey/input0";
	dlg_onkey->input->id.bustype = BUS_I2C;	// DLG 
	dlg_onkey->input->dev.parent = &pdev->dev;

	ret = input_register_device(dlg_onkey->input);
	if (ret) {
		dev_err(&pdev->dev, "Unable to register input device,error: %d\n", ret);
		input_free_device(dlg_onkey->input);
		return ret;
	}

	d2199_register_irq(d2199, D2199_IRQ_ENONKEY_HI, d2199_onkey_event_hi_handler, 
                        0, DRIVER_NAME, d2199);
	d2199_register_irq(d2199, D2199_IRQ_ENONKEY_LO, d2199_onkey_event_lo_handler, 
                        0, DRIVER_NAME, d2199);                         
	dev_info(d2199->dev, "Onkey Driver registered\n");

	sec_powerkey = device_create(sec_class, NULL, 0, "%s", "sec_power_key");
	if(device_create_file(sec_powerkey, &dev_attr_sec_power_key_pressed) < 0)
		 printk("Failed to create device file(%s)!\n", dev_attr_sec_power_key_pressed.attr.name);

	return 0;

}

static int __devexit d2199_onkey_remove(struct platform_device *pdev)
{
	struct d2199 *d2199 = platform_get_drvdata(pdev);
	struct d2199_onkey *dlg_onkey = &d2199->onkey;

#if 0	// 20130729 block
	d2199_free_irq(d2199, D2199_IRQ_ENONKEY_LO);
	d2199_free_irq(d2199, D2199_IRQ_ENONKEY_HI);
#endif
	input_unregister_device(dlg_onkey->input);
	return 0;
}   

static struct platform_driver d2199_onkey_driver = {
	.probe		= d2199_onkey_probe,
	.remove		= __devexit_p(d2199_onkey_remove),
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	}
};

static int __init d2199_onkey_init(void)
{
	return platform_driver_register(&d2199_onkey_driver);
}

static void __exit d2199_onkey_exit(void)
{
	platform_driver_unregister(&d2199_onkey_driver);
}

module_init(d2199_onkey_init);
module_exit(d2199_onkey_exit);   

MODULE_AUTHOR("Dialog Semiconductor Ltd < james.ban@diasemi.com >");
MODULE_DESCRIPTION("Onkey driver for the Dialog D2199 PMIC");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRIVER_NAME);
