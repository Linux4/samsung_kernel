/******************************************************************************
*(C) Copyright 2011 Marvell International Ltd.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as published by
    the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <plat/mfp.h>
#include <linux/fs.h>
#include <asm/gpio.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <mach/gpio-edge.h>
#include <stddef.h>

enum {
	GE_DEBUG_DEBUG = 1U << 0,
	GE_DEBUG_INFO = 1U << 1,
	GE_DEBUG_ERROR = 1U << 2,
};

#define usim_event_debug(mask, x...) \
	do { \
		if (usim_debug_mask & mask) \
			printk(KERN_INFO"usim event: " x); \
	} while (0)

static uint32_t usim_debug_mask = GE_DEBUG_INFO | GE_DEBUG_ERROR;
module_param_named(debug_mask, usim_debug_mask, uint, S_IWUSR | S_IRUGO);

static uint32_t usim_wakeup_timeout_in_ms = 3000;
module_param_named(wakeup_timeout_in_ms, usim_wakeup_timeout_in_ms, uint,
		   S_IWUSR | S_IRUGO);

static uint32_t usim_delay_time_in_jiffies = HZ;
module_param_named(delay_time_in_jiffies, usim_delay_time_in_jiffies, uint,
		   S_IWUSR | S_IRUGO);

static struct class *usim_event_class;

struct usim_event_device {
	char name[16];

	struct device *dev;

	int enable;

	int mfp;
	int gpio;
	struct gpio_edge_desc desc;
	int irq;

	struct delayed_work work;
	struct workqueue_struct *wq;

	spinlock_t lock;
};

static struct usim_event_device usim_event_devices[] = {
	{
	 .mfp = MFP_PIN_GPIO101,
	 .gpio = mfp_to_gpio(MFP_PIN_GPIO101),
	},
};

static void report_usim_event(struct usim_event_device *uedev, int state)
{
	char name_buf[50];
	char *env[3];

	snprintf(name_buf, sizeof(name_buf), "USIM_NAME=%s", uedev->name);

	env[0] = name_buf;
	env[1] = state ? "USIM_EVENT=plugin" : "USIM_EVENT=plugout";
	env[2] = NULL;

	kobject_uevent_env(&uedev->dev->kobj, KOBJ_CHANGE, env);
	usim_event_debug(GE_DEBUG_INFO, "%s: usim uevent [%s %s] is sent\n",
			 __func__, env[0], env[1]);
}

static void usim_event_work(struct work_struct *work)
{
	struct usim_event_device *uedev =
	    container_of(to_delayed_work(work), struct usim_event_device, work);
	int state = !!gpio_get_value(uedev->gpio);

	report_usim_event(uedev, state);
}

static void usim_event_wakeup(int mfp, void *data)
{
	struct usim_event_device *uedev = (struct usim_event_device *)data;
	pm_wakeup_event(uedev->dev, usim_wakeup_timeout_in_ms);
}

irqreturn_t usim_event_handler(int irq, void *dev_id)
{
	struct usim_event_device *uedev = (struct usim_event_device *)dev_id;
	unsigned long flags = 0;
	spin_lock_irqsave(&uedev->lock, flags);
	queue_delayed_work(uedev->wq, &uedev->work, usim_delay_time_in_jiffies);
	spin_unlock_irqrestore(&uedev->lock, flags);

	usim_event_debug(GE_DEBUG_INFO,
			 "%s: gpio event irq received. irq[%d] \n", __func__,
			 irq);
	return IRQ_HANDLED;
}

int usim_enable_interrupt(struct usim_event_device *uedev, int enable)
{
	int ret = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&uedev->lock, flags);
	if(enable != uedev->enable) {
		usim_event_debug(GE_DEBUG_INFO,
			 "%s: %s interrupt\n", __func__,
			 enable ? "enable" : "disable");
		if(enable) {
			ret = mmp_gpio_edge_add(&uedev->desc);
			if (ret < 0) {
				usim_event_debug(GE_DEBUG_ERROR, "%s: gpio edge add failed!\n",
					__func__);
			}
			enable_irq(uedev->irq);
		} else {
			disable_irq(uedev->irq);
			ret = mmp_gpio_edge_del(&uedev->desc);
			if (ret < 0) {
				usim_event_debug(GE_DEBUG_ERROR, "%s: gpio edge del failed!\n",
					__func__);
			}
		}
		uedev->enable = enable;
	}

	spin_unlock_irqrestore(&uedev->lock, flags);

	return ret;
}

static ssize_t usim_send_event(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct usim_event_device *uedev =
	    (struct usim_event_device *)dev_get_drvdata(dev);

	int state = simple_strtoul(buf, NULL, 10);
	report_usim_event(uedev, state);
	return count;
}

static ssize_t usim_show_state(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct usim_event_device *uedev =
	    (struct usim_event_device *)dev_get_drvdata(dev);

	int len;
	int state = !!gpio_get_value(uedev->gpio);
	len = sprintf(buf, "%d\n", state);
	return len;
}

static ssize_t usim_show_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct usim_event_device *uedev =
	    (struct usim_event_device *)dev_get_drvdata(dev);

	int len;
	len = sprintf(buf, "%d\n", uedev->enable);
	return len;
}

static ssize_t usim_enable(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct usim_event_device *uedev =
	    (struct usim_event_device *)dev_get_drvdata(dev);

	int enable = simple_strtoul(buf, NULL, 10);
	usim_enable_interrupt(uedev, enable);
	return count;
}

static DEVICE_ATTR(send_event, 0644, NULL, usim_send_event);
static DEVICE_ATTR(state, 0644, usim_show_state, NULL);
static DEVICE_ATTR(enable, 0666, usim_show_enable, usim_enable);
static struct device_attribute *usim_event_attr[] = {
	&dev_attr_send_event,
	&dev_attr_state,
	&dev_attr_enable,
	NULL
};

static int usim_event_create_sys_device(struct device *dev)
{
	int ret = 0;
	struct device_attribute **attr = usim_event_attr;

	for (; *attr; ++attr) {
		ret = device_create_file(dev, *attr);
		if (ret)
			break;
	}

	if (ret) {
		for (--attr; attr >= usim_event_attr; --attr)
			device_remove_file(dev, *attr);
	}
	return 0;
}

static int usim_event_remove_sys_device(struct device *dev)
{
	struct device_attribute **attr = usim_event_attr;

	for (; *attr; ++attr) {
		device_remove_file(dev, *attr);
	}

	return 0;
}

static int init_device(struct usim_event_device *uedev, int idx)
{
	int ret = -1;

	snprintf(uedev->name, sizeof(uedev->name) - 1, "usim%d", idx);
	spin_lock_init(&uedev->lock);

	uedev->desc.mfp = uedev->mfp;
	uedev->desc.gpio = mfp_to_gpio(uedev->gpio);
	uedev->desc.type = MFP_LPM_EDGE_BOTH;
	uedev->desc.data = uedev;
	uedev->desc.handler = usim_event_wakeup;
	if(uedev->enable) {
		ret = mmp_gpio_edge_add(&uedev->desc);
		if (ret < 0) {
			usim_event_debug(GE_DEBUG_ERROR, "%s: gpio edge add failed!\n",
				__func__);
			goto out;
		}
	}

	ret = gpio_request(uedev->gpio, uedev->name);
	if (ret < 0) {
		usim_event_debug(GE_DEBUG_ERROR,
				 "%s: GPIO[%d] request failed!\n", __func__,
				 uedev->gpio);
		goto del_gpio_edge;
	}

	gpio_direction_input(uedev->gpio);

	uedev->irq = gpio_to_irq(uedev->gpio);

	ret =
	    request_irq(uedev->irq, usim_event_handler,
			IRQF_DISABLED | IRQF_TRIGGER_RISING |
			IRQF_NO_SUSPEND, uedev->name,
			uedev);
	if (ret < 0) {
		usim_event_debug(GE_DEBUG_ERROR, "%s: request irq failed!\n",
				 __func__);
		goto free_gpio;
	}

	if(!uedev->enable)
		disable_irq(uedev->irq);

	INIT_DELAYED_WORK(&uedev->work, usim_event_work);
	uedev->wq = create_workqueue(uedev->name);
	if (uedev->wq == NULL) {
		usim_event_debug(GE_DEBUG_ERROR,
				 "%s:Can't create work queue!\n", __func__);
		ret = -ENOMEM;
		goto free_irq;
	}

	uedev->dev = device_create(usim_event_class, NULL,
				   MKDEV(0, idx), NULL, uedev->name);
	if (IS_ERR(uedev->dev))
	{
		ret = PTR_ERR(uedev->dev);
		goto destroy_wq;
	}

	dev_set_drvdata(uedev->dev, uedev);

	ret = usim_event_create_sys_device(uedev->dev);
	if (ret < 0) {
		usim_event_debug(GE_DEBUG_ERROR,
				 "%s: create sys device failed!\n", __func__);
		goto destroy_device;
	}

	ret = 0;
	goto out;

destroy_device:
	device_destroy(usim_event_class, MKDEV(0, idx));
destroy_wq:
	destroy_workqueue(uedev->wq);
free_irq:
	free_irq(uedev->irq, uedev);
free_gpio:
	gpio_free(uedev->gpio);
del_gpio_edge:
	if(uedev->enable)
		mmp_gpio_edge_del(&uedev->desc);
out:
	return ret;
}

static void deinit_device(struct usim_event_device *uedev, int idx)
{
	usim_event_remove_sys_device(uedev->dev);
	device_destroy(usim_event_class, MKDEV(0, idx));
	if (uedev->wq != NULL)
		destroy_workqueue(uedev->wq);
	free_irq(uedev->irq, uedev);
	gpio_free(uedev->gpio);
	if(uedev->enable)
		mmp_gpio_edge_del(&uedev->desc);
}

static int __init usim_event_init(void)
{
	int ret;
	int i = 0;

	usim_event_class = class_create(THIS_MODULE, "usim_event");
	if (IS_ERR(usim_event_class))
		return PTR_ERR(usim_event_class);

	for (i = 0; i < ARRAY_SIZE(usim_event_devices); ++i) {
		ret = init_device(&usim_event_devices[i], i);
		if (ret < 0)
			goto deinit;
	}

	return 0;

deinit:
	for (--i; i >= 0; --i) {
		deinit_device(&usim_event_devices[i], i);
	}
	class_destroy(usim_event_class);

	return -1;
}

static void __exit usim_event_exit(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(usim_event_devices); ++i) {
		deinit_device(&usim_event_devices[i], i);
	}
	class_destroy(usim_event_class);
}

module_init(usim_event_init);
module_exit(usim_event_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvell");
MODULE_DESCRIPTION("Marvell USIM event notify");
