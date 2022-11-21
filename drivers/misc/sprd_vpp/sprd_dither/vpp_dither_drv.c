/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
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
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/math64.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/io.h>
#include <linux/pid.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/device.h>
#endif

#include <mach/hardware.h>
#include <mach/arch_misc.h>
#include <video/ion_sprd.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "vpp_dither_types.h"
#include "vpp_dither_config.h"
#define	DO_DITHER_WORK 0x01


struct vpp_dither_device *vpp_dither_ctx = NULL;

static int vpp_dither_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct vpp_dither_device *dev = vpp_dither_ctx;

	printk(KERN_INFO "[vpp_dither] %s\n",__func__);

	if(NULL == dev)
	{
		printk(KERN_ERR "[vpp_dither] dev is null!\n");
		ret = -ENODEV;
		goto exit;
	}
	filp->private_data = dev;

exit:
	return ret;
}

static int vpp_dither_release(struct inode *inode, struct file *filp)
{
	struct vpp_dither_device *dev = (vpp_dither_device *)filp->private_data;

	printk(KERN_INFO "[vpp_dither] %s\n",__func__);
	if(NULL == dev)
	{
		printk(KERN_ERR "[vpp_dither] vpp_dither_release error!--dev is null!\n");
		return -ENODEV;
	}
	filp->private_data = NULL;
	return 0;
}

static ssize_t vpp_dither_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
	struct vpp_dither_device *dev = (vpp_dither_device *)filp->private_data;

	printk(KERN_INFO "[vpp_dither] %s\n",__func__);
	if(NULL == dev)
	{
		printk(KERN_ERR "[vpp_dither] %s:error--dev is null!\n",__func__);
		return -ENODEV;
	}
	return 1;
}

static ssize_t vpp_dither_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	struct vpp_dither_device *dev = (vpp_dither_device *)filp->private_data;

	printk(KERN_INFO "[vpp_dither] %s\n",__func__);
	if(NULL == dev)
	{
		printk(KERN_ERR "[vpp_dither] %s:error--dev is null!\n",__func__);
		return -ENODEV;
	}
	return 1;
}

static long vpp_dither_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int ret = 0;

	unsigned int param_size = _IOC_SIZE(cmd);
	struct vpp_dither_device *dev = (vpp_dither_device *)filp->private_data;

//	printk(KERN_INFO "[vpp_dither] %s:,cmd:0x%08x\n",__func__, cmd);

	if(dev->s_suspend_resume_flag == 1)
	{
		printk(KERN_ERR "[vpp_dither] %s,in suspend, ioctl just return",__func__);
		return ret;
	}
	switch(cmd)
	{
		case DO_DITHER_WORK:
			/*VPP_DITHER_SET_PARAM*/
			if(param_size > sizeof(struct vpp_dither_reg_info) || param_size == 0)
			{
				printk(KERN_ERR "[vpp_dither] %s, param_size error!\n");
				return -EFAULT;
			}
			else
			{
				ret = down_interruptible(&dev->hw_resource_sem);
				if(ret)
				{
					printk(KERN_ERR "[vpp_dither] %s,wait gsp-hw sema interrupt by signal,return",__func__);
					ret = -ERESTARTSYS;
					goto exit;
				}

				ret = copy_from_user((void*)&dev->reg_info, (void*)arg, param_size);
				if(ret)
				{
					printk(KERN_ERR "[vpp_dither] %s,copy_params_from_user failed!\n",__func__);
					ret = -EFAULT;
					goto exit1;
				}
				else
				{
					printk(KERN_INFO "[vpp_dither] %s,copy_params_from_user success!\n",__func__);
					VPP_Dither_Init(dev);
					ret = VPP_Dither_Map(dev);
					if(ret)
					{
						printk(KERN_ERR "[vpp_dither] %s, mmu map fail!\n",__func__);
						ret = -EFAULT;
						goto exit2;
					}

					VPP_Dither_CFG_Config(dev);
				}
			}

			/*VPP_DITHER_TRIGGER_RUN*/
//			printk(KERN_INFO "[vpp_dither] %s,trigger to run!\n",__func__);
			VPP_Dither_Trigger();

			/*VPP_DITHER_WAIT_FINISH*/
//			printk(KERN_INFO "[vpp_dither] %s,wait finish!\n",__func__);
			ret = down_timeout(&dev->wait_interrupt_sem, msecs_to_jiffies(30));
			if(ret == 0)
			{
				printk(KERN_INFO "[vpp_dither] %s,wait done sema success!\n",__func__);
			}
			else if(ret == -ETIME)//timeout
			{
				printk(KERN_ERR "[vpp_dither] %s,wait done sema timeout!\n",__func__);
				VPP_Dither_Module_Reset();
				VPP_Dither_Early_Init();
				ret = -EFAULT;

				goto exit2;
			}
			else if(ret)
			{
				printk(KERN_ERR "[vpp_dither] %s,wait done sema interrupted by a signal!\n",__func__);
				VPP_Dither_Module_Reset();
				VPP_Dither_Early_Init();
				ret = -EFAULT;

				goto exit2;
			}

//			VPP_Dither_Wait_Finish();
			VPP_Dither_Unmap(dev);
			VPP_Dither_Deinit(dev);
			up(&dev->hw_resource_sem);

			break;

		default:
			ret = -EINVAL;

	}
	return ret;

