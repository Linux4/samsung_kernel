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

#include <linux/gpio.h>
#include <linux/usb.h>
#include <linux/irq.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/platform_data/modem_v2.h>
//#include <linux/pm_qos_params.h>
#include <mach/regs-gpio.h>
#include <linux/module.h>
#include "modem_prj.h"
#include "modem_utils.h"
#include "modem_link_device_hsic_ncm.h"
#include <plat/usb-phy.h>

#define HOSTWAKE_WAITQ_TIMEOUT		msecs_to_jiffies(10)
#define HOSTWAKE_WAITQ_RETRY		20

#define IOCTL_LINK_CONTROL_ENABLE	_IO('o', 0x30)
#define IOCTL_LINK_CONTROL_ACTIVE	_IO('o', 0x31)
#define IOCTL_LINK_CONNECTED		_IO('o', 0x33)
#define IOCTL_LINK_DEVICE_RESET		_IO('o', 0x44)
#define IOCTL_LINK_FREQ_LOCK		_IO('o', 0x45)
#define IOCTL_LINK_FREQ_UNLOCK		_IO('o', 0x46)

/* /sys/module/link_pm_hsic_m74xx/parameters/...*/
static int l2_delay = 200;
module_param(l2_delay, int, S_IRUGO);
MODULE_PARM_DESC(l2_delay, "HSIC autosuspend delay");

static int hub_delay = 500;
module_param(hub_delay, int, S_IRUGO);
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
	MAIN_CONNECT = 1 << 4,
	LOADER_CONNECT = 1 << 5,
};

struct link_pm_data {
	struct miscdevice miscdev;
	struct platform_device *pdev;
	struct modemlink_pm_data *pdata;

	struct notifier_block usb_nfb;
	struct notifier_block pm_nfb;
	struct notifier_block pm_qos_notifier;
	struct notifier_block phy_nfb;

	struct usb_device *hdev;
	struct usb_device *udev;

	struct list_head link;

	wait_queue_head_t hostwake_waitq;
	struct wake_lock wake;
	struct mutex lock;

	atomic_t status;
	atomic_t dir;
	atomic_t hostwake_status;

	bool pm_request;

	unsigned long irqflag;
	unsigned int irq;
};

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
	{0x04cc, 0x0500, BOOT_DEVICE}, /* Ericsson */
	{0x04cc, 0x2342, MAIN_DEVICE}, /* Ericsson */
};

struct link_pm_device {
	struct list_head pm_data;
	spinlock_t lock;
};

struct link_pm_device usb_devices;

static int (*generic_usb_suspend)(struct usb_device *, pm_message_t);
static int (*generic_usb_resume)(struct usb_device *, pm_message_t);

static inline int check_status(struct link_pm_data *pmdata, enum status exp)
{
	return atomic_read(&pmdata->status) & exp;
}

static inline void set_status(struct link_pm_data *pmdata, enum status status)
{
	atomic_set(&pmdata->status, status);
}

static inline int get_status(struct link_pm_data *pmdata)
{
	return atomic_read(&pmdata->status);
}

static inline int get_pm_status(struct link_pm_data *pmdata)
{
	return atomic_read(&pmdata->status) & 0x0F;
}

static inline void set_pm_status(struct link_pm_data *pmdata,
			enum status new_pm)
{
	int val = (atomic_read(&pmdata->status) & 0xF0) | new_pm;

	atomic_set(&pmdata->status, val);
}

static inline int get_direction(struct link_pm_data *pmdata)
{
	return atomic_read(&pmdata->dir);
}

static inline void set_direction(struct link_pm_data *pmdata,
			enum direction dir)
{
	atomic_set(&pmdata->dir, dir);
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
		if (pmdata && (udev == pmdata->udev ||
				udev == pmdata->hdev)) {
			spin_unlock_bh(&usb_devices.lock);
			return pmdata;
		}
		mif_debug("udev = %p, %s\n", udev, dev_name(&udev->dev));
	}
	spin_unlock_bh(&usb_devices.lock);

	return NULL;
}

#ifdef CONFIG_PM_RUNTIME
#ifndef CONFIG_OF
#define phy_register_notifier(nb) register_usb2phy_notifier(nb)
#define phy_unregister_notifier(nb) unregister_usb2phy_notifier(nb)
#endif

