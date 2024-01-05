/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		 http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/platform_device.h>

#include <plat/gpio-cfg.h>
#include <plat/ehci.h>
#include <plat/devs.h>
#include <plat/udc-hs.h>
#include <mach/ohci.h>

#if defined(CONFIG_USB_HOST_NOTIFY)
#include <linux/host_notify.h>
#include <linux/power_supply.h>
extern int current_cable_type;
#endif

static struct exynos4_ohci_platdata universal4415_ohci_pdata __initdata;
static struct s5p_ehci_platdata universal4415_ehci_pdata __initdata;
static struct s3c_hsotg_plat universal4415_hsotg_pdata __initdata;

static void __init universal4415_ohci_init(void)
{
	exynos4_ohci_set_platdata(&universal4415_ohci_pdata);
}

static void __init universal4415_ehci_init(void)
{
	s5p_ehci_set_platdata(&universal4415_ehci_pdata);
}

static void __init universal4415_hsotg_init(void)
{
	s3c_hsotg_set_platdata(&universal4415_hsotg_pdata);
}

#if defined(CONFIG_USB_HOST_NOTIFY)
void otg_accessory_power(int enable)
{
	u8 on = (u8)!!enable;

	struct power_supply *psy = power_supply_get_by_name("sec-charger");
	union power_supply_propval value;
	if (on)
		value.intval = POWER_SUPPLY_TYPE_OTG;
	else
		value.intval = current_cable_type;
	psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);

	pr_info("%s: otg accessory power = %d\n", __func__, on);
}

void powered_otg_accessory_power(int enable)
{
	u8 on = (u8)!!enable;
/*
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;
	if (on)
		value.intval = POWER_SUPPLY_TYPE_OTG;
	else
		value.intval = current_cable_type;
	psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
*/
	pr_info("%s: powered otg accessory power = %d\n", __func__, on);
}


static struct host_notifier_platform_data host_notifier_pdata = {
	.ndev.name	= "usb_otg",
	.booster	= otg_accessory_power,
	.powered_booster = powered_otg_accessory_power,
	.thread_enable	= 0,
};

struct platform_device host_notifier_device = {
	.name = "host_notifier",
	.dev.platform_data = &host_notifier_pdata,
};
#endif

static struct platform_device *universal4415_usb_devices[] __initdata = {
	&s5p_device_ehci,
	&exynos4_device_ohci,
	&s3c_device_usb_hsotg,
#if defined(CONFIG_USB_HOST_NOTIFY)
	&host_notifier_device,
#endif
};

void __init exynos4_universal4415_usb_init(void)
{
	universal4415_ohci_init();
	universal4415_ehci_init();
	universal4415_hsotg_init();

	platform_add_devices(universal4415_usb_devices,
			ARRAY_SIZE(universal4415_usb_devices));
}
