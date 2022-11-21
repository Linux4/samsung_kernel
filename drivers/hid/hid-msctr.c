#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
//header
#include <uapi/linux/hidraw.h>


struct hidraw {
	unsigned int minor;
	int exist;
	int open;
	wait_queue_head_t wait;
	struct hid_device *hid;
	struct device *dev;
	spinlock_t list_lock;
	struct list_head list;
};

struct hidraw_report {
	__u8 *value;
	int len;
};

struct hidraw_list {
	struct hidraw_report buffer[HIDRAW_BUFFER_SIZE];
	int head;
	int tail;
	struct fasync_struct *fasync;
	struct hidraw *hidraw;
	struct list_head node;
	struct mutex read_mutex;
};
//end header
#include "hid-ids.h"




#include <linux/fs.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/hid.h>
#include <linux/mutex.h>
#include <linux/sched/signal.h>
#include <linux/string.h>

static int hidraw_major;
static struct cdev hidraw_cdev;
static struct class *hidraw_class;
static struct hidraw *hidraw_table[HIDRAW_MAX_DEVICES];
static DEFINE_MUTEX(minors_lock);

static ssize_t hidraw_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
	struct hidraw_list *list = file->private_data;
	int ret = 0, len;
	DECLARE_WAITQUEUE(wait, current);

	mutex_lock(&list->read_mutex);

	while (ret == 0) {
		if (list->head == list->tail) {
			add_wait_queue(&list->hidraw->wait, &wait);
			set_current_state(TASK_INTERRUPTIBLE);

			while (list->head == list->tail) {
				if (signal_pending(current)) {
					ret = -ERESTARTSYS;
					break;
				}
				if (!list->hidraw->exist) {
					ret = -EIO;
					break;
				}
				if (file->f_flags & O_NONBLOCK) {
					ret = -EAGAIN;
					break;
				}

				/* allow O_NONBLOCK to work well from other threads */
				mutex_unlock(&list->read_mutex);
				schedule();
				mutex_lock(&list->read_mutex);
				set_current_state(TASK_INTERRUPTIBLE);
			}

			set_current_state(TASK_RUNNING);
			remove_wait_queue(&list->hidraw->wait, &wait);
		}

		if (ret)
			goto out;

		len = list->buffer[list->tail].len > count ?
			count : list->buffer[list->tail].len;

		if (list->buffer[list->tail].value) {
			if (copy_to_user(buffer, list->buffer[list->tail].value, len)) {
				ret = -EFAULT;
				goto out;
			}
			ret = len;
		}

		kfree(list->buffer[list->tail].value);
		list->buffer[list->tail].value = NULL;
		list->tail = (list->tail + 1) & (HIDRAW_BUFFER_SIZE - 1);
	}
out:
	mutex_unlock(&list->read_mutex);
	return ret;
}

/*
 * The first byte of the report buffer is expected to be a report number.
 *
 * This function is to be called with the minors_lock mutex held.
 */
static ssize_t hidraw_send_report(struct file *file, const char __user *buffer, size_t count, unsigned char report_type)
{
	unsigned int minor = iminor(file_inode(file));
	struct hid_device *dev;
	__u8 *buf;
	int ret = 0;

	lockdep_assert_held(&minors_lock);

	if (!hidraw_table[minor] || !hidraw_table[minor]->exist) {
		ret = -ENODEV;
		goto out;
	}

	dev = hidraw_table[minor]->hid;

	if (count > HID_MAX_BUFFER_SIZE) {
		hid_warn(dev, "pid %d passed too large report\n",
			 task_pid_nr(current));
		ret = -EINVAL;
		goto out;
	}

	if (count < 2) {
		hid_warn(dev, "pid %d passed too short report\n",
			 task_pid_nr(current));
		ret = -EINVAL;
		goto out;
	}

	buf = memdup_user(buffer, count);
	if (IS_ERR(buf)) {
		ret = PTR_ERR(buf);
		goto out;
	}

	if ((report_type == HID_OUTPUT_REPORT) &&
	    !(dev->quirks & HID_QUIRK_NO_OUTPUT_REPORTS_ON_INTR_EP)) {
		ret = hid_hw_output_report(dev, buf, count);
		/*
		 * compatibility with old implementation of USB-HID and I2C-HID:
		 * if the device does not support receiving output reports,
		 * on an interrupt endpoint, fallback to SET_REPORT HID command.
		 */
		if (ret != -ENOSYS)
			goto out_free;
	}

	ret = hid_hw_raw_request(dev, buf[0], buf, count, report_type,
				HID_REQ_SET_REPORT);

out_free:
	kfree(buf);
out:
	return ret;
}

