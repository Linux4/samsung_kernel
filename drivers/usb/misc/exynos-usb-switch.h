/*
 * exynos-usb-switch.h - USB switch driver for Exynos
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 * Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef __EXYNOS_USB_SWITCH
#define __EXYNOS_USB_SWITCH

#define SWITCH_WAIT_TIME	500
#define WAIT_TIMES		10

enum usb_cable_status {
	USB_DEVICE_ATTACHED,
	USB_HOST_ATTACHED,
	USB_DEVICE_DETACHED,
	USB_HOST_DETACHED,
};

struct exynos_usb_switch {
	unsigned long connect;

	int host_detect_irq;
	int device_detect_irq;
	int gpio_host_detect;
	int gpio_device_detect;
	int gpio_host_vbus;

	struct regulator *vbus_reg;

	struct device *ehci_dev;
	struct device *ohci_dev;

	struct device *ehci_rh_dev;
	struct device *ohci_rh_dev;

	struct device *s3c_udc_dev;

	struct platform_driver *ehci_driver;
	struct platform_driver *ohci_driver;

	const struct exynos_usbswitch_drvdata *drvdata;

	struct workqueue_struct	*workqueue;
	struct work_struct switch_work;
	struct mutex mutex;
	struct wake_lock wake_lock;
	atomic_t usb_status;
	int (*get_usb_mode)(void);
	int (*change_usb_mode)(int mode);
};

struct exynos_usbswitch_drvdata {
	bool driver_unregister;
	bool use_change_mode;
};

static const struct of_device_id of_exynos_usbswitch_match[];

static inline const struct exynos_usbswitch_drvdata
*exynos_usb_switch_get_driver_data(struct platform_device *pdev)
{
	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(of_exynos_usbswitch_match,
						pdev->dev.of_node);
		return match->data;
	}

	dev_err(&pdev->dev, "no device node specified and no driver data");
	return NULL;
}
#endif
