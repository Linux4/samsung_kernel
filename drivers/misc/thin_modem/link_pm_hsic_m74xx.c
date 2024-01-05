/* /linux/drivers/misc/modem_if/modem_link_pm_m74xx.c
 *
 * Copyright (C) 2012 Google, Inc.
 * Copyright (C) 2012 Samsung Electronics.
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
#define DEBUG

#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/usb.h>
#include <linux/pm_qos.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/platform_data/modem_v2.h>

#include <plat/usb-phy.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "modem_link_device_hsic_ncm.h"

#include "link_pm_hsic_m74xx.h"

#define HOSTWAKE_WAITQ_TIMEOUT		msecs_to_jiffies(20)
#define HOSTWAKE_WAITQ_RETRY		20

#ifdef CONFIG_WAIT_CP_SUSPEND_READY
#define CP_SUSPEND_WAITQ_TIMEOUT	msecs_to_jiffies(20)
#define CP_SUSPEND_WAITQ_RETRY		25
#endif

#define IOCTL_LINK_CONNECTED		_IO('o', 0x33)
#define IOCTL_LINK_DEVICE_RESET		_IO('o', 0x44)

/* /sys/module/link_pm_hsic_m74xx/parameters/...*/
static int l2_delay = 200;
module_param(l2_delay, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(l2_delay, "HSIC autosuspend delay");

static int hub_delay = 100;
module_param(hub_delay, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(hub_delay, "Root-hub autosuspend delay");

static int pm_enable = 1;
module_param(pm_enable, int, S_IRUGO);
MODULE_PARM_DESC(pm_enable, "HSIC PM enable");

enum direction {
	AP2CP,
	CP2AP,
};

enum status {
	LINK_PM_L0 = 0,
	LINK_PM_L2 = 2,
	LINK_PM_L3 = 3,
	LINK_PM_RESUMING = 1 << 3,
	MAIN_CONNECT = 1 << 4,
	LOADER_CONNECT = 1 << 5,
	CP_CRASH = 1 << 6,
};

#ifdef CONFIG_WAIT_CP_SUSPEND_READY
enum cp_suspend_status {
	CP_SUSPEND_READY,
	CP_SUSPEND_OK,
};
#endif

enum link_pm_dev_type {
	UNKNOWN_DEVICE,
	BOOT_DEVICE,
	MAIN_DEVICE,
};

struct link_pm_dev_id {
	int vid;
	int pid;
	enum link_pm_dev_type type;
};

static struct link_pm_dev_id id[] = {
	{0x04cc, 0x0500, BOOT_DEVICE},
	{0x04cc, 0x2342, MAIN_DEVICE},
};

struct link_pm_device {
	struct list_head pm_data;
	spinlock_t lock;
};

struct link_pm_device usb_devices;

static int (*generic_usb_suspend)(struct usb_device *, pm_message_t);
static int (*generic_usb_resume)(struct usb_device *, pm_message_t);

#define check_status(p, exp)		(atomic_read(&p->status) & exp)
#define clear_status(p)			(atomic_set(&p->status, 0))

#define get_connect_status(p)		((atomic_read(&p->status) >> 4) & 0x03)
#define set_connect_status(p, val)	(atomic_set(&p->status,	\
					(atomic_read(&p->status) & 0x0F) | val))

#define set_crash_status(p, val)	(atomic_set(&p->status, \
					atomic_read(&p->status) | val))

#define get_pm_status(p)		(atomic_read(&p->status) & 0x03)
#define set_pm_status(p, val)		(atomic_set(&p->status,	\
					(atomic_read(&p->status) & 0xF8) | val))

#define set_pm_resuming(p)		(atomic_set(&p->status, \
					atomic_read(&p->status) | LINK_PM_RESUMING))
#define is_pm_resuming(p)		(atomic_read(&p->status) & LINK_PM_RESUMING)
#define clear_pm_resuming(p)		(atomic_set(&p->status, \
					atomic_read(&p->status) & ~LINK_PM_RESUMING))

#define get_direction(p)		(atomic_read(&p->dir))
#define set_direction(p, val)		(atomic_set(&p->dir, val))

#define get_hostwake_status(p)		(atomic_read(&p->hostwake_status))
#define set_hostwake_status(p, val)	(atomic_set(&p->hostwake_status, val))

#ifdef CONFIG_WAIT_CP_SUSPEND_READY
#define get_cp_suspend_ready(p)		(atomic_read(&p->cp_suspend_ready))
#define set_cp_suspend_ready(p, val)	(atomic_set(&p->cp_suspend_ready, val))

