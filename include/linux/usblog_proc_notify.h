/*
 * Copyright (C) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

  /* usb notify layer v2.0 */

#ifndef __LINUX_USBLOG_PROC_NOTIFY_H__
#define __LINUX_USBLOG_PROC_NOTIFY_H__

enum usblog_type {
	NOTIFY_USBSTATE,
	NOTIFY_USBMODE,
	NOTIFY_EVENT,
};

enum usblog_state {
	NOTIFY_CONFIGURED = 1,
	NOTIFY_CONNECTED,
	NOTIFY_DISCONNECTED,
	NOTIFY_RESET,
	NOTIFY_ACCSTART,
};

#ifdef CONFIG_USB_NOTIFY_PROC_LOG
extern void store_usblog_notify(int type, void *param1, void *parma2);
extern int register_usblog_proc(void);
#else
static inline const void store_usblog_notify(int type, void *param1,
			void *parma2) {return; }
static inline const int register_usblog_proc(void)
			{return 0; }
#endif
#endif

