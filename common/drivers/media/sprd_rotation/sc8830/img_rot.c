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
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/vmalloc.h>
#include "dcam_drv.h"
#include "rot_drv.h"
#include "img_rot.h"

#define ROT_DEVICE_NAME "sprd_rotation"
#define ROT_TIMEOUT 5000/*ms*/
#define ROTATION_MINOR MISC_DYNAMIC_MINOR

struct rot_k_private {
	struct semaphore start_sem;
};

struct rot_k_file {
	struct rot_k_private *rot_private;

	struct semaphore rot_done_sem;
	/*for dcam rotation module*/
	struct rot_drv_private drv_private;
	struct device_node *dn;
};

static struct rot_k_private *s_rot_private;

static void rot_k_irq(void *fd)
{
	struct rot_k_file *rot_file = (struct rot_k_file*)fd;

	if (!rot_file) {
		return;
	}

	up(&rot_file->rot_done_sem);
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
	struct rot_k_private *rot_private = s_rot_private;//platform_get_drvdata(rot_get_platform_device())
	struct rot_k_file *fd = NULL;
	struct miscdevice *md = file->private_data ;

	if (!rot_private) {
		ret = -EFAULT;
		printk("rot_k_open fail rot_private NULL \n");
		goto exit;
	}

	fd = vzalloc(sizeof(*fd));
	if (!fd) {
		ret = -ENOMEM;
		printk("rot_k_open fail alloc \n");
		goto exit;
	}
	fd->rot_private = rot_private;
	fd->drv_private.rot_fd = (void*)fd;
	fd ->dn = md->this_device->of_node;

	spin_lock_init(&fd->drv_private.rot_drv_lock);

	sema_init(&fd->rot_done_sem, 0);

	file->private_data = fd;

	ROTATE_TRACE("rot_k_open success\n");

exit:
	return ret;
}

static int rot_k_release(struct inode *node, struct file *file)
{
	struct rot_k_private *rot_private;
	struct rot_k_file *fd;

	fd = file->private_data;
	if (!fd) {
		goto release_exit;
	}

	rot_private = fd->rot_private;
	if (!rot_private) {
		goto fd_free;
	}

	down(&rot_private->start_sem);
	up(&rot_private->start_sem);

fd_free:
	vfree(fd);
	fd = NULL;
	file->private_data = NULL;
release_exit:

	ROTATE_TRACE("rot_k_release\n");

	return 0;
}

static long rot_k_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct rot_k_private *rot_private;
	struct rot_k_file *fd;
	ROT_CFG_T params;
	ROT_PARAM_CFG_T *s;

	fd = file->private_data;
	if (!fd) {
		ret = - EFAULT;
		printk("rot_k_ioctl fail fd NULL \n");
		goto ioctl_exit;
	}

	rot_private = fd->rot_private;
	if (!rot_private) {
		ret = -EFAULT;
		printk("rot_k_ioctl fail rot private NULL \n");
		goto ioctl_exit;
	}

	s = &(fd->drv_private.cfg);

	switch (cmd) {
	case ROT_IO_START:
		down(&rot_private->start_sem);

		ret = rot_k_module_en(fd ->dn);
		if (unlikely(ret)) {
			printk("rot_k_ioctl error : rot_k_module_en\n");
			up(&rot_private->start_sem);
			goto ioctl_exit;
		}

		ret = rot_k_isr_reg(rot_k_irq, &fd->drv_private);
		if (unlikely(ret)) {
			printk("rot_k_ioctl error : Failed to register rot ISR \n");
			rot_k_module_dis(fd ->dn);
			up(&rot_private->start_sem);
			goto ioctl_exit;
		}

		ret = copy_from_user(&params, (ROT_CFG_T *) arg, sizeof(ROT_CFG_T));
		if (ret) {
			printk("rot_k_ioctl error, failed get user info \n");
			rot_k_module_dis(fd ->dn);
			up(&rot_private->start_sem);
			goto ioctl_exit;
		}

		ret = rot_k_io_cfg(&params,&fd->drv_private.cfg);
		if (ret) {
			printk("rot_k_ioctl error, failed cfg \n");
			rot_k_module_dis(fd ->dn);
			up(&rot_private->start_sem);
			goto ioctl_exit;
		}

		ret = rot_k_start(fd);
		if (ret) {
			printk("rot_k_ioctl error: failed start \n");
			rot_k_module_dis(fd ->dn);
			up(&rot_private->start_sem);
			goto ioctl_exit;
		}

		if (0 == rot_k_is_end(s)) {
			ret = down_timeout(&fd->rot_done_sem, msecs_to_jiffies(ROT_TIMEOUT));
			//ret = down_interruptible(&fd->rot_done_sem);
			if (ret) {
				printk("rot_k_ioctl error: rot_k_thread y wait error \n");
				goto ioctl_out;
			}

			ROTATE_TRACE("rot_k_ioctl: rot_k_start y done, uv start. \n");

			rot_k_set_UV_param(s);
			ret = rot_k_start(fd);
			if (ret) {
				printk("rot_k_ioctl error: failed second start \n");
				rot_k_module_dis(fd ->dn);
				up(&rot_private->start_sem);
				goto ioctl_exit;
			}
		}

		ret = down_timeout(&fd->rot_done_sem, msecs_to_jiffies(ROT_TIMEOUT));
		if (ret) {
		printk("rot_k_ioctl error: rot_k_thread  wait error \n");
		goto ioctl_out;
		}

		rot_k_close();

		rot_k_module_dis(fd ->dn);

		up(&rot_private->start_sem);
		break;
	default:
		break;
	}

ioctl_exit:

	ROTATE_TRACE("rot_k_ioctl ret=%d \n", ret);
	return ret;

ioctl_out:
	dcam_rotation_end();
	rot_k_close();
	rot_k_module_dis(fd ->dn);
	up(&rot_private->start_sem);
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

	sema_init(&rot_private->start_sem, 1);

	platform_set_drvdata(pdev, rot_private);

	s_rot_private = rot_private;

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

	devm_kfree(&pdev->dev, rot_private);
	platform_set_drvdata(pdev, NULL);
	printk(KERN_INFO "rot_k_remove Success !\n");

remove_exit:
	return 0;
}

static const struct of_device_id of_match_table_rot[] = {
	{ .compatible = "sprd,sprd_rotation", },
	{ },
};

static struct platform_driver rotation_driver = {
	.probe = rot_k_probe,
	.remove = rot_k_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = ROT_DEVICE_NAME,
		.of_match_table = of_match_ptr(of_match_table_rot),
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