#define get_cp2ap_status(p)		(atomic_read(&p->cp2ap_status))
#define set_cp2ap_status(p, val)	(atomic_set(&p->cp2ap_status, val))
#endif

struct raw_notifier_head cp_crash_notifier;
int register_cp_crash_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&cp_crash_notifier, nb);
}

static void notifier_call_cp_crash(struct link_pm_data *pmdata)
{
	struct modemlink_pm_data *pdata = pmdata->pdata;

	if (!check_status(pmdata, CP_CRASH)) {
		set_crash_status(pmdata, CP_CRASH);
		pmdata->udev->can_submit = 0;

		if (gpio_get_value(pdata->gpio_link_slavewake))
			gpio_direction_output(pdata->gpio_link_slavewake, 0);

		mif_info("CP CRASH EVENT\n");
		raw_notifier_call_chain(&cp_crash_notifier,
				CP_FORCE_CRASH_EVENT, NULL);
	}
}

static int link_pm_known_device(const struct usb_device_descriptor *desc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(id); i++)
		if (id[i].vid == desc->idVendor && id[i].pid == desc->idProduct)
			return id[i].type;

	return UNKNOWN_DEVICE;
}

static struct link_pm_data *get_link_pm_data(struct usb_device *udev)
{
	struct link_pm_data *pmdata = NULL;

	spin_lock_bh(&usb_devices.lock);
	list_for_each_entry(pmdata, &usb_devices.pm_data, link) {
		if (pmdata && (udev == pmdata->udev || udev == pmdata->hdev)) {
			spin_unlock_bh(&usb_devices.lock);
			return pmdata;
		}
	}
	spin_unlock_bh(&usb_devices.lock);

	return NULL;
}

#ifdef CONFIG_PM_RUNTIME
static int link_pm_phy_notify(struct notifier_block *nfb,
				unsigned long event, void *arg)
{
	struct modemlink_pm_data *pdata;
	struct link_pm_data *pmdata =
			container_of(nfb, struct link_pm_data, phy_nfb);

	mif_info("event: %ld\n", event);

	if (!pmdata)
		return NOTIFY_DONE;

	pdata = pmdata->pdata;

	if (check_status(pmdata, MAIN_CONNECT)) {
		switch (event) {
		case STATE_HSIC_LPA_ENTER:	/* event 0 */
		case STATE_HSIC_PHY_SHUTDOWN:	/* event 4 */
			if (gpio_get_value(pdata->gpio_link_active)) {
				gpio_set_value(pdata->gpio_link_active, 0);
				mif_info("phy_exit: active state(%d)\n",
					gpio_get_value(pdata->gpio_link_active));

				set_pm_status(pmdata, LINK_PM_L3);
			}
			break;
		case STATE_HSIC_CHECK_HOSTWAKE: /* event 5 */
			if (gpio_get_value(pdata->gpio_link_hostwake))
				return NOTIFY_BAD;
			break;
		case STATE_HSIC_LPA_WAKE:	/* event 1 */
		case STATE_HSIC_LPA_PHY_INIT:	/* event 2 */
			break;
		}
	}

	return NOTIFY_DONE;
}
#endif

static int link_pm_qos_notify(struct notifier_block *nfb,
				unsigned long event, void *arg)
{
	struct link_pm_data *pmdata =
			container_of(nfb, struct link_pm_data, qos_nfb);

	if (!pmdata->pdata->freq_lock || !pmdata->pdata->freq_unlock)
		return NOTIFY_OK;

	if (event)
		pmdata->pdata->freq_lock(event);
	else
		pmdata->pdata->freq_unlock();

	return NOTIFY_OK;
}

static int wait_hostwake_value(struct link_pm_data *pmdata, int val)
{
	int ret;

	int cnt = HOSTWAKE_WAITQ_RETRY;

	struct modemlink_pm_data *pdata = pmdata->pdata;

	mif_info("\n");

	while (cnt--) {
		ret = wait_event_interruptible_timeout(pmdata->hostwake_waitq,
			get_hostwake_status(pmdata) == val,
			HOSTWAKE_WAITQ_TIMEOUT);
		if (!check_status(pmdata, MAIN_CONNECT) ||
			check_status(pmdata, CP_CRASH))
			return -ENODEV;

		usb_mark_last_busy(pmdata->hdev);

		if (ret)
			return 0;

		mif_info("hostwake: %d\n",
			gpio_get_value(pdata->gpio_link_hostwake));
	}

	mif_err("timeout!\n");

	return -ETIMEDOUT;
}

#ifdef CONFIG_WAIT_CP_SUSPEND_READY
static int wait_cp2ap_status_value(struct link_pm_data *pmdata,
			struct usb_device *udev, int val)
{
	int ret;

