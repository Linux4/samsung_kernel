// SPDX-License-Identifier: GPL-2.0-only
/*
 * ear Class Core
 *
 * Copyright (C) 2005 John Lenz <lenz@cs.wisc.edu>
 * Copyright (C) 2005-2007 Richard Purdie <rpurdie@openedhand.com>
 */
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/audio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/cdev.h>

#ifndef CONFIG_EARJACK
#define CONFIG_EARJACK
#endif

#define DEVICE_NAME "earjack"
#define MINOR_NUM_DEV  0
#define NUM_DEVICES   1

static DEFINE_MUTEX(audio_mtx);
static struct class *audio_class;
static int g_detect = 0;
struct cdev cdev;

static ssize_t state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_info("read earjack state type %d \n",g_detect);
	return sprintf(buf, "%d\n",g_detect);
}

static const struct device_attribute audio_device_attrs =
	__ATTR(state, 0644, state_show, NULL);

static int earjack_notify_state(struct notifier_block *nb, unsigned long event, void *buf)
{
	if (event & SND_JACK_MICROPHONE)
		g_detect = 1;
	else
		g_detect = 0;
	pr_info("earjack detect type %d \n",g_detect);
	return 0;
}
static struct notifier_block earjack_notify_nb = {
	.notifier_call  = earjack_notify_state,
};

static int audio_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int audio_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations audio_fops =
{
	.owner  = THIS_MODULE,
	.open   = audio_open,
	.release = audio_release,
	.llseek = noop_llseek,
};

int register_audio_earjack(struct audiodevice *audio)
{
	int err = 0;

	mutex_lock(&audio_mtx);

	audio->chardev = device_create(audio_class, NULL,MKDEV(AUDIO_MAJOR, MINOR_NUM_DEV),
									NULL, DEVICE_NAME);
	if (IS_ERR_OR_NULL(audio->chardev)) {
		err = PTR_ERR(audio->chardev);
		goto out_class;
	}

	cdev_init(&cdev, &audio_fops);
	cdev.owner = THIS_MODULE;
	err = cdev_add(&cdev, MKDEV(AUDIO_MAJOR, MINOR_NUM_DEV), NUM_DEVICES);
	if (err)
		goto out;

	dev_set_drvdata(audio->chardev, audio);

	err = device_create_file(audio->chardev, &audio_device_attrs);
	if(err) {
		pr_err("class_create failed \n");
		goto out_dev;
	}

	snd_soc_jack_notifier_register(audio->jack, &earjack_notify_nb);
	mutex_unlock(&audio_mtx);
	return 0;
out:
	mutex_unlock(&audio_mtx);
	device_destroy(audio_class, MKDEV(AUDIO_MAJOR,MINOR_NUM_DEV));
out_dev:
	cdev_del(&cdev);
out_class:
	class_destroy(audio_class);
	return err;
}
EXPORT_SYMBOL(register_audio_earjack);

void unregister_audio_earjack(struct audiodevice *audio)
{
	mutex_lock(&audio_mtx);
	device_remove_file(audio->chardev, &audio_device_attrs);
	cdev_del(&cdev);
	device_destroy(audio_class, MKDEV(AUDIO_MAJOR, MINOR_NUM_DEV));
	snd_soc_jack_notifier_unregister(audio->jack, &earjack_notify_nb);
	mutex_unlock(&audio_mtx);
}
EXPORT_SYMBOL(unregister_audio_earjack);

static int __init audio_init(void)
{
	dev_t dev = MKDEV(AUDIO_MAJOR, MINOR_NUM_DEV);
	unsigned int baseminor = 0;
	unsigned int count = 1;

	if (alloc_chrdev_region(&dev, baseminor, count, "earjack") < 0) {
		printk(KERN_ERR "soundcore: sound device already in use.\n");
		return -EBUSY;
	}

	audio_class = class_create(THIS_MODULE, "audio");
	if (IS_ERR(audio_class)) {
		unregister_chrdev_region(0, 1);
		return PTR_ERR(audio_class);
	}
	return 0;
}

static void __exit audio_exit(void)
{
	unregister_chrdev_region(0, 1);
	class_destroy(audio_class);
}

subsys_initcall(audio_init);
module_exit(audio_exit);

MODULE_AUTHOR("Zhouyanhong");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ear Class Interface");