/* alps-input.c
 *
 * Input device driver for alps sensor
 *
 * Copyright (C) 2011-2013 ALPS ELECTRIC CO., LTD. All Rights Reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/mutex.h>

#include <linux/hrtimer.h>
#include <linux/slab.h>

#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>
#include "alps_io.h"
#include "sensors_head.h"

/*
extern int accsns_get_acceleration_data(int *xyz);
extern int hscd_get_magnetic_field_data(int *xyz);
extern void hscd_activate(int flgatm, int flg, int dtime);
extern void accsns_activate(int flgatm, int flg, int dtime);
*/
static struct mutex alps_lock;

static struct platform_device *pdev;

#define EVENT_TYPE_ACCEL_X          ABS_X
#define EVENT_TYPE_ACCEL_Y          ABS_Y
#define EVENT_TYPE_ACCEL_Z          ABS_Z
#define EVENT_TYPE_ACCEL_STATUS     ABS_WHEEL

#define EVENT_TYPE_YAW              ABS_RX
#define EVENT_TYPE_PITCH            ABS_RY
#define EVENT_TYPE_ROLL             ABS_RZ
#define EVENT_TYPE_ORIENT_STATUS    ABS_RUDDER

#define EVENT_TYPE_MAGV_X           ABS_HAT0X
#define EVENT_TYPE_MAGV_Y           ABS_HAT0Y
#define EVENT_TYPE_MAGV_Z           ABS_BRAKE

#define ALPS_INPUT_FUZZ		0	/* input event threshold */
#define ALPS_INPUT_FLAT		0

/*#define POLL_STOP_TIME		400	*//* (msec) */

static int probeM, probeA;
int (*accsns_get_acceleration_data)(int *xyz);
void (*accsns_activate)(int flgatm, int flg, int dtime);

/* driver data */
struct alps_input_data {
	struct input_dev *idev;
	struct hrtimer timer;
	struct work_struct work_alps;
	struct workqueue_struct *wq;
	int flgM, flgA;
	int delay;
	int poll_stop_cnt;
};

static struct alps_input_data *adev;

/* for I/O Control */

static long alps_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = -1, tmpval;
#if defined(CONFIG_SENSORS_ALPS_MAG_HSCDTD008A) || defined(CONFIG_SENSORS_ALPS_ACC_BMA254)
	int restart = 0;
#endif

	switch (cmd) {
	case ALPSIO_SET_MAGACTIVATE:
#if defined(CONFIG_SENSORS_ALPS_MAG_HSCDTD008A)
			ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
			if (ret) {
				pr_info("error : ioctl(ALPSIO_SET_MAGACTIVATE)\n");
				return -EFAULT;
			}
#ifdef ALPS_DEBUG
			pr_info("ioctl(ALPSIO_SET_MAGACTIVATE) M=%d\n", tmpval);
#endif
			mutex_lock(&alps_lock);
			if (probeM == PROBE_SUCCESS)
			{
				if (!adev->flgM && !adev->flgA && tmpval) restart = 1;
				hscd_activate(1, tmpval, adev->delay);
			}
			adev->flgM = tmpval;
			if (restart) {
				hrtimer_start(&adev->timer, ns_to_ktime((u64)adev->delay * NSEC_PER_MSEC), HRTIMER_MODE_REL);
			}
			if ((adev->flgM | adev->flgA) == 0) {
				hrtimer_cancel(&adev->timer);
			}
			mutex_unlock(&alps_lock);
#endif
			break;

	case ALPSIO_SET_ACCACTIVATE:
#if defined(CONFIG_SENSORS_ALPS_ACC_BMA254)
			ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
			if (ret) {
				pr_info("error : ioctl(cmd = ALPSIO_SET_ACCACTIVATE)\n");
				return -EFAULT;
			}
#ifdef ALPS_DEBUG
			pr_info("ioctl(ALPSIO_SET_ACCACTIVATE) A=%d\n", tmpval);
#endif
			mutex_lock(&alps_lock);
			if (probeA == PROBE_SUCCESS)
			{
				if (!adev->flgM && !adev->flgA && tmpval) restart = 1;
				accsns_activate(1, tmpval, adev->delay);
			}
			adev->poll_stop_cnt = 1;
			adev->flgA = tmpval;
			if (restart) {
				hrtimer_start(&adev->timer, ns_to_ktime((u64)adev->delay * NSEC_PER_MSEC), HRTIMER_MODE_REL);
			}
			if ((adev->flgM | adev->flgA) == 0) {
				hrtimer_cancel(&adev->timer);
			}
			mutex_unlock(&alps_lock);
#endif
			break;

	case ALPSIO_SET_DELAY:
			ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
			if (ret) {
				pr_info("error : ioctl(ALPSIO_SET_DELAY)\n");
				return -EFAULT;
			}
#ifdef ALPS_DEBUG
			pr_info("ioctl(ALPSIO_SET_DELAY)\n");
#endif
			if (tmpval <=  10)
				tmpval =  10;
			else if (tmpval <=  20)
				tmpval =  20;
			else if (tmpval <=  70)
				tmpval =  70;
			else
				tmpval = 200;
			mutex_lock(&alps_lock);
			adev->delay = tmpval;
			adev->poll_stop_cnt = 1;
#if defined(CONFIG_SENSORS_ALPS_MAG_HSCDTD008A)
			if (probeM == PROBE_SUCCESS)
				hscd_activate(1, adev->flgM, adev->delay);
#endif
#if defined(CONFIG_SENSORS_ALPS_ACC_BMA254)
			if (probeA == PROBE_SUCCESS)
				accsns_activate(1, adev->flgA, adev->delay);
#endif
			mutex_unlock(&alps_lock);
#ifdef ALPS_DEBUG
			pr_info("     delay = %d\n", adev->delay);
#endif
			break;

	default:
			return -ENOTTY;
	}
	return 0;
}

