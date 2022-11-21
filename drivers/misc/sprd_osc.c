/*
 * Spreadtrum OSC support
 *
 * Copyright (C) 2015 spreadtrum, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/sprd_osc.h>
#include <asm/uaccess.h>

#include "sprd_osc_private.h"

DEFINE_SPINLOCK(sprd_osc_lock);

static unsigned long sprd_osc_status;
static struct sprd_osc_reg osc_reg;

static int sprd_osc_open(struct inode *inode, struct file *file)
{
	spin_lock_irq(&sprd_osc_lock);

	if (sprd_osc_status & SPRD_OSC_IS_OEPN)
		goto out_busy;
	sprd_osc_status |= SPRD_OSC_IS_OEPN;

	spin_unlock_irq(&sprd_osc_lock);

	return 0;

out_busy:
	spin_unlock_irq(&sprd_osc_lock);

	return -EBUSY;
}

static int sprd_osc_release(struct inode *inode, struct file *file)
{
	spin_lock_irq(&sprd_osc_lock);
	sprd_osc_status &= (~SPRD_OSC_IS_OEPN);
	spin_unlock_irq(&sprd_osc_lock);

	return 0;
}

static ssize_t sprd_osc_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned long speed_cnt;
	ssize_t retval;

	if (!(sprd_osc_status & SPRD_OSC_IS_OEPN)) {
		printk(KERN_ERR "sprd_osc: sprd_osc is not opened, read failed!\n");
		return -EIO;
	}

	if (count != sizeof(unsigned int))
		return -EINVAL;

	speed_cnt = sprd_osc_read_obs_speed_cnt();
	if (speed_cnt == 0) {
		return -EINVAL;
	}

	retval = put_user(speed_cnt, (unsigned int __user *)buf ? : sizeof(unsigned int));
	if (!retval)
		retval = count;

	return retval;
}

static long sprd_osc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned int osc_data;

	switch(cmd) {
		case SPRD_OSC_START:
			return sprd_osc_start();
		case SPRD_OSC_STOP:
			return sprd_osc_stop();
		case SPRD_OSC_SET_CHAIN:
			if (copy_from_user(&osc_data, (unsigned int *)arg, sizeof(unsigned int))) {
				printk(KERN_ERR "sprd_osc: SPRD_OSC_SET_CHAIN failed!\n ");
				return -EFAULT;
			}
			return sprd_osc_ctrl_sel(osc_data);
		case SPRD_OSC_CLK_NUM_SET:
			if (copy_from_user(&osc_data, (unsigned int *)arg, sizeof(unsigned int))) {
				printk(KERN_ERR "sprd_osc: SPRD_OSC_CLK_NUM_SET failed!\n");
				return -EFAULT;
			}
			return sprd_osc_ctrl_clk_num_set(osc_data);
	};
}

static const struct file_operations sprd_osc_fops = {
	.owner            = THIS_MODULE,
	.read             = sprd_osc_read,
	.unlocked_ioctl   = sprd_osc_ioctl,
	.open             = sprd_osc_open,
	.release          = sprd_osc_release,
};

//static struct platform_device *osc_pdev;
static struct miscdevice sprd_osc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "sprd_osc",
	.fops  = &sprd_osc_fops,
};


int sprd_osc_ctrl_sel(unsigned int chain)
{
	unsigned int osc_ctrl_cfg;
	unsigned long uninitialized_var(flags);

	if (chain >= OSC_MAX) {
		printk(KERN_ERR "sprd_osc: the chain is invalid, error!\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&sprd_osc_lock, flags);
	osc_ctrl_cfg = __raw_readl(osc_reg.osc_ctrl_reg);
        printk("sprd_osc: %s osc_ctrl_cfg before = %d\n", __func__, osc_ctrl_cfg);
	osc_ctrl_cfg &= (~OSC_CTRL_SEL_MSK);
	osc_ctrl_cfg |= chain << __ffs(OSC_CTRL_SEL_MSK);
	printk("sprd_osc: %s osc_ctrl_cfg after = %d\n", __func__, osc_ctrl_cfg);
	__raw_writel(osc_ctrl_cfg, osc_reg.osc_ctrl_reg);
	spin_unlock_irqrestore(&sprd_osc_lock, flags);

	return 0;
}
EXPORT_SYMBOL(sprd_osc_ctrl_sel);

int sprd_osc_ctrl_clk_num_set(unsigned int num)
{
	unsigned int osc_ctrl_cfg;
	unsigned long uninitialized_var(flags);

	if (num > OSC_CTRL_CLK_NUM_MSK) {
		printk(KERN_ERR "sprd_osc: oso ctrl clk num is invalid, error!\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&sprd_osc_lock, flags);
	osc_ctrl_cfg = __raw_readl(osc_reg.osc_ctrl_reg);
	printk(KERN_ERR "sprd_osc: %s osc_ctrl_cfg before = %d\n", __func__, osc_ctrl_cfg);
	osc_ctrl_cfg &= (~OSC_CTRL_CLK_NUM_MSK);
	osc_ctrl_cfg |= num;
	printk(KERN_ERR "sprd_osc: %s osc_ctrl_cfg after = %d\n", __func__, osc_ctrl_cfg);
	__raw_writel(osc_ctrl_cfg, osc_reg.osc_ctrl_reg);
	spin_unlock_irqrestore(&sprd_osc_lock, flags);

	return 0;
}
EXPORT_SYMBOL(sprd_osc_ctrl_clk_num_set);

int sprd_osc_start_or_stop(bool start)
{
	unsigned int osc_ctrl_cfg;
	unsigned long uninitialized_var(flags);

	osc_ctrl_cfg = __raw_readl(osc_reg.osc_ctrl_reg);

	spin_lock_irqsave(&sprd_osc_lock, flags); 
	if (start) {
		/* reset osc */
		osc_ctrl_cfg &= (~OSC_CTRL_RSTN);
		__raw_writel(osc_ctrl_cfg, osc_reg.osc_ctrl_reg);
		osc_ctrl_cfg |= OSC_CTRL_RSTN;
		__raw_writel(osc_ctrl_cfg, osc_reg.osc_ctrl_reg);

		/* osc enable */
		osc_ctrl_cfg |= OSC_CTRL_EN;
		__raw_writel(osc_ctrl_cfg, osc_reg.osc_ctrl_reg);

		/* osc start */
		osc_ctrl_cfg |= OSC_CTRL_START;
		__raw_writel(osc_ctrl_cfg, osc_reg.osc_ctrl_reg);
	} else {
		osc_ctrl_cfg &= (~OSC_CTRL_START);
		__raw_writel(osc_ctrl_cfg, osc_reg.osc_ctrl_reg);

		osc_ctrl_cfg &= (~OSC_CTRL_EN);
		__raw_writel(osc_ctrl_cfg, osc_reg.osc_ctrl_reg);
	}
	spin_unlock_irqrestore(&sprd_osc_lock, flags); 

	return 0;
}

