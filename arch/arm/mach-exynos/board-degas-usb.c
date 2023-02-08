/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
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

static struct exynos4_ohci_platdata smdk4270_ohci_pdata __initdata;
static struct s5p_ehci_platdata smdk4270_ehci_pdata __initdata;
static struct s3c_hsotg_plat smdk4270_hsotg_pdata __initdata;

static void __init smdk4270_ohci_init(void)
{
	exynos4_ohci_set_platdata(&smdk4270_ohci_pdata);
}

static void __init smdk4270_ehci_init(void)
{
	s5p_ehci_set_platdata(&smdk4270_ehci_pdata);
}

static void __init smdk4270_hsotg_init(void)
{
	s3c_hsotg_set_platdata(&smdk4270_hsotg_pdata);
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
	pr_info("%s: powered otg accessory power = %d\n", __func__, on);
}

#define GPIO_VBUS_IRQ		EXYNOS4_GPX1(7)

static struct host_notifier_platform_data host_notifier_pdata = {
	.ndev.name	= "usb_otg",
	.gpio		= GPIO_VBUS_IRQ,
	.booster	= otg_accessory_power,
	.powered_booster = powered_otg_accessory_power,
	.irq_enable	= 1,
};

struct platform_device host_notifier_device = {
	.name = "host_notifier",
	.dev.platform_data = &host_notifier_pdata,
};

static void __init smdk4270_vbus_gpio_init(void)
{
	int ret;
	unsigned int gpio;

	gpio = GPIO_VBUS_IRQ;
	ret = gpio_request(gpio, "vbus_int");
	if (ret)
		pr_err("%s, Failed to request gpio vbus_int.(%d)\n",
			__func__, ret);

	s3c_gpio_cfgpin(gpio, 0xf);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	gpio_free(gpio);
}
#endif

static struct platform_device *smdk4270_usb_devices[] __initdata = {
	&exynos4_device_ohci,
	&s5p_device_ehci,
	&s3c_device_usb_hsotg,
#if defined(CONFIG_USB_HOST_NOTIFY)
	&host_notifier_device,
#endif
};

void __init exynos4_smdk4270_usb_init(void)
{
	smdk4270_ohci_init();
	smdk4270_ehci_init();
	smdk4270_hsotg_init();
#if defined(CONFIG_USB_HOST_NOTIFY)
	smdk4270_vbus_gpio_init();
#endif

	platform_add_devices(smdk4270_usb_devices,
			ARRAY_SIZE(smdk4270_usb_devices));
}
