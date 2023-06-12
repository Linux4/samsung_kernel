
/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <asm/uaccess.h>


#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/fb.h>

#include "sprdfb_notifier.h"

struct sprd_notifier {
	int fps;
	struct mutex ops_lock;
	struct cdev dev;
};

#define SPRD_DISPC_DEVICE_NODE_NAME  "dispc"
#define SPRD_DISPC_DEVICE_FILE_NAME  "dispc"
#define SPRD_DISPC_DEVICE_CLASS_NAME "dispc"

static int sprd_dispc_major = 0;
static int sprd_dispc_minor = 0;

static struct class* sprd_dispc_class = NULL;
static struct sprd_notifier* sprd_notifier_obj = NULL;

static int sprd_dispc_open(struct inode* inode, struct file* filp);
static int sprd_dispc_release(struct inode* inode, struct file* filp);
static ssize_t sprd_dispc_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos);
static ssize_t sprd_dispc_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos);

static struct file_operations sprd_dispc_fops = {
	.owner = THIS_MODULE,
	.open = sprd_dispc_open,
	.release = sprd_dispc_release,
	.read = sprd_dispc_read,
	.write = sprd_dispc_write,
};

/************ dispc notifier *****************/
static LIST_HEAD(dispc_dev_handlers);
static DEFINE_MUTEX(dispc_dev_lock);

/* register a callback function called before DDR frequency change
*  @handler: callback function
*/
int dispc_notifier_register(struct dispc_dbs *handler)
{
	struct list_head *pos;
	struct dispc_dbs *e;
		printk("dispc_notifier_register \n");

	mutex_lock(&dispc_dev_lock);

	list_for_each(pos, &dispc_dev_handlers) {
		e = list_entry(pos, struct dispc_dbs, link);
		if (e == handler) {
			printk("***** %s, %pf already exsited ****\n",
					__func__, e->dispc_notifier);

			mutex_unlock(&dispc_dev_lock);
			return -1;
		}
	}

	list_add_tail(&handler->link, pos);

	mutex_unlock(&dispc_dev_lock);

	return 0;
}
EXPORT_SYMBOL(dispc_notifier_register);


static unsigned int dispc_change_notification(int state, unsigned int type)
{
	struct dispc_dbs *pos;
	int forbidden;

	printk("dispc_change_notification %d\n", state);

	mutex_lock(&dispc_dev_lock);

	list_for_each_entry(pos, &dispc_dev_handlers, link) {
		if (pos->dispc_notifier != NULL) {
			printk("%s: state:%x, calling %pf,type:%d\n", __func__, state, pos->dispc_notifier,type);

			if (type != pos->type) {
				continue;
			}

			forbidden = pos->dispc_notifier(pos, state);
			if (forbidden) {
				mutex_unlock(&dispc_dev_lock);
				return forbidden;
			}

			printk("%s: calling %pf done \n", __func__, pos->dispc_notifier );
		}
	}

	mutex_unlock(&dispc_dev_lock);

	return 0;
}


static ssize_t sprd_fps_show(struct device* dev, struct device_attribute* attr,  char* buf);
static ssize_t sprd_fps_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);

static DEVICE_ATTR(fps, S_IRUGO | S_IWUSR, sprd_fps_show, sprd_fps_store);


/*
static struct device_attribute dispc_device_attributes[] = {
	__ATTR(fps, 0644, dispc_show_fps, dispc_store_fps),
	__ATTR(fifo_threshold, 0644, dispc_show_threshold,
			 dispc_store_threshold),
//	__ATTR(actual_brightness, 0444, backlight_show_actual_brightness,
//			 NULL),
//	__ATTR(max_brightness, 0444, backlight_show_max_brightness, NULL),
//	__ATTR(type, 0444, backlight_show_type, NULL),
//	__ATTR_NULL,
};
*/


static int sprd_dispc_open(struct inode* inode, struct file* filp)
{
	struct sprd_notifier* dev;

	dev = container_of(inode->i_cdev, struct sprd_notifier, dev);
	filp->private_data = dev;

	return 0;
}

static int sprd_dispc_release(struct inode* inode, struct file* filp)
{
	return 0;
}

static ssize_t sprd_dispc_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos)
{
	ssize_t err = 0;
	return err;
}

static ssize_t sprd_dispc_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos)
{
	struct sprd_notifier* dev = filp->private_data;
	ssize_t err = 0;
	return err;
}

