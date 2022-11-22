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
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <video/sprd_scale_k.h>
#include "img_scale.h"
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include "dcam_drv.h"
#include <linux/vmalloc.h>

#define SCALE_DEVICE_NAME "sprd_scale"
#define SCALE_TIMEOUT 5000/*ms*/
#define SCALE_MINOR MISC_DYNAMIC_MINOR
#define SC_COEFF_BUF_SIZE (24 << 10)

static struct scale_k_private *s_scale_private;

static void scale_k_irq(void *fd)
{
	struct scale_k_file *scale_file = (struct scale_k_file *)fd;

	if (!scale_file) {
		printk("scale_k_irq error: hand is null");
		return;
	}

	printk("sc done.\n");

	up(&scale_file->scale_done_sem);
}

struct platform_device*  scale_k_get_platform_device(void)
{
	struct device *dev;

	dev = bus_find_device_by_name(&platform_bus_type, NULL, SCALE_DEVICE_NAME);
	if (!dev) {
		printk("%s error: find device\n", __func__);
		return NULL;
	}

	return to_platform_device(dev);
}

static void scale_k_file_init(struct scale_k_file *fd, struct scale_k_private *scale_private)
{
	fd->scale_private = scale_private;

	sema_init(&fd->scale_done_sem, 0);

	fd->drv_private.scale_fd = (void*)fd;
	fd->drv_private.path_info.coeff_addr = scale_private->coeff_addr;

	spin_lock_init(&fd->drv_private.scale_drv_lock);
	sema_init(&fd->drv_private.path_info.done_sem, 0);
}

static int scale_k_open(struct inode *node, struct file *file)
{
	int ret = 0;
	struct scale_k_private *scale_private = s_scale_private; //platform_get_drvdata(scale_k_get_platform_device())
	struct scale_k_file *fd = NULL;
	struct miscdevice *md = file->private_data ;

	if (!scale_private) {
		ret = -EFAULT;
		printk("scale_k_open error: scale_private is null \n");
		goto exit;
	}

	fd = vzalloc(sizeof(*fd));
	if (!fd) {
		ret = -ENOMEM;
		printk("scale_k_open error: alloc \n");
		goto exit;
	}
	fd ->dn = md->this_device->of_node;
	scale_k_file_init(fd, scale_private);

	file->private_data = fd;

	SCALE_TRACE("scale_k_open fd=0x%x ret=%d\n", (int)fd, ret);

exit:
	return ret;
}

ssize_t scale_k_read(struct file *file, char __user *u_data, size_t cnt, loff_t *cnt_ret)
{
	uint32_t rt_word[2];

	(void)file; (void)cnt; (void)cnt_ret;

	if (cnt < sizeof(uint32_t)) {
		printk("scale_k_read error: wrong size of u_data: %d \n", cnt);
		return -1;
	}

	rt_word[0] = SCALE_LINE_BUF_LENGTH;
	rt_word[1] = SCALE_SC_COEFF_MAX;

	return copy_to_user(u_data, (void*)rt_word, (uint32_t)(2 * sizeof(uint32_t)));
}

static int scale_k_release(struct inode *node, struct file *file)
{
	struct scale_k_file *fd = NULL;
	struct scale_k_private *scale_private = NULL;

	fd = file->private_data;
	if (!fd) {
		goto exit;
	}

	scale_private = fd->scale_private;
	if (!scale_private) {
		goto fd_free;
	}

	down(&scale_private->start_sem);
	up(&scale_private->start_sem);

fd_free:
	vfree(fd);
	fd = NULL;
	file->private_data = NULL;

exit:
	SCALE_TRACE("scale_k_release\n");

	return 0;
}