static int alps_io_open(struct inode *inode, struct file *filp)
{
#if defined(CONFIG_SENSORS_ALPS_ACC_BMA254)
	if (bma254_open()) {
		probeA = PROBE_SUCCESS;
		pr_info("[ACC] bma254 alps_io_open\n");

		accsns_get_acceleration_data
			= bma254_get_acceleration_data;
		pr_info("[ACC] bma254_get_acceleration_data\n");


		accsns_activate = bma254_activate;
		pr_info("[ACC] bma254_activate\n");

	} else
#endif
	{
		probeA = PROBE_FAIL;
	}
#if defined(CONFIG_SENSORS_ALPS_MAG_HSCDTD008A)
	if (hscd_open()) {
		probeM = PROBE_SUCCESS;
		pr_info("[HSCD] hscd_open SUCCESS\n");
	} else {
		probeM = PROBE_FAIL;
	}
#endif
	return 0;
}

static int alps_io_release(struct inode *inode, struct file *filp)
{
	pr_info("alps_io_release\n");
	return 0;
}

static const struct file_operations alps_fops = {
	.owner   = THIS_MODULE,
	.open    = alps_io_open,
	.release = alps_io_release,
	.unlocked_ioctl = alps_ioctl,
};

static struct miscdevice alps_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "alps_io",
	.fops  = &alps_fops,
};


/* for input device*/
#if defined(CONFIG_SENSORS_ALPS_ACC_BMA254)
static ssize_t accsns_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int x = 0, y = 0, z = 0;
	int xyz[3];

	if (probeA == PROBE_SUCCESS) {
		if (accsns_get_acceleration_data(xyz) == 0) {
			x = xyz[0];
			y = xyz[1];
			z = xyz[2];
		} else {
			x = 0;
			y = 0;
			z = 0;
		}
	}
	return snprintf(buf, PAGE_SIZE, "(%d %d %d)\n", x, y, z);
}
#endif
#if defined(CONFIG_SENSORS_ALPS_MAG_HSCDTD008A)
static ssize_t hscd_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int x = 0, y = 0, z = 0;
	int xyz[3];

	if (probeM == PROBE_SUCCESS) {
		if (hscd_get_magnetic_field_data(xyz) == 0) {
			x = xyz[0];
			y = xyz[1];
			z = xyz[2];
		} else {
			x = 0;
			y = 0;
			z = 0;
		}
	}
	return snprintf(buf, PAGE_SIZE, "(%d %d %d)\n", x, y, z);
}
#endif
static ssize_t alps_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t cnt = 0;
	mutex_lock(&alps_lock);
#if defined(CONFIG_SENSORS_ALPS_ACC_BMA254)
	cnt += accsns_position_show(dev, attr, buf);
#endif
#if defined(CONFIG_SENSORS_ALPS_MAG_HSCDTD008A)
	cnt += hscd_position_show(dev, attr, buf);