static ssize_t __sprd_get_fps(struct sprd_notifier* dev, char* buf)
{
	int val = 0;

	mutex_lock(&dev->ops_lock);
	val = dev->fps;
	mutex_unlock(&dev->ops_lock);

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t __sprd_set_fps(struct sprd_notifier* dev, const char* buf, size_t count)
{
	int val = 0;

	val = simple_strtol(buf, NULL, 10);

	printk("__sprdfb_fps_set_val %d\n", val);

	mutex_lock(&dev->ops_lock);

	dev->fps = val;
	dispc_change_notification(val,DISPC_DBS_FPS);

	mutex_unlock(&dev->ops_lock);

	return count;
}

static ssize_t sprd_fps_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct sprd_notifier* hdev = (struct sprd_notifier*)dev_get_drvdata(dev);

	return __sprd_get_fps(hdev, buf);
}

static ssize_t sprd_fps_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
	struct sprd_notifier* hdev = (struct sprd_notifier*)dev_get_drvdata(dev);

	return __sprd_set_fps(hdev, buf, count);
}

static int  __sprd_dispc_setup_dev(struct sprd_notifier* dev)
{
	int err;
	dev_t devno = MKDEV(sprd_dispc_major, sprd_dispc_minor);

	memset(dev, 0, sizeof(struct sprd_notifier));

	cdev_init(&(dev->dev), &sprd_dispc_fops);
	dev->dev.owner = THIS_MODULE;
	dev->dev.ops = &sprd_dispc_fops;

	err = cdev_add(&(dev->dev),devno, 1);
	if(err) {
		return err;
	}

	mutex_init(&dev->ops_lock);

	dev->fps = 0;

	return 0;
}

static int __init sprd_notifier_init(void)
{
	int err = -1;
	dev_t dev = 0;
	struct device* temp = NULL;

	printk(KERN_ALERT"Initializing sprd dispc device.\n");

	err = alloc_chrdev_region(&dev, 0, 1, SPRD_DISPC_DEVICE_NODE_NAME);
	if(err < 0) {
		printk(KERN_ALERT"Failed to alloc char dev region.\n");
		goto fail;
	}

	sprd_dispc_major = MAJOR(dev);
	sprd_dispc_minor = MINOR(dev);

	sprd_notifier_obj = kmalloc(sizeof(struct sprd_notifier), GFP_KERNEL);
	if(!sprd_notifier_obj) {
		err = -ENOMEM;
		printk(KERN_ALERT"Failed to alloc sprd_notifier.\n");
		goto unregister;
	}

	err = __sprd_dispc_setup_dev(sprd_notifier_obj);
	if(err) {
		printk(KERN_ALERT"Failed to setup dev: %d.\n", err);
		goto cleanup;
	}

	sprd_dispc_class = class_create(THIS_MODULE, SPRD_DISPC_DEVICE_CLASS_NAME);
	if(IS_ERR(sprd_dispc_class)) {
		err = PTR_ERR(sprd_dispc_class);
		printk(KERN_ALERT"Failed to create sprd dispc class.\n");
		goto destroy_cdev;
   }

//	sprd_dispc_class->dev_attrs = dispc_device_attributes;

	temp = device_create(sprd_dispc_class, NULL, dev, "%s", SPRD_DISPC_DEVICE_FILE_NAME);
	if(IS_ERR(temp)) {
		err = PTR_ERR(temp);
		printk(KERN_ALERT"Failed to create sprd dispc device.");
		goto destroy_class;
	}

	err = device_create_file(temp, &dev_attr_fps);
	if(err < 0) {
		printk(KERN_ALERT"Failed to create attribute val.");
		goto destroy_device;
	}

	dev_set_drvdata(temp, sprd_notifier_obj);

	printk(KERN_ALERT"Succedded to initialize sprd dispc device.\n");
	return 0;

destroy_device:
	device_destroy(sprd_dispc_class, dev);

destroy_class:
	class_destroy(sprd_dispc_class);

destroy_cdev:
	cdev_del(&(sprd_notifier_obj->dev));

cleanup:
	kfree(sprd_notifier_obj);

unregister:
	unregister_chrdev_region(MKDEV(sprd_dispc_major, sprd_dispc_minor), 1);

fail:
	return err;
}

static void __exit sprd_notifier_exit(void)
{
	dev_t devno = MKDEV(sprd_dispc_major, sprd_dispc_minor);

	printk(KERN_ALERT"Destroy sprd dispc device.\n");

	if (sprd_dispc_class) {
		device_destroy(sprd_dispc_class, MKDEV(sprd_dispc_major, sprd_dispc_minor));
		class_destroy(sprd_dispc_class);
	}

	if (sprd_notifier_obj) {
		cdev_del(&(sprd_notifier_obj->dev));
		kfree(sprd_notifier_obj);
	}

	unregister_chrdev_region(devno, 1);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Sprd Dispc Driver");

module_init(sprd_notifier_init);
module_exit(sprd_notifier_exit);