static ssize_t hidraw_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret;
	mutex_lock(&minors_lock);
	ret = hidraw_send_report(file, buffer, count, HID_OUTPUT_REPORT);
	mutex_unlock(&minors_lock);
	return ret;
}


/*
 * This function performs a Get_Report transfer over the control endpoint
 * per section 7.2.1 of the HID specification, version 1.1.  The first byte
 * of buffer is the report number to request, or 0x0 if the defice does not
 * use numbered reports. The report_type parameter can be HID_FEATURE_REPORT
 * or HID_INPUT_REPORT.
 *
 * This function is to be called with the minors_lock mutex held.
 */
static ssize_t hidraw_get_report(struct file *file, char __user *buffer, size_t count, unsigned char report_type)
{
	unsigned int minor = iminor(file_inode(file));
	struct hid_device *dev;
	__u8 *buf;
	int ret = 0, len;
	unsigned char report_number;

	lockdep_assert_held(&minors_lock);

	if (!hidraw_table[minor] || !hidraw_table[minor]->exist) {
		ret = -ENODEV;
		goto out;
	}

	dev = hidraw_table[minor]->hid;

	if (!dev->ll_driver->raw_request) {
		ret = -ENODEV;
		goto out;
	}

	if (count > HID_MAX_BUFFER_SIZE) {
		printk(KERN_WARNING "hidraw: pid %d passed too large report\n",
				task_pid_nr(current));
		ret = -EINVAL;
		goto out;
	}

	if (count < 2) {
		printk(KERN_WARNING "hidraw: pid %d passed too short report\n",
				task_pid_nr(current));
		ret = -EINVAL;
		goto out;
	}

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto out;
	}

	/*
	 * Read the first byte from the user. This is the report number,
	 * which is passed to hid_hw_raw_request().
	 */
	if (copy_from_user(&report_number, buffer, 1)) {
		ret = -EFAULT;
		goto out_free;
	}

	ret = hid_hw_raw_request(dev, report_number, buf, count, report_type,
				 HID_REQ_GET_REPORT);

	if (ret < 0)
		goto out_free;

	len = (ret < count) ? ret : count;

	if (copy_to_user(buffer, buf, len)) {
		ret = -EFAULT;
		goto out_free;
	}

	ret = len;

out_free:
	kfree(buf);
out:
	return ret;
}

static __poll_t hidraw_poll(struct file *file, poll_table *wait)
{
	struct hidraw_list *list = file->private_data;
	__poll_t mask = EPOLLOUT | EPOLLWRNORM; /* hidraw is always writable */

	poll_wait(file, &list->hidraw->wait, wait);
	if (list->head != list->tail)
		mask |= EPOLLIN | EPOLLRDNORM;
	if (!list->hidraw->exist)
		mask |= EPOLLERR | EPOLLHUP;
	return mask;
}