#endif
	mutex_unlock(&alps_lock);
	return cnt;
}

static DEVICE_ATTR(position, 0444, alps_position_show, NULL);

static struct attribute *alps_attributes[] = {
	&dev_attr_position.attr,
	NULL,
};

static struct attribute_group alps_attribute_group = {
	.attrs = alps_attributes,
};

static int alps_probe(struct platform_device *dev)
{
	pr_info("alps: alps_probe\n");
	return 0;
}

static int alps_remove(struct platform_device *dev)
{
	pr_info("alps: alps_remove\n");
	return 0;
}

static struct platform_driver alps_driver = {
	.driver	= {
		.name = "alps-input",
		.owner = THIS_MODULE,
	},
	.probe = alps_probe,
	.remove = alps_remove,
};
#if defined(CONFIG_SENSORS_ALPS_ACC_BMA254)
static void accsns_poll(struct input_dev *idev)
{
	if (probeA == PROBE_SUCCESS) {
		int xyz[3];
		if (accsns_get_acceleration_data(xyz) == 0) {
			input_report_abs(idev, EVENT_TYPE_ACCEL_X, xyz[0]);
			input_report_abs(idev, EVENT_TYPE_ACCEL_Y, xyz[1]);
			input_report_abs(idev, EVENT_TYPE_ACCEL_Z, xyz[2]);
			idev->sync = 0;
			input_event(idev, EV_SYN, SYN_REPORT, 1);
		}
	}
}
#endif
#if defined(CONFIG_SENSORS_ALPS_MAG_HSCDTD008A)
static void hscd_poll(struct input_dev *idev)
{
	if (probeM == PROBE_SUCCESS) {
		int xyz[3];
		if (hscd_get_magnetic_field_data(xyz) == 0) {
			input_report_abs(idev, EVENT_TYPE_MAGV_X, xyz[0]);
			input_report_abs(idev, EVENT_TYPE_MAGV_Y, xyz[1]);
			input_report_abs(idev, EVENT_TYPE_MAGV_Z, xyz[2]);
			idev->sync = 0;
			input_event(idev, EV_SYN, SYN_REPORT, 2);
		}
	}
}
#endif

static void alps_poll(struct work_struct *work)
{
#if defined(CONFIG_SENSORS_ALPS_MAG_HSCDTD008A) || defined(CONFIG_SENSORS_ALPS_ACC_BMA254)
	struct input_dev *idev = adev->idev;
#endif

	mutex_lock(&alps_lock);
	if (adev->poll_stop_cnt < 0) {
#if defined(CONFIG_SENSORS_ALPS_MAG_HSCDTD008A)
		if (adev->flgM)
			hscd_poll(idev);
#endif
#if defined(CONFIG_SENSORS_ALPS_ACC_BMA254)
		if (adev->flgA)
			accsns_poll(idev);
#endif
	} else
		adev->poll_stop_cnt--;

	mutex_unlock(&alps_lock);
}

static enum hrtimer_restart alps_timer_function(struct hrtimer *timer)
{
	queue_work(adev->wq, &adev->work_alps);
	hrtimer_forward_now(&adev->timer, ns_to_ktime((u64)adev->delay * NSEC_PER_MSEC));
	return HRTIMER_RESTART;
}