int sprd_osc_start(void)
{
	return sprd_osc_start_or_stop(true);
}
EXPORT_SYMBOL(sprd_osc_start);

int sprd_osc_stop(void)
{
	return sprd_osc_start_or_stop(false);
}
EXPORT_SYMBOL(sprd_osc_stop);

unsigned int sprd_osc_read_obs_speed_cnt(void)
{
	unsigned int osc_obs_cfg;

	osc_obs_cfg = __raw_readl(osc_reg.osc_obs_reg);
	if (osc_obs_cfg | OSC_OBS_OVERFLOW) {
		printk(KERN_ERR "sprd_osc: sprd osc obs overflow, the report value is invalid, error!\n");
		return 0;
	} else
		return (osc_obs_cfg & OSC_OBS_OVERFLOW_CNT_MSK);
}

EXPORT_SYMBOL(sprd_osc_read_obs_speed_cnt);

static int __init sprd_osc_init(void)
{
	if (misc_register(&sprd_osc_dev)) {
		printk(KERN_ERR "sprd_osc: register sprd_osc failed!\n");
		return -ENODEV;
	}

	osc_reg.osc_ctrl_reg = ioremap(0x402e0134, 0x4);
	osc_reg.osc_obs_reg  = ioremap(0x402e0138, 0x4);

	printk("sprd_osc: register sprd osc driver!\n");

	return 0;
}

static void __exit sprd_osc_exit(void)
{
	/*platform_device_unregister(&osc_pdev);*/
	misc_deregister(&sprd_osc_dev);
	iounmap(osc_reg.osc_ctrl_reg);
	iounmap(osc_reg.osc_obs_reg);
	printk("sprd_osc: unregister sprd osc driver!\n");
}

module_init(sprd_osc_init);
module_exit(sprd_osc_exit);

MODULE_AUTHOR("Xiaolong Zhang <xiaolong.zhang@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum OSC linux driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

