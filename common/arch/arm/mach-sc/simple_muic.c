#ifndef __SIMPLE_MUIC_C__
#define __SIMPLE_MUIC_C__

/*
 * Copyright (C) 2011, SAMSUNG Corporation.
 * Author: YongTaek Lee  <ytk.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <mach/adi.h>
#include <linux/device.h>
#include <mach/sci_glb_regs.h>
#include <mach/gpio.h>
#include <mach/usb.h>
#include <mach/adc.h>
#include <mach/sci.h>

#if defined(CONFIG_MACH_CORSICA_VE)
#include "board-corsicave3g.h"
#elif defined(CONFIG_MACH_VIVALTO)
#include "board-vivalto3g.h"
#elif defined(CONFIG_MACH_POCKET2)
#include "board-pocket23g.h"
#endif

extern void charger_enable(int enable);

#include <mach/simple_muic.h>

static int smuic_dm_gpio;
static int smuic_dp_gpio;
static struct smuic_driver_data *ginfo = NULL;

#define SMUIC_INITCALL


extern void sec_charger_cb(u8 attached);

static int simp_charger_is_adapter(void)
{
	uint32_t ret;
	unsigned long irq_flag = 0;

	mdelay(300);

	gpio_request(smuic_dm_gpio, "sprd_charge");
	gpio_request(smuic_dp_gpio, "sprd_charge");
	gpio_direction_input(smuic_dm_gpio);
	gpio_direction_input(smuic_dp_gpio);

	udc_enable();
	udc_phy_down();
	local_irq_save(irq_flag);

	sci_glb_clr(REG_AP_APB_USB_PHY_CTRL, (BIT_DMPULLDOWN | BIT_DPPULLDOWN));

	/* Identify USB charger */
	sci_glb_set(REG_AP_APB_USB_PHY_CTRL, BIT_DMPULLUP);
	mdelay(10);
	ret = gpio_get_value(smuic_dm_gpio);
	sci_glb_clr(REG_AP_APB_USB_PHY_CTRL, (BIT_DMPULLUP));

	local_irq_restore(irq_flag);
	udc_disable();
	gpio_free(smuic_dm_gpio);
	gpio_free(smuic_dp_gpio);

	printk("[%s]usb = %d\n", __func__, ret);

	return ret;
}


static int simp_charger_plugin(int usb_cable, void *data)
{
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");

	charger_enable(1);
	printk("charger plug in interrupt happen[%d]\n", usb_cable);

	charger_enable(1);

	if (usb_cable || !simp_charger_is_adapter()) {
		value.intval = POWER_SUPPLY_TYPE_USB;
	} else {
		value.intval = POWER_SUPPLY_TYPE_MAINS;
	}

	current_cable_type = value.intval;
	psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	return 0;
}

static int simp_charger_plugout(int usb_cable, void *data)
{
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");

	charger_enable(0);

	value.intval = POWER_SUPPLY_TYPE_BATTERY;
	current_cable_type = value.intval;
	psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);

	printk("charger plug out interrupt happen\n");

	return 0;
}

static struct usb_hotplug_callback smuic_charger_cb = {
	.plugin = simp_charger_plugin,
	.plugout = simp_charger_plugout,
	.data = NULL,
};

int __weak usb_register_hotplug_callback(struct usb_hotplug_callback *cb)
{
	return 0;
}

#ifdef SMUIC_INITCALL
void __init simp_muic_init(void)
{
	int ret;
	pr_info("[%s] charger callback\n", __func__);
	ret = usb_register_hotplug_callback(&smuic_charger_cb);
}
late_initcall(simp_muic_init);
#else
int simp_muic_init(void)
{
	int ret;
	printk("[%s] charger callback\n", __func__);
	ret = usb_register_hotplug_callback(&smuic_charger_cb);

	return ret;
}
#endif

static void smuic_init_data(struct smuic_driver_data *info, struct smuic_platform_data *pdata)
{
	info->smuic_dm_gpio = pdata->usb_dm_gpio;
	info->smuic_dp_gpio = pdata->usb_dp_gpio;
	smuic_dm_gpio = pdata->usb_dm_gpio;
	smuic_dp_gpio = pdata->usb_dp_gpio;
	printk("+++++++++smuic_gpio dm : %d dp : %d************", smuic_dm_gpio, smuic_dp_gpio);
}


static int smuic_probe(struct platform_device *pdev)
{
	struct smuic_platform_data *pdata = pdev->dev.platform_data;
	struct smuic_driver_data *info;
	int ret = 0;

	printk("%s\n",__func__);

	info = kzalloc(sizeof(struct smuic_driver_data),GFP_KERNEL);
	if(!info)
		return -ENOMEM;

	ginfo = info;

	mutex_init(&info->lock);
	mutex_init(&info->api_lock);

	platform_set_drvdata(pdev, info);

	smuic_init_data(info, pdata);

#if defined(CONFIG_MACH_LOGAN)|| defined(CONFIG_MACH_CORSICA_VE)
	info->pdata = pdata;
#endif
#ifndef SMUIC_INITCALL
	if(simp_muic_init())
	{
		printk("%s\n fail simp_muic_init",__func__);
		goto smuic_muic_init_fail;
	}
#endif
	return 0;
#ifndef SMUIC_INITCALL
smuic_muic_init_fail:
	kfree(info);
	return ret;
#endif
}


static int smuic_remove(struct platform_device *pdev)
{
	printk("%s\n",__func__);
	return 0;
}

#ifdef CONFIG_PM
static int smuic_suspend(struct device *dev)
{
	printk("%s\n",__func__);
	return 0;
}

static int smuic_resume(struct device *dev)
{
	printk("%s\n",__func__);

	return 0;
}

void smuic_shutdown(struct platform_device *pdev)
{
	struct smuic_driver_data *info = platform_get_drvdata(pdev);
	printk("%s\n", __func__);
	//kfree(info);
}

static struct dev_pm_ops smuic_pm_ops = {
	.suspend	= smuic_suspend,
	.resume		= smuic_resume,
};
#endif



static struct platform_driver smuic_driver = {
        .driver         = {
                .name   = "sprd_simple_muic",
                .owner  = THIS_MODULE,
#ifdef CONFIG_PM
                .pm     = &smuic_pm_ops,
#endif
        },
        .probe          = smuic_probe,
        .remove         = smuic_remove,
	.shutdown	= smuic_shutdown,
};


/* spa Init Function */
static int __init smuic_init(void)
{
	int retVal;

	printk(KERN_ALERT "%s\n", __func__);
	retVal = platform_driver_register(&smuic_driver);

	return (retVal);
}

/* spa Exit Function */
static void __exit smuic_exit(void)
{
	printk("%s\n",__func__);
	platform_driver_unregister(&smuic_driver);
}

subsys_initcall(smuic_init);
module_exit(smuic_exit);

MODULE_AUTHOR("YongTaek Lee <ytk.lee@samsung.com>");
MODULE_DESCRIPTION("Linux Driver for battery monitor");
MODULE_LICENSE("GPL");

#endif //__SPA_C__

