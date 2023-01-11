/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
 *
 * File Name          : pointer_acc.c
 * Description        : POINTER accelerometer sensor API
 *
 *******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
 *

 ******************************************************************************/
#include	<linux/err.h>
#include	<linux/errno.h>
#include	<linux/delay.h>
#include	<linux/fs.h>
#include	<linux/i2c.h>
#include <linux/module.h>
#include	<linux/input.h>
#include	<linux/input-polldev.h>
#include	<linux/miscdevice.h>
#include	<linux/uaccess.h>
#include  <linux/slab.h>

#include	<linux/workqueue.h>
#include	<linux/irq.h>
#include	<linux/gpio.h>
#include	<linux/interrupt.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include        <linux/earlysuspend.h>
#endif

#include "ptaccs.h"

#define DEBUG			0

#define ECS_DATA_DEV_NAME	"ptacc_data"
#define ECS_CTRL_DEV_NAME	"ptacc_ctrl"

static int ptacc_ctrl_open(struct inode *inode, struct file *file);
static int ptacc_ctrl_release(struct inode *inode, struct file *file);
static long ptacc_ctrl_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static DECLARE_WAIT_QUEUE_HEAD(open_wq);

static atomic_t	open_count;
static atomic_t	open_flag;
static atomic_t	reserve_open_flag;

static atomic_t	a_flag;
static atomic_t	m_flag;
static atomic_t	o_flag;

static short ptacc_delay = 20;


static struct input_dev *ptacc_data_device;

static struct file_operations ptacc_ctrl_fops = {
	//.owner		= THIS_MODULE,
	.open		= ptacc_ctrl_open,
	.release	= ptacc_ctrl_release,
	.unlocked_ioctl		= ptacc_ctrl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ptacc_ctrl_ioctl,
#endif
};

static struct miscdevice ptacc_ctrl_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = ECS_CTRL_DEV_NAME,
	.fops = &ptacc_ctrl_fops,
};

static int ptacc_ctrl_open(struct inode *inode, struct file *file)
{
#if 1
	atomic_set(&reserve_open_flag, 1);
	atomic_set(&open_flag, 1);
	atomic_set(&open_count, 1);
	wake_up(&open_wq);

	return 0;
#else
	int ret = -1;

	if (atomic_cmpxchg(&open_count, 0, 1) == 0) {
		if (atomic_cmpxchg(&open_flag, 0, 1) == 0) {
			atomic_set(&reserve_open_flag, 1);
			wake_up(&open_wq);
			ret = 0;
		}
	}

	return ret;
#endif
}

static int ptacc_ctrl_release(struct inode *inode, struct file *file)
{
	atomic_set(&reserve_open_flag, 0);
	atomic_set(&open_flag, 0);
	atomic_set(&open_count, 0);
	wake_up(&open_wq);

	return 0;
}

