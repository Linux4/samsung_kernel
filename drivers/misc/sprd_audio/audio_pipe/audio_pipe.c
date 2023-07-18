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
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

struct point_info_k {
	s32 point_index;
	s32 enable;
	s32 tm_mday;
	s32 tm_hour;
	s32 tm_min;
	s32 tm_sec;
};

#define PTAG		'A'

#define AUDIO_PIPE_WAKEUP_READ		_IOW(PTAG, 0, int)
#define AUDIO_PIPE_WAKEUP_WRITE		_IOW(PTAG, 1, int)
#define AUDIO_PIPE_BUFFERSIZE_SET		_IOW(PTAG, 2, int)
#define AUDIO_PIPE_BUFFERSIZE_GET		_IOR(PTAG, 3, int)
#define AUDIO_PIPE_DATALEN_GET		_IOR(PTAG, 4, int)
#define AUDIO_PIPE_MAXRWSIZE_GET		_IOR(PTAG, 5, int)
#define AUDIO_PIPE_WLOCK_GET			_IO(PTAG, 6)
#define AUDIO_PIPE_WLOCK_PUT			_IO(PTAG, 7)
#define AUDIO_PIPE_POINT_ALLOC		_IOW(PTAG, 8, int)
#define AUDIO_PIPE_POINTINFO_GET	_IOWR(PTAG, 9, struct point_info_k)
#define AUDIO_PIPE_POINTINFO_SET	_IOW(PTAG, 10, struct point_info_k)

#define SPRD_PIPE_NAME_MAX		64
#define DEFAULT_BUFFER_SIZE		(10*1024)
#define DEFAULT_RECORD_LENGTH_BYTES	3
#define GET_USER_BUFFER_SIZE_MAX	(10*1024*1024)
#define GET_USER_COUNT_MAX		500

struct apipe_device {
	struct miscdevice misc_dev;
	char device_name[SPRD_PIPE_NAME_MAX];
	struct kfifo_rec_ptr_2 kf;
	u32 openned;
	bool force_wakeup_r;
	bool force_wakeup_w;
	void *data;
	u32 buffer_size;
	struct mutex mutex;
	wait_queue_head_t wwait;
	wait_queue_head_t rwait;
	struct mutex w_mutex;
	struct point_info_k *point_k;
	u32 points;
	struct device *dev;
};

static int apipe_open(struct inode *inode, struct file *filp)
{
	int ret;
	/*
	 * The miscdevice layer puts our struct miscdevice into the
	 * filp->private_data field. We use this to find our private
	 * data and then overwrite it with our own private structure.
	 */
	struct apipe_device *apipe_dev = container_of(filp->private_data,
			struct apipe_device, misc_dev);

	filp->private_data = apipe_dev;
	mutex_lock(&apipe_dev->mutex);
	if (!apipe_dev->openned) {
		struct kfifo_rec_ptr_2 *kfifo;
		int size = DEFAULT_BUFFER_SIZE;

		size = roundup_pow_of_two(size);
		if (size)
			apipe_dev->data = vmalloc(size);
		else {
			mutex_unlock(&apipe_dev->mutex);
			dev_err(apipe_dev->dev,
				"failed: no mem\n");
			return -ENOMEM;
		}
		if (!apipe_dev->data) {
			mutex_unlock(&apipe_dev->mutex);
			dev_err(apipe_dev->dev,
				"failed: no mem\n");
			return -ENOMEM;
		}
		apipe_dev->openned = 1;
		apipe_dev->buffer_size = size;
		kfifo = (struct kfifo_rec_ptr_2 *) &apipe_dev->kf;
		ret = kfifo_init(kfifo, (void *)apipe_dev->data, size);
		if (ret < 0) {
			vfree(apipe_dev->data);
			apipe_dev->data = NULL;
			mutex_unlock(&apipe_dev->mutex);
			dev_err(apipe_dev->dev,
				"failed: %d\n", ret);
			return ret;
		}
		init_waitqueue_head(&apipe_dev->wwait);
		init_waitqueue_head(&apipe_dev->rwait);
	} else {
		apipe_dev->openned++;
	}

	mutex_unlock(&apipe_dev->mutex);
	return 0;
}

