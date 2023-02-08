/* alps-input.c
 *
 * Input device driver for alps sensor
 *
 * Copyright (C) 2011-2012 ALPS ELECTRIC CO., LTD. All Rights Reserved.
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/sensor/sensors_core.h>
#include "alps_compass_io.h"

extern int hscd_get_magnetic_field_data(int *xyz);
extern void hscd_activate(int flgatm, int flg, int dtime);
extern int hscd_self_test_A(void);
extern int hscd_self_test_B(void);

static DEFINE_MUTEX(alps_lock);

static struct platform_device *pdev;
static struct input_polled_dev *alps_idev;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend alps_early_suspend_handler;
#endif

#define EVENT_TYPE_MAGV_X           REL_X
#define EVENT_TYPE_MAGV_Y           REL_Y
#define EVENT_TYPE_MAGV_Z           REL_Z

#define ALPS_POLL_INTERVAL   200    /* msecs */
#define ALPS_INPUT_FUZZ        0    /* input event threshold */
#define ALPS_INPUT_FLAT        0

#define POLL_STOP_TIME       400    /* (msec) */

#undef ALPS_DEBUG
#define alps_dbgmsg(str, args...) pr_err("%s: " str, __func__, ##args)
#define alps_errmsg(str, args...) pr_err("%s: " str, __func__, ##args)
#define alps_info(str, args...) pr_info("%s: " str, __func__, ##args)

#define SENSOR_DEFAULT_DELAY		(200)	/* 200 ms */
#define SENSOR_MAX_DELAY		(2000)	/* 2000 ms */

static int flag_mag = 0;
static int flg_suspend = 0;
static int mag_delay = SENSOR_DEFAULT_DELAY;
#ifdef CONFIG_HAS_EARLYSUSPEND
static int poll_stop_cnt = 0;
#endif
static struct input_dev *idev;

///////////////////////////////////////////////////////////////////////////////
// for I/O Control

static long alps_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = -1, tmpval;

	switch (cmd) {
	case ALPSIO_SET_MAGACTIVATE:
		ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
		if (ret) {
			alps_errmsg("Failed (cmd = ALPSIO_SET_MAGACTIVATE)\n" );
			return -EFAULT;
		}

		mutex_lock(&alps_lock);
		flag_mag = tmpval;

		hscd_activate(1, tmpval, mag_delay);
		mutex_unlock(&alps_lock);

		alps_info("ALPSIO_SET_MAGACTIVATE, flag_mag = %d\n", flag_mag);
		break;

	case ALPSIO_SET_DELAY:
		ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
		if (ret) {
			alps_errmsg("Failed (cmd = ALPSIO_SET_DELAY)\n" );
			return -EFAULT;
		}

		mutex_lock(&alps_lock);
		if (tmpval <=  15)			tmpval =  10;
		else if (tmpval <=  45)		tmpval =  20;
		else if (tmpval <=  135)	tmpval =  70;
		else	tmpval = 200;

		mag_delay = tmpval;
		hscd_activate(1, flag_mag, mag_delay);
		mutex_unlock(&alps_lock);

		alps_info("ALPSIO_SET_DELAY, delay = %d\n", mag_delay);
		break;

	case ALPSIO_ACT_SELF_TEST_A:

		mutex_lock(&alps_lock);
		ret = hscd_self_test_A();
		mutex_unlock(&alps_lock);

		alps_info("ALPSIO_ACT_SELF_TEST_A, result = %d\n", ret);

		if (copy_to_user(argp, &ret, sizeof(ret))) {
			alps_errmsg("Failed (cmd = ALPSIO_ACT_SELF_TEST_A)\n" );
			return -EFAULT;
		}
		break;

	case ALPSIO_ACT_SELF_TEST_B:

		mutex_lock(&alps_lock);
		ret = hscd_self_test_B();
		mutex_unlock(&alps_lock);

		alps_info("ALPSIO_ACT_SELF_TEST_B, result = %d\n", ret);

		if (copy_to_user(argp, &ret, sizeof(ret))) {
			alps_errmsg("Failed (cmd = ALPSIO_ACT_SELF_TEST_B)\n" );
			return -EFAULT;
		}
		break;

	default:
		return -ENOTTY;
	}

	return 0;
}

static int
alps_io_open( struct inode* inode, struct file* filp )
{
	return 0;
}

static int
alps_io_release( struct inode* inode, struct file* filp )
{
	return 0;
}