static int hidraw_open(struct inode *inode, struct file *file)
{
	unsigned int minor = iminor(inode);
	struct hidraw *dev;
	struct hidraw_list *list;
	unsigned long flags;
	int err = 0;

	if (!(list = kzalloc(sizeof(struct hidraw_list), GFP_KERNEL))) {
		err = -ENOMEM;
		goto out;
	}

	mutex_lock(&minors_lock);
	if (!hidraw_table[minor] || !hidraw_table[minor]->exist) {
		err = -ENODEV;
		goto out_unlock;
	}

	dev = hidraw_table[minor];
	if (!dev->open++) {
		err = hid_hw_power(dev->hid, PM_HINT_FULLON);
		if (err < 0) {
			dev->open--;
			goto out_unlock;
		}

		err = hid_hw_open(dev->hid);
		if (err < 0) {
			hid_hw_power(dev->hid, PM_HINT_NORMAL);
			dev->open--;
			goto out_unlock;
		}
	}

	list->hidraw = hidraw_table[minor];
	mutex_init(&list->read_mutex);
	spin_lock_irqsave(&hidraw_table[minor]->list_lock, flags);
	list_add_tail(&list->node, &hidraw_table[minor]->list);
	spin_unlock_irqrestore(&hidraw_table[minor]->list_lock, flags);
	file->private_data = list;
out_unlock:
	mutex_unlock(&minors_lock);
out:
	if (err < 0)
		kfree(list);
	return err;

}

static int hidraw_fasync(int fd, struct file *file, int on)
{
	struct hidraw_list *list = file->private_data;

	return fasync_helper(fd, file, on, &list->fasync);
}

static void drop_ref(struct hidraw *hidraw, int exists_bit)
{
	if (exists_bit) {
		hidraw->exist = 0;
		if (hidraw->open) {
			hid_hw_close(hidraw->hid);
			wake_up_interruptible(&hidraw->wait);
		}
		device_destroy(hidraw_class,
			       MKDEV(hidraw_major, hidraw->minor));
	} else {
		--hidraw->open;
	}
	if (!hidraw->open) {
		if (!hidraw->exist) {
			hidraw_table[hidraw->minor] = NULL;
			kfree(hidraw);
		} else {
			/* close device for last reader */
			hid_hw_close(hidraw->hid);
			hid_hw_power(hidraw->hid, PM_HINT_NORMAL);
		}
	}
}

static int hidraw_release(struct inode * inode, struct file * file)
{
	unsigned int minor = iminor(inode);
	struct hidraw_list *list = file->private_data;
	unsigned long flags;

	mutex_lock(&minors_lock);

	spin_lock_irqsave(&hidraw_table[minor]->list_lock, flags);
	list_del(&list->node);
	spin_unlock_irqrestore(&hidraw_table[minor]->list_lock, flags);
	kfree(list);

	drop_ref(hidraw_table[minor], 0);

	mutex_unlock(&minors_lock);
	return 0;
}

