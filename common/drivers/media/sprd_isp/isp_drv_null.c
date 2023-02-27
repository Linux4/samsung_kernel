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
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <video/isp_drv_kernel.h>
#include <mach/sci.h>
#include <linux/clk.h>
#include <asm/cacheflush.h>
#include <mach/hardware.h>
#include <linux/of.h>
#include <linux/of_device.h>

#if defined(CONFIG_ARCH_SCX35)
#include "Shark_reg_isp.h"
#elif defined(CONFIG_ARCH_SC8825)
#include "Tiger_reg_isp.h"
#else
#error "Unknown architecture specification"
#endif


#define DEBUG_ISP_DRV
#ifdef DEBUG_ISP_DRV
#define ISP_PRINT   printk
#else
#define ISP_PRINT(...)
#endif
#define ISP_MINOR		MISC_DYNAMIC_MINOR/*isp minor number*/
static struct proc_dir_entry*  isp_proc_file;

/**file operation functions declare**/
static int32_t _isp_kernel_open(struct inode *node, struct file *filp);
static int32_t _isp_kernel_release(struct inode *node, struct file *filp);
static long _isp_kernel_ioctl( struct file *fl, unsigned int cmd, unsigned long param);
/**driver'  functions declare**/
static int32_t _isp_probe(struct platform_device *pdev);
static int32_t _isp_remove(struct platform_device *dev);
/**module'  functions declare**/
static int32_t __init isp_kernel_init(void);
static void isp_kernel_exit(void);

static struct file_operations isp_fops = {
	.owner	= THIS_MODULE,
	.open	= _isp_kernel_open,
	.unlocked_ioctl	= _isp_kernel_ioctl,
	.release	= _isp_kernel_release,
};

static struct miscdevice isp_dev = {
	.minor	= ISP_MINOR,
	.name	= "sprd_isp",
	.fops	= &isp_fops,
};

static const struct of_device_id of_match_table_isp[] = {
	{ .compatible = "sprd,sprd_isp", },
	{ },
};

static struct platform_driver isp_driver = {
	.probe	= _isp_probe,
	.remove	= _isp_remove,
	.driver	= {
		.owner = THIS_MODULE,
		.name = "sprd_isp",
		.of_match_table = of_match_ptr(of_match_table_isp),
	},
};

/**********************************************************
*open the device
*
***********************************************************/
static int32_t _isp_kernel_open (struct inode *node, struct file *pf)
{
	int32_t ret = 0;
	ISP_PRINT ("isp_k: open start \n");

	return ret;
}

//static irqreturn_t _dcam_irq_root(int irq, void *dev_id)
void _dcam_isp_root(void)
{
	int32_t	ret = 0;

	return IRQ_HANDLED;
}

/**********************************************************
*release the device
*
***********************************************************/
static int32_t _isp_kernel_release (struct inode *node, struct file *pf)
{
	int ret = 0;
	ISP_PRINT ("isp_k: release start \n");

	ISP_PRINT ("isp_k: release end \n");
	return ret;
}
/**********************************************************
*read info from  file
*size_t size:
*loff_t p:
***********************************************************/
static int32_t _isp_kernel_proc_read (char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int	 len = 0;
#if 0
	uint32_t	 reg_buf_len = 200;
	uint32_t	 print_len = 0, print_cnt = 0;
	uint32_t	*reg_ptr = 0;

	ISP_PRINT ("isp_k: _isp_kernel_proc_read 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x \n", (uint32_t)page, (uint32_t)start, (uint32_t)off, (uint32_t)count, (uint32_t)eof, (uint32_t)data);

	(void)start; (void)off; (void)count; (void)eof;

	if(0x00==g_isp_device.reg_base_addr)
	{
		return 0x00;
	}

	reg_ptr = (uint32_t*)g_isp_device.reg_base_addr;
	len += sprintf(page + len, "Context for ISP device \n");
	len += sprintf(page + len, "********************************************* \n");
	while (print_len < reg_buf_len) {
	len += sprintf(page + len, "offset 0x%x : 0x%x, 0x%x, 0x%x, 0x%x \n",
		print_len,
		reg_ptr[print_cnt],
		reg_ptr[print_cnt+1],
		reg_ptr[print_cnt+2],
		reg_ptr[print_cnt+3]);
	print_cnt += 4;
	print_len += 16;
	}
	len += sprintf(page + len, "********************************************* \n");
	len += sprintf(page + len, "The end of ISP device \n");
#endif
	return len;
}