static int apipe_release(struct inode *inode, struct file *filp)
{
	struct apipe_device *apipe_dev = filp->private_data;

	mutex_lock(&apipe_dev->mutex);
	if (apipe_dev->openned) {
		apipe_dev->openned--;
		if (apipe_dev->openned == 0) {
			wake_up_interruptible_all(&apipe_dev->wwait);
			wake_up_interruptible_all(&apipe_dev->rwait);
			vfree(apipe_dev->data);
			if (apipe_dev->point_k) {
				vfree(apipe_dev->point_k);
				apipe_dev->point_k = NULL;
			}
			filp->private_data = NULL;
		}
	}
	mutex_unlock(&apipe_dev->mutex);
	return 0;
}

static ssize_t apipe_read(struct file *filp,
	char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copied;
	struct apipe_device *apipe_dev = filp->private_data;
	struct kfifo_rec_ptr_2 *kfifo =
			(struct kfifo_rec_ptr_2 *)&apipe_dev->kf;

	if (filp->f_flags & O_NONBLOCK) {
		ret = kfifo_to_user(kfifo, buf, count, &copied);
		if (ret < 0)
			return ret;
		wake_up_interruptible_all(&apipe_dev->wwait);
		return copied;
	}

	while (1) {
		ret = kfifo_to_user(kfifo, buf, count, &copied);
		if (!ret && copied)
			wake_up_interruptible_all(&apipe_dev->wwait);
		if (apipe_dev->force_wakeup_r || (!ret && copied)) {
			apipe_dev->force_wakeup_r = false;
			break;
		}
		if (ret < 0) {
			dev_dbg(apipe_dev->dev,
				"copy to user err:%d!\n", ret);
		}
		ret = wait_event_interruptible(
				apipe_dev->rwait,
				!kfifo_is_empty(kfifo) ||
				apipe_dev->force_wakeup_r);
		if (ret < 0) {
			dev_err(apipe_dev->dev,
				"wait interrupted!\n");
			break;
		}
	}
	return copied;
}

static ssize_t apipe_write(struct file *filp,
	const char __user *buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	unsigned int copied;
	struct apipe_device *apipe_dev = filp->private_data;
	struct kfifo_rec_ptr_2 *kfifo =
			(struct kfifo_rec_ptr_2 *)&apipe_dev->kf;

	if (filp->f_flags & O_NONBLOCK) {
		ret = kfifo_from_user(kfifo, buf, count, &copied);
		if (ret < 0)
			return ret;
		wake_up_interruptible_all(&apipe_dev->rwait);
		return copied;
	}
	while (1) {
		if (apipe_dev->force_wakeup_w) {
			apipe_dev->force_wakeup_w = false;
			break;
		}
		ret = kfifo_from_user(kfifo, buf, count, &copied);
		if (!ret && copied) {
			wake_up_interruptible_all(&apipe_dev->rwait);
			return copied;
		}
		if (ret < 0) {
			dev_dbg(apipe_dev->dev,
				"copy from user err:%d!\n", ret);
		}
		ret = wait_event_interruptible(
			apipe_dev->wwait,
			!kfifo_is_full(kfifo) ||
			apipe_dev->force_wakeup_w);
		if (ret < 0) {
			dev_err(apipe_dev->dev,
				"wait interrupted!\n");
			break;
		}
	}
	return ret;
}

static unsigned int apipe_poll(struct file *filp, poll_table *wait)
{
	struct apipe_device *apipe_dev = filp->private_data;
	struct kfifo_rec_ptr_2 *kfifo;
	unsigned int mask = 0;

	if (!apipe_dev)
		return -EINVAL;

	poll_wait(filp, &apipe_dev->wwait, wait);
	poll_wait(filp, &apipe_dev->rwait, wait);

	kfifo = (struct kfifo_rec_ptr_2 *) &apipe_dev->kf;
	if (!kfifo_is_empty(kfifo))
		mask |= POLLIN | POLLRDNORM;

	if (!kfifo_is_full(kfifo))
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}