	int cnt = CP_SUSPEND_WAITQ_RETRY;
	unsigned long expires = 0;

	struct modemlink_pm_data *pdata = pmdata->pdata;

	mif_info("\n");

	while (cnt--) {
		ret = wait_event_interruptible_timeout(pmdata->cp_suspend_ready_waitq,
			get_cp2ap_status(pmdata) == val,
			CP_SUSPEND_WAITQ_TIMEOUT);
		if (!check_status(pmdata, MAIN_CONNECT) ||
			check_status(pmdata, CP_CRASH))
			return -ENODEV;

		/* check udev disconnection */
		if (udev->state == USB_STATE_NOTATTACHED)
			return 0;

		if (ret)
			return 0;

		mif_info("cp2ap_status: %d\n",
			gpio_get_value(pdata->gpio_link_cp2ap_status));

		expires = pm_runtime_autosuspend_expiration(&udev->dev);
		if (expires)
			cnt = CP_SUSPEND_WAITQ_RETRY;
	}

	mif_err("timeout\n");

	return -ETIMEDOUT;
}

static int wait_cp_suspend_ready(struct link_pm_data *pmdata,
			struct usb_device *udev)
{
	int ret;

	struct modemlink_pm_data *pdata = pmdata->pdata;

	if (get_cp_suspend_ready(pmdata) == CP_SUSPEND_READY)
		return 0;

	if (!gpio_get_value(pdata->gpio_link_slavewake)) {
		gpio_direction_output(pdata->gpio_link_slavewake, 1);
		mif_info("slavewake: %d\n",
			gpio_get_value(pdata->gpio_link_slavewake));
		set_cp_suspend_ready(pmdata, CP_SUSPEND_READY);
	}

	ret = wait_cp2ap_status_value(pmdata, udev, 1);
	if (ret < 0) {
		/* Revert to the L0 state */
		set_cp_suspend_ready(pmdata, CP_SUSPEND_OK);

		notifier_call_cp_crash(pmdata);
		return ret;
	}

	return ret;
}

static int check_cp_suspend_ready(struct link_pm_data *pmdata)
{
	struct modemlink_pm_data *pdata = pmdata->pdata;

	if (!gpio_get_value(pdata->gpio_link_slavewake) &&
		gpio_get_value(pdata->gpio_link_cp2ap_status)) {
		return RPM_SUSPENDING;
	}

	return 0;
}
#endif

static void link_pm_usb_set_autosuspended(struct usb_device *udev)
{
	pm_runtime_disable(&udev->dev);
	pm_runtime_set_suspended(&udev->dev);
	pm_runtime_enable(&udev->dev);

	mif_info("%s\n", dev_name(&udev->dev));
}

