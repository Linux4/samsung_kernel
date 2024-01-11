/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/compat.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/log2.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif

#define PTAG		'T'
#define AUDIO_VTRIG_WAKEUP_READ		_IOW(PTAG, 0, int)
#define AUDIO_VTRIG_WAKEUP_WRITE	_IOW(PTAG, 1, int)
#define AUDIO_VTRIG_WLOCK_GET		_IO(PTAG, 2)
#define AUDIO_VTRIG_WLOCK_PUT		_IO(PTAG, 3)
#define AUDIO_VTRIG_ENABLE_TRIGGER	_IOW(PTAG, 4, int)
#define AUDIO_VTRIG_DISABLE_TRIGGER	_IOW(PTAG, 5, int)
#define AUDIO_VTRIG_SEND_TRIGGER	_IOW(PTAG, 6, int)

#define SPRD_TRIG_NAME_MAX		64
#define DEFAULT_BUFFER_SIZE		(10*1024)
#define DEFAULT_RECORD_LENGTH_BYTES	3

enum vtrig_cmd_type {
	VTRIG_WAKEUP_READ,
	VTRIG_WAKEUP_WRITE,
	VTRIG_ENABLE_TRIGGER,
	VTRIG_DISABLE_TRIGGER,
	VTRIG_SEND_TRIGGER,
	VTRIG_CMD_MAX
};

enum vtrig_copy_user {
	TRIGGER_SEND,
	TRIGGER_GENERIC,
	TRIGGER_NOT
};

struct vtrigger_device {
	struct miscdevice misc_dev;
	char device_name[SPRD_TRIG_NAME_MAX];
	u32 openned;
	bool force_wakeup_r;
	bool force_wakeup_w;
	struct mutex mutex;
	wait_queue_head_t wwait;
	wait_queue_head_t rwait;
	struct device *dev;
	enum vtrig_cmd_type cmd_type;
	int irq_gpio;
	bool is_triggerred;
};

static int vtrig_open(struct inode *inode, struct file *filp)
{
	/*
	 * The miscdevice layer puts our struct miscdevice into the
	 * filp->private_data field. We use this to find our private
	 * data and then overwrite it with our own private structure.
	 */
	struct vtrigger_device *vtrig_dev = container_of(filp->private_data,
						struct vtrigger_device, misc_dev);
	filp->private_data = vtrig_dev;

	mutex_lock(&vtrig_dev->mutex);
	if (!vtrig_dev->openned) {
		vtrig_dev->openned = 1;
		init_waitqueue_head(&vtrig_dev->wwait);
		init_waitqueue_head(&vtrig_dev->rwait);
	} else {
		vtrig_dev->openned++;
	}

	mutex_unlock(&vtrig_dev->mutex);
	return 0;
}

static int vtrig_release(struct inode *inode, struct file *filp)
{
	struct vtrigger_device *vtrig_dev = filp->private_data;

	mutex_lock(&vtrig_dev->mutex);
	if (vtrig_dev->openned) {
		vtrig_dev->openned--;
		if (vtrig_dev->openned == 0) {
			wake_up_interruptible_all(&vtrig_dev->wwait);
			wake_up_interruptible_all(&vtrig_dev->rwait);
			filp->private_data = NULL;
		}
	}
	mutex_unlock(&vtrig_dev->mutex);
	return 0;
}

static ssize_t vtrig_read(struct file *filp,
	char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	int en_value;
	struct vtrigger_device *vtrig_dev = filp->private_data;

	ret = wait_event_interruptible(vtrig_dev->rwait, vtrig_dev->force_wakeup_r);
	if (ret < 0) {
		dev_err(vtrig_dev->dev, "wait interrupted!\n");
	}
	vtrig_dev->force_wakeup_r = false;

	if (vtrig_dev->cmd_type == VTRIG_SEND_TRIGGER) {
		en_value = TRIGGER_SEND;
		ret = copy_to_user(buf, &en_value, sizeof(int));
		vtrig_dev->cmd_type = VTRIG_CMD_MAX;
	} else if (vtrig_dev->is_triggerred) {
		en_value = TRIGGER_GENERIC;
		ret = copy_to_user(buf, &en_value, sizeof(int));
	} else {
		en_value = TRIGGER_NOT;
		ret = copy_to_user(buf, &en_value, sizeof(int));
	}
	vtrig_dev->is_triggerred = false;

	if (ret) {
		dev_err(vtrig_dev->dev, "%s copy_to_user fail\n", __func__);
		return -EINVAL;
	}
	pr_info("%s, cmd_type %d, is_triggerred %d\n", __func__,
				vtrig_dev->cmd_type, vtrig_dev->is_triggerred);

	return sizeof(int);
}

static ssize_t vtrig_write(struct file *filp,
	const char __user *buf, size_t count, loff_t *ppos)
{
	return 0;
}

static unsigned int vtrig_poll(struct file *filp, poll_table *wait)
{
	struct vtrigger_device *vtrig_dev = filp->private_data;
	unsigned int mask = 0;
	if (!vtrig_dev)
		return -EINVAL;

	return mask;
}

