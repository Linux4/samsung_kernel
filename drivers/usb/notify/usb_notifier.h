/*
 * Copyright (C) 2014 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __LINUX_USB_NOTIFIER_H__
#define __LINUX_USB_NOTIFIER_H__

extern void mt_usb_connect(void);
extern void mt_usb_disconnect(void);
extern int mtk_xhci_driver_load(bool vbus_on);

extern void mtk_xhci_driver_unload(bool vbus_off);


#endif