static int linkpm_phy_notify(struct notifier_block *nfb,
				unsigned long event, void *arg)
{
	struct link_pm_data *pmdata =
			container_of(nfb, struct link_pm_data, phy_nfb);
	struct modemlink_pm_data *pdata = pmdata->pdata;

	mif_info("get usb pm noti !!!\n");

	if (check_status(pmdata, MAIN_CONNECT)) {
		switch (event) {
		case STATE_HSIC_LPA_ENTER:
			gpio_set_value(pdata->gpio_link_active, 0);
			mif_info("enter: active state(%d)\n",
				gpio_get_value(pdata->gpio_link_active));
			break;
		case STATE_HSIC_PHY_SHUTDOWN:
			gpio_set_value(pdata->gpio_link_active, 0);
			mif_info("phy_exit: active state(%d)\n",
				gpio_get_value(pdata->gpio_link_active));
			mutex_lock(&pmdata->lock);
			if (pmdata->pm_request) {
				if (pmdata->udev)
					pm_request_resume(&pmdata->udev->dev);
				pmdata->pm_request = false;
			}
			mutex_unlock(&pmdata->lock);
			break;
		case STATE_HSIC_CHECK_HOSTWAKE:
			if (gpio_get_value(pdata->gpio_link_hostwake))
				return NOTIFY_BAD;
			break;
		case STATE_HSIC_LPA_WAKE:
		case STATE_HSIC_LPA_PHY_INIT:
			break;
		}
	}

	return NOTIFY_DONE;
}
#endif

static int wait_hostwake_value(struct link_pm_data *pmdata, int val)
{
	int cnt = 0;

	while (!wait_event_interruptible_timeout(pmdata->hostwake_waitq,
			atomic_read(&pmdata->hostwake_status) == val ||
			!check_status(pmdata, MAIN_CONNECT),
			HOSTWAKE_WAITQ_TIMEOUT)) {
		if (cnt++ >= HOSTWAKE_WAITQ_RETRY)
			return -ETIMEDOUT;
	}

	return 0;
}

static int link_pm_usb_resume(struct usb_device *udev, pm_message_t msg)
{
	int ret;

	int dir;
	int status;
	int hostactive;

	struct modemlink_pm_data *pdata;
	struct link_pm_data *pmdata = get_link_pm_data(udev);

	if (!pmdata)
		goto generic_resume;

	pdata = pmdata->pdata;

	hostactive = gpio_get_value(pdata->gpio_link_active);
	if (!hostactive)
		set_pm_status(pmdata, LINK_PM_L3);

	status = get_pm_status(pmdata);
	dir = get_direction(pmdata);

	mif_debug("udev: %p, %s\n", udev, dev_name(&udev->dev));

	switch (status) {
	case LINK_PM_L2:
		if (udev == pmdata->hdev) {
			mif_info("host phy resume first\n");
			goto generic_resume;
		} else if (udev == pmdata->udev) {
			mutex_lock(&pmdata->lock);
			if (pmdata->hdev->state != USB_STATE_CONFIGURED) {
				mutex_unlock(&pmdata->lock);
				pmdata->pm_request = true;

				return -EHOSTUNREACH;
			}
			mutex_unlock(&pmdata->lock);
		}

		if (dir == AP2CP) {
			mif_info("ap initiated L2 resume start\n");

			gpio_set_value(pdata->gpio_link_slavewake, 1);
			mif_info("slavewake = %d, hostwake = %d\n",
				gpio_get_value(pdata->gpio_link_slavewake),
				gpio_get_value(pdata->gpio_link_hostwake));

			if (wait_hostwake_value(pmdata, 1) < 0) {
				mif_err("wait_hostwake_value fail\n");
				pdata->cp_force_crash_exit();
			}
		} else
			mif_info("cp initiated L2 resume start\n");
		break;
	case LINK_PM_L3:
		if (udev == pmdata->hdev) {
			if (dir == AP2CP) {
				mif_info("ap initiated L3 resume start\n");
				gpio_set_value(pdata->gpio_link_slavewake, 1);
				mif_info("slavewake = %d\n",
					gpio_get_value(pdata->gpio_link_slavewake));

				msleep(20);
			} else
				mif_info("cp initiated L3 resume start\n");

			gpio_set_value(pdata->gpio_link_active, 1);
			mif_info("hostactive = %d\n",
				gpio_get_value(pdata->gpio_link_active));
		} else if (udev == pmdata->udev) {
			mif_err("no oob signal from cp\n");
			pdata->cp_force_crash_exit();
		}
		break;
	default:
		break;
	}

	if (!wake_lock_active(&pmdata->wake)) {
		wake_lock(&pmdata->wake);
		mif_debug("hold wakelock\n");
	}

generic_resume:
	ret = generic_usb_resume(udev, msg);

	if (pmdata) {
		switch (ret) {
		case 0:
			break;
		case -ENOTCONN:
		case -ESHUTDOWN:
		case -ENODEV:
			mif_err("cp_force_crash - " \
				"generic_resume fail(%d)\n", ret);
			pdata->cp_force_crash_exit();
			break;
		default:
			mif_err("usb_reset_device - " \
				"generic_resume fail(%d)\n", ret);
			usb_lock_device(pmdata->udev);
			if (usb_reset_device(pmdata->udev)) {
				mif_err("usb_reset_device fail(%d)\n", ret);
				pdata->cp_force_crash_exit();
			}
			usb_unlock_device(pmdata->udev);
			break;
		}
	}

	return ret;
}