static long ptacc_ctrl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *pa = (void __user *)arg;
	short flag;
	short delay;
	int parms[3] = {0, 0, 0};

    //printk(KERN_INFO "%s: %s call with cmd 0x%x and arg 0x%x\n",
			//ECS_CTRL_DEV_NAME, __func__, cmd, (unsigned int)arg);

	switch (cmd) {
	case ACC_IOC_SET_MODE:
		break;
	case ACC_IOC_SET_DELAY:
		if (copy_from_user(&delay, pa, sizeof(delay)))
			return -EFAULT;
		ptacc_delay = delay;
		break;
	case ACC_IOC_GET_DELAY:
		delay = ptacc_delay;
		if (copy_to_user(pa, &delay, sizeof(delay)))
			return -EFAULT;
		break;

	case ACC_IOC_SET_AFLAG:
		if (copy_from_user(&flag, pa, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;
		atomic_set(&a_flag, flag);
		break;
	case ACC_IOC_GET_AFLAG:
		flag = atomic_read(&a_flag);
		if (copy_to_user(pa, &flag, sizeof(flag)))
			return -EFAULT;
		break;
	case ACC_IOC_SET_MFLAG:
		if (copy_from_user(&flag, pa, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;
		atomic_set(&m_flag, flag);
		break;
	case ACC_IOC_GET_MFLAG:
		flag = atomic_read(&m_flag);
		if (copy_to_user(pa, &flag, sizeof(flag)))
			return -EFAULT;
		break;
	case ACC_IOC_SET_OFLAG:
		if (copy_from_user(&flag, pa, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;
		atomic_set(&o_flag, flag);
		break;
	case ACC_IOC_GET_OFLAG:
		flag = atomic_read(&o_flag);
		if (copy_to_user(pa, &flag, sizeof(flag)))
			return -EFAULT;
		break;

	case ACC_IOC_SET_APARMS:
		if (copy_from_user(parms, pa, sizeof(parms)))
			return -EFAULT;
		//printk("input hal data %d %d %d\n", parms[0], parms[1],parms[2]);
		input_report_abs(ptacc_data_device, ABS_X, parms[0]);
		input_report_abs(ptacc_data_device, ABS_Y, parms[1]);
		input_report_abs(ptacc_data_device, ABS_Z, parms[2]);
		input_sync(ptacc_data_device);
		break;

	default:
		break;
	}

	return 0;
}

static ssize_t ptacc_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "ptacc_ctrl");
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(ptacc_ctrl, S_IRUGO, ptacc_ctrl_show, NULL);

static int __init ptacc_init(void)
{
	int res = 0;

	pr_info("ptaccelerator driver: init\n");

	ptacc_data_device = input_allocate_device();
	if (!ptacc_data_device) {
		res = -ENOMEM;
		pr_err("%s: failed to allocate input device\n", __FUNCTION__);
		goto out;
	}

	set_bit(EV_ABS, ptacc_data_device->evbit);

	/* 1024 == 1g, range -8g ~ +8g */
	/* ptacceleration x-axis */
	input_set_abs_params(ptacc_data_device, ABS_X,
		-1024*8, 1024*8, 0, 0);
	/* ptacceleration y-axis */
	input_set_abs_params(ptacc_data_device, ABS_Y,
		-1024*8, 1024*8, 0, 0);
	/* ptacceleration z-axis */
	input_set_abs_params(ptacc_data_device, ABS_Z,
		-1024*8, 1024*8, 0, 0);
	/* ptacceleration status, 0 ~ 3 */
	input_set_abs_params(ptacc_data_device, ABS_WHEEL,
		0, 100, 0, 0);


	ptacc_data_device->name = ECS_DATA_DEV_NAME;
	res = input_register_device(ptacc_data_device);
	if (res) {
		pr_err("%s: unable to register input device: %s\n",
			__FUNCTION__, ptacc_data_device->name);
		goto out_free_input;
	}

	res = misc_register(&ptacc_ctrl_device);
	if (res) {
		pr_err("%s: ptacc_ctrl_device register failed\n", __FUNCTION__);
		goto out_free_input;
	}
	res = device_create_file(ptacc_ctrl_device.this_device, &dev_attr_ptacc_ctrl);
	if (res) {
		pr_err("%s: device_create_file failed\n", __FUNCTION__);
		goto out_deregister_misc;
	}

	return 0;

out_deregister_misc:
	misc_deregister(&ptacc_ctrl_device);
out_free_input:
	input_free_device(ptacc_data_device);
out:
	return res;
}

static void __exit ptacc_exit(void)
{
	pr_info("ptaccelerator driver: exit\n");
	device_remove_file(ptacc_ctrl_device.this_device, &dev_attr_ptacc_ctrl);
	misc_deregister(&ptacc_ctrl_device);
	input_free_device(ptacc_data_device);
}

module_init(ptacc_init);
module_exit(ptacc_exit);

MODULE_AUTHOR("Robbie Cao<hjcao@memsic.com>");
MODULE_DESCRIPTION("MEMSIC POINTER ALGO");
MODULE_LICENSE("GPL");