static struct file_operations alps_fops = {
	.owner   = THIS_MODULE,
	.open    = alps_io_open,
	.release = alps_io_release,
	.unlocked_ioctl = alps_ioctl,
};

static struct miscdevice alps_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "alps_compass_io",
	.fops  = &alps_fops,
};


///////////////////////////////////////////////////////////////////////////////
// for input device

static int alps_probe(struct platform_device *dev)
{
	int xyz[3];
	int ret;

	alps_info("is called\n");
	ret = hscd_get_magnetic_field_data(xyz);
	if (ret == -ENODEV) {
		pr_info("alps: hscd probe fail\n");
		return ret;
	}

	return 0;
}

static int alps_remove(struct platform_device *dev)
{
	alps_info("is called\n");
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void alps_early_suspend(struct early_suspend *handler)
{
#ifdef ALPS_DEBUG
	alps_info("is called\n");
#endif
	mutex_lock(&alps_lock);
	flg_suspend = 1;
	mutex_unlock(&alps_lock);
}

static void alps_early_resume(struct early_suspend *handler)
{
#ifdef ALPS_DEBUG
	alps_info("is called\n");
#endif
	mutex_lock(&alps_lock);
	poll_stop_cnt = POLL_STOP_TIME / mag_delay;
	flg_suspend = 0;
	mutex_unlock(&alps_lock);
}
#endif

static struct platform_driver alps_driver = {
	.driver    = {
		.name  = "alps-input",
		.owner = THIS_MODULE,
	},
	.probe     = alps_probe,
	.remove    = alps_remove,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend alps_early_suspend_handler = {
	.suspend = alps_early_suspend,
	.resume  = alps_early_resume,
};
#endif

static void hscd_poll(struct input_dev *idev)
{
	int xyz[3] = {0, };

	if(hscd_get_magnetic_field_data(xyz) == 0) {
#ifdef ALPS_DEBUG
		alps_dbgmsg("%d, %d, %d\n", xyz[0], xyz[1], xyz[2]);
#endif
		input_report_rel(idev, EVENT_TYPE_MAGV_X, xyz[0]);
		input_report_rel(idev, EVENT_TYPE_MAGV_Y, xyz[1]);
		input_report_rel(idev, EVENT_TYPE_MAGV_Z, xyz[2]);
		input_sync(idev);
	}
}


static void alps_poll(struct input_polled_dev *dev)
{
	if (!flg_suspend) {
		mutex_lock(&alps_lock);
		dev->poll_interval = mag_delay;
		if (flag_mag)
			hscd_poll(alps_idev->input);
		mutex_unlock(&alps_lock);
	}
}

static ssize_t
magnetic_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", flag_mag);
}

static ssize_t
magnetic_enable_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	int value;
	int err = 0;

	err = kstrtoint(buf, 10, &value);

	pr_debug("%s, %d value = %d\n", __func__, __LINE__, value);

	mutex_lock(&alps_lock);
	flag_mag = value;
	hscd_activate(1, flag_mag, mag_delay);
	mutex_unlock(&alps_lock);
	return count;
}

static ssize_t
magnetic_delay_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", mag_delay);
}

static ssize_t
magnetic_delay_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int new_delay;
	int err = 0;

	err = kstrtoint(buf, 10, &new_delay);

	if (err) {
		pr_err("%s, kstrtoint failed.", __func__);
		goto done;
	}
	if (new_delay < 0)
		goto done;

	pr_info("%s, new_delay = %d, old_delay = %d", __func__, new_delay,
			mag_delay);

	if (new_delay <= 15)
		new_delay = 10;
	else if (new_delay <= 45)
		new_delay = 20;
	else if (new_delay <= 135)
		new_delay = 70;
	else
		new_delay = SENSOR_DEFAULT_DELAY;

	mag_delay = new_delay;

	mutex_lock(&alps_lock);
	hscd_activate(1, flag_mag, new_delay);
	mutex_unlock(&alps_lock);
done:
	return count;
}

static DEVICE_ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP, magnetic_delay_show,
	magnetic_delay_store);


static struct device_attribute dev_attr_magnetic_enable =
	__ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
			magnetic_enable_show, magnetic_enable_store);


static struct attribute *magnetic_attributes[] = {
	&dev_attr_poll_delay.attr,
	&dev_attr_magnetic_enable.attr,
	NULL
};

static struct attribute_group magnetic_attribute_group = {
	.attrs = magnetic_attributes
};