static long apipe_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct apipe_device *apipe_dev = filp->private_data;
	void __user *argp = (void __user *)(arg);
	u32 buffer_size, count, max;
	void *data;
	int ret;
	struct point_info_k point_info;
	struct kfifo_rec_ptr_2 *kfifo;

	if (!apipe_dev)
		return -EINVAL;

	switch (cmd) {
	case AUDIO_PIPE_WAKEUP_READ:
		apipe_dev->force_wakeup_r = true;
		wake_up_interruptible_all(&apipe_dev->rwait);
		break;
	case AUDIO_PIPE_WAKEUP_WRITE:
		apipe_dev->force_wakeup_w = true;
		wake_up_interruptible_all(&apipe_dev->wwait);
		break;
	case AUDIO_PIPE_BUFFERSIZE_SET:
		kfifo = (struct kfifo_rec_ptr_2 *) &apipe_dev->kf;
		if (get_user(buffer_size, (u32 __user *)argp))
			return -EFAULT;
		mutex_lock(&apipe_dev->mutex);
		buffer_size = roundup_pow_of_two(buffer_size);
		if (buffer_size > 0 && buffer_size < GET_USER_BUFFER_SIZE_MAX)
			data = vmalloc(buffer_size);
		else {
			mutex_unlock(&apipe_dev->mutex);
			return -ENOMEM;
		}
		if (!data) {
			mutex_unlock(&apipe_dev->mutex);
			return -ENOMEM;
		}

		if (apipe_dev->data)
			vfree(apipe_dev->data);
		apipe_dev->data = data;
		ret = kfifo_init(kfifo, (void *)apipe_dev->data, buffer_size);
		if (ret < 0) {
			vfree(apipe_dev->data);
			apipe_dev->data = NULL;
			dev_err(apipe_dev->dev, "apipe_open failed: %d\n", ret);
			mutex_unlock(&apipe_dev->mutex);
			return ret;
		}
		apipe_dev->buffer_size = buffer_size;
		mutex_unlock(&apipe_dev->mutex);
		break;
	case AUDIO_PIPE_BUFFERSIZE_GET:
		if (put_user(apipe_dev->buffer_size, (u32 __user *)argp))
			return -EFAULT;
		break;
	case AUDIO_PIPE_DATALEN_GET:
		kfifo = (struct kfifo_rec_ptr_2 *) &apipe_dev->kf;
		if (put_user(kfifo_len(kfifo), (u32 __user *)argp))
			return -EFAULT;
		break;
	case AUDIO_PIPE_MAXRWSIZE_GET:
		kfifo = (struct kfifo_rec_ptr_2 *) &apipe_dev->kf;
		max = (1 << (kfifo_recsize(kfifo) << 3)) - 1;
		if (put_user(max, (u32 __user *)argp))
			return -EFAULT;
		break;
	case AUDIO_PIPE_WLOCK_GET:
		mutex_lock(&apipe_dev->w_mutex);
		break;
	case AUDIO_PIPE_WLOCK_PUT:
		mutex_unlock(&apipe_dev->w_mutex);
		break;
	case AUDIO_PIPE_POINT_ALLOC:
		if (get_user(count, (u32 __user *)argp))
			return -EFAULT;
		mutex_lock(&apipe_dev->mutex);
		if (apipe_dev->point_k) {
			vfree(apipe_dev->point_k);
			apipe_dev->point_k = NULL;
		}
		if (count > 0 && count < GET_USER_COUNT_MAX)
			apipe_dev->point_k = vmalloc(count *
				sizeof(struct point_info_k));
		else {
			mutex_unlock(&apipe_dev->mutex);
			return -ENOMEM;
		}
		if (!apipe_dev->point_k) {
			mutex_unlock(&apipe_dev->mutex);
			return -ENOMEM;
		}
		apipe_dev->points = count;
		mutex_unlock(&apipe_dev->mutex);
		dev_info(apipe_dev->dev, "POINT_ALLOC count:%d", count);
		break;
	case AUDIO_PIPE_POINTINFO_SET:
		if (copy_from_user(&point_info, argp, sizeof(point_info)))
			return -EINVAL;
		mutex_lock(&apipe_dev->mutex);
		if (!apipe_dev->point_k) {
			mutex_unlock(&apipe_dev->mutex);
			return -ENOMEM;
		}
		if (point_info.point_index < apipe_dev->points)
			memcpy(apipe_dev->point_k + point_info.point_index,
				&point_info, sizeof(point_info));
		else {
			mutex_unlock(&apipe_dev->mutex);
			dev_err(apipe_dev->dev, "POINTINFO_SET: error:point index:%d",
				point_info.point_index);
			return -EINVAL;
		}
		mutex_unlock(&apipe_dev->mutex);
		break;
	case AUDIO_PIPE_POINTINFO_GET:
		if (copy_from_user(&point_info, argp, sizeof(point_info)))
			return -EINVAL;
		mutex_lock(&apipe_dev->mutex);
		if (!apipe_dev->point_k) {
			mutex_unlock(&apipe_dev->mutex);
			return -ENOMEM;
		}
		if (point_info.point_index >= apipe_dev->points) {
			mutex_unlock(&apipe_dev->mutex);
			dev_err(apipe_dev->dev, "POINTINFO_GET: error:point index:%d",
				point_info.point_index);
			return -EINVAL;
		}
		if (copy_to_user(argp, apipe_dev->point_k +
			point_info.point_index,
			sizeof(point_info))) {
			mutex_unlock(&apipe_dev->mutex);
			return -EINVAL;
		}
		mutex_unlock(&apipe_dev->mutex);
		break;
	default:
		dev_err(apipe_dev->dev,
			"default command");
		return -EINVAL;
	}
	return 0;
}

