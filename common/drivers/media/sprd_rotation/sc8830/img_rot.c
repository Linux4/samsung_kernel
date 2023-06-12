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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <linux/file.h>
#include <linux/sched.h>
#include <video/sprd_rot_k.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include "../../sprd_dcam/sc8830/dcam_drv.h"
#include "rot_drv.h"
#include "img_rot.h"


#define ROT_DEVICE_NAME "sprd_rotation"
#define ROT_TIMEOUT 5000/*ms*/
#define ROTATION_MINOR MISC_DYNAMIC_MINOR


struct rot_k_private{
	struct mutex 	sync_lock;
	atomic_t users;
};

struct rot_k_file{
	struct rot_k_private *rot_private;

	struct semaphore rot_done_sem;

	/*for dcam rotation module*/
	struct rot_drv_private drv_private;
};

static void rot_k_irq(void *fd)
{
	struct rot_k_file *rot_file = (struct rot_k_file*)fd;

	if (!rot_file) {
		return;
	}

	ROTATE_TRACE("rot_k_irq start.\n");
	dcam_rotation_end();
	up(&rot_file->rot_done_sem);
	ROTATE_TRACE("rot_k_irq end.\n");
}

static int rot_k_wait_stop(struct rot_k_file *fd)
{
	int ret = 0;

	ROTATE_TRACE("rot_k_wait_stop start.\n");
	ret = down_timeout(&fd->rot_done_sem, msecs_to_jiffies(ROT_TIMEOUT));
	ROTATE_TRACE("rot_k_wait_stop end.\n");
	udelay(1);
	return ret;
}

static int rot_k_start(struct rot_k_file *fd)
{
	int ret = 0;
	ROT_PARAM_CFG_T *s;


	if (!fd) {
		ret = -EFAULT;
		goto start_exit;
	}
	s = &(fd->drv_private.cfg);
	rot_k_register_cfg(s);

	if (0 == rot_k_is_end(s)) {
		ret = rot_k_wait_stop(fd);
		if (ret) {
			printk("rot_k_thread y wait error \n");
			goto start_out;
		}
		ROTATE_TRACE("rot_k_start y done, uv start. \n");
		rot_k_set_UV_param(s);
		rot_k_register_cfg(s);
	}
	ret = rot_k_wait_stop(fd);
	if (ret) {
		printk("rot_k_thread  wait error \n");
		goto start_out;
	}

start_out:
	rot_k_close();
	ROTATE_TRACE("rot_k_start  done \n");

start_exit:

	return ret;
}

struct platform_device*  rot_get_platform_device(void)
{
	struct device *dev;

	dev = bus_find_device_by_name(&platform_bus_type, NULL, ROT_DEVICE_NAME);
	if (!dev) {
		printk("%s: failed to find device\n", __func__);
		return NULL;
	}

	return to_platform_device(dev);
}

static int rot_k_open(struct inode *node, struct file *file)
{
	int ret = 0;
	struct rot_k_private *rot_private = platform_get_drvdata(rot_get_platform_device());
	struct rot_k_file *fd = NULL;

	if (!rot_private) {
		ret = -EFAULT;
		printk("rot_k_open fail rot_private NULL \n");
		goto exit;
	}

	fd = kzalloc(sizeof(*fd), GFP_KERNEL);
	if (!fd) {
		ret = -ENOMEM;
		printk("rot_k_open fail alloc \n");
		goto exit;
	}
	fd->rot_private = rot_private;
	fd->drv_private.rot_fd = (void*)fd;

	sema_init(&fd->rot_done_sem, 0);

	file->private_data  = fd;

	if (1 == atomic_inc_return(&rot_private->users)) {
		ret = rot_k_module_en();
		if (unlikely(ret)) {
			printk("Failed to enable rot module \n");
			ret = -EIO;
			goto faile;
		}
	}

	ret = rot_k_isr_reg(rot_k_irq, &fd->drv_private);
	if (unlikely(ret)) {
		printk("Failed to register rot ISR \n");
		ret = -EACCES;
		goto reg_faile;
	}

	goto open_out;
reg_faile:
	rot_k_module_dis();
faile:
	atomic_dec(&rot_private->users);
	kfree(fd);
	fd = NULL;
open_out:
	ROTATE_TRACE("rot_user %d\n",atomic_read(&rot_private->users));
exit:

	ROTATE_TRACE("rot_k_open fd=0x%x\n", (int)fd);
	ROTATE_TRACE("rot_k_open ret=%d\n", ret);

	return ret;
}