static int link_pm_usb_suspend(struct usb_device *udev, pm_message_t msg)
{
	struct link_pm_data *pmdata = get_link_pm_data(udev);

	if (!pmdata)
		goto generic_suspend;

	if (udev == pmdata->hdev)
		goto generic_suspend;

	set_pm_status(pmdata, LINK_PM_L2);
	set_direction(pmdata, AP2CP);
	mif_info("L2\n");

	mif_debug("release wakelock\n");
	wake_lock_timeout(&pmdata->wake, msecs_to_jiffies(50));

generic_suspend:
	return generic_usb_suspend(udev, msg);
}

static void link_pm_enable(struct link_pm_data *link_pm)
{
	struct usb_device *udev = link_pm->udev;
	struct modemlink_pm_data *pdata = link_pm->pdata;

	struct device *dev = &udev->dev;
	struct device *hdev = udev->bus->root_hub->dev.parent;
	struct device *roothub = &udev->bus->root_hub->dev;

#if 0
	/* before enable_irq, clear the irq pending register
	   due to Exynos4 bsp's irq bug */
	__raw_writel(eint_irq_to_bit(link_pm->irq),
			S5P_EINT_PEND(EINT_REG_NR(link_pm->irq)));
#endif

	if (gpio_get_value(pdata->gpio_link_slavewake))
		gpio_set_value(pdata->gpio_link_slavewake, 0);

	pm_runtime_set_autosuspend_delay(dev, l2_delay);
	pm_runtime_set_autosuspend_delay(roothub, hub_delay);
	pm_runtime_allow(dev);
	pm_runtime_allow(hdev);	/* ehci */

	mif_info("LINK_PM_ENABLE\n");
	enable_irq(link_pm->irq);
}

static int link_pm_notify(struct notifier_block *nfb,
			unsigned long event, void *arg)
{
	struct link_pm_data *pmdata =
			container_of(nfb, struct link_pm_data, pm_nfb);

	if (!pmdata)
		return NOTIFY_DONE;

	if (!check_status(pmdata, MAIN_CONNECT))
		return NOTIFY_DONE;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		mif_info("going to suspend the system\n");
		if (wake_lock_active(&pmdata->wake))
			wake_unlock(&pmdata->wake);
		break;
	case PM_POST_SUSPEND:
		mif_info("suspend finished\n");
		break;
	}

	return NOTIFY_DONE;
}

