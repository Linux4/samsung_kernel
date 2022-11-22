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

struct link_pm_data {
	struct miscdevice miscdev;
	struct platform_device *pdev;
	struct modemlink_pm_data *pdata;

	struct notifier_block usb_nfb;
	struct notifier_block phy_nfb;
#ifndef CONFIG_ARGOS
	struct notifier_block qos_nfb;
#endif
	struct notifier_block pm_nfb;

	struct usb_device *hdev;
	struct usb_device *udev;

	struct list_head link;

	wait_queue_head_t hostwake_waitq;
	struct wake_lock wake;
	spinlock_t lock;

	atomic_t status;
	atomic_t dir;
	atomic_t hostwake_status;

	atomic_t dpm_suspending;

	unsigned int irq;

#ifdef CONFIG_WAIT_CP_SUSPEND_READY
	wait_queue_head_t cp_suspend_ready_waitq;
	unsigned int cp_suspend_ready_irq;
	atomic_t cp_suspend_ready;
	atomic_t cp2ap_status;
	int (*wait_cp_suspend_ready)(struct link_pm_data *pmdata,
				struct usb_device *udev);
	int (*check_cp_suspend_ready)(struct link_pm_data *pmdata);
#endif
};

extern void clear_runtime_error_nowait(struct device *dev);

#endif	/* __LINK_PM_HSIC_SHANNON_H__ */