static long vtrig_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct vtrigger_device *vtrig_dev = filp->private_data;

	if (!vtrig_dev)
		return -EINVAL;

	switch (cmd) {
	case AUDIO_VTRIG_WAKEUP_READ:
		vtrig_dev->force_wakeup_r = true;
		vtrig_dev->cmd_type = VTRIG_WAKEUP_READ;
		wake_up_interruptible_all(&vtrig_dev->rwait);
		break;
	case AUDIO_VTRIG_WAKEUP_WRITE:
		vtrig_dev->force_wakeup_w = true;
		vtrig_dev->cmd_type = VTRIG_WAKEUP_WRITE;
		wake_up_interruptible_all(&vtrig_dev->wwait);
		break;
	case AUDIO_VTRIG_ENABLE_TRIGGER:
		vtrig_dev->cmd_type = VTRIG_ENABLE_TRIGGER;
		break;
	case AUDIO_VTRIG_DISABLE_TRIGGER:
		vtrig_dev->is_triggerred = false;
		vtrig_dev->cmd_type = VTRIG_DISABLE_TRIGGER;
		pr_info("%s do nothing for disable trigger", __func__);
		break;
	case AUDIO_VTRIG_SEND_TRIGGER:
		vtrig_dev->force_wakeup_r = true;
		vtrig_dev->cmd_type = VTRIG_SEND_TRIGGER;
		wake_up_interruptible_all(&vtrig_dev->rwait);
		break;
	default:
		dev_err(vtrig_dev->dev, "default command");
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations vtrig_fops = {
	.open = vtrig_open,
	.release = vtrig_release,
	.read = vtrig_read,
	.write = vtrig_write,
	.poll = vtrig_poll,
	.unlocked_ioctl = vtrig_ioctl,
	.compat_ioctl = vtrig_ioctl,
	.owner = THIS_MODULE,
};

static const struct of_device_id vtrig_match_table[] = {
	{.compatible = "sprd,voice_trig",},
	{ },
};

static irqreturn_t vtrig_irq_handler(int irq, void *dev)
{
	struct vtrigger_device *vtrig_dev = dev;

	vtrig_dev->is_triggerred = true;
	vtrig_dev->force_wakeup_r = true;
	wake_up_interruptible_all(&vtrig_dev->rwait);

	pr_info("%s voice triggerred\n", __func__);
	return IRQ_HANDLED;
}

static int vtrig_parse_dt(struct platform_device *pdev,
	struct vtrigger_device *vtrig_dev)
{
	int ret;
	unsigned int irq;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;

	vtrig_dev->irq_gpio = of_get_named_gpio(np, "irq_gpio", 0);
	ret = gpio_is_valid(vtrig_dev->irq_gpio);
	if (!ret) {
		dev_err(dev,
			"irq_gpio property not found and device is not supported at the moment\n");
		return -ENODEV;
	}

	ret = devm_gpio_request_one(dev, vtrig_dev->irq_gpio, GPIOF_DIR_IN, "vtrig-irq-gpio");
	if (ret < 0) {
		dev_err(dev, "%s gpio request fail, %d\n", __func__, ret);
		return ret;
	}

	irq = gpio_to_irq(vtrig_dev->irq_gpio);
	ret = devm_request_threaded_irq(dev, irq, NULL, vtrig_irq_handler,
		IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND | IRQF_ONESHOT,
		"audio-vtrig-irq", vtrig_dev);
	if (ret < 0) {
		dev_err(dev, "failed to request IRQ_%d(GPIO_%d)\n",
			irq, vtrig_dev->irq_gpio);
		return ret;
	}

	return 0;
}

static int vtrig_probe(struct platform_device *pdev)
{
	int rval = 0;
	const char *name = NULL;
	struct device *dev = &pdev->dev;
	struct vtrigger_device *vtrig_dev;

	vtrig_dev = devm_kzalloc(&pdev->dev, sizeof(struct vtrigger_device),
				GFP_KERNEL);
	if (!vtrig_dev)
		return -ENOMEM;

	name = dev_name(dev);
	if (strlen(name) < SPRD_TRIG_NAME_MAX) {
		strncpy(vtrig_dev->device_name, name, strlen(name));
	} else {
		dev_err(dev,
			"vtrig: name is too long:%d\n", (int)strlen(name));
		return -EFAULT;
	}
	dev_info(&pdev->dev, "name=%s\n", vtrig_dev->device_name);
	vtrig_dev->is_triggerred = false;
	vtrig_dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	vtrig_dev->misc_dev.name = vtrig_dev->device_name;
	vtrig_dev->misc_dev.fops = &vtrig_fops;
	vtrig_dev->misc_dev.parent = NULL;
	vtrig_dev->dev = (struct device *)&pdev->dev;

	rval = vtrig_parse_dt(pdev, vtrig_dev);
	if (rval != 0) {
		dev_err(&pdev->dev, "parse dt failed\n");
		return rval;
	}

	rval = misc_register(&vtrig_dev->misc_dev);
	if (rval != 0) {
		dev_err(&pdev->dev, "Failed to alloc vtrig chrdev\n");
		return rval;
	}

	mutex_init(&vtrig_dev->mutex);
	dev_set_drvdata(&pdev->dev, vtrig_dev);
	return 0;
}

static int vtrig_remove(struct platform_device *pdev)
{
	struct vtrigger_device *vtrig_dev = platform_get_drvdata(pdev);

	if (vtrig_dev) {
		mutex_destroy(&vtrig_dev->mutex);
		misc_deregister((struct miscdevice *)&vtrig_dev->misc_dev);
	}
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver vtrig_driver = {
	.driver = {
		.name = "voice-vtrig",
		.of_match_table = vtrig_match_table,
	},
	.probe = vtrig_probe,
	.remove = vtrig_remove,
};

module_platform_driver(vtrig_driver);

MODULE_AUTHOR("SPRD");
MODULE_DESCRIPTION("audio voice trigger driver");
MODULE_LICENSE("GPL v2");