static int link_pm_usb_notify(struct notifier_block *nfb,
			unsigned long event, void *arg)
{
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

			mif_info("link pm main dev connect\n");

			mutex_lock(&pmdata->lock);

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

			pmdata->pm_request = false;

			mutex_unlock(&pmdata->lock);

			mif_debug("hook: (%pf, %pf), (%pf, %pf)\n",
				generic_usb_resume, udrv->resume,
				generic_usb_suspend, udrv->suspend);

			pmdata->phy_nfb.notifier_call = linkpm_phy_notify;
			phy_register_notifier(&pmdata->phy_nfb);

			set_status(pmdata, MAIN_CONNECT | LINK_PM_L0);
			set_direction(pmdata, AP2CP);

			atomic_set(&pmdata->hostwake_status, 0);

			pmdata->irqflag = IRQF_TRIGGER_HIGH;
			irq_set_irq_type(pmdata->irq, pmdata->irqflag);

			if (pm_enable)
				link_pm_enable(pmdata);
			break;
		case BOOT_DEVICE:
			mif_info("link_pm boot dev connect\n");

			mutex_lock(&pmdata->lock);
			pmdata->udev = udev;
			mutex_unlock(&pmdata->lock);

			set_status(pmdata, LOADER_CONNECT);

			break;
		}
		break;
	case USB_DEVICE_REMOVE:
		switch (link_pm_known_device(desc)) {
		case MAIN_DEVICE:
			disable_irq(pmdata->irq);

			mif_info("link_pm main dev remove\n");

			pm_runtime_forbid(&udev->bus->root_hub->dev);

			phy_unregister_notifier(&pmdata->phy_nfb);

			set_status(pmdata, 0);
			set_direction(pmdata, AP2CP);

			atomic_set(&pmdata->hostwake_status, 0);

			wake_up(&pmdata->hostwake_waitq);

			mutex_lock(&pmdata->lock);

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
			pmdata->pm_request = false;

			mutex_unlock(&pmdata->lock);

			mif_debug("unhook: (%pf, %pf), (%pf, %pf)\n",
				generic_usb_resume, udrv->resume,
				generic_usb_suspend, udrv->suspend);
			break;
		case BOOT_DEVICE:
			mif_info("link_pm boot dev disconnect\n");

			set_status(pmdata, 0);

			mutex_lock(&pmdata->lock);
			pmdata->udev = NULL;
			mutex_unlock(&pmdata->lock);
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
	int hostwake;
	int slavewake;
	int hostactive;
	int status;

	struct link_pm_data *pmdata = data;
	struct modemlink_pm_data *pdata;

	if (!pmdata)
		return IRQ_HANDLED;

	if (get_pm_status(pmdata) == LINK_PM_L0)
		return IRQ_HANDLED;

	pdata = pmdata->pdata;

	status = get_pm_status(pmdata);
	hostwake = gpio_get_value(pdata->gpio_link_hostwake);
	slavewake = gpio_get_value(pdata->gpio_link_slavewake);
	hostactive = gpio_get_value(pdata->gpio_link_active);

	if (hostwake == atomic_read(&pmdata->hostwake_status)) {
		mif_err("spurious wake irq(%d/%ld)\n",
			hostwake, pmdata->irqflag);
		return IRQ_HANDLED;
	}
	atomic_set(&pmdata->hostwake_status, hostwake);

	mif_debug("irqf: %ld, slavewake: %d, hostwake: %d, hostactive: %d\n",
		pmdata->irqflag, slavewake, hostwake, hostactive);

	pmdata->irqflag = pmdata->irqflag == IRQF_TRIGGER_HIGH ?
			IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH;
	irq_set_irq_type(irq, pmdata->irqflag);
#if 0
	/* TODO: W/A for exynos4 bsp's level irq bug */
	__raw_writel(eint_irq_to_bit(irq), S5P_EINT_PEND(EINT_REG_NR(irq)));
#endif

	/* CP initiated resume start */
	if (hostwake && !slavewake) {
		mutex_lock(&pmdata->lock);
		if (pmdata->udev) {
			set_direction(pmdata, CP2AP);
			mif_debug("pm_request_resume\n");
			pm_request_resume(&pmdata->udev->dev);
		} else
			mif_info("pmdata->udev is NULL\n");
		mutex_unlock(&pmdata->lock);
		goto done;
	}

	/* AP initiated resuming */
	if (hostwake && slavewake) {
		wake_up(&pmdata->hostwake_waitq);
		goto done;
	}

	/* CP initiated resume finish */
	if (!hostwake && !slavewake) {
		mif_info("cp initiated L%d resume finish\n", status);
		set_pm_status(pmdata, LINK_PM_L0);
		set_direction(pmdata, AP2CP);
		goto done;
	}

	/* AP initiated resume finish */
	if (!hostwake && slavewake) {
		gpio_set_value(pdata->gpio_link_slavewake, 0);
		mif_info("slavewake = %d\n",
			gpio_get_value(pdata->gpio_link_slavewake));
		mif_info("ap initiated L%d resume finish\n", status);
		set_pm_status(pmdata, LINK_PM_L0);
		set_direction(pmdata, AP2CP);
		goto done;
	}
done:
	return IRQ_HANDLED;
}

#if 0
static int pm_qos_notifier_event(struct notifier_block *this,
				unsigned long qosval, void *ptr)
{
	struct link_pm_data *pmdata =
		container_of(this, struct link_pm_data, pm_qos_notifier);

	if (!pmdata->udev || !pmdata->pdata->freq_lock ||
		!pmdata->pdata->freq_unlock)
		return NOTIFY_OK;

	if (qosval >= 0)
		pmdata->pdata->freq_lock(&pmdata->udev->dev, qosval);
	else
		pmdata->pdata->freq_unlock(&pmdata->udev->dev);

	return NOTIFY_OK;
}
#endif