static long hidraw_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	struct inode *inode = file_inode(file);
	unsigned int minor = iminor(inode);
	long ret = 0;
	struct hidraw *dev;
	void __user *user_arg = (void __user*) arg;

	mutex_lock(&minors_lock);
	dev = hidraw_table[minor];
	if (!dev || !dev->exist) {
		ret = -ENODEV;
		goto out;
	}

	switch (cmd) {
		case HIDIOCGRDESCSIZE:
			if (put_user(dev->hid->rsize, (int __user *)arg))
				ret = -EFAULT;
			break;

		case HIDIOCGRDESC:
			{
				__u32 len;

				if (get_user(len, (int __user *)arg))
					ret = -EFAULT;
				else if (len > HID_MAX_DESCRIPTOR_SIZE - 1)
					ret = -EINVAL;
				else if (copy_to_user(user_arg + offsetof(
					struct hidraw_report_descriptor,
					value[0]),
					dev->hid->rdesc,
					min(dev->hid->rsize, len)))
					ret = -EFAULT;
				break;
			}
		case HIDIOCGRAWINFO:
			{
				struct hidraw_devinfo dinfo;

				dinfo.bustype = dev->hid->bus;
				dinfo.vendor = dev->hid->vendor;
				dinfo.product = dev->hid->product;
				if (copy_to_user(user_arg, &dinfo, sizeof(dinfo)))
					ret = -EFAULT;
				break;
			}
		default:
			{
				struct hid_device *hid = dev->hid;
				if (_IOC_TYPE(cmd) != 'H') {
					ret = -EINVAL;
					break;
				}

				if (_IOC_NR(cmd) == _IOC_NR(HIDIOCSFEATURE(0))) {
					int len = _IOC_SIZE(cmd);
					ret = hidraw_send_report(file, user_arg, len, HID_FEATURE_REPORT);
					break;
				}
				if (_IOC_NR(cmd) == _IOC_NR(HIDIOCGFEATURE(0))) {
					int len = _IOC_SIZE(cmd);
					ret = hidraw_get_report(file, user_arg, len, HID_FEATURE_REPORT);
					break;
				}

				/* Begin Read-only ioctls. */
				if (_IOC_DIR(cmd) != _IOC_READ) {
					ret = -EINVAL;
					break;
				}

				if (_IOC_NR(cmd) == _IOC_NR(HIDIOCGRAWNAME(0))) {
					int len = strlen(hid->name) + 1;
					if (len > _IOC_SIZE(cmd))
						len = _IOC_SIZE(cmd);
					ret = copy_to_user(user_arg, hid->name, len) ?
						-EFAULT : len;
					break;
				}

				if (_IOC_NR(cmd) == _IOC_NR(HIDIOCGRAWPHYS(0))) {
					int len = strlen(hid->phys) + 1;
					if (len > _IOC_SIZE(cmd))
						len = _IOC_SIZE(cmd);
					ret = copy_to_user(user_arg, hid->phys, len) ?
						-EFAULT : len;
					break;
				}
			}

		ret = -ENOTTY;
	}
out:
	mutex_unlock(&minors_lock);
	return ret;
}

static const struct file_operations hidraw_ops = {
	.owner =        THIS_MODULE,
	.read =         hidraw_read,
	.write =        hidraw_write,
	.poll =         hidraw_poll,
	.open =         hidraw_open,
	.release =      hidraw_release,
	.unlocked_ioctl = hidraw_ioctl,
	.fasync =	hidraw_fasync,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = hidraw_ioctl,
#endif
	.llseek =	noop_llseek,
};

static int hidraw_report_event(struct hid_device *hid, u8 *data, int len)
{
	struct hidraw *dev = hid_get_drvdata(hid);
	struct hidraw_list *list;
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&dev->list_lock, flags);
	list_for_each_entry(list, &dev->list, node) {
		int new_head = (list->head + 1) & (HIDRAW_BUFFER_SIZE - 1);

		if (new_head == list->tail)
			continue;

		if (!(list->buffer[list->head].value = kmemdup(data, len, GFP_ATOMIC))) {
			ret = -ENOMEM;
			break;
		}
		list->buffer[list->head].len = len;
		list->head = new_head;
		kill_fasync(&list->fasync, SIGIO, POLL_IN);
	}
	spin_unlock_irqrestore(&dev->list_lock, flags);

	wake_up_interruptible(&dev->wait);
	return ret;
}