static long scale_k_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct scale_k_private *scale_private;
	struct scale_k_file *fd;
	struct scale_frame_param_t frame_params;
	struct scale_slice_param_t slice_params;

	fd = file->private_data;
	if (!fd) {
		ret = - EFAULT;
		printk("scale_k_ioctl error:  fd null \n");
		goto ioctl_exit;
	}

	scale_private = fd->scale_private;
	if (!scale_private) {
		ret = -EFAULT;
		printk("scale_k_ioctl erro: scale private null \n");
		goto ioctl_exit;
	}

	switch (cmd) {
	case SCALE_IO_START:

		down(&scale_private->start_sem);

		ret = scale_k_module_en(fd->dn);
		if (unlikely(ret)) {
			printk("rot_k_ioctl erro: scale_module_en\n");
			up(&scale_private->start_sem);
			goto ioctl_exit;
		}

		ret = scale_k_isr_reg(scale_k_irq, &fd->drv_private);
		if (unlikely(ret)) {
			printk("rot_k_ioctl error:  scale_k_isr_reg\n");
			scale_k_module_dis(fd->dn);
			up(&scale_private->start_sem);
			goto ioctl_exit;

		}

		ret = copy_from_user(&frame_params, (struct scale_frame_param_t *)arg, sizeof(frame_params));
		if (ret) {
			printk("rot_k_ioctl error: get frame param info \n");
			scale_k_module_dis(fd->dn);
			up(&scale_private->start_sem);
			goto ioctl_exit;
		}

		ret = scale_k_start(&frame_params, &fd->drv_private.path_info);
		if (ret) {
			printk("rot_k_ioctl error: frame start \n");
			scale_k_module_dis(fd->dn);
			up(&scale_private->start_sem);
			goto ioctl_exit;
		}

		break;
	case SCALE_IO_DONE:
		ret = down_timeout(&fd->scale_done_sem, msecs_to_jiffies(5000));
		if (ret) {
			printk("scale_k_ioctl error:  interruptible time out\n");
			goto ioctl_out;
		}

		scale_k_stop();

		scale_k_module_dis(fd->dn);

		up(&scale_private->start_sem);

		break;

	case SCALE_IO_CONTINUE:
		/*Caution: slice scale is not supported by current driver.Please do not use it*/
		ret = copy_from_user(&slice_params, (struct scale_slice_param_t *)arg, sizeof(slice_params));
		if (ret) {
			printk("rot_k_ioctl error: get slice param info \n");
			goto ioctl_exit;
		}

		ret = scale_k_continue(&slice_params, &fd->drv_private.path_info);
		if (ret) {
			printk("rot_k_ioctl error: continue \n");
		}
		break;

	default:
		break;
	}

ioctl_exit:
	return ret;

ioctl_out:
	dcam_resize_end();
	scale_k_stop();
	scale_k_module_dis(fd->dn);
	up(&scale_private->start_sem);
	return ret;
}

static struct file_operations scale_fops = {
	.owner = THIS_MODULE,
	.open = scale_k_open,
	.read = scale_k_read,
	.unlocked_ioctl = scale_k_ioctl,
	.release = scale_k_release,
};

static struct miscdevice scale_dev = {
	.minor = SCALE_MINOR,
	.name = SCALE_DEVICE_NAME,
	.fops = &scale_fops,
};

int scale_k_probe(struct platform_device *pdev)
{
	int ret;
	struct scale_k_private *scale_private;

	scale_private = devm_kzalloc(&pdev->dev, sizeof(*scale_private), GFP_KERNEL);
	if (!scale_private) {
		return -ENOMEM;
	}
	scale_private->coeff_addr = (void *)vzalloc(SC_COEFF_BUF_SIZE);
	if (!scale_private->coeff_addr) {
		devm_kfree(&pdev->dev, scale_private);
		return -ENOMEM;
	}
	sema_init(&scale_private->start_sem, 1);

	platform_set_drvdata(pdev, scale_private);

	s_scale_private = scale_private;

	ret = misc_register(&scale_dev);
	if (ret) {
		printk("scale_k_probe error: ret=%d\n", ret);
		ret = -EACCES;
		goto probe_out;
	}

	goto exit;

probe_out:
	vfree(scale_private->coeff_addr);
	devm_kfree(&pdev->dev, scale_private);
	platform_set_drvdata(pdev, NULL);
exit:
	return 0;
}

static int scale_k_remove(struct platform_device *pdev)
{
	struct scale_k_private *scale_private;

	scale_private = platform_get_drvdata(pdev);

	if (!scale_private)
		goto remove_exit;

	misc_deregister(&scale_dev);
	if (scale_private->coeff_addr) {
		vfree(scale_private->coeff_addr);
	}
	devm_kfree(&pdev->dev, scale_private);
	platform_set_drvdata(pdev, NULL);

remove_exit:
	return 0;
}

static const struct of_device_id of_match_table_scale[] = {
	{ .compatible = "sprd,sprd_scale", },
	{ },
};

static struct platform_driver scale_driver =
{
	.probe = scale_k_probe,
	.remove = scale_k_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = SCALE_DEVICE_NAME,
		.of_match_table = of_match_ptr(of_match_table_scale),
	}
};

int __init scale_k_init(void)
{
	if (platform_driver_register(&scale_driver) != 0) {
		printk("platform scale device register Failed \n");
		return -1;
	}

	return 0;
}

void scale_k_exit(void)
{
	platform_driver_unregister(&scale_driver);
}

module_init(scale_k_init);
module_exit(scale_k_exit);
MODULE_DESCRIPTION("Sprd Scale Driver");
MODULE_LICENSE("GPL");