static const struct file_operations apipe_fops = {
	.open = apipe_open,
	.release = apipe_release,
	.read = apipe_read,
	.write = apipe_write,
	.poll = apipe_poll,
	.unlocked_ioctl = apipe_ioctl,
	.compat_ioctl = apipe_ioctl,
	.owner = THIS_MODULE,
};

static const struct of_device_id apipe_match_table[] = {
	{.compatible = "sprd,apipe",},
	{ },
};

static int apipe_parse_dt(struct device *dev,
	struct apipe_device *apipe_dev)
{
	const char *name = NULL;

	name = dev_name(dev);
	if (strlen(name) < SPRD_PIPE_NAME_MAX)
		strncpy(apipe_dev->device_name,
				name, strlen(name));
	else {
		dev_err(apipe_dev->dev,
			"apipe: name is too long:%d\n", (int)strlen(name));
		return -EFAULT;
	}
	return 0;
}

static int apipe_probe(struct platform_device *pdev)
{
	int rval = 0;
	struct apipe_device *apipe_dev;

	apipe_dev = devm_kzalloc(&pdev->dev, sizeof(struct apipe_device),
		GFP_KERNEL);
	if (!apipe_dev)
		return -ENOMEM;

	rval = apipe_parse_dt(&pdev->dev, apipe_dev);
	if (rval != 0) {
		dev_err(apipe_dev->dev, "parse dt failed\n");
		return rval;
	}
	dev_info(apipe_dev->dev, "name=%s\n",
		apipe_dev->device_name);
	apipe_dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	apipe_dev->misc_dev.name = apipe_dev->device_name;
	apipe_dev->misc_dev.fops = &apipe_fops;
	apipe_dev->misc_dev.parent = NULL;
	apipe_dev->dev = (struct device *)&pdev->dev;
	rval = misc_register(&apipe_dev->misc_dev);
	if (rval != 0) {
		dev_err(apipe_dev->dev, "Failed to alloc apipe chrdev\n");
		return rval;
	}
	mutex_init(&apipe_dev->mutex);
	mutex_init(&apipe_dev->w_mutex);
	dev_set_drvdata(&pdev->dev, apipe_dev);

	return 0;
}

static int apipe_remove(struct platform_device *pdev)
{
	struct apipe_device *apipe_dev = platform_get_drvdata(pdev);

	if (apipe_dev) {
		mutex_destroy(&apipe_dev->mutex);
		misc_deregister((struct miscdevice *)&apipe_dev->misc_dev);
	}
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver apipe_driver = {
	.driver = {
		.name = "sprd-apipe",
		.of_match_table = apipe_match_table,
	},
	.probe = apipe_probe,
	.remove = apipe_remove,
};

module_platform_driver(apipe_driver);

MODULE_AUTHOR("SPRD");
MODULE_DESCRIPTION("apipe driver");
MODULE_LICENSE("GPL v2");