static int __init alps_init(void)
{
	int ret;

	alps_info("is called\n");

	ret = platform_driver_register(&alps_driver);
	if (ret)
	    goto out_region;
	alps_dbgmsg("platform_driver_register\n");

	pdev = platform_device_register_simple("alps_compass", -1, NULL, 0);
	if (IS_ERR(pdev)) {
	    ret = PTR_ERR(pdev);
	    goto out_driver;
	}
	alps_dbgmsg("platform_device_register_simple\n");

	alps_idev = input_allocate_polled_device();
	if (!alps_idev) {
		ret = -ENOMEM;
		goto out_device;
	}
	alps_dbgmsg("input_allocate_polled_device\n");

	alps_idev->poll = alps_poll;
	alps_idev->poll_interval = ALPS_POLL_INTERVAL;

	/* initialize the input class */
	idev = alps_idev->input;
	idev->name = "magnetic_sensor";
	//idev->phys = "alps_compass/input0";
	idev->id.bustype = BUS_I2C;
	//idev->dev.parent = &pdev->dev;
	//idev->evbit[0] = BIT_MASK(EV_ABS);

	set_bit(EV_REL, idev->evbit);
	input_set_capability(idev, EV_REL, REL_X);
	input_set_capability(idev, EV_REL, REL_Y);
	input_set_capability(idev, EV_REL, REL_Z);

#if 0//defined(MAG_15BIT)
	input_set_abs_params(idev, EVENT_TYPE_MAGV_X,
			-16384, 16383, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
	input_set_abs_params(idev, EVENT_TYPE_MAGV_Y,
			-16384, 16383, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
	input_set_abs_params(idev, EVENT_TYPE_MAGV_Z,
			-16384, 16383, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
	//#elif defined(MAG_13BIT)
	input_set_abs_params(idev, EVENT_TYPE_MAGV_X,
			-4096, 4095, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
	input_set_abs_params(idev, EVENT_TYPE_MAGV_Y,
			-4096, 4095, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
	input_set_abs_params(idev, EVENT_TYPE_MAGV_Z,
			-4096, 4095, ALPS_INPUT_FUZZ, ALPS_INPUT_FLAT);
#endif

	ret = input_register_polled_device(alps_idev);
	if (ret)
		goto out_alc_poll;
	alps_dbgmsg("input_register_polled_device\n");

	ret = misc_register(&alps_device);
	if (ret) {
		alps_info("alps_io_device register failed\n");
		goto out_reg_poll;
	}
	alps_dbgmsg("misc_register\n");

	ret = sysfs_create_group(&idev->dev.kobj,
				&magnetic_attribute_group);

#if defined(CONFIG_SENSOR_USE_SYMLINK)
	ret =  sensors_initialize_symlink(alps_idev->input);
	if (ret < 0) {
		pr_err("%s: apls_sensors_initialize_symlink error(%d).\n",
                        __func__, ret);
		goto out_reg_poll;
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&alps_early_suspend_handler);
	alps_dbgmsg("register_early_suspend\n");
#endif
	mutex_lock(&alps_lock);
	flg_suspend = 0;
	mutex_unlock(&alps_lock);

	return 0;

out_reg_poll:
	input_unregister_polled_device(alps_idev);
	alps_info("input_unregister_polled_device\n");
out_alc_poll:
	input_free_polled_device(alps_idev);
	alps_info("input_free_polled_device\n");
out_device:
	platform_device_unregister(pdev);
	alps_info("platform_device_unregister\n");
out_driver:
	platform_driver_unregister(&alps_driver);
	alps_info("platform_driver_unregister\n");
out_region:
	return ret;
}

static void __exit alps_exit(void)
{
	alps_info("is called\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&alps_early_suspend_handler);
	alps_dbgmsg("early_suspend_unregister\n");
#endif
	misc_deregister(&alps_device);
	alps_dbgmsg("misc_deregister\n");
	input_unregister_polled_device(alps_idev);
	alps_dbgmsg("input_unregister_polled_device\n");
	input_free_polled_device(alps_idev);
	alps_dbgmsg("input_free_polled_device\n");
	platform_device_unregister(pdev);
	alps_dbgmsg("platform_device_unregister\n");
	platform_driver_unregister(&alps_driver);
	alps_dbgmsg("platform_driver_unregister\n");
}

module_init(alps_init);
module_exit(alps_exit);

MODULE_DESCRIPTION("Alps Input Device");
MODULE_AUTHOR("ALPS ELECTRIC CO., LTD.");
MODULE_LICENSE("GPL v2");