exit2:
	VPP_Dither_Unmap(dev);
	VPP_Dither_Deinit(dev);
	sema_init(&dev->wait_interrupt_sem, 0);
exit1:
	up(&dev->hw_resource_sem);
exit:

	return ret;
}

static struct file_operations vpp_dither_fops =
{
	.owner = THIS_MODULE,
	.open = vpp_dither_open,
	.release = vpp_dither_release,
	.write = vpp_dither_write,
	.read = vpp_dither_read,
	.unlocked_ioctl = vpp_dither_ioctl,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void vpp_dither_early_suspend(struct early_suspend *es)
{
	struct vpp_dither_device *dev = container_of(es, struct vpp_dither_device, early_suspend);

	down(&dev->hw_resource_sem);

	printk(KERN_INFO "[vpp_dither] %s\n",__func__);

	VPP_Dither_Module_Disable(dev);
//	VPP_Dither_Clock_Disable(dev);
	dev->s_suspend_resume_flag = 1;

	up(&dev->hw_resource_sem);
}

static void vpp_dither_late_resume(struct early_suspend *es)
{
	struct vpp_dither_device *dev = container_of(es, struct vpp_dither_device, early_suspend);

	down(&dev->hw_resource_sem);

	printk(KERN_INFO "[vpp_dither] %s\n",__func__);

//	VPP_Dither_Clock_Enable(dev);
	VPP_Dither_Module_Enable(dev);
	VPP_Dither_Early_Init();
	dev->s_suspend_resume_flag = 0;

	up(&dev->hw_resource_sem);
}

#else

static int vpp_dither_suspend(struct platform *pdev, pm_message_t state)
{
	struct vpp_dither_device *dev = platform_get_drvdata(pdev);

	printk(KERN_INFO "[vpp_dither] %s\n",__func__);

	down(&dev->hw_resource_sem);

	VPP_Dither_Module_Disable(dev);
//	VPP_Dither_Clock_Disable(dev);
	dev->s_suspend_resume_flag = 1;

	up(&dev->hw_resource_sem);
	return 0;
}

static int vpp_dither_resume(struct platform_device *pdev)
{
	struct vpp_dither_device *dev = platform_get_drvdata(pdev);

	printk(KERN_INFO "[vpp_dither] %s\n",__func__);

	down(&dev->hw_resource_sem);

//	VPP_Dither_Clock_Enable(dev);
	VPP_Dither_Module_Enable(dev);
	VPP_Dither_Early_Init();
	dev->s_suspend_resume_flag = 0;

	up(&dev->hw_resource_sem);
	return 0;
}
#endif

static int vpp_dither_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct vpp_dither_device *dev = NULL;

#ifdef CONFIG_OF
	struct resource r;
#endif

	printk(KERN_INFO "[vpp_dither] vpp_dither_probe enter!\n");

	dev = kmalloc(sizeof(struct vpp_dither_device), GFP_KERNEL);
	if(!dev)
	{
		printk(KERN_ERR "[vpp_dither] %s,fail to alloc memory!\n",__func__);
		ret = -ENOMEM;
		goto out;
	}
	memset(dev, 0, sizeof(struct vpp_dither_device));

#ifdef CONFIG_OF
	dev->of_dev = &(pdev->dev);
	dev->irq_num = irq_of_parse_and_map(dev->of_dev->of_node, 0);