static int __init alps_init(void)
{
	struct input_dev *idev;
	int ret;
	probeA = PROBE_FAIL;
	probeM = PROBE_FAIL;

	adev = kzalloc(sizeof(struct alps_input_data), GFP_KERNEL);
	if (!adev) {
		return -ENOMEM;
	}

	ret = platform_driver_register(&alps_driver);
	if (ret)
		goto out_region;
	pr_info("alps-init: platform_driver_register\n");
	mutex_init(&alps_lock);
	pdev = platform_device_register_simple("alps", -1, NULL, 0);
	if (IS_ERR(pdev)) {
		ret = PTR_ERR(pdev);
		goto out_driver;
	}
	pr_info("alps-init: platform_device_register_simple\n");

	ret = sysfs_create_group(&pdev->dev.kobj, &alps_attribute_group);
	if (ret)
		goto out_device;
	pr_info("alps-init: sysfs_create_group\n");

	mutex_lock(&alps_lock);
	adev->flgM = 0;
	adev->flgA = 0;
	adev->delay = 200;
	adev->poll_stop_cnt = 1;
	mutex_unlock(&alps_lock);

	idev = input_allocate_device();
	if (!idev) {
		ret = -ENOMEM;
		goto out_group;
	}
	printk(KERN_INFO "alps-init: input_allocate_device\n");

	/* initialize the input class */
	idev->name = "alps";
	idev->phys = "alps/input0";
	idev->id.bustype = BUS_HOST;
	idev->evbit[0] = BIT_MASK(EV_ABS);

#if defined(CONFIG_SENSORS_ALPS_ACC_BMA254)
	input_set_abs_params(idev, EVENT_TYPE_ACCEL_X,
			-4096, 4096, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
	input_set_abs_params(idev, EVENT_TYPE_ACCEL_Y,
			-4096, 4096, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
	input_set_abs_params(idev, EVENT_TYPE_ACCEL_Z,
			-4096, 4096, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
#endif
#if defined(CONFIG_SENSORS_ALPS_MAG_HSCDTD008A)
	input_set_abs_params(idev, EVENT_TYPE_MAGV_X,
			-16384, 16383, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
	input_set_abs_params(idev, EVENT_TYPE_MAGV_Y,
			-16384, 16383, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
	input_set_abs_params(idev, EVENT_TYPE_MAGV_Z,
			-16384, 16383, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
#endif

	ret = input_register_device(idev);
	if (ret)
		goto out_idev;
	printk(KERN_INFO "alps-init: input_register_device\n");
	adev->idev = idev;

	/* symlink */
#ifdef CONFIG_SENSOR_USE_SYMLINK
	ret =  sensors_initialize_symlink(idev);
	if (ret) {
		pr_err("%s: cound not make k3dh sensor symlink(%d).\n",
			__func__, ret);
		goto out_symlink;
	}
#endif

	ret = misc_register(&alps_device);
	if (ret) {
		pr_info("alps-init: alps_io_device register failed\n");
		goto exit_misc_device_register_failed;
	}
	pr_info("alps-init: misc_register\n");

	hrtimer_init(&adev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	adev->timer.function = alps_timer_function;

	adev->wq = create_singlethread_workqueue("alps_wq");
	if (!adev->wq) {
		ret = -ENOMEM;
		goto out_wq;
	}
	INIT_WORK(&adev->work_alps, alps_poll);

	return 0;

out_wq:
	misc_deregister(&alps_device);
	printk(KERN_INFO "alps-init: misc_deregister\n");
exit_misc_device_register_failed:
#ifdef CONFIG_SENSOR_USE_SYMLINK
	sensors_delete_symlink(idev);
out_symlink:
#endif
	input_unregister_device(adev->idev);
	printk(KERN_INFO "alps-init: input_unregister_device\n");
out_idev:
	input_free_device(adev->idev);
	printk(KERN_INFO "alps-init: input_free_device\n");
out_group:
	sysfs_remove_group(&pdev->dev.kobj, &alps_attribute_group);
	pr_info("alps-init: sysfs_remove_group\n");
out_device:
	platform_device_unregister(pdev);
	pr_info("alps-init: platform_device_unregister\n");
out_driver:
	platform_driver_unregister(&alps_driver);
	pr_info("alps-init: platform_driver_unregister\n");
	mutex_destroy(&alps_lock);
out_region:
	kfree(adev);
	printk(KERN_INFO "alps-init: kfree\n");
	return ret;
}

static void __exit alps_exit(void)
{
	misc_deregister(&alps_device);
	pr_info("alps-exit: misc_deregister\n");
	input_unregister_device(adev->idev);
	pr_info("alps-exit: input_unregister_device\n");
	input_free_device(adev->idev);
	pr_info("alps-exit: input_free_device\n");
	sysfs_remove_group(&pdev->dev.kobj, &alps_attribute_group);
	pr_info("alps-exit: sysfs_remove_group\n");
	platform_device_unregister(pdev);
	pr_info("alps-exit: platform_device_unregister\n");
	platform_driver_unregister(&alps_driver);
	pr_info("alps-exit: platform_driver_unregister\n");
	kfree(adev);
	pr_info("alps-exit: kfree\n");
}

module_init(alps_init);
module_exit(alps_exit);

MODULE_DESCRIPTION("Alps Input Device");
MODULE_AUTHOR("ALPS");
MODULE_LICENSE("GPL v2");