static int hidraw_connect(struct hid_device *hid)
{
	int minor, result;
	struct hidraw *dev;

	/* we accept any HID device, all applications */

	dev = kzalloc(sizeof(struct hidraw), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	result = -EINVAL;

	mutex_lock(&minors_lock);

	for (minor = 0; minor < HIDRAW_MAX_DEVICES; minor++) {
		if (hidraw_table[minor])
			continue;
		hidraw_table[minor] = dev;
		result = 0;
		break;
	}

	if (result) {
		mutex_unlock(&minors_lock);
		kfree(dev);
		goto out;
	}

	dev->dev = device_create(hidraw_class, &hid->dev, MKDEV(hidraw_major, minor),
				 NULL, "%s%d", "msctr", minor);

	if (IS_ERR(dev->dev)) {
		hidraw_table[minor] = NULL;
		mutex_unlock(&minors_lock);
		result = PTR_ERR(dev->dev);
		kfree(dev);
		goto out;
	}

	init_waitqueue_head(&dev->wait);
	spin_lock_init(&dev->list_lock);
	INIT_LIST_HEAD(&dev->list);

	dev->hid = hid;
	dev->minor = minor;

	dev->exist = 1;
	hid_set_drvdata(hid, dev);

	mutex_unlock(&minors_lock);
out:
	return result;

}

static void hidraw_disconnect(struct hid_device *hid)
{
	struct hidraw *hidraw = hid_get_drvdata(hid);;

	mutex_lock(&minors_lock);

	drop_ref(hidraw, 1);

	mutex_unlock(&minors_lock);
}

static int hidraw_init(void)
{
	int result;
	dev_t dev_id;

	result = alloc_chrdev_region(&dev_id, HIDRAW_FIRST_MINOR,
			HIDRAW_MAX_DEVICES, "msctr");
	if (result < 0) {
		pr_warn("can't get major number\n");
		goto out;
	}

	hidraw_major = MAJOR(dev_id);

	hidraw_class = class_create(THIS_MODULE, "msctr");
	if (IS_ERR(hidraw_class)) {
		result = PTR_ERR(hidraw_class);
		goto error_cdev;
	}

        cdev_init(&hidraw_cdev, &hidraw_ops);
	result = cdev_add(&hidraw_cdev, dev_id, HIDRAW_MAX_DEVICES);
	if (result < 0)
		goto error_class;

	printk(KERN_INFO "hidraw: raw HID events driver (C) Jiri Kosina\n");
out:
	return result;

error_class:
	class_destroy(hidraw_class);
error_cdev:
	unregister_chrdev_region(dev_id, HIDRAW_MAX_DEVICES);
	goto out;
}

static void hidraw_exit(void)
{
	dev_t dev_id = MKDEV(hidraw_major, 0);

	cdev_del(&hidraw_cdev);
	class_destroy(hidraw_class);
	unregister_chrdev_region(dev_id, HIDRAW_MAX_DEVICES);

}

///////////////////////////////
static int msctr_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size)
{
	return hidraw_report_event(hdev, data, size);
}

static int msctr_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	hid_info(hdev, "[%s]\n",hdev->name);
	
	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT & ~HID_CONNECT_HIDRAW & ~HID_CONNECT_HIDINPUT);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err;
	}
	ret = hidraw_connect(hdev);
	if (ret) {
		hid_err(hdev, "msctr_connect failed\n");
		goto err;
	}
	return 0;
err:
	return ret;
}

static void msctr_remove(struct hid_device *hid)
{
	hidraw_disconnect(hid);

	hid_hw_stop(hid);
}

static const struct hid_device_id msctr_devices[] = {
	{HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_MOTION_CONTROLLER_1) },
	{HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_MOTION_CONTROLLER_2) },
	{HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_MOTION_CONTROLLER_3) },
	{ }
};

MODULE_DEVICE_TABLE(hid, msctr_devices);

static struct hid_driver msctr_driver = {
	.name = "hid-msctr",
	.id_table = msctr_devices,
	.probe = msctr_probe,
	.remove = msctr_remove,
	.raw_event = msctr_raw_event,
};

static int __init msctr_init(void)
{
	int ret;
	
	dbg_hid("msctr:%s\n", __func__);

	ret = hidraw_init();
	if (ret) {
		dbg_hid("msctr:%s\n", "init failed");
		goto err;
	}
	ret = hid_register_driver(&msctr_driver);
	if (ret) {
		dbg_hid("msctr:%s\n", "registerfailed");
		goto err;
	}
	return 0;
err:
	return ret;
}

static void __exit msctr_exit(void)
{
	dbg_hid("msctr:%s\n", __func__);
	hidraw_exit();
	hid_unregister_driver(&msctr_driver);
}
module_init(msctr_init);
module_exit(msctr_exit);

MODULE_AUTHOR("Wooju Yun");
MODULE_DESCRIPTION("HID msctr driver");
MODULE_LICENSE("GPL");