static int rot_k_release(struct inode *node, struct file *file)
{
	struct rot_k_private *rot_private = NULL;
	struct rot_k_file *fd = NULL;


	fd = file->private_data;
	if (!fd) {
		goto release_exit;
	}

	rot_private = fd->rot_private;
	if (!rot_private) {
		goto fd_free;
	}

	if (0 == atomic_dec_return(&rot_private->users)) {
		rot_k_module_dis();
	}

	ROTATE_TRACE("rot_user %d\n",atomic_read(&rot_private->users));

fd_free:
	kfree(fd);
	fd = 0;
	file->private_data = 0;
release_exit:
	
	ROTATE_TRACE("rot_k_release\n");

	return 0;
}

static long rot_k_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct rot_k_private *rot_private;
	struct rot_k_file *fd;


	fd = file->private_data;
	if (!fd) {
		ret = - EFAULT;
		printk("rot_k_ioctl fail fd NULL \n");
		goto ioctl_exit;
	}

	ROTATE_TRACE("rot_k_ioctl fd= 0x%x\n", (int)fd);

	rot_private = fd->rot_private;
	if (!rot_private) {
		ret = -EFAULT;
		printk("rot_k_ioctl fail rot private NULL \n");
		goto ioctl_exit;
	}
	mutex_lock(&rot_private->sync_lock);
	ROTATE_TRACE("rot_k_ioctl, 0x%x \n", cmd);

	switch (cmd) {
	case ROT_IO_START:
		{
			ROT_CFG_T params;

			ret = copy_from_user(&params, (ROT_CFG_T *) arg, sizeof(ROT_CFG_T));
			if (ret) {
				printk("rot_k_ioctl, failed get user info \n");
				goto ioctl_out;
			}

			ret = rot_k_io_cfg(&params,&fd->drv_private.cfg);
			if (ret) {
				printk("rot_k_ioctl, failed cfg \n");
				goto ioctl_out;
			}
			ret = rot_k_start(fd);
			if (ret) {
				ret = -EFAULT;
				printk("rot_k_ioctl, failed start \n");
				goto ioctl_out;
			}
		}
		break;
	default:
		break;
	}

ioctl_out:
	mutex_unlock(&rot_private->sync_lock);
ioctl_exit:

	ROTATE_TRACE("rot_k_ioctl ret=%d \n", ret);

	return ret;
}

static struct file_operations rotation_fops = {
	.owner = THIS_MODULE,
	.open = rot_k_open,
	.unlocked_ioctl = rot_k_ioctl,
	.release = rot_k_release,
};

static struct miscdevice rotation_dev = {
	.minor = ROTATION_MINOR,
	.name = ROT_DEVICE_NAME,
	.fops = &rotation_fops,
};

int rot_k_probe(struct platform_device *pdev)
{
	int ret;
	struct rot_k_private *rot_private;


	printk(KERN_ALERT "rot_k_probe called\n");

	rot_private = devm_kzalloc(&pdev->dev, sizeof(*rot_private), GFP_KERNEL);
	if (!rot_private) {
		return -ENOMEM;
	}

	mutex_init(&rot_private->sync_lock);

	platform_set_drvdata(pdev, rot_private);

	ret = misc_register(&rotation_dev);
	if (ret) {
		printk(KERN_ERR "cannot register miscdev on minor=%d (%d)\n",
			ROTATION_MINOR, ret);
		ret = -EACCES;
		goto probe_out;
	}

	printk(KERN_ALERT " rot_k_probe Success\n");
	goto exit;
probe_out:
	mutex_destroy(&rot_private->sync_lock);
	devm_kfree(&pdev->dev, rot_private);
	platform_set_drvdata(pdev, NULL);
exit:
	return 0;
}

static int rot_k_remove(struct platform_device *pdev)
{
	struct rot_k_private *rot_private;

	rot_private = platform_get_drvdata(pdev);

	if (!rot_private)
		goto remove_exit;

	printk(KERN_INFO "rot_k_remove called !\n");
	misc_deregister(&rotation_dev);

	mutex_destroy(&rot_private->sync_lock);
	devm_kfree(&pdev->dev, rot_private);
	platform_set_drvdata(pdev, NULL);
	printk(KERN_INFO "rot_k_remove Success !\n");

remove_exit:
	return 0;
}

static struct platform_driver rotation_driver = {
	.probe = rot_k_probe,
	.remove = rot_k_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = ROT_DEVICE_NAME,
		},
};

int __init rot_k_init(void)
{
	printk(KERN_INFO "rot_k_init called !\n");
	if (platform_driver_register(&rotation_driver) != 0) {
		printk("platform device register Failed \n");
		return -1;
	}
	return 0;
}

void rot_k_exit(void)
{
	printk(KERN_INFO "rot_k_exit called !\n");
	platform_driver_unregister(&rotation_driver);
}

module_init(rot_k_init);
module_exit(rot_k_exit);

MODULE_DESCRIPTION("rotation Driver");
MODULE_LICENSE("GPL");