/**********************************************************
*the io controller of isp
*unsigned int cmd:
*unsigned long param:
***********************************************************/
static long _isp_kernel_ioctl( struct file *fl, unsigned int cmd, unsigned long param)
{
	long ret = 0;

	if(ISP_IO_IRQ==cmd)
	{
	} else {

		switch (cmd)
		{
			case ISP_IO_READ: {
			}
			break;

			case ISP_IO_WRITE: {
			}
			break;

			case ISP_IO_RST: {
			ISP_PRINT(" isp_k:ioctl restet start \n");
			}
			break;

			case ISP_IO_SETCLK: 
			ISP_PRINT(" isp_k:ioctl set clock start \n");
			break;

			case ISP_IO_STOP: {
			}
			break;

			case ISP_IO_INT: {
			}
			break;

			case ISP_IO_DCAM_INT: {
			}
			break;

			case ISP_IO_LNC_PARAM: {
			}
			break;

			case ISP_IO_LNC: {
			}
			break;

			case ISP_IO_ALLOC: {
			}
			break;

			default:
			ISP_PRINT("isp_k:_ioctl cmd is unsupported, cmd = %x\n", (int32_t)cmd);
			return -EFAULT;
		}

	}

	//ISP_PRINT("isp_k:_ioctl finished\n");
	ISP_IOCTL_EXIT:
	return ret;
}

static int _isp_probe(struct platform_device *pdev)
{
	int ret = 0;
	ISP_PRINT ("isp_k:probe start\n");

	ret = misc_register(&isp_dev);
	if (ret) {
		ISP_PRINT ( "isp_k:probe cannot register miscdev on minor=%d (%d)\n",(int32_t)ISP_MINOR, (int32_t)ret);
		return ret;
	}
/*
	isp_proc_file = create_proc_read_entry("driver/sprd_isp" ,
					0444,
					NULL,
					_isp_kernel_proc_read,
					NULL);
	if (unlikely(NULL == isp_proc_file)) {
		ISP_PRINT("isp_k:probe Can't create an entry for isp in /proc \n");
		ret = ENOMEM;
		return ret;
	}
*/
	isp_dev.this_device->of_node = pdev->dev.of_node;
	ISP_PRINT (" isp_k:probe end\n");
	return 0;
}

static int _isp_remove(struct platform_device * dev)
{
	ISP_PRINT ("isp_k: remove start \n");

	misc_deregister(&isp_dev);

	if (isp_proc_file) {
		remove_proc_entry("driver/sprd_isp", NULL);
	}

	ISP_PRINT ("isp_k: remove end !\n");

	return 0;
}

static int32_t __init isp_kernel_init(void)
{
	int32_t ret = 0;
	uint32_t addr = 0;
	ISP_PRINT ("isp_k: init start \n");
	if (platform_driver_register(&isp_driver) != 0) {
		ISP_PRINT ("isp_kernel_init: platform device register error \n");
		return -1;
	}

	ISP_PRINT ("isp_k: init end\n");

	return ret;
}

static void isp_kernel_exit(void)
{
	ISP_PRINT ("isp_k: exit start \n");

	platform_driver_unregister(&isp_driver);

	ISP_PRINT ("isp_k: exit end \n");
}

module_init(isp_kernel_init);
module_exit(isp_kernel_exit);

MODULE_DESCRIPTION("Isp Driver");
MODULE_LICENSE("GPL");