	if(0 != of_address_to_resource(dev->of_dev->of_node, 0, &r))
	{
		printk(KERN_ERR "[vpp_dither] probe fail! (can't get register base address)\n");
		goto out;
	}
	dev->reg_base_addr = r.start;
	printk(KERN_INFO "[vpp_dither] irq_num = %d, reg_base_addr = 0x%x\n", dev->irq_num, dev->reg_base_addr);
#else
	dev->irq_num = VPP_DITHER_IRQ_NUM;
	dev->reg_base_addr = VPP_DITHER_BASE_ADDR;
#endif
/*
	ret = VPP_Dither_Clock_Init(dev);
	if(ret)
	{
		printk(KERN_ERR "[vpp_dither] vpp clock init fail!\n");
		goto out;
	}
	VPP_Dither_Clock_Enable(dev);*/

	vpp_dither_ctx = dev;

	VPP_Dither_Early_Init();

//	VPP_Dither_Clock_Disable(dev);

	dev->misc_dev.minor = MISC_DYNAMIC_MINOR,
	dev->misc_dev.name = "sprd_vpp_dither",
	dev->misc_dev.fops = &vpp_dither_fops,

	ret = misc_register(&dev->misc_dev);
	if(ret)
	{
		printk(KERN_ERR "[vpp_dither] can't register miscdev!\n");
		goto out;
	}

	ret = request_irq(dev->irq_num, VPP_Dither_IRQ_Handler, IRQF_SHARED, "vpp_dither", dev);
	if(ret)
	{
		printk(KERN_ERR "[vpp_dither] can't request irq %d\n", dev->irq_num);
		goto out1;
	}

	dev->is_device_free = 1;	//device free
	sema_init(&dev->hw_resource_sem, 1);
	sema_init(&dev->wait_interrupt_sem, 0);

#ifdef CONFIG_HAS_EARLYSUSPEND
	dev->early_suspend.suspend = vpp_dither_early_suspend;
	dev->early_suspend.resume = vpp_dither_late_resume;
	dev->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
	register_early_suspend(&dev->early_suspend);
#endif

	platform_set_drvdata(pdev, dev);

	printk(KERN_INFO "[vpp_dither] vpp_dither_probe out!\n");

	return ret;

out1:
	misc_deregister(&dev->misc_dev);
out:
	return ret;
}

static int vpp_dither_remove(struct platform_device *pdev)
{
	struct vpp_dither_device *dev = platform_get_drvdata(pdev);
	printk(KERN_INFO "[vpp_dither] vpp_dither_remove called!\n");

	free_irq(dev->irq_num, dev);

	misc_deregister(&dev->misc_dev);
	kfree(dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id vpp_dither_dt_ids[] =
{
	{.compatible = "sprd,sprd_vpp",},
	{}
};
#endif

static struct platform_driver vpp_dither_driver =
{
	.probe = vpp_dither_probe,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = vpp_dither_suspend,
	.resume = vpp_dither_resume,
#endif
	.remove = vpp_dither_remove,
	.driver =
	{
		.owner = THIS_MODULE,
		.name = "sprd_vpp_dither",

#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(vpp_dither_dt_ids),
#endif

	}
};

static int __init vpp_dither_init(void)
{
	printk(KERN_INFO "[vpp_dither] vpp_dither_init enter!\n");

	if(platform_driver_register(&vpp_dither_driver))
	{
		printk(KERN_ERR "[vpp_dither] platform driver register fail!\n");
		return -1;
	}
	else
	{
		printk(KERN_INFO "[vpp_dither] platform driver register successful!\n");
	}
	return 0;
}

static void __exit vpp_dither_exit(void)
{
	platform_driver_unregister(&vpp_dither_driver);
	printk(KERN_INFO "[vpp_dither] platform driver unregistered!\n");
}

module_init(vpp_dither_init);
module_exit(vpp_dither_exit);

MODULE_DESCRIPTION("VPP_Dither Driver");
MODULE_LICENSE("GPL");