static long link_pm_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret;

	struct link_pm_data *pmdata = file->private_data;
	//struct modemlink_pm_data *pdata = pmdata->pdata;

	mif_info("%x\n", cmd);

	switch (cmd) {
	case IOCTL_LINK_CONNECTED:
		return get_status(pmdata) >> 4;
	case IOCTL_LINK_DEVICE_RESET:
		mutex_lock(&pmdata->lock);
		if (!check_status(pmdata, MAIN_CONNECT |
			LOADER_CONNECT)) {
			mutex_unlock(&pmdata->lock);
			return -ENODEV;
		}

		if (pmdata->udev) {
			usb_lock_device(pmdata->udev);
			ret = usb_reset_device(pmdata->udev);
			if (ret)
				mif_err("usb_reset_device fail(%d)\n", ret);
			usb_unlock_device(pmdata->udev);
		}
		mutex_unlock(&pmdata->lock);
		break;
	case IOCTL_LINK_FREQ_LOCK:
#if 0
		if (pdata->freq_lock && pmdata->udev)
			pdata->freq_lock(&pmdata->udev->dev, 25 * 1024);
#endif
		break;
	case IOCTL_LINK_FREQ_UNLOCK:
#if 0
		if (pdata->freq_unlock && pmdata->udev)
			pdata->freq_unlock(&pmdata->udev->dev);
#endif
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

	mutex_init(&pmdata->lock);
	spin_lock_bh(&usb_devices.lock);
	list_add(&pmdata->link, &usb_devices.pm_data);
	spin_unlock_bh(&usb_devices.lock);

	irq = gpio_to_irq(pdata->gpio_link_hostwake);

	pmdata->irqflag = IRQF_TRIGGER_HIGH;
	ret = request_threaded_irq(irq, NULL, link_pm_hostwake_handler,
		IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
		"hostwake", pmdata);
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

	pmdata->usb_nfb.notifier_call = link_pm_usb_notify;
	usb_register_notify(&pmdata->usb_nfb);

	pmdata->pm_nfb.notifier_call = link_pm_notify;
	register_pm_notifier(&pmdata->pm_nfb);

#if 0
	pmdata->pm_qos_notifier.notifier_call = pm_qos_notifier_event;
	pm_qos_add_notifier(PM_QOS_NETWORK_THROUGHPUT,
			&pmdata->pm_qos_notifier);
#endif

	wake_lock_init(&pmdata->wake, WAKE_LOCK_SUSPEND, "l2_hsic");
	init_waitqueue_head(&pmdata->hostwake_waitq);

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
	unregister_pm_notifier(&pmdata->pm_nfb);

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
	mif_debug("hostwake: %d\n", hostwake);
	if (hostwake)
		return -EBUSY;

	return 0;
}

static int link_pm_resume(struct device *dev)
{
	int hostactive;

	struct modemlink_pm_data *pdata;
	struct link_pm_data *pmdata = dev_get_drvdata(dev);

	if (!pmdata)
		return 0;

	if (!check_status(pmdata, MAIN_CONNECT))
		return 0;

	mif_info("\n");

	pdata = pmdata->pdata;

	hostactive = gpio_get_value(pdata->gpio_link_active);
	if (!hostactive) {
		mutex_lock(&pmdata->lock);
		if (pmdata->udev) {
			mif_debug("pm_request_resume\n");
			pm_request_resume(&pmdata->udev->dev);
		} else
			mif_info("pmdata's udev is NULL\n");
		mutex_unlock(&pmdata->lock);
	}

	return 0;
}

static int link_pm_resume_noirq(struct device *dev)
{
	struct link_pm_data *pmdata = dev_get_drvdata(dev);

	if (!pmdata)
		return 0;

	if (!check_status(pmdata, MAIN_CONNECT))
		return 0;

	mif_debug("enable hostwakeup irq\n");
	enable_irq(pmdata->irq);

	return 0;
}

static int link_pm_suspend_noirq(struct device *dev)
{
	int hostwake;

	struct modemlink_pm_data *pdata;
	struct link_pm_data *pmdata = dev_get_drvdata(dev);

	if (!pmdata)
		return 0;

	if (!check_status(pmdata, MAIN_CONNECT))
		return 0;

	mif_debug("disable hostwakeup irq\n");
	disable_irq(pmdata->irq);
	pdata = pmdata->pdata;

	hostwake = gpio_get_value(pdata->gpio_link_hostwake);
	mif_debug("hostwake: %d\n", hostwake);
	if (hostwake) {
		mif_debug("enable hostwakeup irq\n");
		enable_irq(pmdata->irq);
		return -EBUSY;
	}

	return 0;
}

static const struct dev_pm_ops link_pm_ops = {
	.suspend = link_pm_suspend,
	.resume = link_pm_resume,
	.resume_noirq = link_pm_resume_noirq,
	.suspend_noirq = link_pm_suspend_noirq,
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
