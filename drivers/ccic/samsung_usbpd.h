/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#ifndef _SAMSUNG_USBPD_H_
#define _SAMSUNG_USBPD_H_

#include <linux/usb/usbpd.h>

#include <linux/types.h>
#include <linux/device.h>

/**
 * samsung_usbpd_get() - setup usbpd module
 *
 * @dev: device instance of the caller
 * @cb: struct containing callback function pointers.
 *
 * This function allows the client to initialize the usbpd
 * module. The module will communicate with usb driver and
 * handles the power delivery (PD) communication with the
 * sink/usb device. This module will notify the client using
 * the callback functions about the connection and status.
 */
int samsung_usbpd_get(struct device *dev, struct samsung_usbpd_private *usbpd);

void samsung_usbpd_put(struct device *dev, struct samsung_usbpd_private *usbpd);
#endif /* _SAMSUNG_USBPD_H_ */