static int link_pm_usb_hdev_check_resume(struct link_pm_data *pmdata)
{
	int ret;

	unsigned long flags;

	if (!pmdata->hdev)
		return -ENODEV;

	spin_lock_irqsave(&pmdata->lock, flags);
	if (pmdata->hdev->state != USB_STATE_SUSPENDED) {
		spin_unlock_irqrestore(&pmdata->lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&pmdata->lock, flags);

	mif_info("root hub resume first\n");
	ret = pm_runtime_resume(&pmdata->hdev->dev);
	if (ret < 0) {
		mif_err("root hub pm_runtime_resume fail(%d)\n", ret);
		return ret;
	}

	return ret;
}

static int link_pm_usb_ap_init_resume_gpio_ctrl(struct link_pm_data *pmdata)
{
	int ret;

	struct modemlink_pm_data *pdata = pmdata->pdata;

	gpio_direction_output(pdata->gpio_link_slavewake, 1);
	mif_info("AP init'ed L%d resume start - %d/%d/%d\n",
			get_pm_status(pmdata),
			gpio_get_value(pdata->gpio_link_slavewake),
			gpio_get_value(pdata->gpio_link_hostwake),
			gpio_get_value(pdata->gpio_link_active));

	ret = wait_hostwake_value(pmdata, 1);
	if (ret < 0)
		mif_err("wait_hostwake_value fail\n");

	return ret;
}

static int link_pm_usb_resume(struct usb_device *udev, pm_message_t msg)
{
	int ret;

	int dir;
	int status;

	struct modemlink_pm_data *pdata = NULL;
	struct link_pm_data *pmdata = get_link_pm_data(udev);

	if (!pmdata)
		goto generic_resume;

	pdata = pmdata->pdata;

	if (!check_status(pmdata, MAIN_CONNECT))
		goto generic_resume;

	if (check_status(pmdata, CP_CRASH))
		goto generic_resume;

	/* before resume, make sure the pm status is LINK_PM_L3 or not */
	if (!gpio_get_value(pdata->gpio_link_active))
		set_pm_status(pmdata, LINK_PM_L3);
	status = get_pm_status(pmdata);
	dir = get_direction(pmdata);

	if (!PMSG_IS_AUTO(msg) && dir != CP2AP) {
		mif_info("%s will be resumed later\n", dev_name(&udev->dev));
		udev->can_submit = 0;

		return -EBUSY;
	}

	wake_lock(&pmdata->wake);

	if (pmdata->dpm_suspending) {
		mif_info("dpm_suspending, will be resume later\n");
		udev->can_submit = 0;

		return -EBUSY;
	}

	mif_debug("%s - dir: %d status: %d pm event: %x pm state: %d/%d\n",
		dev_name(&udev->dev), dir, status, msg.event,
		pmdata->udev->state, pmdata->hdev->state);

	usb_mark_last_busy(udev);
	set_pm_resuming(pmdata);

	switch (status) {
	case LINK_PM_L2:
		if (udev == pmdata->hdev)
			goto generic_resume;
		else {
			ret = link_pm_usb_hdev_check_resume(pmdata);
			if (ret < 0)
				goto err_exit;
		}

		dir = get_direction(pmdata);
		if (dir == AP2CP) {
			ret = link_pm_usb_ap_init_resume_gpio_ctrl(pmdata);
			if (ret < 0)
				goto err_exit;
		}

		break;
	case LINK_PM_L3:
		udev->reset_resume = 1;
		if (udev == pmdata->hdev) {
			dir = get_direction(pmdata);
			if (dir == AP2CP) {
				/* delay for CP to wake up from hibernation */
				mif_info("10ms delay for waiting CP\n");
				mdelay(10);

				ret = link_pm_usb_ap_init_resume_gpio_ctrl(
						pmdata);
				if (ret < 0)
					goto err_exit;
			}
			if (!gpio_get_value(pdata->gpio_link_active))
				gpio_set_value(pdata->gpio_link_active, 1);
		} else {
			ret = link_pm_usb_hdev_check_resume(pmdata);
			if (ret < 0)
				goto err_exit;
		}

		break;
	default:
		break;
	}

generic_resume:
	ret = generic_usb_resume(udev, msg);
	if (ret < 0)
		goto err_exit;

	if (pmdata) {
		if (udev == pmdata->udev) {
			if (gpio_get_value(pdata->gpio_link_slavewake))
				gpio_direction_output(pdata->gpio_link_slavewake, 0);

			clear_pm_resuming(pmdata);
			set_pm_status(pmdata, LINK_PM_L0);
		}

		mif_info("%s resumed by pm event %x - %d/%d/%d\n",
			dev_name(&udev->dev), msg.event,
			gpio_get_value(pdata->gpio_link_slavewake),
			gpio_get_value(pdata->gpio_link_hostwake),
			gpio_get_value(pdata->gpio_link_active));
	}

	return ret;

err_exit:
	if (pmdata) {
		mif_err("%s usb_resume fail(%d) - %d/%d/%d\n",
			dev_name(&udev->dev), ret,
			gpio_get_value(pdata->gpio_link_slavewake),
			gpio_get_value(pdata->gpio_link_hostwake),
			gpio_get_value(pdata->gpio_link_active));

		notifier_call_cp_crash(pmdata);
		ret = 0;
	}

	return ret;
}

static int link_pm_usb_suspend(struct usb_device *udev, pm_message_t msg)
{
	int ret;

	struct link_pm_data *pmdata = get_link_pm_data(udev);
	struct modemlink_pm_data *pdata;

	if (pmdata) {
		if (check_status(pmdata, CP_CRASH))
			return -EBUSY;

		mif_debug("%s suspend by pm event: %x\n",
			dev_name(&udev->dev), msg.event);

		if (msg.event == PM_EVENT_SUSPEND)
			link_pm_usb_set_autosuspended(udev);
	}

	ret = generic_usb_suspend(udev, msg);

	if (ret < 0) {
		mif_info("%s generic_usb_suspend fail(%d)\n",
			dev_name(&udev->dev), ret);
		return ret;
	}

	if (!pmdata)
		goto suspend_exit;

	if (udev == pmdata->hdev)
		goto suspend_exit;

#ifdef CONFIG_WAIT_CP_SUSPEND_READY
	ret = wait_cp2ap_status_value(pmdata, udev, 0);
	if (ret < 0)
		notifier_call_cp_crash(pmdata);
#endif
	set_pm_status(pmdata, LINK_PM_L2);

	/* Check hostwake gpio to determine the correct direction.
	  CP could receive suspend signal and tried to set hostwake
	  high to request cp init'ed resume - this is on IRQ context! -
	  before finishing this suspend process in AP side. So to avoid
	  this confusion, we should make sure the right direction */
	pdata = pmdata->pdata;
	if (gpio_get_value(pdata->gpio_link_hostwake))
		set_direction(pmdata, CP2AP);
	else
		set_direction(pmdata, AP2CP);

	wake_lock_timeout(&pmdata->wake, msecs_to_jiffies(50));
	mif_info("L2 - release wakelock\n");

suspend_exit:
	return ret;
}

static void link_pm_enable(struct link_pm_data *pmdata)
{
	struct modemlink_pm_data *pdata = pmdata->pdata;
	struct device *ehci = pmdata->udev->bus->root_hub->dev.parent;

	if (gpio_get_value(pdata->gpio_link_slavewake))
		gpio_direction_output(pdata->gpio_link_slavewake, 0);

	pm_runtime_set_autosuspend_delay(&pmdata->udev->dev, l2_delay);
	pm_runtime_set_autosuspend_delay(&pmdata->hdev->dev, hub_delay);
	pm_runtime_allow(&pmdata->udev->dev);
	pm_runtime_allow(&pmdata->hdev->dev);

	if (ehci->power.runtime_auto == false)
		pm_runtime_allow(ehci);

	mif_info("LINK_PM_ENABLE\n");
	enable_irq(pmdata->irq);
#ifdef CONFIG_WAIT_CP_SUSPEND_READY
	enable_irq(pmdata->cp_suspend_ready_irq);
#endif
}

static int link_pm_notify(struct notifier_block *nfb,
					unsigned long event, void *arg)
{
	struct link_pm_data *pmdata =
			container_of(nfb, struct link_pm_data, pm_nfb);

	if (!pmdata)
		return NOTIFY_DONE;

	mif_debug("event: %ld\n", event);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		if (!wake_lock_active(&pmdata->wake))
			pmdata->dpm_suspending = true;
		break;
	case PM_POST_SUSPEND:
		pmdata->dpm_suspending = false;
		if (wake_lock_active(&pmdata->wake)) {
			if (is_pm_resuming(pmdata))
				return NOTIFY_DONE;

			if (get_pm_status(pmdata) != LINK_PM_L0) {
				link_pm_usb_set_autosuspended(pmdata->hdev);
				link_pm_usb_set_autosuspended(pmdata->udev);

				mif_debug("pm_request_resume\n");
				pm_request_resume(&pmdata->udev->dev);
			}
		}
		break;
	}

	return NOTIFY_DONE;
}


