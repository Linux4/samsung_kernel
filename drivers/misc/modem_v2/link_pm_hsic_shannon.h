/*
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINK_PM_HSIC_SHANNON_H__
#define __LINK_PM_HSIC_SHANNON_H__

#define CONFIG_WAIT_CP_SUSPEND_READY
/* #define CONFIG_DPM_RESUME */

struct link_pm_data {
	struct miscdevice miscdev;
	struct platform_device *pdev;
	struct modemlink_pm_data *pdata;

	struct notifier_block usb_nfb;
	struct notifier_block pm_nfb;
#ifdef CONFIG_OF
	struct notifier_block phy_nfb;
#endif

	struct usb_device *hdev;
	struct usb_device *udev;

	struct list_head link;

	wait_queue_head_t hostwake_waitq;
	struct wake_lock wake;
	spinlock_t lock;

	atomic_t status;
	atomic_t dir;
	atomic_t hostwake_status;

	bool pm_request;

	unsigned long irqflag;
	unsigned int irq;

#ifdef CONFIG_WAIT_CP_SUSPEND_READY
	wait_queue_head_t cp_suspend_ready_waitq;
	unsigned long cp_suspend_ready_irqflag;
	unsigned int cp_suspend_ready_irq;
	atomic_t cp_suspend_ready;
	atomic_t cp2ap_status;
	int (*wait_cp_suspend_ready)(struct link_pm_data *pmdata,
				struct usb_device *udev);
#endif
};

#endif	/* __LINK_PM_HSIC_SHANNON_H__ */