static int link_pm_usb_notify(struct notifier_block *nfb,
			unsigned long event, void *arg)
{
	unsigned long flags;

	struct usb_device *udev = arg;
	struct usb_device_driver *udrv =
			to_usb_device_driver(udev->dev.driver);
	const struct usb_device_descriptor *desc = &udev->descriptor;

	struct link_pm_data *pmdata =
			container_of(nfb, struct link_pm_data, usb_nfb);

	switch (event) {
	case USB_DEVICE_ADD:
		switch (link_pm_known_device(desc)) {
		case MAIN_DEVICE:
			if (pmdata->udev) {
				mif_info("pmdata is assigned to udev(%p)\n",
					pmdata->udev);
				return NOTIFY_DONE;
			}

			spin_lock_irqsave(&pmdata->lock, flags);
			pmdata->udev = udev;
			pmdata->hdev = udev->bus->root_hub;

			if (!generic_usb_resume && udrv->resume) {
				generic_usb_resume = udrv->resume;
				udrv->resume = link_pm_usb_resume;
			}
			if (!generic_usb_suspend && udrv->suspend) {
				generic_usb_suspend = udrv->suspend;
				udrv->suspend = link_pm_usb_suspend;
			}

			spin_unlock_irqrestore(&pmdata->lock, flags);

			pmdata->phy_nfb.notifier_call = link_pm_phy_notify;
			register_usb2phy_notifier(&pmdata->phy_nfb);

			pmdata->qos_nfb.notifier_call = link_pm_qos_notify;
			pm_qos_add_notifier(PM_QOS_NETWORK_THROUGHPUT,
					&pmdata->qos_nfb);

			pmdata->pm_nfb.notifier_call = link_pm_notify;
			register_pm_notifier(&pmdata->pm_nfb);

			set_connect_status(pmdata, MAIN_CONNECT);
			set_pm_status(pmdata, LINK_PM_L0);
			set_hostwake_status(pmdata, 0);
			set_direction(pmdata, AP2CP);

			pmdata->dpm_suspending = false;

			irq_set_irq_type(pmdata->irq, IRQF_TRIGGER_HIGH);
#ifdef CONFIG_WAIT_CP_SUSPEND_READY
			irq_set_irq_type(pmdata->cp_suspend_ready_irq,
						IRQF_TRIGGER_HIGH);
			pmdata->wait_cp_suspend_ready = wait_cp_suspend_ready;
			pmdata->check_cp_suspend_ready = check_cp_suspend_ready;
			set_cp_suspend_ready(pmdata, CP_SUSPEND_OK);
#endif
			dev_set_drvdata(&udev->dev, pmdata);

			if (pm_enable)
				link_pm_enable(pmdata);

			wake_lock(&pmdata->wake);

			mif_info("link pm main dev connect\n");

			break;
		case BOOT_DEVICE:
			spin_lock_irqsave(&pmdata->lock, flags);
			pmdata->udev = udev;
			spin_unlock_irqrestore(&pmdata->lock, flags);

			set_connect_status(pmdata, LOADER_CONNECT);

			mif_info("link_pm boot dev connect\n");

			break;
		}
		break;
	case USB_DEVICE_REMOVE:
		switch (link_pm_known_device(desc)) {
		case MAIN_DEVICE:
			disable_irq(pmdata->irq);
#ifdef CONFIG_WAIT_CP_SUSPEND_READY
			disable_irq(pmdata->cp_suspend_ready_irq);
#endif
			pm_runtime_forbid(&udev->bus->root_hub->dev);

			spin_lock_irqsave(&pmdata->lock, flags);
			if (generic_usb_resume) {
				udrv->resume = generic_usb_resume;
				generic_usb_resume = NULL;
			}
			if (!generic_usb_suspend) {
				udrv->suspend = generic_usb_suspend;
				generic_usb_suspend = NULL;
			}

			pmdata->hdev = NULL;
			pmdata->udev = NULL;

			set_hostwake_status(pmdata, 0);
			set_direction(pmdata, AP2CP);
			clear_status(pmdata);
			spin_unlock_irqrestore(&pmdata->lock, flags);

			unregister_usb2phy_notifier(&pmdata->phy_nfb);
			pm_qos_remove_notifier(PM_QOS_NETWORK_THROUGHPUT,
					&pmdata->qos_nfb);
			unregister_pm_notifier(&pmdata->pm_nfb);

			if (wake_lock_active(&pmdata->wake))
				wake_unlock(&pmdata->wake);

			wake_up_interruptible(&pmdata->hostwake_waitq);

#ifdef CONFIG_WAIT_CP_SUSPEND_READY
			set_cp_suspend_ready(pmdata, CP_SUSPEND_OK);
			wake_up_interruptible(&pmdata->cp_suspend_ready_waitq);
			pmdata->wait_cp_suspend_ready = NULL;
			pmdata->check_cp_suspend_ready = NULL;
#endif
			mif_info("link_pm main dev remove\n");

			break;
		case BOOT_DEVICE:
			spin_lock_irqsave(&pmdata->lock, flags);
			pmdata->udev = NULL;
			spin_unlock_irqrestore(&pmdata->lock, flags);

			clear_status(pmdata);

			mif_info("link_pm boot dev disconnect\n");

			break;
		}
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static irqreturn_t link_pm_hostwake_handler(int irq, void *data)
{
	int dir;
	int status;

	int hostwake;
	int slavewake;
	int hostactive;

	char *log = "fail";

	struct link_pm_data *pmdata = data;
	struct modemlink_pm_data *pdata;

	if (!pmdata)
		return IRQ_HANDLED;

	pdata = pmdata->pdata;

	slavewake = gpio_get_value(pdata->gpio_link_slavewake);
	hostwake = gpio_get_value(pdata->gpio_link_hostwake);
	hostactive = gpio_get_value(pdata->gpio_link_active);

	mif_info("hostwake: %d\n", hostwake);

	irq_set_irq_type(irq, hostwake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

	/* these two variable - dir and status - are for log message */
	dir = get_direction(pmdata);
	status = get_pm_status(pmdata) ? : LINK_PM_L2;
	if (!hostactive)
		status = LINK_PM_L3;

	if (hostwake == get_hostwake_status(pmdata)) {
		mif_err("spurious wake irq(%d)\n", hostwake);
		return IRQ_HANDLED;
	}
	set_hostwake_status(pmdata, hostwake);

	/* CP initiated resume start */
	if (hostwake && !slavewake) {
		spin_lock(&pmdata->lock);
		if (pmdata->udev) {
			dir = CP2AP;
			set_direction(pmdata, dir);
			wake_lock(&pmdata->wake);

			if (pmdata->dpm_suspending)
				mif_info("dpm_suspending\n");
			else
				pm_request_resume(&pmdata->udev->dev);

			log = "start";
		}
		spin_unlock(&pmdata->lock);

		goto done;
	}

	/* AP initiated resuming */
	if (hostwake && slavewake) {
		wake_up_interruptible(&pmdata->hostwake_waitq);
		log = "started";

		goto done;
	}

	/* CP initiated resume finish */
	if (!hostwake && !slavewake) {
		if (hostactive)
			log = "finish";
		/*
		else {
			Wait... CP'll send hostwake-up again.
			If AP receive hostwake-up sideband siganl and
			after that AP set hostactive low by notifier,
			pm status mismatch is occured between AP and CP.
			So in this case, CP set hostwake up low after receiving
			hostactive, and then retry sending hostwake-up high.
		}
		*/

		goto done;
	}

	/* AP initiated resume finish */
	if (!hostwake && slavewake) {
		if (hostactive)
			log = "finish";
		else
			goto err_phy_off;

		goto done;
	}
done:
	mif_info("%s init'ed L%d resume %s - %d/%d/%d\n",
		dir == AP2CP ? "AP" : "CP", status, log,
		gpio_get_value(pdata->gpio_link_slavewake),
		gpio_get_value(pdata->gpio_link_hostwake),
		gpio_get_value(pdata->gpio_link_active));

	return IRQ_HANDLED;

err_phy_off:
	mif_info("%s init'ed L%d resume fail - %d/%d/%d\n",
		dir == AP2CP ? "AP" : "CP", status,
		gpio_get_value(pdata->gpio_link_slavewake),
		gpio_get_value(pdata->gpio_link_hostwake),
		gpio_get_value(pdata->gpio_link_active));
	notifier_call_cp_crash(pmdata);

	return IRQ_HANDLED;
}

#ifdef CONFIG_WAIT_CP_SUSPEND_READY
static irqreturn_t link_pm_cp_suspend_ready_handler(int irq, void *data)
{
	int gpio_cp2ap_status;

	struct link_pm_data *pmdata = data;
	struct modemlink_pm_data *pdata;

	if (!pmdata)
		return IRQ_HANDLED;

	pdata = pmdata->pdata;
	gpio_cp2ap_status = gpio_get_value(pdata->gpio_link_cp2ap_status);

	mif_info("cp2ap_status: %d\n", gpio_cp2ap_status);

	irq_set_irq_type(irq, gpio_cp2ap_status ? IRQF_TRIGGER_LOW :
			IRQF_TRIGGER_HIGH);

	if (gpio_cp2ap_status == get_cp2ap_status(pmdata)) {
		mif_err("spurious wake irq(%d)\n", gpio_cp2ap_status);
		return IRQ_HANDLED;
	}
	set_cp2ap_status(pmdata, gpio_cp2ap_status);

	if (gpio_cp2ap_status)
		gpio_direction_output(pdata->gpio_link_slavewake, 0);
	else
		set_cp_suspend_ready(pmdata, CP_SUSPEND_OK);

	wake_up_interruptible(&pmdata->cp_suspend_ready_waitq);

	mif_info("%d/%d\n", gpio_get_value(pdata->gpio_link_slavewake),
		gpio_get_value(pdata->gpio_link_cp2ap_status));

	return IRQ_HANDLED;
}
#endif

static long link_pm_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret;

	struct link_pm_data *pmdata = file->private_data;

	mif_info("%x\n", cmd);

	switch (cmd) {
	case IOCTL_LINK_CONNECTED:
		return get_connect_status(pmdata);
	case IOCTL_LINK_DEVICE_RESET:
		if (!get_connect_status(pmdata))
			return -ENODEV;

		if (pmdata->udev &&
			pmdata->udev->state != USB_STATE_NOTATTACHED) {
			usb_lock_device(pmdata->udev);
			ret = usb_reset_device(pmdata->udev);
			if (ret)
				mif_err("usb_reset_device fail(%d)\n", ret);
			usb_unlock_device(pmdata->udev);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int link_pm_open(struct inode *inode, struct file *file)
{
	struct link_pm_data *pmdata =
		container_of(file->private_data,
				struct link_pm_data, miscdev);

	file->private_data = (void *)pmdata;

	return 0;
}

static int link_pm_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

static const struct file_operations link_pm_fops = {
	.owner = THIS_MODULE,
	.open = link_pm_open,
	.release = link_pm_release,
	.unlocked_ioctl = link_pm_ioctl,
};

static int link_pm_probe(struct platform_device *pdev)
{
	int ret;
	int irq;

	struct link_pm_data *pmdata;
	struct modemlink_pm_data *pdata = pdev->dev.platform_data;

	pmdata = kzalloc(sizeof(struct link_pm_data), GFP_KERNEL);
	if (!pmdata) {
		mif_err("link_pm_data kzalloc fail\n");
		return -ENOMEM;
	}

	dev_set_drvdata(&pdev->dev, pmdata);
	pmdata->pdata = pdata;

	pmdata->miscdev.name = "link_pm";
	pmdata->miscdev.fops = &link_pm_fops;
	pmdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	ret = misc_register(&pmdata->miscdev);
	if (ret < 0) {
		mif_err("link_pm misc device register fail(%d)\n", ret);
		goto err_misc_register;
	}

	spin_lock_init(&pmdata->lock);

	spin_lock_bh(&usb_devices.lock);
	list_add(&pmdata->link, &usb_devices.pm_data);
	spin_unlock_bh(&usb_devices.lock);

	wake_lock_init(&pmdata->wake, WAKE_LOCK_SUSPEND, "l2_hsic");

	/* hostwake irq handler */
	irq = gpio_to_irq(pdata->gpio_link_hostwake);
	ret = request_irq(irq, link_pm_hostwake_handler,
		IRQF_NO_SUSPEND | IRQF_TRIGGER_HIGH, "hostwake", pmdata);
	if (ret) {
		mif_err("hostwake request_irq fail(%d)\n", ret);
		goto err_request_irq;
	}
	disable_irq(irq);

	ret = enable_irq_wake(irq);
	if (ret) {
		mif_err("hostwake enable_irq_wake fail(%d)\n", ret);
		goto err_enable_irq_wake;
	}
	pmdata->irq = irq;
	init_waitqueue_head(&pmdata->hostwake_waitq);

#ifdef CONFIG_WAIT_CP_SUSPEND_READY
	/* suspend ready irq handler */
	irq = gpio_to_irq(pdata->gpio_link_cp2ap_status);
	ret = request_irq(irq, link_pm_cp_suspend_ready_handler,
			IRQF_TRIGGER_HIGH, "cp_suspend_ready", pmdata);
	if (ret) {
		mif_err("cp_suspend_ready request_irq fail(%d)\n", ret);
		goto err_enable_irq_wake;
	}
	disable_irq(irq);
	pmdata->cp_suspend_ready_irq = irq;
	init_waitqueue_head(&pmdata->cp_suspend_ready_waitq);
#endif

	pmdata->usb_nfb.notifier_call = link_pm_usb_notify;
	usb_register_notify(&pmdata->usb_nfb);

	mif_info("success");

	return 0;

err_enable_irq_wake:
	free_irq(irq, pmdata);
err_request_irq:
	misc_deregister(&pmdata->miscdev);
err_misc_register:
	kfree(pmdata);

	return ret;
}

static int link_pm_remove(struct platform_device *pdev)
{
	struct link_pm_data *pmdata = dev_get_drvdata(&pdev->dev);

	if (!pmdata)
		return 0;

	usb_unregister_notify(&pmdata->usb_nfb);
	spin_lock_bh(&usb_devices.lock);
	list_del(&pmdata->link);
	spin_unlock_bh(&usb_devices.lock);

	kfree(pmdata);

	return 0;
}

static int link_pm_suspend(struct device *dev)
{
	int hostwake;

	struct modemlink_pm_data *pdata;
	struct link_pm_data *pmdata = dev_get_drvdata(dev);

	if (!pmdata)
		return 0;

	if (!check_status(pmdata, MAIN_CONNECT))
		return 0;

	pdata = pmdata->pdata;

	hostwake = gpio_get_value(pdata->gpio_link_hostwake);
	if (hostwake) {
		mif_info("hostwake: %d\n", hostwake);
		set_direction(pmdata, CP2AP);
		return -EBUSY;
	}

	return 0;
}

static const struct dev_pm_ops link_pm_ops = {
	.suspend = link_pm_suspend,
};

static struct platform_driver link_pm_driver = {
	.probe = link_pm_probe,
	.remove = link_pm_remove,
	.driver = {
		.name = "link_pm_hsic",
		.owner = THIS_MODULE,
		.pm = &link_pm_ops,
	},
};

static int __init link_pm_init(void)
{
	int ret;

	INIT_LIST_HEAD(&usb_devices.pm_data);
	spin_lock_init(&usb_devices.lock);

	ret = platform_driver_register(&link_pm_driver);
	if (ret)
		mif_err("link_pm driver register fail(%d)\n", ret);

	return ret;
}

static void __exit link_pm_exit(void)
{
	platform_driver_unregister(&link_pm_driver);
}

late_initcall(link_pm_init);
module_exit(link_pm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Samsung Modem Interface Runtime PM Driver");
